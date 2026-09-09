#include "th02/core/enemy_update.hpp"

#include "th02/core/random.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using namespace th02::core;

constexpr std::size_t back_page = 1U;
using ScriptSlot = std::array<std::uint8_t, stage_script_size>;

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	));
}

class Callbacks final : public EnemyUpdateCallbacks {
public:
	std::array<std::uint8_t, 16U> sounds{};
	std::size_t sound_count{};
	std::array<EnemySprite, 32U> renders{};
	std::size_t render_count{};
	std::array<EnemySprite, 16U> hit_renders{};
	std::size_t hit_render_count{};
	std::size_t tile_count{};

	void sound_effect(std::uint8_t sound_id) noexcept override
	{
		if(sound_count < sounds.size()) sounds[sound_count] = sound_id;
		++sound_count;
	}

	void tile_ring_set_and_put_both_8(
		std::int16_t, std::int16_t, std::int16_t
	) noexcept override
	{
		++tile_count;
	}

	void render_enemy(EnemySprite sprite) noexcept override
	{
		if(render_count < renders.size()) renders[render_count] = sprite;
		++render_count;
	}

	void render_enemy_hit_white(EnemySprite sprite) noexcept override
	{
		if(hit_render_count < hit_renders.size()) {
			hit_renders[hit_render_count] = sprite;
		}
		++hit_render_count;
	}
};

struct Fixture {
	EnemyPoolState enemies{};
	EnemyUpdateState update{};
	std::array<EnemyTemplate, 3U> templates{};
	std::array<ScriptSlot, 3U> scripts{};
	std::array<std::uint8_t, 3U> script_lengths{};
	EnemyBulletPoolState bullets{};
	RandomState random{};
	PlayerFrameState frame{};
	PlayerHitState hit{};
	PlayerShotPoolState shots{};
	PlayerShotStreamState stream{};
	ScoreState score{};
	ItemPoolState items{};
	SparkPoolState sparks{};
	EnemyRunContext run;
	EnemyUpdateContext context;
	Callbacks callbacks{};

	Fixture() noexcept :
		run{bullets, random, frame, 0, 1, false, 0U, 0},
		context{{templates, scripts, script_lengths}, run, hit, shots, stream,
			score, items, sparks, 0U, 1U, 64U}
	{
		frame.page_front = 0U;
		frame.page_back = back_page;
		frame.player.current = {300, 300};
		frame.player.on_page[0U] = {300, 300};
		frame.player.on_page[1U] = {300, 300};
		for(std::size_t i = 0U; i < random.ring.size(); ++i) {
			random.ring[i] = static_cast<std::uint8_t>(i);
		}
		for(std::size_t i = 0U; i < templates.size(); ++i) {
			templates[i].script_index = static_cast<std::uint32_t>(i);
			templates[i].hp = 1;
			templates[i].patnum = 40;
			templates[i].item = static_cast<std::uint8_t>(ItemType::point);
			scripts[i][0U] = 0U;
			scripts[i][1U] = 1U;
			script_lengths[i] = 2U;
		}
	}

	[[nodiscard]] Enemy enemy(std::int16_t template_id = 0) const noexcept
	{
		Enemy value{};
		value.flag = enemy_alive;
		value.template_id = template_id;
		value.pos_on_page = {{{90, 90}, {100, 100}}};
		value.render_as = 1;
		value.pellet_group = enemy_bullet_group::one;
		value.pellet_speed = 32U;
		return value;
	}
};

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

[[nodiscard]] bool test_carried_finished_skips_kill_then_runs_next_slot() noexcept
{
	Fixture fixture{};
	fixture.enemies.enemies_loop_bound = 3U;
	fixture.enemies.enemies[0U] = fixture.enemy(0);
	fixture.enemies.enemies[1U] = fixture.enemy(1);
	fixture.enemies.enemies[1U].in_kill_anim = 1U;
	fixture.enemies.enemies[1U].age = 0;
	fixture.enemies.enemies[2U] = fixture.enemy(2);
	fixture.enemies.enemies[2U].pos_on_page[back_page] = {140, 150};
	fixture.scripts[0U][0U] = 16U;
	fixture.script_lengths[0U] = 1U;

	if(enemies_update_and_render(fixture.enemies, fixture.update, fixture.context,
		fixture.callbacks) != EnemyUpdateResult::completed) return false;
	return fixture.enemies.enemies[0U].flag == enemy_remove &&
		fixture.enemies.enemies[1U].age == 0 &&
		fixture.enemies.enemies[1U].flag == enemy_alive &&
		fixture.enemies.enemies[2U].anim_frame == 1U &&
		fixture.callbacks.render_count == 1U &&
		fixture.callbacks.renders[0U] == EnemySprite{{140, 150}, 40} &&
		fixture.update.boss_position == ScreenPoint{148, 158};
}

