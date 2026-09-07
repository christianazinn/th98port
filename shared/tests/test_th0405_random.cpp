#include "pc98/th0405_random.hpp"

#include <array>
#include <cstdlib>
#include <iostream>

namespace {
using namespace pc98::th0405;

// Byte-addressed instruction model: ring immediately followed by WORD cursor.
std::uint16_t draw(std::array<std::uint8_t, 258> &memory)
{
	const unsigned bx = memory[256] + 256U * memory[257];
	const auto ax = static_cast<std::uint16_t>(memory[bx] + 256U * memory[bx + 1U]);
	memory[256] = static_cast<std::uint8_t>((memory[256] + 1U) % 256U);
	return ax;
}

bool test_fill()
{
	for(auto seed : std::array<std::uint32_t, 6>{0, 1, 18, 0xFFFF, 0x12345678, 0xFFFFFFFF}) {
		RandomState state{seed, {}, 0xBEEF};
		std::array<std::uint8_t, 256> expected{};
		std::uint64_t wide_seed = seed;
		for(std::size_t i = 256; i > 0; --i) {
			wide_seed = (wide_seed * 22695477ULL + 1ULL) % 4294967296ULL;
			expected[i - 1U] = static_cast<std::uint8_t>((wide_seed / 65536ULL) % 256ULL);
		}
		randring_fill(state);
		if(state.ring != expected || state.seed != wide_seed || state.cursor != 0) return false;
	}
	return true;
}

bool test_draws()
{
	RandomState state{};
	std::array<std::uint8_t, 258> memory{};
	for(unsigned i = 0; i < 256; ++i) {
		state.ring[i] = memory[i] = static_cast<std::uint8_t>((i * 73U + 19U) % 256U);
	}
	const auto ring = state.ring;
	for(unsigned i = 0; i < 65536; ++i) {
		const auto expected = draw(memory);
		if(randring_next16(state) != expected || state.cursor != memory[256] ||
			state.seed != 0 || state.ring != ring) return false;
	}
	// Prevent substituting the TH02/03 post-increment spill or wrapping to ring[0].
	state.cursor = 255;
	state.ring[255] = 0x42;
	if(randring_next16(state) != 0xFF42 || state.cursor != 0) return false;

	// All word values with masks/divisors covering high bits and non-powers of 2.
	for(unsigned word = 0; word < 65536; ++word) {
		state.ring[17] = static_cast<std::uint8_t>(word);
		state.ring[18] = static_cast<std::uint8_t>(word >> 8U);
		for(auto operand : std::array<std::uint16_t, 10>{0, 1, 2, 3, 7, 255, 256, 32768, 50000, 65535}) {
			state.cursor = 17;
			if(randring_next16_and(state, operand) != (word & operand) || state.cursor != 18) return false;
			state.cursor = 17;
			const auto remainder = randring_next16_mod(state, operand);
			if(state.cursor != 18) return false;
			if(operand == 0 ? remainder.has_value() : remainder != (word % operand)) return false;
		}
	}
	// Unsupported high cursor bytes must not be silently reduced modulo 256.
	for(unsigned cursor = 256; cursor < 65536; ++cursor) {
		state.cursor = static_cast<std::uint16_t>(cursor);
		const auto before = state;
		if(randring_next16(state) || randring_next16_and(state, 255) ||
			randring_next16_mod(state, 3) || state != before) return false;
	}
	return true;
}

bool test_guest_start_rng()
{
	// Captured c6c2ee53 normalized demo-1 starts, TH04 and TH05. This only
	// checks the RNG group, not the other initialized subsystems or schedule.
	RandomState state{318, {}, 0};
	randring_fill(state);
	// items_init: one IRAND; sparks_init: 96, including in TH05 (not 64).
	for(unsigned i = 0; i < 97; ++i) static_cast<void>(pc98::irand(state.seed));
	if(state.seed != 0x0DF1B6F7U || state.cursor != 0) return false;
	std::array<std::uint8_t, 262> bytes{};
	for(unsigned i = 0; i < 4; ++i) bytes[i] = static_cast<std::uint8_t>(state.seed >> (8U * i));
	for(std::size_t i = 0; i < state.ring.size(); ++i) bytes[i + 4U] = state.ring[i];
	std::uint32_t a = 0x811C9DC5U;
	std::uint32_t b = 0x7EE3623AU;
	for(std::size_t i = 0; i < bytes.size(); ++i) {
		a = (a ^ bytes[i]) * 16777619U;
		b = (b ^ static_cast<std::uint8_t>(bytes[i] ^ static_cast<std::uint8_t>(i))) * 16777619U;
	}
	return a == 0x97EE20ACU && b == 0xCF449D40U;
}
}

int main()
{
	if(!test_fill() || !test_draws() || !test_guest_start_rng()) {
		std::cerr << "TH04/05 fill, word-read ordering, operand, or refusal mismatch\n";
		return EXIT_FAILURE;
	}
	std::cout << "TH04/05 reverse fill, pre-increment cursor spill, and word operations checked\n";
	return EXIT_SUCCESS;
}
