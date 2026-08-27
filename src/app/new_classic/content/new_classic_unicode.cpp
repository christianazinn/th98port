#include "th03/app/new_classic/content/new_classic_unicode.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <ranges>
#include <span>

#include "th03/app/content/main_compiled_content.hpp"
#include "th03/app/content/resources.hpp"
#include "th03/core/utf8.hpp"

namespace th03::app {
namespace {

constexpr std::array<std::byte, 8U> magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'U'}, std::byte{'F'},
	std::byte{'N'}, std::byte{'0'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::size_t header_bytes = 96U;
constexpr std::size_t entry_bytes = 40U;
constexpr std::uint16_t version = 1U;

[[nodiscard]] std::uint16_t u16(
	std::span<const std::byte> bytes, std::size_t offset
) noexcept
{
	return static_cast<std::uint16_t>(
		std::to_integer<std::uint8_t>(bytes[offset]) |
		(static_cast<std::uint16_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 1U])
		) << 8U)
	);
}

[[nodiscard]] std::uint32_t u32(
	std::span<const std::byte> bytes, std::size_t offset
) noexcept
{
	std::uint32_t result = 0U;
	for(std::size_t byte = 0U; byte < 4U; ++byte) {
		result |= static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[offset + byte])
		) << (byte * 8U);
	}
	return result;
}

[[nodiscard]] NewClassicUnicodeFontError error(
	NewClassicUnicodeFontErrorCode code, std::size_t offset = 0U
) noexcept
{
	return {.code = code, .offset = offset};
}

[[nodiscard]] bool noncharacter(std::uint32_t scalar) noexcept
{
	return (scalar >= 0xFDD0U && scalar <= 0xFDEFU) ||
		(scalar <= 0x10FFFFU && (scalar & 0xFFFFU) >= 0xFFFEU);
}

[[nodiscard]] bool in_range(
	std::uint32_t value, std::uint32_t first, std::uint32_t last
) noexcept
{
	return value >= first && value <= last;
}

} // namespace

std::optional<NewClassicUtf8Scalar> next_new_classic_utf8_scalar(
	std::string_view text, std::size_t offset
) noexcept
{
	const auto scalar = next_utf8_scalar(text, offset);
	return scalar ? std::optional{NewClassicUtf8Scalar{
		scalar->value, scalar->bytes
	}} : std::nullopt;
}

bool valid_new_classic_utf8(std::string_view text) noexcept
{
	return valid_utf8(text);
}

bool new_classic_unicode_combining(std::uint32_t scalar) noexcept
{
	return unicode_combining(scalar);
}

bool new_classic_unicode_printable_scalar(std::uint32_t scalar) noexcept
{
	return unicode_printable_scalar(scalar);
}

std::uint8_t new_classic_unicode_cell_width(std::uint32_t scalar) noexcept
{
	return unicode_cell_width(scalar);
}

std::optional<NewClassicUnicodeGlyph> NewClassicUnicodeFont::glyph(
	std::uint32_t scalar
) const noexcept
{
	const auto found = std::ranges::lower_bound(
		glyphs_, scalar, {}, &NewClassicUnicodeGlyph::scalar
	);
	return found != glyphs_.end() && found->scalar == scalar ?
		std::optional{*found} : std::nullopt;
}

std::size_t NewClassicUnicodeFont::glyph_count() const noexcept
{
	return glyphs_.size();
}

std::expected<NewClassicUnicodeFont, NewClassicUnicodeFontError>
decode_new_classic_unicode_font(std::span<const std::byte> bytes)
{
	if(bytes.size() < header_bytes ||
		!std::ranges::equal(bytes.first(magic.size()), magic)) {
		return std::unexpected(error(NewClassicUnicodeFontErrorCode::input_size));
	}
	const auto total = static_cast<std::size_t>(u32(bytes, 12U));
	const auto count = static_cast<std::size_t>(u32(bytes, 16U));
	if(u16(bytes, 8U) != version || u16(bytes, 10U) != header_bytes ||
		total != bytes.size() || u32(bytes, 20U) != entry_bytes ||
		count > (std::numeric_limits<std::size_t>::max() - header_bytes) /
			entry_bytes || header_bytes + count * entry_bytes != bytes.size() ||
		!std::ranges::all_of(bytes.subspan(88U, 8U), [](std::byte byte) {
			return byte == std::byte{0U};
		})) {
		return std::unexpected(error(NewClassicUnicodeFontErrorCode::header_shape));
	}
	MainExecutableSha256 expected{};
	for(std::size_t index = 0U; index < expected.size(); ++index) {
		expected[index] = std::to_integer<std::uint8_t>(bytes[56U + index]);
	}
	if(main_executable_sha256(bytes.subspan(header_bytes)) != expected) {
		return std::unexpected(error(NewClassicUnicodeFontErrorCode::body_digest));
	}
	try {
		NewClassicUnicodeFont result{};
		result.glyphs_.reserve(count);
		std::uint32_t previous = 0U;
		bool first = true;
		for(std::size_t index = 0U; index < count; ++index) {
			const auto offset = header_bytes + index * entry_bytes;
			const auto scalar = u32(bytes, offset);
			const auto width = std::to_integer<std::uint8_t>(bytes[offset + 4U]);
			const auto advance = std::to_integer<std::uint8_t>(bytes[offset + 5U]);
			if((width != 8U && width != 16U) || advance != width ||
				u16(bytes, offset + 6U) != 0U || scalar > 0x10FFFFU ||
				in_range(scalar, 0xD800U, 0xDFFFU) || noncharacter(scalar)) {
				return std::unexpected(error(
					NewClassicUnicodeFontErrorCode::glyph_shape, offset
				));
			}
			if(!first && scalar <= previous) {
				return std::unexpected(error(
					NewClassicUnicodeFontErrorCode::glyph_order, offset
				));
			}
			NewClassicUnicodeGlyph glyph{.scalar = scalar, .width = width};
			for(std::size_t row = 0U; row < glyph.rows.size(); ++row) {
				glyph.rows[row] = static_cast<std::uint16_t>(
					static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(
						bytes[offset + 8U + row * 2U]
					)) << 8U
				) | std::to_integer<std::uint8_t>(
					bytes[offset + 9U + row * 2U]
				);
			}
			if(width == 8U && std::ranges::any_of(glyph.rows, [](auto row) {
				return (row & 0x00FFU) != 0U;
			})) {
				return std::unexpected(error(
					NewClassicUnicodeFontErrorCode::glyph_shape, offset
				));
			}
			result.glyphs_.push_back(glyph);
			previous = scalar;
			first = false;
		}
		for(const auto required : {0x00A1U, 0x00BFU, 0x00E9U, 0x3042U, 0xFFFDU}) {
			if(!result.glyph(required)) {
				return std::unexpected(error(
					NewClassicUnicodeFontErrorCode::required_glyph
				));
			}
		}
		return result;
	} catch(const std::bad_alloc &) {
		return std::unexpected(error(
			NewClassicUnicodeFontErrorCode::allocation_failed
		));
	}
}

} // namespace th03::app
