#include "th03/audio/mmd_soundfont.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <new>
#include <optional>
#include <utility>
#include <vector>

#include <tsf.h>

namespace th03::audio {
namespace {

constexpr std::size_t mmd_header_bytes = 0x50U;
constexpr std::size_t mmd_track_count = 18U;
constexpr std::size_t midi_channel_count = 16U;
constexpr std::size_t midi_note_count = 128U;
constexpr std::size_t mmd_loop_depth = 8U;
constexpr std::uint64_t tempo_scale = 64U;
constexpr std::size_t maximum_events_per_tick = 4'096U;
// GeneralUser GS SoftSynth reaches full-scale on dense TH03 arrangements at
// TinySoundFont's unity gain. Preserve headroom for PMD sound effects mixed
// after synthesis while keeping MIDI's average level close to the FM path.
constexpr float soundfont_output_gain = 0.7F;

struct Event final {
	std::uint8_t command = 0U;
	std::uint8_t delay = 0U;
	std::uint8_t parameter_one = 0U;
	std::uint8_t parameter_two = 0U;
};

struct PreparedTrack final {
	std::uint8_t transpose = 0U;
	std::uint8_t channel = 0xFFU;
	std::vector<Event> events{};
};

struct PreparedSong final {
	std::uint8_t tempo = 0U;
	std::uint8_t global_transpose = 0U;
	std::array<PreparedTrack, mmd_track_count> tracks{};
};

struct PlayingTrack final {
	struct LoopFrame final {
		std::size_t start = 0U;
		std::uint8_t completed = 0U;
	};

	std::size_t event = 0U;
	std::array<LoopFrame, mmd_loop_depth> loops{};
	std::size_t loop_count = 0U;
	std::uint16_t delay = 0U;
	std::array<std::uint16_t, midi_note_count> note_lengths{};
	bool ended = false;
};

[[nodiscard]] MmdSoundfontError error(
	MmdSoundfontErrorCode code,
	std::size_t offset = 0U,
	std::uint8_t command = 0U
) noexcept
{
	return {.code = code, .offset = offset, .command = command};
}

[[nodiscard]] std::uint16_t read_u16(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	return static_cast<std::uint16_t>(
		std::to_integer<std::uint8_t>(bytes[offset]) |
		(static_cast<std::uint16_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 1U])
		) << 8U)
	);
}

[[nodiscard]] bool supported_command(std::uint8_t command) noexcept
{
	return command <= 0x7FU || command == 0xE7U || command == 0xEBU ||
		command == 0xECU || command == 0xEEU || command == 0xF8U ||
		command == 0xF9U || command == 0xFEU;
}

[[nodiscard]] std::expected<PreparedTrack, MmdSoundfontError> parse_track(
	std::span<const std::byte> bytes,
	std::size_t pointer,
	std::uint8_t transpose,
	std::uint8_t channel
)
{
	if(pointer <= mmd_header_bytes || pointer >= bytes.size()) {
		return std::unexpected(error(
			MmdSoundfontErrorCode::invalid_song, pointer
		));
	}
	PreparedTrack result{.transpose = transpose, .channel = channel};
	std::array<std::uint8_t, 4U> cache{};
	auto position = pointer;
	while(position < bytes.size()) {
		const auto event_offset = position;
		const auto first = std::to_integer<std::uint8_t>(bytes[position++]);
		if(first == 0xFEU) {
			result.events.push_back({.command = first});
			return result;
		}
		if(first >= 0x80U && first <= 0x8FU) {
			const auto mask = static_cast<std::uint8_t>(first & 0x0FU);
			for(std::size_t index = 0U; index < cache.size(); ++index) {
				const auto bit = static_cast<std::uint8_t>(0x08U >> index);
				if((mask & bit) == 0U) {
					continue;
				}
				if(position >= bytes.size()) {
					return std::unexpected(error(
						MmdSoundfontErrorCode::invalid_song, position
					));
				}
				cache[index] = std::to_integer<std::uint8_t>(bytes[position++]);
			}
		} else {
			if(position + 3U > bytes.size()) {
				return std::unexpected(error(
					MmdSoundfontErrorCode::invalid_song, position
				));
			}
			cache = {
				first,
				std::to_integer<std::uint8_t>(bytes[position]),
				std::to_integer<std::uint8_t>(bytes[position + 1U]),
				std::to_integer<std::uint8_t>(bytes[position + 2U]),
			};
			position += 3U;
		}
		if(!supported_command(cache[0U])) {
			return std::unexpected(error(
				MmdSoundfontErrorCode::unsupported_song_command,
				event_offset,
				cache[0U]
			));
		}
		result.events.push_back({
			.command = cache[0U],
			.delay = cache[1U],
			.parameter_one = cache[2U],
			.parameter_two = cache[3U],
		});
	}
	return std::unexpected(error(
		MmdSoundfontErrorCode::invalid_song, position
	));
}

