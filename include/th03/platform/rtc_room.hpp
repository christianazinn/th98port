#ifndef TH03_PLATFORM_RTC_ROOM_HPP
#define TH03_PLATFORM_RTC_ROOM_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace th03::platform {

enum class RtcRoomMode : std::uint8_t {
	create,
	join,
};

enum class RtcRoomState : std::uint8_t {
	connecting,
	waiting_peer,
	negotiating,
	ready,
	peer_left,
	closed,
	failed,
};

enum class RtcRoomChannel : std::uint8_t {
	gameplay,
	control,
};

enum class RtcRoomErrorCode : std::uint8_t {
	invalid_origin,
	invalid_credentials,
	invalid_room_code,
	invalid_ice_server,
	websocket_create,
	websocket_failed,
	websocket_protocol,
	signaling_invalid,
	signaling_order,
	peer_create,
	peer_failed,
	data_channel,
	message_limit,
	queue_limit,
	send_failed,
	closed,
};

struct RtcRoomError final {
	RtcRoomErrorCode code = RtcRoomErrorCode::websocket_failed;
};

struct RtcRoomConfig final {
	RtcRoomMode mode = RtcRoomMode::create;
	std::string origin{};
	std::string user_id{};
	std::string token{};
	std::string room_code{};
	std::vector<std::string> ice_servers{};
	bool relay_only = false;
};

struct RtcRoomSnapshot final {
	RtcRoomState state = RtcRoomState::connecting;
	std::optional<RtcRoomError> error{};
	std::string room_code{};
	std::uint8_t player = 0U;
	std::optional<std::chrono::milliseconds> round_trip_time{};
};

struct RtcRoomMessage final {
	RtcRoomChannel channel = RtcRoomChannel::control;
	std::vector<std::byte> bytes{};
};

// Converts the fixed service origin into the exact signaling URL. Production
// permits HTTPS only; HTTP is accepted solely for loopback Wrangler tests.
[[nodiscard]] std::expected<std::string, RtcRoomError>
rtc_room_websocket_url(
	std::string_view origin,
	RtcRoomMode mode,
	std::string_view room_code
);

// The token travels in a WebSocket subprotocol because libdatachannel's
// WebSocket client deliberately exposes no arbitrary-header surface. The
// Worker selects only T3ROOM1 and strips the credential before entering the
// Durable Object.
[[nodiscard]] std::expected<std::string, RtcRoomError>
rtc_room_authentication_protocol(
	std::string_view user_id,
	std::string_view token
);

class RtcRoom final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<RtcRoom>,
		RtcRoomError
	> open(RtcRoomConfig config) noexcept;

	~RtcRoom();
	RtcRoom(const RtcRoom &) = delete;
	RtcRoom &operator=(const RtcRoom &) = delete;
	RtcRoom(RtcRoom &&) = delete;
	RtcRoom &operator=(RtcRoom &&) = delete;

	[[nodiscard]] RtcRoomSnapshot snapshot() const noexcept;
	[[nodiscard]] std::expected<void, RtcRoomError> send(
		RtcRoomChannel channel,
		std::span<const std::byte> bytes
	) noexcept;
	[[nodiscard]] std::expected<
		std::optional<RtcRoomMessage>,
		RtcRoomError
	> receive() noexcept;
	void close() noexcept;

private:
	struct Impl;
	explicit RtcRoom(std::unique_ptr<Impl> impl) noexcept;
	std::unique_ptr<Impl> impl_;
};

} // namespace th03::platform

#endif
