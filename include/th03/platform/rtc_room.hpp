#ifndef TH03_PLATFORM_RTC_ROOM_HPP
#define TH03_PLATFORM_RTC_ROOM_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace th03::platform {

enum class RtcRoomMode : std::uint8_t {
	matchmaking,
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
	tls_ca_unavailable,
	tls_failed,
	websocket_timeout,
};

struct RtcRoomError final {
	RtcRoomErrorCode code = RtcRoomErrorCode::websocket_failed;
};

struct RtcIceServer final {
	std::string url{};
	std::string username{};
	std::string credential{};
};

struct RtcOnlineCredentials final {
	std::string user_id{};
	std::string token{};

	friend bool operator==(
		const RtcOnlineCredentials &,
		const RtcOnlineCredentials &
	) = default;
};

enum class RtcOnlineIdentityErrorCode : std::uint8_t {
	invalid_origin,
	invalid_path,
	status,
	open,
	read,
	format,
	write,
	flush,
	publish,
	readback,
	tls_ca_unavailable,
	websocket_create,
	websocket_failed,
	websocket_timeout,
};

struct RtcOnlineIdentityError final {
	RtcOnlineIdentityErrorCode code =
		RtcOnlineIdentityErrorCode::invalid_origin;
	std::error_code system{};
};

struct RtcRoomConfig final {
	RtcRoomMode mode = RtcRoomMode::create;
	std::string origin{};
	std::string user_id{};
	std::string token{};
	std::string room_code{};
	std::uint8_t rank = 1U;
	std::vector<RtcIceServer> ice_servers{};
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
	std::string_view room_code,
	std::uint8_t rank = 0U
);

// The token travels in a WebSocket subprotocol because libdatachannel's
// WebSocket client deliberately exposes no arbitrary-header surface. The
// Worker selects only T3ROOM2 and strips the account credential before entering
// the Durable Object. Short-lived TURN credentials arrive inside the bounded
// authenticated welcome and never enter deterministic application state.
[[nodiscard]] std::expected<std::string, RtcRoomError>
rtc_room_authentication_protocol(
	std::string_view user_id,
	std::string_view token
);

// Loads one owner-only POSIX credential file or enrolls an opaque account over
// the RTC dependency's authenticated WebSocket stack. Existing malformed files
// fail closed and are never silently replaced. Windows uses its DPAPI owner.
[[nodiscard]] std::expected<RtcOnlineCredentials, RtcOnlineIdentityError>
load_or_enroll_rtc_online_credentials(
	const std::filesystem::path &path,
	std::string_view origin
) noexcept;

// Stable diagnostic vocabulary for product logs. These names are not part of
// the deterministic netplay wire or replay format.
[[nodiscard]] const char *rtc_room_error_code_name(
	RtcRoomErrorCode code
) noexcept;

// libdatachannel reports `false` when SCTP accepted a message into its bounded
// send queue rather than writing it immediately. That is successful transport
// ownership while the channel remains open, not a packet failure.
[[nodiscard]] constexpr bool rtc_room_data_channel_send_accepted(
	bool sent_immediately,
	bool channel_open_after_send
) noexcept
{
	return sent_immediately || channel_open_after_send;
}

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
