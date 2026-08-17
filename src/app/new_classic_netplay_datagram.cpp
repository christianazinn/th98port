#include "th03/app/new_classic_netplay_datagram.hpp"

#include <algorithm>

namespace th03::app {
namespace {

constexpr std::array magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'N'}, std::byte{'P'}
};
constexpr std::size_t input_offset = 40U;
constexpr std::size_t digest_kind_offset = 72U;
constexpr std::size_t boundary_proof_offset = 73U;
constexpr std::size_t digest_reserved_offset = 74U;

[[nodiscard]] NewClassicNetplayDatagramError error(
	NewClassicNetplayDatagramErrorCode code
) noexcept
{
	return NewClassicNetplayDatagramError{.code = code};
}

void put_u64(std::span<std::byte> output, std::size_t offset,
	std::uint64_t value) noexcept
{
	for(std::size_t index = 0U; index < 8U; ++index) {
		output[offset + index] = std::byte(
			static_cast<std::uint8_t>(value >> (index * 8U))
		);
	}
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

[[nodiscard]] bool valid_type(NewClassicNetplayDatagramType type) noexcept
{
	return type == NewClassicNetplayDatagramType::hello ||
		type == NewClassicNetplayDatagramType::accept ||
		type == NewClassicNetplayDatagramType::input ||
		type == NewClassicNetplayDatagramType::disconnect ||
		type == NewClassicNetplayDatagramType::simulation_digest ||
		type == NewClassicNetplayDatagramType::simulation_digest_ack ||
		type == NewClassicNetplayDatagramType::pause_waiting ||
		type == NewClassicNetplayDatagramType::resume_ready ||
		type == NewClassicNetplayDatagramType::forfeit_request ||
		type == NewClassicNetplayDatagramType::forfeit_ack;
}

[[nodiscard]] bool valid_sender(NewClassicNetplayPlayer sender) noexcept
{
	return sender == NewClassicNetplayPlayer::player_one ||
		sender == NewClassicNetplayPlayer::player_two;
}

[[nodiscard]] bool valid_digest_kind(
	NewClassicNetplaySimulationDigestKind kind
) noexcept
{
	return kind == NewClassicNetplaySimulationDigestKind::initial ||
		kind == NewClassicNetplaySimulationDigestKind::periodic ||
		kind == NewClassicNetplaySimulationDigestKind::boundary;
}

[[nodiscard]] bool valid_boundary_proof(
	NewClassicNetplayBoundaryProof boundary
) noexcept
{
	return boundary == NewClassicNetplayBoundaryProof::none ||
		boundary == NewClassicNetplayBoundaryProof::phase_transition ||
		boundary == NewClassicNetplayBoundaryProof::round_transition ||
		boundary == NewClassicNetplayBoundaryProof::terminal ||
		boundary == NewClassicNetplayBoundaryProof::pause;
}

[[nodiscard]] std::expected<void, NewClassicNetplayDatagramError>
validate(const NewClassicNetplayDatagram &datagram) noexcept
{
	if(!valid_type(datagram.type)) {
		return std::unexpected(error(
			NewClassicNetplayDatagramErrorCode::invalid_type
		));
	}
	if(!valid_sender(datagram.sender)) {
		return std::unexpected(error(
			NewClassicNetplayDatagramErrorCode::invalid_sender
		));
	}
	if(datagram.contract_hash == 0U) {
		return std::unexpected(error(
			NewClassicNetplayDatagramErrorCode::invalid_contract
		));
	}
	const bool hello = datagram.type ==
		NewClassicNetplayDatagramType::hello;
	if(hello != (datagram.session_id == 0U)) {
		return std::unexpected(error(
			NewClassicNetplayDatagramErrorCode::invalid_session
		));
	}
	const bool input = datagram.type ==
		NewClassicNetplayDatagramType::input;
	const bool digest = datagram.type ==
		NewClassicNetplayDatagramType::simulation_digest ||
		datagram.type ==
			NewClassicNetplayDatagramType::simulation_digest_ack;
	const bool digest_ack = datagram.type ==
		NewClassicNetplayDatagramType::simulation_digest_ack;
	const bool pause_signal = datagram.type ==
			NewClassicNetplayDatagramType::pause_waiting ||
		datagram.type == NewClassicNetplayDatagramType::resume_ready ||
		datagram.type == NewClassicNetplayDatagramType::forfeit_request ||
		datagram.type == NewClassicNetplayDatagramType::forfeit_ack;
	if(input) {
		if(datagram.input_count == 0U ||
			datagram.input_count > datagram.inputs.size() ||
			datagram.latest_frame + 1U < datagram.input_count ||
			std::ranges::any_of(datagram.simulation_digest,
				[](std::uint8_t byte) { return byte != 0U; })) {
			return std::unexpected(error(
				NewClassicNetplayDatagramErrorCode::invalid_history
			));
		}
	} else if(digest) {
		const bool initial = datagram.simulation_digest_kind ==
			NewClassicNetplaySimulationDigestKind::initial;
		const bool periodic = datagram.simulation_digest_kind ==
			NewClassicNetplaySimulationDigestKind::periodic;
		const bool boundary = datagram.simulation_digest_kind ==
			NewClassicNetplaySimulationDigestKind::boundary;
		if(datagram.input_count != 0U ||
			!valid_digest_kind(datagram.simulation_digest_kind) ||
			!valid_boundary_proof(datagram.boundary_proof) ||
			(initial != (datagram.latest_frame == 0U &&
				datagram.confirmed_frame_count == 0U)) ||
			(digest_ack && initial) ||
			(periodic && (datagram.latest_frame == 0U ||
				datagram.confirmed_frame_count == 0U ||
				datagram.confirmed_frame_count % 120U != 0U ||
				datagram.boundary_proof !=
					NewClassicNetplayBoundaryProof::none)) ||
			(boundary && (datagram.latest_frame == 0U ||
				datagram.boundary_proof ==
					NewClassicNetplayBoundaryProof::none)) ||
			(initial && datagram.boundary_proof !=
				NewClassicNetplayBoundaryProof::none) ||
			std::ranges::all_of(datagram.simulation_digest,
				[](std::uint8_t byte) { return byte == 0U; })) {
			return std::unexpected(error(
				NewClassicNetplayDatagramErrorCode::invalid_digest
			));
		}
	} else if(pause_signal) {
		if(datagram.input_count != 0U || datagram.latest_frame == 0U ||
			datagram.confirmed_frame_count == 0U ||
			datagram.simulation_digest_kind !=
				NewClassicNetplaySimulationDigestKind::initial ||
			datagram.boundary_proof != NewClassicNetplayBoundaryProof::pause ||
			std::ranges::any_of(datagram.simulation_digest,
				[](std::uint8_t byte) { return byte != 0U; })) {
			return std::unexpected(error(
				NewClassicNetplayDatagramErrorCode::invalid_history
			));
		}
	} else if(datagram.input_count != 0U || datagram.latest_frame != 0U ||
		datagram.confirmed_frame_count != 0U ||
		datagram.simulation_digest_kind !=
			NewClassicNetplaySimulationDigestKind::initial ||
		datagram.boundary_proof != NewClassicNetplayBoundaryProof::none ||
		std::ranges::any_of(datagram.simulation_digest,
			[](std::uint8_t byte) { return byte != 0U; })) {
		return std::unexpected(error(
			NewClassicNetplayDatagramErrorCode::invalid_history
		));
	}
	for(std::size_t index = 0U; index < datagram.input_count; ++index) {
		const auto encoded = encode_new_classic_netplay_input(
			datagram.inputs[index]
		);
		if(!encoded) {
			return std::unexpected(NewClassicNetplayDatagramError{
				.code = NewClassicNetplayDatagramErrorCode::invalid_input,
				.input_index = index,
				.input = encoded.error(),
			});
		}
	}
	return {};
}

} // namespace

