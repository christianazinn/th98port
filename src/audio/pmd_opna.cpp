#include "th03/audio/pmd_opna.hpp"

#include "pmd_shim.h"
#include "ymfm_opn.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <new>
#include <optional>
#include <utility>
#include <vector>

namespace th03::audio {
namespace {

constexpr std::uint32_t fnv_offset = 2'166'136'261U;
constexpr std::uint32_t fnv_prime = 16'777'619U;

std::uint32_t clock_divisor(PmdOpnaFidelity fidelity) noexcept
{
	switch(fidelity) {
	case PmdOpnaFidelity::minimum:
		return 48U;
	case PmdOpnaFidelity::medium:
		return 24U;
	case PmdOpnaFidelity::maximum:
		return 8U;
	}
	return 0U;
}

ymfm::opn_fidelity ymfm_fidelity(PmdOpnaFidelity fidelity) noexcept
{
	switch(fidelity) {
	case PmdOpnaFidelity::minimum:
		return ymfm::OPN_FIDELITY_MIN;
	case PmdOpnaFidelity::medium:
		return ymfm::OPN_FIDELITY_MED;
	case PmdOpnaFidelity::maximum:
		return ymfm::OPN_FIDELITY_MAX;
	}
	return ymfm::OPN_FIDELITY_MAX;
}

struct PmdDeleter {
	void operator()(th03_pmd_handle *handle) const noexcept
	{
		th03_pmd_destroy(handle);
	}
};

using PmdPointer = std::unique_ptr<th03_pmd_handle, PmdDeleter>;

PmdPointer make_pmd() noexcept
{
	return PmdPointer(th03_pmd_create());
}

std::int32_t saturated_sum(std::int32_t left, std::int32_t right) noexcept
{
	const auto sum = static_cast<std::int64_t>(left) +
		static_cast<std::int64_t>(right);
	return static_cast<std::int32_t>(std::clamp(
		sum,
		static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
		static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())
	));
}

} // namespace

struct PmdOpnaEngine::Impl final : ymfm::ymfm_interface {
	explicit Impl(PmdOpnaConfig requested) :
		config(requested),
		clocks_per_sample(clock_divisor(requested.fidelity)),
		native_sample_rate(requested.input_clock_hz / clocks_per_sample),
		rhythm_rom(requested.rhythm_rom.size())
	{
		std::transform(
			requested.rhythm_rom.begin(), requested.rhythm_rom.end(),
			rhythm_rom.begin(),
			[](std::byte value) { return std::to_integer<std::uint8_t>(value); }
		);
		config.rhythm_rom = {};
		reset_chip();
	}

	void ymfm_set_timer(
		std::uint32_t timer,
		std::int32_t duration_in_clocks
	) override
	{
		if(timer >= timer_deadlines.size()) {
			return;
		}
		timer_deadlines[timer] = duration_in_clocks < 0 ? -1 :
			static_cast<std::int64_t>(chip_clocks) + duration_in_clocks;
	}

	void ymfm_update_irq(bool asserted) override
	{
		irq_asserted = asserted;
	}

	std::uint8_t ymfm_external_read(
		ymfm::access_class type,
		std::uint32_t address
	) override
	{
		if(type == ymfm::ACCESS_ADPCM_A && address < rhythm_rom.size()) {
			return rhythm_rom[address];
		}
		return 0U;
	}

	void reset_chip() noexcept
	{
		// ym2608::reset() intentionally retains the SSG resampler phase. A new
		// source song process starts with a fresh chip instance, so reconstruct it
		// here rather than carrying that presentation-only phase across Play.
		chip.reset();
		chip.emplace(*this);
		chip->set_fidelity(ymfm_fidelity(config.fidelity));
		chip_clocks = 0U;
		samples_rendered = 0U;
		register_writes = 0U;
		register_hash = fnv_offset;
		registers.fill(0U);
		timer_deadlines.fill(-1);
		irq_asserted = false;
		chip->reset();
		timer_deadlines.fill(-1);
		irq_asserted = false;
	}

