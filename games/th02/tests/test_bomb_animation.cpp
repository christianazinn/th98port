#include "th02/core/bomb_animation.hpp"

#include "th02/core/random.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using namespace th02::core;

enum class EventKind : std::uint8_t {
	color,
	palette_set,
	palette_show,
	tile_get,
	tile_set,
	boxfill,
	bft,
	tram,
	smear,
	pi,
	sound,
	overlay,
	bullet_invalidation,
	particle,
	circle,
	off,
};

struct Event {
	EventKind kind{};
	std::uint8_t byte0{};
	std::uint8_t byte1{};
	std::int16_t value0{};
	std::int16_t value1{};
	std::int16_t value2{};
	std::int16_t value3{};
	ScreenPoint point{};
	BombRgb color{};
};

class Recorder final : public BombAnimationCallbacks {
public:
	std::array<Event, 512U> events{};
	std::size_t count{};
	std::array<BombRgb, 4U> palettes{};
	std::uint8_t tile_mode{1U};

	BombRgb palette_get(std::uint8_t index) noexcept override
	{
		return palettes[index];
	}

	void palette_set(std::uint8_t index, BombRgb color) noexcept override
	{
		palettes[index] = color;
		add({EventKind::palette_set, index, 0U, 0, 0, 0, 0, {}, color});
	}

	void palette_show() noexcept override
	{
		add({EventKind::palette_show});
	}

	std::uint8_t tile_mode_get() noexcept override
	{
		add({EventKind::tile_get, tile_mode});
		return tile_mode;
	}

	void tile_mode_set(std::uint8_t mode) noexcept override
	{
		tile_mode = mode;
		add({EventKind::tile_set, mode});
	}

	void boxfill(std::int16_t left, std::int16_t top, std::int16_t right,
		std::int16_t bottom) noexcept override
	{
		add({EventKind::boxfill, 0U, 0U, left, top, right, bottom});
	}

	void bomb_bft(std::int16_t cel) noexcept override
	{
		add({EventKind::bft, 0U, 0U, cel});
	}

	void bomb_b_tram(std::uint8_t cel) noexcept override
	{
		add({EventKind::tram, cel});
	}

	void bomb_smear(std::uint8_t column, std::int16_t bottom) noexcept override
	{
		add({EventKind::smear, column, 0U, bottom});
	}

	void pi_put(std::int16_t left, std::int16_t top,
		std::uint8_t image) noexcept override
	{
		add({EventKind::pi, image, 0U, left, top});
	}

	void sound_effect(std::uint8_t sound_id) noexcept override
	{
		add({EventKind::sound, sound_id});
	}

	void overlay_wipe() noexcept override
	{
		add({EventKind::overlay});
	}

	void grcg_set_color(std::uint8_t color) noexcept override
	{
		add({EventKind::color, color});
	}

	void circle_point(ScreenPoint point) noexcept override
	{
		add({EventKind::circle, 0U, 0U, 0, 0, 0, 0, point});
	}

	void particle(ScreenPoint point, std::int16_t cel) noexcept override
	{
		add({EventKind::particle, 0U, 0U, cel, 0, 0, 0, point});
	}

	void grcg_off() noexcept override
	{
		add({EventKind::off});
	}

	void invalidate_bullet(EnemyBulletInvalidation invalidation) noexcept override
	{
		add({EventKind::bullet_invalidation, 0U, 0U,
			invalidation.topleft.x, invalidation.topleft.y,
			invalidation.width, invalidation.height});
	}

	[[nodiscard]] std::size_t count_kind(EventKind kind) const noexcept
	{
		std::size_t found = 0U;
		for(std::size_t i = 0U; i < count && i < events.size(); ++i) {
			if(events[i].kind == kind) ++found;
		}
		return found;
	}

private:
	void add(Event event) noexcept
	{
		if(count < events.size()) events[count] = event;
		++count;
	}
};

[[nodiscard]] BombState bomb_at(std::int16_t frame) noexcept
{
	BombState bomb{};
	bomb.bombing = true;
	bomb.bomb_frame = frame;
	bomb.bomb_circle_center = {220, 196};
	return bomb;
}

