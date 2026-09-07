#include "th03/present/staff_flakes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string_view>

namespace {

using th03::present::Index4;
using th03::present::PackedRowOrder;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;
using th03::present::StaffFlake;
using th03::present::StaffFlakeMasks;
using th03::present::StaffFlakeState;
using th03::present::StaffFlakeTrigTables;

int failures = 0;

void expect(bool condition, std::string_view message) {
	if(!condition) {
		std::fprintf(
			stderr,
			"FAIL: %.*s\n",
			static_cast<int>(message.size()),
			message.data()
		);
		++failures;
	}
}

void expect_pixel(
	const Raster &raster,
	Page page,
	Point point,
	Index4 expected,
	std::string_view message
) {
	expect(raster.pixel(page, point) == expected, message);
}

struct SyntheticInputs {
	std::array<std::uint8_t, th03::present::staff_flake_mask_bytes> masks{};
	std::array<std::int16_t, 256U> sine{};
	std::array<std::int16_t, 256U> cosine{};

	[[nodiscard]] StaffFlakeMasks mask_view() const noexcept {
		return {masks, PackedRowOrder::top_to_bottom};
	}

	[[nodiscard]] StaffFlakeTrigTables trig_view() const noexcept {
		return {sine, cosine};
	}
};

[[nodiscard]] bool same_flake(
	const StaffFlake &left,
	const StaffFlake &right
) noexcept {
	return left.alive == right.alive &&
		left.left_q4 == right.left_q4 &&
		left.top_q4 == right.top_q4 &&
		left.velocity_x_q4 == right.velocity_x_q4 &&
		left.velocity_y_q4 == right.velocity_y_q4 && left.cel == right.cel;
}

[[nodiscard]] bool same_state(
	const StaffFlakeState &left,
	const StaffFlakeState &right
) noexcept {
	if(left.random_seed != right.random_seed || left.frame != right.frame ||
		left.active_count != right.active_count ||
		left.back_page != right.back_page ||
		left.reset_pending != right.reset_pending ||
		left.cdg_put_alpha != right.cdg_put_alpha) {
		return false;
	}
	for(std::size_t i = 0U; i < left.flakes.size(); ++i) {
		if(!same_flake(left.flakes[i], right.flakes[i])) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] std::uint16_t reference_irand(std::uint32_t &seed) noexcept {
	seed = (seed * 0x015A4E35U) + 1U;
	return static_cast<std::uint16_t>((seed >> 16U) & 0x7FFFU);
}

void test_initialization_reset_and_frame_helpers() {
	constexpr std::uint32_t resident_seed = 0x89ABCDEFU;
	StaffFlakeState state(resident_seed);
	expect(state.random_seed == resident_seed, "resident seed is preserved");
	expect(state.frame == 0, "staff-roll frame starts at zero");
	expect(
		state.active_count == th03::present::staff_flake_count,
		"all 80 records initially participate"
	);
	expect(state.back_page == Page::zero, "initial back page is page zero");
	expect(state.reset_pending, "initial overrun reset is pending");
	expect(state.cdg_put_alpha, "initial CDG alpha puts are enabled");
	for(const auto &flake : state.flakes) {
		expect(!flake.alive, "all initialized records are dead");
	}

	state.flakes.front() = {true, 11, 22, 33, 44, 3U};
	state.flakes.back() = {true, 55, 66, 77, 88, 2U};
	th03::present::staff_flakes_reset_records(state);
	expect(!state.flakes.front().alive, "record reset clears first alive flag");
	expect(!state.flakes.back().alive, "record reset clears last alive flag");
	expect(
		state.flakes.front().left_q4 == 11 &&
			state.flakes.front().velocity_y_q4 == 44 &&
			state.flakes.front().cel == 3U,
		"record reset preserves non-alive bytes"
	);

	state.frame = 123;
	th03::present::staff_flakes_restart_phase(state);
	expect(state.frame == 0, "phase restart assigns frame zero only");
	state.frame = 32767;
	th03::present::staff_flakes_advance_frame(state);
	expect(state.frame == -32768, "frame advance wraps as a source word");
}

void test_first_spawn_rng_and_vector() {
	SyntheticInputs input{};
	input.sine.fill(128);
	input.cosine.fill(-256);
	StaffFlakeState state(0x12345678U);
	Raster raster;

	expect(
		th03::present::staff_flakes_tick(
			raster, state, input.mask_view(), input.trig_view(), false
		),
		"first staff flake tick succeeds"
	);
	const auto &flake = state.flakes[0];
	expect(flake.alive, "frame zero spawns record zero");
	expect(!state.flakes[1].alive, "frame zero does not spawn record one");
	expect(state.random_seed == 0x9EFC21B4U, "spawn consumes four exact IRANDs");
	expect(flake.left_q4 == 10048, "right-edge spawn updates left by cosine");
	expect(flake.top_q4 == 5502, "edge spawn updates random top by sine");
	expect(flake.velocity_x_q4 == -64, "vector X uses signed Q8 cosine");
	expect(flake.velocity_y_q4 == 32, "vector Y arithmetic-shifts Q8 sine");
	expect(flake.cel == 0U, "cel uses the fourth random value");
	expect(state.frame == 0, "tick leaves driver-owned frame unchanged");

	SyntheticInputs fractional{};
	fractional.sine.fill(-257);
	fractional.cosine.fill(-1);
	StaffFlakeState fractional_state(0x12345678U);
	Raster fractional_raster;
	expect(
		th03::present::staff_flakes_tick(
			fractional_raster,
			fractional_state,
			fractional.mask_view(),
			fractional.trig_view(),
			false
		),
		"fractional negative-vector tick succeeds"
	);
	expect(
		fractional_state.flakes[0].velocity_x_q4 == -1,
		"negative vector products use arithmetic right shift"
	);
	expect(
		fractional_state.flakes[0].velocity_y_q4 == -65,
		"negative nonintegral Q8 product rounds toward negative infinity"
	);
}

void test_all_eighty_spawn_records_and_rng_order() {
	SyntheticInputs input{};
	input.sine.fill(256);
	input.cosine.fill(-256);
	constexpr std::uint32_t resident_seed = 0x0BADCAFEU;
	StaffFlakeState state(resident_seed);
	state.frame = 632;
	Raster raster;

	expect(
		th03::present::staff_flakes_tick(
			raster, state, input.mask_view(), input.trig_view(), false
		),
		"80-record spawn tick succeeds"
	);

	auto expected_seed = resident_seed;
	for(std::size_t i = 0U; i < th03::present::staff_flake_count; ++i) {
		const auto position_random = reference_irand(expected_seed);
		(void)reference_irand(expected_seed);
		const auto length_random = reference_irand(expected_seed);
		const auto cel_random = reference_irand(expected_seed);
		const auto length = static_cast<std::int16_t>(
			(length_random % 64U) + 48U
		);
		auto expected_left = ((i & 3U) != 0U)
			? static_cast<std::int16_t>(position_random % 10112U)
			: th03::present::staff_flake_left_max_q4;
		auto expected_top = ((i & 3U) != 0U)
			? static_cast<std::int16_t>(0)
			: static_cast<std::int16_t>(position_random % 6272U);
		expected_left = static_cast<std::int16_t>(expected_left - length);
		expected_top = static_cast<std::int16_t>(expected_top + length);
		if(expected_left <= 0) {
			expected_left = static_cast<std::int16_t>(
				expected_left + th03::present::staff_flake_left_max_q4
			);
		}
		if(expected_top >= th03::present::staff_flake_top_max_q4) {
			expected_top = static_cast<std::int16_t>(
				expected_top - th03::present::staff_flake_top_max_q4
			);
		}

		const auto &flake = state.flakes[i];
		expect(flake.alive, "eligible record is spawned");
		expect(flake.left_q4 == expected_left, "record left follows RNG order");
		expect(flake.top_q4 == expected_top, "record top follows RNG order");
		expect(flake.velocity_x_q4 == -length, "record cosine velocity matches");
		expect(flake.velocity_y_q4 == length, "record sine velocity matches");
		expect(
			flake.cel == static_cast<std::uint8_t>(cel_random & 3U),
			"record cel follows fourth RNG read"
		);
	}
	expect(state.random_seed == expected_seed, "80 spawns consume 320 IRANDs");
}

void test_update_wrap_rules() {
	SyntheticInputs input{};
	StaffFlakeState state(1U);
	state.frame = -1;
	state.active_count = 5U;
	state.reset_pending = false;
	state.flakes[0] = {true, 1, 100, -2, 0, 0U};
	state.flakes[1] = {true, 0, 100, 0, 0, 0U};
	state.flakes[2] = {true, 100, 6271, 0, 2, 0U};
	state.flakes[3] = {true, 100, -1, 0, 0, 0U};
	state.flakes[4] = {true, 32767, 32767, 1, 1, 0U};
	Raster raster;

	expect(
		th03::present::staff_flakes_tick(
			raster, state, input.mask_view(), input.trig_view(), false
		),
		"wrap-rule tick succeeds"
	);
	expect(state.flakes[0].left_q4 == 10111, "left <= 0 wraps once");
	expect(state.flakes[1].left_q4 == 10112, "left zero wraps to maximum");
	expect(state.flakes[2].top_q4 == 1, "top >= maximum wraps once");
	expect(state.flakes[3].top_q4 == -1, "negative top is not wrapped");
	expect(
		state.flakes[4].left_q4 == -22656,
		"word overflow occurs before one left correction"
	);
	expect(
		state.flakes[4].top_q4 == -32768,
		"wrapped-negative top bypasses the upper-bound correction"
	);
}

void test_blue_clear_and_tick_page_order() {
	SyntheticInputs input{};
	input.masks[0U] = 0x80U;
	Raster clear;
	expect(clear.plot({1, 1}, 1U), "seed blue-only pixel");
	expect(clear.plot({2, 1}, 3U), "seed blue-red pixel");
	expect(clear.plot({3, 1}, 15U), "seed white pixel");
	expect(clear.plot({4, 1}, 14U), "seed non-blue pixel");
	expect(clear.set_access_page(Page::one), "select page one for clear seed");
	expect(clear.plot({1, 1}, 9U), "seed page-one blue pixel");
	expect(clear.set_access_page(Page::zero), "restore page zero for clear");
	th03::present::staffroll_blue_plane_clear(clear);
	expect_pixel(clear, Page::zero, {1, 1}, 0U, "clear removes blue-only bit");
	expect_pixel(clear, Page::zero, {2, 1}, 2U, "clear preserves red plane");
	expect_pixel(clear, Page::zero, {3, 1}, 14U, "clear preserves RGE planes");
	expect_pixel(clear, Page::zero, {4, 1}, 14U, "clear keeps non-blue color");
	expect_pixel(clear, Page::one, {1, 1}, 9U, "clear keeps other page");

	StaffFlakeState state(1U);
	state.frame = -1;
	state.active_count = 1U;
	state.reset_pending = false;
	state.flakes[0] = {true, 256, 160, 0, 0, 0U};
	Raster raster;
	expect(raster.set_shown_page(Page::one), "seed source initial shown page");
	expect(
		th03::present::staff_flakes_tick(
			raster, state, input.mask_view(), input.trig_view(), false
		),
		"first page-order tick succeeds"
	);
	expect_pixel(raster, Page::zero, {16, 10}, 1U, "tick renders old access page");
	expect(raster.shown_page() == Page::zero, "tick shows old back page");
	expect(state.back_page == Page::one, "tick toggles back-page state");
	expect(raster.access_page() == Page::one, "tick accesses toggled page");

	expect(
		th03::present::staff_flakes_tick(
			raster, state, input.mask_view(), input.trig_view(), false
		),
		"second page-order tick succeeds"
	);
	expect_pixel(raster, Page::one, {16, 10}, 1U, "second tick renders page one");
	expect(raster.shown_page() == Page::one, "second tick shows page one");
	expect(state.back_page == Page::zero, "second tick toggles to page zero");
	expect(raster.access_page() == Page::zero, "second tick accesses page zero");
}

void test_overrun_reset_happens_after_render() {
	SyntheticInputs input{};
	input.masks[0U] = 0x80U;
	StaffFlakeState state(1U);
	state.frame = -1;
	state.flakes[79] = {true, 1600, 320, 0, 0, 0U};
	Raster raster;

	expect(
		th03::present::staff_flakes_tick(
			raster, state, input.mask_view(), input.trig_view(), true
		),
		"overrun-reset tick succeeds"
	);
	expect_pixel(
		raster,
		Page::zero,
		{100, 20},
		1U,
		"record 79 renders before active count shrinks"
	);
	expect(
		state.active_count == th03::present::staff_flake_count_after_overrun,
		"overrun reset reduces active records to 50"
	);
	expect(!state.reset_pending, "overrun reset clears its pending flag");
	expect(!state.cdg_put_alpha, "overrun reset disables CDG alpha puts");
	expect(
		th03::present::staff_flakes_tick(
			raster, state, input.mask_view(), input.trig_view(), false
		),
		"post-reset tick succeeds"
	);
	expect_pixel(
		raster,
		Page::one,
		{100, 20},
		0U,
		"record 79 no longer updates or renders after reset"
	);
}

void test_invalid_inputs_are_atomic() {
	SyntheticInputs input{};
	StaffFlakeState state(0x13579BDFU);
	state.frame = 8;
	Raster raster;
	expect(raster.plot({2, 3}, 7U), "seed invalid-input raster");

	auto expect_rejected = [&](const StaffFlakeMasks &masks,
		const StaffFlakeTrigTables &trig, std::string_view message) {
		const auto state_before = state;
		const auto hash_before = raster.display_state_hash();
		const auto shown_before = raster.shown_page();
		const auto access_before = raster.access_page();
		expect(
			!th03::present::staff_flakes_tick(
				raster, state, masks, trig, false
			),
			message
		);
		expect(same_state(state, state_before), "invalid tick preserves state");
		expect(
			raster.display_state_hash() == hash_before &&
				raster.shown_page() == shown_before &&
				raster.access_page() == access_before,
			"invalid tick preserves raster and page selection"
		);
	};

	const StaffFlakeMasks short_masks{
		std::span<const std::uint8_t>(input.masks).first(input.masks.size() - 1U),
		PackedRowOrder::top_to_bottom,
	};
	expect_rejected(short_masks, input.trig_view(), "short masks are rejected");
	const StaffFlakeMasks wrong_order{
		input.masks,
		PackedRowOrder::bottom_to_top,
	};
	expect_rejected(wrong_order, input.trig_view(), "wrong mask order is rejected");
	const StaffFlakeTrigTables short_trig{
		std::span<const std::int16_t>(input.sine).first(255U),
		input.cosine,
	};
	expect_rejected(input.mask_view(), short_trig, "short trig table is rejected");

	state.active_count = 81U;
	expect_rejected(
		input.mask_view(), input.trig_view(), "oversized active count is rejected"
	);
	state.active_count = 1U;
	state.flakes[0].alive = true;
	state.flakes[0].cel = 4U;
	expect_rejected(
		input.mask_view(), input.trig_view(), "invalid live cel is rejected"
	);
	state.flakes[0].cel = 0U;
	state.back_page = static_cast<Page>(0xFFU);
	expect_rejected(
		input.mask_view(), input.trig_view(), "invalid back page is rejected"
	);
	state.back_page = Page::zero;
	expect(raster.set_access_page(Page::one), "create access/back-page mismatch");
	expect_rejected(
		input.mask_view(),
		input.trig_view(),
		"access/back-page mismatch is rejected"
	);
}

} // namespace

int main() {
	test_initialization_reset_and_frame_helpers();
	test_first_spawn_rng_and_vector();
	test_all_eighty_spawn_records_and_rng_order();
	test_update_wrap_rules();
	test_blue_clear_and_tick_page_order();
	test_overrun_reset_happens_after_render();
	test_invalid_inputs_are_atomic();
	if(failures == 0) {
		std::fputs("th03 staff-roll flake tests passed\n", stdout);
	}
	return failures == 0 ? 0 : 1;
}
