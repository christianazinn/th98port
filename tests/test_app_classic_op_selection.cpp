#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "classic_op_selection_internal.hpp"
#include "th03/exact/trig.hpp"

namespace {

using th03::GameInit;
using th03::app::ClassicAudioCommandType;
using th03::app::ClassicMusicCue;
using th03::app::ClassicOpCharacterSelection;
using th03::app::ClassicOpPhase;
using th03::app::ClassicOpSelectionCarry;
using th03::app::ClassicOpSelectionCompletion;
using th03::app::ClassicOpSelectionInput;
using th03::app::ClassicOpSelectionMode;
using th03::app::ClassicOpSelectionOutcome;
using th03::app::ClassicOpSelectionPhase;
using th03::app::ClassicOpSelectionTestAccess;
using th03::app::ClassicOpTitleMenuScene;
using th03::app::ClassicOpTitleMode;
using th03::app::LoadedMainResource;
using th03::app::MainResourceRole;
using th03::app::ResourceBytes;
using th03::assets::BfntConsumerPolicy;
using th03::assets::BfntSheet;
using th03::assets::CdgFile;
using th03::assets::Palette16Rgb4;
using th03::assets::PlanarBitmap4;
using th03::assets::PlanarCel4;
using th03::present::Page;
using th03::present::Raster;
using th03::present::Tram;

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

void append_le32(std::vector<std::byte> &bytes, std::uint32_t value)
{
	append_u8(bytes, static_cast<std::uint8_t>(value));
	append_u8(bytes, static_cast<std::uint8_t>(value >> 8U));
	append_u8(bytes, static_cast<std::uint8_t>(value >> 16U));
	append_u8(bytes, static_cast<std::uint8_t>(value >> 24U));
}

std::vector<std::byte> cdg_bytes(
	std::uint16_t width,
	std::uint16_t height,
	std::uint8_t count
)
{
	const auto plane_size = static_cast<std::size_t>(width / 8U) * height;
	std::vector<std::byte> bytes;
	append_le16(bytes, static_cast<std::uint16_t>(plane_size));
	append_le16(bytes, width);
	append_le16(bytes, height);
	append_le16(bytes, static_cast<std::uint16_t>(80U * (height - 1U)));
	append_le16(bytes, static_cast<std::uint16_t>(width / 32U));
	append_u8(bytes, count);
	append_u8(bytes, 0U);
	append_le16(bytes, 0U);
	append_le16(bytes, 0U);
	for(std::uint8_t cel_index = 0U; cel_index < count; ++cel_index) {
		bytes.insert(bytes.end(), plane_size, std::byte{0xFFU});
		for(std::uint8_t plane = 0U; plane < 4U; ++plane) {
			bytes.insert(
				bytes.end(), plane_size,
				((cel_index + plane) & 1U) != 0U ?
					std::byte{0xFFU} : std::byte{0U}
			);
		}
	}
	return bytes;
}

std::vector<std::byte> name_bfnt_bytes()
{
	std::vector<std::byte> bytes;
	for(const auto value : {'B', 'F', 'N', 'T'}) {
		append_u8(bytes, static_cast<std::uint8_t>(value));
	}
	append_u8(bytes, 0x1AU);
	append_u8(bytes, 0x83U);
	append_u8(bytes, 0x16U);
	append_u8(bytes, 0U);
	append_le16(bytes, 64U);
	append_le16(bytes, 16U);
	append_le16(bytes, 0U);
	append_le16(bytes, 17U);
	for(std::size_t index = 0U; index < 8U; ++index) {
		append_u8(bytes, 0U);
	}
	append_le32(bytes, 0U);
	append_le16(bytes, 0U);
	append_le16(bytes, 0U);
	for(std::uint8_t color = 0U; color < 16U; ++color) {
		append_u8(bytes, static_cast<std::uint8_t>(color << 4U));
		append_u8(bytes, static_cast<std::uint8_t>((15U - color) << 4U));
		append_u8(bytes, static_cast<std::uint8_t>((color / 2U) << 4U));
	}
	for(std::size_t glyph = 0U; glyph < 18U; ++glyph) {
		for(std::size_t pixel = 0U; pixel < (64U * 16U); pixel += 2U) {
			const auto left = static_cast<std::uint8_t>((glyph + pixel) & 0x0FU);
			const auto right = static_cast<std::uint8_t>(
				(glyph + pixel + 1U) & 0x0FU
			);
			append_u8(bytes, static_cast<std::uint8_t>((left << 4U) | right));
		}
	}
	return bytes;
}

std::vector<LoadedMainResource> encoded_resources()
{
	std::vector<LoadedMainResource> result;
	for(const auto key : th03::app::op_character_selection_resource_keys()) {
		auto spec = th03::app::main_resource_spec(key);
		expect(spec.has_value(), "selection key resolves to canonical metadata");
		if(!spec) { continue; }
		std::vector<std::byte> bytes;
		switch(key.role) {
		case MainResourceRole::character_selection_portrait:
			bytes = cdg_bytes(192U, 192U, 2U);
			break;
		case MainResourceRole::selection_extra_icons:
			bytes = cdg_bytes(32U, 44U, 9U);
			break;
		case MainResourceRole::selection_story_portrait:
			bytes = cdg_bytes(192U, 192U, 1U);
			break;
		case MainResourceRole::selection_stats_sheet:
			bytes = cdg_bytes(128U, 64U, 1U);
			break;
		case MainResourceRole::selection_extra_background:
			bytes = cdg_bytes(64U, 64U, 1U);
			break;
		case MainResourceRole::selection_name_sheet:
			bytes = name_bfnt_bytes();
			break;
		case MainResourceRole::selection_palette:
			bytes.resize(48U, std::byte{0x03U});
			break;
		case MainResourceRole::selection_music:
			bytes.resize(spec->expected_bytes, std::byte{0x4DU});
			break;
		default:
			expect(false, "selection resource switch remains exhaustive");
			break;
		}
		expect(bytes.size() == spec->expected_bytes,
			"encoded selection fixture matches canonical size");
		result.push_back({
			.spec = std::move(*spec),
			.bytes = std::make_shared<const std::vector<std::byte>>(
				std::move(bytes)
			),
		});
	}
	return result;
}

PlanarCel4 cel(
	std::uint16_t width,
	std::uint16_t height,
	std::uint8_t color,
	bool alpha
)
{
	PlanarCel4 result{};
	result.pixel_width = width;
	result.pixel_height = height;
	result.bitplane_size = static_cast<std::uint16_t>(
		(static_cast<std::size_t>(width) / 8U) * height
	);
	result.offset_at_bottom_left = static_cast<std::int16_t>(
		static_cast<std::size_t>(height - 1U) * 80U
	);
	result.vram_dword_width = static_cast<std::uint16_t>(width / 32U);
	const auto plane_size = static_cast<std::size_t>(result.bitplane_size);
	if(alpha) {
		result.alpha.emplace(plane_size, std::uint8_t{0xFFU});
	}
	for(std::size_t plane = 0U; plane < result.color_planes.size(); ++plane) {
		result.color_planes[plane].assign(
			plane_size,
			((color & static_cast<std::uint8_t>(1U << plane)) != 0U) ?
				0xFFU : 0U
		);
	}
	return result;
}

CdgFile file(
	std::uint16_t width,
	std::uint16_t height,
	std::size_t count,
	std::uint8_t first_color,
	bool alpha
)
{
	CdgFile result{};
	result.header.pixel_width = width;
	result.header.pixel_height = height;
	for(std::size_t index = 0U; index < count; ++index) {
		result.cels.push_back(cel(
			width,
			height,
			static_cast<std::uint8_t>(first_color + index),
			alpha
		));
	}
	return result;
}

BfntSheet names()
{
	BfntSheet result{};
	result.consumer = BfntConsumerPolicy::superimpose;
	result.header.pixel_width = 64U;
	result.header.pixel_height = 16U;
	result.header.first_code = 0U;
	result.header.last_code = 17U;
	result.transparent_color = 0U;
	constexpr std::size_t plane_size = (64U / 8U) * 16U;
	for(std::uint16_t code = 0U; code < 18U; ++code) {
		PlanarBitmap4 bitmap{};
		for(auto &plane : bitmap.color_planes) {
			plane.assign(plane_size, 0U);
		}
		bitmap.color_planes[code % bitmap.color_planes.size()].assign(
			plane_size, 0xFFU
		);
		result.glyphs.push_back({code, std::move(bitmap)});
	}
	return result;
}

Palette16Rgb4 palette()
{
	Palette16Rgb4 result{};
	for(std::size_t index = 0U; index < result.colors.size(); ++index) {
		result.colors[index] = {
			static_cast<std::uint8_t>(index),
			static_cast<std::uint8_t>(15U - index),
			static_cast<std::uint8_t>(index / 2U),
		};
	}
	return result;
}

Tram tram_with_cursors()
{
	Tram result{};
	for(std::uint8_t glyph = 0x12U; glyph <= 0x19U; ++glyph) {
		std::array<std::uint8_t, th03::present::gaiji_glyph_bytes> rows{};
		rows.fill(static_cast<std::uint8_t>(0x80U >> (glyph & 7U)));
		expect(result.load_gaiji(glyph, rows), "synthetic cursor gaiji installs");
	}
	return result;
}

std::uint8_t scaled_angle_reference(
	std::uint8_t base,
	std::int16_t offset,
	std::uint16_t frequency
)
{
	const auto angle = static_cast<std::uint8_t>(
		static_cast<std::int16_t>(base) + offset
	);
	const auto product = static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(angle) * frequency
	);
	auto signed_product = std::bit_cast<std::int16_t>(product);
	if(signed_product < 0) {
		signed_product = static_cast<std::int16_t>(signed_product + 255);
	}
	return static_cast<std::uint8_t>(signed_product / 256);
}

