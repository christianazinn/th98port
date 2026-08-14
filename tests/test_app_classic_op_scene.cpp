#include "../src/app/classic_op_scene_internal.hpp"
#include "../src/app/classic_op_selection_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

using th03::GameInit;
using th03::app::ClassicAudioCommandType;
using th03::app::ClassicMusicCue;
using th03::app::ClassicOpErrorCode;
using th03::app::ClassicOpInput;
using th03::app::ClassicOpMenuChoice;
using th03::app::ClassicOpPhase;
using th03::app::ClassicOpRoute;
using th03::app::ClassicOpSceneTestAccess;
using th03::app::ClassicOpTitleMenuScene;
using th03::app::ClassicOpTitleMode;
using th03::app::ClassicOpSelectionMode;
using th03::app::ClassicOpVsChoice;
using th03::assets::BfntConsumerPolicy;
using th03::assets::BfntGlyph;
using th03::assets::BfntSheet;
using th03::assets::MonochromeBitmap1;
using th03::assets::PiImage;
using th03::assets::PlanarBitmap4;
using th03::present::Page;

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

PiImage fullscreen_pi(bool title)
{
	PiImage result{};
	result.palette.emplace();
	for(std::size_t index = 0U;
		index < result.palette->colors.size();
		++index) {
		result.palette->colors[index] = {
			static_cast<std::uint8_t>(index),
			static_cast<std::uint8_t>(15U - index),
			static_cast<std::uint8_t>(index / 2U),
		};
	}
	result.image.width = th03::present::graphics_width;
	result.image.height = th03::present::graphics_height;
	result.image.indices.resize(th03::present::graphics_page_pixels);
	for(std::size_t y = 0U; y < result.image.height; ++y) {
		for(std::size_t x = 0U; x < result.image.width; ++x) {
			result.image.indices[(y * result.image.width) + x] = title ?
				static_cast<std::uint8_t>((x / 16U) & 0x0FU) : 4U;
		}
	}
	return result;
}

BfntSheet gaiji_sheet()
{
	BfntSheet sheet{};
	sheet.consumer = BfntConsumerPolicy::gaiji;
	sheet.header.pixel_width = th03::present::gaiji_width;
	sheet.header.pixel_height = th03::present::gaiji_height;
	sheet.header.first_code = 0U;
	sheet.header.last_code = 255U;
	sheet.glyphs.reserve(th03::present::gaiji_count);
	for(std::size_t index = 0U; index < th03::present::gaiji_count; ++index) {
		MonochromeBitmap1 bitmap{};
		bitmap.bits.assign(th03::present::gaiji_glyph_bytes, 0U);
		bitmap.bits[index % bitmap.bits.size()] = 0x80U;
		sheet.glyphs.push_back({
			static_cast<std::uint16_t>(index), std::move(bitmap)
		});
	}
	return sheet;
}

BfntSheet window_sheet()
{
	BfntSheet sheet{};
	sheet.consumer = BfntConsumerPolicy::superimpose;
	sheet.header.pixel_width = 16U;
	sheet.header.pixel_height = 128U;
	sheet.header.first_code = 0U;
	sheet.header.last_code = 2U;
	sheet.transparent_color = 0U;
	constexpr std::size_t plane_size = (16U / 8U) * 128U;
	for(std::uint16_t code = 0U; code < 3U; ++code) {
		PlanarBitmap4 bitmap{};
		for(auto &plane : bitmap.color_planes) {
			plane.assign(plane_size, 0U);
		}
		bitmap.color_planes[(code == 2U) ? 1U : 0U].assign(
			plane_size, 0xFFU
		);
		sheet.glyphs.push_back({code, std::move(bitmap)});
	}
	return sheet;
}

