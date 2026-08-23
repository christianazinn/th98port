#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace th03 {

struct Utf8Scalar final {
	std::uint32_t value = 0U;
	std::size_t bytes = 0U;
};

[[nodiscard]] constexpr bool unicode_noncharacter(
	std::uint32_t scalar
) noexcept
{
	return (scalar >= 0xFDD0U && scalar <= 0xFDEFU) ||
		(scalar <= 0x10FFFFU && (scalar & 0xFFFFU) >= 0xFFFEU);
}

[[nodiscard]] constexpr std::optional<Utf8Scalar> next_utf8_scalar(
	std::string_view text,
	std::size_t offset
) noexcept
{
	if(offset >= text.size()) {
		return std::nullopt;
	}
	const auto first = static_cast<std::uint8_t>(text[offset]);
	std::uint32_t scalar = 0U;
	std::size_t count = 0U;
	std::uint32_t minimum = 0U;
	if(first <= 0x7FU) {
		scalar = first;
		count = 1U;
	} else if(first >= 0xC2U && first <= 0xDFU) {
		scalar = first & 0x1FU;
		count = 2U;
		minimum = 0x80U;
	} else if(first >= 0xE0U && first <= 0xEFU) {
		scalar = first & 0x0FU;
		count = 3U;
		minimum = 0x800U;
	} else if(first >= 0xF0U && first <= 0xF4U) {
		scalar = first & 0x07U;
		count = 4U;
		minimum = 0x10000U;
	} else {
		return std::nullopt;
	}
	if(count > text.size() - offset) {
		return std::nullopt;
	}
	for(std::size_t index = 1U; index < count; ++index) {
		const auto byte = static_cast<std::uint8_t>(text[offset + index]);
		if((byte & 0xC0U) != 0x80U) {
			return std::nullopt;
		}
		scalar = (scalar << 6U) | (byte & 0x3FU);
	}
	if(scalar < minimum || scalar > 0x10FFFFU ||
		(scalar >= 0xD800U && scalar <= 0xDFFFU) ||
		unicode_noncharacter(scalar)) {
		return std::nullopt;
	}
	return Utf8Scalar{scalar, count};
}

[[nodiscard]] constexpr bool valid_utf8(std::string_view text) noexcept
{
	for(std::size_t offset = 0U; offset < text.size();) {
		const auto scalar = next_utf8_scalar(text, offset);
		if(!scalar) {
			return false;
		}
		offset += scalar->bytes;
	}
	return true;
}

[[nodiscard]] constexpr bool unicode_combining(std::uint32_t scalar) noexcept
{
	const auto in = [scalar](std::uint32_t first, std::uint32_t last) {
		return scalar >= first && scalar <= last;
	};
	return in(0x0300U, 0x036FU) || in(0x1AB0U, 0x1AFFU) ||
		in(0x1DC0U, 0x1DFFU) || in(0x20D0U, 0x20FFU) ||
		in(0xFE00U, 0xFE0FU) || in(0xFE20U, 0xFE2FU) ||
		in(0xE0100U, 0xE01EFU);
}

[[nodiscard]] constexpr bool unicode_unsupported_format(
	std::uint32_t scalar
) noexcept
{
	const auto in = [scalar](std::uint32_t first, std::uint32_t last) {
		return scalar >= first && scalar <= last;
	};
	// New Classic has no bidi or contextual-shaping engine yet. Reject invisible
	// direction/word-joining controls instead of accepting misleading account
	// names that the deterministic bitmap renderer cannot faithfully display.
	return scalar == 0x00ADU || scalar == 0x061CU ||
		in(0x200BU, 0x200FU) || in(0x202AU, 0x202EU) ||
		in(0x2060U, 0x206FU) || scalar == 0xFEFFU ||
		in(0xFFF9U, 0xFFFBU) || in(0xE0000U, 0xE007FU);
}

[[nodiscard]] constexpr std::uint8_t unicode_cell_width(
	std::uint32_t scalar
) noexcept
{
	if(unicode_combining(scalar)) {
		return 0U;
	}
	const auto in = [scalar](std::uint32_t first, std::uint32_t last) {
		return scalar >= first && scalar <= last;
	};
	return (in(0x1100U, 0x115FU) || scalar == 0x2329U ||
		scalar == 0x232AU || in(0x2E80U, 0xA4CFU) ||
		in(0xAC00U, 0xD7A3U) || in(0xF900U, 0xFAFFU) ||
		in(0xFE10U, 0xFE19U) || in(0xFE30U, 0xFE6FU) ||
		in(0xFF01U, 0xFF60U) || in(0xFFE0U, 0xFFE6U) ||
		in(0x1F000U, 0x1FAFFU) || in(0x20000U, 0x3FFFDU)) ? 2U : 1U;
}

[[nodiscard]] constexpr bool unicode_printable_scalar(
	std::uint32_t scalar
) noexcept
{
	return scalar >= 0x20U && scalar != 0x7FU &&
		!(scalar >= 0x80U && scalar <= 0x9FU) &&
		!unicode_unsupported_format(scalar);
}

} // namespace th03
