#include "th03/app/content/main_compiled_content.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "../src/app/content/main_compiled_content_internal.hpp"

namespace {

using th03::app::MainCompiledContentErrorCode;
using th03::app::MainExecutableSha256;
using th03::present::MainResultLabel;

constexpr std::size_t stock_file_bytes = 130882U;
constexpr std::size_t load_image_file_offset = 0x1860U;

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

void put_u16(
	std::vector<std::byte> &bytes,
	std::size_t offset,
	std::uint16_t value
)
{
	bytes[offset] = static_cast<std::byte>(
		static_cast<std::uint8_t>(value)
	);
	bytes[offset + 1U] = static_cast<std::byte>(
		static_cast<std::uint8_t>(value >> 8U)
	);
}

void put_byte(
	std::vector<std::byte> &bytes,
	std::size_t load_offset,
	std::uint8_t value
)
{
	bytes[load_image_file_offset + load_offset] = static_cast<std::byte>(value);
}

[[nodiscard]] constexpr std::uint8_t score_pattern(std::size_t index) noexcept
{
	return static_cast<std::uint8_t>((index * 37U + 11U) & 0xFFU);
}

[[nodiscard]] constexpr std::uint8_t pellet_pattern(std::size_t index) noexcept
{
	return static_cast<std::uint8_t>((index * 29U + 7U) & 0xFFU);
}

std::vector<std::byte> synthetic_executable()
{
	std::vector<std::byte> bytes(stock_file_bytes);
	put_u16(bytes, 0U, 0x5A4DU);
	put_u16(bytes, 2U, 322U);
	put_u16(bytes, 4U, 256U);
	put_u16(bytes, 6U, 0U);
	put_u16(bytes, 8U, 390U);
	put_u16(bytes, 24U, 28U);

	put_byte(bytes, 0x1DD7CU, 0x0CU);
	put_byte(bytes, 0x1DD7DU, 0x0DU);
	put_byte(bytes, 0x1DD7EU, 0U);
	for(std::size_t index = 0U; index < 4U; ++index) {
		constexpr std::array<std::uint8_t, 4U> glyphs = {
			0x0AU, 0x0BU, 0x0EU, 0x0FU,
		};
		put_byte(bytes, 0x1DD7FU + index, glyphs[index]);
	}
	put_byte(bytes, 0x1DD83U, 0U);
	for(std::size_t index = 0U; index < 8U; ++index) {
		put_byte(bytes, 0x1DD84U + index, 0x20U);
	}
	put_byte(bytes, 0x1DD8CU, 0U);

	struct TextRange {
		std::size_t offset;
		std::size_t payload;
		MainResultLabel label;
	};
	constexpr std::array<TextRange, 9U> text_ranges = {{
		{0x1DE4AU, 26U, MainResultLabel::max_combo},
		{0x1DE65U, 26U, MainResultLabel::gauge_attack},
		{0x1DE80U, 26U, MainResultLabel::boss_attack},
		{0x1DE9BU, 26U, MainResultLabel::boss_attack_reversed},
		{0x1DEB6U, 26U, MainResultLabel::boss_panic},
		{0x1DED1U, 26U, MainResultLabel::total},
		{0x1DEECU, 32U, MainResultLabel::winner_bonus},
		{0x1DF0DU, 32U, MainResultLabel::all_clear_bonus},
		{0x1DF2EU, 26U, MainResultLabel::player_remaining},
	}};
	for(const auto range : text_ranges) {
		const auto label_index = static_cast<std::size_t>(range.label);
		put_byte(bytes, range.offset, static_cast<std::uint8_t>('A' + label_index));
		constexpr std::array<std::uint8_t, 6U> shift_jis_pairs = {
			0x82U, 0x60U,
			0x82U, 0x81U,
			0x82U, 0xA0U,
		};
		for(std::size_t index = 0U; index < shift_jis_pairs.size(); ++index) {
			put_byte(bytes, range.offset + 1U + index, shift_jis_pairs[index]);
		}
		for(std::size_t index = 7U; index < range.payload; ++index) {
			put_byte(
				bytes,
				range.offset + index,
				static_cast<std::uint8_t>('a' + label_index)
			);
		}
		put_byte(bytes, range.offset + range.payload, 0U);
	}
	for(std::size_t index = 0U;
		index < th03::present::score_font_mask_bytes;
		++index) {
		put_byte(bytes, 0x1DDB8U + index, score_pattern(index));
	}
	for(std::size_t index = 0U;
		index < th03::present::pellet_mask_bytes;
		++index) {
		put_byte(bytes, 0x1DF4AU + index, pellet_pattern(index));
	}
	return bytes;
}

template <std::size_t Size>
std::array<std::byte, Size> byte_string(const char (&text)[Size + 1U])
{
	std::array<std::byte, Size> bytes{};
	for(std::size_t index = 0U; index < Size; ++index) {
		bytes[index] = static_cast<std::byte>(
			static_cast<std::uint8_t>(text[index])
		);
	}
	return bytes;
}

template <std::size_t Size>
std::array<std::byte, Size> repeated_a()
{
	std::array<std::byte, Size> bytes{};
	bytes.fill(std::byte{'a'});
	return bytes;
}

void test_sha256_known_vectors_and_mutation()
{
	constexpr MainExecutableSha256 empty_digest = {
		0xE3U, 0xB0U, 0xC4U, 0x42U, 0x98U, 0xFCU, 0x1CU, 0x14U,
		0x9AU, 0xFBU, 0xF4U, 0xC8U, 0x99U, 0x6FU, 0xB9U, 0x24U,
		0x27U, 0xAEU, 0x41U, 0xE4U, 0x64U, 0x9BU, 0x93U, 0x4CU,
		0xA4U, 0x95U, 0x99U, 0x1BU, 0x78U, 0x52U, 0xB8U, 0x55U,
	};
	constexpr MainExecutableSha256 abc_digest = {
		0xBAU, 0x78U, 0x16U, 0xBFU, 0x8FU, 0x01U, 0xCFU, 0xEAU,
		0x41U, 0x41U, 0x40U, 0xDEU, 0x5DU, 0xAEU, 0x22U, 0x23U,
		0xB0U, 0x03U, 0x61U, 0xA3U, 0x96U, 0x17U, 0x7AU, 0x9CU,
		0xB4U, 0x10U, 0xFFU, 0x61U, 0xF2U, 0x00U, 0x15U, 0xADU,
	};
	constexpr MainExecutableSha256 a55_digest = {
		0x9FU, 0x43U, 0x90U, 0xF8U, 0xD3U, 0x0CU, 0x2DU, 0xD9U,
		0x2EU, 0xC9U, 0xF0U, 0x95U, 0xB6U, 0x5EU, 0x2BU, 0x9AU,
		0xE9U, 0xB0U, 0xA9U, 0x25U, 0xA5U, 0x25U, 0x8EU, 0x24U,
		0x1CU, 0x9FU, 0x1EU, 0x91U, 0x0FU, 0x73U, 0x43U, 0x18U,
	};
	constexpr MainExecutableSha256 a56_digest = {
		0xB3U, 0x54U, 0x39U, 0xA4U, 0xACU, 0x6FU, 0x09U, 0x48U,
		0xB6U, 0xD6U, 0xF9U, 0xE3U, 0xC6U, 0xAFU, 0x0FU, 0x5FU,
		0x59U, 0x0CU, 0xE2U, 0x0FU, 0x1BU, 0xDEU, 0x70U, 0x90U,
		0xEFU, 0x79U, 0x70U, 0x68U, 0x6EU, 0xC6U, 0x73U, 0x8AU,
	};
	constexpr MainExecutableSha256 a63_digest = {
		0x7DU, 0x3EU, 0x74U, 0xA0U, 0x5DU, 0x7DU, 0xB1U, 0x5BU,
		0xCEU, 0x4AU, 0xD9U, 0xECU, 0x06U, 0x58U, 0xEAU, 0x98U,
		0xE3U, 0xF0U, 0x6EU, 0xEEU, 0xCFU, 0x16U, 0xB4U, 0xC6U,
		0xFFU, 0xF2U, 0xDAU, 0x45U, 0x7DU, 0xDCU, 0x2FU, 0x34U,
	};
	constexpr MainExecutableSha256 a64_digest = {
		0xFFU, 0xE0U, 0x54U, 0xFEU, 0x7AU, 0xE0U, 0xCBU, 0x6DU,
		0xC6U, 0x5CU, 0x3AU, 0xF9U, 0xB6U, 0x1DU, 0x52U, 0x09U,
		0xF4U, 0x39U, 0x85U, 0x1DU, 0xB4U, 0x3DU, 0x0BU, 0xA5U,
		0x99U, 0x73U, 0x37U, 0xDFU, 0x15U, 0x46U, 0x68U, 0xEBU,
	};
	expect(
		th03::app::main_executable_sha256({}) == empty_digest,
		"SHA-256 empty known vector"
	);
	auto abc = byte_string<3U>("abc");
	expect(
		th03::app::main_executable_sha256(abc) == abc_digest,
		"SHA-256 abc known vector"
	);
	expect(
		th03::app::main_executable_sha256(repeated_a<55U>()) == a55_digest,
		"SHA-256 55-byte pre-padding-boundary vector"
	);
	expect(
		th03::app::main_executable_sha256(repeated_a<56U>()) == a56_digest,
		"SHA-256 56-byte padding-boundary vector"
	);
	expect(
		th03::app::main_executable_sha256(repeated_a<63U>()) == a63_digest,
		"SHA-256 63-byte pre-block-boundary vector"
	);
	expect(
		th03::app::main_executable_sha256(repeated_a<64U>()) == a64_digest,
		"SHA-256 64-byte block-boundary vector"
	);
	abc[2U] = std::byte{'d'};
	expect(
		th03::app::main_executable_sha256(abc) != abc_digest,
		"SHA-256 detects a one-byte mutation"
	);
}

void test_mz_and_profile_rejections()
{
	std::array<std::byte, 3U> short_input{};
	auto result = th03::app::import_main_compiled_content(short_input);
	expect(
		!result && result.error().code ==
			MainCompiledContentErrorCode::input_too_short,
		"short input rejected"
	);

	auto bytes = synthetic_executable();
	bytes[0U] = std::byte{0U};
	result = th03::app::import_main_compiled_content(bytes);
	expect(
		!result && result.error().code ==
			MainCompiledContentErrorCode::invalid_mz_signature,
		"invalid MZ signature rejected"
	);

	bytes = synthetic_executable();
	put_u16(bytes, 4U, 255U);
	result = th03::app::import_main_compiled_content(bytes);
	expect(
		!result && result.error().code ==
			MainCompiledContentErrorCode::invalid_mz_layout,
		"MZ declared size mismatch rejected"
	);

	bytes = synthetic_executable();
	result = th03::app::import_main_compiled_content(bytes);
	expect(
		!result && result.error().code ==
			MainCompiledContentErrorCode::unsupported_profile,
		"structurally valid unrecognized profile rejected"
	);
}

void test_hermetic_owned_content()
{
	std::optional<th03::app::MainCompiledContent> stable_content;
	{
		auto bytes = synthetic_executable();
		const auto digest = th03::app::main_executable_sha256(bytes);
		auto imported = th03::app::detail::import_main_compiled_content_for_test(
			bytes, digest
		);
		expect(static_cast<bool>(imported), "synthetic compiled content parses");
		if(!imported) {
			return;
		}
		stable_content.emplace(std::move(*imported));
		std::fill(bytes.begin(), bytes.end(), std::byte{0U});
	}
	expect(stable_content.has_value(), "synthetic import returns owning content");
	if(!stable_content) {
		return;
	}

	const auto score = stable_content->score_font_masks();
	expect(
		score.rows.size() == th03::present::score_font_mask_bytes &&
		score.row_order == th03::present::PackedRowOrder::bottom_to_top,
		"synthetic score accessor shape"
	);
	for(std::size_t index = 0U; index < score.rows.size(); ++index) {
		expect(score.rows[index] == score_pattern(index), "owned score byte");
	}
	const auto pellets = stable_content->pellet_masks();
	expect(
		pellets.rows.size() == th03::present::pellet_mask_bytes &&
		pellets.row_order == th03::present::PackedRowOrder::top_to_bottom,
		"synthetic pellet accessor shape"
	);
	for(std::size_t index = 0U; index < pellets.rows.size(); ++index) {
		expect(
			pellets.rows[index] == pellet_pattern(index),
			"owned pellet byte"
		);
	}

	const auto tram = stable_content->hud_result_tram_assets();
	// Independent expected PC-98 TRAM words for Shift-JIS 82:60, 82:81,
	// and 82:A0. These cover trails below 80h, from 80h through 9Eh,
	// and from 9Fh upward, including each right-half marker.
	constexpr std::array<th03::present::TramCode, 6U> expected_jis = {
		0x4103U, 0x4183U,
		0x6103U, 0x6183U,
		0x2204U, 0x2284U,
	};
	for(std::size_t label_index = 0U;
		label_index < tram.result_labels.size();
		++label_index) {
		const auto label = static_cast<MainResultLabel>(label_index);
		const auto expected_size =
			((label == MainResultLabel::winner_bonus) ||
			(label == MainResultLabel::all_clear_bonus)) ? 32U : 26U;
		const auto cells = tram.result_labels[label_index];
		expect(cells.size() == expected_size, "synthetic label exact length");
		if(cells.size() != expected_size) {
			continue;
		}
		expect(
			cells[0U].code == static_cast<std::uint16_t>('A' + label_index),
			"synthetic label identity"
		);
		for(std::size_t index = 0U; index < expected_jis.size(); ++index) {
			expect(
				cells[index + 1U].code == expected_jis[index],
				"synthetic label raw JIS cell"
			);
		}
		for(std::size_t index = 7U; index < cells.size(); ++index) {
			expect(
				cells[index].code ==
					static_cast<std::uint16_t>('a' + label_index),
				"synthetic label trailing ANK identity"
			);
		}
		for(const auto cell : cells) {
			expect(
				cell.attribute == th03::present::tram_white,
				"synthetic label white attribute"
			);
		}
	}
	constexpr std::array<std::uint8_t, 2U> hit_ids = {0x0CU, 0x0DU};
	for(std::size_t index = 0U; index < hit_ids.size(); ++index) {
		expect(
			tram.combo_hit[index * 2U].code ==
				th03::present::gaiji_cell_code(
					hit_ids[index], th03::present::GaijiHalf::left
				) && tram.combo_hit[(index * 2U) + 1U].code ==
				th03::present::gaiji_cell_code(
					hit_ids[index], th03::present::GaijiHalf::right
				),
			"synthetic combo hit identity"
		);
	}
	constexpr std::array<std::uint8_t, 4U> bonus_ids = {
		0x0AU, 0x0BU, 0x0EU, 0x0FU,
	};
	for(std::size_t index = 0U; index < bonus_ids.size(); ++index) {
		expect(
			tram.combo_bonus[index * 2U].code ==
				th03::present::gaiji_cell_code(
					bonus_ids[index], th03::present::GaijiHalf::left
				) && tram.combo_bonus[(index * 2U) + 1U].code ==
				th03::present::gaiji_cell_code(
					bonus_ids[index], th03::present::GaijiHalf::right
				),
			"synthetic combo bonus identity"
		);
	}
	for(const auto cell : tram.combo_erase) {
		expect(
			cell == th03::present::TramCell{' ', th03::present::tram_white},
			"synthetic combo erase identity"
		);
	}
}

void test_hermetic_late_parser_paths()
{
	auto bytes = synthetic_executable();
	auto digest = th03::app::main_executable_sha256(bytes);
	auto result = th03::app::detail::import_main_compiled_content_for_test(
		bytes, digest
	);

	bytes = synthetic_executable();
	put_u16(bytes, 8U, 4096U);
	digest = th03::app::main_executable_sha256(bytes);
	result = th03::app::detail::import_main_compiled_content_for_test(
		bytes, digest
	);
	expect(
		!result && result.error().code ==
			MainCompiledContentErrorCode::compiled_range_out_of_bounds,
		"compiled range outside load image rejected"
	);

	bytes = synthetic_executable();
	put_byte(bytes, 0x1DD7CU, 0x0BU);
	digest = th03::app::main_executable_sha256(bytes);
	result = th03::app::detail::import_main_compiled_content_for_test(
		bytes, digest
	);
	expect(
		!result && result.error().code ==
			MainCompiledContentErrorCode::invalid_combo_data,
		"invalid combo gaiji sequence rejected"
	);

	bytes = synthetic_executable();
	put_byte(bytes, 0x1DE4AU + 3U, 0U);
	digest = th03::app::main_executable_sha256(bytes);
	result = th03::app::detail::import_main_compiled_content_for_test(
		bytes, digest
	);
	expect(
		!result && result.error().code ==
			MainCompiledContentErrorCode::malformed_source_text,
		"embedded source-text terminator rejected"
	);

	bytes = synthetic_executable();
	put_byte(bytes, 0x1DE4AU + 25U, 0x82U);
	digest = th03::app::main_executable_sha256(bytes);
	result = th03::app::detail::import_main_compiled_content_for_test(
		bytes, digest
	);
	expect(
		!result && result.error().code ==
			MainCompiledContentErrorCode::malformed_source_text,
		"dangling Shift-JIS lead byte rejected"
	);

	bytes = synthetic_executable();
	digest = th03::app::main_executable_sha256(bytes);
	digest[0U] ^= 1U;
	result = th03::app::detail::import_main_compiled_content_for_test(
		bytes, digest
	);
	expect(
		!result && result.error().code ==
			MainCompiledContentErrorCode::unsupported_profile,
		"full digest mismatch rejected after successful parse"
	);
}

std::vector<std::byte> read_binary(const char *path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if(!file) {
		return {};
	}
	const auto end = file.tellg();
	if(end <= 0) {
		return {};
	}
	std::vector<char> chars(static_cast<std::size_t>(end));
	file.seekg(0, std::ios::beg);
	file.read(chars.data(), static_cast<std::streamsize>(chars.size()));
	if(!file) {
		return {};
	}
	std::vector<std::byte> bytes(chars.size());
	for(std::size_t index = 0U; index < chars.size(); ++index) {
		bytes[index] = static_cast<std::byte>(
			static_cast<unsigned char>(chars[index])
		);
	}
	return bytes;
}

void test_optional_local_profile(const char *path)
{
	constexpr MainExecutableSha256 expected_stock_digest = {
		0xF4U, 0x1FU, 0xDEU, 0x47U, 0xEAU, 0x36U, 0xBFU, 0x4DU,
		0x98U, 0x5FU, 0xF9U, 0x12U, 0x7BU, 0x67U, 0xFEU, 0x93U,
		0xD5U, 0xECU, 0xB5U, 0x8CU, 0xC3U, 0x6EU, 0x7CU, 0xFFU,
		0xABU, 0x86U, 0x95U, 0x9DU, 0xB7U, 0xF2U, 0xCEU, 0x6BU,
	};
	auto bytes = read_binary(path);
	expect(bytes.size() == stock_file_bytes, "local profile exact size");
	if(bytes.size() != stock_file_bytes) {
		return;
	}
	expect(
		th03::app::main_executable_sha256(bytes) == expected_stock_digest,
		"local profile exact SHA-256"
	);
	auto imported = th03::app::import_main_compiled_content(bytes);
	expect(static_cast<bool>(imported), "local authenticated profile imports");
	if(!imported) {
		return;
	}
	expect(
		imported->profile() ==
			th03::app::MainExecutableProfile::original_japanese,
		"local profile identity"
	);
	const auto score = imported->score_font_masks();
	expect(
		score.rows.size() == th03::present::score_font_mask_bytes &&
		score.row_order == th03::present::PackedRowOrder::bottom_to_top,
		"local score masks shape and row order"
	);
	for(std::size_t index = 0U; index < score.rows.size(); ++index) {
		expect(
			score.rows[index] == std::to_integer<std::uint8_t>(
				bytes[load_image_file_offset + 0x1DDB8U + index]
			),
			"local score byte copied exactly"
		);
	}
	const auto pellets = imported->pellet_masks();
	expect(
		pellets.rows.size() == th03::present::pellet_mask_bytes &&
		pellets.row_order == th03::present::PackedRowOrder::top_to_bottom,
		"local pellet masks shape and row order"
	);
	for(std::size_t index = 0U; index < pellets.rows.size(); ++index) {
		expect(
			pellets.rows[index] == std::to_integer<std::uint8_t>(
				bytes[load_image_file_offset + 0x1DF4AU + index]
			),
			"local pellet byte copied exactly"
		);
	}
	const auto tram = imported->hud_result_tram_assets();
	for(std::size_t index = 0U; index < tram.result_labels.size(); ++index) {
		const auto label = static_cast<MainResultLabel>(index);
		const auto expected_size =
			((label == MainResultLabel::winner_bonus) ||
			(label == MainResultLabel::all_clear_bonus)) ? 32U : 26U;
		expect(
			tram.result_labels[index].size() == expected_size,
			"local result label span length"
		);
		for(const auto cell : tram.result_labels[index]) {
			expect(
				cell.attribute == th03::present::tram_white,
				"local result label attribute"
			);
		}
	}
	expect(
		tram.combo_hit.size() == 4U && tram.combo_bonus.size() == 8U &&
		tram.combo_erase.size() == 8U,
		"local combo cell span lengths"
	);
	expect(
		tram.combo_hit[0U].code == th03::present::gaiji_cell_code(
			0x0CU, th03::present::GaijiHalf::left
		) && tram.combo_hit[1U].code == th03::present::gaiji_cell_code(
			0x0CU, th03::present::GaijiHalf::right
		),
		"local combo hit begins with source gaiji ID 0C"
	);
	for(const auto cell : tram.combo_erase) {
		expect(
			cell.code == ' ' && cell.attribute == th03::present::tram_white,
			"local combo erase cells"
		);
	}

	bytes[0x100U] ^= std::byte{1U};
	const auto mutated = th03::app::import_main_compiled_content(bytes);
	expect(
		!mutated && mutated.error().code ==
			MainCompiledContentErrorCode::unsupported_profile,
		"one-byte local executable mutation rejected"
	);
}

} // namespace

int main(int argc, char **argv)
{
	test_sha256_known_vectors_and_mutation();
	test_mz_and_profile_rejections();
	test_hermetic_owned_content();
	test_hermetic_late_parser_paths();
	if(argc == 2) {
		test_optional_local_profile(argv[1]);
	} else if(argc != 1) {
		expect(false, "expected zero arguments or one local MAIN.EXE path");
	}
	if(failures != 0U) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << "All " << checks << " app compiled-content checks passed\n";
	return 0;
}
