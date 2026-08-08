#ifndef TH03_FORMATS_V11_HPP
#define TH03_FORMATS_V11_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "th03/error.hpp"
#include "th03/replay/session.hpp"

namespace th03::formats {

inline constexpr std::uint16_t replay_v11_version = 11;

// Validation-only historical reader. V11 structures do not cross this API;
// finalized V13 will produce the same version-neutral ReplaySession.
std::expected<replay::ReplaySession, Error> parse_v11_replay(
	std::span<const std::byte> bytes
);

} // namespace th03::formats

#endif
