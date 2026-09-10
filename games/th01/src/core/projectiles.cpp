#include "th01/core/projectiles.hpp"

#include "th01/core/state.hpp"
#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/random.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>

namespace th01::core {
namespace {

constexpr std::int16_t playfield_left = 0;
constexpr std::int16_t playfield_top = 64;
constexpr std::int16_t playfield_right = 640;
constexpr std::int16_t playfield_bottom = 400;
constexpr std::int16_t player_width = 32;
constexpr std::int16_t player_height = 32;
constexpr std::int16_t player_top = playfield_bottom - player_height;
constexpr std::int16_t pellet_width = 8;
constexpr std::int16_t pellet_height = 8;
constexpr std::int16_t missile_width = 16;
constexpr std::int16_t missile_height = 16;
constexpr std::int16_t pellet_velocity_max = 8 * pellet_subpixel_factor;
constexpr std::int16_t pellet_decay_frames = 20;
constexpr std::int16_t pellet_destroy_score = 10;
constexpr std::int16_t missile_new = -1;
constexpr std::int16_t shootout_laser_count = 10;

[[nodiscard]] constexpr std::int16_t word(std::int32_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] constexpr std::int16_t word_add(
	std::int16_t left, std::int16_t right
) noexcept
{
	return word(static_cast<std::int32_t>(left) + right);
}

[[nodiscard]] constexpr std::int16_t word_subtract(
	std::int16_t left, std::int16_t right
) noexcept
{
	return word(static_cast<std::int32_t>(left) - right);
}

[[nodiscard]] constexpr std::int16_t q12_4_to_pixel(
	std::int16_t value
) noexcept
{
	const auto wide = static_cast<std::int32_t>(value);
	return static_cast<std::int16_t>((wide >= 0)
		? (wide / pellet_subpixel_factor)
		: -(((-wide) + (pellet_subpixel_factor - 1)) / pellet_subpixel_factor));
}

[[nodiscard]] constexpr std::int32_t q24_8_to_pixel(
	std::int32_t value
) noexcept
{
	return (value >= 0) ? (value / 256) : -(((-value) + 255) / 256);
}

[[nodiscard]] constexpr std::int32_t q24_8_to_vram_byte_amount(
	std::int32_t value
) noexcept
{
	constexpr std::int32_t divisor = 256 * 8;
	return (value >= 0) ? (value / divisor) : -(((-value) + divisor - 1) / divisor);
}

[[nodiscard]] constexpr std::int16_t arithmetic_half(std::int16_t value) noexcept
{
	const auto wide = static_cast<std::int32_t>(value);
	return static_cast<std::int16_t>((wide >= 0) ? (wide / 2) : -(((-wide) + 1) / 2));
}

[[nodiscard]] constexpr std::int32_t arithmetic_shift_right_3(
	std::int32_t value
) noexcept
{
	return (value >= 0) ? (value / 8) : -(((-value) + 7) / 8);
}

[[nodiscard]] constexpr bool overlap_lt_gt(
	std::int16_t left,
	std::int16_t top,
	std::int16_t width,
	std::int16_t height,
	std::int16_t other_left,
	std::int16_t other_top,
	std::int16_t other_width,
	std::int16_t other_height
) noexcept
{
	return (
		left < word(static_cast<std::int32_t>(other_left) + other_width) &&
		word(static_cast<std::int32_t>(left) + width) > other_left &&
		top < word(static_cast<std::int32_t>(other_top) + other_height) &&
		word(static_cast<std::int32_t>(top) + height) > other_top
	);
}

[[nodiscard]] constexpr std::int16_t rank_speed_base(std::int16_t rank) noexcept
{
	switch(rank) {
	case 0: return 0;
	case 1: return 6;
	case 2: return 12;
	default: return 18;
	}
}

[[nodiscard]] std::uint8_t angle_to_player(
	const State &state,
	std::int16_t pellet_left,
	std::int16_t pellet_top,
	std::uint8_t plus_angle
) noexcept
{
	const auto y = word(static_cast<std::int32_t>(player_top + (player_height / 2)) - pellet_top);
	const auto x = word(static_cast<std::int32_t>(state.player.left + (player_width / 2) - (pellet_width / 2)) - pellet_left);
	const auto angle = pc98::masterlib::iatan2_source(y, x).value_or(0U);
	return static_cast<std::uint8_t>(angle + plus_angle);
}

[[nodiscard]] pc98::masterlib::Vector2 vector_to_player(
	const State &state,
	std::int16_t pellet_left,
	std::int16_t pellet_top,
	std::int16_t length,
	std::uint8_t plus_angle
) noexcept
{
	return pc98::masterlib::vector2(
		angle_to_player(state, pellet_left, pellet_top, plus_angle), length
	);
}

[[nodiscard]] bool group_velocity_set(
	State &state,
	PelletGroup group,
	std::int16_t speed,
	std::int16_t group_i,
	std::int16_t pellet_left,
	std::int16_t pellet_top,
	std::int16_t &velocity_x,
	std::int16_t &velocity_y
) noexcept
{
	bool done = false;
	std::uint8_t angle = 0U;
	std::uint8_t spread_delta = 0U;
	bool aimed = false;

	switch(group) {
	case PelletGroup::one:
		velocity_x = 0;
		velocity_y = speed;
		return true;
	case PelletGroup::one_aimed: {
		const auto velocity = vector_to_player(
			state, pellet_left, pellet_top, speed, 0U
		);
		velocity_x = velocity.x;
		velocity_y = velocity.y;
		return true;
	}
	case PelletGroup::two_spread_wide:
	case PelletGroup::two_spread_wide_aimed:
		spread_delta = 0x08U;
		if(group_i == 0) {
			angle = static_cast<std::uint8_t>(0x04U + spread_delta);
		} else if(group_i == 1) {
			angle = static_cast<std::uint8_t>(0U - 0x04U - spread_delta);
			done = true;
		}
		break;
	case PelletGroup::two_spread_narrow:
	case PelletGroup::two_spread_narrow_aimed:
		if(group_i == 0) {
			angle = 0x04U;
		} else if(group_i == 1) {
			angle = static_cast<std::uint8_t>(0U - 0x04U);
			done = true;
		}
		break;
	case PelletGroup::three_spread_wide:
	case PelletGroup::three_spread_wide_aimed:
		spread_delta = 0x05U;
		if(group_i == 0) {
			angle = 0U;
		} else if(group_i == 1) {
			angle = static_cast<std::uint8_t>(0x04U + spread_delta);
		} else if(group_i == 2) {
			angle = static_cast<std::uint8_t>(0U - 0x04U - spread_delta);
			done = true;
		}
		break;
	case PelletGroup::three_spread_narrow:
	case PelletGroup::three_spread_narrow_aimed:
		if(group_i == 0) {
			angle = 0U;
		} else if(group_i == 1) {
			angle = 0x04U;
		} else if(group_i == 2) {
			angle = static_cast<std::uint8_t>(0U - 0x04U);
			done = true;
		}
		break;
	case PelletGroup::four_spread_wide:
	case PelletGroup::four_spread_wide_aimed:
		spread_delta = 0x04U;
		if(group_i == 0) {
			angle = static_cast<std::uint8_t>(0x04U + spread_delta);
		} else if(group_i == 1) {
			angle = static_cast<std::uint8_t>(0U - 0x04U - spread_delta);
		} else if(group_i == 2) {
			angle = static_cast<std::uint8_t>(0x0CU + (spread_delta * 3U));
		} else if(group_i == 3) {
			angle = static_cast<std::uint8_t>(0U - 0x0CU - (spread_delta * 3U));
			done = true;
		}
		break;
	case PelletGroup::four_spread_narrow:
	case PelletGroup::four_spread_narrow_aimed:
		if(group_i == 0) {
			angle = 0x04U;
		} else if(group_i == 1) {
			angle = static_cast<std::uint8_t>(0U - 0x04U);
		} else if(group_i == 2) {
			angle = 0x0CU;
		} else if(group_i == 3) {
			angle = static_cast<std::uint8_t>(0U - 0x0CU);
			done = true;
		}
		break;
	case PelletGroup::five_spread_wide:
	case PelletGroup::five_spread_wide_aimed:
		spread_delta = 0x04U;
		if(group_i == 0) {
			angle = 0U;
		} else if(group_i == 1) {
			angle = static_cast<std::uint8_t>(0x04U + spread_delta);
		} else if(group_i == 2) {
			angle = static_cast<std::uint8_t>(0U - 0x04U - spread_delta);
		} else if(group_i == 3) {
			angle = static_cast<std::uint8_t>(0x08U + (spread_delta * 2U));
		} else if(group_i == 4) {
			angle = static_cast<std::uint8_t>(0U - 0x08U - (spread_delta * 2U));
			done = true;
		}
		break;
	case PelletGroup::five_spread_narrow:
	case PelletGroup::five_spread_narrow_aimed:
		if(group_i == 0) {
			angle = 0U;
		} else if(group_i == 1) {
			angle = 0x04U;
		} else if(group_i == 2) {
			angle = static_cast<std::uint8_t>(0U - 0x04U);
		} else if(group_i == 3) {
			angle = 0x08U;
		} else if(group_i == 4) {
			angle = static_cast<std::uint8_t>(0U - 0x08U);
			done = true;
		}
		break;
	case PelletGroup::one_random_narrow_aimed:
		angle = static_cast<std::uint8_t>((pc98::irand(state.random_seed) & 0x0FU) - 0x07U);
		done = true;
		aimed = true;
		break;
	case PelletGroup::one_random_wide:
		angle = static_cast<std::uint8_t>((pc98::irand(state.random_seed) & 0x3FU) + 0x20U);
		done = true;
		break;
	case PelletGroup::none:
	default:
		// `group_velocity_set()` leaves velocity uninitialized for PG_NONE and
		// invalid values. Boss/stage source never deliberately requests either.
		return false;
	}

	aimed = aimed || (static_cast<std::int16_t>(group) >= 21);
	if(aimed) {
		const auto velocity = vector_to_player(
			state, pellet_left, pellet_top, speed, angle
		);
		velocity_x = velocity.x;
		velocity_y = velocity.y;
	} else {
		const auto velocity = pc98::masterlib::vector2(
			static_cast<std::uint8_t>(angle + 0x40U), speed
		);
		velocity_x = velocity.x;
		velocity_y = velocity.y;
	}
	return done;
}

void pellet_motion_apply(State &state, Pellet &pellet) noexcept
{
	switch(pellet.motion) {
	case PelletMotion::regular:
		break;
	case PelletMotion::gravity:
		pellet.velocity_y = word_add(pellet.velocity_y, pellet.speed);
		break;
	case PelletMotion::sling_aimed: {
		if(pellet.sling_direction == PelletSlingDirection::none) {
			pellet.sling_direction = (pc98::irand(state.random_seed) & 1U)
				? PelletSlingDirection::counterclockwise
				: PelletSlingDirection::clockwise;
		}
		const auto velocity = pc98::masterlib::vector2(
			static_cast<std::uint8_t>(pellet.angle), pellet.speed
		);
		if(pellet.sling_direction == PelletSlingDirection::clockwise) {
			pellet.velocity_x = velocity.x;
			pellet.velocity_y = velocity.y;
		} else {
			pellet.velocity_y = velocity.x;
			pellet.velocity_x = velocity.y;
		}
		pellet.angle = word_add(pellet.angle, 0x08);
		if(pellet.angle > 0x100) {
			const auto aimed = vector_to_player(
				state,
				q12_4_to_pixel(pellet.cur_left),
				q12_4_to_pixel(pellet.cur_top),
				pellet.speed,
				0U
			);
			pellet.velocity_x = aimed.x;
			pellet.velocity_y = aimed.y;
			pellet.from_group = PelletGroup::one_aimed;
			pellet.angle = 0;
		}
		break;
	}
	case PelletMotion::bounce_from_top_then_gravity:
		// Preserve the source's mixed pixel/subpixel comparisons, including the
		// unreachable right-edge clause.
		if(
			(pellet.cur_left <= playfield_left) ||
			(q12_4_to_pixel(pellet.cur_left) >= pellet_pixels_to_subpixels(
				static_cast<std::int16_t>(playfield_right - pellet_width)
			)) ||
			(pellet.cur_top <= pellet_pixels_to_subpixels(
				static_cast<std::int16_t>(playfield_top + ((pellet_height / 4) * 3))
			))
		) {
			pellet.velocity_x = word(-static_cast<std::int32_t>(pellet.velocity_x));
			pellet.velocity_y = 0;
			pellet.motion = PelletMotion::gravity;
			if(pellet.cur_left <= pellet_pixels_to_subpixels(playfield_left)) {
				pellet.cur_left = pellet_pixels_to_subpixels(
					static_cast<std::int16_t>(playfield_left + 1)
				);
			}
			if(pellet.cur_left >= pellet_pixels_to_subpixels(
				static_cast<std::int16_t>(playfield_right - pellet_width)
			)) {
				pellet.cur_left = pellet_pixels_to_subpixels(
					static_cast<std::int16_t>(playfield_right - pellet_width - 1)
				);
			}
			if(pellet.cur_top <= pellet_pixels_to_subpixels(playfield_top)) {
				pellet.cur_top = pellet_pixels_to_subpixels(
					static_cast<std::int16_t>(playfield_top + 1)
				);
			}
		}
		break;
	case PelletMotion::fall_straight_from_top_then_regular:
		if(q12_4_to_pixel(pellet.cur_top) <=
			(static_cast<std::int16_t>(playfield_top + ((pellet_height / 4) * 3)))
		) {
			pellet.velocity_x = 0;
			pellet.velocity_y = pellet.speed;
			pellet.motion = PelletMotion::regular;
			if(q12_4_to_pixel(pellet.cur_top) <= playfield_top) {
				pellet.cur_top = pellet_pixels_to_subpixels(
					static_cast<std::int16_t>(playfield_top + 1)
				);
			}
		}
		break;
	case PelletMotion::spin: {
		const auto angle = static_cast<std::uint8_t>(pellet.angle);
		const auto velocity = pc98::masterlib::vector2(angle, 16 * pellet_subpixel_factor);
		pellet.cur_left = word_add(pellet.spin_center_left, velocity.x);
		pellet.cur_top = word_add(pellet.spin_center_top, velocity.y);
		pellet.spin_center_left = word_add(
			pellet.spin_center_left, pellet.spin_velocity_x
		);
		pellet.spin_center_top = word_add(
			pellet.spin_center_top, pellet.spin_velocity_y
		);
		pellet.velocity_x = 0;
		pellet.velocity_y = 0;
		pellet.angle = word_add(pellet.angle, 0x04);
		break;
	}
	case PelletMotion::chase: {
		const auto target = vector_to_player(
			state,
			q12_4_to_pixel(pellet.cur_left),
			q12_4_to_pixel(pellet.cur_top),
			pellet.speed,
			0U
		);
		if(pellet.cur_top < pellet_pixels_to_subpixels(
			static_cast<std::int16_t>(playfield_bottom - 80)
		)) {
			auto chase = [](std::int16_t current, std::int16_t target_value) constexpr {
				return static_cast<std::int16_t>((current > target_value) ? -1 :
					(current < target_value) ? 1 : 0);
			};
			pellet.velocity_x = word_add(
				pellet.velocity_x, chase(pellet.velocity_x, target.x)
			);
			pellet.velocity_y = word_add(
				pellet.velocity_y, chase(pellet.velocity_y, target.y)
			);
		}
		break;
	}
	}
}

[[nodiscard]] bool pellet_visible_after_hittests(
	State &state,
	Pellet &pellet,
	std::int16_t pellet_left,
	std::int16_t pellet_top
) noexcept
{
	for(std::size_t i = 0U; i < 3U; ++i) {
		const auto &shot = state.shots.slots[i];
		if(
			shot.moving && overlap_lt_gt(
				pellet_left, pellet_top, pellet_width, pellet_height,
				shot.left, shot.top, 16, 16
			)
		) {
			return false;
		}
		if(overlap_lt_gt(
			static_cast<std::int16_t>(pellet_left + 8), pellet_top,
			pellet_width, pellet_height, shot.left, shot.top, 16, 16
		)) {
			return false;
		}
	}
	if(pellet.decay_frame != 0) {
		return true;
	}
	if(
		overlap_lt_gt(
			pellet_left, pellet_top, pellet_width, pellet_height,
			state.orb.horizontal.left, state.orb.top, 32, 32
		) ||
		overlap_lt_gt(
			static_cast<std::int16_t>(pellet_left + 8), pellet_top,
			pellet_width, pellet_height,
			state.orb.horizontal.left, state.orb.top, 32, 32
		)
	) {
		pellet.velocity_y = arithmetic_half(pellet.velocity_y);
		pellet.velocity_x = arithmetic_half(pellet.velocity_x);
		pellet.decay_frame = 1;
		state.projectiles.pellet_destroy_score_delta = static_cast<std::uint16_t>(
			state.projectiles.pellet_destroy_score_delta + pellet_destroy_score
		);
		return true;
	}
	if(
		state.player.deflecting && overlap_lt_gt(
			pellet_left, pellet_top, pellet_width, pellet_height,
			static_cast<std::int16_t>(state.player.left - pellet_width),
			static_cast<std::int16_t>(player_top - (pellet_height * 2)),
			static_cast<std::int16_t>(player_width + (pellet_width * 2)),
			static_cast<std::int16_t>(player_height + (pellet_height * 2))
		)
	) {
		const auto angle = (q12_4_to_pixel(pellet.cur_left) <=
			static_cast<std::int16_t>(state.player.left + 12)) ? 0x80U : 0U;
		const auto velocity = pc98::masterlib::vector2(
			static_cast<std::uint8_t>(angle), 8 * pellet_subpixel_factor
		);
		pellet.velocity_x = velocity.x;
		pellet.velocity_y = velocity.y;
		if(pellet.from_group == PelletGroup::none) {
			pellet.motion = PelletMotion::regular;
		}
		return false;
	}
	return true;
}

void pellet_decay_tick(ProjectileState &projectiles, Pellet &pellet) noexcept
{
	pellet.decay_frame = word_add(pellet.decay_frame, 1);
	if(pellet.decay_frame > (pellet_decay_frames + 1)) {
		pellet.decay_frame = 0;
		pellet.moving = false;
		projectiles.pellet_alive_count = word_add(
			projectiles.pellet_alive_count, -1
		);
	}
}

[[nodiscard]] bool pellet_hittest_player(
	State &state, const Pellet &pellet
) noexcept
{
	if(state.player.invincible || pellet.decay_frame != 0) {
		return false;
	}
	const auto left = q12_4_to_pixel(pellet.cur_left);
	const auto top = q12_4_to_pixel(pellet.cur_top);
	if(
		left >= static_cast<std::int16_t>(state.player.left + 4) &&
		left <= static_cast<std::int16_t>(state.player.left + 20) &&
		top >= static_cast<std::int16_t>(player_top + (state.player.sliding ? 8 : 0)) &&
		top < static_cast<std::int16_t>(player_top + player_height - pellet_height)
	) {
		state.input.player_is_hit = true;
		return true;
	}
	return false;
}

void pellets_clouds_update(ProjectileState &projectiles) noexcept
{
	for(auto &pellet : projectiles.pellets) {
		if(pellet.cloud_frame == 0) {
			continue;
		}
		pellet.cloud_frame = word_add(pellet.cloud_frame, 1);
		if(pellet.cloud_frame == 9) {
			pellet.cloud_frame = 0;
			pellet.moving = true;
		}
	}
}

void shootout_laser_hittest_and_advance(
	State &state, ShootoutLaser &laser, std::int16_t pixel_count, bool put_flag
) noexcept
{
	auto left = q24_8_to_pixel(laser.ray_i_left);
	auto top = q24_8_to_pixel(laser.ray_i_top);
	for(std::int32_t i = 0; i < pixel_count; ++i) {
		if(
			(arithmetic_shift_right_3(left) !=
			 q24_8_to_vram_byte_amount(laser.ray_i_left)) ||
			(top != q24_8_to_pixel(laser.ray_i_top))
		) {
			if(put_flag && !state.player.invincible && laser.damaging) {
				if(
					top > static_cast<std::int32_t>(player_top + 8) &&
					static_cast<std::int32_t>(state.player.left + 4) <= left &&
					static_cast<std::int32_t>(state.player.left + 24) > left
				) {
					state.input.player_is_hit = true;
				}
			}
			left = q24_8_to_pixel(laser.ray_i_left);
			top = q24_8_to_pixel(laser.ray_i_top);
			if(
				left > (playfield_right - 2) || left < playfield_left ||
				top < playfield_top || top > (playfield_bottom - 1)
			) {
				break;
			}
		}
		laser.ray_i_left += laser.step_x;
		laser.ray_i_top += laser.step_y;
	}
}

void linebullet_hittest(
	State &state, std::int16_t right, std::int16_t bottom
) noexcept
{
	if(
		right > state.player.left &&
		right < static_cast<std::int16_t>(state.player.left + player_width) &&
		bottom > static_cast<std::int16_t>(player_top + (player_height / 2)) &&
		!state.player.invincible
	) {
		state.input.player_is_hit = true;
	}
}

} // namespace

void pellets_add_group(
	State &state,
	std::int16_t left,
	std::int16_t top,
	PelletGroup group,
	std::int16_t speed_base_q12_4
) noexcept
{
	auto &projectiles = state.projectiles;
	if(projectiles.pellet_alive_count > 100) {
		return;
	}
	if(
		left >= playfield_right || top < static_cast<std::int16_t>(playfield_top - pellet_height) ||
		left < static_cast<std::int16_t>(playfield_left - pellet_width) || top > playfield_bottom
	) {
		return;
	}
	auto speed = word_add(speed_base_q12_4, rank_speed_base(state.rank));
	speed = word(static_cast<std::int32_t>(speed) +
		(static_cast<std::int32_t>(projectiles.pellet_speed_x40) * speed) / 40);
	if(speed < pellet_subpixel_factor) {
		speed = pellet_subpixel_factor;
	}

	std::int16_t group_i = 0;
	for(auto &pellet : projectiles.pellets) {
		if(pellet.moving || pellet.cloud_frame != 0) {
			continue;
		}
		pellet.decay_frame = 0;
		pellet.cur_left = pellet_pixels_to_subpixels(left);
		pellet.cur_top = pellet_pixels_to_subpixels(top);
		pellet.cloud_left = left;
		pellet.cloud_top = top;
		if(projectiles.pellet_spawn_with_cloud) {
			pellet.cloud_frame = 1;
		} else {
			pellet.moving = true;
		}
		pellet.from_group = group;
		pellet.prev_left = -1;
		pellet.age = 0;
		projectiles.pellet_alive_count = word_add(projectiles.pellet_alive_count, 1);
		std::int16_t velocity_x{};
		std::int16_t velocity_y{};
		const auto done = group_velocity_set(
			state, group, speed, group_i, left, top, velocity_x, velocity_y
		);
		pellet.velocity_x = velocity_x;
		pellet.velocity_y = velocity_y;
		if(done) {
			return;
		}
		group_i = word_add(group_i, 1);
	}
}

void pellets_add_single(
	State &state,
	std::int16_t left,
	std::int16_t top,
	std::uint8_t angle,
	std::int16_t speed_base_q12_4,
	PelletMotion motion,
	std::int16_t fixed_motion_speed_q12_4,
	std::int16_t spin_center_left,
	std::int16_t spin_center_top
) noexcept
{
	auto &projectiles = state.projectiles;
	if(projectiles.pellet_alive_count > 100) {
		return;
	}
	auto speed = word_add(speed_base_q12_4, rank_speed_base(state.rank));
	speed = word(static_cast<std::int32_t>(speed) +
		(static_cast<std::int32_t>(projectiles.pellet_speed_x40) * speed) / 40);
	if(speed < pellet_subpixel_factor) {
		speed = pellet_subpixel_factor;
	}

	for(auto &pellet : projectiles.pellets) {
		if(pellet.moving || pellet.cloud_frame != 0) {
			continue;
		}
		pellet.decay_frame = 0;
		pellet.cur_left = pellet_pixels_to_subpixels(left);
		pellet.cur_top = pellet_pixels_to_subpixels(top);
		pellet.cloud_left = left;
		pellet.cloud_top = top;
		if(projectiles.pellet_spawn_with_cloud) {
			pellet.cloud_frame = 1;
		} else {
			pellet.moving = true;
		}
		pellet.from_group = PelletGroup::none;
		pellet.motion = motion;
		pellet.prev_left = -1;
		pellet.age = 0;
		projectiles.pellet_alive_count = word_add(projectiles.pellet_alive_count, 1);
		pellet.spin_center_left = pellet_pixels_to_subpixels(spin_center_left);
		pellet.spin_center_top = pellet_pixels_to_subpixels(spin_center_top);
		if(motion == PelletMotion::spin) {
			const auto spin_velocity = pc98::masterlib::vector2(angle, fixed_motion_speed_q12_4);
			pellet.spin_velocity_x = spin_velocity.x;
			pellet.spin_velocity_y = spin_velocity.y;
			pellet.angle = static_cast<std::int16_t>(pc98::masterlib::iatan2_source(
				word_subtract(pellet.cur_top, pellet.spin_center_top),
				word_subtract(pellet.cur_left, pellet.spin_center_left)
			).value_or(0U));
		}
		const auto velocity = pc98::masterlib::vector2(angle, speed);
		pellet.speed = fixed_motion_speed_q12_4;
		pellet.velocity_x = velocity.x;
		pellet.velocity_y = velocity.y;
		return;
	}
}

void pellet_speed_lower(
	State &state, std::int16_t max_x40, std::int16_t negative_delta_x40
) noexcept
{
	auto &speed = state.projectiles.pellet_speed_x40;
	if(speed > max_x40) {
		speed = max_x40;
	} else {
		speed = word_add(speed, negative_delta_x40);
	}
	if(speed < -15) {
		speed = -15;
	}
}

void pellet_speed_raise(State &state, std::int16_t delta_x40) noexcept
{
	auto &speed = state.projectiles.pellet_speed_x40;
	speed = word_add(speed, delta_x40);
	if(speed > 20) {
		speed = 20;
	}
}

ShotOrbUpdateStatus pellets_update_and_shots(State &state) noexcept
{
	auto &projectiles = state.projectiles;
	projectiles.pellet_interlace_field = !projectiles.pellet_interlace_field;
	pellets_clouds_update(projectiles);

	for(auto &pellet : projectiles.pellets) {
		if(!pellet.moving) {
			continue;
		}
		if(pellet.from_group == PelletGroup::none &&
			pellet.motion != PelletMotion::regular) {
			pellet_motion_apply(state, pellet);
		}
		if(pellet.velocity_y > pellet_velocity_max) {
			pellet.velocity_y = pellet_velocity_max;
		}
		if(pellet.velocity_x > pellet_velocity_max) {
			pellet.velocity_x = pellet_velocity_max;
		}
		pellet.cur_top = word_add(pellet.cur_top, pellet.velocity_y);
		pellet.cur_left = word_add(pellet.cur_left, pellet.velocity_x);

		if(pellet.decay_frame == 0) {
			if(hit_pellet(
				state.shots, q12_4_to_pixel(pellet.cur_left), q12_4_to_pixel(pellet.cur_top)
			)) {
				pellet.decay_frame = 0;
				pellet.moving = false;
				projectiles.pellet_alive_count = word_add(
					projectiles.pellet_alive_count, -1
				);
			}
		}
		if(
			pellet.cur_top >= pellet_pixels_to_subpixels(playfield_bottom) ||
			pellet.cur_top < pellet_pixels_to_subpixels(playfield_top) ||
			pellet.cur_left >= pellet_pixels_to_subpixels(
				static_cast<std::int16_t>(playfield_right - pellet_width)
			) ||
			pellet.cur_left <= pellet_pixels_to_subpixels(playfield_left)
		) {
			pellet.moving = false;
			projectiles.pellet_alive_count = word_add(
				projectiles.pellet_alive_count, -1
			);
			pellet.decay_frame = 0;
		}
	}

	const auto shots = update_shots_with_orb(
		state.shots, state.orb, state.random_seed
	);
	if(shots.status != ShotOrbUpdateStatus::complete) {
		return shots.status;
	}

	for(std::size_t i = 0U; i < projectiles.pellets.size(); ++i) {
		auto &pellet = projectiles.pellets[i];
		if(!pellet.moving) {
			continue;
		}
		if(!projectiles.pellet_interlace ||
			(projectiles.pellet_interlace_field == ((i % 2U) != 0U))) {
			if(pellet_visible_after_hittests(
				state, pellet, q12_4_to_pixel(pellet.cur_left), q12_4_to_pixel(pellet.cur_top)
			)) {
				if(pellet.not_rendered) {
					pellet.not_rendered = false;
				}
			} else {
				pellet.not_rendered = true;
			}
			pellet.prev_left = pellet.cur_left;
			pellet.prev_top = pellet.cur_top;
		}
		pellet.age = word_add(pellet.age, 1);
		if(pellet.decay_frame != 0) {
			pellet_decay_tick(projectiles, pellet);
		} else if(pellet_hittest_player(state, pellet)) {
			pellet.moving = false;
			projectiles.pellet_alive_count = word_add(
				projectiles.pellet_alive_count, -1
			);
			pellet.decay_frame = 0;
		}
	}
	return ShotOrbUpdateStatus::complete;
}

void pellets_decay(State &state) noexcept
{
	for(auto &pellet : state.projectiles.pellets) {
		if(!pellet.moving || pellet.decay_frame != 0) {
			continue;
		}
		pellet.velocity_y = static_cast<std::int16_t>(pellet.velocity_y / 1.5);
		pellet.velocity_x = static_cast<std::int16_t>(pellet.velocity_x / 1.5);
		pellet.decay_frame = 1;
		state.projectiles.pellet_destroy_score_delta = static_cast<std::uint16_t>(
			state.projectiles.pellet_destroy_score_delta + pellet_destroy_score
		);
	}
}

void pellets_reset_nonclouds(State &state) noexcept
{
	for(auto &pellet : state.projectiles.pellets) {
		if(!pellet.moving) {
			continue;
		}
		pellet.moving = false;
		pellet.decay_frame = 0;
		pellet.cloud_frame = 0;
	}
	state.projectiles.pellet_alive_count = 0;
}

void pellets_unput_and_reset_nonclouds(State &state) noexcept
{
	// The source's only additional work is page-1 restoration. Its mutation
	// semantics are exactly pellets_reset_nonclouds(), including live clouds.
	pellets_reset_nonclouds(state);
}

void pellet_destroy_score_delta_commit(State &state) noexcept
{
	state.score += state.projectiles.pellet_destroy_score_delta;
	state.projectiles.pellet_destroy_score_delta = 0U;
}

void missiles_set_ptn_id_base(State &state, std::uint8_t ptn_id_base) noexcept
{
	state.projectiles.missile_ptn_id_base = ptn_id_base;
}

void missiles_add(
	State &state,
	std::int16_t left,
	std::int16_t top,
	std::int16_t velocity_x_q12_4,
	std::int16_t velocity_y_q12_4,
	std::int8_t unknown
) noexcept
{
	for(auto &missile : state.projectiles.missiles) {
		if(missile.flag != MissileFlag::free) {
			continue;
		}
		missile.cur_left = pellet_pixels_to_subpixels(left);
		missile.cur_top = pellet_pixels_to_subpixels(top);
		missile.prev_left = missile_new;
		missile.velocity_x = velocity_x_q12_4;
		missile.velocity_y = velocity_y_q12_4;
		missile.unknown = unknown;
		missile.flag = MissileFlag::moving;
		return;
	}
}

void missiles_reset(State &state) noexcept
{
	for(auto &missile : state.projectiles.missiles) {
		missile.flag = MissileFlag::free;
	}
}

void missiles_update(State &state) noexcept
{
	for(std::size_t i = 0U; i < state.projectiles.missiles.size(); ++i) {
		auto &missile = state.projectiles.missiles[i];
		if(missile.flag == MissileFlag::free) {
			continue;
		}
		const auto in_interlace_field = ((i % 2U) == (state.frame_rand & 1U));
		if(missile.flag == MissileFlag::moving) {
			missile.cur_left = word_add(missile.cur_left, missile.velocity_x);
			missile.cur_top = word_add(missile.cur_top, missile.velocity_y);
			const auto left = q12_4_to_pixel(missile.cur_left);
			const auto top = q12_4_to_pixel(missile.cur_top);
			if(
				left < playfield_left || top < playfield_top ||
				left > static_cast<std::int16_t>(playfield_right - missile_width) ||
				top > static_cast<std::int16_t>(playfield_bottom - 1)
			) {
				missile.flag = MissileFlag::hit;
				if(left < playfield_left) {
					missile.prev_left = pellet_pixels_to_subpixels(playfield_left);
				} else if(left > static_cast<std::int16_t>(playfield_right - missile_width)) {
					missile.prev_left = pellet_pixels_to_subpixels(
						static_cast<std::int16_t>(playfield_right - missile_width)
					);
				}
				if(top < playfield_top) {
					missile.prev_top = pellet_pixels_to_subpixels(playfield_top);
				} else if(top > static_cast<std::int16_t>(playfield_bottom - 1)) {
					missile.prev_top = pellet_pixels_to_subpixels(
						static_cast<std::int16_t>(playfield_bottom - (missile_height / 2))
					);
				}
			}
		} else {
			if(in_interlace_field) {
				missile.flag = static_cast<MissileFlag>(
					static_cast<std::uint8_t>(missile.flag) + 1U
				);
			}
			if(static_cast<std::uint8_t>(missile.flag) >
				static_cast<std::uint8_t>(MissileFlag::hit_last)) {
				missile.flag = MissileFlag::free;
			}
		}
	}

	if(state.player.invincible) {
		return;
	}
	for(const auto &missile : state.projectiles.missiles) {
		if(missile.flag == MissileFlag::free) {
			continue;
		}
		const auto left = q12_4_to_pixel(missile.cur_left);
		const auto top = q12_4_to_pixel(missile.cur_top);
		if(
			left > static_cast<std::int16_t>(state.player.left - (missile_width / 2)) &&
			left < static_cast<std::int16_t>(state.player.left + (player_width / 2) + (missile_width / 2)) &&
			top < static_cast<std::int16_t>(player_top + player_height) &&
			top > static_cast<std::int16_t>(player_top - (missile_height / 2))
		) {
			state.input.player_is_hit = true;
			return;
		}
	}
}

void shootout_lasers_init(State &state) noexcept
{
	for(std::size_t i = 0U; i < state.projectiles.shootout_lasers.size(); ++i) {
		state.projectiles.shootout_lasers[i].id = static_cast<std::int8_t>(i);
	}
}

void shootout_laser_spawn(
	State &state,
	std::size_t slot,
	std::int16_t origin_left,
	std::int16_t origin_top,
	std::int16_t target_left,
	std::int16_t target_top,
	std::int16_t speed_multiplied_by_8,
	std::uint8_t color,
	std::int16_t moveout_at_age,
	std::int16_t width
) noexcept
{
	if(slot >= state.projectiles.shootout_lasers.size()) {
		return;
	}
	auto &laser = state.projectiles.shootout_lasers[slot];
	if(laser.alive) {
		return;
	}
	laser.origin_left = static_cast<std::int32_t>(origin_left) << 8;
	laser.origin_top = static_cast<std::int32_t>(origin_top) << 8;
	laser.ray_start_left = laser.origin_left;
	laser.ray_start_top = laser.origin_top;
	laser.target_left = target_left;
	laser.target_top = target_top;
	laser.unknown = -256;
	const auto angle = pc98::masterlib::iatan2_source(
		word(static_cast<std::int32_t>(target_top) - origin_top),
		word(static_cast<std::int32_t>(target_left) - origin_left)
	).value_or(0U);
	laser.step_x = pc98::masterlib::cosine8(angle);
	laser.step_y = pc98::masterlib::sine8(angle);
	laser.velocity_x = arithmetic_shift_right_3(
		static_cast<std::int32_t>(speed_multiplied_by_8) * laser.step_x
	);
	laser.velocity_y = arithmetic_shift_right_3(
		static_cast<std::int32_t>(speed_multiplied_by_8) * laser.step_y
	);
	laser.ray_extend_speed = static_cast<std::int16_t>(speed_multiplied_by_8 / 8);
	laser.width_cel = static_cast<std::uint8_t>(width - 1);
	laser.alive = true;
	laser.damaging = true;
	laser.moveout_at_age = moveout_at_age;
	laser.age = 0;
	laser.color = color;
	laser.ray_moveout_speed = laser.ray_extend_speed;
	laser.ray_length = 0;
}

void shootout_laser_spawn_safe(
	State &state,
	std::size_t index,
	std::int16_t origin_left,
	std::int16_t origin_top,
	std::int16_t target_left,
	std::int16_t target_top,
	std::int16_t speed_multiplied_by_8,
	std::uint8_t color,
	std::int16_t moveout_at_age,
	std::int16_t width
) noexcept
{
	shootout_laser_spawn(
		state,
		index % state.projectiles.shootout_lasers.size(),
		origin_left,
		origin_top,
		target_left,
		target_top,
		speed_multiplied_by_8,
		color,
		moveout_at_age,
		width
	);
}

void shootout_laser_update(State &state, std::size_t slot) noexcept
{
	if(slot >= state.projectiles.shootout_lasers.size()) {
		return;
	}
	auto &laser = state.projectiles.shootout_lasers[slot];
	if(!laser.alive) {
		return;
	}
	if(laser.age >= laser.moveout_at_age) {
		laser.ray_i_left = laser.ray_start_left;
		laser.ray_i_top = laser.ray_start_top;
		shootout_laser_hittest_and_advance(
			state, laser, laser.ray_moveout_speed, false
		);
		laser.ray_start_left = laser.ray_i_left;
		laser.ray_start_top = laser.ray_i_top;
		if(
			laser.ray_start_left >= (static_cast<std::int32_t>(playfield_right) << 8) ||
			laser.ray_start_left < (static_cast<std::int32_t>(playfield_left) << 8) ||
			laser.ray_start_top < (static_cast<std::int32_t>(playfield_top) << 8) ||
			laser.ray_start_top >= (static_cast<std::int32_t>(playfield_bottom) << 8)
		) {
			laser.alive = false;
			return;
		}
	} else {
		laser.ray_length = word_add(laser.ray_length, laser.ray_extend_speed);
	}
	laser.age = word_add(laser.age, 1);
	laser.ray_i_left = laser.ray_start_left;
	laser.ray_i_top = laser.ray_start_top;
	shootout_laser_hittest_and_advance(state, laser, laser.ray_length, true);
}

void shootout_lasers_update(State &state) noexcept
{
	for(std::size_t i = 0U; i < state.projectiles.shootout_lasers.size(); ++i) {
		shootout_laser_update(state, i);
	}
}

void shootout_lasers_reset_first(State &state, std::size_t count) noexcept
{
	const auto end = (count < state.projectiles.shootout_lasers.size())
		? count : state.projectiles.shootout_lasers.size();
	for(std::size_t i = 0U; i < end; ++i) {
		state.projectiles.shootout_lasers[i].alive = false;
	}
}

void linebullet_unput(
	State &,
	std::int16_t,
	std::int16_t,
	std::int16_t,
	std::int16_t
) noexcept
{
	// graph_r_line_unput() only restores the prior page-1 pixels. The headless
	// gameplay State has no page storage; unlike the put calls, it has no
	// collision or other simulation-side effect.
}

void linebullet_put_and_hittest(
	State &state,
	std::int16_t,
	std::int16_t,
	std::int16_t right,
	std::int16_t bottom,
	std::uint8_t
) noexcept
{
	linebullet_hittest(state, right, bottom);
}

void linebullet_put_patterned_and_hittest(
	State &state,
	std::int16_t,
	std::int16_t,
	std::int16_t right,
	std::int16_t bottom,
	std::uint8_t,
	std::uint16_t
) noexcept
{
	linebullet_hittest(state, right, bottom);
}

} // namespace th01::core