[[nodiscard]] std::expected<PreparedSong, MmdSoundfontError> parse_song(
	std::span<const std::byte> bytes
)
{
	if(bytes.size() <= mmd_header_bytes) {
		return std::unexpected(error(MmdSoundfontErrorCode::invalid_song));
	}
	PreparedSong result{
		.tempo = std::to_integer<std::uint8_t>(bytes[0U]),
		.global_transpose = std::to_integer<std::uint8_t>(bytes[1U]),
	};
	if(result.tempo == 0U) {
		return std::unexpected(error(MmdSoundfontErrorCode::invalid_song));
	}
	std::size_t first_track = bytes.size();
	for(std::size_t index = 0U; index < mmd_track_count; ++index) {
		const auto header = 2U + (index * 4U);
		first_track = std::min<std::size_t>(first_track, read_u16(bytes, header));
	}
	if(first_track <= mmd_header_bytes || first_track > bytes.size() ||
		std::find(
			bytes.begin() + static_cast<std::ptrdiff_t>(mmd_header_bytes),
			bytes.begin() + static_cast<std::ptrdiff_t>(first_track),
			std::byte{0U}
		) == bytes.begin() + static_cast<std::ptrdiff_t>(first_track)) {
		return std::unexpected(error(MmdSoundfontErrorCode::invalid_song));
	}
	for(std::size_t index = 0U; index < mmd_track_count; ++index) {
		const auto header = 2U + (index * 4U);
		auto parsed = parse_track(
			bytes,
			read_u16(bytes, header),
			std::to_integer<std::uint8_t>(bytes[header + 2U]),
			std::to_integer<std::uint8_t>(bytes[header + 3U])
		);
		if(!parsed) {
			return std::unexpected(parsed.error());
		}
		if(parsed->channel != 0xFFU && parsed->channel >= midi_channel_count) {
			return std::unexpected(error(
				MmdSoundfontErrorCode::invalid_song, header + 3U
			));
		}
		result.tracks[index] = std::move(*parsed);
	}
	return result;
}

[[nodiscard]] constexpr int signed_transpose(std::uint8_t value) noexcept
{
	return static_cast<int>(std::bit_cast<std::int8_t>(value));
}

[[nodiscard]] std::int32_t mix_sample(
	std::int32_t existing,
	std::int16_t midi
) noexcept
{
	constexpr auto midi_scale = std::int64_t{1} << 16U;
	const auto sum = static_cast<std::int64_t>(existing) +
		(static_cast<std::int64_t>(midi) * midi_scale);
	return static_cast<std::int32_t>(std::clamp(
		sum,
		static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::min()),
		static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max())
	));
}

} // namespace

struct MmdSoundfontEngine::Impl final {
	struct SoundfontCloser final {
		void operator()(tsf *value) const noexcept
		{
			if(value != nullptr) {
				tsf_close(value);
			}
		}
	};

	std::unique_ptr<tsf, SoundfontCloser> synth{};
	std::uint32_t sample_rate = pmd_canonical_sample_rate_hz;
	std::optional<PreparedSong> prepared{};
	std::array<PlayingTrack, mmd_track_count> tracks{};
	std::vector<std::int16_t> scratch{};
	std::uint64_t tempo_phase = 0U;
	std::uint64_t elapsed_ticks = 0U;
	std::uint8_t tempo_multiplier = static_cast<std::uint8_t>(tempo_scale);
	std::uint8_t fade_volume = 0U;
	std::int8_t fade_speed = 0;
	std::uint8_t music_volume_percent = 100U;
	bool playing = false;
	bool paused = false;

	[[nodiscard]] std::uint64_t tempo_increment() const noexcept
	{
		return static_cast<std::uint64_t>(prepared->tempo) *
			tempo_multiplier * mmd_ticks_per_quarter_note;
	}

