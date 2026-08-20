#include "th03/app/update_activation_store.hpp"

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
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace th03::app {
namespace {

namespace fs = std::filesystem;

enum class CandidateStatus : std::uint8_t {
	absent,
	valid,
	invalid,
};

struct Candidate final {
	CandidateStatus status = CandidateStatus::absent;
	UpdateActivationState state{};
	std::error_code system{};
	std::optional<UpdateActivationCodecError> codec{};
};

struct Family final {
	std::array<Candidate, 2U> candidate{};
	std::optional<std::size_t> selected{};
	bool recovered = false;
};

[[nodiscard]] UpdateActivationStoreError store_error(
	UpdateActivationStoreErrorCode code,
	std::error_code system = {},
	std::optional<UpdateActivationCodecError> codec = std::nullopt
) noexcept
{
	return {.code = code, .system = system, .codec = std::move(codec)};
}

[[nodiscard]] std::error_code last_system_error() noexcept
{
#if defined(_WIN32)
	return {static_cast<int>(GetLastError()), std::system_category()};
#else
	return {errno, std::generic_category()};
#endif
}

[[nodiscard]] fs::path sibling(const fs::path &path, char suffix)
{
	auto result = path;
	result += fs::path(std::string{"."} + suffix).native();
	return result;
}

class FamilyLock final {
public:
#if defined(_WIN32)
	using Handle = HANDLE;
#else
	using Handle = int;
#endif

	FamilyLock() = default;
	explicit FamilyLock(Handle handle) noexcept : handle_(handle) {}
	FamilyLock(const FamilyLock &) = delete;
	FamilyLock &operator=(const FamilyLock &) = delete;
	FamilyLock(FamilyLock &&other) noexcept : handle_(other.handle_)
	{
		other.handle_ = invalid_handle();
	}
	FamilyLock &operator=(FamilyLock &&) = delete;
	~FamilyLock()
	{
		if(handle_ == invalid_handle()) {
			return;
		}
#if defined(_WIN32)
		(void)CloseHandle(handle_);
#else
		(void)::flock(handle_, LOCK_UN);
		(void)::close(handle_);
#endif
	}

private:
	[[nodiscard]] static Handle invalid_handle() noexcept
	{
#if defined(_WIN32)
		return nullptr;
#else
		return -1;
#endif
	}
	Handle handle_ = invalid_handle();
};

[[nodiscard]] std::expected<FamilyLock, UpdateActivationStoreError>
lock_family(const fs::path &path)
{
	auto lock_path = path;
	lock_path += fs::path(".lock").native();
#if defined(_WIN32)
	const auto handle = CreateFileW(
		lock_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0U, nullptr,
		OPEN_ALWAYS, FILE_ATTRIBUTE_HIDDEN, nullptr
	);
	if(handle == INVALID_HANDLE_VALUE) {
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::lock, last_system_error()
		));
	}
	return FamilyLock(handle);
#else
	const auto handle = ::open(
		lock_path.c_str(), O_CREAT | O_RDWR | O_NOFOLLOW, 0600
	);
	if(handle < 0) {
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::lock, last_system_error()
		));
	}
	if(::flock(handle, LOCK_EX) != 0) {
		const auto system = last_system_error();
		(void)::close(handle);
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::lock, system
		));
	}
	return FamilyLock(handle);
#endif
}

[[nodiscard]] Candidate read_candidate(const fs::path &path)
{
	std::error_code system;
	const auto status = fs::symlink_status(path, system);
	if(system == std::errc::no_such_file_or_directory) {
		return {};
	}
	if(system) {
		return {.status = CandidateStatus::invalid, .system = system};
	}
	if(!fs::is_regular_file(status)) {
		return {.status = CandidateStatus::invalid};
	}
	const auto size = fs::file_size(path, system);
	if(system || size != update_activation_file_bytes) {
		return {.status = CandidateStatus::invalid, .system = system};
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return {
			.status = CandidateStatus::invalid,
			.system = std::make_error_code(std::errc::io_error),
		};
	}
	std::array<std::byte, update_activation_file_bytes> bytes{};
	input.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	if(!input || input.peek() != std::ifstream::traits_type::eof()) {
		return {
			.status = CandidateStatus::invalid,
			.system = std::make_error_code(std::errc::io_error),
		};
	}
	auto decoded = decode_update_activation_state(bytes);
	if(!decoded) {
		return {
			.status = CandidateStatus::invalid,
			.codec = decoded.error(),
		};
	}
	return {.status = CandidateStatus::valid, .state = std::move(*decoded)};
}

