#include "th03/app/update_manifest_authentication.hpp"

#include <mbedtls/bignum.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace th03::app {
namespace {

[[nodiscard]] UpdateAuthenticationError error(
	UpdateAuthenticationErrorCode code,
	int provider_result = 0
) noexcept
{
	return {.code = code, .provider_result = provider_result};
}

[[nodiscard]] std::span<const unsigned char> unsigned_bytes(
	std::span<const std::byte> bytes
) noexcept
{
	return {
		reinterpret_cast<const unsigned char *>(bytes.data()), bytes.size()
	};
}

[[nodiscard]] bool allocation_failure(int result) noexcept
{
	return result == MBEDTLS_ERR_MPI_ALLOC_FAILED ||
		result == MBEDTLS_ERR_ECP_ALLOC_FAILED;
}

struct PublicKey final {
	mbedtls_pk_context value;

	PublicKey() noexcept { mbedtls_pk_init(&value); }
	~PublicKey() { mbedtls_pk_free(&value); }
	PublicKey(const PublicKey &) = delete;
	PublicKey &operator=(const PublicKey &) = delete;
};

struct Group final {
	mbedtls_ecp_group value;

	Group() noexcept { mbedtls_ecp_group_init(&value); }
	~Group() { mbedtls_ecp_group_free(&value); }
	Group(const Group &) = delete;
	Group &operator=(const Group &) = delete;
};

struct Point final {
	mbedtls_ecp_point value;

	Point() noexcept { mbedtls_ecp_point_init(&value); }
	~Point() { mbedtls_ecp_point_free(&value); }
	Point(const Point &) = delete;
	Point &operator=(const Point &) = delete;
};

struct Integer final {
	mbedtls_mpi value;

	Integer() noexcept { mbedtls_mpi_init(&value); }
	~Integer() { mbedtls_mpi_free(&value); }
	Integer(const Integer &) = delete;
	Integer &operator=(const Integer &) = delete;
};

} // namespace

std::expected<UpdateManifest, UpdateAuthenticationError>
authenticate_update_manifest(
	std::span<const std::byte> bytes,
	std::span<const UpdatePinnedKey> pinned_keys
)
{
	const auto parsed = parse_update_manifest(bytes);
	if(!parsed) {
		return std::unexpected(UpdateAuthenticationError{
			.code = UpdateAuthenticationErrorCode::invalid_manifest,
			.manifest_error = parsed.error(),
		});
	}

	const UpdatePinnedKey *pin = nullptr;
	for(const auto &candidate : pinned_keys) {
		if(candidate.key_id != parsed->signing_key_id) {
			continue;
		}
		if(pin != nullptr) {
			return std::unexpected(error(
				UpdateAuthenticationErrorCode::duplicate_key_id
			));
		}
		pin = &candidate;
	}
	if(pin == nullptr) {
		return std::unexpected(error(
			UpdateAuthenticationErrorCode::unknown_key_id
		));
	}

	std::array<unsigned char, 32U> public_key_digest{};
	const auto public_key_bytes = unsigned_bytes(pin->subject_public_key_info);
	int provider_result = mbedtls_sha256(
		public_key_bytes.data(), public_key_bytes.size(),
		public_key_digest.data(), 0
	);
	if(provider_result != 0) {
		return std::unexpected(error(
			UpdateAuthenticationErrorCode::crypto_failure, provider_result
		));
	}
	if(!std::ranges::equal(
		pin->key_id,
		std::span<const unsigned char>(public_key_digest).first(pin->key_id.size())
	)) {
		return std::unexpected(error(
			UpdateAuthenticationErrorCode::invalid_pinned_key_id
		));
	}

	PublicKey public_key{};
	provider_result = mbedtls_pk_parse_public_key(
		&public_key.value, public_key_bytes.data(), public_key_bytes.size()
	);
	if(provider_result != 0) {
		return std::unexpected(error(
			UpdateAuthenticationErrorCode::invalid_public_key, provider_result
		));
	}
	if(mbedtls_pk_can_do(&public_key.value, MBEDTLS_PK_ECDSA) == 0) {
		return std::unexpected(error(
			UpdateAuthenticationErrorCode::wrong_key_type
		));
	}
	const auto *elliptic_key = mbedtls_pk_ec(public_key.value);
	if(elliptic_key == nullptr) {
		return std::unexpected(error(
			UpdateAuthenticationErrorCode::wrong_key_type
		));
	}
	if(mbedtls_ecp_keypair_get_group_id(elliptic_key) !=
		MBEDTLS_ECP_DP_SECP256R1) {
		return std::unexpected(error(UpdateAuthenticationErrorCode::wrong_curve));
	}

	Group group{};
	Point point{};
	provider_result = mbedtls_ecp_export(
		elliptic_key, &group.value, nullptr, &point.value
	);
	if(provider_result != 0) {
		return std::unexpected(error(
			UpdateAuthenticationErrorCode::crypto_failure, provider_result
		));
	}

	Integer signature_r{};
	Integer signature_s{};
	provider_result = mbedtls_mpi_read_binary(
		&signature_r.value, parsed->signature.data(), 32U
	);
	if(provider_result == 0) {
		provider_result = mbedtls_mpi_read_binary(
			&signature_s.value, parsed->signature.data() + 32U, 32U
		);
	}
	if(provider_result != 0) {
		return std::unexpected(error(
			UpdateAuthenticationErrorCode::crypto_failure, provider_result
		));
	}

	std::array<unsigned char, 32U> signed_digest{};
	const auto signed_bytes = unsigned_bytes(bytes.first(parsed->signed_bytes));
	provider_result = mbedtls_sha256(
		signed_bytes.data(), signed_bytes.size(), signed_digest.data(), 0
	);
	if(provider_result != 0) {
		return std::unexpected(error(
			UpdateAuthenticationErrorCode::crypto_failure, provider_result
		));
	}
	provider_result = mbedtls_ecdsa_verify(
		&group.value, signed_digest.data(), signed_digest.size(), &point.value,
		&signature_r.value, &signature_s.value
	);
	if(provider_result != 0) {
		return std::unexpected(error(
			allocation_failure(provider_result) ?
				UpdateAuthenticationErrorCode::crypto_failure :
				UpdateAuthenticationErrorCode::invalid_signature,
			provider_result
		));
	}
	return *parsed;
}

} // namespace th03::app
