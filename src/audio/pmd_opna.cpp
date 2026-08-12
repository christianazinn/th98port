#include "th03/audio/pmd_opna.hpp"

#include "pmd_opna_internal.hpp"

#include "pmd_shim.h"
#include "ymfm_opn.h"

#include <algorithm>
#include <array>
#include <cmath>
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
constexpr std::size_t effect_pointer_entries = 128U;
constexpr std::size_t effect_pointer_bytes = effect_pointer_entries * 2U;
constexpr std::size_t fm_tone_record_bytes = 0x1AU;

struct PreparedEffectBank {
	std::vector<std::uint8_t> bytes{};
	std::size_t tone_offset = 0U;
	std::vector<std::size_t> relocation_words{};
};

struct ActiveEffectLayout {
	std::uint16_t table_offset = 0U;
	std::uint16_t tone_offset = 0U;
};

std::uint16_t read_le16(
	std::span<const std::uint8_t> bytes,
	std::size_t offset
) noexcept
{
	return static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(bytes[offset]) |
		(static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U)
	);
}

void write_le16(
	std::span<std::uint8_t> bytes,
	std::size_t offset,
	std::uint16_t value
) noexcept
{
	bytes[offset] = static_cast<std::uint8_t>(value);
	bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
}

std::expected<PreparedEffectBank, PmdOpnaError> validate_effect_bank(
	std::span<const std::byte> bank
)
{
	if(bank.size() < (effect_pointer_bytes + fm_tone_record_bytes) ||
		bank.size() > pmd_effect_bank_max_bytes) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::invalid_effect_bank_size,
			.supplied_size = bank.size(),
		});
	}

	PreparedEffectBank candidate{};
	candidate.bytes.resize(bank.size());
	std::memcpy(candidate.bytes.data(), bank.data(), bank.size());
	const std::span<const std::uint8_t> bytes(candidate.bytes);
	std::array<std::uint16_t, effect_pointer_entries> pointers{};
	for(std::size_t index = 0U; index < pointers.size(); ++index) {
		pointers[index] = read_le16(bytes, index * 2U);
	}
	if(pointers.front() != effect_pointer_bytes) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::invalid_effect_bank,
			.supplied_size = 0U,
		});
	}
	for(std::size_t index = 0U; index + 1U < pointers.size(); ++index) {
		if(pointers[index] >= pointers[index + 1U] ||
			pointers[index + 1U] > bank.size()) {
			return std::unexpected(PmdOpnaError{
				.code = PmdOpnaErrorCode::invalid_effect_bank,
				.supplied_size = index,
			});
		}
	}
	candidate.tone_offset = pointers.back();
	if(candidate.tone_offset + fm_tone_record_bytes > bank.size()) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::invalid_effect_bank,
			.supplied_size = candidate.tone_offset,
		});
	}

	std::array<bool, 256U> referenced_tones{};
	for(std::size_t effect = 0U; effect < pmd_effect_count; ++effect) {
		std::size_t cursor = pointers[effect];
		const std::size_t end = pointers[effect + 1U];
		bool terminated = false;
		while(cursor < end) {
			const auto command = bytes[cursor];
			if(command == 0x80U) {
				terminated = true;
				++cursor;
				break;
			}
			if(command < 0x80U) {
				if(cursor + 2U > end) {
					break;
				}
				cursor += 2U;
				continue;
			}

			std::size_t argument_bytes = 0U;
			switch(command) {
			case 0xFFU:
				argument_bytes = 1U;
				break;
			case 0xFDU:
				argument_bytes = 1U;
				break;
			case 0xF3U:
			case 0xFBU:
				argument_bytes = 0U;
				break;
			case 0xF9U:
				argument_bytes = 2U;
				break;
			case 0xF8U:
			case 0xF2U:
				argument_bytes = 4U;
				break;
			case 0xF1U:
			case 0xF5U:
				argument_bytes = 1U;
				break;
			case 0xDAU:
				argument_bytes = 3U;
				break;
			default:
				return std::unexpected(PmdOpnaError{
					.code = PmdOpnaErrorCode::invalid_effect_bank,
					.supplied_size = cursor,
				});
			}
			if(cursor + 1U + argument_bytes > end) {
				break;
			}
			if(command == 0xFFU) {
				referenced_tones[bytes[cursor + 1U]] = true;
			} else if(command == 0xF9U) {
				const auto target = read_le16(bytes, cursor + 1U);
				if(target < pointers[effect] ||
					static_cast<std::size_t>(target) + 1U >= end) {
					return std::unexpected(PmdOpnaError{
						.code = PmdOpnaErrorCode::invalid_effect_bank,
						.supplied_size = cursor,
					});
				}
				candidate.relocation_words.push_back(cursor + 1U);
			} else if(command == 0xF8U) {
				const auto target = read_le16(bytes, cursor + 3U);
				if(static_cast<std::size_t>(target) + 2U <
					pointers[effect] ||
					static_cast<std::size_t>(target) + 2U >= end) {
					return std::unexpected(PmdOpnaError{
						.code = PmdOpnaErrorCode::invalid_effect_bank,
						.supplied_size = cursor,
					});
				}
				candidate.relocation_words.push_back(cursor + 3U);
			}
			cursor += 1U + argument_bytes;
		}
		if(!terminated || cursor != end) {
			return std::unexpected(PmdOpnaError{
				.code = PmdOpnaErrorCode::invalid_effect_bank,
				.supplied_size = effect,
			});
		}
	}

	std::array<bool, 256U> available_tones{};
	for(std::size_t offset = candidate.tone_offset;
		offset + fm_tone_record_bytes <= bank.size();
		offset += fm_tone_record_bytes) {
		const auto tone = bytes[offset];
		if(available_tones[tone]) {
			return std::unexpected(PmdOpnaError{
				.code = PmdOpnaErrorCode::invalid_effect_bank,
				.supplied_size = tone,
			});
		}
		available_tones[tone] = true;
	}
	for(std::size_t tone = 0U; tone < referenced_tones.size(); ++tone) {
		if(referenced_tones[tone] && !available_tones[tone]) {
			return std::unexpected(PmdOpnaError{
				.code = PmdOpnaErrorCode::invalid_effect_bank,
				.supplied_size = tone,
			});
		}
	}
	return candidate;
}

