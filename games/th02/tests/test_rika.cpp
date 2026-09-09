#include "th02/core/rika.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using namespace th02::core;

constexpr std::size_t front_page = 0U;
constexpr std::size_t back_page = 1U;

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	));
}

enum class EventKind : std::uint8_t { invalidate, sprite, sound, explosion };

class RecordingCallbacks final : public RikaCallbacks {
public:
	std::array<EventKind, 64U> events{};
	std::size_t event_count{};
	std::array<RikaInvalidation, 4U> invalidations{};
	std::size_t invalidation_count{};
	std::array<RikaSprite, 8U> sprites{};
	std::size_t sprite_count{};
	std::array<std::uint8_t, 16U> sounds{};
	std::size_t sound_count{};
	std::size_t explosion_sprite_count{};
	std::size_t explosion_ring_count{};
	std::size_t explosion_invalidation_count{};
	std::size_t explosion_palette_show_count{};

	void invalidate_rika(RikaInvalidation invalidation) noexcept override
	{
		record(EventKind::invalidate);
		if(invalidation_count < invalidations.size()) {
			invalidations[invalidation_count] = invalidation;
		}
		++invalidation_count;
	}

	void render_rika(RikaSprite sprite) noexcept override
	{
		record(EventKind::sprite);
		if(sprite_count < sprites.size()) sprites[sprite_count] = sprite;
		++sprite_count;
	}

	void sound_effect(std::uint8_t id) noexcept override
	{
		record(EventKind::sound);
		if(sound_count < sounds.size()) sounds[sound_count] = id;
		++sound_count;
	}

	void explosion_margins_clear() noexcept override { record(EventKind::explosion); }
	void explosion_egc_begin() noexcept override { record(EventKind::explosion); }
	void explosion_egc_end() noexcept override { record(EventKind::explosion); }
	void explosion_ring(ScreenPoint, std::int16_t, bool) noexcept override
	{
		record(EventKind::explosion);
		++explosion_ring_count;
	}
	void explosion_color(std::uint8_t) noexcept override { record(EventKind::explosion); }
	void explosion_grcg_off() noexcept override { record(EventKind::explosion); }
	void explosion_invalidate(ScreenPoint) noexcept override
	{
		record(EventKind::explosion);
		++explosion_invalidation_count;
	}
	void explosion_tiles_render() noexcept override { record(EventKind::explosion); }
	void explosion_sprite(ScreenPoint, std::int16_t) noexcept override
	{
		record(EventKind::explosion);
		++explosion_sprite_count;
	}
	void explosion_palette_show() noexcept override
	{
		record(EventKind::explosion);
		++explosion_palette_show_count;
	}
	void explosion_graph_scroll(std::uint16_t, std::uint16_t,
		std::uint16_t) noexcept override
	{
		record(EventKind::explosion);
	}

private:
	void record(EventKind event) noexcept
	{
		if(event_count < events.size()) events[event_count] = event;
		++event_count;
	}
};

struct Fixture {
	BossState boss{};
	RikaState rika{};
	PlayerFrameState frame{};
	PlayerHitState hit{};
	PlayerShotPoolState shots{};
	ScoreState score{};
	SparkPoolState sparks{};
	EnemyBulletPoolState bullets{};
	LaserPoolState lasers{};
	RandomState random{};
	std::uint32_t stage_frame{};
	std::uint8_t slowdown_factor{};
	std::uint16_t palette_tone{};
	RikaCombatContext context;
	RecordingCallbacks callbacks{};

	Fixture() noexcept :
		context{frame, hit, shots, score, sparks, bullets, lasers, random,
			stage_frame, 0, 1, false, 0U, 1U, 0, slowdown_factor, palette_tone}
	{
		frame.page_front = front_page;
		frame.page_back = back_page;
		frame.player.current = {300, 300};
		frame.player.on_page[front_page] = {300, 300};
		frame.player.on_page[back_page] = {300, 300};
		boss.topleft_on_page = {{{100, 48}, {100, 48}}};
		rika.topleft = {100, 48};
		for(std::size_t i = 0U; i < random.ring.size(); ++i) {
			random.ring[i] = static_cast<std::uint8_t>(i);
		}
	}
};

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

void add_plain_hit_shot(Fixture &fixture, ScreenPoint topleft) noexcept
{
	auto &shot = fixture.shots.shots[0U];
	shot.flag = player_shot_alive;
	shot.decay_cel = 0U;
	shot.pos_on_page[back_page] = {
		to_subpixels(topleft.x), to_subpixels(topleft.y),
	};
	shot.patnum = 0x31U;
	shot.from_option = false;
}

