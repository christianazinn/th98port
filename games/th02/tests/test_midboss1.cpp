#include "th02/core/midboss1.hpp"

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

enum class EventKind : std::uint8_t { invalidate, tile, sound, sprite };

class RecordingCallbacks final : public Midboss1Callbacks {
public:
	explicit RecordingCallbacks(
		const BossState *boss_arg = nullptr,
		const PlayerFrameState *frame_arg = nullptr
	) noexcept : boss(boss_arg), frame(frame_arg)
	{
	}

	const BossState *boss{};
	const PlayerFrameState *frame{};
	std::array<EventKind, 32U> events{};
	std::size_t event_count{};
	std::array<Midboss1Invalidation, 4U> invalidations{};
	std::size_t invalidation_count{};
	std::array<std::array<std::int16_t, 3U>, 16U> tiles{};
	std::size_t tile_count{};
	std::array<std::uint8_t, 8U> sounds{};
	std::size_t sound_count{};
	std::array<Midboss1Sprite, 16U> sprites{};
	std::size_t sprite_count{};
	ScreenPoint observed_back{};

	void invalidate_midboss(Midboss1Invalidation invalidation) noexcept override
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

	void tile_ring_set_and_put_both_8(
		std::int16_t left, std::int16_t top, std::int16_t tile
	) noexcept override
	{
		record(EventKind::tile);
		if(tile_count < tiles.size()) tiles[tile_count] = {left, top, tile};
		++tile_count;
	}

	void sound_effect(std::uint8_t sound_id) noexcept override
	{
		record(EventKind::sound);
		if(sound_count < sounds.size()) sounds[sound_count] = sound_id;
		++sound_count;
	}

	void render_midboss(Midboss1Sprite sprite) noexcept override
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
	Midboss1State state{};
	PlayerFrameState frame{};
	PlayerShotPoolState shots{};
	ScoreState score{};
	SparkPoolState sparks{};
	EnemyBulletPoolState bullets{};
	RandomState random{};
	Midboss1Context context;
	RecordingCallbacks callbacks;

	Fixture() noexcept :
		context{frame, shots, score, sparks, bullets, random, 0, 1, false,
			0U, 1U, 0U, 0, 0},
		callbacks(&boss, &frame)
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
	if(midboss1_invalidate(fixture.boss, fixture.frame, fixture.state,
		fixture.callbacks) != Midboss1InvalidateResult::active ||
		fixture.callbacks.invalidation_count != 1U ||
		fixture.callbacks.observed_back != ScreenPoint{300, 310} ||
		fixture.callbacks.invalidations[0U] !=
			Midboss1Invalidation{{300, 310}, 64, 96} ||
		fixture.boss.topleft_on_page[back_page] != ScreenPoint{100, 200}) {
		return false;
	}

	fixture.boss.topleft_on_page[back_page] = {300, 310};
	fixture.boss.phase = 1U;
	fixture.state.active = 0;
	return midboss1_invalidate(fixture.boss, fixture.frame, fixture.state,
		fixture.callbacks) == Midboss1InvalidateResult::inactive &&
		fixture.callbacks.invalidation_count == 2U &&
		fixture.callbacks.invalidations[1U] ==
			Midboss1Invalidation{{284, 310}, 96, 96};
}

[[nodiscard]] bool test_invalid_page_follows_initialization_prefix() noexcept
{
	Fixture fixture{};
	fixture.frame.page_back = 2U;
	if(midboss1_update_and_render(fixture.boss, fixture.state, fixture.context,
		fixture.callbacks) != Midboss1UpdateResult::completed ||
		fixture.boss.phase_frame != 1 || fixture.boss.target != ScreenPoint{216, 64} ||
		fixture.boss.topleft_on_page != std::array<ScreenPoint, 2U>{{{192, -16},
			{192, -16}}} || fixture.state.active != 1 ||
		fixture.callbacks.event_count != 0U) {
		return false;
	}
	return midboss1_update_and_render(fixture.boss, fixture.state,
		fixture.context, fixture.callbacks) == Midboss1UpdateResult::invalid_page &&
		fixture.boss.phase_frame == 2 && fixture.boss.target == ScreenPoint{216, 64} &&
		fixture.callbacks.event_count == 0U;
}

