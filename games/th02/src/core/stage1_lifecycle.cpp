#include "th02/core/stage1_lifecycle.hpp"

#include <bit>

namespace th02::core {

void rika_combat_setup_after_dialogue(BossState &boss, RikaState &rika,
	std::uint32_t &stage_frame, bool &lasers_enabled,
	std::int16_t scroll_line, std::int8_t rank) noexcept
{
	boss.explode_angle_offset = 0x20U;
	boss.topleft_on_page[0U].x = 192;
	boss.topleft_on_page[1U].x = 192;
	boss.damage = 0;
	boss.phase = 0U;
	boss.patnum = 150;
	stage_frame = 0U;
	rika.move_state = 0;
	lasers_enabled = true;
	boss.patnum = 150;
	rika.topleft.y = std::bit_cast<std::int16_t>(
		static_cast<std::uint16_t>(static_cast<std::int32_t>(scroll_line) + 48));
	if(rika.topleft.y >= 400) {
		rika.topleft.y = static_cast<std::int16_t>(rika.topleft.y - 400);
	}
	if(rank != 0) {
		boss.rank_param = {enemy_bullet_group::spread_3_medium_aimed,
			enemy_bullet_group::spread_5_wide, enemy_bullet_group::one,
			enemy_bullet_group::spread_2_medium, 3U};
	} else {
		boss.rank_param = {enemy_bullet_group::one_aimed,
			enemy_bullet_group::spread_5_wide, enemy_bullet_group::one,
			enemy_bullet_group::one, 7U};
	}
}

void rika_stage_leave_finish(std::uint8_t &stage_id,
	SparkUpdateParameters &sparks, StageBgFlashState &flash) noexcept
{
	stage_id = static_cast<std::uint8_t>(stage_id + 1U);
	sparks.acceleration_x = 0;
	flash.frame = 0;
}

} // namespace th02::core
