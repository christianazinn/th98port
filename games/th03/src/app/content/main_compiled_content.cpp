#include "th03/app/content/main_compiled_content.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string_view>
#include <utility>

#include "content/main_compiled_content_internal.hpp"
#include "th03/app/new_classic/content/new_classic_game_text.hpp"

namespace th03::app {
namespace {

using present::MainResultLabel;
using present::TramCell;

constexpr std::size_t stock_file_bytes = 130882U;
constexpr std::size_t mz_minimum_header_bytes = 28U;
constexpr std::size_t mz_page_bytes = 512U;
constexpr std::size_t combo_hit_load_offset = 0x1DD7CU;
constexpr std::size_t combo_bonus_load_offset = 0x1DD7FU;
constexpr std::size_t combo_erase_load_offset = 0x1DD84U;
constexpr std::size_t score_font_load_offset = 0x1DDB8U;
constexpr std::size_t pellet_masks_load_offset = 0x1DF4AU;

constexpr MainExecutableSha256 stock_sha256 = {
	0xF4U, 0x1FU, 0xDEU, 0x47U, 0xEAU, 0x36U, 0xBFU, 0x4DU,
	0x98U, 0x5FU, 0xF9U, 0x12U, 0x7BU, 0x67U, 0xFEU, 0x93U,
	0xD5U, 0xECU, 0xB5U, 0x8CU, 0xC3U, 0x6EU, 0x7CU, 0xFFU,
	0xABU, 0x86U, 0x95U, 0x9DU, 0xB7U, 0xF2U, 0xCEU, 0x6BU,
};

struct ResultSpec {
	MainResultLabel label;
	MainCompiledContentItem item;
	std::size_t load_offset;
	std::size_t payload_bytes;
};

constexpr std::array<ResultSpec, present::main_result_label_count>
result_specs = {{
	{MainResultLabel::max_combo, MainCompiledContentItem::result_max_combo,
		0x1DE4AU, 26U},
	{MainResultLabel::gauge_attack,
		MainCompiledContentItem::result_gauge_attack, 0x1DE65U, 26U},
	{MainResultLabel::boss_attack,
		MainCompiledContentItem::result_boss_attack, 0x1DE80U, 26U},
	{MainResultLabel::boss_attack_reversed,
		MainCompiledContentItem::result_boss_attack_reversed, 0x1DE9BU, 26U},
	{MainResultLabel::boss_panic,
		MainCompiledContentItem::result_boss_panic, 0x1DEB6U, 26U},
	{MainResultLabel::total, MainCompiledContentItem::result_total,
		0x1DED1U, 26U},
	{MainResultLabel::winner_bonus,
		MainCompiledContentItem::result_winner_bonus, 0x1DEECU, 32U},
	{MainResultLabel::all_clear_bonus,
		MainCompiledContentItem::result_all_clear_bonus, 0x1DF0DU, 32U},
	{MainResultLabel::player_remaining,
		MainCompiledContentItem::result_player_remaining, 0x1DF2EU, 26U},
}};

struct ExtractedContent {
	std::array<std::uint8_t, present::score_font_mask_bytes> score_font{};
	std::array<std::uint8_t, present::pellet_mask_bytes> pellet_masks{};
	std::array<std::array<TramCell, 32U>, present::main_result_label_count>
		result_labels{};
	std::array<TramCell, 4U> combo_hit{};
	std::array<TramCell, 8U> combo_bonus{};
	std::array<TramCell, 8U> combo_erase{};
};

struct MzLayout {
	std::size_t header_bytes = 0U;
};

[[nodiscard]] constexpr std::uint32_t rotate_right(
	std::uint32_t value,
	unsigned count
) noexcept {
	return std::rotr(value, static_cast<int>(count));
}

constexpr std::array<std::uint32_t, 64U> sha256_constants = {
	0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U,
	0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
	0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U,
	0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
	0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU,
	0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
	0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
	0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
	0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U,
	0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
	0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U,
	0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
	0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U,
	0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
	0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U,
	0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U,
};

void sha256_compress(
	std::array<std::uint32_t, 8U> &state,
	std::span<const std::byte, 64U> block
) noexcept {
	std::array<std::uint32_t, 64U> words{};
	for(std::size_t index = 0U; index < 16U; ++index) {
		const auto byte = index * 4U;
		words[index] =
			(static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(block[byte])) << 24U) |
			(static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(block[byte + 1U])) << 16U) |
			(static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(block[byte + 2U])) << 8U) |
			static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(block[byte + 3U]));
	}
	for(std::size_t index = 16U; index < words.size(); ++index) {
		const auto previous_15 = words[index - 15U];
		const auto previous_2 = words[index - 2U];
		const auto sigma0 = rotate_right(previous_15, 7U) ^
			rotate_right(previous_15, 18U) ^ (previous_15 >> 3U);
		const auto sigma1 = rotate_right(previous_2, 17U) ^
			rotate_right(previous_2, 19U) ^ (previous_2 >> 10U);
		words[index] = words[index - 16U] + sigma0 + words[index - 7U] + sigma1;
	}

	auto a = state[0U];
	auto b = state[1U];
	auto c = state[2U];
	auto d = state[3U];
	auto e = state[4U];
	auto f = state[5U];
	auto g = state[6U];
	auto h = state[7U];
	for(std::size_t index = 0U; index < words.size(); ++index) {
		const auto sum1 = rotate_right(e, 6U) ^ rotate_right(e, 11U) ^
			rotate_right(e, 25U);
		const auto choose = (e & f) ^ ((~e) & g);
		const auto temporary1 = h + sum1 + choose +
			sha256_constants[index] + words[index];
		const auto sum0 = rotate_right(a, 2U) ^ rotate_right(a, 13U) ^
			rotate_right(a, 22U);
		const auto majority = (a & b) ^ (a & c) ^ (b & c);
		const auto temporary2 = sum0 + majority;
		h = g;
		g = f;
		f = e;
		e = d + temporary1;
		d = c;
		c = b;
		b = a;
		a = temporary1 + temporary2;
	}
	state[0U] += a;
	state[1U] += b;
	state[2U] += c;
	state[3U] += d;
	state[4U] += e;
	state[5U] += f;
	state[6U] += g;
	state[7U] += h;
}

