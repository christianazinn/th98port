#include "netplay_lobby_internal.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iostream>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_timer.h>

#include "diagnostics.hpp"
#include "runtime_environment.hpp"

#include "th03/app/new_classic/netplay/new_classic_netplay_datagram.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_identity.hpp"
#include "th03/app/runtime/tick_input.hpp"
#include "th03/platform/udp_socket.hpp"

namespace th03::app::launcher_support {
namespace {

constexpr std::uint64_t maximum_idle_delay_ns = 1'000'000U;

[[nodiscard]] std::uint64_t fresh_netplay_nonce() noexcept
{
	static std::uint64_t sequence = 0U;
	auto value = static_cast<std::uint64_t>(
		std::chrono::steady_clock::now().time_since_epoch().count()
	) + (++sequence * 0x9E37'79B9'7F4A'7C15ULL);
	value = (value ^ (value >> 30U)) * 0xBF58'476D'1CE4'E5B9ULL;
	value = (value ^ (value >> 27U)) * 0x94D0'49BB'1331'11EBULL;
	value ^= value >> 31U;
	return value == 0U ? sequence : value;
}

} // namespace

[[nodiscard]] std::uint32_t fresh_netplay_seed() noexcept
{
	static std::uint32_t previous = 0U;
	const auto nonce = fresh_netplay_nonce();
	auto result = static_cast<std::uint32_t>(nonce ^ (nonce >> 32U));
	if(result == 0U || result == previous) {
		++result;
		if(result == 0U) {
			result = 1U;
		}
	}
	previous = result;
	return result;
}

namespace {

[[nodiscard]] th03::app::NewClassicNetplayHostLock netplay_host_lock(
	const th03::app::DirectVsCliRequest &request
) noexcept
{
	return {
		.player_one = {
			.character = static_cast<std::uint8_t>(
				request.launch.process.player_one.character
			),
			.palette = static_cast<std::uint8_t>(
				request.launch.process.player_one.palette
			),
		},
		.player_one_autofire = request.netplay_local_autofire,
		.rank = request.launch.process.rank,
		.seed = request.launch.process.seed,
		.cadence_ticks = request.tick_rate.ticks,
		.cadence_seconds = request.tick_rate.seconds,
		.input_delay_frames = request.netplay_input_delay_frames,
	};
}

[[nodiscard]] th03::app::NewClassicNetplayJoinLock netplay_join_lock(
	const th03::app::DirectVsCliRequest &request
) noexcept
{
	return {
		.player_two = {
			.character = static_cast<std::uint8_t>(
				request.launch.process.player_two.character
			),
			.palette = static_cast<std::uint8_t>(
				request.launch.process.player_two.palette
			),
		},
		.player_two_autofire = request.netplay_local_autofire,
	};
}

enum class DirectNetplayLobbyExit : std::uint8_t {
	none,
	back,
	application_quit,
};

[[nodiscard]] DirectNetplayLobbyExit direct_netplay_lobby_exit(
	SDL_WindowID main_window
) noexcept
{
	SDL_Event event{};
	while(SDL_PollEvent(&event)) {
		if(event.type == SDL_EVENT_QUIT ||
			(event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
			 event.window.windowID == main_window)) {
			return DirectNetplayLobbyExit::application_quit;
		}
		if(event.type == SDL_EVENT_KEY_DOWN &&
			 event.key.windowID == main_window &&
			 event.key.scancode == SDL_SCANCODE_ESCAPE) {
			return DirectNetplayLobbyExit::back;
		}
	}
	return DirectNetplayLobbyExit::none;
}

[[nodiscard]] std::expected<void, int> pump_retained_menu_audio(
	th03::app::TickInputScheduler *scheduler,
	const RetainedMenuAudio *retained_audio
)
{
	if(!scheduler || !retained_audio) {
		return {};
	}
	const auto cutoff = th03::platform::sdl_ticks_now_ns();
	for(;;) {
		auto due = scheduler->pop_due(cutoff);
		if(!due) {
			return std::unexpected(fail_tick(due.error()));
		}
		if(!*due) {
			return {};
		}
		if(const auto advanced = retained_audio->advance_tick(); !advanced) {
			return std::unexpected(advanced.error());
		}
	}
}

template <typename NetworkPump>
[[nodiscard]] std::expected<void, int> hold_netplay_match_ready(
	th03::platform::SdlPlatform &platform,
	th03::app::TickInputScheduler *retained_audio_scheduler,
	const RetainedMenuAudio *retained_audio,
	bool *application_quit,
	NetworkPump &&pump_network
)
{
	if(!retained_audio) {
		return {};
	}
	if(const auto started = retained_audio->begin_match_ready(); !started) {
		return std::unexpected(started.error());
	}
	const auto cancel_audio = [&]() -> std::expected<void, int> {
		return retained_audio->cancel_match_ready();
	};
	const auto fail_after_cancel = [&](int failure) -> std::expected<void, int> {
		if(const auto canceled = cancel_audio(); !canceled) {
			return std::unexpected(canceled.error());
		}
		return std::unexpected(failure);
	};
	std::cout << "status=match-found\nwaiting-for-round-clear-jingle=true\n";
	std::cout.flush();
	for(;;) {
		if(const auto pumped = pump_retained_menu_audio(
			retained_audio_scheduler, retained_audio
		); !pumped) {
			return fail_after_cancel(pumped.error());
		}
		auto complete = retained_audio->match_ready_complete();
		if(!complete) {
			return fail_after_cancel(complete.error());
		}
		if(*complete) {
			return {};
		}
		const auto exit = direct_netplay_lobby_exit(
			static_cast<SDL_WindowID>(platform.window_id())
		);
		if(exit != DirectNetplayLobbyExit::none) {
			if(application_quit &&
				exit == DirectNetplayLobbyExit::application_quit) {
				*application_quit = true;
			}
			std::cout << "status=left\n";
			if(const auto canceled = cancel_audio(); !canceled) {
				return std::unexpected(canceled.error());
			}
			return std::unexpected(0);
		}
		if(const auto network = pump_network(); !network) {
			return fail_after_cancel(network.error());
		}
		if(auto refreshed = refresh_host_presentability(
			platform, "netplay-ready-presentability"
		); !refreshed) {
			return fail_after_cancel(refreshed.error());
		}
		SDL_DelayPrecise(maximum_idle_delay_ns);
	}
}

} // namespace

[[nodiscard]] std::expected<DirectNetplayLobbyResult, int>
negotiate_direct_netplay_lobby(
	const th03::app::DirectVsCliRequest &request,
	th03::platform::SdlPlatform &platform,
	std::shared_ptr<const th03::app::NewClassicReplayPatchAssets> patch_assets,
	const RetainedMenuAudio *retained_audio,
	bool *application_quit
)
{
	using th03::app::DirectVsNetplayMode;
	using th03::app::NewClassicNetplayLobby;
	using th03::app::NewClassicNetplayLobbyDatagramType;
	using th03::app::NewClassicNetplayLobbyStatus;
	std::optional<th03::app::TickInputScheduler> retained_audio_scheduler{};
	if(retained_audio) {
		auto created = th03::app::TickInputScheduler::create(
			request.tick_rate, th03::platform::sdl_ticks_now_ns(),
			th03::app::TickInputCatchUpPolicy::discard_missed_deadlines
		);
		if(!created) {
			return std::unexpected(fail_tick(created.error()));
		}
		retained_audio_scheduler.emplace(std::move(*created));
	}
	const auto mode = request.netplay_mode;
	const auto local_lock = mode == DirectVsNetplayMode::host ?
		netplay_host_lock(request).player_one :
		netplay_join_lock(request).player_two;
	th03::app::NewClassicNetplayLobbyView lobby_view(
		std::move(patch_assets), request.presentation_language,
		mode == DirectVsNetplayMode::host ?
			th03::app::NewClassicNetplayLobbyViewRole::host :
			th03::app::NewClassicNetplayLobbyViewRole::join,
		local_lock, static_cast<std::uint8_t>(request.launch.process.rank),
		mode == DirectVsNetplayMode::host ?
			std::optional<std::uint16_t>{request.netplay_host_port} : std::nullopt
	);
	if(!lobby_view.valid()) {
		return std::unexpected(fail_netplay(
			"netplay-lobby-view", "surface-create-failed"
		));
	}
	if(const auto presented = platform.present(lobby_view.surface()); !presented) {
		return std::unexpected(fail(
			"netplay-lobby-present", presented.error()
		));
	}
	auto socket = th03::platform::UdpSocket::bind(
		mode == DirectVsNetplayMode::host ? request.netplay_host_port : 0U
	);
	if(!socket) {
		return std::unexpected(fail_netplay("netplay-socket", "bind-failed"));
	}
	std::optional<th03::platform::UdpIpv4Endpoint> peer{};
	if(mode == DirectVsNetplayMode::join) {
		const auto endpoint = th03::platform::resolve_udp_ipv4_endpoint(
			request.netplay_join_endpoint
		);
		if(!endpoint) {
			return std::unexpected(fail_netplay(
				"netplay-endpoint", "resolve-ipv4-endpoint"
			));
		}
		peer = *endpoint;
		std::cout << "resolved-peer=" <<
			th03::platform::format_udp_ipv4_endpoint(*peer) << '\n';
	}
	auto created_lobby = mode == DirectVsNetplayMode::host ?
		NewClassicNetplayLobby::create_host(
			netplay_host_lock(request),
			fresh_netplay_nonce()
		) :
		NewClassicNetplayLobby::create_join(netplay_join_lock(request));
	if(!created_lobby) {
		return std::unexpected(fail_netplay(
			"netplay-lobby", "invalid-local-lock"
		));
	}
	auto lobby = std::move(*created_lobby);
	std::cout << "status=waiting\nrole=" <<
		(mode == DirectVsNetplayMode::host ? "host-p1" : "join-p2") << '\n';
	if(mode == DirectVsNetplayMode::host) {
		std::cout << "listen-port=" << request.netplay_host_port << '\n';
	} else {
		std::cout << "peer=" << request.netplay_join_endpoint << '\n';
	}
	std::cout.flush();

	const auto started = std::chrono::steady_clock::now();
	auto timeout_anchor = started;
	auto last_send = started - std::chrono::milliseconds(100);
	std::array<std::byte, th03::app::new_classic_netplay_datagram_bytes>
		receive_buffer{};
	while(lobby.status() != NewClassicNetplayLobbyStatus::ready) {
		if(const auto pumped = pump_retained_menu_audio(
			retained_audio_scheduler ? &*retained_audio_scheduler : nullptr,
			retained_audio
		); !pumped) {
			return std::unexpected(pumped.error());
		}
		const auto now = std::chrono::steady_clock::now();
		const auto exit = direct_netplay_lobby_exit(
			static_cast<SDL_WindowID>(platform.window_id())
		);
		if(exit != DirectNetplayLobbyExit::none) {
			if(application_quit &&
				exit == DirectNetplayLobbyExit::application_quit) {
				*application_quit = true;
			}
			std::cout << "status=left\n";
			return std::unexpected(0);
		}
		const auto elapsed_ms = static_cast<std::uint64_t>(
			std::chrono::duration_cast<std::chrono::milliseconds>(
				now - timeout_anchor
			).count()
		);
		if(th03::app::new_classic_netplay_lobby_wait_expired(
			lobby.role(), peer.has_value(), elapsed_ms
		)) {
			return std::unexpected(fail_netplay(
				"netplay-lobby", "peer-handshake-timeout"
			));
		}
		for(;;) {
			const auto received = (*socket)->receive(receive_buffer);
			if(!received) {
				return std::unexpected(fail_netplay(
					"netplay-lobby", "receive-failed"
				));
			}
			if(!*received) {
				break;
			}
			if((*received)->size !=
				th03::app::new_classic_netplay_lobby_bytes) {
				continue;
			}
			const auto packet =
				th03::app::decode_new_classic_netplay_lobby_datagram(
					std::span(receive_buffer).first((*received)->size)
				);
			if(!packet || (peer && *peer != (*received)->sender)) {
				continue;
			}
			if(mode == DirectVsNetplayMode::host && !peer &&
				packet->type != NewClassicNetplayLobbyDatagramType::join_request) {
				continue;
			}
			if(const auto accepted = lobby.receive(*packet); !accepted) {
				return std::unexpected(fail_netplay(
					"netplay-lobby", "peer-lock-conflict"
				));
			}
			if(!peer) {
				peer = (*received)->sender;
				timeout_anchor = now;
			}
			if(const auto outbound = lobby.outbound(); outbound &&
				!send_netplay_lobby_datagram(**socket, *peer, *outbound)) {
				return std::unexpected(fail_netplay(
					"netplay-lobby", "send-failed"
				));
			}
			last_send = now;
		}
		if(lobby.status() == NewClassicNetplayLobbyStatus::ready) {
			break;
		}
		if(peer && now - last_send >= std::chrono::milliseconds(100)) {
			if(const auto outbound = lobby.outbound(); outbound &&
				!send_netplay_lobby_datagram(**socket, *peer, *outbound)) {
				return std::unexpected(fail_netplay(
					"netplay-lobby", "send-failed"
				));
			}
			last_send = now;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	const auto contract = lobby.contract();
	if(!peer || !contract || lobby.session_id() == 0U) {
		return std::unexpected(fail_netplay(
			"netplay-lobby", "incomplete-ready-state"
		));
	}
	if(!lobby_view.mark_ready(*contract)) {
		return std::unexpected(fail_netplay(
			"netplay-lobby-view", "ready-surface-failed"
		));
	}
	if(const auto presented = platform.present(lobby_view.surface()); !presented) {
		return std::unexpected(fail(
			"netplay-lobby-present", presented.error()
		));
	}
	const auto held = hold_netplay_match_ready(
		platform,
		retained_audio_scheduler ? &*retained_audio_scheduler : nullptr,
		retained_audio, application_quit,
		[&]() -> std::expected<void, int> {
			const auto now = std::chrono::steady_clock::now();
			for(;;) {
				const auto received = (*socket)->receive(receive_buffer);
				if(!received) {
					return std::unexpected(fail_netplay(
						"netplay-lobby", "receive-failed"
					));
				}
				if(!*received) {
					break;
				}
				if((*received)->sender != *peer || (*received)->size !=
					th03::app::new_classic_netplay_lobby_bytes) {
					continue;
				}
				const auto packet =
					th03::app::decode_new_classic_netplay_lobby_datagram(
						std::span(receive_buffer).first((*received)->size)
					);
				if(packet) {
					static_cast<void>(lobby.receive(*packet));
				}
			}
			if(now - last_send >= std::chrono::milliseconds(100)) {
				if(const auto outbound = lobby.outbound(); outbound &&
					!send_netplay_lobby_datagram(**socket, *peer, *outbound)) {
					return std::unexpected(fail_netplay(
						"netplay-lobby", "send-failed"
					));
				}
				last_send = now;
			}
			return {};
		}
	);
	if(!held) {
		return std::unexpected(held.error());
	}
	return DirectNetplayLobbyResult{
		.wire = NetplayWire::udp(std::move(*socket), *peer),
		.lobby = std::move(lobby),
		.contract = *contract,
	};
}

#if defined(TH03_ENABLE_RTC_TRANSPORT)
[[nodiscard]] std::expected<DirectNetplayLobbyResult, int>
negotiate_rtc_netplay_lobby(
	const th03::app::DirectVsCliRequest &request,
	const RtcNetplayRequest &rtc_request,
	th03::platform::SdlPlatform &platform,
	std::shared_ptr<const th03::app::NewClassicReplayPatchAssets> patch_assets,
	const RetainedMenuAudio *retained_audio,
	bool *application_quit
)
{
	using th03::app::DirectVsNetplayMode;
	using th03::app::NewClassicNetplayLobby;
	using th03::app::NewClassicNetplayLobbyStatus;
	using th03::platform::RtcRoomState;
	std::optional<th03::app::TickInputScheduler> retained_audio_scheduler{};
	if(retained_audio) {
		auto created = th03::app::TickInputScheduler::create(
			request.tick_rate, th03::platform::sdl_ticks_now_ns(),
			th03::app::TickInputCatchUpPolicy::discard_missed_deadlines
		);
		if(!created) {
			return std::unexpected(fail_tick(created.error()));
		}
		retained_audio_scheduler.emplace(std::move(*created));
	}
	const auto provisional_mode = request.netplay_mode;
	const bool matchmaking =
		rtc_request.mode == th03::platform::RtcRoomMode::matchmaking;
	const auto local_lock = provisional_mode == DirectVsNetplayMode::host ?
		netplay_host_lock(request).player_one :
		netplay_join_lock(request).player_two;
	th03::app::NewClassicNetplayLobbyView lobby_view(
		std::move(patch_assets), request.presentation_language,
		matchmaking ? th03::app::NewClassicNetplayLobbyViewRole::matchmaking :
			(provisional_mode == DirectVsNetplayMode::host ?
				th03::app::NewClassicNetplayLobbyViewRole::host :
				th03::app::NewClassicNetplayLobbyViewRole::join),
		local_lock, static_cast<std::uint8_t>(request.launch.process.rank)
	);
	if(!lobby_view.valid()) {
		return std::unexpected(fail_netplay(
			"netplay-room-view", "surface-create-failed"
		));
	}
	if(const auto presented = platform.present(lobby_view.surface()); !presented) {
		return std::unexpected(fail(
			"netplay-room-present", presented.error()
		));
	}
	auto opened = th03::platform::RtcRoom::open({
		.mode = rtc_request.mode,
		.origin = rtc_request.origin,
		.user_id = rtc_request.user_id,
		.token = rtc_request.token,
		.room_code = rtc_request.room_code,
		.rank = rtc_request.rank,
		.ice_servers = rtc_request.ice_servers,
		.relay_only = rtc_request.relay_only,
	});
	if(!opened) {
		return std::unexpected(fail_netplay(
			"netplay-room-connect",
			th03::platform::rtc_room_error_code_name(opened.error().code)
		));
	}
	auto room = std::move(*opened);
	std::string displayed_code{};
	std::uint8_t assigned_player = 0U;
	std::optional<th03::app::NewClassicNetplayRoomPaired> profiles{};
	std::optional<th03::app::AppSha256> identity_digest{};
	const auto room_started = std::chrono::steady_clock::now();
	for(;;) {
		if(const auto pumped = pump_retained_menu_audio(
			retained_audio_scheduler ? &*retained_audio_scheduler : nullptr,
			retained_audio
		); !pumped) {
			return std::unexpected(pumped.error());
		}
		const auto exit = direct_netplay_lobby_exit(
			static_cast<SDL_WindowID>(platform.window_id())
		);
		if(exit != DirectNetplayLobbyExit::none) {
			if(application_quit &&
				exit == DirectNetplayLobbyExit::application_quit) {
				*application_quit = true;
			}
			std::cout << "status=left\n";
			return std::unexpected(0);
		}
		const auto snapshot = room->snapshot();
		if(!snapshot.room_code.empty() && snapshot.room_code != displayed_code) {
			if(!lobby_view.mark_room_code(snapshot.room_code)) {
				return std::unexpected(fail_netplay(
					"netplay-room-view", "invalid-room-code"
				));
			}
			displayed_code = snapshot.room_code;
			std::cout << "room-code=" << displayed_code << '\n';
			std::cout.flush();
			if(const auto presented = platform.present(lobby_view.surface());
				!presented) {
				return std::unexpected(fail(
					"netplay-room-present", presented.error()
				));
			}
		}
		if(snapshot.state == RtcRoomState::ready) {
			assigned_player = snapshot.player;
			if(!snapshot.identities) {
				return std::unexpected(fail_netplay(
					"netplay-room-connect", "paired-profile-missing"
				));
			}
			auto player_one_nametag =
				th03::app::layout_new_classic_nametag(
					snapshot.identities->player_one.nametag
				);
			auto player_two_nametag =
				th03::app::layout_new_classic_nametag(
					snapshot.identities->player_two.nametag
				);
			const auto &local_identity = assigned_player == 1U ?
				snapshot.identities->player_one :
				snapshot.identities->player_two;
			if(!player_one_nametag || !player_two_nametag ||
				local_identity.user_id != rtc_request.user_id) {
				return std::unexpected(fail_netplay(
					"netplay-room-connect", "paired-profile-invalid"
				));
			}
			profiles = th03::app::NewClassicNetplayRoomPaired{
				.player_one = {
					.user_id = snapshot.identities->player_one.user_id,
					.nametag = std::move(*player_one_nametag),
				},
				.player_two = {
					.user_id = snapshot.identities->player_two.user_id,
					.nametag = std::move(*player_two_nametag),
				},
			};
			const auto paired_digest =
				th03::app::new_classic_netplay_identity_digest({
					.room_id = snapshot.room_id,
					.profiles = *profiles,
				});
			if(!paired_digest) {
				return std::unexpected(fail_netplay(
					"netplay-room-connect", "paired-profile-invalid"
				));
			}
			identity_digest = *paired_digest;
			break;
		}
		if(snapshot.state == RtcRoomState::failed ||
			snapshot.state == RtcRoomState::closed ||
			snapshot.state == RtcRoomState::peer_left) {
			const auto code = snapshot.error ?
				th03::platform::rtc_room_error_code_name(snapshot.error->code) :
				(snapshot.state == RtcRoomState::peer_left ?
					"peer-left" : "transport-failed");
			return std::unexpected(fail_netplay(
				"netplay-room-connect", code
			));
		}
		if(!matchmaking && provisional_mode == DirectVsNetplayMode::join &&
			std::chrono::steady_clock::now() - room_started >
				std::chrono::seconds(30)) {
			return std::unexpected(fail_netplay(
				"netplay-room-connect", "peer-handshake-timeout"
			));
		}
		if(auto refreshed = refresh_host_presentability(
			platform, "netplay-room-presentability"
		); !refreshed) {
			return std::unexpected(refreshed.error());
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	if((assigned_player != 1U && assigned_player != 2U) ||
		!identity_digest) {
		return std::unexpected(fail_netplay(
			"netplay-room-connect", "invalid-player-assignment"
		));
	}
	const auto mode = assigned_player == 1U ?
		DirectVsNetplayMode::host : DirectVsNetplayMode::join;

	auto wire = NetplayWire::rtc(std::move(room));
	auto host_lock = netplay_host_lock(request);
	host_lock.identity_digest = *identity_digest;
	auto join_lock = netplay_join_lock(request);
	join_lock.identity_digest = *identity_digest;
	auto created_lobby = mode == DirectVsNetplayMode::host ?
		NewClassicNetplayLobby::create_host(
			host_lock,
			fresh_netplay_nonce()
		) : NewClassicNetplayLobby::create_join(join_lock);
	if(!created_lobby) {
		return std::unexpected(fail_netplay(
			"netplay-lobby", "invalid-local-lock"
		));
	}
	auto lobby = std::move(*created_lobby);
	auto last_send = std::chrono::steady_clock::now() -
		std::chrono::milliseconds(100);
	const auto lobby_started = std::chrono::steady_clock::now();
	while(lobby.status() != NewClassicNetplayLobbyStatus::ready) {
		if(const auto pumped = pump_retained_menu_audio(
			retained_audio_scheduler ? &*retained_audio_scheduler : nullptr,
			retained_audio
		); !pumped) {
			return std::unexpected(pumped.error());
		}
		const auto exit = direct_netplay_lobby_exit(
			static_cast<SDL_WindowID>(platform.window_id())
		);
		if(exit != DirectNetplayLobbyExit::none) {
			if(application_quit &&
				exit == DirectNetplayLobbyExit::application_quit) {
				*application_quit = true;
			}
			std::cout << "status=left\n";
			return std::unexpected(0);
		}
		const auto now = std::chrono::steady_clock::now();
		if(now - lobby_started > std::chrono::seconds(30)) {
			return std::unexpected(fail_netplay(
				"netplay-lobby", "peer-handshake-timeout"
			));
		}
		for(;;) {
			auto received = wire.receive();
			if(!received) {
				return std::unexpected(fail_netplay(
					"netplay-lobby", received.error()
				));
			}
			if(!*received) {
				break;
			}
			if((**received).channel != NetplayWireChannel::control) {
				return std::unexpected(fail_netplay(
					"netplay-lobby", "unexpected-gameplay-data"
				));
			}
			const auto packet =
				th03::app::decode_new_classic_netplay_lobby_datagram(
					(**received).bytes
				);
			if(!packet) {
				return std::unexpected(fail_netplay(
					"netplay-lobby", "invalid-peer-lock"
				));
			}
			if(const auto accepted = lobby.receive(*packet); !accepted) {
				return std::unexpected(fail_netplay(
					"netplay-lobby", "peer-lock-conflict"
				));
			}
			if(const auto outbound = lobby.outbound(); outbound &&
				!send_netplay_lobby_wire(wire, *outbound)) {
				return std::unexpected(fail_netplay(
					"netplay-lobby", "send-failed"
				));
			}
			last_send = now;
		}
		if(lobby.status() == NewClassicNetplayLobbyStatus::ready) {
			break;
		}
		if(now - last_send >= std::chrono::milliseconds(100)) {
			if(const auto outbound = lobby.outbound(); outbound &&
				!send_netplay_lobby_wire(wire, *outbound)) {
				return std::unexpected(fail_netplay(
					"netplay-lobby", "send-failed"
				));
			}
			last_send = now;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	const auto contract = lobby.contract();
	if(!contract || lobby.session_id() == 0U) {
		return std::unexpected(fail_netplay(
			"netplay-lobby", "incomplete-ready-state"
		));
	}
	if(!lobby_view.mark_ready(*contract)) {
		return std::unexpected(fail_netplay(
			"netplay-room-view", "ready-surface-failed"
		));
	}
	if(const auto presented = platform.present(lobby_view.surface()); !presented) {
		return std::unexpected(fail(
			"netplay-room-present", presented.error()
		));
	}
	const auto held = hold_netplay_match_ready(
		platform,
		retained_audio_scheduler ? &*retained_audio_scheduler : nullptr,
		retained_audio, application_quit,
		[&]() -> std::expected<void, int> {
			const auto now = std::chrono::steady_clock::now();
			for(;;) {
				auto received = wire.receive();
				if(!received) {
					return std::unexpected(fail_netplay(
						"netplay-lobby", received.error()
					));
				}
				if(!*received) {
					break;
				}
				if((**received).channel != NetplayWireChannel::control) {
					continue;
				}
				const auto packet =
					th03::app::decode_new_classic_netplay_lobby_datagram(
						(**received).bytes
					);
				if(packet) {
					static_cast<void>(lobby.receive(*packet));
				}
			}
			if(now - last_send >= std::chrono::milliseconds(100)) {
				if(const auto outbound = lobby.outbound(); outbound &&
					!send_netplay_lobby_wire(wire, *outbound)) {
					return std::unexpected(fail_netplay(
						"netplay-lobby", "send-failed"
					));
				}
				last_send = now;
			}
			return {};
		}
	);
	if(!held) {
		return std::unexpected(held.error());
	}
	return DirectNetplayLobbyResult{
		.wire = std::move(wire),
		.lobby = std::move(lobby),
		.contract = *contract,
		.profiles = std::move(profiles),
	};
}
#endif

} // namespace th03::app::launcher_support
