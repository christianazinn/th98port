#include "th02/core/marisa.hpp"

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

class RecordingCallbacks final : public MarisaCallbacks {
public:
	std::array<std::uint8_t, 64U> sounds{};
	std::size_t sound_count{};
	std::size_t marisa_count{};
	std::size_t orb_count{};
	std::size_t line_count{};
	std::size_t front_clears{};

	void sound_effect(std::uint8_t id) noexcept override
	{
		if(sound_count < sounds.size()) sounds[sound_count] = id;
		++sound_count;
	}
	void render_marisa(MarisaSprite) noexcept override { ++marisa_count; }
	void render_orb_line(ScreenPoint, ScreenPoint) noexcept override { ++line_count; }
	void render_orb(MarisaOrbSprite) noexcept override { ++orb_count; }
	void clear_orb_on_front_page(std::size_t, ScreenPoint) noexcept override
	{
		++front_clears;
	}
	void explosion_margins_clear() noexcept override {}
	void explosion_egc_begin() noexcept override {}
	void explosion_egc_end() noexcept override {}
	void explosion_ring(ScreenPoint, std::int16_t, bool) noexcept override {}
	void explosion_color(std::uint8_t) noexcept override {}
	void explosion_grcg_off() noexcept override {}
	void explosion_invalidate(ScreenPoint) noexcept override {}
	void explosion_tiles_render() noexcept override {}
	void explosion_sprite(ScreenPoint, std::int16_t) noexcept override {}
	void explosion_palette_show() noexcept override {}
	void explosion_graph_scroll(std::uint16_t, std::uint16_t,
		std::uint16_t) noexcept override {}
};

class ParticleCallbacks final : public BgParticleCallbacks {
public:
	void invalidate_scenery(StageSceneryInvalidation) noexcept override {}
	void palette_set(std::uint8_t, StageSceneryRgb) noexcept override {}
	void palette_tone_set(std::uint16_t) noexcept override {}
	void palette_show() noexcept override {}
	void grc_set_clip(std::int16_t, std::int16_t, std::int16_t,
		std::int16_t) noexcept override {}
	void egc_off() noexcept override {}
	void grcg_set_color(std::uint8_t) noexcept override {}
	void dot_square(ScreenPoint, std::uint8_t) noexcept override {}
	void dot_square_unput(ScreenPoint, std::uint8_t) noexcept override {}
	void grcg_off() noexcept override {}
};

struct Fixture {
	BossState boss{};
	MarisaState marisa{};
	PlayerFrameState frame{};
	PlayerHitState hit{};
	PlayerShotPoolState shots{};
	ItemPoolState items{};
	ScoreState score{};
	SparkPoolState sparks{};
	EnemyBulletPoolState bullets{};
	EnemyBulletSpecialState bullet_special{};
	RandomState random{};
	BgParticleState background_particles{};
	std::uint32_t stage_frame{};
	std::uint8_t bomb_damage_frame_mask{1U};
	std::uint8_t slowdown{};
	std::uint16_t tone{};
	ParticleCallbacks particle_callbacks{};
	MarisaCombatContext context;
	RecordingCallbacks callbacks{};

	Fixture() noexcept :
		context{frame, hit, shots, items, score, sparks, bullets, bullet_special,
			random, background_particles, particle_callbacks, stage_frame, false, 0,
			1, false, 0U, bomb_damage_frame_mask, 0U, 0, slowdown, tone}
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
	shot.pos_on_page[back_page] = {to_subpixels(topleft.x),
		to_subpixels(topleft.y)};
	shot.patnum = 0x31U;
}

[[nodiscard]] std::size_t alive_bullet_count(
	const EnemyBulletPoolState &bullets
) noexcept
{
	std::size_t count{};
	for(const auto &bullet : bullets.bullets) {
		if(bullet.flag == enemy_bullet_alive) ++count;
	}
	return count;
}

[[nodiscard]] bool test_shared_state_and_reset() noexcept
{
	static_assert(std::is_same_v<decltype(MarisaState{}.topleft), ScreenPoint>);
	static_assert(std::is_same_v<decltype(MarisaState{}.intro_step), std::int16_t>);
	static_assert(std::is_same_v<decltype(MarisaState{}.intro_direction), std::int16_t>);
	static_assert(std::is_same_v<decltype(MarisaState{}.hit_flash), bool>);
	Fixture fixture{};
	fixture.marisa.pattern_side = -1;
	fixture.marisa.star_drift_angle = 0xFEU;
	fixture.marisa.orb_kill_frame[0U] = 9;
	marisa_combat_reset(fixture.boss, fixture.marisa, 0);
	return fixture.marisa.topleft == ScreenPoint{176, 64} &&
		fixture.boss.topleft_on_page == std::array<ScreenPoint, 2U>{
			ScreenPoint{176, 64}, ScreenPoint{176, 64}} &&
		fixture.marisa.intro_step == 0 && fixture.marisa.intro_direction == 0 &&
		!fixture.marisa.hit_flash && fixture.marisa.orb_flag ==
			std::array<std::int16_t, marisa_orb_count>{2, 2, 2, 2} &&
		fixture.marisa.bg_particle_col_i == 0 &&
		fixture.marisa.orb_kill_frame[0U] == 9 && fixture.marisa.pattern_side == -1 &&
		fixture.marisa.star_drift_angle == 0xFEU && fixture.boss.rank_param ==
			std::array<std::uint8_t, 5U>{enemy_bullet_group::spread_5_wide_aimed,
			enemy_bullet_group::spread_4_wide_aimed, enemy_bullet_group::one, 2U, 0U};
}