void add_live_bullet(EnemyBulletPoolState &bullets) noexcept
{
	bullets.bullets[0U].flag = enemy_bullet_alive;
	bullets.bullets[0U].size_type = enemy_bullet_16x16;
	bullets.bullets[0U].screen_topleft_on_page[0U] = {50, 60};
	bullets.bullets[0U].screen_topleft_on_page[1U] = {70, 80};
}

[[nodiscard]] bool test_a_opening_palette_and_bft() noexcept
{
	auto bomb = bomb_at(1);
	BombAnimationState animation{};
	BombEffectState effects{};
	SparkPoolState sparks{};
	EnemyBulletPoolState bullets{};
	RandomState random{};
	Recorder callbacks;
	callbacks.palettes[0U] = {1U, 2U, 3U};
	callbacks.palettes[3U] = {4U, 5U, 6U};
	if(bomb_reimu_a(bomb, animation, effects, sparks, bullets, random, 0U,
		1U, 0, false, callbacks) != BombAnimationResult::running) {
		return false;
	}
	return animation.col0_before_bomb_a == BombRgb{1U, 2U, 3U} &&
		animation.col3_before_bomb_a == BombRgb{4U, 5U, 6U} &&
		callbacks.count_kind(EventKind::bft) == 1U &&
		callbacks.events[0U].kind == EventKind::color &&
		callbacks.events[0U].byte0 == 0U &&
		callbacks.events[1U].kind == EventKind::palette_set &&
		callbacks.events[1U].byte0 == 3U &&
		callbacks.events[3U].kind == EventKind::color &&
		callbacks.events[3U].byte0 == 3U &&
		callbacks.events[4U].kind == EventKind::bft &&
		callbacks.events[4U].value0 == 0 &&
		callbacks.events[5U].kind == EventKind::off;
}

[[nodiscard]] bool test_a_sparks_particles_and_reduce_effects() noexcept
{
	auto bomb = bomb_at(65);
	BombAnimationState animation{};
	BombEffectState effects{};
	SparkPoolState sparks{};
	EnemyBulletPoolState bullets{};
	RandomState random{};
	Recorder callbacks;
	if(bomb_reimu_a(bomb, animation, effects, sparks, bullets, random, 0U,
		1U, 0, false, callbacks) != BombAnimationResult::running ||
		sparks.ring_index != 1U || sparks.sparks[0U].flag != spark_alive ||
		random.cursor != 4U || effects.particle_cels[2U] != 1U ||
		callbacks.count_kind(EventKind::pi) != 1U ||
		callbacks.count_kind(EventKind::particle) == 0U ||
		callbacks.count_kind(EventKind::off) != 2U ||
		callbacks.events[3U].kind != EventKind::off ||
		callbacks.events[4U].kind != EventKind::pi ||
		callbacks.events[5U].kind != EventKind::color) {
		return false;
	}

	auto reduced_bomb = bomb_at(66);
	BombAnimationState reduced_animation{};
	BombEffectState reduced_effects{};
	SparkPoolState reduced_sparks{};
	EnemyBulletPoolState reduced_bullets{};
	RandomState reduced_random{};
	Recorder reduced_callbacks;
	if(bomb_reimu_a(reduced_bomb, reduced_animation, reduced_effects,
		reduced_sparks, reduced_bullets, reduced_random, 0U, 1U, 0, true,
		reduced_callbacks) != BombAnimationResult::running ||
		reduced_callbacks.count_kind(EventKind::pi) != 0U) {
		return false;
	}

	BombAnimationState normal_animation{};
	BombEffectState normal_effects{};
	SparkPoolState normal_sparks{};
	EnemyBulletPoolState normal_bullets{};
	RandomState normal_random{};
	Recorder normal_callbacks;
	if(bomb_reimu_a(reduced_bomb, normal_animation, normal_effects,
		normal_sparks, normal_bullets, normal_random, 0U, 1U, 0, false,
		normal_callbacks) != BombAnimationResult::running) {
		return false;
	}
	return normal_callbacks.count_kind(EventKind::pi) == 1U;
}