	[[nodiscard]] std::uint64_t tempo_threshold() const noexcept
	{
		return static_cast<std::uint64_t>(sample_rate) * 60U * tempo_scale;
	}

	void apply_gain() noexcept
	{
		const auto audible = static_cast<float>(255U - fade_volume) / 255.0F;
		const auto user = static_cast<float>(music_volume_percent) / 100.0F;
		tsf_set_volume(synth.get(), audible * user * soundfont_output_gain);
	}

	void reset_synth() noexcept
	{
		tsf_reset(synth.get());
		for(std::size_t channel = 0U; channel < midi_channel_count; ++channel) {
			static_cast<void>(tsf_channel_set_presetnumber(
				synth.get(), static_cast<int>(channel), 0,
				channel == 9U ? 1 : 0
			));
			static_cast<void>(tsf_channel_set_pitchrange(
				synth.get(), static_cast<int>(channel), 12.0F
			));
		}
		apply_gain();
	}

	void note_off(std::size_t track_index, std::size_t note) noexcept
	{
		const auto channel = prepared->tracks[track_index].channel;
		if(channel != 0xFFU) {
			tsf_channel_note_off(
				synth.get(), static_cast<int>(channel), static_cast<int>(note)
			);
		}
	}

	[[nodiscard]] bool process_due_events(std::size_t track_index) noexcept
	{
		auto &state = tracks[track_index];
		const auto &track = prepared->tracks[track_index];
		for(std::size_t processed = 0U;
			processed < maximum_events_per_tick && !state.ended &&
			state.delay == 0U;
			++processed) {
			if(state.event >= track.events.size()) {
				return false;
			}
			const auto event_index = state.event++;
			const auto &event = track.events[event_index];
			const auto channel = static_cast<int>(track.channel);
			if(event.command <= 0x7FU) {
				if(track.channel != 0xFFU && event.parameter_one != 0U &&
					event.parameter_two != 0U) {
					const auto drum = (track.transpose & 0x80U) != 0U ||
						track.channel == 9U;
					const auto transpose = drum ? 0 :
						signed_transpose(prepared->global_transpose) +
						signed_transpose(track.transpose);
					const auto note = std::clamp(
						static_cast<int>(event.command) + transpose, 0, 127
					);
					if(!tsf_channel_note_on(
						synth.get(), channel, note,
						static_cast<float>(event.parameter_two) / 127.0F
					)) {
						return false;
					}
					state.note_lengths[static_cast<std::size_t>(note)] =
						event.parameter_one;
				}
				state.delay = event.delay;
				continue;
			}
			switch(event.command) {
			case 0xE7U:
				if(event.parameter_one == 0U) {
					return false;
				}
				tempo_multiplier = event.parameter_one;
				state.delay = event.delay;
				break;
			case 0xEBU:
				if(track.channel != 0xFFU && !tsf_channel_midi_control(
					synth.get(), channel, event.parameter_one,
					event.parameter_two
				)) {
					return false;
				}
				state.delay = event.delay;
				break;
			case 0xECU:
				if(track.channel != 0xFFU && !tsf_channel_set_presetnumber(
					synth.get(), channel, event.parameter_one,
					track.channel == 9U ? 1 : 0
				)) {
					return false;
				}
				state.delay = event.delay;
				break;
			case 0xEEU:
				if(track.channel != 0xFFU && !tsf_channel_set_pitchwheel(
					synth.get(), channel,
					static_cast<int>(event.parameter_one) +
						(static_cast<int>(event.parameter_two) << 7)
				)) {
					return false;
				}
				state.delay = event.delay;
				break;
			case 0xF9U:
				if(state.loop_count >= state.loops.size()) {
					return false;
				}
				state.loops[state.loop_count++] = {
					.start = state.event,
					.completed = 1U,
				};
				break;
			case 0xF8U:
				if(state.loop_count == 0U) {
					return false;
				}
				{
					auto &loop = state.loops[state.loop_count - 1U];
					// MMD stores the total iteration count in F8's delay byte.
					// Zero denotes the source song's infinite outer loop.
					if(event.delay == 0U) {
						state.event = loop.start;
					} else if(loop.completed < event.delay) {
						++loop.completed;
						state.event = loop.start;
					} else {
						--state.loop_count;
					}
				}
				break;
			case 0xFEU:
				state.ended = true;
				break;
			default:
				return false;
			}
		}
		return state.ended || state.delay != 0U;
	}