[[nodiscard]] bool test_vm_cap_counts_dispatched_opcodes() noexcept
{
	Fixture fixture{};
	fixture.enemies.enemies_loop_bound = 1U;
	fixture.enemies.enemies[0U] = fixture.enemy();
	fixture.scripts[0U][0U] = 160U;
	fixture.scripts[0U][1U] = 0U;
	fixture.script_lengths[0U] = 2U;
	fixture.context.max_vm_steps = 3U;

	return enemies_update_and_render(fixture.enemies, fixture.update,
		fixture.context, fixture.callbacks) == EnemyUpdateResult::vm_step_limit &&
		fixture.enemies.enemies[0U].script_ip == 0 &&
		fixture.enemies.enemies[0U].anim_frame == 0U &&
		fixture.callbacks.render_count == 0U;
}

[[nodiscard]] bool test_kill_animation_sparks_selector_and_retirement() noexcept
{
	Fixture fixture{};
	auto enemy = fixture.enemy();
	EnemyTemplate template_zero{};
	template_zero.explode_sprite = 0;
	const auto first = enemy_kill_update_and_render(enemy, template_zero,
		{100, 380}, back_page, 30, fixture.sparks, fixture.callbacks);
	if(first != EnemyKillUpdateResult::animating || enemy.age != 1 ||
		fixture.callbacks.render_count != 1U || fixture.callbacks.renders[0U] !=
		EnemySprite{{100, 10}, 10}) {
		return false;
	}
	for(std::size_t i = 0U; i < 8U; ++i) {
		if(fixture.sparks.sparks[i].flag != spark_alive ||
			fixture.sparks.sparks[i].velocity != SparkSubpixelPoint{20, 0}) {
			return false;
		}
	}

	fixture.callbacks.render_count = 0U;
	enemy = fixture.enemy();
	enemy.age = 3;
	EnemyTemplate template_nonzero{};
	template_nonzero.explode_sprite = 7;
	if(enemy_kill_update_and_render(enemy, template_nonzero, {100, 100},
		back_page, 0, fixture.sparks, fixture.callbacks) !=
		EnemyKillUpdateResult::animating || enemy.age != 4 ||
		fixture.callbacks.renders[0U] != EnemySprite{{100, 100}, 1}) {
		return false;
	}

	fixture.callbacks.render_count = 0U;
	enemy = fixture.enemy();
	enemy.age = 23;
	return enemy_kill_update_and_render(enemy, template_nonzero, {100, 100},
		back_page, 0, fixture.sparks, fixture.callbacks) ==
		EnemyKillUpdateResult::removed && enemy.flag == enemy_remove &&
		fixture.callbacks.render_count == 0U;
}

[[nodiscard]] bool test_animation_hp_minus_one_autofire_and_boss_target() noexcept
{
	Fixture fixture{};
	fixture.enemies.enemies_loop_bound = 2U;
	fixture.enemies.enemies[0U] = fixture.enemy(0);
	fixture.enemies.enemies[0U].in_kill_anim = 1U;
	fixture.enemies.enemies[0U].render_as = 0;
	fixture.enemies.enemies[0U].patnum_delta = 5;
	fixture.enemies.enemies[0U].anim_frame = 1U;
	fixture.enemies.enemies[0U].pos_on_page[back_page] = {100, 100};
	fixture.templates[0U].hp = -1;
	fixture.templates[0U].patnum = 20;
	fixture.templates[0U].autofire_interval = 2;

	fixture.enemies.enemies[1U] = fixture.enemy(1);
	fixture.enemies.enemies[1U].pos_on_page[back_page] = {150, 200};
	fixture.templates[1U].anim_cels = 2;
	fixture.templates[1U].anim_frames_per_cel = 1;
	fixture.enemies.enemies[1U].anim_frame = 255U;
	fixture.enemies.enemies[1U].patnum_delta = 1;

	if(enemies_update_and_render(fixture.enemies, fixture.update, fixture.context,
		fixture.callbacks) != EnemyUpdateResult::completed) return false;
	return fixture.sparks.sparks[0U].flag == spark_free &&
		fixture.bullets.bullets[0U].flag == enemy_bullet_alive &&
		fixture.callbacks.render_count == 2U &&
		fixture.callbacks.renders[0U] == EnemySprite{{100, 100}, 25} &&
		fixture.enemies.enemies[1U].anim_frame == 0U &&
		fixture.enemies.enemies[1U].patnum_delta == 0 &&
		fixture.update.boss_position == ScreenPoint{158, 208};
}

