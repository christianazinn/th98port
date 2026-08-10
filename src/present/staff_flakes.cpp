#include "th03/present/staff_flakes.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace th03::present {
namespace {

constexpr std::size_t trig_entry_count = 256U;
constexpr std::uint8_t spawn_angle_base = 0x50U;
constexpr std::uint16_t spawn_angle_span = 0x20U;
constexpr std::uint16_t spawn_length_base_q4 = 3U * 16U;
constexpr std::uint16_t spawn_length_span_q4 = 4U * 16U;

[[nodiscard]] constexpr bool valid_page(Page page) noexcept {
	return page == Page::zero || page == Page::one;
}

[[nodiscard]] constexpr Page other_page(Page page) noexcept {
	return page == Page::zero ? Page::one : Page::zero;
}

[[nodiscard]] bool valid_inputs(
	const StaffFlakeState &state,
	const StaffFlakeMasks &masks,
	const StaffFlakeTrigTables &trig
) noexcept {
	if(state.active_count > staff_flake_count || !valid_page(state.back_page) ||
		masks.rows.size() != staff_flake_mask_bytes ||
		masks.row_order != PackedRowOrder::top_to_bottom ||
		trig.sine_q8.size() != trig_entry_count ||
		trig.cosine_q8.size() != trig_entry_count) {
		return false;
	}
	for(std::size_t i = 0U; i < state.active_count; ++i) {
		if(state.flakes[i].alive &&
			state.flakes[i].cel >= staff_flake_cel_count) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] constexpr std::int16_t wrap_i16(std::int32_t value) noexcept {
	const auto bits = static_cast<std::uint16_t>(value);
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] constexpr std::int16_t add_i16(
	std::int16_t left,
	std::int16_t right
) noexcept {
	return wrap_i16(
		static_cast<std::int32_t>(left) + static_cast<std::int32_t>(right)
	);
}

[[nodiscard]] constexpr std::int16_t arithmetic_shift_right_8(
	std::int32_t value
) noexcept {
	if(value >= 0) {
		return wrap_i16(value / 256);
	}
	const auto magnitude = -static_cast<std::int64_t>(value);
	return wrap_i16(static_cast<std::int32_t>(
		-((magnitude + 255) / 256)
	));
}

[[nodiscard]] constexpr std::int32_t q4_to_pixel(
	std::int16_t value
) noexcept {
	if(value >= 0) {
		return static_cast<std::int32_t>(value) / 16;
	}
	const auto magnitude = -static_cast<std::int32_t>(value);
	return -((magnitude + 15) / 16);
}

[[nodiscard]] std::uint16_t next_random(StaffFlakeState &state) noexcept {
	state.random_seed = (state.random_seed * 0x015A4E35U) + 1U;
	return static_cast<std::uint16_t>((state.random_seed >> 16U) & 0x7FFFU);
}

void spawn(
	StaffFlakeState &state,
	const StaffFlakeTrigTables &trig
) noexcept {
	for(std::size_t i = 0U; i < state.active_count; ++i) {
		auto &flake = state.flakes[i];
		const auto spawn_frame = static_cast<std::int32_t>(i) * 8;
		if(flake.alive || spawn_frame > state.frame) {
			continue;
		}

		flake.alive = true;
		if((i & 3U) != 0U) {
			flake.left_q4 = static_cast<std::int16_t>(
				next_random(state) %
				static_cast<std::uint16_t>(staff_flake_left_max_q4)
			);
			flake.top_q4 = 0;
		} else {
			flake.left_q4 = staff_flake_left_max_q4;
			flake.top_q4 = static_cast<std::int16_t>(
				next_random(state) %
				static_cast<std::uint16_t>(staff_flake_top_max_q4)
			);
		}

		const auto angle = static_cast<std::uint8_t>(
			(next_random(state) % spawn_angle_span) + spawn_angle_base
		);
		const auto length_q4 = static_cast<std::int16_t>(
			(next_random(state) % spawn_length_span_q4) +
			spawn_length_base_q4
		);
		flake.cel = static_cast<std::uint8_t>(
			next_random(state) & (staff_flake_cel_count - 1U)
		);
		const auto angle_index = static_cast<std::size_t>(angle);
		flake.velocity_x_q4 = arithmetic_shift_right_8(
			static_cast<std::int32_t>(length_q4) *
			static_cast<std::int32_t>(trig.cosine_q8[angle_index])
		);
		flake.velocity_y_q4 = arithmetic_shift_right_8(
			static_cast<std::int32_t>(length_q4) *
			static_cast<std::int32_t>(trig.sine_q8[angle_index])
		);
	}
}

void update(StaffFlakeState &state) noexcept {
	for(std::size_t i = 0U; i < state.active_count; ++i) {
		auto &flake = state.flakes[i];
		if(!flake.alive) {
			continue;
		}

		flake.left_q4 = add_i16(flake.left_q4, flake.velocity_x_q4);
		flake.top_q4 = add_i16(flake.top_q4, flake.velocity_y_q4);
		if(flake.left_q4 <= 0) {
			flake.left_q4 = add_i16(
				flake.left_q4,
				staff_flake_left_max_q4
			);
		}
		if(flake.top_q4 >= staff_flake_top_max_q4) {
			flake.top_q4 = add_i16(
				flake.top_q4,
				-static_cast<std::int16_t>(staff_flake_top_max_q4)
			);
		}
	}
}

void render(
	Raster &raster,
	const StaffFlakeState &state,
	const StaffFlakeMasks &masks
) noexcept {
	for(std::size_t i = 0U; i < state.active_count; ++i) {
		const auto &flake = state.flakes[i];
		if(!flake.alive) {
			continue;
		}
		(void)staff_flake_put_blue_or(
			raster,
			q4_to_pixel(flake.left_q4),
			q4_to_pixel(flake.top_q4),
			masks,
			flake.cel
		);
	}
}

} // namespace

StaffFlakeState::StaffFlakeState(std::uint32_t resident_seed) noexcept :
	random_seed(resident_seed)
{
	staff_flakes_reset_records(*this);
}

void staff_flakes_reset_records(StaffFlakeState &state) noexcept {
	for(auto &flake : state.flakes) {
		flake.alive = false;
	}
}

void staff_flakes_restart_phase(StaffFlakeState &state) noexcept {
	state.frame = 0;
}

void staff_flakes_advance_frame(StaffFlakeState &state) noexcept {
	state.frame = wrap_i16(static_cast<std::int32_t>(state.frame) + 1);
}

void staffroll_blue_plane_clear(Raster &raster) noexcept {
	(void)raster.clear_plane(PlanarPlane::blue);
}

bool staff_flakes_tick(
	Raster &raster,
	StaffFlakeState &state,
	const StaffFlakeMasks &masks,
	const StaffFlakeTrigTables &trig,
	bool vsync_count_above_one
) noexcept {
	if(raster.access_page() != state.back_page ||
		!valid_inputs(state, masks, trig)) {
		return false;
	}

	spawn(state, trig);
	update(state);
	render(raster, state, masks);
	if(state.reset_pending && vsync_count_above_one) {
		state.cdg_put_alpha = false;
		state.active_count = staff_flake_count_after_overrun;
		state.reset_pending = false;
	}

	const auto shown_page = state.back_page;
	const bool show_succeeded = raster.set_shown_page(shown_page);
	state.back_page = other_page(shown_page);
	const bool access_succeeded = raster.set_access_page(state.back_page);
	return show_succeeded && access_succeeded;
}

} // namespace th03::present
