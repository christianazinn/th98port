#pragma once
#include "th02/core/stage1_run.hpp"
#include "th02/core/stage2_run.hpp"
#include "th02/core/stage3_run.hpp"
#include "th02/core/stage4_run.hpp"
#include "th02/core/stage5_run.hpp"
#include "th02/core/stage_extra_run.hpp"
#include "th02/core/stage1_setup.hpp"

namespace th02::headless {
using namespace core;

// No rasterization or sound device. The two presentation-readable values
// remain real state: bomb animation saves/restores the tile mode and palette.
class Presentation final : public EnemyUpdateCallbacks, public RikaCallbacks,
	public MeiraCallbacks, public Midboss2Callbacks, public StonesCallbacks,
	public MarisaCallbacks, public MimaCallbacks, public Midboss1Callbacks,
	public SigmaCallbacks, public MidbossxCallbacks, public EnemyInvalidationCallbacks,
	public PlayerFrameInvalidationCallbacks, public LaserInvalidationCallbacks,
	public ItemScheduleCallbacks, public SparkInvalidationCallbacks,
	public BombAnimationCallbacks, public BombPresentationCallbacks,
	public PlayerMissPresentationCallbacks, public ItemLifecyclePresentationCallbacks,
	public LaserUpdateCallbacks, public EnemyBulletPresentationCallbacks,
	public Stage1PreparationPresentation, public Stage1SetupPresentation,
	public Stage1FramePresentation, public TileInitialCallbacks,
	public Stage1NotificationPresentation, public Stage5EndPresentation,
	public Stage1EndPresentation {
public:
	explicit Presentation(StageState &state) : state_(state) {}
	void grcg_vline(std::int16_t, std::int16_t, std::int16_t) noexcept override {}
	void grcg_bytebox_fill(std::uint8_t, std::int16_t, std::uint8_t, std::int16_t) noexcept override {}
	void render_sigma(SigmaSprite) noexcept override {}
	void render_sigma_blast(SigmaBlastSprite) noexcept override {}
	void grcg_line(ScreenPoint, ScreenPoint) noexcept override {}
	void grcg_circle(ScreenPoint, std::int16_t) noexcept override {}
	void grcg_circle_fill(ScreenPoint, std::int16_t) noexcept override {}
	void dot_square_ring(ScreenPoint, std::uint8_t, std::uint8_t) noexcept override {}
	void render_mima(MimaSprite) noexcept override {}
	void render_mima_orb(MimaOrbSprite) noexcept override {}
	void music_fade(std::uint8_t) noexcept override {}
	void sound_effect_force(std::uint8_t) noexcept override {}
	void load_second_form() noexcept override {}
	void sound_effects_update() noexcept override {}
	void stage_title_clear() noexcept override {}
	void bgm_title_show(std::uint8_t) noexcept override {}
	void bgm_title_clear() noexcept override {}
	void music_load(std::string_view) noexcept override {}
	void music_stop() noexcept override {}
	void music_play() noexcept override {}
	void boss_put(std::int16_t, std::int16_t, std::int16_t) noexcept override {}
	void scene(BossIntroScene, std::int16_t, const StageState &) noexcept override {}
	void render_marisa(MarisaSprite) noexcept override {}
	void render_orb_line(ScreenPoint, ScreenPoint) noexcept override {}
	void render_orb(MarisaOrbSprite) noexcept override {}
	void clear_orb_on_front_page(std::size_t, ScreenPoint) noexcept override {}
	void graph_scrollup(std::int16_t) noexcept override {}
	void box_area_invalidate() noexcept override {}
	void player_put(const StageState &) noexcept override {}
	void box_put(std::int16_t, std::int16_t, std::int16_t) noexcept override {}
	void box_wipe() noexcept override {}
	void face_put(std::int16_t) noexcept override {}
	void text_put(std::uint8_t, std::span<const std::uint8_t>) noexcept override {}
	void bonus_title() noexcept override {}
	void bonus_row(std::uint8_t, std::int16_t) noexcept override {}
	void bonus_total(const StageClearBonusTally &, std::int16_t) noexcept override {}
	void leave_cell(std::uint8_t, std::uint8_t, std::uint8_t) noexcept override {}
	void put_initial_row(std::int16_t, const TileSectionRow &) noexcept override {}
	void copy_line(const TileLineCopy &) noexcept override {}
	void egc_end() noexcept override {}
	void graph_scroll(ScrollPresentation) noexcept override {}
	void wait_for_presentation(std::uint8_t) noexcept override {}
	void page_show(std::size_t) noexcept override {}
	void page_access(std::size_t) noexcept override {}
	void score_events(const ScoreEvents &) noexcept override {}
	void background_particles_reset() noexcept override {}
	void sound_effects_reset() noexcept override {}
	void egc_begin() noexcept override {}
	void egc_off() noexcept override {}
	void invalidate_scenery(StageSceneryInvalidation) noexcept override {}
	void palette_set(std::uint8_t index, StageSceneryRgb color) noexcept override {
		palette_[index & 15U] = {color.r, color.g, color.b};
	}
	void palette_tone_set(std::uint16_t) noexcept override {}
	void grc_set_clip(std::int16_t, std::int16_t, std::int16_t, std::int16_t) noexcept override {}
	void dot_square(ScreenPoint, std::uint8_t) noexcept override {}
	void tiles_render(const StageState &) noexcept override {}
	void sound_effect(std::uint8_t) noexcept override {}
	void tile_ring_set_and_put_both_8(std::int16_t, std::int16_t, std::int16_t) noexcept override {}
	void render_enemy(EnemySprite) noexcept override {}
	void render_enemy_hit_white(EnemySprite) noexcept override {}
	void invalidate_rika(RikaInvalidation) noexcept override {}
	void render_rika(RikaSprite) noexcept override {}
	void invalidate_meira(MeiraInvalidation) noexcept override {}
	void render_meira(MeiraSprite) noexcept override {}
	void render_stone(StonesSprite) noexcept override {}
	void put_takeover_tile(std::int16_t, std::int16_t, std::int16_t) noexcept override {}
	void invalidate_midboss(Midboss2Invalidation) noexcept override {}
	void render_midboss(Midboss2Sprite) noexcept override {}
	void explosion_margins_clear() noexcept override {}
	void explosion_egc_begin() noexcept override {}
	void explosion_egc_end() noexcept override {}
	void explosion_ring(ScreenPoint, std::int16_t, bool) noexcept override {}
	void explosion_color(std::uint8_t) noexcept override {}
	void explosion_grcg_off() noexcept override {}
	void explosion_invalidate(ScreenPoint) noexcept override {}
	void explosion_tiles_render() noexcept override {}
	void explosion_sprite(ScreenPoint, std::int16_t) noexcept override {}
	void explosion_palette_show() noexcept override {}
	void explosion_graph_scroll(std::uint16_t, std::uint16_t, std::uint16_t) noexcept override {}
	void invalidate_midboss(Midboss1Invalidation) noexcept override {}
	void render_midboss(Midboss1Sprite) noexcept override {}
	void invalidate_enemy(EnemyInvalidation) noexcept override {}
	void invalidate_shot(PlayerShotInvalidation) noexcept override {}
	void invalidate_player(PlayerFrameInvalidation) noexcept override {}
	void invalidate_option(PlayerFrameInvalidation) noexcept override {}
	void invalidate_laser(LaserInvalidation) noexcept override {}
	void pointnums_init_for_rank_and_reset() noexcept override {}
	void invalidate_item(ItemInvalidation) noexcept override {}
	void pointnums_invalidate() noexcept override {}
	void invalidate_spark(SparkInvalidation) noexcept override {}
	void invalidate_bullet(EnemyBulletInvalidation) noexcept override {}
	void hud_bombs(std::int8_t) noexcept override {}
	void invalidate_bomb(BombInvalidation) noexcept override {}
	void render_all() noexcept override {}
	BombRgb palette_get(std::uint8_t index) noexcept override { return palette_[index & 15U]; }
	void palette_set(std::uint8_t index, BombRgb color) noexcept override { palette_[index & 15U] = color; }
	void palette_show() noexcept override {}
	std::uint8_t tile_mode_get() noexcept override { return state_.tile_mode; }
	void tile_mode_set(std::uint8_t value) noexcept override { state_.tile_mode = value; }
	void boxfill(std::int16_t, std::int16_t, std::int16_t, std::int16_t) noexcept override {}
	void bomb_bft(std::int16_t) noexcept override {}
	void bomb_b_tram(std::uint8_t) noexcept override {}
	void bomb_smear(std::uint8_t, std::int16_t) noexcept override {}
	void pi_put(std::int16_t, std::int16_t, std::uint8_t) noexcept override {}
	void overlay_wipe() noexcept override {}
	void grcg_set_color(std::uint8_t) noexcept override {}
	void circle_point(ScreenPoint) noexcept override {}
	void particle(ScreenPoint, std::int16_t) noexcept override {}
	void grcg_off() noexcept override {}
	void hud_power(std::uint8_t, std::uint8_t) noexcept override {}
	void hud_lives(std::int8_t) noexcept override {}
	void render_player(PlayerMissSprite) noexcept override {}
	void pointnum_add(std::int16_t, std::int16_t, std::uint16_t) noexcept override {}
	void render_item(ItemSprite) noexcept override {}
	void pointnums_update_and_render() noexcept override {}
	void render_laser(LaserPresentation) noexcept override {}
	void present(EnemyBulletPresentationEvent) noexcept override {}
private:
	StageState &state_;
	std::array<BombRgb, 16> palette_{};
};
} // namespace th02::headless
