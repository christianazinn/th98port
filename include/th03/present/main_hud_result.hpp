#ifndef TH03_PRESENT_MAIN_HUD_RESULT_HPP
#define TH03_PRESENT_MAIN_HUD_RESULT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "th03/core/observer.hpp"
#include "th03/present/mono.hpp"
#include "th03/present/staff_flakes.hpp"
#include "th03/present/superimpose.hpp"
#include "th03/present/tram.hpp"

namespace th03::present {

enum class MainHudResultStatus : std::uint8_t {
	ready,
	invalid_assets,
	invalid_event_sequence,
	draw_failed,
};

struct MainHudResultSheets {
	const PreparedSuperimposeSheet *lose = nullptr;
	const PreparedSuperimposeSheet *round = nullptr;
	const PreparedSuperimposeSheet *zikicw = nullptr;
};

enum class MainResultLabel : std::uint8_t {
	winner_bonus,
	all_clear_bonus,
	max_combo,
	gauge_attack,
	boss_attack,
	boss_attack_reversed,
	boss_panic,
	player_remaining,
	total,
	count,
};

inline constexpr std::size_t main_result_label_count =
	static_cast<std::size_t>(MainResultLabel::count);

// Already-authenticated raw TRAM cells. Result label spans contain 26 or 32
// half-width cells as documented by their source rows. Combo spans contain
// complete two-cell gaiji pairs or ANK erase cells; no text decoding occurs.
struct MainHudResultTramAssets {
	std::array<
		std::span<const TramCell>,
		main_result_label_count
	> result_labels{};
	std::span<const TramCell> combo_hit{};
	std::span<const TramCell> combo_bonus{};
	std::span<const TramCell> combo_erase{};
};

// Process-scoped reconstruction of MAIN's HUD-start animation, dynamic score
// and combo rows, result summary, and retained opening/closing transition.
// Construct at MAIN process start and retain across repeats. Raster, Tram, all
// prepared sheets, and the backing storage of every font, trig, and TRAM span
// must outlive this presenter. Attaching after process start cannot reproduce
// stale frame-0 result snapshots or retained transition coordinates exactly.
class MainHudResultPresenter final : public GameplayObserver {
public:
	MainHudResultPresenter(
		Raster &raster,
		Tram &tram,
		ScoreFontMasks score_font,
		StaffFlakeTrigTables trig,
		MainHudResultSheets sheets,
		MainHudResultTramAssets tram_assets,
		std::uint8_t continues_used_digit
	) noexcept;

	void hud_start_frame_step(const HudStartFrameStep &step) noexcept override;
	void hud_start_compatibility_read(
		const HudStartCompatibilityRead &read
	) noexcept override;
	void main_hud_start_state(const MainHudStartState &state) noexcept override;
	void main_hud_player_value_changed(
		const MainHudPlayerValueChanged &changed
	) noexcept override;
	void main_hud_story_lives_changed(
		const MainHudStoryLivesChanged &changed
	) noexcept override;
	void main_result_state(const MainResultState &state) noexcept override;
	void main_score_state(const MainScoreState &state) noexcept override;
	void main_combo_state(const MainComboState &state) noexcept override;
	void main_result_transition_state(
		const MainResultTransitionState &state
	) noexcept override;

	[[nodiscard]] MainHudResultStatus status() const noexcept;
	[[nodiscard]] bool ready() const noexcept;

private:
	struct HudBannerRecord {
		std::int16_t x = 0;
		std::int16_t y = 0;
		std::int16_t velocity = 0;
		std::uint8_t active = 0U;
	};
	struct HudParticleRecord {
		std::int16_t x_q4 = 0;
		std::int16_t y_q4 = 0;
		std::uint8_t angle = 0U;
		std::int8_t angle_delta_first = 0;
		std::int8_t angle_delta_second = 0;
		std::uint8_t active = 0U;
	};

	Raster *raster_;
	Tram *tram_;
	ScoreFontMasks score_font_{};
	StaffFlakeTrigTables trig_{};
	MainHudResultSheets sheets_{};
	MainHudResultTramAssets tram_assets_{};
	std::uint8_t continues_used_digit_ = 0U;

	std::array<HudBannerRecord, 16U> hud_banner_{};
	std::array<HudParticleRecord, 128U> hud_particles_{};
	std::uint16_t hud_compatibility_read_count_ = 0U;
	std::int16_t hud_banner_x_ = 0;
	std::uint8_t hud_pattern_base_ = 0U;
	bool hud_particle_reads_pending_ = false;
	bool hud_completed_batch_waiting_ = false;
	bool hud_late_state_pending_ = false;
	bool hud_expected_story_mode_ = false;
	std::uint8_t hud_expected_story_stage_ = 0U;
	std::uint8_t hud_expected_round_id_ = 0U;
	std::uint8_t hud_expected_state_after_ = 0U;
	std::uint16_t hud_expected_frame_after_ = 0U;

	MainResultWinnerStats result_snapshot_{};
	std::uint16_t result_initial_remainder_ = 0U;
	std::uint16_t result_initial_quotient_ = 0U;
	std::uint8_t result_final_lives_ = 0U;
	std::array<std::int16_t, 8U> transition_coordinates_{};
	MainHudResultStatus status_ = MainHudResultStatus::ready;
};

} // namespace th03::present

#endif