std::unique_ptr<ClassicOpTitleMenuScene> make_scene(
	ClassicOpTitleMode mode,
	std::int32_t resident_rand = 40
)
{
	GameInit resident{};
	resident.resident_rand = resident_rand;
	resident.fast_op_animation =
		(mode == ClassicOpTitleMode::fast_fade) ? 1U : 0U;
	auto created = ClassicOpSceneTestAccess::create_with_predecoded_assets(
		resident, mode, fullscreen_pi(true), fullscreen_pi(false),
		window_sheet(), gaiji_sheet()
	);
	expect(created.has_value(), "synthetic OP scene creates");
	return created ? std::move(*created) : nullptr;
}

void expect_start_audio(ClassicOpTitleMenuScene &scene)
{
	const auto stop = scene.take_audio_command();
	const auto load = scene.take_audio_command();
	expect(
		stop && stop->type == ClassicAudioCommandType::stop_song &&
			stop->cue == ClassicMusicCue::none,
		"OP startup first stops the previous song"
	);
	expect(
		load && load->type == ClassicAudioCommandType::load_song &&
			load->cue == ClassicMusicCue::menu_main,
		"OP startup then loads OP.M through the semantic audio boundary"
	);
	expect(
		!scene.take_audio_command(),
		"startup audio queue drains in source order"
	);
}

void reach_title_wait(ClassicOpTitleMenuScene &scene)
{
	expect(
		scene.phase() == ClassicOpPhase::fast_black,
		"fast title starts at the source black VSync"
	);
	expect(
		!scene.take_audio_command(),
		"fast title does not stop or load audio before the black VSync"
	);
	auto tick = scene.advance();
	expect(
		tick && tick->phase == ClassicOpPhase::fast_initial_pause,
		"fast title consumes the initial palette-show VSync"
	);
	expect_start_audio(scene);
	tick = scene.advance();
	expect(
		tick && tick->phase == ClassicOpPhase::fast_fade &&
			tick->surface_changed,
		"fast title draws TL02 at tone zero before the fade"
	);
	const auto play = scene.take_audio_command();
	expect(
		play && play->type == ClassicAudioCommandType::play_song &&
			play->cue == ClassicMusicCue::menu_main,
		"fast title starts OP.M before the first fade sample"
	);
	for(std::uint16_t frame = 0U; frame < 25U; ++frame) {
		tick = scene.advance();
		expect(tick.has_value(), "fast fade frame advances");
	}
	expect(
		scene.phase() == ClassicOpPhase::wait_title_input &&
			scene.raster().palette_tone() == 100,
		"fast title reaches input after tones 4 through 100"
	);
	expect(
		std::all_of(
			scene.raster().graphics(Page::one).begin(),
			scene.raster().graphics(Page::one).end(),
			[](auto pixel) { return pixel == 4U; }
		),
		"fast title retains a bare TL02 page 1 for later graphics-font unputs"
	);
}

void reach_main_menu(ClassicOpTitleMenuScene &scene)
{
	reach_title_wait(scene);
	const auto accepted = scene.advance({.ok = true});
	expect(
		accepted && accepted->input_sampled &&
			scene.phase() == ClassicOpPhase::box_to_main,
		"title input is sampled before the blocking window animation"
	);
	for(std::uint16_t frame = 0U; frame < 14U; ++frame) {
		const auto box = scene.advance();
		expect(
			box && box->surface_changed,
			"each 8-pixel OP window step consumes one VSync"
		);
	}
	expect(
		scene.phase() == ClassicOpPhase::main_menu,
		"24-to-136-pixel main window expansion takes fourteen steps"
	);
}

