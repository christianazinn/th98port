#ifndef TH03_APP_CLASSIC_CONFIG_STORE_HPP
#define TH03_APP_CLASSIC_CONFIG_STORE_HPP

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <system_error>

#include "th03/app/classic_config.hpp"

namespace th03::app {

enum class ClassicConfigStoreDisposition : std::uint8_t {
	loaded,
	created_defaults,
	recovered_defaults,
};

enum class ClassicConfigStoreErrorCode : std::uint8_t {
	invalid_path,
	source_status,
	source_not_regular,
	source_size,
	source_open,
	source_read,
	codec,
	temporary_create,
	temporary_write,
	temporary_flush,
	temporary_seek,
	temporary_read,
	temporary_validation,
	temporary_close,
	replace_target,
};

struct ClassicConfigStoreError final {
	ClassicConfigStoreErrorCode code =
		ClassicConfigStoreErrorCode::invalid_path;
	std::error_code system{};
	std::optional<ClassicConfigError> codec{};
};

struct ClassicConfigStoreLoadResult final {
	ClassicConfigOptions options{};
	ClassicConfigStoreDisposition disposition =
		ClassicConfigStoreDisposition::loaded;
	std::optional<ClassicConfigError> recovered_from{};
};

// Reads one exact eight-byte Classic document. Missing or malformed inputs are
// atomically replaced with the supplied defaults. DOS-only metadata is decoded
// for validation but never escapes into native runtime state.
[[nodiscard]] std::expected<
	ClassicConfigStoreLoadResult,
	ClassicConfigStoreError
> load_classic_config_store(
	const std::filesystem::path &path,
	ClassicConfigOptions defaults = classic_config_defaults()
);

// Writes a deterministic Classic-compatible file with zero unused, resident,
// and debug metadata through a same-directory flush/read-back/replace
// transaction. Every failure before replacement preserves the target.
[[nodiscard]] std::expected<void, ClassicConfigStoreError>
commit_classic_config_store(
	const std::filesystem::path &path,
	ClassicConfigOptions options
);

} // namespace th03::app

#endif
