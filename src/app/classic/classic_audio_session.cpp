#include "th03/app/classic/classic_audio_session.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <limits>
#include <new>
#include <string_view>
#include <utility>
#include <vector>

namespace th03::app {
namespace {

constexpr std::size_t native_chunk_frames = 1'024U;
constexpr std::size_t canonical_chunk_frames = 1'024U;
constexpr std::uintmax_t maximum_soundfont_bytes = 64U * 1024U * 1024U;
constexpr std::uintmax_t maximum_mmd_song_bytes = 64U * 1024U;
constexpr std::array<std::string_view, 21U> required_mmd_filenames{{
	"00MM.MMD", "01MM.MMD", "02MM.MMD", "03MM.MMD", "04MM.MMD",
	"05MM.MMD", "06MM.MMD", "07MM.MMD", "08MM.MMD", "DEC.MMD",
	"DEMO1.MMD", "DEMO2.MMD", "DEMO3.MMD", "DEMO4.MMD", "DEMO5.MMD",
	"ED.MMD", "OP.MMD", "OVER.MMD", "SCORE.MMD", "SELECT.MMD", "WIN.MMD",
}};

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

[[nodiscard]] ClassicAudioSessionError midi_engine_error(
	ClassicAudioCommand command,
	audio::MmdSoundfontError detail
) noexcept
{
	auto result = session_error(
		ClassicAudioSessionErrorCode::engine_failed, command
	);
	result.midi_engine_error = detail;
	return result;
}

[[nodiscard]] std::expected<std::vector<std::byte>, ClassicAudioSessionError>
read_midi_file(
	const std::filesystem::path &path,
	std::uintmax_t maximum_bytes,
	ClassicAudioCommand command = {}
)
{
	std::error_code status_error{};
	const auto size = std::filesystem::file_size(path, status_error);
	if(status_error || size == 0U || size > maximum_bytes ||
		size > static_cast<std::uintmax_t>(
			std::numeric_limits<std::streamsize>::max()
		)) {
		return std::unexpected(session_error(
			ClassicAudioSessionErrorCode::resource_load_failed, command
		));
	}
	try {
		std::vector<std::byte> result(static_cast<std::size_t>(size));
		std::ifstream input(path, std::ios::binary);
		if(!input || !input.read(
			reinterpret_cast<char *>(result.data()),
			static_cast<std::streamsize>(result.size())
		) || input.peek() != std::ifstream::traits_type::eof()) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::resource_load_failed, command
			));
		}
		return result;
	} catch(const std::bad_alloc &) {
		return std::unexpected(session_error(
			ClassicAudioSessionErrorCode::allocation_failed, command
		));
	}
}

