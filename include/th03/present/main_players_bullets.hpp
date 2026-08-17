#ifndef TH03_PRESENT_MAIN_PLAYERS_BULLETS_HPP
#define TH03_PRESENT_MAIN_PLAYERS_BULLETS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "th03/assets/types.hpp"
#include "th03/core/observer.hpp"
#include "th03/present/mono.hpp"
#include "th03/present/sprite16.hpp"
#include "th03/present/staff_flakes.hpp"
#include "th03/present/superimpose.hpp"

namespace th03::present {

inline constexpr std::size_t main_player_pose_count = 3U;
inline constexpr std::size_t main_bullet16_cel_count = 4U;
inline constexpr std::size_t main_pellet_cloud_cel_count = 3U;

// Source's optional-paletted character encoding. The player facts and the
// decoded character sheets carry these values so a mismatched round asset set
// can be rejected without comparing a character to an unnamed integer.
enum class MainPalettedCharacter : std::int8_t {
	reimu_primary = 1,
	reimu_alternate,
	mima_primary,
	mima_alternate,
	marisa_primary,
	marisa_alternate,
	ellen_primary,
	ellen_alternate,
	kotohime_primary,
	kotohime_alternate,
	kana_primary,
	kana_alternate,
	rikako_primary,
	rikako_alternate,
	chiyuri_primary,
	chiyuri_alternate,
	yumemi_primary,
	yumemi_alternate,
};

// Semantic slots in MAIN's committed Sprite16 atlas. Pose order is neutral,
// left, right. Bullet16 cel order is the source's four adjacent cels.
struct MainPlayersBulletsAtlasLayout {
	std::array<
		std::array<Sprite16AtlasRect, main_player_pose_count>,
		PlayersBulletsView::player_count
	> player_pose_cels{};
	Sprite16AtlasRect defeat_ring{};
	std::array<
		std::array<Sprite16AtlasRect, main_bullet16_cel_count>,
		PlayersBulletsView::player_count
	> default_bullet16_cels{};
	std::array<
		std::array<Sprite16AtlasRect, main_bullet16_cel_count>,
		PlayersBulletsView::player_count
	> kana_bullet16_cels{};
	std::array<Sprite16AtlasRect, main_pellet_cloud_cel_count>
		pellet_cloud_cels{};
};

// Source-derived coordinates only; decoded content remains caller-owned.
[[nodiscard]] MainPlayersBulletsAtlasLayout
	authentic_main_players_bullets_atlas_layout() noexcept;

// The two character sheets contain the nine source-local glow/shot patterns.
// The gauge sheet is ZIKICW's source-local 16-pattern range. Global master.lib
// pattern numbers never cross this boundary.
struct MainPlayerOverlayAssets {
	std::array<
		const assets::BfntSheet *,
		PlayersBulletsView::player_count
	> character_sheets{};
	std::array<
		MainPalettedCharacter,
		PlayersBulletsView::player_count
	> loaded_characters{};
	const assets::BfntSheet *gauge_sheet = nullptr;
};

using MainPlayerFacts = std::array<
	PlayerFact,
	PlayersBulletsView::player_count
>;
using MainBulletFacts = std::array<
	BulletFact,
	PlayersBulletsView::bullet_count
>;
using MainBulletTrail = std::array<
	RawPoint,
	PlayersBulletsView::trail_point_count
>;
using MainBulletTrails = std::array<
	MainBulletTrail,
	PlayersBulletsView::bullet_count
>;
using MainPlayersBulletsMiddlePass = bool (*)(
	void *context,
	const MainPhaseContext &phase
) noexcept;

enum class MainPlayersBulletsStatus : std::uint8_t {
	ready,
	invalid_layout,
	invalid_assets,
	invalid_observation,
	staging_not_committed,
	draw_failed,
};

// Authentic consumer for MAIN's final ordinary-entity phase. It owns the
// source-shared player pose byte, each committed player pose, the shared defeat
// anchor/radius, and the construction style of all 320 Bullet16 slots.
// Its complete-phase entry retains the exact common-effect insertion point
// between player overlays and the three bullet passes.
class MainPlayersBulletsPresenter final : public GameplayObserver {
public:
	MainPlayersBulletsPresenter(
		Raster &raster,
		const Sprite16Staging &staging,
		MainPlayersBulletsAtlasLayout layout,
		MainPlayerOverlayAssets overlays,
		PelletMasks pellet_masks,
		StaffFlakeTrigTables trig
	);

