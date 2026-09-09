#include "th02/core/stage_bonus.hpp"

#include "th02/core/bomb.hpp"
#include "th02/core/item.hpp"
#include "th02/core/player.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using namespace th02::core;

struct Fixture {
	ScoreState score{99990, 10, 99990, 99U, 0U, 4, 2, 0};
	PlayerMissState miss{};
	BombState bomb{};
	ItemLifecycleState items{};
	std::uint8_t start_lives{2U};
};

[[nodiscard]] bool test_regular_rows_tally_and_real_score_grant() noexcept
{
	Fixture fixture{};
	fixture.miss.playperf = 0;
	fixture.miss.start_bombs = 3U;
	fixture.bomb.stage_bombs_used = 0U;
	fixture.items.point_items_collected = 2;
	StageClearBonusTally tally{};
	ScoreEvents expected_events{};
	expected_events.entries = {
		ScoreEvent{ScoreEventKind::score, 102700},
		ScoreEvent{ScoreEventKind::hiscore, 102700},
		ScoreEvent{ScoreEventKind::lives, 5},
		ScoreEvent{ScoreEventKind::sound, 8},
	};
	expected_events.count = 4U;
	if(stage_clear_bonus_apply(fixture.score, 1, fixture.miss, fixture.bomb,
		fixture.items, fixture.start_lives, tally) !=
		StageClearBonusResult::completed ||
		tally.row_values_x10 != std::array<std::int16_t, 6U>{
			2000, 3200, 2500, 3000, 800, 2000} || tally.sum != 1350 ||
		tally.sum_x10 != 13500 || tally.total_x10 != 27000 ||
		tally.bonus_delta != 2700 || fixture.score.score != 102700 ||
		fixture.score.score_delta != 2710 ||
		fixture.score.score_delta_transferred_prev != 99U ||
		fixture.score.hiscore != 102700 || fixture.score.extends_gained != 1U ||
		fixture.score.lives != 5 || fixture.score.bombs != 2 ||
		tally.score_events != expected_events) {
		return false;
	}
	return true;
}

[[nodiscard]] bool test_row_clamps_signed_words_and_pending_delta_retention() noexcept
{
	Fixture fixture{};
	fixture.score = {100, 77, 100, 33U, 0U, 1, 1, 0};
	fixture.miss.playperf = -17;
	fixture.miss.stage_miss_count = 4U;
	fixture.miss.start_bombs = 5U;
	fixture.bomb.stage_bombs_used = 6U;
	fixture.items.point_items_collected = 0;
	fixture.start_lives = 5U;
	StageClearBonusTally tally{};
	ScoreEvents expected_events{};
	expected_events.entries[0] = {ScoreEventKind::score, 177};
	expected_events.entries[1] = {ScoreEventKind::hiscore, 177};
	expected_events.count = 2U;
	if(stage_clear_bonus_apply(fixture.score, -1, fixture.miss, fixture.bomb,
		fixture.items, fixture.start_lives, tally) !=
		StageClearBonusResult::completed ||
		tally.row_values_x10 != std::array<std::int16_t, 6U>{
			-2000, -200, 0, 0, -800, -1000} || tally.sum != -400 ||
		tally.sum_x10 != -4000 || tally.total_x10 != 0 ||
		tally.bonus_delta != 0 || fixture.score.score != 177 ||
		fixture.score.hiscore != 177 ||
		fixture.score.score_delta != 77 ||
		fixture.score.score_delta_transferred_prev != 33U ||
		tally.score_events != expected_events) {
		return false;
	}
	return true;
}

[[nodiscard]] bool test_dword_total_and_score_table_fault_prefix() noexcept
{
	Fixture fixture{};
	fixture.score = {123, 100, 500, 17U, 6U, 4, 2, 0};
	fixture.miss.playperf = 0;
	fixture.miss.start_bombs = 3U;
	fixture.items.point_items_collected = 2;
	StageClearBonusTally tally{};
	const auto before = fixture.score;
	if(stage_clear_bonus_apply(fixture.score, 1, fixture.miss, fixture.bomb,
		fixture.items, fixture.start_lives, tally) !=
		StageClearBonusResult::source_score_grant_fault ||
		tally.sum != 1350 || tally.bonus_delta != 2700 ||
		fixture.score.score_delta != 2800 || fixture.score.score != before.score ||
		fixture.score.hiscore != before.hiscore ||
		fixture.score.score_delta_transferred_prev !=
		before.score_delta_transferred_prev ||
		fixture.score.extends_gained != before.extends_gained ||
		fixture.score.lives != before.lives || fixture.score.bombs != before.bombs ||
		tally.score_events != ScoreEvents{}) {
		return false;
	}

	fixture = {};
	fixture.score = {0, 0, 0, 0U, 0U, 1, 1, 0};
	fixture.miss.playperf = 0;
	fixture.miss.start_bombs = 3U;
	fixture.items.point_items_collected = 30000;
	if(stage_clear_bonus_apply(fixture.score, 1, fixture.miss, fixture.bomb,
		fixture.items, fixture.start_lives, tally) !=
		StageClearBonusResult::completed || tally.total_x10 != 405000000 ||
		tally.bonus_delta != 40500000 || fixture.score.score != 40500000 ||
		fixture.score.score_delta != 40500000) {
		return false;
	}
	return true;
}

} // namespace

int main()
{
	if(!test_regular_rows_tally_and_real_score_grant()) {
		std::fputs("FAIL regular\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_row_clamps_signed_words_and_pending_delta_retention()) {
		std::fputs("FAIL row\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_dword_total_and_score_table_fault_prefix()) {
		std::fputs("FAIL fault\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