[[nodiscard]] bool test_death_drops_and_hp_minus_two_paths() noexcept
{
	Fixture lunatic{};
	lunatic.enemies.enemies_loop_bound = 1U;
	lunatic.enemies.enemies[0U] = lunatic.enemy();
	lunatic.templates[0U].hp = 1;
	lunatic.templates[0U].score = 1000;
	lunatic.templates[0U].item = static_cast<std::uint8_t>(ItemType::power);
	lunatic.context.run.rank = 3;
	add_plain_hit_shot(lunatic, {100, 100});
	if(enemies_update_and_render(lunatic.enemies, lunatic.update, lunatic.context,
		lunatic.callbacks) != EnemyUpdateResult::completed ||
		lunatic.enemies.enemies[0U].in_kill_anim != 1U ||
		lunatic.enemies.enemies[0U].age != 0 || lunatic.score.score_delta != 1001 ||
		lunatic.items.items[0U].flag != item_alive ||
		lunatic.items.semirandom_cycle != 1U ||
		lunatic.items.semirandom_ring_p != 1U || lunatic.random.cursor != 1U ||
		lunatic.bullets.bullets[0U].flag != enemy_bullet_alive ||
		lunatic.bullets.bullets[0U].size_type != enemy_bullet_pellet ||
		lunatic.bullets.bullets[0U].group_or_special_motion != 0U ||
		lunatic.bullets.bullets[0U].angle != 0U ||
		lunatic.bullets.bullets[0U].velocity != ShotSubpixelPoint{41, 43} ||
		lunatic.bullets.bullets[0U].screen_topleft_on_page[back_page] !=
			ShotSubpixelPoint{1600, 1600} || lunatic.callbacks.sound_count != 1U ||
		lunatic.callbacks.sounds[0U] != 3U) {
		return false;
	}

	Fixture skipped{};
	skipped.enemies.enemies_loop_bound = 1U;
	skipped.enemies.enemies[0U] = skipped.enemy();
	skipped.templates[0U].hp = 1;
	skipped.templates[0U].item = static_cast<std::uint8_t>(ItemType::power);
	skipped.items.semirandom_cycle = 1U;
	add_plain_hit_shot(skipped, {100, 100});
	if(enemies_update_and_render(skipped.enemies, skipped.update,
		skipped.context, skipped.callbacks) != EnemyUpdateResult::completed ||
		skipped.items.items[0U].flag != item_free ||
		skipped.items.semirandom_cycle != 2U || skipped.random.cursor != 0U ||
		skipped.items.semirandom_ring_p != 0U) {
		return false;
	}

	Fixture ordinary{};
	ordinary.enemies.enemies_loop_bound = 1U;
	ordinary.enemies.enemies[0U] = ordinary.enemy();
	ordinary.templates[0U].hp = 1;
	ordinary.templates[0U].item = static_cast<std::uint8_t>(ItemType::big_power);
	add_plain_hit_shot(ordinary, {100, 100});
	if(enemies_update_and_render(ordinary.enemies, ordinary.update,
		ordinary.context, ordinary.callbacks) != EnemyUpdateResult::completed ||
		ordinary.items.items[0U].flag != item_alive ||
		ordinary.items.items[0U].type != ItemType::point ||
		ordinary.random.cursor != 0U) {
		return false;
	}

	Fixture special{};
	special.enemies.enemies_loop_bound = 1U;
	special.enemies.enemies[0U] = special.enemy();
	special.templates[0U].hp = -2;
	special.templates[0U].item = static_cast<std::uint8_t>(ItemType::big_power);
	add_plain_hit_shot(special, {100, 100});
	return enemies_update_and_render(special.enemies, special.update,
		special.context, special.callbacks) == EnemyUpdateResult::completed &&
		special.enemies.enemies[0U].damage == 0 &&
		special.enemies.enemies[0U].in_kill_anim == 0U &&
		special.items.items[0U].flag == item_free &&
		special.callbacks.sound_count == 1U && special.callbacks.sounds[0U] == 4U &&
		special.callbacks.render_count == 1U && special.callbacks.hit_render_count == 0U;
}

[[nodiscard]] bool test_nonlethal_hit_flashes_and_skips_autofire() noexcept
{
	Fixture fixture{};
	fixture.enemies.enemies_loop_bound = 1U;
	fixture.enemies.enemies[0U] = fixture.enemy();
	fixture.templates[0U].hp = 2;
	fixture.templates[0U].autofire_interval = 2;
	fixture.enemies.enemies[0U].anim_frame = 1U;
	add_plain_hit_shot(fixture, {100, 100});

	if(enemies_update_and_render(fixture.enemies, fixture.update,
		fixture.context, fixture.callbacks) != EnemyUpdateResult::completed) {
		return false;
	}
	return fixture.enemies.enemies[0U].damage == 1 &&
		fixture.enemies.enemies[0U].in_kill_anim == 0U &&
		fixture.callbacks.sound_count == 1U && fixture.callbacks.sounds[0U] == 4U &&
		fixture.callbacks.hit_render_count == 1U &&
		fixture.callbacks.hit_renders[0U] == EnemySprite{{100, 100}, 40} &&
		fixture.callbacks.render_count == 0U &&
		fixture.bullets.bullets[0U].flag == enemy_bullet_free;
}

