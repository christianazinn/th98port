#include "th03/app/new_classic_netplay_lobby.hpp"

#include <algorithm>

namespace th03::app {
namespace {

constexpr std::array magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'N'}, std::byte{'L'}
};

[[nodiscard]] NewClassicNetplayLobbyDatagramError datagram_error(
	NewClassicNetplayLobbyDatagramErrorCode code
) noexcept
{
	return NewClassicNetplayLobbyDatagramError{.code = code};
}

[[nodiscard]] NewClassicNetplayLobbyError lobby_error(
	NewClassicNetplayLobbyErrorCode code
) noexcept
{
	return NewClassicNetplayLobbyError{.code = code};
}

void put_u32(std::span<std::byte> output, std::size_t offset,
	std::uint32_t value) noexcept
{
	for(std::size_t index = 0U; index < 4U; ++index) {
		output[offset + index] = std::byte{
			static_cast<std::uint8_t>(value >> (index * 8U))
		};
	}
}

void put_u64(std::span<std::byte> output, std::size_t offset,
	std::uint64_t value) noexcept
{
	for(std::size_t index = 0U; index < 8U; ++index) {
		output[offset + index] = std::byte{
			static_cast<std::uint8_t>(value >> (index * 8U))
		};
	}
}

[[nodiscard]] std::uint32_t get_u32(
	std::span<const std::byte> input,
	std::size_t offset
) noexcept
{
	std::uint32_t value = 0U;
	for(std::size_t index = 0U; index < 4U; ++index) {
		value |= static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(input[offset + index])
		) << (index * 8U);
	}
	return value;
}

[[nodiscard]] std::uint64_t get_u64(
	std::span<const std::byte> input,
	std::size_t offset
) noexcept
{
	std::uint64_t value = 0U;
	for(std::size_t index = 0U; index < 8U; ++index) {
		value |= static_cast<std::uint64_t>(
			std::to_integer<std::uint8_t>(input[offset + index])
		) << (index * 8U);
	}
	return value;
}

[[nodiscard]] bool valid_type(
	NewClassicNetplayLobbyDatagramType type
) noexcept
{
	return type == NewClassicNetplayLobbyDatagramType::join_request ||
		type == NewClassicNetplayLobbyDatagramType::host_offer ||
		type == NewClassicNetplayLobbyDatagramType::join_ready ||
		type == NewClassicNetplayLobbyDatagramType::host_accept;
}

[[nodiscard]] std::expected<
	NewClassicNetplayMatchContract,
	NewClassicNetplayLobbyDatagramError
> full_contract(const NewClassicNetplayLobbyDatagram &datagram) noexcept
{
	if(!datagram.host || !datagram.join) {
		return std::unexpected(datagram_error(
			NewClassicNetplayLobbyDatagramErrorCode::invalid_lock_shape
		));
	}
	const auto contract = create_new_classic_netplay_match_contract(
		*datagram.host, *datagram.join
	);
	if(!contract) {
		return std::unexpected(NewClassicNetplayLobbyDatagramError{
			.code = NewClassicNetplayLobbyDatagramErrorCode::invalid_contract,
			.contract = contract.error(),
		});
	}
	const auto hash = new_classic_netplay_match_contract_hash(*contract);
	if(!hash || *hash != datagram.contract_hash) {
		return std::unexpected(NewClassicNetplayLobbyDatagramError{
			.code = NewClassicNetplayLobbyDatagramErrorCode::invalid_contract,
			.contract = hash ?
				std::optional<NewClassicNetplayMatchContractError>{} :
				hash.error(),
		});
	}
	return *contract;
}

[[nodiscard]] std::expected<void, NewClassicNetplayLobbyDatagramError>
validate(const NewClassicNetplayLobbyDatagram &datagram) noexcept
{
	if(!valid_type(datagram.type)) {
		return std::unexpected(datagram_error(
			NewClassicNetplayLobbyDatagramErrorCode::invalid_type
		));
	}
	if(datagram.type == NewClassicNetplayLobbyDatagramType::join_request) {
		if(datagram.session_id != 0U || datagram.host || !datagram.join ||
			datagram.contract_hash != 0U) {
			return std::unexpected(datagram_error(
				NewClassicNetplayLobbyDatagramErrorCode::invalid_lock_shape
			));
		}
		const auto probe = create_new_classic_netplay_match_contract(
			NewClassicNetplayHostLock{}, *datagram.join
		);
		if(!probe) {
			return std::unexpected(NewClassicNetplayLobbyDatagramError{
				.code = NewClassicNetplayLobbyDatagramErrorCode::invalid_contract,
				.contract = probe.error(),
			});
		}
		return {};
	}
	if(datagram.session_id == 0U) {
		return std::unexpected(datagram_error(
			NewClassicNetplayLobbyDatagramErrorCode::invalid_session
		));
	}
	if(const auto contract = full_contract(datagram); !contract) {
		return std::unexpected(contract.error());
	}
	return {};
}

