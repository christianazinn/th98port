#include "th02/core/midboss2.hpp"

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

enum class EventKind : std::uint8_t { invalidate, sound, sprite };

class RecordingCallbacks final : public Midboss2Callbacks {
public:
	RecordingCallbacks(const BossState *boss_arg,
		const PlayerFrameState *frame_arg) noexcept : boss(boss_arg), frame(frame_arg)
	{
	}

	const BossState *boss{};
	const PlayerFrameState *frame{};
	std::array<EventKind, 32U> events{};
	std::size_t event_count{};
	std::array<Midboss2Invalidation, 4U> invalidations{};
	std::size_t invalidation_count{};
	std::array<std::uint8_t, 8U> sounds{};
	std::size_t sound_count{};
	std::array<Midboss2Sprite, 16U> sprites{};
	std::size_t sprite_count{};
	ScreenPoint observed_back{};

	void invalidate_midboss(Midboss2Invalidation invalidation) noexcept override
	{
		record(EventKind::invalidate);
		if(boss != nullptr && frame != nullptr &&
			frame->page_back < boss->topleft_on_page.size()) {
			observed_back = boss->topleft_on_page[frame->page_back];
		}
		if(invalidation_count < invalidations.size()) {
			invalidations[invalidation_count] = invalidation;
		}
		++invalidation_count;
	}

	void sound_effect(std::uint8_t sound_id) noexcept override
	{
		record(EventKind::sound);
		if(sound_count < sounds.size()) sounds[sound_count] = sound_id;
		++sound_count;
	}

