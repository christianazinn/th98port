#include "th03/app/replay_slot_store.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "th03/formats/v13.hpp"

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

std::atomic<ReplaySlotStoreFaultPoint> fault_point{
	ReplaySlotStoreFaultPoint::none
};

[[nodiscard]] bool fail(ReplaySlotStoreFaultPoint point) noexcept
{
	auto expected = point;
	return fault_point.compare_exchange_strong(
		expected, ReplaySlotStoreFaultPoint::none, std::memory_order_relaxed
	);
}

[[nodiscard]] ReplaySlotStoreError store_error(
	ReplaySlotStoreErrorCode code,
	std::error_code system = {},
	std::optional<Error> format = std::nullopt
) noexcept
{
	return {.code = code, .system = system, .format = std::move(format)};
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

class NativeTemporaryFile final {
public:
#if defined(_WIN32)
	using Handle = HANDLE;
	static constexpr Handle invalid_handle = nullptr;
#else
	using Handle = int;
	static constexpr Handle invalid_handle = -1;
#endif

	NativeTemporaryFile(std::filesystem::path path, Handle handle) noexcept :
		path_(std::move(path)), handle_(handle)
	{
	}
	NativeTemporaryFile(const NativeTemporaryFile &) = delete;
	NativeTemporaryFile &operator=(const NativeTemporaryFile &) = delete;
	NativeTemporaryFile(NativeTemporaryFile &&other) noexcept :
		path_(std::move(other.path_)),
		handle_(std::exchange(other.handle_, invalid_handle))
	{
	}
	NativeTemporaryFile &operator=(NativeTemporaryFile &&) = delete;
	~NativeTemporaryFile()
	{
		close_ignoring_error();
		if(!published_) {
			std::error_code ignored;
			std::filesystem::remove(path_, ignored);
		}
	}

	[[nodiscard]] Handle handle() const noexcept { return handle_; }
	[[nodiscard]] const std::filesystem::path &path() const noexcept { return path_; }
	[[nodiscard]] bool close() noexcept
	{
		if(handle_ == invalid_handle) {
			return true;
		}
#if defined(_WIN32)
		if(CloseHandle(handle_) == 0) {
			return false;
		}
#else
		if(::close(handle_) != 0) {
			return false;
		}
#endif
		handle_ = invalid_handle;
		return true;
	}
	void mark_published() noexcept { published_ = true; }

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
	bool published_ = false;
};

[[nodiscard]] std::expected<NativeTemporaryFile, ReplaySlotStoreError>
create_temporary(const std::filesystem::path &target)
{
	if(fail(ReplaySlotStoreFaultPoint::temporary_create)) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::temporary_create,
			std::make_error_code(std::errc::io_error)
		));
	}
	static std::atomic<std::uint64_t> sequence{0U};
	for(std::size_t attempt = 0U; attempt < 128U; ++attempt) {
		auto candidate = target;
		candidate += std::filesystem::path(
			".tmp-" + std::to_string(process_id()) + "-" +
			std::to_string(sequence.fetch_add(1U, std::memory_order_relaxed))
		).native();
#if defined(_WIN32)
		const auto handle = CreateFileW(
			candidate.c_str(), GENERIC_READ | GENERIC_WRITE, 0U, nullptr,
			CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr
		);
		if(handle != INVALID_HANDLE_VALUE) {
			return NativeTemporaryFile(candidate, handle);
		}
		const auto system = last_system_error();
		if(system.value() != ERROR_FILE_EXISTS &&
			system.value() != ERROR_ALREADY_EXISTS) {
			return std::unexpected(store_error(
				ReplaySlotStoreErrorCode::temporary_create, system
			));
		}
#else
		const auto handle = ::open(candidate.c_str(), O_CREAT | O_EXCL | O_RDWR, 0666);
		if(handle >= 0) {
			return NativeTemporaryFile(candidate, handle);
		}
		const auto system = last_system_error();
		if(system.value() != EEXIST) {
			return std::unexpected(store_error(
				ReplaySlotStoreErrorCode::temporary_create, system
			));
		}
#endif
	}
	return std::unexpected(store_error(
		ReplaySlotStoreErrorCode::temporary_create,
		std::make_error_code(std::errc::file_exists)
	));
}

