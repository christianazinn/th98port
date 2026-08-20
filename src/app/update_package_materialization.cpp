#include "th03/app/update_package_materialization.hpp"
#include "th03/app/update_activation.hpp"

#include <mbedtls/sha256.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <stdio.h>
#else
#include <linux/fs.h>
#include <sys/syscall.h>
#endif
#endif

namespace th03::app {
namespace {

namespace fs = std::filesystem;

[[nodiscard]] UpdateMaterializationError materialization_error(
	UpdateMaterializationErrorCode code,
	std::error_code system = {},
	std::size_t member = 0U
) noexcept
{
	return {.code = code, .member = member, .system = system};
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

[[nodiscard]] std::optional<std::array<std::uint8_t, 32U>> digest_bytes(
	std::span<const std::byte> bytes
) noexcept
{
	std::array<std::uint8_t, 32U> digest{};
	const auto result = mbedtls_sha256(
		reinterpret_cast<const unsigned char *>(bytes.data()), bytes.size(),
		digest.data(), 0
	);
	return result == 0 ? std::optional{digest} : std::nullopt;
}

[[nodiscard]] std::optional<std::array<std::uint8_t, 32U>> digest_file(
	const fs::path &path,
	std::uint64_t expected_size,
	std::error_code &system
)
{
	const auto size = fs::file_size(path, system);
	if(system || size != expected_size) {
		return std::nullopt;
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		system = std::make_error_code(std::errc::io_error);
		return std::nullopt;
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
		result = -1;
		system = std::make_error_code(std::errc::io_error);
	}
	std::array<std::uint8_t, 32U> digest{};
	if(result == 0) {
		result = mbedtls_sha256_finish(&context, digest.data());
	}
	mbedtls_sha256_free(&context);
	if(result != 0) {
		return std::nullopt;
	}
	return digest;
}

class TemporaryGeneration final {
public:
	TemporaryGeneration() = default;
	TemporaryGeneration(fs::path root, fs::path path) noexcept
		: root_(std::move(root)), path_(std::move(path))
	{
	}
	TemporaryGeneration(const TemporaryGeneration &) = delete;
	TemporaryGeneration &operator=(const TemporaryGeneration &) = delete;
	TemporaryGeneration(TemporaryGeneration &&other) noexcept
		: root_(std::move(other.root_)), path_(std::move(other.path_))
	{
		other.path_.clear();
	}
	TemporaryGeneration &operator=(TemporaryGeneration &&) = delete;
	~TemporaryGeneration()
	{
		if(path_.empty()) {
			return;
		}
		std::error_code ignored;
		const auto root = fs::absolute(root_, ignored).lexically_normal();
		if(ignored) {
			return;
		}
		const auto path = fs::absolute(path_, ignored).lexically_normal();
		if(ignored || path.parent_path() != root ||
			!path.filename().string().starts_with(".PODD-GEN-TMP-")) {
			return;
		}
		fs::remove_all(path, ignored);
	}
	[[nodiscard]] const fs::path &path() const noexcept { return path_; }
	void mark_published() noexcept { path_.clear(); }

private:
	fs::path root_{};
	fs::path path_{};
};

[[nodiscard]] std::expected<TemporaryGeneration, UpdateMaterializationError>
create_temporary_generation(const fs::path &root)
{
	static std::atomic<std::uint64_t> next_sequence{0U};
	for(std::size_t attempt = 0U; attempt < 128U; ++attempt) {
		const auto name = ".PODD-GEN-TMP-" + std::to_string(process_id()) +
			"-" + std::to_string(next_sequence.fetch_add(
				1U, std::memory_order_relaxed
			));
		const auto candidate = root / name;
		std::error_code system;
		if(fs::create_directory(candidate, system)) {
#if !defined(_WIN32)
			if(::chmod(candidate.c_str(), S_IRWXU) != 0) {
				const auto permission_error = last_system_error();
				std::error_code ignored;
				fs::remove(candidate, ignored);
				return std::unexpected(materialization_error(
					UpdateMaterializationErrorCode::permissions,
					permission_error
				));
			}
#endif
			return TemporaryGeneration(root, candidate);
		}
		if(system != std::errc::file_exists) {
			return std::unexpected(materialization_error(
				UpdateMaterializationErrorCode::temporary_create, system
			));
		}
	}
	return std::unexpected(materialization_error(
		UpdateMaterializationErrorCode::temporary_create,
		std::make_error_code(std::errc::file_exists)
	));
}

[[nodiscard]] std::expected<void, UpdateMaterializationError> write_member(
	const fs::path &path,
	std::span<const std::byte> bytes,
	bool executable,
	std::size_t member
)
{
#if defined(_WIN32)
	const auto handle = CreateFileW(
		path.c_str(), GENERIC_WRITE, 0U, nullptr, CREATE_NEW,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr
	);
	if(handle == INVALID_HANDLE_VALUE) {
		return std::unexpected(materialization_error(
			UpdateMaterializationErrorCode::file_create, last_system_error(),
			member
		));
	}
	std::size_t written = 0U;
	while(written < bytes.size()) {
		DWORD count = 0U;
		const auto remaining = static_cast<DWORD>(std::min<std::size_t>(
			bytes.size() - written,
			static_cast<std::size_t>(std::numeric_limits<DWORD>::max())
		));
		if(WriteFile(
			handle, bytes.data() + written, remaining, &count, nullptr
		) == 0 || count == 0U) {
			const auto system = last_system_error();
			(void)CloseHandle(handle);
			return std::unexpected(materialization_error(
				UpdateMaterializationErrorCode::file_write, system, member
			));
		}
		written += count;
	}
	const bool flushed = FlushFileBuffers(handle) != 0;
	const auto flush_error = flushed ? std::error_code{} : last_system_error();
	const bool closed = CloseHandle(handle) != 0;
	if(!flushed || !closed) {
		return std::unexpected(materialization_error(
			UpdateMaterializationErrorCode::file_flush,
			flushed ? last_system_error() : flush_error,
			member
		));
	}
	(void)executable;
#else
	const auto mode = static_cast<mode_t>(executable ? 0700 : 0600);
	const auto handle = ::open(
		path.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, mode
	);
	if(handle < 0) {
		return std::unexpected(materialization_error(
			UpdateMaterializationErrorCode::file_create, last_system_error(),
			member
		));
	}
	std::size_t written = 0U;
	while(written < bytes.size()) {
		const auto count = ::write(
			handle, bytes.data() + written, bytes.size() - written
		);
		if(count <= 0) {
			const auto system = last_system_error();
			(void)::close(handle);
			return std::unexpected(materialization_error(
				UpdateMaterializationErrorCode::file_write, system, member
			));
		}
		written += static_cast<std::size_t>(count);
	}
	if(::fchmod(handle, mode) != 0) {
		const auto system = last_system_error();
		(void)::close(handle);
		return std::unexpected(materialization_error(
			UpdateMaterializationErrorCode::permissions, system, member
		));
	}
	const bool flushed = ::fsync(handle) == 0;
	const auto flush_error = flushed ? std::error_code{} : last_system_error();
	const bool closed = ::close(handle) == 0;
	if(!flushed || !closed) {
		return std::unexpected(materialization_error(
			UpdateMaterializationErrorCode::file_flush,
			flushed ? last_system_error() : flush_error,
			member
		));
	}
#endif
	return {};
}

[[nodiscard]] std::set<std::string> expected_directories(
	const UpdatePackage &package
)
{
	std::set<std::string> result;
	for(const auto &entry : package.entries) {
		auto path = fs::path(entry.path).parent_path();
		while(!path.empty()) {
			result.insert(path.generic_string());
			path = path.parent_path();
		}
	}
	return result;
}

[[nodiscard]] bool validate_generation(
	const fs::path &target,
	const UpdatePackage &package,
	std::error_code &system
)
{
	const auto target_status = fs::symlink_status(target, system);
	if(system || !fs::is_directory(target_status)) {
		return false;
	}
	std::set<std::string> expected_files;
	for(const auto &entry : package.entries) {
		expected_files.insert(entry.path);
		const auto member_path = target / fs::path(entry.path);
		const auto status = fs::symlink_status(member_path, system);
		if(system || !fs::is_regular_file(status)) {
			return false;
		}
		auto digest = digest_file(member_path, entry.size, system);
		if(!digest || *digest != entry.sha256) {
			return false;
		}
#if !defined(_WIN32)
		const auto expected = entry.executable ?
			(fs::perms::owner_read | fs::perms::owner_write |
				fs::perms::owner_exec) :
			(fs::perms::owner_read | fs::perms::owner_write);
		if((status.permissions() & fs::perms::mask) != expected) {
			return false;
		}
#endif
	}
	const auto directories = expected_directories(package);
	for(fs::recursive_directory_iterator iterator(target, system), end;
		!system && iterator != end; ++iterator) {
		const auto relative = iterator->path().lexically_relative(target);
		const auto key = relative.generic_string();
		const auto status = iterator->symlink_status(system);
		if(system || (fs::is_regular_file(status) ?
			!expected_files.contains(key) :
			!fs::is_directory(status) || !directories.contains(key))) {
			return false;
		}
#if !defined(_WIN32)
		if(fs::is_directory(status) &&
			(status.permissions() & fs::perms::mask) !=
				(fs::perms::owner_read | fs::perms::owner_write |
					fs::perms::owner_exec)) {
			return false;
		}
#endif
	}
	return !system;
}

[[nodiscard]] bool sync_directory(const fs::path &path) noexcept
{
#if defined(_WIN32)
	(void)path;
	return true;
#else
	const auto handle = ::open(path.c_str(), O_RDONLY | O_DIRECTORY);
	if(handle < 0) {
		return false;
	}
	const bool result = ::fsync(handle) == 0;
	(void)::close(handle);
	return result;
#endif
}

[[nodiscard]] bool publish_generation(
	const fs::path &temporary,
	const fs::path &target,
	std::error_code &system
) noexcept
{
#if defined(_WIN32)
	if(MoveFileExW(
		temporary.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH
	) == 0) {
		system = last_system_error();
		return false;
	}
	return true;
#elif defined(__APPLE__)
	if(renamex_np(temporary.c_str(), target.c_str(), RENAME_EXCL) != 0) {
		system = last_system_error();
		return false;
	}
	return true;
#else
	if(::syscall(
		SYS_renameat2, AT_FDCWD, temporary.c_str(), AT_FDCWD,
		target.c_str(), RENAME_NOREPLACE
	) != 0) {
		system = last_system_error();
		return false;
	}
	return true;
#endif
}

} // namespace

std::expected<fs::path, UpdateMaterializationError>
materialize_authenticated_update_package(
	std::span<const std::byte> manifest_bytes,
	std::span<const UpdatePinnedKey> pinned_keys,
	UpdatePlatform platform,
	UpdateChannel channel,
	std::uint64_t current_sequence,
	std::span<const std::byte> package_bytes,
	const fs::path &generation_root
)
{
	auto authenticated = authenticate_update_manifest(
		manifest_bytes, pinned_keys
	);
	if(!authenticated) {
		return std::unexpected(UpdateMaterializationError{
			.code = UpdateMaterializationErrorCode::authentication,
			.authentication = std::move(authenticated.error()),
		});
	}
	const auto disposition = update_disposition(
		*authenticated, platform, channel, current_sequence
	);
	if(disposition == UpdateDisposition::wrong_platform) {
		return std::unexpected(materialization_error(
			UpdateMaterializationErrorCode::wrong_platform
		));
	}
	if(disposition == UpdateDisposition::wrong_channel) {
		return std::unexpected(materialization_error(
			UpdateMaterializationErrorCode::wrong_channel
		));
	}
	if(disposition == UpdateDisposition::current_or_older) {
		return std::unexpected(materialization_error(
			UpdateMaterializationErrorCode::current_or_older
		));
	}
	if(authenticated->artifact_size != package_bytes.size()) {
		return std::unexpected(materialization_error(
			UpdateMaterializationErrorCode::artifact_size
		));
	}
	const auto artifact_digest = digest_bytes(package_bytes);
	if(!artifact_digest || *artifact_digest != authenticated->artifact_sha256) {
		return std::unexpected(materialization_error(
			UpdateMaterializationErrorCode::artifact_digest
		));
	}
	const auto package = parse_update_package(package_bytes);
	if(!package) {
		return std::unexpected(UpdateMaterializationError{
			.code = UpdateMaterializationErrorCode::package,
			.package = package.error(),
		});
	}
	for(std::size_t index = 0U; index < package->entries.size(); ++index) {
		const auto &entry = package->entries[index];
		const auto member = package_bytes.subspan(
			static_cast<std::size_t>(entry.payload_offset),
			static_cast<std::size_t>(entry.size)
		);
		const auto digest = digest_bytes(member);
		if(!digest || *digest != entry.sha256) {
			return std::unexpected(materialization_error(
				UpdateMaterializationErrorCode::member_digest, {}, index
			));
		}
	}
	if(generation_root.empty()) {
		return std::unexpected(materialization_error(
			UpdateMaterializationErrorCode::invalid_directory
		));
	}
	std::error_code system;
	fs::create_directories(generation_root, system);
	const auto root_status = fs::symlink_status(generation_root, system);
	if(system || !fs::is_directory(root_status)) {
		return std::unexpected(materialization_error(
			UpdateMaterializationErrorCode::invalid_directory, system
		));
	}
	const UpdateGenerationIdentity identity{
		.sequence = authenticated->sequence,
		.artifact_sha256 = authenticated->artifact_sha256,
	};
	const auto target = generation_root /
		update_generation_directory_name(identity);
	if(fs::exists(target, system)) {
		if(system || !validate_generation(target, *package, system)) {
			return std::unexpected(materialization_error(
				UpdateMaterializationErrorCode::target_conflict, system
			));
		}
		return target;
	}
	if(system) {
		return std::unexpected(materialization_error(
			UpdateMaterializationErrorCode::target_conflict, system
		));
	}
	auto temporary = create_temporary_generation(generation_root);
	if(!temporary) {
		return std::unexpected(std::move(temporary.error()));
	}
	for(std::size_t index = 0U; index < package->entries.size(); ++index) {
		const auto &entry = package->entries[index];
		const auto path = temporary->path() / fs::path(entry.path);
		fs::create_directories(path.parent_path(), system);
		if(system) {
			return std::unexpected(materialization_error(
				UpdateMaterializationErrorCode::directory_create, system, index
			));
		}
#if !defined(_WIN32)
		if(::chmod(path.parent_path().c_str(), S_IRWXU) != 0) {
			return std::unexpected(materialization_error(
				UpdateMaterializationErrorCode::permissions,
				last_system_error(), index
			));
		}
#endif
		const auto member = package_bytes.subspan(
			static_cast<std::size_t>(entry.payload_offset),
			static_cast<std::size_t>(entry.size)
		);
		if(auto written = write_member(
			path, member, entry.executable, index
		); !written) {
			return std::unexpected(std::move(written.error()));
		}
		auto digest = digest_file(path, entry.size, system);
		if(!digest || *digest != entry.sha256) {
			return std::unexpected(materialization_error(
				UpdateMaterializationErrorCode::file_validation, system, index
			));
		}
	}
	const auto directories = expected_directories(*package);
#if !defined(_WIN32)
	for(const auto &directory : directories) {
		if(::chmod(
			(temporary->path() / fs::path(directory)).c_str(), S_IRWXU
		) != 0) {
			return std::unexpected(materialization_error(
				UpdateMaterializationErrorCode::permissions,
				last_system_error()
			));
		}
	}
#endif
	for(auto iterator = directories.rbegin(); iterator != directories.rend();
		++iterator) {
		if(!sync_directory(temporary->path() / fs::path(*iterator))) {
			return std::unexpected(materialization_error(
				UpdateMaterializationErrorCode::file_flush,
				last_system_error()
			));
		}
	}
	if(!sync_directory(temporary->path())) {
		return std::unexpected(materialization_error(
			UpdateMaterializationErrorCode::file_flush, last_system_error()
		));
	}
	if(!publish_generation(temporary->path(), target, system)) {
		if((system == std::errc::file_exists ||
			system == std::errc::directory_not_empty) &&
			validate_generation(target, *package, system)) {
			return target;
		}
		return std::unexpected(materialization_error(
			UpdateMaterializationErrorCode::publish, system
		));
	}
	temporary->mark_published();
	if(!sync_directory(generation_root)) {
		return std::unexpected(materialization_error(
			UpdateMaterializationErrorCode::publish, last_system_error()
		));
	}
	return target;
}

} // namespace th03::app