[[nodiscard]] std::expected<Family, UpdateActivationStoreError> read_family(
	const fs::path &path
)
{
	Family result{};
	result.candidate[0] = read_candidate(sibling(path, 'a'));
	result.candidate[1] = read_candidate(sibling(path, 'b'));
	const bool present_a =
		result.candidate[0].status != CandidateStatus::absent;
	const bool present_b =
		result.candidate[1].status != CandidateStatus::absent;
	const bool valid_a = result.candidate[0].status == CandidateStatus::valid;
	const bool valid_b = result.candidate[1].status == CandidateStatus::valid;
	if(!valid_a && !valid_b) {
		if(!present_a && !present_b) {
			return result;
		}
		const auto &candidate = present_a ?
			result.candidate[0] : result.candidate[1];
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::corrupt,
			candidate.system, candidate.codec
		));
	}
	if(valid_a && valid_b) {
		const auto revision_a = result.candidate[0].state.revision;
		const auto revision_b = result.candidate[1].state.revision;
		if(revision_a == revision_b &&
			result.candidate[0].state != result.candidate[1].state) {
			return std::unexpected(store_error(
				UpdateActivationStoreErrorCode::ambiguous
			));
		}
		result.selected = revision_b > revision_a ? 1U : 0U;
		return result;
	}
	result.selected = valid_a ? 0U : 1U;
	result.recovered = present_a && present_b;
	return result;
}

class TemporaryFile final {
public:
	explicit TemporaryFile(fs::path path) : path_(std::move(path)) {}
	TemporaryFile(const TemporaryFile &) = delete;
	TemporaryFile &operator=(const TemporaryFile &) = delete;
	~TemporaryFile()
	{
		if(path_.empty()) {
			return;
		}
		std::error_code ignored;
		fs::remove(path_, ignored);
	}
	[[nodiscard]] const fs::path &path() const noexcept { return path_; }
	void published() noexcept { path_.clear(); }

private:
	fs::path path_{};
};

[[nodiscard]] fs::path temporary_path(const fs::path &target)
{
	static std::atomic<std::uint64_t> sequence{0U};
	const auto process =
#if defined(_WIN32)
		static_cast<std::uint64_t>(GetCurrentProcessId());
#else
		static_cast<std::uint64_t>(::getpid());
#endif
	auto result = target;
	result += fs::path(
		".tmp-" + std::to_string(process) + "-" +
		std::to_string(sequence.fetch_add(
			1U, std::memory_order_relaxed
		))
	).native();
	return result;
}

[[nodiscard]] std::expected<void, UpdateActivationStoreError> write_file(
	const fs::path &path,
	std::span<const std::byte> bytes
)
{
#if defined(_WIN32)
	const auto handle = CreateFileW(
		path.c_str(), GENERIC_WRITE, 0U, nullptr, CREATE_NEW,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr
	);
	if(handle == INVALID_HANDLE_VALUE) {
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::temporary_create,
			last_system_error()
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
			return std::unexpected(store_error(
				UpdateActivationStoreErrorCode::write, system
			));
		}
		written += count;
	}
	const bool flushed = FlushFileBuffers(handle) != 0;
	const auto flush_error = flushed ? std::error_code{} : last_system_error();
	const bool closed = CloseHandle(handle) != 0;
	if(!flushed || !closed) {
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::flush,
			flushed ? last_system_error() : flush_error
		));
	}
