#include "th03/platform/rtc_room.hpp"

#include <rtc/rtc.hpp>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <expected>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "th03/app/new_classic_netplay_signaling.hpp"

namespace th03::platform {
namespace {

constexpr std::size_t maximum_data_channel_message_bytes = 16U * 1024U;
constexpr std::size_t maximum_received_messages = 256U;
constexpr std::size_t maximum_received_bytes = 64U * 1024U;
constexpr std::size_t maximum_ice_servers = 16U;
constexpr std::chrono::milliseconds websocket_timeout{15'000};
constexpr std::chrono::milliseconds websocket_ping_interval{15'000};

[[nodiscard]] RtcRoomError error(RtcRoomErrorCode code) noexcept
{
	return {.code = code};
}

[[nodiscard]] bool valid_host(std::string_view host) noexcept
{
	if(host.empty() || host.size() > 253U || host.front() == '.' ||
		host.back() == '.') {
		return false;
	}
	std::size_t label_size = 0U;
	bool previous_hyphen = false;
	for(const auto character : host) {
		if(character == '.') {
			if(label_size == 0U || previous_hyphen) {
				return false;
			}
			label_size = 0U;
			previous_hyphen = false;
			continue;
		}
		const bool alphanumeric =
			(character >= 'a' && character <= 'z') ||
			(character >= 'A' && character <= 'Z') ||
			(character >= '0' && character <= '9');
		if(!alphanumeric && character != '-') {
			return false;
		}
		if((label_size == 0U && character == '-') || ++label_size > 63U) {
			return false;
		}
		previous_hyphen = character == '-';
	}
	return label_size != 0U && !previous_hyphen;
}

struct ParsedOrigin final {
	bool secure = true;
	std::string authority{};
	std::string host{};
};

[[nodiscard]] std::optional<ParsedOrigin> parse_origin(
	std::string_view origin
)
{
	if(origin.empty() || origin.size() > 512U ||
		origin.find_first_of("?#@\\\r\n") != std::string_view::npos) {
		return std::nullopt;
	}
	bool secure = true;
	std::string_view authority{};
	if(origin.starts_with("https://")) {
		authority = origin.substr(8U);
	} else if(origin.starts_with("http://")) {
		secure = false;
		authority = origin.substr(7U);
	} else {
		return std::nullopt;
	}
	if(authority.empty()) {
		return std::nullopt;
	}
	if(authority.find('/') != std::string_view::npos) {
		return std::nullopt;
	}
	auto host = authority;
	const auto colon = authority.rfind(':');
	if(colon != std::string_view::npos) {
		if(authority.find(':') != colon || colon == 0U ||
			colon + 1U == authority.size()) {
			return std::nullopt;
		}
		host = authority.substr(0U, colon);
		unsigned port = 0U;
		const auto port_text = authority.substr(colon + 1U);
		const auto parsed = std::from_chars(
			port_text.data(), port_text.data() + port_text.size(), port
		);
		if(parsed.ec != std::errc{} ||
			parsed.ptr != port_text.data() + port_text.size() ||
			port == 0U || port > std::numeric_limits<std::uint16_t>::max()) {
			return std::nullopt;
		}
	}
	if(!valid_host(host) || (!secure && host != "localhost" &&
		host != "127.0.0.1")) {
		return std::nullopt;
	}
	return ParsedOrigin{
		.secure = secure,
		.authority = std::string(authority),
		.host = std::string(host),
	};
}

[[nodiscard]] bool uuid_shape(std::string_view value) noexcept
{
	if(value.size() != 36U) {
		return false;
	}
	for(std::size_t index = 0U; index < value.size(); ++index) {
		if(index == 8U || index == 13U || index == 18U || index == 23U) {
			if(value[index] != '-') {
				return false;
			}
		} else if(!((value[index] >= '0' && value[index] <= '9') ||
			(value[index] >= 'a' && value[index] <= 'f'))) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool token_shape(std::string_view value) noexcept
{
	return value.size() >= 32U && value.size() <= 128U &&
		std::ranges::all_of(value, [](char character) {
			return (character >= 'A' && character <= 'Z') ||
				(character >= 'a' && character <= 'z') ||
				(character >= '0' && character <= '9') ||
				character == '-' || character == '_';
		});
}

[[nodiscard]] std::optional<rtc::IceServer> ice_server(
	const RtcIceServer &server
)
{
	if(server.url.empty() || server.url.size() > 1024U ||
		server.url.find_first_of("\r\n") != std::string_view::npos ||
		server.url.find('\0') != std::string_view::npos ||
		!(server.url.starts_with("stun:") || server.url.starts_with("turn:") ||
			server.url.starts_with("turns:")) ||
		server.username.size() > 512U || server.credential.size() > 512U) {
		return std::nullopt;
	}
	try {
		rtc::IceServer parsed(server.url);
		if(parsed.type == rtc::IceServer::Type::Stun) {
			if(!server.username.empty() || !server.credential.empty()) {
				return std::nullopt;
			}
		} else {
			if(server.username.empty() != server.credential.empty()) {
				return std::nullopt;
			}
			parsed.username = server.username;
			parsed.password = server.credential;
		}
		return parsed;
	} catch(...) {
		return std::nullopt;
	}
}

[[nodiscard]] bool final_state(RtcRoomState state) noexcept
{
	return state == RtcRoomState::peer_left ||
		state == RtcRoomState::closed || state == RtcRoomState::failed;
}

} // namespace

struct RtcRoom::Impl final {
	struct Core final {
		std::mutex mutex{};
		// libdatachannel is allowed to invoke callbacks synchronously from
		// send(). Keep signaling order serialized without holding the state
		// mutex across library code that can re-enter us.
		std::mutex signaling_send_mutex{};
		RtcRoomConfig config{};
		RtcRoomState state = RtcRoomState::connecting;
		std::optional<RtcRoomError> failure{};
		std::string room_code{};
		std::uint8_t player = 0U;
		std::uint32_t outbound_sequence = 0U;
		std::uint32_t inbound_sequence = 0U;
		bool welcomed = false;
		bool paired = false;
		bool gameplay_open = false;
		bool control_open = false;
		bool closing = false;
		std::size_t queued_bytes = 0U;
		std::deque<RtcRoomMessage> received{};
		std::shared_ptr<rtc::WebSocket> websocket{};
		std::shared_ptr<rtc::PeerConnection> peer{};
		std::shared_ptr<rtc::DataChannel> gameplay{};
		std::shared_ptr<rtc::DataChannel> control{};
	};

	std::shared_ptr<Core> core{};

	static void fail(
		const std::shared_ptr<Core> &core,
		RtcRoomErrorCode code
	) noexcept
	{
		std::scoped_lock lock(core->mutex);
		if(!core->closing && !final_state(core->state)) {
			core->failure = error(code);
			core->state = RtcRoomState::failed;
		}
	}

	static void update_ready(const std::shared_ptr<Core> &core) noexcept
	{
		std::scoped_lock lock(core->mutex);
		if(!core->closing && !final_state(core->state) &&
			core->gameplay_open && core->control_open) {
			core->state = RtcRoomState::ready;
		}
	}

	static void queue_message(
		const std::shared_ptr<Core> &core,
		RtcRoomChannel channel,
		rtc::binary bytes
	) noexcept
	{
		if(bytes.empty() || bytes.size() > maximum_data_channel_message_bytes) {
			fail(core, RtcRoomErrorCode::message_limit);
			return;
		}
		std::vector<std::byte> copied(bytes.size());
		std::transform(bytes.begin(), bytes.end(), copied.begin(),
			[](rtc::byte byte) { return static_cast<std::byte>(byte); });
		std::scoped_lock lock(core->mutex);
		if(core->closing || final_state(core->state)) {
			return;
		}
		if(core->received.size() >= maximum_received_messages ||
			core->queued_bytes + copied.size() > maximum_received_bytes) {
			core->failure = error(RtcRoomErrorCode::queue_limit);
			core->state = RtcRoomState::failed;
			return;
		}
		core->queued_bytes += copied.size();
		core->received.push_back({channel, std::move(copied)});
	}

	static void install_channel(
		const std::shared_ptr<Core> &core,
		std::shared_ptr<rtc::DataChannel> channel,
		RtcRoomChannel kind
	)
	{
		const std::weak_ptr<Core> weak = core;
		channel->onOpen([weak, kind]() {
			if(const auto state = weak.lock()) {
				{
					std::scoped_lock lock(state->mutex);
					if(kind == RtcRoomChannel::gameplay) {
						state->gameplay_open = true;
					} else {
						state->control_open = true;
					}
				}
				update_ready(state);
			}
		});
		channel->onClosed([weak]() {
			if(const auto state = weak.lock()) {
				std::scoped_lock lock(state->mutex);
				if(!state->closing && !final_state(state->state)) {
					state->state = RtcRoomState::peer_left;
				}
			}
		});
		channel->onError([weak](std::string) {
			if(const auto state = weak.lock()) {
				fail(state, RtcRoomErrorCode::data_channel);
			}
		});
		channel->onMessage(
			[weak, kind](rtc::binary bytes) {
				if(const auto state = weak.lock()) {
					queue_message(state, kind, std::move(bytes));
				}
			},
			[weak](std::string) {
				if(const auto state = weak.lock()) {
					fail(state, RtcRoomErrorCode::data_channel);
				}
			}
		);
	}

	static bool send_signal(
		const std::shared_ptr<Core> &core,
		app::NewClassicNetplayRoomSignalType type,
		std::string payload
	) noexcept
	{
		std::scoped_lock send_lock(core->signaling_send_mutex);
		std::shared_ptr<rtc::WebSocket> websocket{};
		std::string encoded{};
		std::uint32_t sequence = 0U;
		{
			std::scoped_lock state_lock(core->mutex);
			if(core->closing || final_state(core->state) || !core->websocket ||
				!core->websocket->isOpen()) {
				return false;
			}
			websocket = core->websocket;
			sequence = core->outbound_sequence;
			auto candidate = app::encode_new_classic_netplay_room_signal({
				.type = type,
				.sequence = sequence,
				.payload = std::move(payload),
			});
			if(!candidate) {
				return false;
			}
			encoded = std::move(*candidate);
		}
		try {
			if(!websocket->send(encoded)) {
				return false;
			}
		} catch(...) {
			return false;
		}
		{
			std::scoped_lock state_lock(core->mutex);
			if(core->outbound_sequence != sequence) {
				return false;
			}
			++core->outbound_sequence;
		}
		return true;
	}

	static void install_peer(
		const std::shared_ptr<Core> &core,
		bool offerer
	) noexcept
	{
		{
			std::scoped_lock lock(core->mutex);
			if(core->closing || final_state(core->state) || core->peer) {
				return;
			}
			core->state = RtcRoomState::negotiating;
		}
		try {
			rtc::Configuration configuration{};
			for(const auto &server : core->config.ice_servers) {
				const auto parsed = ice_server(server);
				if(!parsed) {
					fail(core, RtcRoomErrorCode::invalid_ice_server);
					return;
				}
				configuration.iceServers.push_back(*parsed);
			}
			configuration.iceTransportPolicy = core->config.relay_only ?
				rtc::TransportPolicy::Relay : rtc::TransportPolicy::All;
			configuration.maxMessageSize = maximum_data_channel_message_bytes;
			auto peer = std::make_shared<rtc::PeerConnection>(configuration);
			const std::weak_ptr<Core> weak = core;
			peer->onLocalDescription([weak](rtc::Description description) {
				if(const auto state = weak.lock()) {
					const auto type = description.type() ==
						rtc::Description::Type::Offer ?
						app::NewClassicNetplayRoomSignalType::offer :
						app::NewClassicNetplayRoomSignalType::answer;
					if((description.type() != rtc::Description::Type::Offer &&
						description.type() != rtc::Description::Type::Answer) ||
						!send_signal(state, type, std::string(description))) {
						fail(state, RtcRoomErrorCode::send_failed);
					}
				}
			});
			peer->onLocalCandidate([weak](rtc::Candidate candidate) {
				if(const auto state = weak.lock()) {
					const auto payload = candidate.mid() + "\n" +
						std::string(candidate);
					if(!send_signal(
						state, app::NewClassicNetplayRoomSignalType::ice,
						payload
					)) {
						fail(state, RtcRoomErrorCode::send_failed);
					}
				}
			});
			peer->onStateChange([weak](rtc::PeerConnection::State state) {
				if(const auto core_state = weak.lock()) {
					if(state == rtc::PeerConnection::State::Failed) {
						fail(core_state, RtcRoomErrorCode::peer_failed);
					} else if(state == rtc::PeerConnection::State::Closed ||
						state == rtc::PeerConnection::State::Disconnected) {
						std::scoped_lock lock(core_state->mutex);
						if(!core_state->closing &&
							!final_state(core_state->state)) {
							core_state->state = RtcRoomState::peer_left;
						}
					}
				}
			});
			peer->onDataChannel([weak](std::shared_ptr<rtc::DataChannel> channel) {
				if(const auto state = weak.lock()) {
					RtcRoomChannel kind{};
					if(channel->label() == "T3NP07-gameplay") {
						kind = RtcRoomChannel::gameplay;
					} else if(channel->label() == "T3NC01-control") {
						kind = RtcRoomChannel::control;
					} else {
						channel->close();
						fail(state, RtcRoomErrorCode::data_channel);
						return;
					}
					bool duplicate = false;
					{
						std::scoped_lock lock(state->mutex);
						auto &slot = kind == RtcRoomChannel::gameplay ?
							state->gameplay : state->control;
						if(slot) {
							state->failure = error(RtcRoomErrorCode::data_channel);
							state->state = RtcRoomState::failed;
							duplicate = true;
						} else {
							slot = channel;
						}
					}
					if(duplicate) {
						channel->close();
						return;
					}
					install_channel(state, std::move(channel), kind);
				}
			});
			{
				std::scoped_lock lock(core->mutex);
				core->peer = peer;
			}
			if(offerer) {
				rtc::DataChannelInit gameplay_init{};
				gameplay_init.reliability.unordered = true;
				gameplay_init.reliability.maxRetransmits = 0U;
				gameplay_init.protocol = "T3NP07";
				auto gameplay = peer->createDataChannel(
					"T3NP07-gameplay", gameplay_init
				);
				rtc::DataChannelInit control_init{};
				control_init.protocol = "T3NC01";
				auto control = peer->createDataChannel(
					"T3NC01-control", control_init
				);
				{
					std::scoped_lock lock(core->mutex);
					core->gameplay = gameplay;
					core->control = control;
				}
				install_channel(core, std::move(gameplay),
					RtcRoomChannel::gameplay);
				install_channel(core, std::move(control),
					RtcRoomChannel::control);
			}
		} catch(...) {
			fail(core, RtcRoomErrorCode::peer_create);
		}
	}

	static void handle_signal(
		const std::shared_ptr<Core> &core,
		const app::NewClassicNetplayRoomSignal &signal
	) noexcept
	{
		{
			std::scoped_lock lock(core->mutex);
			if(signal.sequence != core->inbound_sequence) {
				core->failure = error(RtcRoomErrorCode::signaling_order);
				core->state = RtcRoomState::failed;
				return;
			}
			++core->inbound_sequence;
		}
		try {
			if(signal.type == app::NewClassicNetplayRoomSignalType::offer) {
				{
					std::scoped_lock lock(core->mutex);
					if(core->player != 2U || core->peer) {
						core->failure = error(
							RtcRoomErrorCode::signaling_invalid
						);
						core->state = RtcRoomState::failed;
						return;
					}
				}
				install_peer(core, false);
				std::shared_ptr<rtc::PeerConnection> peer{};
				{
					std::scoped_lock lock(core->mutex);
					peer = core->peer;
				}
				if(!peer) {
					fail(core, RtcRoomErrorCode::peer_create);
					return;
				}
				peer->setRemoteDescription(rtc::Description(
					signal.payload, rtc::Description::Type::Offer
				));
			} else if(signal.type ==
				app::NewClassicNetplayRoomSignalType::answer) {
				std::shared_ptr<rtc::PeerConnection> peer{};
				{
					std::scoped_lock lock(core->mutex);
					if(core->player != 1U) {
						core->failure = error(
							RtcRoomErrorCode::signaling_invalid
						);
						core->state = RtcRoomState::failed;
						return;
					}
					peer = core->peer;
				}
				if(!peer) {
					fail(core, RtcRoomErrorCode::signaling_invalid);
					return;
				}
				peer->setRemoteDescription(rtc::Description(
					signal.payload, rtc::Description::Type::Answer
				));
			} else if(signal.type ==
				app::NewClassicNetplayRoomSignalType::ice) {
				const auto separator = signal.payload.find('\n');
				std::shared_ptr<rtc::PeerConnection> peer{};
				{
					std::scoped_lock lock(core->mutex);
					peer = core->peer;
				}
				if(separator == std::string::npos || separator == 0U ||
					separator + 1U == signal.payload.size() || !peer) {
					fail(core, RtcRoomErrorCode::signaling_invalid);
					return;
				}
				peer->addRemoteCandidate(rtc::Candidate(
					signal.payload.substr(separator + 1U),
					signal.payload.substr(0U, separator)
				));
			} else {
				fail(core, RtcRoomErrorCode::signaling_invalid);
			}
		} catch(...) {
			fail(core, RtcRoomErrorCode::signaling_invalid);
		}
	}

	static void handle_websocket_message(
		const std::shared_ptr<Core> &core,
		std::string wire
	) noexcept
	{
		const auto decoded = app::decode_new_classic_netplay_room_message(wire);
		if(!decoded) {
			fail(core, RtcRoomErrorCode::signaling_invalid);
			return;
		}
		if(const auto *welcome = std::get_if<
			app::NewClassicNetplayRoomWelcome>(&*decoded)) {
			std::scoped_lock lock(core->mutex);
			const auto expected_player = core->config.mode == RtcRoomMode::create ?
				1U : (core->config.mode == RtcRoomMode::join ? 2U : 0U);
			if(core->welcomed ||
				(expected_player != 0U && welcome->player != expected_player) ||
				core->config.ice_servers.size() + welcome->ice_servers.size() >
					maximum_ice_servers ||
				(core->config.mode == RtcRoomMode::join &&
				 welcome->code != core->config.room_code)) {
				core->failure = error(RtcRoomErrorCode::signaling_invalid);
				core->state = RtcRoomState::failed;
				return;
			}
			for(const auto &server : welcome->ice_servers) {
				core->config.ice_servers.push_back({
					.url = server.url,
					.username = server.username,
					.credential = server.credential,
				});
			}
			if(core->config.relay_only && !std::ranges::any_of(
				core->config.ice_servers, [](const RtcIceServer &server) {
					return (server.url.starts_with("turn:") ||
						server.url.starts_with("turns:")) &&
						!server.username.empty() && !server.credential.empty();
				}
			)) {
				core->failure = error(RtcRoomErrorCode::invalid_ice_server);
				core->state = RtcRoomState::failed;
				return;
			}
			core->welcomed = true;
			core->player = welcome->player;
			core->room_code = welcome->code;
			core->state = RtcRoomState::waiting_peer;
			return;
		}
		if(std::holds_alternative<app::NewClassicNetplayRoomPaired>(*decoded)) {
			bool offerer = false;
			{
				std::scoped_lock lock(core->mutex);
				if(!core->welcomed || core->paired) {
					core->failure = error(RtcRoomErrorCode::signaling_invalid);
					core->state = RtcRoomState::failed;
					return;
				}
				core->paired = true;
				core->state = RtcRoomState::negotiating;
				offerer = core->player == 1U;
			}
			if(offerer) {
				install_peer(core, true);
			}
			return;
		}
		if(const auto *left = std::get_if<
			app::NewClassicNetplayRoomPeerLeft>(&*decoded)) {
			std::scoped_lock lock(core->mutex);
			if(!core->welcomed || left->player == core->player) {
				core->failure = error(RtcRoomErrorCode::signaling_invalid);
				core->state = RtcRoomState::failed;
			} else if(core->state != RtcRoomState::ready &&
				!final_state(core->state)) {
				core->state = RtcRoomState::peer_left;
			}
			return;
		}
		handle_signal(core, std::get<app::NewClassicNetplayRoomSignal>(*decoded));
	}

	static std::expected<std::unique_ptr<Impl>, RtcRoomError> create(
		RtcRoomConfig config
	) noexcept
	{
		const auto url = rtc_room_websocket_url(
			config.origin, config.mode, config.room_code, config.rank
		);
		if(!url) {
			return std::unexpected(url.error());
		}
		const auto authentication = rtc_room_authentication_protocol(
			config.user_id, config.token
		);
		if(!authentication) {
			return std::unexpected(authentication.error());
		}
		if(config.ice_servers.size() > maximum_ice_servers) {
			return std::unexpected(error(RtcRoomErrorCode::invalid_ice_server));
		}
		for(const auto &server : config.ice_servers) {
			if(!ice_server(server)) {
				return std::unexpected(error(
					RtcRoomErrorCode::invalid_ice_server
				));
			}
		}
		try {
			auto core = std::make_shared<Core>();
			core->config = std::move(config);
			rtc::WebSocket::Configuration websocket_config{};
			websocket_config.protocols = {
				std::string(app::new_classic_netplay_room_protocol),
				*authentication,
			};
			websocket_config.connectionTimeout = websocket_timeout;
			websocket_config.pingInterval = websocket_ping_interval;
			websocket_config.maxOutstandingPings = 2;
			websocket_config.maxMessageSize =
				app::new_classic_netplay_room_signal_max_bytes;
			auto websocket = std::make_shared<rtc::WebSocket>(websocket_config);
			const std::weak_ptr<Core> weak = core;
			websocket->onError([weak](std::string) {
				if(const auto state = weak.lock()) {
					std::scoped_lock lock(state->mutex);
					if(state->state != RtcRoomState::ready && !state->closing &&
						!final_state(state->state)) {
						state->failure = error(
							RtcRoomErrorCode::websocket_failed
						);
						state->state = RtcRoomState::failed;
					}
				}
			});
			websocket->onClosed([weak]() {
				if(const auto state = weak.lock()) {
					std::scoped_lock lock(state->mutex);
					if(state->state != RtcRoomState::ready && !state->closing &&
						!final_state(state->state)) {
						state->state = RtcRoomState::peer_left;
					}
				}
			});
			websocket->onMessage(
				[weak](rtc::binary) {
					if(const auto state = weak.lock()) {
						fail(state, RtcRoomErrorCode::websocket_protocol);
					}
				},
				[weak](std::string message) {
					if(const auto state = weak.lock()) {
						handle_websocket_message(state, std::move(message));
					}
				}
			);
			core->websocket = websocket;
			websocket->open(*url);
			auto result = std::make_unique<Impl>();
			result->core = std::move(core);
			return result;
		} catch(...) {
			return std::unexpected(error(RtcRoomErrorCode::websocket_create));
		}
	}

	void close() noexcept
	{
		std::shared_ptr<rtc::DataChannel> gameplay{};
		std::shared_ptr<rtc::DataChannel> control{};
		std::shared_ptr<rtc::PeerConnection> peer{};
		std::shared_ptr<rtc::WebSocket> websocket{};
		{
			std::scoped_lock lock(core->mutex);
			if(core->closing) {
				return;
			}
			core->closing = true;
			core->state = RtcRoomState::closed;
			gameplay = std::move(core->gameplay);
			control = std::move(core->control);
			peer = std::move(core->peer);
			websocket = std::move(core->websocket);
		}
		try {
			if(gameplay) {
				gameplay->resetCallbacks();
				gameplay->close();
			}
			if(control) {
				control->resetCallbacks();
				control->close();
			}
			if(peer) {
				peer->resetCallbacks();
				peer->close();
			}
			if(websocket) {
				websocket->resetCallbacks();
				websocket->close();
			}
		} catch(...) {
			// Destruction is best-effort and cannot re-enter deterministic state.
		}
	}
};

std::expected<std::string, RtcRoomError> rtc_room_websocket_url(
	std::string_view origin,
	RtcRoomMode mode,
	std::string_view room_code,
	std::uint8_t rank
)
{
	const auto parsed = parse_origin(origin);
	if(!parsed) {
		return std::unexpected(error(RtcRoomErrorCode::invalid_origin));
	}
	if(mode == RtcRoomMode::matchmaking &&
		(!room_code.empty() || rank > 3U)) {
		return std::unexpected(error(RtcRoomErrorCode::invalid_room_code));
	}
	if(mode == RtcRoomMode::join &&
		!app::valid_new_classic_netplay_room_code(room_code)) {
		return std::unexpected(error(RtcRoomErrorCode::invalid_room_code));
	}
	if(mode == RtcRoomMode::create && !room_code.empty()) {
		return std::unexpected(error(RtcRoomErrorCode::invalid_room_code));
	}
	if(mode == RtcRoomMode::matchmaking) {
		return std::string(parsed->secure ? "wss://" : "ws://") +
			parsed->authority + "/api/th03/netplay/matchmaking?rank=" +
			std::to_string(static_cast<unsigned>(rank));
	}
	return std::string(parsed->secure ? "wss://" : "ws://") +
		parsed->authority + "/api/th03/netplay/rooms/" +
		(mode == RtcRoomMode::create ? std::string("new") :
			std::string(room_code));
}

std::expected<std::string, RtcRoomError> rtc_room_authentication_protocol(
	std::string_view user_id,
	std::string_view token
)
{
	if(!uuid_shape(user_id) || !token_shape(token)) {
		return std::unexpected(error(RtcRoomErrorCode::invalid_credentials));
	}
	return "t3auth." + std::string(user_id) + "." + std::string(token);
}

RtcRoom::RtcRoom(std::unique_ptr<Impl> impl) noexcept : impl_(std::move(impl)) {}

RtcRoom::~RtcRoom()
{
	close();
}

std::expected<std::unique_ptr<RtcRoom>, RtcRoomError> RtcRoom::open(
	RtcRoomConfig config
) noexcept
{
	auto created = Impl::create(std::move(config));
	if(!created) {
		return std::unexpected(created.error());
	}
	return std::unique_ptr<RtcRoom>(new RtcRoom(std::move(*created)));
}

RtcRoomSnapshot RtcRoom::snapshot() const noexcept
{
	if(!impl_ || !impl_->core) {
		return {
			.state = RtcRoomState::closed,
			.error = error(RtcRoomErrorCode::closed),
		};
	}
	RtcRoomSnapshot result{};
	std::shared_ptr<rtc::PeerConnection> peer{};
	{
		std::scoped_lock lock(impl_->core->mutex);
		result.state = impl_->core->state;
		result.error = impl_->core->failure;
		result.room_code = impl_->core->room_code;
		result.player = impl_->core->player;
		peer = impl_->core->peer;
	}
	if(peer) {
		try {
			result.round_trip_time = peer->rtt();
		} catch(...) {
			// Metrics are advisory and never turn a healthy session into failure.
		}
	}
	return result;
}

std::expected<void, RtcRoomError> RtcRoom::send(
	RtcRoomChannel channel,
	std::span<const std::byte> bytes
) noexcept
{
	if(!impl_ || !impl_->core) {
		return std::unexpected(error(RtcRoomErrorCode::closed));
	}
	if(bytes.empty() || bytes.size() > maximum_data_channel_message_bytes) {
		return std::unexpected(error(RtcRoomErrorCode::message_limit));
	}
	std::shared_ptr<rtc::DataChannel> data_channel{};
	{
		std::scoped_lock lock(impl_->core->mutex);
		if(impl_->core->state != RtcRoomState::ready) {
			return std::unexpected(impl_->core->failure.value_or(
				error(RtcRoomErrorCode::closed)
			));
		}
		data_channel = channel == RtcRoomChannel::gameplay ?
			impl_->core->gameplay : impl_->core->control;
	}
	if(!data_channel) {
		return std::unexpected(error(RtcRoomErrorCode::data_channel));
	}
	try {
		const auto *data = reinterpret_cast<const rtc::byte *>(bytes.data());
		if(!data_channel->send(data, bytes.size())) {
			return std::unexpected(error(RtcRoomErrorCode::send_failed));
		}
	} catch(...) {
		return std::unexpected(error(RtcRoomErrorCode::send_failed));
	}
	return {};
}

std::expected<std::optional<RtcRoomMessage>, RtcRoomError> RtcRoom::receive()
	noexcept
{
	if(!impl_ || !impl_->core) {
		return std::unexpected(error(RtcRoomErrorCode::closed));
	}
	std::scoped_lock lock(impl_->core->mutex);
	if(impl_->core->received.empty()) {
		if(impl_->core->failure) {
			return std::unexpected(*impl_->core->failure);
		}
		if(final_state(impl_->core->state)) {
			return std::unexpected(error(RtcRoomErrorCode::closed));
		}
		return std::optional<RtcRoomMessage>{};
	}
	auto result = std::move(impl_->core->received.front());
	impl_->core->received.pop_front();
	impl_->core->queued_bytes -= result.bytes.size();
	return std::optional<RtcRoomMessage>{std::move(result)};
}

void RtcRoom::close() noexcept
{
	if(impl_) {
		impl_->close();
	}
}

} // namespace th03::platform
