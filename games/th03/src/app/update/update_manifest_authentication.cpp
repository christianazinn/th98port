#include "th03/app/update/update_manifest_authentication.hpp"

#include <mbedtls/bignum.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#endif

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

[[nodiscard]] UpdateStagingError staging_error(
	UpdateStagingErrorCode code,
	std::error_code system = {}
) noexcept
{
	return {.code = code, .system = system};
}

[[nodiscard]] std::error_code last_system_error() noexcept
{
#if defined(_WIN32)
	return {static_cast<int>(GetLastError()), std::system_category()};
#else
	return {errno, std::generic_category()};
#endif
}

[[nodiscard]] std::uint64_t process_id() noexcept
{
#if defined(_WIN32)
	return static_cast<std::uint64_t>(GetCurrentProcessId());
#else
	return static_cast<std::uint64_t>(::getpid());
#endif
}

class TemporaryStage final {
public:
#if defined(_WIN32)
	using Handle = HANDLE;
	static constexpr Handle invalid_handle = nullptr;
#else
	using Handle = int;
	static constexpr Handle invalid_handle = -1;
#endif

	TemporaryStage() = default;
	TemporaryStage(std::filesystem::path path, Handle handle) noexcept
		: path_(std::move(path)), handle_(handle)
	{
	}
	TemporaryStage(const TemporaryStage &) = delete;
	TemporaryStage &operator=(const TemporaryStage &) = delete;
	TemporaryStage(TemporaryStage &&other) noexcept
		: path_(std::move(other.path_)),
		  handle_(std::exchange(other.handle_, invalid_handle))
	{
	}
	TemporaryStage &operator=(TemporaryStage &&) = delete;
	~TemporaryStage()
	{
		close_ignoring_error();
		if(!path_.empty()) {
			std::error_code ignored;
			std::filesystem::remove(path_, ignored);
		}
	}

	[[nodiscard]] Handle handle() const noexcept { return handle_; }
	[[nodiscard]] const std::filesystem::path &path() const noexcept
	{
		return path_;
	}
	[[nodiscard]] bool close() noexcept
	{
		if(handle_ == invalid_handle) {
			return true;
		}
#if defined(_WIN32)
		const bool closed = CloseHandle(handle_) != 0;
#else
		const bool closed = ::close(handle_) == 0;
#endif
		handle_ = invalid_handle;
		return closed;
	}
	void mark_published() noexcept { path_.clear(); }

private:
	void close_ignoring_error() noexcept
	{
		if(handle_ == invalid_handle) {
			return;
		}
#if defined(_WIN32)
		(void)CloseHandle(handle_);
#else
		(void)::close(handle_);
#endif
		handle_ = invalid_handle;
	}

	std::filesystem::path path_{};
	Handle handle_ = invalid_handle;
};

[[nodiscard]] std::expected<TemporaryStage, UpdateStagingError>
create_temporary_stage(const std::filesystem::path &target)
{
	static std::atomic<std::uint64_t> next_sequence{0U};
	for(std::size_t attempt = 0U; attempt < 128U; ++attempt) {
		auto candidate = target;
		candidate += std::filesystem::path(
			".tmp-" + std::to_string(process_id()) + "-" +
			std::to_string(next_sequence.fetch_add(
				1U, std::memory_order_relaxed
			))
		).native();
#if defined(_WIN32)
		const auto handle = CreateFileW(
			candidate.c_str(), GENERIC_WRITE, 0U, nullptr, CREATE_NEW,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr
		);
		if(handle != INVALID_HANDLE_VALUE) {
			return TemporaryStage(candidate, handle);
		}
		const auto system = last_system_error();
		if(system.value() != ERROR_FILE_EXISTS &&
			system.value() != ERROR_ALREADY_EXISTS) {
			return std::unexpected(staging_error(
				UpdateStagingErrorCode::temporary_create, system
			));
		}
#else
		const auto handle = ::open(
			candidate.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600
		);
		if(handle >= 0) {
			return TemporaryStage(candidate, handle);
		}
		const auto system = last_system_error();
		if(system.value() != EEXIST) {
			return std::unexpected(staging_error(
				UpdateStagingErrorCode::temporary_create, system
			));
		}
#endif
	}
	return std::unexpected(staging_error(
		UpdateStagingErrorCode::temporary_create,
		std::make_error_code(std::errc::file_exists)
	));
}