[[nodiscard]] bool removal_matches_at(
	std::int16_t x, std::int16_t y, std::uint8_t vertical_despawn,
	bool expected_removed
) noexcept
{
	Fixture fixture{};
	fixture.enemies.enemies_loop_bound = 1U;
	fixture.enemies.enemies[0U] = fixture.enemy();
	fixture.enemies.enemies[0U].pos_on_page[back_page] = {x, y};
	fixture.enemies.enemies[0U].despawn_when_offscreen_vertically =
		vertical_despawn;
	fixture.templates[0U].hp = -1;
	fixture.templates[0U].patnum = 0;
	return enemies_update_and_render(fixture.enemies, fixture.update,
		fixture.context, fixture.callbacks) == EnemyUpdateResult::completed &&
		(fixture.enemies.enemies[0U].flag == enemy_remove) == expected_removed;
}

[[nodiscard]] bool test_clip_and_vertical_despawn_edges() noexcept
{
	return removal_matches_at(0, 100, 0U, false) &&
		removal_matches_at(418, 100, 0U, false) &&
		removal_matches_at(-1, 100, 0U, true) &&
		removal_matches_at(419, 100, 0U, true) &&
		removal_matches_at(100, -16, 0U, false) &&
		removal_matches_at(100, 384, 0U, false) &&
		removal_matches_at(100, -16, 1U, true) &&
		removal_matches_at(100, 384, 1U, true);
}

[[nodiscard]] bool test_render_as_two_uses_absolute_patnum_delta() noexcept
{
	Fixture fixture{};
	fixture.enemies.enemies_loop_bound = 1U;
	fixture.enemies.enemies[0U] = fixture.enemy();
	fixture.enemies.enemies[0U].render_as = 2;
	fixture.enemies.enemies[0U].patnum_delta = 77;
	fixture.templates[0U].hp = -1;
	fixture.templates[0U].patnum = 40;

	return enemies_update_and_render(fixture.enemies, fixture.update,
		fixture.context, fixture.callbacks) == EnemyUpdateResult::completed &&
		fixture.callbacks.render_count == 1U &&
		fixture.callbacks.renders[0U] == EnemySprite{{100, 100}, 77};
}

[[nodiscard]] bool test_vm_and_animation_refusals_preserve_prefix() noexcept
{
	Fixture opcode{};
	opcode.enemies.enemies_loop_bound = 1U;
	opcode.enemies.enemies[0U] = opcode.enemy();
	opcode.update.boss_position = {7, 8};
	opcode.scripts[0U][0U] = 0xFEU;
	opcode.script_lengths[0U] = 1U;
	if(enemies_update_and_render(opcode.enemies, opcode.update, opcode.context,
		opcode.callbacks) != EnemyUpdateResult::unsupported_opcode ||
		opcode.update.boss_position != ScreenPoint{-1, -1} ||
		opcode.enemies.enemies[0U].anim_frame != 0U ||
		opcode.callbacks.render_count != 0U) {
		return false;
	}

	Fixture divisor{};
	divisor.enemies.enemies_loop_bound = 1U;
	divisor.enemies.enemies[0U] = divisor.enemy();
	divisor.templates[0U].anim_cels = 1;
	divisor.templates[0U].anim_frames_per_cel = 0;
	return enemies_update_and_render(divisor.enemies, divisor.update,
		divisor.context, divisor.callbacks) == EnemyUpdateResult::source_divide_fault &&
		divisor.enemies.enemies[0U].anim_frame == 1U &&
		divisor.callbacks.render_count == 0U;
}

} // namespace

int main()
{
	if(!test_carried_finished_skips_kill_then_runs_next_slot()) {
		std::fputs("FAIL enemy update carried status\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_vm_cap_counts_dispatched_opcodes()) {
		std::fputs("FAIL enemy update VM cap\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_kill_animation_sparks_selector_and_retirement()) {
		std::fputs("FAIL enemy update kill animation\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_animation_hp_minus_one_autofire_and_boss_target()) {
		std::fputs("FAIL enemy update animation/autofire\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_death_drops_and_hp_minus_two_paths()) {
		std::fputs("FAIL enemy update damage/drop\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_nonlethal_hit_flashes_and_skips_autofire()) {
		std::fputs("FAIL enemy update nonlethal hit\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_clip_and_vertical_despawn_edges()) {
		std::fputs("FAIL enemy update clip edges\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_render_as_two_uses_absolute_patnum_delta()) {
		std::fputs("FAIL enemy update render selector\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_vm_and_animation_refusals_preserve_prefix()) {
		std::fputs("FAIL enemy update refusal prefix\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