NewClassicNetplayProofOrder new_classic_netplay_proof_order(
	std::uint64_t verified_frame,
	std::uint64_t received_frame
) noexcept
{
	if(received_frame < verified_frame) {
		return NewClassicNetplayProofOrder::stale;
	}
	if(received_frame == verified_frame) {
		return NewClassicNetplayProofOrder::current;
	}
	return NewClassicNetplayProofOrder::future;
}

std::expected<
	std::array<std::byte, new_classic_netplay_datagram_bytes>,
	NewClassicNetplayDatagramError
> encode_new_classic_netplay_datagram(
	const NewClassicNetplayDatagram &datagram
) noexcept
{
	if(const auto valid = validate(datagram); !valid) {
		return std::unexpected(valid.error());
	}
	std::array<std::byte, new_classic_netplay_datagram_bytes> result{};
	std::ranges::copy(magic, result.begin());
	result[4U] = std::byte{new_classic_netplay_datagram_version};
	result[5U] = std::byte(static_cast<std::uint8_t>(datagram.type));
	result[6U] = std::byte(static_cast<std::uint8_t>(datagram.sender));
	result[7U] = std::byte{datagram.input_count};
	put_u64(result, 8U, datagram.session_id);
	put_u64(result, 16U, datagram.contract_hash);
	put_u64(result, 24U, datagram.latest_frame);
	put_u64(result, 32U, datagram.confirmed_frame_count);
	if(datagram.type == NewClassicNetplayDatagramType::simulation_digest ||
		datagram.type ==
			NewClassicNetplayDatagramType::simulation_digest_ack) {
		for(std::size_t index = 0U;
			index < datagram.simulation_digest.size(); ++index) {
			result[input_offset + index] = std::byte{
				datagram.simulation_digest[index]
			};
		}
		result[digest_kind_offset] = std::byte{
			static_cast<std::uint8_t>(datagram.simulation_digest_kind)
		};
		result[boundary_proof_offset] = std::byte{
			static_cast<std::uint8_t>(datagram.boundary_proof)
		};
	} else if(datagram.type ==
			NewClassicNetplayDatagramType::pause_waiting ||
		datagram.type == NewClassicNetplayDatagramType::resume_ready ||
		datagram.type == NewClassicNetplayDatagramType::forfeit_request ||
		datagram.type == NewClassicNetplayDatagramType::forfeit_ack) {
		result[boundary_proof_offset] = std::byte{
			static_cast<std::uint8_t>(datagram.boundary_proof)
		};
	}
	for(std::size_t index = 0U; index < datagram.input_count; ++index) {
		const auto encoded = encode_new_classic_netplay_input(
			datagram.inputs[index]
		);
		if(!encoded) {
			return std::unexpected(NewClassicNetplayDatagramError{
				.code = NewClassicNetplayDatagramErrorCode::invalid_input,
				.input_index = index,
				.input = encoded.error(),
			});
		}
		std::ranges::copy(
			*encoded,
			result.begin() + static_cast<std::ptrdiff_t>(
				input_offset + index * encoded->size()
			)
		);
	}
	return result;
}

