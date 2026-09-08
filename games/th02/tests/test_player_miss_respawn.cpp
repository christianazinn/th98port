#include "th02/core/item.hpp"
#include "th02/core/player.hpp"
#include "th02/core/random.hpp"
#include "th02/core/score.hpp"
#include "th02/core/spark.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
using namespace th02::core;

enum class EventKind : std::uint8_t { sound, power, lives, bombs, sprite };

struct Event {
	EventKind kind{};
	std::int16_t first{};
	std::int16_t second{};
	PlayerMissSprite sprite{};
	bool operator==(const Event &) const = default;
};

class RecordingPresentation final : public PlayerMissPresentationCallbacks {
public:
	std::array<Event, 80U> events{};
	std::size_t count{};
	const PlayerHitState *hit{};
	const PlayerMissState *miss{};
	const ScoreState *score{};
	bool initial_sound_saw_prefix{};

	void sound_effect(std::uint8_t sound_id) noexcept override
	{
		initial_sound_saw_prefix = hit != nullptr && miss != nullptr && score != nullptr &&
			!hit->miss_active && miss->stage_miss_count == 0U &&
			miss->total_miss_count == 0U && score->score_delta == 50;
		events[count++] = {EventKind::sound, sound_id, 0, {}};
	}

	void hud_power(std::uint8_t power, std::uint8_t shot_level) noexcept override
	{
		events[count++] = {EventKind::power, power, shot_level, {}};
	}

	void hud_lives(std::int8_t lives) noexcept override
	{
		events[count++] = {EventKind::lives, lives, 0, {}};
	}

	void hud_bombs(std::int8_t bombs) noexcept override
	{
		events[count++] = {EventKind::bombs, bombs, 0, {}};
	}

	void render_player(PlayerMissSprite sprite) noexcept override
	{
		events[count++] = {EventKind::sprite, 0, 0, sprite};
	}
};

struct MissFixture {
	PlayerMissState miss{0U, 80U, 2, 0U, 0U, 3U};
	PlayerFrameState frame{};
	PlayerHitState hit{1U, 0U, false, false};
	PlayerShotStreamState stream{};
	ScoreState score{};
	ItemPoolState items{};
	SparkPoolState sparks{};
	RandomState random{};
	RecordingPresentation presentation{};

	MissFixture() noexcept
	{
		frame.page_front = 0U;
		frame.page_back = 1U;
		frame.player.on_page[0] = {190, 290};
		frame.player.on_page[1] = {200, 300};
		frame.player.current = {-11, -12};
		stream.shot_level = 9U;
		score.score = 100;
		score.score_delta = 50;
		score.score_delta_transferred_prev = 4U;
		score.lives = 1;
		score.bombs = 1;
		presentation.hit = &hit;
		presentation.miss = &miss;
		presentation.score = &score;
	}
};

[[nodiscard]] std::size_t item_count_with_type(const ItemPoolState &items,
	ItemType type
) noexcept
{
	std::size_t count = 0U;
	for(const auto &item : items.items) {
		if(item.flag == item_alive && item.type == type) ++count;
	}
	return count;
}

[[nodiscard]] bool test_power_table_has_raw_21st_entry() noexcept
{
	constexpr std::array<std::uint8_t, 21U> expected{
		0U, 1U, 2U, 2U, 3U, 3U, 4U, 4U, 5U, 5U, 6U,
		6U, 6U, 7U, 7U, 7U, 8U, 8U, 8U, 8U, 9U,
	};
	for(std::size_t index = 0U; index < expected.size(); ++index) {
		const auto power = static_cast<std::uint8_t>(index << 2U);
		std::uint8_t level = 0xA5U;
		if(!hud_shot_level_from_power(power, level) || level != expected[index]) {
			return false;
		}
	}
	for(const auto power : {81U, 82U, 83U}) {
		std::uint8_t level = 0U;
		if(!hud_shot_level_from_power(static_cast<std::uint8_t>(power), level) ||
			level != 9U) return false;
	}
	std::uint8_t unchanged = 0x5AU;
	return !hud_shot_level_from_power(84U, unchanged) && unchanged == 0x5AU;
}