[[nodiscard]] bool test_background_carries_only_x() noexcept
{
	Fixture fixture{};
	fixture.boss.topleft_on_page = {{{100, 12}, {300, 244}}};
	fixture.boss.phase_frame = 2;
	fixture.rika.move_state = 0;
	if(rika_bg_render(fixture.boss, fixture.rika, fixture.frame,
		fixture.callbacks) != RikaBackgroundResult::completed) {
		return false;
	}
	return fixture.callbacks.invalidation_count == 2U &&
		fixture.callbacks.invalidations[0U] ==
			RikaInvalidation{{300, 48}, 64, 96} &&
		fixture.callbacks.invalidations[1U] ==
			RikaInvalidation{{208, 16}, 32, 32} &&
		fixture.boss.topleft_on_page[back_page] == ScreenPoint{100, 244};
}

[[nodiscard]] bool test_target_precedes_state_1_phase_quirk() noexcept
{
	Fixture fixture{};
	fixture.boss.damage = 1400;
	fixture.boss.phase_frame = 191;
	fixture.boss.topleft_on_page[back_page] = {192, 48};
	fixture.rika.move_state = 1;
	if(rika_update(fixture.boss, fixture.rika, fixture.context,
		fixture.callbacks) != RikaUpdateResult::boss ||
		fixture.boss.target != ScreenPoint{124, 48} ||
		fixture.boss.phase_frame != 192 || fixture.rika.move_state != 2 ||
		fixture.rika.topleft.x != 192 || fixture.rika.pattern_frame != 1U) {
		return false;
	}
	return rika_update(fixture.boss, fixture.rika, fixture.context,
		fixture.callbacks) == RikaUpdateResult::boss &&
		fixture.boss.target == ScreenPoint{216, 48} &&
		fixture.boss.phase_frame == 0 && fixture.rika.pattern_frame == 2U;
}

[[nodiscard]] bool test_low_and_middle_patterns_use_real_bullet_groups() noexcept
{
	Fixture low{};
	low.boss.phase_frame = 63;
	low.rika.move_state = 3;
	low.boss.rank_param = {enemy_bullet_group::spread_3_medium_aimed,
		enemy_bullet_group::spread_5_wide, 0U, 0U, 0U};
	if(rika_update(low.boss, low.rika, low.context, low.callbacks) !=
		RikaUpdateResult::boss || low.rika.move_state != 0 ||
		alive_bullet_count(low.bullets) != 8U || low.callbacks.sound_count != 1U ||
		low.callbacks.sounds[0U] != 3U || low.callbacks.event_count < 2U ||
		low.callbacks.events[0U] != EventKind::sprite ||
		low.callbacks.events[1U] != EventKind::sound) {
		return false;
	}

	Fixture middle{};
	middle.boss.damage = 700;
	middle.rika.move_state = 3;
	middle.stage_frame = 0U;
	middle.boss.rank_param = {0U, 0U, enemy_bullet_group::spread_2_medium,
		enemy_bullet_group::one, 0U};
	if(rika_update(middle.boss, middle.rika, middle.context,
		middle.callbacks) != RikaUpdateResult::boss ||
		middle.rika.pattern_angle != 24U || alive_bullet_count(middle.bullets) != 6U ||
		middle.callbacks.sound_count != 1U || middle.callbacks.sounds[0U] != 3U) {
		return false;
	}
	return middle.bullets.bullets[0U].screen_topleft_on_page[back_page] ==
		ShotSubpixelPoint{1728, 1792} &&
		middle.bullets.bullets[3U].screen_topleft_on_page[back_page] ==
		ShotSubpixelPoint{2432, 1792};
}

[[nodiscard]] bool test_high_pattern_lasers_and_balls() noexcept
{
	Fixture lasers{};
	lasers.boss.damage = 1400;
	lasers.rika.move_state = 3;
	lasers.rika.pattern_frame = 99U;
	if(rika_update(lasers.boss, lasers.rika, lasers.context,
		lasers.callbacks) != RikaUpdateResult::boss ||
		lasers.rika.pattern_frame != 100U || lasers.lasers.wait_frames != 0x40U ||
		lasers.lasers.lasers[0U].origin != ScreenPoint{102, 112} ||
		lasers.lasers.lasers[1U].origin != ScreenPoint{146, 112} ||
		lasers.callbacks.sound_count != 2U || lasers.callbacks.sounds[0U] != 6U ||
		lasers.callbacks.sounds[1U] != 6U) {
		return false;
	}

	Fixture balls{};
	balls.boss.damage = 1400;
	balls.boss.rank_param[4U] = 3U;
	balls.rika.move_state = 3;
	balls.rika.pattern_frame = 279U;
	if(rika_update(balls.boss, balls.rika, balls.context, balls.callbacks) !=
		RikaUpdateResult::boss) {
		return false;
	}
	return balls.rika.pattern_frame == 280U &&
		balls.bullets.bullets[0U].flag == enemy_bullet_alive &&
		balls.bullets.bullets[0U].size_type == enemy_bullet_16x16 &&
		balls.bullets.bullets[0U].patnum == 19U;
}