void draw_curves_reference(
	Raster &raster,
	std::uint8_t cycle,
	std::uint8_t trail_count
)
{
	const auto triangle = (cycle < 128U) ?
		static_cast<std::uint16_t>(cycle) :
		static_cast<std::uint16_t>(256U - cycle);
	const auto other = static_cast<std::uint16_t>(256U + triangle);
	const auto x_frequency = static_cast<std::uint16_t>(256U + (triangle * 2U));
	const auto y_frequency = static_cast<std::uint16_t>(512U + (triangle * 2U));
	const auto put_pair = [&raster, cycle, other, x_frequency, y_frequency](
		std::int16_t offset,
		th03::present::Index4 color
	) {
		const auto c = static_cast<std::int16_t>(cycle);
		const std::array<std::int16_t, 4U> offsets{
			static_cast<std::int16_t>(static_cast<std::uint8_t>(c - (offset * 2))),
			static_cast<std::int16_t>(static_cast<std::uint8_t>((c * 2) - (offset * 4))),
			static_cast<std::int16_t>(static_cast<std::uint8_t>((-c) + (offset * 2))),
			static_cast<std::int16_t>(static_cast<std::uint8_t>((c / 2) - offset)),
		};
		for(std::uint16_t base = 0U; base < 256U; ++base) {
			const auto angle = static_cast<std::uint8_t>(base);
			const auto outer_y = 200 + th03::exact::vector2(
				scaled_angle_reference(angle, offsets[1U], y_frequency), 220
			).y;
			if(static_cast<std::uint32_t>(outer_y) < th03::present::graphics_height) {
				const auto outer_x = 320 + th03::exact::vector2(
					scaled_angle_reference(angle, offsets[0U], other), 220
				).x;
				expect(raster.plot({outer_x, outer_y}, color),
					"reference outer curve point is in bounds");
			}
			const auto inner_y = 200 + th03::exact::vector2(
				scaled_angle_reference(angle, offsets[3U], other), 120
			).y;
			if(static_cast<std::uint32_t>(inner_y) < th03::present::graphics_height) {
				const auto inner_x = 320 + th03::exact::vector2(
					scaled_angle_reference(angle, offsets[2U], x_frequency), 120
				).x;
				expect(raster.plot({inner_x, inner_y}, color),
					"reference inner curve point is in bounds");
			}
		}
	};
	put_pair(0, 6U);
	const auto bright_end = static_cast<std::uint8_t>((trail_count + 1U) / 2U);
	for(std::uint8_t offset = 1U; offset <= trail_count; ++offset) {
		put_pair(
			static_cast<std::int16_t>(offset),
			(offset <= bright_end) ? 5U : 1U
		);
	}
}