[[nodiscard]] std::uint16_t read_u16(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept {
	return static_cast<std::uint16_t>(
		std::to_integer<std::uint8_t>(bytes[offset]) |
		(static_cast<std::uint16_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 1U])
		) << 8U)
	);
}

[[nodiscard]] MainCompiledContentError error(
	MainCompiledContentErrorCode code,
	MainCompiledContentItem item = MainCompiledContentItem::none,
	std::size_t offset = 0U
) noexcept {
	return {code, item, offset};
}

[[nodiscard]] std::expected<MzLayout, MainCompiledContentError>
parse_mz(std::span<const std::byte> bytes) noexcept {
	if(bytes.size() < mz_minimum_header_bytes) {
		return std::unexpected(error(MainCompiledContentErrorCode::input_too_short));
	}
	if(read_u16(bytes, 0U) != 0x5A4DU) {
		return std::unexpected(error(
			MainCompiledContentErrorCode::invalid_mz_signature,
			MainCompiledContentItem::none,
			0U
		));
	}
	const auto last_page_bytes = static_cast<std::size_t>(read_u16(bytes, 2U));
	const auto page_count = static_cast<std::size_t>(read_u16(bytes, 4U));
	const auto relocation_count = static_cast<std::size_t>(read_u16(bytes, 6U));
	const auto header_paragraphs = static_cast<std::size_t>(read_u16(bytes, 8U));
	const auto relocation_offset = static_cast<std::size_t>(read_u16(bytes, 24U));
	if(page_count == 0U || last_page_bytes >= mz_page_bytes) {
		return std::unexpected(error(MainCompiledContentErrorCode::invalid_mz_layout));
	}
	const auto declared_bytes = ((page_count - 1U) * mz_page_bytes) +
		((last_page_bytes == 0U) ? mz_page_bytes : last_page_bytes);
	const auto header_bytes = header_paragraphs * 16U;
	if(declared_bytes != bytes.size() ||
		header_bytes < mz_minimum_header_bytes || header_bytes > declared_bytes ||
		relocation_offset > header_bytes ||
		relocation_count > ((header_bytes - relocation_offset) / 4U)) {
		return std::unexpected(error(MainCompiledContentErrorCode::invalid_mz_layout));
	}
	return MzLayout{header_bytes};
}