[[nodiscard]] bool test_normal_68_frame_miss_path() noexcept
{
	MissFixture fixture{};
	for(std::size_t call = 1U; call <= 68U; ++call) {
		if(player_miss_state_step(fixture.miss, fixture.frame, fixture.hit,
			fixture.stream, fixture.score, fixture.items, fixture.sparks,
			fixture.random, fixture.presentation) != PlayerMissResult::completed) {
			return false;
		}
		if(call == 1U && (!fixture.presentation.initial_sound_saw_prefix ||
			fixture.score.score != 150 || fixture.score.score_delta != 0 ||
			fixture.miss.stage_miss_count != 1U ||
			fixture.miss.total_miss_count != 1U || !fixture.hit.miss_active ||
			fixture.sparks.ring_index != 16U ||
			fixture.sparks.sparks[0].screen_topleft[1] !=
				SparkSubpixelPoint{3456, 5184} ||
			fixture.frame.player.current != ScreenPoint{200, 300})) return false;
		if(call == 24U && (fixture.miss.power != 64U ||
			fixture.stream.shot_level != 8U || fixture.hit.invincibility_time != 220U ||
			fixture.miss.playperf != 0 || fixture.random.cursor != 5U ||
			fixture.sparks.ring_index != 48U || item_count_with_type(fixture.items,
				ItemType::big_power) != 1U)) return false;
		if(call == 25U && (fixture.frame.player.current != ScreenPoint{200, 300} ||
			fixture.frame.player.on_page[1] != ScreenPoint{200, 304})) return false;
		if(call == 43U && (fixture.frame.player.current != ScreenPoint{200, 372} ||
			fixture.frame.player.on_page[0] != ScreenPoint{208, 366} ||
			fixture.frame.player.on_page[1] != ScreenPoint{208, 366} ||
			fixture.score.lives != 0 || fixture.score.bombs != 5 ||
			fixture.sparks.ring_index != 0U)) return false;
	}

	return fixture.miss.miss_frame == 0U && !fixture.hit.miss_active &&
		fixture.hit.raw_hit == 0U && fixture.hit.invincibility_time == 220U &&
		fixture.frame.player.current == ScreenPoint{208, 318} &&
		fixture.frame.player.on_page[0] == ScreenPoint{208, 366} &&
		fixture.frame.player.on_page[1] == ScreenPoint{208, 318} &&
		fixture.presentation.count == 51U &&
		fixture.presentation.events[0] == Event{EventKind::sound, 2, 0, {}} &&
		fixture.presentation.events[1] == Event{EventKind::sprite, 0, 0, {{200, 300}, 4}} &&
		fixture.presentation.events[4] == Event{EventKind::sprite, 0, 0, {{200, 300}, 5}} &&
		fixture.presentation.events[23] == Event{EventKind::sprite, 0, 0, {{200, 300}, 9}} &&
		fixture.presentation.events[24] == Event{EventKind::power, 64, 8, {}} &&
		fixture.presentation.events[25] == Event{EventKind::lives, 0, 0, {}} &&
		fixture.presentation.events[26] == Event{EventKind::bombs, 5, 0, {}} &&
		fixture.presentation.events[27] == Event{EventKind::sprite, 0, 0, {{208, 364}, 0}} &&
		fixture.presentation.events[50] == Event{EventKind::sprite, 0, 0, {{208, 318}, 0}} &&
		item_count_with_type(fixture.items, ItemType::power) == 4U &&
		fixture.items.miss_add_gameover == false && fixture.random.cursor == 5U &&
		fixture.sparks.ring_index == 0U;
}

[[nodiscard]] bool test_terminal_24_frame_path() noexcept
{
	MissFixture fixture{};
	fixture.frame.player.on_page[1] = {180, 280};
	fixture.score.lives = 0;
	fixture.miss.start_bombs = 4U;
	for(std::size_t call = 1U; call <= 24U; ++call) {
		if(player_miss_state_step(fixture.miss, fixture.frame, fixture.hit,
			fixture.stream, fixture.score, fixture.items, fixture.sparks,
			fixture.random, fixture.presentation) != PlayerMissResult::completed) {
			return false;
		}
	}
	return fixture.miss.miss_frame == 0U && fixture.hit.miss_active &&
		fixture.hit.raw_hit == 0xFFU && fixture.hit.invincibility_time == 220U &&
		fixture.score.score == 150 && fixture.score.score_delta == 0 &&
		fixture.miss.stage_miss_count == 1U && fixture.miss.total_miss_count == 1U &&
		fixture.random.cursor == 0U && item_count_with_type(fixture.items,
			ItemType::big_power) == static_cast<std::size_t>(item_miss_count) &&
		!fixture.items.miss_add_gameover &&
		fixture.sparks.ring_index == 48U && fixture.presentation.count == 24U &&
		fixture.presentation.events[0] == Event{EventKind::sound, 2, 0, {}} &&
		fixture.presentation.events[23] == Event{EventKind::sprite, 0, 0, {{180, 280}, 9}};
}