std::unique_ptr<ClassicOpCharacterSelection> make_scene(
	ClassicOpSelectionMode mode,
	GameInit resident = {},
	ClassicOpSelectionCarry carry = {},
	std::uint8_t available_character_count = 9U
)
{
	if(resident.resident_rand == 0) {
		resident.resident_rand = 100;
	}
	if(mode != ClassicOpSelectionMode::story) {
		if(resident.paletted_character[0U] < 1 ||
			resident.paletted_character[0U] > 18 ||
			resident.paletted_character[1U] < 1 ||
			resident.paletted_character[1U] > 18) {
			resident.paletted_character = {1, 3};
		}
	}
	std::array<CdgFile, 9U> portraits{};
	for(std::size_t index = 0U; index < portraits.size(); ++index) {
		portraits[index] = file(
			192U, 192U, 2U, static_cast<std::uint8_t>(1U + index), true
		);
	}
	auto created = ClassicOpSelectionTestAccess::create_with_predecoded_assets(
		resident, mode, available_character_count, carry,
		Raster{}, tram_with_cursors(),
		std::move(portraits), file(192U, 192U, 1U, 12U, true),
		file(128U, 64U, 1U, 4U, false),
		file(64U, 64U, 1U, 3U, false),
		file(32U, 44U, 9U, 1U, false), names(), palette()
	);
	expect(created.has_value(), "synthetic selector creates");
	return created ? std::move(*created) : nullptr;
}

void reach_input(ClassicOpCharacterSelection &scene, std::size_t count);
void drain_frame(ClassicOpCharacterSelection &scene);
std::optional<th03::app::ClassicOpSelectionTick>
drain_confirmation(ClassicOpCharacterSelection &scene);