void test_fast_title_and_held_first_choice()
{
	auto scene = make_scene(ClassicOpTitleMode::fast_fade);
	if(!scene) {
		return;
	}
	expect(
		scene->resident().fast_op_animation == 0U,
		"fast title toggles the resident flag back to the long path"
	);
	reach_main_menu(*scene);
	const auto selected = scene->advance({.down = true, .ok = true});
	expect(
		selected && !selected->ready && selected->selection_changed &&
			scene->selected_choice() == ClassicOpMenuChoice::versus &&
			scene->phase() == ClassicOpPhase::vs_grow_window,
		"first held menu sample moves and enters the blocking VS submenu"
	);
	for(std::uint16_t frame = 0U; frame < 13U; ++frame) {
		expect(scene->advance().has_value(), "VS submenu grow frame advances");
	}
	expect(
		scene->phase() == ClassicOpPhase::vs_menu &&
			scene->selected_vs_choice() == ClassicOpVsChoice::one_player_cpu,
		"136-to-240-pixel grow publishes the source default VS choice"
	);
	const auto confirmed = scene->advance({.ok = true});
	expect(
		confirmed && confirmed->ready,
		"first VS submenu sample can confirm while held"
	);
	auto completion = scene->take_completion();
	expect(
		completion && completion->route() == ClassicOpRoute::versus &&
			completion->selection_mode() ==
				ClassicOpSelectionMode::vs_1p_cpu &&
			completion->resident().resident_rand == 41,
		"VS handoff omits the outer random increment after a non-returning route"
	);
	if(completion) {
		expect(
			completion->tram().cell({27, 18})->attribute ==
				th03::present::tram_white,
			"MIKOFT VS labels use the source fixed submenu columns"
		);
		expect(
			completion->raster().pixel(Page::zero, {160, 256}) == 1U &&
			completion->raster().pixel(Page::zero, {392, 256}) == 2U,
			"OPWIN left and final right glyphs remain on TL02"
		);
	}
	expect(
		scene->phase() == ClassicOpPhase::consumed,
		"typed OP handoff is single-consumer"
	);
}

