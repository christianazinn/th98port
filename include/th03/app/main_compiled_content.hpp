#ifndef TH03_APP_MAIN_COMPILED_CONTENT_HPP
#define TH03_APP_MAIN_COMPILED_CONTENT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "th03/present/main_hud_result.hpp"
#include "th03/present/mono.hpp"

namespace th03::app {

using MainExecutableSha256 = std::array<std::uint8_t, 32U>;

// Kept public for profile diagnostics and known-vector tests. Profile
// descriptors, accepted digests, and compiled-data offsets remain private to
// the importer.
[[nodiscard]] MainExecutableSha256 main_executable_sha256(
	std::span<const std::byte> bytes
) noexcept;

enum class MainExecutableProfile : std::uint8_t {
	original_japanese,
};

enum class MainCompiledContentItem : std::uint8_t {
	none,
	combo_hit,
	combo_bonus,
	combo_erase,
	result_max_combo,
	result_gauge_attack,
	result_boss_attack,
	result_boss_attack_reversed,
	result_boss_panic,
	result_total,
	result_winner_bonus,
	result_all_clear_bonus,
	result_player_remaining,
	score_font,
	pellet_masks,
};

enum class MainCompiledContentErrorCode : std::uint8_t {
	input_too_short,
	invalid_mz_signature,
	invalid_mz_layout,
	unsupported_profile,
	compiled_range_out_of_bounds,
	invalid_combo_data,
	malformed_source_text,
};

struct MainCompiledContentError {
	MainCompiledContentErrorCode code =
		MainCompiledContentErrorCode::input_too_short;
	MainCompiledContentItem item = MainCompiledContentItem::none;
	// Absolute byte offset in the supplied executable. Diagnostics must never
	// print the source byte stored at this location.
	std::size_t offset = 0U;
};

// Owning process-lifetime values extracted from one authenticated MAIN.EXE
// profile. Move this object into its final stable storage before constructing
// any presenter from the span-returning accessors below. Moving or destroying
// it afterward invalidates those presenter-retained spans.
class MainCompiledContent {
public:
	[[nodiscard]] MainExecutableProfile profile() const noexcept;
	[[nodiscard]] present::ScoreFontMasks score_font_masks() const noexcept;
	[[nodiscard]] present::PelletMasks pellet_masks() const noexcept;
	[[nodiscard]] present::MainHudResultTramAssets
		hud_result_tram_assets() const noexcept;

private:
	friend struct MainCompiledContentImportAccess;
	friend std::expected<MainCompiledContent, MainCompiledContentError>
	import_main_compiled_content(std::span<const std::byte> main_executable) noexcept;

	MainCompiledContent() = default;

	MainExecutableProfile profile_ = MainExecutableProfile::original_japanese;
	std::array<std::uint8_t, present::score_font_mask_bytes> score_font_{};
	std::array<std::uint8_t, present::pellet_mask_bytes> pellet_masks_{};
	std::array<
		std::array<present::TramCell, 32U>,
		present::main_result_label_count
	> result_labels_{};
	std::array<present::TramCell, 4U> combo_hit_{};
	std::array<present::TramCell, 8U> combo_bonus_{};
	std::array<present::TramCell, 8U> combo_erase_{};
};

// Validates the MZ container, exact supported profile size and SHA-256, every
// fixed compiled-data range, and source text/gaiji structure before returning
// an owning value. Failure publishes no borrowed view or partially initialized
// object.
[[nodiscard]] std::expected<MainCompiledContent, MainCompiledContentError>
import_main_compiled_content(
	std::span<const std::byte> main_executable
) noexcept;

} // namespace th03::app

#endif
