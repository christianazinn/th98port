#ifndef TH03_APP_MAINL_COMPILED_TEXT_INTERNAL_HPP
#define TH03_APP_MAINL_COMPILED_TEXT_INTERNAL_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "classic_mainl_stage_internal.hpp"
#include "th03/app/main_compiled_content.hpp"

namespace th03::app {

enum class MainlExecutableProfile : std::uint8_t {
	unpacked_original_japanese,
};

enum class MainlCompiledTextItem : std::uint8_t {
	none,
	title,
	name,
};

enum class MainlCompiledTextErrorCode : std::uint8_t {
	input_too_short,
	invalid_mz_signature,
	invalid_mz_layout,
	unsupported_profile,
	compiled_range_out_of_bounds,
	malformed_source_text,
};

struct MainlCompiledTextError final {
	MainlCompiledTextErrorCode code =
		MainlCompiledTextErrorCode::input_too_short;
	MainlCompiledTextItem item = MainlCompiledTextItem::none;
	std::uint8_t character_index = 0U;
	// Absolute byte offset in the supplied executable. Diagnostics must never
	// print the source byte stored at this location.
	std::size_t offset = 0U;
};

struct MainlStageTextImport final {
	MainlExecutableProfile profile =
		MainlExecutableProfile::unpacked_original_japanese;
	MainlStageTextAssets assets{};
};

// Authenticates the complete supported unpacked MAINL.EXE profile before
// consulting any private compiled-data range. The returned text owns all
// bytes and remains valid after the caller destroys or overwrites the input.
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
