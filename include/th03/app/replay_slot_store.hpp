#ifndef TH03_APP_REPLAY_SLOT_STORE_HPP
#define TH03_APP_REPLAY_SLOT_STORE_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <system_error>

#include "th03/formats/replay.hpp"

namespace th03::app {

inline constexpr std::size_t replay_slot_count = 100U;
inline constexpr std::uintmax_t maximum_replay_file_bytes = 64U * 1024U * 1024U;

enum class ReplaySlotStoreErrorCode : std::uint8_t {
	invalid_root,
	invalid_slot,
	invalid_local_view,
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
	remove_target,
};

struct ReplaySlotStoreError final {
	ReplaySlotStoreErrorCode code = ReplaySlotStoreErrorCode::invalid_root;
	std::error_code system{};
	std::optional<Error> format{};
};

enum class ReplaySlotLocalOrigin : std::uint8_t {
	ordinary,
	netplay,
};

// Catalog presentation derived from the self-contained V14 recorder role.
// The transient type remains useful to the save UI, but it is never persisted
// separately from the replay.
struct ReplaySlotLocalView final {
	ReplaySlotLocalOrigin origin = ReplaySlotLocalOrigin::ordinary;
	std::uint8_t local_player = 0U;

	[[nodiscard]] friend constexpr bool operator==(
		const ReplaySlotLocalView &,
		const ReplaySlotLocalView &
	) = default;
};

struct ReplaySlotLoadResult final {
	std::optional<formats::ReplayDocument> document{};
	std::optional<ReplaySlotLocalView> local_view{};
};

[[nodiscard]] std::filesystem::path replay_slot_path(
	const std::filesystem::path &replay_root,
	std::uint8_t slot
);
[[nodiscard]] std::filesystem::path replay_fallback_path(
	const std::filesystem::path &replay_root
);

// Developer tools may name an exact V14 artifact without projecting it into a
// player-facing slot. A missing path is reported as an empty result.
[[nodiscard]] std::expected<ReplaySlotLoadResult, ReplaySlotStoreError>
load_replay_file(const std::filesystem::path &path);

// Missing slots are ordinary empty results. Existing bytes must be bounded,
// regular, and accepted by the frozen V14 reader.
[[nodiscard]] std::expected<ReplaySlotLoadResult, ReplaySlotStoreError>
load_replay_slot(
	const std::filesystem::path &replay_root,
	std::uint8_t slot
);

// Encodes through the canonical V14 writer, writes a same-directory exclusive
// temporary, flushes and reads it back, reparses it, then atomically replaces
// the selected slot. Netplay presentation ownership is encoded in the V14
// header itself; no installation-local sidecar is created.
[[nodiscard]] std::expected<void, ReplaySlotStoreError> commit_replay_slot(
	const std::filesystem::path &replay_root,
	std::uint8_t slot,
	const formats::ReplayDocument &document,
	std::optional<ReplaySlotLocalView> local_view = std::nullopt
);

// Publishes already-encoded, strictly valid V14 bytes without re-encoding
// them. This preserves content-addressed optional tails while retaining the
// slot store's exclusive temporary, flush, read-back, parse, and atomic
// replacement contract.
[[nodiscard]] std::expected<void, ReplaySlotStoreError>
commit_exact_replay_file(
	const std::filesystem::path &path,
	std::span<const std::byte> bytes
);

// Builds before the memory-only recorder may have left TH3LAST behind. An
// ordinary fresh application launch removes that legacy artifact before OP.
[[nodiscard]] std::expected<void, ReplaySlotStoreError>
discard_replay_fallback_on_startup(
	const std::filesystem::path &replay_root
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