void test_replay_patch_title_if_assets_supplied()
{
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
	const auto *path = std::getenv("TH03_AZINN_DAT");
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
	if(!path || *path == '\0') {
		return;
	}
	auto loaded = th03::app::load_new_classic_replay_patch_assets(path);
	expect(loaded.has_value(), "authenticated Replay Patch title assets load");
	if(!loaded) {
		return;
	}
	GameInit resident{};
	resident.resident_rand = 40;
	resident.fast_op_animation = 1U;
	auto assets = std::make_shared<const th03::app::NewClassicReplayPatchAssets>(
		std::move(*loaded)
	);
	auto created = ClassicOpSceneTestAccess::create_with_predecoded_assets(
		resident, ClassicOpTitleMode::fast_fade, fullscreen_pi(true),
		fullscreen_pi(false), window_sheet(), gaiji_sheet(), assets
	);
	expect(created.has_value(), "Replay Patch-profile OP title creates");
	if(!created) {
		return;
	}
	auto &scene = **created;
	reach_main_menu(scene);
	expect(
		scene.raster().pixel(Page::zero, {160, 264}) == 1U &&
		scene.raster().pixel(Page::zero, {288, 264}) == 2U,
		"Replay Patch title uses the source 264-pixel seven-row window"
	);
	for(std::uint8_t step = 0U; step < 4U; ++step) {
		expect(scene.advance({.down = true}).has_value(),
			"Replay Patch main-menu selection advances");
		expect(scene.advance().has_value(),
			"Replay Patch main-menu direction lock releases");
	}
	expect(scene.selected_choice() == ClassicOpMenuChoice::replay,
		"the fifth New Classic title row is Replay");
	const auto selected = scene.advance({.ok = true});
	auto completion = scene.take_completion();
	expect(selected && selected->ready && completion &&
		completion->route() == ClassicOpRoute::replay,
		"the proportional title Replay row publishes its typed route");

	GameInit vs_resident{};
	vs_resident.resident_rand = 40;
	vs_resident.fast_op_animation = 1U;
	auto vs_created = ClassicOpSceneTestAccess::create_with_predecoded_assets(
		vs_resident, ClassicOpTitleMode::fast_fade, fullscreen_pi(true),
		fullscreen_pi(false), window_sheet(), gaiji_sheet(), assets
	);
	expect(vs_created.has_value(),
		"Replay Patch VS cleanup scene creates");
	if(!vs_created) {
		return;
	}
	reach_main_menu(**vs_created);
	const auto entered_vs = (*vs_created)->advance({.down = true, .ok = true});
	expect(entered_vs && !entered_vs->surface_changed &&
		!entered_vs->tram_changed && (*vs_created)->phase() ==
		ClassicOpPhase::vs_grow_window,
		"Replay Patch VS cleanup remains private until the first grow frame");
	bool credits_clean = true;
	for(std::int32_t y = 0; y < 32 && credits_clean; ++y) {
		for(std::int32_t x = 0; x < th03::present::graphics_width; ++x) {
			credits_clean = credits_clean &&
				(*vs_created)->raster().pixel(Page::zero, {x, y}) == 4U;
		}
	}
	expect(credits_clean,
		"VS entry unputs both rasterized title-credit lines from bare page 1");
	bool rows_clean = true;
	for(std::int32_t y = 272; y < 384 && rows_clean; ++y) {
		for(std::int32_t x = 162; x < 294; ++x) {
			const auto expected = ((x + (y - 264)) & 1) == 0 ? 3U : 4U;
			rows_clean = rows_clean &&
				(*vs_created)->raster().pixel(Page::zero, {x, y}) == expected;
		}
	}
	expect(rows_clean,
		"VS entry removes all seven proportional main labels and restores checker transparency");
	for(std::uint16_t frame = 0U; frame < 13U; ++frame) {
		expect((*vs_created)->advance().has_value(),
			"Replay Patch VS submenu expansion advances");
	}
	expect(
		(*vs_created)->tram().cell({28, 19})->attribute ==
			th03::present::tram_white &&
		(*vs_created)->tram().cell({28, 20})->attribute ==
			th03::present::tram_black &&
		(*vs_created)->tram().cell({28, 21})->attribute ==
			th03::present::tram_black,
		"Replay Patch VS submenu uses the exact three-row shifted geometry"
	);
	const auto canceled = (*vs_created)->advance({.cancel = true});
	expect(canceled && canceled->phase == ClassicOpPhase::vs_shrink_window &&
		!canceled->surface_changed && !canceled->tram_changed,
		"Replay Patch VS Cancel privately begins the retained-title contraction");
}

