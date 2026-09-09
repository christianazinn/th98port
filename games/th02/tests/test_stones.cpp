#include "th02/core/stones.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

namespace {

using namespace th02::core;

constexpr std::size_t front_page = 0U;
constexpr std::size_t back_page = 1U;

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U));
}

class RecordingCallbacks final : public StonesCallbacks {
public:
	std::array<StonesSprite, 32U> stones{};
	std::size_t stone_count{};
	std::array<std::uint8_t, 64U> sounds{};
	std::size_t sound_count{};
	std::size_t tile_count{};
	std::size_t explosion_ring_count{};

	void sound_effect(std::uint8_t id) noexcept override
	{
		if(sound_count < sounds.size()) sounds[sound_count] = id;
		++sound_count;
	}

	void render_stone(StonesSprite sprite) noexcept override
	{
		if(stone_count < stones.size()) stones[stone_count] = sprite;
		++stone_count;
	}

	void put_takeover_tile(std::int16_t, std::int16_t,
		std::int16_t) noexcept override
	{
		++tile_count;
	}

	void explosion_margins_clear() noexcept override {}
	void explosion_egc_begin() noexcept override {}
	void explosion_egc_end() noexcept override {}
	void explosion_ring(ScreenPoint, std::int16_t, bool) noexcept override
	{
		++explosion_ring_count;
	}
	void explosion_color(std::uint8_t) noexcept override {}
	void explosion_grcg_off() noexcept override {}
	void explosion_invalidate(ScreenPoint) noexcept override {}
	void explosion_tiles_render() noexcept override {}
	void explosion_sprite(ScreenPoint, std::int16_t) noexcept override {}
	void explosion_palette_show() noexcept override {}
	void explosion_graph_scroll(std::uint16_t, std::uint16_t,
		std::uint16_t) noexcept override {}
};

struct Fixture {
	BossState boss{};
	StonesState stones{};
	PlayerFrameState frame{};
	PlayerHitState hit{};
	PlayerShotPoolState shots{};
	ItemPoolState items{};
	ScoreState score{};
	SparkPoolState sparks{};
	EnemyBulletPoolState bullets{};
	LaserPoolState lasers{};
	RandomState random{};
	std::uint32_t stage_frame{};
	std::uint8_t slowdown_factor{};
	std::uint16_t palette_tone{};
	StonesCombatContext context;
	RecordingCallbacks callbacks{};

	Fixture() noexcept :
		context{frame, hit, shots, items, score, sparks, bullets, lasers, random,
			stage_frame, 0, 1, false, 0U, 1U, 0, slowdown_factor, palette_tone}
	{
		frame.page_front = front_page;
		frame.page_back = back_page;
		frame.player.current = {300, 300};
		frame.player.on_page[front_page] = {300, 300};
		frame.player.on_page[back_page] = {300, 300};
		for(std::size_t i = 0U; i < random.ring.size(); ++i) {
			random.ring[i] = static_cast<std::uint8_t>(i);
		}
	}
};

void add_plain_hit_shot(Fixture &fixture, ScreenPoint topleft) noexcept
{
	auto &shot = fixture.shots.shots[0U];
	shot.flag = player_shot_alive;
	shot.decay_cel = 0U;
	shot.pos_on_page[back_page] = {to_subpixels(topleft.x),
		to_subpixels(topleft.y)};
	shot.patnum = 0x31U;
	shot.from_option = false;
}

[[nodiscard]] bool test_shared_storage_and_combat_reset() noexcept
{
	static_assert(std::is_same_v<decltype(StonesState{}.flag),
		std::array<std::uint8_t, 5U>>);
	static_assert(std::is_same_v<decltype(StonesState{}.damage),
		std::array<std::int16_t, 5U>>);
	static_assert(std::is_same_v<decltype(StonesState{}.patnum),
		std::array<std::int16_t, 5U>>);

	Fixture fixture{};
	fixture.stones.kill_frame[0U] = 17;
	fixture.boss.rank_param[4U] = 0xA5U;
	stones_combat_reset(fixture.boss, fixture.stones, fixture.lasers, 0, 350);
	return fixture.stones.flag == std::array<std::uint8_t, stone_count>{} &&
		fixture.stones.damage == std::array<std::int16_t, stone_count>{} &&
		fixture.stones.patnum == std::array<std::int16_t, stone_count>{
			148, 148, 148, 148, 148} &&
		fixture.stones.topleft == std::array<ScreenPoint, stone_count>{
			ScreenPoint{128, 32}, ScreenPoint{288, 32}, ScreenPoint{48, 48},
			ScreenPoint{368, 48}, ScreenPoint{208, 32}} &&
		fixture.stones.muzzle == ScreenPoint{216, 40} &&
		fixture.stones.tile_row == 46 && fixture.stones.kill_frame[0U] == 17 &&
		fixture.lasers.wait_frames == 12U && fixture.boss.explode_angle_offset == 0x20U &&
		fixture.boss.rank_param == std::array<std::uint8_t, 5U>{
			enemy_bullet_group::spread_3_wide_aimed, 2U,
			enemy_bullet_group::ring_4, enemy_bullet_group::ring_2, 0xA5U};
}