std::expected<NewClassicNetplayDatagram, NewClassicNetplayDatagramError>
decode_new_classic_netplay_datagram(
	std::span<const std::byte> wire
) noexcept
{
	if(wire.size() != new_classic_netplay_datagram_bytes) {
		return std::unexpected(error(
			NewClassicNetplayDatagramErrorCode::invalid_size
		));
	}
	if(!std::ranges::equal(magic, wire.first(magic.size()))) {
		return std::unexpected(error(
			NewClassicNetplayDatagramErrorCode::invalid_magic
		));
	}
	if(std::to_integer<std::uint8_t>(wire[4U]) !=
		new_classic_netplay_datagram_version) {
		return std::unexpected(error(
			NewClassicNetplayDatagramErrorCode::unsupported_version
		));
	}
	NewClassicNetplayDatagram result{
		.type = static_cast<NewClassicNetplayDatagramType>(
			std::to_integer<std::uint8_t>(wire[5U])
		),
		.sender = static_cast<NewClassicNetplayPlayer>(
			std::to_integer<std::uint8_t>(wire[6U])
		),
		.input_count = std::to_integer<std::uint8_t>(wire[7U]),
		.session_id = get_u64(wire, 8U),
		.contract_hash = get_u64(wire, 16U),
		.latest_frame = get_u64(wire, 24U),
		.confirmed_frame_count = get_u64(wire, 32U),
	};
	if(result.type == NewClassicNetplayDatagramType::simulation_digest ||
		result.type ==
			NewClassicNetplayDatagramType::simulation_digest_ack) {
		for(std::size_t index = 0U;
			index < result.simulation_digest.size(); ++index) {
			result.simulation_digest[index] =
				std::to_integer<std::uint8_t>(wire[input_offset + index]);
		}
		result.simulation_digest_kind =
			static_cast<NewClassicNetplaySimulationDigestKind>(
				std::to_integer<std::uint8_t>(wire[digest_kind_offset])
			);
		result.boundary_proof = static_cast<NewClassicNetplayBoundaryProof>(
			std::to_integer<std::uint8_t>(wire[boundary_proof_offset])
		);
		if(std::ranges::any_of(
			wire.subspan(digest_reserved_offset),
			[](std::byte byte) { return byte != std::byte{0U}; }
		)) {
			return std::unexpected(error(
				NewClassicNetplayDatagramErrorCode::invalid_digest
			));
		}
	} else if(result.type ==
			NewClassicNetplayDatagramType::pause_waiting ||
		result.type == NewClassicNetplayDatagramType::resume_ready ||
		result.type == NewClassicNetplayDatagramType::forfeit_request ||
		result.type == NewClassicNetplayDatagramType::forfeit_ack) {
		result.boundary_proof = static_cast<NewClassicNetplayBoundaryProof>(
			std::to_integer<std::uint8_t>(wire[boundary_proof_offset])
		);
		if(std::ranges::any_of(
			wire.subspan(input_offset, boundary_proof_offset - input_offset),
			[](std::byte byte) { return byte != std::byte{0U}; }
		) || std::ranges::any_of(
			wire.subspan(digest_reserved_offset),
			[](std::byte byte) { return byte != std::byte{0U}; }
		)) {
			return std::unexpected(error(
				NewClassicNetplayDatagramErrorCode::invalid_history
			));
		}
	}
	if(result.input_count > result.inputs.size()) {
		return std::unexpected(error(
			NewClassicNetplayDatagramErrorCode::invalid_history
		));
	}
	for(std::size_t index = 0U; index < result.input_count; ++index) {
		const auto decoded = decode_new_classic_netplay_input(wire.subspan(
			input_offset + index * new_classic_netplay_input_wire_bytes,
			new_classic_netplay_input_wire_bytes
		));
		if(!decoded) {
			return std::unexpected(NewClassicNetplayDatagramError{
				.code = NewClassicNetplayDatagramErrorCode::invalid_input,
				.input_index = index,
				.input = decoded.error(),
			});
		}
		result.inputs[index] = *decoded;
	}
	if(const auto valid = validate(result); !valid) {
		return std::unexpected(valid.error());
	}
	return result;
}

} // namespace th03::app