void test_story_seed_and_vs_cancel_return()
{
	auto powered_on = make_scene(ClassicOpTitleMode::fast_fade, 0);
	if(!powered_on) {
		return;
	}
	reach_main_menu(*powered_on);
	const auto powered_selected = powered_on->advance({.ok = true});
	auto powered_completion = powered_on->take_completion();
	expect(
		powered_selected && powered_selected->ready && powered_completion &&
			powered_completion->route() == ClassicOpRoute::story &&
			powered_completion->resident().resident_rand == 1,
		"source-zero power-on adds one accepted title poll before Story"
	);

	auto story = make_scene(ClassicOpTitleMode::fast_fade);
	if(!story) {
		return;
	}
	reach_main_menu(*story);
	const auto selected = story->advance({.ok = true});
	auto story_completion = story->take_completion();
	expect(
		selected && selected->ready && story_completion &&
			story_completion->route() == ClassicOpRoute::story &&
			story_completion->selection_mode() ==
				ClassicOpSelectionMode::story &&
			story_completion->resident().resident_rand == 41,
		"Story selector receives the pre-main-loop seed without a forged increment"
	);

	auto versus = make_scene(ClassicOpTitleMode::fast_fade);
	if(!versus) {
		return;
	}
	reach_main_menu(*versus);
	static_cast<void>(versus->advance({.down = true, .ok = true}));
	for(std::uint16_t frame = 0U; frame < 13U; ++frame) {
		static_cast<void>(versus->advance());
	}
	expect(
		versus->tram().cell({27, 18})->attribute ==
			th03::present::tram_white &&
		versus->tram().cell({27, 19})->attribute ==
			th03::present::tram_black &&
		versus->tram().cell({27, 20})->attribute ==
			th03::present::tram_black &&
		versus->tram().cell({32, 21})->attribute ==
			th03::present::tram_black,
		"Anniversary Classic draws three modes and its fourth Quit row"
	);
	const auto simultaneous = versus->advance({.up = true, .down = true});
	expect(
		simultaneous && simultaneous->selection_changed &&
			versus->selected_vs_choice() ==
				ClassicOpVsChoice::quit,
		"simultaneous VS Up and Down performs one source step with Up priority"
	);
	expect(
		versus->advance({.up = true, .down = true}).has_value() &&
			versus->selected_vs_choice() ==
				ClassicOpVsChoice::quit,
		"held VS input remains suppressed until release"
	);
	static_cast<void>(versus->advance());
	const auto canceled = versus->advance({.ok = true});
	expect(
		canceled && canceled->phase == ClassicOpPhase::vs_shrink_window &&
			!canceled->surface_changed && !canceled->tram_changed &&
			versus->resident().resident_rand == 42,
		"confirming Anniversary Quit defers publication to the first shrink frame"
	);
	for(std::uint16_t frame = 0U; frame < 14U; ++frame) {
		static_cast<void>(versus->advance());
	}
	const auto returned = versus->advance();
	expect(
		returned && returned->phase == ClassicOpPhase::main_menu,
		"VS contraction reaches the retained main-menu phase"
	);
	expect(
		versus->selected_choice() == ClassicOpMenuChoice::versus,
		"retained main menu restores the VS highlight"
	);
	expect(
		versus->resident().resident_rand == 43,
		"VS Quit and retained-main redraw each complete one outer-loop frame"
	);
	expect(versus->advance().has_value(),
		"retained main menu releases the confirming input");
	expect(
		versus->advance({.ok = true}).has_value(),
		"retained VS main-menu choice can reopen the submenu"
	);
	for(std::uint16_t frame = 0U; frame < 13U; ++frame) {
		expect(versus->advance().has_value(), "reopened VS submenu grows");
	}
	const auto canceled_by_input = versus->advance({.cancel = true});
	expect(
		canceled_by_input && canceled_by_input->phase ==
			ClassicOpPhase::vs_shrink_window &&
			!canceled_by_input->surface_changed &&
			!canceled_by_input->tram_changed &&
			versus->resident().resident_rand == 45,
		"Anniversary VS Cancel also defers publication to the first shrink frame"
	);
}

void test_menu_input_lock_and_cancel()
{
	auto scene = make_scene(ClassicOpTitleMode::fast_fade);
	if(!scene) {
		return;
	}
	reach_main_menu(*scene);
	auto tick = scene->advance({.down = true});
	expect(
		tick && scene->selected_choice() == ClassicOpMenuChoice::versus,
		"first down selects VS"
	);
	tick = scene->advance({.down = true});
	expect(
		tick && !tick->selection_changed &&
			scene->selected_choice() == ClassicOpMenuChoice::versus,
		"held down is locked until a no-input sample"
	);
	static_cast<void>(scene->advance());
	tick = scene->advance({.down = true});
	expect(
		tick && scene->selected_choice() == ClassicOpMenuChoice::music_room,
		"released down can advance to Music Room"
	);
	static_cast<void>(scene->advance());
	tick = scene->advance({.cancel = true});
	auto completion = scene->take_completion();
	expect(
		tick && tick->ready && completion &&
			completion->route() == ClassicOpRoute::quit,
		"Cancel exits from every main-menu selection"
	);
}

