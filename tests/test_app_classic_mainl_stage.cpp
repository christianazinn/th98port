#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/app/main_process.hpp"
#include "th03/app/resources.hpp"
#include "th03/present/source_surface.hpp"
#include "th03/present/text.hpp"

#include "classic_mainl_stage_internal.hpp"
#include "classic_op_scene_internal.hpp"
#include "classic_op_selection_internal.hpp"

namespace {

using th03::app::ClassicMainlStageErrorCode;
using th03::app::ClassicMainlStagePhase;
using th03::app::ClassicMainlStageTestAccess;
using th03::app::ClassicAudioCommand;
using th03::app::ClassicAudioCommandType;
using th03::app::ClassicGameplayMusic;
using th03::app::ClassicMusicCue;
using th03::app::ClassicOpSelectionCompletion;
using th03::app::ClassicOpSelectionMode;
using th03::app::ClassicOpSelectionOutcome;
using th03::app::ClassicOpSelectionTestAccess;
using th03::app::ClassicOpCompletion;
using th03::app::ClassicOpRoute;
using th03::app::ClassicOpSceneTestAccess;
using th03::app::LoadedMainResource;
using th03::app::MainCharacter;
using th03::app::MainPaletteVariant;
using th03::app::MainResourceKey;
using th03::app::MainResourceRole;
using th03::app::MainStoryConfig;
using th03::app::MainlStageTextAssets;
using th03::app::ResourceBytes;
using th03::present::AnkGlyph;
using th03::present::AnkGlyphProvider;
using th03::present::JisGlyph;
using th03::present::Page;
using th03::present::Point;

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

void append_be16(std::vector<std::byte> &bytes, std::uint16_t value)
{
	append_u8(bytes, static_cast<std::uint8_t>(value >> 8U));
	append_u8(bytes, static_cast<std::uint8_t>(value));
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

class BitWriter final {
public:
	void bit(std::uint8_t value)
	{
		if((count_ % 8U) == 0U) {
			bytes_.push_back(std::byte{0U});
		}
		const auto shift = static_cast<unsigned int>(7U - (count_ % 8U));
		auto byte = std::to_integer<std::uint8_t>(bytes_.back());
		byte = static_cast<std::uint8_t>(
			byte | static_cast<std::uint8_t>((value & 1U) << shift)
		);
		bytes_.back() = static_cast<std::byte>(byte);
		++count_;
	}

	void bits(std::uint32_t value, unsigned int count)
	{
		for(auto remaining = count; remaining > 0U; --remaining) {
			bit(static_cast<std::uint8_t>(
				(value >> (remaining - 1U)) & 1U
			));
		}
	}

	[[nodiscard]] const std::vector<std::byte> &bytes() const noexcept
	{
		return bytes_;
	}

private:
	std::vector<std::byte> bytes_{};
	std::size_t count_ = 0U;
};

class ColorEncoder final {
public:
	ColorEncoder()
	{
		for(std::size_t previous = 0U; previous < table_.size(); ++previous) {
			for(std::size_t index = 0U; index < table_[previous].size(); ++index) {
				table_[previous][index] = static_cast<std::uint8_t>(
					(previous + index + 1U) & 0x0FU
				);
			}
		}
	}

	void color(BitWriter &bits, std::uint8_t previous, std::uint8_t color)
	{
		auto &table = table_[previous & 0x0FU];
		const auto found = std::find(table.begin(), table.end(), color);
		const auto index = static_cast<std::uint8_t>(
			std::distance(table.begin(), found)
		);
		write_code(bits, static_cast<std::uint8_t>(index ^ 0x0FU));
		std::move(found + 1, table.end(), found);
		table.back() = color;
	}

private:
	static void write_code(BitWriter &bits, std::uint8_t code)
	{
		if(code <= 1U) {
			bits.bit(1U);
			bits.bits(code, 1U);
		} else if(code <= 3U) {
			bits.bits(0U, 2U);
			bits.bits(static_cast<std::uint32_t>(code - 2U), 1U);
		} else if(code <= 7U) {
			bits.bits(2U, 3U);
			bits.bits(static_cast<std::uint32_t>(code - 4U), 2U);
		} else {
			bits.bits(3U, 3U);
			bits.bits(static_cast<std::uint32_t>(code - 8U), 3U);
		}
	}

	std::array<std::array<std::uint8_t, 16U>, 16U> table_{};
};

void write_position(BitWriter &bits, std::uint8_t position)
{
	if(position < 3U) {
		bits.bits(position, 2U);
	} else {
		bits.bits(3U, 2U);
		bits.bit(static_cast<std::uint8_t>(position - 3U));
	}
}

void write_length(BitWriter &bits, std::size_t length)
{
	unsigned int suffix = 0U;
	for(auto remaining = length; remaining > 1U; remaining >>= 1U) {
		++suffix;
	}
	for(unsigned int index = 0U; index < suffix; ++index) {
		bits.bit(1U);
	}
	bits.bit(0U);
	const auto base = std::size_t{1U} << suffix;
	bits.bits(static_cast<std::uint32_t>(length - base), suffix);
}

std::vector<std::byte> make_pi(
	std::uint16_t width,
	std::uint16_t height,
	std::size_t exact_size,
	std::uint8_t fill,
	std::uint8_t palette_seed
)
{
	std::vector<std::byte> bytes;
	for(const auto value : {'P', 'i', 'T', '3'}) {
		append_u8(bytes, static_cast<std::uint8_t>(value));
	}
	append_u8(bytes, 0x1AU);
	append_u8(bytes, 0U);
	append_u8(bytes, 0U);
	append_u8(bytes, 0U);
	append_u8(bytes, 0U);
	append_u8(bytes, 4U);
	for(const auto value : {1U, 2U, 3U, 4U}) {
		append_u8(bytes, static_cast<std::uint8_t>(value));
	}
	append_be16(bytes, 0U);
	append_be16(bytes, width);
	append_be16(bytes, height);
	for(std::uint8_t color = 0U; color < 16U; ++color) {
		append_u8(bytes, static_cast<std::uint8_t>(
			((color + palette_seed) & 0x0FU) << 4U
		));
		append_u8(bytes, static_cast<std::uint8_t>(
			((15U - color + palette_seed) & 0x0FU) << 4U
		));
		append_u8(bytes, static_cast<std::uint8_t>(
			((color ^ palette_seed) & 0x0FU) << 4U
		));
	}
	BitWriter bits;
	ColorEncoder colors;
	colors.color(bits, 0U, fill);
	colors.color(bits, fill, fill);
	write_position(bits, 1U);
	write_length(bits, (static_cast<std::size_t>(width) * height / 2U) - 1U);
	bytes.insert(bytes.end(), bits.bytes().begin(), bits.bytes().end());
	expect(bytes.size() <= exact_size, "synthetic PI fits manifest size");
	bytes.resize(exact_size, std::byte{0U});
	return bytes;
}

std::vector<std::byte> make_bfnt_gaiji()
{
	std::vector<std::byte> bytes;
	for(const auto value : {'B', 'F', 'N', 'T'}) {
		append_u8(bytes, static_cast<std::uint8_t>(value));
	}
	append_u8(bytes, 0x1AU);
	append_u8(bytes, 0U);
	append_u8(bytes, 0x16U);
	append_u8(bytes, 0U);
	append_le16(bytes, 16U);
	append_le16(bytes, 16U);
	append_le16(bytes, 0U);
	append_le16(bytes, 255U);
	for(std::size_t index = 0U; index < 8U; ++index) {
		append_u8(bytes, 0U);
	}
	append_le32(bytes, 0U);
	append_le16(bytes, 0U);
	append_le16(bytes, 0U);
	for(std::size_t glyph = 0U; glyph < 256U; ++glyph) {
		for(std::size_t byte = 0U; byte < 32U; ++byte) {
			append_u8(bytes, static_cast<std::uint8_t>(glyph + byte + 1U));
		}
	}
	return bytes;
}

std::vector<std::byte> make_cdg(
	std::uint16_t width,
	std::uint16_t height,
	std::uint8_t cel_count,
	bool portrait
)
{
	const auto stride = static_cast<std::size_t>(width / 8U);
	const auto plane_size = stride * height;
	std::vector<std::byte> bytes;
	append_le16(bytes, static_cast<std::uint16_t>(plane_size));
	append_le16(bytes, width);
	append_le16(bytes, height);
	append_le16(bytes, static_cast<std::uint16_t>(80U * (height - 1U)));
	append_le16(bytes, static_cast<std::uint16_t>(width / 32U));
	append_u8(bytes, cel_count);
	append_u8(bytes, 0U);
	append_le16(bytes, 0U);
	append_le16(bytes, 0U);
	for(std::size_t cel = 0U; cel < cel_count; ++cel) {
		bytes.insert(bytes.end(), plane_size, std::byte{0xFFU});
		for(std::size_t plane = 0U; plane < 4U; ++plane) {
			for(std::size_t row = 0U; row < height; ++row) {
				for(std::size_t x = 0U; x < stride; ++x) {
					std::uint8_t value = 0U;
					if(portrait) {
						if(plane == 0U && x < (stride / 2U)) {
							value = 0xFFU;
						}
						if(plane == 1U && x >= (stride / 2U)) {
							value = 0xFFU;
						}
						if(plane == 3U && cel == 1U) {
							value = 0xFFU;
						}
					} else {
						const auto color = static_cast<std::uint8_t>(
							1U + (cel % 14U)
						);
						if((color & (1U << plane)) != 0U) {
							value = 0xFFU;
						}
					}
					append_u8(bytes, value);
				}
			}
		}
	}
	return bytes;
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
		JisGlyph glyph{};
		for(std::size_t row = 0U; row < 16U; ++row) {
			glyph.row_halves[row * 2U] = (row == 0U) ? 0x80U : 0U;
		}
		return glyph;
	}
};

bool kotohime_fade(
	void *,
	const th03::BombFrameStep &,
	std::uint8_t,
	std::uint8_t &red_out
) noexcept
{
	red_out = 0U;
	return true;
}

MainStoryConfig story_config(
	std::uint8_t stage = 0U,
	std::int8_t player_one = 2
) noexcept
{
	MainStoryConfig config{};
	auto &resident = config.resident;
	resident.resident_rand = 12345;
	resident.random_seed = 12345U;
	resident.game_mode = th03::game_mode::story;
	resident.rank = 1U;
	resident.key_mode = 0U;
	resident.story_stage = stage;
	resident.story_lives = 2U;
	resident.remaining_credits = 3U;
	resident.skill = 3U;
	resident.is_cpu = {0U, 1U};
	resident.paletted_character[0U] = player_one;
	constexpr std::array<std::uint8_t, 9U> fixed_base{
		1U, 0U, 0U, 2U, 0U, 3U, 5U, 4U, 6U,
	};
	const auto player_base = static_cast<std::size_t>(
		(static_cast<std::uint8_t>(player_one) - 1U) / 2U
	);
	const auto fixed = fixed_base[player_base];
	std::size_t early = 0U;
	for(std::uint8_t base = 0U; base < 7U; ++base) {
		if(base == fixed) {
			continue;
		}
		auto paletted = static_cast<std::int8_t>((base * 2U) + 1U);
		if(paletted == player_one) {
			++paletted;
		}
		resident.story_opponents[early++] = paletted;
	}
	for(std::size_t current = 6U; current < 9U; ++current) {
		const auto base = (current == 6U) ? fixed :
			static_cast<std::uint8_t>(current);
		auto paletted = static_cast<std::int8_t>((base * 2U) + 1U);
		if(paletted == player_one) {
			++paletted;
		}
		resident.story_opponents[current] = paletted;
	}
	resident.paletted_character[1U] = resident.story_opponents[stage];
	config.kotohime_compatibility.component_provider = &kotohime_fade;
	return config;
}

MainlStageTextAssets text_assets()
{
	MainlStageTextAssets result{};
	for(std::size_t index = 0U; index < result.characters.size(); ++index) {
		result.characters[index].title = {
			static_cast<std::uint8_t>('A' + index),
		};
		result.characters[index].name = {
			static_cast<std::uint8_t>('a' + index),
		};
	}
	return result;
}

std::uint16_t extra_height(MainResourceKey key)
{
	return (key.character == MainCharacter::reimu ||
		key.character == MainCharacter::kana ||
		key.character == MainCharacter::rikako) ? 144U : 128U;
}

std::uint8_t pi_color(MainResourceKey key)
{
	switch(key.role) {
	case MainResourceRole::stage_splash_base: return 2U;
	case MainResourceRole::stage_splash_background: return 3U;
	case MainResourceRole::character_stage_shot:
		return (key.palette == MainPaletteVariant::first) ? 4U : 5U;
	case MainResourceRole::character_stage_extra_shot:
		return (key.palette == MainPaletteVariant::first) ? 6U : 7U;
	case MainResourceRole::stage_enemy_center: return 8U;
	case MainResourceRole::character_stage_enemy_group: return 9U;
	default: break;
	}
	return 1U;
}

std::vector<std::byte> bytes_for(const th03::app::MainResourceSpec &spec)
{
	switch(spec.key.role) {
	case MainResourceRole::mainl_gaiji_font:
		return make_bfnt_gaiji();
	case MainResourceRole::character_stage_portrait:
		return make_cdg(192U, 192U, 2U, true);
	case MainResourceRole::stage_number_sheet:
		return make_cdg(32U, 34U, 10U, false);
	case MainResourceRole::stage_splash_base:
		return make_pi(640U, 400U, spec.expected_bytes, 2U, 1U);
	case MainResourceRole::stage_splash_background:
		return make_pi(640U, 96U, spec.expected_bytes, 3U, 2U);
	case MainResourceRole::character_stage_shot:
		return make_pi(
			320U, 16U, spec.expected_bytes, pi_color(spec.key), 3U
		);
	case MainResourceRole::character_stage_extra_shot:
		return make_pi(
			320U,
			extra_height(spec.key),
			spec.expected_bytes,
			pi_color(spec.key),
			4U
		);
	case MainResourceRole::stage_enemy_center:
		return make_pi(640U, 48U, spec.expected_bytes, 8U, 5U);
	case MainResourceRole::character_stage_enemy_group:
		return make_pi(640U, 192U, spec.expected_bytes, 9U, 6U);
	default:
		break;
	}
	return {};
}

std::vector<LoadedMainResource> resources_for(const MainStoryConfig &config)
{
	const auto p1 = *th03::app::main_player_selection_from_paletted(
		config.resident.paletted_character[0U]
	);
	const auto p2 = *th03::app::main_player_selection_from_paletted(
		config.resident.paletted_character[1U]
	);
	const auto keys = *th03::app::mainl_stage_resource_keys({
		.player_one = p1,
		.player_two = p2,
		.game_mode = config.resident.game_mode,
		.story_stage = config.resident.story_stage,
	});
	std::vector<LoadedMainResource> result;
	result.reserve(keys.size());
	for(const auto key : keys) {
		const auto spec = *th03::app::main_resource_spec(key);
		auto bytes = std::make_shared<const std::vector<std::byte>>(
			bytes_for(spec)
		);
		result.push_back({.spec = spec, .bytes = std::move(bytes)});
	}
	return result;
}

th03::GameInit vs_resident(
	ClassicOpSelectionMode mode,
	std::int8_t player_one = 2,
	std::int8_t player_two = 17
) noexcept
{
	th03::GameInit resident{};
	resident.resident_rand = -1234567;
	resident.random_seed = std::bit_cast<std::uint32_t>(
		resident.resident_rand
	);
	resident.rank = 3U;
	resident.key_mode = 2U;
	resident.story_lives = 77U;
	resident.remaining_credits = 2U;
	resident.skill = 203U;
	resident.fast_op_animation = 1U;
	resident.story_opponents.fill(13);
	resident.paletted_character = {player_one, player_two};
	switch(mode) {
	case ClassicOpSelectionMode::vs_1p_cpu:
		resident.game_mode = th03::game_mode::vs_1p_cpu;
		resident.is_cpu = {0U, 1U};
		break;
	case ClassicOpSelectionMode::vs_1p_2p:
		resident.game_mode = th03::game_mode::vs_1p_2p;
		resident.is_cpu = {0U, 0U};
		break;
	case ClassicOpSelectionMode::vs_cpu_cpu:
		resident.game_mode = th03::game_mode::vs_cpu_cpu;
		resident.is_cpu = {1U, 1U};
		break;
	case ClassicOpSelectionMode::story:
		resident.game_mode = th03::game_mode::story;
		resident.is_cpu = {0U, 1U};
		break;
	}
	return resident;
}

ClassicOpSelectionCompletion vs_completion(
	ClassicOpSelectionMode mode,
	th03::GameInit resident
) noexcept
{
	return ClassicOpSelectionTestAccess::make_completion(
		ClassicOpSelectionOutcome::selected,
		mode,
		std::move(resident)
	);
}

std::vector<LoadedMainResource> resources_for(
	const ClassicOpSelectionCompletion &completion
)
{
	const auto &resident = completion.resident();
	const auto p1 = *th03::app::main_player_selection_from_paletted(
		resident.paletted_character[0U]
	);
	const auto p2 = *th03::app::main_player_selection_from_paletted(
		resident.paletted_character[1U]
	);
	const auto keys = *th03::app::mainl_stage_resource_keys({
		.player_one = p1,
		.player_two = p2,
		.game_mode = resident.game_mode,
		.story_stage = resident.story_stage,
	});
	std::vector<LoadedMainResource> result;
	result.reserve(keys.size());
	for(const auto key : keys) {
		const auto spec = *th03::app::main_resource_spec(key);
		result.push_back({
			.spec = spec,
			.bytes = std::make_shared<const std::vector<std::byte>>(
				bytes_for(spec)
			),
		});
	}
	return result;
}

th03::GameInit demo_resident(std::uint8_t number) noexcept
{
	constexpr std::array<std::array<std::int8_t, 2U>, 4U> pairings{{
		{{3, 1}}, {{5, 13}}, {{7, 11}}, {{9, 5}},
	}};
	constexpr std::array<std::int32_t, 4U> random{{
		600, 1000, 3200, 500,
	}};
	th03::GameInit resident{};
	resident.game_mode = th03::game_mode::demo;
	resident.demo_number = number;
	resident.is_cpu = {1U, 1U};
	resident.paletted_character = pairings[number - 1U];
	resident.resident_rand = random[number - 1U];
	resident.random_seed = std::bit_cast<std::uint32_t>(
		resident.resident_rand
	);
	resident.rank = 1U;
	resident.key_mode = 2U;
	resident.remaining_credits = 2U;
	resident.story_lives = 77U;
	resident.skill = 203U;
	resident.story_opponents.fill(13);
	return resident;
}

ClassicOpCompletion demo_completion(th03::GameInit resident) noexcept
{
	return ClassicOpSceneTestAccess::completion(
		ClassicOpRoute::demo, std::move(resident)
	);
}

std::vector<LoadedMainResource> resources_for(
	const ClassicOpCompletion &completion
)
{
	const auto &resident = completion.resident();
	const auto p1 = *th03::app::main_player_selection_from_paletted(
		resident.paletted_character[0U]
	);
	const auto p2 = *th03::app::main_player_selection_from_paletted(
		resident.paletted_character[1U]
	);
	const auto keys = *th03::app::mainl_stage_resource_keys({
		.player_one = p1,
		.player_two = p2,
		.game_mode = resident.game_mode,
		.story_stage = resident.story_stage,
	});
	std::vector<LoadedMainResource> result;
	result.reserve(keys.size());
	for(const auto key : keys) {
		const auto spec = *th03::app::main_resource_spec(key);
		result.push_back({
			.spec = spec,
			.bytes = std::make_shared<const std::vector<std::byte>>(
				bytes_for(spec)
			),
		});
	}
	return result;
}

void expect_pixel(
	const th03::present::Raster &raster,
	Page page,
	Point point,
	std::uint8_t expected,
	std::string_view message
)
{
	expect(raster.pixel(page, point) == expected, message);
}

void advance_to_waiting(th03::app::ClassicMainlStageTransition &stage)
{
	for(std::size_t tick = 0U; tick < 18U; ++tick) {
		expect(stage.advance(false).has_value(), "black-in tick advances");
	}
	expect(stage.phase() == ClassicMainlStagePhase::waiting,
		"black-in reaches waiting after exact fade boundaries");
}

void test_visible_stage_and_exact_timing()
{
	const auto config = story_config(0U, 2);
	auto resources = resources_for(config);
	SyntheticFont font;
	auto stage = ClassicMainlStageTestAccess::create_with_synthetic_resources(
		config, resources, text_assets(), font
	);
	expect(stage.has_value(), "synthetic Story stage creates");
	if(!stage) {
		return;
	}
	resources.clear();
	expect((*stage)->phase() == ClassicMainlStagePhase::black_in &&
		(*stage)->raster().palette_tone() == 0,
		"created stage publishes first black-in boundary");
	expect_pixel((*stage)->raster(), Page::zero, {10, 10}, 3U,
		"top background overlays the full-page base");
	expect_pixel((*stage)->raster(), Page::zero, {10, 200}, 2U,
		"full-page base remains below the top strip");
	expect_pixel((*stage)->raster(), Page::zero, {96, 96}, 9U,
		"P1 second-palette portrait selects cel one left half");
	expect_pixel((*stage)->raster(), Page::zero, {352, 96}, 2U,
		"P2 first-palette portrait is horizontally flipped");
	expect_pixel((*stage)->raster(), Page::zero, {384, 46}, 2U,
		"Story stage zero draws ST cel one");
	expect_pixel((*stage)->raster(), Page::zero, {80, 292}, 15U,
		"P1 title draws white bold graphics text");
	expect_pixel((*stage)->raster(), Page::zero, {128, 308}, 15U,
		"P1 name draws in its distinct source slot");
	expect_pixel((*stage)->raster(), Page::zero, {336, 292}, 15U,
		"P2 title draws in its distinct source slot");
	expect_pixel((*stage)->raster(), Page::zero, {384, 308}, 15U,
		"P2 name draws in its distinct source slot");

	for(std::size_t tick = 0U; tick < 17U; ++tick) {
		const auto result = (*stage)->advance(false);
		expect(result.has_value(), "black-in tone step succeeds");
		expect((*stage)->raster().palette_tone() ==
			static_cast<std::int16_t>(tick * 6U),
			"black-in tone schedule is exact");
	}
	expect((*stage)->advance(false).has_value() &&
		(*stage)->phase() == ClassicMainlStagePhase::waiting &&
		(*stage)->raster().palette_tone() == 100,
		"black-in final tone stages the hidden sheet");
	expect_pixel((*stage)->raster(), Page::zero, {128, 308}, 15U,
		"fully revealed stage retains all prepared character text");
	std::vector<th03::present::ComposedPixel> composed_pixels(
		th03::present::graphics_page_pixels
	);
	const auto composed = th03::present::compose_source_surface(
		std::span<
			th03::present::ComposedPixel,
			th03::present::graphics_page_pixels
		>(composed_pixels.data(), composed_pixels.size()),
		(*stage)->raster(),
		(*stage)->tram(),
		font
	);
	expect(composed && std::ranges::all_of(
		std::array{
			Point{80, 292}, Point{128, 308},
			Point{336, 292}, Point{384, 308},
		},
		[&composed_pixels](Point point) {
			const auto &pixel = composed_pixels[
				(static_cast<std::size_t>(point.y) *
					th03::present::graphics_width) +
				static_cast<std::size_t>(point.x)
			];
			return pixel.layer ==
				th03::present::ComposedPixelLayer::graphics &&
				pixel.value == 15U;
		}
	), "shown-surface composition publishes all four pre-round strings");
	const auto stop = (*stage)->take_audio_command();
	const auto load = (*stage)->take_audio_command();
	const auto effects = (*stage)->take_audio_command();
	expect(stop == ClassicAudioCommand{
		.type = ClassicAudioCommandType::stop_song,
	} && load == ClassicAudioCommand{
		.type = ClassicAudioCommandType::load_song,
		.cue = ClassicMusicCue::gameplay,
		.parameter = static_cast<std::uint16_t>(ClassicGameplayMusic::stage_00),
	} && effects == ClassicAudioCommand{
		.type = ClassicAudioCommandType::load_effects,
	} && !(*stage)->take_audio_command(),
		"MAINL stages Stop, opponent song Load, and effect-bank Load exactly");
	auto decision = story_config(6U, 2);
	const auto decision_song = th03::app::classic_main_gameplay_song(
		decision.resident
	);
	expect(decision_song == ClassicAudioCommand{
		.type = ClassicAudioCommandType::load_song,
		.cue = ClassicMusicCue::gameplay,
		.parameter = static_cast<std::uint16_t>(
			ClassicGameplayMusic::decision
		),
	}, "Story stage six selects DEC.M instead of the opponent theme");
	expect((*stage)->raster().access_page() == Page::one,
		"hidden sheet leaves page one accessed");
	expect_pixel((*stage)->raster(), Page::one, {0, 200}, 5U,
		"P1 paletted NN16 occupies the first hidden slot");
	expect_pixel((*stage)->raster(), Page::one, {320, 200}, 4U,
		"P2 paletted NN16 occupies the second hidden slot");
	expect_pixel((*stage)->raster(), Page::one, {0, 280}, 8U,
		"EN2 occupies its hidden slot");
	expect_pixel((*stage)->raster(), Page::one, {0, 304}, 9U,
		"opponent enemy group occupies its hidden slot");

	for(std::uint8_t count = 1U; count <= 32U; ++count) {
		const auto tick = (*stage)->advance(true);
		expect(tick && tick->phase == ClassicMainlStagePhase::waiting &&
			!tick->input_accepted && tick->wait_count == count,
			"input is ignored through wait count 32");
	}
	const auto accepted = (*stage)->advance(true);
	expect(accepted && accepted->phase == ClassicMainlStagePhase::white_out &&
		accepted->input_accepted && accepted->wait_count == 33U,
		"input is accepted at wait count 33");

	for(std::size_t tick = 0U; tick < 17U; ++tick) {
		expect((*stage)->advance(false).has_value(), "white-out step succeeds");
		expect((*stage)->raster().palette_tone() ==
			static_cast<std::int16_t>(100U + (tick * 6U)),
			"white-out tone schedule is exact");
	}
	expect((*stage)->advance(false).has_value() &&
		(*stage)->phase() == ClassicMainlStagePhase::white_in &&
		(*stage)->raster().palette_tone() == 200,
		"white-out clears page zero and enters white-in");
	expect_pixel((*stage)->raster(), Page::zero, {80, 292}, 0U,
		"white-out clears the complete visible page");
	for(std::size_t tick = 0U; tick < 17U; ++tick) {
		expect((*stage)->advance(false).has_value(), "white-in step succeeds");
		expect((*stage)->raster().palette_tone() ==
			static_cast<std::int16_t>(200U - (tick * 6U)),
			"white-in tone schedule is exact");
	}
	expect((*stage)->advance(false).has_value() &&
		(*stage)->phase() == ClassicMainlStagePhase::ready &&
		(*stage)->raster().palette_tone() == 100,
		"white-in completes the MAIN handoff at tone 100");
	expect((*stage)->raster().access_page() == Page::zero &&
		(*stage)->raster().shown_page() == Page::zero &&
		(*stage)->raster().raster_mode() ==
			th03::present::RasterMode::lines_400,
		"ready handoff has exact page and raster roles");
	const auto handoff = ClassicMainlStageTestAccess::handoff(**stage);
	expect(handoff != nullptr, "ready stage publishes one private handoff");
	expect(!(*stage)->advance(false) &&
		(*stage)->phase() == ClassicMainlStagePhase::ready,
		"ready stage rejects extra visual ticks without mutation");
}

void test_exact_gameplay_song_selection()
{
	constexpr std::array expected{
		ClassicGameplayMusic::stage_00,
		ClassicGameplayMusic::stage_01,
		ClassicGameplayMusic::stage_02,
		ClassicGameplayMusic::stage_03,
		ClassicGameplayMusic::stage_04,
		ClassicGameplayMusic::stage_05,
		ClassicGameplayMusic::stage_06,
		ClassicGameplayMusic::stage_07,
		ClassicGameplayMusic::stage_08,
	};
	for(std::size_t character = 0U; character < expected.size(); ++character) {
		for(const std::int8_t palette_offset : {std::int8_t{1}, std::int8_t{2}}) {
			th03::GameInit resident{};
			resident.game_mode = th03::game_mode::vs_1p_2p;
			resident.paletted_character[1U] = static_cast<std::int8_t>(
				(character * 2U) + static_cast<std::size_t>(palette_offset)
			);
			const auto song = th03::app::classic_main_gameplay_song(resident);
			expect(song == ClassicAudioCommand{
				.type = ClassicAudioCommandType::load_song,
				.cue = ClassicMusicCue::gameplay,
				.parameter = static_cast<std::uint16_t>(expected[character]),
			}, "both palettes map every opponent to its exact gameplay song");
		}
	}

	th03::GameInit invalid{};
	invalid.game_mode = th03::game_mode::vs_1p_2p;
	invalid.paletted_character[1U] = 0;
	expect(!th03::app::classic_main_gameplay_song(invalid),
		"invalid opponent identity cannot publish a gameplay-song command");
}

void test_timeout_and_resource_atomicity()
{
	const auto config = story_config(8U, 17);
	SyntheticFont font;
	auto resources = resources_for(config);
	auto wrong_order = resources;
	std::swap(wrong_order[0U], wrong_order[1U]);
	auto rejected = ClassicMainlStageTestAccess::create_with_synthetic_resources(
		config, wrong_order, text_assets(), font
	);
	expect(!rejected && rejected.error().code ==
		ClassicMainlStageErrorCode::resource_set_mismatch,
		"stage rejects a reordered resource transaction");

	auto missing = resources;
	missing.back().bytes.reset();
	rejected = ClassicMainlStageTestAccess::create_with_synthetic_resources(
		config, missing, text_assets(), font
	);
	expect(!rejected && rejected.error().code ==
		ClassicMainlStageErrorCode::resource_bytes_missing,
		"stage rejects a late missing resource without publication");

	auto public_rejected = th03::app::ClassicMainlStageTransition::create(
		config, resources, text_assets(), font
	);
	expect(!public_rejected && public_rejected.error().code ==
		ClassicMainlStageErrorCode::resource_integrity_failed,
		"production stage path reauthenticates source bytes");

	auto stage = ClassicMainlStageTestAccess::create_with_synthetic_resources(
		config, resources, text_assets(), font
	);
	expect(stage.has_value(), "timeout fixture creates");
	if(!stage) {
		return;
	}
	advance_to_waiting(**stage);
	for(std::uint8_t count = 1U; count <= 96U; ++count) {
		const auto tick = (*stage)->advance(false);
		expect(tick && tick->phase == ClassicMainlStagePhase::waiting,
			"wait remains active through count 96");
	}
	const auto timeout = (*stage)->advance(false);
	expect(timeout && timeout->phase == ClassicMainlStagePhase::white_out &&
		!timeout->input_accepted && timeout->wait_count == 97U,
		"wait automatically exits after count 96");
}

void test_winner_preload_and_adoption()
{
	const auto config = story_config(3U, 8);
	auto resources = resources_for(config);
	SyntheticFont font;
	auto stage = ClassicMainlStageTestAccess::create_with_synthetic_resources(
		config, resources, text_assets(), font
	);
	expect(stage.has_value(), "winner-preload stage fixture creates");
	if(!stage) {
		return;
	}

	th03::present::Raster winner;
	expect(winner.set_raster_mode(
		th03::present::RasterMode::lines_400
	) && winner.set_shown_page(Page::zero) &&
		winner.set_access_page(Page::zero) && winner.clear(
			{0, 0, th03::present::graphics_width,
			 th03::present::graphics_height},
			7U
		) && winner.set_access_page(Page::one) && winner.clear(
			{0, 0, th03::present::graphics_width,
			 th03::present::graphics_height},
			6U
		), "winner preload target initializes");
	for(std::uint8_t index = 0U; index < 16U; ++index) {
		expect(winner.set_pending_palette_entry_rgb8(index, {
			.r = static_cast<std::uint8_t>(index),
			.g = 0U,
			.b = 0U,
		}), "winner palette seed initializes");
	}
	winner.set_palette_tone(50);
	winner.apply_pending_palette();
	const auto applied_before = winner.applied_palette();

	expect(ClassicMainlStageTestAccess::preload_splash_into(
		**stage, winner
	), "prepared stage loads into the winner hidden page");
	expect(winner.shown_page() == Page::zero &&
		winner.access_page() == Page::one &&
		winner.palette_tone() == 50 &&
		winner.applied_palette() == applied_before,
		"preload preserves winner shown page and applied palette");
	expect_pixel(winner, Page::zero, {10, 10}, 7U,
		"preload preserves the visible winner page");
	expect_pixel(winner, Page::one, {10, 10}, 3U,
		"preload copies the stage background to hidden page one");
	expect_pixel(winner, Page::one, {10, 200}, 2U,
		"preload copies the lower stage base to hidden page one");
	expect(winner.pending_palette() == (*stage)->raster().pending_palette(),
		"preload copies the unapplied stage palette");

	expect(ClassicMainlStageTestAccess::adopt_preloaded_raster(
		**stage, std::move(winner)
	), "stage adopts the completed winner Raster");
	expect((*stage)->phase() == ClassicMainlStagePhase::black_in &&
		(*stage)->raster().shown_page() == Page::zero &&
		(*stage)->raster().access_page() == Page::zero &&
		(*stage)->raster().palette_tone() == 0,
		"adoption begins the existing stage black-in without a tick");
	expect_pixel((*stage)->raster(), Page::zero, {10, 10}, 3U,
		"adoption publishes the preloaded splash on page zero");
	expect_pixel((*stage)->raster(), Page::zero, {96, 96}, 9U,
		"adoption draws the prepared portrait over the splash");
	expect_pixel((*stage)->raster(), Page::zero, {80, 292}, 15U,
		"adoption draws the prepared graphics-page text");

	expect((*stage)->advance(false).has_value() &&
		(*stage)->raster().palette_tone() == 0,
		"first post-winner stage VSync keeps the source tone-zero step");
	th03::present::Raster second_target;
	expect(!ClassicMainlStageTestAccess::preload_splash_into(
		**stage, second_target
	), "preload cannot be repeated after stage presentation starts");
}

void test_all_story_scenarios_construct()
{
	SyntheticFont font;
	for(std::int8_t player = 1; player <= 18; ++player) {
		for(std::uint8_t stage_number = 0U; stage_number <= 8U; ++stage_number) {
			const auto config = story_config(stage_number, player);
			auto resources = resources_for(config);
			auto stage = ClassicMainlStageTestAccess::create_with_synthetic_resources(
				config, resources, text_assets(), font
			);
			expect(stage.has_value(),
				"every paletted-player and Story-stage splash constructs");
		}
	}
}

void test_all_classic_vs_modes_from_selection()
{
	SyntheticFont font;
	for(const auto mode : {
		ClassicOpSelectionMode::vs_1p_cpu,
		ClassicOpSelectionMode::vs_1p_2p,
		ClassicOpSelectionMode::vs_cpu_cpu,
	}) {
		auto completion = vs_completion(mode, vs_resident(mode));
		auto resources = resources_for(completion);
		const auto p1 = *th03::app::main_player_selection_from_paletted(2);
		const auto p2 = *th03::app::main_player_selection_from_paletted(17);
		const std::vector<MainResourceKey> exact_non_story_keys{
			{.role = MainResourceRole::mainl_gaiji_font},
			{
				.role = MainResourceRole::character_stage_portrait,
				.character = p1.character,
			},
			{
				.role = MainResourceRole::character_stage_portrait,
				.character = p2.character,
			},
			{.role = MainResourceRole::stage_splash_base},
			{
				.role = MainResourceRole::stage_splash_background,
				.stage_background = th03::app::MainlStageBackground::non_story,
			},
			{
				.role = MainResourceRole::character_stage_shot,
				.character = p1.character,
				.palette = p1.palette,
			},
			{
				.role = MainResourceRole::character_stage_extra_shot,
				.character = p1.character,
				.palette = p1.palette,
			},
			{
				.role = MainResourceRole::character_stage_shot,
				.character = p2.character,
				.palette = p2.palette,
			},
			{
				.role = MainResourceRole::character_stage_extra_shot,
				.character = p2.character,
				.palette = p2.palette,
			},
			{.role = MainResourceRole::stage_enemy_center},
			{
				.role = MainResourceRole::character_stage_enemy_group,
				.character = p2.character,
			},
		};
		const auto exact_closure = std::equal(
			resources.begin(), resources.end(),
			exact_non_story_keys.begin(), exact_non_story_keys.end(),
			[](const auto &resource, const auto &key) {
				return resource.spec.key == key;
			}
		);
		expect(resources.size() == exact_non_story_keys.size() && exact_closure,
			"Classic VS uses the exact STNX5/no-stage-number MAINL closure");
		auto stage = ClassicMainlStageTestAccess::
			create_vs_with_synthetic_resources(
				completion,
				{.component_provider = &kotohime_fade},
				resources,
				text_assets(),
				font
			);
		expect(stage.has_value(),
			"every Classic VS mode constructs from typed selection provenance");
		if(!stage) {
			continue;
		}
		expect((*stage)->phase() == ClassicMainlStagePhase::black_in &&
			(*stage)->raster().shown_page() == Page::zero &&
			(*stage)->raster().access_page() == Page::zero,
			"Classic VS starts the common non-Story splash boundary");
		expect_pixel((*stage)->raster(), Page::zero, {10, 10}, 3U,
			"Classic VS uses the non-Story top background");
		expect_pixel((*stage)->raster(), Page::zero, {384, 46}, 3U,
			"Classic VS omits the Story stage-number cel");
		for(std::size_t tick = 0U; tick < 18U; ++tick) {
			expect((*stage)->advance(false).has_value(),
				"Classic VS black-in reaches hidden staging");
		}
		expect_pixel((*stage)->raster(), Page::one, {0, 200}, 5U,
			"Classic VS stages the selected P1 paletted NN16");
		expect_pixel((*stage)->raster(), Page::one, {320, 200}, 4U,
			"Classic VS stages the selected P2 paletted NN16");
	}
}

void test_classic_vs_completion_validation()
{
	SyntheticFont font;
	auto valid_completion = vs_completion(
		ClassicOpSelectionMode::vs_1p_cpu,
		vs_resident(ClassicOpSelectionMode::vs_1p_cpu)
	);
	auto valid_resources = resources_for(valid_completion);
	const auto expect_invalid = [&](th03::GameInit resident,
		ClassicOpSelectionMode mode,
		std::string_view message) {
		auto completion = vs_completion(mode, std::move(resident));
		auto rejected = ClassicMainlStageTestAccess::
			create_vs_with_synthetic_resources(
				completion,
				{.component_provider = &kotohime_fade},
				valid_resources,
				text_assets(),
				font
			);
		expect(!rejected && rejected.error().code ==
			ClassicMainlStageErrorCode::invalid_config, message);
	};

	auto resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	resident.game_mode = th03::game_mode::vs_1p_2p;
	expect_invalid(resident, ClassicOpSelectionMode::vs_1p_cpu,
		"VS mode and resident game mode cannot be paired independently");
	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	resident.is_cpu = {1U, 0U};
	expect_invalid(resident, ClassicOpSelectionMode::vs_1p_cpu,
		"impossible CPU ownership is rejected");
	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	resident.last_score[1U][7U] = 1U;
	expect_invalid(resident, ClassicOpSelectionMode::vs_1p_cpu,
		"OP-to-MAINL VS entry requires both cleared score rows");
	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	resident.paletted_character[1U] = resident.paletted_character[0U];
	expect_invalid(resident, ClassicOpSelectionMode::vs_1p_cpu,
		"selection collision cannot cross the typed completion boundary");
	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	resident.runtime_snapshot.emplace();
	expect_invalid(resident, ClassicOpSelectionMode::vs_1p_cpu,
		"snapshot-backed state cannot enter the fresh Classic VS path");
	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	resident.compact_runtime_snapshot.emplace();
	expect_invalid(resident, ClassicOpSelectionMode::vs_1p_cpu,
		"compact snapshot state cannot enter the fresh Classic VS path");
	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	resident.round_start_snapshot.emplace();
	expect_invalid(resident, ClassicOpSelectionMode::vs_1p_cpu,
		"round snapshot state cannot enter the fresh Classic VS path");
	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	resident.practice_rules.emplace();
	expect_invalid(resident, ClassicOpSelectionMode::vs_1p_cpu,
		"Practice state cannot enter the fresh Classic VS path");
	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	resident.random_seed ^= 1U;
	expect_invalid(resident, ClassicOpSelectionMode::vs_1p_cpu,
		"Classic VS seed transfer must match the selector resident RNG");
	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	resident.global_frame = 1U;
	expect_invalid(resident, ClassicOpSelectionMode::vs_1p_cpu,
		"Classic VS begins a fresh MAIN process at global frame zero");
	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	resident.paletted_character[0U] = 0;
	expect_invalid(resident, ClassicOpSelectionMode::vs_1p_cpu,
		"Classic VS rejects an invalid raw paletted character");
	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	resident.rank = 4U;
	expect_invalid(resident, ClassicOpSelectionMode::vs_1p_cpu,
		"Classic VS rejects rank outside the source domain");
	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	resident.key_mode = 3U;
	expect_invalid(resident, ClassicOpSelectionMode::vs_1p_cpu,
		"Classic VS rejects key mode outside the source domain");
	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	resident.fast_op_animation = 2U;
	expect_invalid(resident, ClassicOpSelectionMode::vs_1p_cpu,
		"Classic VS rejects an impossible fast-OP flag");
	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	resident.remaining_credits = 4U;
	expect_invalid(resident, ClassicOpSelectionMode::vs_1p_cpu,
		"Classic VS rejects credits outside the source resident domain");
	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	resident.autofire_mask = 4U;
	expect_invalid(resident, ClassicOpSelectionMode::vs_1p_cpu,
		"Classic VS rejects unsupported autofire bits");

	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu);
	auto canceled = ClassicOpSelectionTestAccess::make_completion(
		ClassicOpSelectionOutcome::canceled,
		ClassicOpSelectionMode::vs_1p_cpu,
		resident
	);
	auto canceled_stage = ClassicMainlStageTestAccess::
		create_vs_with_synthetic_resources(
			canceled,
			{.component_provider = &kotohime_fade},
			valid_resources,
			text_assets(),
			font
		);
	expect(!canceled_stage && canceled_stage.error().code ==
		ClassicMainlStageErrorCode::invalid_config,
		"a canceled selector completion cannot authorize MAINL");

