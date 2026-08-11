#include "th03/app/classic_audio_session.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace th03::app {
namespace {

constexpr std::size_t native_chunk_frames = 1'024U;
constexpr std::size_t canonical_chunk_frames = 1'024U;

[[nodiscard]] ClassicAudioSessionError session_error(
	ClassicAudioSessionErrorCode code,
	ClassicAudioCommand command = {}
) noexcept
{
	return {.code = code, .command = command};
}

[[nodiscard]] ClassicAudioSessionError resource_error(
	ClassicAudioCommand command,
	ResourceLoadError detail
) noexcept
{
	auto result = session_error(
		ClassicAudioSessionErrorCode::resource_load_failed, command
	);
	result.resource_error = detail;
	return result;
}

[[nodiscard]] ClassicAudioSessionError engine_error(
	ClassicAudioCommand command,
	audio::PmdOpnaError detail
) noexcept
{
	auto result = session_error(
		ClassicAudioSessionErrorCode::engine_failed, command
	);
	result.engine_error = detail;
	return result;
}

[[nodiscard]] ClassicAudioSessionError service_error(
	ClassicAudioCommand command,
	audio::Th03PmdServiceError detail
) noexcept
{
	auto result = session_error(
		ClassicAudioSessionErrorCode::service_failed, command
	);
	result.service_error = detail;
	return result;
}

[[nodiscard]] std::expected<MainResourceKey, ClassicAudioSessionError>
song_key(ClassicAudioCommand command) noexcept
{
	if(command.type != ClassicAudioCommandType::load_song) {
		return std::unexpected(session_error(
			ClassicAudioSessionErrorCode::invalid_cue, command
		));
	}
	if(command.cue != ClassicMusicCue::music_room_track &&
		command.parameter != 0U) {
		return std::unexpected(session_error(
			ClassicAudioSessionErrorCode::invalid_parameter, command
		));
	}
	switch(command.cue) {
	case ClassicMusicCue::menu_main:
		return MainResourceKey{.role = MainResourceRole::op_menu_music};
	case ClassicMusicCue::character_select:
		return MainResourceKey{.role = MainResourceRole::selection_music};
	case ClassicMusicCue::music_room_track:
		if(command.parameter >= static_cast<std::uint16_t>(
			static_cast<std::uint8_t>(MainMusicRoomTrack::high_score)
		)) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::invalid_parameter, command
			));
		}
		return MainResourceKey{
			.role = MainResourceRole::op_music_room_track,
			.music_room_track = static_cast<MainMusicRoomTrack>(
				command.parameter + 1U
			),
		};
	case ClassicMusicCue::winner:
		return MainResourceKey{.role = MainResourceRole::win_music};
	case ClassicMusicCue::game_over:
		return MainResourceKey{.role = MainResourceRole::gameover_music};
	case ClassicMusicCue::staff:
		return MainResourceKey{.role = MainResourceRole::staff_music};
	case ClassicMusicCue::high_score:
		return MainResourceKey{.role = MainResourceRole::registration_music};
	case ClassicMusicCue::cutscene_one:
	case ClassicMusicCue::cutscene_two:
	case ClassicMusicCue::cutscene_three:
	case ClassicMusicCue::cutscene_four:
	case ClassicMusicCue::cutscene_five:
		return MainResourceKey{
			.role = MainResourceRole::cutscene_music,
			.cutscene_music = static_cast<MainCutsceneMusic>(
				static_cast<std::uint8_t>(command.cue) -
				static_cast<std::uint8_t>(ClassicMusicCue::cutscene_one) + 1U
			),
		};
	case ClassicMusicCue::none:
		break;
	}
	return std::unexpected(session_error(
		ClassicAudioSessionErrorCode::invalid_cue, command
	));
}

[[nodiscard]] bool same_song_identity(
	const ClassicAudioCommand &left,
	const ClassicAudioCommand &right
) noexcept
{
	return left.cue == right.cue && left.parameter == right.parameter;
}

[[nodiscard]] bool valid_music_cue(ClassicMusicCue cue) noexcept
{
	switch(cue) {
	case ClassicMusicCue::none:
	case ClassicMusicCue::menu_main:
	case ClassicMusicCue::character_select:
	case ClassicMusicCue::music_room_track:
	case ClassicMusicCue::winner:
	case ClassicMusicCue::game_over:
	case ClassicMusicCue::staff:
	case ClassicMusicCue::high_score:
	case ClassicMusicCue::cutscene_one:
	case ClassicMusicCue::cutscene_two:
	case ClassicMusicCue::cutscene_three:
	case ClassicMusicCue::cutscene_four:
	case ClassicMusicCue::cutscene_five:
		return true;
	}
	return false;
}