void test_live_input_mode_normalization()
{
	const ClassicOpSelectionInput scripted{
		.single_player = 0x0001U,
		.player_one = 0x0002U,
		.player_two = 0x0004U,
	};
	const auto unchanged = ClassicOpSelectionTestAccess::normalize_input(
		ClassicOpSelectionMode::vs_1p_2p, 2U, scripted
	);
	expect(unchanged.single_player == scripted.single_player &&
		unchanged.player_one == scripted.player_one &&
		unchanged.player_two == scripted.player_two,
		"scripted selection input remains post-mode and unchanged");

	ClassicOpSelectionInput live{
		.single_player = 0x7000U,
		.player_one = 0x7000U,
		.player_two = 0x7000U,
		.physical_domains = th03::app::PhysicalInputDomains{
			.keyboard = {
				.p1 = 0x0004U,
				.p2 = 0x0008U,
				.system = 0x1032U,
				.charge_mask = 0x03U,
			},
			.first_joystick = 0x0009U,
			.first_joystick_present = true,
		},
	};
	const auto story = ClassicOpSelectionTestAccess::normalize_input(
		ClassicOpSelectionMode::story, 0U, live
	);
	expect(story.single_player == 0x103FU,
		"Story selection merges input_sp, keyboard P1, and joystick zero");
	const auto key_key = ClassicOpSelectionTestAccess::normalize_input(
		ClassicOpSelectionMode::vs_1p_2p, 0U, live
	);
	expect(key_key.player_one == 0x0004U && key_key.player_two == 0x0008U,
		"selection Key-vs-Key uses both keyboard multiplayer domains");
	const auto joy_key = ClassicOpSelectionTestAccess::normalize_input(
		ClassicOpSelectionMode::vs_1p_2p, 1U, live
	);
	expect(joy_key.player_one == 0x0009U && joy_key.player_two == 0x1032U,
		"selection Joy-vs-Key routes joystick zero and input_sp");
	const auto key_joy = ClassicOpSelectionTestAccess::normalize_input(
		ClassicOpSelectionMode::vs_1p_2p, 2U, live
	);
	expect(key_joy.player_one == 0x1032U && key_joy.player_two == 0x0009U,
		"selection Key-vs-Joy routes input_sp and joystick zero");
	live.physical_domains->first_joystick_present = false;
	const auto missing = ClassicOpSelectionTestAccess::normalize_input(
		ClassicOpSelectionMode::vs_1p_2p, 2U, live
	);
	expect(missing.player_one == 0x0004U && missing.player_two == 0x0008U,
		"selection keeps keyboard fallback words when joystick zero is absent");
}

void test_persisted_selection_ring_and_cpu_delay_width()
{
	GameInit persisted{};
	persisted.resident_rand = 100;
	persisted.paletted_character = {17, 3};
	auto up = make_scene(
		ClassicOpSelectionMode::vs_1p_cpu, persisted, {}, 7U
	);
	auto down = make_scene(
		ClassicOpSelectionMode::vs_1p_cpu, persisted, {}, 7U
	);
	if(!up || !down) { return; }
	reach_input(*up, 30U);
	auto tick = up->advance({.single_player = 0x0001U});
	expect(
		tick && up->local_selection()[0U] == 7U,
		"donor ring_dec maps persisted base 8 Up to 7 when only seven unlock"
	);
	reach_input(*down, 30U);
	tick = down->advance({.single_player = 0x0002U});
	expect(
		tick && down->local_selection()[0U] == 0U,
		"donor ring_inc_ge maps persisted base 8 Down to 0"
	);

	auto delayed = make_scene(ClassicOpSelectionMode::vs_1p_cpu);
	if(!delayed) { return; }
	reach_input(*delayed, 30U);
	for(std::size_t frame = 0U; frame < 260U; ++frame) {
		tick = delayed->advance();
		expect(tick.has_value(), "long unconfirmed CPU-selection frame advances");
		drain_frame(*delayed);
	}
	tick = delayed->advance({.single_player = 0x0020U});
	expect(tick.has_value(), "late P1 confirmation starts");
	drain_confirmation(*delayed);
	expect(
		delayed->phase() == ClassicOpSelectionPhase::draw_and_input,
		"16-bit CPU quirk counter does not wrap before late P1 confirmation"
	);
}

void drain_start_audio(ClassicOpCharacterSelection &scene)
{
	const auto stop = scene.take_audio_command();
	const auto load = scene.take_audio_command();
	const auto play = scene.take_audio_command();
	expect(
		stop && stop->type == ClassicAudioCommandType::stop_song,
		"selection first stops the OP song"
	);
	expect(
		load && load->type == ClassicAudioCommandType::load_song &&
			load->cue == ClassicMusicCue::character_select,
		"selection then loads SELECT.M"
	);
	expect(
		play && play->type == ClassicAudioCommandType::play_song &&
			play->cue == ClassicMusicCue::character_select,
		"selection then starts SELECT.M"
	);
	expect(!scene.take_audio_command(), "selection startup audio drains exactly");
}

void reach_input(ClassicOpCharacterSelection &scene, std::size_t count)
{
	drain_start_audio(scene);
	for(std::size_t tick = 0U; tick < count; ++tick) {
		const auto advanced = scene.advance();
		expect(advanced.has_value(), "initial selection VSync advances");
	}
	expect(
		scene.phase() == ClassicOpSelectionPhase::draw_and_input,
		"initial delay ends at the first draw/input boundary"
	);
}

void drain_frame(ClassicOpCharacterSelection &scene)
{
	while(scene.phase() == ClassicOpSelectionPhase::frame_wait) {
		const auto tick = scene.advance();
		expect(tick.has_value(), "selection frame wait advances");
	}
}

std::optional<th03::app::ClassicOpSelectionTick>
drain_confirmation(ClassicOpCharacterSelection &scene)
{
	std::array<std::int16_t, 18U> tones{};
	std::optional<th03::app::ClassicOpSelectionTick> final_tick{};
	for(std::size_t frame = 0U; frame < tones.size(); ++frame) {
		tones[frame] = scene.raster().palette_tone();
		const auto tick = scene.advance();
		expect(tick.has_value(), "confirmation callback VSync advances");
		if(tick) { final_tick = *tick; }
	}
	for(std::size_t frame = 0U; frame < tones.size(); ++frame) {
		const auto expected = std::max<std::int16_t>(
			100, static_cast<std::int16_t>(200 - (frame * 6U))
		);
		expect(tones[frame] == expected, "white-in tone staircase is exact");
	}
	return final_tick;
}