[[nodiscard]] bool test_inner_entrance_activates_on_frame_64() noexcept
{
	Fixture fixture{};
	stones_combat_reset(fixture.boss, fixture.stones, fixture.lasers, 1, 0);
	for(std::size_t frame = 0U; frame < 64U; ++frame) {
		if(stones_update(fixture.boss, fixture.stones, fixture.context,
			fixture.callbacks) != StonesUpdateResult::boss) {
			return false;
		}
	}
	return fixture.stones.phase == 1U && fixture.boss.phase_frame == 0 &&
		fixture.stones.flag[stone_inner_west] ==
			static_cast<std::uint8_t>(StoneFlag::active) &&
		fixture.stones.flag[stone_inner_east] ==
			static_cast<std::uint8_t>(StoneFlag::active) &&
		fixture.stones.patnum[stone_inner_west] == 155 &&
		fixture.stones.patnum[stone_inner_east] == 155;
}

[[nodiscard]] bool test_lethal_outer_hit_uses_item_and_kill_paths() noexcept
{
	Fixture fixture{};
	stones_combat_reset(fixture.boss, fixture.stones, fixture.lasers, 1, 0);
	fixture.stones.flag[stone_outer_west] =
		static_cast<std::uint8_t>(StoneFlag::active);
	fixture.stones.damage[stone_outer_west] = 119;
	add_plain_hit_shot(fixture, {60, 60});
	if(stones_update(fixture.boss, fixture.stones, fixture.context,
		fixture.callbacks) != StonesUpdateResult::boss) {
		return false;
	}
	return fixture.stones.flag[stone_outer_west] ==
		static_cast<std::uint8_t>(StoneFlag::kill_anim) &&
		fixture.stones.damage[stone_outer_west] == 120 &&
		fixture.score.score_delta == 30001 && fixture.items.items[0U].flag == item_alive &&
		fixture.items.items[0U].type == ItemType::big_power &&
		fixture.sparks.sparks[0U].flag == spark_alive &&
		fixture.callbacks.sound_count == 1U && fixture.callbacks.sounds[0U] == 2U;
}

[[nodiscard]] bool test_phase_four_lasers_use_laser_pool() noexcept
{
	Fixture fixture{};
	stones_combat_reset(fixture.boss, fixture.stones, fixture.lasers, 1, 0);
	fixture.stones.phase = 4U;
	fixture.stones.pattern = 0U;
	fixture.stones.flag[stone_north] =
		static_cast<std::uint8_t>(StoneFlag::active);
	fixture.boss.phase_frame = 49;
	if(stones_update(fixture.boss, fixture.stones, fixture.context,
		fixture.callbacks) != StonesUpdateResult::boss) {
		return false;
	}
	return fixture.lasers.wait_frames == 36U &&
		fixture.lasers.lasers[0U].origin == ScreenPoint{48, 96} &&
		fixture.lasers.lasers[1U].origin == ScreenPoint{192, 96} &&
		fixture.lasers.lasers[2U].origin == ScreenPoint{240, 96} &&
		fixture.lasers.lasers[3U].origin == ScreenPoint{384, 96} &&
		fixture.callbacks.sound_count == 4U && fixture.callbacks.sounds[0U] == 6U;
}

[[nodiscard]] bool test_terminal_and_invalid_hittest_refusal() noexcept
{
	Fixture terminal{};
	for(auto &flag : terminal.stones.flag) {
		flag = static_cast<std::uint8_t>(StoneFlag::removed);
	}
	if(stones_update(terminal.boss, terminal.stones, terminal.context,
		terminal.callbacks) != StonesUpdateResult::stage_clear ||
		terminal.boss.phase_frame != 1 || terminal.stones.timeout_frame != 1) {
		return false;
	}

	Fixture invalid{};
	invalid.stones.flag[stone_north] = static_cast<std::uint8_t>(StoneFlag::active);
	invalid.frame.page_back = 2U;
	return stones_update(invalid.boss, invalid.stones, invalid.context,
		invalid.callbacks) == StonesUpdateResult::invalid_shot_hittest &&
		invalid.boss.phase_frame == 1 && invalid.stones.timeout_frame == 1 &&
		invalid.stones.phase_frame_unused == 1;
}

} // namespace

int main()
{
	const std::array tests{
		test_shared_storage_and_combat_reset,
		test_inner_entrance_activates_on_frame_64,
		test_lethal_outer_hit_uses_item_and_kill_paths,
		test_phase_four_lasers_use_laser_pool,
		test_terminal_and_invalid_hittest_refusal,
	};
	for(std::size_t i = 0U; i < tests.size(); ++i) {
		if(!tests[i]()) {
			std::fprintf(stderr, "stones test %zu failed\n", i);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