[[nodiscard]] bool test_refusals_and_byte_frame_wrap_keep_prefix() noexcept
{
	MissFixture invalid_page{};
	invalid_page.frame.page_back = 2U;
	const auto invalid_current = invalid_page.frame.player.current;
	if(player_miss_state_step(invalid_page.miss, invalid_page.frame,
		invalid_page.hit, invalid_page.stream, invalid_page.score,
		invalid_page.items, invalid_page.sparks, invalid_page.random,
		invalid_page.presentation) != PlayerMissResult::invalid_page ||
		invalid_page.presentation.count != 1U || !invalid_page.hit.miss_active ||
		invalid_page.miss.stage_miss_count != 1U || invalid_page.miss.total_miss_count != 1U ||
		invalid_page.score.score != 150 || invalid_page.score.score_delta != 0 ||
		invalid_page.frame.player.current != invalid_current || invalid_page.sparks.ring_index != 0U) {
		return false;
	}

	MissFixture failed_spark{};
	failed_spark.sparks.ring_index = 64U;
	const auto failed_current = failed_spark.frame.player.current;
	if(player_miss_state_step(failed_spark.miss, failed_spark.frame,
		failed_spark.hit, failed_spark.stream, failed_spark.score,
		failed_spark.items, failed_spark.sparks, failed_spark.random,
		failed_spark.presentation) != PlayerMissResult::spark_allocation_failed ||
		failed_spark.presentation.count != 1U || !failed_spark.hit.miss_active ||
		failed_spark.miss.stage_miss_count != 1U || failed_spark.miss.total_miss_count != 1U ||
		failed_spark.score.score != 150 || failed_spark.frame.player.current != failed_current) {
		return false;
	}

	MissFixture wrap{};
	wrap.miss.miss_frame = 255U;
	wrap.score.score_delta = 0;
	if(player_miss_state_step(wrap.miss, wrap.frame, wrap.hit, wrap.stream,
		wrap.score, wrap.items, wrap.sparks, wrap.random, wrap.presentation) !=
		PlayerMissResult::completed) return false;
	return wrap.miss.miss_frame == 0U && wrap.hit.raw_hit == 1U &&
		!wrap.hit.miss_active && wrap.score.score == 100 &&
		wrap.presentation.count == 1U &&
		wrap.presentation.events[0] == Event{EventKind::sprite, 0, 0, {{200, 300}, 4}};
}

[[nodiscard]] bool test_table_refusal_and_initial_counter_wrap_keep_prefix() noexcept
{
	MissFixture unsupported{};
	unsupported.miss.miss_frame = 23U;
	unsupported.hit.miss_active = true;
	unsupported.stream.shot_level = 10U;
	if(player_miss_state_step(unsupported.miss, unsupported.frame, unsupported.hit,
		unsupported.stream, unsupported.score, unsupported.items,
		unsupported.sparks, unsupported.random, unsupported.presentation) !=
		PlayerMissResult::unsupported_shot_level ||
		unsupported.miss.miss_frame != 24U ||
		unsupported.hit.invincibility_time != 220U ||
		unsupported.sparks.ring_index != 32U ||
		unsupported.miss.power != 80U ||
		unsupported.presentation.count != 0U ||
		unsupported.random.cursor != 0U) {
		return false;
	}

	MissFixture counters{};
	counters.miss.stage_miss_count = 0xFFU;
	counters.miss.total_miss_count = 0xFFU;
	if(player_miss_state_step(counters.miss, counters.frame, counters.hit,
		counters.stream, counters.score, counters.items, counters.sparks,
		counters.random, counters.presentation) != PlayerMissResult::completed) {
		return false;
	}
	return counters.miss.stage_miss_count == 0U &&
		counters.miss.total_miss_count == 0U && counters.score.score == 150 &&
		counters.sparks.ring_index == 16U;
}

[[nodiscard]] bool test_typed_update_bridge_converts_completed_gameover() noexcept
{
	MissFixture fixture{};
	fixture.miss.miss_frame = 23U;
	fixture.score.lives = 0;
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState lifetime{};
	PlayerMissUpdateCallbacks callbacks{fixture.miss, fixture.frame, fixture.stream,
		fixture.score, fixture.items, fixture.sparks, fixture.random,
		fixture.presentation};
	bool quit = false;
	return player_update_state_step(fixture.hit, quit, shots, lifetime, 1U, 0U,
		callbacks) == PlayerUpdateResult::miss_handled && quit &&
		fixture.hit.raw_hit == 0U && fixture.miss.miss_frame == 0U &&
		fixture.items.miss_add_gameover == false &&
		item_count_with_type(fixture.items, ItemType::big_power) ==
			static_cast<std::size_t>(item_miss_count);
}

} // namespace

int main()
{
	if(!test_power_table_has_raw_21st_entry()) {
		std::fputs("FAIL power table\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_normal_68_frame_miss_path()) {
		std::fputs("FAIL normal 68-frame miss\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_terminal_24_frame_path()) {
		std::fputs("FAIL terminal 24-frame miss\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_refusals_and_byte_frame_wrap_keep_prefix()) {
		std::fputs("FAIL refusal/wrap prefix\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_table_refusal_and_initial_counter_wrap_keep_prefix()) {
		std::fputs("FAIL table/counter prefix\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_typed_update_bridge_converts_completed_gameover()) {
		std::fputs("FAIL update bridge gameover\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