std::optional<ActiveEffectLayout> append_effect_bank(
	std::vector<std::uint8_t> &song,
	const PreparedEffectBank &bank
)
{
	if(song.empty() ||
		song.size() + bank.bytes.size() >
			std::numeric_limits<std::uint16_t>::max()) {
		return std::nullopt;
	}
	const auto vector_offset = song.size();
	const auto pmd_offset = vector_offset - 1U;
	song.insert(song.end(), bank.bytes.begin(), bank.bytes.end());
	std::span<std::uint8_t> active(song);
	for(std::size_t index = 0U; index < effect_pointer_entries; ++index) {
		const auto local = read_le16(bank.bytes, index * 2U);
		write_le16(
			active,
			vector_offset + (index * 2U),
			static_cast<std::uint16_t>(pmd_offset + local)
		);
	}
	for(const auto word_offset : bank.relocation_words) {
		const auto local = read_le16(bank.bytes, word_offset);
		write_le16(
			active,
			vector_offset + word_offset,
			static_cast<std::uint16_t>(pmd_offset + local)
		);
	}
	return ActiveEffectLayout{
		.table_offset = static_cast<std::uint16_t>(pmd_offset),
		.tone_offset = static_cast<std::uint16_t>(
			pmd_offset + bank.tone_offset
		),
	};
}

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

class DecodedRhythm final {
public:
	DecodedRhythm() = default;