	resident = vs_resident(ClassicOpSelectionMode::vs_1p_cpu, 9, 1);
	auto kotohime = vs_completion(
		ClassicOpSelectionMode::vs_1p_cpu, resident
	);
	auto kotohime_resources = resources_for(kotohime);
	auto default_kotohime = ClassicMainlStageTestAccess::
		create_vs_with_synthetic_resources(
			kotohime,
			{},
			kotohime_resources,
			text_assets(),
			font
		);
	expect(default_kotohime.has_value(),
		"Kotohime VS entry accepts the deterministic default normalization");

	auto same_base = vs_completion(
		ClassicOpSelectionMode::vs_1p_2p,
		vs_resident(ClassicOpSelectionMode::vs_1p_2p, 1, 2)
	);
	auto same_base_resources = resources_for(same_base);
	auto same_base_stage = ClassicMainlStageTestAccess::
		create_vs_with_synthetic_resources(
			same_base,
			{.component_provider = &kotohime_fade},
			same_base_resources,
			text_assets(),
			font
		);
	expect(same_base_stage.has_value(),
		"same-character opposite-palette VS is a legal raw selection pair");

	resident = vs_resident(ClassicOpSelectionMode::story);
	auto story_completion = vs_completion(
		ClassicOpSelectionMode::story, std::move(resident)
	);
	auto rejected = ClassicMainlStageTestAccess::
		create_vs_with_synthetic_resources(
			story_completion,
			{.component_provider = &kotohime_fade},
			valid_resources,
			text_assets(),
			font
		);
	expect(!rejected && rejected.error().code ==
		ClassicMainlStageErrorCode::invalid_config,
		"Story completion cannot enter the Classic VS factory");
}