void test_high_score_route_contract()
{
	GameInit resident{};
	resident.resident_rand = 40;
	resident.fast_op_animation = 1U;
	resident.game_mode = th03::game_mode::story;
	resident.story_stage = 5U;
	resident.show_score_menu = 0U;
	resident.last_score[0U].fill(9U);
	resident.last_score[1U].fill(8U);
	auto created = ClassicOpSceneTestAccess::create_with_predecoded_assets(
		resident, ClassicOpTitleMode::fast_fade, fullscreen_pi(true),
		fullscreen_pi(false), window_sheet(), gaiji_sheet()
	);
	expect(created.has_value(), "HiScore source-capability scene creates");
	if(!created) {
		return;
	}
	auto &scene = **created;
	reach_main_menu(scene);
	for(std::uint8_t step = 0U; step < 3U; ++step) {
		expect(scene.advance({.down = true}).has_value(),
			"HiScore menu selection advances downward");
		expect(scene.advance().has_value(),
			"HiScore menu selection releases the direction lock");
	}
	expect(
		scene.selected_choice() == ClassicOpMenuChoice::high_scores,
		"three source rows select HiScore"
	);
	const auto selected = scene.advance({.ok = true});
	const auto stop = scene.take_audio_command();
	auto completion = scene.take_completion();
	expect(
		selected && selected->ready && stop &&
			stop->type == ClassicAudioCommandType::stop_song &&
			stop->cue == ClassicMusicCue::menu_main && completion &&
			completion->route() == ClassicOpRoute::high_scores &&
			!completion->selection_mode(),
		"HiScore stops OP.M and publishes the typed MAINL route"
	);
	if(!completion) {
		return;
	}
	const auto &carried = completion->resident();
	expect(
		carried.story_stage == 0xFFU && carried.show_score_menu == 1U &&
			carried.game_mode == th03::game_mode::none &&
			carried.last_score ==
				std::array<std::array<std::uint8_t, 8U>, 2U>{},
		"HiScore applies score_menu()'s exact resident reset"
	);
}

void test_demo_timeout_and_blackout()
{
	auto scene = make_scene(ClassicOpTitleMode::fast_fade);
	if(!scene) {
		return;
	}
	reach_title_wait(*scene);
	for(std::uint16_t frame = 0U; frame < 520U; ++frame) {
		const auto tick = scene->advance();
		expect(
			tick && tick->phase == ClassicOpPhase::wait_title_input,
			"first 520 idle polls stay on the title"
		);
	}
	const auto timeout = scene->advance();
	expect(
		timeout && timeout->phase == ClassicOpPhase::demo_black_out &&
			timeout->boundary == th03::app::ClassicOpBoundary::input_poll,
		"poll 521 starts the demo before another frame delay"
	);
	for(std::uint16_t frame = 0U; frame < 18U; ++frame) {
		const auto fade = scene->advance();
		expect(fade.has_value(), "demo black-out cadence advances");
	}
	expect(
		scene->phase() == ClassicOpPhase::ready &&
			scene->raster().palette_tone() == 0 &&
			scene->raster().graphics(Page::zero).size() ==
				th03::present::graphics_page_pixels &&
			scene->raster().graphics(Page::one).size() ==
				th03::present::graphics_page_pixels,
		"palette_black_out(1) consumes one alignment plus seventeen fade VSyncs"
	);
	const auto stop = scene->take_audio_command();
	auto completion = scene->take_completion();
	expect(
		stop && stop->type == ClassicAudioCommandType::stop_song && completion &&
			completion->route() == ClassicOpRoute::demo,
		"demo handoff stops OP.M after the black-out"
	);
	if(completion) {
		const auto &resident = completion->resident();
		expect(
			resident.game_mode == th03::game_mode::demo &&
				resident.demo_number == 1U && resident.is_cpu[0U] == 1U &&
				resident.is_cpu[1U] == 1U &&
				resident.paletted_character == std::array<std::int8_t, 2U>{3, 1} &&
				resident.resident_rand == 600 &&
				resident.random_seed == 600U && resident.global_frame == 0U &&
				resident.last_score == decltype(resident.last_score){} &&
				!resident.runtime_snapshot && !resident.compact_runtime_snapshot &&
				!resident.round_start_snapshot && !resident.practice_rules,
			"first demo publishes the exact Mima/Reimu resident recipe"
		);
	}
}

