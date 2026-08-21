#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <variant>

#include "th03/app/new_classic_netplay_identity.hpp"
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
		"{\"protocol\":\"T3ROOM3\",\"type\":\"welcome\","
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
		"{\"protocol\":\"T3ROOM3\",\"type\":\"paired\","
		"\"p1UserId\":\"01234567-89ab-cdef-0123-456789abcdef\","
		"\"p1Nametag\":\"Resident of Fantasy Who Runs Through Time\","
		"\"p2UserId\":\"fedcba98-7654-3210-fedc-ba9876543210\","
		"\"p2Nametag\":\"Second Player\"}"
	);
	check(paired && std::holds_alternative<NewClassicNetplayRoomPaired>(*paired),
		"paired decodes");
	check(paired && std::get<NewClassicNetplayRoomPaired>(*paired).
		player_one.nametag.lines == std::array<std::string, 2U>{
			"Resident of Fantasy Who Runs", "Through Time",
		}, "paired profile uses the exact title-row layout");
	check(paired && std::get<NewClassicNetplayRoomPaired>(*paired).
		player_two.user_id == "fedcba98-7654-3210-fedc-ba9876543210",
		"paired profile preserves the stable P2 UUID");
	const auto identity = paired ? encode_new_classic_netplay_identity({
		.room_id = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",
		.profiles = std::get<NewClassicNetplayRoomPaired>(*paired),
	}) : std::expected<
		std::array<std::byte, new_classic_netplay_identity_bytes>,
		NewClassicNetplayIdentityError
	>{std::unexpected(NewClassicNetplayIdentityError::invalid_profile)};
	const auto identity_digest = paired ? new_classic_netplay_identity_digest({
		.room_id = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",
		.profiles = std::get<NewClassicNetplayRoomPaired>(*paired),
	}) : std::expected<AppSha256, NewClassicNetplayIdentityError>{
		std::unexpected(NewClassicNetplayIdentityError::invalid_profile)
	};
	check(identity && (*identity)[0U] == std::byte{'T'} &&
		(*identity)[7U] == std::byte{'1'} && (*identity)[8U] == std::byte{1U} &&
		(*identity)[9U] == std::byte{41U} &&
		(*identity)[10U] == std::byte{13U},
		"T3NID001 freezes the account kind and exact nametag lengths");
	check(identity && identity_digest && std::equal(
		identity->begin() + 192U, identity->end(), identity_digest->begin(),
		[](std::byte left, std::uint8_t right) {
			return std::to_integer<std::uint8_t>(left) == right;
		}
	), "T3NID001 carries the complete-body SHA-256 used by peer agreement");
	if(paired && identity_digest) {
		const auto other_room = new_classic_netplay_identity_digest({
			.room_id = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeef",
			.profiles = std::get<NewClassicNetplayRoomPaired>(*paired),
		});
		check(other_room && *other_room != *identity_digest,
			"the same profiles cannot reuse identity proof across rooms");
	}
	const auto departed = decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM3\",\"type\":\"peer-left\",\"player\":2}"
	);
	check(departed && std::get<NewClassicNetplayRoomPeerLeft>(*departed).player ==
		2U, "peer-left decodes");

	check(valid_new_classic_netplay_room_code("2ABCDE"), "valid room code");
	check(!valid_new_classic_netplay_room_code("1ABCDE"), "rejects one");
	check(!valid_new_classic_netplay_room_code("OABCDE"), "rejects O");
	check(!decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM3\",\"type\":\"paired\",\"extra\":0}"
	), "rejects extensions");
	check(!decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM1\",\"type\":\"paired\"}"
	), "rejects protocol mismatch");
	check(!decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM3\",\"type\":\"ice\","
		"\"sequence\":4294967296,\"payload\":\"x\"}"
	), "rejects sequence overflow");
	check(!decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM3\",\"type\":\"paired\","
		"\"protocol\":\"T3ROOM3\"}"
	), "rejects duplicate keys");
	check(!decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM3\",\"type\":\"paired\"} trailing"
	), "rejects trailing bytes");
	check(!decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM3\",\"type\":\"welcome\","
		"\"code\":\"2ABCDE\","
		"\"roomId\":\"01234567-89ab-cdef-0123-456789abcdef\","
		"\"player\":1,\"expiresAtUnixMs\":1,\"stunUrls\":\"\","
		"\"turnUrls\":\"turn:turn.example.test:3478\","
		"\"turnUsername\":\"user\",\"turnCredential\":\"\"}"
	), "rejects incomplete TURN credentials");
	check(!decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM3\",\"type\":\"paired\","
		"\"p1UserId\":\"01234567-89ab-cdef-0123-456789abcdef\","
		"\"p1Nametag\":\"One\","
		"\"p2UserId\":\"01234567-89ab-cdef-0123-456789abcdef\","
		"\"p2Nametag\":\"Two\"}"
	), "rejects one account occupying both paired seats");
	check(!decode_new_classic_netplay_room_message(
		"{\"protocol\":\"T3ROOM3\",\"type\":\"paired\","
		"\"p1UserId\":\"01234567-89ab-cdef-0123-456789abcdef\","
		"\"p1Nametag\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAA\","
		"\"p2UserId\":\"fedcba98-7654-3210-fedc-ba9876543210\","
		"\"p2Nametag\":\"Two\"}"
	), "rejects a paired nametag outside the title geometry");
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