	DecodedRhythm(
		const PmdOpnaRhythmPcm &source,
		std::uint32_t output_rate
	) : present_(source.sample_rate_hz != 0U)
	{
		if(!present_) {
			return;
		}
		for(std::size_t index = 0U; index < voices_.size(); ++index) {
			auto &voice = voices_[index];
			voice.samples.assign(
				source.voices[index].begin(), source.voices[index].end()
			);
			voice.end = static_cast<std::uint64_t>(voice.samples.size()) << 10U;
			voice.step =
				(static_cast<std::uint64_t>(source.sample_rate_hz) << 10U) /
				output_rate;
		}
		for(std::size_t attenuation = 0U;
			attenuation < attenuation_gain_.size(); ++attenuation) {
			const auto full_scale = std::llround(
				65'536.0 * std::exp2(
					-static_cast<double>(attenuation) / 8.0
				)
			) - 1LL;
			attenuation_gain_[attenuation] = static_cast<std::int32_t>(
				full_scale >> 4U
			);
		}
		reset();
	}

	[[nodiscard]] bool present() const noexcept { return present_; }

	void reset() noexcept
	{
		active_mask_ = 0U;
		total_attenuation_ = 63U;
		for(auto &voice : voices_) {
			voice.cursor = voice.end;
			voice.pan = 0U;
			voice.attenuation = 31U;
		}
	}

	void write(std::uint16_t address, std::uint8_t value) noexcept
	{
		if(!present_) {
			return;
		}
		if(address == 0x10U) {
			const auto mask = static_cast<std::uint8_t>(value & 0x3FU);
			if((value & 0x80U) != 0U) {
				active_mask_ = static_cast<std::uint8_t>(
					active_mask_ & static_cast<std::uint8_t>(~mask)
				);
				return;
			}
			active_mask_ = static_cast<std::uint8_t>(active_mask_ | mask);
			for(std::size_t index = 0U; index < voices_.size(); ++index) {
				if((mask & (1U << index)) != 0U) {
					voices_[index].cursor = 0U;
				}
			}
			return;
		}
		if(address == 0x11U) {
			total_attenuation_ = static_cast<std::uint8_t>(~value & 0x3FU);
			return;
		}
		if(address >= 0x18U && address <= 0x1DU) {
			auto &voice = voices_[address - 0x18U];
			voice.pan = static_cast<std::uint8_t>((value >> 6U) & 0x03U);
			voice.attenuation = static_cast<std::uint8_t>(~value & 0x1FU);
		}
	}

	[[nodiscard]] std::array<std::int32_t, 2U> render() noexcept
	{
		std::array<std::int32_t, 2U> mixed{};
		if(!present_) {
			return mixed;
		}
		for(std::size_t index = 0U; index < voices_.size(); ++index) {
			auto &voice = voices_[index];
			if((active_mask_ & (1U << index)) == 0U ||
				voice.cursor >= voice.end) {
				continue;
			}
			const auto source_index = static_cast<std::size_t>(
				voice.cursor >> 10U
			);
			voice.cursor += voice.step;
			const auto attenuation = static_cast<std::size_t>(
				total_attenuation_ + voice.attenuation
			);
			const auto scaled = static_cast<std::int32_t>(
				(static_cast<std::int64_t>(voice.samples[source_index]) *
					attenuation_gain_[attenuation]) >> 12U
			);
			if((voice.pan & 0x02U) != 0U) {
				mixed[0] += scaled;
			}
			if((voice.pan & 0x01U) != 0U) {
				mixed[1] += scaled;
			}
		}
		return mixed;
	}

private:
	struct Voice final {
		std::vector<std::int16_t> samples{};
		std::uint64_t end = 0U;
		std::uint64_t cursor = 0U;
		std::uint64_t step = 0U;
		std::uint8_t pan = 0U;
		std::uint8_t attenuation = 31U;
	};

	bool present_ = false;
	std::uint8_t active_mask_ = 0U;
	std::uint8_t total_attenuation_ = 63U;
	std::array<Voice, ym2608_rhythm_voice_count> voices_{};
	std::array<std::int32_t, 95U> attenuation_gain_{};
};

} // namespace