[[nodiscard]] std::expected<void, ReplaySlotStoreError> write_all(
	NativeTemporaryFile &file,
	std::span<const std::byte> bytes
)
{
	if(fail(ReplaySlotStoreFaultPoint::temporary_write)) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::temporary_write,
			std::make_error_code(std::errc::io_error)
		));
	}
	std::size_t written = 0U;
	while(written < bytes.size()) {
#if defined(_WIN32)
		DWORD chunk = 0U;
		const auto remaining = static_cast<DWORD>(std::min<std::size_t>(
			bytes.size() - written,
			static_cast<std::size_t>(std::numeric_limits<DWORD>::max())
		));
		if(WriteFile(file.handle(), bytes.data() + written, remaining, &chunk, nullptr) == 0 ||
			chunk == 0U) {
			return std::unexpected(store_error(
				ReplaySlotStoreErrorCode::temporary_write, last_system_error()
			));
		}
		written += static_cast<std::size_t>(chunk);
#else
		const auto count = ::write(
			file.handle(), bytes.data() + written, bytes.size() - written
		);
		if(count <= 0) {
			return std::unexpected(store_error(
				ReplaySlotStoreErrorCode::temporary_write, last_system_error()
			));
		}
		written += static_cast<std::size_t>(count);
#endif
	}
	return {};
}

[[nodiscard]] std::expected<void, ReplaySlotStoreError> flush_and_rewind(
	NativeTemporaryFile &file
)
{
	if(fail(ReplaySlotStoreFaultPoint::temporary_flush)) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::temporary_flush,
			std::make_error_code(std::errc::io_error)
		));
	}
#if defined(_WIN32)
	if(FlushFileBuffers(file.handle()) == 0) {
#else
	if(::fsync(file.handle()) != 0) {
#endif
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::temporary_flush, last_system_error()
		));
	}
	if(fail(ReplaySlotStoreFaultPoint::temporary_seek)) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::temporary_seek,
			std::make_error_code(std::errc::io_error)
		));
	}
#if defined(_WIN32)
	LARGE_INTEGER zero{};
	if(SetFilePointerEx(file.handle(), zero, nullptr, FILE_BEGIN) == 0) {
#else
	if(::lseek(file.handle(), 0, SEEK_SET) < 0) {
#endif
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::temporary_seek, last_system_error()
		));
	}
	return {};
}

[[nodiscard]] std::expected<std::vector<std::byte>, ReplaySlotStoreError>
read_back(NativeTemporaryFile &file, std::size_t size)
{
	if(fail(ReplaySlotStoreFaultPoint::temporary_read)) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::temporary_read,
			std::make_error_code(std::errc::io_error)
		));
	}
	std::vector<std::byte> bytes(size);
	std::size_t read = 0U;
	while(read < bytes.size()) {
#if defined(_WIN32)
		DWORD chunk = 0U;
		const auto remaining = static_cast<DWORD>(std::min<std::size_t>(
			bytes.size() - read,
			static_cast<std::size_t>(std::numeric_limits<DWORD>::max())
		));
		if(ReadFile(file.handle(), bytes.data() + read, remaining, &chunk, nullptr) == 0 ||
			chunk == 0U) {
			return std::unexpected(store_error(
				ReplaySlotStoreErrorCode::temporary_read, last_system_error()
			));
		}
		read += static_cast<std::size_t>(chunk);
#else
		const auto count = ::read(file.handle(), bytes.data() + read, bytes.size() - read);
		if(count <= 0) {
			return std::unexpected(store_error(
				ReplaySlotStoreErrorCode::temporary_read, last_system_error()
			));
		}
		read += static_cast<std::size_t>(count);
#endif
	}
	return bytes;
}

