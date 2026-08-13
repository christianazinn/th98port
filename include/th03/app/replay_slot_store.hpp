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
	publish_slot,
	remove_target,
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
[[nodiscard]] std::filesystem::path replay_fallback_path(
	const std::filesystem::path &replay_root
);

// Developer tools may name an exact V13 artifact without projecting it into a
// player-facing slot. A missing path is reported as an empty result.
[[nodiscard]] std::expected<ReplaySlotLoadResult, ReplaySlotStoreError>
load_replay_file(const std::filesystem::path &path);

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

// Replay Patch 0.4.12 keeps the live user recording in TH3LAST.RPY. Native
// checkpoints replace the whole canonical V13 artifact atomically, which is
// the host equivalent of the patch's flushed header/input prefix.
[[nodiscard]] std::expected<ReplaySlotLoadResult, ReplaySlotStoreError>
load_replay_fallback(const std::filesystem::path &replay_root);
[[nodiscard]] std::expected<void, ReplaySlotStoreError>
checkpoint_replay_fallback(
	const std::filesystem::path &replay_root,
	const formats::ReplayDocument &document
);
[[nodiscard]] std::expected<void, ReplaySlotStoreError>
remove_replay_fallback(const std::filesystem::path &replay_root);

// Rewrites the fallback with the final menu-edited document, validates it, and
// then atomically renames that exact artifact over the selected slot. A failed
// publication preserves the old slot and leaves the fallback recoverable.
[[nodiscard]] std::expected<void, ReplaySlotStoreError>
commit_replay_fallback_to_slot(
	const std::filesystem::path &replay_root,
	std::uint8_t slot,
	const formats::ReplayDocument &document
);

enum class ReplaySlotStoreFaultPoint : std::uint8_t {
	none,
	directory_create,
	temporary_create,
	temporary_write,
	temporary_flush,
	temporary_seek,
	temporary_read,
	temporary_validation,
	temporary_close,
	replace_target,
	publish_slot,
	remove_target,
};

// Deterministic one-shot storage failures for unit tests. Production never
// arms this seam.
struct ReplaySlotStoreTestAccess final {
	static void fail_next(ReplaySlotStoreFaultPoint point) noexcept;
	static void clear_failure() noexcept;
};

} // namespace th03::app

#endif
