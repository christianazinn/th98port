#include "th03/app/new_classic_netplay_protocol.hpp"
#include "th03/app/new_classic_netplay_datagram.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <span>
#include <string_view>

namespace {

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::fprintf(stderr, "FAIL: %.*s\n",
			static_cast<int>(message.size()), message.data());
	}
}

void test_wire_vectors()
{
	using namespace th03::app;
	constexpr NewClassicNetplayOwnedInput input{
		.word = 0x0F25U,
		.charge = 1U,
	};
	const auto encoded = encode_new_classic_netplay_input(input);
	constexpr std::array<std::byte, 4U> golden{
		std::byte{0x25U}, std::byte{0x0FU},
		std::byte{0x01U}, std::byte{0x00U},
	};
	expect(encoded && *encoded == golden,
		"T3NET001 owned input has exact four-byte little-endian vector");
	const auto decoded = decode_new_classic_netplay_input(golden);
	expect(decoded && *decoded == input,
		"T3NET001 owned input golden vector round-trips exactly");
	const auto truncated = decode_new_classic_netplay_input(
		std::span(golden).first<3U>()
	);
	expect(!truncated && truncated.error().code ==
		NewClassicNetplayInputErrorCode::invalid_wire_size,
		"owned input rejects truncated wire before reading");
}

void test_validation_and_composition()
{
	using namespace th03::app;
	constexpr NewClassicNetplayOwnedInput p1{
		.word = 0x0F21U,
		.charge = 1U,
	};
	constexpr NewClassicNetplayOwnedInput p2{
		.word = 0x0032U,
	};
	const auto combined = combine_new_classic_netplay_inputs(p1, p2);
	expect(combined && combined->p1 == p1.word &&
		combined->p2 == p2.word && combined->system == 0U &&
		combined->charge_mask == 0x01U,
		"independent player values compose with no source-global injection");

	th03::CoreInput retained{
		.p1 = 0x0001U,
		.p2 = 0x0002U,
		.system = 0x7F3FU,
		.charge_mask = 0x03U,
	};
	expect(apply_new_classic_netplay_input(
		retained, NewClassicNetplayPlayer::player_two,
		{.word = 0x0020U, .charge = 0U}
	) && retained.p1 == 0x0001U && retained.p2 == 0x0020U &&
		retained.system == 0x7F3FU && retained.charge_mask == 0x01U,
		"one owned player cannot overwrite the other player or system word");

	const auto invalid_word = encode_new_classic_netplay_input({
		.word = 0x8000U,
	});
	const auto invalid_charge = encode_new_classic_netplay_input({
		.charge = 2U,
	});
	const auto invalid_reserved = encode_new_classic_netplay_input({
		.reserved_zero = 1U,
	});
	expect(!invalid_word && invalid_word.error().code ==
		NewClassicNetplayInputErrorCode::invalid_player_word,
		"owned input rejects non-player logical bits");
	expect(!invalid_charge && invalid_charge.error().code ==
		NewClassicNetplayInputErrorCode::invalid_charge,
		"owned input rejects non-binary Charge");
	expect(!invalid_reserved && invalid_reserved.error().code ==
		NewClassicNetplayInputErrorCode::nonzero_reserved,
		"owned input reserves its fourth byte for a protocol revision");

	constexpr std::array<std::byte, 4U> malformed_reserved{
		std::byte{0U}, std::byte{0U}, std::byte{0U}, std::byte{1U},
	};
	const auto malformed = decode_new_classic_netplay_input(malformed_reserved);
	expect(!malformed && malformed.error().code ==
		NewClassicNetplayInputErrorCode::nonzero_reserved,
		"wire decoder applies the same fail-closed semantic validation");

	const auto invalid_player = apply_new_classic_netplay_input(
		retained, static_cast<NewClassicNetplayPlayer>(2U), {}
	);
	expect(!invalid_player && invalid_player.error().code ==
		NewClassicNetplayInputErrorCode::invalid_player,
		"unknown player ownership cannot alias either side");
}

