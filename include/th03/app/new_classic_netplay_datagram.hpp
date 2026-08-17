#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_DATAGRAM_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_DATAGRAM_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "th03/app/new_classic_netplay_protocol.hpp"

namespace th03::app {

inline constexpr std::uint8_t new_classic_netplay_datagram_version = 7U;
inline constexpr std::size_t new_classic_netplay_history_values = 16U;
inline constexpr std::size_t new_classic_netplay_datagram_bytes = 104U;
inline constexpr std::size_t new_classic_netplay_simulation_digest_bytes = 32U;

enum class NewClassicNetplayDatagramType : std::uint8_t {
	hello = 1U,
	accept = 2U,
	input = 3U,
	disconnect = 4U,
	simulation_digest = 5U,
	simulation_digest_ack = 6U,
	pause_waiting = 7U,
	resume_ready = 8U,
	forfeit_request = 9U,
	forfeit_ack = 10U,
};

enum class NewClassicNetplayProofOrder : std::uint8_t {
	stale,
	current,
	future,
};

// Classifies one runtime proof against the newest completely verified proof.
// Both digest and acknowledgement receive paths must ignore stale UDP copies.
[[nodiscard]] NewClassicNetplayProofOrder new_classic_netplay_proof_order(
	std::uint64_t verified_frame,
	std::uint64_t received_frame
) noexcept;

struct NewClassicNetplayDatagram final {
	NewClassicNetplayDatagramType type =
		NewClassicNetplayDatagramType::hello;
	NewClassicNetplayPlayer sender = NewClassicNetplayPlayer::player_one;
	std::uint8_t input_count = 0U;
	std::uint64_t session_id = 0U;
	std::uint64_t contract_hash = 0U;
	std::uint64_t latest_frame = 0U;
	std::uint64_t confirmed_frame_count = 0U;
	// Digest packets use latest_frame as their exact pre-frame identity and
	// confirmed_frame_count as their confirmed-gameplay milestone. Both are
	// zero only for the initial proof. No peer state is accepted.
	std::array<
		std::uint8_t,
		new_classic_netplay_simulation_digest_bytes
	> simulation_digest{};
	NewClassicNetplaySimulationDigestKind simulation_digest_kind =
		NewClassicNetplaySimulationDigestKind::initial;
	NewClassicNetplayBoundaryProof boundary_proof =
		NewClassicNetplayBoundaryProof::none;
	// Newest first. Element i owns latest_frame-i.
	std::array<
		NewClassicNetplayOwnedInput,
		new_classic_netplay_history_values
	> inputs{};
};

enum class NewClassicNetplayDatagramErrorCode : std::uint8_t {
	invalid_size,
	invalid_magic,
	unsupported_version,
	invalid_type,
	invalid_sender,
	invalid_session,
	invalid_contract,
	invalid_history,
	invalid_digest,
	invalid_input,
};

struct NewClassicNetplayDatagramError final {
	NewClassicNetplayDatagramErrorCode code =
		NewClassicNetplayDatagramErrorCode::invalid_size;
	std::size_t input_index = 0U;
	NewClassicNetplayInputError input{};
};

[[nodiscard]] std::expected<
	std::array<std::byte, new_classic_netplay_datagram_bytes>,
	NewClassicNetplayDatagramError
> encode_new_classic_netplay_datagram(
	const NewClassicNetplayDatagram &datagram
) noexcept;

[[nodiscard]] std::expected<
	NewClassicNetplayDatagram,
	NewClassicNetplayDatagramError
> decode_new_classic_netplay_datagram(
	std::span<const std::byte> wire
) noexcept;

} // namespace th03::app

#endif