void test_raw_input_presence_and_lock()
{
	auto scene = make_scene(ClassicOpTitleMode::fast_fade);
	if(!scene) {
		return;
	}
	reach_title_wait(*scene);
	expect(
		scene->advance({.raw_any = true}).has_value() &&
			scene->phase() == ClassicOpPhase::box_to_main,
		"a raw-only input_sp bit wakes the title"
	);
	for(std::uint16_t frame = 0U; frame < 14U; ++frame) {
		expect(scene->advance({.raw_any = true}).has_value(),
			"raw-only held input survives the main-window animation");
	}
	expect(scene->advance({.raw_any = true}).has_value(),
		"raw-only first menu sample arms the complete-word lock");
	const auto selected = scene->selected_choice();
	expect(
		scene->advance({.raw_any = true, .down = true}).has_value() &&
			scene->selected_choice() == selected,
		"raw-only held state locks later menu actions"
	);
	expect(scene->advance().has_value(),
		"an exact-zero input_sp sample releases the menu lock");
	expect(
		scene->advance({.down = true}).has_value() &&
			scene->selected_choice() != selected,
		"direction acts after the complete input_sp word releases"
	);
}

void test_long_title_cadence()
{
	auto scene = make_scene(ClassicOpTitleMode::long_animation);
	if(!scene) {
		return;
	}
	expect_start_audio(*scene);
	expect(
		scene->resident().fast_op_animation == 1U &&
			scene->raster().shown_page() == Page::one &&
			scene->raster().pixel(Page::zero, {639, 0}) == 7U,
		"long title toggles the next startup to fast and initially shows page 1"
	);
	for(std::uint16_t frame = 0U; frame < 72U; ++frame) {
		expect(scene->advance().has_value(), "long shift frame advances");
	}
	expect(
		scene->phase() == ClassicOpPhase::long_background_fade,
		"title kanji shift consumes exactly 72 page-flipped frames"
	);
	expect(
		scene->raster().pixel(Page::zero, {639, 0}) == 7U &&
			scene->raster().pixel(Page::one, {639, 0}) == 7U,
		"whole-page EGC shift retains the old right edge instead of exposing black"
	);
	for(std::uint16_t frame = 0U; frame < 56U; ++frame) {
		expect(scene->advance().has_value(), "long background fade advances");
	}
	for(std::uint16_t frame = 0U; frame < 16U; ++frame) {
		expect(scene->advance().has_value(), "selection preload pause advances");
	}
	for(std::uint16_t frame = 0U; frame < 16U; ++frame) {
		expect(scene->advance().has_value(), "first title flash advances");
	}
	expect(
		scene->phase() == ClassicOpPhase::long_menu_swap_bright,
		"first eight bright/normal flashes end before the TL02 swap"
	);
	const auto swap = scene->advance();
	expect(
		swap && swap->surface_changed &&
			scene->phase() == ClassicOpPhase::long_menu_swap_normal,
		"TL02 replaces TL01 at tone 200"
	);
	const auto play = scene->take_audio_command();
	expect(
		play && play->type == ClassicAudioCommandType::play_song,
		"long title starts OP.M at the TL02 swap"
	);
	expect(scene->advance().has_value(), "post-swap tone-100 frame advances");
	for(std::uint16_t frame = 0U; frame < 16U; ++frame) {
		expect(scene->advance().has_value(), "second title flash advances");
	}
	expect(
		scene->phase() == ClassicOpPhase::wait_title_input,
		"long title reaches input after the second eight flashes"
	);
	expect(
		std::all_of(
			scene->raster().graphics(Page::one).begin(),
			scene->raster().graphics(Page::one).end(),
			[](auto pixel) { return pixel == 4U; }
		),
		"long title populates bare TL02 page 1 only after the second flash"
	);
}

