#ifndef TH03_FORMATS_V14_HPP
#define TH03_FORMATS_V14_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "th03/error.hpp"
#include "th03/formats/replay.hpp"

namespace th03::formats {

inline constexpr std::uint16_t replay_v14_version = 14U;
inline constexpr std::size_t replay_v14_header_size = 0x300U;
inline constexpr std::size_t replay_v14_checkpoint_size = 289U;

// Finalized player-facing adapter. It consumes a caller-owned byte span and
// preserves V14 metadata outside the deterministic gameplay core.
std::expected<ReplayDocument, Error> parse_v14_replay(
	std::span<const std::byte> bytes
);

// Canonical inverse of parse_v14_replay(). The writer owns no filesystem and
// validates its complete output through the frozen reader before returning it.
std::expected<std::vector<std::byte>, Error> write_v14_replay(
	const ReplayDocument &document
);

} // namespace th03::formats

#endif
