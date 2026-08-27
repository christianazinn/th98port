#include "th03/app/new_classic/online/new_classic_online_store.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <span>
#include <system_error>
#include <utility>
#include <vector>

namespace th03::app {
namespace {

[[nodiscard]] NewClassicOnlineStoreError store_error(
	NewClassicOnlineStoreErrorCode code,
	std::error_code system = {},
	std::optional<NewClassicOnlineError> codec = std::nullopt
) noexcept
{
	return {code, system, std::move(codec)};
}

[[nodiscard]] std::filesystem::path sibling(
	const std::filesystem::path &path,
	const std::filesystem::path &suffix
)
{
	auto result = path;
	result += suffix.native();
	return result;
}

[[nodiscard]] bool exists_no_error(const std::filesystem::path &path) noexcept
{
	std::error_code ignored;
	return std::filesystem::exists(path, ignored);
}

[[nodiscard]] std::expected<std::vector<std::byte>, NewClassicOnlineStoreError>
read_cache(const std::filesystem::path &path)
{
	std::error_code system;
	const auto status = std::filesystem::status(path, system);
	if(system) {
		return std::unexpected(store_error(
			NewClassicOnlineStoreErrorCode::source_status, system
		));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(store_error(
			NewClassicOnlineStoreErrorCode::source_not_regular
		));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system || size > new_classic_online_cache_max_bytes ||
		size < new_classic_online_cache_header_bytes +
			new_classic_online_cache_footer_bytes) {
		return std::unexpected(store_error(
			NewClassicOnlineStoreErrorCode::codec, system,
			NewClassicOnlineError{NewClassicOnlineErrorCode::input_size, 0U, 0U}
		));
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(store_error(
			NewClassicOnlineStoreErrorCode::source_open
		));
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	input.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	if(!input || input.peek() != std::ifstream::traits_type::eof()) {
		return std::unexpected(store_error(
			NewClassicOnlineStoreErrorCode::source_read
		));
	}
	return bytes;
}

[[nodiscard]] std::expected<void, NewClassicOnlineStoreError> write_validated(
	const std::filesystem::path &path,
	const NewClassicOnlineCacheDocument &document
)
{
	auto encoded = encode_new_classic_online_cache(document);
	if(!encoded) {
		return std::unexpected(store_error(
			NewClassicOnlineStoreErrorCode::codec, {}, encoded.error()
		));
	}
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if(!output) {
		return std::unexpected(store_error(
			NewClassicOnlineStoreErrorCode::temporary_open
		));
	}
	output.write(
		reinterpret_cast<const char *>(encoded->data()),
		static_cast<std::streamsize>(encoded->size())
	);
	output.flush();
	if(!output) {
		return std::unexpected(store_error(
			NewClassicOnlineStoreErrorCode::temporary_write
		));
	}
	output.close();
	auto readback = read_cache(path);
	if(!readback) {
		return std::unexpected(store_error(
			NewClassicOnlineStoreErrorCode::temporary_validation,
			readback.error().system, readback.error().codec
		));
	}
	auto decoded = decode_new_classic_online_cache(*readback);
	if(*readback != *encoded || !decoded || *decoded != document) {
		return std::unexpected(store_error(
			NewClassicOnlineStoreErrorCode::temporary_validation, {},
			decoded ? std::nullopt :
				std::optional<NewClassicOnlineError>{decoded.error()}
		));
	}
	return {};
}

[[nodiscard]] std::expected<void, NewClassicOnlineStoreError> publish(
	const std::filesystem::path &path,
	const NewClassicOnlineCacheDocument &document
)
{
	const auto temporary = sibling(path, ".tmp");
	const auto old = sibling(path, ".old");
	std::error_code system;
	std::filesystem::remove(temporary, system);
	if(system) {
		return std::unexpected(store_error(
			NewClassicOnlineStoreErrorCode::temporary_open, system
		));
	}
	if(auto written = write_validated(temporary, document); !written) {
		return std::unexpected(std::move(written.error()));
	}
	std::filesystem::remove(old, system);
	if(system) {
		return std::unexpected(store_error(
			NewClassicOnlineStoreErrorCode::replace_target, system
		));
	}
	const bool had_canonical = exists_no_error(path);
	if(had_canonical) {
		std::filesystem::rename(path, old, system);
		if(system) {
			return std::unexpected(store_error(
				NewClassicOnlineStoreErrorCode::replace_target, system
			));
		}
	}
	std::filesystem::rename(temporary, path, system);
	if(system) {
		if(had_canonical) {
			std::error_code ignored;
			std::filesystem::rename(old, path, ignored);
		}
		return std::unexpected(store_error(
			NewClassicOnlineStoreErrorCode::replace_target, system
		));
	}
	std::filesystem::remove(old, system);
	return {};
}

struct Candidate final {
	NewClassicOnlineCacheDocument document{};
	std::size_t source = 0U;
};

} // namespace

std::expected<NewClassicOnlineStoreLoadResult, NewClassicOnlineStoreError>
load_new_classic_online_store(const std::filesystem::path &path)
{
	if(path.empty() || path.filename().empty()) {
		return std::unexpected(store_error(
			NewClassicOnlineStoreErrorCode::invalid_path
		));
	}
	const std::array paths{
		path,
		sibling(path, ".tmp"),
		sibling(path, ".old"),
	};
	std::vector<Candidate> candidates;
	std::optional<NewClassicOnlineError> last_codec;
	bool any_present = false;
	for(std::size_t source = 0U; source < paths.size(); ++source) {
		std::error_code system;
		const bool exists = std::filesystem::exists(paths[source], system);
		if(system) {
			return std::unexpected(store_error(
				NewClassicOnlineStoreErrorCode::source_status, system
			));
		}
		if(!exists) {
			continue;
		}
		const auto status = std::filesystem::status(paths[source], system);
		if(system) {
			return std::unexpected(store_error(
				NewClassicOnlineStoreErrorCode::source_status, system
			));
		}
		any_present = true;
		if(!std::filesystem::is_regular_file(status)) {
			return std::unexpected(store_error(
				NewClassicOnlineStoreErrorCode::source_not_regular
			));
		}
		auto bytes = read_cache(paths[source]);
		if(!bytes) {
			if(bytes.error().code == NewClassicOnlineStoreErrorCode::codec) {
				last_codec = bytes.error().codec;
				continue;
			}
			return std::unexpected(std::move(bytes.error()));
		}
		auto decoded = decode_new_classic_online_cache(*bytes);
		if(!decoded) {
			last_codec = decoded.error();
			continue;
		}
		candidates.push_back({std::move(*decoded), source});
	}
	if(candidates.empty()) {
		if(any_present) {
			return std::unexpected(store_error(
				NewClassicOnlineStoreErrorCode::no_valid_candidate, {}, last_codec
			));
		}
		return NewClassicOnlineStoreLoadResult{
			NewClassicOnlineCacheDocument{},
			NewClassicOnlineStoreDisposition::empty,
		};
	}
	const auto newest = std::ranges::max_element(
		candidates,
		[](const Candidate &lhs, const Candidate &rhs) {
			if(lhs.document.generation != rhs.document.generation) {
				return lhs.document.generation < rhs.document.generation;
			}
			return lhs.source > rhs.source;
		}
	);
	auto document = newest->document;
	const bool recovered = newest->source != 0U;
	if(recovered) {
		if(auto promoted = publish(path, document); !promoted) {
			return std::unexpected(std::move(promoted.error()));
		}
	} else {
		std::error_code ignored;
		std::filesystem::remove(paths[1U], ignored);
		std::filesystem::remove(paths[2U], ignored);
	}
	return NewClassicOnlineStoreLoadResult{
		std::move(document),
		recovered ? NewClassicOnlineStoreDisposition::recovered_candidate :
			NewClassicOnlineStoreDisposition::loaded,
	};
}

std::expected<void, NewClassicOnlineStoreError>
commit_new_classic_online_store(
	const std::filesystem::path &path,
	NewClassicOnlineCacheDocument &document
)
{
	if(path.empty() || path.filename().empty()) {
		return std::unexpected(store_error(
			NewClassicOnlineStoreErrorCode::invalid_path
		));
	}
	auto next = document;
	if(next.generation != std::numeric_limits<std::uint32_t>::max()) {
		++next.generation;
	}
	if(auto valid = validate_new_classic_online_cache(next); !valid) {
		return std::unexpected(store_error(
			NewClassicOnlineStoreErrorCode::codec, {}, valid.error()
		));
	}
	if(auto committed = publish(path, next); !committed) {
		return std::unexpected(std::move(committed.error()));
	}
	document = std::move(next);
	return {};
}

} // namespace th03::app