[[nodiscard]] std::expected<std::span<const std::byte>, MainCompiledContentError>
compiled_range(
	std::span<const std::byte> bytes,
	const MzLayout &layout,
	std::size_t load_offset,
	std::size_t count,
	MainCompiledContentItem item
) noexcept {
	if(load_offset > (bytes.size() - layout.header_bytes)) {
		return std::unexpected(error(
			MainCompiledContentErrorCode::compiled_range_out_of_bounds,
			item,
			layout.header_bytes
		));
	}
	const auto absolute = layout.header_bytes + load_offset;
	if(count > (bytes.size() - absolute)) {
		return std::unexpected(error(
			MainCompiledContentErrorCode::compiled_range_out_of_bounds,
			item,
			absolute
		));
	}
	return bytes.subspan(absolute, count);
}

template <std::size_t Size>
void copy_bytes(
	std::array<std::uint8_t, Size> &destination,
	std::span<const std::byte> source
) noexcept {
	for(std::size_t index = 0U; index < Size; ++index) {
		destination[index] = std::to_integer<std::uint8_t>(source[index]);
	}
}

[[nodiscard]] bool is_shift_jis_lead(std::uint8_t byte) noexcept {
	return ((byte >= 0x81U) && (byte <= 0x9FU)) ||
		((byte >= 0xE0U) && (byte <= 0xFCU));
}

[[nodiscard]] bool is_shift_jis_trail(std::uint8_t byte) noexcept {
	return ((byte >= 0x40U) && (byte <= 0x7EU)) ||
		((byte >= 0x80U) && (byte <= 0xFCU));
}

[[nodiscard]] present::TramCode shift_jis_to_first_tram_code(
	std::uint8_t lead,
	std::uint8_t trail
) noexcept {
	std::uint16_t value = static_cast<std::uint16_t>(
		(static_cast<std::uint16_t>(lead) << 9U) | trail
	);
	bool carry = false;
	if(trail < 0x9FU) {
		const auto sum = static_cast<std::uint32_t>(value) + 0xFEDFU +
			((trail < 0x80U) ? 1U : 0U);
		value = static_cast<std::uint16_t>(sum);
		carry = (sum > std::numeric_limits<std::uint16_t>::max());
	}
	auto low = static_cast<std::uint8_t>(value);
	low = static_cast<std::uint8_t>(
		low - static_cast<std::uint8_t>(0xFEU + (carry ? 1U : 0U))
	);
	value = static_cast<std::uint16_t>((value & 0xFF00U) | low);
	value &= 0x7F7FU;
	return static_cast<present::TramCode>((value << 8U) | (value >> 8U));
}

