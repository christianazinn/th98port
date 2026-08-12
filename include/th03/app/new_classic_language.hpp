#ifndef TH03_APP_NEW_CLASSIC_LANGUAGE_HPP
#define TH03_APP_NEW_CLASSIC_LANGUAGE_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>

#include "th03/app/resources.hpp"

namespace th03::app {

enum class NewClassicLanguageOverlayErrorCode : std::uint8_t {
	invalid_path,
	source_status,
	source_not_regular,
	source_size,
	source_open,
	source_read,
	digest_mismatch,
	header_shape,
	directory_shape,
	member_shape,
	member_set,
	allocation_failed,
};

struct NewClassicLanguageOverlayError final {
	NewClassicLanguageOverlayErrorCode code =
		NewClassicLanguageOverlayErrorCode::invalid_path;
	std::error_code system{};
	std::size_t offset = 0U;
};

class NewClassicLanguageOverlay final {
public:
	NewClassicLanguageOverlay() = default;
	[[nodiscard]] std::size_t member_count() const noexcept;
	[[nodiscard]] bool contains(std::string_view name) const noexcept;
	[[nodiscard]] std::map<std::string, ResourceBytes> release_members() &&;

private:
	friend struct NewClassicLanguageOverlayAccess;
	std::map<std::string, ResourceBytes> members_{};
};

// Accepts only the complete Replay Patch 0.4.10 TH3EN.DAT profile. The whole
// archive is authenticated before its encrypted directory or any translated
// payload is consulted.
[[nodiscard]] std::expected<
	NewClassicLanguageOverlay,
	NewClassicLanguageOverlayError
> load_new_classic_language_overlay(const std::filesystem::path &path);

} // namespace th03::app

#endif
