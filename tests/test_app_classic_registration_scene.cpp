#include "../src/app/classic_registration_scene_internal.hpp"
#include "../src/app/classic_mainl_continue_internal.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../src/app/mainl_compiled_text_internal.hpp"

namespace {

namespace fs = std::filesystem;
using th03::GameInit;
using th03::app::ClassicAudioCommandType;
using th03::app::ClassicMusicCue;
using th03::app::ClassicOpRoute;
using th03::app::ClassicOpSelectionMode;
using th03::app::ClassicRegistrationBoundary;
using th03::app::ClassicRegistrationErrorCode;
using th03::app::ClassicRegistrationGlyph;
using th03::app::ClassicRegistrationInput;
using th03::app::ClassicRegistrationPhase;
using th03::app::ClassicRegistrationScene;
using th03::app::ClassicRegistrationSceneTestAccess;
using th03::app::ClassicMainlContinuePhase;
using th03::app::ClassicMainlContinueScene;
using th03::app::ClassicScoreCleared;
using th03::app::ClassicScoreCodecFields;
using th03::app::ClassicScoreStoreDisposition;
using th03::app::MainlStageTextAssets;
using th03::app::MainlRegistrationNextScreen;
using th03::present::AnkGlyph;
using th03::present::AnkGlyphProvider;
using th03::present::JisGlyph;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;

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

class TemporaryRoot final {
public:
	TemporaryRoot()
	{
		const auto parent = fs::temp_directory_path();
		const auto nonce = std::chrono::high_resolution_clock::now()
			.time_since_epoch().count();
		for(std::uint32_t suffix = 0U; suffix < 1000U; ++suffix) {
			path_ = parent / ("th03-registration-scene-test-" +
				std::to_string(nonce) + "-" + std::to_string(suffix));
			std::error_code error;
			if(fs::create_directory(path_, error)) {
				parent_ = path_.parent_path();
				return;
			}
		}
		path_.clear();
	}

	~TemporaryRoot() { erase(); }

	void erase() noexcept
	{
		if(path_.empty() || path_.parent_path() != parent_ ||
			path_.filename().string().rfind(
				"th03-registration-scene-test-", 0U
			) != 0U) {
			return;
		}
		std::error_code error;
		fs::remove_all(path_, error);
	}

	[[nodiscard]] const fs::path &path() const noexcept { return path_; }
	[[nodiscard]] bool valid() const noexcept { return !path_.empty(); }

private:
	fs::path path_{};
	fs::path parent_{};
};

class SyntheticFont final : public AnkGlyphProvider {
public:
	[[nodiscard]] std::optional<AnkGlyph> glyph(
		std::uint8_t
	) const override
	{
		return std::nullopt;
	}

	[[nodiscard]] std::optional<JisGlyph> jis_glyph(
		std::uint16_t canonical_jis
	) const override
	{
		if(missing_) {
			return std::nullopt;
		}
		JisGlyph result{};
		result.row_halves[0U] = static_cast<std::uint8_t>(
			0x80U >> (canonical_jis & 0x07U)
		);
		return result;
	}

