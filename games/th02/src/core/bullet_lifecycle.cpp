#include "th02/core/bullet_lifecycle.hpp"

namespace th02::core {
namespace {

constexpr std::int16_t subpixel_bits = 4;
constexpr std::int8_t bullet_remove = 2;

[[nodiscard]] std::int16_t to_pixels(std::int16_t subpixels) noexcept
{
	// This is the source SPPoint::to_pixel() arithmetic shift, expressed without
	// relying on implementation-defined signed right shift.
	const auto widened = static_cast<std::int32_t>(subpixels);
	return static_cast<std::int16_t>((widened >= 0) ?
		(widened / (1 << subpixel_bits)) :
		-(((-widened) + ((1 << subpixel_bits) - 1)) /
			(1 << subpixel_bits)));
}

[[nodiscard]] bool page_is_valid(std::size_t page) noexcept
{
	return page < EnemyBullet{}.screen_topleft_on_page.size();
}

} // namespace

EnemyBulletLifecycleResult bullets_invalidate(
	EnemyBulletPoolState &state, std::size_t page_back, std::size_t page_front,
	EnemyBulletInvalidationCallbacks &callbacks) noexcept
{
	if(!page_is_valid(page_back) || !page_is_valid(page_front)) {
		return EnemyBulletLifecycleResult::invalid_page;
	}

	for(auto &bullet : state.bullets) {
		if(bullet.flag == enemy_bullet_free) continue;

		const auto &back = bullet.screen_topleft_on_page[page_back];
		const auto size = static_cast<std::int16_t>(
			static_cast<std::int16_t>(bullet.size_type) * 8
		);
		callbacks.invalidate_bullet({
			{to_pixels(back.x), to_pixels(back.y)}, size, size,
		});
		if(bullet.flag == bullet_remove) {
			bullet.flag = enemy_bullet_free;
			continue;
		}
		bullet.screen_topleft_on_page[page_back] =
			bullet.screen_topleft_on_page[page_front];
	}
	return EnemyBulletLifecycleResult::completed;
}

EnemyBulletLifecycleResult bullets_clear(
	EnemyBulletPoolState &state, std::size_t page_back, std::size_t page_front,
	EnemyBulletInvalidationCallbacks &callbacks) noexcept
{
	if(bullets_invalidate(state, page_back, page_front, callbacks) !=
		EnemyBulletLifecycleResult::completed) {
		return EnemyBulletLifecycleResult::invalid_page;
	}
	for(auto &bullet : state.bullets) {
		if(bullet.flag == enemy_bullet_alive) {
			bullet.flag = bullet_remove;
		}
	}
	return EnemyBulletLifecycleResult::completed;
}

} // namespace th02::core
