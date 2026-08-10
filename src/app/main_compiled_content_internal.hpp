#ifndef TH03_APP_MAIN_COMPILED_CONTENT_INTERNAL_HPP
#define TH03_APP_MAIN_COMPILED_CONTENT_INTERNAL_HPP

#include <expected>
#include <span>

#include "th03/app/main_compiled_content.hpp"

namespace th03::app::detail {

// Hermetic test seam that replaces only the accepted digest, then runs the
// same container and compiled-content parser and returns the same owning
// value. Production startup never supplies a digest; it calls the public
// importer, whose accepted profile is fixed in main_compiled_content.cpp.
[[nodiscard]] std::expected<MainCompiledContent, MainCompiledContentError>
import_main_compiled_content_for_test(
	std::span<const std::byte> main_executable,
	const MainExecutableSha256 &accepted_digest
) noexcept;

} // namespace th03::app::detail

#endif