[[nodiscard]] bool test_initialize_then_enter_on_back_page() noexcept
{
	Fixture fixture{};
	fixture.context.scroll_line = 5;
	if(midboss1_update_and_render(fixture.boss, fixture.state, fixture.context,
		fixture.callbacks) != Midboss1UpdateResult::completed ||
		fixture.boss.target != ScreenPoint{216, 64} ||
		fixture.boss.phase_frame != 1 || fixture.boss.damage != 0 ||
		fixture.boss.topleft_on_page[front_page] != ScreenPoint{192, -16} ||
		fixture.boss.topleft_on_page[back_page] != ScreenPoint{192, -16} ||
		fixture.state.patnum != 148 || fixture.state.active != 1 ||
		fixture.state.scenery_frame != 1 || fixture.callbacks.event_count != 0U) {
		return false;
	}
	if(midboss1_update_and_render(fixture.boss, fixture.state, fixture.context,
		fixture.callbacks) != Midboss1UpdateResult::completed) {
		return false;
	}
	return fixture.boss.phase_frame == 2 &&
		fixture.boss.topleft_on_page[front_page] == ScreenPoint{192, -16} &&
		fixture.boss.topleft_on_page[back_page] == ScreenPoint{192, -14} &&
		fixture.callbacks.sprite_count == 1U && fixture.callbacks.sprites[0U] ==
			Midboss1Sprite{Midboss1SpriteKind::normal, {192, 391}, 148, 0};
}

[[nodiscard]] bool test_pattern_uses_real_pellets_before_tiles_and_render() noexcept
{
	Fixture fixture{};
	fixture.boss.phase_frame = 127;
	fixture.boss.topleft_on_page[back_page] = {100, 100};
	fixture.context.scroll_line = 16;
	if(midboss1_update_and_render(fixture.boss, fixture.state, fixture.context,
		fixture.callbacks) != Midboss1UpdateResult::completed) {
		return false;
	}
	const auto &bullet = fixture.bullets.bullets[0U];
	return fixture.boss.phase_frame == 128 &&
		bullet.flag == enemy_bullet_alive &&
		fixture.bullets.bullets[3U].flag == enemy_bullet_alive &&
		bullet.screen_topleft_on_page[back_page] == ShotSubpixelPoint{2048, 3008} &&
		fixture.callbacks.tile_count == 4U &&
		fixture.callbacks.tiles[0U] == std::array<std::int16_t, 3U>{100, 64, 60} &&
		fixture.callbacks.tiles[3U] == std::array<std::int16_t, 3U>{148, 64, 63} &&
		fixture.callbacks.event_count == 5U &&
		fixture.callbacks.events[0U] == EventKind::tile &&
		fixture.callbacks.events[3U] == EventKind::tile &&
		fixture.callbacks.events[4U] == EventKind::sprite &&
		fixture.callbacks.sprites[0U] ==
			Midboss1Sprite{Midboss1SpriteKind::normal, {100, 116}, 148, 0};
}