[[nodiscard]] bool current_cue_or_none(
	ClassicMusicCue cue,
	const std::optional<ClassicAudioCommand> &loaded
) noexcept
{
	return cue == ClassicMusicCue::none || (loaded && cue == loaded->cue);
}

[[nodiscard]] constexpr std::uint64_t divide_round_up(
	std::uint64_t numerator,
	std::uint64_t denominator
) noexcept
{
	return (numerator / denominator) +
		((numerator % denominator) == 0U ? 0U : 1U);
}

} // namespace

struct ClassicAudioSession::Impl final {
	MainResourceLoader loader;
	audio::PmdOpnaEngine engine;
	audio::Th03SoundEffectScheduler effects;
	audio::PmdFixedResampler resampler{};
	RationalTickRate tick_rate{};
	std::uint64_t native_frames_numerator = 0U;
	std::uint64_t native_fraction = 0U;
	std::uint64_t tick = 0U;
	bool bgm_active = true;
	std::optional<ClassicAudioCommand> loaded{};
	std::vector<audio::PmdCanonicalStereoSample> output{};
	std::array<audio::PmdOpnaStereoSample, native_chunk_frames> native_scratch{};
	std::array<audio::PmdCanonicalStereoSample, canonical_chunk_frames>
		canonical_scratch{};

	Impl(
		MainResourceLoader loader_,
		audio::PmdOpnaEngine engine_,
		RationalTickRate tick_rate_,
		std::uint64_t native_frames_numerator_,
		bool bgm_active_
	) :
		loader(std::move(loader_)),
		engine(std::move(engine_)),
		effects(bgm_active_),
		tick_rate(tick_rate_),
		native_frames_numerator(native_frames_numerator_),
		bgm_active(bgm_active_)
	{}
};

std::expected<std::unique_ptr<ClassicAudioSession>, ClassicAudioSessionError>
ClassicAudioSession::create(ClassicAudioSessionConfig config)
{
	if(config.data_root.empty() || config.tick_rate.ticks == 0U ||
		config.tick_rate.seconds == 0U) {
		return std::unexpected(session_error(
			ClassicAudioSessionErrorCode::invalid_config
		));
	}
	const auto numerator = static_cast<std::uint64_t>(
		audio::pmd_native_sample_rate_hz
	) * config.tick_rate.seconds;
	const auto maximum_tick_frames = divide_round_up(
		numerator, config.tick_rate.ticks
	);
	if(maximum_tick_frames > classic_audio_maximum_native_frames_per_tick) {
		return std::unexpected(session_error(
			ClassicAudioSessionErrorCode::invalid_config
		));
	}

	auto engine = audio::PmdOpnaEngine::create({
		.input_clock_hz = classic_opna_clock_hz,
		.fidelity = audio::PmdOpnaFidelity::minimum,
		.rhythm_rom = config.rhythm_rom,
	});
	if(!engine) {
		return std::unexpected(engine_error({}, engine.error()));
	}
	MainResourceLoader loader(std::move(config.data_root), config.integrity);
	const ClassicAudioCommand load_effects{
		.type = ClassicAudioCommandType::load_effects,
	};
	auto bank = loader.load({.role = MainResourceRole::audio_effect_bank});
	if(!bank) {
		return std::unexpected(resource_error(load_effects, bank.error()));
	}
	auto loaded_bank = engine->load_effect_bank(**bank);
	if(!loaded_bank) {
		return std::unexpected(engine_error(load_effects, loaded_bank.error()));
	}

	try {
		auto impl = std::make_unique<Impl>(
			std::move(loader), std::move(*engine), config.tick_rate, numerator,
			config.bgm_active
		);
		const auto initial_capacity = divide_round_up(
			(maximum_tick_frames + audio::pmd_resampler_lookahead_samples + 1U) *
				audio::pmd_canonical_sample_rate_hz,
			audio::pmd_native_sample_rate_hz
		);
		impl->output.reserve(static_cast<std::size_t>(initial_capacity));
		return std::unique_ptr<ClassicAudioSession>(
			new ClassicAudioSession(std::move(impl))
		);
	} catch(const std::bad_alloc &) {
		return std::unexpected(session_error(
			ClassicAudioSessionErrorCode::allocation_failed
		));
	}
}