void test_entry_and_raw_locking()
{
	auto scene = make_scene(ClassicOpSelectionMode::vs_1p_cpu);
	if(!scene) { return; }
	expect(
		scene->raster().access_page() == Page::one,
		"selector enters with page 1 cleared and accessed"
	);
	reach_input(*scene, 30U);
	expect(
		scene->raster().access_page() == Page::zero &&
			scene->raster().shown_page() == Page::one,
		"Anniversary entry leaves access 0 and shown 1"
	);
	const auto hash_before_invalid = scene->raster().display_state_hash();
	const auto rand_before_invalid = scene->resident().resident_rand;
	const auto invalid = scene->advance({.single_player = 0x8000U});
	expect(
		!invalid && invalid.error().code ==
			th03::app::ClassicOpSelectionErrorCode::invalid_input &&
		scene->raster().display_state_hash() == hash_before_invalid &&
			scene->resident().resident_rand == rand_before_invalid,
		"invalid raw input rejects before selection or presentation mutation"
	);
	const auto initial = scene->local_selection()[0U];
	auto tick = scene->advance({.single_player = 0x0003U});
	expect(tick && tick->input_sampled, "Up+Down is one raw sampled input");
	expect(
		scene->local_selection()[0U] == initial &&
			scene->carry().input_locked[0U] && !tick->resident_changed,
		"independent Up and Down net to the same selection and lock input"
	);
	drain_frame(*scene);
	tick = scene->advance({.single_player = 0x0002U});
	expect(
		tick && scene->local_selection()[0U] == initial,
		"held nonzero input remains suppressed while locked"
	);
	drain_frame(*scene);
	tick = scene->advance();
	expect(
		tick && scene->local_selection()[0U] == initial &&
			!scene->carry().input_locked[0U],
		"exact zero unlocks and consumes the update"
	);
	drain_frame(*scene);
	tick = scene->advance({.single_player = 0x0002U});
	expect(
		tick && scene->local_selection()[0U] == initial + 1U,
		"a later Down moves after the release boundary"
	);
}

void test_curve_and_player_rendering()
{
	ClassicOpSelectionCarry carry{};
	carry.curve_cycle = 73U;
	auto curve_scene = make_scene(
		ClassicOpSelectionMode::vs_1p_cpu, {}, carry
	);
	if(!curve_scene) { return; }
	expect(
		ClassicOpSelectionTestAccess::draw_curves_only(*curve_scene),
		"curve-only test seam renders"
	);
	Raster expected{};
	expect(expected.set_access_page(Page::one),
		"reference selects the constructor access page");
	draw_curves_reference(expected, 73U, 8U);
	expect(
		std::ranges::equal(
			curve_scene->raster().graphics(Page::one),
			expected.graphics(Page::one)
		),
		"all 256 points of every curve and trail match the donor formula"
	);
	expect(curve_scene->carry().curve_cycle == 75U,
		"curve cycle advances by two after the complete draw");

	GameInit resident{};
	resident.resident_rand = 100;
	resident.paletted_character = {2, 3};
	auto scene = make_scene(ClassicOpSelectionMode::vs_1p_cpu, resident);
	if(!scene) { return; }
	expect(
		ClassicOpSelectionTestAccess::draw_player_only(*scene, 0U) &&
		scene->raster().pixel(Page::one, {40, 100}) == 1U,
		"unconfirmed portrait always uses the base cel despite resident palette"
	);
	expect(
		ClassicOpSelectionTestAccess::draw_cursor_only(*scene, 1U) &&
		std::ranges::all_of(
			scene->raster().graphics(Page::one),
			[](auto pixel) { return pixel == 0U; }
		),
		"CPU-mode P2 cursor stays hidden until P1 confirms"
	);
	reach_input(*scene, 30U);
	auto tick = scene->advance({.single_player = 0x0010U});
	expect(tick && tick->resident_changed,
		"P1 Bomb mutates the resident at confirmation start");
	const auto rand_before_callback = scene->resident().resident_rand;
	const auto final_callback = drain_confirmation(*scene);
	expect(
		scene->resident().resident_rand == rand_before_callback + 1 &&
			scene->phase() == ClassicOpSelectionPhase::draw_and_input &&
			final_callback && final_callback->page_flipped &&
			final_callback->resident_changed,
		"final callback publishes and increments resident rand on VSync 18"
	);
	expect(
		ClassicOpSelectionTestAccess::draw_player_only(*scene, 0U) &&
		scene->raster().pixel(Page::one, {40, 100}) == 2U,
		"confirmed portrait uses the loaded second-palette cel"
	);
	expect(
		ClassicOpSelectionTestAccess::draw_cursor_only(*scene, 1U) &&
		std::ranges::any_of(
			scene->raster().graphics(Page::one),
			[](auto pixel) { return pixel != 0U; }
		),
		"CPU-mode P2 cursor appears after P1 confirmation"
	);
}

