#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "th03/platform/rtc_room.hpp"

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

int main(int argc, char **argv)
{
	using namespace th03::platform;
	if(argc == 4 && std::string_view(argv[1]) == "--enroll") {
		const auto enrolled = load_or_enroll_rtc_online_credentials(
			argv[2], argv[3]
		);
		if(!enrolled) {
			std::cerr << "identity-error=" <<
				static_cast<unsigned>(enrolled.error().code) << '\n';
			return 1;
		}
		std::cout << "identity-user-shape=" << enrolled->user_id.size() <<
			" token-shape=" << enrolled->token.size() << '\n';
		return 0;
	}
	const auto created = rtc_room_websocket_url(
		"https://podd.example", RtcRoomMode::create, ""
	);
	check(created && *created ==
		"wss://podd.example/api/th03/netplay/rooms/new", "create URL");
	const auto joined = rtc_room_websocket_url(
		"https://podd.example:8443", RtcRoomMode::join, "2ABCDE"
	);
	check(joined && *joined ==
		"wss://podd.example:8443/api/th03/netplay/rooms/2ABCDE",
		"join URL");
	const auto matched = rtc_room_websocket_url(
		"https://podd.example", RtcRoomMode::matchmaking, "", 3U
	);
	check(matched && *matched ==
		"wss://podd.example/api/th03/netplay/matchmaking?rank=3",
		"Quick Match URL binds selected rank");
	check(!rtc_room_websocket_url(
		"https://podd.example", RtcRoomMode::matchmaking, "", 4U
	), "rejects Quick Match rank outside source range");
	const auto local = rtc_room_websocket_url(
		"http://127.0.0.1:8787", RtcRoomMode::create, ""
	);
	check(local && local->starts_with("ws://"), "loopback Wrangler URL");
	check(!rtc_room_websocket_url(
		"http://podd.example", RtcRoomMode::create, ""
	), "rejects remote cleartext");
	check(!rtc_room_websocket_url(
		"https://podd.example/path", RtcRoomMode::create, ""
	), "rejects origin path");
	check(!rtc_room_websocket_url(
		"https://podd.example", RtcRoomMode::join, "ABCD0F"
	), "rejects bad room alphabet");
	check(!rtc_room_websocket_url(
		"https://podd.example", RtcRoomMode::create, "2ABCDE"
	), "rejects create room code");

	const std::string user = "01234567-89ab-cdef-0123-456789abcdef";
	const std::string token(43U, 'A');
	const auto protocol = rtc_room_authentication_protocol(user, token);
	check(protocol && *protocol == "t3auth." + user + "." + token,
		"authentication subprotocol");
	check(!rtc_room_authentication_protocol("bad", token),
		"rejects bad user");
	check(!rtc_room_authentication_protocol(user, "bad"),
		"rejects bad token");
	check(std::string(rtc_room_error_code_name(
		RtcRoomErrorCode::tls_ca_unavailable
	)) == "tls-ca-unavailable", "stable missing CA diagnostic");
	check(std::string(rtc_room_error_code_name(
		RtcRoomErrorCode::tls_failed
	)) == "tls-failed", "stable TLS failure diagnostic");
	check(std::string(rtc_room_error_code_name(
		RtcRoomErrorCode::peer_failed
	)) == "peer-failed", "stable peer failure diagnostic");
	check(rtc_room_data_channel_send_accepted(true, true) &&
		rtc_room_data_channel_send_accepted(false, true) &&
		!rtc_room_data_channel_send_accepted(false, false),
		"SCTP buffered send is accepted while the channel remains open");

	const auto scratch = std::filesystem::temp_directory_path() /
		("th03-rtc-identity-" + std::to_string(
			std::chrono::steady_clock::now().time_since_epoch().count()
		));
	std::error_code filesystem_error{};
	std::filesystem::create_directory(scratch, filesystem_error);
	check(!filesystem_error, "identity scratch directory");
	const auto identity_path = scratch / "PODD.AUTH";
	{
		std::ofstream identity(identity_path, std::ios::binary);
		identity << "T3AUTH01\n" << user << '\n' << token << '\n';
	}
	const auto loaded_identity = load_or_enroll_rtc_online_credentials(
		identity_path, "not-an-origin"
	);
	check(loaded_identity && loaded_identity->user_id == user &&
		loaded_identity->token == token,
		"durable identity loads without contacting the service");
	{
		std::ofstream malformed(identity_path, std::ios::binary | std::ios::trunc);
		malformed << "not credentials\n";
	}
	const auto malformed_identity = load_or_enroll_rtc_online_credentials(
		identity_path, "not-an-origin"
	);
	check(!malformed_identity && malformed_identity.error().code ==
		RtcOnlineIdentityErrorCode::format,
		"malformed durable identity fails closed");
	std::filesystem::remove_all(scratch, filesystem_error);
	check(!filesystem_error, "identity scratch cleanup");

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
