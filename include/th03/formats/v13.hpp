#ifndef TH03_FORMATS_V13_HPP
#define TH03_FORMATS_V13_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "th03/error.hpp"
#include "th03/formats/replay.hpp"

namespace th03::formats {

inline constexpr std::uint16_t replay_v13_version = 13U;
inline constexpr std::size_t replay_v13_header_size = 622U;
inline constexpr std::size_t replay_v13_checkpoint_size = 289U;

// Finalized player-facing adapter. It consumes a caller-owned byte span and
// preserves V13 metadata outside the deterministic gameplay core.
std::expected<ReplayDocument, Error> parse_v13_replay(
	std::span<const std::byte> bytes
);

} // namespace th03::formats

#endif
