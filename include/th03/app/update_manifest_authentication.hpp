#ifndef TH03_APP_UPDATE_MANIFEST_AUTHENTICATION_HPP
#define TH03_APP_UPDATE_MANIFEST_AUTHENTICATION_HPP

#include "th03/app/update_manifest.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace th03::app {

struct UpdatePinnedKey final {
	std::array<std::uint8_t, 16U> key_id{};
	// Complete DER SubjectPublicKeyInfo. Storage is borrowed for the duration of
	// authenticate_update_manifest().
	std::span<const std::byte> subject_public_key_info{};
};

enum class UpdateAuthenticationErrorCode : std::uint8_t {
	invalid_manifest,
	unknown_key_id,
	duplicate_key_id,
	invalid_pinned_key_id,
	invalid_public_key,
	wrong_key_type,
	wrong_curve,
	invalid_signature,
	crypto_failure,
};

struct UpdateAuthenticationError final {
	UpdateAuthenticationErrorCode code =
		UpdateAuthenticationErrorCode::invalid_manifest;
	UpdateManifestError manifest_error{};
	// Diagnostic-only provider result. Callers must branch on code, not this
	// implementation detail.
	int provider_result = 0;
};

// Reparses the supplied bytes, selects exactly one caller-pinned key ID,
// validates SHA-256(SPKI)[0..16] against that pin, requires ECDSA P-256, and
// verifies the fixed P1363 signature over the exact parsed signed prefix.
// Network access and artifact download are intentionally outside this owner.
[[nodiscard]] std::expected<UpdateManifest, UpdateAuthenticationError>
authenticate_update_manifest(
	std::span<const std::byte> bytes,
	std::span<const UpdatePinnedKey> pinned_keys
);

} // namespace th03::app

#endif
