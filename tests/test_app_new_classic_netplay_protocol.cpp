#include "th03/app/new_classic_netplay_protocol.hpp"

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

} // namespace

int main()
{
	test_wire_vectors();
	test_validation_and_composition();
	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return 1;
	}
	std::printf("all %zu New Classic netplay input checks passed\n", checks);
	return 0;
}
