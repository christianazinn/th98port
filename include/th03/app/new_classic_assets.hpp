#ifndef TH03_APP_NEW_CLASSIC_ASSETS_HPP
#define TH03_APP_NEW_CLASSIC_ASSETS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>
#include <vector>

#include "th03/assets/types.hpp"
#include "th03/app/new_classic_messages.hpp"
#include "th03/app/new_classic_game_text.hpp"
#include "th03/app/new_classic_cutscene_text.hpp"
#include "th03/app/new_classic_picture_patch.hpp"
#include "th03/app/new_classic_unicode.hpp"
#include "th03/present/raster.hpp"
#include "th03/present/source_surface.hpp"

namespace th03::app {

enum class NewClassicAssetsErrorCode : std::uint8_t {
	invalid_path,
	source_status,
	source_not_regular,
	source_size,
	source_open,
	source_read,
	digest_mismatch,
	archive_header,
	archive_directory,
	archive_member,
	archive_member_set,
	font_shape,
	font_checksum,
	pause_gaiji_shape,
	pause_gaiji_checksum,
	menu_background_shape,
	unicode_font_shape,
	pi_decode,
	pi_geometry,
	allocation_failed,
};

struct NewClassicAssetsError final {
	NewClassicAssetsErrorCode code = NewClassicAssetsErrorCode::invalid_path;
	std::error_code system{};
	std::size_t offset = 0U;
	std::optional<assets::AssetError> asset_error{};
};

// Native form of Replay Patch 0.4.12's MNUFONT.PF. The renderer deliberately
// preserves mnufont.cpp's 16x16, MSB-first, GRCG-RMW contract: set glyph bits
// replace pixels with [color], while zero bits leave the destination intact.
class NewClassicMenuFont final {
public:
	static constexpr std::size_t glyph_count = 95U;
	static constexpr std::uint8_t first_character = 0x20U;
	static constexpr std::uint8_t last_character = 0x7EU;
	static constexpr std::uint8_t fallback_character = 0x3FU;
	static constexpr std::uint16_t glyph_width = 16U;
	static constexpr std::uint16_t glyph_height = 16U;
	static constexpr std::size_t glyph_bytes = 32U;

