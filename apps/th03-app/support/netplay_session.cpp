#include "netplay_session.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_timer.h>

#include "../default_bindings.hpp"
#include "audio_output.hpp"
#include "diagnostics.hpp"
#include "netplay_wire.hpp"
#include "replay_save.hpp"
#include "runtime_environment.hpp"

#include "th03/app/classic/classic_audio_session.hpp"
#include "th03/app/main/main_direct_vs_runner.hpp"
#include "th03/app/new_classic/content/new_classic_language.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_datagram.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_identity.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_lobby.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_main_session.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_match_contract.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_match_intro.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_pause_barrier.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_post_match.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_report.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_replay_capture.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_room.hpp"
#include "th03/app/new_classic/new_classic_pause.hpp"
#include "th03/app/runtime/tick_input.hpp"
#include "th03/formats/v14.hpp"

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

void apply_netplay_contract(
	th03::app::DirectVsCliRequest &request,
	const th03::app::NewClassicNetplayMatchContract &contract
) noexcept
{
	request.launch.process.player_one = {
		.character = static_cast<th03::app::MainCharacter>(
			contract.player_one.character
		),
		.palette = static_cast<th03::app::MainPaletteVariant>(
			contract.player_one.palette
		),
	};
	request.launch.process.player_two = {
		.character = static_cast<th03::app::MainCharacter>(
			contract.player_two.character
		),
		.palette = static_cast<th03::app::MainPaletteVariant>(
			contract.player_two.palette
		),
	};
	request.launch.process.rank = contract.rank;
	request.launch.process.seed = contract.seed;
	request.tick_rate = {
		.ticks = contract.cadence_ticks,
		.seconds = contract.cadence_seconds,
	};
	request.netplay_input_delay_frames = contract.input_delay_frames;
	request.launch.process.autofire_mask = contract.autofire_mask;
	request.launch.process.game_options.input_protocol =
		th03::InputProtocol::live;
	request.launch.process.game_options.three_digit_combos = true;
}

[[nodiscard]] th03::app::NewClassicNetplayOwnedInput netplay_owned_input(
	const th03::CoreInput &input
) noexcept
{
	// Each peer uses its ordinary local-player bindings. The negotiated role
	// decides which logical lane receives this value; it must not force the
	// joining player onto the local P2/numpad binding set.
	return {
		.word = input.p1,
		.charge = static_cast<std::uint8_t>(input.charge_mask & 1U),
	};
}

void log_netplay_session_error(
	const th03::app::NewClassicNetplayMainSessionError &error
)
{
	std::cerr << "netplay-session-code=" <<
		static_cast<unsigned>(error.code) << '\n';
	if(error.timeline) {
		std::cerr << "netplay-timeline-code=" <<
			static_cast<unsigned>(error.timeline->code) << '\n';
	}
	if(error.rollback) {
		std::cerr << "netplay-rollback-code=" <<
			static_cast<unsigned>(error.rollback->code) << '\n';
		if(error.rollback->rollback) {
			std::cerr << "netplay-snapshot-code=" << static_cast<unsigned>(
				error.rollback->rollback->code
			) << '\n';
		}
	}
}

struct DirectNetplayLobbyResult final {
	NetplayWire wire;
	th03::app::NewClassicNetplayLobby lobby{};
	th03::app::NewClassicNetplayMatchContract contract{};
	std::optional<th03::app::NewClassicNetplayRoomPaired> profiles{};
};

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

[[nodiscard]] std::expected<DirectNetplayLobbyResult, int>
negotiate_direct_netplay_lobby(
	const th03::app::DirectVsCliRequest &request,
	th03::platform::SdlPlatform &platform,
	std::shared_ptr<const th03::app::NewClassicReplayPatchAssets> patch_assets,
	const RetainedMenuAudio *retained_audio = nullptr,
	bool *application_quit = nullptr
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
	const RetainedMenuAudio *retained_audio = nullptr,
	bool *application_quit = nullptr
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

[[nodiscard]] bool send_netplay_input_history(
	NetplayWire &wire,
	const th03::app::NewClassicNetplayMainSession &session,
	th03::app::NewClassicNetplayPlayer local_player,
	std::uint64_t session_id,
	std::uint64_t contract_hash,
	std::uint64_t newest_frame
)
{
	th03::app::NewClassicNetplayDatagram datagram{
		.type = th03::app::NewClassicNetplayDatagramType::input,
		.sender = local_player,
		.session_id = session_id,
		.contract_hash = contract_hash,
		.latest_frame = newest_frame,
		.confirmed_frame_count = session.confirmed_frame_count(),
	};
	for(std::size_t index = 0U;
		index < datagram.inputs.size() && index <= newest_frame; ++index) {
		const auto input = session.local_input(newest_frame - index);
		if(!input) {
			break;
		}
		datagram.inputs[index] = *input;
		++datagram.input_count;
	}
	return datagram.input_count != 0U &&
		send_netplay_datagram(wire, datagram);
}

[[nodiscard]] int run_netplay_report_modal(
	th03::app::RationalTickRate tick_rate,
	th03::platform::SdlPlatform &platform,
	th03::platform::SdlInputMapper &mapper,
	std::shared_ptr<const th03::app::NewClassicReplayPatchAssets> patch_assets,
	th03::app::NewClassicNetplayReportKind kind,
	th03::app::NewClassicNetplayReportMetrics metrics,
	th03::app::NewClassicLanguageAssetProfile language =
		th03::app::NewClassicLanguageAssetProfile::english,
	const RetainedMenuAudio *retained_audio = nullptr,
	bool *application_quit = nullptr
)
{
	if(application_quit) {
		*application_quit = false;
	}
	th03::app::NewClassicNetplayReport report(
		kind, metrics, std::move(patch_assets), language
	);
	if(!report.valid()) {
		return fail_netplay("netplay-report", "surface-create-failed");
	}
	auto scheduler = th03::app::TickInputScheduler::create(
		tick_rate, th03::platform::sdl_ticks_now_ns(),
		th03::app::TickInputCatchUpPolicy::discard_missed_deadlines
	);
	if(!scheduler) {
		return fail_tick(scheduler.error());
	}
	const auto started_at = th03::platform::sdl_ticks_now_ns();
	if(const auto reset = mapper.reset_after_modal(*scheduler, started_at);
		!reset) {
		return fail("netplay-report-input", reset.error());
	}
	if(const auto presented = platform.present(report.surface()); !presented) {
		return fail("netplay-report-present", presented.error());
	}

	bool retry_blocked_event = false;
	while(report.status() ==
		th03::app::NewClassicNetplayReportStatus::running) {
		const auto cutoff = th03::platform::sdl_ticks_now_ns();
		if(!retry_blocked_event) {
			auto pumped = mapper.pump_events(*scheduler, false);
			if(!pumped) {
				return fail("netplay-report-input", pumped.error());
			}
			retry_blocked_event = pumped->queue_blocked;
		}
		for(;;) {
			auto due = scheduler->pop_due(cutoff);
			if(!due) {
				return fail_tick(due.error());
			}
			if(!*due) {
				break;
			}
			if(retained_audio) {
				const auto advanced = retained_audio->advance_tick();
				if(!advanced) {
					return advanced.error();
				}
			}
			const auto consumed = report.consume(th03::app::new_classic_menu_ui(
				**due, true
			));
			if(retry_blocked_event) {
				auto retried = mapper.pump_events(*scheduler, false);
				if(!retried) {
					return fail("netplay-report-input", retried.error());
				}
				retry_blocked_event = retried->queue_blocked;
			}
			if(consumed.status !=
				th03::app::NewClassicNetplayReportStatus::running) {
				break;
			}
		}
		if(auto refreshed = refresh_host_presentability(
			platform, "netplay-report-presentability"
		); !refreshed) {
			return refreshed.error();
		}
		const auto deadline = scheduler->next_deadline_ns();
		const auto now = th03::platform::sdl_ticks_now_ns();
		if(deadline && *deadline > now) {
			SDL_DelayPrecise(std::min(*deadline - now, maximum_idle_delay_ns));
		}
	}
	if(application_quit && report.status() ==
		th03::app::NewClassicNetplayReportStatus::application_quit) {
		*application_quit = true;
	}
	return 0;
}

[[nodiscard]] std::shared_ptr<const th03::app::NewClassicPauseNetplayText>
prepare_netplay_pause_text(
	const th03::app::NewClassicReplayPatchAssets &assets,
	th03::app::NewClassicLanguageAssetProfile language
)
{
	auto result = std::make_shared<th03::app::NewClassicPauseNetplayText>();
	const auto &messages = assets.messages(language);
	const auto render = [&assets](
		std::string_view text,
		th03::app::NewClassicPauseNetplayText::Mask &mask,
		std::optional<std::int32_t> left
	) {
		th03::present::Raster raster{};
		const bool put = left ? assets.menu_font.put(
			raster, *left, 0, text, 15U
		) : assets.menu_font.put_centered(raster, 320, 0, text, 15U);
		if(!put) {
			return false;
		}
		const auto pixels = raster.graphics(th03::present::Page::zero);
		for(std::size_t y = 0U;
			y < th03::app::NewClassicPauseNetplayText::height; ++y) {
			for(std::size_t x = 0U;
				x < th03::app::NewClassicPauseNetplayText::width; ++x) {
				const auto source = (y * th03::present::graphics_width) +
					192U + x;
				mask[(y * th03::app::NewClassicPauseNetplayText::width) + x] =
					static_cast<std::uint8_t>(pixels[source] != 0U);
			}
		}
		return true;
	};
	if(!render(
		messages.text(th03::app::NewClassicMessageId::netplay_pause_forfeit),
		result->forfeit,
		static_cast<std::int32_t>(
			192U + th03::app::NewClassicPauseNetplayText::choice_text_left
		)
	)) {
		return {};
	}
	if(!render(
		messages.text(
			th03::app::NewClassicMessageId::netplay_pause_waiting_for_opponent
		),
		result->waiting_for_opponent, std::nullopt
	)) {
		return {};
	}
	const auto prefix = messages.text(
		th03::app::NewClassicMessageId::netplay_pause_resume_in
	);
	if(prefix.size() + 2U > 64U) {
		return {};
	}
	for(std::uint8_t second = 3U; second >= 1U; --second) {
		std::array<char, 64U> text{};
		std::copy(prefix.begin(), prefix.end(), text.begin());
		text[prefix.size()] = ' ';
		text[prefix.size() + 1U] = static_cast<char>('0' + second);
		if(!render(
			std::string_view(text.data(), prefix.size() + 2U),
			result->resume_countdown[3U - second], std::nullopt
		)) {
			return {};
		}
	}
	return result;
}

} // namespace

