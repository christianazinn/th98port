#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_MATCH_CONTRACT_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_MATCH_CONTRACT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace th03::app {

inline constexpr std::uint8_t new_classic_netplay_match_contract_version = 2U;
inline constexpr std::uint32_t new_classic_netplay_ruleset_version = 1U;
inline constexpr std::size_t new_classic_netplay_match_contract_bytes = 32U;

struct NewClassicNetplayCharacterLock final {
	// Source base character IDs are 1..9; palettes are 1..2.
	std::uint8_t character = 1U;
	std::uint8_t palette = 1U;

	[[nodiscard]] friend constexpr bool operator==(
		const NewClassicNetplayCharacterLock &,
		const NewClassicNetplayCharacterLock &
	) = default;
};

struct NewClassicNetplayHostLock final {
	NewClassicNetplayCharacterLock player_one{};
	std::uint8_t rank = 1U;
	std::uint32_t seed = 0U;
	std::uint32_t cadence_ticks = 526'315U;
	std::uint32_t cadence_seconds = 9'328U;
	std::uint8_t input_delay_frames = 0U;
};

struct NewClassicNetplayJoinLock final {
	NewClassicNetplayCharacterLock player_two{
		.character = 1U,
		.palette = 2U,
	};
};

struct NewClassicNetplayMatchContract final {
	NewClassicNetplayCharacterLock player_one{};
	NewClassicNetplayCharacterLock player_two{
		.character = 1U,
		.palette = 2U,
	};
	std::uint8_t rank = 1U;
	std::uint32_t seed = 0U;
	std::uint32_t cadence_ticks = 526'315U;
	std::uint32_t cadence_seconds = 9'328U;
	std::uint8_t input_delay_frames = 0U;

	[[nodiscard]] friend constexpr bool operator==(
		const NewClassicNetplayMatchContract &,
		const NewClassicNetplayMatchContract &
	) = default;
};

enum class NewClassicNetplayMatchContractErrorCode : std::uint8_t {
	invalid_host_lock,
	invalid_join_lock,
	invalid_input_delay,
	invalid_size,
	invalid_magic,
	unsupported_version,
	unsupported_ruleset,
	nonzero_reserved,
};

struct NewClassicNetplayMatchContractError final {
	NewClassicNetplayMatchContractErrorCode code =
		NewClassicNetplayMatchContractErrorCode::invalid_host_lock;
};

[[nodiscard]] std::expected<
	NewClassicNetplayMatchContract,
	NewClassicNetplayMatchContractError
> create_new_classic_netplay_match_contract(
	NewClassicNetplayHostLock host,
	NewClassicNetplayJoinLock join
) noexcept;

[[nodiscard]] std::expected<
	std::array<std::byte, new_classic_netplay_match_contract_bytes>,
	NewClassicNetplayMatchContractError
> encode_new_classic_netplay_match_contract(
	const NewClassicNetplayMatchContract &contract
) noexcept;

[[nodiscard]] std::expected<
	NewClassicNetplayMatchContract,
	NewClassicNetplayMatchContractError
> decode_new_classic_netplay_match_contract(
	std::span<const std::byte> wire
) noexcept;

[[nodiscard]] std::expected<std::uint64_t,
	NewClassicNetplayMatchContractError>
new_classic_netplay_match_contract_hash(
	const NewClassicNetplayMatchContract &contract
) noexcept;

} // namespace th03::app

#endif
