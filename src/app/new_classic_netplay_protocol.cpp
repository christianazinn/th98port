#include "th03/app/new_classic_netplay_protocol.hpp"

namespace th03::app {
namespace {

[[nodiscard]] std::expected<void, NewClassicNetplayInputError>
validate(NewClassicNetplayOwnedInput input) noexcept
{
	if((input.word & ~new_classic_netplay_player_input_bits) != 0U) {
		return std::unexpected(NewClassicNetplayInputError{
			.code = NewClassicNetplayInputErrorCode::invalid_player_word,
		});
	}
	if(input.charge > 1U) {
		return std::unexpected(NewClassicNetplayInputError{
			.code = NewClassicNetplayInputErrorCode::invalid_charge,
		});
	}
	if(input.control != NewClassicNetplayControl::none &&
		input.control != NewClassicNetplayControl::pause_request) {
		return std::unexpected(NewClassicNetplayInputError{
			.code = NewClassicNetplayInputErrorCode::invalid_control,
		});
	}
	return {};
}

} // namespace

std::expected<
	std::array<std::byte, new_classic_netplay_input_wire_bytes>,
	NewClassicNetplayInputError
> encode_new_classic_netplay_input(
	NewClassicNetplayOwnedInput input
) noexcept
{
	if(const auto valid = validate(input); !valid) {
		return std::unexpected(valid.error());
	}
	return std::array<std::byte, new_classic_netplay_input_wire_bytes>{
		std::byte{static_cast<std::uint8_t>(input.word & 0x00FFU)},
		std::byte{static_cast<std::uint8_t>(input.word >> 8U)},
		std::byte{input.charge},
		std::byte{static_cast<std::uint8_t>(input.control)},
	};
}

std::expected<NewClassicNetplayOwnedInput, NewClassicNetplayInputError>
decode_new_classic_netplay_input(std::span<const std::byte> wire) noexcept
{
	if(wire.size() != new_classic_netplay_input_wire_bytes) {
		return std::unexpected(NewClassicNetplayInputError{
			.code = NewClassicNetplayInputErrorCode::invalid_wire_size,
		});
	}
	const NewClassicNetplayOwnedInput result{
		.word = static_cast<std::uint16_t>(
			std::to_integer<std::uint8_t>(wire[0U]) |
			(static_cast<std::uint16_t>(
				std::to_integer<std::uint8_t>(wire[1U])
			) << 8U)
		),
		.charge = std::to_integer<std::uint8_t>(wire[2U]),
		.control = static_cast<NewClassicNetplayControl>(
			std::to_integer<std::uint8_t>(wire[3U])
		),
	};
	if(const auto valid = validate(result); !valid) {
		return std::unexpected(valid.error());
	}
	return result;
}

std::expected<void, NewClassicNetplayInputError>
apply_new_classic_netplay_input(
	CoreInput &combined,
	NewClassicNetplayPlayer player,
	NewClassicNetplayOwnedInput input
) noexcept
{
	if(const auto valid = validate(input); !valid) {
		return std::unexpected(valid.error());
	}
	switch(player) {
	case NewClassicNetplayPlayer::player_one:
		combined.p1 = input.word;
		combined.charge_mask = static_cast<std::uint8_t>(
			(combined.charge_mask & 0x02U) | input.charge
		);
		return {};
	case NewClassicNetplayPlayer::player_two:
		combined.p2 = input.word;
		combined.charge_mask = static_cast<std::uint8_t>(
			(combined.charge_mask & 0x01U) | (input.charge << 1U)
		);
		return {};
	}
	return std::unexpected(NewClassicNetplayInputError{
		.code = NewClassicNetplayInputErrorCode::invalid_player,
	});
}

std::expected<CoreInput, NewClassicNetplayInputError>
combine_new_classic_netplay_inputs(
	NewClassicNetplayOwnedInput player_one,
	NewClassicNetplayOwnedInput player_two
) noexcept
{
	CoreInput result{};
	if(const auto applied = apply_new_classic_netplay_input(
		result, NewClassicNetplayPlayer::player_one, player_one
	); !applied) {
		return std::unexpected(applied.error());
	}
	if(const auto applied = apply_new_classic_netplay_input(
		result, NewClassicNetplayPlayer::player_two, player_two
	); !applied) {
		return std::unexpected(applied.error());
	}
	return result;
}

} // namespace th03::app