void test_two_player_confirmation_order()
{
	GameInit resident{};
	resident.paletted_character = {1, 1};
	auto scene = make_scene(ClassicOpSelectionMode::vs_1p_2p, resident);
	if(!scene) { return; }
	reach_input(*scene, 46U);
	auto tick = scene->advance({
		.player_one = 0x0020U,
		.player_two = 0x0020U,
	});
	expect(
		tick && tick->confirming_player == 0U &&
			scene->resident().paletted_character[0U] == 1,
		"P1 confirmation starts before P2 on the same sample"
	);
	drain_confirmation(*scene);
	expect(
		scene->phase() == ClassicOpSelectionPhase::confirm_white_in,
		"P2 confirmation starts as soon as P1 callback completes"
	);
	expect(
		scene->resident().paletted_character[1U] == 2,
		"later confirmer toggles palette on exact collision"
	);
	drain_confirmation(*scene);
	expect(
		scene->confirmed()[0U] && scene->confirmed()[1U] &&
			scene->phase() == ClassicOpSelectionPhase::draw_and_input,
		"P2 final callback publishes the sampled frame on VSync 18"
	);
}

void test_shot_bomb_and_cancel()
{
	auto scene = make_scene(ClassicOpSelectionMode::vs_1p_cpu);
	if(!scene) { return; }
	reach_input(*scene, 30U);
	auto tick = scene->advance({.single_player = 0x0030U});
	expect(tick && tick->confirming_player == 0U, "Shot starts first flash");
	drain_confirmation(*scene);
	expect(
		scene->phase() == ClassicOpSelectionPhase::confirm_white_in &&
			scene->resident().paletted_character[0U] == 2,
		"Bomb starts a second flash and determines the final palette"
	);
	drain_confirmation(*scene);
	drain_frame(*scene);
	tick = scene->advance({.single_player = 0x1000U});
	expect(tick && tick->ready, "global Cancel wins on a later input boundary");
	const auto stop = scene->take_audio_command();
	expect(
		stop && stop->type == ClassicAudioCommandType::stop_song,
		"Cancel stops SELECT.M"
	);
	auto completion = scene->take_completion();
	expect(
		completion && completion->outcome() == ClassicOpSelectionOutcome::canceled &&
			completion->resident().game_mode == th03::game_mode::none &&
			completion->raster().access_page() == Page::zero &&
			completion->raster().shown_page() == Page::zero,
		"VS cancel returns the exact black page-0 handoff and clears mode only"
	);
}

void test_story_success()
{
	GameInit resident{};
	resident.rank = 2U;
	resident.resident_rand = 0x12345678;
	resident.paletted_character[0U] = 9;
	auto scene = make_scene(ClassicOpSelectionMode::story, resident);
	if(!scene) { return; }
	reach_input(*scene, 30U);
	auto tick = scene->advance({.single_player = 0x0020U});
	expect(tick && scene->resident().paletted_character[0U] == 1,
		"Story visually starts from Reimu without using prior resident P1");
	drain_confirmation(*scene);
	drain_frame(*scene);
	for(std::size_t frames = 0U;
		scene->phase() != ClassicOpSelectionPhase::ready && frames < 200U;
		++frames) {
		if(scene->phase() == ClassicOpSelectionPhase::draw_and_input) {
			tick = scene->advance();
		} else {
			tick = scene->advance();
		}
		expect(tick.has_value(), "Story fade route advances");
	}
	expect(scene->phase() == ClassicOpSelectionPhase::ready,
		"Story selection reaches the source fade terminal");
	expect(
		scene->raster().graphics(Page::zero).size() ==
			th03::present::graphics_page_pixels &&
		scene->raster().graphics(Page::one).size() ==
			th03::present::graphics_page_pixels,
		"selected scene remains composable while its Stop command is pending"
	);
	const auto stop = scene->take_audio_command();
	expect(stop && stop->type == ClassicAudioCommandType::stop_song,
		"Story handoff stops SELECT.M");
	auto completion = scene->take_completion();
	expect(
		completion && completion->outcome() == ClassicOpSelectionOutcome::selected,
		"Story publishes one selected completion"
	);
	if(!completion) { return; }
	const auto &result = completion->resident();
	std::array<bool, 7U> seen{};
	bool six_distinct = true;
	for(std::size_t stage = 0U; stage < 6U; ++stage) {
		const auto base = static_cast<std::size_t>(
			(static_cast<std::uint8_t>(result.story_opponents[stage]) - 1U) / 2U
		);
		six_distinct = six_distinct && base < seen.size() && !seen[base];
		if(base < seen.size()) { seen[base] = true; }
	}
	expect(
		result.game_mode == th03::game_mode::story && result.story_lives == 2U &&
			result.remaining_credits == 3U && result.skill == 120U &&
			result.paletted_character[1U] == result.story_opponents[0U] &&
			result.story_opponents[6U] == 3 &&
			result.story_opponents[7U] == 15 && result.story_opponents[8U] == 17 &&
			six_distinct,
		"Story finalizer produces the source-fresh route and resident values"
	);
}

