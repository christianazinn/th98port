#include <algorithm>
#include <array>
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
#include "th03/present/text.hpp"

#include "classic_mainl_stage_internal.hpp"

namespace {

using th03::app::ClassicMainlStageErrorCode;
using th03::app::ClassicMainlStagePhase;
using th03::app::ClassicMainlStageTestAccess;
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
	config.kotohime_compatibility.red_provider = &kotohime_fade;
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

} // namespace

int main()
{
	test_visible_stage_and_exact_timing();
	test_timeout_and_resource_atomicity();
	test_all_story_scenarios_construct();

	if(failures != 0U) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << checks << " checks passed\n";
	return 0;
}