[[nodiscard]] bool mmd_corpus_present(
	const std::filesystem::path &root
) noexcept
{
	for(const auto filename : required_mmd_filenames) {
		std::error_code status_error{};
		if(!std::filesystem::is_regular_file(root / filename, status_error) ||
			status_error) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] std::expected<std::string_view, ClassicAudioSessionError>
mmd_filename(ClassicAudioCommand command) noexcept
{
	static constexpr std::array<std::string_view, 18U> music_room{{
		"OP.MMD", "SELECT.MMD", "00MM.MMD", "01MM.MMD", "02MM.MMD",
		"03MM.MMD", "04MM.MMD", "05MM.MMD", "06MM.MMD", "DEC.MMD",
		"07MM.MMD", "08MM.MMD", "DEMO1.MMD", "DEMO2.MMD",
		"DEMO3.MMD", "DEMO4.MMD", "DEMO5.MMD", "ED.MMD",
	}};
	if(command.type != ClassicAudioCommandType::load_song) {
		return std::unexpected(session_error(
			ClassicAudioSessionErrorCode::invalid_cue, command
		));
	}
	switch(command.cue) {
	case ClassicMusicCue::menu_main: return "OP.MMD";
	case ClassicMusicCue::character_select: return "SELECT.MMD";
	case ClassicMusicCue::gameplay: {
		static constexpr std::array<std::string_view, 10U> gameplay{{
			"00MM.MMD", "01MM.MMD", "02MM.MMD", "03MM.MMD", "04MM.MMD",
			"05MM.MMD", "06MM.MMD", "DEC.MMD", "07MM.MMD", "08MM.MMD",
		}};
		if(command.parameter >= gameplay.size()) {
			break;
		}
		return gameplay[command.parameter];
	}
	case ClassicMusicCue::music_room_track:
		if(command.parameter < music_room.size()) {
			return music_room[command.parameter];
		}
		break;
	case ClassicMusicCue::winner: return "WIN.MMD";
	case ClassicMusicCue::game_over: return "OVER.MMD";
	case ClassicMusicCue::staff: return "ED.MMD";
	case ClassicMusicCue::high_score: return "SCORE.MMD";
	case ClassicMusicCue::cutscene_one: return "DEMO1.MMD";
	case ClassicMusicCue::cutscene_two: return "DEMO2.MMD";
	case ClassicMusicCue::cutscene_three: return "DEMO3.MMD";
	case ClassicMusicCue::cutscene_four: return "DEMO4.MMD";
	case ClassicMusicCue::cutscene_five: return "DEMO5.MMD";
	case ClassicMusicCue::none: break;
	}
	return std::unexpected(session_error(
		ClassicAudioSessionErrorCode::invalid_parameter, command
	));
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
		command.cue != ClassicMusicCue::gameplay && command.parameter != 0U) {
		return std::unexpected(session_error(
			ClassicAudioSessionErrorCode::invalid_parameter, command
		));
	}
	switch(command.cue) {
	case ClassicMusicCue::menu_main:
		return MainResourceKey{.role = MainResourceRole::op_menu_music};
	case ClassicMusicCue::character_select:
		return MainResourceKey{.role = MainResourceRole::selection_music};
	case ClassicMusicCue::gameplay:
		if(command.parameter > static_cast<std::uint16_t>(
			static_cast<std::uint8_t>(ClassicGameplayMusic::stage_08)
		)) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::invalid_parameter, command
			));
		}
		return MainResourceKey{
			.role = MainResourceRole::op_music_room_track,
			.music_room_track = static_cast<MainMusicRoomTrack>(
				static_cast<std::uint8_t>(MainMusicRoomTrack::stage_00) +
				static_cast<std::uint8_t>(command.parameter)
			),
		};
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
	case ClassicMusicCue::gameplay:
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
	std::optional<audio::MmdSoundfontEngine> midi{};
	audio::Th03SoundEffectScheduler effects;
	audio::PmdFixedResampler resampler{};
	RationalTickRate tick_rate{};
	std::uint64_t native_frames_numerator = 0U;
	std::uint64_t native_fraction = 0U;
	std::uint64_t tick = 0U;
	ClassicBgmMode bgm_mode = ClassicBgmMode::fm;
	bool effects_active = true;
	bool effects_follow_bgm = true;
	std::uint8_t music_volume_percent = 100U;
	std::uint8_t effect_volume_percent = 100U;
	std::optional<ClassicAudioCommand> loaded{};
	std::filesystem::path midi_root{};
	std::vector<audio::PmdCanonicalStereoSample> output{};
	std::array<audio::PmdOpnaStereoSample, native_chunk_frames> native_scratch{};
	std::array<audio::PmdCanonicalStereoSample, canonical_chunk_frames>
		canonical_scratch{};

	Impl(
		MainResourceLoader loader_,
		audio::PmdOpnaEngine engine_,
		std::optional<audio::MmdSoundfontEngine> midi_,
		std::filesystem::path midi_root_,
		RationalTickRate tick_rate_,
		std::uint64_t native_frames_numerator_,
		ClassicBgmMode bgm_mode_,
		bool effects_active_,
		bool effects_follow_bgm_,
		std::uint8_t music_volume_percent_,
		std::uint8_t effect_volume_percent_
	) :
		loader(std::move(loader_)),
		engine(std::move(engine_)),
		midi(std::move(midi_)),
		effects(effects_active_ &&
			(!effects_follow_bgm_ || bgm_mode_ != ClassicBgmMode::off)),
		tick_rate(tick_rate_),
		native_frames_numerator(native_frames_numerator_),
		bgm_mode(bgm_mode_),
		effects_active(effects_active_),
		effects_follow_bgm(effects_follow_bgm_),
		music_volume_percent(music_volume_percent_),
		effect_volume_percent(effect_volume_percent_),
		midi_root(std::move(midi_root_))
	{}

	[[nodiscard]] bool effects_available() const noexcept
	{
		return effects_active &&
			(!effects_follow_bgm || bgm_mode != ClassicBgmMode::off);
	}
};