[[nodiscard]] bool test_a_clear_and_completion() noexcept
{
	auto bomb = bomb_at(136);
	BombAnimationState animation{};
	animation.col0_before_bomb_a = {7U, 8U, 9U};
	animation.tile_mode_before_bomb_a = 1U;
	animation.slowdown_factor = 2U;
	BombEffectState effects{};
	SparkPoolState sparks{};
	EnemyBulletPoolState bullets{};
	add_live_bullet(bullets);
	RandomState random{};
	Recorder callbacks;
	callbacks.palettes[0U] = {100U, 100U, 100U};
	callbacks.tile_mode = 2U;
	if(bomb_reimu_a(bomb, animation, effects, sparks, bullets, random, 0U,
		1U, 0, false, callbacks) != BombAnimationResult::running ||
		bullets.bullets[0U].flag != enemy_bullet_remove ||
		animation.palette_tone != 200U || animation.slowdown_factor != 1U ||
		callbacks.tile_mode != 1U || callbacks.count_kind(EventKind::sound) != 1U ||
		callbacks.count_kind(EventKind::bullet_invalidation) != 1U) {
		return false;
	}
	bomb.bomb_frame = 156;
	callbacks = {};
	return bomb_reimu_a(bomb, animation, effects, sparks, bullets, random, 0U,
		1U, 0, false, callbacks) == BombAnimationResult::completed &&
		callbacks.count_kind(EventKind::off) == 1U;
}

[[nodiscard]] bool test_a_refusals_preserve_prefix() noexcept
{
	{
		auto bomb = bomb_at(136);
		BombAnimationState animation{};
		BombEffectState effects{};
		SparkPoolState sparks{};
		EnemyBulletPoolState bullets{};
		add_live_bullet(bullets);
		RandomState random{};
		Recorder callbacks;
		const auto result = bomb_reimu_a(bomb, animation, effects, sparks,
			bullets, random, 0U, 2U, 0, false, callbacks);
		if(result != BombAnimationResult::invalid_page ||
			bullets.bullets[0U].flag != enemy_bullet_alive ||
			animation.palette_tone != 200U ||
			callbacks.count_kind(EventKind::sound) != 1U) {
			return false;
		}
	}
	{
		auto bomb = bomb_at(65);
		BombAnimationState animation{};
		BombEffectState effects{};
		SparkPoolState sparks{};
		sparks.ring_index = static_cast<std::uint16_t>(spark_count);
		EnemyBulletPoolState bullets{};
		RandomState random{};
		Recorder callbacks;
		const auto result = bomb_reimu_a(bomb, animation, effects, sparks,
			bullets, random, 0U, 1U, 0, false, callbacks);
		return result == BombAnimationResult::spark_allocation_failed &&
			random.cursor == 4U && callbacks.count_kind(EventKind::pi) == 1U;
	}
}

[[nodiscard]] bool test_b_cel_jump_and_clear() noexcept
{
	auto bomb = bomb_at(32);
	BombAnimationState animation{};
	BombEffectState effects{};
	SparkPoolState sparks{};
	EnemyBulletPoolState bullets{};
	RandomState random{};
	Recorder callbacks;
	callbacks.tile_mode = 1U;
	if(bomb_reimu_b(bomb, animation, effects, sparks, bullets, random, 0U,
		1U, 0, false, callbacks) != BombAnimationResult::running ||
		animation.bomb_b_cel != 0U || animation.tile_mode_before_bomb_b != 1U ||
		callbacks.tile_mode != 2U) {
		return false;
	}
	bomb.bomb_frame = 100;
	animation.bomb_b_cel = 17U;
	callbacks = {};
	if(bomb_reimu_b(bomb, animation, effects, sparks, bullets, random, 0U,
		1U, 0, false, callbacks) != BombAnimationResult::running ||
		bomb.bomb_frame != 132 || animation.bomb_b_cel != 18U ||
		random.cursor != 4U || callbacks.count_kind(EventKind::tram) != 1U ||
		callbacks.count_kind(EventKind::overlay) != 1U ||
		callbacks.count_kind(EventKind::particle) == 0U) {
		return false;
	}
	bomb.bomb_frame = 84;
	callbacks = {};
	if(bomb_reimu_b(bomb, animation, effects, sparks, bullets, random, 0U,
		1U, 0, true, callbacks) != BombAnimationResult::running ||
		callbacks.count_kind(EventKind::pi) != 0U) {
		return false;
	}
	callbacks = {};
	if(bomb_reimu_b(bomb, animation, effects, sparks, bullets, random, 0U,
		1U, 0, false, callbacks) != BombAnimationResult::running ||
		callbacks.count_kind(EventKind::pi) != 1U) {
		return false;
	}
	bomb.bomb_frame = 164;
	add_live_bullet(bullets);
	callbacks = {};
	if(bomb_reimu_b(bomb, animation, effects, sparks, bullets, random, 0U,
		1U, 0, false, callbacks) != BombAnimationResult::running ||
		bullets.bullets[0U].flag != enemy_bullet_remove ||
		animation.palette_tone != 200U || animation.slowdown_factor != 1U ||
		callbacks.count_kind(EventKind::bullet_invalidation) != 1U) {
		return false;
	}
	bomb.bomb_frame = 184;
	callbacks = {};
	return bomb_reimu_b(bomb, animation, effects, sparks, bullets, random, 0U,
		1U, 0, false, callbacks) == BombAnimationResult::completed &&
		callbacks.count_kind(EventKind::off) == 1U;
}