int run_direct_netplay_on_platform(
	th03::app::DirectVsCliRequest parsed,
	th03::platform::SdlPlatform &platform,
	th03::platform::SdlInputMapper &mapper,
	std::optional<RtcNetplayRequest> rtc_request,
	const RetainedMenuAudio *retained_menu_audio,
	bool *gameplay_started,
	bool *application_quit,
	const th03::app::NewClassicKeyConfig *key_config,
	const th03::app::NewClassicJoyConfig *joy_config,
	const th03::app::NewClassicDeviceAssignment *device_assignment
)
{
	using th03::app::DirectVsNetplayMode;
	using th03::app::NewClassicNetplayDatagram;
	using th03::app::NewClassicNetplayDatagramType;
	using th03::app::NewClassicNetplayMainSessionStatus;
	using th03::app::NewClassicNetplayPlayer;
	using th03::app::NewClassicNetplayPostMatchStatus;
	if(gameplay_started) {
		*gameplay_started = false;
	}
	if(application_quit) {
		*application_quit = false;
	}
	parsed.launch.process.classic_audio_lifecycle = true;
	const auto audio_data_root = parsed.launch.content.data_root;
	const auto post_match_content = parsed.launch.content;
	const auto replay_save_content = parsed.launch.content;
	auto loaded_pause_assets = th03::app::load_new_classic_replay_patch_assets(
		parsed.launch.content.data_root / "AZINN.DAT",
		parsed.launch.content.new_classic_menu_assets,
		parsed.launch.content.font_rom
	);
	if(!loaded_pause_assets) {
		return fail("netplay-pause-assets", loaded_pause_assets.error());
	}
	auto pause_assets = std::make_shared<
		const th03::app::NewClassicReplayPatchAssets
	>(std::move(*loaded_pause_assets));
	auto pause_text = prepare_netplay_pause_text(
		*pause_assets, parsed.presentation_language
	);
	if(!pause_text) {
		return fail_netplay("netplay-pause-assets", "text-render-failed");
	}
	auto negotiated = [&]() -> std::expected<DirectNetplayLobbyResult, int> {
#if defined(TH03_ENABLE_RTC_TRANSPORT)
		if(rtc_request) {
			return negotiate_rtc_netplay_lobby(
				parsed, *rtc_request, platform, pause_assets,
				retained_menu_audio, application_quit
			);
		}
#else
		if(rtc_request) {
			return std::unexpected(fail_netplay(
				"netplay-transport", "rtc-not-built"
			));
		}
#endif
		return negotiate_direct_netplay_lobby(
			parsed, platform, pause_assets, retained_menu_audio,
			application_quit
		);
	}();
	if(!negotiated) {
		return negotiated.error();
	}
	if(gameplay_started) {
		*gameplay_started = true;
	}
	if(retained_menu_audio) {
		retained_menu_audio->suspend();
	}
	const auto mode = negotiated->lobby.role() ==
		th03::app::NewClassicNetplayLobbyRole::host ?
			DirectVsNetplayMode::host : DirectVsNetplayMode::join;
	const auto local_player = mode == DirectVsNetplayMode::host ?
		NewClassicNetplayPlayer::player_one :
		NewClassicNetplayPlayer::player_two;
	const auto remote_player = mode == DirectVsNetplayMode::host ?
			NewClassicNetplayPlayer::player_two :
			NewClassicNetplayPlayer::player_one;
	apply_netplay_contract(parsed, negotiated->contract);
	const auto created_contract_hash =
		th03::app::new_classic_netplay_match_contract_hash(
			negotiated->contract
		);
	if(!created_contract_hash) {
		return fail_netplay("netplay-contract", "invalid-match-lock");
	}
	const auto contract_hash = *created_contract_hash;
	auto wire = std::move(negotiated->wire);
	auto lobby = std::move(negotiated->lobby);
	std::uint64_t session_id = lobby.session_id();

	auto created_audio = th03::app::ClassicAudioSession::create({
		.data_root = audio_data_root,
		.tick_rate = parsed.tick_rate,
		.bgm_active = true,
		.effects_active = true,
		.effects_follow_bgm = false,
		.music_volume_percent = parsed.music_volume_percent,
		.effect_volume_percent = parsed.effect_volume_percent,
	});
	if(!created_audio) {
		return fail("netplay-audio-create", created_audio.error());
	}
	auto audio_session = std::move(*created_audio);
	std::optional<th03::platform::SdlAudioDevice> audio_device{};
	auto created_device = th03::platform::SdlAudioDevice::create({
		.start_paused = true,
	});
	if(created_device) {
		audio_device.emplace(std::move(*created_device));
	} else {
		warn_audio_device("netplay-audio-device-create", created_device.error());
	}
	bool audio_started = false;
	bool audio_focus_muted = false;
	std::vector<th03::audio::PmdCanonicalStereoSample> startup_silence{};
	std::vector<th03::audio::PmdCanonicalStereoSample> gain_frames{};
	const auto advance_audio_tick = [&]() -> std::expected<void, int> {
		auto tick = audio_session->advance_tick();
		if(!tick) {
			return std::unexpected(fail(
				"netplay-audio-tick", tick.error()
			));
		}
		if(!audio_device || audio_focus_muted) {
			return {};
		}
		auto output = output_gain_frames(
			tick->canonical_frames, parsed.master_volume_percent, gain_frames
		);
		if(!output) {
			return std::unexpected(fail_netplay(
				"netplay-audio-gain", "invalid-volume"
			));
		}
		if(!audio_started && !output->empty() &&
			output->size() <=
				audio_device->maximum_queued_frames() / 3U) {
			startup_silence.assign(
				output->size() * 2U, {}
			);
			const auto prefilled = audio_device->submit(startup_silence);
			if(!prefilled) {
				warn_audio_device("netplay-audio-prefill", prefilled.error());
				audio_device.reset();
				return {};
			}
		}
		const auto submitted = audio_device->submit(*output);
		if(!submitted) {
			warn_audio_device("netplay-audio-submit", submitted.error());
			audio_device.reset();
		} else if(!audio_started && !output->empty()) {
			const auto resumed = audio_device->resume();
			if(!resumed) {
				warn_audio_device("netplay-audio-resume", resumed.error());
				audio_device.reset();
			} else {
				audio_started = true;
			}
		}
		return {};
	};
	auto scheduler = th03::app::TickInputScheduler::create(
		parsed.tick_rate,
		th03::platform::sdl_ticks_now_ns(),
		th03::app::TickInputCatchUpPolicy::preserve_all
	);
	if(!scheduler) {
		return fail_tick(scheduler.error());
	}
	std::vector<th03::present::ComposedPixel> pixels(
		th03::present::graphics_page_pixels
	);
	const auto pixel_span = std::span<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	>(pixels.data(), pixels.size());

	if(!parsed.launch.content.mainl_executable.empty()) {
		auto intro = th03::app::NewClassicNetplayMatchIntro::create({
			.content = parsed.launch.content,
			.process = parsed.launch.process,
			.language = parsed.presentation_language,
			.profiles = negotiated->profiles,
		});
		if(!intro) {
			return fail("netplay-match-intro", intro.error());
		}
		auto intro_surface = (*intro)->compose_shown_surface(pixel_span);
		if(!intro_surface) {
			return fail("netplay-match-intro-compose", intro_surface.error());
		}
		if(const auto presented = platform.present(*intro_surface); !presented) {
			return fail("netplay-match-intro-present", presented.error());
		}
		bool intro_queue_blocked = false;
		while((*intro)->status() ==
			th03::app::NewClassicNetplayMatchIntroStatus::running) {
			if(!intro_queue_blocked) {
				auto events = mapper.pump_events(*scheduler, false);
				if(!events) {
					return fail("netplay-match-intro-input", events.error());
				}
				intro_queue_blocked = events->queue_blocked;
			}
			const auto now = th03::platform::sdl_ticks_now_ns();
			bool surface_changed = false;
			for(;;) {
				auto due = scheduler->pop_due(now);
				if(!due) {
					return fail_tick(due.error());
				}
				if(!*due) {
					break;
				}
				if((**due).app_quit_requested) {
					if(application_quit) {
						*application_quit = true;
					}
					return 0;
				}
				const bool dismiss_pressed = (**due).input.system != 0U ||
					(**due).input.p1 != 0U || (**due).input.p2 != 0U;
				auto advanced = (*intro)->advance(dismiss_pressed);
				if(!advanced) {
					return fail("netplay-match-intro", advanced.error());
				}
				while(const auto command = (*intro)->take_audio_command()) {
					if(auto applied = audio_session->apply(*command); !applied) {
						return fail("netplay-match-intro-audio", applied.error());
					}
				}
				const bool desired_focus_mute = parsed.mute_when_unfocused &&
					!mapper.main_window_focused();
				if(desired_focus_mute && !audio_focus_muted && audio_device) {
					if(auto cleared = audio_device->clear(); !cleared) {
						warn_audio_device(
							"netplay-match-intro-focus-clear", cleared.error()
						);
						audio_device.reset();
					}
				}
				audio_focus_muted = desired_focus_mute;
				if(const auto audio = advance_audio_tick(); !audio) {
					return audio.error();
				}
				surface_changed = surface_changed || advanced->surface_changed;
				if(intro_queue_blocked) {
					auto retried = mapper.pump_events(*scheduler, false);
					if(!retried) {
						return fail(
							"netplay-match-intro-input", retried.error()
						);
					}
					intro_queue_blocked = retried->queue_blocked;
				}
				if((*intro)->status() ==
					th03::app::NewClassicNetplayMatchIntroStatus::ready) {
					break;
				}
			}
			if(surface_changed) {
				intro_surface = (*intro)->compose_shown_surface(pixel_span);
				if(!intro_surface) {
					return fail(
						"netplay-match-intro-compose", intro_surface.error()
					);
				}
				if(const auto presented = platform.present(*intro_surface);
					!presented) {
					return fail(
						"netplay-match-intro-present", presented.error()
					);
				}
			} else if(auto refreshed = refresh_host_presentability(
				platform, "netplay-match-intro-presentability"
			); !refreshed) {
				return refreshed.error();
			}
			const auto deadline = scheduler->next_deadline_ns();
			const auto after = th03::platform::sdl_ticks_now_ns();
			if(deadline && *deadline > after) {
				SDL_DelayPrecise(std::min(
					*deadline - after, maximum_idle_delay_ns
				));
			}
		}
		if(auto rebased = scheduler->rebase_after_modal(
			th03::platform::sdl_ticks_now_ns()
		); !rebased) {
			return fail_tick(rebased.error());
		}
	}

	auto process = th03::app::load_direct_vs_process(std::move(parsed.launch));
	if(!process) {
		return fail_content(process.error());
	}
	const auto process_round_start = (*process)->round_start_observation();
	const auto process_start = process_round_start.start;
	th03::app::NewClassicReplayIdentity replay_identity{
		.ruleset = th03::formats::ReplayRuleset::stock,
		.recording_flags = th03::formats::replay_recording_flag_netplay,
		.recorder_role = local_player == NewClassicNetplayPlayer::player_one ?
			th03::formats::ReplayRecorderRole::player_one :
			th03::formats::ReplayRecorderRole::player_two,
		.recorder_source = th03::formats::ReplayRecorderSource::native,
	};
	if(negotiated->profiles) {
		const auto player_one = th03::app::parse_new_classic_replay_account_uuid(
			negotiated->profiles->player_one.user_id
		);
		const auto player_two = th03::app::parse_new_classic_replay_account_uuid(
			negotiated->profiles->player_two.user_id
		);
		if(!player_one || !player_two) {
			return fail_netplay("netplay-replay", "profile-uuid-invalid");
		}
		replay_identity.player_uuid = {*player_one, *player_two};
		replay_identity.player_nametag_snapshot = {
			negotiated->profiles->player_one.nametag.source,
			negotiated->profiles->player_two.nametag.source,
		};
	}
	std::copy_n(
		negotiated->contract.identity_digest.begin(),
		replay_identity.match_id.size(), replay_identity.match_id.begin()
	);
	auto created_replay_capture =
		th03::app::NewClassicNetplayReplayCapture::create(
			process_round_start, 0U, replay_identity
		);
	if(!created_replay_capture) {
		return fail_netplay("netplay-replay", "capture-create-failed");
	}
	auto replay_capture = std::move(*created_replay_capture);
	auto session = th03::app::NewClassicNetplayMainSession::create(
		local_player, std::move(*process), negotiated->contract.input_delay_frames
	);
	if(!session) {
		return fail_netplay("netplay-session", "session-create-failed");
	}
	const auto initial_simulation_digest = (*session)->simulation_digest(0U);
	if(!initial_simulation_digest) {
		log_netplay_session_error(initial_simulation_digest.error());
		return fail_netplay("netplay-identity", "digest-create-failed");
	}
	while(const auto command = (*session)->take_audio_command()) {
		if(auto applied = audio_session->apply(*command); !applied) {
			return fail("netplay-audio-command", applied.error());
		}
	}
	const auto local_autofire = (negotiated->contract.autofire_mask &
		(local_player == NewClassicNetplayPlayer::player_one ? 1U : 2U)) != 0U;
	const auto netplay_bindings = key_config && joy_config && device_assignment ?
		th03::app::detail::new_classic_netplay_story_bindings(
			*key_config, *joy_config, *device_assignment, local_autofire
		) : th03::app::detail::new_classic_netplay_story_bindings(local_autofire);
	if(auto replaced = mapper.replace_bindings(netplay_bindings); !replaced) {
		return fail("input-bindings", replaced.error());
	}
	const auto publish_confirmed_audio = [&]() -> std::expected<void, int> {
		for(;;) {
			auto confirmed = (*session)->take_confirmed_audio_frame();
			if(!confirmed) {
				log_netplay_session_error(confirmed.error());
				return std::unexpected(fail_netplay(
					"netplay-audio", "journal-failed"
				));
			}
			if(!*confirmed) {
				return {};
			}
			for(std::size_t index = 0U;
				index < (**confirmed).command_count; ++index) {
				if(auto applied = audio_session->apply(
					(**confirmed).commands[index]
				); !applied) {
					return std::unexpected(fail(
						"netplay-audio-command", applied.error()
					));
				}
			}
			for(std::uint8_t vsync = 0U;
				vsync < (**confirmed).source_vsyncs; ++vsync) {
				if(const auto advanced = advance_audio_tick(); !advanced) {
					return std::unexpected(advanced.error());
				}
			}
		}
	};
	auto surface = (*session)->compose_shown_surface(pixel_span);
	if(!surface) {
		return fail("compose", surface.error());
	}
	if(const auto presented = platform.present(*surface); !presented) {
		return fail("present", presented.error());
	}

	const auto start_ns = th03::platform::sdl_ticks_now_ns();
	auto last_send_ns = start_ns - 250'000'000U;
	auto last_initial_digest_send_ns = start_ns - 250'000'000U;
	auto last_runtime_digest_send_ns = start_ns - 250'000'000U;
	auto last_receive_ns = start_ns;
	bool established = false;
	bool peer_started_input = false;
	bool retry_blocked_event = false;
	std::optional<th03::app::NewClassicNetplayOwnedInput> stalled_input{};
	std::optional<std::uint64_t> newest_local_frame{};
	using ConfirmedSimulationDigest =
		th03::app::NewClassicNetplayConfirmedSimulationDigest;
	std::optional<ConfirmedSimulationDigest> local_runtime_digest{};
	std::optional<ConfirmedSimulationDigest> remote_runtime_digest{};
	std::optional<ConfirmedSimulationDigest> remote_runtime_digest_ack{};
	std::optional<ConfirmedSimulationDigest> verified_runtime_digest{};
	std::uint64_t verified_periodic_gameplay_frame = 0U;
	std::vector<std::uint8_t> local_deadline_missed{};
	std::unique_ptr<th03::app::NewClassicNetplayPostMatch> post_match{};
	std::optional<th03::app::NewClassicNetplayPauseBarrier> pause_barrier{};
	std::optional<th03::app::NewClassicPauseMenu> pause_overlay{};
	std::optional<th03::app::TickTimestampNs> resume_countdown_started_ns{};
	std::optional<std::uint8_t> resume_countdown_seconds{};
	bool runtime_digest_peer_ack = false;
	bool transport_detached = false;
	bool presentation_pending = false;
	std::uint64_t local_frames = 0U;
	auto last_pause_signal_send_ns = start_ns - 250'000'000U;
	auto last_network_metric_sample_ns = start_ns - 250'000'000U;
	std::optional<std::uint64_t> latest_round_trip_time_ms{};
	std::cout << "status=match-locked\ncontract=" << contract_hash << '\n';
	std::cout.flush();
	const auto report_metrics = [&]() {
		return th03::app::NewClassicNetplayReportMetrics{
			.confirmed_frames = (*session)->confirmed_frame_count(),
			.session = (*session)->stats(),
			.missed_deadlines = static_cast<std::uint64_t>(std::count(
				local_deadline_missed.begin(), local_deadline_missed.end(),
				std::uint8_t{1U}
			)),
			.round_trip_time_ms = latest_round_trip_time_ms,
		};
	};
	const auto show_report = [&](
		th03::app::NewClassicNetplayReportKind kind
	) -> int {
		return run_netplay_report_modal(
			parsed.tick_rate, platform, mapper, pause_assets, kind,
			report_metrics(), parsed.presentation_language, nullptr,
			application_quit
		);
	};
	const auto fail_with_report = [&] (
		const char *stage,
		const char *code,
		th03::app::NewClassicNetplayReportKind kind
	) -> int {
		const int failure = fail_netplay(stage, code);
		// The retained room owns its own source-styled failure child and caller
		// stack. Let that owner present the failure after it has restored title
		// audio and menu bindings; the standalone CLI has no such owner.
		if(retained_menu_audio) {
			return failure;
		}
		const int report_failure = show_report(kind);
		return report_failure == 0 ? failure : report_failure;
	};
	const auto send_simulation_digest = [&](
		NewClassicNetplayDatagramType type,
		std::uint64_t frame,
		std::uint64_t confirmed_gameplay_frame_count,
		th03::app::NewClassicNetplaySimulationDigestKind kind,
		th03::app::NewClassicNetplayBoundaryProof boundary,
		const th03::app::AppSha256 &digest
	) -> bool {
		if(session_id == 0U) {
			return false;
		}
		return send_netplay_datagram(wire, {
			.type = type,
			.sender = local_player,
			.session_id = session_id,
			.contract_hash = contract_hash,
			.latest_frame = frame,
			.confirmed_frame_count = confirmed_gameplay_frame_count,
			.simulation_digest = digest,
			.simulation_digest_kind = kind,
			.boundary_proof = boundary,
		});
	};
	const auto same_proof_identity = [](
		const ConfirmedSimulationDigest &left,
		const ConfirmedSimulationDigest &right
	) noexcept {
		return left.frame == right.frame &&
			left.confirmed_gameplay_frame_count ==
				right.confirmed_gameplay_frame_count &&
			left.kind == right.kind && left.boundary == right.boundary;
	};
	const auto same_proof = [&](const ConfirmedSimulationDigest &left,
		const ConfirmedSimulationDigest &right) noexcept {
		return same_proof_identity(left, right) &&
			left.digest == right.digest;
	};
	const auto leave_preview = [&]() -> int {
		if(session_id != 0U) {
			const NewClassicNetplayDatagram departure{
				.type = NewClassicNetplayDatagramType::disconnect,
				.sender = local_player,
				.session_id = session_id,
				.contract_hash = contract_hash,
			};
			// UDP has no delivery acknowledgement. Three bounded copies make an
			// intentional local leave prompt without turning shutdown into a wait.
			for(std::uint8_t attempt = 0U; attempt < 3U; ++attempt) {
				(void)send_netplay_datagram(wire, departure);
			}
		}
		std::cout << "status=left\n";
		return 0;
	};
	const auto finish_forfeit = [&] (
		const char *outcome,
		th03::app::NewClassicNetplayReportKind kind
	) -> int {
		std::cout << "status=match-forfeited\nforfeit-outcome=" << outcome <<
			"\nreplay-ready=0\nreplay-published=0\n";
		std::cout.flush();
		return show_report(kind);
	};
	const auto match_runtime_digests = [&]() -> std::expected<void, int> {
		if(!local_runtime_digest || !remote_runtime_digest) {
			return {};
		}
		if(!same_proof_identity(
			*local_runtime_digest, *remote_runtime_digest
		)) {
			return std::unexpected(fail_netplay(
				"netplay-identity", "runtime-proof-order"
			));
		}
		if(local_runtime_digest->digest != remote_runtime_digest->digest) {
			return std::unexpected(fail_with_report(
				"netplay-identity", "runtime-simulation-mismatch",
				th03::app::NewClassicNetplayReportKind::desync
			));
		}
		if(remote_runtime_digest_ack && !same_proof(
			*remote_runtime_digest_ack, *local_runtime_digest
		)) {
			return std::unexpected(fail_netplay(
				"netplay-identity", "runtime-proof-order"
			));
		}
		if(!send_simulation_digest(
			NewClassicNetplayDatagramType::simulation_digest_ack,
			local_runtime_digest->frame,
			local_runtime_digest->confirmed_gameplay_frame_count,
			local_runtime_digest->kind,
			local_runtime_digest->boundary,
			local_runtime_digest->digest
		)) {
			return std::unexpected(fail_netplay(
				"netplay-identity", "send-failed"
			));
		}
		verified_runtime_digest = *local_runtime_digest;
		verified_periodic_gameplay_frame = std::max(
			verified_periodic_gameplay_frame,
			(verified_runtime_digest->confirmed_gameplay_frame_count /
				th03::app::new_classic_netplay_simulation_digest_interval) *
				th03::app::new_classic_netplay_simulation_digest_interval
		);
		runtime_digest_peer_ack = remote_runtime_digest_ack.has_value();
		std::cout << "status=identity-confirmed\nidentity-frame=" <<
			verified_runtime_digest->frame << '\n';
		std::cout.flush();
		local_runtime_digest.reset();
		remote_runtime_digest.reset();
		remote_runtime_digest_ack.reset();
		return {};
	};
	const auto drain_local_runtime_digests = [&]() -> std::expected<void, int> {
		if(local_runtime_digest ||
			(verified_runtime_digest && !runtime_digest_peer_ack)) {
			return match_runtime_digests();
		}
		for(;;) {
			auto digest = (*session)->take_confirmed_simulation_digest();
			if(!digest) {
				log_netplay_session_error(digest.error());
				return std::unexpected(fail_netplay(
					"netplay-identity", "digest-journal-failed"
				));
			}
			if(!*digest) {
				return match_runtime_digests();
			}
			const bool periodic = (**digest).kind ==
				th03::app::NewClassicNetplaySimulationDigestKind::periodic;
			const bool boundary = (**digest).kind ==
				th03::app::NewClassicNetplaySimulationDigestKind::boundary;
			const auto expected_periodic = verified_periodic_gameplay_frame +
				th03::app::new_classic_netplay_simulation_digest_interval;
			if((periodic && ((**digest).boundary !=
					th03::app::NewClassicNetplayBoundaryProof::none ||
				(**digest).confirmed_gameplay_frame_count != expected_periodic)) ||
				(boundary && (**digest).boundary ==
					th03::app::NewClassicNetplayBoundaryProof::none) ||
				(!periodic && !boundary) ||
				(verified_runtime_digest &&
				 ((**digest).frame <= verified_runtime_digest->frame ||
				  (**digest).confirmed_gameplay_frame_count <
					verified_runtime_digest->confirmed_gameplay_frame_count))) {
				return std::unexpected(fail_netplay(
					"netplay-identity", "runtime-proof-lag"
				));
			}
			local_runtime_digest = **digest;
			if(!send_simulation_digest(
				NewClassicNetplayDatagramType::simulation_digest,
				local_runtime_digest->frame,
				local_runtime_digest->confirmed_gameplay_frame_count,
				local_runtime_digest->kind,
				local_runtime_digest->boundary,
				local_runtime_digest->digest
			)) {
				return std::unexpected(fail_netplay(
					"netplay-identity", "send-failed"
				));
			}
		}
	};
	const auto drain_confirmed_transcript = [&]() -> std::expected<void, int> {
		for(;;) {
			auto frame = (*session)->take_confirmed_input_frame();
			if(!frame) {
				log_netplay_session_error(frame.error());
				return std::unexpected(fail_netplay(
					"netplay-replay", "transcript-journal-failed"
				));
			}
			if(!*frame) {
				return {};
			}
			if((**frame).frame != replay_capture.frame_count() ||
				(**frame).frame >= local_deadline_missed.size()) {
				return std::unexpected(fail_netplay(
					"netplay-replay", "transcript-order-failed"
				));
			}
			if(auto appended = replay_capture.append_confirmed_frame(
				**frame,
				local_deadline_missed[static_cast<std::size_t>(
					(**frame).frame
				)] != 0U
			); !appended) {
				return std::unexpected(fail_netplay(
					"netplay-replay", "capture-append-failed"
				));
			}
		}
	};
	const auto classify_local_frame = [&](
		std::optional<std::uint64_t> frame
	) -> std::expected<void, int> {
		if(!frame) {
			return {};
		}
		if(*frame != local_deadline_missed.size()) {
			return std::unexpected(fail_netplay(
				"netplay-replay", "deadline-order-failed"
			));
		}
		const auto next_deadline = scheduler->next_deadline_ns();
		local_deadline_missed.push_back(static_cast<std::uint8_t>(
			next_deadline &&
			th03::platform::sdl_ticks_now_ns() >= *next_deadline
		));
		return {};
	};
	const auto drain_post_match_audio = [&]() -> std::expected<void, int> {
		while(post_match) {
			const auto command = post_match->take_audio_command();
			if(!command) {
				break;
			}
			if(auto applied = audio_session->apply(*command); !applied) {
				return std::unexpected(fail(
					"netplay-post-match-audio", applied.error()
				));
			}
		}
		return {};
	};
	const auto resume_pause_if_ready = [
		&
	](th03::app::TickTimestampNs loop_cutoff_ns)
		-> std::expected<void, int> {
		if(!pause_barrier || !pause_barrier->ready() ||
			(*session)->status() !=
				NewClassicNetplayMainSessionStatus::paused) {
			return {};
		}
		if(!resume_countdown_started_ns) {
			resume_countdown_started_ns = loop_cutoff_ns;
			resume_countdown_seconds =
				th03::app::new_classic_netplay_resume_countdown_seconds;
			if(pause_overlay) {
				pause_overlay->set_resume_countdown(resume_countdown_seconds);
			}
			presentation_pending = true;
			std::cout << "status=resume-countdown\nseconds=" <<
				static_cast<unsigned>(
					th03::app::new_classic_netplay_resume_countdown_seconds
				) << '\n';
			std::cout.flush();
			return {};
		}
		const auto elapsed = loop_cutoff_ns >= *resume_countdown_started_ns ?
			loop_cutoff_ns - *resume_countdown_started_ns : 0U;
		const auto remaining =
			th03::app::new_classic_netplay_resume_countdown_remaining(elapsed);
		if(remaining) {
			if(resume_countdown_seconds != remaining) {
				resume_countdown_seconds = remaining;
				if(pause_overlay) {
					pause_overlay->set_resume_countdown(*remaining);
				}
				presentation_pending = true;
			}
			return {};
		}
		if(auto applied = audio_session->apply({
			.type = th03::app::ClassicAudioCommandType::resume_song,
		}); !applied) {
			return std::unexpected(fail(
				"netplay-pause-audio", applied.error()
			));
		}
		const auto resumed = (*session)->resume_pause();
		if(!resumed || !resumed->pause_completion) {
			if(!resumed) {
				log_netplay_session_error(resumed.error());
			}
			return std::unexpected(fail_netplay(
				"netplay-pause", "resume-failed"
			));
		}
		if(auto recorded = replay_capture.pause_resumed(
			*resumed->pause_completion
		); !recorded) {
			return std::unexpected(fail_netplay(
				"netplay-replay", "pause-resume-capture-failed"
			));
		}
		while(const auto command = (*session)->take_audio_command()) {
			if(auto applied = audio_session->apply(*command); !applied) {
				return std::unexpected(fail(
					"netplay-pause-audio", applied.error()
				));
			}
		}
		// Every scheduler observation in one host iteration must use its single
		// captured cutoff. Sampling a newer clock here would make the remaining
		// work in that iteration look like a timestamp regression.
		if(auto rebased = scheduler->rebase_after_modal(loop_cutoff_ns); !rebased) {
			return std::unexpected(fail_tick(rebased.error()));
		}
		if(auto reset = mapper.reset_after_modal(*scheduler, loop_cutoff_ns);
			!reset) {
			return std::unexpected(fail(
				"netplay-pause-input", reset.error()
			));
		}
		retry_blocked_event = false;
		stalled_input.reset();
		pause_overlay.reset();
		resume_countdown_started_ns.reset();
		resume_countdown_seconds.reset();
		presentation_pending = true;
		last_pause_signal_send_ns = 0U;
		std::cout << "status=resumed\nframe=" << pause_barrier->frame() << '\n';
		std::cout.flush();
		return {};
	};

	const auto submit_input_datagram = [&](const NewClassicNetplayDatagram &packet)
		-> bool {
		if(packet.type != NewClassicNetplayDatagramType::input ||
			packet.sender != remote_player || packet.session_id != session_id ||
			packet.contract_hash != contract_hash) {
			return false;
		}
		for(std::size_t offset = packet.input_count; offset > 0U; --offset) {
			const auto index = offset - 1U;
			const auto frame = packet.latest_frame - index;
			const auto submitted = (*session)->submit_remote(
				frame, packet.inputs[index]
			);
			if(!submitted) {
				return false;
			}
			presentation_pending = presentation_pending ||
				submitted->surface_publishable;
		}
		return true;
	};
	const auto settle_confirmed_boundary = [
		&
	](th03::app::TickTimestampNs loop_cutoff_ns)
		-> std::expected<void, int> {
		if((*session)->status() !=
			NewClassicNetplayMainSessionStatus::boundary_confirmed) {
			return {};
		}
		if(const auto digests = drain_local_runtime_digests(); !digests) {
			return digests;
		}
		const auto expected_boundary = (*session)->boundary() ?
			th03::app::new_classic_netplay_boundary_proof(
				*(*session)->boundary()
			) :
			th03::app::NewClassicNetplayBoundaryProof::none;
		if(local_runtime_digest || remote_runtime_digest ||
			remote_runtime_digest_ack || !verified_runtime_digest ||
			!runtime_digest_peer_ack) {
			return {};
		}
		if(verified_runtime_digest->kind !=
				th03::app::NewClassicNetplaySimulationDigestKind::boundary ||
			verified_runtime_digest->boundary != expected_boundary ||
			verified_runtime_digest->frame != (*session)->next_frame()) {
			return std::unexpected(fail_netplay(
				"netplay-boundary", "identity-missing"
			));
		}
		const auto continued = (*session)->continue_confirmed_boundary();
		if(!continued) {
			std::cerr << "netplay-boundary-status=" <<
				((*session)->boundary() ? static_cast<unsigned>(
					*(*session)->boundary()
				) : 255U) << " netplay-input-context=" <<
				static_cast<unsigned>((*session)->input_context()) << '\n';
			log_netplay_session_error(continued.error());
			return std::unexpected(fail_netplay(
				"netplay-boundary", "lifecycle-failed"
			));
		}
		presentation_pending = presentation_pending ||
			continued->surface_publishable;
		if(const auto transcript = drain_confirmed_transcript(); !transcript) {
			return std::unexpected(transcript.error());
		}
		if(*continued->boundary == th03::StepStatus::paused) {
			const auto retained_frame_count = (*session)->next_frame();
			if(!continued->frame || retained_frame_count == 0U ||
				*continued->frame + 1U != retained_frame_count ||
				local_deadline_missed.size() < retained_frame_count) {
				return std::unexpected(fail_netplay(
					"netplay-pause", "host-timeline-order"
				));
			}
			// A remote Pause edge can roll back and discard a locally predicted
			// suffix. Keep every host-side cursor on the retained confirmed prefix
			// so Resume cannot resend or classify one of those discarded frames.
			local_deadline_missed.resize(static_cast<std::size_t>(
				retained_frame_count
			));
			local_frames = std::min(local_frames, retained_frame_count);
			newest_local_frame = *continued->frame;
			auto barrier = th03::app::NewClassicNetplayPauseBarrier::create(
				verified_runtime_digest->frame,
				verified_runtime_digest->confirmed_gameplay_frame_count
			);
			if(!barrier) {
				return std::unexpected(fail_netplay(
					"netplay-pause", "boundary-identity"
				));
			}
			pause_barrier = std::move(*barrier);
			pause_overlay.emplace(
				false, pause_assets,
				th03::app::NewClassicPausePresentation::netplay_wait,
				pause_text
			);
			resume_countdown_started_ns.reset();
			resume_countdown_seconds.reset();
			if(pause_overlay->consume({}) !=
				th03::app::NewClassicPauseStatus::running) {
				return std::unexpected(fail_netplay(
					"netplay-pause", "overlay-state"
				));
			}
			presentation_pending = true;
			if(auto applied = audio_session->apply({
				.type = th03::app::ClassicAudioCommandType::pause_song,
			}); !applied) {
				return std::unexpected(fail(
					"netplay-pause-audio", applied.error()
				));
			}
			if(auto rebased = scheduler->rebase_after_modal(loop_cutoff_ns);
				!rebased) {
				return std::unexpected(fail_tick(rebased.error()));
			}
			if(auto reset = mapper.reset_after_modal(*scheduler, loop_cutoff_ns);
				!reset) {
				return std::unexpected(fail(
					"netplay-pause-input", reset.error()
				));
			}
			retry_blocked_event = false;
			stalled_input.reset();
			last_pause_signal_send_ns = 0U;
			std::cout << "status=paused\n"
				"resume=release-then-escape-on-both-peers\n";
			std::cout.flush();
		} else if(*continued->boundary == th03::StepStatus::round_transition) {
			if(auto started = replay_capture.round_started(
				(*session)->process().round_start_observation()
			); !started) {
				return std::unexpected(fail_netplay(
					"netplay-replay", "round-start-failed"
				));
			}
		}
		std::cout << "status=boundary-confirmed\nboundary=" <<
			static_cast<unsigned>(*continued->boundary) << "\nframe=" <<
			*continued->frame << '\n';
		std::cout.flush();
		return {};
	};

	for(;;) {
		const auto now_ns = th03::platform::sdl_ticks_now_ns();
		if(!transport_detached &&
			now_ns - last_network_metric_sample_ns >= 250'000'000U) {
			if(const auto round_trip = wire.round_trip_time_ms()) {
				latest_round_trip_time_ms = *round_trip;
			}
			last_network_metric_sample_ns = now_ns;
		}
		if(now_ns - start_ns > 30'000'000'000ULL && !established) {
			return fail_netplay("netplay-handshake", "timeout");
		}
		if(established && !post_match &&
			now_ns - last_receive_ns > 5'000'000'000ULL) {
			return fail_with_report(
				"netplay-peer", "peer-timeout",
				th03::app::NewClassicNetplayReportKind::peer_timeout
			);
		}

		if(!retry_blocked_event) {
			// Escape is source-global Cancel in this route and therefore the
			// synchronized Pause request. Only window close/SDL quit leaves.
			auto events = mapper.pump_events(*scheduler, false);
			if(!events) {
				return fail("input-pump", events.error());
			}
			retry_blocked_event = events->queue_blocked;
		}
		const bool desired_focus_mute = parsed.mute_when_unfocused &&
			!mapper.main_window_focused();
		if(desired_focus_mute && !audio_focus_muted && audio_device) {
			if(auto cleared = audio_device->clear(); !cleared) {
				warn_audio_device("netplay-audio-focus-clear", cleared.error());
				audio_device.reset();
			}
		}
		audio_focus_muted = desired_focus_mute;

		for(; !transport_detached;) {
			const auto received = wire.receive();
			if(!received) {
				if(!th03::app::new_classic_netplay_peer_departure_is_fatal(
					static_cast<bool>(post_match)
				)) {
					transport_detached = true;
					std::cout << "status=post-match-peer-detached\n";
					std::cout.flush();
					break;
				}
				return fail_netplay("netplay-receive", received.error());
			}
			if(!*received) {
				break;
			}
			if((**received).channel == NetplayWireChannel::control &&
				(**received).bytes.size() ==
				th03::app::new_classic_netplay_lobby_bytes) {
				const auto lobby_packet =
					th03::app::decode_new_classic_netplay_lobby_datagram(
						(**received).bytes
					);
				if(mode == DirectVsNetplayMode::host && lobby_packet &&
					(lobby_packet->type == th03::app::
						NewClassicNetplayLobbyDatagramType::join_request ||
					 lobby_packet->type == th03::app::
						NewClassicNetplayLobbyDatagramType::join_ready) &&
					lobby.receive(*lobby_packet)) {
					if(const auto response = lobby.outbound(); response) {
						const auto encoded = th03::app::
							encode_new_classic_netplay_lobby_datagram(*response);
						if(encoded) {
							(void)wire.send(NetplayWireChannel::control, *encoded);
						}
					}
				}
				continue;
			}
			const auto packet = th03::app::decode_new_classic_netplay_datagram(
				(**received).bytes
			);
			if(!packet || packet->contract_hash != contract_hash) {
				continue;
			}
			if(wire.rtc() &&
				(((**received).channel == NetplayWireChannel::gameplay) !=
				 (packet->type == NewClassicNetplayDatagramType::input))) {
				return fail_netplay(
					"netplay-receive", "channel-contract"
				);
			}
			if(mode == DirectVsNetplayMode::host &&
				packet->type == NewClassicNetplayDatagramType::hello &&
				packet->sender == remote_player) {
				const NewClassicNetplayDatagram accepted{
					.type = NewClassicNetplayDatagramType::accept,
					.sender = local_player,
					.session_id = session_id,
					.contract_hash = contract_hash,
				};
				if(!send_netplay_datagram(wire, accepted)) {
					return fail_netplay("netplay-accept", "send-failed");
				}
				last_receive_ns = now_ns;
				continue;
			}
			if(mode == DirectVsNetplayMode::join &&
				packet->type == NewClassicNetplayDatagramType::accept &&
				packet->sender == remote_player && packet->session_id != 0U) {
				if(session_id != 0U && session_id != packet->session_id) {
					continue;
				}
				session_id = packet->session_id;
				last_receive_ns = now_ns;
				continue;
			}
			if(packet->type ==
					NewClassicNetplayDatagramType::simulation_digest &&
				packet->sender == remote_player &&
				packet->session_id == session_id) {
				last_receive_ns = now_ns;
				if(packet->simulation_digest_kind ==
					th03::app::NewClassicNetplaySimulationDigestKind::initial) {
					if(packet->simulation_digest != *initial_simulation_digest) {
						return fail_with_report(
							"netplay-identity", "initial-simulation-mismatch",
							th03::app::NewClassicNetplayReportKind::desync
						);
					}
					if(!established) {
						established = true;
						if(const auto rebased = scheduler->rebase_after_modal(
							now_ns
						); !rebased) {
							return fail_tick(rebased.error());
						}
						std::cout << "status=connected\nsession=" << session_id <<
							"\nidentity-frame=0\n";
						std::cout.flush();
					}
					continue;
				}
				if(!established) {
					return fail_netplay(
						"netplay-identity", "runtime-proof-order"
					);
				}
				const ConfirmedSimulationDigest received_digest{
					.frame = packet->latest_frame,
					.confirmed_gameplay_frame_count =
						packet->confirmed_frame_count,
					.kind = packet->simulation_digest_kind,
					.boundary = packet->boundary_proof,
					.digest = packet->simulation_digest,
				};
				if(verified_runtime_digest &&
					received_digest.frame == verified_runtime_digest->frame) {
					if(!same_proof(
						received_digest, *verified_runtime_digest
					)) {
						return fail_with_report(
							"netplay-identity", "runtime-simulation-mismatch",
							th03::app::NewClassicNetplayReportKind::desync
						);
					}
					if(!send_simulation_digest(
						NewClassicNetplayDatagramType::simulation_digest_ack,
						verified_runtime_digest->frame,
						verified_runtime_digest->
							confirmed_gameplay_frame_count,
						verified_runtime_digest->kind,
						verified_runtime_digest->boundary,
						verified_runtime_digest->digest
					)) {
						return fail_netplay(
							"netplay-identity", "send-failed"
						);
					}
					continue;
				}
				if(verified_runtime_digest &&
					received_digest.frame < verified_runtime_digest->frame) {
					continue;
				}
				const bool periodic = received_digest.kind ==
					th03::app::NewClassicNetplaySimulationDigestKind::periodic;
				const bool boundary = received_digest.kind ==
					th03::app::NewClassicNetplaySimulationDigestKind::boundary;
				const auto expected_periodic =
					verified_periodic_gameplay_frame +
					th03::app::new_classic_netplay_simulation_digest_interval;
				if((periodic && (received_digest.boundary !=
						th03::app::NewClassicNetplayBoundaryProof::none ||
					received_digest.confirmed_gameplay_frame_count !=
						expected_periodic)) ||
					(boundary && received_digest.boundary ==
						th03::app::NewClassicNetplayBoundaryProof::none) ||
					(!periodic && !boundary) ||
					(verified_runtime_digest &&
					 received_digest.confirmed_gameplay_frame_count <
						verified_runtime_digest->confirmed_gameplay_frame_count) ||
					(remote_runtime_digest &&
					 !same_proof(*remote_runtime_digest, received_digest))) {
					return fail_netplay(
						"netplay-identity", "runtime-proof-order"
					);
				}
				remote_runtime_digest = received_digest;
				if(const auto matched = match_runtime_digests(); !matched) {
					return matched.error();
				}
				continue;
			}
			if(packet->type ==
					NewClassicNetplayDatagramType::simulation_digest_ack &&
				packet->sender == remote_player &&
				packet->session_id == session_id) {
				last_receive_ns = now_ns;
				if(!established) {
					return fail_netplay(
						"netplay-identity", "runtime-proof-order"
					);
				}
				const ConfirmedSimulationDigest received_ack{
					.frame = packet->latest_frame,
					.confirmed_gameplay_frame_count =
						packet->confirmed_frame_count,
					.kind = packet->simulation_digest_kind,
					.boundary = packet->boundary_proof,
					.digest = packet->simulation_digest,
				};
				if(verified_runtime_digest &&
					th03::app::new_classic_netplay_proof_order(
						verified_runtime_digest->frame, received_ack.frame
					) == th03::app::NewClassicNetplayProofOrder::stale) {
					continue;
				}
				if(verified_runtime_digest &&
					received_ack.frame == verified_runtime_digest->frame) {
					if(!same_proof(received_ack, *verified_runtime_digest)) {
						return fail_with_report(
							"netplay-identity", "runtime-simulation-mismatch",
							th03::app::NewClassicNetplayReportKind::desync
						);
					}
					const bool peer_was_already_acknowledged =
						runtime_digest_peer_ack;
					runtime_digest_peer_ack = true;
					// A peer that repeats an acknowledgement after we already
					// observed theirs is still waiting for ours: the prior reply
					// was lost. Reply once; the peer's first observation does not
					// echo, so this cannot become an acknowledgement loop.
					if(peer_was_already_acknowledged &&
						!send_simulation_digest(
							NewClassicNetplayDatagramType::
								simulation_digest_ack,
							verified_runtime_digest->frame,
							verified_runtime_digest->
								confirmed_gameplay_frame_count,
							verified_runtime_digest->kind,
							verified_runtime_digest->boundary,
							verified_runtime_digest->digest
						)) {
						return fail_netplay(
							"netplay-identity", "send-failed"
						);
					}
					continue;
				}
				if(!local_runtime_digest ||
					!same_proof(received_ack, *local_runtime_digest) ||
					(remote_runtime_digest_ack &&
					 !same_proof(*remote_runtime_digest_ack, received_ack))) {
					return fail_netplay(
						"netplay-identity", "runtime-proof-order"
					);
				}
				remote_runtime_digest_ack = received_ack;
				if(remote_runtime_digest &&
					!same_proof(*remote_runtime_digest, received_ack)) {
					return fail_with_report(
						"netplay-identity", "runtime-simulation-mismatch",
						th03::app::NewClassicNetplayReportKind::desync
					);
				}
				remote_runtime_digest = received_ack;
				if(const auto matched = match_runtime_digests(); !matched) {
					return matched.error();
				}
				continue;
			}
			if(packet->type == NewClassicNetplayDatagramType::disconnect &&
				packet->sender == remote_player &&
				packet->session_id == session_id) {
				if(!th03::app::new_classic_netplay_peer_departure_is_fatal(
					static_cast<bool>(post_match)
				)) {
					transport_detached = true;
					std::cout << "status=post-match-peer-detached\n";
					std::cout.flush();
					break;
				}
				std::cout << "status=peer-left\n";
				return show_report(
					th03::app::NewClassicNetplayReportKind::peer_left
				);
			}
			if((packet->type ==
					NewClassicNetplayDatagramType::pause_waiting ||
				 packet->type ==
					NewClassicNetplayDatagramType::resume_ready ||
				 packet->type ==
					NewClassicNetplayDatagramType::forfeit_request ||
				 packet->type ==
					NewClassicNetplayDatagramType::forfeit_ack) &&
				packet->sender == remote_player &&
				packet->session_id == session_id) {
				if(!pause_barrier ||
					packet->latest_frame < pause_barrier->frame()) {
					continue;
				}
				if(const auto received_pause = pause_barrier->receive(
					packet->type, packet->latest_frame,
					packet->confirmed_frame_count
				); !received_pause) {
					return fail_netplay(
						"netplay-pause", "signal-identity"
					);
				}
				last_receive_ns = now_ns;
				if(packet->type ==
					NewClassicNetplayDatagramType::forfeit_request) {
					const bool mutual = pause_barrier->local_forfeit();
					const NewClassicNetplayDatagram acknowledgement{
						.type = NewClassicNetplayDatagramType::forfeit_ack,
						.sender = local_player,
						.session_id = session_id,
						.contract_hash = contract_hash,
						.latest_frame = pause_barrier->frame(),
						.confirmed_frame_count =
							pause_barrier->confirmed_gameplay_frame_count(),
						.boundary_proof =
							th03::app::NewClassicNetplayBoundaryProof::pause,
					};
					bool sent = false;
					for(std::uint8_t attempt = 0U; attempt < 3U; ++attempt) {
						sent = send_netplay_datagram(
							wire, acknowledgement
						) || sent;
					}
					if(!sent) {
						return fail_netplay(
							"netplay-forfeit", "ack-send-failed"
						);
					}
					return finish_forfeit(
						mutual ? "mutual" : "win",
						mutual ?
							th03::app::NewClassicNetplayReportKind::forfeit_mutual :
							th03::app::NewClassicNetplayReportKind::forfeit_win
					);
				}
				if(packet->type == NewClassicNetplayDatagramType::forfeit_ack) {
					return finish_forfeit(
						"loss",
						th03::app::NewClassicNetplayReportKind::forfeit_loss
					);
				}
				if(const auto resumed = resume_pause_if_ready(now_ns); !resumed) {
					return resumed.error();
				}
				continue;
			}
			if(established &&
				packet->type == NewClassicNetplayDatagramType::input &&
				packet->sender == remote_player &&
				packet->session_id == session_id) {
				if(pause_barrier && packet->latest_frame >=
					pause_barrier->frame()) {
					if((*session)->status() ==
						NewClassicNetplayMainSessionStatus::paused) {
						if(const auto observed =
							pause_barrier->observe_peer_gameplay(
								packet->latest_frame
							); !observed) {
							return fail_netplay(
								"netplay-pause", "peer-resume-order"
							);
						}
						if(const auto resumed = resume_pause_if_ready(now_ns);
							!resumed) {
							return resumed.error();
						}
					}
				}
				if(!submit_input_datagram(*packet)) {
					continue;
				}
				if(pause_barrier && packet->latest_frame >=
					pause_barrier->frame() && (*session)->status() !=
						NewClassicNetplayMainSessionStatus::paused) {
					pause_barrier.reset();
				}
				last_receive_ns = now_ns;
				peer_started_input = true;
				if(const auto transcript = drain_confirmed_transcript();
					!transcript) {
					return transcript.error();
				}
			}
		}
		if(established && !transport_detached) {
			if(const auto digests = drain_local_runtime_digests(); !digests) {
				return digests.error();
			}
		}
		if(const auto settled = settle_confirmed_boundary(now_ns); !settled) {
			return settled.error();
		}
		if(const auto published = publish_confirmed_audio(); !published) {
			return published.error();
		}

		if(!established && mode == DirectVsNetplayMode::join &&
			session_id == 0U &&
			now_ns - last_send_ns >= 100'000'000U) {
			const NewClassicNetplayDatagram hello{
				.type = NewClassicNetplayDatagramType::hello,
				.sender = local_player,
				.contract_hash = contract_hash,
			};
			if(!send_netplay_datagram(wire, hello)) {
				return fail_netplay("netplay-hello", "send-failed");
			}
			last_send_ns = now_ns;
		}
		if(session_id != 0U && !peer_started_input &&
			now_ns - last_initial_digest_send_ns >= 100'000'000U) {
			if(!send_simulation_digest(
				NewClassicNetplayDatagramType::simulation_digest,
				0U, 0U,
				th03::app::NewClassicNetplaySimulationDigestKind::initial,
				th03::app::NewClassicNetplayBoundaryProof::none,
				*initial_simulation_digest
			)) {
				return fail_netplay("netplay-identity", "send-failed");
			}
			last_initial_digest_send_ns = now_ns;
		}

		if(established && stalled_input &&
			th03::app::new_classic_netplay_main_session_accepts_local_input(
				(*session)->status()
			)) {
			const auto frame = (*session)->next_frame();
			const auto advanced = (*session)->advance_local(*stalled_input);
			if(!advanced) {
				log_netplay_session_error(advanced.error());
				return fail_netplay("netplay-advance", "session-failed");
			}
			if(advanced->status !=
					NewClassicNetplayMainSessionStatus::prediction_stalled &&
				advanced->status !=
					NewClassicNetplayMainSessionStatus::remote_waiting) {
				stalled_input.reset();
				if(advanced->frame) {
					newest_local_frame = frame;
					++local_frames;
				}
				if(const auto classified = classify_local_frame(advanced->frame);
					!classified) {
					return classified.error();
				}
				presentation_pending = presentation_pending ||
					advanced->surface_publishable;
				if(const auto transcript = drain_confirmed_transcript();
					!transcript) {
					return transcript.error();
				}
			}
			if(const auto settled = settle_confirmed_boundary(now_ns); !settled) {
				return settled.error();
			}
		}

		if(established && !stalled_input &&
			th03::app::new_classic_netplay_main_session_accepts_local_input(
				(*session)->status()
			)) {
			for(;;) {
				const auto due = scheduler->pop_due(now_ns);
				if(!due) {
					return fail_tick(due.error());
				}
				if(!*due) {
					break;
				}
				if((**due).app_quit_requested) {
					return leave_preview();
				}
				auto input = netplay_owned_input((**due).input);
				if(((**due).input.system & 0x1000U) != 0U) {
					input.control = th03::app::
						NewClassicNetplayControl::pause_request;
				}
				const auto frame = (*session)->next_frame();
				const auto advanced = (*session)->advance_local(input);
				if(!advanced) {
					log_netplay_session_error(advanced.error());
					return fail_netplay("netplay-advance", "session-failed");
				}
				const bool input_staged = advanced->frame.has_value() ||
					advanced->status ==
						NewClassicNetplayMainSessionStatus::prediction_stalled ||
					advanced->status ==
						NewClassicNetplayMainSessionStatus::remote_waiting;
				if(input_staged) {
					newest_local_frame = frame;
					if(!send_netplay_input_history(
						wire, **session, local_player, session_id,
						contract_hash, frame
					)) {
						return fail_netplay("netplay-input", "send-failed");
					}
					last_send_ns = now_ns;
				}
				presentation_pending = presentation_pending ||
					advanced->surface_publishable;
				if(const auto classified = classify_local_frame(advanced->frame);
					!classified) {
					return classified.error();
				}
				if(const auto transcript = drain_confirmed_transcript();
					!transcript) {
					return transcript.error();
				}
				if(advanced->status ==
						NewClassicNetplayMainSessionStatus::prediction_stalled ||
					advanced->status ==
						NewClassicNetplayMainSessionStatus::remote_waiting) {
					stalled_input = input;
					break;
				}
				if(advanced->frame) {
					++local_frames;
				}
				if(const auto settled = settle_confirmed_boundary(now_ns); !settled) {
					return settled.error();
				}
				if(advanced->status !=
					NewClassicNetplayMainSessionStatus::running) {
					break;
				}
				if(retry_blocked_event) {
					auto retried = mapper.pump_events(*scheduler, false);
					if(!retried) {
						return fail("input-pump", retried.error());
					}
					retry_blocked_event = retried->queue_blocked;
				}
			}
		}
		if(established && pause_barrier && (*session)->status() ==
			NewClassicNetplayMainSessionStatus::paused) {
			for(;;) {
				const auto due = scheduler->pop_due(now_ns);
				if(!due) {
					return fail_tick(due.error());
				}
				if(!*due) {
					break;
				}
				if((**due).app_quit_requested) {
					return leave_preview();
				}
				if(!pause_overlay) {
					return fail_netplay("netplay-pause", "overlay-missing");
				}
				const auto was_ready = pause_barrier->local_ready();
				const auto was_forfeit = pause_barrier->local_forfeit();
				const auto pause_status = pause_overlay->consume(
					th03::app::project_new_classic_netplay_pause_input(
						(**due).input
					)
				);
				presentation_pending = true;
				if(pause_status == th03::app::NewClassicPauseStatus::resume) {
					pause_barrier->choose_local_resume();
				} else if(pause_status ==
					th03::app::NewClassicPauseStatus::discard_exit) {
					pause_barrier->choose_local_forfeit();
				} else if(pause_status !=
					th03::app::NewClassicPauseStatus::running) {
					return fail_netplay("netplay-pause", "invalid-choice");
				}
				if(!was_ready && pause_barrier->local_ready()) {
					if(pause_overlay) {
						pause_overlay->set_waiting_for_opponent(
							!pause_barrier->ready()
						);
					}
					last_pause_signal_send_ns = 0U;
					std::cout << "status=resume-ready-local\n";
					std::cout.flush();
				}
				if(!was_forfeit && pause_barrier->local_forfeit()) {
					last_pause_signal_send_ns = 0U;
					std::cout << "status=forfeit-requested-local\n";
					std::cout.flush();
				}
				if(const auto resumed = resume_pause_if_ready(now_ns); !resumed) {
					return resumed.error();
				}
				if((*session)->status() !=
					NewClassicNetplayMainSessionStatus::paused) {
					break;
				}
			}
		}
		if(post_match && post_match->status() ==
			NewClassicNetplayPostMatchStatus::running) {
			for(;;) {
				const auto due = scheduler->pop_due(now_ns);
				if(!due) {
					return fail_tick(due.error());
				}
				if(!*due) {
					break;
				}
				const bool dismiss_pressed = (**due).input.system != 0U ||
					(**due).input.p1 != 0U || (**due).input.p2 != 0U;
				auto advanced = post_match->advance(
					audio_session->status(), dismiss_pressed,
					(**due).app_quit_requested
				);
				if(!advanced) {
					return fail("netplay-post-match", advanced.error());
				}
				if(const auto drained = drain_post_match_audio(); !drained) {
					return drained.error();
				}
				if(const auto audio = advance_audio_tick(); !audio) {
					return audio.error();
				}
				presentation_pending = presentation_pending ||
					advanced->surface_changed;
				if(advanced->status ==
					NewClassicNetplayPostMatchStatus::quit_requested) {
					return leave_preview();
				}
				if(advanced->status ==
					NewClassicNetplayPostMatchStatus::complete) {
					break;
				}
			}
		}

		if(established && !transport_detached && pause_barrier &&
			now_ns - last_pause_signal_send_ns >= 50'000'000U) {
			const NewClassicNetplayDatagram pause_signal{
				.type = pause_barrier->outbound_signal(),
				.sender = local_player,
				.session_id = session_id,
				.contract_hash = contract_hash,
				.latest_frame = pause_barrier->frame(),
				.confirmed_frame_count =
					pause_barrier->confirmed_gameplay_frame_count(),
				.boundary_proof =
					th03::app::NewClassicNetplayBoundaryProof::pause,
			};
			if(!send_netplay_datagram(wire, pause_signal)) {
				return fail_netplay("netplay-pause", "send-failed");
			}
			last_pause_signal_send_ns = now_ns;
		}
		if(established && !transport_detached && !post_match &&
			newest_local_frame &&
			th03::app::new_classic_netplay_main_session_resends_input_history(
				(*session)->status()
			) &&
			now_ns - last_send_ns >= 50'000'000U) {
			if(!send_netplay_input_history(
				wire, **session, local_player, session_id,
				contract_hash, *newest_local_frame
			)) {
				return fail_netplay("netplay-resend", "send-failed");
			}
			last_send_ns = now_ns;
		}
		if(established && !transport_detached) {
			if(const auto digests = drain_local_runtime_digests(); !digests) {
				return digests.error();
			}
			const auto &outbound_digest = local_runtime_digest ?
				local_runtime_digest : verified_runtime_digest;
			if(outbound_digest && now_ns - last_runtime_digest_send_ns >=
				100'000'000U &&
				(local_runtime_digest || !runtime_digest_peer_ack)) {
				if(!send_simulation_digest(
					local_runtime_digest ?
						NewClassicNetplayDatagramType::simulation_digest :
						NewClassicNetplayDatagramType::simulation_digest_ack,
					outbound_digest->frame,
					outbound_digest->confirmed_gameplay_frame_count,
					outbound_digest->kind,
					outbound_digest->boundary,
					outbound_digest->digest
				)) {
					return fail_netplay("netplay-identity", "send-failed");
				}
				last_runtime_digest_send_ns = now_ns;
			}
		}
		if(const auto published = publish_confirmed_audio(); !published) {
			return published.error();
		}
		const bool runtime_proof_pending = local_runtime_digest ||
			remote_runtime_digest || remote_runtime_digest_ack ||
			(verified_runtime_digest && !runtime_digest_peer_ack);
		if((*session)->status() ==
				NewClassicNetplayMainSessionStatus::complete &&
			!runtime_proof_pending && !post_match) {
			if(!(*session)->transition()) {
				return fail_netplay(
					"netplay-post-match", "transition-missing"
				);
			}
			auto created = th03::app::NewClassicNetplayPostMatch::create({
				.content = post_match_content,
				.process_start = process_start,
				.terminal = (*session)->process().canonical_state(),
				.transition = *(*session)->transition(),
			});
			if(!created) {
				return fail("netplay-post-match-create", created.error());
			}
			post_match = std::move(*created);
			if(const auto drained = drain_post_match_audio(); !drained) {
				return drained.error();
			}
			const auto resumed_at = th03::platform::sdl_ticks_now_ns();
			if(auto rebased = scheduler->rebase_after_modal(resumed_at);
				!rebased) {
				return fail_tick(rebased.error());
			}
			if(auto reset = mapper.reset_after_modal(*scheduler, resumed_at);
				!reset) {
				return fail("netplay-post-match-rebase", reset.error());
			}
			retry_blocked_event = false;
			presentation_pending = true;
			std::cout << "status=post-match\n";
			std::cout.flush();
		}

		if(presentation_pending) {
			if(post_match) {
				auto composed = post_match->compose_shown_surface(pixel_span);
				if(!composed) {
					return fail("netplay-post-match-compose", composed.error());
				}
				surface = *composed;
			} else {
				surface = (*session)->compose_shown_surface(pixel_span);
				if(!surface) {
					return fail("compose", surface.error());
				}
				if(pause_overlay) {
					*surface = pause_overlay->compose_over(*surface);
				}
			}
			if(const auto presented = platform.present(*surface); !presented) {
				return fail("present", presented.error());
			}
			presentation_pending = false;
		}
		if(post_match && post_match->status() ==
			NewClassicNetplayPostMatchStatus::complete) {
			if(!(*session)->transition()) {
				return fail_netplay("netplay-replay", "transition-missing");
			}
			auto completed = replay_capture.finish_mainl(
				(*session)->transition()->resident
			);
			if(!completed) {
				return fail_netplay("netplay-replay", "finalize-failed");
			}
			auto encoded = th03::formats::write_v14_replay(*completed);
			if(!encoded) {
				return fail_netplay("netplay-replay", "encode-failed");
			}
			bool replay_published = false;
			if(parsed.netplay_replay_output) {
				auto published = th03::app::commit_exact_replay_file(
					*parsed.netplay_replay_output, *encoded
				);
				if(!published) {
					std::cerr << "netplay-replay-store-code=" <<
						static_cast<unsigned>(published.error().code) << '\n';
					return fail_netplay(
						"netplay-replay", "publish-failed"
					);
				}
				replay_published = true;
			} else {
				auto resources =
					prepare_direct_netplay_replay_save_resources(
						replay_save_content
					);
				if(!resources) {
					return resources.error();
				}
				if(!parsed.replay_root.empty()) {
					resources->replay_root = parsed.replay_root;
				}
				auto saved = run_replay_save(
					std::move(*completed), resources->replay_root,
					parsed.tick_rate, platform, mapper,
					std::move(resources->patch_assets),
					std::move(resources->save_art),
					{
						.session = audio_session.get(),
						.device = &audio_device,
						.started = &audio_started,
					},
					th03::app::ReplaySlotLocalView{
						.origin = th03::app::ReplaySlotLocalOrigin::netplay,
						.local_player = static_cast<std::uint8_t>(
							local_player == NewClassicNetplayPlayer::player_one ?
								0U : 1U
						),
					}
				);
				if(saved.exit_code != 0) {
					return saved.exit_code;
				}
				if(saved.application_quit) {
					return leave_preview();
				}
				replay_published = saved.replay_published;
			}
			const auto metrics = report_metrics();
			const auto &netplay_stats = metrics.session;
			const auto missed_deadlines = metrics.missed_deadlines;
			std::cout << "status=match-complete\nframes=" <<
				(*session)->confirmed_frame_count() << "\nreplay-frames=" <<
				replay_capture.frame_count() << "\nreplay-ready=1\nreplay-bytes=" <<
				encoded->size() << "\nreplay-published=" <<
				(replay_published ? 1 : 0) <<
				"\nnetplay-resolved-frames=" << netplay_stats.resolved_frames <<
				"\nnetplay-predicted-frames=" << netplay_stats.predicted_frames <<
				"\nnetplay-rollback-requests=" << netplay_stats.rollback_requests <<
				"\nnetplay-rewinds=" << netplay_stats.rewinds <<
				"\nnetplay-resimulated-frames=" <<
					netplay_stats.resimulated_frames <<
				"\nnetplay-prediction-stalls=" <<
					netplay_stats.prediction_stall_events <<
				"\nnetplay-remote-waits=" <<
					netplay_stats.remote_wait_events <<
				"\nnetplay-maximum-prediction=" << static_cast<unsigned>(
					netplay_stats.maximum_prediction_depth
				) << "\nnetplay-missed-deadlines=" << missed_deadlines;
			if(metrics.round_trip_time_ms) {
				std::cout << "\nnetplay-round-trip-ms=" <<
					*metrics.round_trip_time_ms;
			}
			std::cout << '\n';
			std::cout.flush();
			return show_report(
				th03::app::NewClassicNetplayReportKind::match_complete
			);
		}
		if(parsed.smoke_ticks && local_frames >= *parsed.smoke_ticks &&
			(*session)->confirmed_frame_count() >= *parsed.smoke_ticks &&
			!runtime_proof_pending) {
			std::cout << "status=smoke-complete\nframes=" << local_frames
				<< "\nreplay-frames=" << replay_capture.frame_count()
				<< "\nreplay-ready=0\n";
			return 0;
		}

		if(!established) {
			// Discard pre-match source deadlines while still applying/retaining
			// physical state; the connected rebase starts frame zero cleanly.
			for(;;) {
				const auto due = scheduler->pop_due(now_ns);
				if(!due) {
					return fail_tick(due.error());
				}
				if(!*due) {
					break;
				}
				if((**due).app_quit_requested ||
					(((**due).input.system & 0x1000U) != 0U)) {
					return leave_preview();
				}
			}
		}
		SDL_DelayPrecise(500'000U);
	}
}

} // namespace th03::app::launcher_support
