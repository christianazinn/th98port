#include "th03/app/new_classic_netplay_datagram.hpp"
#include "th03/app/new_classic_netplay_lobby.hpp"
#include "th03/app/new_classic_netplay_pause_barrier.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace th03::app;

[[nodiscard]] char hex_digit(std::uint8_t value) noexcept
{
	return static_cast<char>(value < 10U ? '0' + value : 'A' + value - 10U);
}

[[nodiscard]] std::optional<std::uint8_t> hex_value(char value) noexcept
{
	if(value >= '0' && value <= '9') {
		return static_cast<std::uint8_t>(value - '0');
	}
	if(value >= 'A' && value <= 'F') {
		return static_cast<std::uint8_t>(value - 'A' + 10);
	}
	if(value >= 'a' && value <= 'f') {
		return static_cast<std::uint8_t>(value - 'a' + 10);
	}
	return std::nullopt;
}

template<std::size_t Size>
[[nodiscard]] std::string encode_hex(const std::array<std::byte, Size> &wire)
{
	std::string result(Size * 2U, '0');
	for(std::size_t index = 0U; index < Size; ++index) {
		const auto value = std::to_integer<std::uint8_t>(wire[index]);
		result[index * 2U] = hex_digit(static_cast<std::uint8_t>(value >> 4U));
		result[index * 2U + 1U] = hex_digit(
			static_cast<std::uint8_t>(value & 0x0FU)
		);
	}
	return result;
}

[[nodiscard]] std::optional<std::vector<std::byte>> decode_hex(
	std::string_view text
)
{
	if(text.empty() || (text.size() % 2U) != 0U) {
		return std::nullopt;
	}
	std::vector<std::byte> result(text.size() / 2U);
	for(std::size_t index = 0U; index < result.size(); ++index) {
		const auto high = hex_value(text[index * 2U]);
		const auto low = hex_value(text[index * 2U + 1U]);
		if(!high || !low) {
			return std::nullopt;
		}
		result[index] = std::byte{
			static_cast<std::uint8_t>((*high << 4U) | *low)
		};
	}
	return result;
}

template<class Error, std::size_t Size>
[[nodiscard]] bool emit_wire(
	const std::expected<std::array<std::byte, Size>, Error> &wire
)
{
	if(!wire) {
		return false;
	}
	std::cout << "WIRE " << encode_hex(*wire) << std::endl;
	return true;
}

[[nodiscard]] bool is_pause_signal(NewClassicNetplayDatagramType type) noexcept
{
	return type == NewClassicNetplayDatagramType::pause_waiting ||
		type == NewClassicNetplayDatagramType::resume_ready ||
		type == NewClassicNetplayDatagramType::forfeit_request ||
		type == NewClassicNetplayDatagramType::forfeit_ack;
}

[[nodiscard]] std::optional<std::uint64_t> contract_hash(
	const NewClassicNetplayLobby &lobby
) noexcept
{
	const auto contract = lobby.contract();
	if(!contract) {
		return std::nullopt;
	}
	const auto hash = new_classic_netplay_match_contract_hash(*contract);
	return hash ? std::optional{*hash} : std::nullopt;
}

[[nodiscard]] NewClassicNetplayDatagram base_datagram(
	const NewClassicNetplayLobby &lobby,
	NewClassicNetplayPlayer player,
	NewClassicNetplayDatagramType type
) noexcept
{
	return NewClassicNetplayDatagram{
		.type = type,
		.sender = player,
		.session_id = lobby.session_id(),
		.contract_hash = contract_hash(lobby).value_or(0U),
	};
}

