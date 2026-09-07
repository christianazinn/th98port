#ifndef TH03_APP_NEW_CLASSIC_SCORE_STORE_HPP
#define TH03_APP_NEW_CLASSIC_SCORE_STORE_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <system_error>

#include "th03/app/classic/persistence/classic_score.hpp"
#include "th03/app/new_classic/persistence/new_classic_score.hpp"

namespace th03::app {

enum class NewClassicScoreStoreDisposition : std::uint8_t {
	loaded,
	created_empty,
	migrated_classic,
	recovered_candidate,
	repaired_boards,
};

enum class NewClassicScoreStoreErrorCode : std::uint8_t {
	invalid_path,
	source_status,
	source_not_regular,
	source_size,
	source_open,
	source_read,
	codec,
	temporary_open,
	temporary_write,
	temporary_validation,
	replace_target,
};

struct NewClassicScoreStoreError final {
	NewClassicScoreStoreErrorCode code =
		NewClassicScoreStoreErrorCode::invalid_path;
	std::error_code system{};
	std::optional<NewClassicScoreError> codec{};
	std::optional<ClassicScoreError> legacy_codec{};
};

struct NewClassicScoreStoreLoadResult final {
	NewClassicScoreDocument document{};
	NewClassicScoreStoreDisposition disposition =
		NewClassicScoreStoreDisposition::loaded;
	std::size_t repaired_board_count = 0U;
};

// Reads the canonical expanded file and its fixed .tmp/.old crash candidates,
// choosing the greatest valid generation and repairing only invalid boards.
// If no expanded header survives, a valid legacy Classic document is imported
// by value; the legacy file is never renamed, rewritten, or deleted.
[[nodiscard]] std::expected<
	NewClassicScoreStoreLoadResult,
	NewClassicScoreStoreError
> load_new_classic_score_store(
	const std::filesystem::path &path,
	const std::filesystem::path &legacy_path = {}
);

// Same recovery/migration selection as the production loader, but performs no
// repair publication, candidate cleanup, migration store, or other write.
// Replay playback uses this to reconstruct the recorded score screen without
// making viewing itself observable in persistence.
[[nodiscard]] std::expected<
	NewClassicScoreStoreLoadResult,
	NewClassicScoreStoreError
> load_new_classic_score_store_read_only(
	const std::filesystem::path &path,
	const std::filesystem::path &legacy_path = {}
);

// Increments generation (saturating), validates fixed-size read-back, and uses
// fixed .tmp/.old siblings so an interrupted replacement is recoverable on the
// next load.
[[nodiscard]] std::expected<void, NewClassicScoreStoreError>
commit_new_classic_score_store(
	const std::filesystem::path &path,
	NewClassicScoreDocument &document
);

} // namespace th03::app

#endif