[[nodiscard]] bool test_background_prefix_consumes_rng_before_combat() noexcept
{
	Fixture fixture{};
	marisa_combat_reset(fixture.boss, fixture.marisa, 1);
	fixture.context.stage_frame = 1U;
	fixture.background_particles.speed_initial = 24;
	fixture.background_particles.speed_delta = 4;
	fixture.background_particles.edge_step = 24;
	fixture.background_particles.color = 9U;
	fixture.random.ring[0U] = 0x34U;
	fixture.random.ring[1U] = 0x12U;
	if(marisa_update(fixture.boss, fixture.marisa, fixture.context,
		fixture.callbacks) != MarisaUpdateResult::boss) return false;
	const auto &particle = fixture.background_particles.particles[0U];
	return fixture.random.cursor == 1U && particle.flag == bg_particle_alive &&
		particle.speed == 28U && fixture.boss.phase_frame == 1;
}

[[nodiscard]] bool test_intro_spawns_orbs_in_source_timing() noexcept
{
	Fixture fixture{};
	marisa_combat_reset(fixture.boss, fixture.marisa, 1);
	for(std::size_t frame = 0U; frame < 130U; ++frame) {
		if(marisa_update(fixture.boss, fixture.marisa, fixture.context,
			fixture.callbacks) != MarisaUpdateResult::boss) return false;
	}
	if(fixture.marisa.intro_step != 1 || fixture.boss.phase_frame != 0) return false;
	for(std::size_t frame = 0U; frame < 154U; ++frame) {
		if(marisa_update(fixture.boss, fixture.marisa, fixture.context,
			fixture.callbacks) != MarisaUpdateResult::boss) return false;
	}
	return fixture.marisa.intro_step == 2 && fixture.marisa.pattern == 1 &&
		fixture.marisa.orb_flag == std::array<std::int16_t, marisa_orb_count>{0, 0, 0, 0} &&
		fixture.marisa.orb_radius == std::array<std::int16_t, marisa_orb_count>{54, 54, 54, 54};
}

[[nodiscard]] bool test_orb_hit_uses_real_item_score_and_spark_paths() noexcept
{
	Fixture fixture{};
	marisa_combat_reset(fixture.boss, fixture.marisa, 1);
	fixture.marisa.intro_step = 2;
	fixture.marisa.orb_flag[0U] = static_cast<std::int16_t>(MarisaOrbFlag::alive);
	fixture.marisa.orb_damage[0U] = 109;
	fixture.marisa.orb_on_page[back_page][0U] = {300, 100};
	fixture.marisa.orb_on_page[front_page][0U] = {300, 100};
	add_plain_hit_shot(fixture, {310, 110});
	if(marisa_update(fixture.boss, fixture.marisa, fixture.context,
		fixture.callbacks) != MarisaUpdateResult::boss) return false;
	return fixture.marisa.orb_flag[0U] ==
		static_cast<std::int16_t>(MarisaOrbFlag::kill_anim) &&
		fixture.marisa.orb_damage[0U] == 110 && fixture.score.score_delta == 5001 &&
		fixture.items.items[0U].flag == item_alive &&
		fixture.items.items[0U].type == ItemType::big_power &&
		fixture.callbacks.sound_count == 1U && fixture.callbacks.sounds[0U] == 2U &&
		fixture.marisa.orb_kill_frame[0U] == 1;
}

[[nodiscard]] bool test_ranked_spread_pattern_allocates_real_bullets() noexcept
{
	Fixture fixture{};
	marisa_combat_reset(fixture.boss, fixture.marisa, 1);
	fixture.marisa.intro_step = 2;
	fixture.marisa.pattern = -2;
	fixture.boss.phase_frame = 71;
	if(marisa_update(fixture.boss, fixture.marisa, fixture.context,
		fixture.callbacks) != MarisaUpdateResult::boss) return false;
	return alive_bullet_count(fixture.bullets) == 9U;
}

[[nodiscard]] bool test_defeat_and_invalid_page_prefix() noexcept
{
	Fixture defeat{};
	marisa_combat_reset(defeat.boss, defeat.marisa, 1);
	defeat.boss.phase = 1U;
	defeat.marisa.defeat_frame = 99;
	if(marisa_update(defeat.boss, defeat.marisa, defeat.context,
		defeat.callbacks) != MarisaUpdateResult::stage_clear ||
		defeat.boss.phase != 3U || defeat.marisa.defeat_frame != 0) return false;

	Fixture invalid{};
	marisa_combat_reset(invalid.boss, invalid.marisa, 1);
	invalid.frame.page_back = 2U;
	return marisa_update(invalid.boss, invalid.marisa, invalid.context,
		invalid.callbacks) == MarisaUpdateResult::invalid_page &&
		invalid.boss.target == ScreenPoint{216, 104} && invalid.boss.phase_frame == 1;
}

} // namespace

int main()
{
	const std::array tests{
		test_shared_state_and_reset,
		test_background_prefix_consumes_rng_before_combat,
		test_intro_spawns_orbs_in_source_timing,
		test_orb_hit_uses_real_item_score_and_spark_paths,
		test_ranked_spread_pattern_allocates_real_bullets,
		test_defeat_and_invalid_page_prefix,
	};
	for(std::size_t i = 0U; i < tests.size(); ++i) {
		if(!tests[i]()) {
			std::fprintf(stderr, "marisa test %zu failed\n", i);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
