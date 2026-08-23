#include "th03/app/classic_audio_session.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace fs = std::filesystem;
using th03::app::ClassicAudioCommand;
using th03::app::ClassicAudioCommandType;
using th03::app::ClassicAudioSession;
using th03::app::ClassicAudioSessionConfig;
using th03::app::ClassicAudioSessionErrorCode;
using th03::app::ClassicMusicCue;
using th03::app::MainResourceArchive;
using th03::app::MainResourceDecoder;
using th03::app::MainResourceIntegrity;
using th03::app::MainResourceKey;
using th03::app::MainResourceRole;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

class TemporaryRoot final {
public:
	TemporaryRoot()
	{
		const auto nonce = std::chrono::steady_clock::now()
			.time_since_epoch().count();
		for(std::uint32_t suffix = 0U; suffix < 1000U; ++suffix) {
			path_ = fs::temp_directory_path() /
				("th03-classic-audio-" + std::to_string(nonce) + "-" +
					std::to_string(suffix));
			std::error_code error;
			if(fs::create_directory(path_, error)) {
				return;
			}
		}
		path_.clear();
	}

	~TemporaryRoot()
	{
		if(path_.empty() || path_.parent_path() != fs::temp_directory_path() ||
			path_.filename().string().rfind("th03-classic-audio-", 0U) != 0U) {
			return;
		}
		std::error_code error;
		fs::remove_all(path_, error);
	}

	[[nodiscard]] const fs::path &path() const noexcept { return path_; }
	[[nodiscard]] bool valid() const noexcept { return !path_.empty(); }

private:
	fs::path path_{};
};