void test_rejections()
{
	GameInit mismatch{};
	mismatch.fast_op_animation = 1U;
	auto rejected = ClassicOpSceneTestAccess::create_with_predecoded_assets(
		mismatch, ClassicOpTitleMode::long_animation, fullscreen_pi(true),
		fullscreen_pi(false), window_sheet(), gaiji_sheet()
	);
	expect(
		!rejected && rejected.error().code ==
			ClassicOpErrorCode::invalid_state,
		"title mode must match the resident alternating-animation flag"
	);

	GameInit resident{};
	auto bad_window = window_sheet();
	bad_window.header.pixel_width = 8U;
	rejected = ClassicOpSceneTestAccess::create_with_predecoded_assets(
		resident, ClassicOpTitleMode::long_animation, fullscreen_pi(true),
		fullscreen_pi(false), std::move(bad_window), gaiji_sheet()
	);
	expect(
		!rejected && rejected.error().code ==
			ClassicOpErrorCode::invalid_asset_geometry,
		"OPWIN geometry is rejected before scene publication"
	);
}

void local_original_gate(const std::filesystem::path &content_root)
{
	th03::app::MainResourceLoader loader(content_root);
	const auto closure = loader.load_op_title_menu();
	expect(
		closure.has_value(),
		"local TL01, TL02, OPWIN, MIKOFT, and OP.M closure authenticates"
	);
	if(!closure) {
		return;
	}
	GameInit resident{};
	const auto created = ClassicOpTitleMenuScene::create(
		resident, ClassicOpTitleMode::long_animation, *closure
	);
	expect(
		created.has_value(),
		"local OP title assets decode and prepare at source geometry"
	);
	if(!created) {
		return;
	}
	auto &scene = **created;
	expect_start_audio(scene);
	for(std::uint16_t frame = 0U; frame < 72U + 56U + 16U + 16U; ++frame) {
		expect(scene.advance().has_value(), "local long-title prefix advances");
	}
	expect(scene.advance().has_value(), "local TL02 title swap advances");
	const auto play = scene.take_audio_command();
	expect(
		play && play->type == ClassicAudioCommandType::play_song,
		"local long-title swap publishes OP.M playback"
	);
	expect(scene.advance().has_value(), "local normal swap frame advances");
	for(std::uint16_t frame = 0U; frame < 16U; ++frame) {
		expect(scene.advance().has_value(), "local second flash advances");
	}
	expect(
		scene.phase() == ClassicOpPhase::wait_title_input,
		"local original title reaches the first source input boundary"
	);
	expect(scene.advance({.ok = true}).has_value(),
		"local title accepts the main-menu opening input");
	for(std::uint16_t frame = 0U; frame < 14U; ++frame) {
		expect(scene.advance().has_value(), "local main-menu box expands");
	}
	expect(scene.advance({.down = true, .ok = true}).has_value(),
		"local main menu enters the VS submenu");
	for(std::uint16_t frame = 0U; frame < 13U; ++frame) {
		expect(scene.advance().has_value(), "local VS submenu box expands");
	}
	expect(
		scene.phase() == ClassicOpPhase::vs_menu &&
			scene.tram().cell({32, 21})->attribute ==
				th03::present::tram_black,
		"local MIKOFT renders the Anniversary Classic Quit row"
	);
}

} // namespace

int main(int argc, char **argv)
{
	test_fast_title_and_held_first_choice();
	test_replay_patch_title_if_assets_supplied();
	test_story_seed_and_vs_cancel_return();
	test_menu_input_lock_and_cancel();
	test_high_score_route_contract();
	test_demo_timeout_and_blackout();
	test_raw_input_presence_and_lock();
	test_long_title_cadence();
	test_rejections();
	if(argc == 2) {
		local_original_gate(argv[1]);
	} else {
		expect(argc == 1, "optional local gate accepts one content root");
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
