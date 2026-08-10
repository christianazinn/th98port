#include "th03/app/main_process.hpp"
#include "th03/app/main_direct_vs_runner.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/app/authentic_text.hpp"
#include "th03/assets/decode.hpp"
#include "th03/present/main_static.hpp"

#include "../src/app/main_compiled_content_internal.hpp"
#include "../src/app/main_process_internal.hpp"

namespace {

using th03::app::LoadedMainResource;
using th03::app::MainCharacter;
using th03::app::MainDirectVsConfig;
using th03::app::MainPaletteVariant;
using th03::app::MainPlayerSelection;
using th03::app::MainProcess;
using th03::app::MainProcessErrorCode;
using th03::app::MainProcessTestAccess;
using th03::app::MainResourceKey;
using th03::app::MainResourceRole;
using th03::app::ResourceBytes;

constexpr std::size_t stock_file_bytes = 130882U;
constexpr std::size_t load_image_file_offset = 0x1860U;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::fprintf(
			stderr,
			"FAIL: %.*s\n",
			static_cast<int>(message.size()),
			message.data()
		);
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

void put_u16(
	std::vector<std::byte> &bytes,
	std::size_t offset,
	std::uint16_t value
)
{
	bytes[offset] = static_cast<std::byte>(static_cast<std::uint8_t>(value));
	bytes[offset + 1U] = static_cast<std::byte>(
		static_cast<std::uint8_t>(value >> 8U)
	);
}

void put_load_byte(
	std::vector<std::byte> &bytes,
	std::size_t load_offset,
	std::uint8_t value
)
{
	bytes[load_image_file_offset + load_offset] = static_cast<std::byte>(value);
}

class BitWriter {
public:
	void bit(std::uint8_t value)
	{
		if((bit_count_ % 8U) == 0U) {
			bytes_.push_back(std::byte{0U});
		}
		const auto shift = static_cast<unsigned>(7U - (bit_count_ % 8U));
		auto byte = std::to_integer<std::uint8_t>(bytes_.back());
		byte = static_cast<std::uint8_t>(
			byte | static_cast<std::uint8_t>((value & 1U) << shift)
		);
		bytes_.back() = static_cast<std::byte>(byte);
		++bit_count_;
	}