	bool missing_ = false;
};

MainlStageTextAssets stage_text()
{
	MainlStageTextAssets result{};
	for(std::size_t index = 0U; index < result.characters.size(); ++index) {
		result.characters[index].name = {
			static_cast<std::uint8_t>('A' + index),
		};
	}
	for(std::size_t index = 0U;
		index < result.registration_playchars.size();
		++index) {
		result.registration_playchars[index].assign(12U, ' ');
	}
	return result;
}

th03::assets::PiImage background(
	std::uint8_t fill = 1U,
	std::uint8_t palette_seed = 5U
)
{
	th03::assets::PiImage result{};
	result.palette.emplace();
	for(std::uint8_t index = 0U; index < 16U; ++index) {
		result.palette->colors[index] = {
			.r = index,
			.g = static_cast<std::uint8_t>(15U - index),
			.b = static_cast<std::uint8_t>(index ^ palette_seed),
		};
	}
	result.image.width = th03::present::graphics_width;
	result.image.height = th03::present::graphics_height;
	result.image.indices.assign(th03::present::graphics_page_pixels, fill);
	return result;
}

std::array<std::optional<th03::assets::PlanarCel4>, 4U> choice_cels()
{
	std::array<std::optional<th03::assets::PlanarCel4>, 4U> result{};
	constexpr std::size_t plane_size = 492U;
	for(std::size_t cel = 0U; cel < result.size(); ++cel) {
		th03::assets::PlanarCel4 value{};
		value.bitplane_size = static_cast<std::uint16_t>(plane_size);
		value.pixel_width = 96U;
		value.pixel_height = 41U;
		value.offset_at_bottom_left = 3200;
		value.vram_dword_width = 3U;
		const auto color = static_cast<std::uint8_t>(cel + 1U);
		for(std::size_t plane = 0U; plane < value.color_planes.size(); ++plane) {
			value.color_planes[plane].assign(
				plane_size,
				(color & (1U << plane)) != 0U ? 0xFFU : 0U
			);
		}
		result[cel].emplace(std::move(value));
	}
	return result;
}

th03::assets::PlanarCel4 rank_cel()
{
	constexpr std::size_t plane_size = 3520U;
	th03::assets::PlanarCel4 result{};
	result.bitplane_size = static_cast<std::uint16_t>(plane_size);
	result.pixel_width = 320U;
	result.pixel_height = 88U;
	result.offset_at_bottom_left = 6960;
	result.vram_dword_width = 10U;
	for(std::size_t plane = 0U; plane < result.color_planes.size(); ++plane) {
		result.color_planes[plane].assign(
			plane_size, plane == 1U ? 0x80U : 0U
		);
	}
	result.alpha.emplace(plane_size, static_cast<std::uint8_t>(0x80U));
	return result;
}

th03::assets::BfntSheet glyph_sheet(std::uint8_t color)
{
	constexpr std::size_t plane_size = 128U;
	th03::assets::BfntSheet result{};
	result.consumer = th03::assets::BfntConsumerPolicy::superimpose;
	result.header.pixel_width = 32U;
	result.header.pixel_height = 32U;
	result.header.first_code = 0U;
	result.header.last_code = 48U;
	result.transparent_color = 0U;
	result.glyphs.reserve(49U);
	for(std::uint16_t code = 0U; code <= 48U; ++code) {
		th03::assets::PlanarBitmap4 pixels{};
		for(std::size_t plane = 0U; plane < pixels.color_planes.size(); ++plane) {
			pixels.color_planes[plane].assign(
				plane_size,
				(color & (1U << plane)) != 0U ? 0xFFU : 0U
			);
		}
		result.glyphs.push_back({.code = code, .pixels = std::move(pixels)});
	}
	return result;
}

GameInit resident()
{
	GameInit result{};
	result.game_mode = th03::game_mode::story;
	result.rank = 2U;
	result.story_stage = 5U;
	result.remaining_credits = 1U;
	result.paletted_character = {1, 18};
	result.resident_rand = std::bit_cast<std::int32_t>(0x12345678U);
	result.last_score[0U] = {0U, 0U, 0U, 0U, 0U, 0U, 0U, 1U};
	return result;
}

GameInit viewer_resident(std::uint8_t remaining_credits = 2U)
{
	auto result = resident();
	result.game_mode = th03::game_mode::none;
	result.story_stage = 0xFFU;
	result.show_score_menu = 1U;
	result.remaining_credits = remaining_credits;
	result.paletted_character = {-1, -1};
	result.last_score[0U].fill(9U);
	return result;
}

bool write_stock(const fs::path &path)
{
	return th03::app::commit_classic_score_store(
		path, th03::app::classic_score_stock_file()
	).has_value();
}

std::expected<
	std::unique_ptr<ClassicRegistrationScene>,
	th03::app::ClassicRegistrationError
> create_scene(
	const GameInit &value,
	const fs::path &score_path,
	SyntheticFont &font
)
{
	const bool preload_continue = value.remaining_credits != 0U &&
		value.story_stage != 99U;
	return ClassicRegistrationSceneTestAccess::create_with_predecoded_assets(
		value, score_path, background(), rank_cel(), glyph_sheet(4U),
		glyph_sheet(15U),
		preload_continue ? background(5U, 1U) : background(9U, 7U),
		preload_continue ? choice_cels() :
			std::array<std::optional<th03::assets::PlanarCel4>, 4U>{},
		stage_text(), font, Raster{}
	);
}

std::expected<
	std::unique_ptr<ClassicRegistrationScene>,
	th03::app::ClassicRegistrationError
> create_viewer_scene(
	const GameInit &value,
	const fs::path &score_path,
	SyntheticFont &font
)
{
	const bool preload_continue = value.remaining_credits != 0U;
	return ClassicRegistrationSceneTestAccess::create_viewer_with_predecoded_assets(
		value, score_path, background(), rank_cel(), glyph_sheet(4U),
		glyph_sheet(15U),
		preload_continue ? background(5U, 1U) : background(9U, 7U),
		preload_continue ? choice_cels() :
			std::array<std::optional<th03::assets::PlanarCel4>, 4U>{},
		stage_text(), font
	);
}

void drain_initial_audio(ClassicRegistrationScene &scene)
{
	const auto load = scene.take_audio_command();
	const auto play = scene.take_audio_command();
	expect(
		load && load->type == ClassicAudioCommandType::load_song &&
			load->cue == ClassicMusicCue::high_score,
		"registration queues SCORE.M load first"
	);
	expect(
		play && play->type == ClassicAudioCommandType::play_song &&
			play->cue == ClassicMusicCue::high_score &&
			!scene.take_audio_command(),
		"registration queues SCORE.M playback second"
	);
}

bool advance_fade(
	ClassicRegistrationScene &scene,
	ClassicRegistrationPhase final_phase
)
{
	for(std::uint8_t boundary = 0U; boundary < 35U; ++boundary) {
		const auto tick = scene.advance({});
		expect(
			tick && tick->boundary == ClassicRegistrationBoundary::vsync,
			"each speed-2 palette fade step consumes one source VSync"
		);
		if(!tick) {
			return false;
		}
		const bool source_palette_show = boundary != 0U &&
			((boundary % 2U) == 1U || boundary == 34U);
		expect(
			tick->surface_changed == source_palette_show,
			"speed-2 fade reports only source palette-show boundaries"
		);
		if(boundary < 34U) {
			expect(
				tick->phase != final_phase,
				"palette fade does not finish before boundary 35"
			);
		}
	}
	expect(
		scene.phase() == final_phase,
		"palette fade finishes on the exact 35th VSync boundary"
	);
	return scene.phase() == final_phase;
}

std::vector<ClassicScoreCodecFields> source_codecs(
	std::uint32_t seed,
	std::size_t count
)
{
	auto next = [&seed]() {
		seed = (seed * 0x015A4E35U) + 1U;
		return static_cast<std::uint8_t>((seed >> 16U) & 0x7FFFU);
	};
	std::vector<ClassicScoreCodecFields> result;
	result.reserve(count);
	for(std::size_t index = 0U; index < count; ++index) {
		const auto key1 = next();
		const auto key2 = next();
		const auto unknown = next();
		result.push_back({.unknown = unknown, .key1 = key1, .key2 = key2});
	}
	return result;
}

void test_qualified_name_schedule_and_commit()
{
	TemporaryRoot root;
	expect(root.valid(), "qualified registration temporary root exists");
	if(!root.valid()) {
		return;
	}
	const auto score_path = root.path() / "YUME.NEM";
	expect(write_stock(score_path), "qualified fixture writes stock score file");
	SyntheticFont font;
	auto scene = create_scene(resident(), score_path, font);
	expect(scene.has_value(), "qualified registration scene constructs");
	if(!scene) {
		return;
	}
	expect(
		(*scene)->entered_place() == 0U,
		"candidate higher than stock first place enters at place zero"
	);
	for(const auto page : {Page::zero, Page::one}) {
		expect(
			(*scene)->raster().pixel(page, {320, 316}) == 2U &&
				(*scene)->raster().pixel(page, {321, 316}) == 1U,
			"rank plate alpha preserves REGIB pixels on both retained pages"
		);
	}
	for(std::size_t index = 0U; index < 10U; ++index) {
		expect(
			ClassicRegistrationSceneTestAccess::playchar_pixel_width(
				**scene, index
			) == 96U,
			"registration character labels retain the six-cell source width"
		);
	}
	const auto blocked = (*scene)->advance({});
	expect(
		!blocked && blocked.error().code ==
			ClassicRegistrationErrorCode::audio_command_pending &&
			(*scene)->phase() == ClassicRegistrationPhase::black_in,
		"unconsumed audio commands block all scene mutation"
	);
	drain_initial_audio(**scene);
	if(!advance_fade(**scene, ClassicRegistrationPhase::name_entry)) {
		return;
	}
	expect(
		(*scene)->raster().palette_tone() == 100,
		"black-in leaves the source palette tone at 100"
	);

	for(std::size_t letter = 0U; letter < 8U; ++letter) {
		const auto entered = (*scene)->advance({.ok = true});
		expect(
			entered && entered->boundary == ClassicRegistrationBoundary::vsync &&
				entered->input_sampled && entered->name_changed,
			"each accepted letter consumes one polled frame"
		);
		if(letter < 7U) {
			const auto released = (*scene)->advance({});
			expect(
				released && released->phase ==
					ClassicRegistrationPhase::name_entry,
				"OK edge lock releases between letters"
			);
		}
	}
	expect(
		(*scene)->phase() == ClassicRegistrationPhase::finalize_name &&
			ClassicRegistrationSceneTestAccess::name_cursor_backwards(**scene) == 0U,
		"the eighth glyph preserves the source final name-entry frame without cursor underflow"
	);
	const auto &raw_row = ClassicRegistrationSceneTestAccess::document(**scene)
		.boards[2U].rows[0U];
	std::array<ClassicRegistrationGlyph, 8U> raw_name{};
	raw_name.fill(ClassicRegistrationGlyph::a);
	expect(
		raw_row.name_backwards == raw_name,
		"the final name-entry VSync still shows the source uniform name"
	);
	const auto raw_page = (*scene)->raster().graphics(Page::zero);
	const std::vector raw_pixels(raw_page.begin(), raw_page.end());

	const auto held = (*scene)->advance({.ok = true});
	expect(
		held && held->phase == ClassicRegistrationPhase::wait_release &&
			held->boundary == ClassicRegistrationBoundary::vsync,
		"post-boundary finalization replaces the name before waiting for release"
	);
	expect(
		!std::equal(
			raw_pixels.begin(), raw_pixels.end(),
			(*scene)->raster().graphics(Page::zero).begin()
		),
		"uniform-name replacement changes the retained raster only after the final name-entry VSync"
	);
	const auto &row = ClassicRegistrationSceneTestAccess::document(**scene)
		.boards[2U].rows[0U];
	const std::array expected_backwards{
		ClassicRegistrationGlyph::space,
		ClassicRegistrationGlyph::space,
		ClassicRegistrationGlyph::space,
		ClassicRegistrationGlyph::u,
		ClassicRegistrationGlyph::m,
		ClassicRegistrationGlyph::i,
		ClassicRegistrationGlyph::e,
		ClassicRegistrationGlyph::r,
	};
	expect(
		row.name_backwards == expected_backwards,
		"uniform eight-letter input is replaced by the source character default"
	);

	const auto released = (*scene)->advance({});
	expect(
		released && released->phase == ClassicRegistrationPhase::wait_press &&
			released->boundary == ClassicRegistrationBoundary::input_poll,
		"release is accepted without an invented VSync delay"
	);
	const auto idle = (*scene)->advance({});
	expect(
		idle && idle->phase == ClassicRegistrationPhase::wait_press &&
			idle->boundary == ClassicRegistrationBoundary::vsync,
		"dismissal wait consumes VSyncs while no input is pressed"
	);
	const auto pressed = (*scene)->advance({.bomb = true});
	expect(
		pressed && pressed->phase == ClassicRegistrationPhase::black_out &&
			pressed->boundary == ClassicRegistrationBoundary::input_poll &&
			!(*scene)->take_audio_command(),
		"ordinary loss registration enters black-out without fading SCORE.M"
	);
	if(!advance_fade(**scene, ClassicRegistrationPhase::ready)) {
		return;
	}
	expect(
		(*scene)->raster().pixel(Page::zero, Point{0, 0}) == 5U &&
			(*scene)->raster().pending_palette().colors[0U] ==
				th03::present::Rgb8{0U, 255U, 17U},
		"registration preloads CONTI pixels and palette under black"
	);
	auto completion = (*scene)->take_completion();
	expect(
		completion && completion->entered_place() == 0U &&
			completion->next_screen() ==
				MainlRegistrationNextScreen::continue_menu &&
			completion->load_disposition() ==
				ClassicScoreStoreDisposition::loaded &&
			completion->raster().palette_tone() == 0,
		"completion owns the black retained Raster and qualification result"
	);
	if(completion) {
		auto continue_scene = ClassicMainlContinueScene::create_from_registration(
			std::move(*completion), resident().remaining_credits, font
		);
		expect(
			continue_scene && (*continue_scene)->phase() ==
				ClassicMainlContinuePhase::black_in &&
				(*continue_scene)->raster().pixel(Page::zero, {0, 0}) == 5U &&
				(*continue_scene)->raster().pixel(Page::zero, {192, 272}) == 1U &&
				(*continue_scene)->raster().pixel(Page::zero, {352, 272}) == 4U,
			"Continue consumes the registration preload without recreating its Raster"
		);
	}
	const auto loaded = th03::app::load_classic_score_store(score_path);
	const auto codecs = source_codecs(0x12345678U, 1U);
	expect(
		loaded && loaded->document.boards[2U].rows[0U].name_backwards ==
			expected_backwards && loaded->document.codec[2U] == codecs[0U],
		"final boundary atomically commits the mutated board and first source RNG codec"
	);
}

void test_direction_repeat_and_doublewide_navigation()
{
	TemporaryRoot root;
	expect(root.valid(), "direction temporary root exists");
	if(!root.valid()) {
		return;
	}
	const auto score_path = root.path() / "YUME.NEM";
	expect(write_stock(score_path), "direction fixture writes stock score file");
	SyntheticFont font;
	auto scene = create_scene(resident(), score_path, font);
	expect(scene.has_value(), "direction registration scene constructs");
	if(!scene) {
		return;
	}
	drain_initial_audio(**scene);
	if(!advance_fade(**scene, ClassicRegistrationPhase::name_entry)) {
		return;
	}
	for(std::size_t poll = 0U; poll < 33U; ++poll) {
		expect(
			static_cast<bool>((*scene)->advance({.right = true})),
			"held-right name-entry poll advances"
		);
	}
	expect(
		ClassicRegistrationSceneTestAccess::selected(**scene) ==
			ClassicRegistrationGlyph::c &&
			ClassicRegistrationSceneTestAccess::direction_hold(**scene)[3U] == 33U,
		"held direction triggers at polls 0 and 32, not at poll 30"
	);

	for(std::size_t step = 0U; step < 10U; ++step) {
		expect(static_cast<bool>((*scene)->advance({})),
			"direction edge release advances");
		expect(static_cast<bool>((*scene)->advance({.right = true})),
			"direction edge press advances");
	}
	expect(
		ClassicRegistrationSceneTestAccess::selected(**scene) ==
			ClassicRegistrationGlyph::m,
		"edge navigation reaches M from C"
	);
	expect(static_cast<bool>((*scene)->advance({})),
		"M navigation releases right");
	expect(static_cast<bool>((*scene)->advance({.right = true})) &&
		ClassicRegistrationSceneTestAccess::selected(**scene) ==
			ClassicRegistrationGlyph::space,
		"M skips the hidden second half into the doublewide SP glyph"
	);
	expect(static_cast<bool>((*scene)->advance({})),
		"SP navigation releases right");
	expect(static_cast<bool>((*scene)->advance({.right = true})) &&
		ClassicRegistrationSceneTestAccess::selected(**scene) ==
			ClassicRegistrationGlyph::a,
		"right from doublewide SP wraps to the row start"
	);
}

void test_missing_file_clear_and_recreate_rng_schedule()
{
	TemporaryRoot root;
	expect(root.valid(), "missing-file temporary root exists");
	if(!root.valid()) {
		return;
	}
	auto value = resident();
	value.story_stage = 99U;
	value.remaining_credits = 3U;
	value.last_score[0U].fill(0U);
	const auto score_path = root.path() / "YUME.NEM";
	SyntheticFont font;
	auto scene = create_scene(value, score_path, font);
	expect(scene.has_value(), "missing score file is recreated before presentation");
	if(!scene) {
		return;
	}
	expect(
		!(*scene)->entered_place() && fs::is_regular_file(score_path),
		"non-qualifying clear route publishes stock persistence and no name entry"
	);
	drain_initial_audio(**scene);
	if(!advance_fade(**scene, ClassicRegistrationPhase::wait_release)) {
		return;
	}
	expect(static_cast<bool>((*scene)->advance({})) &&
		(*scene)->phase() == ClassicRegistrationPhase::wait_press,
		"no-qualification route still performs release gating");
	expect(static_cast<bool>((*scene)->advance({.ok = true})) &&
		(*scene)->phase() == ClassicRegistrationPhase::black_out,
		"no-qualification route accepts the next press");
	const auto fade = (*scene)->take_audio_command();
	expect(
		fade && fade->type == ClassicAudioCommandType::fade_song &&
			fade->cue == ClassicMusicCue::high_score && fade->parameter == 16U,
		"stage 99 queues the source long SCORE.M fade"
	);
	if(!advance_fade(**scene, ClassicRegistrationPhase::wait_volume)) {
		return;
	}
	const auto waiting = (*scene)->advance({}, {.volume = 254U});
	expect(
		waiting && waiting->phase == ClassicRegistrationPhase::wait_volume &&
			waiting->boundary == ClassicRegistrationBoundary::audio_volume,
		"stage 99 waits for PMD's exact faded volume without inventing a frame"
	);
	const auto silenced = (*scene)->advance({}, {.volume = 255U});
	expect(
		silenced && silenced->phase == ClassicRegistrationPhase::ready &&
			silenced->boundary == ClassicRegistrationBoundary::immediate,
		"volume 255 queues the source stop immediately"
	);
	expect(
		!(*scene)->take_completion(),
		"the owner cannot publish completion before consuming the source stop"
	);
	const auto stop = (*scene)->take_audio_command();
	expect(
		stop && stop->type == ClassicAudioCommandType::stop_song &&
			stop->cue == ClassicMusicCue::high_score,
		"stage 99 stops SCORE.M only after the volume wait"
	);
	const auto loaded = th03::app::load_classic_score_store(score_path);
	const auto codecs = source_codecs(0x12345678U, 5U);
	bool stock_codecs_match = static_cast<bool>(loaded);
	if(loaded) {
		for(std::size_t rank = 0U; rank < 4U; ++rank) {
			if(rank != value.rank) {
				stock_codecs_match = stock_codecs_match &&
					loaded->document.codec[rank] == codecs[rank];
			}
		}
	}
	expect(
		loaded && stock_codecs_match &&
			loaded->document.codec[value.rank] == codecs[4U] &&
			loaded->document.boards[value.rank].cleared ==
				ClassicScoreCleared::cleared,
		"recreation consumes four codec triplets before the selected-rank save and records the 1CC"
	);
	auto completion = (*scene)->take_completion();
	expect(
		completion && completion->load_disposition() ==
			ClassicScoreStoreDisposition::created_stock &&
			completion->next_screen() ==
				MainlRegistrationNextScreen::game_over &&
			completion->raster().pixel(Page::zero, Point{0, 0}) == 9U &&
			completion->raster().pending_palette().colors[0U] ==
				th03::present::Rgb8{0U, 255U, 119U},
		"stage 99 completion reports recreation and carries preloaded OVER"
	);
}

void test_view_only_schedule_and_unchanged_board()
{
	TemporaryRoot root;
	expect(root.valid(), "viewer temporary root exists");
	if(!root.valid()) {
		return;
	}
	const auto score_path = root.path() / "YUME.NEM";
	expect(write_stock(score_path), "viewer fixture writes stock score file");
	const auto before = th03::app::load_classic_score_store(score_path);
	expect(before.has_value(), "viewer fixture reloads its source document");
	if(!before) {
		return;
	}

	SyntheticFont font;
	const auto value = viewer_resident();
	auto scene = create_viewer_scene(value, score_path, font);
	expect(scene.has_value(), "typed view-only registration scene constructs");
	if(!scene) {
		return;
	}
	expect(
		ClassicRegistrationSceneTestAccess::view_only(**scene) &&
			!(*scene)->entered_place() &&
			ClassicRegistrationSceneTestAccess::document(**scene).boards ==
				before->document.boards,
		"viewer skips candidate insertion despite a synthetic winning last score"
	);
	drain_initial_audio(**scene);
	if(!advance_fade(**scene, ClassicRegistrationPhase::wait_release)) {
		return;
	}
	const auto held = (*scene)->advance({.shot = true});
	expect(
		held && held->phase == ClassicRegistrationPhase::wait_release &&
			held->boundary == ClassicRegistrationBoundary::vsync,
		"viewer waits for the held title confirmation to release"
	);
	const auto released = (*scene)->advance({});
	expect(
		released && released->phase == ClassicRegistrationPhase::wait_press &&
			released->boundary == ClassicRegistrationBoundary::input_poll,
		"viewer accepts the exact release boundary without a VSync"
	);
	const auto dismissed = (*scene)->advance({.cancel = true});
	expect(
		dismissed && dismissed->phase == ClassicRegistrationPhase::black_out &&
			dismissed->boundary == ClassicRegistrationBoundary::input_poll &&
			!(*scene)->take_audio_command(),
		"nonzero-credit viewer dismisses on Cancel without fading SCORE.M"
	);
	if(!advance_fade(**scene, ClassicRegistrationPhase::ready)) {
		return;
	}
	expect(
		(*scene)->raster().pixel(Page::zero, Point{0, 0}) == 5U,
		"nonzero-credit viewer preserves the source's unused CONTI preload"
	);
	auto completion = (*scene)->take_completion();
	expect(
		completion && !completion->entered_place() &&
			completion->next_screen() ==
				MainlRegistrationNextScreen::continue_menu &&
			completion->resident().story_stage == 0xFFU &&
			completion->resident().show_score_menu == 1U &&
			completion->resident().game_mode == th03::game_mode::none,
		"viewer completion returns the exact non-registering resident capability"
	);
	const auto after = th03::app::load_classic_score_store(score_path);
	const auto codecs = source_codecs(0x12345678U, 1U);
	expect(
		after && after->document.boards == before->document.boards &&
			after->document.codec[2U] == codecs[0U],
		"viewer rewrites an unchanged board with the source's fresh rank codec"
	);

	const auto zero_path = root.path() / "YUME0.NEM";
	expect(write_stock(zero_path), "zero-credit viewer fixture writes score file");
	auto zero = create_viewer_scene(viewer_resident(0U), zero_path, font);
	expect(zero.has_value(), "zero-credit viewer constructs");
	if(!zero) {
		return;
	}
	drain_initial_audio(**zero);
	if(!advance_fade(**zero, ClassicRegistrationPhase::wait_release)) {
		return;
	}
	expect(static_cast<bool>((*zero)->advance({})),
		"zero-credit viewer accepts release");
	expect(static_cast<bool>((*zero)->advance({.ok = true})),
		"zero-credit viewer accepts dismissal");
	const auto fade = (*zero)->take_audio_command();
	expect(
		fade && fade->type == ClassicAudioCommandType::fade_song &&
			fade->parameter == 16U,
		"zero-credit viewer preserves the conditional long SCORE.M fade"
	);
	if(!advance_fade(**zero, ClassicRegistrationPhase::wait_volume)) {
		return;
	}
	expect(
		static_cast<bool>((*zero)->advance({}, {.volume = 255U})),
		"zero-credit viewer reaches ready at PMD silence"
	);
	const auto stop = (*zero)->take_audio_command();
	expect(
		stop && stop->type == ClassicAudioCommandType::stop_song,
		"zero-credit viewer stops SCORE.M after the volume wait"
	);
	auto zero_completion = (*zero)->take_completion();
	expect(
		zero_completion && zero_completion->next_screen() ==
			MainlRegistrationNextScreen::game_over &&
			zero_completion->raster().pixel(Page::zero, Point{0, 0}) == 9U,
		"zero-credit viewer preserves the source's unused OVER preload"
	);
}

void test_zero_credit_audio_exit()
{
	TemporaryRoot root;
	expect(root.valid(), "zero-credit temporary root exists");
	if(!root.valid()) {
		return;
	}
	const auto score_path = root.path() / "YUME.NEM";
	expect(write_stock(score_path), "zero-credit fixture writes stock score file");
	auto value = resident();
	value.remaining_credits = 0U;
	value.last_score[0U].fill(0U);
	SyntheticFont font;
	auto scene = create_scene(value, score_path, font);
	expect(scene.has_value(), "zero-credit registration scene constructs");
	if(!scene) {
		return;
	}
	drain_initial_audio(**scene);
	if(!advance_fade(**scene, ClassicRegistrationPhase::wait_release)) {
		return;
	}
	expect(static_cast<bool>((*scene)->advance({})),
		"zero-credit registration accepts release");
	expect(static_cast<bool>((*scene)->advance({.cancel = true})),
		"source Cancel dismisses zero-credit registration");
	const auto fade = (*scene)->take_audio_command();
	expect(
		fade && fade->type == ClassicAudioCommandType::fade_song &&
			fade->parameter == 16U,
		"zero credits select the source long SCORE.M fade"
	);
	if(!advance_fade(**scene, ClassicRegistrationPhase::wait_volume)) {
		return;
	}
	expect(
		static_cast<bool>((*scene)->advance({}, {.volume = 255U})) &&
			(*scene)->phase() == ClassicRegistrationPhase::ready,
		"zero-credit registration reaches ready only after faded silence"
	);
	const auto stop = (*scene)->take_audio_command();
	expect(
		stop && stop->type == ClassicAudioCommandType::stop_song,
		"zero-credit registration stops SCORE.M before Game Over handoff"
	);
	auto completion = (*scene)->take_completion();
	expect(
		completion && completion->next_screen() ==
			MainlRegistrationNextScreen::game_over &&
			completion->raster().pixel(Page::zero, Point{0, 0}) == 9U,
		"zero-credit completion publishes preloaded OVER only after stop"
	);
}

void test_rejections_and_commit_failure()
{
	TemporaryRoot root;
	expect(root.valid(), "rejection temporary root exists");
	if(!root.valid()) {
		return;
	}
	SyntheticFont font;
	auto invalid = resident();
	invalid.story_stage = 0U;
	const auto untouched = root.path() / "untouched.nem";
	auto rejected = create_scene(invalid, untouched, font);
	expect(
		!rejected && rejected.error().code ==
			ClassicRegistrationErrorCode::invalid_context &&
			!fs::exists(untouched),
		"invalid Story context fails before touching persistence"
	);

	auto invalid_viewer = th03::app::ClassicOpSceneTestAccess::completion(
		ClassicOpRoute::demo, viewer_resident()
	);
	auto rejected_viewer = ClassicRegistrationScene::create_viewer(
		invalid_viewer, untouched, {}, stage_text(), font
	);
	expect(
		!rejected_viewer && rejected_viewer.error().code ==
			ClassicRegistrationErrorCode::invalid_context &&
			!fs::exists(untouched),
		"viewer rejects a non-HiScore OP capability before resources or persistence"
	);
	auto selection_viewer = th03::app::ClassicOpSceneTestAccess::completion(
		ClassicOpRoute::high_scores, viewer_resident(),
		static_cast<ClassicOpSelectionMode>(0U)
	);
	rejected_viewer = ClassicRegistrationScene::create_viewer(
		selection_viewer, untouched, {}, stage_text(), font
	);
	expect(
		!rejected_viewer && rejected_viewer.error().code ==
			ClassicRegistrationErrorCode::invalid_context &&
			!fs::exists(untouched),
		"viewer rejects a selection-bearing OP capability before side effects"
	);

	auto bad_rank = rank_cel();
	bad_rank.pixel_height = 87U;
	rejected = ClassicRegistrationSceneTestAccess::create_with_predecoded_assets(
		resident(), untouched, background(), std::move(bad_rank), glyph_sheet(4U),
		glyph_sheet(15U), background(5U, 1U), choice_cels(), stage_text(), font,
		Raster{}
	);
	expect(
		!rejected && rejected.error().code ==
			ClassicRegistrationErrorCode::invalid_asset_geometry &&
			!fs::exists(untouched),
		"invalid late asset geometry fails before touching persistence"
	);

	font.missing_ = true;
	rejected = create_scene(resident(), untouched, font);
	expect(
		!rejected && rejected.error().code ==
			ClassicRegistrationErrorCode::font_prepare_failed &&
			!fs::exists(untouched),
		"missing machine-font glyph fails before touching persistence"
	);
	font.missing_ = false;

	auto no_place = resident();
	no_place.last_score[0U].fill(0U);
	const auto doomed = root.path() / "doomed.nem";
	expect(write_stock(doomed), "commit-failure fixture writes stock score file");
	auto scene = create_scene(no_place, doomed, font);
	expect(scene.has_value(), "commit-failure scene constructs");
	if(!scene) {
		return;
	}
	drain_initial_audio(**scene);
	if(!advance_fade(**scene, ClassicRegistrationPhase::wait_release)) {
		return;
	}
	expect(static_cast<bool>((*scene)->advance({})),
		"commit-failure route accepts release");
	expect(static_cast<bool>((*scene)->advance({.ok = true})),
		"commit-failure route accepts dismissal");
	root.erase();
	expect(!fs::exists(root.path()),
		"commit-failure fixture removes its exact temporary directory");
	for(std::uint8_t boundary = 0U; boundary < 34U; ++boundary) {
		expect(
			static_cast<bool>((*scene)->advance({})),
			"black-out remains in-memory before final commit boundary"
		);
	}
	const auto failed = (*scene)->advance({});
	expect(!failed, "final-boundary I/O failure is reported");
	expect(
		!failed && failed.error().code ==
			ClassicRegistrationErrorCode::store_commit_failed,
		"final-boundary I/O failure has the named commit error"
	);
	expect(
		(*scene)->phase() == ClassicRegistrationPhase::failed,
		"final-boundary I/O failure moves the scene to failed"
	);
	expect(
		!(*scene)->take_completion(),
		"final-boundary I/O failure publishes no partial completion"
	);
}

std::optional<std::vector<std::byte>> read_file(const fs::path &path)
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

void local_original_gate(
	const fs::path &content_root,
	const fs::path &mainl_path
)
{
	const auto executable = read_file(mainl_path);
	expect(executable.has_value(), "local unpacked MAINL input is readable");
	if(!executable) {
		return;
	}
	const auto imported = th03::app::import_mainl_stage_text(*executable);
	expect(imported.has_value(), "local unpacked MAINL profile authenticates");
	if(!imported) {
		return;
	}
	th03::app::MainResourceLoader loader(content_root);
	const auto closure = loader.load_mainl_registration({
		.rank = th03::app::MainRegistrationRank::hard,
		.next_screen = th03::app::MainlRegistrationNextScreen::continue_menu,
	});
	expect(closure.has_value(), "local registration resource closure authenticates");
	if(!closure) {
		return;
	}
	TemporaryRoot root;
	expect(root.valid(), "local registration gate temporary root exists");
	if(!root.valid()) {
		return;
	}
	const auto score_path = root.path() / "YUME.NEM";
	expect(write_stock(score_path), "local registration gate writes disposable score file");
	SyntheticFont font;
	const auto created = ClassicRegistrationScene::create(
		resident(), score_path, *closure, imported->assets, font, Raster{}
	);
	expect(
		created.has_value(),
		"local REGIB, RFT2, REGI2, and REGI1 assets decode at source geometry"
	);
	if(created) {
		auto capture = [&](
			std::uint8_t raw,
			std::uint16_t width,
			std::uint16_t height
		) {
			std::vector<std::uint8_t> pixels{};
			pixels.reserve(static_cast<std::size_t>(width) * height);
			const auto left = 64 + (static_cast<std::int32_t>(raw % 16U) * 32);
			const auto top = 320 + (static_cast<std::int32_t>(raw / 16U) * 24);
			for(std::int32_t y = 0; y < height; ++y) {
				for(std::int32_t x = 0; x < width; ++x) {
					pixels.push_back(*(*created)->raster().pixel(
						Page::zero, {left + x, top + y}
					));
				}
			}
			return pixels;
		};
		for(std::uint8_t raw = 0U; raw <= 45U; ++raw) {
			if(raw == 13U || raw == 14U || raw == 15U || raw == 29U ||
				raw == 30U || raw == 31U) {
				continue;
			}
			const auto glyph = static_cast<ClassicRegistrationGlyph>(raw);
			const auto height = static_cast<std::uint16_t>(
				raw < 32U ? 24U : 32U
			);
			const auto normal = capture(raw, 32U, height);
			expect(
				ClassicRegistrationSceneTestAccess::draw_alphabet_glyph(
					**created, glyph, true
				) && capture(raw, 32U, height) != normal,
				"authentic ordinary keyboard glyph swaps to REGI1 highlight pixels"
			);
			expect(
				ClassicRegistrationSceneTestAccess::draw_alphabet_glyph(
					**created, glyph, false
				),
				"authentic ordinary keyboard glyph accepts its REGI2 normal redraw"
			);
		}
		for(const auto raw : {
			std::uint8_t{14U}, std::uint8_t{30U}, std::uint8_t{46U},
		}) {
			const auto glyph = static_cast<ClassicRegistrationGlyph>(raw);
			const auto height = static_cast<std::uint16_t>(
				raw < 32U ? 24U : 32U
			);
			const auto normal = capture(raw, 64U, height);
			expect(
				ClassicRegistrationSceneTestAccess::draw_alphabet_glyph(
					**created, glyph, true
				) && capture(raw, 64U, height) != normal,
				"authentic doublewide keyboard glyph swaps both highlight cells"
			);
			expect(
				ClassicRegistrationSceneTestAccess::draw_alphabet_glyph(
					**created, glyph, false
				),
				"authentic doublewide keyboard glyph accepts both normal cells"
			);
		}
	}

	const auto viewer_closure = loader.load_mainl_registration({
		.rank = th03::app::MainRegistrationRank::hard,
		.next_screen = th03::app::MainlRegistrationNextScreen::continue_menu,
	});
	expect(
		viewer_closure.has_value(),
		"local HiScore viewer resource closure authenticates"
	);
	if(!viewer_closure) {
		return;
	}
	auto viewer = viewer_resident();
	viewer.rank = 2U;
	auto completion = th03::app::ClassicOpSceneTestAccess::completion(
		ClassicOpRoute::high_scores, viewer
	);
	const auto created_viewer = ClassicRegistrationScene::create_viewer(
		completion, score_path, *viewer_closure, imported->assets, font
	);
	expect(
		created_viewer.has_value() && !(*created_viewer)->entered_place(),
		"typed OP HiScore capability constructs the authenticated production viewer"
	);
}

} // namespace

int main(int argc, char **argv)
{
	test_qualified_name_schedule_and_commit();
	test_direction_repeat_and_doublewide_navigation();
	test_missing_file_clear_and_recreate_rng_schedule();
	test_view_only_schedule_and_unchanged_board();
	test_zero_credit_audio_exit();
	test_rejections_and_commit_failure();
	if(argc == 3) {
		local_original_gate(argv[1], argv[2]);
	} else {
		expect(argc == 1, "optional local gate accepts content root and MAINL");
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
