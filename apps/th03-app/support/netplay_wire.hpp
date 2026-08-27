#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "th03/app/new_classic/netplay/new_classic_netplay_datagram.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_lobby.hpp"
#include "th03/platform/rtc_room.hpp"
#include "th03/platform/udp_socket.hpp"

namespace th03::app::launcher_support {

enum class NetplayWireChannel : std::uint8_t {
	gameplay,
	control,
};

struct NetplayWireMessage final {
	NetplayWireChannel channel = NetplayWireChannel::control;
	std::vector<std::byte> bytes{};
};

class NetplayWire final {
public:
	[[nodiscard]] static NetplayWire udp(
		std::unique_ptr<platform::UdpSocket> socket,
		platform::UdpIpv4Endpoint peer
	) noexcept;

#if defined(TH03_ENABLE_RTC_TRANSPORT)
	[[nodiscard]] static NetplayWire rtc(
		std::unique_ptr<platform::RtcRoom> room
	) noexcept;
#endif

	NetplayWire(NetplayWire &&) noexcept;
	NetplayWire &operator=(NetplayWire &&) noexcept;
	NetplayWire(const NetplayWire &) = delete;
	NetplayWire &operator=(const NetplayWire &) = delete;
	~NetplayWire();

	[[nodiscard]] bool send(
		NetplayWireChannel channel,
		std::span<const std::byte> bytes
	) noexcept;

	[[nodiscard]] std::expected<
		std::optional<NetplayWireMessage>,
		const char *
	> receive() noexcept;

	[[nodiscard]] bool rtc() const noexcept;

	[[nodiscard]] std::optional<std::uint64_t>
	round_trip_time_ms() const noexcept;

private:
	NetplayWire() = default;
	std::unique_ptr<platform::UdpSocket> udp_socket_{};
	std::optional<platform::UdpIpv4Endpoint> udp_peer_{};
#if defined(TH03_ENABLE_RTC_TRANSPORT)
	std::unique_ptr<platform::RtcRoom> rtc_room_{};
#endif
};

[[nodiscard]] bool send_netplay_datagram(
	NetplayWire &wire,
	const NewClassicNetplayDatagram &datagram
) noexcept;

[[nodiscard]] bool send_netplay_lobby_datagram(
	platform::UdpSocket &socket,
	platform::UdpIpv4Endpoint peer,
	const NewClassicNetplayLobbyDatagram &datagram
);

#if defined(TH03_ENABLE_RTC_TRANSPORT)
[[nodiscard]] bool send_netplay_lobby_wire(
	NetplayWire &wire,
	const NewClassicNetplayLobbyDatagram &datagram
) noexcept;
#endif

} // namespace th03::app::launcher_support
