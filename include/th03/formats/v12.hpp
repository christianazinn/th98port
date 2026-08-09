#ifndef TH03_FORMATS_V12_HPP
#define TH03_FORMATS_V12_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "th03/error.hpp"
#include "th03/replay/session.hpp"

namespace th03::formats {

inline constexpr std::uint16_t replay_v12_version = 12;

// Historical adapter for finalized V12. It always starts from the portable
// process anchor and ignores optional broad-state retry accelerators.
std::expected<replay::ReplaySession, Error> parse_v12_replay(
	std::span<const std::byte> bytes
);

} // namespace th03::formats

#endif