[[nodiscard]] std::expected<void, UpdateStagingError> write_stage(
	TemporaryStage &file,
	std::span<const std::byte> bytes
)
{
	std::size_t written = 0U;
	while(written < bytes.size()) {
#if defined(_WIN32)
		DWORD chunk = 0U;
		const auto remaining = static_cast<DWORD>(std::min<std::size_t>(
			bytes.size() - written,
			static_cast<std::size_t>(std::numeric_limits<DWORD>::max())
		));
		if(WriteFile(
			file.handle(), bytes.data() + written, remaining, &chunk, nullptr
		) == 0 || chunk == 0U) {
			return std::unexpected(staging_error(
				UpdateStagingErrorCode::temporary_write, last_system_error()
			));
		}
		written += static_cast<std::size_t>(chunk);
#else
		const auto count = ::write(
			file.handle(), bytes.data() + written, bytes.size() - written
		);
		if(count <= 0) {
			return std::unexpected(staging_error(
				UpdateStagingErrorCode::temporary_write, last_system_error()
			));
		}
		written += static_cast<std::size_t>(count);
#endif
	}
#if defined(_WIN32)
	if(FlushFileBuffers(file.handle()) == 0) {
#else
	if(::fsync(file.handle()) != 0) {
#endif
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::temporary_flush, last_system_error()
		));
	}
	if(!file.close()) {
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::temporary_flush, last_system_error()
		));
	}
	return {};
}

[[nodiscard]] std::expected<std::array<std::uint8_t, 32U>, UpdateStagingError>
file_digest(const std::filesystem::path &path, std::uint64_t expected_size)
{
	std::error_code system;
	const auto size = std::filesystem::file_size(path, system);
	if(system || size != expected_size) {
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::temporary_validation, system
		));
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::temporary_validation,
			std::make_error_code(std::errc::io_error)
		));
	}
	mbedtls_sha256_context context;
	mbedtls_sha256_init(&context);
	int result = mbedtls_sha256_starts(&context, 0);
	std::array<char, 64U * 1024U> buffer{};
	while(result == 0 && input) {
		input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		const auto count = input.gcount();
		if(count > 0) {
			result = mbedtls_sha256_update(
				&context,
				reinterpret_cast<const unsigned char *>(buffer.data()),
				static_cast<std::size_t>(count)
			);
		}
	}
	if(input.bad()) {
		mbedtls_sha256_free(&context);
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::temporary_validation,
			std::make_error_code(std::errc::io_error)
		));
	}
	std::array<std::uint8_t, 32U> digest{};
	if(result == 0) {
		result = mbedtls_sha256_finish(&context, digest.data());
	}
	mbedtls_sha256_free(&context);
	if(result != 0) {
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::temporary_validation
		));
	}
	return digest;
}

[[nodiscard]] std::string staging_filename(const UpdateManifest &manifest)
{
	constexpr char hex[] = "0123456789abcdef";
	std::string digest;
	digest.reserve(16U);
	for(std::size_t index = 0U; index < 8U; ++index) {
		digest.push_back(hex[manifest.artifact_sha256[index] >> 4U]);
		digest.push_back(hex[manifest.artifact_sha256[index] & 0x0FU]);
	}
	return "PODD-UPDATE-" +
		std::to_string(static_cast<unsigned>(manifest.platform)) + "-" +
		std::to_string(static_cast<unsigned>(manifest.channel)) + "-" +
		std::to_string(manifest.sequence) + "-" + digest + ".stage";
}