[[nodiscard]] std::expected<void, MainCompiledContentError> decode_result_label(
	std::array<TramCell, 32U> &destination,
	std::span<const std::byte> source,
	const ResultSpec &spec,
	std::size_t absolute
) noexcept {
	if(std::to_integer<std::uint8_t>(source[spec.payload_bytes]) != 0U) {
		return std::unexpected(error(
			MainCompiledContentErrorCode::malformed_source_text,
			spec.item,
			absolute + spec.payload_bytes
		));
	}
	std::size_t source_index = 0U;
	std::size_t output_index = 0U;
	while(source_index < spec.payload_bytes) {
		const auto first = std::to_integer<std::uint8_t>(source[source_index]);
		if(first == 0U) {
			return std::unexpected(error(
				MainCompiledContentErrorCode::malformed_source_text,
				spec.item,
				absolute + source_index
			));
		}
		if(!is_shift_jis_lead(first)) {
			destination[output_index++] = {first, present::tram_white};
			++source_index;
			continue;
		}
		if((source_index + 1U) >= spec.payload_bytes) {
			return std::unexpected(error(
				MainCompiledContentErrorCode::malformed_source_text,
				spec.item,
				absolute + source_index
			));
		}
		const auto second = std::to_integer<std::uint8_t>(source[source_index + 1U]);
		if(!is_shift_jis_trail(second)) {
			return std::unexpected(error(
				MainCompiledContentErrorCode::malformed_source_text,
				spec.item,
				absolute + source_index + 1U
			));
		}
		const auto left = shift_jis_to_first_tram_code(first, second);
		destination[output_index++] = {left, present::tram_white};
		destination[output_index++] = {
			static_cast<present::TramCode>(left | 0x0080U),
			present::tram_white,
		};
		source_index += 2U;
	}
	if(output_index != spec.payload_bytes) {
		return std::unexpected(error(
			MainCompiledContentErrorCode::malformed_source_text,
			spec.item,
			absolute
		));
	}
	return {};
}

[[nodiscard]] std::expected<ExtractedContent, MainCompiledContentError>
import_impl(
	std::span<const std::byte> bytes,
	const MainExecutableSha256 &accepted_digest
) noexcept {
	const auto mz = parse_mz(bytes);
	if(!mz) {
		return std::unexpected(mz.error());
	}
	if(bytes.size() != stock_file_bytes) {
		return std::unexpected(error(MainCompiledContentErrorCode::unsupported_profile));
	}
	if(main_executable_sha256(bytes) != accepted_digest) {
		return std::unexpected(error(MainCompiledContentErrorCode::unsupported_profile));
	}

	ExtractedContent content{};
	const auto hit = compiled_range(
		bytes, *mz, combo_hit_load_offset, 3U,
		MainCompiledContentItem::combo_hit
	);
	if(!hit) {
		return std::unexpected(hit.error());
	}
	constexpr std::array<std::uint8_t, 3U> expected_hit = {0x0CU, 0x0DU, 0U};
	for(std::size_t index = 0U; index < expected_hit.size(); ++index) {
		if(std::to_integer<std::uint8_t>((*hit)[index]) != expected_hit[index]) {
			return std::unexpected(error(
				MainCompiledContentErrorCode::invalid_combo_data,
				MainCompiledContentItem::combo_hit,
				mz->header_bytes + combo_hit_load_offset + index
			));
		}
	}
	for(std::size_t index = 0U; index < 2U; ++index) {
		const auto glyph = expected_hit[index];
		content.combo_hit[index * 2U] = {
			present::gaiji_cell_code(glyph, present::GaijiHalf::left),
			present::tram_white,
		};
		content.combo_hit[(index * 2U) + 1U] = {
			present::gaiji_cell_code(glyph, present::GaijiHalf::right),
			present::tram_white,
		};
	}

	const auto bonus = compiled_range(
		bytes, *mz, combo_bonus_load_offset, 5U,
		MainCompiledContentItem::combo_bonus
	);
	if(!bonus) {
		return std::unexpected(bonus.error());
	}
	constexpr std::array<std::uint8_t, 5U> expected_bonus = {
		0x0AU, 0x0BU, 0x0EU, 0x0FU, 0U,
	};
	for(std::size_t index = 0U; index < expected_bonus.size(); ++index) {
		if(std::to_integer<std::uint8_t>((*bonus)[index]) != expected_bonus[index]) {
			return std::unexpected(error(
				MainCompiledContentErrorCode::invalid_combo_data,
				MainCompiledContentItem::combo_bonus,
				mz->header_bytes + combo_bonus_load_offset + index
			));
		}
	}
	for(std::size_t index = 0U; index < 4U; ++index) {
		const auto glyph = expected_bonus[index];
		content.combo_bonus[index * 2U] = {
			present::gaiji_cell_code(glyph, present::GaijiHalf::left),
			present::tram_white,
		};
		content.combo_bonus[(index * 2U) + 1U] = {
			present::gaiji_cell_code(glyph, present::GaijiHalf::right),
			present::tram_white,
		};
	}

	const auto erase = compiled_range(
		bytes, *mz, combo_erase_load_offset, 9U,
		MainCompiledContentItem::combo_erase
	);
	if(!erase) {
		return std::unexpected(erase.error());
	}
	for(std::size_t index = 0U; index < 9U; ++index) {
		const auto expected = (index == 8U) ? 0U : 0x20U;
		if(std::to_integer<std::uint8_t>((*erase)[index]) != expected) {
			return std::unexpected(error(
				MainCompiledContentErrorCode::invalid_combo_data,
				MainCompiledContentItem::combo_erase,
				mz->header_bytes + combo_erase_load_offset + index
			));
		}
		if(index < content.combo_erase.size()) {
			content.combo_erase[index] = {' ', present::tram_white};
		}
	}

	for(const auto &spec : result_specs) {
		const auto source = compiled_range(
			bytes, *mz, spec.load_offset, spec.payload_bytes + 1U, spec.item
		);
		if(!source) {
			return std::unexpected(source.error());
		}
		const auto result = decode_result_label(
			content.result_labels[static_cast<std::size_t>(spec.label)],
			*source,
			spec,
			mz->header_bytes + spec.load_offset
		);
		if(!result) {
			return std::unexpected(result.error());
		}
	}

	const auto score = compiled_range(
		bytes, *mz, score_font_load_offset, content.score_font.size(),
		MainCompiledContentItem::score_font
	);
	if(!score) {
		return std::unexpected(score.error());
	}
	copy_bytes(content.score_font, *score);
	const auto pellets = compiled_range(
		bytes, *mz, pellet_masks_load_offset, content.pellet_masks.size(),
		MainCompiledContentItem::pellet_masks
	);
	if(!pellets) {
		return std::unexpected(pellets.error());
	}
	copy_bytes(content.pellet_masks, *pellets);

	return content;
}