[[nodiscard]] bool equal_host_lock(
	const NewClassicNetplayHostLock &left,
	const NewClassicNetplayHostLock &right
) noexcept
{
	return left.player_one == right.player_one && left.rank == right.rank &&
		left.seed == right.seed &&
		left.cadence_ticks == right.cadence_ticks &&
		left.cadence_seconds == right.cadence_seconds;
}

[[nodiscard]] bool equal_join_lock(
	const NewClassicNetplayJoinLock &left,
	const NewClassicNetplayJoinLock &right
) noexcept
{
	return left.player_two == right.player_two;
}

[[nodiscard]] std::optional<NewClassicNetplayMatchContract> derive(
	const std::optional<NewClassicNetplayHostLock> &host,
	const std::optional<NewClassicNetplayJoinLock> &join
) noexcept
{
	if(!host || !join) {
		return std::nullopt;
	}
	const auto contract = create_new_classic_netplay_match_contract(
		*host, *join
	);
	return contract ? std::optional{*contract} : std::nullopt;
}

[[nodiscard]] std::uint64_t contract_hash(
	const NewClassicNetplayMatchContract &contract
) noexcept
{
	const auto hash = new_classic_netplay_match_contract_hash(contract);
	return hash.value_or(0U);
}

} // namespace

bool new_classic_netplay_lobby_wait_expired(
	NewClassicNetplayLobbyRole role,
	bool peer_selected,
	std::uint64_t elapsed_ms
) noexcept
{
	return (role == NewClassicNetplayLobbyRole::join || peer_selected) &&
		elapsed_ms > new_classic_netplay_lobby_timeout_ms;
}

std::expected<
	std::array<std::byte, new_classic_netplay_lobby_bytes>,
	NewClassicNetplayLobbyDatagramError
> encode_new_classic_netplay_lobby_datagram(
	const NewClassicNetplayLobbyDatagram &datagram
) noexcept
{
	if(const auto valid = validate(datagram); !valid) {
		return std::unexpected(valid.error());
	}
	std::array<std::byte, new_classic_netplay_lobby_bytes> result{};
	std::ranges::copy(magic, result.begin());
	result[4U] = std::byte{new_classic_netplay_lobby_version};
	result[5U] = std::byte{static_cast<std::uint8_t>(datagram.type)};
	put_u64(result, 8U, datagram.session_id);
	if(datagram.host) {
		result[16U] = std::byte{datagram.host->player_one.character};
		result[17U] = std::byte{datagram.host->player_one.palette};
		result[18U] = std::byte{datagram.host->rank};
		put_u32(result, 20U, datagram.host->seed);
		put_u32(result, 24U, datagram.host->cadence_ticks);
		put_u32(result, 28U, datagram.host->cadence_seconds);
	}
	if(datagram.join) {
		result[32U] = std::byte{datagram.join->player_two.character};
		result[33U] = std::byte{datagram.join->player_two.palette};
	}
	put_u64(result, 40U, datagram.contract_hash);
	return result;
}

std::expected<NewClassicNetplayLobbyDatagram,
	NewClassicNetplayLobbyDatagramError>