	[[nodiscard]] bool advance_sequence_tick() noexcept
	{
		for(std::size_t track_index = 0U; track_index < tracks.size();
			++track_index) {
			auto &state = tracks[track_index];
			for(std::size_t note = 0U; note < state.note_lengths.size(); ++note) {
				auto &length = state.note_lengths[note];
				if(length == 0U) {
					continue;
				}
				--length;
				if(length == 0U) {
					note_off(track_index, note);
				}
			}
			if(state.ended) {
				continue;
			}
			if(state.delay != 0U) {
				--state.delay;
			}
			if(state.delay == 0U && !process_due_events(track_index)) {
				return false;
			}
		}
		++elapsed_ticks;
		if(std::all_of(tracks.begin(), tracks.end(), [](const auto &track) {
			return track.ended;
		})) {
			playing = false;
			tsf_note_off_all(synth.get());
		}
		return true;
	}

	void advance_fade() noexcept
	{
		if(!playing || paused || fade_speed == 0) {
			return;
		}
		const auto next = static_cast<int>(fade_volume) + fade_speed;
		if(next <= 0) {
			fade_volume = 0U;
			fade_speed = 0;
		} else if(next >= 255) {
			fade_volume = 255U;
			fade_speed = 0;
		} else {
			fade_volume = static_cast<std::uint8_t>(next);
		}
		apply_gain();
	}
};

std::expected<MmdSoundfontEngine, MmdSoundfontError>
MmdSoundfontEngine::create(MmdSoundfontConfig config)
{
	if(config.soundfont.empty() || config.soundfont.size() >
		static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
		config.sample_rate_hz == 0U || config.sample_rate_hz > 384'000U) {
		return std::unexpected(error(MmdSoundfontErrorCode::invalid_config));
	}
	auto *const loaded = tsf_load_memory(
		config.soundfont.data(), static_cast<int>(config.soundfont.size())
	);
	if(loaded == nullptr) {
		return std::unexpected(error(MmdSoundfontErrorCode::invalid_soundfont));
	}
	std::unique_ptr<tsf, Impl::SoundfontCloser> owner(loaded);
	try {
		auto impl = std::make_unique<Impl>();
		impl->synth = std::move(owner);
		impl->sample_rate = config.sample_rate_hz;
		tsf_set_output(
			impl->synth.get(), TSF_STEREO_INTERLEAVED,
			static_cast<int>(config.sample_rate_hz), 0.0F
		);
		impl->reset_synth();
		return MmdSoundfontEngine(std::move(impl));
	} catch(const std::bad_alloc &) {
		return std::unexpected(error(MmdSoundfontErrorCode::allocation_failed));
	}
}

MmdSoundfontEngine::MmdSoundfontEngine(std::unique_ptr<Impl> impl) noexcept :
	impl_(std::move(impl))
{}

MmdSoundfontEngine::MmdSoundfontEngine(MmdSoundfontEngine &&) noexcept = default;
MmdSoundfontEngine &MmdSoundfontEngine::operator=(MmdSoundfontEngine &&) noexcept =
	default;
MmdSoundfontEngine::~MmdSoundfontEngine() = default;

std::expected<void, MmdSoundfontError> MmdSoundfontEngine::load_song(
	std::span<const std::byte> song
)
{
	try {
		auto parsed = parse_song(song);
		if(!parsed) {
			return std::unexpected(parsed.error());
		}
		impl_->prepared = std::move(*parsed);
		impl_->playing = false;
		impl_->paused = false;
		impl_->elapsed_ticks = 0U;
		impl_->fade_volume = 0U;
		impl_->fade_speed = 0;
		impl_->reset_synth();
		return {};
	} catch(const std::bad_alloc &) {
		return std::unexpected(error(MmdSoundfontErrorCode::allocation_failed));
	}
}

std::expected<void, MmdSoundfontError> MmdSoundfontEngine::play()
{
	if(!impl_->prepared) {
		return std::unexpected(error(MmdSoundfontErrorCode::invalid_song));
	}
	impl_->tracks = {};
	impl_->tempo_phase = 0U;
	impl_->elapsed_ticks = 0U;
	impl_->tempo_multiplier = static_cast<std::uint8_t>(tempo_scale);
	impl_->fade_volume = 0U;
	impl_->fade_speed = 0;
	impl_->playing = true;
	impl_->paused = false;
	impl_->reset_synth();
	for(std::size_t track = 0U; track < impl_->tracks.size(); ++track) {
		if(!impl_->process_due_events(track)) {
			impl_->playing = false;
			return std::unexpected(error(MmdSoundfontErrorCode::synth_failed));
		}
	}
	return {};
}