[[nodiscard]] constexpr std::size_t label_size(MainResultLabel label) noexcept {
	return ((label == MainResultLabel::winner_bonus) ||
		(label == MainResultLabel::all_clear_bonus)) ? 32U : 26U;
}

} // namespace

struct MainCompiledContentImportAccess {
	[[nodiscard]] static MainCompiledContent from(
		ExtractedContent content
	) noexcept {
		MainCompiledContent destination{};
		destination.score_font_ = std::move(content.score_font);
		destination.pellet_masks_ = std::move(content.pellet_masks);
		destination.result_labels_ = std::move(content.result_labels);
		destination.combo_hit_ = std::move(content.combo_hit);
		destination.combo_bonus_ = std::move(content.combo_bonus);
		destination.combo_erase_ = std::move(content.combo_erase);
		return destination;
	}
};

AppSha256 app_sha256(
	std::span<const std::byte> bytes
) noexcept {
	std::array<std::uint32_t, 8U> state = {
		0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
		0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U,
	};
	std::size_t offset = 0U;
	while((bytes.size() - offset) >= 64U) {
		sha256_compress(state, std::span<const std::byte, 64U>(
			bytes.data() + offset, 64U
		));
		offset += 64U;
	}
	std::array<std::byte, 128U> final_blocks{};
	const auto remaining = bytes.size() - offset;
	if(remaining != 0U) {
		std::copy_n(bytes.data() + offset, remaining, final_blocks.data());
	}
	final_blocks[remaining] = std::byte{0x80U};
	const auto padded_bytes = (remaining < 56U) ? 64U : 128U;
	const auto bit_count = static_cast<std::uint64_t>(bytes.size()) * 8U;
	for(std::size_t index = 0U; index < 8U; ++index) {
		final_blocks[padded_bytes - 1U - index] = static_cast<std::byte>(
			static_cast<std::uint8_t>(bit_count >> (index * 8U))
		);
	}
	sha256_compress(state, std::span<const std::byte, 64U>(
		final_blocks.data(), 64U
	));
	if(padded_bytes == 128U) {
		sha256_compress(state, std::span<const std::byte, 64U>(
			final_blocks.data() + 64U, 64U
		));
	}
	AppSha256 digest{};
	for(std::size_t index = 0U; index < state.size(); ++index) {
		for(std::size_t byte = 0U; byte < 4U; ++byte) {
			digest[(index * 4U) + byte] = static_cast<std::uint8_t>(
			state[index] >> ((3U - byte) * 8U)
			);
		}
	}
	return digest;
}