int run(std::string_view role)
{
	const bool is_host = role == "host";
	if(!is_host && role != "join") {
		return 2;
	}
	constexpr NewClassicNetplayHostLock host_lock{
		.player_one = {.character = 1U, .palette = 1U},
		.rank = 1U,
		.seed = 34U,
		.input_delay_frames = 4U,
	};
	constexpr NewClassicNetplayJoinLock join_lock{
		.player_two = {.character = 2U, .palette = 2U},
	};
	constexpr std::uint64_t session_id = 0x0102030405060708ULL;
	auto created = is_host ?
		NewClassicNetplayLobby::create_host(host_lock, session_id) :
		NewClassicNetplayLobby::create_join(join_lock);
	if(!created) {
		return 3;
	}
	auto lobby = std::move(*created);
	const auto local_player = is_host ? NewClassicNetplayPlayer::player_one :
		NewClassicNetplayPlayer::player_two;
	const auto peer_player = is_host ? NewClassicNetplayPlayer::player_two :
		NewClassicNetplayPlayer::player_one;
	std::optional<NewClassicNetplayDatagram> last_datagram{};
	std::optional<NewClassicNetplayPauseBarrier> pause{};
	std::string line{};
	while(std::getline(std::cin, line)) {
		std::istringstream input(line);
		std::string command{};
		input >> command;
		if(command == "OUTBOUND") {
			const auto outbound = lobby.outbound();
			if(!outbound) {
				std::cout << "NONE" << std::endl;
				continue;
			}
			if(!emit_wire(encode_new_classic_netplay_lobby_datagram(*outbound))) {
				return 4;
			}
			continue;
		}
		if(command == "RECEIVE") {
			std::string hex{};
			input >> hex;
			const auto wire = decode_hex(hex);
			if(!wire) {
				return 5;
			}
			if(wire->size() == new_classic_netplay_lobby_bytes) {
				const auto decoded = decode_new_classic_netplay_lobby_datagram(*wire);
				if(!decoded || !lobby.receive(*decoded)) {
					return 6;
				}
				std::cout << "LOBBY " << static_cast<unsigned>(decoded->type) <<
					std::endl;
				continue;
			}
			const auto decoded = decode_new_classic_netplay_datagram(*wire);
			const auto hash = contract_hash(lobby);
			if(!decoded || !hash || decoded->sender != peer_player ||
				decoded->contract_hash != *hash ||
				(decoded->type != NewClassicNetplayDatagramType::hello &&
					decoded->session_id != lobby.session_id())) {
				return 7;
			}
			if(pause && is_pause_signal(decoded->type) && !pause->receive(
				decoded->type, decoded->latest_frame,
				decoded->confirmed_frame_count
			)) {
				return 8;
			}
			last_datagram = *decoded;
			std::cout << "TYPE " << static_cast<unsigned>(decoded->type) <<
				" FRAME " << decoded->latest_frame << " WORD " <<
				(decoded->input_count == 0U ? 0U : decoded->inputs[0U].word) <<
				" STATUS " << (pause ? static_cast<unsigned>(pause->status()) :
					255U) << std::endl;
			continue;
		}
		if(command == "CONTRACT") {
			const auto hash = contract_hash(lobby);
			const auto contract = lobby.contract();
			if(!hash || !contract || lobby.status() !=
				NewClassicNetplayLobbyStatus::ready) {
				return 9;
			}
			std::cout << "CONTRACT " << lobby.session_id() << ' ' << *hash <<
				' ' << static_cast<unsigned>(contract->input_delay_frames) <<
				std::endl;
			continue;
		}
		if(command == "MAKE_INPUT") {
			std::uint64_t frame = 0U;
			unsigned word = 0U;
			unsigned charge = 0U;
			unsigned control = 0U;
			input >> frame >> word >> charge >> control;
			auto datagram = base_datagram(
				lobby, local_player, NewClassicNetplayDatagramType::input
			);
			datagram.input_count = 1U;
			datagram.latest_frame = frame;
			datagram.inputs[0U] = NewClassicNetplayOwnedInput{
				.word = static_cast<std::uint16_t>(word),
				.charge = static_cast<std::uint8_t>(charge),
				.control = static_cast<NewClassicNetplayControl>(control),
			};
			if(!emit_wire(encode_new_classic_netplay_datagram(datagram))) {
				return 10;
			}
			continue;
		}
		if(command == "MAKE_PROOF") {
			std::uint64_t frame = 0U;
			std::uint64_t confirmed = 0U;
			unsigned kind = 0U;
			unsigned boundary = 0U;
			unsigned fill = 0U;
			input >> frame >> confirmed >> kind >> boundary >> fill;
			auto datagram = base_datagram(lobby, local_player,
				NewClassicNetplayDatagramType::simulation_digest);
			datagram.latest_frame = frame;
			datagram.confirmed_frame_count = confirmed;
			datagram.simulation_digest_kind =
				static_cast<NewClassicNetplaySimulationDigestKind>(kind);
			datagram.boundary_proof =
				static_cast<NewClassicNetplayBoundaryProof>(boundary);
			for(std::size_t index = 0U;
				index < datagram.simulation_digest.size(); ++index) {
				datagram.simulation_digest[index] = static_cast<std::uint8_t>(
					fill + static_cast<unsigned>(index)
				);
			}
			if(!emit_wire(encode_new_classic_netplay_datagram(datagram))) {
				return 11;
			}
			continue;
		}
		if(command == "ACK_LAST") {
			if(!last_datagram || last_datagram->type !=
				NewClassicNetplayDatagramType::simulation_digest) {
				return 12;
			}
			auto acknowledged = *last_datagram;
			acknowledged.type =
				NewClassicNetplayDatagramType::simulation_digest_ack;
			acknowledged.sender = local_player;
			if(!emit_wire(encode_new_classic_netplay_datagram(acknowledged))) {
				return 13;
			}
			continue;
		}
		if(command == "PAUSE_CREATE") {
			std::uint64_t frame = 0U;
			std::uint64_t confirmed = 0U;
			input >> frame >> confirmed;
			auto created_pause = NewClassicNetplayPauseBarrier::create(
				frame, confirmed
			);
			if(!created_pause) {
				return 14;
			}
			pause = std::move(*created_pause);
			std::cout << "STATUS " << static_cast<unsigned>(pause->status()) <<
				std::endl;
			continue;
		}
		if(command == "PAUSE_RELEASE") {
			if(!pause) {
				return 15;
			}
			pause->consume_local({});
			std::cout << "STATUS " << static_cast<unsigned>(pause->status()) <<
				std::endl;
			continue;
		}
		if(command == "PAUSE_RESUME") {
			if(!pause) {
				return 16;
			}
			pause->choose_local_resume();
			std::cout << "STATUS " << static_cast<unsigned>(pause->status()) <<
				std::endl;
			continue;
		}
		if(command == "PAUSE_FORFEIT") {
			if(!pause) {
				return 17;
			}
			pause->choose_local_forfeit();
			std::cout << "STATUS " << static_cast<unsigned>(pause->status()) <<
				std::endl;
			continue;
		}
		if(command == "PAUSE_WIRE") {
			if(!pause) {
				return 18;
			}
			auto datagram = base_datagram(
				lobby, local_player, pause->outbound_signal()
			);
			datagram.latest_frame = pause->frame();
			datagram.confirmed_frame_count =
				pause->confirmed_gameplay_frame_count();
			datagram.boundary_proof = NewClassicNetplayBoundaryProof::pause;
			if(!emit_wire(encode_new_classic_netplay_datagram(datagram))) {
				return 19;
			}
			continue;
		}
		if(command == "MAKE_DISCONNECT") {
			if(!emit_wire(encode_new_classic_netplay_datagram(base_datagram(
				lobby, local_player, NewClassicNetplayDatagramType::disconnect
			)))) {
				return 20;
			}
			continue;
		}
		if(command == "EXIT") {
			std::cout << "BYE" << std::endl;
			return 0;
		}
		return 21;
	}
	return 22;
}

} // namespace

int main(int argc, char **argv)
{
	if(argc != 2) {
		return 2;
	}
	return run(argv[1]);
}
