#ifndef TH03_APP_NEW_CLASSIC_CHARACTER_SELECT_ASSETS_HPP
#define TH03_APP_NEW_CLASSIC_CHARACTER_SELECT_ASSETS_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include "th03/app/main_compiled_content.hpp"
#include "th03/assets/types.hpp"

namespace th03::app {

inline constexpr std::uint16_t new_classic_character_select_stat_width = 56U;
inline constexpr std::uint16_t new_classic_character_select_stat_height = 48U;

enum class NewClassicCharacterSelectAssetsErrorCode : std::uint8_t {
	invalid_path,
	source_status,
	source_not_regular,
	source_size,
	source_open,
	source_read,
	header_shape,
	body_digest_mismatch,
	locale_shape,
	stat_asset_shape,
	asset_decode_failed,
	allocation_failed,
};

struct NewClassicCharacterSelectAssetsError final {
	NewClassicCharacterSelectAssetsErrorCode code =
		NewClassicCharacterSelectAssetsErrorCode::invalid_path;
	std::size_t offset = 0U;
	std::error_code system{};
	std::optional<assets::AssetError> asset_error{};
};

// Source-free fixed-cell replacements for localizable Character Select stat
// labels. Invariant proper names continue through the authenticated CHNAME.BFT
// source path. The package binds its pixels to the exact active CHNAME.BFT,
// SLWIN.CDG, and TLSL.RGB inputs so it cannot silently draw against a different
// source layout or palette.
class NewClassicCharacterSelectAssets final {
public:
	[[nodiscard]] std::string_view locale() const noexcept;
	[[nodiscard]] const assets::PlanarCel4 &stat_labels() const noexcept;
	[[nodiscard]] bool matches_source(
		std::span<const std::byte> names,
		std::span<const std::byte> stats,
		std::span<const std::byte> palette
	) const noexcept;

private:
	friend std::expected<
		NewClassicCharacterSelectAssets,
		NewClassicCharacterSelectAssetsError
	> load_new_classic_character_select_assets(const std::filesystem::path &);

	std::string locale_{};
	AppSha256 source_name_sha256_{};
	AppSha256 source_stats_sha256_{};
	AppSha256 source_palette_sha256_{};
	assets::PlanarCel4 stat_labels_{};
};

[[nodiscard]] std::expected<
	NewClassicCharacterSelectAssets,
	NewClassicCharacterSelectAssetsError
> load_new_classic_character_select_assets(const std::filesystem::path &path);

} // namespace th03::app

#endif
