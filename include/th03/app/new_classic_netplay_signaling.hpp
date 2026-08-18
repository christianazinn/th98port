#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_SIGNALING_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_SIGNALING_HPP

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace th03::app {

inline constexpr std::string_view new_classic_netplay_room_protocol =
	"T3ROOM2";
inline constexpr std::size_t new_classic_netplay_room_signal_max_bytes =
	16U * 1024U;
inline constexpr std::size_t new_classic_netplay_room_payload_max_bytes =
	12U * 1024U;

enum class NewClassicNetplayRoomSignalType : std::uint8_t {
	offer,
	answer,
	ice,
	control,
};

struct NewClassicNetplayRoomSignal final {
	NewClassicNetplayRoomSignalType type =
		NewClassicNetplayRoomSignalType::offer;
	std::uint32_t sequence = 0U;
	std::string payload{};

	friend bool operator==(
		const NewClassicNetplayRoomSignal &,
		const NewClassicNetplayRoomSignal &
	) = default;
};

struct NewClassicNetplayIceServer final {
	std::string url{};
	std::string username{};
	std::string credential{};

	friend bool operator==(
		const NewClassicNetplayIceServer &,
		const NewClassicNetplayIceServer &
	) = default;
};

struct NewClassicNetplayRoomWelcome final {
	std::string code{};
	std::string room_id{};
	std::uint8_t player = 0U;
	std::uint64_t expires_at_unix_ms = 0U;
	std::vector<NewClassicNetplayIceServer> ice_servers{};

	friend bool operator==(
		const NewClassicNetplayRoomWelcome &,
		const NewClassicNetplayRoomWelcome &
	) = default;
};

struct NewClassicNetplayRoomPaired final {
	friend bool operator==(
		const NewClassicNetplayRoomPaired &,
		const NewClassicNetplayRoomPaired &
	) = default;
};

struct NewClassicNetplayRoomPeerLeft final {
	std::uint8_t player = 0U;

	friend bool operator==(
		const NewClassicNetplayRoomPeerLeft &,
		const NewClassicNetplayRoomPeerLeft &
	) = default;
};

using NewClassicNetplayRoomMessage = std::variant<
	NewClassicNetplayRoomSignal,
	NewClassicNetplayRoomWelcome,
	NewClassicNetplayRoomPaired,
	NewClassicNetplayRoomPeerLeft
>;

enum class NewClassicNetplayRoomMessageErrorCode : std::uint8_t {
	invalid_size,
	invalid_json,
	invalid_shape,
	unsupported_protocol,
	invalid_type,
	invalid_sequence,
	invalid_payload,
	invalid_room,
	invalid_player,
	invalid_expiration,
	invalid_ice_server,
};

struct NewClassicNetplayRoomMessageError final {
	NewClassicNetplayRoomMessageErrorCode code =
		NewClassicNetplayRoomMessageErrorCode::invalid_json;
};

[[nodiscard]] bool valid_new_classic_netplay_room_code(
	std::string_view code
) noexcept;

[[nodiscard]] std::expected<
	std::string,
	NewClassicNetplayRoomMessageError
> encode_new_classic_netplay_room_signal(
	const NewClassicNetplayRoomSignal &signal
);

[[nodiscard]] std::expected<
	NewClassicNetplayRoomMessage,
	NewClassicNetplayRoomMessageError
> decode_new_classic_netplay_room_message(std::string_view wire);

} // namespace th03::app

#endif
