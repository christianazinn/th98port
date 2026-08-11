#include "../src/app/classic_op_options_scene_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string_view>
#include <utility>

namespace {

using th03::GameInit;
using th03::app::ClassicAudioCommandType;
using th03::app::ClassicBgmMode;
using th03::app::ClassicConfigOptions;
using th03::app::ClassicKeyMode;
using th03::app::ClassicMusicCue;
using th03::app::ClassicOpErrorCode;
using th03::app::ClassicOpInput;
using th03::app::ClassicOpOptionsChoice;
using th03::app::ClassicOpOptionsPhase;
using th03::app::ClassicOpOptionsScene;
using th03::app::ClassicRank;
using th03::assets::BfntConsumerPolicy;
using th03::assets::BfntSheet;
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

PiImage menu_background()
{
	PiImage result{};
	result.palette.emplace();
	result.image.width = th03::present::graphics_width;
	result.image.height = th03::present::graphics_height;
	result.image.indices.assign(th03::present::graphics_page_pixels, 4U);
	return result;
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

th03::present::Raster main_menu_raster()
{
	th03::present::Raster raster{};
	const auto background = menu_background();
	expect(
		raster.blit({
			.pixels = background.image.indices,
			.width = background.image.width,
			.height = background.image.height,
			.stride = background.image.width,
		}, {0, 0}),
		"synthetic main-menu background draws"
	);
	expect(
		raster.clear({280, 256, 16U, 128U}, 2U),
		"synthetic main-menu right edge draws"
	);
	return raster;
}

std::unique_ptr<ClassicOpOptionsScene> make_scene(
	ClassicConfigOptions config = {},
	bool fm_available = true,
	std::int32_t resident_rand = 40
)
{
	GameInit resident{};
	resident.rank = static_cast<std::uint8_t>(config.rank);
	resident.key_mode = static_cast<std::uint8_t>(config.key_mode);
	resident.resident_rand = resident_rand;
	auto created = ClassicOpOptionsScene::create(
		resident, config, fm_available, main_menu_raster(), {},
		menu_background(), window_sheet()
	);
	expect(created.has_value(), "synthetic Options scene creates");
	return created ? std::move(*created) : nullptr;
}

void expect_gaiji(
	const th03::present::Tram &tram,
	std::int32_t x,
	std::int32_t y,
	std::uint8_t glyph,
	th03::present::TramAttribute attribute,
	std::string_view message
)
{
	const auto cell = tram.cell({x, y});
	const auto decoded = cell ?
		th03::present::decode_gaiji_cell(cell->code) : std::nullopt;
	expect(
		decoded && decoded->glyph_id == glyph &&
			decoded->half == th03::present::GaijiHalf::left &&
			cell->attribute == attribute,
		message
	);
}

void grow_and_enter(
	ClassicOpOptionsScene &scene,
	ClassicOpInput transition_input
)
{
	const auto initial_rand = scene.resident().resident_rand;
	for(std::uint16_t frame = 0U; frame < 13U; ++frame) {
		const auto tick = scene.advance(
			frame == 0U ? transition_input : ClassicOpInput{.cancel = true}
		);
		expect(tick.has_value(), "Options grow frame advances");
		if(tick) {
			expect(
				tick->input_sampled == (frame == 0U),
				"Options grow samples input only before the blocking animation"
			);
		}
		expect(
			scene.resident().resident_rand == initial_rand,
			"Options grow VSyncs do not advance resident RNG"
		);
	}
	expect(
		scene.phase() == ClassicOpOptionsPhase::enter_options,
		"13 source grow VSyncs reach initial option drawing"
	);
	const auto entered = scene.advance({.right = true});
	expect(
		entered && entered->phase == ClassicOpOptionsPhase::options_menu &&
			!entered->input_sampled && entered->tram_changed,
		"initial Options draw consumes the latched pre-animation sample"
	);
	expect(
		scene.resident().resident_rand == initial_rand + 1,
		"initial Options loop tail advances resident RNG once"
	);
}

void test_exact_rows_and_transition_lock()
{
	auto scene = make_scene();
	if(!scene) {
		return;
	}
	const auto first = scene->advance({.ok = true});
	expect(
		first && first->input_sampled && first->tram_changed,
		"menu-change iteration samples and clears before box growth"
	);
	expect(
		scene->raster().pixel(Page::zero, {288, 256}) == 2U,
		"first grow frame moves the source right edge by eight pixels"
	);
	for(std::uint16_t frame = 1U; frame < 13U; ++frame) {
		expect(
			scene->advance({.cancel = true}).has_value(),
			"remaining grow frame advances"
		);
	}
	expect(
		scene->raster().pixel(Page::zero, {384, 256}) == 2U,
		"final expanded right edge starts at x=384"
	);
	expect(scene->advance().has_value(), "initial option rows draw");
	expect(
		!scene->input_allowed(),
		"held main-menu confirmation remains locked after the grow"
	);

	expect_gaiji(
		scene->tram(), 25, 17, 0x6EU, th03::present::tram_white,
		"Rank label preserves source x/y and selected attribute"
	);
	expect_gaiji(
		scene->tram(), 37, 17, 0x74U, th03::present::tram_white,
		"Normal value preserves source shifted center"
	);
	expect_gaiji(
		scene->tram(), 25, 19, 0x4BU, th03::present::tram_black,
		"Music label preserves source row"
	);
	expect_gaiji(
		scene->tram(), 35, 19, 0xCFU, th03::present::tram_black,
		"FM value preserves the fixed left padding"
	);
	expect_gaiji(
		scene->tram(), 23, 21, 0x5CU, th03::present::tram_black,
		"KeyConfig keeps the source off-center left column"
	);
	expect_gaiji(
		scene->tram(), 32, 22, 0x48U, th03::present::tram_black,
		"Quit is centered in the full submenu"
	);

	const auto held = scene->advance({.right = true});
	expect(
		held && !held->tram_changed,
		"held transition input advances without redrawing"
	);
	expect(
		scene->config().rank == ClassicRank::normal,
		"held transition input cannot alter Rank"
	);
	expect(scene->advance().has_value(), "empty sample unlocks Options");
	expect(scene->input_allowed(), "empty sample enables input");
}

void test_config_audio_and_return_cadence()
{
	auto scene = make_scene();
	if(!scene) {
		return;
	}
	grow_and_enter(*scene, {.ok = true});
	expect(scene->advance({.right = true}).has_value(), "held Right is ignored");
	expect(scene->advance().has_value(), "Rank unlock sample advances");
	const auto rank = scene->advance({.right = true});
	expect(
		rank && rank->config_changed &&
			scene->config().rank == ClassicRank::hard &&
			scene->resident().rank == static_cast<std::uint8_t>(ClassicRank::hard),
		"Rank changes resident and persisted option together"
	);
	expect(scene->advance().has_value(), "post-Rank release advances");
	const auto opposed = scene->advance({.up = true, .down = true});
	expect(
		opposed && opposed->selection_changed &&
			scene->selected_choice() == ClassicOpOptionsChoice::rank,
		"simultaneous Up and Down execute sequentially and cancel"
	);
	expect(scene->advance().has_value(), "post-navigation release advances");
	const auto leave = scene->advance({
		.down = true,
		.left = true,
		.cancel = true,
	});
	expect(
		leave && leave->phase == ClassicOpOptionsPhase::shrink_window &&
			leave->selection_changed && leave->config_changed &&
			scene->config().bgm_mode == ClassicBgmMode::off,
		"vertical, horizontal, and Cancel ordering matches option_update"
	);
	const auto stop = scene->take_audio_command();
	expect(
		stop && stop->type == ClassicAudioCommandType::stop_song &&
			stop->cue == ClassicMusicCue::none,
		"FM-to-Off emits only the source Stop command"
	);
	expect(!scene->take_audio_command(), "FM-to-Off audio queue drains");

	const auto rand_before_shrink = scene->resident().resident_rand;
	for(std::uint16_t frame = 0U; frame < 14U; ++frame) {
		const auto tick = scene->advance(
			frame == 0U ? ClassicOpInput{.cancel = true} : ClassicOpInput{}
		);
		expect(tick.has_value(), "Options shrink frame advances");
		if(tick) {
			expect(
				tick->input_sampled == (frame == 0U),
				"Options shrink samples only before the blocking animation"
			);
		}
		expect(
			scene->resident().resident_rand == rand_before_shrink,
			"Options shrink VSyncs do not advance resident RNG"
		);
	}
	expect(
		scene->phase() == ClassicOpOptionsPhase::return_main,
		"14 source shrink VSyncs reach main-menu redraw"
	);
	expect(
		scene->raster().pixel(Page::zero, {280, 256}) == 2U &&
			scene->raster().pixel(Page::zero, {384, 256}) == 4U,
		"shrink restores TL02 and returns the right edge to x=280"
	);
	const auto returned = scene->advance({});
	expect(
		returned && returned->ready &&
			returned->phase == ClassicOpOptionsPhase::ready,
		"main-menu redraw and common loop tail publish completion"
	);
	auto completion = scene->take_completion();
	expect(completion.has_value(), "Options completion is available once");
	if(completion) {
		expect(
			completion->config().rank == ClassicRank::hard &&
				completion->config().bgm_mode == ClassicBgmMode::off,
			"Options completion retains all in-memory configuration edits"
		);
		expect(
			!completion->main_input_allowed(),
			"held pre-shrink Cancel keeps returned main input locked"
		);
		expect_gaiji(
			completion->tram(), 25, 21, 0x3DU,
			th03::present::tram_white,
			"returned main menu redraws Option as selected"
		);
		expect(
			completion->resident().resident_rand == rand_before_shrink + 1,
			"returned main loop tail increments resident RNG once"
		);
	}
	expect(
		!scene->take_completion() &&
			scene->phase() == ClassicOpOptionsPhase::consumed,
		"Options completion is single-consumer"
	);
}

void select_bgm(ClassicOpOptionsScene &scene)
{
	grow_and_enter(scene, {});
	expect(scene.advance({.down = true}).has_value(), "BGM row selects");
	expect(scene.advance().has_value(), "BGM row release advances");
}

void test_bgm_modes_and_disabled_selection()
{
	ClassicConfigOptions off{};
	off.bgm_mode = ClassicBgmMode::off;
	auto from_off = make_scene(off);
	if(from_off) {
		select_bgm(*from_off);
		const auto changed = from_off->advance({.right = true});
		expect(
			changed && changed->config_changed &&
				from_off->config().bgm_mode == ClassicBgmMode::fm,
			"Off toggles to FM in either horizontal direction"
		);
		const auto stop = from_off->take_audio_command();
		const auto play = from_off->take_audio_command();
		expect(
			stop && stop->type == ClassicAudioCommandType::stop_song &&
				play && play->type == ClassicAudioCommandType::play_song &&
				play->cue == ClassicMusicCue::menu_main,
			"Off-to-FM emits Stop then menu Play"
		);
	}

	ClassicConfigOptions midi{};
	midi.bgm_mode = ClassicBgmMode::midi;
	auto from_midi = make_scene(midi);
	if(from_midi) {
		select_bgm(*from_midi);
		expect(
			from_midi->advance({.left = true}).has_value() &&
				from_midi->config().bgm_mode == ClassicBgmMode::off,
			"MIDI collapses to Off instead of ringing to FM"
		);
		expect(
			from_midi->take_audio_command()->type ==
				ClassicAudioCommandType::stop_song &&
				!from_midi->take_audio_command(),
			"MIDI-to-Off emits only Stop"
		);
	}

	auto disabled = make_scene(off, false);
	if(disabled) {
		select_bgm(*disabled);
		const auto redraw = disabled->advance({.left = true});
		expect(
			redraw && redraw->tram_changed && !redraw->config_changed &&
				disabled->config().bgm_mode == ClassicBgmMode::off &&
				!disabled->take_audio_command(),
			"disabled BGM input redraws but cannot mutate or emit audio"
		);
	}
}

void test_key_ring_and_rejections()
{
	auto scene = make_scene();
	if(scene) {
		grow_and_enter(*scene, {});
		expect(scene->advance({.down = true}).has_value(), "BGM selects");
		expect(scene->advance().has_value(), "first release advances");
		expect(scene->advance({.down = true}).has_value(), "Key Mode selects");
		expect(scene->advance().has_value(), "second release advances");
		const auto wrapped = scene->advance({.left = true, .right = true});
		expect(
			wrapped && wrapped->config_changed &&
				scene->config().key_mode == ClassicKeyMode::key_joy &&
				scene->resident().key_mode == 2U,
			"Left takes precedence and wraps Key-vs-Key to Key-vs-Joy"
		);
	}

	ClassicConfigOptions fm{};
	GameInit resident{};
	resident.rank = static_cast<std::uint8_t>(fm.rank);
	resident.key_mode = static_cast<std::uint8_t>(fm.key_mode);
	auto unavailable = ClassicOpOptionsScene::create(
		resident, fm, false, main_menu_raster(), {}, menu_background(),
		window_sheet()
	);
	expect(
		!unavailable && unavailable.error().code ==
			ClassicOpErrorCode::invalid_state,
		"unavailable FM rejects the source-unreachable enabled BGM state"
	);

	resident.rank = 0U;
	auto mismatch = ClassicOpOptionsScene::create(
		resident, fm, true, main_menu_raster(), {}, menu_background(),
		window_sheet()
	);
	expect(
		!mismatch && mismatch.error().code == ClassicOpErrorCode::invalid_state,
		"resident and persisted Rank mismatch is rejected"
	);

	auto bad_window = window_sheet();
	bad_window.header.pixel_width = 8U;
	resident.rank = static_cast<std::uint8_t>(fm.rank);
	auto geometry = ClassicOpOptionsScene::create(
		resident, fm, true, main_menu_raster(), {}, menu_background(),
		std::move(bad_window)
	);
	expect(
		!geometry && geometry.error().code ==
			ClassicOpErrorCode::invalid_asset_geometry,
		"invalid retained OPWIN geometry is rejected"
	);
}

} // namespace

int main()
{
	test_exact_rows_and_transition_lock();
	test_config_audio_and_return_cadence();
	test_bgm_modes_and_disabled_selection();
	test_key_ring_and_rejections();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
