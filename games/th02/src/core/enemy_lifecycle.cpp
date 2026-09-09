#include "th02/core/enemy_lifecycle.hpp"

namespace th02::core {
namespace {

constexpr std::int16_t enemy_width = 32;
constexpr std::int16_t enemy_height = 32;

[[nodiscard]] bool page_is_valid(std::size_t page) noexcept
{
	return page < Enemy{}.pos_on_page.size();
}

} // namespace

EnemyLifecycleResult enemies_invalidate(
	EnemyPoolState &state, std::size_t page_back, std::size_t page_front,
	EnemyInvalidationCallbacks &callbacks) noexcept
{
	if(!page_is_valid(page_back) || !page_is_valid(page_front)) {
		return EnemyLifecycleResult::invalid_page;
	}

	state.enemies_loop_bound = 0U;
	for(std::size_t i = 0U; i < state.enemies.size(); ++i) {
		auto &enemy = state.enemies[i];
		if(enemy.flag == enemy_free) continue;

		callbacks.invalidate_enemy({
			enemy.pos_on_page[page_back], enemy_width, enemy_height,
		});
		if(enemy.flag == enemy_remove) {
			enemy.flag = enemy_free;
			continue;
		}
		state.enemies_loop_bound = static_cast<std::uint8_t>(i + 1U);
		enemy.pos_on_page[page_back] = enemy.pos_on_page[page_front];
	}
	return EnemyLifecycleResult::completed;
}

void enemies_remove_all(EnemyPoolState &state) noexcept
{
	for(auto &enemy : state.enemies) {
		if(enemy.flag != enemy_free) enemy.flag = enemy_remove;
	}
	state.enemies_loop_bound = 0U;
}

} // namespace th02::core
