#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_PAUSE_BARRIER_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_PAUSE_BARRIER_HPP

#include <cstdint>
#include <expected>

#include "th03/app/new_classic_netplay_datagram.hpp"
#include "th03/core/game.hpp"

namespace th03::app {

enum class NewClassicNetplayPauseBarrierStatus : std::uint8_t {
	waiting_release,
	waiting_local_ready,
	waiting_remote_ready,
	ready,
};

enum class NewClassicNetplayPauseBarrierErrorCode : std::uint8_t {
	invalid_identity,
	invalid_signal,
	peer_resumed_before_local_ready,
};

struct NewClassicNetplayPauseBarrierError final {
	NewClassicNetplayPauseBarrierErrorCode code =
		NewClassicNetplayPauseBarrierErrorCode::invalid_identity;
};

// Platform-neutral ready barrier for one already-confirmed Pause boundary.
// Wall-clock retransmission belongs to the transport adapter. Held opening
// input cannot become Resume, and neither signal advances the source clock.
class NewClassicNetplayPauseBarrier final {
public:
	[[nodiscard]] static std::expected<
		NewClassicNetplayPauseBarrier,
		NewClassicNetplayPauseBarrierError
	> create(
		std::uint64_t frame,
		std::uint64_t confirmed_gameplay_frame_count
	) noexcept;

	void consume_local(CoreInput input) noexcept;
	[[nodiscard]] std::expected<void, NewClassicNetplayPauseBarrierError>
	receive(
		NewClassicNetplayDatagramType signal,
		std::uint64_t frame,
		std::uint64_t confirmed_gameplay_frame_count
	) noexcept;
	// A post-Pause input packet proves the peer already crossed the barrier.
	// This closes the sole packet-loss window after that peer stops sending its
	// Resume-ready beacon.
	[[nodiscard]] std::expected<void, NewClassicNetplayPauseBarrierError>
	observe_peer_gameplay(std::uint64_t frame) noexcept;

	[[nodiscard]] NewClassicNetplayDatagramType outbound_signal() const noexcept;
	[[nodiscard]] NewClassicNetplayPauseBarrierStatus status() const noexcept;
	[[nodiscard]] std::uint64_t frame() const noexcept;
	[[nodiscard]] std::uint64_t confirmed_gameplay_frame_count() const noexcept;
	[[nodiscard]] bool ready() const noexcept;
	[[nodiscard]] bool local_ready() const noexcept;
	[[nodiscard]] bool remote_ready() const noexcept;

private:
	std::uint64_t frame_ = 0U;
	std::uint64_t confirmed_gameplay_frame_count_ = 0U;
	bool released_ = false;
	bool local_ready_ = false;
	bool remote_ready_ = false;
};

} // namespace th03::app

#endif
