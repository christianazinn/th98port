#include "th03/app/new_classic_netplay_room.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

namespace {

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

void put_u16(std::vector<std::byte> &bytes, std::size_t at, std::uint16_t value)
{
	bytes[at] = static_cast<std::byte>(value & 0xFFU);
	bytes[at + 1U] = static_cast<std::byte>(value >> 8U);
}

std::vector<std::byte> font_fixture()
{
	std::vector<std::byte> bytes(3'168U, std::byte{0U});
	constexpr std::array magic{'T', '3', 'P', 'F', 'N', 'T', '1'};
	for(std::size_t index = 0U; index < magic.size(); ++index) {
		bytes[index] = static_cast<std::byte>(magic[index]);
	}
	put_u16(bytes, 8U, 3'168U);
	put_u16(bytes, 10U, 32U);
	put_u16(bytes, 12U, 32U);
	put_u16(bytes, 14U, 128U);
	put_u16(bytes, 16U, 95U);
	bytes[18U] = std::byte{0x20U};
	bytes[19U] = std::byte{0x7EU};
	bytes[20U] = std::byte{16U};
	bytes[21U] = std::byte{16U};
	bytes[22U] = std::byte{0x3FU};
	for(std::size_t glyph = 0U; glyph < 95U; ++glyph) {
		bytes[32U + glyph] = std::byte{8U};
		bytes[128U + (glyph * 32U)] = std::byte{0x80U};
	}
	std::uint16_t checksum = 0U;
	for(std::size_t index = 32U; index < bytes.size(); ++index) {
		checksum = static_cast<std::uint16_t>(
			checksum + std::to_integer<std::uint8_t>(bytes[index])
		);
	}
	put_u16(bytes, 24U, checksum);
	return bytes;
}

std::shared_ptr<const th03::app::NewClassicReplayPatchAssets> assets_fixture()
{
	auto decoded = th03::app::detail::decode_new_classic_menu_font(font_fixture());
	if(!decoded) {
		return {};
	}
	auto assets = std::make_shared<th03::app::NewClassicReplayPatchAssets>();
	assets->menu_font = std::move(*decoded);
	auto &background = assets->replay_list_background;
	background.palette.emplace();
	for(std::size_t index = 0U; index < 16U; ++index) {
		background.palette->colors[index] = {
			static_cast<std::uint8_t>(index),
			static_cast<std::uint8_t>(index),
			static_cast<std::uint8_t>(index),
		};
	}
	background.image.width = 640U;
	background.image.height = 400U;
	background.image.indices.assign(256'000U, 2U);
	return assets;
}

void release(th03::app::NewClassicNetplayRoom &room)
{
	static_cast<void>(room.consume({}));
}

void test_host_intent_is_player_lock_gated()
{
	using namespace th03::app;
	NewClassicNetplayRoom room(assets_fixture());
	expect(room.valid() && room.surface().pixels.size() == 256'000U &&
		!room.surface().host_overlay,
		"room menu is an authenticated source-domain surface");
	auto held = room.consume({.ui = {.confirm = true}});
	expect(held.status == NewClassicNetplayRoomStatus::running &&
		!room.take_launch(),
		"held VS confirmation cannot create a network launch intent");
	release(room);
	static_cast<void>(room.consume({.ui = {.down = true}}));
	release(room);
	auto entered = room.consume({.ui = {.confirm = true}});
	expect(entered.surface_changed && !room.take_launch(),
		"Create Room first enters an offline player-lock owner");
	release(room);
	static_cast<void>(room.consume({.ui = {.right = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.down = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.right = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.down = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.right = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.down = true}}));
	release(room);
	auto launched = room.consume({.ui = {.confirm = true}});
	expect(!room.resume_after_launch(),
		"a launch owner cannot resume before its intent is consumed");
	auto intent = room.take_launch();
	expect(launched.status == NewClassicNetplayRoomStatus::launch_requested &&
		intent && intent->mode == NewClassicNetplayRoomLaunchMode::host &&
		intent->transport == NewClassicNetplayRoomTransport::online_service &&
		!intent->relay_only &&
		intent->host_port == 31'415U && intent->join_endpoint.empty() &&
		intent->character == 2U && intent->palette == 2U && intent->rank == 2U &&
		intent->input_delay_frames == 0U,
		"online Create publishes directly after the player lock");
	expect(!room.take_launch(), "launch intent is single-consumer");
	expect(room.show_launch_failure() &&
		room.status() == NewClassicNetplayRoomStatus::running,
		"a pre-game host failure stays inside the retained room owner");
	release(room);
	const auto dismissed = room.consume({.ui = {.bomb = true}});
	expect(dismissed.surface_changed && dismissed.status ==
		NewClassicNetplayRoomStatus::running,
		"any fresh failure-screen input restores host player setup");
	release(room);
	const auto retry = room.consume({.ui = {.confirm = true}});
	const auto retry_intent = room.take_launch();
	expect(retry.status == NewClassicNetplayRoomStatus::launch_requested &&
		retry_intent && retry_intent->mode ==
			NewClassicNetplayRoomLaunchMode::host,
		"the restored Continue row retries without a redundant code page");
	expect(room.resume_after_launch() &&
		room.status() == NewClassicNetplayRoomStatus::running,
		"a canceled Create Room connection restores its caller");
	release(room);
	const auto host_back = room.consume({.ui = {.cancel = true}});
	expect(host_back.status == NewClassicNetplayRoomStatus::running,
		"Cancel from restored Create Room returns one screen, not title");
}

void test_join_entry_and_validation()
{
	using namespace th03::app;
	NewClassicNetplayRoom room(assets_fixture());
	release(room);
	for(std::uint8_t step = 0U; step < 2U; ++step) {
		static_cast<void>(room.consume({.ui = {.down = true}}));
		release(room);
	}
	static_cast<void>(room.consume({.ui = {.confirm = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.down = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.down = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.confirm = true}}));
	release(room);
	const auto invalid = room.consume({.ui = {.confirm = true}});
	expect(invalid.status == NewClassicNetplayRoomStatus::running &&
		invalid.surface_changed && !room.take_launch(),
		"empty Join endpoint is visibly rejected without networking");
	release(room);
	expect(room.text_entry_active(),
		"Join Room enables physical SDL text input only inside its editor");
	constexpr std::string_view endpoint = "2AXCD5";
	for(const char character : endpoint) {
		const auto typed = room.consume({
			.ui = {.bomb = character == 'X'},
			.text_character = character,
		});
		expect(typed.surface_changed,
			"each accepted endpoint character redraws the local editor");
	}
	static_cast<void>(room.consume({.erase = true}));
	static_cast<void>(room.consume({.text_character = '5'}));
	const auto connected = room.consume({.ui = {.confirm = true}});
	auto intent = room.take_launch();
	expect(connected.status == NewClassicNetplayRoomStatus::launch_requested &&
		intent && intent->mode == NewClassicNetplayRoomLaunchMode::join &&
		intent->transport == NewClassicNetplayRoomTransport::online_service &&
		!intent->relay_only &&
		intent->join_endpoint == endpoint && intent->character == 1U &&
		intent->palette == 2U,
		"validated room code becomes one typed join intent");
	expect(room.show_launch_failure() &&
		room.status() == NewClassicNetplayRoomStatus::running,
		"a pre-game Join failure stays inside the retained room owner");
	release(room);
	const auto dismissed = room.consume({.ui = {.confirm = true}});
	expect(dismissed.surface_changed && room.text_entry_active(),
		"dismissing Join failure restores the populated room-code editor");
	release(room);
	const auto retried = room.consume({.ui = {.confirm = true}});
	const auto retry_intent = room.take_launch();
	expect(retried.status == NewClassicNetplayRoomStatus::launch_requested &&
		retry_intent && retry_intent->join_endpoint == endpoint,
		"the preserved room code can be retried without re-entry");
	expect(room.resume_after_launch() && room.status() ==
		NewClassicNetplayRoomStatus::running,
		"a canceled Join Room connection restores its editor");
	release(room);
	const auto join_back = room.consume({.ui = {.cancel = true}});
	expect(join_back.status == NewClassicNetplayRoomStatus::running,
		"Cancel from restored Join Room returns one screen, not title");
}

void test_relay_only_launch_intent()
{
	using namespace th03::app;
	NewClassicNetplayRoom room(assets_fixture());
	release(room);
	for(std::uint8_t step = 0U; step < 3U; ++step) {
		static_cast<void>(room.consume({.ui = {.down = true}}));
		release(room);
	}
	static_cast<void>(room.consume({.ui = {.confirm = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.down = true}}));
	release(room);
	expect(room.consume({.ui = {.right = true}}).surface_changed,
		"Online Connection toggles to Relay Only");
	release(room);
	static_cast<void>(room.consume({.ui = {.cancel = true}}));
	release(room);
	for(std::uint8_t step = 0U; step < 2U; ++step) {
		static_cast<void>(room.consume({.ui = {.up = true}}));
		release(room);
	}
	static_cast<void>(room.consume({.ui = {.confirm = true}}));
	release(room);
	for(std::uint8_t step = 0U; step < 3U; ++step) {
		static_cast<void>(room.consume({.ui = {.down = true}}));
		release(room);
	}
	const auto launched = room.consume({.ui = {.confirm = true}});
	const auto intent = room.take_launch();
	expect(launched.status == NewClassicNetplayRoomStatus::launch_requested &&
		intent && intent->transport ==
			NewClassicNetplayRoomTransport::online_service && intent->relay_only,
		"Relay Only survives confirmation into the typed network intent");
}

void test_direct_endpoint_uses_physical_text()
{
	using namespace th03::app;
	NewClassicNetplayRoom room(assets_fixture());
	release(room);
	for(std::uint8_t step = 0U; step < 3U; ++step) {
		static_cast<void>(room.consume({.ui = {.down = true}}));
		release(room);
	}
	static_cast<void>(room.consume({.ui = {.confirm = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.right = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.cancel = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.up = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.confirm = true}}));
	release(room);
	for(std::uint8_t step = 0U; step < 2U; ++step) {
		static_cast<void>(room.consume({.ui = {.down = true}}));
		release(room);
	}
	static_cast<void>(room.consume({.ui = {.confirm = true}}));
	release(room);
	expect(room.text_entry_active(),
		"Direct Join enters the same physical text-input owner");
	constexpr std::string_view endpoint = "myxhost:31415";
	for(const char character : endpoint) {
		static_cast<void>(room.consume({.text_character = character}));
	}
	static_cast<void>(room.consume({.text_character = ';'}));
	const auto connected = room.consume({.ui = {.confirm = true}});
	const auto intent = room.take_launch();
	expect(connected.status == NewClassicNetplayRoomStatus::launch_requested &&
		intent && intent->transport == NewClassicNetplayRoomTransport::direct_udp &&
		intent->join_endpoint == endpoint,
		"X and shifted colon survive Direct Join while semicolon is rejected");
}

void test_direct_host_keeps_socket_confirmation()
{
	using namespace th03::app;
	NewClassicNetplayRoom room(
		assets_fixture(), NewClassicLanguageAssetProfile::english, 1U,
		{.connection = NewClassicNetplayConnection::direct_udp}
	);
	release(room);
	static_cast<void>(room.consume({.ui = {.down = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.confirm = true}}));
	release(room);
	for(std::uint8_t step = 0U; step < 3U; ++step) {
		static_cast<void>(room.consume({.ui = {.down = true}}));
		release(room);
	}
	const auto confirmation = room.consume({.ui = {.confirm = true}});
	expect(confirmation.surface_changed && confirmation.status ==
		NewClassicNetplayRoomStatus::running && !room.take_launch(),
		"Direct UDP Create retains its explicit socket confirmation");
	release(room);
	const auto launched = room.consume({.ui = {.confirm = true}});
	const auto intent = room.take_launch();
	expect(launched.status == NewClassicNetplayRoomStatus::launch_requested &&
		intent && intent->mode == NewClassicNetplayRoomLaunchMode::host &&
		intent->transport == NewClassicNetplayRoomTransport::direct_udp,
		"Direct UDP confirmation publishes the explicit host intent");
}

void test_settings_back_and_application_quit()
{
	using namespace th03::app;
	NewClassicNetplayRoom room(assets_fixture());
	release(room);
	for(std::uint8_t step = 0U; step < 3U; ++step) {
		static_cast<void>(room.consume({.ui = {.down = true}}));
		release(room);
	}
	static_cast<void>(room.consume({.ui = {.confirm = true}}));
	release(room);
	const auto transport = room.consume({.ui = {.right = true}});
	expect(transport.surface_changed,
		"Network Settings explicitly exposes the direct UDP fallback");
	release(room);
	static_cast<void>(room.consume({.ui = {.down = true}}));
	release(room);
	const auto ignored = room.consume({.ui = {.right = true}});
	expect(!ignored.surface_changed && room.preferences().direct_udp_port ==
		31'415U,
		"Direct host port ignores impractical left/right adjustment");
	const auto editing = room.consume({.ui = {.confirm = true}});
	expect(editing.surface_changed && room.text_entry_active(),
		"Direct host port opens a bounded numeric editor");
	release(room);
	for(std::uint8_t digit = 0U; digit < 5U; ++digit) {
		static_cast<void>(room.consume({.erase = true}));
	}
	static_cast<void>(room.consume({.text_character = 'X'}));
	for(const char digit : std::string_view{"80"}) {
		static_cast<void>(room.consume({.text_character = digit}));
	}
	const auto invalid_port = room.consume({.ui = {.confirm = true}});
	expect(invalid_port.surface_changed && room.text_entry_active() &&
		room.preferences().direct_udp_port == 31'415U,
		"letters are ignored and a privileged direct port is visibly rejected");
	release(room);
	expect(!room.consume({.text_character = 'X'}).surface_changed,
		"invalid letters cannot clear or redraw the numeric-port error state");
	for(std::uint8_t digit = 0U; digit < 2U; ++digit) {
		static_cast<void>(room.consume({.erase = true}));
	}
	for(const char digit : std::string_view{"42424"}) {
		static_cast<void>(room.consume({.text_character = digit}));
	}
	const auto accepted_port = room.consume({.ui = {.confirm = true}});
	expect(accepted_port.surface_changed && !room.text_entry_active() &&
		room.preferences().direct_udp_port == 42'424U,
		"valid numeric direct host port commits without thousands of key presses");
	release(room);
	static_cast<void>(room.consume({.ui = {.down = true}}));
	release(room);
	const auto delayed = room.consume({.ui = {.right = true}});
	expect(delayed.surface_changed,
		"Network Settings adjusts the symmetric gameplay delay locally");
	expect(room.preferences() == NewClassicNetplayPreferences{
		.connection = NewClassicNetplayConnection::direct_udp,
		.relay_only = false,
		.direct_udp_port = 42'424U,
		.input_delay_frames = 1U,
	}, "Network Settings exposes its complete persistent preference value");
	release(room);
	static_cast<void>(room.consume({.ui = {.cancel = true}}));
	release(room);
	const auto back = room.consume({.ui = {.cancel = true}});
	expect(back.status == NewClassicNetplayRoomStatus::back,
		"room menu Cancel returns to New Classic without a socket");

	NewClassicNetplayRoom quitting(assets_fixture());
	const auto quit = quitting.consume({
		.ui = {.app_quit_requested = true},
	});
	expect(quit.status == NewClassicNetplayRoomStatus::application_quit,
		"application quit remains unconditional in every room phase");
}

void test_quick_match_intent()
{
	using namespace th03::app;
	const NewClassicNetplayPreferences stored{
		.connection = NewClassicNetplayConnection::direct_udp,
		.relay_only = true,
		.direct_udp_port = 42'424U,
		.input_delay_frames = 4U,
	};
	NewClassicNetplayRoom room(
		assets_fixture(), NewClassicLanguageAssetProfile::english, 1U, stored
	);
	expect(room.valid() && room.preferences() == stored,
		"room frontend starts from every persisted network preference");
	release(room);
	static_cast<void>(room.consume({.ui = {.confirm = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.right = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.down = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.right = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.down = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.right = true}}));
	release(room);
	static_cast<void>(room.consume({.ui = {.down = true}}));
	release(room);
	const auto launched = room.consume({.ui = {.confirm = true}});
	const auto intent = room.take_launch();
	expect(launched.status == NewClassicNetplayRoomStatus::launch_requested &&
		intent && intent->mode == NewClassicNetplayRoomLaunchMode::quick_match &&
		intent->transport == NewClassicNetplayRoomTransport::online_service &&
		intent->join_endpoint.empty() && intent->character == 2U &&
		intent->palette == 2U && intent->rank == 2U &&
		intent->input_delay_frames == 4U,
		"Quick Match publishes one server-assigned online launch intent");
	expect(room.preferences() == stored,
		"Quick Match does not overwrite the player's Direct UDP preference");
	expect(room.resume_after_launch() &&
		room.status() == NewClassicNetplayRoomStatus::running,
		"a canceled or failed Quick Match restores player setup");
	release(room);
	const auto quick_back = room.consume({.ui = {.cancel = true}});
	expect(quick_back.status == NewClassicNetplayRoomStatus::running,
		"Cancel from restored Quick Match returns to its room root");
}

void test_missing_assets_fail_closed()
{
	th03::app::NewClassicNetplayRoom room({});
	expect(!room.valid() && room.status() ==
		th03::app::NewClassicNetplayRoomStatus::back,
		"room UI has no host-font fallback when authenticated art is absent");
	th03::app::NewClassicNetplayRoom invalid(
		assets_fixture(), th03::app::NewClassicLanguageAssetProfile::english, 1U,
		{.direct_udp_port = 80U}
	);
	expect(!invalid.valid(),
		"room UI rejects preferences outside the persisted port domain");
}

void test_waiting_and_ready_lobby_views()
{
	using namespace th03::app;
	NewClassicNetplayLobbyView view(
		assets_fixture(), NewClassicLanguageAssetProfile::english,
		NewClassicNetplayLobbyViewRole::host,
		{.character = 2U, .palette = 1U}, 2U
	);
	expect(view.valid() && view.surface().pixels.size() == 256'000U &&
		!view.surface().host_overlay,
		"host handshake starts on an authenticated waiting surface");
	const std::vector waiting_pixels(
		view.surface().pixels.begin(), view.surface().pixels.end()
	);
	expect(view.mark_room_code("2ABCD5") && !std::equal(
		waiting_pixels.begin(), waiting_pixels.end(), view.surface().pixels.begin()
	), "the service-assigned room code is visible on the waiting surface");
	expect(!view.mark_room_code("BAD0DE"),
		"waiting presentation rejects an invalid room code");
	const NewClassicNetplayMatchContract contract{
		.player_one = {.character = 2U, .palette = 1U},
		.player_two = {.character = 7U, .palette = 2U},
		.rank = 2U,
		.seed = 0x1234'5678U,
		.input_delay_frames = 3U,
	};
	expect(view.mark_ready(contract) && !std::equal(
		waiting_pixels.begin(), waiting_pixels.end(), view.surface().pixels.begin()
	), "the accepted synchronized contract redraws a distinct ready surface");
	auto invalid = contract;
	invalid.player_two.character = 0U;
	expect(!view.mark_ready(invalid),
		"ready presentation rejects an invalid peer character lock");

	NewClassicNetplayLobbyView missing(
		{}, NewClassicLanguageAssetProfile::english,
		NewClassicNetplayLobbyViewRole::join,
		{.character = 1U, .palette = 2U}, 1U
	);
	expect(!missing.valid(),
		"waiting presentation has no unauthenticated host-font fallback");

	NewClassicNetplayLobbyView matching(
		assets_fixture(), NewClassicLanguageAssetProfile::english,
		NewClassicNetplayLobbyViewRole::matchmaking,
		{.character = 4U, .palette = 2U}, 3U
	);
	expect(matching.valid() && matching.mark_room_code("2ABCDE"),
		"Quick Match waiting view accepts the server-assigned display code");

	NewClassicNetplayLobbyView direct(
		assets_fixture(), NewClassicLanguageAssetProfile::english,
		NewClassicNetplayLobbyViewRole::host,
		{.character = 1U, .palette = 1U}, 1U,
		static_cast<std::uint16_t>(42'424U)
	);
	expect(direct.valid() && !std::equal(
		waiting_pixels.begin(), waiting_pixels.end(), direct.surface().pixels.begin()
	), "Direct host waiting surface displays the join port and instructions");
	NewClassicNetplayLobbyView invalid_port(
		assets_fixture(), NewClassicLanguageAssetProfile::english,
		NewClassicNetplayLobbyViewRole::host,
		{.character = 1U, .palette = 1U}, 1U,
		static_cast<std::uint16_t>(80U)
	);
	expect(!invalid_port.valid(),
		"Direct waiting surface rejects a port outside the public editor domain");
}

void test_match_ready_audio_contract()
{
	using namespace th03::app;
	expect(new_classic_netplay_match_ready_audio_commands ==
		std::array<ClassicAudioCommand, 3U>{{
			{
				.type = ClassicAudioCommandType::stop_song,
				.cue = ClassicMusicCue::menu_main,
			},
			{
				.type = ClassicAudioCommandType::load_song,
				.cue = ClassicMusicCue::winner,
			},
			{
				.type = ClassicAudioCommandType::play_song,
				.cue = ClassicMusicCue::winner,
			},
		}}, "match-ready audio immediately replaces OP.M with WIN.M");
	expect(new_classic_netplay_menu_audio_restore_commands ==
		std::array<ClassicAudioCommand, 3U>{{
			{
				.type = ClassicAudioCommandType::stop_song,
			},
			{
				.type = ClassicAudioCommandType::load_song,
				.cue = ClassicMusicCue::menu_main,
			},
			{
				.type = ClassicAudioCommandType::play_song,
				.cue = ClassicMusicCue::menu_main,
			},
		}}, "canceled match-ready audio restores the title cue from a typed stop");
	NewClassicNetplayMatchReadyAudioGate gate{};
	gate.advance({.bgm_active = true, .song_playing = false});
	expect(!gate.complete && !gate.observed_jingle,
		"match-ready cannot finish before the jingle starts");
	gate.advance({.bgm_active = true, .song_playing = true});
	expect(!gate.complete && gate.observed_jingle,
		"match-ready latches actual PMD playback");
	gate.advance({.bgm_active = true, .song_playing = false});
	expect(gate.complete,
		"match-ready finishes only after the observed jingle stops");
	NewClassicNetplayMatchReadyAudioGate silent{};
	silent.advance({.bgm_active = false});
	expect(silent.complete,
		"Music Off cannot strand the match-ready transition");
}

} // namespace

int main()
{
	test_host_intent_is_player_lock_gated();
	test_join_entry_and_validation();
	test_relay_only_launch_intent();
	test_direct_endpoint_uses_physical_text();
	test_direct_host_keeps_socket_confirmation();
	test_settings_back_and_application_quit();
	test_quick_match_intent();
	test_missing_assets_fail_closed();
	test_waiting_and_ready_lobby_views();
	test_match_ready_audio_contract();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