void test_datagram_contract()
{
	using namespace th03::app;
	NewClassicNetplayDatagram input{
		.type = NewClassicNetplayDatagramType::input,
		.sender = NewClassicNetplayPlayer::player_two,
		.input_count = 2U,
		.session_id = 0x0102030405060708ULL,
		.contract_hash = 0x8877665544332211ULL,
		.latest_frame = 9U,
		.confirmed_frame_count = 7U,
	};
	input.inputs[0U] = {.word = 0x0F25U, .charge = 1U};
	input.inputs[1U] = {.word = 0x0012U};
	const auto encoded = encode_new_classic_netplay_datagram(input);
	expect(encoded && encoded->size() == 104U &&
		(*encoded)[0U] == std::byte{'T'} &&
		(*encoded)[1U] == std::byte{'3'} &&
		(*encoded)[2U] == std::byte{'N'} &&
		(*encoded)[3U] == std::byte{'P'} &&
		(*encoded)[4U] == std::byte{3U} &&
		(*encoded)[5U] == std::byte{3U} &&
		(*encoded)[6U] == std::byte{1U} &&
		(*encoded)[7U] == std::byte{2U} &&
		(*encoded)[40U] == std::byte{0x25U} &&
		(*encoded)[41U] == std::byte{0x0FU},
		"T3NP input datagram has frozen header and newest-first history bytes");
	if(encoded) {
		const auto decoded = decode_new_classic_netplay_datagram(*encoded);
		expect(decoded && decoded->type == input.type &&
			decoded->sender == input.sender &&
			decoded->input_count == input.input_count &&
			decoded->session_id == input.session_id &&
			decoded->contract_hash == input.contract_hash &&
			decoded->latest_frame == input.latest_frame &&
			decoded->confirmed_frame_count == input.confirmed_frame_count &&
			decoded->inputs[0U] == input.inputs[0U] &&
			decoded->inputs[1U] == input.inputs[1U],
			"T3NP datagram round-trips exact session, contract, and history");
		auto corrupt = *encoded;
		corrupt[0U] = std::byte{0U};
		const auto bad_magic = decode_new_classic_netplay_datagram(corrupt);
		expect(!bad_magic && bad_magic.error().code ==
			NewClassicNetplayDatagramErrorCode::invalid_magic,
			"unknown UDP traffic cannot enter the netplay protocol");
		corrupt = *encoded;
		corrupt[43U] = std::byte{1U};
		const auto bad_reserved = decode_new_classic_netplay_datagram(corrupt);
		expect(!bad_reserved && bad_reserved.error().code ==
			NewClassicNetplayDatagramErrorCode::invalid_input &&
			bad_reserved.error().input_index == 0U,
			"nested T3NET001 reserved bytes remain strict in UDP history");
	} else {
		expect(false, "valid datagram reaches decode checks");
		expect(false, "valid datagram reaches corruption checks");
		expect(false, "valid datagram reaches nested input checks");
	}

	NewClassicNetplayDatagram hello{
		.type = NewClassicNetplayDatagramType::hello,
		.sender = NewClassicNetplayPlayer::player_two,
		.contract_hash = 1U,
	};
	expect(encode_new_classic_netplay_datagram(hello).has_value(),
		"join hello is the sole session-zero packet");
	hello.session_id = 1U;
	const auto bad_hello = encode_new_classic_netplay_datagram(hello);
	expect(!bad_hello && bad_hello.error().code ==
		NewClassicNetplayDatagramErrorCode::invalid_session,
		"hello with a peer-selected session fails closed");
	auto bad = input;
	bad.input_count = 17U;
	const auto bad_count = encode_new_classic_netplay_datagram(bad);
	expect(!bad_count && bad_count.error().code ==
		NewClassicNetplayDatagramErrorCode::invalid_history,
		"history above the fixed redundant window fails closed");

	NewClassicNetplayDatagram digest{
		.type = NewClassicNetplayDatagramType::simulation_digest,
		.sender = NewClassicNetplayPlayer::player_one,
		.session_id = 0x0102030405060708ULL,
		.contract_hash = 0x8877665544332211ULL,
		.latest_frame = 120U,
		.confirmed_frame_count = 120U,
	};
	for(std::size_t index = 0U;
		index < digest.simulation_digest.size(); ++index) {
		digest.simulation_digest[index] = static_cast<std::uint8_t>(index + 1U);
	}
	const auto encoded_digest = encode_new_classic_netplay_datagram(digest);
	expect(encoded_digest && (*encoded_digest)[5U] == std::byte{5U} &&
		(*encoded_digest)[24U] == std::byte{120U} &&
		(*encoded_digest)[32U] == std::byte{120U} &&
		(*encoded_digest)[40U] == std::byte{1U} &&
		(*encoded_digest)[71U] == std::byte{32U},
		"simulation digest reuses the bounded history payload exactly");
	const auto decoded_digest = encoded_digest ?
		decode_new_classic_netplay_datagram(*encoded_digest) :
		std::expected<NewClassicNetplayDatagram,
			NewClassicNetplayDatagramError>{
				std::unexpected(NewClassicNetplayDatagramError{})
			};
	expect(decoded_digest && decoded_digest->type == digest.type &&
		decoded_digest->latest_frame == 120U &&
		decoded_digest->confirmed_frame_count == 120U &&
		decoded_digest->simulation_digest == digest.simulation_digest,
		"simulation digest packet round-trips frame identity and SHA-256");
	digest.type = NewClassicNetplayDatagramType::simulation_digest_ack;
	const auto encoded_ack = encode_new_classic_netplay_datagram(digest);
	const auto decoded_ack = encoded_ack ?
		decode_new_classic_netplay_datagram(*encoded_ack) :
		std::expected<NewClassicNetplayDatagram,
			NewClassicNetplayDatagramError>{
				std::unexpected(NewClassicNetplayDatagramError{})
			};
	expect(decoded_ack && decoded_ack->type ==
		NewClassicNetplayDatagramType::simulation_digest_ack &&
		decoded_ack->latest_frame == digest.latest_frame &&
		decoded_ack->simulation_digest == digest.simulation_digest,
		"digest acknowledgement freezes the same bounded proof payload");
	digest.latest_frame = 0U;
	digest.confirmed_frame_count = 0U;
	const auto zero_frame_ack = encode_new_classic_netplay_datagram(digest);
	expect(!zero_frame_ack && zero_frame_ack.error().code ==
		NewClassicNetplayDatagramErrorCode::invalid_digest,
		"frame-zero proof acknowledgement is not a valid runtime message");
	digest.type = NewClassicNetplayDatagramType::simulation_digest;
	const auto initial_digest = encode_new_classic_netplay_datagram(digest);
	expect(static_cast<bool>(initial_digest),
		"frame-zero digest keeps both proof identities zero");
	digest.simulation_digest = {};
	const auto zero_digest = encode_new_classic_netplay_datagram(digest);
	expect(!zero_digest && zero_digest.error().code ==
		NewClassicNetplayDatagramErrorCode::invalid_digest,
		"all-zero simulation identity fails closed");
}

} // namespace

int main()
{
	test_wire_vectors();
	test_validation_and_composition();
	test_datagram_contract();
	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return 1;
	}
	std::printf("all %zu New Classic netplay input checks passed\n", checks);
	return 0;
}