void test_all_automatic_demo_routes()
{
	SyntheticFont font;
	for(std::uint8_t number = 1U; number <= 4U; ++number) {
		auto completion = demo_completion(demo_resident(number));
		auto resources = resources_for(completion);
		auto stage = ClassicMainlStageTestAccess::
			create_demo_with_synthetic_resources(
				completion,
				{.component_provider = &kotohime_fade},
				resources,
				text_assets(),
				font,
				{
					.input_protocol = th03::InputProtocol::live,
					.three_digit_combos = true,
				}
			);
		expect(stage.has_value(),
			"each stock automatic-Demo recipe constructs its MAINL splash");
		if(!stage) {
			continue;
		}
		expect(ClassicMainlStageTestAccess::entry_game_options(**stage).
			three_digit_combos,
			"New Classic automatic Demo retains its combo-display policy");
		expect_pixel((*stage)->raster(), Page::zero, {10, 10}, 3U,
			"automatic Demo uses the non-Story STNX5 background");
		for(std::size_t tick = 0U;
			tick < 18U + 33U + 18U + 18U;
			++tick) {
			const bool dismiss = tick >= 18U;
			auto advanced = (*stage)->advance(dismiss);
			expect(advanced.has_value(),
				"automatic Demo stage reaches its typed MAIN handoff");
			if(!advanced || advanced->handoff_ready) {
				break;
			}
		}
		expect((*stage)->phase() == ClassicMainlStagePhase::ready &&
			ClassicMainlStageTestAccess::handoff(**stage) != nullptr,
			"automatic Demo stage publishes a ready Raster capability");
	}

	auto invalid_resident = demo_resident(1U);
	invalid_resident.paletted_character = {1, 3};
	auto invalid_completion = demo_completion(std::move(invalid_resident));
	auto valid_completion = demo_completion(demo_resident(1U));
	auto resources = resources_for(valid_completion);
	auto rejected = ClassicMainlStageTestAccess::
		create_demo_with_synthetic_resources(
			invalid_completion,
			{.component_provider = &kotohime_fade},
			resources,
			text_assets(),
			font
		);
	expect(!rejected && rejected.error().code ==
		ClassicMainlStageErrorCode::invalid_config,
		"a forged automatic-Demo pairing cannot authorize MAINL");

	auto wrong_random_resident = demo_resident(1U);
	++wrong_random_resident.resident_rand;
	auto wrong_random = demo_completion(std::move(wrong_random_resident));
	rejected = ClassicMainlStageTestAccess::
		create_demo_with_synthetic_resources(
			wrong_random,
			{.component_provider = &kotohime_fade},
			resources,
			text_assets(),
			font
		);
	expect(!rejected && rejected.error().code ==
		ClassicMainlStageErrorCode::invalid_config,
		"a forged automatic-Demo resident seed cannot authorize MAINL");

	auto resumed_resident = demo_resident(1U);
	resumed_resident.global_frame = 1U;
	auto resumed = demo_completion(std::move(resumed_resident));
	rejected = ClassicMainlStageTestAccess::
		create_demo_with_synthetic_resources(
			resumed,
			{.component_provider = &kotohime_fade},
			resources,
			text_assets(),
			font
		);
	expect(!rejected && rejected.error().code ==
		ClassicMainlStageErrorCode::invalid_config,
		"a resumed automatic-Demo process cannot authorize fresh MAINL");

	auto kotohime = demo_completion(demo_resident(4U));
	auto kotohime_resources = resources_for(kotohime);
	auto default_kotohime = ClassicMainlStageTestAccess::
		create_demo_with_synthetic_resources(
			kotohime, {}, kotohime_resources, text_assets(), font
		);
	expect(default_kotohime.has_value(),
		"Demo 4 accepts the deterministic Kotohime normalization");
}

} // namespace

int main()
{
	test_visible_stage_and_exact_timing();
	test_exact_gameplay_song_selection();
	test_timeout_and_resource_atomicity();
	test_winner_preload_and_adoption();
	test_all_story_scenarios_construct();
	test_all_classic_vs_modes_from_selection();
	test_classic_vs_completion_validation();
	test_all_automatic_demo_routes();

	if(failures != 0U) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << checks << " checks passed\n";
	return 0;
}
