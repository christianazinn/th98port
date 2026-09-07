#include "../src/app/classic/scenes/classic_staff_scene_internal.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "th03/assets/decode.hpp"
#include "th03/app/content/authentic_text.hpp"

namespace {

using th03::app::ClassicAudioCommandType;
using th03::app::ClassicMusicCue;
using th03::app::ClassicStaffBoundary;
using th03::app::ClassicStaffErrorCode;
using th03::app::ClassicStaffInput;
using th03::app::ClassicStaffPhase;
using th03::app::ClassicStaffScene;
using th03::app::ClassicStaffSceneTestAccess;
using th03::app::LoadedMainResource;
using th03::app::MainResourceKey;
using th03::app::MainResourceRole;
using th03::app::MainStaffCdg;
using th03::app::MainlStaffCompiledAssets;
using th03::app::MainlStageTextAssets;
using th03::app::NewClassicGameTextCatalog;
using th03::GameInit;
using th03::present::AnkGlyph;
using th03::present::AnkGlyphProvider;
using th03::present::JisGlyph;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;
using th03::present::StaffFlakeTrigTables;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

void append_u8(std::vector<std::byte> &bytes, std::uint8_t value)
{
	bytes.push_back(static_cast<std::byte>(value));
}

void append_le16(std::vector<std::byte> &bytes, std::uint16_t value)
{
	append_u8(bytes, static_cast<std::uint8_t>(value));
	append_u8(bytes, static_cast<std::uint8_t>(value >> 8U));
}

std::vector<std::byte> make_rgb()
{
	std::vector<std::byte> result;
	result.reserve(48U);
	for(std::uint8_t color = 0U; color < 16U; ++color) {
		append_u8(result, color);
		append_u8(result, static_cast<std::uint8_t>(15U - color));
		append_u8(result, static_cast<std::uint8_t>(color ^ 7U));
	}
	return result;
}

std::vector<std::byte> make_cdg(
	std::uint16_t width,
	std::uint16_t height,
	std::uint8_t color
)
{
	const auto plane_size = (static_cast<std::size_t>(width) / 8U) * height;
	std::vector<std::byte> bytes;
	bytes.reserve(16U + (plane_size * 5U));
	append_le16(bytes, static_cast<std::uint16_t>(plane_size));
	append_le16(bytes, width);
	append_le16(bytes, height);
	append_le16(bytes, static_cast<std::uint16_t>(80U * (height - 1U)));
	append_le16(bytes, static_cast<std::uint16_t>(width / 32U));
	append_u8(bytes, 0U);
	append_u8(bytes, 0U);
	append_le16(bytes, 0U);
	append_le16(bytes, 0U);
	bytes.insert(bytes.end(), plane_size, std::byte{0xFFU});
	for(std::size_t plane = 0U; plane < 4U; ++plane) {
		const auto value = ((color & (1U << plane)) != 0U) ?
			std::byte{0xFFU} : std::byte{0U};
		bytes.insert(bytes.end(), plane_size, value);
	}
	return bytes;
}

constexpr std::array<std::array<std::uint16_t, 2U>, 12U> geometries{{
	{{256U, 48U}}, {{384U, 26U}}, {{416U, 32U}}, {{320U, 40U}},
	{{256U, 32U}}, {{512U, 32U}}, {{448U, 32U}}, {{256U, 230U}},
	{{192U, 36U}}, {{128U, 138U}}, {{288U, 24U}}, {{224U, 36U}},
}};

bool same_cel(
	const th03::assets::PlanarCel4 &left,
	const th03::assets::PlanarCel4 &right
)
{
	return left.bitplane_size == right.bitplane_size &&
		left.pixel_width == right.pixel_width &&
		left.pixel_height == right.pixel_height &&
		left.offset_at_bottom_left == right.offset_at_bottom_left &&
		left.vram_dword_width == right.vram_dword_width &&
		left.row_order == right.row_order && left.bit_order == right.bit_order &&
		left.alpha == right.alpha && left.color_planes == right.color_planes;
}

std::vector<std::byte> bytes_for(const th03::app::MainResourceSpec &spec)
{
	if(spec.key.role == MainResourceRole::staff_background_palette) {
		return make_rgb();
	}
	if(spec.key.role == MainResourceRole::staff_music) {
		return std::vector<std::byte>(spec.expected_bytes, std::byte{0x5AU});
	}
	if(spec.key.role == MainResourceRole::staff_cdg) {
		const auto index = static_cast<std::size_t>(
			static_cast<std::uint8_t>(spec.key.staff_cdg) - 1U
		);
		return make_cdg(
			geometries[index][0U], geometries[index][1U],
			static_cast<std::uint8_t>((index + 1U) & 0x0FU)
		);
	}
	return {};
}

std::vector<LoadedMainResource> resources()
{
	std::vector<LoadedMainResource> result;
	for(const auto key : th03::app::mainl_staff_resource_keys()) {
		const auto spec = *th03::app::main_resource_spec(key);
		auto bytes = std::make_shared<const std::vector<std::byte>>(
			bytes_for(spec)
		);
		expect(
			bytes->size() == spec.expected_bytes,
			"synthetic staff resource has its canonical byte count"
		);
		result.push_back({.spec = spec, .bytes = std::move(bytes)});
	}
	return result;
}

class SyntheticFont final : public AnkGlyphProvider {
public:
	[[nodiscard]] std::optional<AnkGlyph> glyph(
		std::uint8_t
	) const override
	{
		return std::nullopt;
	}

