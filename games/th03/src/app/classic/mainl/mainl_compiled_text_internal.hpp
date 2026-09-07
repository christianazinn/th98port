#ifndef TH03_APP_MAINL_COMPILED_TEXT_INTERNAL_HPP
#define TH03_APP_MAINL_COMPILED_TEXT_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "classic/mainl/classic_mainl_stage_internal.hpp"
#include "th03/app/content/main_compiled_content.hpp"

namespace th03::app {

enum class MainlExecutableProfile : std::uint8_t {
	unpacked_original_japanese,
	packed_original_japanese,
};

enum class MainlCompiledTextItem : std::uint8_t {
	none,
	title,
	name,
	registration_playchar,
	staff_rank,
	staff_number,
	staff_point,
	staff_flake_masks,
};

enum class MainlCompiledTextErrorCode : std::uint8_t {
	input_too_short,
	invalid_mz_signature,
	invalid_mz_layout,
	unsupported_profile,
	packed_decode_failed,
	compiled_range_out_of_bounds,
	malformed_source_text,
};

struct MainlCompiledTextError final {
	MainlCompiledTextErrorCode code =
		MainlCompiledTextErrorCode::input_too_short;
	MainlCompiledTextItem item = MainlCompiledTextItem::none;
	std::uint8_t character_index = 0U;
	// Absolute byte offset in the supplied executable. For packed_decode_failed,
	// this is the decoder's byte-silent packed/IDATA coordinate. Diagnostics
	// must never print the source byte stored at this location.
	std::size_t offset = 0U;
};

struct MainlStaffCompiledAssets final {
	std::array<std::vector<std::uint8_t>, 4U> ranks{};
	std::array<std::vector<std::uint8_t>, 10U> numbers{};
	std::vector<std::uint8_t> point{};
	std::array<std::uint8_t, 64U> flake_masks{};
};

struct MainlStageTextImport final {
	MainlExecutableProfile profile =
		MainlExecutableProfile::unpacked_original_japanese;
	MainlStageTextAssets assets{};
	MainlStaffCompiledAssets staff{};
};

// Authenticates a complete supported packed or unpacked MAINL.EXE profile
// before consulting any private compiled-data range. Packed input is boundedly
// reconstructed and authenticated against the unpacked profile first. The
// returned text and staff masks own all bytes.
[[nodiscard]] std::expected<
	MainlStageTextImport,
	MainlCompiledTextError
> import_mainl_stage_text(std::span<const std::byte> mainl_executable);

namespace detail {

// Hermetic test seam that replaces only the accepted whole-file digest and
// then runs the same MZ/range/text importer. Production never supplies a
// digest or layout descriptor.
[[nodiscard]] std::expected<
	MainlStageTextImport,
	MainlCompiledTextError
> import_mainl_stage_text_for_test(
	std::span<const std::byte> mainl_executable,
	const MainExecutableSha256 &accepted_digest
);

} // namespace detail
} // namespace th03::app

#endif