	[[nodiscard]] std::int32_t width(std::string_view text) const noexcept;
	[[nodiscard]] std::int32_t width_n(
		std::string_view text,
		std::size_t count
	) const noexcept;
	[[nodiscard]] bool put(
		present::Raster &raster,
		std::int32_t left,
		std::int32_t top,
		std::string_view text,
		present::Index4 color
	) const noexcept;
	[[nodiscard]] bool put_n(
		present::Raster &raster,
		std::int32_t left,
		std::int32_t top,
		std::string_view text,
		std::size_t count,
		present::Index4 color
	) const noexcept;
	[[nodiscard]] bool put_centered(
		present::Raster &raster,
		std::int32_t center_x,
		std::int32_t top,
		std::string_view text,
		present::Index4 color
	) const noexcept;
	[[nodiscard]] bool put_centered_clipped(
		present::Raster &raster,
		std::int32_t center_x,
		std::int32_t top,
		std::string_view text,
		present::Index4 color,
		present::Rect clip
	) const noexcept;
	[[nodiscard]] bool put_right(
		present::Raster &raster,
		std::int32_t right,
		std::int32_t top,
		std::string_view text,
		present::Index4 color
	) const noexcept;
	// Draws one source-font glyph with its bitmap rows mirrored vertically.
	// Navigation uses this to derive an up caret from the exact lowercase v,
	// keeping both directions identical in width and visual weight.
	[[nodiscard]] bool put_vertical_flipped(
		present::Raster &raster,
		std::int32_t left,
		std::int32_t top,
		char character,
		present::Index4 color
	) const noexcept;
	[[nodiscard]] bool put_cells(
		present::Raster &raster,
		std::int32_t cell_left,
		std::int32_t top,
		std::string_view text,
		present::Index4 color
	) const noexcept;
	// Exact page-1-to-page-0 EGC restore granularity used by mnufont.cpp.
	[[nodiscard]] bool restore_rect(
		present::Raster &raster,
		std::int32_t left,
		std::int32_t top,
		std::uint16_t width,
		std::uint16_t height
	) const;
	// Installs the same exact PC-98 FONT.ROM already authenticated by the
	// gameplay owners. Japanese T3LNG text is CP932 and uses these machine
	// glyphs; printable ASCII continues to use proportional MNUFONT.PF.
	[[nodiscard]] bool install_machine_font(
		std::span<const std::byte> font_rom
	);
	[[nodiscard]] bool has_machine_font() const noexcept;
	[[nodiscard]] bool uses_machine_glyphs(std::string_view text) const noexcept;
	[[nodiscard]] bool install_unicode_font(
		std::span<const std::byte> package
	);
	[[nodiscard]] bool has_unicode_font() const noexcept;
	[[nodiscard]] bool supports_utf8(std::string_view text) const noexcept;

private:
	friend struct NewClassicAssetsAccess;
	std::array<std::uint8_t, glyph_count> advances_{};
	std::array<
		std::array<std::uint8_t, glyph_bytes>, glyph_count
	> bitmaps_{};
	std::vector<std::byte> machine_font_{};
	NewClassicUnicodeFont unicode_font_{};
	[[nodiscard]] static std::size_t glyph_index(std::uint8_t value) noexcept;
	[[nodiscard]] bool put_n_clipped(
		present::Raster &raster,
		std::int32_t left,
		std::int32_t top,
		std::string_view text,
		std::size_t count,
		present::Index4 color,
		present::Rect clip
	) const noexcept;
};

enum class NewClassicMenuBackground : std::uint8_t {
	neutral,
	replay,
	high_score,
	online_profile,
	sample_004,
	netplay,
	report_success,
	report_failure,
	count,
};

struct NewClassicReplayPatchAssets final {
	assets::PiImage replay_list_background{};
	assets::PiImage replay_name_background{};
	std::array<
		assets::PiImage,
		static_cast<std::size_t>(NewClassicMenuBackground::count)
	> new_menu_backgrounds{};
	NewClassicMenuFont menu_font{};
	std::array<std::array<std::uint8_t, 32U>, 40U> pause_gaiji{};
	// Optional developer/translator package. All production New Classic scenes
	// otherwise use the frozen built-in profile selected below.
	std::shared_ptr<const NewClassicMessageCatalog> custom_messages{};
	// Optional source-constrained T3GTL001 catalog. This owns only New Classic
	// graphics text; Classic and authenticated Japanese source text ignore it.
	std::shared_ptr<const NewClassicGameTextCatalog> custom_game_text{};
	// Optional complete T3CTP001 scripted-dialogue catalog. The story owner
	// validates all selected source scripts before publishing a localized run.
	std::shared_ptr<const NewClassicCutsceneTextCatalog> custom_cutscene_text{};
	// Optional source-bound T3PTP001 decoded-picture overlay. Story owners
	// select it by locale; sealed Classic never passes it to a picture scene.
	std::shared_ptr<const NewClassicPicturePatchCatalog> custom_picture_patch{};
	[[nodiscard]] const NewClassicMessageCatalog &messages(
		NewClassicLanguageAssetProfile profile =
			NewClassicLanguageAssetProfile::english
	) const noexcept;
	[[nodiscard]] const assets::PiImage &menu_background(
		NewClassicMenuBackground background
	) const noexcept
	{
		return new_menu_backgrounds[static_cast<std::size_t>(background)];
	}
};

// Graphics-page owner for Replay Patch-native full-screen UI. Unlike the
// product shell canvas, every pixel originates in the authenticated PI/font
// pipeline and remains in the PC-98 graphics palette domain.
class NewClassicPatchSurface final {
public:
	NewClassicPatchSurface();
	[[nodiscard]] bool reset(const assets::PiImage &background) noexcept;
	// Replay Patch full-screen menus compose every new state on the hidden
	// graphics page and publish only after the frame is complete.
	[[nodiscard]] bool begin_hidden_frame(
		const assets::PiImage &background
	) noexcept;
	// Name-entry and dialog input mutate the displayed Replay Patch page in
	// place. The host snapshot remains detached until publish() below.
	[[nodiscard]] bool begin_shown_frame(
		const assets::PiImage &background
	) noexcept;
	[[nodiscard]] std::optional<present::Page> copy_shown_to_hidden() noexcept;
	[[nodiscard]] bool terminal_black_clear() noexcept;
	[[nodiscard]] present::Raster &raster() noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] bool publish(present::Page page = present::Page::zero) noexcept;
	[[nodiscard]] present::SourceSurface surface() const noexcept;

private:
	present::Raster raster_{};
	std::vector<present::ComposedPixel> pixels_{};
	bool initialized_ = false;
};

// Accepts the authenticated Replay Patch source archive or the legacy
// extended development archive. Original-derived members always remain in
// caller-owned AZINN.DAT. Repo-owned custom menu backgrounds are preferentially
// loaded from a sibling PODDUI directory, with embedded extended-archive copies
// retained only for compatibility with older standalone builds.
[[nodiscard]] std::expected<
	NewClassicReplayPatchAssets,
	NewClassicAssetsError
> load_new_classic_replay_patch_assets(
	const std::filesystem::path &path,
	const std::filesystem::path &menu_background_root = {},
	const std::filesystem::path &font_rom = {}
);

namespace detail {

[[nodiscard]] std::expected<NewClassicMenuFont, NewClassicAssetsError>
decode_new_classic_menu_font(std::span<const std::byte> bytes);

[[nodiscard]] std::expected<
	std::array<std::array<std::uint8_t, 32U>, 40U>,
	NewClassicAssetsError
> decode_new_classic_pause_gaiji(std::span<const std::byte> bytes);

// BG2 is host-only artwork, so it deliberately bypasses the source PI
// compressor and its legacy 16-bit archive-member ceiling. The two supplied
// 640x200, 4-bpp BMP halves are joined without resampling. Palette indices 12
// and 15 are removed from background pixels and reserved for yellow selected
// text and white normal text respectively.
[[nodiscard]] std::expected<assets::PiImage, NewClassicAssetsError>
decode_new_classic_menu_background(
	std::span<const std::byte> top,
	std::span<const std::byte> bottom
);

} // namespace detail
} // namespace th03::app

#endif
