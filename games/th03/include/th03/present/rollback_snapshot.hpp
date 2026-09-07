#ifndef TH03_PRESENT_ROLLBACK_SNAPSHOT_HPP
#define TH03_PRESENT_ROLLBACK_SNAPSHOT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>

namespace th03::present {

class Raster;
class Tram;

// Fixed local envelope for both graphics pages, palettes, TRAM, and gaiji.
// Callers should keep rings on the heap: sixteen slots reserve about 8.1 MiB.
inline constexpr std::size_t rollback_visual_snapshot_capacity = 520U * 1024U;

struct RollbackVisualSnapshot final {
	std::uint32_t used_size = 0U;
	std::array<std::byte, rollback_visual_snapshot_capacity> bytes{};

	friend bool operator==(
		const RollbackVisualSnapshot &,
		const RollbackVisualSnapshot &
	) = default;
};

static_assert(
	sizeof(RollbackVisualSnapshot) ==
		sizeof(std::uint32_t) + rollback_visual_snapshot_capacity
);

enum class RollbackVisualErrorCode : std::uint8_t {
	bad_magic,
	unsupported_version,
	unsupported_layout,
	truncated,
	count_mismatch,
	checksum_mismatch,
	invalid_value,
};

struct RollbackVisualError final {
	RollbackVisualErrorCode code = RollbackVisualErrorCode::invalid_value;
	const char *message = "";
	std::size_t offset = 0U;
};

// T3RBV001 is the shared visual-machine section of the future complete
// T3RBK001 image. The caller supplies reusable storage so source-frame capture
// never allocates. Neither function copies a C++ object image.
[[nodiscard]] std::expected<void, RollbackVisualError>
capture_rollback_visual(
	const Raster &raster,
	const Tram &tram,
	RollbackVisualSnapshot &snapshot
) noexcept;

[[nodiscard]] std::expected<void, RollbackVisualError>
restore_rollback_visual(
	Raster &raster,
	Tram &tram,
	const RollbackVisualSnapshot &snapshot
) noexcept;

} // namespace th03::present

#endif