[[nodiscard]] bool publish_stage(
	TemporaryStage &temporary,
	const std::filesystem::path &target,
	std::error_code &system
) noexcept
{
#if defined(_WIN32)
	if(MoveFileExW(
		temporary.path().c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH
	) == 0) {
		system = last_system_error();
		return false;
	}
#else
	if(::link(temporary.path().c_str(), target.c_str()) != 0) {
		system = last_system_error();
		return false;
	}
	if(::unlink(temporary.path().c_str()) != 0) {
		system = last_system_error();
		return false;
	}
#endif
	temporary.mark_published();
	return true;
}

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

std::expected<std::filesystem::path, UpdateStagingError>
stage_authenticated_update(
	std::span<const std::byte> manifest_bytes,
	std::span<const UpdatePinnedKey> pinned_keys,
	UpdatePlatform platform,
	UpdateChannel channel,
	std::uint64_t current_sequence,
	std::span<const std::byte> artifact_bytes,
	const std::filesystem::path &staging_directory
)
{
	auto authenticated = authenticate_update_manifest(
		manifest_bytes, pinned_keys
	);
	if(!authenticated) {
		return std::unexpected(UpdateStagingError{
			.code = UpdateStagingErrorCode::authentication,
			.authentication = std::move(authenticated.error()),
		});
	}
	const auto disposition = update_disposition(
		*authenticated, platform, channel, current_sequence
	);
	if(disposition == UpdateDisposition::wrong_platform) {
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::wrong_platform
		));
	}
	if(disposition == UpdateDisposition::wrong_channel) {
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::wrong_channel
		));
	}
	if(disposition == UpdateDisposition::current_or_older) {
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::current_or_older
		));
	}
	if(authenticated->artifact_size != artifact_bytes.size()) {
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::artifact_size
		));
	}
	std::array<std::uint8_t, 32U> artifact_digest{};
	const auto artifact = unsigned_bytes(artifact_bytes);
	const auto digest_result = mbedtls_sha256(
		artifact.data(), artifact.size(), artifact_digest.data(), 0
	);
	if(digest_result != 0) {
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::temporary_validation
		));
	}
	if(artifact_digest != authenticated->artifact_sha256) {
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::artifact_digest
		));
	}
	if(staging_directory.empty()) {
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::invalid_directory
		));
	}
	std::error_code system;
	std::filesystem::create_directories(staging_directory, system);
	if(system || !std::filesystem::is_directory(staging_directory, system) ||
		system) {
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::invalid_directory, system
		));
	}
	const auto target = staging_directory / staging_filename(*authenticated);
	if(std::filesystem::exists(target, system)) {
		if(system) {
			return std::unexpected(staging_error(
				UpdateStagingErrorCode::target_conflict, system
			));
		}
		auto digest = file_digest(target, authenticated->artifact_size);
		if(digest && *digest == authenticated->artifact_sha256) {
			return target;
		}
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::target_conflict,
			digest ? std::error_code{} : digest.error().system
		));
	}
	if(system) {
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::target_conflict, system
		));
	}
	auto temporary = create_temporary_stage(target);
	if(!temporary) {
		return std::unexpected(std::move(temporary.error()));
	}
	if(auto written = write_stage(*temporary, artifact_bytes); !written) {
		return std::unexpected(std::move(written.error()));
	}
	auto readback = file_digest(
		temporary->path(), authenticated->artifact_size
	);
	if(!readback || *readback != authenticated->artifact_sha256) {
		return std::unexpected(staging_error(
			UpdateStagingErrorCode::temporary_validation,
			readback ? std::error_code{} : readback.error().system
		));
	}
	if(publish_stage(*temporary, target, system)) {
		return target;
	}
	// A concurrent identical transaction may have won publication. Preserve
	// that target and accept it only after complete size/digest revalidation.
	readback = file_digest(target, authenticated->artifact_size);
	if(readback && *readback == authenticated->artifact_sha256) {
		return target;
	}
	return std::unexpected(staging_error(
		UpdateStagingErrorCode::publish, system
	));
}

} // namespace th03::app