	void render_midboss(Midboss2Sprite sprite) noexcept override
	{
		record(EventKind::sprite);
		if(sprite_count < sprites.size()) sprites[sprite_count] = sprite;
		++sprite_count;
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
	Midboss2State state{};
	PlayerFrameState frame{};
	PlayerShotPoolState shots{};
	ScoreState score{};
	SparkPoolState sparks{};
	EnemyBulletPoolState bullets{};
	RandomState random{};
	StageBgFlashState flash{};
	Midboss2Context context;
	RecordingCallbacks callbacks;

	Fixture() noexcept :
		context{frame, shots, score, sparks, bullets, random, flash, 0, 1,
			false, 0U, 1U, 0U, 0},
		callbacks{&boss, &frame}
	{
		frame.page_front = front_page;
		frame.page_back = back_page;
		frame.player.current = {300, 300};
		frame.player.on_page[front_page] = {300, 300};
		frame.player.on_page[back_page] = {300, 300};
		boss.topleft_on_page = {{{100, 100}, {120, 100}}};
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
	shot.pos_on_page[back_page] = {
		to_subpixels(topleft.x), to_subpixels(topleft.y),
	};
	shot.patnum = 0x31U;
	shot.from_option = false;
}

[[nodiscard]] bool test_invalidate_carries_after_old_back_callback() noexcept
{
	Fixture fixture{};
	fixture.boss.topleft_on_page = {{{100, 200}, {300, 310}}};
	fixture.state.active = -1;
	if(midboss2_invalidate(fixture.boss, fixture.frame, fixture.state,
		fixture.callbacks) != Midboss2InvalidateResult::active ||
		fixture.callbacks.invalidation_count != 1U ||
		fixture.callbacks.observed_back != ScreenPoint{300, 310} ||
		fixture.callbacks.invalidations[0U] !=
			Midboss2Invalidation{{300, 310}, 64, 64} ||
		fixture.boss.topleft_on_page[back_page] != ScreenPoint{100, 200}) {
		return false;
	}

	const auto before = fixture.boss;
	fixture.frame.page_front = 2U;
	return midboss2_invalidate(fixture.boss, fixture.frame, fixture.state,
		fixture.callbacks) == Midboss2InvalidateResult::invalid_page &&
		fixture.boss == before && fixture.callbacks.invalidation_count == 1U;
}

[[nodiscard]] bool test_initialize_and_entry_back_page_motion() noexcept
{
	Fixture fixture{};
	fixture.boss.topleft_on_page[back_page] = {100, 200};
	if(midboss2_update_and_render(fixture.boss, fixture.state, fixture.context,
		fixture.callbacks) != Midboss2UpdateResult::completed ||
		fixture.boss.target != ScreenPoint{216, 224} ||
		fixture.boss.phase_frame != 1 || fixture.boss.damage != 0 ||
		fixture.boss.topleft_on_page != std::array<ScreenPoint, 2U>{{{192, -16},
			{192, -16}}} || fixture.state.active != 1 ||
		fixture.flash.frame != 1 || fixture.callbacks.event_count != 0U) {
		return false;
	}
	fixture.context.scroll_line = 5;
	if(midboss2_update_and_render(fixture.boss, fixture.state, fixture.context,
		fixture.callbacks) != Midboss2UpdateResult::completed) {
		return false;
	}
	return fixture.boss.target == ScreenPoint{216, 8} &&
		fixture.boss.phase_frame == 2 &&
		fixture.boss.topleft_on_page[front_page] == ScreenPoint{192, -16} &&
		fixture.boss.topleft_on_page[back_page] == ScreenPoint{192, -16} &&
		fixture.callbacks.sprite_count == 1U && fixture.callbacks.sprites[0U] ==
			Midboss2Sprite{Midboss2SpriteKind::normal, {192, 389}, 139, 0};
}

[[nodiscard]] bool test_entry_hit_uses_one_damage_per_frame() noexcept
{
	Fixture fixture{};
	fixture.boss.phase_frame = 10;
	fixture.boss.topleft_on_page[back_page] = {100, 100};
	add_plain_hit_shot(fixture, {120, 120});
	if(midboss2_update_and_render(fixture.boss, fixture.state, fixture.context,
		fixture.callbacks) != Midboss2UpdateResult::completed) {
		return false;
	}
	return fixture.boss.phase_frame == 11 && fixture.boss.damage == 1 &&
		fixture.callbacks.event_count == 2U &&
		fixture.callbacks.events[0U] == EventKind::sound &&
		fixture.callbacks.events[1U] == EventKind::sprite &&
		fixture.callbacks.sounds[0U] == 4U && fixture.callbacks.sprites[0U] ==
			Midboss2Sprite{Midboss2SpriteKind::hit_white, {100, 100}, 136, 0};
}

[[nodiscard]] bool test_living_pattern_uses_real_chase_bullet_without_rng() noexcept
{
	Fixture fixture{};
	fixture.boss.phase_frame = 383;
	fixture.boss.topleft_on_page[back_page] = {100, 100};
	fixture.random.cursor = 19U;
	const auto random_before = fixture.random;
	if(midboss2_update_and_render(fixture.boss, fixture.state, fixture.context,
		fixture.callbacks) != Midboss2UpdateResult::completed) {
		return false;
	}
	const auto &bullet = fixture.bullets.bullets[0U];
	return fixture.boss.phase_frame == 384 && bullet.flag == enemy_bullet_alive &&
		bullet.size_type == enemy_bullet_16x16 &&
		bullet.group_or_special_motion == enemy_bullet_special_motion::chase &&
		bullet.patnum == 84U && bullet.speed == 30U && bullet.angle == 0U &&
		bullet.screen_topleft_on_page[back_page] == ShotSubpixelPoint{2048, 2112} &&
		fixture.random == random_before && fixture.callbacks.sprite_count == 1U &&
		fixture.callbacks.sprites[0U] ==
			Midboss2Sprite{Midboss2SpriteKind::normal, {100, 100}, 137, 0};
}

[[nodiscard]] bool test_living_hit_threshold_and_scrolled_edge() noexcept
{
	Fixture killed{};
	killed.boss.phase_frame = 383;
	killed.boss.damage = 380;
	killed.boss.topleft_on_page[back_page] = {100, 100};
	add_plain_hit_shot(killed, {120, 120});
	if(midboss2_update_and_render(killed.boss, killed.state, killed.context,
		killed.callbacks) != Midboss2UpdateResult::completed ||
		killed.boss.damage != 381 || killed.boss.phase != 1U ||
		killed.score.score_delta != 20001 || killed.callbacks.sound_count != 1U ||
		killed.callbacks.sounds[0U] != 2U || killed.callbacks.sprite_count != 0U) {
		return false;
	}

	Fixture edge{};
	edge.boss.phase_frame = 383;
	edge.boss.damage = 380;
	edge.boss.topleft_on_page[back_page] = {100, 304};
	add_plain_hit_shot(edge, {120, 320});
	if(midboss2_update_and_render(edge.boss, edge.state, edge.context,
		edge.callbacks) != Midboss2UpdateResult::completed ||
		edge.boss.damage != 381 || edge.boss.phase != 0U ||
		edge.score.score_delta != 1 || edge.callbacks.sound_count != 1U ||
		edge.callbacks.sounds[0U] != 4U || edge.callbacks.sprite_count != 1U ||
		edge.callbacks.sprites[0U] !=
			Midboss2Sprite{Midboss2SpriteKind::hit_white, {100, 304}, 136, 0}) {
		return false;
	}

	Fixture leave{};
	leave.boss.phase_frame = 1600;
	return midboss2_update_and_render(leave.boss, leave.state, leave.context,
		leave.callbacks) == Midboss2UpdateResult::completed &&
		leave.boss.phase == 1U && leave.score.score_delta == 0 &&
		leave.callbacks.sound_count == 0U && leave.callbacks.sprite_count == 1U;
}

[[nodiscard]] bool test_defeat_sparks_terminal_and_refusal_prefix() noexcept
{
	Fixture progressing{};
	progressing.boss.phase = 1U;
	progressing.boss.phase_frame = 700;
	progressing.boss.topleft_on_page[back_page] = {200, 100};
	progressing.state.active = 1;
	progressing.state.defeat_frame = 5;
	progressing.context.scroll_line = 350;
	if(midboss2_update_and_render(progressing.boss, progressing.state,
		progressing.context, progressing.callbacks) != Midboss2UpdateResult::completed ||
		progressing.boss.phase_frame != 701 || progressing.state.defeat_frame != 6 ||
		progressing.sparks.sparks[0U].flag != spark_alive ||
		progressing.sparks.sparks[23U].flag != spark_alive ||
		progressing.callbacks.sprite_count != 1U ||
		progressing.callbacks.sprites[0U] != Midboss2Sprite{
			Midboss2SpriteKind::defeat_zoom, {200, 50}, 10, 2}) {
		return false;
	}

	Fixture terminal{};
	terminal.boss.phase = 1U;
	terminal.boss.phase_frame = 700;
	terminal.boss.topleft_on_page[back_page] = {200, 100};
	terminal.state.active = 1;
	terminal.state.defeat_frame = 63;
	if(midboss2_update_and_render(terminal.boss, terminal.state,
		terminal.context, terminal.callbacks) != Midboss2UpdateResult::completed ||
		terminal.boss.phase != 0U || terminal.boss.phase_frame != 0 ||
		terminal.boss.target != ScreenPoint{-1, -1} || terminal.state.active != 0 ||
		terminal.state.defeat_frame != 0 || terminal.callbacks.event_count != 0U) {
		return false;
	}

	Fixture refused{};
	refused.boss.phase = 1U;
	refused.boss.phase_frame = 700;
	refused.boss.topleft_on_page[back_page] = {200, 100};
	refused.state.defeat_frame = 5;
	refused.sparks.ring_index = 64U;
	return midboss2_update_and_render(refused.boss, refused.state,
		refused.context, refused.callbacks) ==
		Midboss2UpdateResult::defeat_spark_allocation_refused &&
		refused.boss.target == ScreenPoint{216, 124} &&
		refused.boss.phase_frame == 701 && refused.state.defeat_frame == 5 &&
		refused.callbacks.event_count == 0U;
}

} // namespace

int main()
{
	const std::array tests{
		test_invalidate_carries_after_old_back_callback,
		test_initialize_and_entry_back_page_motion,
		test_entry_hit_uses_one_damage_per_frame,
		test_living_pattern_uses_real_chase_bullet_without_rng,
		test_living_hit_threshold_and_scrolled_edge,
		test_defeat_sparks_terminal_and_refusal_prefix,
	};
	for(std::size_t i = 0U; i < tests.size(); ++i) {
		if(!tests[i]()) {
			std::fprintf(stderr, "midboss2 test %zu failed\n", i);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