[[nodiscard]] bool test_c_smears_and_clear() noexcept
{
	auto bomb = bomb_at(32);
	BombAnimationState animation{};
	BombEffectState effects{};
	SparkPoolState sparks{};
	EnemyBulletPoolState bullets{};
	RandomState random{};
	Recorder callbacks;
	callbacks.tile_mode = 1U;
	if(bomb_reimu_c(bomb, animation, effects, sparks, bullets, random, 0U,
		1U, 0, false, callbacks) != BombAnimationResult::running ||
		animation.smear_bottoms[0U] != 20 || animation.smear_bottoms[47U] != 20 ||
		animation.tile_mode_before_bomb_c != 1U || callbacks.tile_mode != 2U ||
		callbacks.count_kind(EventKind::sound) != 1U) {
		return false;
	}
	bomb.bomb_frame = 80;
	callbacks = {};
	if(bomb_reimu_c(bomb, animation, effects, sparks, bullets, random, 0U,
		1U, 0, false, callbacks) != BombAnimationResult::running ||
		animation.smear_bottoms[0U] != 24 || animation.smear_bottoms[1U] != 23 ||
		animation.smear_bottoms[7U] != 20 || animation.smear_bottoms[47U] != 24 ||
		animation.palette_tone != 100U || animation.slowdown_factor != 2U ||
	random.cursor != 4U || callbacks.count_kind(EventKind::smear) != 48U) {
		return false;
	}
	bomb.bomb_frame = 112;
	add_live_bullet(bullets);
	callbacks = {};
	if(bomb_reimu_c(bomb, animation, effects, sparks, bullets, random, 0U,
		1U, 0, false, callbacks) != BombAnimationResult::running ||
		bullets.bullets[0U].flag != enemy_bullet_remove ||
		animation.palette_tone != 200U || animation.slowdown_factor != 1U ||
		callbacks.count_kind(EventKind::bullet_invalidation) != 1U) {
		return false;
	}
	bomb.bomb_frame = 128;
	callbacks = {};
	return bomb_reimu_c(bomb, animation, effects, sparks, bullets, random, 0U,
		1U, 0, false, callbacks) == BombAnimationResult::completed &&
		callbacks.count_kind(EventKind::off) == 1U;
}

} // namespace

int main()
{
	if(!test_a_opening_palette_and_bft()) {
		std::fputs("FAIL A opening\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_a_sparks_particles_and_reduce_effects()) {
		std::fputs("FAIL A sparks/reduce\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_a_clear_and_completion()) {
		std::fputs("FAIL A clear/completion\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_a_refusals_preserve_prefix()) {
		std::fputs("FAIL A refusal\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_b_cel_jump_and_clear()) {
		std::fputs("FAIL B\n", stderr);
		return EXIT_FAILURE;
	}
	if(!test_c_smears_and_clear()) {
		std::fputs("FAIL C\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