struct PmdOpnaEngine::Impl final : ymfm::ymfm_interface {
	explicit Impl(PmdOpnaConfig requested) :
		config(requested),
		clocks_per_sample(clock_divisor(requested.fidelity)),
		native_sample_rate(requested.input_clock_hz / clocks_per_sample),
		rhythm_rom(requested.rhythm_rom.size()),
		decoded_rhythm(requested.rhythm_pcm, native_sample_rate)
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
		decoded_rhythm.reset();
		timer_deadlines.fill(-1);
		irq_asserted = false;
	}

	void write_register(unsigned address, unsigned value) noexcept
	{
		const auto bounded_address = static_cast<std::uint16_t>(address & 0x1FFU);
		const auto bounded_value = static_cast<std::uint8_t>(value);
		registers[bounded_address] = bounded_value;
		decoded_rhythm.write(bounded_address, bounded_value);
		const bool is_rhythm_register =
			bounded_address >= 0x10U && bounded_address < 0x20U;
		const bool publish_to_chip =
			!is_rhythm_register || !rhythm_rom.empty();
		if(!publish_to_chip && bounded_address == 0x10U) {
			// The decoded/no-data profiles bypass ymfm's ADPCM-A decoder. Keep
			// its six internal channels explicitly dumped as PMD writes the
			// source control register; zero external bytes are valid ADPCM and
			// therefore cannot represent silence.
			chip->write(0U, 0x10U);
			chip->write(1U, 0xBFU);
		}
		if(publish_to_chip && (bounded_address & 0x100U) != 0U) {
			chip->write(2U, static_cast<std::uint8_t>(bounded_address));
			chip->write(3U, bounded_value);
		} else if(publish_to_chip) {
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
		const auto rhythm = decoded_rhythm.render();
		static_assert(ymfm::ym2608::OUTPUTS == 3U);
		sample.left = detail::pmd_mix_to_q31(
			generated.data[0], generated.data[2], rhythm[0]
		);
		sample.right = detail::pmd_mix_to_q31(
			generated.data[1], generated.data[2], rhythm[1]
		);
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
	DecodedRhythm decoded_rhythm{};
	std::vector<std::uint8_t> prepared_song{};
	std::optional<PreparedEffectBank> prepared_effect_bank{};
	std::vector<std::uint8_t> active_song{};
	PmdPointer active_pmd{};
	bool active_effect_bank = false;
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
	const auto pcm_voice_count = static_cast<std::size_t>(std::count_if(
		config.rhythm_pcm.voices.begin(), config.rhythm_pcm.voices.end(),
		[](auto voice) { return !voice.empty(); }
	));
	const bool pcm_requested =
		pcm_voice_count != 0U || config.rhythm_pcm.sample_rate_hz != 0U;
	if((pcm_requested &&
		(pcm_voice_count != ym2608_rhythm_voice_count ||
		 config.rhythm_pcm.sample_rate_hz == 0U)) ||
		(!config.rhythm_rom.empty() && pcm_requested)) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::invalid_rhythm_pcm,
			.supplied_size = pcm_voice_count,
		});
	}
	if(pcm_voice_count != 0U) {
		const auto native_rate = config.input_clock_hz / divisor;
		const auto step =
			(static_cast<std::uint64_t>(config.rhythm_pcm.sample_rate_hz) << 10U) /
			native_rate;
		if(step == 0U || std::any_of(
			config.rhythm_pcm.voices.begin(), config.rhythm_pcm.voices.end(),
			[](auto voice) {
				return voice.size() >
					(std::numeric_limits<std::uint64_t>::max() >> 10U);
			}
		)) {
			return std::unexpected(PmdOpnaError{
				.code = PmdOpnaErrorCode::invalid_rhythm_pcm,
				.supplied_size = pcm_voice_count,
			});
		}
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

std::expected<void, PmdOpnaError> PmdOpnaEngine::load_effect_bank(
	std::span<const std::byte> bank
)
{
	try {
		auto candidate = validate_effect_bank(bank);
		if(!candidate) {
			return std::unexpected(candidate.error());
		}
		impl_->prepared_effect_bank = std::move(*candidate);
		return {};
	} catch(const std::bad_alloc &) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::allocation_failed,
			.supplied_size = bank.size(),
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
		std::optional<ActiveEffectLayout> effect_layout{};
		if(impl_->prepared_effect_bank) {
			effect_layout = append_effect_bank(
				candidate_song,
				*impl_->prepared_effect_bank
			);
			if(!effect_layout) {
				return std::unexpected(PmdOpnaError{
					.code = PmdOpnaErrorCode::invalid_song_size,
					.supplied_size = impl_->prepared_song.size() +
						impl_->prepared_effect_bank->bytes.size(),
				});
			}
		}
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
		if(effect_layout && !th03_pmd_configure_effect_bank(
			candidate_pmd.get(),
			effect_layout->table_offset,
			effect_layout->tone_offset,
			pmd_effect_count
		)) {
			return std::unexpected(PmdOpnaError{
				.code = PmdOpnaErrorCode::invalid_effect_bank,
			});
		}
		impl_->active_song = std::move(candidate_song);
		impl_->active_pmd = std::move(candidate_pmd);
		impl_->active_effect_bank = effect_layout.has_value();
		return {};
	} catch(const std::bad_alloc &) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::allocation_failed,
		});
	}
}