std::expected<std::unique_ptr<ClassicAudioSession>, ClassicAudioSessionError>
ClassicAudioSession::create(ClassicAudioSessionConfig config)
{
	if(config.data_root.empty() || config.tick_rate.ticks == 0U ||
		config.tick_rate.seconds == 0U || config.music_volume_percent > 100U ||
		config.effect_volume_percent > 100U ||
		(config.music_volume_percent % 5U) != 0U ||
		(config.effect_volume_percent % 5U) != 0U) {
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
		.rhythm_pcm = config.rhythm_pcm,
	});
	if(!engine) {
		return std::unexpected(engine_error({}, engine.error()));
	}
	auto data_root = config.data_root;
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
	const auto initial_mode = config.bgm_active ?
		(config.midi_bgm ? ClassicBgmMode::midi : ClassicBgmMode::fm) :
		ClassicBgmMode::off;
	if(auto volumes = engine->set_user_volumes(
		initial_mode == ClassicBgmMode::midi ? 0U :
			config.music_volume_percent,
		config.effect_volume_percent
	); !volumes) {
		return std::unexpected(engine_error({}, volumes.error()));
	}

	std::optional<audio::MmdSoundfontEngine> midi{};
	auto soundfont_path = std::move(config.midi_soundfont);
	if(soundfont_path.empty()) {
		soundfont_path = data_root / "PODDUI" / "MIDI" /
			"GeneralUser-GS-SoftSynth-1.44.sf2";
	}
	std::error_code soundfont_status_error{};
	const auto soundfont_present = std::filesystem::is_regular_file(
		soundfont_path, soundfont_status_error
	) && !soundfont_status_error;
	const auto midi_assets_present = soundfont_present &&
		mmd_corpus_present(data_root);
	if(midi_assets_present) {
		auto soundfont = read_midi_file(
			soundfont_path, maximum_soundfont_bytes
		);
		if(!soundfont) {
			return std::unexpected(soundfont.error());
		}
		auto created_midi = audio::MmdSoundfontEngine::create({
			.soundfont = *soundfont,
		});
		if(!created_midi) {
			return std::unexpected(midi_engine_error({}, created_midi.error()));
		}
		if(auto volume = created_midi->set_user_volume(
			config.music_volume_percent
		); !volume) {
			return std::unexpected(midi_engine_error({}, volume.error()));
		}
		midi.emplace(std::move(*created_midi));
	} else if(initial_mode == ClassicBgmMode::midi) {
		return std::unexpected(session_error(
			ClassicAudioSessionErrorCode::resource_load_failed
		));
	}

	try {
		auto impl = std::make_unique<Impl>(
			std::move(loader), std::move(*engine), std::move(midi),
			std::move(data_root), config.tick_rate, numerator, initial_mode,
			config.effects_active, config.effects_follow_bgm,
			config.music_volume_percent, config.effect_volume_percent
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
		if(impl_->midi) {
			auto filename = mmd_filename(command);
			if(!filename) {
				return std::unexpected(filename.error());
			}
			auto song = read_midi_file(
				impl_->midi_root / *filename, maximum_mmd_song_bytes, command
			);
			if(!song) {
				return std::unexpected(song.error());
			}
			if(auto midi_loaded = impl_->midi->load_song(*song); !midi_loaded) {
				return std::unexpected(midi_engine_error(
					command, midi_loaded.error()
				));
			}
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
		if(impl_->bgm_mode == ClassicBgmMode::off) {
			// Replay Patch can leave the PMD effect service active while Music is
			// Off. Start the fresh carrier, then Stop only its song; PMD retains
			// Timer A/FM6 for subsequent external effects. Classic reaches the
			// original no-op through effects_follow_bgm.
			if(impl_->effects_available()) {
				if(auto played = impl_->engine.play(); !played) {
					return std::unexpected(engine_error(command, played.error()));
				}
				impl_->engine.stop();
			}
			return {};
		}
		if(impl_->bgm_mode == ClassicBgmMode::midi) {
			if(!impl_->midi) {
				return std::unexpected(session_error(
					ClassicAudioSessionErrorCode::command_order, command
				));
			}
			if(auto carrier = impl_->engine.play(); !carrier) {
				return std::unexpected(engine_error(command, carrier.error()));
			}
			impl_->engine.stop();
			if(auto played = impl_->midi->play(); !played) {
				return std::unexpected(midi_engine_error(command, played.error()));
			}
			return {};
		}
		if(impl_->midi) {
			impl_->midi->stop();
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
		impl_->engine.stop();
		if(impl_->midi) {
			impl_->midi->stop();
		}
		return {};
	case ClassicAudioCommandType::pause_song:
	case ClassicAudioCommandType::resume_song:
		if(command.cue != ClassicMusicCue::none || command.parameter != 0U) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::invalid_parameter, command
			));
		}
		if(impl_->bgm_mode != ClassicBgmMode::off) {
			if(command.type == ClassicAudioCommandType::pause_song) {
				if(impl_->bgm_mode == ClassicBgmMode::midi && impl_->midi) {
					impl_->midi->pause();
				} else {
					impl_->engine.pause();
				}
			} else {
				if(impl_->bgm_mode == ClassicBgmMode::midi && impl_->midi) {
					impl_->midi->resume();
				} else {
					impl_->engine.resume();
				}
			}
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
		if(impl_->bgm_mode != ClassicBgmMode::off) {
			const auto speed = std::bit_cast<std::int8_t>(
				static_cast<std::uint8_t>(command.parameter)
			);
			if(impl_->bgm_mode == ClassicBgmMode::midi && impl_->midi) {
				impl_->midi->fade(speed);
			} else {
				impl_->engine.fade(speed);
			}
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
	case ClassicAudioCommandType::reset_effects:
		if(command.cue != ClassicMusicCue::none || command.parameter != 0U) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::invalid_parameter, command
			));
		}
		// snd_se_reset() only clears TH03's one-slot scheduler. It does not
		// explicitly stop the PMD effect that has already reached the driver.
		impl_->effects.reset();
		return {};
	case ClassicAudioCommandType::dispatch_effect:
		if(command.cue != ClassicMusicCue::none ||
			command.parameter < audio::th03_sound_effect_first ||
			command.parameter > audio::th03_sound_effect_last) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::invalid_parameter, command
			));
		}
		if(!impl_->effects_available()) {
			return {};
		}
		if(!impl_->engine.status().song_loaded ||
			!impl_->engine.status().effect_bank_loaded) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::command_order, command
			));
		}
		if(auto played = impl_->engine.play_effect(
			static_cast<std::uint8_t>(command.parameter)
		); !played) {
			return std::unexpected(engine_error(command, played.error()));
		}
		return {};
	case ClassicAudioCommandType::play_effect:
		if(command.cue != ClassicMusicCue::none ||
			command.parameter < audio::th03_sound_effect_first ||
			command.parameter > audio::th03_sound_effect_last) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::invalid_parameter, command
			));
		}
		if(!impl_->effects_available()) {
			// Classic collapses here when the fresh process did not determine an
			// FM source; New Classic may instead keep effects independent.
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
		if(command.cue != ClassicMusicCue::none || command.parameter >
			static_cast<std::uint16_t>(ClassicBgmMode::midi)) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::invalid_parameter, command
			));
		}
		return set_bgm_mode(static_cast<ClassicBgmMode>(command.parameter));
	case ClassicAudioCommandType::set_effects_active:
		if(command.cue != ClassicMusicCue::none || command.parameter > 1U) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::invalid_parameter, command
			));
		}
		set_effects_active(command.parameter != 0U);
		return {};
	case ClassicAudioCommandType::set_music_volume:
	case ClassicAudioCommandType::set_effect_volume: {
		if(command.cue != ClassicMusicCue::none || command.parameter > 100U ||
			(command.parameter % 5U) != 0U) {
			return std::unexpected(session_error(
				ClassicAudioSessionErrorCode::invalid_parameter, command
			));
		}
		const auto percent = static_cast<std::uint8_t>(command.parameter);
		return set_user_volumes(
			command.type == ClassicAudioCommandType::set_music_volume ? percent :
				impl_->music_volume_percent,
			command.type == ClassicAudioCommandType::set_effect_volume ? percent :
				impl_->effect_volume_percent
		);
	}
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
	if(!impl_->effects_available()) {
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
	static_cast<void>(set_bgm_mode(
		active ? ClassicBgmMode::fm : ClassicBgmMode::off
	));
}