[[nodiscard]] bool test_lethal_hit_still_runs_pattern() noexcept
{
	Fixture fixture{};
	fixture.boss.damage = 2260;
	fixture.rika.move_state = 3;
	fixture.rika.pattern_frame = 99U;
	add_plain_hit_shot(fixture, {120, 60});
	if(rika_update(fixture.boss, fixture.rika, fixture.context,
		fixture.callbacks) != RikaUpdateResult::boss) {
		return false;
	}
	return fixture.boss.phase == 1U && fixture.score.score_delta == 20001 &&
		fixture.hit.invincibility_time == 200U && fixture.callbacks.sprite_count == 0U &&
		fixture.rika.pattern_frame == 100U &&
		fixture.lasers.lasers[0U].flag == laser_alive &&
		fixture.lasers.lasers[1U].flag == laser_alive &&
		fixture.callbacks.sound_count == 2U && fixture.callbacks.sounds[0U] == 6U;
}

[[nodiscard]] bool test_defeat_effect_and_terminal_frame() noexcept
{
	Fixture progressing{};
	progressing.boss.phase = 1U;
	progressing.boss.phase_frame = 31;
	progressing.rika.defeat_frame = 0;
	if(rika_update(progressing.boss, progressing.rika, progressing.context,
		progressing.callbacks) != RikaUpdateResult::boss ||
		progressing.rika.defeat_frame != 1 ||
		progressing.sparks.sparks[0U].flag != spark_alive ||
		progressing.sparks.sparks[1U].flag != spark_alive ||
		progressing.callbacks.sprite_count != 1U ||
		progressing.callbacks.sprites[0U] != RikaSprite{
			RikaSpriteKind::defeat_zoom, {100, 64}, 2, 6} ||
		progressing.callbacks.sound_count != 1U || progressing.callbacks.sounds[0U] != 18U) {
		return false;
	}

	Fixture terminal{};
	terminal.boss.phase = 1U;
	terminal.rika.defeat_frame = 79;
	return rika_update(terminal.boss, terminal.rika, terminal.context,
		terminal.callbacks) == RikaUpdateResult::stage_clear &&
		terminal.rika.defeat_frame == 0 && terminal.callbacks.event_count == 0U &&
		terminal.sparks.sparks[0U].flag == spark_alive &&
		terminal.sparks.sparks[1U].flag == spark_alive;
}

[[nodiscard]] bool test_complete_defeat_effect_timeline() noexcept
{
	Fixture fixture{};
	fixture.boss.phase = 1U;
	for(std::int16_t call = 1; call < 80; ++call) {
		fixture.callbacks = {};
		if(rika_update(fixture.boss, fixture.rika, fixture.context,
			fixture.callbacks) != RikaUpdateResult::boss) {
			return false;
		}
		if(call == 32 && (fixture.slowdown_factor != 3U ||
			fixture.palette_tone != 100U)) {
			return false;
		}
		if(call == 40 && (fixture.slowdown_factor != 3U ||
			fixture.palette_tone != 100U)) {
			return false;
		}
		if(call == 53 && (fixture.slowdown_factor != 2U ||
			fixture.palette_tone != 100U)) {
			return false;
		}
		if(call == 56 && (fixture.slowdown_factor != 1U ||
			fixture.palette_tone != 100U)) {
			return false;
		}
	}
	fixture.callbacks = {};
	return rika_update(fixture.boss, fixture.rika, fixture.context,
		fixture.callbacks) == RikaUpdateResult::stage_clear &&
		fixture.rika.defeat_frame == 0 && fixture.callbacks.event_count == 0U &&
		fixture.slowdown_factor == 1U && fixture.palette_tone == 100U &&
		fixture.sparks.sparks[0U].flag == spark_alive;
}

[[nodiscard]] bool test_invalid_page_preserves_update_prefix() noexcept
{
	Fixture fixture{};
	fixture.frame.page_back = 2U;
	fixture.boss.phase_frame = 7;
	return rika_update(fixture.boss, fixture.rika, fixture.context,
		fixture.callbacks) == RikaUpdateResult::invalid_page &&
		fixture.boss.target == ScreenPoint{124, 48} &&
		fixture.boss.phase_frame == 8 && fixture.callbacks.event_count == 0U;
}

} // namespace

int main()
{
	const std::array tests{
		test_background_carries_only_x,
		test_target_precedes_state_1_phase_quirk,
		test_low_and_middle_patterns_use_real_bullet_groups,
		test_high_pattern_lasers_and_balls,
		test_lethal_hit_still_runs_pattern,
		test_defeat_effect_and_terminal_frame,
		test_complete_defeat_effect_timeline,
		test_invalid_page_preserves_update_prefix,
	};
	for(std::size_t i = 0U; i < tests.size(); ++i) {
		if(!tests[i]()) {
			std::fprintf(stderr, "rika test %zu failed\n", i);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
