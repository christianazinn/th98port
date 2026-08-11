#ifndef TH03_APP_CLASSIC_SCORE_STORE_HPP
#define TH03_APP_CLASSIC_SCORE_STORE_HPP

#include <array>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <system_error>

#include "th03/app/classic_score.hpp"

namespace th03::app {

enum class ClassicScoreStoreDisposition : std::uint8_t {
	loaded,
	created_stock,
	recovered_stock,
};

enum class ClassicScoreStoreErrorCode : std::uint8_t {
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

struct ClassicScoreStoreError final {
	ClassicScoreStoreErrorCode code =
		ClassicScoreStoreErrorCode::invalid_path;
	std::error_code system{};
	std::optional<ClassicScoreError> codec{};
};

struct ClassicScoreStoreLoadResult final {
	ClassicScoreDocument document{};
	ClassicScoreStoreDisposition disposition =
		ClassicScoreStoreDisposition::loaded;
	// Present only when malformed existing bytes caused source-compatible
	// all-rank stock recreation. Missing-file creation has no prior error.
	std::optional<ClassicScoreError> recovered_from{};
};

// Reads one exact 824-byte document. A missing file is created from the stock
// board; any malformed existing document is atomically replaced by all four
// stock ranks, matching scoredat_recreate(). I/O failures never masquerade as
// corruption and never publish a partial document.
[[nodiscard]] std::expected<
	ClassicScoreStoreLoadResult,
	ClassicScoreStoreError
> load_classic_score_store(
	const std::filesystem::path &path,
	std::array<ClassicScoreCodecFields, classic_score_rank_count> stock_codec = {}
);

// Encodes from a const value, writes a same-directory temporary, flushes and
// validates it by read-back, then atomically replaces the target. Every failure
// before replacement preserves the previous target.
[[nodiscard]] std::expected<void, ClassicScoreStoreError>
commit_classic_score_store(
	const std::filesystem::path &path,
	const ClassicScoreDocument &document
);

} // namespace th03::app

#endif
