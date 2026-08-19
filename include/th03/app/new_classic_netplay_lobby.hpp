#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_LOBBY_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_LOBBY_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

#include "th03/app/new_classic_netplay_match_contract.hpp"

namespace th03::app {

inline constexpr std::uint8_t new_classic_netplay_lobby_version = 3U;
inline constexpr std::size_t new_classic_netplay_lobby_bytes = 64U;
inline constexpr std::uint64_t new_classic_netplay_lobby_timeout_ms = 30'000U;

enum class NewClassicNetplayLobbyDatagramType : std::uint8_t {
	join_request = 1U,
	host_offer = 2U,
	join_ready = 3U,
	host_accept = 4U,
};

struct NewClassicNetplayLobbyDatagram final {
	NewClassicNetplayLobbyDatagramType type =
		NewClassicNetplayLobbyDatagramType::join_request;
	std::uint64_t session_id = 0U;
	std::optional<NewClassicNetplayHostLock> host{};
	std::optional<NewClassicNetplayJoinLock> join{};
	std::uint64_t contract_hash = 0U;
};

enum class NewClassicNetplayLobbyDatagramErrorCode : std::uint8_t {
	invalid_size,
	invalid_magic,
	unsupported_version,
	invalid_type,
	invalid_session,
	invalid_lock_shape,
	invalid_contract,
	nonzero_reserved,
};

struct NewClassicNetplayLobbyDatagramError final {
	NewClassicNetplayLobbyDatagramErrorCode code =
		NewClassicNetplayLobbyDatagramErrorCode::invalid_size;
	std::optional<NewClassicNetplayMatchContractError> contract{};
};

[[nodiscard]] std::expected<
	std::array<std::byte, new_classic_netplay_lobby_bytes>,
	NewClassicNetplayLobbyDatagramError
> encode_new_classic_netplay_lobby_datagram(
	const NewClassicNetplayLobbyDatagram &datagram
) noexcept;

[[nodiscard]] std::expected<
	NewClassicNetplayLobbyDatagram,
	NewClassicNetplayLobbyDatagramError
> decode_new_classic_netplay_lobby_datagram(
	std::span<const std::byte> wire
) noexcept;

enum class NewClassicNetplayLobbyRole : std::uint8_t {
	host,
	join,
};

enum class NewClassicNetplayLobbyStatus : std::uint8_t {
	waiting,
	ready,
};

// A host has no deadline before selecting its first valid peer. A joiner, or
// a host completing an already-started exchange, retains a bounded failure
// path. The caller owns the monotonic elapsed-time anchor.
[[nodiscard]] bool new_classic_netplay_lobby_wait_expired(
	NewClassicNetplayLobbyRole role,
	bool peer_selected,
	std::uint64_t elapsed_ms
) noexcept;

enum class NewClassicNetplayLobbyErrorCode : std::uint8_t {
	invalid_host_lock,
	invalid_join_lock,
	invalid_session,
	wrong_direction,
	lock_changed,
	contract_mismatch,
};

struct NewClassicNetplayLobbyError final {
	NewClassicNetplayLobbyErrorCode code =
		NewClassicNetplayLobbyErrorCode::invalid_host_lock;
};

class NewClassicNetplayLobby final {
public:
	[[nodiscard]] static std::expected<
		NewClassicNetplayLobby,
		NewClassicNetplayLobbyError
	> create_host(NewClassicNetplayHostLock lock,
		std::uint64_t session_id) noexcept;
	[[nodiscard]] static std::expected<
		NewClassicNetplayLobby,
		NewClassicNetplayLobbyError
	> create_join(NewClassicNetplayJoinLock lock) noexcept;

	[[nodiscard]] NewClassicNetplayLobbyRole role() const noexcept;
	[[nodiscard]] NewClassicNetplayLobbyStatus status() const noexcept;
	[[nodiscard]] std::optional<NewClassicNetplayLobbyDatagram>
	outbound() const noexcept;
	[[nodiscard]] std::expected<void, NewClassicNetplayLobbyError> receive(
		const NewClassicNetplayLobbyDatagram &datagram
	) noexcept;
	[[nodiscard]] std::optional<NewClassicNetplayMatchContract>
	contract() const noexcept;
	[[nodiscard]] std::uint64_t session_id() const noexcept;

private:
	NewClassicNetplayLobbyRole role_ = NewClassicNetplayLobbyRole::host;
	NewClassicNetplayLobbyStatus status_ =
		NewClassicNetplayLobbyStatus::waiting;
	std::uint64_t session_id_ = 0U;
	std::optional<NewClassicNetplayHostLock> host_{};
	std::optional<NewClassicNetplayJoinLock> join_{};
	std::optional<NewClassicNetplayMatchContract> contract_{};
};

} // namespace th03::app

#endif
