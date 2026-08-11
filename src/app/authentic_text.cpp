#include "th03/app/authentic_text.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <variant>

namespace th03::app {
namespace {

constexpr std::size_t ank_offset = 0x800U;
constexpr std::size_t jis_offset = 0x1800U;
constexpr std::uint8_t jis_first_min = 0x21U;
constexpr std::uint8_t jis_first_max = 0x7CU;
constexpr std::uint8_t jis_second_min = 0x20U;
constexpr std::uint8_t jis_second_max = 0x7FU;
constexpr std::size_t jis_columns = 96U;
constexpr std::size_t jis_source_bytes = 32U;

[[nodiscard]] constexpr std::uint8_t byte_value(std::byte value) noexcept
{
	return std::to_integer<std::uint8_t>(value);
}

[[nodiscard]] AuthenticTextError gameft_error(
	AuthenticTextErrorCode code,
	std::size_t index = 0U
) noexcept
{
	return {code, index};
}

} // namespace

Pc98FontRomProvider::Pc98FontRomProvider(
	std::span<const std::byte> font_rom
) noexcept : font_rom_(font_rom)
{
}

std::expected<Pc98FontRomProvider, AuthenticTextError>
Pc98FontRomProvider::from_exact_font_rom(
	std::span<const std::byte> font_rom
) noexcept
{
	if(font_rom.size() != font_rom_size) {
		return std::unexpected(AuthenticTextError{
			AuthenticTextErrorCode::font_rom_size,
			font_rom.size(),
		});
	}
	const auto contains_ink = [](std::span<const std::byte> domain) noexcept {
		return std::any_of(
			domain.begin(), domain.end(),
			[](std::byte value) { return value != std::byte{0U}; }
		);
	};
	if(!contains_ink(font_rom.subspan(ank_offset, jis_offset - ank_offset))) {
		return std::unexpected(AuthenticTextError{
			AuthenticTextErrorCode::font_rom_content,
			ank_offset,
		});
	}
	if(!contains_ink(font_rom.subspan(jis_offset))) {
		return std::unexpected(AuthenticTextError{
			AuthenticTextErrorCode::font_rom_content,
			jis_offset,
		});
	}
	return Pc98FontRomProvider(font_rom);
}

std::optional<present::AnkGlyph> Pc98FontRomProvider::glyph(
	std::uint8_t code
) const
{
	present::AnkGlyph result{};
	const auto source_offset = ank_offset +
		(static_cast<std::size_t>(code) * present::ank_height);
	for(std::size_t row = 0U; row < present::ank_height; ++row) {
		result.rows[row] = byte_value(font_rom_[source_offset + row]);
	}
	return result;
}

std::optional<present::JisGlyph> Pc98FontRomProvider::jis_glyph(
	std::uint16_t canonical_jis
) const
{
	const auto first = static_cast<std::uint8_t>(canonical_jis >> 8U);
	const auto second = static_cast<std::uint8_t>(canonical_jis & 0x00FFU);
	if(first < jis_first_min || first > jis_first_max ||
		second < jis_second_min || second > jis_second_max) {
		return std::nullopt;
	}

	const auto block = static_cast<std::size_t>(first - jis_first_min);
	const auto glyph = static_cast<std::size_t>(second - jis_second_min);
	const auto source_offset = jis_offset +
		(block * jis_columns * jis_source_bytes) +
		(glyph * jis_source_bytes);
	present::JisGlyph result{};
	for(std::size_t row = 0U; row < present::jis_height; ++row) {
		result.row_halves[(row * present::jis_bytes_per_row)] =
			byte_value(font_rom_[source_offset + row]);
		result.row_halves[(row * present::jis_bytes_per_row) + 1U] =
			byte_value(font_rom_[source_offset + present::jis_height + row]);
	}
	return result;
}

std::expected<PreparedGameftGaiji, AuthenticTextError>
PreparedGameftGaiji::prepare(const assets::BfntSheet &sheet)
{
	// gaiji_entry_bfnt() checks precisely these header fields. In particular,
	// it accepts the version and skips the extended header without interpreting
	// it, so this portable transaction intentionally does the same.
	if(sheet.consumer != assets::BfntConsumerPolicy::gaiji ||
		sheet.header.color_flags != 0U ||
		sheet.header.pixel_width != present::gaiji_width ||
		sheet.header.pixel_height != present::gaiji_height ||
		sheet.header.first_code != 0U ||
		sheet.header.last_code != 255U) {
		return std::unexpected(gameft_error(
			AuthenticTextErrorCode::gameft_header
		));
	}
	if(sheet.glyphs.size() != present::gaiji_count) {
		return std::unexpected(gameft_error(
			AuthenticTextErrorCode::gameft_glyph_count,
			sheet.glyphs.size()
		));
	}

	PreparedGameftGaiji result{};
	for(std::size_t index = 0U; index < present::gaiji_count; ++index) {
		const auto &source = sheet.glyphs[index];
		if(source.code != index) {
			return std::unexpected(gameft_error(
				AuthenticTextErrorCode::gameft_glyph_code,
				index
			));
		}
		const auto *bitmap = std::get_if<assets::MonochromeBitmap1>(
			&source.pixels
		);
		if(bitmap == nullptr ||
			bitmap->row_order != assets::PlanarRowOrder::top_to_bottom ||
			bitmap->bit_order !=
				assets::PlanarBitOrder::most_significant_bit_left) {
			return std::unexpected(gameft_error(
				AuthenticTextErrorCode::gameft_glyph_format,
				index
			));
		}
		if(bitmap->bits.size() != present::gaiji_glyph_bytes) {
			return std::unexpected(gameft_error(
				AuthenticTextErrorCode::gameft_glyph_size,
				index
			));
		}
		std::copy(
			bitmap->bits.begin(),
			bitmap->bits.end(),
			result.glyphs_[index].row_halves.begin()
		);
	}
	return result;
}

void PreparedGameftGaiji::install(present::Tram &tram) const noexcept
{
	for(std::size_t index = 0U; index < glyphs_.size(); ++index) {
		static_cast<void>(tram.load_gaiji(
			static_cast<std::uint8_t>(index),
			glyphs_[index].row_halves
		));
	}
}

} // namespace th03::app
