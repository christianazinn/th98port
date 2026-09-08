#include "th02/core/shot.hpp"

#include "pc98/masterlib_vector.hpp"

#include <array>
#include <bit>
#include <cstdlib>
#include <cstddef>
#include <cstdint>

namespace {
using namespace th02::core;

[[nodiscard]] std::int16_t add_word(std::int16_t lhs, std::int16_t rhs)
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(lhs)) +
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(rhs))
	));
}

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels)
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	));
}

[[nodiscard]] std::int16_t sar4(std::int32_t value)
{
	auto shifted = std::bit_cast<std::uint32_t>(value) >> 4U;
	if(value < 0) shifted |= 0xF0000000U;
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(shifted));
}

[[nodiscard]] PlayerShotPoolState patterned_state()
{
	PlayerShotPoolState state{};
	for(std::size_t i = 0; i < state.shots.size(); ++i) {
		state.shots[i] = {
			static_cast<std::uint8_t>(i + 3U),
			static_cast<std::uint8_t>(i + 7U),
			{{
				{static_cast<std::int16_t>(100 + static_cast<int>(i)),
					static_cast<std::int16_t>(200 + static_cast<int>(i))},
				{static_cast<std::int16_t>(300 + static_cast<int>(i)),
					static_cast<std::int16_t>(400 + static_cast<int>(i))},
			}},
			{static_cast<std::int16_t>(500 + static_cast<int>(i)),
				static_cast<std::int16_t>(600 + static_cast<int>(i))},
			static_cast<std::uint8_t>(i + 11U), (i & 1U) != 0U,
		};
	}
	state.current_slot = 7;
	state.spawn_top = -320;
	state.patnum = 0x34;
	state.option_patnum = 0x3F;
	return state;
}

[[nodiscard]] bool test_regular_writer()
{
	auto state = patterned_state();
	const auto before = state;
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	if(!shot_add(state, positions, 1U, -24, 64U)) return false;
	const auto &shot = state.shots[7];
	auto expected = before;
	expected.shots[7].flag = player_shot_alive;
	expected.shots[7].decay_cel = 0;
	expected.shots[7].pos_on_page[1] = {to_subpixels(76), -320};
	expected.shots[7].velocity = {0, 192};
	expected.shots[7].patnum = 0x34;
	expected.shots[7].from_option = false;
	return shot == expected.shots[7] && state == expected;
}

[[nodiscard]] bool test_regular_angle_and_word_widths()
{
	for(unsigned raw_angle = 0U; raw_angle < 256U; ++raw_angle) {
		auto state = patterned_state();
		state.current_slot = 0;
		const auto angle = static_cast<std::uint8_t>(raw_angle);
		if(!shot_add(state, {{32760, 0}, {}}, 0U, 8, angle)) return false;
		const auto &shot = state.shots[0];
		const ShotSubpixelPoint source_velocity{
			sar4(static_cast<std::int32_t>(pc98::masterlib::cosine8(angle)) * 12),
			sar4(static_cast<std::int32_t>(pc98::masterlib::sine8(angle)) * 12),
		};
		const auto shared_velocity = pc98::masterlib::vector2(angle, 192);
		if(shot.pos_on_page[0].x != to_subpixels(add_word(32760, 8)) ||
			shot.velocity != source_velocity ||
			shot.velocity != ShotSubpixelPoint{shared_velocity.x, shared_velocity.y}) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool test_option_writer()
{
	auto state = patterned_state();
	const auto before = state;
	const PlayerShotWriterPositions positions{{100, 200}, {32760, -2048}};
	const ShotSubpixelPoint velocity{-160, 160};
	if(!shot_option_add(state, positions, 0U, 8, velocity)) return false;
	const auto &shot = state.shots[7];
	auto expected = before;
	expected.shots[7].flag = player_shot_alive;
	expected.shots[7].decay_cel = 1;
	expected.shots[7].pos_on_page[0] = {
		to_subpixels(add_word(32760, 8)), to_subpixels(-2048)};
	expected.shots[7].velocity = velocity;
	expected.shots[7].patnum = 0x3F;
	expected.shots[7].from_option = true;
	return shot == expected.shots[7] && state == expected;
}

[[nodiscard]] bool test_invalid_inputs_do_not_mutate()
{
	const PlayerShotWriterPositions positions{{100, 200}, {300, 400}};
	for(const std::int16_t slot : std::array<std::int16_t, 3>{-1, 38, 32767}) {
		auto state = patterned_state();
		state.current_slot = slot;
		const auto before = state;
		if(shot_add(state, positions, 0U, 0, 0U) || state != before) return false;
		if(shot_option_add(state, positions, 0U, 0, {}) || state != before) return false;
	}
	for(const std::size_t page : std::array<std::size_t, 2>{2U, 31U}) {
		auto state = patterned_state();
		const auto before = state;
		if(shot_add(state, positions, page, 0, 0U) || state != before) return false;
		if(shot_option_add(state, positions, page, 0, {}) || state != before) return false;
	}
	return true;
}

[[nodiscard]] bool test_free_all_preserves_payload()
{
	auto state = patterned_state();
	const auto before = state;
	shots_free_all(state);
	for(std::size_t i = 0; i < state.shots.size(); ++i) {
		if(state.shots[i].flag != player_shot_free) return false;
		auto expected = before.shots[i];
		expected.flag = player_shot_free;
		if(state.shots[i] != expected) return false;
	}
	return state.current_slot == before.current_slot && state.spawn_top == before.spawn_top &&
		state.patnum == before.patnum && state.option_patnum == before.option_patnum;
}

} // namespace

int main()
{
	return (test_regular_writer() && test_regular_angle_and_word_widths() &&
		test_option_writer() && test_invalid_inputs_do_not_mutate() &&
		test_free_all_preserves_payload()) ? EXIT_SUCCESS : EXIT_FAILURE;
}
