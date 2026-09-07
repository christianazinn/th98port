#include "th02/core/random.hpp"
#include "pc98/random.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

namespace {
int failures{};
void check(bool condition, const char *message)
{
	if(!condition) { ++failures; std::cerr << message << '\n'; }
}
}

int main()
{
	using namespace th02::core;
	for(const auto seed : std::array<std::uint32_t, 6>{
		0U, 1U, 0xFFFFFFFFU, 0xFFFF0000U, 0xFFFFU, 0xE9B35FBAU}) {
		for(unsigned cursor = 0; cursor < 256U; ++cursor) {
			RandomState state;
			state.seed = seed;
			state.cursor = static_cast<std::uint8_t>(cursor);
			state.ring.fill(0xCD);
			std::array<std::uint8_t, 256> expected;
			std::uint64_t reference = seed;
			for(auto &value : expected) {
				reference = ((reference * 0x015A4E35ULL) + 1U) & 0xFFFFFFFFULL;
				value = static_cast<std::uint8_t>((reference >> 16U) & 0xFFU);
			}
			randring_fill(state);
			check(state.ring == expected, "Fill writes every byte in forward IRAND order");
			check(state.seed == reference, "Fill consumes exactly 256 IRAND calls");
			check(state.cursor == cursor, "Fill preserves every possible cursor value");
		}
	}

	// Model the source's 257-byte adjacent memory and MOV/INC/MOV read order.
	// Every starting cursor is covered, including words at 254/255, repeated
	// wraparound, and alternating byte/word callers sharing the same cursor.
	for(unsigned start = 0; start < 256U; ++start) {
		RandomState state;
		state.seed = 0xDEADBEEFU;
		std::array<std::uint8_t, 257> memory{};
		for(std::size_t i = 0; i < state.ring.size(); ++i) {
			state.ring[i] = static_cast<std::uint8_t>((i * 73U) + 17U);
			memory[i] = state.ring[i];
		}
		state.cursor = memory[256] = static_cast<std::uint8_t>(start);
		const auto ring_before = state.ring;
		for(unsigned sample = 0; sample < 1024U; ++sample) {
			const std::size_t address = memory[256];
			memory[256] = static_cast<std::uint8_t>(memory[256] + 1U);
			if(sample % 3U != 0U) {
				const auto expected = static_cast<std::uint16_t>(memory[address] |
					(static_cast<unsigned>(memory[address + 1U]) << 8U));
				check(randring_next16(state) == expected, "Word read follows adjacent memory");
			} else {
				check(randring_next8(state) == memory[address], "Byte read uses saved cursor");
			}
			check(state.cursor == memory[256], "Either width advances the cursor once");
		}
		check(state.seed == 0xDEADBEEFU && state.ring == ring_before,
			"Ring reads never advance IRAND or mutate ring bytes");
	}
	RandomState boundary;
	boundary.ring[0] = 0xEE;
	boundary.ring[255] = 0xAB;
	boundary.cursor = 255;
	check(randring_next16(boundary) == 0x00AB && boundary.cursor == 0,
		"Final word high byte is postincrement cursor 0, not first ring byte");

	// First normalized-demo RNG row captured with the historical PC-98
	// oracle d5ddbfdf9dc413c3a46f6964b0be1b554f086aea on 2026-09-07.
	// Seed 18 -> 64 sparks (two IRAND calls each) -> forward ring fill -> one
	// item initialization byte read. This checks only this RNG payload;
	// it does not implement or verify the surrounding startup schedule.
	RandomState guest;
	guest.seed = 18;
	for(int i = 0; i < 128; ++i) { (void)pc98::irand(guest.seed); }
	check(guest.seed == 0x6976AF92U, "Guest pre-fill seed after spark initialization");
	randring_fill(guest);
	check(randring_next8(guest) == 0x64 && guest.ring[255] == 0xF3 &&
		guest.seed == 0x1EF30E92U && guest.cursor == 1, "Guest first-row RNG scalars");
	std::uint32_t pass_a = 0x811C9DC5U;
	std::uint32_t pass_b = 0x7EE3623AU;
	std::uint16_t offset{};
	const auto byte = [&](std::uint8_t value) {
		pass_a = (pass_a ^ value) * 0x01000193U;
		pass_b = (pass_b ^ static_cast<std::uint8_t>(value ^
			static_cast<std::uint8_t>(offset))) * 0x01000193U;
		++offset;
	};
	for(unsigned shift = 0; shift < 32U; shift += 8U) {
		byte(static_cast<std::uint8_t>(guest.seed >> shift));
	}
	for(const auto value : guest.ring) byte(value);
	byte(guest.cursor);
	// The wire stores B then A; the TSV renders the little-endian pair as
	// one 64-bit integer, placing A in the high 32 bits.
	const auto observed_hash = (static_cast<std::uint64_t>(pass_a) << 32U) | pass_b;
	check(offset == 261 && observed_hash == 0xE7775247A8E61EEEULL,
		"Native complete ring payload matches the independently captured PC-98 row");
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