void PmdOpnaEngine::stop() noexcept
{
	th03_pmd_stop(impl_->active_pmd.get());
}

void PmdOpnaEngine::pause() noexcept
{
	th03_pmd_pause(impl_->active_pmd.get());
}

void PmdOpnaEngine::resume() noexcept
{
	th03_pmd_resume(impl_->active_pmd.get());
}

void PmdOpnaEngine::fade(std::int8_t speed) noexcept
{
	th03_pmd_fade(impl_->active_pmd.get(), speed);
}

void PmdOpnaEngine::set_volume(std::uint8_t volume) noexcept
{
	th03_pmd_set_volume(impl_->active_pmd.get(), volume);
}

std::expected<void, PmdOpnaError> PmdOpnaEngine::play_effect(
	std::uint8_t effect_id
) noexcept
{
	if(effect_id >= pmd_effect_count) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::invalid_effect_id,
			.supplied_size = effect_id,
		});
	}
	if(!impl_->active_pmd) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::no_song_loaded,
		});
	}
	if(!impl_->active_effect_bank) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::no_effect_bank_loaded,
		});
	}
	if(!th03_pmd_play_effect(impl_->active_pmd.get(), effect_id)) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::invalid_effect_bank,
			.supplied_size = effect_id,
		});
	}
	return {};
}

void PmdOpnaEngine::stop_effect() noexcept
{
	th03_pmd_stop_effect(impl_->active_pmd.get());
}

std::expected<void, PmdOpnaError> PmdOpnaEngine::set_part_mask(
	std::uint8_t part,
	bool masked
) noexcept
{
	if(part > 23U) {
		return std::unexpected(PmdOpnaError{
			.code = PmdOpnaErrorCode::invalid_part,
			.supplied_size = part,
		});
	}
	if(impl_->active_pmd) {
		static_cast<void>(th03_pmd_set_part_mask(
			impl_->active_pmd.get(),
			part,
			masked
		));
	}
	return {};
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
		.effect_bank_loaded = impl_->prepared_effect_bank.has_value(),
		.rhythm_rom_present = !impl_->rhythm_rom.empty(),
		.rhythm_pcm_present = impl_->decoded_rhythm.present(),
		.active_effect = [pmd]() -> std::optional<std::uint8_t> {
			const auto effect = th03_pmd_active_effect(pmd);
			return effect == 0xFFU ? std::nullopt : std::optional{effect};
		}(),
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
		.fade_speed = th03_pmd_fade_speed(pmd),
		.part_mask = th03_pmd_part_mask(pmd),
	};
}

} // namespace th03::audio
