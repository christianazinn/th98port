#include "th03/app/new_classic_netplay_match_contract.hpp"

#include <algorithm>

namespace th03::app {
namespace {

constexpr std::array magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'N'}, std::byte{'C'}
};

[[nodiscard]] NewClassicNetplayMatchContractError error(
	NewClassicNetplayMatchContractErrorCode code
) noexcept
{
	return NewClassicNetplayMatchContractError{.code = code};
}

[[nodiscard]] bool valid_character_lock(
	NewClassicNetplayCharacterLock lock
) noexcept
{
	return lock.character >= 1U && lock.character <= 9U &&
		lock.palette >= 1U && lock.palette <= 2U;
}

[[nodiscard]] bool valid_contract(
	const NewClassicNetplayMatchContract &contract
) noexcept
{
	return valid_character_lock(contract.player_one) &&
		valid_character_lock(contract.player_two) && contract.rank <= 3U &&
		contract.cadence_ticks != 0U && contract.cadence_seconds != 0U &&
		contract.input_delay_frames <= 6U && contract.autofire_mask <= 3U &&
		(contract.player_one.character != contract.player_two.character ||
			contract.player_one.palette != contract.player_two.palette);
}

void put_u32(std::span<std::byte> output, std::size_t offset,
	std::uint32_t value) noexcept
{
	for(std::size_t index = 0U; index < 4U; ++index) {
		output[offset + index] = std::byte(
			static_cast<std::uint8_t>(value >> (index * 8U))
		);
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

} // namespace

std::expected<
	NewClassicNetplayMatchContract,
	NewClassicNetplayMatchContractError
> create_new_classic_netplay_match_contract(
	NewClassicNetplayHostLock host,
	NewClassicNetplayJoinLock join
) noexcept
{
	if(!valid_character_lock(host.player_one) || host.rank > 3U ||
		host.cadence_ticks == 0U || host.cadence_seconds == 0U) {
		return std::unexpected(error(
			NewClassicNetplayMatchContractErrorCode::invalid_host_lock
		));
	}
	if(host.input_delay_frames > 6U) {
		return std::unexpected(error(
			NewClassicNetplayMatchContractErrorCode::invalid_input_delay
		));
	}
	if(!valid_character_lock(join.player_two)) {
		return std::unexpected(error(
			NewClassicNetplayMatchContractErrorCode::invalid_join_lock
		));
	}
	if(host.player_one == join.player_two) {
		join.player_two.palette = join.player_two.palette == 1U ? 2U : 1U;
	}
	return NewClassicNetplayMatchContract{
		.player_one = host.player_one,
		.player_two = join.player_two,
		.rank = host.rank,
		.seed = host.seed,
		.cadence_ticks = host.cadence_ticks,
		.cadence_seconds = host.cadence_seconds,
		.input_delay_frames = host.input_delay_frames,
		.autofire_mask = static_cast<std::uint8_t>(
			(host.player_one_autofire ? 1U : 0U) |
			(join.player_two_autofire ? 2U : 0U)
		),
	};
}

std::expected<
	std::array<std::byte, new_classic_netplay_match_contract_bytes>,
	NewClassicNetplayMatchContractError
> encode_new_classic_netplay_match_contract(
	const NewClassicNetplayMatchContract &contract
) noexcept
{
	if(!valid_contract(contract)) {
		return std::unexpected(error(
			NewClassicNetplayMatchContractErrorCode::invalid_host_lock
		));
	}
	std::array<std::byte, new_classic_netplay_match_contract_bytes> result{};
	std::ranges::copy(magic, result.begin());
	result[4U] = std::byte{new_classic_netplay_match_contract_version};
	result[5U] = std::byte{contract.rank};
	result[6U] = std::byte{contract.player_one.character};
	result[7U] = std::byte{contract.player_one.palette};
	result[8U] = std::byte{contract.player_two.character};
	result[9U] = std::byte{contract.player_two.palette};
	result[10U] = std::byte{contract.input_delay_frames};
	result[11U] = std::byte{contract.autofire_mask};
	put_u32(result, 12U, contract.seed);
	put_u32(result, 16U, contract.cadence_ticks);
	put_u32(result, 20U, contract.cadence_seconds);
	put_u32(result, 24U, new_classic_netplay_ruleset_version);
	return result;
}

std::expected<
	NewClassicNetplayMatchContract,
	NewClassicNetplayMatchContractError
> decode_new_classic_netplay_match_contract(
	std::span<const std::byte> wire
) noexcept
{
	if(wire.size() != new_classic_netplay_match_contract_bytes) {
		return std::unexpected(error(
			NewClassicNetplayMatchContractErrorCode::invalid_size
		));
	}
	if(!std::ranges::equal(magic, wire.first(magic.size()))) {
		return std::unexpected(error(
			NewClassicNetplayMatchContractErrorCode::invalid_magic
		));
	}
	if(std::to_integer<std::uint8_t>(wire[4U]) !=
		new_classic_netplay_match_contract_version) {
		return std::unexpected(error(
			NewClassicNetplayMatchContractErrorCode::unsupported_version
		));
	}
	if(get_u32(wire, 24U) != new_classic_netplay_ruleset_version) {
		return std::unexpected(error(
			NewClassicNetplayMatchContractErrorCode::unsupported_ruleset
		));
	}
	if(std::to_integer<std::uint8_t>(wire[11U]) > 3U ||
		std::ranges::any_of(wire.subspan(28U), [](std::byte value) {
			return value != std::byte{0U};
		})) {
		return std::unexpected(error(
			NewClassicNetplayMatchContractErrorCode::nonzero_reserved
		));
	}
	const NewClassicNetplayHostLock host{
		.player_one = {
			.character = std::to_integer<std::uint8_t>(wire[6U]),
			.palette = std::to_integer<std::uint8_t>(wire[7U]),
		},
		.player_one_autofire =
			(std::to_integer<std::uint8_t>(wire[11U]) & 1U) != 0U,
		.rank = std::to_integer<std::uint8_t>(wire[5U]),
		.seed = get_u32(wire, 12U),
		.cadence_ticks = get_u32(wire, 16U),
		.cadence_seconds = get_u32(wire, 20U),
		.input_delay_frames = std::to_integer<std::uint8_t>(wire[10U]),
	};
	const NewClassicNetplayJoinLock join{
		.player_two = {
			.character = std::to_integer<std::uint8_t>(wire[8U]),
			.palette = std::to_integer<std::uint8_t>(wire[9U]),
		},
		.player_two_autofire =
			(std::to_integer<std::uint8_t>(wire[11U]) & 2U) != 0U,
	};
	return create_new_classic_netplay_match_contract(host, join);
}

std::expected<std::uint64_t, NewClassicNetplayMatchContractError>
new_classic_netplay_match_contract_hash(
	const NewClassicNetplayMatchContract &contract
) noexcept
{
	const auto wire = encode_new_classic_netplay_match_contract(contract);
	if(!wire) {
		return std::unexpected(wire.error());
	}
	std::uint64_t hash = 14695981039346656037ULL;
	for(const auto byte : *wire) {
		hash ^= std::to_integer<std::uint8_t>(byte);
		hash *= 1099511628211ULL;
	}
	return hash == 0U ? 1U : hash;
}

} // namespace th03::app
