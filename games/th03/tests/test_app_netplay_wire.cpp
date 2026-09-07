#include "support/netplay_wire.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string_view>
#include <thread>

namespace {

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::fprintf(
			stderr, "FAIL: %.*s\n",
			static_cast<int>(message.size()), message.data()
		);
	}
}

[[nodiscard]] th03::platform::UdpIpv4Endpoint loopback(
	const th03::platform::UdpSocket &socket
)
{
	return {
		.address = {127U, 0U, 0U, 1U},
		.port = socket.local_endpoint().port,
	};
}

void test_udp_wire()
{
	using namespace th03::app;
	using namespace th03::app::launcher_support;
	using th03::platform::UdpSocket;

	auto wire_socket = UdpSocket::bind(0U);
	auto peer_socket = UdpSocket::bind(0U);
	auto other_socket = UdpSocket::bind(0U);
	expect(wire_socket && peer_socket && other_socket,
		"wire test binds three loopback sockets");
	if(!wire_socket || !peer_socket || !other_socket) {
		return;
	}
	const auto wire_endpoint = loopback(**wire_socket);
	const auto peer_endpoint = loopback(**peer_socket);
	auto wire = NetplayWire::udp(std::move(*wire_socket), peer_endpoint);
	expect(!wire.rtc() && !wire.round_trip_time_ms(),
		"UDP wire does not claim RTC telemetry");

	const NewClassicNetplayDatagram hello{
		.type = NewClassicNetplayDatagramType::hello,
		.sender = NewClassicNetplayPlayer::player_two,
		.contract_hash = 23U,
	};
	expect(send_netplay_datagram(wire, hello),
		"gameplay helper encodes and sends through the wire");
	std::array<std::byte, new_classic_netplay_datagram_bytes> gameplay{};
	std::optional<th03::platform::UdpReceivedDatagram> received_gameplay{};
	for(std::size_t attempt = 0U; attempt < 100U && !received_gameplay;
		++attempt) {
		auto received = (*peer_socket)->receive(gameplay);
		expect(received.has_value(), "peer gameplay receive remains operational");
		if(!received) {
			break;
		}
		received_gameplay = *received;
		if(!received_gameplay) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	const auto decoded_gameplay = received_gameplay ?
		decode_new_classic_netplay_datagram(gameplay) :
		std::expected<NewClassicNetplayDatagram,
			NewClassicNetplayDatagramError>{std::unexpected(
				NewClassicNetplayDatagramError{}
			)};
	expect(decoded_gameplay && decoded_gameplay->type == hello.type &&
		decoded_gameplay->session_id == hello.session_id &&
		decoded_gameplay->contract_hash == hello.contract_hash,
		"wire preserves an encoded gameplay datagram");

	auto join = NewClassicNetplayLobby::create_join({
		.player_two = {.character = 2U, .palette = 2U},
	});
	const auto lobby = join ? join->outbound() : std::nullopt;
	expect(lobby.has_value(), "join lobby produces its initial request");
	if(!lobby) {
		return;
	}
	expect(send_netplay_lobby_datagram(**peer_socket, wire_endpoint, *lobby),
		"lobby helper sends an encoded control datagram");
	std::expected<std::optional<NetplayWireMessage>, const char *> received_lobby{
		std::optional<NetplayWireMessage>{}
	};
	for(std::size_t attempt = 0U;
		attempt < 100U && received_lobby && !*received_lobby; ++attempt) {
		received_lobby = wire.receive();
		if(received_lobby && !*received_lobby) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}
	const auto encoded_lobby = lobby ?
		encode_new_classic_netplay_lobby_datagram(*lobby) :
		std::expected<
			std::array<std::byte, new_classic_netplay_lobby_bytes>,
			NewClassicNetplayLobbyDatagramError
		>{std::unexpected(NewClassicNetplayLobbyDatagramError{})};
	expect(received_lobby && *received_lobby && encoded_lobby &&
		(**received_lobby).channel == NetplayWireChannel::control &&
		(**received_lobby).bytes.size() == encoded_lobby->size() &&
		std::equal(
			(**received_lobby).bytes.begin(), (**received_lobby).bytes.end(),
			encoded_lobby->begin()
		), "wire classifies and preserves a lobby control datagram");

	expect(send_netplay_lobby_datagram(
		**other_socket, wire_endpoint, *lobby
	), "unselected peer can place a packet on the socket");
	std::this_thread::sleep_for(std::chrono::milliseconds(2));
	const auto ignored = wire.receive();
	expect(ignored && !*ignored,
		"wire consumes and rejects a datagram from the wrong peer");
}

} // namespace

int main()
{
	test_udp_wire();
	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return 1;
	}
	std::printf("all %zu launcher netplay wire checks passed\n", checks);
	return 0;
}
