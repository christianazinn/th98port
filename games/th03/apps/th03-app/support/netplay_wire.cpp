#include "netplay_wire.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <utility>

namespace th03::app::launcher_support {

NetplayWire NetplayWire::udp(
	std::unique_ptr<platform::UdpSocket> socket,
	platform::UdpIpv4Endpoint peer
) noexcept
{
	NetplayWire result{};
	result.udp_socket_ = std::move(socket);
	result.udp_peer_ = peer;
	return result;
}

#if defined(TH03_ENABLE_RTC_TRANSPORT)
NetplayWire NetplayWire::rtc(std::unique_ptr<platform::RtcRoom> room) noexcept
{
	NetplayWire result{};
	result.rtc_room_ = std::move(room);
	return result;
}
#endif

NetplayWire::NetplayWire(NetplayWire &&) noexcept = default;
NetplayWire &NetplayWire::operator=(NetplayWire &&) noexcept = default;
NetplayWire::~NetplayWire() = default;

bool NetplayWire::send(
	NetplayWireChannel channel,
	std::span<const std::byte> bytes
) noexcept
{
#if !defined(TH03_ENABLE_RTC_TRANSPORT)
	static_cast<void>(channel);
#endif
	if(udp_socket_) {
		return udp_peer_ && udp_socket_->send(*udp_peer_, bytes).has_value();
	}
#if defined(TH03_ENABLE_RTC_TRANSPORT)
	if(rtc_room_) {
		const auto sent = rtc_room_->send(
			channel == NetplayWireChannel::gameplay ?
				platform::RtcRoomChannel::gameplay :
				platform::RtcRoomChannel::control,
			bytes
		);
		if(!sent) {
			std::cerr << "netplay-rtc-send-code=" <<
				platform::rtc_room_error_code_name(sent.error().code) << '\n';
		}
		return sent.has_value();
	}
#endif
	return false;
}

std::expected<std::optional<NetplayWireMessage>, const char *>
NetplayWire::receive() noexcept
{
	if(udp_socket_) {
		std::array<std::byte, new_classic_netplay_datagram_bytes> buffer{};
		for(;;) {
			auto received = udp_socket_->receive(buffer);
			if(!received) {
				return std::unexpected("socket-failed");
			}
			if(!*received) {
				return std::optional<NetplayWireMessage>{};
			}
			if(!udp_peer_ || (*received)->sender != *udp_peer_) {
				continue;
			}
			std::vector<std::byte> bytes((*received)->size);
			std::ranges::copy(
				std::span(buffer).first((*received)->size), bytes.begin()
			);
			return std::optional<NetplayWireMessage>{NetplayWireMessage{
				.channel = (*received)->size ==
					new_classic_netplay_lobby_bytes ?
					NetplayWireChannel::control : NetplayWireChannel::gameplay,
				.bytes = std::move(bytes),
			}};
		}
	}
#if defined(TH03_ENABLE_RTC_TRANSPORT)
	if(rtc_room_) {
		auto received = rtc_room_->receive();
		if(!received) {
			return std::unexpected(
				platform::rtc_room_error_code_name(received.error().code)
			);
		}
		if(!*received) {
			return std::optional<NetplayWireMessage>{};
		}
		return std::optional<NetplayWireMessage>{NetplayWireMessage{
			.channel = (**received).channel ==
				platform::RtcRoomChannel::gameplay ?
				NetplayWireChannel::gameplay : NetplayWireChannel::control,
			.bytes = std::move((**received).bytes),
		}};
	}
#endif
	return std::unexpected("wire-closed");
}

bool NetplayWire::rtc() const noexcept
{
#if defined(TH03_ENABLE_RTC_TRANSPORT)
	return rtc_room_ != nullptr;
#else
	return false;
#endif
}

std::optional<std::uint64_t> NetplayWire::round_trip_time_ms() const noexcept
{
#if defined(TH03_ENABLE_RTC_TRANSPORT)
	if(rtc_room_) {
		const auto observed = rtc_room_->snapshot().round_trip_time;
		if(observed && observed->count() >= 0 && observed->count() <= 60'000) {
			return static_cast<std::uint64_t>(observed->count());
		}
	}
#endif
	return std::nullopt;
}

bool send_netplay_datagram(
	NetplayWire &wire,
	const NewClassicNetplayDatagram &datagram
) noexcept
{
	const auto encoded = encode_new_classic_netplay_datagram(datagram);
	return encoded && wire.send(
		datagram.type == NewClassicNetplayDatagramType::input ?
			NetplayWireChannel::gameplay : NetplayWireChannel::control,
		*encoded
	);
}

bool send_netplay_lobby_datagram(
	platform::UdpSocket &socket,
	platform::UdpIpv4Endpoint peer,
	const NewClassicNetplayLobbyDatagram &datagram
)
{
	const auto encoded = encode_new_classic_netplay_lobby_datagram(datagram);
	return encoded && socket.send(peer, *encoded).has_value();
}

#if defined(TH03_ENABLE_RTC_TRANSPORT)
bool send_netplay_lobby_wire(
	NetplayWire &wire,
	const NewClassicNetplayLobbyDatagram &datagram
) noexcept
{
	const auto encoded = encode_new_classic_netplay_lobby_datagram(datagram);
	return encoded && wire.send(NetplayWireChannel::control, *encoded);
}
#endif

} // namespace th03::app::launcher_support
