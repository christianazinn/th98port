#ifndef TH03_APP_REPLAY_SLOT_STORE_HPP
#define TH03_APP_REPLAY_SLOT_STORE_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <system_error>

#include "th03/formats/replay.hpp"

namespace th03::app {

inline constexpr std::size_t replay_slot_count = 100U;
inline constexpr std::uintmax_t maximum_replay_file_bytes = 64U * 1024U * 1024U;

enum class ReplaySlotStoreErrorCode : std::uint8_t {
	invalid_root,
	invalid_slot,
	directory_create,
	source_status,
	source_not_regular,
	source_size,
	source_too_large,
	source_open,
	source_read,
	format,
	temporary_create,
	temporary_write,
	temporary_flush,
	temporary_seek,
	temporary_read,
	temporary_validation,
	temporary_close,
	replace_target,
};

struct ReplaySlotStoreError final {
	ReplaySlotStoreErrorCode code = ReplaySlotStoreErrorCode::invalid_root;
	std::error_code system{};
	std::optional<Error> format{};
};

struct ReplaySlotLoadResult final {
	std::optional<formats::ReplayDocument> document{};
};

[[nodiscard]] std::filesystem::path replay_slot_path(
	const std::filesystem::path &replay_root,
	std::uint8_t slot
);

// Missing slots are ordinary empty results. Existing bytes must be bounded,
// regular, and accepted by the frozen V13 reader.
[[nodiscard]] std::expected<ReplaySlotLoadResult, ReplaySlotStoreError>
load_replay_slot(
	const std::filesystem::path &replay_root,
	std::uint8_t slot
);

// Encodes through the canonical V13 writer, writes a same-directory exclusive
// temporary, flushes and reads it back, reparses it, then atomically replaces
// the selected slot. Every pre-replacement failure preserves the old slot.
[[nodiscard]] std::expected<void, ReplaySlotStoreError> commit_replay_slot(
	const std::filesystem::path &replay_root,
	std::uint8_t slot,
	const formats::ReplayDocument &document
);

} // namespace th03::app

#endif
