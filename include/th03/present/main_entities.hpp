#ifndef TH03_PRESENT_MAIN_ENTITIES_HPP
#define TH03_PRESENT_MAIN_ENTITIES_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "th03/core/observer.hpp"
#include "th03/present/sprite16.hpp"

namespace th03::present {

inline constexpr std::size_t main_shot_cel_count = 4U;
inline constexpr std::size_t main_enemy_family_count = 4U;
inline constexpr std::size_t main_explosion_cel_count = 4U;
inline constexpr std::size_t main_fireball_variant_count = 2U;
inline constexpr std::size_t main_fireball_cel_count = 2U;

// Semantic slots in MAIN's committed Sprite16 atlas. The application injects
// both these rectangles and their decoded pixels; no file, asset handle, or
// PC-98 byte offset crosses the observer boundary.
struct MainEntityAtlasLayout {
	std::array<
		std::array<Sprite16AtlasRect, main_shot_cel_count>,
		2U
	> shot_cels{};
	// One undamaged living-enemy anchor for each 16/32/48/64-pixel family.
	// Earlier damage cels precede this rectangle by one sprite width per HP.
	std::array<Sprite16AtlasRect, main_enemy_family_count>
		living_enemy_undamaged{};
	// Families are the 32/48/64/80-pixel explosion sizes.
	std::array<
		std::array<Sprite16AtlasRect, main_explosion_cel_count>,
		main_enemy_family_count
	> explosion_cels{};
	// Variant order is blue, red; cel order is the source's first, second cel.
	std::array<
		std::array<Sprite16AtlasRect, main_fireball_cel_count>,
		main_fireball_variant_count
	> falling_fireball_cels{};
	std::array<
		std::array<Sprite16AtlasRect, main_fireball_cel_count>,
		main_fireball_variant_count
	> transferring_fireball_cels{};
};

// Source-derived semantic layout for the MAIN atlas staged by the original PI
// resources. The returned value contains coordinates only, never asset bytes.
[[nodiscard]] MainEntityAtlasLayout authentic_main_entity_atlas_layout() noexcept;

using MainShotPairFacts = std::array<
	ShotPairFact,
	ShotsEnemiesView::shot_count
>;
using MainEnemyFacts = std::array<EnemyFact, ShotsEnemiesView::enemy_count>;
using MainFireballFacts = std::array<
	FireballFact,
	FireballsView::fireball_count
>;

enum class MainEntitiesStatus : std::uint8_t {
	ready,
	invalid_layout,
	invalid_observation,
	staging_not_committed,
	draw_failed,
};

// Authentic consumer for the first two ordinary-entity phase views. The
// observer callbacks are synchronous and draw to Raster's current access page.
// Shot animation belongs here: ShotPairSpawned resets a slot to cel 0 and each
// observed live draw advances that slot once through all four cels.
class MainEntitiesPresenter final : public GameplayObserver {
public:
	MainEntitiesPresenter(
		Raster &raster,
		const Sprite16Staging &staging,
		MainEntityAtlasLayout layout
	) noexcept;

	void shot_pair_spawned(const ShotPairSpawned &spawn) noexcept override;
	void observe(const ShotsEnemiesView &view) noexcept override;
	void observe(const FireballsView &view) noexcept override;

	// Fixed-capacity fact entry points for synthetic tests and trace adapters.
	// Facts must identify their exact ascending array slot. All facts are
	// validated before the first pixel changes.
	[[nodiscard]] bool present_shots_enemies(
		const MainPhaseContext &context,
		const MainShotPairFacts &shots,
		const MainEnemyFacts &enemies
	) noexcept;
	[[nodiscard]] bool present_fireballs(
		const MainPhaseContext &context,
		const MainFireballFacts &fireballs
	) noexcept;

	[[nodiscard]] MainEntitiesStatus status() const noexcept;
	[[nodiscard]] bool ready() const noexcept;

private:
	Raster *raster_;
	const Sprite16Staging *staging_;
	MainEntityAtlasLayout layout_{};
	std::array<std::uint8_t, ShotsEnemiesView::shot_count> shot_cels_{};
	std::array<PlayerId, ShotsEnemiesView::shot_count> shot_players_{};
	std::array<bool, ShotsEnemiesView::shot_count> shot_spawn_observed_{};
	MainEntitiesStatus status_ = MainEntitiesStatus::ready;
};

} // namespace th03::present

#endif
