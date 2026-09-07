#include "th03/app/classic/persistence/classic_config_store.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
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

[[nodiscard]] ClassicConfigStoreError store_error(
	ClassicConfigStoreErrorCode code,
	std::error_code system = {},
	std::optional<ClassicConfigError> codec = std::nullopt
) noexcept
{
	return {.code = code, .system = system, .codec = std::move(codec)};
}

[[nodiscard]] std::error_code last_system_error() noexcept
{
#if defined(_WIN32)
	return {
		static_cast<int>(GetLastError()),
		std::system_category(),
	};
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

	NativeTemporaryFile() = default;
	NativeTemporaryFile(std::filesystem::path path, Handle handle) noexcept
		: path_(std::move(path)), handle_(handle)
	{
	}
	NativeTemporaryFile(const NativeTemporaryFile &) = delete;
	NativeTemporaryFile &operator=(const NativeTemporaryFile &) = delete;
	NativeTemporaryFile(NativeTemporaryFile &&other) noexcept
		: path_(std::move(other.path_)),
		  handle_(std::exchange(other.handle_, invalid_handle))
	{
	}
	NativeTemporaryFile &operator=(NativeTemporaryFile &&) = delete;
	~NativeTemporaryFile()
	{
		close_ignoring_error();
		if(!published_ && !path_.empty()) {
			std::error_code ignored;
			std::filesystem::remove(path_, ignored);
		}
	}

	[[nodiscard]] Handle handle() const noexcept
	{
		return handle_;
	}
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
	void mark_published() noexcept
	{
		published_ = true;
		path_.clear();
	}

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

[[nodiscard]] std::filesystem::path temporary_candidate(
	const std::filesystem::path &target,
	std::uint64_t sequence
)
{
	auto candidate = target;
	const auto suffix = std::filesystem::path(
		".tmp-" + std::to_string(process_id()) + "-" +
		std::to_string(sequence)
	);
	candidate += suffix.native();
	return candidate;
}

[[nodiscard]] std::expected<NativeTemporaryFile, ClassicConfigStoreError>
create_temporary(const std::filesystem::path &target)
{
	static std::atomic<std::uint64_t> next_sequence{0U};
	for(std::size_t attempt = 0U; attempt < 128U; ++attempt) {
		const auto candidate = temporary_candidate(
			target,
			next_sequence.fetch_add(1U, std::memory_order_relaxed)
		);
#if defined(_WIN32)
		const auto handle = CreateFileW(
			candidate.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			0U,
			nullptr,
			CREATE_NEW,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
			nullptr
		);
		if(handle != INVALID_HANDLE_VALUE) {
			return NativeTemporaryFile(candidate, handle);
		}
		const auto system = last_system_error();
		if(system.value() != ERROR_FILE_EXISTS &&
			system.value() != ERROR_ALREADY_EXISTS) {
			return std::unexpected(store_error(
				ClassicConfigStoreErrorCode::temporary_create, system
			));
		}
#else
		const auto handle = ::open(
			candidate.c_str(), O_CREAT | O_EXCL | O_RDWR, 0666
		);
		if(handle >= 0) {
			return NativeTemporaryFile(candidate, handle);
		}
		const auto system = last_system_error();
		if(system.value() != EEXIST) {
			return std::unexpected(store_error(
				ClassicConfigStoreErrorCode::temporary_create, system
			));
		}
#endif
	}
	return std::unexpected(store_error(
		ClassicConfigStoreErrorCode::temporary_create,
		std::make_error_code(std::errc::file_exists)
	));
}

[[nodiscard]] std::expected<void, ClassicConfigStoreError> write_all(
	NativeTemporaryFile &file,
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
			return std::unexpected(store_error(
				ClassicConfigStoreErrorCode::temporary_write,
				last_system_error()
			));
		}
		written += static_cast<std::size_t>(chunk);
#else
		const auto count = ::write(
			file.handle(), bytes.data() + written, bytes.size() - written
		);
		if(count <= 0) {
			return std::unexpected(store_error(
				ClassicConfigStoreErrorCode::temporary_write,
				last_system_error()
			));
		}
		written += static_cast<std::size_t>(count);
#endif
	}
	return {};
}