std::expected<void, ClassicAudioSessionError> ClassicAudioSession::set_bgm_mode(
	ClassicBgmMode mode
) noexcept
{
	if(static_cast<std::uint8_t>(mode) >
		static_cast<std::uint8_t>(ClassicBgmMode::midi) ||
		(mode == ClassicBgmMode::midi && !impl_->midi)) {
		return std::unexpected(session_error(
			ClassicAudioSessionErrorCode::invalid_parameter,
			ClassicAudioCommand{
				.type = ClassicAudioCommandType::set_bgm_active,
				.parameter = static_cast<std::uint16_t>(mode),
			}
		));
	}
	impl_->bgm_mode = mode;
	if(auto volumes = impl_->engine.set_user_volumes(
		mode == ClassicBgmMode::midi ? 0U : impl_->music_volume_percent,
		impl_->effect_volume_percent
	); !volumes) {
		return std::unexpected(engine_error({}, volumes.error()));
	}
	const auto effects_available = impl_->effects_available();
	impl_->effects.set_active(effects_available);
	if(!effects_available) {
		impl_->effects.reset();
		impl_->engine.stop_effect();
	}
	return {};
}

void ClassicAudioSession::set_effects_active(bool active) noexcept
{
	impl_->effects_active = active;
	const auto effects_available = impl_->effects_available();
	impl_->effects.set_active(effects_available);
	if(!effects_available) {
		impl_->effects.reset();
		impl_->engine.stop_effect();
	}
}