	void write_register(unsigned address, unsigned value) noexcept
	{
		const auto bounded_address = static_cast<std::uint16_t>(address & 0x1FFU);
		const auto bounded_value = static_cast<std::uint8_t>(value);
		registers[bounded_address] = bounded_value;
		if((bounded_address & 0x100U) != 0U) {
			chip->write(2U, static_cast<std::uint8_t>(bounded_address));
			chip->write(3U, bounded_value);
		} else {
			chip->write(0U, static_cast<std::uint8_t>(bounded_address));
			chip->write(1U, bounded_value);
		}

		++register_writes;
		for(const auto byte : {
			static_cast<std::uint8_t>(bounded_address >> 8U),
			static_cast<std::uint8_t>(bounded_address),
			bounded_value,
		}) {
			register_hash ^= byte;
			register_hash *= fnv_prime;
		}
		if(trace_sink) {
			trace_sink->register_written(PmdOpnaRegisterWrite{
				.sequence = register_writes,
				.chip_clock = chip_clocks,
				.address = bounded_address,
				.value = bounded_value,
			});
		}
	}

	unsigned read_register(unsigned address) const noexcept
	{
		return registers[address & 0x1FFU];
	}

	std::uint8_t read_status(bool upper_port) noexcept
	{
		if(upper_port) {
			return chip->read_status_hi();
		}
		return static_cast<std::uint8_t>(chip->read_status() & 0x83U);
	}

	void expire_timer(std::uint32_t timer) noexcept
	{
		timer_deadlines[timer] = -1;
		if(m_engine) {
			m_engine->engine_timer_expired(timer);
		}
		if(irq_asserted && active_pmd) {
			th03_pmd_interrupt(active_pmd.get());
		}
	}

	void render_one(PmdOpnaStereoSample &sample) noexcept
	{
		for(std::uint32_t elapsed = 0U; elapsed < clocks_per_sample; ++elapsed) {
			++chip_clocks;
			for(std::uint32_t timer = 0U; timer < timer_deadlines.size(); ++timer) {
				const auto deadline = timer_deadlines[timer];
				if(deadline >= 0 &&
					static_cast<std::uint64_t>(deadline) <= chip_clocks) {
					expire_timer(timer);
				}
			}
		}

		ymfm::ym2608::output_data generated;
		generated.clear();
		chip->generate(&generated, 1U);
		static_assert(ymfm::ym2608::OUTPUTS == 3U);
		sample.left = saturated_sum(generated.data[0], generated.data[2]);
		sample.right = saturated_sum(generated.data[1], generated.data[2]);
		++samples_rendered;
	}

	static unsigned read_register_callback(void *context, unsigned address)
	{
		return static_cast<Impl *>(context)->read_register(address);
	}

	static void write_register_callback(
		void *context,
		unsigned address,
		unsigned value
	)
	{
		static_cast<Impl *>(context)->write_register(address, value);
	}

	static std::uint8_t read_status_callback(void *context, bool upper_port)
	{
		return static_cast<Impl *>(context)->read_status(upper_port);
	}

	PmdOpnaConfig config{};
	std::optional<ymfm::ym2608> chip{};
	std::uint32_t clocks_per_sample = 0U;
	std::uint32_t native_sample_rate = 0U;
	std::vector<std::uint8_t> rhythm_rom{};
	std::vector<std::uint8_t> prepared_song{};
	std::vector<std::uint8_t> active_song{};
	PmdPointer active_pmd{};
	std::array<std::uint8_t, 512U> registers{};
	std::array<std::int64_t, 2U> timer_deadlines{-1, -1};
	bool irq_asserted = false;
	std::uint64_t chip_clocks = 0U;
	std::uint64_t samples_rendered = 0U;
	std::uint64_t register_writes = 0U;
	std::uint32_t register_hash = fnv_offset;
	PmdOpnaRegisterTraceSink *trace_sink = nullptr;
};

std::expected<PmdOpnaEngine, PmdOpnaError> PmdOpnaEngine::create(
	PmdOpnaConfig config
)
{
	const auto divisor = clock_divisor(config.fidelity);
	if(divisor == 0U || config.input_clock_hz < divisor ||
		(config.input_clock_hz % divisor) != 0U) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::invalid_config,
		});
	}
	if(!config.rhythm_rom.empty() &&
		config.rhythm_rom.size() != ym2608_rhythm_rom_bytes) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::invalid_rhythm_rom_size,
			.supplied_size = config.rhythm_rom.size(),
		});
	}
	try {
		return PmdOpnaEngine(std::make_unique<Impl>(config));
	} catch(const std::bad_alloc &) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::allocation_failed,
		});
	}
}

PmdOpnaEngine::PmdOpnaEngine(std::unique_ptr<Impl> impl) noexcept :
	impl_(std::move(impl))
{
}

