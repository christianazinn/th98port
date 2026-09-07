#ifndef TH03_APP_NEW_CLASSIC_ONLINE_STORE_HPP
#define TH03_APP_NEW_CLASSIC_ONLINE_STORE_HPP

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <system_error>

#include "th03/app/new_classic/online/new_classic_online.hpp"

namespace th03::app {

enum class NewClassicOnlineStoreDisposition : std::uint8_t {
	loaded,
	empty,
	recovered_candidate,
};

enum class NewClassicOnlineStoreErrorCode : std::uint8_t {
	invalid_path,
	source_status,
	source_not_regular,
	source_open,
	source_read,
	no_valid_candidate,
	codec,
	temporary_open,
	temporary_write,
	temporary_validation,
	replace_target,
};

struct NewClassicOnlineStoreError final {
	NewClassicOnlineStoreErrorCode code =
		NewClassicOnlineStoreErrorCode::invalid_path;
	std::error_code system{};
	std::optional<NewClassicOnlineError> codec{};
};

struct NewClassicOnlineStoreLoadResult final {
	NewClassicOnlineCacheDocument document{};
	NewClassicOnlineStoreDisposition disposition =
		NewClassicOnlineStoreDisposition::loaded;
};

// Loads the newest valid canonical/.tmp/.old cache candidate. A wholly absent
// cache is a normal empty offline state; a present but wholly corrupt cache is
// reported and never allowed to affect local scores, replays, or gameplay.
[[nodiscard]] std::expected<
	NewClassicOnlineStoreLoadResult,
	NewClassicOnlineStoreError
> load_new_classic_online_store(const std::filesystem::path &path);

// Increments the local cache generation, validates an exact read-back, and
// atomically replaces the canonical cache through fixed .tmp/.old siblings.
[[nodiscard]] std::expected<void, NewClassicOnlineStoreError>
commit_new_classic_online_store(
	const std::filesystem::path &path,
	NewClassicOnlineCacheDocument &document
);

} // namespace th03::app

#endif
