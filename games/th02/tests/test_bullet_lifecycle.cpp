#include "th02/core/bullet_lifecycle.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

using namespace th02::core;

constexpr std::int8_t bullet_remove = 2;

[[nodiscard]] std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(std::bit_cast<std::uint16_t>(pixels)) << 4U
	));
}

[[nodiscard]] EnemyBulletPoolState patterned_state() noexcept
{
	EnemyBulletPoolState state{};
	for(std::size_t i = 0U; i < state.bullets.size(); ++i) {
		auto &bullet = state.bullets[i];
		bullet.flag = enemy_bullet_free;
		bullet.size_type = static_cast<std::int8_t>(i + 1U);
		bullet.screen_topleft_on_page[0U] = {
			to_subpixels(static_cast<std::int16_t>(100 + i)),
			to_subpixels(static_cast<std::int16_t>(200 + i)),
		};
		bullet.screen_topleft_on_page[1U] = {
			to_subpixels(static_cast<std::int16_t>(300 + i)),
			to_subpixels(static_cast<std::int16_t>(400 + i)),
		};
	}
	return state;
}

struct Event {
	std::size_t slot{};
	EnemyBulletInvalidation invalidation{};
	bool was_alive{};
	bool was_remove{};
	ScreenPoint back_before{};
	bool operator==(const Event &) const = default;
};

class Recorder final : public EnemyBulletInvalidationCallbacks {
public:
	explicit Recorder(const EnemyBulletPoolState &state_arg,
		std::size_t page_back_arg) noexcept
		: state(&state_arg), page_back(page_back_arg),
		before_bullets(state_arg.bullets)
	{
	}

	const EnemyBulletPoolState *state{};
	std::size_t page_back{};
	std::array<EnemyBullet, enemy_bullet_count> before_bullets{};
	std::array<Event, enemy_bullet_count> events{};
	std::size_t count{};
	std::size_t next_slot{};
	bool callback_precedes_state_change{true};

	void invalidate_bullet(EnemyBulletInvalidation invalidation) noexcept override
	{
		if(state == nullptr || count >= events.size()) return;
		while(next_slot < state->bullets.size() &&
			state->bullets[next_slot].flag == enemy_bullet_free) {
			++next_slot;
		}
		if(next_slot >= state->bullets.size()) return;
		const auto slot = next_slot++;
		const auto &bullet = state->bullets[slot];
		const auto before = bullet.screen_topleft_on_page[page_back];
		events[count] = {
			slot, invalidation, bullet.flag == enemy_bullet_alive,
			bullet.flag == bullet_remove,
			{to_pixels(before.x), to_pixels(before.y)},
		};
		callback_precedes_state_change &= bullet == before_bullets[slot];
		++count;
	}

private:
	[[nodiscard]] static std::int16_t to_pixels(std::int16_t subpixels) noexcept
	{
		const auto widened = static_cast<std::int32_t>(subpixels);
		return static_cast<std::int16_t>((widened >= 0) ?
			(widened / 16) : -(((-widened) + 15) / 16));
	}
};

[[nodiscard]] bool test_invalidate_flags_sizes_and_page_carry() noexcept
{
	auto state = patterned_state();
	for(auto &bullet : state.bullets) bullet.flag = enemy_bullet_free;
	state.bullets[0U].flag = enemy_bullet_alive;
	state.bullets[0U].size_type = 1;
	state.bullets[0U].screen_topleft_on_page[0U] = {
		to_subpixels(10), to_subpixels(20)};
	state.bullets[0U].screen_topleft_on_page[1U] = {
		to_subpixels(50), to_subpixels(60)};
	state.bullets[1U].flag = bullet_remove;
	state.bullets[1U].size_type = 2;
	state.bullets[1U].screen_topleft_on_page[0U] = {
		to_subpixels(11), to_subpixels(21)};
	state.bullets[1U].screen_topleft_on_page[1U] = {
		to_subpixels(51), to_subpixels(61)};
	state.bullets[2U].flag = static_cast<std::int8_t>(0x7F);
	state.bullets[2U].size_type = static_cast<std::int8_t>(-1);
	state.bullets[2U].screen_topleft_on_page[0U] = {
		to_subpixels(12), to_subpixels(22)};
	state.bullets[2U].screen_topleft_on_page[1U] = {
		ShotSubpixelPoint{-1, -17}};
	state.bullets[3U].flag = static_cast<std::int8_t>(0x80);
	state.bullets[3U].size_type = static_cast<std::int8_t>(127);
	state.bullets[3U].screen_topleft_on_page[0U] = {
		to_subpixels(13), to_subpixels(23)};
	state.bullets[3U].screen_topleft_on_page[1U] = {
		to_subpixels(53), to_subpixels(63)};
	const auto before = state;
	Recorder recorder{state, 1U};
	if(bullets_invalidate(state, 1U, 0U, recorder) !=
		EnemyBulletLifecycleResult::completed || recorder.count != 4U ||
		!recorder.callback_precedes_state_change ||
		recorder.events[0U].slot != 0U || recorder.events[1U].slot != 1U ||
		recorder.events[2U].slot != 2U || recorder.events[3U].slot != 3U ||
		recorder.events[0U].invalidation !=
			EnemyBulletInvalidation{{50, 60}, 8, 8} ||
		recorder.events[1U].invalidation !=
			EnemyBulletInvalidation{{51, 61}, 16, 16} ||
		recorder.events[2U].invalidation !=
			EnemyBulletInvalidation{{-1, -2}, -8, -8} ||
		recorder.events[3U].invalidation !=
			EnemyBulletInvalidation{{53, 63}, 1016, 1016}) {
		std::fputs("FAIL invalidate callback\n", stderr);
		return false;
	}
	if(state.bullets[0U].flag != enemy_bullet_alive ||
		state.bullets[0U].screen_topleft_on_page[1U] !=
			before.bullets[0U].screen_topleft_on_page[0U] ||
		state.bullets[1U].flag != enemy_bullet_free ||
		state.bullets[1U].screen_topleft_on_page[1U] !=
			before.bullets[1U].screen_topleft_on_page[1U] ||
		state.bullets[2U].flag != static_cast<std::int8_t>(0x7F) ||
		state.bullets[2U].screen_topleft_on_page[1U] !=
			before.bullets[2U].screen_topleft_on_page[0U] ||
		state.bullets[3U].flag != static_cast<std::int8_t>(0x80) ||
		state.bullets[3U].screen_topleft_on_page[1U] !=
			before.bullets[3U].screen_topleft_on_page[0U]) {
		std::fputs("FAIL invalidate state\n", stderr);
		return false;
	}
	return true;
}