ClassicAudioSession::ClassicAudioSession(std::unique_ptr<Impl> impl) noexcept :
	impl_(std::move(impl))
{}

ClassicAudioSession::~ClassicAudioSession() = default;

std::expected<void, ClassicAudioSessionError> ClassicAudioSession::apply(
	ClassicAudioCommand command
)
{
	switch(command.type) {
	case ClassicAudioCommandType::load_song: {
		auto key = song_key(command);
		if(!key) {
			return std::unexpected(key.error());
		}
		auto resource = impl_->loader.load(*key);
		if(!resource) {
			return std::unexpected(resource_error(command, resource.error()));
		}
		auto loaded = impl_->engine.load_song(**resource);
		if(!loaded) {
			return std::unexpected(engine_error(command, loaded.error()));
		}
		impl_->loaded = command;
		return {};
	}
	case ClassicAudioCommandType::play_song:
		if(!impl_->loaded || command.cue == ClassicMusicCue::none ||
			!same_song_identity(command, *impl_->loaded)) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::command_order, command
			));
		}
		if(!impl_->bgm_active) {
			// snd_kaja_interrupt() makes Play a no-op when the fresh MAIN or
			// MAINL process skipped sound-mode detection for Music Off.
			return {};
		}
		if(auto played = impl_->engine.play(); !played) {
			return std::unexpected(engine_error(command, played.error()));
		}
		return {};
	case ClassicAudioCommandType::stop_song:
		if(command.parameter != 0U || !valid_music_cue(command.cue) ||
			!current_cue_or_none(command.cue, impl_->loaded)) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::invalid_parameter, command
			));
		}
		if(impl_->bgm_active) {
			impl_->engine.stop();
		}
		return {};
	case ClassicAudioCommandType::fade_song:
		if(command.parameter == 0U || command.parameter > 0x7FU ||
			!valid_music_cue(command.cue) ||
			!current_cue_or_none(command.cue, impl_->loaded)) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::invalid_parameter, command
			));
		}
		if(impl_->bgm_active) {
			impl_->engine.fade(std::bit_cast<std::int8_t>(
				static_cast<std::uint8_t>(command.parameter)
			));
		}
		return {};
	case ClassicAudioCommandType::load_effects: {
		if(command.cue != ClassicMusicCue::none || command.parameter != 0U) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::invalid_parameter, command
			));
		}
		auto resource = impl_->loader.load({
			.role = MainResourceRole::audio_effect_bank,
		});
		if(!resource) {
			return std::unexpected(resource_error(command, resource.error()));
		}
		auto loaded = impl_->engine.load_effect_bank(**resource);
		if(!loaded) {
			return std::unexpected(engine_error(command, loaded.error()));
		}
		impl_->effects.reset();
		return {};
	}
	case ClassicAudioCommandType::play_effect:
		if(command.cue != ClassicMusicCue::none ||
			command.parameter < audio::th03_sound_effect_first ||
			command.parameter > audio::th03_sound_effect_last) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::invalid_parameter, command
			));
		}
		if(!impl_->bgm_active) {
			// snd_se_play_force() collapses to no work when the fresh MAIN or
			// MAINL process did not determine an FM source.
			return {};
		}
		if(!impl_->engine.status().song_loaded ||
			!impl_->engine.status().effect_bank_loaded) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::command_order, command
			));
		}
		if(auto forced = impl_->effects.force(
			static_cast<std::uint8_t>(command.parameter)
		); !forced) {
			return std::unexpected(service_error(command, forced.error()));
		} else if(*forced) {
			auto played = impl_->engine.play_effect((*forced)->effect_id);
			if(!played) {
				return std::unexpected(engine_error(command, played.error()));
			}
		}
		return {};
	case ClassicAudioCommandType::set_bgm_active:
		if(command.cue != ClassicMusicCue::none || command.parameter > 1U) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::invalid_parameter, command
			));
		}
		set_bgm_active(command.parameter != 0U);
		return {};
	}
	return std::unexpected(session_error(
		ClassicAudioSessionErrorCode::invalid_parameter, command
	));
}