	void bits(std::uint32_t value, unsigned count)
	{
		for(unsigned remaining = count; remaining > 0U; --remaining) {
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
	std::size_t bit_count_ = 0U;
};

class ColorEncoder {
public:
	ColorEncoder()
	{
		for(std::size_t previous = 0U; previous < tables_.size(); ++previous) {
			for(std::size_t index = 0U; index < tables_[previous].size(); ++index) {
				tables_[previous][index] = static_cast<std::uint8_t>(
					(previous + index + 1U) & 0x0FU
				);
			}
		}
	}

	void color(BitWriter &bits, std::uint8_t previous, std::uint8_t color)
	{
		auto &table = tables_[previous & 0x0FU];
		const auto found = std::find(table.begin(), table.end(), color);
		const auto table_index = static_cast<std::uint8_t>(
			std::distance(table.begin(), found)
		);
		write_code(bits, static_cast<std::uint8_t>(table_index ^ 0x0FU));
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

	std::array<std::array<std::uint8_t, 16U>, 16U> tables_{};
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
	unsigned suffix_width = 0U;
	for(auto remaining = length; remaining > 1U; remaining >>= 1U) {
		++suffix_width;
	}
	for(unsigned index = 0U; index < suffix_width; ++index) {
		bits.bit(1U);
	}
	bits.bit(0U);
	const auto base = std::size_t{1U} << suffix_width;
	bits.bits(static_cast<std::uint32_t>(length - base), suffix_width);
}

std::vector<std::byte> single_color_stream(
	std::size_t pixel_count,
	std::uint8_t color
)
{
	BitWriter bits;
	ColorEncoder colors;
	colors.color(bits, 0U, color);
	colors.color(bits, color, color);
	write_position(bits, 1U);
	write_length(bits, (pixel_count / 2U) - 1U);
	return bits.bytes();
}

std::vector<std::byte> make_pi(
	std::uint16_t width,
	std::uint16_t height,
	std::size_t exact_size,
	bool inline_palette,
	std::uint8_t fill_color = 10U
)
{
	std::vector<std::byte> bytes;
	append_u8(bytes, static_cast<std::uint8_t>('P'));
	append_u8(bytes, static_cast<std::uint8_t>('i'));
	append_u8(bytes, static_cast<std::uint8_t>('T'));
	append_u8(bytes, static_cast<std::uint8_t>('3'));
	append_u8(bytes, 0x1AU);
	append_u8(bytes, 0U);
	append_u8(bytes, inline_palette ? 0U : 0x80U);
	append_u8(bytes, 0U);
	append_u8(bytes, 0U);
	append_u8(bytes, 4U);
	for(const auto value : {1U, 2U, 3U, 4U}) {
		append_u8(bytes, static_cast<std::uint8_t>(value));
	}
	append_be16(bytes, 0U);
	append_be16(bytes, width);
	append_be16(bytes, height);
	if(inline_palette) {
		for(std::uint8_t color = 0U; color < 16U; ++color) {
			append_u8(bytes, static_cast<std::uint8_t>(color << 4U));
			append_u8(bytes, static_cast<std::uint8_t>((15U - color) << 4U));
			append_u8(bytes, static_cast<std::uint8_t>((color ^ 5U) << 4U));
		}
	}
	const auto compressed = single_color_stream(
		static_cast<std::size_t>(width) * height,
		fill_color
	);
	bytes.insert(bytes.end(), compressed.begin(), compressed.end());
	expect(bytes.size() <= exact_size, "synthetic PI fits canonical byte count");
	bytes.resize(exact_size, std::byte{0U});
	return bytes;
}

std::vector<std::byte> make_bfnt(
	std::uint16_t width,
	std::uint16_t height,
	std::uint16_t glyph_count,
	bool monochrome,
	std::uint8_t color_seed = 0U
)
{
	std::vector<std::byte> bytes;
	for(const auto value : {'B', 'F', 'N', 'T'}) {
		append_u8(bytes, static_cast<std::uint8_t>(value));
	}
	append_u8(bytes, 0x1AU);
	append_u8(bytes, monochrome ? 0U : 0x83U);
	append_u8(bytes, 0x16U);
	append_u8(bytes, 0U);
	append_le16(bytes, width);
	append_le16(bytes, height);
	append_le16(bytes, 0U);
	append_le16(bytes, static_cast<std::uint16_t>(glyph_count - 1U));
	for(const auto value : std::array<char, 8U>{
		'S', 'Y', 'N', 'T', 'H', '0', '3', '\0',
	}) {
		append_u8(bytes, static_cast<std::uint8_t>(value));
	}
	append_le32(bytes, 0x44332211U);
	append_le16(bytes, 0U);
	append_le16(bytes, 0U);
	if(!monochrome) {
		for(std::uint8_t color = 0U; color < 16U; ++color) {
			append_u8(bytes, static_cast<std::uint8_t>(color << 4U));
			append_u8(bytes, static_cast<std::uint8_t>((15U - color) << 4U));
			append_u8(bytes, static_cast<std::uint8_t>((color ^ 5U) << 4U));
		}
	}
	const auto pixel_count = static_cast<std::size_t>(width) * height;
	for(std::size_t glyph = 0U; glyph < glyph_count; ++glyph) {
		if(monochrome) {
			for(std::size_t index = 0U; index < (pixel_count / 8U); ++index) {
				append_u8(bytes, static_cast<std::uint8_t>(
					(glyph * 17U) + (index * 3U) + 1U
				));
			}
			continue;
		}
		for(std::size_t pixel = 0U; pixel < pixel_count; pixel += 2U) {
			const auto left = static_cast<std::uint8_t>(
				1U + ((glyph + pixel + color_seed) % 15U)
			);
			const auto right = static_cast<std::uint8_t>(
				1U + ((glyph + pixel + 1U + color_seed) % 15U)
			);
			append_u8(bytes, static_cast<std::uint8_t>((left << 4U) | right));
		}
	}
	return bytes;
}

std::uint8_t paletted_ordinal(const MainResourceKey &key)
{
	const auto character = static_cast<std::uint8_t>(key.character);
	const auto palette = static_cast<std::uint8_t>(key.palette);
	return static_cast<std::uint8_t>(
		((character - 1U) * 2U) + (palette - 1U)
	);
}

std::uint8_t pi_sentinel(const MainResourceKey &key)
{
	switch(key.role) {
	case MainResourceRole::stage_enemy_center:
		return 13U;
	case MainResourceRole::character_stage_shot:
		return static_cast<std::uint8_t>(
			1U + (paletted_ordinal(key) % 15U)
		);
	case MainResourceRole::character_stage_extra_shot:
		return static_cast<std::uint8_t>(
			1U + ((paletted_ordinal(key) + 8U) % 15U)
		);
	case MainResourceRole::character_stage_enemy_group:
		return static_cast<std::uint8_t>(
			1U + ((static_cast<std::uint8_t>(key.character) + 10U) % 15U)
		);
	default:
		return 10U;
	}
}

std::uint8_t mrs_sentinel(const MainResourceKey &key)
{
	const auto role_offset = (key.role ==
		MainResourceRole::character_bomb_portrait) ? 0x20U : 0U;
	return static_cast<std::uint8_t>(
		1U + paletted_ordinal(key) + role_offset
	);
}

std::vector<std::byte> make_mrs(const th03::app::MainResourceSpec &spec)
{
	constexpr std::size_t row_bytes = 36U;
	constexpr std::size_t plane_bytes = row_bytes * 184U;
	std::vector<std::byte> bytes(spec.expected_bytes, std::byte{0U});
	const auto sentinel = mrs_sentinel(spec.key);
	for(std::size_t plane = 0U; plane < 5U; ++plane) {
		const auto base = plane * plane_bytes;
		bytes[base] = static_cast<std::byte>(static_cast<std::uint8_t>(
			sentinel + plane
		));
		bytes[base + row_bytes - 1U] = static_cast<std::byte>(
			static_cast<std::uint8_t>(sentinel + plane + 0x40U)
		);
	}
	return bytes;
}

std::uint8_t reverse_bits(std::uint8_t value)
{
	value = static_cast<std::uint8_t>((value >> 4U) | (value << 4U));
	value = static_cast<std::uint8_t>(
		((value & 0xCCU) >> 2U) | ((value & 0x33U) << 2U)
	);
	return static_cast<std::uint8_t>(
		((value & 0xAAU) >> 1U) | ((value & 0x55U) << 1U)
	);
}

std::vector<std::byte> make_enedat()
{
	std::vector<std::byte> bytes;
	bytes.reserve(5072U);
	append_le16(bytes, 5068U);
	append_le16(bytes, 0U);
	append_le16(bytes, 16U);
	for(std::size_t enemy = 0U; enemy < 16U; ++enemy) {
		append_u8(bytes, 255U);
		bytes.insert(bytes.end(), 255U, std::byte{0U});
	}
	append_le16(bytes, 4U);
	for(const auto length : {255U, 255U, 255U, 197U}) {
		append_u8(bytes, static_cast<std::uint8_t>(length));
		bytes.insert(bytes.end(), length, std::byte{0U});
	}
	append_le16(bytes, 0U);
	expect(bytes.size() == 5072U, "synthetic ENEDAT canonical byte count");
	return bytes;
}

std::vector<std::byte> make_compiled_executable()
{
	std::vector<std::byte> bytes(stock_file_bytes);
	put_u16(bytes, 0U, 0x5A4DU);
	put_u16(bytes, 2U, 322U);
	put_u16(bytes, 4U, 256U);
	put_u16(bytes, 6U, 0U);
	put_u16(bytes, 8U, 390U);
	put_u16(bytes, 24U, 28U);

	for(const auto [offset, value] : std::array{
		std::pair{0x1DD7CU, 0x0CU}, std::pair{0x1DD7DU, 0x0DU},
		std::pair{0x1DD7EU, 0x00U}, std::pair{0x1DD7FU, 0x0AU},
		std::pair{0x1DD80U, 0x0BU}, std::pair{0x1DD81U, 0x0EU},
		std::pair{0x1DD82U, 0x0FU}, std::pair{0x1DD83U, 0x00U},
	}) {
		put_load_byte(bytes, offset, static_cast<std::uint8_t>(value));
	}
	for(std::size_t index = 0U; index < 8U; ++index) {
		put_load_byte(bytes, 0x1DD84U + index, 0x20U);
	}
	put_load_byte(bytes, 0x1DD8CU, 0U);

	struct LabelRange {
		std::size_t offset;
		std::size_t payload;
	};
	constexpr std::array labels{
		LabelRange{0x1DE4AU, 26U}, LabelRange{0x1DE65U, 26U},
		LabelRange{0x1DE80U, 26U}, LabelRange{0x1DE9BU, 26U},
		LabelRange{0x1DEB6U, 26U}, LabelRange{0x1DED1U, 26U},
		LabelRange{0x1DEECU, 32U}, LabelRange{0x1DF0DU, 32U},
		LabelRange{0x1DF2EU, 26U},
	};
	for(std::size_t label = 0U; label < labels.size(); ++label) {
		for(std::size_t index = 0U; index < labels[label].payload; ++index) {
			put_load_byte(bytes, labels[label].offset + index,
				static_cast<std::uint8_t>('A' + label));
		}
		put_load_byte(bytes, labels[label].offset + labels[label].payload, 0U);
	}
	for(std::size_t index = 0U;
		index < th03::present::score_font_mask_bytes;
		++index) {
		put_load_byte(bytes, 0x1DDB8U + index,
			static_cast<std::uint8_t>((index * 37U + 11U) & 0xFFU));
	}
	for(std::size_t index = 0U;
		index < th03::present::pellet_mask_bytes;
		++index) {
		put_load_byte(bytes, 0x1DF4AU + index,
			static_cast<std::uint8_t>((index * 29U + 7U) & 0xFFU));
	}
	return bytes;
}

auto make_compiled_content()
{
	auto bytes = make_compiled_executable();
	return th03::app::detail::import_main_compiled_content_for_test(
		bytes,
		th03::app::main_executable_sha256(bytes)
	);
}

std::vector<std::byte> bytes_for(const th03::app::MainResourceSpec &spec)
{
	switch(spec.key.role) {
	case MainResourceRole::enemy_scripts:
		return make_enedat();
	case MainResourceRole::game_font:
		return make_bfnt(16U, 16U, 256U, true);
	case MainResourceRole::hud_lose_sheet:
		return make_bfnt(64U, 32U, 4U, false);
	case MainResourceRole::hud_round_sheet:
		return make_bfnt(32U, 16U, 22U, false);
	case MainResourceRole::hud_result_sheet:
		return make_bfnt(32U, 16U, 16U, false);
	case MainResourceRole::stage_enemy_center:
		return make_pi(
			640U, 48U, spec.expected_bytes, false, pi_sentinel(spec.key)
		);
	case MainResourceRole::character_sheet:
		return make_bfnt(
			32U, 32U, 9U, false, paletted_ordinal(spec.key)
		);
	case MainResourceRole::character_extra_portrait:
	case MainResourceRole::character_bomb_portrait:
		return make_mrs(spec);
	case MainResourceRole::character_stage_shot:
		return make_pi(
			320U, 16U, spec.expected_bytes, false, pi_sentinel(spec.key)
		);
	case MainResourceRole::character_stage_extra_shot:
		return make_pi(
			320U, 128U, spec.expected_bytes, false, pi_sentinel(spec.key)
		);
	case MainResourceRole::character_stage_enemy_group:
		return make_pi(
			640U, 192U, spec.expected_bytes, true, pi_sentinel(spec.key)
		);
	case MainResourceRole::selection_palette:
		break;
	}
	return {};
}

std::vector<LoadedMainResource> make_resources(
	MainPlayerSelection player_one,
	MainPlayerSelection player_two
)
{
	const auto keys = th03::app::direct_vs_resource_keys(player_one, player_two);
	expect(static_cast<bool>(keys), "direct-VS synthetic resource keys");
	std::vector<LoadedMainResource> resources;
	if(!keys) {
		return resources;
	}
	resources.reserve(keys->size());
	for(const auto key : *keys) {
		const auto spec = th03::app::main_resource_spec(key);
		expect(static_cast<bool>(spec), "canonical synthetic resource spec");
		if(!spec) {
			continue;
		}
		auto bytes = bytes_for(*spec);
		expect(bytes.size() == spec->expected_bytes,
			"synthetic resource has canonical byte count");
		resources.push_back({
			.spec = *spec,
			.bytes = std::make_shared<const std::vector<std::byte>>(
				std::move(bytes)
			),
		});
	}
	return resources;
}

LoadedMainResource *find_role(
	std::vector<LoadedMainResource> &resources,
	MainResourceRole role
) noexcept
{
	const auto found = std::find_if(
		resources.begin(), resources.end(), [role](const auto &resource) {
			return resource.spec.key.role == role;
		}
	);
	return (found == resources.end()) ? nullptr : &*found;
}

void replace_bytes(
	LoadedMainResource &resource,
	std::vector<std::byte> bytes
)
{
	resource.bytes = std::make_shared<const std::vector<std::byte>>(
		std::move(bytes)
	);
}

std::vector<std::byte> make_font_rom()
{
	return std::vector<std::byte>(
		th03::app::Pc98FontRomProvider::font_rom_size,
		std::byte{0U}
	);
}

auto create_synthetic(
	const MainDirectVsConfig &config,
	std::span<const LoadedMainResource> resources
)
{
	auto compiled = make_compiled_content();
	expect(static_cast<bool>(compiled), "synthetic MAIN compiled content");
	if(!compiled) {
		return std::expected<std::unique_ptr<MainProcess>, th03::app::MainProcessError>{
			std::unexpected(th03::app::MainProcessError{
				.code = MainProcessErrorCode::presenter_setup_failed,
			})
		};
	}
	const auto font_rom = make_font_rom();
	return MainProcessTestAccess::create_with_synthetic_resources(
		config,
		resources,
		std::move(*compiled),
		font_rom
	);
}

bool same_canonical(
	const th03::CanonicalState &left,
	const th03::CanonicalState &right
) noexcept
{
	return left.route == right.route && left.game_mode == right.game_mode &&
		left.story_stage == right.story_stage &&
		left.round_id == right.round_id && left.winner == right.winner &&
		left.round_speed == right.round_speed &&
		left.global_frame == right.global_frame &&
		left.round_frame == right.round_frame &&
		left.round_or_result_frame == right.round_or_result_frame &&
		left.p1_score == right.p1_score && left.p2_score == right.p2_score &&
		left.resident_rand == right.resident_rand &&
		left.state_hash == right.state_hash;
}

bool presentation_failure(const th03::app::MainProcessError &error) noexcept
{
	return error.code == MainProcessErrorCode::presentation_failed &&
		error.presentation_status != th03::present::MainRoundStatus::ready;
}

void test_raw_bf2_shapes()
{
	const MainPlayerSelection reimu{
		.character = MainCharacter::reimu,
		.palette = MainPaletteVariant::first,
	};
	const auto resources = make_resources(reimu, reimu);
	struct Shape {
		MainResourceRole role;
		std::uint16_t width;
		std::uint16_t height;
		std::size_t glyphs;
		th03::assets::BfntConsumerPolicy policy;
	};
	constexpr std::array shapes{
		Shape{MainResourceRole::game_font, 16U, 16U, 256U,
			th03::assets::BfntConsumerPolicy::gaiji},
		Shape{MainResourceRole::hud_lose_sheet, 64U, 32U, 4U,
			th03::assets::BfntConsumerPolicy::superimpose},
		Shape{MainResourceRole::hud_round_sheet, 32U, 16U, 22U,
			th03::assets::BfntConsumerPolicy::superimpose},
		Shape{MainResourceRole::hud_result_sheet, 32U, 16U, 16U,
			th03::assets::BfntConsumerPolicy::superimpose},
		Shape{MainResourceRole::character_sheet, 32U, 32U, 9U,
			th03::assets::BfntConsumerPolicy::superimpose},
	};
	for(const auto &shape : shapes) {
		const auto resource = std::find_if(
			resources.begin(), resources.end(), [&shape](const auto &candidate) {
				return candidate.spec.key.role == shape.role;
			}
		);
		expect(resource != resources.end(), "raw BF2 fixture resource present");
		if(resource == resources.end()) {
			continue;
		}
		const auto decoded = th03::assets::decode_bfnt(
			*resource->bytes,
			shape.policy
		);
		expect(decoded && decoded->header.pixel_width == shape.width &&
			decoded->header.pixel_height == shape.height &&
			decoded->glyphs.size() == shape.glyphs,
			"raw BF2 fixture retains authentic consumer shape");
	}
}

void test_distinct_selection_first_frame()
{
	MainDirectVsConfig config{};
	config.player_one = {
		.character = MainCharacter::reimu,
		.palette = MainPaletteVariant::first,
	};
	config.player_two = {
		.character = MainCharacter::kana,
		.palette = MainPaletteVariant::second,
	};
	config.seed = 0x12345678U;
	config.rank = 2U;
	auto resources = make_resources(config.player_one, config.player_two);
	expect(resources.size() == 17U,
		"distinct selections require seventeen physical resources");
	auto created = create_synthetic(config, resources);
	expect(static_cast<bool>(created), "distinct-selection process constructs");
	if(!created) {
		return;
	}
	auto &process = **created;
	resources.clear();
	resources.shrink_to_fit();
	const auto observation = process.round_start_observation();
	expect(observation.start.game_mode == th03::game_mode::vs_1p_2p &&
		observation.start.rank == 2U &&
		observation.start.paletted_character ==
			std::array<std::int8_t, 2U>{1, 12},
		"direct-VS configuration reaches the core round start");
	expect(process.presentation_status() == th03::present::MainRoundStatus::ready,
		"constructed presenter is ready");

	const auto p1_key = [&](MainResourceRole role) {
		return MainResourceKey{
			.role = role,
			.character = config.player_one.character,
			.palette = config.player_one.palette,
		};
	};
	const auto p2_key = [&](MainResourceRole role) {
		return MainResourceKey{
			.role = role,
			.character = config.player_two.character,
			.palette = config.player_two.palette,
		};
	};
	const auto pixel_is = [&](th03::present::Point point, std::uint8_t color) {
		return process.raster().pixel(th03::present::Page::zero, point) == color;
	};
	expect(
		pixel_is({0, 200}, pi_sentinel(p1_key(
			MainResourceRole::character_stage_shot
		))) && pixel_is({320, 200}, pi_sentinel(p2_key(
			MainResourceRole::character_stage_shot
		))) && pixel_is({0, 208}, pi_sentinel(p1_key(
			MainResourceRole::character_stage_extra_shot
		))) && pixel_is({320, 208}, pi_sentinel(p2_key(
			MainResourceRole::character_stage_extra_shot
		))) && pixel_is({0, 280}, pi_sentinel({
			.role = MainResourceRole::stage_enemy_center,
		})) && pixel_is({0, 304}, pi_sentinel({
			.role = MainResourceRole::character_stage_enemy_group,
			.character = config.player_two.character,
		})),
		"all six semantic PI roles occupy their exact committed atlas coordinates"
	);
	expect(
		pixel_is({544, 328}, static_cast<std::uint8_t>(
			1U + (paletted_ordinal(p1_key(
				MainResourceRole::character_sheet
			)) % 15U)
		)) && pixel_is({544, 360}, static_cast<std::uint8_t>(
			1U + (paletted_ordinal(p2_key(
				MainResourceRole::character_sheet
			)) % 15U)
		)),
		"selected P1 and P2 character BFNT identities reach their atlas rows"
	);
	for(const auto bomb : {false, true}) {
		for(std::size_t player = 0U; player < 2U; ++player) {
			const auto *portrait = MainProcessTestAccess::portrait(
				process, bomb, player
			);
			const auto role = bomb ?
				MainResourceRole::character_bomb_portrait :
				MainResourceRole::character_extra_portrait;
			const auto key = (player == 0U) ? p1_key(role) : p2_key(role);
			const auto expected = mrs_sentinel(key);
			expect(portrait != nullptr && portrait->cel.alpha &&
				portrait->orientation == (player == 0U ?
					th03::assets::MrsOrientation::as_stored :
					th03::assets::MrsOrientation::horizontally_flipped) &&
				portrait->cel.alpha->front() == (player == 0U ? expected :
					reverse_bits(static_cast<std::uint8_t>(expected + 0x40U))),
				"EX/BM portrait role, player identity, and P2 persistent flip are distinct"
			);
		}
	}

	const auto first = process.step({});
	expect(first && first->status == th03::StepStatus::running,
		"first neutral logical sample runs through core and presenter");
	expect(process.presentation_status() == th03::present::MainRoundStatus::ready,
		"first presented sample remains ready");

	std::vector<th03::present::ComposedPixel> pixels(
		th03::present::graphics_page_pixels
	);
	const auto surface = process.compose_shown_surface(
		std::span<
			th03::present::ComposedPixel,
			th03::present::graphics_page_pixels
		>(pixels.data(), pixels.size())
	);
	expect(surface && surface->pixels.data() == pixels.data() &&
		surface->pixels.size() == pixels.size(),
		"first presented frame composes into caller-owned surface");
	if(surface) {
		expect(surface->graphics_palette.colors[3U] ==
			th03::assets::Rgb4{3U, 12U, 6U},
			"opponent ENEMY PI palette reaches the shown surface");
		expect(std::any_of(pixels.begin(), pixels.end(), [](const auto pixel) {
			return pixel.value != 0U;
		}), "first surface contains presented graphics or text");
	}
	const auto &tram = process.tram();
	expect(tram.visible() && std::any_of(
		tram.cells().begin(), tram.cells().end(), [](const auto cell) {
			return cell.attribute != 0U;
		}), "first frame retains visible populated HUD TRAM");
	expect(tram.gaiji(7U).row_halves[0U] ==
		static_cast<std::uint8_t>((7U * 17U) + 1U),
		"GAMEFT raw glyph bytes are installed into process TRAM");
}

void test_same_selection_shared_raw_construction()
{
	MainDirectVsConfig config{};
	config.player_one = {
		.character = MainCharacter::reimu,
		.palette = MainPaletteVariant::first,
	};
	config.player_two = config.player_one;
	auto resources = make_resources(config.player_one, config.player_two);
	expect(resources.size() == 12U,
		"same selection deduplicates to twelve physical resources");
	const auto mrs = std::find_if(
		resources.begin(), resources.end(), [](const auto &resource) {
			return resource.spec.key.role ==
				MainResourceRole::character_extra_portrait;
		}
	);
	expect(mrs != resources.end(), "same-selection shared MRS resource present");
	const auto raw_mrs = (mrs == resources.end()) ? std::vector<std::byte>{} :
		*mrs->bytes;
	auto created = create_synthetic(config, resources);
	expect(static_cast<bool>(created), "same-selection process constructs");
	if(mrs != resources.end()) {
		expect(*mrs->bytes == raw_mrs,
			"P2 persistent MRS flip mutates only its decoded owned copy");
	}
	resources.clear();
	resources.shrink_to_fit();
	if(created) {
		const auto *p1_extra = MainProcessTestAccess::portrait(
			**created, false, 0U
		);
		const auto *p2_extra = MainProcessTestAccess::portrait(
			**created, false, 1U
		);
		const auto expected = mrs_sentinel({
			.role = MainResourceRole::character_extra_portrait,
			.character = config.player_one.character,
			.palette = config.player_one.palette,
		});
		expect(p1_extra != nullptr && p2_extra != nullptr &&
			p1_extra != p2_extra && p1_extra->cel.alpha && p2_extra->cel.alpha &&
			p1_extra->orientation == th03::assets::MrsOrientation::as_stored &&
			p2_extra->orientation ==
				th03::assets::MrsOrientation::horizontally_flipped &&
			p1_extra->cel.alpha->front() == expected &&
			p2_extra->cel.alpha->front() == reverse_bits(
				static_cast<std::uint8_t>(expected + 0x40U)
			),
			"deduplicated raw MRS produces independent P1 stored and P2 flipped values"
		);
		const auto first = (*created)->step({});
		expect(first && first->status == th03::StepStatus::running,
			"same-selection process owns both semantic decodes after raw release");
	}
}

void test_recorded_pause_deferred_frame()
{
	MainDirectVsConfig config{};
	config.player_two = {
		.character = MainCharacter::kana,
		.palette = MainPaletteVariant::second,
	};
	config.game_options.input_protocol = th03::InputProtocol::recorded_replay;
	auto resources = make_resources(config.player_one, config.player_two);
	auto created = create_synthetic(config, resources);
	expect(static_cast<bool>(created),
		"recorded-pause process constructs with the full presenter");
	if(!created) {
		return;
	}
	resources.clear();
	resources.shrink_to_fit();
	auto &process = **created;
	const auto opened = process.step(
		th03::CoreInput{.system = 0x1000U},
		th03::InputContext::gameplay
	);
	const auto held = process.step(
		th03::CoreInput{.system = 0x1000U},
		th03::InputContext::interstitial
	);
	const auto released = process.step({}, th03::InputContext::interstitial);
	const auto resumed = process.finish_deferred_frame(
		th03::DeferredFrameAction::resume
	);
	expect(opened && opened->status == th03::StepStatus::phase_transition &&
		held && held->status == th03::StepStatus::running &&
		released && released->status == th03::StepStatus::running &&
		resumed && resumed->status == th03::StepStatus::running &&
		process.presentation_status() == th03::present::MainRoundStatus::ready &&
		process.canonical_state().round_frame == 1U,
		"deferred replay-pause suffix resumes through the retained observer"
	);

	const auto reopened = process.step(
		th03::CoreInput{.system = 0x1000U},
		th03::InputContext::gameplay
	);
	const auto exit_release = process.step(
		{}, th03::InputContext::interstitial
	);
	const auto exited = process.finish_deferred_frame(
		th03::DeferredFrameAction::exit_session
	);
	expect(reopened &&
		reopened->status == th03::StepStatus::phase_transition &&
		exit_release && exit_release->status == th03::StepStatus::running &&
		exited && exited->status == th03::StepStatus::terminal &&
		process.presentation_status() == th03::present::MainRoundStatus::ready &&
		process.canonical_state().round_frame == 2U,
		"deferred replay-pause exit publishes its final observed footer before terminal"
	);
}

void test_live_runner_owns_the_actual_process()
{
	MainDirectVsConfig config{};
	auto resources = make_resources(config.player_one, config.player_two);
	auto created = create_synthetic(config, resources);
	expect(static_cast<bool>(created),
		"live process constructs for the concrete runner adapter");
	if(!created) {
		return;
	}
	auto runner = th03::app::MainDirectVsRunner::create(std::move(*created));
	expect(static_cast<bool>(runner),
		"live runner accepts and owns the concrete MainProcess");
	if(runner) {
		const auto consumed = (*runner)->consume({
			.tick_index = 0U,
			.deadline_ns = 10U,
		});
		expect(consumed && consumed->process_advanced &&
			consumed->step_result && consumed->step_result->status ==
				th03::StepStatus::running &&
			(*runner)->process().canonical_state().round_frame == 1U,
			"runner's production operation table advances the retained process once");
	}

	config.game_options.input_protocol = th03::InputProtocol::recorded_replay;
	resources = make_resources(config.player_one, config.player_two);
	auto recorded = create_synthetic(config, resources);
	expect(static_cast<bool>(recorded),
		"recorded process remains available to replay validation");
	if(recorded) {
		auto rejected = th03::app::MainDirectVsRunner::create(
			std::move(*recorded)
		);
		expect(!rejected && rejected.error().code ==
			th03::app::MainDirectVsRunnerErrorCode::unsupported_input_protocol,
			"live direct-play runner rejects the deferred recorded-pause protocol");
	}
}

void test_invalid_runtime_arguments_are_atomic()
{
	MainDirectVsConfig config{};
	auto resources = make_resources(config.player_one, config.player_two);
	auto created = create_synthetic(config, resources);
	expect(static_cast<bool>(created),
		"process constructs for runtime argument validation");
	if(!created) {
		return;
	}
	auto &process = **created;
	const auto canonical_before = process.canonical_state();
	const auto raster_before = process.raster().display_state_hash();
	const auto tram_before = process.tram().visible_state_hash();
	const auto invalid_context = process.step(
		{}, static_cast<th03::InputContext>(0xFFU)
	);
	const auto invalid_action = process.finish_deferred_frame(
		static_cast<th03::DeferredFrameAction>(0xFFU)
	);
	expect(!invalid_context && invalid_context.error().code ==
			MainProcessErrorCode::invalid_runtime_argument &&
		!invalid_action && invalid_action.error().code ==
			MainProcessErrorCode::invalid_runtime_argument,
		"invalid runtime enum domains are rejected by the process boundary"
	);
	expect(same_canonical(process.canonical_state(), canonical_before) &&
		process.raster().display_state_hash() == raster_before &&
		process.tram().visible_state_hash() == tram_before &&
		process.presentation_status() == th03::present::MainRoundStatus::ready,
		"invalid runtime enums preserve core, Raster, TRAM, and presenter state"
	);
}

void test_resource_failures_are_atomic()
{
	MainDirectVsConfig config{};
	config.player_two = {
		.character = MainCharacter::kana,
		.palette = MainPaletteVariant::second,
	};
	auto authenticated_resources = make_resources(
		config.player_one,
		config.player_two
	);
	auto authenticated_compiled = make_compiled_content();
	expect(static_cast<bool>(authenticated_compiled),
		"compiled content for public resource authentication");
	if(authenticated_compiled) {
		const auto font_rom = make_font_rom();
		auto authenticated = MainProcess::create(
			config,
			authenticated_resources,
			std::move(*authenticated_compiled),
			font_rom
		);
		expect(!authenticated && authenticated.error().code ==
			MainProcessErrorCode::resource_integrity_failed &&
			authenticated.error().resource ==
				authenticated_resources.front().spec.key,
			"public factory rejects synthetic bytes that lack canonical fingerprints");
	}

	auto resources = make_resources(config.player_one, config.player_two);
	resources[2U].spec.filename = "SPOOF.BF2";
	auto result = create_synthetic(config, resources);
	expect(!result && result.error().code ==
		MainProcessErrorCode::resource_set_mismatch,
		"spoofed resource metadata publishes no process");

	resources = make_resources(config.player_one, config.player_two);
	resources.pop_back();
	result = create_synthetic(config, resources);
	expect(!result && result.error().code ==
		MainProcessErrorCode::resource_set_mismatch,
		"missing physical resource publishes no process");

	resources = make_resources(config.player_one, config.player_two);
	resources.back() = resources.front();
	result = create_synthetic(config, resources);
	expect(!result && result.error().code ==
		MainProcessErrorCode::resource_set_mismatch,
		"duplicate physical resource publishes no process");

	resources = make_resources(config.player_one, config.player_two);
	const auto missing_key = resources.back().spec.key;
	resources.back().bytes.reset();
	result = create_synthetic(config, resources);
	expect(!result && result.error().code ==
		MainProcessErrorCode::resource_bytes_missing &&
		result.error().resource == missing_key,
		"missing resource ownership publishes no process");

	resources = make_resources(config.player_one, config.player_two);
	const auto late_key = resources.back().spec.key;
	auto corrupt = *resources.back().bytes;
	corrupt[0U] = std::byte{'X'};
	resources.back().bytes =
		std::make_shared<const std::vector<std::byte>>(std::move(corrupt));
	result = create_synthetic(config, resources);
	expect(!result && result.error().code ==
		MainProcessErrorCode::asset_decode_failed &&
		result.error().resource == late_key && result.error().asset_error,
		"late opponent PI decode failure publishes no process");

	resources = make_resources(config.player_one, config.player_two);
	auto compiled = make_compiled_content();
	expect(static_cast<bool>(compiled), "compiled content for FONT.ROM rejection");
	if(compiled) {
		auto short_font = make_font_rom();
		short_font.pop_back();
		result = MainProcessTestAccess::create_with_synthetic_resources(
			config,
			resources,
			std::move(*compiled),
			short_font
		);
		expect(!result && result.error().code ==
			MainProcessErrorCode::font_rom_failed,
			"non-exact FONT.ROM publishes no process");
	}
}

void test_normalized_sheet_bundle_rejections()
{
	MainDirectVsConfig config{};
	config.player_two = {
		.character = MainCharacter::kana,
		.palette = MainPaletteVariant::second,
	};
	struct WrongSheet {
		MainResourceRole role;
		std::uint16_t width;
		std::uint16_t height;
		std::uint16_t glyphs;
	};
	constexpr std::array wrong_sheets{
		WrongSheet{MainResourceRole::hud_lose_sheet, 32U, 16U, 16U},
		WrongSheet{MainResourceRole::hud_round_sheet, 32U, 32U, 11U},
		WrongSheet{MainResourceRole::hud_result_sheet, 64U, 32U, 4U},
	};
	for(const auto wrong : wrong_sheets) {
		auto resources = make_resources(config.player_one, config.player_two);
		auto *resource = find_role(resources, wrong.role);
		expect(resource != nullptr, "normalized wrong-sheet target present");
		if(resource == nullptr) {
			continue;
		}
		auto bytes = make_bfnt(
			wrong.width, wrong.height, wrong.glyphs, false
		);
		expect(bytes.size() == resource->spec.expected_bytes,
			"wrong normalized sheet retains canonical byte count");
		replace_bytes(*resource, std::move(bytes));
		const auto result = create_synthetic(config, resources);
		expect(!result && result.error().code ==
			MainProcessErrorCode::presenter_setup_failed,
			"normalized wrong HUD sheet bundle publishes no process");
	}
}

void test_decode_and_staging_rejections()
{
	MainDirectVsConfig config{};
	config.player_two = {
		.character = MainCharacter::kana,
		.palette = MainPaletteVariant::second,
	};

	auto resources = make_resources(config.player_one, config.player_two);
	auto *resource = find_role(resources, MainResourceRole::enemy_scripts);
	expect(resource != nullptr, "malformed ENEDAT target present");
	if(resource != nullptr) {
		auto bytes = *resource->bytes;
		bytes[0U] = std::byte{0U};
		replace_bytes(*resource, std::move(bytes));
		const auto result = create_synthetic(config, resources);
		expect(!result && result.error().code ==
			MainProcessErrorCode::enedat_decode_failed,
			"malformed canonical-size ENEDAT publishes no process");
	}

	resources = make_resources(config.player_one, config.player_two);
	resource = find_role(resources, MainResourceRole::game_font);
	expect(resource != nullptr, "malformed GAMEFT target present");
	if(resource != nullptr) {
		auto bytes = *resource->bytes;
		bytes[0U] = std::byte{'X'};
		replace_bytes(*resource, std::move(bytes));
		const auto result = create_synthetic(config, resources);
		expect(!result && result.error().code ==
			MainProcessErrorCode::asset_decode_failed &&
			result.error().resource == resource->spec.key,
			"malformed canonical-size GAMEFT publishes no process");
	}

	resources = make_resources(config.player_one, config.player_two);
	resource = find_role(resources, MainResourceRole::character_sheet);
	expect(resource != nullptr, "malformed character BF2 target present");
	if(resource != nullptr) {
		auto bytes = *resource->bytes;
		bytes[0U] = std::byte{'X'};
		replace_bytes(*resource, std::move(bytes));
		const auto result = create_synthetic(config, resources);
		expect(!result && result.error().code ==
			MainProcessErrorCode::asset_decode_failed &&
			result.error().resource == resource->spec.key,
			"malformed canonical-size character BF2 publishes no process");
	}

	resources = make_resources(config.player_one, config.player_two);
	resource = find_role(
		resources,
		MainResourceRole::character_extra_portrait
	);
	expect(resource != nullptr, "wrong-size MRS target present");
	if(resource != nullptr) {
		auto bytes = *resource->bytes;
		bytes.pop_back();
		replace_bytes(*resource, std::move(bytes));
		const auto result = create_synthetic(config, resources);
		expect(!result && result.error().code ==
			MainProcessErrorCode::resource_integrity_failed &&
			result.error().resource == resource->spec.key,
			"MRS size corruption is rejected before its fixed-layout decoder");
	}

	resources = make_resources(config.player_one, config.player_two);
	resource = find_role(
		resources,
		MainResourceRole::character_stage_enemy_group
	);
	expect(resource != nullptr, "opponent PI palette target present");
	if(resource != nullptr) {
		replace_bytes(*resource, make_pi(
			640U, 192U, resource->spec.expected_bytes, false
		));
		const auto result = create_synthetic(config, resources);
		expect(!result && result.error().code ==
			MainProcessErrorCode::pi_palette_missing &&
			result.error().resource == resource->spec.key,
			"opponent PI without inline palette publishes no process");
	}

	resources = make_resources(config.player_one, config.player_two);
	resource = find_role(resources, MainResourceRole::stage_enemy_center);
	expect(resource != nullptr, "wrong-shape PI staging target present");
	if(resource != nullptr) {
		replace_bytes(*resource, make_pi(
			320U, 48U, resource->spec.expected_bytes, false
		));
		const auto result = create_synthetic(config, resources);
		expect(!result && result.error().code ==
			MainProcessErrorCode::pi_staging_failed,
			"decoded PI with the wrong staging geometry publishes no process");
	}
}

void test_sticky_presentation_preflight()
{
	MainDirectVsConfig config{};
	config.player_two = {
		.character = MainCharacter::kana,
		.palette = MainPaletteVariant::second,
	};
	auto resources = make_resources(config.player_one, config.player_two);
	auto created = create_synthetic(config, resources);
	expect(static_cast<bool>(created),
		"process constructs for sticky-presentation preflight");
	if(!created) {
		return;
	}
	auto &process = **created;
	const auto canonical_before = process.canonical_state();
	const auto raster_before = process.raster().display_state_hash();
	const auto tram_before = process.tram().visible_state_hash();
	MainProcessTestAccess::fail_presentation(process);
	const auto failure_status = process.presentation_status();
	expect(failure_status != th03::present::MainRoundStatus::ready &&
		same_canonical(process.canonical_state(), canonical_before) &&
		process.raster().display_state_hash() == raster_before &&
		process.tram().visible_state_hash() == tram_before,
		"impossible footer latches failure without core/Raster/TRAM mutation");

	const auto stepped = process.step({});
	const auto deferred = process.finish_deferred_frame(
		th03::DeferredFrameAction::resume
	);
	const auto repeated = process.start_next_round();
	const auto finished = process.finish_main_process();
	const auto transitioned = process.session_transition();
	expect(!stepped && presentation_failure(stepped.error()) &&
		!deferred && presentation_failure(deferred.error()) &&
		!repeated && presentation_failure(repeated.error()) &&
		!finished && presentation_failure(finished.error()) &&
		!transitioned && presentation_failure(transitioned.error()),
		"every process lifecycle entry preflights the sticky presentation failure");
	expect(process.presentation_status() == failure_status &&
		same_canonical(process.canonical_state(), canonical_before) &&
		process.raster().display_state_hash() == raster_before &&
		process.tram().visible_state_hash() == tram_before,
		"failed lifecycle calls preserve core, Raster, TRAM, and sticky status");
}

bool kotohime_red(
	void *,
	const th03::BombFrameStep &,
	std::uint8_t,
	std::uint8_t &red_out
) noexcept
{
	red_out = 0U;
	return true;
}

void test_invalid_config_and_kotohime_policy()
{
	MainDirectVsConfig invalid{};
	invalid.rank = 4U;
	auto result = create_synthetic(invalid, {});
	expect(!result && result.error().code == MainProcessErrorCode::invalid_config,
		"rank outside the direct-VS domain is rejected before publication");

	invalid = {};
	invalid.player_one.character = MainCharacter::none;
	result = create_synthetic(invalid, {});
	expect(!result && result.error().code == MainProcessErrorCode::invalid_config,
		"invalid character selection is rejected before publication");

	invalid = {};
	invalid.player_one.palette = static_cast<MainPaletteVariant>(3U);
	result = create_synthetic(invalid, {});
	expect(!result && result.error().code == MainProcessErrorCode::invalid_config,
		"invalid palette selection is rejected before publication");

	invalid = {};
	invalid.key_mode = 3U;
	result = create_synthetic(invalid, {});
	expect(!result && result.error().code == MainProcessErrorCode::invalid_config,
		"key mode outside the source domain is rejected before publication");

	invalid = {};
	invalid.game_options.input_protocol = static_cast<th03::InputProtocol>(0xFFU);
	result = create_synthetic(invalid, {});
	expect(!result && result.error().code == MainProcessErrorCode::invalid_config,
		"invalid input protocol is rejected before publication");

	MainDirectVsConfig kotohime{};
	kotohime.player_one = {
		.character = MainCharacter::kotohime,
		.palette = MainPaletteVariant::first,
	};
	kotohime.player_two = {
		.character = MainCharacter::reimu,
		.palette = MainPaletteVariant::second,
	};
	auto resources = make_resources(kotohime.player_one, kotohime.player_two);
	result = create_synthetic(kotohime, resources);
	expect(!result && result.error().code == MainProcessErrorCode::invalid_config,
		"Kotohime requires an explicit bomb-fade compatibility policy");

	kotohime.kotohime_compatibility.red_provider = kotohime_red;
	result = create_synthetic(kotohime, resources);
	expect(static_cast<bool>(result),
		"Kotohime constructs when the explicit compatibility policy is present");
}

std::optional<std::vector<std::byte>> read_file(
	const std::filesystem::path &path
)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if(!file) {
		return std::nullopt;
	}
	const auto end = file.tellg();
	if(end < 0) {
		return std::nullopt;
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(end));
	file.seekg(0, std::ios::beg);
	if(!bytes.empty() && !file.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	)) {
		return std::nullopt;
	}
	return bytes;
}

bool run_local_authentic_gate(
	const std::filesystem::path &content_root,
	const std::filesystem::path &main_executable_path,
	const std::filesystem::path &font_rom_path
)
{
	const auto main_executable = read_file(main_executable_path);
	const auto font_rom = read_file(font_rom_path);
	if(!main_executable || !font_rom) {
		std::fprintf(stderr, "local gate input read failed\n");
		return false;
	}
	th03::app::MainResourceLoader loader(content_root);
	std::vector<th03::present::ComposedPixel> surface_pixels(
		th03::present::graphics_page_pixels
	);
	const auto surface_span = std::span<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	>(surface_pixels.data(), surface_pixels.size());
	for(std::uint8_t player_one = 0U; player_one < 18U; ++player_one) {
		for(std::uint8_t player_two = 0U; player_two < 18U; ++player_two) {
			const auto selection = [](std::uint8_t index) {
				return MainPlayerSelection{
					.character = static_cast<MainCharacter>(1U + (index / 2U)),
					.palette = static_cast<MainPaletteVariant>(
						1U + (index % 2U)
					),
				};
			};
			MainDirectVsConfig config{};
			config.player_one = selection(player_one);
			config.player_two = selection(player_two);
			config.kotohime_compatibility.red_provider = kotohime_red;
			auto resources = loader.load_direct_vs(
				config.player_one,
				config.player_two
			);
			if(!resources) {
				std::fprintf(
					stderr,
					"local gate load failed p1=%u p2=%u code=%u\n",
					static_cast<unsigned>(player_one),
					static_cast<unsigned>(player_two),
					static_cast<unsigned>(resources.error().code)
				);
				return false;
			}
			auto compiled = th03::app::import_main_compiled_content(
				*main_executable
			);
			if(!compiled) {
				std::fprintf(stderr, "local gate MAIN authentication failed\n");
				return false;
			}
			auto process = MainProcess::create(
				config,
				*resources,
				std::move(*compiled),
				*font_rom
			);
			if(!process) {
				std::fprintf(
					stderr,
					"local gate process failed p1=%u p2=%u code=%u resource=%u\n",
					static_cast<unsigned>(player_one),
					static_cast<unsigned>(player_two),
					static_cast<unsigned>(process.error().code),
					process.error().resource ? static_cast<unsigned>(
						process.error().resource->role
					) : 255U
				);
				return false;
			}
			resources->clear();
			resources->shrink_to_fit();
			const auto stepped = (*process)->step({});
			const auto surface = (*process)->compose_shown_surface(surface_span);
			if(!stepped || stepped->status != th03::StepStatus::running ||
				!surface || (*process)->presentation_status() !=
					th03::present::MainRoundStatus::ready ||
				(*process)->round_start_observation().start.paletted_character !=
					std::array<std::int8_t, 2U>{
						static_cast<std::int8_t>(player_one + 1U),
						static_cast<std::int8_t>(player_two + 1U),
					}) {
				std::fprintf(
					stderr,
					"local gate runtime failed p1=%u p2=%u step=%u surface=%u status=%u\n",
					static_cast<unsigned>(player_one),
					static_cast<unsigned>(player_two),
					stepped ? static_cast<unsigned>(stepped->status) : 255U,
					static_cast<unsigned>(static_cast<bool>(surface)),
					static_cast<unsigned>((*process)->presentation_status())
				);
				return false;
			}
		}
	}
	return true;
}

} // namespace

int main(int argc, char **argv)
{
	test_raw_bf2_shapes();
	test_distinct_selection_first_frame();
	test_same_selection_shared_raw_construction();
	test_recorded_pause_deferred_frame();
	test_live_runner_owns_the_actual_process();
	test_invalid_runtime_arguments_are_atomic();
	test_resource_failures_are_atomic();
	test_normalized_sheet_bundle_rejections();
	test_decode_and_staging_rejections();
	test_sticky_presentation_preflight();
	test_invalid_config_and_kotohime_policy();
	if(argc == 4) {
		expect(run_local_authentic_gate(argv[1], argv[2], argv[3]),
			"optional authenticated 18x18 direct-VS process gate");
	} else if(argc != 1) {
		expect(false,
			"usage requires content root, MAIN executable, and FONT.ROM together");
	}

	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return 1;
	}
	std::printf("all %zu app MAIN process checks passed\n", checks);
	return 0;
}
