#include "th03/app/main_process.hpp"
#include "th03/app/main_direct_vs_runner.hpp"

#include <algorithm>
#include <array>
#include <bit>
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
#include "../src/app/classic_op_scene_internal.hpp"
#include "../src/app/classic_op_selection_internal.hpp"
#include "../src/app/main_process_internal.hpp"

namespace {

using th03::app::LoadedMainResource;
using th03::app::ClassicMainlStagePhase;
using th03::app::ClassicMainlStageTestAccess;
using th03::app::ClassicOpSelectionMode;
using th03::app::ClassicOpSelectionOutcome;
using th03::app::ClassicOpSelectionTestAccess;
using th03::app::ClassicOpRoute;
using th03::app::ClassicOpSceneTestAccess;
using th03::app::MainCharacter;
using th03::app::MainDirectVsConfig;
using th03::app::MainPaletteVariant;
using th03::app::MainPlayerSelection;
using th03::app::MainProcess;
using th03::app::MainProcessClassicVsAccess;
using th03::app::MainProcessDemoAccess;
using th03::app::MainProcessErrorCode;
using th03::app::MainProcessStoryAccess;
using th03::app::MainProcessTestAccess;
using th03::app::MainResourceKey;
using th03::app::MainResourceRole;
using th03::app::MainStoryConfig;
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

bool kotohime_red(
	void *,
	const th03::BombFrameStep &,
	std::uint8_t,
	std::uint8_t &red_out
) noexcept;

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

std::uint16_t stage_extra_height(const MainResourceKey &key)
{
	return (key.character == MainCharacter::reimu ||
		key.character == MainCharacter::kana ||
		key.character == MainCharacter::rikako) ? 144U : 128U;
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
			320U,
			stage_extra_height(spec.key),
			spec.expected_bytes,
			false,
			pi_sentinel(spec.key)
		);
	case MainResourceRole::character_stage_enemy_group:
		return make_pi(
			640U, 192U, spec.expected_bytes, true, pi_sentinel(spec.key)
		);
	case MainResourceRole::mainl_gaiji_font:
		return make_bfnt(16U, 16U, 256U, true);
	case MainResourceRole::character_stage_portrait:
		return make_cdg(192U, 192U, 2U, true);
	case MainResourceRole::stage_number_sheet:
		return make_cdg(32U, 34U, 10U, false);
	case MainResourceRole::stage_splash_base:
		return make_pi(640U, 400U, spec.expected_bytes, true, 2U);
	case MainResourceRole::stage_splash_background:
		return make_pi(640U, 96U, spec.expected_bytes, true, 3U);
	case MainResourceRole::op_title_long_background:
	case MainResourceRole::op_menu_background:
	case MainResourceRole::op_window_sheet:
	case MainResourceRole::op_gaiji_font:
	case MainResourceRole::op_menu_music:
	case MainResourceRole::audio_effect_bank:
	case MainResourceRole::op_music_room_background:
	case MainResourceRole::op_music_room_comments:
	case MainResourceRole::op_music_room_track:
	case MainResourceRole::selection_palette:
	case MainResourceRole::character_selection_portrait:
	case MainResourceRole::selection_extra_icons:
	case MainResourceRole::selection_story_portrait:
	case MainResourceRole::selection_stats_sheet:
	case MainResourceRole::selection_extra_background:
	case MainResourceRole::selection_name_sheet:
	case MainResourceRole::selection_music:
	case MainResourceRole::win_logo_palette_pre:
	case MainResourceRole::win_logo_fade_cels:
	case MainResourceRole::win_logo_final:
	case MainResourceRole::win_logo_palette_post:
	case MainResourceRole::win_music:
	case MainResourceRole::character_win_portrait:
	case MainResourceRole::character_win_message:
	case MainResourceRole::continue_background:
	case MainResourceRole::continue_choice_cels:
	case MainResourceRole::gameover_background:
	case MainResourceRole::gameover_music:
	case MainResourceRole::cutscene_script:
	case MainResourceRole::cutscene_picture:
	case MainResourceRole::cutscene_music:
	case MainResourceRole::staff_background_palette:
	case MainResourceRole::staff_cdg:
	case MainResourceRole::staff_music:
	case MainResourceRole::registration_background:
	case MainResourceRole::registration_rank:
	case MainResourceRole::registration_normal_glyphs:
	case MainResourceRole::registration_highlight_glyphs:
	case MainResourceRole::registration_music:
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

th03::app::MainlStageTextAssets mainl_stage_text_assets()
{
	th03::app::MainlStageTextAssets result{};
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

std::vector<LoadedMainResource> make_mainl_stage_resources(
	const th03::GameInit &init
)
{
	const auto player_one = th03::app::main_player_selection_from_paletted(
		init.paletted_character[0U]
	);
	const auto player_two = th03::app::main_player_selection_from_paletted(
		init.paletted_character[1U]
	);
	expect(player_one && player_two,
		"Classic VS stage fixture selections are valid");
	if(!player_one || !player_two) {
		return {};
	}
	const auto keys = th03::app::mainl_stage_resource_keys({
		.player_one = *player_one,
		.player_two = *player_two,
		.game_mode = init.game_mode,
		.story_stage = init.story_stage,
	});
	expect(keys.has_value(), "Classic VS stage resource keys resolve");
	if(!keys) {
		return {};
	}
	std::vector<LoadedMainResource> resources;
	resources.reserve(keys->size());
	for(const auto key : *keys) {
		const auto spec = th03::app::main_resource_spec(key);
		expect(spec.has_value(), "Classic VS stage resource spec resolves");
		if(!spec) {
			continue;
		}
		auto bytes = bytes_for(*spec);
		if(key.role == MainResourceRole::stage_enemy_center) {
			bytes = make_pi(
				640U, 48U, spec->expected_bytes, true, pi_sentinel(key)
			);
		} else if(key.role == MainResourceRole::character_stage_shot) {
			bytes = make_pi(
				320U, 16U, spec->expected_bytes, true, pi_sentinel(key)
			);
		} else if(key.role ==
			MainResourceRole::character_stage_extra_shot) {
			bytes = make_pi(
				320U,
				stage_extra_height(key),
				spec->expected_bytes,
				true,
				pi_sentinel(key)
			);
		}
		expect(bytes.size() == spec->expected_bytes,
			"Classic VS stage asset has its canonical byte count");
		resources.push_back({
			.spec = *spec,
			.bytes = std::make_shared<const std::vector<std::byte>>(
				std::move(bytes)
			),
		});
	}
	return resources;
}

bool advance_mainl_stage_to_ready(
	th03::app::ClassicMainlStageTransition &stage
)
{
	for(std::size_t tick = 0U; tick < 18U; ++tick) {
		if(!stage.advance(false)) {
			return false;
		}
	}
	for(std::size_t tick = 0U; tick < 33U; ++tick) {
		if(!stage.advance(true)) {
			return false;
		}
	}
	for(std::size_t tick = 0U; tick < 36U; ++tick) {
		if(!stage.advance(false)) {
			return false;
		}
	}
	return stage.phase() == ClassicMainlStagePhase::ready &&
		ClassicMainlStageTestAccess::handoff(stage) != nullptr;
}

std::vector<LoadedMainResource> select_resources(
	std::span<const LoadedMainResource> resources,
	std::span<const MainResourceKey> keys
)
{
	std::vector<LoadedMainResource> selected;
	selected.reserve(keys.size());
	for(const auto key : keys) {
		const auto found = std::find_if(
			resources.begin(), resources.end(), [key](const auto &resource) {
				return resource.spec.key == key;
			}
		);
		expect(found != resources.end(),
			"selected synthetic resource exists in the direct batch");
		if(found != resources.end()) {
			selected.push_back(*found);
		}
	}
	return selected;
}

MainStoryConfig story_config(
	std::uint8_t stage,
	std::int8_t player_one,
	std::uint8_t remaining_credits
) noexcept;

std::unique_ptr<th03::app::ClassicMainlStageTransition>
make_ready_story_transition(
	const MainStoryConfig &config,
	std::span<const LoadedMainResource> resources
)
{
	const auto player_one = th03::app::main_player_selection_from_paletted(
		config.resident.paletted_character[0U]
	);
	const auto player_two = th03::app::main_player_selection_from_paletted(
		config.resident.paletted_character[1U]
	);
	expect(player_one && player_two, "Story handoff selections are valid");
	if(!player_one || !player_two) {
		return {};
	}
	const auto find = [resources](MainResourceKey key) {
		const auto found = std::find_if(
			resources.begin(), resources.end(), [key](const auto &resource) {
				return resource.spec.key == key;
			}
		);
		return (found == resources.end()) ? nullptr : &*found;
	};
	const std::array selections{*player_one, *player_two};
	std::array<th03::assets::PiImage, 2U> shots{};
	std::array<th03::assets::PiImage, 2U> extra_shots{};
	for(std::size_t player = 0U; player < selections.size(); ++player) {
		const auto *shot = find({
			.role = MainResourceRole::character_stage_shot,
			.character = selections[player].character,
			.palette = selections[player].palette,
		});
		const auto *extra = find({
			.role = MainResourceRole::character_stage_extra_shot,
			.character = selections[player].character,
			.palette = selections[player].palette,
		});
		expect(shot != nullptr && extra != nullptr,
			"Story handoff player PI resources exist");
		if(shot == nullptr || extra == nullptr) {
			return {};
		}
		auto decoded_shot = th03::assets::decode_pi(*shot->bytes);
		auto decoded_extra = th03::assets::decode_pi(*extra->bytes);
		expect(decoded_shot && decoded_extra,
			"Story handoff player PIs decode");
		if(!decoded_shot || !decoded_extra) {
			return {};
		}
		shots[player] = std::move(*decoded_shot);
		extra_shots[player] = std::move(*decoded_extra);
	}
	const auto *center_resource = find({
		.role = MainResourceRole::stage_enemy_center,
	});
	const auto *opponent_resource = find({
		.role = MainResourceRole::character_stage_enemy_group,
		.character = player_two->character,
	});
	expect(center_resource != nullptr && opponent_resource != nullptr,
		"Story handoff enemy PI resources exist");
	if(center_resource == nullptr || opponent_resource == nullptr) {
		return {};
	}
	auto center = th03::assets::decode_pi(*center_resource->bytes);
	auto opponent = th03::assets::decode_pi(*opponent_resource->bytes);
	expect(center && opponent && opponent->palette,
		"Story handoff enemy PIs and palette decode");
	if(!center || !opponent || !opponent->palette) {
		return {};
	}
	th03::present::Raster raster{};
	if(!th03::present::main_prepare_sprite16_pi_source_sheet(
		raster,
		{
			.player_shots = {{&shots[0U], &shots[1U]}},
			.player_extra_shots = {{&extra_shots[0U], &extra_shots[1U]}},
			.enemy_center = &*center,
			.opponent_enemies = &*opponent,
		}
	)) {
		expect(false, "Story handoff six-PI sheet stages");
		return {};
	}
	for(std::size_t index = 0U; index < opponent->palette->colors.size(); ++index) {
		if(!raster.set_pending_palette_entry(
			static_cast<std::uint8_t>(index),
			opponent->palette->colors[index]
		)) {
			expect(false, "Story handoff enemy palette installs");
			return {};
		}
	}
	raster.set_palette_tone(100);
	raster.apply_pending_palette();
	expect(raster.set_access_page(th03::present::Page::zero) &&
		raster.set_shown_page(th03::present::Page::zero),
		"Story handoff publishes source page roles");
	return ClassicMainlStageTestAccess::create_ready_with_handoff(
		config,
		std::move(raster)
	);
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
	expect(!process.take_audio_command(),
		"developer Direct-VS remains outside the Classic MAINL audio lifecycle");
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

void test_live_bomb_first_frame()
{
	for(std::uint8_t character = 1U; character <= 9U; ++character) {
		for(std::uint8_t player = 0U; player < 2U; ++player) {
			MainDirectVsConfig config{};
			config.player_one = {
				.character = static_cast<MainCharacter>(character),
				.palette = MainPaletteVariant::first,
			};
			config.player_two = {
				.character = static_cast<MainCharacter>(character),
				.palette = MainPaletteVariant::second,
			};
			config.kotohime_compatibility.red_provider = kotohime_red;
			auto resources = make_resources(config.player_one, config.player_two);
			auto created = create_synthetic(config, resources);
			expect(static_cast<bool>(created),
				"live Bomb regression process constructs");
			if(!created) {
				continue;
			}
			const auto input = (player == 0U) ?
				th03::CoreInput{.p1 = 0x10U} :
				th03::CoreInput{.p2 = 0x10U};
			const auto preparing = (*created)->step(input);
			const auto active = preparing ? (*created)->step({}) :
				std::expected<th03::StepResult, th03::app::MainProcessError>{
					std::unexpected(preparing.error())
				};
			if(!active) {
				std::fprintf(
					stderr,
					"Bomb regression character=%u player=%u code=%u "
					"presentation=%u bombs=%u\n",
					static_cast<unsigned>(character),
					static_cast<unsigned>(player),
					static_cast<unsigned>(active.error().code),
					static_cast<unsigned>(active.error().presentation_status),
					static_cast<unsigned>(
						MainProcessTestAccess::bombs_status(**created)
					)
				);
			}
			expect(preparing && active &&
				preparing->status == th03::StepStatus::running &&
				active->status == th03::StepStatus::running &&
				(*created)->presentation_status() ==
					th03::present::MainRoundStatus::ready,
				"live Bomb frame 0 and active frame 1 preserve the presenter");
		}
	}
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

void test_story_factory_transaction_and_lifetime()
{
	const auto config = story_config(0U, 2, 3U);
	expect(th03::app::valid_main_story_config(config),
		"Story process fixture is source-valid");
	const auto player_one = *th03::app::main_player_selection_from_paletted(
		config.resident.paletted_character[0U]
	);
	const auto player_two = *th03::app::main_player_selection_from_paletted(
		config.resident.paletted_character[1U]
	);
	auto resources = make_resources(player_one, player_two);
	auto transition = make_ready_story_transition(config, resources);
	expect(transition != nullptr &&
		transition->phase() == ClassicMainlStagePhase::ready &&
		ClassicMainlStageTestAccess::handoff(*transition) != nullptr,
		"Story process fixture owns one ready stage capability");
	if(!transition) {
		return;
	}
	const auto dynamic_keys = th03::app::main_private_resource_keys(
		player_one, player_two
	);
	expect(dynamic_keys && dynamic_keys->size() == 6U,
		"Story process requests only per-MAIN dynamic resources");
	if(!dynamic_keys) {
		return;
	}
	auto dynamic_resources = select_resources(resources, *dynamic_keys);
	auto compiled = make_compiled_content();
	expect(static_cast<bool>(compiled),
		"Story process static content has synthetic MAIN data");
	if(!compiled) {
		return;
	}
	auto font_rom = make_font_rom();
	auto static_content =
		MainProcessTestAccess::prepare_static_with_synthetic_resources(
			resources,
			std::move(*compiled),
			font_rom
		);
	expect(static_cast<bool>(static_content),
		"Story process campaign-static content prepares once");
	if(!static_content) {
		return;
	}
	const auto handoff_hash = transition->raster().display_state_hash();
	auto missing = dynamic_resources;
	missing.pop_back();
	auto rejected = MainProcessStoryAccess::create_story(
		config,
		*transition,
		missing,
		*static_content,
		false
	);
	expect(!rejected && rejected.error().code ==
		MainProcessErrorCode::resource_set_mismatch,
		"Story process rejects a missing late dynamic owner");
	expect(transition->phase() == ClassicMainlStagePhase::ready &&
		ClassicMainlStageTestAccess::handoff(*transition) != nullptr &&
		transition->raster().display_state_hash() == handoff_hash,
		"failed Story construction preserves the exact retryable handoff");

	auto created = MainProcessStoryAccess::create_story(
		config,
		*transition,
		dynamic_resources,
		*static_content,
		false
	);
	expect(static_cast<bool>(created),
		"Story process constructs from the retained handoff");
	if(!created) {
		return;
	}
	expect(transition->phase() == ClassicMainlStagePhase::consumed &&
		ClassicMainlStageTestAccess::handoff(*transition) == nullptr,
		"successful Story construction consumes the capability exactly once");

	resources.clear();
	resources.shrink_to_fit();
	dynamic_resources.clear();
	dynamic_resources.shrink_to_fit();
	font_rom.clear();
	font_rom.shrink_to_fit();
	static_content->reset();
	transition.reset();
	auto &process = **created;
	const auto expected_song = th03::app::classic_main_gameplay_song(
		config.resident
	);
	auto expected_play = expected_song.value_or(
		th03::app::ClassicAudioCommand{}
	);
	expected_play.type = th03::app::ClassicAudioCommandType::play_song;
	const auto reset_audio = process.take_audio_command();
	const auto play_audio = process.take_audio_command();
	expect(reset_audio == th03::app::ClassicAudioCommand{
		.type = th03::app::ClassicAudioCommandType::reset_effects,
	} && play_audio == expected_play && !process.take_audio_command(),
		"fresh Story MAIN resets effects then publishes Play after startup");
	const auto observation = process.round_start_observation();
	expect(process.input_protocol() == th03::InputProtocol::live &&
		observation.start.game_mode == th03::game_mode::story &&
		observation.start.story_stage == 0U &&
		observation.start.paletted_character ==
			std::array<std::int8_t, 2U>{2, 1},
		"Story resident and live protocol reach the fresh core process");
	const auto pixel_is = [&](th03::present::Point point, std::uint8_t color) {
		return process.raster().pixel(th03::present::Page::zero, point) == color;
	};
	expect(
		pixel_is({0, 200}, pi_sentinel(MainResourceKey{
			.role = MainResourceRole::character_stage_shot,
			.character = player_one.character,
			.palette = player_one.palette,
		})) && pixel_is({320, 200}, pi_sentinel(MainResourceKey{
			.role = MainResourceRole::character_stage_shot,
			.character = player_two.character,
			.palette = player_two.palette,
		})) && pixel_is({0, 304}, pi_sentinel(MainResourceKey{
			.role = MainResourceRole::character_stage_enemy_group,
			.character = player_two.character,
		})),
		"Story MAIN preserves the staged hidden atlas into committed page zero");
	std::vector<th03::present::ComposedPixel> pixels(
		th03::present::graphics_page_pixels
	);
	const auto surface = process.compose_shown_surface(std::span<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	>(pixels.data(), pixels.size()));
	const auto stepped = process.step({});
	expect(surface && stepped && stepped->status == th03::StepStatus::running &&
		process.presentation_status() == th03::present::MainRoundStatus::ready,
		"Story process remains usable after every caller-owned input is destroyed");
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

MainStoryConfig story_config(
	std::uint8_t stage,
	std::int8_t player_one,
	std::uint8_t remaining_credits
) noexcept
{
	MainStoryConfig config{};
	auto &resident = config.resident;
	resident.global_frame = 1000U + stage;
	resident.resident_rand = 12345;
	resident.random_seed = 12345U;
	resident.game_mode = th03::game_mode::story;
	resident.rank = 1U;
	resident.key_mode = 0U;
	resident.story_stage = stage;
	resident.story_lives = 2U;
	resident.remaining_credits = remaining_credits;
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
	config.kotohime_compatibility.red_provider = kotohime_red;
	return config;
}

std::vector<LoadedMainResource> story_dynamic_resources(
	const MainStoryConfig &config,
	std::span<const LoadedMainResource> complete
)
{
	const auto player_one = th03::app::main_player_selection_from_paletted(
		config.resident.paletted_character[0U]
	);
	const auto player_two = th03::app::main_player_selection_from_paletted(
		config.resident.paletted_character[1U]
	);
	expect(player_one.has_value() && player_two.has_value(),
		"Story fixture selections are valid");
	if(!player_one || !player_two) {
		return {};
	}
	const auto keys = th03::app::main_private_resource_keys(
		*player_one, *player_two
	);
	expect(keys.has_value(), "Story fixture MAIN-private keys resolve");
	if(!keys) {
		return {};
	}
	return select_resources(complete, *keys);
}

th03::GameInit classic_vs_init(
	std::uint8_t mode,
	std::uint8_t remaining_credits
) noexcept
{
	th03::GameInit init{};
	init.resident_rand = -1234567;
	init.random_seed = std::bit_cast<std::uint32_t>(init.resident_rand);
	init.game_mode = mode;
	init.rank = 1U;
	init.key_mode = 2U;
	init.story_lives = 77U;
	init.remaining_credits = remaining_credits;
	init.skill = 203U;
	init.paletted_character = {2, 1};
	init.story_opponents.fill(13);
	init.fast_op_animation = 1U;
	init.autofire_mask = 3U;
	switch(mode) {
	case th03::game_mode::vs_1p_cpu:
		init.is_cpu = {0U, 1U};
		break;
	case th03::game_mode::vs_1p_2p:
		init.is_cpu = {0U, 0U};
		break;
	case th03::game_mode::vs_cpu_cpu:
		init.is_cpu = {1U, 1U};
		break;
	default:
		break;
	}
	return init;
}

th03::GameInit classic_demo_init() noexcept
{
	auto init = classic_vs_init(th03::game_mode::vs_cpu_cpu, 2U);
	init.game_mode = th03::game_mode::demo;
	init.demo_number = 1U;
	init.paletted_character = {3, 1};
	init.resident_rand = 600;
	init.random_seed = std::bit_cast<std::uint32_t>(init.resident_rand);
	init.story_stage = 0U;
	init.global_frame = 0U;
	return init;
}

void test_classic_demo_handoff_and_runtime()
{
	const auto init = classic_demo_init();
	const auto player_one = *th03::app::main_player_selection_from_paletted(
		init.paletted_character[0U]
	);
	const auto player_two = *th03::app::main_player_selection_from_paletted(
		init.paletted_character[1U]
	);
	auto resources = make_resources(player_one, player_two);
	auto stage_resources = make_mainl_stage_resources(init);
	auto font_rom = make_font_rom();
	auto stage_font = th03::app::Pc98FontRomProvider::from_exact_font_rom(
		font_rom
	);
	expect(stage_font.has_value(),
		"Demo fixture prepares the exact-size MAINL text provider");
	if(!stage_font) {
		return;
	}
	auto completion = ClassicOpSceneTestAccess::completion(
		ClassicOpRoute::demo, init
	);
	auto stage = ClassicMainlStageTestAccess::
		create_demo_with_synthetic_resources(
			completion,
			{.red_provider = kotohime_red},
			stage_resources,
			mainl_stage_text_assets(),
			*stage_font
		);
	expect(stage && advance_mainl_stage_to_ready(**stage),
		"typed automatic Demo reaches one complete MAINL Raster handoff");
	if(!stage || (*stage)->phase() != ClassicMainlStagePhase::ready) {
		return;
	}

	const auto dynamic_keys = th03::app::main_private_resource_keys(
		player_one, player_two
	);
	expect(dynamic_keys.has_value(),
		"Demo fixture resolves its exact MAIN-private closure");
	if(!dynamic_keys) {
		return;
	}
	auto dynamic_resources = select_resources(resources, *dynamic_keys);
	auto compiled = make_compiled_content();
	expect(compiled.has_value(),
		"Demo fixture imports synthetic compiled MAIN content");
	if(!compiled) {
		return;
	}
	auto static_content =
		MainProcessTestAccess::prepare_static_with_synthetic_resources(
			resources, std::move(*compiled), font_rom
		);
	expect(static_content.has_value(),
		"Demo fixture prepares shared MAIN static content");
	if(!static_content) {
		return;
	}

	auto missing = dynamic_resources;
	missing.pop_back();
	auto rejected = MainProcessDemoAccess::create_demo(
		**stage, missing, *static_content, false
	);
	expect(!rejected && rejected.error().code ==
		MainProcessErrorCode::resource_set_mismatch &&
		(*stage)->phase() == ClassicMainlStagePhase::ready &&
		ClassicMainlStageTestAccess::handoff(**stage) != nullptr,
		"failed Demo process construction preserves the retryable MAINL handoff");

	auto created = MainProcessDemoAccess::create_demo(
		**stage, dynamic_resources, *static_content, false
	);
	expect(created.has_value() &&
		(*stage)->phase() == ClassicMainlStagePhase::consumed &&
		ClassicMainlStageTestAccess::handoff(**stage) == nullptr,
		"successful Demo MAIN publication consumes exactly one typed handoff");
	if(!created) {
		return;
	}
	const auto reset_audio = (*created)->take_audio_command();
	const auto play_audio = (*created)->take_audio_command();
	expect(reset_audio == th03::app::ClassicAudioCommand{
		.type = th03::app::ClassicAudioCommandType::reset_effects,
	} && play_audio == th03::app::ClassicAudioCommand{
		.type = th03::app::ClassicAudioCommandType::play_song,
		.cue = th03::app::ClassicMusicCue::gameplay,
		.parameter = static_cast<std::uint16_t>(
			th03::app::ClassicGameplayMusic::stage_00
		),
	} && !(*created)->take_audio_command(),
		"automatic Demo MAIN publishes Reset then source stage Play");
	const auto observation = (*created)->round_start_observation();
	expect(
		observation.start.game_mode == th03::game_mode::demo &&
			observation.start.demo_number == 1U &&
			observation.start.is_cpu == std::array<std::uint8_t, 2U>{1U, 1U} &&
			observation.start.paletted_character ==
				std::array<std::int8_t, 2U>{3, 1} &&
			observation.effective_reset_seed == 600U &&
			observation.round.state.cpu_safety_frames ==
				std::array<std::uint16_t, 2U>{0xFFFFU, 0xFFFFU} &&
			observation.round.state.round_speed == 64U &&
			observation.round.state.bullet_base_speed == 0U &&
			observation.round.state.boss_attack_level == 8U &&
			observation.round.state.gauge_attack_level ==
				std::array<std::uint8_t, 2U>{9U, 9U},
		"Demo MAIN applies the exact autonomous CPU/RNG/difficulty recipe"
	);
	expect((*created)->input_protocol() == th03::InputProtocol::live &&
		MainProcessTestAccess::continues_used_digit(**created) == 0U,
		"automatic Demo uses live attract input and the non-Story HUD digit");

	const auto exited = (*created)->step({.system = 0x1000U});
	expect(exited && exited->status == th03::StepStatus::terminal &&
		exited->input_boundary_state.route == 0U,
		"normalized physical Demo input requests source terminal route zero");
	expect((*created)->finish_main_process().has_value(),
		"terminal Demo persists its final MAIN boundary");
	expect((*created)->take_audio_command() == th03::app::ClassicAudioCommand{
		.type = th03::app::ClassicAudioCommandType::stop_song,
	} && !(*created)->take_audio_command(),
		"terminal Demo MAIN publishes exactly one source Stop command");
	const auto transition = (*created)->session_transition();
	expect(transition &&
		transition->action == th03::SessionAction::exit_to_operator &&
		transition->resident.demo_number == 1U &&
		transition->resident.fast_op_animation == init.fast_op_animation &&
		!transition->high_score_registration,
		"Demo route zero returns the exact resident to a fresh OP process");
}

void test_classic_vs_handoff_transaction_and_lifetime()
{
	const auto staging_config = story_config(0U, 2, 3U);
	const auto player_one = *th03::app::main_player_selection_from_paletted(2);
	const auto player_two = *th03::app::main_player_selection_from_paletted(1);
	auto resources = make_resources(player_one, player_two);
	auto staging_transition = make_ready_story_transition(
		staging_config, resources
	);
	expect(staging_transition != nullptr,
		"Classic VS fixture prepares one exact six-PI Raster handoff");
	if(!staging_transition) {
		return;
	}
	const auto handoff_raster = staging_transition->raster();
	const auto dynamic_keys = th03::app::main_private_resource_keys(
		player_one, player_two
	);
	expect(dynamic_keys && dynamic_keys->size() == 6U,
		"Classic VS process requests only the six MAIN-private resources");
	if(!dynamic_keys) {
		return;
	}
	auto dynamic_resources = select_resources(resources, *dynamic_keys);
	auto compiled = make_compiled_content();
	expect(compiled.has_value(),
		"Classic VS fixture imports synthetic compiled MAIN content");
	if(!compiled) {
		return;
	}
	auto font_rom = make_font_rom();
	auto static_content =
		MainProcessTestAccess::prepare_static_with_synthetic_resources(
			resources,
			std::move(*compiled),
			font_rom
		);
	expect(static_content.has_value(),
		"Classic VS fixture prepares one stable shared MAIN content owner");
	if(!static_content) {
		return;
	}
	auto stage_font = th03::app::Pc98FontRomProvider::from_exact_font_rom(
		font_rom
	);
	expect(stage_font.has_value(),
		"Classic VS fixture prepares the exact-size MAINL text provider");
	if(!stage_font) {
		return;
	}
	const auto story_policy_hash =
		staging_transition->raster().display_state_hash();
	auto wrong_policy = MainProcessClassicVsAccess::create_vs(
		*staging_transition, dynamic_resources, *static_content, false
	);
	expect(!wrong_policy && wrong_policy.error().code ==
		MainProcessErrorCode::invalid_config &&
		staging_transition->phase() == ClassicMainlStagePhase::ready &&
		ClassicMainlStageTestAccess::handoff(*staging_transition) != nullptr &&
		staging_transition->raster().display_state_hash() == story_policy_hash,
		"Classic VS MAIN rejects a Story-policy stage without mutation");

	struct Case final {
		std::uint8_t mode;
		std::array<std::uint8_t, 2U> is_cpu;
		std::uint8_t remaining_credits;
	};
	constexpr std::array cases{
		Case{th03::game_mode::vs_1p_cpu, {0U, 1U}, 0U},
		Case{th03::game_mode::vs_1p_2p, {0U, 0U}, 1U},
		Case{th03::game_mode::vs_cpu_cpu, {1U, 1U}, 2U},
		Case{th03::game_mode::vs_1p_cpu, {0U, 1U}, 3U},
	};
	std::vector<std::unique_ptr<MainProcess>> processes;
	processes.reserve(cases.size());
	for(std::size_t index = 0U; index < cases.size(); ++index) {
		const auto init = classic_vs_init(
			cases[index].mode, cases[index].remaining_credits
		);
		std::unique_ptr<th03::app::ClassicMainlStageTransition> transition;
		if(index == 0U) {
			{
				auto completion = ClassicOpSelectionTestAccess::make_completion(
					ClassicOpSelectionOutcome::selected,
					ClassicOpSelectionMode::vs_1p_cpu,
					init
				);
				auto stage_resources = make_mainl_stage_resources(init);
				auto created_stage = ClassicMainlStageTestAccess::
					create_vs_with_synthetic_resources(
						completion,
						{.red_provider = kotohime_red},
						stage_resources,
						mainl_stage_text_assets(),
						*stage_font
					);
				expect(created_stage.has_value(),
					"typed selector completion constructs the production VS stage");
				if(created_stage) {
					transition = std::move(*created_stage);
				}
			}
			expect(transition && advance_mainl_stage_to_ready(*transition),
				"production VS stage reaches its exact ready MAIN handoff");
		} else {
			transition =
				ClassicMainlStageTestAccess::create_ready_vs_with_handoff(
					init,
					{.red_provider = kotohime_red},
					handoff_raster
				);
		}
		expect(transition != nullptr &&
			transition->phase() == ClassicMainlStagePhase::ready &&
			ClassicMainlStageTestAccess::handoff(*transition) != nullptr,
			"Classic VS fixture owns one ready typed handoff");
		if(!transition) {
			continue;
		}
		const auto handoff_hash = transition->raster().display_state_hash();
		if(index == 0U) {
			auto missing = dynamic_resources;
			missing.pop_back();
			auto rejected = MainProcessClassicVsAccess::create_vs(
				*transition, missing, *static_content, false
			);
			expect(!rejected && rejected.error().code ==
				MainProcessErrorCode::resource_set_mismatch &&
				transition->phase() == ClassicMainlStagePhase::ready &&
				ClassicMainlStageTestAccess::handoff(*transition) != nullptr &&
				transition->raster().display_state_hash() == handoff_hash,
				"missing Classic VS private content preserves the retryable handoff");

			auto malformed = dynamic_resources;
			auto character = std::find_if(
				malformed.begin(), malformed.end(), [](const auto &resource) {
					return resource.spec.key.role ==
						MainResourceRole::character_sheet;
				}
			);
			expect(character != malformed.end(),
				"Classic VS late-failure fixture finds a character sheet");
			if(character != malformed.end()) {
				replace_bytes(*character, std::vector<std::byte>(
					character->spec.expected_bytes, std::byte{0U}
				));
			}
			rejected = MainProcessClassicVsAccess::create_vs(
				*transition, malformed, *static_content, false
			);
			expect(!rejected && rejected.error().code ==
				MainProcessErrorCode::asset_decode_failed &&
				transition->phase() == ClassicMainlStagePhase::ready &&
				ClassicMainlStageTestAccess::handoff(*transition) != nullptr &&
				transition->raster().display_state_hash() == handoff_hash,
				"late Classic VS decode failure leaves the exact handoff reusable");
		}

		auto created = MainProcessClassicVsAccess::create_vs(
			*transition, dynamic_resources, *static_content, false
		);
		expect(created.has_value() &&
			transition->phase() == ClassicMainlStagePhase::consumed &&
			ClassicMainlStageTestAccess::handoff(*transition) == nullptr,
			"successful Classic VS publication consumes exactly one handoff");
		if(!created) {
			continue;
		}
		const auto observation = (*created)->round_start_observation();
		expect((*created)->input_protocol() == th03::InputProtocol::live &&
			observation.start.game_mode == cases[index].mode &&
			observation.start.is_cpu == cases[index].is_cpu &&
			observation.start.paletted_character ==
				std::array<std::int8_t, 2U>{2, 1} &&
			observation.start.rank == 1U &&
			observation.start.key_mode == 2U,
			"Classic VS mode, CPU ownership, and selections reach fresh MAIN");
		const auto expected_safety = (cases[index].mode ==
			th03::game_mode::vs_cpu_cpu) ? std::uint16_t{0xFFFFU} :
			std::uint16_t{1900U};
		expect(observation.round.state.cpu_safety_frames ==
			std::array<std::uint16_t, 2U>{expected_safety, expected_safety},
			"Classic VS distinguishes CPU-CPU infinite safety from the rank table");
		expect(observation.start.story_lives == 77U &&
			observation.start.remaining_credits ==
				cases[index].remaining_credits &&
			observation.start.skill == 203U &&
			observation.start.story_opponents == init.story_opponents &&
			observation.start.fast_op_animation == 1U &&
			observation.start.autofire_mask == 3U &&
			observation.start.resident_rand == init.resident_rand &&
			observation.effective_reset_seed == init.random_seed,
			"Classic VS preserves source-stale resident fields and exact reset seed");
		expect(MainProcessTestAccess::continues_used_digit(**created) == 0U,
			"Classic VS always uses the donor literal continues digit zero");
		const auto reset_audio = (*created)->take_audio_command();
		const auto play_audio = (*created)->take_audio_command();
		expect(reset_audio == th03::app::ClassicAudioCommand{
			.type = th03::app::ClassicAudioCommandType::reset_effects,
		} && play_audio == th03::app::ClassicAudioCommand{
			.type = th03::app::ClassicAudioCommandType::play_song,
			.cue = th03::app::ClassicMusicCue::gameplay,
			.parameter = static_cast<std::uint16_t>(
				th03::app::ClassicGameplayMusic::stage_00
			),
		} && !(*created)->take_audio_command(),
			"Classic VS MAIN publishes Reset then the retained P2 theme Play");
		auto consumed_retry = MainProcessClassicVsAccess::create_vs(
			*transition, dynamic_resources, *static_content, false
		);
		expect(!consumed_retry && consumed_retry.error().code ==
			MainProcessErrorCode::invalid_config &&
			transition->phase() == ClassicMainlStagePhase::consumed &&
			ClassicMainlStageTestAccess::handoff(*transition) == nullptr,
			"a consumed Classic VS handoff cannot publish a second MAIN process");
		processes.push_back(std::move(*created));
	}

	resources.clear();
	resources.shrink_to_fit();
	dynamic_resources.clear();
	dynamic_resources.shrink_to_fit();
	font_rom.clear();
	font_rom.shrink_to_fit();
	staging_transition.reset();
	static_content->reset();
	expect(processes.size() == cases.size(),
		"all Classic VS modes publish before caller storage is destroyed");
	std::vector<th03::present::ComposedPixel> pixels(
		th03::present::graphics_page_pixels
	);
	const auto output = std::span<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	>(pixels.data(), pixels.size());
	for(auto &process : processes) {
		const auto stepped = process->step({});
		const auto surface = process->compose_shown_surface(output);
		expect(stepped && stepped->status == th03::StepStatus::running &&
			surface && process->presentation_status() ==
				th03::present::MainRoundStatus::ready &&
			process->tram().gaiji(255U).row_halves[31U] != 0U,
			"Classic VS remains usable after transition, resource, and static owners die");
	}
}

void test_story_handoff_transaction_and_shared_lifetime()
{
	const auto first_config = story_config(0U, 2, 3U);
	expect(th03::app::valid_main_story_config(first_config),
		"first Story process fixture satisfies the public source-fresh contract");
	const auto first_p1 = *th03::app::main_player_selection_from_paletted(
		first_config.resident.paletted_character[0U]
	);
	const auto first_p2 = *th03::app::main_player_selection_from_paletted(
		first_config.resident.paletted_character[1U]
	);
	auto first_complete = make_resources(first_p1, first_p2);
	auto compiled = make_compiled_content();
	expect(compiled.has_value(), "Story fixture imports synthetic compiled content");
	if(!compiled) {
		return;
	}
	auto font_rom = make_font_rom();
	auto static_content = MainProcessTestAccess::prepare_static_with_synthetic_resources(
		first_complete,
		std::move(*compiled),
		font_rom
	);
	expect(static_content.has_value(),
		"Story session prepares one stable shared MAIN content owner");
	if(!static_content) {
		return;
	}
	font_rom.clear();
	font_rom.shrink_to_fit();

	auto first_transition = make_ready_story_transition(
		first_config, first_complete
	);
	auto first_dynamic = story_dynamic_resources(
		first_config, first_complete
	);
	first_complete.clear();
	first_complete.shrink_to_fit();
	expect(first_transition != nullptr,
		"first Story fixture stages a complete six-PI handoff");
	if(!first_transition) {
		return;
	}
	const auto handoff_before = first_transition->raster().display_state_hash();
	const auto mismatched_config = story_config(1U, 2, 3U);
	auto rejected = MainProcessStoryAccess::create_story(
		mismatched_config,
		*first_transition,
		first_dynamic,
		*static_content,
		false
	);
	expect(!rejected && rejected.error().code ==
		MainProcessErrorCode::invalid_config &&
		first_transition->phase() == ClassicMainlStagePhase::ready &&
		ClassicMainlStageTestAccess::handoff(*first_transition) != nullptr &&
		first_transition->raster().display_state_hash() == handoff_before,
		"Story MAIN rejects a handoff from a different stage without mutation");
	auto malformed = first_dynamic;
	const auto first_sheet = std::find_if(
		malformed.begin(), malformed.end(), [](const auto &resource) {
			return resource.spec.key.role == MainResourceRole::character_sheet;
		}
	);
	expect(first_sheet != malformed.end(),
		"Story failure fixture finds its first character sheet");
	if(first_sheet != malformed.end()) {
		replace_bytes(*first_sheet, std::vector<std::byte>(
			first_sheet->spec.expected_bytes, std::byte{0U}
		));
	}
	rejected = MainProcessStoryAccess::create_story(
		first_config,
		*first_transition,
		malformed,
		*static_content,
		false
	);
	expect(!rejected && rejected.error().code ==
		MainProcessErrorCode::asset_decode_failed &&
		first_transition->phase() == ClassicMainlStagePhase::ready &&
		ClassicMainlStageTestAccess::handoff(*first_transition) != nullptr &&
		first_transition->raster().display_state_hash() == handoff_before,
		"a late Story MAIN failure leaves the ready handoff reusable and exact");

	auto first_process = MainProcessStoryAccess::create_story(
		first_config,
		*first_transition,
		first_dynamic,
		*static_content,
		false
	);
	expect(first_process.has_value() &&
		first_transition->phase() == ClassicMainlStagePhase::consumed &&
		ClassicMainlStageTestAccess::handoff(*first_transition) == nullptr,
		"successful Story MAIN publication consumes exactly one private handoff");
	if(!first_process) {
		return;
	}

	const auto final_config = story_config(8U, 18, 0U);
	expect(th03::app::valid_main_story_config(final_config),
		"final Story process fixture satisfies the public source-fresh contract");
	const auto final_p1 = *th03::app::main_player_selection_from_paletted(
		final_config.resident.paletted_character[0U]
	);
	const auto final_p2 = *th03::app::main_player_selection_from_paletted(
		final_config.resident.paletted_character[1U]
	);
	auto final_complete = make_resources(final_p1, final_p2);
	auto final_transition = make_ready_story_transition(
		final_config, final_complete
	);
	auto final_dynamic = story_dynamic_resources(
		final_config, final_complete
	);
	final_complete.clear();
	final_complete.shrink_to_fit();
	expect(final_transition != nullptr,
		"final Story fixture stages a complete six-PI handoff");
	if(!final_transition) {
		return;
	}
	auto final_process = MainProcessStoryAccess::create_story(
		final_config,
		*final_transition,
		final_dynamic,
		*static_content,
		false
	);
	expect(final_process.has_value() &&
		final_transition->phase() == ClassicMainlStagePhase::consumed,
		"the same shared static content starts a later Story MAIN process");
	if(!final_process) {
		return;
	}

	first_dynamic.clear();
	first_dynamic.shrink_to_fit();
	final_dynamic.clear();
	final_dynamic.shrink_to_fit();
	first_transition.reset();
	final_transition.reset();
	static_content->reset();

	const auto &first = **first_process;
	const auto &final = **final_process;
	expect(first.input_protocol() == th03::InputProtocol::live &&
		final.input_protocol() == th03::InputProtocol::live &&
		first.round_start_observation().start.story_stage == 0U &&
		final.round_start_observation().start.story_stage == 8U &&
		first.round_start_observation().start.remaining_credits == 3U &&
		final.round_start_observation().start.remaining_credits == 0U,
		"both Story processes retain their own exact resident values");
	expect(MainProcessTestAccess::continues_used_digit(first) == 0U &&
		MainProcessTestAccess::continues_used_digit(final) == 3U,
		"Story derives the displayed continues digit from remaining credits");
	const auto atlas_matches = [](const MainProcess &process,
		const MainStoryConfig &config) {
		constexpr std::array points{
			th03::present::Point{0, 200},
			th03::present::Point{320, 200},
			th03::present::Point{0, 208},
			th03::present::Point{320, 208},
			th03::present::Point{0, 280},
			th03::present::Point{0, 304},
		};
		const auto player_one = *th03::app::main_player_selection_from_paletted(
			config.resident.paletted_character[0U]
		);
		const auto player_two = *th03::app::main_player_selection_from_paletted(
			config.resident.paletted_character[1U]
		);
		const std::array expected{
			pi_sentinel({
				.role = MainResourceRole::character_stage_shot,
				.character = player_one.character,
				.palette = player_one.palette,
			}),
			pi_sentinel({
				.role = MainResourceRole::character_stage_shot,
				.character = player_two.character,
				.palette = player_two.palette,
			}),
			pi_sentinel({
				.role = MainResourceRole::character_stage_extra_shot,
				.character = player_one.character,
				.palette = player_one.palette,
			}),
			pi_sentinel({
				.role = MainResourceRole::character_stage_extra_shot,
				.character = player_two.character,
				.palette = player_two.palette,
			}),
			pi_sentinel({.role = MainResourceRole::stage_enemy_center}),
			pi_sentinel({
				.role = MainResourceRole::character_stage_enemy_group,
				.character = player_two.character,
			}),
		};
		for(std::size_t index = 0U; index < points.size(); ++index) {
			if(process.raster().pixel(
				th03::present::Page::zero, points[index]
			) != expected[index]) {
				return false;
			}
		}
		return true;
	};
	expect(atlas_matches(first, first_config) &&
		atlas_matches(final, final_config),
		"each Story process clones its own semantic MAINL six-PI handoff");
	expect(first.raster().applied_palette().colors[3U] ==
		th03::assets::Rgb4{3U, 12U, 6U} &&
		final.raster().applied_palette().colors[3U] ==
			th03::assets::Rgb4{3U, 12U, 6U},
		"both Story processes retain the final MAINL enemy palette");

	const auto first_step = (*first_process)->step({});
	const auto final_step = (*final_process)->step({});
	expect(first_step && final_step &&
		first_step->status == th03::StepStatus::running &&
		final_step->status == th03::StepStatus::running,
		"both Story processes remain live after every loader and handoff owner dies");
	std::vector<th03::present::ComposedPixel> pixels(
		th03::present::graphics_page_pixels
	);
	const auto surface = (*final_process)->compose_shown_surface(
		std::span<
			th03::present::ComposedPixel,
			th03::present::graphics_page_pixels
		>(pixels.data(), pixels.size())
	);
	expect(surface.has_value() &&
		(*first_process)->tram().gaiji(255U).row_halves[31U] != 0U &&
		(*final_process)->tram().gaiji(255U).row_halves[31U] != 0U,
		"shared GAMEFT/FONT ownership survives into both Story presenters");
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
	test_live_bomb_first_frame();
	test_same_selection_shared_raw_construction();
	test_recorded_pause_deferred_frame();
	test_live_runner_owns_the_actual_process();
	test_invalid_runtime_arguments_are_atomic();
	test_resource_failures_are_atomic();
	test_normalized_sheet_bundle_rejections();
	test_decode_and_staging_rejections();
	test_sticky_presentation_preflight();
	test_story_factory_transaction_and_lifetime();
	test_invalid_config_and_kotohime_policy();
	test_classic_demo_handoff_and_runtime();
	test_classic_vs_handoff_transaction_and_lifetime();
	test_story_handoff_transaction_and_shared_lifetime();
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
