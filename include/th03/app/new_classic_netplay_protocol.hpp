#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_PROTOCOL_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_PROTOCOL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "th03/core/game.hpp"

namespace th03::app {

inline constexpr std::size_t new_classic_netplay_input_wire_bytes = 4U;
inline constexpr std::uint16_t new_classic_netplay_player_input_bits = 0x0F3FU;

enum class NewClassicNetplayPlayer : std::uint8_t {
	player_one,
	player_two,
};

enum class NewClassicNetplaySimulationDigestKind : std::uint8_t {
	initial = 0U,
	periodic = 1U,
	boundary = 2U,
};

enum class NewClassicNetplayBoundaryProof : std::uint8_t {
	none = 0U,
	phase_transition = 1U,
	round_transition = 2U,
	terminal = 3U,
};

[[nodiscard]] constexpr NewClassicNetplayBoundaryProof
new_classic_netplay_boundary_proof(StepStatus status) noexcept
{
	switch(status) {
	case StepStatus::phase_transition:
		return NewClassicNetplayBoundaryProof::phase_transition;
	case StepStatus::round_transition:
		return NewClassicNetplayBoundaryProof::round_transition;
	case StepStatus::terminal:
		return NewClassicNetplayBoundaryProof::terminal;
	case StepStatus::running:
	case StepStatus::paused:
	case StepStatus::invalid_input_context:
	case StepStatus::simulation_not_implemented:
		return NewClassicNetplayBoundaryProof::none;
	}
	return NewClassicNetplayBoundaryProof::none;
}

// T3NET001 backend value. This is post-device/post-KeyConfig logical input for
// exactly one owned player. It contains no player identity, system word, raw
// scancode, application event, clock, or host-device information.
struct NewClassicNetplayOwnedInput final {
	std::uint16_t word = 0U;
	std::uint8_t charge = 0U;
	std::uint8_t reserved_zero = 0U;

	friend constexpr bool operator==(
		const NewClassicNetplayOwnedInput &,
		const NewClassicNetplayOwnedInput &
	) = default;
};

enum class NewClassicNetplayInputErrorCode : std::uint8_t {
	invalid_wire_size,
	invalid_player_word,
	invalid_charge,
	nonzero_reserved,
	invalid_player,
};

struct NewClassicNetplayInputError final {
	NewClassicNetplayInputErrorCode code =
		NewClassicNetplayInputErrorCode::invalid_wire_size;
};

[[nodiscard]] std::expected<
	std::array<std::byte, new_classic_netplay_input_wire_bytes>,
	NewClassicNetplayInputError
> encode_new_classic_netplay_input(
	NewClassicNetplayOwnedInput input
) noexcept;

[[nodiscard]] std::expected<
	NewClassicNetplayOwnedInput,
	NewClassicNetplayInputError
> decode_new_classic_netplay_input(
	std::span<const std::byte> wire
) noexcept;

[[nodiscard]] std::expected<void, NewClassicNetplayInputError>
apply_new_classic_netplay_input(
	CoreInput &combined,
	NewClassicNetplayPlayer player,
	NewClassicNetplayOwnedInput input
) noexcept;

[[nodiscard]] std::expected<CoreInput, NewClassicNetplayInputError>
combine_new_classic_netplay_inputs(
	NewClassicNetplayOwnedInput player_one,
	NewClassicNetplayOwnedInput player_two
) noexcept;

} // namespace th03::app

#endif