[[nodiscard]] std::expected<void, ClassicConfigStoreError> flush_file(
	NativeTemporaryFile &file
)
{
#if defined(_WIN32)
	if(FlushFileBuffers(file.handle()) == 0) {
#else
	if(::fsync(file.handle()) != 0) {
#endif
		return std::unexpected(store_error(
			ClassicConfigStoreErrorCode::temporary_flush,
			last_system_error()
		));
	}
	return {};
}

[[nodiscard]] std::expected<void, ClassicConfigStoreError> rewind_file(
	NativeTemporaryFile &file
)
{
#if defined(_WIN32)
	LARGE_INTEGER zero{};
	if(SetFilePointerEx(file.handle(), zero, nullptr, FILE_BEGIN) == 0) {
#else
	if(::lseek(file.handle(), 0, SEEK_SET) < 0) {
#endif
		return std::unexpected(store_error(
			ClassicConfigStoreErrorCode::temporary_seek,
			last_system_error()
		));
	}
	return {};
}

[[nodiscard]] std::expected<
	std::array<std::byte, classic_config_file_bytes>,
	ClassicConfigStoreError
> read_back(NativeTemporaryFile &file)
{
	auto bytes = std::array<std::byte, classic_config_file_bytes>{};
	std::size_t read = 0U;
	while(read < bytes.size()) {
#if defined(_WIN32)
		DWORD chunk = 0U;
		if(ReadFile(
			file.handle(),
			bytes.data() + read,
			static_cast<DWORD>(bytes.size() - read),
			&chunk,
			nullptr
		) == 0 || chunk == 0U) {
			return std::unexpected(store_error(
				ClassicConfigStoreErrorCode::temporary_read,
				last_system_error()
			));
		}
		read += static_cast<std::size_t>(chunk);
#else
		const auto count = ::read(
			file.handle(), bytes.data() + read, bytes.size() - read
		);
		if(count <= 0) {
			return std::unexpected(store_error(
				ClassicConfigStoreErrorCode::temporary_read,
				last_system_error()
			));
		}
		read += static_cast<std::size_t>(count);
#endif
	}
	return bytes;
}

[[nodiscard]] std::expected<void, ClassicConfigStoreError> replace_target(
	NativeTemporaryFile &temporary,
	const std::filesystem::path &target
)
{
#if defined(_WIN32)
	if(MoveFileExW(
		temporary.path().c_str(),
		target.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
	) == 0) {
#else
	if(::rename(temporary.path().c_str(), target.c_str()) != 0) {
#endif
		return std::unexpected(store_error(
			ClassicConfigStoreErrorCode::replace_target,
			last_system_error()
		));
	}
	temporary.mark_published();
	return {};
}

[[nodiscard]] constexpr ClassicConfigError input_size_error(
	std::size_t size = classic_config_file_bytes
) noexcept
{
	return {.code = ClassicConfigErrorCode::input_size, .offset = size};
}

} // namespace

std::expected<void, ClassicConfigStoreError> commit_classic_config_store(
	const std::filesystem::path &path,
	ClassicConfigOptions options
)
{
	if(path.empty() || !path.has_filename()) {
		return std::unexpected(store_error(
			ClassicConfigStoreErrorCode::invalid_path
		));
	}
	auto encoded = encode_classic_config_file(classic_config_document(options));
	if(!encoded) {
		return std::unexpected(store_error(
			ClassicConfigStoreErrorCode::codec, {}, encoded.error()
		));
	}
	auto temporary = create_temporary(path);
	if(!temporary) {
		return std::unexpected(std::move(temporary.error()));
	}
	if(auto written = write_all(*temporary, *encoded); !written) {
		return std::unexpected(std::move(written.error()));
	}
	if(auto flushed = flush_file(*temporary); !flushed) {
		return std::unexpected(std::move(flushed.error()));
	}
	if(auto rewound = rewind_file(*temporary); !rewound) {
		return std::unexpected(std::move(rewound.error()));
	}
	auto readback = read_back(*temporary);
	if(!readback) {
		return std::unexpected(std::move(readback.error()));
	}
	auto decoded = decode_classic_config_file(*readback);
	if(*readback != *encoded || !decoded ||
		*decoded != classic_config_document(options)) {
		return std::unexpected(store_error(
			ClassicConfigStoreErrorCode::temporary_validation,
			{},
			decoded ? std::nullopt :
				std::optional<ClassicConfigError>{decoded.error()}
		));
	}
	if(!temporary->close()) {
		return std::unexpected(store_error(
			ClassicConfigStoreErrorCode::temporary_close,
			last_system_error()
		));
	}
	return replace_target(*temporary, path);
}

std::expected<ClassicConfigStoreLoadResult, ClassicConfigStoreError>
load_classic_config_store(
	const std::filesystem::path &path,
	ClassicConfigOptions defaults
)
{
	if(path.empty() || !path.has_filename()) {
		return std::unexpected(store_error(
			ClassicConfigStoreErrorCode::invalid_path
		));
	}
	if(const auto encoded = encode_classic_config_file(
		classic_config_document(defaults)
	); !encoded) {
		return std::unexpected(store_error(
			ClassicConfigStoreErrorCode::codec,
			{},
			encoded.error()
		));
	}
	std::error_code system;
	const auto status = std::filesystem::status(path, system);
	if(system && system != std::errc::no_such_file_or_directory) {
		return std::unexpected(store_error(
			ClassicConfigStoreErrorCode::source_status, system
		));
	}
	const bool missing = system == std::errc::no_such_file_or_directory ||
		!std::filesystem::exists(status);
	if(missing) {
		if(auto committed = commit_classic_config_store(path, defaults);
			!committed) {
			return std::unexpected(std::move(committed.error()));
		}
		return ClassicConfigStoreLoadResult{
			.options = defaults,
			.disposition = ClassicConfigStoreDisposition::created_defaults,
		};
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(store_error(
			ClassicConfigStoreErrorCode::source_not_regular
		));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system) {
		return std::unexpected(store_error(
			ClassicConfigStoreErrorCode::source_size, system
		));
	}

	std::optional<ClassicConfigError> malformed{};
	auto bytes = std::array<std::byte, classic_config_file_bytes>{};
	if(size != classic_config_file_bytes) {
		malformed = input_size_error(static_cast<std::size_t>(size));
	} else {
		std::ifstream input(path, std::ios::binary);
		if(!input) {
			return std::unexpected(store_error(
				ClassicConfigStoreErrorCode::source_open
			));
		}
		input.read(
			reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
		if(!input || input.peek() != std::ifstream::traits_type::eof()) {
			return std::unexpected(store_error(
				ClassicConfigStoreErrorCode::source_read
			));
		}
		auto decoded = decode_classic_config_file(bytes);
		if(decoded) {
			return ClassicConfigStoreLoadResult{
				.options = decoded->options,
				.disposition = ClassicConfigStoreDisposition::loaded,
			};
		}
		malformed = decoded.error();
	}

	if(auto committed = commit_classic_config_store(path, defaults);
		!committed) {
		return std::unexpected(std::move(committed.error()));
	}
	return ClassicConfigStoreLoadResult{
		.options = defaults,
		.disposition = ClassicConfigStoreDisposition::recovered_defaults,
		.recovered_from = malformed,
	};
}

} // namespace th03::app