decode_new_classic_netplay_lobby_datagram(
	std::span<const std::byte> wire
) noexcept
{
	if(wire.size() != new_classic_netplay_lobby_bytes) {
		return std::unexpected(datagram_error(
			NewClassicNetplayLobbyDatagramErrorCode::invalid_size
		));
	}
	if(!std::ranges::equal(magic, wire.first(magic.size()))) {
		return std::unexpected(datagram_error(
			NewClassicNetplayLobbyDatagramErrorCode::invalid_magic
		));
	}
	if(std::to_integer<std::uint8_t>(wire[4U]) !=
		new_classic_netplay_lobby_version) {
		return std::unexpected(datagram_error(
			NewClassicNetplayLobbyDatagramErrorCode::unsupported_version
		));
	}
	const auto type = static_cast<NewClassicNetplayLobbyDatagramType>(
		std::to_integer<std::uint8_t>(wire[5U])
	);
	if(wire[6U] != std::byte{0U} || wire[7U] != std::byte{0U} ||
		wire[19U] != std::byte{0U} ||
		std::ranges::any_of(wire.subspan(34U, 6U), [](std::byte value) {
			return value != std::byte{0U};
		}) || std::ranges::any_of(wire.subspan(48U), [](std::byte value) {
			return value != std::byte{0U};
		})) {
		return std::unexpected(datagram_error(
			NewClassicNetplayLobbyDatagramErrorCode::nonzero_reserved
		));
	}
	NewClassicNetplayLobbyDatagram result{
		.type = type,
		.session_id = get_u64(wire, 8U),
		.contract_hash = get_u64(wire, 40U),
	};
	const bool host_present = wire[16U] != std::byte{0U} ||
		wire[17U] != std::byte{0U} || wire[18U] != std::byte{0U} ||
		get_u32(wire, 20U) != 0U || get_u32(wire, 24U) != 0U ||
		get_u32(wire, 28U) != 0U;
	if(host_present) {
		result.host = NewClassicNetplayHostLock{
			.player_one = {
				.character = std::to_integer<std::uint8_t>(wire[16U]),
				.palette = std::to_integer<std::uint8_t>(wire[17U]),
			},
			.rank = std::to_integer<std::uint8_t>(wire[18U]),
			.seed = get_u32(wire, 20U),
			.cadence_ticks = get_u32(wire, 24U),
			.cadence_seconds = get_u32(wire, 28U),
		};
	}
	const bool join_present = wire[32U] != std::byte{0U} ||
		wire[33U] != std::byte{0U};
	if(join_present) {
		result.join = NewClassicNetplayJoinLock{
			.player_two = {
				.character = std::to_integer<std::uint8_t>(wire[32U]),
				.palette = std::to_integer<std::uint8_t>(wire[33U]),
			},
		};
	}
	if(const auto valid = validate(result); !valid) {
		return std::unexpected(valid.error());
	}
	return result;
}

std::expected<NewClassicNetplayLobby, NewClassicNetplayLobbyError>
NewClassicNetplayLobby::create_host(
	NewClassicNetplayHostLock lock,
	std::uint64_t session_id
) noexcept
{
	if(session_id == 0U) {
		return std::unexpected(lobby_error(
			NewClassicNetplayLobbyErrorCode::invalid_session
		));
	}
	if(!create_new_classic_netplay_match_contract(
		lock, NewClassicNetplayJoinLock{}
	)) {
		return std::unexpected(lobby_error(
			NewClassicNetplayLobbyErrorCode::invalid_host_lock
		));
	}
	NewClassicNetplayLobby result{};
	result.role_ = NewClassicNetplayLobbyRole::host;
	result.session_id_ = session_id;
	result.host_ = lock;
	return result;
}

std::expected<NewClassicNetplayLobby, NewClassicNetplayLobbyError>
NewClassicNetplayLobby::create_join(NewClassicNetplayJoinLock lock) noexcept
{
	if(!create_new_classic_netplay_match_contract(
		NewClassicNetplayHostLock{}, lock
	)) {
		return std::unexpected(lobby_error(
			NewClassicNetplayLobbyErrorCode::invalid_join_lock
		));
	}
	NewClassicNetplayLobby result{};
	result.role_ = NewClassicNetplayLobbyRole::join;
	result.join_ = lock;
	return result;
}

NewClassicNetplayLobbyRole NewClassicNetplayLobby::role() const noexcept
{
	return role_;
}

NewClassicNetplayLobbyStatus NewClassicNetplayLobby::status() const noexcept
{
	return status_;
}

