#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "classic/op/classic_op_selection_internal.hpp"
#include "th03/app/new_classic/content/new_classic_assets.hpp"
#include "th03/app/new_classic/content/new_classic_messages.hpp"
#include "th03/app/new_classic/replay/new_classic_replay.hpp"
#include "th03/exact/trig.hpp"

namespace {

using th03::GameInit;
using th03::app::classic_op_player_count;
using th03::app::ClassicAudioCommandType;
using th03::app::ClassicMusicCue;
using th03::app::ClassicOpCharacterSelection;
using th03::app::ClassicOpPhase;
using th03::app::ClassicOpPracticeBackgroundStatus;
using th03::app::ClassicOpSelectionCarry;
using th03::app::ClassicOpSelectionCompletion;
using th03::app::ClassicOpSelectionInput;
using th03::app::ClassicOpSelectionLocalizedText;
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

class SyntheticFont final : public th03::present::AnkGlyphProvider {
public:
	[[nodiscard]] std::optional<th03::present::AnkGlyph> glyph(
		std::uint8_t
	) const override
	{
		return std::nullopt;
	}

	[[nodiscard]] std::optional<th03::present::JisGlyph> jis_glyph(
		std::uint16_t
	) const override
	{
		th03::present::JisGlyph result{};
		result.row_halves.fill(0xFFU);
		return result;
	}
};

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

void put_le16(std::vector<std::byte> &bytes, std::size_t at, std::uint16_t value)
{
	bytes[at] = static_cast<std::byte>(value & 0xFFU);
	bytes[at + 1U] = static_cast<std::byte>(value >> 8U);
}

std::vector<std::byte> menu_font_bytes(std::uint8_t advance = 6U)
{
	std::vector<std::byte> bytes(3'168U, std::byte{0U});
	constexpr std::array magic{'T', '3', 'P', 'F', 'N', 'T', '1'};
	for(std::size_t index = 0U; index < magic.size(); ++index) {
		bytes[index] = static_cast<std::byte>(magic[index]);
	}
	put_le16(bytes, 8U, 3'168U);
	put_le16(bytes, 10U, 32U);
	put_le16(bytes, 12U, 32U);
	put_le16(bytes, 14U, 128U);
	put_le16(bytes, 16U, 95U);
	bytes[18U] = std::byte{0x20U};
	bytes[19U] = std::byte{0x7EU};
	bytes[20U] = std::byte{16U};
	bytes[21U] = std::byte{16U};
	bytes[22U] = std::byte{0x3FU};
	for(std::size_t glyph = 0U; glyph < 95U; ++glyph) {
		bytes[32U + glyph] = static_cast<std::byte>(advance);
		for(std::size_t row = 0U; row < 16U; ++row) {
			bytes[128U + (glyph * 32U) + (row * 2U)] =
				std::byte{0x80U};
		}
	}
	std::uint16_t checksum = 0U;
	for(std::size_t index = 32U; index < bytes.size(); ++index) {
		checksum = static_cast<std::uint16_t>(
			checksum + std::to_integer<std::uint8_t>(bytes[index])
		);
	}
	put_le16(bytes, 24U, checksum);
	return bytes;
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

void set_cel_pixel(
	PlanarCel4 &cel,
	std::size_t x,
	std::size_t y,
	std::uint8_t color
)
{
	const auto stride = static_cast<std::size_t>(cel.pixel_width) / 8U;
	const auto stored_y = static_cast<std::size_t>(cel.pixel_height) - 1U - y;
	const auto offset = (stored_y * stride) + (x / 8U);
	const auto mask = static_cast<std::uint8_t>(0x80U >> (x & 7U));
	for(std::size_t plane = 0U; plane < cel.color_planes.size(); ++plane) {
		if((color & static_cast<std::uint8_t>(1U << plane)) != 0U) {
			cel.color_planes[plane][offset] |= mask;
		} else {
			cel.color_planes[plane][offset] &= static_cast<std::uint8_t>(~mask);
		}
	}
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
	std::uint8_t available_character_count = 9U,
	bool retain_for_new_classic_practice = false,
	const ClassicOpSelectionLocalizedText *localized_text = nullptr,
	bool expect_success = true,
	const th03::app::NewClassicCharacterSelectAssets *generated_assets = nullptr,
	bool source_charge_overhang = false
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
	auto stats = file(128U, 64U, 1U, 4U, false);
	if(source_charge_overhang) {
		// Model the source English "Charge SP" overhang plus a neighboring
		// frame pixel so the generated-label test proves selective cleanup.
		set_cel_pixel(stats.cels.front(), 4U, 42U, 7U);
		set_cel_pixel(stats.cels.front(), 5U, 43U, 7U);
		set_cel_pixel(stats.cels.front(), 7U, 51U, 7U);
		set_cel_pixel(stats.cels.front(), 4U, 43U, 10U);
	}
	auto created = ClassicOpSelectionTestAccess::create_with_predecoded_assets(
		resident, mode, available_character_count, carry,
		Raster{}, tram_with_cursors(),
		std::move(portraits), file(192U, 192U, 1U, 12U, true),
		std::move(stats),
		file(64U, 64U, 1U, 3U, false),
		file(32U, 44U, 9U, 1U, false), names(), palette(),
		retain_for_new_classic_practice, localized_text, generated_assets
	);
	if(expect_success) {
		expect(created.has_value(), "synthetic selector creates");
	}
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

void test_player_index_precondition()
{
	// Exercise both legal players before proving that the source-private
	// forwarders reject the first invalid value and SIZE_MAX without clearing,
	// changing page state, or mutating selection/resident state.
	auto scene = make_scene(ClassicOpSelectionMode::vs_1p_cpu);
	if(!scene) { return; }
	expect(
		ClassicOpSelectionTestAccess::draw_player_only(*scene, 0U),
		"P1 player-only seam accepts the first player"
	);
	expect(
		ClassicOpSelectionTestAccess::draw_cursor_only(*scene, 1U),
		"P2 cursor-only seam accepts the second player"
	);
	expect(
		ClassicOpSelectionTestAccess::draw_stats_base_only(*scene, 0U),
		"P1 stats-base seam accepts the first player"
	);

	const auto hash_before_invalid = scene->raster().display_state_hash();
	const auto access_before_invalid = scene->raster().access_page();
	const auto shown_before_invalid = scene->raster().shown_page();
	const auto tone_before_invalid = scene->raster().palette_tone();
	const auto scroll_before_invalid = scene->raster().scroll_y();
	const auto carry_before_invalid = scene->carry();
	const auto selection_before_invalid = scene->local_selection();
	const auto confirmed_before_invalid = scene->confirmed();
	const auto resident_rand_before_invalid = scene->resident().resident_rand;
	const auto palette_before_invalid = scene->resident().paletted_character;
	const std::array<std::size_t, 2U> invalid_players{
		classic_op_player_count, std::numeric_limits<std::size_t>::max(),
	};
	for(const auto player : invalid_players) {
		expect(
			!ClassicOpSelectionTestAccess::draw_player_only(*scene, player),
			"player-only seam rejects an invalid player index"
		);
		expect(
			scene->raster().display_state_hash() == hash_before_invalid &&
				scene->raster().access_page() == access_before_invalid &&
				scene->raster().shown_page() == shown_before_invalid &&
				scene->raster().palette_tone() == tone_before_invalid &&
				scene->raster().scroll_y() == scroll_before_invalid &&
				scene->carry() == carry_before_invalid &&
				scene->local_selection() == selection_before_invalid &&
				scene->confirmed() == confirmed_before_invalid &&
				scene->resident().resident_rand == resident_rand_before_invalid &&
				scene->resident().paletted_character == palette_before_invalid,
			"invalid player-only index leaves the scene unchanged"
		);
		expect(
			!ClassicOpSelectionTestAccess::draw_cursor_only(*scene, player),
			"cursor-only seam rejects an invalid player index"
		);
		expect(
			scene->raster().display_state_hash() == hash_before_invalid &&
				scene->raster().access_page() == access_before_invalid &&
				scene->raster().shown_page() == shown_before_invalid &&
				scene->raster().palette_tone() == tone_before_invalid &&
				scene->raster().scroll_y() == scroll_before_invalid &&
				scene->carry() == carry_before_invalid &&
				scene->local_selection() == selection_before_invalid &&
				scene->confirmed() == confirmed_before_invalid &&
				scene->resident().resident_rand == resident_rand_before_invalid &&
				scene->resident().paletted_character == palette_before_invalid,
			"invalid cursor-only index leaves the scene unchanged"
		);
		expect(
			!ClassicOpSelectionTestAccess::draw_stats_base_only(*scene, player),
			"stats-base seam rejects an invalid player index"
		);
		expect(
			scene->raster().display_state_hash() == hash_before_invalid &&
				scene->raster().access_page() == access_before_invalid &&
				scene->raster().shown_page() == shown_before_invalid &&
				scene->raster().palette_tone() == tone_before_invalid &&
				scene->raster().scroll_y() == scroll_before_invalid &&
				scene->carry() == carry_before_invalid &&
				scene->local_selection() == selection_before_invalid &&
				scene->confirmed() == confirmed_before_invalid &&
				scene->resident().resident_rand == resident_rand_before_invalid &&
				scene->resident().paletted_character == palette_before_invalid,
			"invalid stats-base index leaves the scene unchanged"
		);
	}
}

void test_localized_name_rendering()
{
	SyntheticFont font;
	std::array<std::vector<std::uint8_t>, 9U> source{};
	for(std::size_t character = 0U; character < source.size(); ++character) {
		source[character].assign(character + 1U, 'A');
	}
	ClassicOpSelectionLocalizedText localized{};
	for(std::size_t character = 0U; character < source.size(); ++character) {
		localized.character_names[character] = source[character];
	}
	localized.font = &font;
	auto scene = make_scene(
		ClassicOpSelectionMode::story, {}, {}, 9U, false, &localized
	);
	if(!scene) { return; }
	expect(
		ClassicOpSelectionTestAccess::draw_names_only(*scene),
		"localized selection names draw without the source BFNT rows"
	);
	expect(
		scene->raster().pixel(Page::one, {315, 136}) == 0U &&
			scene->raster().pixel(Page::one, {316, 136}) == 15U &&
			scene->raster().pixel(Page::one, {323, 136}) == 15U &&
			scene->raster().pixel(Page::one, {324, 136}) == 0U,
		"one-glyph localized name is centered in the source 128-pixel row"
	);
	expect(
		scene->raster().pixel(Page::one, {311, 156}) == 0U &&
			scene->raster().pixel(Page::one, {312, 156}) == 15U &&
			scene->raster().pixel(Page::one, {327, 156}) == 15U &&
			scene->raster().pixel(Page::one, {328, 156}) == 0U,
		"two-glyph localized name keeps fixed-center alignment"
	);

	auto classic = make_scene(ClassicOpSelectionMode::story);
	if(!classic) { return; }
	expect(
		ClassicOpSelectionTestAccess::draw_names_only(*classic) &&
			classic->raster().pixel(Page::one, {256, 136}) != 0U &&
			classic->raster().pixel(Page::one, {384, 136}) == 0U,
		"sealed Classic keeps the exact two-cel source BFNT path"
	);

	constexpr std::array<th03::app::NewClassicMessageId, 9U> name_ids{
		th03::app::NewClassicMessageId::replay_character_reimu,
		th03::app::NewClassicMessageId::replay_character_mima,
		th03::app::NewClassicMessageId::replay_character_marisa,
		th03::app::NewClassicMessageId::replay_character_ellen,
		th03::app::NewClassicMessageId::replay_character_kotohime,
		th03::app::NewClassicMessageId::replay_character_kana,
		th03::app::NewClassicMessageId::replay_character_rikako,
		th03::app::NewClassicMessageId::replay_character_chiyuri,
		th03::app::NewClassicMessageId::replay_character_yumemi,
	};
	for(const auto profile : {
		th03::app::NewClassicLanguageAssetProfile::japanese,
		th03::app::NewClassicLanguageAssetProfile::english,
	}) {
		const auto &messages = th03::app::new_classic_builtin_messages(profile);
		ClassicOpSelectionLocalizedText catalog_names{};
		catalog_names.font = &font;
		for(std::size_t character = 0U; character < name_ids.size(); ++character) {
			const auto name = messages.text(name_ids[character]);
			catalog_names.character_names[character] = {
				reinterpret_cast<const std::uint8_t *>(name.data()), name.size(),
			};
		}
		auto catalog_scene = make_scene(
			ClassicOpSelectionMode::story, {}, {}, 9U, false, &catalog_names
		);
		expect(
			catalog_scene &&
				ClassicOpSelectionTestAccess::draw_names_only(*catalog_scene),
			"shipped locale compact names fit the character-selector rows"
		);
	}
}

void test_localized_stat_rendering()
{
	SyntheticFont graphics_font;
	std::array<std::array<std::uint8_t, 1U>, 9U> names{};
	for(auto &name : names) {
		name[0U] = 'A';
	}
	auto menu_font = th03::app::detail::decode_new_classic_menu_font(
		menu_font_bytes()
	);
	expect(menu_font.has_value(), "synthetic stat-label menu font decodes");
	if(!menu_font) { return; }
	ClassicOpSelectionLocalizedText localized{};
	for(std::size_t character = 0U; character < names.size(); ++character) {
		localized.character_names[character] = names[character];
	}
	localized.stat_labels = {"Speed", "Power", "Chrg"};
	localized.font = &graphics_font;
	localized.menu_font = &*menu_font;
	auto classic = make_scene(ClassicOpSelectionMode::story);
	auto new_classic = make_scene(
		ClassicOpSelectionMode::story, {}, {}, 9U, false, &localized
	);
	if(!classic || !new_classic) { return; }
	expect(
		ClassicOpSelectionTestAccess::draw_stats_base_only(*classic) &&
			ClassicOpSelectionTestAccess::draw_stats_base_only(*new_classic),
		"Classic and localized stat sheets draw independently"
	);
	bool outside_identical = true;
	bool source_untouched = true;
	std::array<bool, 3U> rows_have_ink{};
	for(std::int32_t y = 304; y < 368; ++y) {
		for(std::int32_t x = 32; x < 160; ++x) {
			const auto source = classic->raster().pixel(Page::zero, {x, y});
			const auto localized_pixel = new_classic->raster().pixel(
				Page::zero, {x, y}
			);
			source_untouched = source_untouched && source == 4U;
			const auto local_x = x - 32;
			const auto local_y = y - 304;
			const auto row = (local_y >= 7 && local_y < 55) ?
				((local_y - 7) / 16) : -1;
			const auto inside = local_x >= 8 && local_x < 64 &&
				row >= 0 && row < 3;
			if(!inside) {
				outside_identical = outside_identical &&
					localized_pixel == source;
			} else if(localized_pixel == 7U) {
				rows_have_ink[static_cast<std::size_t>(row)] = true;
			}
		}
	}
	expect(
		source_untouched,
		"sealed Classic retains the source selection-stat sheet pixels"
	);
	expect(
		outside_identical,
		"localized stat labels preserve every border, rule, and star-field pixel"
	);
	expect(
		std::ranges::all_of(rows_have_ink, [](bool ink) noexcept { return ink; }),
		"all three localized stat rows render foreground ink"
	);

	auto too_wide_font = th03::app::detail::decode_new_classic_menu_font(
		menu_font_bytes(16U)
	);
	expect(too_wide_font.has_value(), "wide stat-label fixture decodes");
	if(!too_wide_font) { return; }
	localized.menu_font = &*too_wide_font;
	auto rejected = make_scene(
		ClassicOpSelectionMode::story, {}, {}, 9U, false, &localized, false
	);
	expect(
		!rejected,
		"localized stat labels that exceed their source region fail closed"
	);
}

void test_generated_character_select_asset_rendering()
{
	const auto path = std::filesystem::path(TH03_TEST_SOURCE_DIR) /
		"assets" / "new-classic" / "menu-backgrounds" /
		"T3CSP001-ES-TEST.BIN";
	auto generated = th03::app::load_new_classic_character_select_assets(path);
	expect(generated.has_value(),
		"tracked generated Character Select assets load for scene coverage");
	if(!generated) { return; }
	auto classic = make_scene(
		ClassicOpSelectionMode::story, {}, {}, 9U, false, nullptr, true,
		nullptr, true
	);
	auto localized = make_scene(
		ClassicOpSelectionMode::story, {}, {}, 9U, false, nullptr, true,
		&*generated, true
	);
	if(!classic || !localized) { return; }
	expect(
		ClassicOpSelectionTestAccess::draw_names_only(*classic) &&
		ClassicOpSelectionTestAccess::draw_names_only(*localized),
		"source BFNT names use the production superimpose path"
	);
	expect(
		std::ranges::equal(
			classic->raster().graphics(Page::one),
			localized->raster().graphics(Page::one)
		),
		"generated stat localization passes authenticated English names through byte-for-byte"
	);
	expect(
		ClassicOpSelectionTestAccess::draw_stats_base_only(*classic) &&
		ClassicOpSelectionTestAccess::draw_stats_base_only(*localized),
		"generated CDG stat labels compose over the source sheet"
	);
	bool protected_pixels_match = true;
	bool label_pixels_changed = false;
	bool source_residue_existed = false;
	bool source_residue_cleared = true;
	bool residue_decoration_preserved = true;
	for(std::int32_t y = 304; y < 368; ++y) {
		for(std::int32_t x = 32; x < 160; ++x) {
			const auto before = classic->raster().pixel(Page::zero, {x, y});
			const auto after = localized->raster().pixel(Page::zero, {x, y});
			const auto inside_label =
				x >= 40 && x < 96 && y >= 311 && y < 359;
			const auto inside_charge_overhang =
				x >= 36 && x < 40 && y >= 346 && y < 356;
			const auto inside = inside_label || inside_charge_overhang;
			if(inside) {
				label_pixels_changed = label_pixels_changed || before != after;
				if(inside_charge_overhang) {
					if(before == 7U) {
						source_residue_existed = true;
						source_residue_cleared =
							source_residue_cleared && after == 14U;
					} else {
						residue_decoration_preserved =
							residue_decoration_preserved && after == before;
					}
				}
			} else {
				protected_pixels_match = protected_pixels_match && before == after;
			}
		}
	}
	expect(label_pixels_changed,
		"generated stat labels replace pixels inside the reviewed safe cells");
	expect(protected_pixels_match,
		"generated stat alpha preserves every border and star-field pixel");
	expect(
		source_residue_existed && source_residue_cleared &&
			residue_decoration_preserved,
		"generated Charge label clears only the source foreground overhang"
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
	resident.resident_rand = 1;
	const auto initial_rand = resident.resident_rand;
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
	expect(
		result.resident_rand == initial_rand + 33 &&
			result.resident_rand == 34,
		"earliest Story confirmation publishes 33 selector frames and reaches seed 34"
	);
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

void test_new_classic_practice_retains_selection()
{
	auto scene = make_scene(
		ClassicOpSelectionMode::vs_1p_cpu, {}, {}, 9U, true
	);
	if(!scene) { return; }
	reach_input(*scene, 30U);
	expect(scene->advance({.single_player = 0x0020U}).has_value(),
		"Practice-retained selector confirms P1");
	drain_confirmation(*scene);
	for(std::size_t frame = 0U; frame < 14U; ++frame) {
		expect(scene->advance().has_value(),
			"Practice-retained selector preserves the P1 handoff delay");
		drain_frame(*scene);
	}
	expect(scene->advance({.single_player = 0x0020U}).has_value(),
		"Practice-retained selector confirms CPU");
	drain_confirmation(*scene);
	expect(
		scene->phase() == ClassicOpSelectionPhase::ready &&
		!scene->take_audio_command(),
		"Practice Setup receives the published confirmed frame without stopping SELECT.M"
	);
	const auto selected = scene->local_selection();
	auto completion = scene->take_completion();
	const auto shown_before_practice = scene->raster().shown_page();
	const auto cycle_before_practice = scene->carry().curve_cycle;
	const auto rand_before_practice = std::bit_cast<std::uint32_t>(
		scene->resident().resident_rand
	);
	const auto practice_wait_1 =
		scene->advance_new_classic_practice_background();
	const auto cycle_after_wait_1 = scene->carry().curve_cycle;
	const auto shown_after_wait_1 = scene->raster().shown_page();
	const auto rand_after_wait_1 = std::bit_cast<std::uint32_t>(
		scene->resident().resident_rand
	);
	const auto practice_wait_2 =
		scene->advance_new_classic_practice_background();
	const auto cycle_after_wait_2 = scene->carry().curve_cycle;
	const auto shown_after_wait_2 = scene->raster().shown_page();
	const auto rand_after_wait_2 = std::bit_cast<std::uint32_t>(
		scene->resident().resident_rand
	);
	const auto practice_background =
		scene->advance_new_classic_practice_background();
	const auto practice_page = scene->raster().graphics(
		scene->raster().shown_page()
	);
	const auto curves_only = std::ranges::all_of(
		practice_page,
		[](std::uint8_t color) {
			return color == 0U || color == 1U || color == 5U || color == 6U;
		}
	) && std::ranges::any_of(
		practice_page, [](std::uint8_t color) { return color != 0U; }
	);
	expect(
		practice_wait_1 == ClassicOpPracticeBackgroundStatus::waiting &&
		practice_wait_2 == ClassicOpPracticeBackgroundStatus::waiting &&
		practice_background == ClassicOpPracticeBackgroundStatus::published &&
		cycle_after_wait_1 == static_cast<std::uint8_t>(
			cycle_before_practice + 2U
		) && cycle_after_wait_2 == cycle_after_wait_1 &&
		shown_after_wait_1 == shown_before_practice &&
		shown_after_wait_2 == shown_before_practice &&
		rand_after_wait_1 == rand_before_practice &&
		rand_after_wait_2 == rand_before_practice &&
		scene->raster().shown_page() != shown_before_practice &&
		scene->carry().curve_cycle == static_cast<std::uint8_t>(
			cycle_before_practice + 2U
		) &&
		std::bit_cast<std::uint32_t>(scene->resident().resident_rand) ==
			rand_before_practice + 1U &&
		scene->resident().random_seed == rand_before_practice + 1U &&
		curves_only,
		"Practice waits three VSyncs, then advances retained curve/page/rand owners without redrawing selector portraits"
	);
	const auto cancel_wait_1 =
		scene->advance_new_classic_practice_background();
	const auto cancel_wait_2 =
		scene->advance_new_classic_practice_background();
	const auto cancel_background =
		scene->advance_new_classic_practice_background();
	expect(
		cancel_wait_1 == ClassicOpPracticeBackgroundStatus::waiting &&
		cancel_wait_2 == ClassicOpPracticeBackgroundStatus::waiting &&
		cancel_background == ClassicOpPracticeBackgroundStatus::published &&
		scene->carry().curve_cycle == static_cast<std::uint8_t>(
			cycle_before_practice + 4U
		) &&
		std::bit_cast<std::uint32_t>(scene->resident().resident_rand) ==
			rand_before_practice + 2U,
		"Practice Esc can publish its required second three-VSync curves-only frame"
	);
	expect(
		completion && scene->resume_from_new_classic_practice() &&
		scene->phase() == ClassicOpSelectionPhase::draw_and_input &&
		scene->local_selection() == selected &&
		scene->confirmed() == std::array<bool, 2U>{false, false} &&
		!scene->take_audio_command(),
		"Practice Esc resumes the retained selector in place without reloading or restarting music"
	);
	const auto held = scene->advance({.single_player = 0x1000U});
	expect(
		held && !held->ready,
		"Practice Esc remains release-locked when the retained selector resumes"
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

void test_new_classic_restart_resident_preparation()
{
	GameInit handoff{};
	handoff.global_frame = 41U;
	handoff.resident_rand = 1;
	handoff.random_seed = 2U;
	handoff.game_mode = th03::game_mode::story;
	th03::app::preserve_new_classic_restart_random_seed(
		handoff, 0xFEDCBA98U
	);
	expect(
		std::bit_cast<std::uint32_t>(handoff.resident_rand) == 0xFEDCBA98U &&
		handoff.random_seed == 2U && handoff.global_frame == 41U &&
		handoff.game_mode == th03::game_mode::story,
		"0.4.11 restart handoff replaces only resident rand with the live seed"
	);

	GameInit story{
		.global_frame = 900U,
		.resident_rand = 1234,
		.game_mode = th03::game_mode::story,
		.rank = 2U,
		.key_mode = 1U,
		.story_stage = 6U,
		.story_lives = 0U,
		.remaining_credits = 1U,
		.paletted_character = {1, 15},
		.runtime_snapshot = {},
		.compact_runtime_snapshot = {},
		.round_start_snapshot = {},
		.practice_rules = {},
	};
	story.last_score[0U].fill(9U);
	story.practice_rules = th03::PracticeRules{};
	const auto first = th03::app::prepare_new_classic_story_restart(story);
	const auto second = th03::app::prepare_new_classic_story_restart(story);
	expect(first.game_mode == th03::game_mode::story &&
		first.story_stage == 0U && first.story_lives == 2U &&
		first.remaining_credits == 3U && first.skill == 120U &&
		first.is_cpu == std::array<std::uint8_t, 2U>{0U, 1U} &&
		first.paletted_character[0U] == 1 &&
		first.paletted_character[1U] == first.story_opponents[0U] &&
		first.story_opponents == second.story_opponents,
		"Story Restart rebuilds deterministic Stage 1 opponents and native stocks");
	expect(first.global_frame == 0U && first.random_seed == 1234U &&
		!first.runtime_snapshot && !first.compact_runtime_snapshot &&
		!first.round_start_snapshot && !first.practice_rules &&
		first.last_score == decltype(first.last_score){},
		"Story Restart preserves the outgoing resident seed but clears run-local state");

	GameInit versus{
		.global_frame = 500U,
		.resident_rand = 4321,
		.game_mode = th03::game_mode::vs_1p_2p,
		.rank = 3U,
		.key_mode = 2U,
		.story_stage = 4U,
		.paletted_character = {3, 12},
		.autofire_mask = 3U,
		.runtime_snapshot = {},
		.compact_runtime_snapshot = {},
		.round_start_snapshot = {},
		.practice_rules = {},
	};
	versus.last_score[1U].fill(8U);
	const auto restarted = th03::app::prepare_new_classic_vs_restart(versus);
	expect(restarted.game_mode == th03::game_mode::vs_1p_2p &&
		restarted.paletted_character == versus.paletted_character &&
		restarted.rank == versus.rank && restarted.key_mode == versus.key_mode &&
		restarted.autofire_mask == versus.autofire_mask &&
		restarted.is_cpu == std::array<std::uint8_t, 2U>{0U, 0U} &&
		restarted.story_stage == 0U && restarted.global_frame == 0U &&
		restarted.random_seed == 4321U &&
		restarted.last_score == decltype(restarted.last_score){},
		"VS Restart preserves the matchup/options and resets the same-mode match");
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
		*selection_completion, *title_resources, {}
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
	test_player_index_precondition();
	test_localized_name_rendering();
	test_localized_stat_rendering();
	test_generated_character_select_asset_rendering();
	test_two_player_confirmation_order();
	test_shot_bomb_and_cancel();
	test_story_success();
	test_new_classic_practice_retains_selection();
	test_hidden_terminal_cancel();
	test_invalid_geometry();
	test_encoded_resource_boundary_and_lifetime();
	test_new_classic_restart_resident_preparation();
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