[[nodiscard]] bool test_hit_defeat_and_scroll_forced_defeat_order() noexcept
{
	Fixture hit{};
	hit.boss.phase_frame = 17;
	hit.boss.damage = 300;
	hit.boss.topleft_on_page[back_page] = {100, 100};
	add_plain_hit_shot(hit, {120, 120});
	if(midboss1_update_and_render(hit.boss, hit.state, hit.context,
		hit.callbacks) != Midboss1UpdateResult::completed || hit.boss.damage != 301 ||
		hit.boss.phase != 1U || hit.score.score_delta != 10001 ||
		hit.callbacks.sound_count != 1U || hit.callbacks.sounds[0U] != 2U ||
		hit.callbacks.sprite_count != 0U || hit.callbacks.tile_count != 4U) {
		return false;
	}

	Fixture edge{};
	edge.boss.phase_frame = 17;
	edge.boss.damage = 300;
	edge.boss.topleft_on_page[back_page] = {100, 304};
	add_plain_hit_shot(edge, {120, 320});
	if(midboss1_update_and_render(edge.boss, edge.state, edge.context,
		edge.callbacks) != Midboss1UpdateResult::completed || edge.boss.phase != 0U ||
		edge.boss.damage != 301 || edge.score.score_delta != 1 ||
		edge.callbacks.sounds[0U] != 4U || edge.callbacks.sprite_count != 1U ||
		edge.callbacks.sprites[0U] !=
			Midboss1Sprite{Midboss1SpriteKind::hit_white, {100, 304}, 148, 0}) {
		return false;
	}

	Fixture forced{};
	forced.boss.phase_frame = 17;
	forced.boss.topleft_on_page[back_page] = {100, 304};
	forced.context.scroll_step = 184;
	return midboss1_update_and_render(forced.boss, forced.state, forced.context,
		forced.callbacks) == Midboss1UpdateResult::completed &&
		forced.boss.phase == 1U && forced.callbacks.sound_count == 1U &&
		forced.callbacks.sounds[0U] == 2U && forced.callbacks.sprite_count == 0U;
}

[[nodiscard]] bool test_defeat_terminal_and_refusal_prefix() noexcept
{
	Fixture progressing{};
	progressing.boss.phase = 1U;
	progressing.boss.phase_frame = 17;
	progressing.boss.topleft_on_page[back_page] = {200, 100};
	progressing.state.defeat_frame = 3;
	progressing.context.scroll_line = 350;
	if(midboss1_update_and_render(progressing.boss, progressing.state,
		progressing.context, progressing.callbacks) != Midboss1UpdateResult::completed ||
		progressing.state.defeat_frame != 4 ||
		progressing.boss.topleft_on_page[back_page] != ScreenPoint{200, 101} ||
		progressing.sparks.sparks[0U].flag != spark_alive ||
		progressing.callbacks.sprite_count != 1U ||
		progressing.callbacks.sprites[0U] != Midboss1Sprite{
			Midboss1SpriteKind::defeat_zoom, {184, 51}, 3, 10}) {
		return false;
	}

	Fixture terminal{};
	terminal.boss.phase = 1U;
	terminal.boss.phase_frame = 17;
	terminal.boss.topleft_on_page[back_page] = {200, 100};
	terminal.state.defeat_frame = 63;
	terminal.state.active = 1;
	if(midboss1_update_and_render(terminal.boss, terminal.state,
		terminal.context, terminal.callbacks) != Midboss1UpdateResult::completed ||
		terminal.state.defeat_frame != 0 || terminal.state.active != 0 ||
		terminal.boss.phase_frame != 0 || terminal.boss.target != ScreenPoint{-1, -1} ||
		terminal.callbacks.event_count != 0U) {
		return false;
	}
	std::size_t alive{};
	for(const auto &spark : terminal.sparks.sparks) {
		if(spark.flag == spark_alive) ++alive;
	}
	if(alive != 49U) return false;

	Fixture refused{};
	refused.boss.phase = 1U;
	refused.boss.phase_frame = 17;
	refused.state.defeat_frame = 0;
	refused.sparks.ring_index = 64U;
	return midboss1_update_and_render(refused.boss, refused.state,
		refused.context, refused.callbacks) ==
		Midboss1UpdateResult::defeat_spark_allocation_refused &&
		refused.boss.target == ScreenPoint{216, 64} &&
		refused.boss.phase_frame == 18 && refused.state.defeat_frame == 1 &&
		refused.state.active == 0 && refused.callbacks.event_count == 0U;
}

} // namespace

int main()
{
	const std::array tests{
		test_invalidate_carries_after_old_back_callback,
		test_invalid_page_follows_initialization_prefix,
		test_initialize_then_enter_on_back_page,
		test_pattern_uses_real_pellets_before_tiles_and_render,
		test_hit_defeat_and_scroll_forced_defeat_order,
		test_defeat_terminal_and_refusal_prefix,
	};
	for(std::size_t i = 0U; i < tests.size(); ++i) {
		if(!tests[i]()) {
			std::fprintf(stderr, "midboss1 test %zu failed\n", i);
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
