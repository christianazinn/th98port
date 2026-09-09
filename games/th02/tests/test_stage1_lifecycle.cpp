#include "th02/core/stage1_lifecycle.hpp"

#include <array>
#include <cstdlib>

using namespace th02::core;

int main()
{
	for(const auto rank : std::array<std::int8_t, 5>{0, 1, 2, 3, -1}) {
		BossState boss{};
		boss.topleft_on_page = {{{-10, 31}, {20, 41}}};
		boss.phase_frame = 99;
		boss.target = {45, 65};
		boss.damage = 123;
		boss.phase = 1U;
		RikaState rika{{-57, -21}, 2, 79, 123U, 65535U};
		std::uint32_t frame = 999U;
		bool lasers = false;
		rika_combat_setup_after_dialogue(boss, rika, frame, lasers, 399, rank);
		if(boss.topleft_on_page != std::array<ScreenPoint, 2>{{{192, 31}, {192, 41}}}
			|| boss.phase_frame != 99 || boss.target != ScreenPoint{45, 65}
			|| boss.damage != 0 || boss.phase != 0U || boss.patnum != 150
			|| boss.explode_angle_offset != 32U || frame != 0U || !lasers
			|| rika != RikaState{{-57, 47}, 0, 79, 123U, 65535U}) {
			return EXIT_FAILURE;
		}
		const std::array<std::uint8_t, 5> expected = rank == 0
			? std::array<std::uint8_t, 5>{25, 21, 32, 32, 7}
			: std::array<std::uint8_t, 5>{11, 21, 32, 2, 3};
		if(boss.rank_param != expected) return EXIT_FAILURE;
		rika_combat_setup_after_dialogue(boss, rika, frame, lasers, 32767, rank);
		if(rika.topleft.y != -32721) return EXIT_FAILURE;
		rika_combat_setup_after_dialogue(boss, rika, frame, lasers, -100, rank);
		if(rika.topleft.y != -52) return EXIT_FAILURE;
	}
	std::uint8_t stage = 255U;
	SparkUpdateParameters sparks{-1, 88U};
	StageBgFlashState flash{99, 1234U};
	rika_stage_leave_finish(stage, sparks, flash);
	if(stage != 0U || sparks != SparkUpdateParameters{0, 88U}
		|| flash != StageBgFlashState{0, 1234U}) return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