#else
	const auto handle = ::open(
		path.c_str(), O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, 0600
	);
	if(handle < 0) {
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::temporary_create,
			last_system_error()
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
			return std::unexpected(store_error(
				UpdateActivationStoreErrorCode::write, system
			));
		}
		written += static_cast<std::size_t>(count);
	}
	const bool flushed = ::fsync(handle) == 0;
	const auto flush_error = flushed ? std::error_code{} : last_system_error();
	const bool closed = ::close(handle) == 0;
	if(!flushed || !closed) {
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::flush,
			flushed ? last_system_error() : flush_error
		));
	}
#endif
	return {};
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

[[nodiscard]] bool publish(
	const fs::path &temporary,
	const fs::path &target,
	std::error_code &system
) noexcept
{
#if defined(_WIN32)
	if(MoveFileExW(
		temporary.c_str(), target.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
	) == 0) {
		system = last_system_error();
		return false;
	}
	return true;
#else
	if(::rename(temporary.c_str(), target.c_str()) != 0) {
		system = last_system_error();
		return false;
	}
	return true;
#endif
}

} // namespace

std::expected<UpdateActivationLoadResult, UpdateActivationStoreError>
load_update_activation_state(const fs::path &path)
{
	if(path.empty() || !path.has_filename() || path.parent_path().empty()) {
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::invalid_path
		));
	}
	auto family = read_family(path);
	if(!family) {
		return std::unexpected(std::move(family.error()));
	}
	if(!family->selected) {
		return UpdateActivationLoadResult{};
	}
	return UpdateActivationLoadResult{
		.state = family->candidate[*family->selected].state,
		.file_present = true,
		.recovered_sibling = family->recovered,
	};
}

std::expected<void, UpdateActivationStoreError>
commit_update_activation_state(
	const fs::path &path,
	const UpdateActivationState &state
)
{
	if(path.empty() || !path.has_filename() || path.parent_path().empty()) {
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::invalid_path
		));
	}
	auto encoded = encode_update_activation_state(state);
	if(!encoded) {
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::codec, {}, encoded.error()
		));
	}
	std::error_code system;
	fs::create_directories(path.parent_path(), system);
	if(system) {
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::status, system
		));
	}
	auto lock = lock_family(path);
	if(!lock) {
		return std::unexpected(std::move(lock.error()));
	}
	// The transition may have been computed before this process obtained the
	// family lock. Re-read under the lock and enforce the exact next revision.
	auto family = read_family(path);
	if(!family) {
		return std::unexpected(std::move(family.error()));
	}
	std::uint64_t current_revision = 0U;
	if(family->selected) {
		const auto &current = family->candidate[*family->selected].state;
		if(current == state) {
			return {};
		}
		current_revision = current.revision;
	}
	if(current_revision == std::numeric_limits<std::uint64_t>::max() ||
		state.revision != current_revision + 1U) {
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::stale_revision
		));
	}
	const auto target_index = family->selected ? (1U - *family->selected) : 0U;
	const auto target = sibling(path, target_index == 0U ? 'a' : 'b');
	TemporaryFile temporary(temporary_path(target));
	if(auto written = write_file(temporary.path(), *encoded); !written) {
		return std::unexpected(std::move(written.error()));
	}
	const auto temporary_candidate = read_candidate(temporary.path());
	if(temporary_candidate.status != CandidateStatus::valid ||
		temporary_candidate.state != state) {
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::readback,
			temporary_candidate.system, temporary_candidate.codec
		));
	}
	if(!publish(temporary.path(), target, system)) {
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::publish, system
		));
	}
	temporary.published();
	if(!sync_directory(path.parent_path())) {
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::flush, last_system_error()
		));
	}
	const auto published = read_candidate(target);
	if(published.status != CandidateStatus::valid || published.state != state) {
		return std::unexpected(store_error(
			UpdateActivationStoreErrorCode::readback,
			published.system, published.codec
		));
	}
	return {};
}

} // namespace th03::app
