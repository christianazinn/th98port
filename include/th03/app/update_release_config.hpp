#ifndef TH03_APP_UPDATE_RELEASE_CONFIG_HPP
#define TH03_APP_UPDATE_RELEASE_CONFIG_HPP

#include "th03/app/update_manifest_authentication.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace th03::app {

struct UpdateReleaseConfiguration final {
	std::string_view manifest_url{};
	UpdatePlatform platform = UpdatePlatform::windows_standalone;
	UpdateChannel channel = UpdateChannel::beta;
	std::uint64_t installed_sequence = 0U;
	std::span<const UpdatePinnedKey> pinned_keys{};
};

// The trust root is valid only when every field is present, the URL is an
// absolute credential-free HTTPS URL, the installed release sequence is
// nonzero, and every nonempty pin ID is unique. Cryptographic SPKI validation
// remains in authenticate_update_manifest().
[[nodiscard]] bool valid_update_release_configuration(
	const UpdateReleaseConfiguration &configuration
) noexcept;

// Ordinary builds return nullopt. A release build can enable this only by
// supplying every authenticated CMake input; runtime files and environment
// variables can never replace the compiled trust root.
[[nodiscard]] const std::optional<UpdateReleaseConfiguration> &
compiled_update_release_configuration() noexcept;

} // namespace th03::app

#endif
