#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>

#include "th03/app/main_compiled_content.hpp"
#include "th03/app/new_classic_netplay_signaling.hpp"

namespace th03::app {

inline constexpr std::size_t new_classic_netplay_identity_bytes = 224U;

struct NewClassicNetplayIdentitySnapshot final {
	std::string room_id{};
	NewClassicNetplayRoomPaired profiles{};

	friend bool operator==(
		const NewClassicNetplayIdentitySnapshot &,
		const NewClassicNetplayIdentitySnapshot &
	) = default;
};

enum class NewClassicNetplayIdentityError : std::uint8_t {
	invalid_room,
	invalid_profile,
};

// T3NID001 is presentation identity, not simulation state. Its final 32 bytes
// are SHA-256 over the preceding fixed body. The same digest is carried once
// by the pre-game match contract and never enters per-frame traffic.
[[nodiscard]] std::expected<
	std::array<std::byte, new_classic_netplay_identity_bytes>,
	NewClassicNetplayIdentityError
> encode_new_classic_netplay_identity(
	const NewClassicNetplayIdentitySnapshot &snapshot
) noexcept;

[[nodiscard]] std::expected<AppSha256, NewClassicNetplayIdentityError>
new_classic_netplay_identity_digest(
	const NewClassicNetplayIdentitySnapshot &snapshot
) noexcept;

} // namespace th03::app
