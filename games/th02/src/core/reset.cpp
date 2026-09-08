#include "th02/core/reset.hpp"

namespace th02::core {
namespace {

constexpr std::uint8_t option_patnum_a = 72U;
constexpr std::int16_t no_boss_position = -1;

void reset_common(PlayerResetState &reset, bool &quit, PlayerHitState &hit,
	PlayerMissState &miss, PlayerShotPoolState &shots,
	PlayerShotLifetimeState &lifetime, PlayerShotStreamState &stream,
	PlayerShotVolleyContext &volley) noexcept
{
	shots_free_all(shots);
	lifetime.option_shots_alive = 0U;
	hit.invincibility_time = 0U;
	hit.invincible_via_bomb = false;
	quit = false;
	miss.stage_miss_count = 0U;
	miss.miss_frame = 0U;
	reset.shot_unused = 8U;
	reset.shot_unused_2 = 0U;
	hit.raw_hit = 0U;
	hit.miss_active = false;
	reset.option_patnum = option_patnum_a;
	volley.boss_position = {no_boss_position, no_boss_position};
	volley.boss_pos_x_unused = no_boss_position;
	stream.spread_angle_delta = 0;
	reset.player_unused = 0U;
}
}

void player_reset(PlayerResetState &reset, bool &quit, PlayerHitState &hit,
	PlayerMissState &miss, PlayerShotPoolState &shots,
	PlayerShotLifetimeState &lifetime, PlayerShotStreamState &stream,
	PlayerShotVolleyContext &volley, MovementSpeeds &speeds,
	std::uint8_t shottype) noexcept
{
	reset_common(reset, quit, hit, miss, shots, lifetime, stream, volley);

	switch(shottype) {
	case static_cast<std::uint8_t>(PlayerShotType::a):
		volley.type = PlayerShotType::a;
		speeds = {5, 5, 4, 4};
		stream.shot_patnum_powered = 0x30U;
		break;

	case static_cast<std::uint8_t>(PlayerShotType::b):
		volley.type = PlayerShotType::b;
		speeds = {4, 4, 3, 3};
		stream.shot_patnum_powered = 0x34U;
		stream.shot_option_patnum_powered = 0x37U;
		++reset.option_patnum;
		break;

	case static_cast<std::uint8_t>(PlayerShotType::c):
		volley.type = PlayerShotType::c;
		speeds = {3, 3, 3, 3};
		lifetime.option_decay_interval = 3;
		stream.shot_patnum_powered = 0x7DU;
		stream.shot_option_patnum_powered = 0x3BU;
		reset.option_patnum = static_cast<std::uint8_t>(
			reset.option_patnum + 2U
		);
		break;
	}
}

} // namespace th02::core