void test_hidden_terminal_cancel()
{
	auto scene = make_scene(ClassicOpSelectionMode::story);
	if(!scene) { return; }
	reach_input(*scene, 30U);
	auto tick = scene->advance({.single_player = 0x0020U});
	expect(tick.has_value(), "terminal-cancel fixture confirms Story P1");
	drain_confirmation(*scene);
	for(std::size_t frame = 0U;
		scene->raster().palette_tone() != 2 && frame < 40U; ++frame) {
		tick = scene->advance();
		expect(tick.has_value(), "confirmed fade input frame advances");
		drain_frame(*scene);
	}
	expect(
		scene->phase() == ClassicOpSelectionPhase::draw_and_input &&
			scene->raster().palette_tone() == 2,
		"fade count 33 reaches tone 2 before the hidden terminal iteration"
	);
	const auto rand_before = scene->resident().resident_rand;
	tick = scene->advance({.single_player = 0x1000U});
	auto completion = scene->take_completion();
	expect(
		tick && tick->ready && !tick->page_flipped &&
			scene->phase() == ClassicOpSelectionPhase::consumed && completion &&
			completion->outcome() == ClassicOpSelectionOutcome::canceled &&
			completion->resident().resident_rand == rand_before,
		"hidden post-fade iteration still accepts Cancel without flip or rand"
	);
}

void test_invalid_geometry()
{
	std::array<CdgFile, 9U> portraits{};
	for(auto &portrait : portraits) {
		portrait = file(192U, 192U, 2U, 1U, true);
	}
	portraits.back() = file(192U, 191U, 2U, 1U, true);
	auto rejected = ClassicOpSelectionTestAccess::create_with_predecoded_assets(
		GameInit{}, ClassicOpSelectionMode::story, 9U, {}, Raster{},
		tram_with_cursors(), std::move(portraits),
		file(192U, 192U, 1U, 12U, true),
		file(128U, 64U, 1U, 4U, false),
		file(64U, 64U, 1U, 3U, false),
		file(32U, 44U, 9U, 1U, false), names(), palette()
	);
	expect(!rejected, "late wrong portrait geometry publishes no selector");
}

void test_encoded_resource_boundary_and_lifetime()
{
	auto resources = encoded_resources();
	GameInit resident{};
	resident.paletted_character = {1, 3};
	auto created = ClassicOpSelectionTestAccess::create_with_synthetic_resources(
		resident,
		ClassicOpSelectionMode::vs_1p_cpu,
		9U,
		{},
		Raster{},
		tram_with_cursors(),
		resources
	);
	expect(created.has_value(),
		"actual CDG/BFNT/RGB decoders construct the selection owner");
	if(!created) {
		std::cerr << "encoded-create-code=" << static_cast<unsigned int>(
			created.error().code
		);
		if(created.error().resource) {
			std::cerr << " role=" << static_cast<unsigned int>(
				created.error().resource->role
			);
		}
		if(created.error().asset_error) {
			std::cerr << " asset=" << static_cast<unsigned int>(
				created.error().asset_error->code
			) << " offset=" << created.error().asset_error->offset;
		}
		std::cerr << '\n';
	}
	resources.clear();
	if(created) {
		drain_start_audio(**created);
		const auto tick = (*created)->advance();
		expect(tick.has_value(),
			"selection remains usable after every caller resource owner dies");
	}

	auto malformed = encoded_resources();
	const auto last = malformed.size() - 1U;
	expect(
		malformed[last].spec.key.role ==
			MainResourceRole::character_selection_portrait,
		"late encoded fixture is Yumemi portrait"
	);
	auto corrupt = std::make_shared<std::vector<std::byte>>(
		*malformed[last].bytes
	);
	(*corrupt)[2U] = std::byte{0xA0U};
	(*corrupt)[3U] = std::byte{0x00U};
	malformed[last].bytes = corrupt;
	auto rejected = ClassicOpSelectionTestAccess::create_with_synthetic_resources(
		resident,
		ClassicOpSelectionMode::vs_1p_cpu,
		9U,
		{},
		Raster{},
		tram_with_cursors(),
		malformed
	);
	expect(!rejected,
		"late wrong encoded portrait geometry publishes no scene");
}

bool rectangle_has_ink(
	const Raster &raster,
	Page page,
	std::int32_t left,
	std::int32_t top,
	std::int32_t right,
	std::int32_t bottom
)
{
	for(auto y = top; y < bottom; ++y) {
		for(auto x = left; x < right; ++x) {
			const auto pixel = raster.pixel(page, {x, y});
			if(pixel && *pixel != 0U) { return true; }
		}
	}
	return false;
}