PmdOpnaEngine::PmdOpnaEngine(PmdOpnaEngine &&) noexcept = default;
PmdOpnaEngine &PmdOpnaEngine::operator=(PmdOpnaEngine &&) noexcept = default;
PmdOpnaEngine::~PmdOpnaEngine() = default;

std::expected<void, PmdOpnaError> PmdOpnaEngine::load_song(
	std::span<const std::byte> song
)
{
	if(song.empty() || song.size() > std::numeric_limits<std::uint16_t>::max()) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::invalid_song_size,
			.supplied_size = song.size(),
		});
	}
	try {
		std::vector<std::uint8_t> candidate(song.size());
		std::memcpy(candidate.data(), song.data(), song.size());
		auto validator = make_pmd();
		if(!validator) {
			return std::unexpected(PmdOpnaError{
				.code = PmdOpnaErrorCode::allocation_failed,
				.supplied_size = song.size(),
			});
		}
		if(!th03_pmd_load(
			validator.get(),
			candidate.data(),
			static_cast<std::uint16_t>(candidate.size())
		)) {
			return std::unexpected(PmdOpnaError{
				.code = PmdOpnaErrorCode::invalid_song,
				.supplied_size = song.size(),
			});
		}
		impl_->prepared_song.resize(song.size());
		std::memcpy(impl_->prepared_song.data(), song.data(), song.size());
		return {};
	} catch(const std::bad_alloc &) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::allocation_failed,
			.supplied_size = song.size(),
		});
	}
}

std::expected<void, PmdOpnaError> PmdOpnaEngine::play()
{
	if(impl_->prepared_song.empty()) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::no_song_loaded,
		});
	}
	try {
		auto candidate_song = impl_->prepared_song;
		auto candidate_pmd = make_pmd();
		if(!candidate_pmd) {
			return std::unexpected(PmdOpnaError{
				.code = PmdOpnaErrorCode::allocation_failed,
			});
		}
		if(!th03_pmd_load(
			candidate_pmd.get(),
			candidate_song.data(),
			static_cast<std::uint16_t>(candidate_song.size())
		)) {
			return std::unexpected(PmdOpnaError{
				.code = PmdOpnaErrorCode::invalid_song,
				.supplied_size = candidate_song.size(),
			});
		}

		impl_->reset_chip();
		if(!th03_pmd_start(
			candidate_pmd.get(),
			impl_.get(),
			&Impl::read_register_callback,
			&Impl::write_register_callback,
			&Impl::read_status_callback
		)) {
			return std::unexpected(PmdOpnaError{
				.code = PmdOpnaErrorCode::invalid_song,
				.supplied_size = candidate_song.size(),
			});
		}
		impl_->active_song = std::move(candidate_song);
		impl_->active_pmd = std::move(candidate_pmd);
		return {};
	} catch(const std::bad_alloc &) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::allocation_failed,
		});
	}
}

void PmdOpnaEngine::render_native(
	std::span<PmdOpnaStereoSample> output
) noexcept
{
	if(!impl_->active_pmd) {
		std::fill(output.begin(), output.end(), PmdOpnaStereoSample{});
		return;
	}
	for(auto &sample : output) {
		impl_->render_one(sample);
	}
}

void PmdOpnaEngine::set_register_trace_sink(
	PmdOpnaRegisterTraceSink *sink
) noexcept
{
	impl_->trace_sink = sink;
}

PmdOpnaStatus PmdOpnaEngine::status() const noexcept
{
	const auto *pmd = impl_->active_pmd.get();
	return PmdOpnaStatus{
		.song_loaded = !impl_->prepared_song.empty(),
		.playing = th03_pmd_is_playing(pmd),
		.paused = th03_pmd_is_paused(pmd),
		.rhythm_rom_present = !impl_->rhythm_rom.empty(),
		.native_sample_rate = impl_->native_sample_rate,
		.samples_rendered = impl_->samples_rendered,
		.chip_clocks = impl_->chip_clocks,
		.register_writes = impl_->register_writes,
		.register_hash = impl_->register_hash,
		.timer_b = th03_pmd_timer_b(pmd),
		.timer_b_count = th03_pmd_timer_b_count(pmd),
		.loop_count = th03_pmd_loop_count(pmd),
		.measure = th03_pmd_measure(pmd),
		.fade_volume = th03_pmd_fade_volume(pmd),
	};
}

} // namespace th03::audio