void set_le16(
	std::vector<std::byte> &bytes,
	std::size_t offset,
	std::uint16_t value
)
{
	bytes[offset] = static_cast<std::byte>(value);
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

void append(std::vector<std::byte> &bytes, std::uint8_t value)
{
	bytes.push_back(static_cast<std::byte>(value));
}

// Newly authored from the public PMD header layout. Padding is inert and lets
// one generated image satisfy each exact manifest size in size-only tests.
std::vector<std::byte> synthetic_song(std::size_t size)
{
	constexpr std::size_t file_prefix = 1U;
	constexpr std::size_t header_size = 0x18U;
	std::vector<std::byte> bytes(file_prefix + header_size, std::byte{0U});
	const auto emit = [&bytes](std::initializer_list<std::uint8_t> stream) {
		const auto offset = static_cast<std::uint16_t>(bytes.size() - 1U);
		for(const auto value : stream) {
			append(bytes, value);
		}
		return offset;
	};
	const auto fm1 = emit({
		0x24U, 0x30U, 0x28U, 0x30U, 0x2BU, 0x18U, 0x2FU, 0x18U, 0x80U,
	});
	const auto ssg1 = emit({0x40U, 0x18U, 0x43U, 0x18U, 0x80U});
	const auto empty = emit({0x80U});
	const auto rhythm_table = emit({0x80U});
	const std::array<std::uint16_t, 11U> parts{
		fm1, empty, empty, empty, empty, empty,
		ssg1, empty, empty, empty, empty,
	};
	for(std::size_t part = 0U; part < parts.size(); ++part) {
		set_le16(bytes, file_prefix + (part * 2U), parts[part]);
	}
	set_le16(bytes, file_prefix + 0x16U, rhythm_table);
	bytes.resize(size, std::byte{0U});
	return bytes;
}

// Exactly 1,304 newly authored bytes: 127 bounded programs ending at offset
// 1,278, followed by one 26-byte tone. Effect 1 is audible; the long final
// program merely fills the authentic manifest extent with valid PMD notes.
std::vector<std::byte> synthetic_effect_bank()
{
	constexpr std::size_t pointer_entries = 128U;
	constexpr std::size_t tone_offset = 1'278U;
	std::vector<std::byte> bytes(pointer_entries * 2U, std::byte{0U});
	for(std::size_t effect = 0U;
		effect < th03::audio::pmd_effect_count;
		++effect) {
		set_le16(bytes, effect * 2U, static_cast<std::uint16_t>(bytes.size()));
		if(effect == 1U) {
			for(const auto value : {
				0xFFU, 0x00U, 0xFDU, 0x7FU, 0x40U,
				0x03U, 0xF3U, 0x43U, 0x02U, 0x80U,
			}) {
				append(bytes, static_cast<std::uint8_t>(value));
			}
		} else if(effect + 1U == th03::audio::pmd_effect_count) {
			while(bytes.size() + 2U < tone_offset) {
				append(bytes, 0x40U);
				append(bytes, 0x01U);
			}
			append(bytes, 0x80U);
		} else {
			append(bytes, 0x80U);
		}
	}
	set_le16(
		bytes,
		static_cast<std::size_t>(th03::audio::pmd_effect_count) * 2U,
		static_cast<std::uint16_t>(bytes.size())
	);
	expect(bytes.size() == tone_offset,
		"generated effect programs end at the exact tone boundary");
	for(const auto value : {
		0x00U,
		0x01U, 0x01U, 0x01U, 0x01U,
		0x00U, 0x00U, 0x00U, 0x00U,
		0x1FU, 0x1FU, 0x1FU, 0x1FU,
		0x00U, 0x00U, 0x00U, 0x00U,
		0x00U, 0x00U, 0x00U, 0x00U,
		0x0FU, 0x0FU, 0x0FU, 0x0FU,
		0x07U,
	}) {
		append(bytes, static_cast<std::uint8_t>(value));
	}
	expect(bytes.size() == 1'304U, "generated effect bank has manifest size");
	return bytes;
}

bool write_bytes(const fs::path &path, std::span<const std::byte> bytes)
{
	std::ofstream output(path, std::ios::binary);
	if(!output) {
		return false;
	}
	output.write(
		reinterpret_cast<const char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	return static_cast<bool>(output);
}

bool prepare_audio_root(const fs::path &root)
{
	std::error_code error;
	fs::create_directories(root / "YUME1.DAT", error);
	if(error) {
		return false;
	}
	for(const auto &spec : th03::app::main_resource_manifest()) {
		if(spec.decoder != MainResourceDecoder::pmd_music) {
			continue;
		}
		const auto directory = spec.archive == MainResourceArchive::yume1 ?
			"YUME1.DAT" : "YUME2.DAT";
		fs::create_directories(root / directory, error);
		if(error || !write_bytes(
			root / directory / spec.filename,
			synthetic_song(spec.expected_bytes)
		)) {
			return false;
		}
	}
	const auto bank = synthetic_effect_bank();
	return write_bytes(root / "YUME1.DAT" / "YUME.EFC", bank);
}

ClassicAudioSessionConfig config(const fs::path &root)
{
	return {
		.data_root = root,
		.tick_rate = {.ticks = 60U, .seconds = 1U},
		.integrity = MainResourceIntegrity::size_only,
	};
}

ClassicAudioCommand load(ClassicMusicCue cue, std::uint16_t parameter = 0U)
{
	return {
		.type = ClassicAudioCommandType::load_song,
		.cue = cue,
		.parameter = parameter,
	};
}

void test_exact_cue_mapping()
{
	TemporaryRoot root;
	expect(root.valid(), "cue-mapping temporary root creates");
	if(!root.valid()) {
		return;
	}
	std::error_code error;
	fs::create_directories(root.path() / "YUME1.DAT", error);
	const auto bank = synthetic_effect_bank();
	expect(!error && write_bytes(
		root.path() / "YUME1.DAT" / "YUME.EFC", bank
	), "cue-mapping root contains only the required effect bank");
	auto created = ClassicAudioSession::create(config(root.path()));
	expect(created.has_value(), "effect-only root creates an audio session");
	if(!created) {
		return;
	}

	struct Mapping final {
		ClassicAudioCommand command{};
		MainResourceKey key{};
	};
	std::vector<Mapping> mappings{
		{load(ClassicMusicCue::menu_main),
			{.role = MainResourceRole::op_menu_music}},
		{load(ClassicMusicCue::character_select),
			{.role = MainResourceRole::selection_music}},
		{load(ClassicMusicCue::winner),
			{.role = MainResourceRole::win_music}},
		{load(ClassicMusicCue::game_over),
			{.role = MainResourceRole::gameover_music}},
		{load(ClassicMusicCue::staff),
			{.role = MainResourceRole::staff_music}},
		{load(ClassicMusicCue::high_score),
			{.role = MainResourceRole::registration_music}},
	};
	for(std::uint16_t track = 0U; track < 10U; ++track) {
		mappings.push_back({
			load(ClassicMusicCue::gameplay, track),
			{
				.role = MainResourceRole::op_music_room_track,
				.music_room_track = static_cast<th03::app::MainMusicRoomTrack>(
					static_cast<std::uint8_t>(
						th03::app::MainMusicRoomTrack::stage_00
					) + static_cast<std::uint8_t>(track)
				),
			},
		});
	}
	for(std::uint16_t track = 0U; track < 19U; ++track) {
		mappings.push_back({
			load(ClassicMusicCue::music_room_track, track),
			{
				.role = MainResourceRole::op_music_room_track,
				.music_room_track = static_cast<th03::app::MainMusicRoomTrack>(
					track + 1U
				),
			},
		});
	}
	for(std::uint8_t index = 0U; index < 5U; ++index) {
		mappings.push_back({
			load(static_cast<ClassicMusicCue>(
				static_cast<std::uint8_t>(ClassicMusicCue::cutscene_one) +
					index
			)),
			{
				.role = MainResourceRole::cutscene_music,
				.cutscene_music = static_cast<th03::app::MainCutsceneMusic>(
					index + 1U
				),
			},
		});
	}
	for(const auto &mapping : mappings) {
		const auto result = (*created)->apply(mapping.command);
		expect(!result && result.error().code ==
			ClassicAudioSessionErrorCode::resource_load_failed &&
			result.error().resource_error &&
			result.error().resource_error->key == mapping.key,
			"each semantic cue resolves to its exact authenticated key");
	}
}

void test_config_and_resource_failures()
{
	const auto empty = ClassicAudioSession::create({});
	expect(
		!empty && empty.error().code ==
			ClassicAudioSessionErrorCode::invalid_config,
		"empty audio config is rejected"
	);
	TemporaryRoot missing;
	auto slow = config(missing.path());
	slow.tick_rate = {.ticks = 1U, .seconds = 2U};
	const auto invalid_rate = ClassicAudioSession::create(slow);
	expect(
		!invalid_rate && invalid_rate.error().code ==
			ClassicAudioSessionErrorCode::invalid_config,
		"more than one native second per logical tick is rejected"
	);
	auto invalid_gain_config = config(missing.path());
	invalid_gain_config.music_volume_percent = 42U;
	const auto invalid_gain = ClassicAudioSession::create(invalid_gain_config);
	expect(!invalid_gain && invalid_gain.error().code ==
		ClassicAudioSessionErrorCode::invalid_config,
		"category gains reject values outside five-percent steps before I/O");
	std::array<std::byte, 1U> bad_rom{};
	auto invalid_rom_config = config(missing.path());
	invalid_rom_config.rhythm_rom = bad_rom;
	const auto invalid_rom = ClassicAudioSession::create(invalid_rom_config);
	expect(
		!invalid_rom && invalid_rom.error().code ==
			ClassicAudioSessionErrorCode::engine_failed &&
			invalid_rom.error().engine_error &&
			invalid_rom.error().engine_error->code ==
				th03::audio::PmdOpnaErrorCode::invalid_rhythm_rom_size,
		"invalid rhythm ROM size remains a typed engine failure"
	);
	const auto missing_bank = ClassicAudioSession::create(config(missing.path()));
	expect(
		!missing_bank && missing_bank.error().code ==
			ClassicAudioSessionErrorCode::resource_load_failed &&
			missing_bank.error().resource_error,
		"missing effect bank prevents a false available-audio claim"
	);
}

void test_commands_cadence_and_determinism(const fs::path &root)
{
	auto first_result = ClassicAudioSession::create(config(root));
	auto second_result = ClassicAudioSession::create(config(root));
	expect(first_result && second_result, "equal generated audio sessions create");
	if(!first_result || !second_result) {
		return;
	}
	auto &first = **first_result;
	auto &second = **second_result;
	expect(first.status().bgm_active && !first.status().song_playing &&
		!first.loaded_song(),
		"created session reports the resident BGM driver before any song loads");

	const ClassicAudioCommand premature{
		.type = ClassicAudioCommandType::play_song,
		.cue = ClassicMusicCue::menu_main,
	};
	const auto rejected_play = first.apply(premature);
	expect(
		!rejected_play && rejected_play.error().code ==
			ClassicAudioSessionErrorCode::command_order,
		"Play before Load is rejected without inventing a song"
	);

	constexpr std::array cues{
		ClassicAudioCommand{ClassicAudioCommandType::load_song,
			ClassicMusicCue::menu_main, 0U},
		ClassicAudioCommand{ClassicAudioCommandType::load_song,
			ClassicMusicCue::character_select, 0U},
		ClassicAudioCommand{ClassicAudioCommandType::load_song,
			ClassicMusicCue::gameplay, 0U},
		ClassicAudioCommand{ClassicAudioCommandType::load_song,
			ClassicMusicCue::gameplay, 9U},
		ClassicAudioCommand{ClassicAudioCommandType::load_song,
			ClassicMusicCue::music_room_track, 0U},
		ClassicAudioCommand{ClassicAudioCommandType::load_song,
			ClassicMusicCue::music_room_track, 18U},
		ClassicAudioCommand{ClassicAudioCommandType::load_song,
			ClassicMusicCue::winner, 0U},
		ClassicAudioCommand{ClassicAudioCommandType::load_song,
			ClassicMusicCue::game_over, 0U},
		ClassicAudioCommand{ClassicAudioCommandType::load_song,
			ClassicMusicCue::staff, 0U},
		ClassicAudioCommand{ClassicAudioCommandType::load_song,
			ClassicMusicCue::high_score, 0U},
		ClassicAudioCommand{ClassicAudioCommandType::load_song,
			ClassicMusicCue::cutscene_one, 0U},
		ClassicAudioCommand{ClassicAudioCommandType::load_song,
			ClassicMusicCue::cutscene_two, 0U},
		ClassicAudioCommand{ClassicAudioCommandType::load_song,
			ClassicMusicCue::cutscene_three, 0U},
		ClassicAudioCommand{ClassicAudioCommandType::load_song,
			ClassicMusicCue::cutscene_four, 0U},
		ClassicAudioCommand{ClassicAudioCommandType::load_song,
			ClassicMusicCue::cutscene_five, 0U},
	};
	for(const auto &command : cues) {
		expect(first.apply(command).has_value() &&
			first.loaded_song() == command,
			"each defined Classic music cue maps to an authenticated PMD role");
	}

	const auto invalid_none = first.apply(load(ClassicMusicCue::none));
	const auto invalid_track = first.apply(load(
		ClassicMusicCue::music_room_track, 19U
	));
	const auto invalid_gameplay = first.apply(load(
		ClassicMusicCue::gameplay, 10U
	));
	const auto invalid_parameter = first.apply(load(
		ClassicMusicCue::winner, 1U
	));
	expect(!invalid_none && invalid_none.error().code ==
		ClassicAudioSessionErrorCode::invalid_cue,
		"none is not a loadable song cue");
	expect(!invalid_track && invalid_track.error().code ==
		ClassicAudioSessionErrorCode::invalid_parameter,
		"Music Room indices outside 0 through 18 are rejected");
	expect(!invalid_gameplay && invalid_gameplay.error().code ==
		ClassicAudioSessionErrorCode::invalid_parameter,
		"gameplay indices outside the ten source songs are rejected");
	expect(!invalid_parameter && invalid_parameter.error().code ==
		ClassicAudioSessionErrorCode::invalid_parameter,
		"non-indexed song cues reject stray parameters");
	expect(first.apply({
		.type = ClassicAudioCommandType::reset_effects,
	}).has_value(), "source scheduler reset command is accepted");
	const auto invalid_reset = first.apply({
		.type = ClassicAudioCommandType::reset_effects,
		.parameter = 1U,
	});
	const auto invalid_reset_cue = first.apply({
		.type = ClassicAudioCommandType::reset_effects,
		.cue = ClassicMusicCue::gameplay,
	});
	expect(!invalid_reset && invalid_reset.error().code ==
		ClassicAudioSessionErrorCode::invalid_parameter,
		"scheduler reset rejects stray source parameters");
	expect(!invalid_reset_cue && invalid_reset_cue.error().code ==
		ClassicAudioSessionErrorCode::invalid_parameter,
		"scheduler reset rejects a forged song identity");

	const auto menu = load(ClassicMusicCue::menu_main);
	const ClassicAudioCommand play{
		.type = ClassicAudioCommandType::play_song,
		.cue = ClassicMusicCue::menu_main,
	};
	for(auto *session : {&first, &second}) {
		expect(session->apply(menu).has_value() &&
			session->apply(play).has_value(),
			"equal sessions load and start the menu song");
	}
	expect(first.status().bgm_active && first.status().song_playing &&
		first.status().volume == 0U,
		"fresh Play exposes the distinct driver-active and PMD-playing states");

	std::uint64_t native_total = 0U;
	std::uint64_t canonical_total = 0U;
	for(std::size_t tick = 0U; tick < 60U; ++tick) {
		auto first_tick = first.advance_tick();
		auto second_tick = second.advance_tick();
		expect(first_tick && second_tick, "rational audio tick advances");
		if(!first_tick || !second_tick) {
			return;
		}
		native_total += first_tick->native_frames;
		canonical_total += first_tick->canonical_frames.size();
		expect(
			first_tick->tick_index == tick &&
			first_tick->native_frames == second_tick->native_frames &&
			std::equal(
				first_tick->canonical_frames.begin(),
				first_tick->canonical_frames.end(),
				second_tick->canonical_frames.begin(),
				second_tick->canonical_frames.end()
			),
			"equal command/tick streams produce identical canonical chunks"
		);
	}
	expect(native_total == th03::audio::pmd_native_sample_rate_hz,
		"sixty 60 Hz ticks allocate exactly one native audio second");
	expect(canonical_total > 47'900U && canonical_total <= 48'000U,
		"streaming lookahead emits the bounded first-second canonical prefix");
	expect(first.tick_index() == 60U && first.status().bgm_active,
		"audio-clock PMD playback advances independently of a device");
	const auto paused_measure = first.status().measure;
	expect(first.apply({
		.type = ClassicAudioCommandType::pause_song,
	}).has_value(), "New Classic pause reaches the PMD pause service");
	for(std::size_t tick = 0U; tick < 10U; ++tick) {
		expect(first.advance_tick().has_value(),
			"paused audio still supplies a device interval");
	}
	expect(first.status().measure == paused_measure,
		"pause freezes the PMD sequencer measure while device time advances");
	expect(first.apply({
		.type = ClassicAudioCommandType::resume_song,
	}).has_value(), "New Classic resume reaches the PMD resume service");
	bool measure_resumed = false;
	for(std::size_t tick = 0U; tick < 120U && !measure_resumed; ++tick) {
		auto resumed = first.advance_tick();
		measure_resumed = resumed && first.status().measure != paused_measure;
	}
	expect(measure_resumed, "resume restarts PMD sequencer progress");
	const auto invalid_pause = first.apply({
		.type = ClassicAudioCommandType::pause_song,
		.cue = ClassicMusicCue::menu_main,
	});
	expect(!invalid_pause && invalid_pause.error().code ==
		ClassicAudioSessionErrorCode::invalid_parameter,
		"pause rejects a forged cue or parameter");

	const ClassicAudioCommand invalid_fade{
		.type = ClassicAudioCommandType::fade_song,
		.parameter = 256U,
	};
	const auto rejected_fade = first.apply(invalid_fade);
	expect(!rejected_fade && rejected_fade.error().code ==
		ClassicAudioSessionErrorCode::invalid_parameter,
		"16-bit fade values are checked before signed-byte conversion");
	for(const auto parameter : {std::uint16_t{0U}, std::uint16_t{128U}}) {
		const auto rejected = first.apply({
			.type = ClassicAudioCommandType::fade_song,
			.parameter = parameter,
		});
		expect(!rejected && rejected.error().code ==
			ClassicAudioSessionErrorCode::invalid_parameter,
			"fade speed must remain in the positive signed-byte domain");
	}
	const auto wrong_fade_cue = first.apply({
		.type = ClassicAudioCommandType::fade_song,
		.cue = ClassicMusicCue::winner,
		.parameter = 16U,
	});
	expect(!wrong_fade_cue && wrong_fade_cue.error().code ==
		ClassicAudioSessionErrorCode::invalid_parameter,
		"fade cannot silently target a cue other than the loaded song");
	expect(first.apply({
		.type = ClassicAudioCommandType::fade_song,
		.parameter = 16U,
	}).has_value(), "source fade speed 16 is accepted");
	expect(first.apply({
		.type = ClassicAudioCommandType::load_effects,
	}).has_value(), "explicit source effect-bank reload is supported");

	const auto cleared_request = first.request_effect(2U);
	expect(cleared_request && *cleared_request,
		"ordinary MAIN effect request enters the source slot before reset");
	expect(first.apply({
		.type = ClassicAudioCommandType::reset_effects,
	}).has_value(), "repeat-round source reset clears the pending effect slot");
	auto effect_tick = first.advance_tick();
	expect(effect_tick && !effect_tick->effect_started,
		"scheduler reset does not dispatch the cleared pending effect");

	const auto requested = first.request_effect(2U);
	expect(requested && *requested,
		"ordinary MAIN effect request re-enters the slot after reset");
	effect_tick = first.advance_tick();
	expect(effect_tick && effect_tick->effect_started == 2U,
		"the one-slot scheduler dispatches once at the tick boundary");
	const auto lower_priority = first.request_effect(1U);
	expect(lower_priority && !*lower_priority,
		"lower-priority request cannot replace the held source slot");
	const auto invalid_effect = first.request_effect(23U);
	expect(!invalid_effect && invalid_effect.error().code ==
		ClassicAudioSessionErrorCode::service_failed,
		"out-of-table ordinary effect requests are bounded");
	expect(first.apply({
		.type = ClassicAudioCommandType::play_effect,
		.parameter = 3U,
	}).has_value(), "forced cutscene effect resets and starts immediately");
	const auto wide_effect = first.apply({
		.type = ClassicAudioCommandType::play_effect,
		.parameter = 257U,
	});
	expect(!wide_effect && wide_effect.error().code ==
		ClassicAudioSessionErrorCode::invalid_parameter,
		"16-bit script effect values are checked before narrowing");
	const auto zero_effect = first.apply({
		.type = ClassicAudioCommandType::play_effect,
		.parameter = 0U,
	});
	expect(!zero_effect && zero_effect.error().code ==
		ClassicAudioSessionErrorCode::invalid_parameter,
		"forced effect zero is outside TH03's 1 through 22 domain");

	expect(first.apply({
		.type = ClassicAudioCommandType::stop_song,
	}).has_value() && first.status().bgm_active &&
		!first.status().song_playing && first.status().volume == 255U,
		"Stop preserves driver availability while exposing PMD Stop and silence");

	expect(first.apply(load(ClassicMusicCue::gameplay, 8U)).has_value() &&
		first.apply({
			.type = ClassicAudioCommandType::play_song,
			.cue = ClassicMusicCue::gameplay,
			.parameter = 8U,
		}).has_value(),
		"replay gameplay song is active before its MAINL cutscene");
	const auto mislabeled_cutscene_stop = first.apply({
		.type = ClassicAudioCommandType::stop_song,
		.cue = ClassicMusicCue::cutscene_two,
	});
	expect(!mislabeled_cutscene_stop &&
		mislabeled_cutscene_stop.error().code ==
			ClassicAudioSessionErrorCode::invalid_parameter,
		"a future cutscene cue cannot identify the still-loaded gameplay song");
	expect(first.apply({
		.type = ClassicAudioCommandType::stop_song,
	}).has_value() &&
		first.apply(load(ClassicMusicCue::cutscene_two)).has_value() &&
		first.apply({
			.type = ClassicAudioCommandType::play_song,
			.cue = ClassicMusicCue::cutscene_two,
		}).has_value(),
		"source Stop/Load/Play crosses from replay gameplay to cutscene music");
}

void test_music_off_disables_fresh_process_sfx(const fs::path &root)
{
	auto disabled_config = config(root);
	disabled_config.bgm_active = false;
	auto created = ClassicAudioSession::create(disabled_config);
	expect(created && !(*created)->status().bgm_active,
		"an Off configuration exposes the inactive BGM-driver flag");
	if(!created) {
		return;
	}
	auto &session = **created;
	expect(session.apply(load(ClassicMusicCue::menu_main)).has_value() &&
		session.apply({
			.type = ClassicAudioCommandType::play_song,
			.cue = ClassicMusicCue::menu_main,
		}).has_value(), "inactive BGM accepts source load/play calls as no-ops");
	auto silent_tick = session.advance_tick();
	expect(silent_tick && !silent_tick->status.bgm_active && std::all_of(
		silent_tick->canonical_frames.begin(),
		silent_tick->canonical_frames.end(),
		[](const auto &sample) {
			return sample.left == 0 && sample.right == 0;
		}
	), "inactive BGM advances a deterministic silent source block");

	const auto inactive_request = session.request_effect(1U);
	expect(inactive_request && !*inactive_request,
		"Music Off suppresses ordinary MAIN sound-effect requests");
	expect(session.apply({
		.type = ClassicAudioCommandType::play_effect,
		.parameter = 1U,
	}).has_value(), "Music Off makes a forced MAINL effect a source no-op");
	auto effect_tick = session.advance_tick();
	expect(effect_tick && !effect_tick->effect_started && std::all_of(
		effect_tick->canonical_frames.begin(),
		effect_tick->canonical_frames.end(),
		[](const auto &sample) {
			return sample.left == 0 && sample.right == 0;
		}
	), "Music Off keeps both song and effect output silent");

	expect(session.apply({
		.type = ClassicAudioCommandType::set_bgm_active,
		.parameter = 1U,
	}).has_value() && session.apply({
		.type = ClassicAudioCommandType::play_song,
		.cue = ClassicMusicCue::menu_main,
	}).has_value() && session.status().bgm_active,
		"enabling the BGM driver lets the already-loaded song start");
	expect(session.request_effect(1U).value_or(false),
		"enabling the FM driver also enables ordinary sound effects");
	const auto enabled_effect = session.advance_tick();
	expect(enabled_effect && enabled_effect->effect_started == 1U,
		"the enabled sound-effect request starts on the next update");
	const auto invalid_active = session.apply({
		.type = ClassicAudioCommandType::set_bgm_active,
		.parameter = 2U,
	});
	expect(!invalid_active && invalid_active.error().code ==
		ClassicAudioSessionErrorCode::invalid_parameter &&
		session.status().bgm_active,
		"invalid BGM-active values reject atomically");
	expect(session.apply({
		.type = ClassicAudioCommandType::stop_song,
	}).has_value() && session.status().bgm_active,
		"Stop leaves the enabled BGM-driver flag intact");
}

void test_new_classic_independent_sfx(const fs::path &root)
{
	auto effects_only_config = config(root);
	effects_only_config.bgm_active = false;
	effects_only_config.effects_follow_bgm = false;
	auto effects_only = ClassicAudioSession::create(effects_only_config);
	expect(effects_only.has_value(),
		"New Classic effects-only audio session creates");
	if(!effects_only) {
		return;
	}
	expect((*effects_only)->apply(load(ClassicMusicCue::menu_main)).has_value() &&
		(*effects_only)->apply({
			.type = ClassicAudioCommandType::play_song,
			.cue = ClassicMusicCue::menu_main,
		}).has_value() && (*effects_only)->request_effect(1U).value_or(false),
		"Music Off still loads the PMD carrier and accepts SFX in New Classic");
	const auto effect = (*effects_only)->advance_tick();
	expect(effect && !effect->status.bgm_active &&
		effect->effect_started == 1U,
		"independent New Classic SFX starts while BGM status remains Off");
	expect((*effects_only)->apply({
		.type = ClassicAudioCommandType::set_effects_active,
		.parameter = 0U,
	}).has_value() && !(*effects_only)->request_effect(1U).value_or(true),
		"live New Classic SFX Off immediately suppresses the active effect service");
	expect((*effects_only)->apply({
		.type = ClassicAudioCommandType::set_effects_active,
		.parameter = 1U,
	}).has_value() && (*effects_only)->request_effect(1U).value_or(false),
		"live New Classic SFX On immediately restores independent effects");
	const auto invalid_effects_active = (*effects_only)->apply({
		.type = ClassicAudioCommandType::set_effects_active,
		.parameter = 2U,
	});
	expect(!invalid_effects_active && invalid_effects_active.error().code ==
		ClassicAudioSessionErrorCode::invalid_parameter,
		"live SFX policy rejects values outside the source boolean domain");
	expect((*effects_only)->apply({
		.type = ClassicAudioCommandType::set_music_volume,
		.parameter = 35U,
	}).has_value() && (*effects_only)->apply({
		.type = ClassicAudioCommandType::set_effect_volume,
		.parameter = 65U,
	}).has_value(),
		"New Classic music and SFX gains apply independently at runtime");
	const auto invalid_category_gain = (*effects_only)->apply({
		.type = ClassicAudioCommandType::set_effect_volume,
		.parameter = 64U,
	});
	expect(!invalid_category_gain && invalid_category_gain.error().code ==
		ClassicAudioSessionErrorCode::invalid_parameter,
		"live category gain rejects non-five-percent values atomically");

	auto music_only_config = config(root);
	music_only_config.effects_active = false;
	music_only_config.effects_follow_bgm = false;
	auto music_only = ClassicAudioSession::create(music_only_config);
	expect(music_only &&
		(*music_only)->apply(load(ClassicMusicCue::menu_main)).has_value() &&
		(*music_only)->apply({
			.type = ClassicAudioCommandType::play_song,
			.cue = ClassicMusicCue::menu_main,
		}).has_value() &&
		!(*music_only)->request_effect(1U).value_or(true),
		"New Classic SFX Off suppresses effects without disabling music");
}

void test_source_boundary_effect_dispatch(const fs::path &root)
{
	auto dispatched_result = ClassicAudioSession::create(config(root));
	auto baseline_result = ClassicAudioSession::create(config(root));
	expect(dispatched_result && baseline_result,
		"source-boundary effect comparison sessions create");
	if(!dispatched_result || !baseline_result) {
		return;
	}
	auto &dispatched = **dispatched_result;
	auto &baseline = **baseline_result;
	const auto premature = dispatched.apply({
		.type = ClassicAudioCommandType::dispatch_effect,
		.parameter = 1U,
	});
	expect(!premature && premature.error().code ==
		ClassicAudioSessionErrorCode::command_order,
		"a source-boundary effect cannot dispatch before its carrier song loads");
	for(auto *session : {&dispatched, &baseline}) {
		expect(
			session->apply(load(ClassicMusicCue::menu_main)).has_value() &&
			session->apply({
				.type = ClassicAudioCommandType::play_song,
				.cue = ClassicMusicCue::menu_main,
			}).has_value(),
			"source-boundary comparison sessions start the same song"
		);
	}
	expect(dispatched.apply({
		.type = ClassicAudioCommandType::dispatch_effect,
		.parameter = 1U,
	}).has_value(),
		"an already-arbitrated MAIN effect starts without a second scheduler update");
	bool scheduler_stayed_empty = true;
	bool rendered_effect = false;
	for(unsigned int tick = 0; tick < 3U; ++tick) {
		const auto dispatched_tick = dispatched.advance_tick();
		const auto baseline_tick = baseline.advance_tick();
		if(!dispatched_tick || !baseline_tick) {
			scheduler_stayed_empty = false;
			break;
		}
		scheduler_stayed_empty = scheduler_stayed_empty &&
			!dispatched_tick->effect_started && !baseline_tick->effect_started &&
			dispatched_tick->canonical_frames.size() ==
				baseline_tick->canonical_frames.size();
		rendered_effect = rendered_effect || !std::equal(
			dispatched_tick->canonical_frames.begin(),
			dispatched_tick->canonical_frames.end(),
			baseline_tick->canonical_frames.begin()
		);
	}
	expect(scheduler_stayed_empty && rendered_effect,
		"direct dispatch changes audio at the first PMD Timer-A effect window while leaving the session scheduler empty");
	for(const auto invalid : {
		ClassicAudioCommand{
			.type = ClassicAudioCommandType::dispatch_effect,
			.parameter = 0U,
		},
		ClassicAudioCommand{
			.type = ClassicAudioCommandType::dispatch_effect,
			.parameter = 23U,
		},
		ClassicAudioCommand{
			.type = ClassicAudioCommandType::dispatch_effect,
			.cue = ClassicMusicCue::winner,
			.parameter = 3U,
		},
	}) {
		const auto rejected = dispatched.apply(invalid);
		expect(!rejected && rejected.error().code ==
			ClassicAudioSessionErrorCode::invalid_parameter,
			"direct MAIN effect dispatch rejects a malformed command");
	}
}

void test_authentic_session(const fs::path &data_root)
{
	auto created = ClassicAudioSession::create({
		.data_root = data_root,
		.tick_rate = {.ticks = 60U, .seconds = 1U},
		.integrity = MainResourceIntegrity::size_and_fnv1a64,
	});
	expect(static_cast<bool>(created),
		"authentic manifest creates a strict Classic audio session");
	if(!created) {
		return;
	}
	auto &session = **created;
	std::vector<ClassicAudioCommand> songs{
		load(ClassicMusicCue::menu_main),
		load(ClassicMusicCue::character_select),
		load(ClassicMusicCue::winner),
		load(ClassicMusicCue::game_over),
		load(ClassicMusicCue::staff),
		load(ClassicMusicCue::high_score),
		load(ClassicMusicCue::cutscene_one),
		load(ClassicMusicCue::cutscene_two),
		load(ClassicMusicCue::cutscene_three),
		load(ClassicMusicCue::cutscene_four),
		load(ClassicMusicCue::cutscene_five),
	};
	for(std::uint16_t track = 0U; track < 10U; ++track) {
		songs.push_back(load(ClassicMusicCue::gameplay, track));
	}
	for(std::uint16_t track = 0U; track < 19U; ++track) {
		songs.push_back(load(ClassicMusicCue::music_room_track, track));
	}

	std::size_t songs_passed = 0U;
	for(const auto &song : songs) {
		const ClassicAudioCommand play{
			.type = ClassicAudioCommandType::play_song,
			.cue = song.cue,
			.parameter = song.parameter,
		};
		const auto loaded = session.apply(song);
		const auto played = loaded ? session.apply(play) :
			std::expected<void, th03::app::ClassicAudioSessionError>{
				std::unexpected(loaded.error())
			};
		const auto tick = played ? session.advance_tick() :
			std::expected<
				th03::app::ClassicAudioTick,
				th03::app::ClassicAudioSessionError
			>{std::unexpected(played.error())};
		expect(
			loaded && played && tick && tick->native_frames != 0U &&
				tick->status.bgm_active,
			"authentic Classic cue loads, starts, and renders"
		);
		if(loaded && played && tick && tick->status.bgm_active) {
			++songs_passed;
		}
		expect(session.apply({
			.type = ClassicAudioCommandType::stop_song,
		}).has_value(), "authentic Classic song stops cleanly");
	}

	expect(session.apply(load(ClassicMusicCue::menu_main)).has_value() &&
		session.apply({
			.type = ClassicAudioCommandType::play_song,
			.cue = ClassicMusicCue::menu_main,
		}).has_value(), "authentic effect gate starts its carrier song");
	std::size_t effects_passed = 0U;
	for(std::uint16_t effect = 1U; effect <= 22U; ++effect) {
		const auto started = session.apply({
			.type = ClassicAudioCommandType::play_effect,
			.parameter = effect,
		});
		const auto tick = started ? session.advance_tick() :
			std::expected<
				th03::app::ClassicAudioTick,
				th03::app::ClassicAudioSessionError
			>{std::unexpected(started.error())};
		expect(started && tick && tick->native_frames != 0U,
			"authentic EFC entry starts and renders through the session");
		if(started && tick) {
			++effects_passed;
		}
	}
	expect(songs_passed == songs.size(),
		"all 40 reachable authentic Classic song identities pass");
	expect(effects_passed == 22U,
		"all 22 authentic Classic effect identities pass");
	std::cout << "authentic_session_songs=" << songs_passed
		<< " authentic_session_effects=" << effects_passed << '\n';
}

} // namespace

int main(int argc, char **argv)
{
	test_config_and_resource_failures();
	test_exact_cue_mapping();
	TemporaryRoot root;
	expect(root.valid(), "temporary Classic audio root creates");
	if(root.valid()) {
		expect(prepare_audio_root(root.path()),
			"generated audio manifest root is written");
		test_commands_cadence_and_determinism(root.path());
		test_music_off_disables_fresh_process_sfx(root.path());
		test_new_classic_independent_sfx(root.path());
		test_source_boundary_effect_dispatch(root.path());
	}
	if(argc == 3 && std::string_view(argv[1]) == "--authentic") {
		test_authentic_session(argv[2]);
	} else if(argc != 1) {
		std::cerr << "usage: th03-app-classic-audio-session-tests "
			"[--authentic <data-root>]\n";
		return 2;
	}

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