[[nodiscard]] std::expected<void, ReplaySlotStoreError> replace_target(
	NativeTemporaryFile &file,
	const std::filesystem::path &target
)
{
	if(fail(ReplaySlotStoreFaultPoint::replace_target)) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::replace_target,
			std::make_error_code(std::errc::io_error)
		));
	}
#if defined(_WIN32)
	if(MoveFileExW(file.path().c_str(), target.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
#else
	if(::rename(file.path().c_str(), target.c_str()) != 0) {
#endif
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::replace_target, last_system_error()
		));
	}
	file.mark_published();
	return {};
}

[[nodiscard]] std::expected<ReplaySlotLoadResult, ReplaySlotStoreError>
load_document(const std::filesystem::path &path)
{
	std::error_code system;
	const auto status = std::filesystem::status(path, system);
	if(system == std::errc::no_such_file_or_directory ||
		(!system && !std::filesystem::exists(status))) {
		return ReplaySlotLoadResult{};
	}
	if(system) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::source_status, system
		));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::source_not_regular
		));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::source_size, system
		));
	}
	if(size == 0U || size > maximum_replay_file_bytes ||
		size > std::numeric_limits<std::size_t>::max()) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::source_too_large
		));
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(store_error(ReplaySlotStoreErrorCode::source_open));
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	input.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	if(!input || input.peek() != std::ifstream::traits_type::eof()) {
		return std::unexpected(store_error(ReplaySlotStoreErrorCode::source_read));
	}
	auto parsed = formats::parse_v13_replay(bytes);
	if(!parsed) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::format, {}, parsed.error()
		));
	}
	return ReplaySlotLoadResult{.document = std::move(*parsed)};
}

[[nodiscard]] std::expected<void, ReplaySlotStoreError> write_document(
	const std::filesystem::path &replay_root,
	const std::filesystem::path &target,
	const formats::ReplayDocument &document
)
{
	auto encoded = formats::write_v13_replay(document);
	if(!encoded) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::format, {}, encoded.error()
		));
	}
	if(encoded->size() > maximum_replay_file_bytes) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::source_too_large
		));
	}
	if(fail(ReplaySlotStoreFaultPoint::directory_create)) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::directory_create,
			std::make_error_code(std::errc::io_error)
		));
	}
	std::error_code system;
	std::filesystem::create_directories(replay_root, system);
	if(system) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::directory_create, system
		));
	}
	auto temporary = create_temporary(target);
	if(!temporary) {
		return std::unexpected(std::move(temporary.error()));
	}
	if(auto written = write_all(*temporary, *encoded); !written) {
		return std::unexpected(std::move(written.error()));
	}
	if(auto flushed = flush_and_rewind(*temporary); !flushed) {
		return std::unexpected(std::move(flushed.error()));
	}
	auto readback = read_back(*temporary, encoded->size());
	if(!readback) {
		return std::unexpected(std::move(readback.error()));
	}
	auto parsed = formats::parse_v13_replay(*readback);
	if(fail(ReplaySlotStoreFaultPoint::temporary_validation) ||
		*readback != *encoded || !parsed) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::temporary_validation, {},
			parsed ? std::nullopt : std::optional<Error>{parsed.error()}
		));
	}
	if(fail(ReplaySlotStoreFaultPoint::temporary_close)) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::temporary_close,
			std::make_error_code(std::errc::io_error)
		));
	}
	if(!temporary->close()) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::temporary_close, last_system_error()
		));
	}
	return replace_target(*temporary, target);
}

} // namespace

std::filesystem::path replay_slot_path(
	const std::filesystem::path &replay_root,
	std::uint8_t slot
)
{
	const auto tens = static_cast<char>('0' + (slot / 10U));
	const auto ones = static_cast<char>('0' + (slot % 10U));
	std::string filename{"TH3R00.RPY"};
	filename[4] = tens;
	filename[5] = ones;
	return replay_root / filename;
}

std::filesystem::path replay_fallback_path(
	const std::filesystem::path &replay_root
)
{
	return replay_root / "TH3LAST.RPY";
}