MainExecutableSha256 main_executable_sha256(
	std::span<const std::byte> bytes
) noexcept {
	return app_sha256(bytes);
}

MainExecutableProfile MainCompiledContent::profile() const noexcept {
	return profile_;
}

present::ScoreFontMasks MainCompiledContent::score_font_masks() const noexcept {
	return {score_font_, present::PackedRowOrder::bottom_to_top};
}

present::PelletMasks MainCompiledContent::pellet_masks() const noexcept {
	return {pellet_masks_, present::PackedRowOrder::top_to_bottom};
}

present::MainHudResultTramAssets
MainCompiledContent::hud_result_tram_assets() const noexcept {
	present::MainHudResultTramAssets assets{};
	for(std::size_t index = 0U; index < result_labels_.size(); ++index) {
		const auto label = static_cast<MainResultLabel>(index);
		assets.result_labels[index] = std::span<const TramCell>(
			result_labels_[index].data(), label_size(label)
		);
	}
	assets.combo_hit = combo_hit_;
	assets.combo_bonus = combo_bonus_;
	assets.combo_erase = combo_erase_;
	return assets;
}

void MainCompiledContent::apply_new_classic_english_results(
	const NewClassicGameTextCatalog &text
) noexcept
{
	constexpr std::array labels{
		NewClassicGameTextId::result_gauge_attack,
		NewClassicGameTextId::result_boss_attack,
		NewClassicGameTextId::result_boss_flip,
		NewClassicGameTextId::result_boss_panic,
		NewClassicGameTextId::result_lives_left,
	};
	constexpr std::array destinations{
		MainResultLabel::gauge_attack,
		MainResultLabel::boss_attack,
		MainResultLabel::boss_attack_reversed,
		MainResultLabel::boss_panic,
		MainResultLabel::player_remaining,
	};
	for(std::size_t index = 0U; index < labels.size(); ++index) {
		const auto label = text.bytes(labels[index]);
		if(label.size() != 26U) {
			continue;
		}
		std::array<std::byte, 27U> source{};
		for(std::size_t byte = 0U; byte < label.size(); ++byte) {
			source[byte] = static_cast<std::byte>(label[byte]);
		}
		const ResultSpec spec{
			.label = destinations[index],
			.item = MainCompiledContentItem::none,
			.load_offset = 0U,
			.payload_bytes = 26U,
		};
		static_cast<void>(decode_result_label(
			result_labels_[static_cast<std::size_t>(destinations[index])],
			source,
			spec,
			0U
		));
	}
}

std::expected<MainCompiledContent, MainCompiledContentError>
import_main_compiled_content(
	std::span<const std::byte> main_executable
) noexcept {
	auto parsed = import_impl(main_executable, stock_sha256);
	if(!parsed) {
		return std::unexpected(parsed.error());
	}
	return MainCompiledContentImportAccess::from(std::move(*parsed));
}

namespace detail {

std::expected<MainCompiledContent, MainCompiledContentError>
import_main_compiled_content_for_test(
	std::span<const std::byte> main_executable,
	const MainExecutableSha256 &accepted_digest
) noexcept {
	auto parsed = import_impl(main_executable, accepted_digest);
	if(!parsed) {
		return std::unexpected(parsed.error());
	}
	return MainCompiledContentImportAccess::from(std::move(*parsed));
}

} // namespace detail
} // namespace th03::app
