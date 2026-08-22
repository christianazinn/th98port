#ifndef TH03_APP_NEW_CLASSIC_SETTINGS_STORE_HPP
#define TH03_APP_NEW_CLASSIC_SETTINGS_STORE_HPP

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <system_error>

#include "th03/app/classic_config.hpp"
#include "th03/app/new_classic_settings.hpp"

namespace th03::app {

enum class NewClassicSettingsStoreDisposition : std::uint8_t {
	loaded,
	upgraded_v1,
	upgraded_v2,
	upgraded_v3,
	upgraded_v4,
	upgraded_v5,
	upgraded_v6,
	migrated_classic,
	created_defaults,
	recovered_defaults,
};

enum class NewClassicSettingsStoreErrorCode : std::uint8_t {
	invalid_path,
	invalid_defaults,
	source_status,
	source_not_regular,
	source_size,
	source_open,
	source_read,
	migration_status,
	migration_open,
	migration_read,
	temporary_create,
	temporary_write,
	temporary_read,
	temporary_validation,
	publish,
	codec,
};

struct NewClassicSettingsStoreError final {
	NewClassicSettingsStoreErrorCode code =
		NewClassicSettingsStoreErrorCode::invalid_path;
	std::error_code system{};
	std::optional<NewClassicSettingsError> codec{};
};

struct NewClassicSettingsStoreLoadResult final {
	NewClassicSettings settings{};
	NewClassicSettingsStoreDisposition disposition =
		NewClassicSettingsStoreDisposition::loaded;
	std::optional<NewClassicSettingsError> recovered_from{};
};

// An absent New Classic document is seeded from a valid Classic YUME.CFG by
// value. Missing or malformed Classic input falls back to [defaults] without
// modifying that file. A present malformed New Classic document recovers to
// [defaults] and is never re-migrated.
[[nodiscard]] std::expected<
	NewClassicSettingsStoreLoadResult,
	NewClassicSettingsStoreError
> load_new_classic_settings_store(
	const std::filesystem::path &path,
	const std::filesystem::path &classic_config_path,
	NewClassicSettings defaults = new_classic_settings_defaults()
);

[[nodiscard]] std::expected<void, NewClassicSettingsStoreError>
commit_new_classic_settings_store(
	const std::filesystem::path &path,
	const NewClassicSettings &settings
);

} // namespace th03::app

#endif