std::optional<NewClassicNetplayLobbyDatagram>
NewClassicNetplayLobby::outbound() const noexcept
{
	if(role_ == NewClassicNetplayLobbyRole::join && !contract_) {
		return NewClassicNetplayLobbyDatagram{
			.type = NewClassicNetplayLobbyDatagramType::join_request,
			.join = join_,
		};
	}
	if(!contract_) {
		return std::nullopt;
	}
	const auto hash = contract_hash(*contract_);
	if(hash == 0U) {
		return std::nullopt;
	}
	if(role_ == NewClassicNetplayLobbyRole::host) {
		return NewClassicNetplayLobbyDatagram{
			.type = status_ == NewClassicNetplayLobbyStatus::ready ?
				NewClassicNetplayLobbyDatagramType::host_accept :
				NewClassicNetplayLobbyDatagramType::host_offer,
			.session_id = session_id_,
			.host = host_,
			.join = join_,
			.contract_hash = hash,
		};
	}
	return NewClassicNetplayLobbyDatagram{
		.type = NewClassicNetplayLobbyDatagramType::join_ready,
		.session_id = session_id_,
		.host = host_,
		.join = join_,
		.contract_hash = hash,
	};
}

std::expected<void, NewClassicNetplayLobbyError>
NewClassicNetplayLobby::receive(
	const NewClassicNetplayLobbyDatagram &datagram
) noexcept
{
	if(role_ == NewClassicNetplayLobbyRole::host) {
		if(datagram.type == NewClassicNetplayLobbyDatagramType::join_request) {
			if(!datagram.join) {
				return std::unexpected(lobby_error(
					NewClassicNetplayLobbyErrorCode::invalid_join_lock
				));
			}
			if(join_ && !equal_join_lock(*join_, *datagram.join)) {
				return std::unexpected(lobby_error(
					NewClassicNetplayLobbyErrorCode::lock_changed
				));
			}
			join_ = datagram.join;
			contract_ = derive(host_, join_);
			if(!contract_) {
				return std::unexpected(lobby_error(
					NewClassicNetplayLobbyErrorCode::invalid_join_lock
				));
			}
			return {};
		}
		if(datagram.type != NewClassicNetplayLobbyDatagramType::join_ready) {
			return std::unexpected(lobby_error(
				NewClassicNetplayLobbyErrorCode::wrong_direction
			));
		}
	} else {
		if(datagram.type != NewClassicNetplayLobbyDatagramType::host_offer &&
			datagram.type != NewClassicNetplayLobbyDatagramType::host_accept) {
			return std::unexpected(lobby_error(
				NewClassicNetplayLobbyErrorCode::wrong_direction
			));
		}
	}
	if(datagram.session_id == 0U ||
		(session_id_ != 0U && datagram.session_id != session_id_)) {
		return std::unexpected(lobby_error(
			NewClassicNetplayLobbyErrorCode::invalid_session
		));
	}
	if(!datagram.host || !datagram.join ||
		(role_ == NewClassicNetplayLobbyRole::host &&
			(!host_ || !join_ || !equal_host_lock(*host_, *datagram.host) ||
			 !equal_join_lock(*join_, *datagram.join))) ||
		(role_ == NewClassicNetplayLobbyRole::join &&
			(!join_ || !equal_join_lock(*join_, *datagram.join)))) {
		return std::unexpected(lobby_error(
			NewClassicNetplayLobbyErrorCode::lock_changed
		));
	}
	const auto received_contract = create_new_classic_netplay_match_contract(
		*datagram.host, *datagram.join
	);
	if(!received_contract || contract_hash(*received_contract) == 0U ||
		contract_hash(*received_contract) != datagram.contract_hash) {
		return std::unexpected(lobby_error(
			NewClassicNetplayLobbyErrorCode::contract_mismatch
		));
	}
	if(contract_ && *contract_ != *received_contract) {
		return std::unexpected(lobby_error(
			NewClassicNetplayLobbyErrorCode::contract_mismatch
		));
	}
	session_id_ = datagram.session_id;
	host_ = datagram.host;
	contract_ = *received_contract;
	if((role_ == NewClassicNetplayLobbyRole::host &&
		datagram.type == NewClassicNetplayLobbyDatagramType::join_ready) ||
		(role_ == NewClassicNetplayLobbyRole::join &&
		datagram.type == NewClassicNetplayLobbyDatagramType::host_accept)) {
		status_ = NewClassicNetplayLobbyStatus::ready;
	}
	return {};
}

std::optional<NewClassicNetplayMatchContract>
NewClassicNetplayLobby::contract() const noexcept
{
	return contract_;
}

std::uint64_t NewClassicNetplayLobby::session_id() const noexcept
{
	return session_id_;
}

} // namespace th03::app