[[nodiscard]] bool test_same_page_and_invalid_pages() noexcept
{
	auto state = patterned_state();
	for(auto &bullet : state.bullets) bullet.flag = enemy_bullet_free;
	state.bullets[3U].flag = enemy_bullet_alive;
	state.bullets[3U].size_type = 3;
	state.bullets[3U].screen_topleft_on_page[1U] = {
		to_subpixels(70), to_subpixels(80)};
	state.bullets[enemy_bullet_count - 1U].flag =
		static_cast<std::int8_t>(0x7F);
	state.bullets[enemy_bullet_count - 1U].size_type = 1;
	state.bullets[enemy_bullet_count - 1U].screen_topleft_on_page[1U] = {
		to_subpixels(90), to_subpixels(100)};
	const auto before = state;
	Recorder same_page{state, 1U};
	if(bullets_invalidate(state, 1U, 1U, same_page) !=
		EnemyBulletLifecycleResult::completed || same_page.count != 2U ||
		same_page.events[0U].slot != 3U ||
		same_page.events[1U].slot != enemy_bullet_count - 1U ||
		same_page.events[0U].invalidation !=
			EnemyBulletInvalidation{{70, 80}, 24, 24} ||
		same_page.events[1U].invalidation !=
			EnemyBulletInvalidation{{90, 100}, 8, 8} ||
		state != before) {
		std::fputs("FAIL same-page\n", stderr);
		return false;
	}
	Recorder repeated{state, 1U};
	if(bullets_invalidate(state, 1U, 1U, repeated) !=
		EnemyBulletLifecycleResult::completed || repeated.count != 2U ||
		repeated.events[0U].slot != 3U ||
		repeated.events[1U].slot != enemy_bullet_count - 1U || state != before) {
		std::fputs("FAIL repeated same-page\n", stderr);
		return false;
	}
	const auto after = state;
	Recorder rejected{state, 1U};
	if(bullets_invalidate(state, 2U, 1U, rejected) !=
		EnemyBulletLifecycleResult::invalid_page || rejected.count != 0U ||
		state != after ||
		bullets_invalidate(state, 1U, 2U, rejected) !=
			EnemyBulletLifecycleResult::invalid_page || rejected.count != 0U ||
		state != after) {
		std::fputs("FAIL invalid page\n", stderr);
		return false;
	}
	return true;
}

[[nodiscard]] bool test_clear_composes_invalidation_then_alive_remove() noexcept
{
	auto state = patterned_state();
	for(auto &bullet : state.bullets) bullet.flag = enemy_bullet_free;
	state.bullets[0U].flag = enemy_bullet_alive;
	state.bullets[0U].size_type = 1;
	state.bullets[0U].screen_topleft_on_page[0U] = {
		to_subpixels(10), to_subpixels(20)};
	state.bullets[0U].screen_topleft_on_page[1U] = {
		to_subpixels(50), to_subpixels(60)};
	state.bullets[1U].flag = bullet_remove;
	state.bullets[1U].size_type = 2;
	state.bullets[1U].screen_topleft_on_page[1U] = {
		to_subpixels(100), to_subpixels(110)};
	state.bullets[2U].flag = static_cast<std::int8_t>(0x7F);
	state.bullets[2U].size_type = 3;
	state.bullets[2U].screen_topleft_on_page[0U] = {
		to_subpixels(7), to_subpixels(8)};
	state.bullets[2U].screen_topleft_on_page[1U] = {
		to_subpixels(120), to_subpixels(130)};
	const auto before = state;
	Recorder recorder{state, 1U};
	if(bullets_clear(state, 1U, 0U, recorder) !=
		EnemyBulletLifecycleResult::completed || recorder.count != 3U ||
		!recorder.callback_precedes_state_change ||
		state.bullets[0U].flag != bullet_remove ||
		state.bullets[0U].screen_topleft_on_page[1U] !=
			before.bullets[0U].screen_topleft_on_page[0U] ||
		state.bullets[1U].flag != enemy_bullet_free ||
		state.bullets[2U].flag != static_cast<std::int8_t>(0x7F) ||
		state.bullets[2U].screen_topleft_on_page[1U] !=
			before.bullets[2U].screen_topleft_on_page[0U]) {
		std::fputs("FAIL clear composition\n", stderr);
		return false;
	}

	const auto after = state;
	Recorder rejected{state, 1U};
	if(bullets_clear(state, 1U, 2U, rejected) !=
		EnemyBulletLifecycleResult::invalid_page || rejected.count != 0U ||
		state != after) {
		std::fputs("FAIL clear invalid page\n", stderr);
		return false;
	}
	return true;
}

} // namespace

int main()
{
	if(!test_invalidate_flags_sizes_and_page_carry() ||
		!test_same_page_and_invalid_pages() ||
		!test_clear_composes_invalidation_then_alive_remove()) {
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