	[[nodiscard]] std::optional<JisGlyph> jis_glyph(
		std::uint16_t
	) const override
	{
		if(missing_) {
			return std::nullopt;
		}
		JisGlyph result{};
		result.row_halves[0U] = 0x80U;
		return result;
	}

	bool missing_ = false;
};

MainlStageTextAssets stage_text()
{
	MainlStageTextAssets result{};
	for(std::size_t index = 0U; index < result.characters.size(); ++index) {
		result.characters[index].name.push_back(static_cast<std::uint8_t>(
			'A' + index
		));
	}
	return result;
}

MainlStaffCompiledAssets compiled_assets()
{
	MainlStaffCompiledAssets result{};
	for(std::size_t index = 0U; index < result.ranks.size(); ++index) {
		result.ranks[index].push_back(static_cast<std::uint8_t>('R' + index));
	}
	for(std::size_t index = 0U; index < result.numbers.size(); ++index) {
		result.numbers[index].push_back(static_cast<std::uint8_t>('0' + index));
	}
	result.point.push_back(static_cast<std::uint8_t>('.'));
	return result;
}

GameInit resident()
{
	GameInit result{};
	result.game_mode = th03::game_mode::story;
	result.rank = 2U;
	result.remaining_credits = 1U;
	result.skill = 5U;
	result.paletted_character = {1, 18};
	result.resident_rand = std::bit_cast<std::int32_t>(0x12345678U);
	result.last_score[0U] = {5U, 4U, 3U, 2U, 1U, 8U, 3U, 0U};
	return result;
}

std::array<std::int16_t, 256U> sine{};
std::array<std::int16_t, 256U> cosine{};

StaffFlakeTrigTables trig()
{
	return {sine, cosine};
}

std::expected<std::unique_ptr<ClassicStaffScene>, th03::app::ClassicStaffError>
create_scene(
	SyntheticFont &font,
	bool fast_forward_unlocked = false,
	const NewClassicGameTextCatalog *game_text = nullptr
)
{
	auto closure = resources();
	return ClassicStaffSceneTestAccess::create_with_synthetic_resources(
		resident(), closure, stage_text(), compiled_assets(), font, trig(), Raster{},
		fast_forward_unlocked, game_text
	);
}

void expect_command(
	ClassicStaffScene &scene,
	ClassicAudioCommandType type,
	ClassicMusicCue cue,
	std::uint16_t parameter,
	std::string_view message
)
{
	const auto command = scene.take_audio_command();
	expect(
		command && command->type == type && command->cue == cue &&
			command->parameter == parameter,
		message
	);
}

ClassicStaffInput high_measure(bool overrun = false)
{
	return {
		.audio = {.bgm_active = true, .measure = 0xFFFFU, .volume = 255U},
		.vsync_count_above_one = overrun,
	};
}

ClassicStaffInput music_on(std::uint8_t volume = 0U)
{
	return {.audio = {.bgm_active = true, .volume = volume}};
}

void enter_presentation(ClassicStaffScene &scene)
{
	auto tick = scene.advance({});
	expect(tick && tick->boundary == ClassicStaffBoundary::immediate,
		"staff entry queues the source fade immediately");
	expect_command(
		scene, ClassicAudioCommandType::fade_song, ClassicMusicCue::none, 16U,
		"staff entry requests the source fade speed"
	);

	tick = scene.advance(music_on());
	expect(tick && tick->boundary == ClassicStaffBoundary::vsync,
		"blackout begins with one alignment VSync");
	for(std::size_t wait = 0U; wait < 68U; ++wait) {
		tick = scene.advance(music_on());
		expect(tick && tick->boundary == ClassicStaffBoundary::vsync,
			"blackout preserves all 68 tone waits");
	}
	tick = scene.advance(music_on());
	expect(
		tick && tick->phase == ClassicStaffPhase::wait_volume &&
			tick->boundary == ClassicStaffBoundary::audio_volume &&
			!tick->surface_changed,
		"blackout reaches the source volume-255 wait without a fake frame"
	);

	tick = scene.advance(music_on(255U));
	expect(tick && tick->boundary == ClassicStaffBoundary::immediate,
		"volume completion queues stop immediately");
	expect_command(
		scene, ClassicAudioCommandType::stop_song, ClassicMusicCue::none, 0U,
		"staff setup stops the faded prior song"
	);
	tick = scene.advance(music_on(255U));
	expect(tick && tick->boundary == ClassicStaffBoundary::immediate,
		"staff song load is an immediate command boundary");
	expect_command(
		scene, ClassicAudioCommandType::load_song, ClassicMusicCue::staff, 0U,
		"staff setup loads ED.M"
	);
	tick = scene.advance(music_on(255U));
	expect(
		tick && tick->phase == ClassicStaffPhase::presentation &&
			tick->surface_changed,
		"palette, border, pages, and flake state publish atomically"
	);
	expect_command(
		scene, ClassicAudioCommandType::play_song, ClassicMusicCue::staff, 0U,
		"staff setup starts ED.M only after the visual handoff"
	);
	tick = scene.advance(high_measure());
	expect(tick && tick->boundary == ClassicStaffBoundary::vsync,
		"staff setup retains the source one-frame post-load delay");
}

void test_music_off_volume_wait_escape()
{
	SyntheticFont font;
	auto scene = create_scene(font);
	expect(static_cast<bool>(scene), "Music-Off staff scene constructs");
	if(!scene) {
		return;
	}
	auto tick = (*scene)->advance({});
	expect(tick && tick->boundary == ClassicStaffBoundary::immediate,
		"Music-Off staff entry queues its fade command");
	expect_command(
		**scene, ClassicAudioCommandType::fade_song, ClassicMusicCue::none, 16U,
		"Music-Off staff entry preserves the source fade request"
	);
	tick = (*scene)->advance({});
	expect(tick && tick->boundary == ClassicStaffBoundary::vsync,
		"Music-Off staff blackout begins normally");
	for(std::size_t wait = 0U; wait < 68U; ++wait) {
		tick = (*scene)->advance({});
		expect(tick && tick->boundary == ClassicStaffBoundary::vsync,
			"Music-Off staff blackout retains every source VSync");
	}
	tick = (*scene)->advance({});
	expect(
		tick && tick->phase == ClassicStaffPhase::wait_volume &&
			tick->boundary == ClassicStaffBoundary::immediate,
		"Music Off skips only the donor's unsatisfiable volume-driver wait"
	);
	expect_command(
		**scene, ClassicAudioCommandType::stop_song, ClassicMusicCue::none, 0U,
		"Music-Off staff setup still publishes the source stop command"
	);
}

void test_complete_schedule_and_verdict()
{
	SyntheticFont font;
	auto scene = create_scene(font);
	expect(static_cast<bool>(scene),
		"canonical synthetic staff closure constructs transactionally");
	if(!scene) {
		return;
	}
	enter_presentation(**scene);

	auto first = (*scene)->advance(high_measure(true));
	expect(
		first && ClassicStaffSceneTestAccess::active_flake_count(**scene) == 50U &&
			!ClassicStaffSceneTestAccess::overrun_reset_pending(**scene) &&
			!ClassicStaffSceneTestAccess::cdg_put_alpha(**scene),
		"one sampled VSync overrun applies the exact 80-to-50 alpha downgrade"
	);

	std::size_t ticks = 1U;
	while((*scene)->phase() == ClassicStaffPhase::presentation && ticks < 5000U) {
		const auto advanced = (*scene)->advance(high_measure());
		expect(static_cast<bool>(advanced),
			"each staff presentation tick remains valid");
		if(!advanced) {
			return;
		}
		++ticks;
	}
	expect(
		(*scene)->phase() == ClassicStaffPhase::verdict && ticks == 3877U,
		"all source phase floors, nine slide cycles, cleanups, dissolve, and immediate verdict install are exact"
	);
	const auto p0 = (*scene)->raster().pixel(Page::zero, Point{352, 174});
	const auto p1 = (*scene)->raster().pixel(Page::one, Point{352, 174});
	expect(
		p0 && p1 && *p0 == 15U && *p1 == 15U,
		"verdict text is installed on both graphics pages"
	);

	for(std::size_t frame = 0U; frame < 256U; ++frame) {
		const auto waiting = (*scene)->advance({
			.audio = high_measure().audio,
			.any_input = true,
		});
		expect(waiting && !(*scene)->take_audio_command(),
			"input remains locked through verdict frame 256");
	}
	const auto dismissed = (*scene)->advance({
		.audio = high_measure().audio,
		.any_input = true,
	});
	expect(dismissed && dismissed->input_sampled,
		"the first post-lock input is sampled on its flake tick");
	expect_command(
		**scene, ClassicAudioCommandType::fade_song, ClassicMusicCue::staff, 8U,
		"verdict dismissal queues the exact staff fade"
	);
	for(std::size_t frame = 0U; frame < 199U; ++frame) {
		const auto fading = (*scene)->advance(high_measure());
		expect(static_cast<bool>(fading), "verdict fade tick remains valid");
	}
	expect(
		(*scene)->phase() == ClassicStaffPhase::ready &&
			(*scene)->raster().palette_tone() == 1,
		"final fade shows tone 1 and completes without inventing a tone-0 frame"
	);
	auto completion = (*scene)->take_completion();
	expect(
		completion && (*scene)->phase() == ClassicStaffPhase::consumed &&
			completion->raster().palette_tone() == 1,
		"completed staff Raster transfers exactly once"
	);
}

void test_new_classic_localized_staff_art()
{
	SyntheticFont font;
	auto source = create_scene(font);
	auto english = create_scene(
		font, false, &th03::app::new_classic_builtin_english_game_text()
	);
	auto japanese = create_scene(
		font, false, &th03::app::new_classic_builtin_japanese_game_text()
	);
	expect(
		source.has_value() && english.has_value() && japanese.has_value(),
		"source, English, and Japanese staff scenes construct"
	);
	if(!source || !english || !japanese) {
		return;
	}
	std::size_t locale_differences = 0U;
	for(std::size_t index = 0U; index < geometries.size(); ++index) {
		const auto &source_cel = ClassicStaffSceneTestAccess::cel(**source, index);
		const auto &english_cel = ClassicStaffSceneTestAccess::cel(**english, index);
		const auto &japanese_cel = ClassicStaffSceneTestAccess::cel(**japanese, index);
		expect(
			english_cel.pixel_width == source_cel.pixel_width &&
			english_cel.pixel_height == source_cel.pixel_height &&
			english_cel.alpha.has_value() == source_cel.alpha.has_value() &&
			japanese_cel.pixel_width == source_cel.pixel_width &&
			japanese_cel.pixel_height == source_cel.pixel_height &&
			japanese_cel.alpha.has_value() == source_cel.alpha.has_value(),
			"localized staff cels preserve source geometry and alpha ownership"
		);
		if(index == 7U) {
			expect(
				same_cel(source_cel, english_cel) &&
				same_cel(source_cel, japanese_cel),
				"decorative STF8 character art remains source-identical"
			);
			continue;
		}
		expect(
			!same_cel(source_cel, english_cel) &&
				same_cel(source_cel, japanese_cel),
			"English rebuilds semantic staff cels while Japanese keeps source art"
		);
		if(!same_cel(english_cel, japanese_cel)) {
			++locale_differences;
		}
	}
	expect(
		locale_differences >= 6U,
		"English and Japanese visibly differ across the translated staff roll"
	);
	expect(
		ClassicStaffSceneTestAccess::playchar_width(**japanese) ==
			ClassicStaffSceneTestAccess::playchar_width(**source) &&
		ClassicStaffSceneTestAccess::rank_width(**japanese) ==
			ClassicStaffSceneTestAccess::rank_width(**source) &&
		ClassicStaffSceneTestAccess::point_glyph_count(**japanese) ==
			ClassicStaffSceneTestAccess::point_glyph_count(**source),
		"Japanese verdict text retains the authenticated source presentation"
	);
}

void test_current_audio_gate_and_reset_lifetime()
{
	SyntheticFont font;
	auto scene = create_scene(font);
	expect(static_cast<bool>(scene), "second staff scene constructs");
	if(!scene) {
		return;
	}
	enter_presentation(**scene);
	ClassicStaffInput stalled{
		.audio = {.bgm_active = true, .measure = 0U, .volume = 255U},
	};
	for(std::size_t frame = 0U; frame < 257U; ++frame) {
		expect(static_cast<bool>((*scene)->advance(stalled)),
			"active low-measure music keeps the source phase running");
	}
	expect(
		ClassicStaffSceneTestAccess::overrun_reset_pending(**scene) &&
			ClassicStaffSceneTestAccess::active_flake_count(**scene) == 80U,
		"no-overrun first phase retains all 80 flakes and the pending latch"
	);
	stalled.audio.bgm_active = false;
	expect(static_cast<bool>((*scene)->advance(stalled)),
		"current inactive audio switches immediately to the frame fallback");

	while(ClassicStaffSceneTestAccess::sequence_index(**scene) == 0U) {
		const auto advanced = (*scene)->advance(high_measure());
		expect(static_cast<bool>(advanced), "first source slide cycle advances");
		if(!advanced) {
			return;
		}
	}
	expect(
		ClassicStaffSceneTestAccess::sequence_index(**scene) == 1U &&
			!ClassicStaffSceneTestAccess::overrun_reset_pending(**scene) &&
			ClassicStaffSceneTestAccess::active_flake_count(**scene) == 80U &&
			ClassicStaffSceneTestAccess::cdg_put_alpha(**scene),
		"STF11 disables only the overrun latch without degrading a clean run"
	);
}

void test_fast_forward_unlock_and_frame_policy()
{
	SyntheticFont font;
	auto locked = create_scene(font, false);
	auto unlocked = create_scene(font, true);
	expect(locked && unlocked,
		"locked and previously-cleared staff profiles both construct");
	if(!locked || !unlocked) {
		return;
	}
	enter_presentation(**locked);
	enter_presentation(**unlocked);
	ClassicStaffInput held{
		.audio = {.bgm_active = true, .measure = 0U, .volume = 255U},
		.fast_forward_held = true,
	};
	expect((*locked)->advance(held).has_value() &&
		ClassicStaffSceneTestAccess::frame(**locked) == 1,
		"physical Z cannot accelerate the first-clear locked staff roll");
	expect((*unlocked)->advance(held).has_value() &&
		ClassicStaffSceneTestAccess::frame(**unlocked) == 4,
		"an unlocked held-Z tick advances exactly four visual iterations");
	for(std::size_t tick = 1U; tick < 65U; ++tick) {
		expect((*locked)->advance(held).has_value() &&
			(*unlocked)->advance(held).has_value(),
			"locked and accelerated low-measure staff phases remain valid");
	}
	expect(
		ClassicStaffSceneTestAccess::frame(**locked) == 65 &&
		ClassicStaffSceneTestAccess::frame(**unlocked) < 16,
		"held-Z substitutes the donor's frame threshold while ED.M remains at normal speed"
	);
}

void test_overlay_forces_alpha_after_performance_downgrade()
{
	SyntheticFont font;
	auto scene = create_scene(font);
	expect(static_cast<bool>(scene), "overlay probe scene constructs");
	if(!scene) {
		return;
	}
	enter_presentation(**scene);
	expect(
		ClassicStaffSceneTestAccess::forced_overlay_alpha_probe(**scene),
		"STF3 overlay forces alpha even after the primary CDG downgrade"
	);
}

void test_atomic_rejections_and_audio_guard()
{
	SyntheticFont font;
	auto valid_resident = resident();
	auto cels = std::array<th03::assets::PlanarCel4, 12U>{};
	for(std::size_t index = 0U; index < cels.size(); ++index) {
		auto decoded = th03::assets::decode_cdg_cel(
			make_cdg(
				geometries[index][0U], geometries[index][1U],
				static_cast<std::uint8_t>(index + 1U)
			),
			0U,
			(index == 2U || index == 3U) ?
				th03::assets::CdgAlphaPolicy::preserve :
				th03::assets::CdgAlphaPolicy::discard
		);
		expect(static_cast<bool>(decoded), "predecoded staff cel fixture is valid");
		cels[index] = std::move(*decoded);
	}
	const auto palette = *th03::assets::decode_rgb48(make_rgb());

	auto invalid_score = valid_resident;
	invalid_score.last_score[0U][7U] = 10U;
	auto rejected = ClassicStaffSceneTestAccess::create_with_predecoded_assets(
		invalid_score, cels, palette, stage_text(), compiled_assets(), font,
		trig(), Raster{}
	);
	expect(
		!rejected && rejected.error().code == ClassicStaffErrorCode::invalid_context,
		"invalid source score digits fail before verdict indexing"
	);

	auto invalid_cels = cels;
	invalid_cels.back().pixel_width = 192U;
	rejected = ClassicStaffSceneTestAccess::create_with_predecoded_assets(
		valid_resident, std::move(invalid_cels), palette, stage_text(),
		compiled_assets(), font, trig(), Raster{}
	);
	expect(
		!rejected && rejected.error().code ==
			ClassicStaffErrorCode::invalid_asset_geometry &&
			rejected.error().resource && rejected.error().resource->staff_cdg ==
				MainStaffCdg::staff_12,
		"late wrong staff geometry is named before a scene is published"
	);

	font.missing_ = true;
	rejected = ClassicStaffSceneTestAccess::create_with_predecoded_assets(
		valid_resident, std::move(cels), palette, stage_text(), compiled_assets(),
		font, trig(), Raster{}
	);
	expect(
		!rejected && rejected.error().code == ClassicStaffErrorCode::font_prepare_failed,
		"missing machine glyphs fail before the initial Raster is mutated"
	);

	font.missing_ = false;
	auto scene = create_scene(font);
	expect(static_cast<bool>(scene), "audio guard scene constructs");
	if(scene) {
		expect(static_cast<bool>((*scene)->advance({})),
			"first audio command is queued");
		const auto blocked = (*scene)->advance({});
		expect(
			!blocked && blocked.error().code ==
				ClassicStaffErrorCode::audio_command_pending &&
				(*scene)->phase() == ClassicStaffPhase::prelude,
			"an unconsumed audio command blocks all scene mutation"
		);
	}
}

std::optional<std::vector<std::byte>> read_file(
	const std::filesystem::path &path
)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	if(!input) {
		return std::nullopt;
	}
	const auto end = input.tellg();
	if(end < 0) {
		return std::nullopt;
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(end));
	input.seekg(0, std::ios::beg);
	if(!bytes.empty() && !input.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	)) {
		return std::nullopt;
	}
	return bytes;
}

