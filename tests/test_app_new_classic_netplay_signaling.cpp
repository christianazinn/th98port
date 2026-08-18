#include <cstdint>
#include <iostream>
#include <string>
#include <variant>

#include "th03/app/new_classic_netplay_signaling.hpp"

namespace {

int checks = 0;
int failures = 0;

void check(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

} // namespace

int main()
{
	using namespace th03::app;
	const NewClassicNetplayRoomSignal source{
		.type = NewClassicNetplayRoomSignalType::offer,
		.sequence = 17U,
		.payload = "v=0\r\na=quoted:\"slash\\tab\t\n",
	};
	const auto encoded = encode_new_classic_netplay_room_signal(source);
	check(encoded.has_value(), "signal encodes");
	const auto decoded = encoded ?
		decode_new_classic_netplay_room_message(*encoded) :
		std::expected<NewClassicNetplayRoomMessage,
			NewClassicNetplayRoomMessageError>{std::unexpected(
				NewClassicNetplayRoomMessageError{}
			)};
	check(decoded.has_value(), "signal decodes");
	check(decoded && std::holds_alternative<NewClassicNetplayRoomSignal>(*decoded),
		"signal variant");
	check(decoded && std::get<NewClassicNetplayRoomSignal>(*decoded) == source,
		"signal round trip");

	const auto welcome = decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM2\",\"type\":\"welcome\","
		"\"code\":\"2ABCDE\","
		"\"roomId\":\"01234567-89ab-cdef-0123-456789abcdef\","
		"\"player\":1,\"expiresAtUnixMs\":9007199254740991,"
		"\"stunUrls\":\"stun:stun.example.test:3478\","
		"\"turnUrls\":\"turn:turn.example.test:3478?transport=udp;"
		"turns:turn.example.test:5349?transport=tcp\","
		"\"turnUsername\":\"short-user\","
		"\"turnCredential\":\"short-secret\"}"
	);
	check(welcome.has_value(), "welcome decodes");
	check(welcome && std::get<NewClassicNetplayRoomWelcome>(*welcome).code ==
		"2ABCDE", "welcome room code");
	check(welcome && std::get<NewClassicNetplayRoomWelcome>(*welcome).player ==
		1U, "welcome seat");
	check(welcome && std::get<NewClassicNetplayRoomWelcome>(*welcome).
		ice_servers.size() == 3U, "welcome ICE server count");
	check(welcome && std::get<NewClassicNetplayRoomWelcome>(*welcome).
		ice_servers[0] == NewClassicNetplayIceServer{
			.url = "stun:stun.example.test:3478",
		}, "welcome STUN server");
	check(welcome && std::get<NewClassicNetplayRoomWelcome>(*welcome).
		ice_servers[1] == NewClassicNetplayIceServer{
			.url = "turn:turn.example.test:3478?transport=udp",
			.username = "short-user",
			.credential = "short-secret",
		}, "welcome TURN credentials");

	const auto paired = decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM2\",\"type\":\"paired\"}"
	);
	check(paired && std::holds_alternative<NewClassicNetplayRoomPaired>(*paired),
		"paired decodes");
	const auto departed = decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM2\",\"type\":\"peer-left\",\"player\":2}"
	);
	check(departed && std::get<NewClassicNetplayRoomPeerLeft>(*departed).player ==
		2U, "peer-left decodes");

	check(valid_new_classic_netplay_room_code("2ABCDE"), "valid room code");
	check(!valid_new_classic_netplay_room_code("1ABCDE"), "rejects one");
	check(!valid_new_classic_netplay_room_code("OABCDE"), "rejects O");
	check(!decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM2\",\"type\":\"paired\",\"extra\":0}"
	), "rejects extensions");
	check(!decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM1\",\"type\":\"paired\"}"
	), "rejects protocol mismatch");
	check(!decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM2\",\"type\":\"ice\","
		"\"sequence\":4294967296,\"payload\":\"x\"}"
	), "rejects sequence overflow");
	check(!decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM2\",\"type\":\"paired\","
		"\"protocol\":\"T3ROOM2\"}"
	), "rejects duplicate keys");
	check(!decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM2\",\"type\":\"paired\"} trailing"
	), "rejects trailing bytes");
	check(!decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM2\",\"type\":\"welcome\","
		"\"code\":\"2ABCDE\","
		"\"roomId\":\"01234567-89ab-cdef-0123-456789abcdef\","
		"\"player\":1,\"expiresAtUnixMs\":1,\"stunUrls\":\"\","
		"\"turnUrls\":\"turn:turn.example.test:3478\","
		"\"turnUsername\":\"user\",\"turnCredential\":\"\"}"
	), "rejects incomplete TURN credentials");
	check(!decode_new_classic_netplay_room_message(
		std::string(new_classic_netplay_room_signal_max_bytes + 1U, 'x')
	), "rejects oversized wire");
	check(!encode_new_classic_netplay_room_signal({
		.type = NewClassicNetplayRoomSignalType::ice,
		.sequence = 0U,
		.payload = std::string(
			new_classic_netplay_room_payload_max_bytes + 1U, 'x'
		),
	}), "rejects oversized outbound payload");

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