void MmdSoundfontEngine::stop() noexcept
{
	impl_->playing = false;
	impl_->paused = false;
	impl_->fade_volume = 255U;
	impl_->fade_speed = 0;
	tsf_channel_sounds_off_all(impl_->synth.get(), 0);
	for(int channel = 1; channel < static_cast<int>(midi_channel_count); ++channel) {
		tsf_channel_sounds_off_all(impl_->synth.get(), channel);
	}
	impl_->apply_gain();
}

void MmdSoundfontEngine::pause() noexcept
{
	if(impl_->playing) {
		impl_->paused = true;
	}
}

void MmdSoundfontEngine::resume() noexcept
{
	if(impl_->playing) {
		impl_->paused = false;
	}
}

void MmdSoundfontEngine::fade(std::int8_t speed) noexcept
{
	if(impl_->playing) {
		impl_->fade_speed = speed;
	}
}

void MmdSoundfontEngine::set_volume(std::uint8_t volume) noexcept
{
	if(impl_->playing) {
		impl_->fade_volume = volume;
		impl_->fade_speed = 0;
		impl_->apply_gain();
	}
}

std::expected<void, MmdSoundfontError> MmdSoundfontEngine::set_user_volume(
	std::uint8_t percent
) noexcept
{
	if(percent > 100U || (percent % 5U) != 0U) {
		return std::unexpected(error(MmdSoundfontErrorCode::invalid_config));
	}
	impl_->music_volume_percent = percent;
	impl_->apply_gain();
	return {};
}

std::expected<void, MmdSoundfontError> MmdSoundfontEngine::render_mix(
	std::span<PmdCanonicalStereoSample> output
)
{
	if(output.empty() || !impl_->playing || impl_->paused) {
		return {};
	}
	try {
		impl_->scratch.resize(output.size() * 2U);
	} catch(const std::bad_alloc &) {
		return std::unexpected(error(MmdSoundfontErrorCode::allocation_failed));
	}
	auto output_position = std::size_t{0U};
	while(output_position < output.size() && impl_->playing) {
		const auto increment = impl_->tempo_increment();
		const auto threshold = impl_->tempo_threshold();
		if(increment == 0U || impl_->tempo_phase >= threshold) {
			return std::unexpected(error(MmdSoundfontErrorCode::invalid_song));
		}
		const auto until_tick = (threshold - impl_->tempo_phase + increment - 1U) /
			increment;
		const auto frames = static_cast<std::size_t>(std::min<std::uint64_t>(
			until_tick, output.size() - output_position
		));
		tsf_render_short(
			impl_->synth.get(), impl_->scratch.data() + (output_position * 2U),
			static_cast<int>(frames), 0
		);
		impl_->tempo_phase += increment * frames;
		output_position += frames;
		if(impl_->tempo_phase >= threshold) {
			impl_->tempo_phase -= threshold;
			if(!impl_->advance_sequence_tick()) {
				return std::unexpected(error(MmdSoundfontErrorCode::synth_failed));
			}
		}
	}
	if(output_position < output.size()) {
		std::fill(
			impl_->scratch.begin() + static_cast<std::ptrdiff_t>(output_position * 2U),
			impl_->scratch.end(),
			std::int16_t{0}
		);
	}
	for(std::size_t index = 0U; index < output.size(); ++index) {
		output[index].left = mix_sample(output[index].left, impl_->scratch[index * 2U]);
		output[index].right = mix_sample(
			output[index].right, impl_->scratch[(index * 2U) + 1U]
		);
	}
	impl_->advance_fade();
	return {};
}

MmdSoundfontStatus MmdSoundfontEngine::status() const noexcept
{
	return {
		.song_loaded = impl_->prepared.has_value(),
		.playing = impl_->playing,
		.paused = impl_->paused,
		.measure = static_cast<std::uint16_t>(std::min<std::uint64_t>(
			impl_->elapsed_ticks / mmd_ticks_per_measure,
			std::numeric_limits<std::uint16_t>::max()
		)),
		.fade_volume = impl_->fade_volume,
		.fade_speed = impl_->fade_speed,
		.music_volume_percent = impl_->music_volume_percent,
	};
}

} // namespace th03::audio
