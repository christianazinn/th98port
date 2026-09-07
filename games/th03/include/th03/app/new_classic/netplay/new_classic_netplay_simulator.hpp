#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_SIMULATOR_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_SIMULATOR_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

#include "th03/app/new_classic/netplay/new_classic_netplay_protocol.hpp"

namespace th03::app {

inline constexpr std::size_t new_classic_netplay_simulated_mtu = 1200U;
inline constexpr std::size_t new_classic_netplay_simulated_queue = 256U;

struct NewClassicNetplayDeliveryPlan final {
	std::uint8_t primary_delay_frames = 0U;
	bool drop_primary = false;
	std::optional<std::uint8_t> duplicate_delay_frames{};
};

enum class NewClassicNetplaySimulatorErrorCode : std::uint8_t {
	invalid_endpoint,
	empty_payload,
	payload_too_large,
	queue_full,
	frame_overflow,
	output_too_small,
};

struct NewClassicNetplaySimulatorError final {
	NewClassicNetplaySimulatorErrorCode code =
		NewClassicNetplaySimulatorErrorCode::invalid_endpoint;
};

struct NewClassicNetplaySimulatedDatagram final {
	std::uint64_t datagram_id = 0U;
	std::uint64_t delivery_frame = 0U;
	NewClassicNetplayPlayer sender = NewClassicNetplayPlayer::player_one;
	std::size_t payload_size = 0U;
	bool duplicate = false;
};

struct NewClassicNetplaySimulatorStats final {
	std::uint64_t sends = 0U;
	std::uint64_t primary_drops = 0U;
	std::uint64_t duplicate_copies = 0U;
	std::uint64_t enqueued_copies = 0U;
	std::uint64_t delivered_copies = 0U;
};

// Deterministic, host-clock-free test transport. It owns opaque datagrams and
// never interprets rollback inputs. Construction allocates its fixed storage;
// send/receive perform no later allocation.
class NewClassicNetplayTransportSimulator final {
public:
	NewClassicNetplayTransportSimulator();
	~NewClassicNetplayTransportSimulator();
	NewClassicNetplayTransportSimulator(
		const NewClassicNetplayTransportSimulator &
	) = delete;
	NewClassicNetplayTransportSimulator &operator=(
		const NewClassicNetplayTransportSimulator &
	) = delete;
	NewClassicNetplayTransportSimulator(
		NewClassicNetplayTransportSimulator &&
	) = delete;
	NewClassicNetplayTransportSimulator &operator=(
		NewClassicNetplayTransportSimulator &&
	) = delete;

	[[nodiscard]] std::expected<std::uint64_t,
		NewClassicNetplaySimulatorError> send(
		std::uint64_t current_frame,
		NewClassicNetplayPlayer sender,
		std::span<const std::byte> payload,
		NewClassicNetplayDeliveryPlan delivery
	) noexcept;

	[[nodiscard]] std::expected<
		std::optional<NewClassicNetplaySimulatedDatagram>,
		NewClassicNetplaySimulatorError
	> receive(
		std::uint64_t current_frame,
		NewClassicNetplayPlayer recipient,
		std::span<std::byte> output
	) noexcept;

	[[nodiscard]] NewClassicNetplaySimulatorStats stats() const noexcept;
	[[nodiscard]] std::size_t queued() const noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace th03::app

#endif