void test_local_original_closure(
	const std::filesystem::path &content_root,
	const std::filesystem::path &mainl_path,
	const std::optional<std::filesystem::path> &font_rom_path
)
{
	const auto mainl = read_file(mainl_path);
	expect(mainl.has_value(), "local supported MAINL input is readable");
	if(!mainl) {
		return;
	}
	const auto imported = th03::app::import_mainl_stage_text(*mainl);
	expect(imported.has_value(), "local supported MAINL profile authenticates");
	if(!imported) {
		return;
	}
	th03::app::MainResourceLoader loader(content_root);
	const auto closure = loader.load_mainl_staff();
	expect(closure.has_value(), "local staff resource closure authenticates");
	if(!closure) {
		return;
	}
	SyntheticFont synthetic_font;
	const AnkGlyphProvider *font = &synthetic_font;
	std::optional<th03::app::Pc98FontRomProvider> authentic_font{};
	if(font_rom_path) {
		const auto bytes = read_file(*font_rom_path);
		expect(bytes.has_value(), "local FONT.ROM input is readable");
		if(!bytes) {
			return;
		}
		auto created = th03::app::Pc98FontRomProvider::from_exact_font_rom(*bytes);
		expect(created.has_value(), "local FONT.ROM authenticates");
		if(!created) {
			return;
		}
		authentic_font.emplace(std::move(*created));
		font = &*authentic_font;
	}
	const auto created = ClassicStaffScene::create(
		resident(), *closure, imported->assets, imported->staff, *font, trig(),
		Raster{}
	);
	expect(
		created.has_value(),
		"local staff palette and all twelve CDGs decode at source geometry"
	);
	if(!font_rom_path) {
		return;
	}
	const auto english = ClassicStaffScene::create(
		resident(), *closure, imported->assets, imported->staff, *font, trig(),
		Raster{}, false, &th03::app::new_classic_builtin_english_game_text()
	);
	const auto japanese = ClassicStaffScene::create(
		resident(), *closure, imported->assets, imported->staff, *font, trig(),
		Raster{}, false, &th03::app::new_classic_builtin_japanese_game_text()
	);
	expect(
		english.has_value() && japanese.has_value(),
		"authentic English and Japanese staff cels fit every source geometry"
	);
	if(created && english && japanese) {
		expect(
			same_cel(
				ClassicStaffSceneTestAccess::cel(**created, 7U),
				ClassicStaffSceneTestAccess::cel(**english, 7U)
			) && same_cel(
				ClassicStaffSceneTestAccess::cel(**created, 7U),
				ClassicStaffSceneTestAccess::cel(**japanese, 7U)
			),
			"authentic STF8 decorative art is unchanged in both locales"
		);
	}
}

} // namespace

int main(int argc, char **argv)
{
	test_complete_schedule_and_verdict();
	test_new_classic_localized_staff_art();
	test_music_off_volume_wait_escape();
	test_current_audio_gate_and_reset_lifetime();
	test_fast_forward_unlock_and_frame_policy();
	test_overlay_forces_alpha_after_performance_downgrade();
	test_atomic_rejections_and_audio_guard();
	if(argc == 3 || argc == 4) {
		test_local_original_closure(
			argv[1], argv[2],
			argc == 4 ? std::optional<std::filesystem::path>{argv[3]} : std::nullopt
		);
	} else {
		expect(
			argc == 1,
			"optional local gate accepts content root, MAINL, and FONT.ROM"
		);
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