std::expected<void, ClassicAudioSessionError>
ClassicAudioSession::set_user_volumes(
	std::uint8_t music_percent,
	std::uint8_t effect_percent
) noexcept
{
	const ClassicAudioCommand command{
		.type = ClassicAudioCommandType::set_music_volume,
		.parameter = music_percent,
	};
	if(music_percent > 100U || effect_percent > 100U ||
		(music_percent % 5U) != 0U || (effect_percent % 5U) != 0U) {
		return std::unexpected(session_error(
			ClassicAudioSessionErrorCode::invalid_parameter, command
		));
	}
	if(auto applied = impl_->engine.set_user_volumes(
		impl_->bgm_mode == ClassicBgmMode::midi ? 0U : music_percent,
		effect_percent
	); !applied) {
		return std::unexpected(engine_error(command, applied.error()));
	}
	impl_->music_volume_percent = music_percent;
	impl_->effect_volume_percent = effect_percent;
	if(impl_->midi) {
		if(auto applied = impl_->midi->set_user_volume(music_percent); !applied) {
			return std::unexpected(midi_engine_error(command, applied.error()));
		}
	}
	return {};
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
	if(impl_->bgm_mode == ClassicBgmMode::midi && impl_->midi) {
		if(auto mixed = impl_->midi->render_mix(impl_->output); !mixed) {
			return std::unexpected(midi_engine_error({}, mixed.error()));
		}
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
	if(impl_->bgm_mode == ClassicBgmMode::midi && impl_->midi) {
		const auto status = impl_->midi->status();
		return {
			.bgm_active = true,
			.song_playing = status.playing,
			.measure = status.measure,
			.volume = status.fade_volume,
		};
	}
	const auto status = impl_->engine.status();
	return {
		// snd_bgm_active() reports snd_active, not Play/Stop state.
		.bgm_active = impl_->bgm_mode != ClassicBgmMode::off,
		.song_playing = status.playing,
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