void local_original_gate(const std::filesystem::path &content_root)
{
	th03::app::MainResourceLoader loader(content_root);
	const auto title_resources = loader.load_op_title_menu();
	const auto selection_resources = loader.load_op_character_selection();
	expect(title_resources.has_value() && selection_resources.has_value(),
		"local OP title and selection closures authenticate");
	if(!title_resources || !selection_resources) { return; }

	GameInit resident{};
	resident.fast_op_animation = 1U;
	auto title = ClassicOpTitleMenuScene::create(
		resident, ClassicOpTitleMode::fast_fade, *title_resources
	);
	expect(title.has_value(),
		"local authenticated OP parent prepares MIKOFT and title state");
	if(!title) { return; }
	expect((*title)->advance().has_value(), "local fast-title black frame advances");
	expect((*title)->take_audio_command().has_value() &&
		(*title)->take_audio_command().has_value(),
		"local fast-title stop/load audio drains");
	expect((*title)->advance().has_value(), "local fast-title image frame advances");
	expect((*title)->take_audio_command().has_value(),
		"local fast-title play audio drains");
	for(std::uint16_t frame = 0U; frame < 25U; ++frame) {
		expect((*title)->advance().has_value(), "local fast-title fade advances");
	}
	expect((*title)->advance({.ok = true}).has_value(),
		"local title input opens the menu");
	for(std::uint16_t frame = 0U; frame < 14U; ++frame) {
		expect((*title)->advance().has_value(), "local OP main box expands");
	}
	expect((*title)->phase() == ClassicOpPhase::main_menu,
		"local OP reaches the main menu");
	expect((*title)->advance({.down = true, .ok = true}).has_value(),
		"local OP selects Versus");
	for(std::uint16_t frame = 0U; frame < 13U; ++frame) {
		expect((*title)->advance().has_value(),
			"local OP VS submenu expands");
	}
	expect((*title)->advance({.down = true}).has_value(),
		"local OP selects 1P-vs-2P");
	expect((*title)->advance().has_value(),
		"local OP releases the VS input lock");
	expect((*title)->advance({.ok = true}).has_value(),
		"local OP confirms 1P-vs-2P");
	auto title_completion = (*title)->take_completion();
	expect(title_completion.has_value(),
		"local OP publishes the typed MIKOFT-bearing completion");
	if(!title_completion) { return; }

	auto mismatched = ClassicOpCharacterSelection::create_from_op(
		*title_completion,
		ClassicOpSelectionMode::vs_1p_cpu,
		9U,
		{},
		*selection_resources
	);
	expect(
		!mismatched && mismatched.error().code ==
			th03::app::ClassicOpSelectionErrorCode::invalid_config &&
			title_completion->selection_mode() ==
				ClassicOpSelectionMode::vs_1p_2p,
		"selector rejects a mismatched VS mode without consuming the title handoff"
	);
	auto selection = ClassicOpCharacterSelection::create_from_op(
		*title_completion,
		ClassicOpSelectionMode::vs_1p_2p,
		9U,
		{},
		*selection_resources
	);
	expect(selection.has_value(),
		"local selection authenticates and decodes every original child asset");
	if(!selection) { return; }
	drain_start_audio(**selection);
	for(std::uint16_t frame = 0U; frame < 46U; ++frame) {
		expect((*selection)->advance().has_value(),
			"local two-player selection delay advances");
	}
	const auto first = (*selection)->advance();
	expect(first && first->input_sampled,
		"local selection renders and samples its first frame");
	expect(
		rectangle_has_ink((*selection)->raster(), Page::zero, 240, 128, 256, 160) &&
		rectangle_has_ink((*selection)->raster(), Page::zero, 384, 128, 400, 160),
		"authenticated MIKOFT renders both source cursor halves"
	);
	drain_frame(**selection);
	expect((*selection)->advance({.single_player = 0x1000U}).has_value(),
		"authenticated selector accepts Cancel");
	expect((*selection)->take_audio_command().has_value(),
		"authenticated selector Cancel publishes Stop");
	auto selection_completion = (*selection)->take_completion();
	expect(
		selection_completion && selection_completion->outcome() ==
			ClassicOpSelectionOutcome::canceled,
		"authenticated selector publishes its canceled completion"
	);
	if(!selection_completion) { return; }
	auto returned = ClassicOpTitleMenuScene::return_from_canceled_selection(
		*selection_completion, *title_resources
	);
	expect(
		returned && (*returned)->phase() == ClassicOpPhase::fast_black &&
			(*returned)->resident().fast_op_animation ==
				selection_completion->resident().fast_op_animation &&
			!(*returned)->take_audio_command(),
		"canceled selector returns through the fast title without toggling state"
	);
}

} // namespace

int main(int argc, char **argv)
{
	test_live_input_mode_normalization();
	test_entry_and_raw_locking();
	test_persisted_selection_ring_and_cpu_delay_width();
	test_curve_and_player_rendering();
	test_two_player_confirmation_order();
	test_shot_bomb_and_cancel();
	test_story_success();
	test_hidden_terminal_cancel();
	test_invalid_geometry();
	test_encoded_resource_boundary_and_lifetime();
	if(argc == 2) {
		local_original_gate(argv[1]);
	} else {
		expect(argc == 1, "optional local gate accepts one content root");
	}
	if(failures != 0U) {
		std::cerr << "checks=" << checks << " failures=" << failures << '\n';
		return 1;
	}
	std::cout << "all " << checks << " Classic OP selection checks passed\n";
	return 0;
}
