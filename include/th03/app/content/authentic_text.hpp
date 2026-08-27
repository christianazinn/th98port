#ifndef TH03_APP_AUTHENTIC_TEXT_HPP
#define TH03_APP_AUTHENTIC_TEXT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

#include "th03/assets/types.hpp"
#include "th03/present/text.hpp"
#include "th03/present/tram.hpp"

namespace th03::app {

enum class AuthenticTextErrorCode : std::uint8_t {
	font_rom_size,
	font_rom_content,
	gameft_header,
	gameft_glyph_count,
	gameft_glyph_code,
	gameft_glyph_format,
	gameft_glyph_size,
};

struct AuthenticTextError {
	AuthenticTextErrorCode code = AuthenticTextErrorCode::font_rom_size;
	std::size_t index_or_size = 0U;

	[[nodiscard]] friend constexpr bool operator==(
		const AuthenticTextError &,
		const AuthenticTextError &
	) = default;
};

// A non-owning view of the exact PC-98 FONT.ROM container used by DOSBox-X:
// 256 8x16 ANK glyphs at 0x800, followed at 0x1800 by 92 blocks of
// 96 fullwidth glyphs. Canonical JIS (first, second) maps to
// 0x1800 + (first - 0x21) * 96 * 32 + (second - 0x20) * 32; each glyph stores
// all 16 left bytes before all 16 right bytes. The caller-owned bytes must
// remain alive and immutable for the entire provider lifetime. Both addressable
// glyph domains must contain ink; this rejects exact-size placeholder files
// that would otherwise make all machine-font text silently invisible.
class Pc98FontRomProvider final : public present::AnkGlyphProvider {
public:
	static constexpr std::size_t font_rom_size = 288768U;

	[[nodiscard]] static std::expected<
		Pc98FontRomProvider,
		AuthenticTextError
	> from_exact_font_rom(std::span<const std::byte> font_rom) noexcept;

	[[nodiscard]] std::optional<present::AnkGlyph> glyph(
		std::uint8_t code
	) const override;
	[[nodiscard]] std::optional<present::JisGlyph> jis_glyph(
		std::uint16_t canonical_jis
	) const override;

private:
	explicit Pc98FontRomProvider(
		std::span<const std::byte> font_rom
	) noexcept;

	std::span<const std::byte> font_rom_{};
};

// A validated, fixed copy of GAMEFT.BFT's complete gaiji bank. prepare()
// validates every glyph before retaining any installable table; install() is
// consequently infallible and cannot partially reject a late glyph.
class PreparedGameftGaiji final {
public:
	[[nodiscard]] static std::expected<
		PreparedGameftGaiji,
		AuthenticTextError
	> prepare(const assets::BfntSheet &sheet);

	void install(present::Tram &tram) const noexcept;

private:
	PreparedGameftGaiji() = default;

	std::array<present::GaijiGlyph, present::gaiji_count> glyphs_{};
};

} // namespace th03::app

#endif