std::expected<ReplaySlotLoadResult, ReplaySlotStoreError> load_replay_slot(
	const std::filesystem::path &replay_root,
	std::uint8_t slot
)
{
	if(replay_root.empty()) {
		return std::unexpected(store_error(ReplaySlotStoreErrorCode::invalid_root));
	}
	if(slot >= replay_slot_count) {
		return std::unexpected(store_error(ReplaySlotStoreErrorCode::invalid_slot));
	}
	return load_document(replay_slot_path(replay_root, slot));
}

std::expected<void, ReplaySlotStoreError> commit_replay_slot(
	const std::filesystem::path &replay_root,
	std::uint8_t slot,
	const formats::ReplayDocument &document
)
{
	if(replay_root.empty()) {
		return std::unexpected(store_error(ReplaySlotStoreErrorCode::invalid_root));
	}
	if(slot >= replay_slot_count) {
		return std::unexpected(store_error(ReplaySlotStoreErrorCode::invalid_slot));
	}
	return write_document(
		replay_root, replay_slot_path(replay_root, slot), document
	);
}

std::expected<ReplaySlotLoadResult, ReplaySlotStoreError>
load_replay_fallback(const std::filesystem::path &replay_root)
{
	if(replay_root.empty()) {
		return std::unexpected(store_error(ReplaySlotStoreErrorCode::invalid_root));
	}
	return load_document(replay_fallback_path(replay_root));
}

std::expected<void, ReplaySlotStoreError> checkpoint_replay_fallback(
	const std::filesystem::path &replay_root,
	const formats::ReplayDocument &document
)
{
	if(replay_root.empty()) {
		return std::unexpected(store_error(ReplaySlotStoreErrorCode::invalid_root));
	}
	return write_document(
		replay_root, replay_fallback_path(replay_root), document
	);
}

std::expected<void, ReplaySlotStoreError> remove_replay_fallback(
	const std::filesystem::path &replay_root
)
{
	if(replay_root.empty()) {
		return std::unexpected(store_error(ReplaySlotStoreErrorCode::invalid_root));
	}
	if(fail(ReplaySlotStoreFaultPoint::remove_target)) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::remove_target,
			std::make_error_code(std::errc::io_error)
		));
	}
	std::error_code system;
	static_cast<void>(std::filesystem::remove(
		replay_fallback_path(replay_root), system
	));
	if(system) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::remove_target, system
		));
	}
	return {};
}

std::expected<void, ReplaySlotStoreError> commit_replay_fallback_to_slot(
	const std::filesystem::path &replay_root,
	std::uint8_t slot,
	const formats::ReplayDocument &document
)
{
	if(replay_root.empty()) {
		return std::unexpected(store_error(ReplaySlotStoreErrorCode::invalid_root));
	}
	if(slot >= replay_slot_count) {
		return std::unexpected(store_error(ReplaySlotStoreErrorCode::invalid_slot));
	}
	if(auto staged = checkpoint_replay_fallback(replay_root, document); !staged) {
		return staged;
	}
	const auto fallback = replay_fallback_path(replay_root);
	const auto target = replay_slot_path(replay_root, slot);
	if(fail(ReplaySlotStoreFaultPoint::publish_slot)) {
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::publish_slot,
			std::make_error_code(std::errc::io_error)
		));
	}
#if defined(_WIN32)
	if(MoveFileExW(fallback.c_str(), target.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0) {
#else
	if(::rename(fallback.c_str(), target.c_str()) != 0) {
#endif
		return std::unexpected(store_error(
			ReplaySlotStoreErrorCode::publish_slot, last_system_error()
		));
	}
	return {};
}

void ReplaySlotStoreTestAccess::fail_next(
	ReplaySlotStoreFaultPoint point
) noexcept
{
	fault_point.store(point, std::memory_order_relaxed);
}

void ReplaySlotStoreTestAccess::clear_failure() noexcept
{
	fault_point.store(ReplaySlotStoreFaultPoint::none, std::memory_order_relaxed);
}

} // namespace th03::app