std::expected<bool, ClassicAudioSessionError>
ClassicAudioSession::request_effect(std::uint8_t effect_id) noexcept
{
	const ClassicAudioCommand command{
		.type = ClassicAudioCommandType::play_effect,
		.parameter = effect_id,
	};
	if(const auto rule = audio::th03_sound_effect_rule(effect_id); !rule) {
		return std::unexpected(service_error(command, rule.error()));
	}
	if(!impl_->bgm_active) {
		return false;
	}
	const auto engine_status = impl_->engine.status();
	if(!engine_status.song_loaded || !engine_status.effect_bank_loaded) {
		return std::unexpected(session_error(
			ClassicAudioSessionErrorCode::command_order, command
		));
	}
	auto requested = impl_->effects.request(effect_id);
	if(!requested) {
		return std::unexpected(service_error(command, requested.error()));
	}
	return *requested;
}

void ClassicAudioSession::set_bgm_active(bool active) noexcept
{
	impl_->bgm_active = active;
	impl_->effects.set_active(active);
	if(!active) {
		impl_->effects.reset();
		impl_->engine.stop_effect();
	}
}

std::expected<ClassicAudioTick, ClassicAudioSessionError>
ClassicAudioSession::advance_tick()
{
	const auto accumulated = impl_->native_fraction +
		impl_->native_frames_numerator;
	const auto frames_u64 = accumulated / impl_->tick_rate.ticks;
	const auto next_fraction = accumulated % impl_->tick_rate.ticks;
	if(frames_u64 > classic_audio_maximum_native_frames_per_tick) {
		return std::unexpected(session_error(
			ClassicAudioSessionErrorCode::invalid_config
		));
	}
	const auto frames = static_cast<std::size_t>(frames_u64);
	const auto required_capacity = divide_round_up(
		(frames_u64 + audio::pmd_resampler_lookahead_samples + 1U) *
			audio::pmd_canonical_sample_rate_hz,
		audio::pmd_native_sample_rate_hz
	);
	try {
		impl_->output.clear();
		impl_->output.reserve(static_cast<std::size_t>(required_capacity));
	} catch(const std::bad_alloc &) {
		return std::unexpected(session_error(
			ClassicAudioSessionErrorCode::allocation_failed
		));
	}

	std::optional<std::uint8_t> started{};
	if(const auto effect = impl_->effects.update()) {
		auto played = impl_->engine.play_effect(effect->effect_id);
		if(!played) {
			return std::unexpected(engine_error(
				ClassicAudioCommand{
					.type = ClassicAudioCommandType::play_effect,
					.parameter = effect->effect_id,
				},
				played.error()
			));
		}
		started = effect->effect_id;
	}

	std::size_t remaining = frames;
	while(remaining != 0U) {
		const auto native_count = std::min(remaining, impl_->native_scratch.size());
		impl_->engine.render_native(std::span{
			impl_->native_scratch.data(), native_count
		});
		std::span<const audio::PmdOpnaStereoSample> input{
			impl_->native_scratch.data(), native_count
		};
		while(!input.empty()) {
			auto progress = impl_->resampler.process(input, impl_->canonical_scratch);
			if(!progress) {
				auto error = session_error(
					ClassicAudioSessionErrorCode::resampler_failed
				);
				error.resampler_error = progress.error();
				return std::unexpected(error);
			}
			if(progress->input_consumed == 0U &&
				progress->output_written == 0U) {
				return std::unexpected(session_error(
					ClassicAudioSessionErrorCode::resampler_failed
				));
			}
			impl_->output.insert(
				impl_->output.end(),
				impl_->canonical_scratch.begin(),
				impl_->canonical_scratch.begin() +
					static_cast<std::ptrdiff_t>(progress->output_written)
			);
			input = input.subspan(progress->input_consumed);
		}
		remaining -= native_count;
	}

	impl_->native_fraction = next_fraction;
	const auto result = ClassicAudioTick{
		.tick_index = impl_->tick,
		.native_frames = frames,
		.canonical_frames = impl_->output,
		.effect_started = started,
		.status = status(),
	};
	++impl_->tick;
	return result;
}

ClassicAudioStatus ClassicAudioSession::status() const noexcept
{
	const auto status = impl_->engine.status();
	return {
		// snd_bgm_active() reports snd_active, not Play/Stop state.
		.bgm_active = impl_->bgm_active,
		.measure = status.measure,
		.volume = status.fade_volume,
	};
}

std::uint64_t ClassicAudioSession::tick_index() const noexcept
{
	return impl_->tick;
}

std::optional<ClassicAudioCommand> ClassicAudioSession::loaded_song() const noexcept
{
	return impl_->loaded;
}

} // namespace th03::app
