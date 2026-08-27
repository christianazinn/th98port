#ifndef TH03_APP_NEW_CLASSIC_UNICODE_HPP
#define TH03_APP_NEW_CLASSIC_UNICODE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace th03::app {

enum class NewClassicUnicodeFontErrorCode : std::uint8_t {
	input_size,
	header_shape,
	body_digest,
	glyph_shape,
	glyph_order,
	required_glyph,
	allocation_failed,
};

struct NewClassicUnicodeFontError final {
	NewClassicUnicodeFontErrorCode code =
		NewClassicUnicodeFontErrorCode::input_size;
	std::size_t offset = 0U;
};

struct NewClassicUnicodeGlyph final {
	std::uint32_t scalar = 0U;
	std::uint8_t width = 0U;
	std::array<std::uint16_t, 16U> rows{};
};

// Strict scalar decoder shared by online names, language packs, and the
// source-independent fallback renderer. It rejects overlong encodings,
// surrogates, noncharacters, and truncated sequences.
struct NewClassicUtf8Scalar final {
	std::uint32_t value = 0U;
	std::size_t bytes = 0U;
};

[[nodiscard]] std::optional<NewClassicUtf8Scalar>
next_new_classic_utf8_scalar(
	std::string_view text,
	std::size_t offset
) noexcept;
[[nodiscard]] bool valid_new_classic_utf8(std::string_view text) noexcept;
[[nodiscard]] bool new_classic_unicode_combining(std::uint32_t scalar) noexcept;
[[nodiscard]] bool new_classic_unicode_printable_scalar(
	std::uint32_t scalar
) noexcept;
[[nodiscard]] std::uint8_t new_classic_unicode_cell_width(
	std::uint32_t scalar
) noexcept;

class NewClassicUnicodeFont final {
public:
	[[nodiscard]] std::optional<NewClassicUnicodeGlyph> glyph(
		std::uint32_t scalar
	) const noexcept;
	[[nodiscard]] std::size_t glyph_count() const noexcept;

private:
	friend std::expected<NewClassicUnicodeFont, NewClassicUnicodeFontError>
	decode_new_classic_unicode_font(std::span<const std::byte>);
	std::vector<NewClassicUnicodeGlyph> glyphs_{};
};

[[nodiscard]] std::expected<
	NewClassicUnicodeFont,
	NewClassicUnicodeFontError
> decode_new_classic_unicode_font(std::span<const std::byte> bytes);

} // namespace th03::app
#endif