	void bullet16_spawned(const Bullet16Spawned &spawn) noexcept override;
	void player_pose_committed(const PlayerPoseCommitted &pose) noexcept override;
	void player_defeat_step(const PlayerDefeatStep &step) noexcept override;
	void observe(const PlayersBulletsView &view) noexcept override;
	void accelerator_shared_velocity_restore(std::int8_t velocity_x_q4_4)
		noexcept;

	// Separating these fixed-capacity pass entry points retains the source's
	// common-effect insertion slot between player overlays and Bullet16. Each
	// pass validates all of its facts before changing a pixel. The observer
	// callback additionally validates both passes as one transaction.
	[[nodiscard]] bool present_players(
		const MainPhaseContext &context,
		const MainPlayerFacts &players
	) noexcept;
	[[nodiscard]] bool present_bullets(
		const MainPhaseContext &context,
		const MainBulletFacts &bullets,
		const MainBulletTrails &trails
	) noexcept;

	// Complete final-phase transaction with the exact SUB_CEE0 insertion seam.
	// Both player and bullet facts validate before the first pixel changes;
	// [middle] then runs after player overlays and before Bullet16/cloud/pellet
	// passes. This synchronous function pointer is deliberately narrower than a
	// generic render callback or event bus.
	[[nodiscard]] bool present_phase_with_middle(
		const MainPhaseContext &context,
		const MainPlayerFacts &players,
		const MainBulletFacts &bullets,
		const MainBulletTrails &trails,
		MainPlayersBulletsMiddlePass middle,
		void *middle_context,
		std::optional<Page> source_page_back = std::nullopt
	) noexcept;

	[[nodiscard]] MainPlayersBulletsStatus status() const noexcept;
	[[nodiscard]] bool ready() const noexcept;

private:
	enum class PlayerPose : std::uint8_t {
		neutral,
		left,
		right,
	};

	struct DefeatState {
		std::int16_t anchor_left = 0;
		std::int16_t anchor_top = 0;
		std::int16_t radius = 0;
	};

	struct Bullet16StyleState {
		PlayerId source_player = PlayerId::p1;
		Bullet16ConstructionKind construction =
			Bullet16ConstructionKind::default_attack;
		bool animated = true;
		bool observed = false;
	};

	[[nodiscard]] bool validate_players(
		const MainPlayerFacts &players
	) noexcept;
	[[nodiscard]] bool validate_bullets(
		const MainBulletFacts &bullets,
		const MainBulletTrails &trails
	) noexcept;
	[[nodiscard]] bool draw_players(
		const MainPhaseContext &context,
		const MainPlayerFacts &players,
		std::optional<Page> source_page_back
	) noexcept;
	[[nodiscard]] bool draw_bullets(
		const MainPhaseContext &context,
		const MainBulletFacts &bullets,
		const MainBulletTrails &trails
	) noexcept;
	[[nodiscard]] bool present_phase(
		const MainPhaseContext &context,
		const MainPlayerFacts &players,
		const MainBulletFacts &bullets,
		const MainBulletTrails &trails
	) noexcept;

	Raster *raster_;
	const Sprite16Staging *staging_;
	MainPlayersBulletsAtlasLayout layout_{};
	std::array<
		MainPalettedCharacter,
		PlayersBulletsView::player_count
	> loaded_characters_{};
	std::array<
		std::optional<PreparedSuperimposeSheet>,
		PlayersBulletsView::player_count
	> prepared_character_sheets_{};
	std::optional<PreparedSuperimposeSheet> prepared_gauge_sheet_{};
	PelletMasks pellet_masks_{};
	StaffFlakeTrigTables trig_{};
	std::int8_t shared_velocity_x_q4_4_ = 0;
	std::array<PlayerPose, PlayersBulletsView::player_count> player_poses_{};
	DefeatState defeat_{};
	std::array<Bullet16StyleState, PlayersBulletsView::bullet_count>
		bullet16_styles_{};
	MainPlayersBulletsStatus status_ = MainPlayersBulletsStatus::ready;
};

} // namespace th03::present

#endif
