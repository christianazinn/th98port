#ifndef TH03_APP_UPDATE_GENERATION_HPP
#define TH03_APP_UPDATE_GENERATION_HPP

#include "th03/app/update/update_activation.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

namespace th03::app {

inline constexpr std::string_view update_generation_marker_filename =
	"PODD.GEN";
inline constexpr std::size_t update_generation_marker_bytes = 96U;

enum class UpdateGenerationMarkerErrorCode : std::uint8_t {
	input_size,
	magic,
	version,
	declared_size,
	reserved,
	identity,
};

struct UpdateGenerationMarkerError final {
	UpdateGenerationMarkerErrorCode code =
		UpdateGenerationMarkerErrorCode::input_size;
	std::size_t offset = 0U;

	friend constexpr bool operator==(
		const UpdateGenerationMarkerError &,
		const UpdateGenerationMarkerError &
	) = default;
};

// PODD.GEN is written into the unpublished temporary directory and therefore
// becomes visible atomically with a complete immutable generation. Cleanup
// never treats a directory name alone as proof that the tree is updater-owned.
[[nodiscard]] std::expected<
	std::array<std::byte, update_generation_marker_bytes>,
	UpdateGenerationMarkerError
> encode_update_generation_marker(
	const UpdateGenerationIdentity &identity
) noexcept;

[[nodiscard]] std::expected<
	UpdateGenerationIdentity,
	UpdateGenerationMarkerError
> decode_update_generation_marker(std::span<const std::byte> bytes) noexcept;

} // namespace th03::app

#endif
