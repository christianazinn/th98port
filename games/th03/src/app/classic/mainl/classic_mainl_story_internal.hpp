#ifndef TH03_APP_CLASSIC_MAINL_STORY_INTERNAL_HPP
#define TH03_APP_CLASSIC_MAINL_STORY_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

#include "th03/app/main/main_process.hpp"

namespace th03::app {

enum class ClassicMainlScene : std::uint8_t {
	stage_splash,
	start_main,
	win,
	interstage_cutscene,
	high_score_registration,
	continue_menu,
	game_over,
	ending,
	staff_verdict,
	extra_ending,
	exit_to_operator,
};

enum class ClassicInterstageCutscene : std::uint8_t {
	none,
	chiyuri,
	yumemi,
};

struct ClassicMainlStep {
	ClassicMainlScene scene = ClassicMainlScene::exit_to_operator;
	ClassicInterstageCutscene cutscene = ClassicInterstageCutscene::none;

	friend constexpr bool operator==(
		const ClassicMainlStep &,
		const ClassicMainlStep &
	) = default;
};

struct ClassicMainlWinContext {
	std::int8_t winner_paletted_character = 0;
	std::int8_t loser_paletted_character = 0;
	// Exact zero-based block in the winner's fixed @NNTX table. Blocks 9 and
	// 10 are the source's decisive-stage and Chiyuri-stage messages.
	std::uint8_t message_block = 0U;
	// sub_9887() returning zero makes win_animate_and_wait() preload the next
	// stage splash into the hidden page before it begins polling dismissal.
	bool preload_stage_splash = false;

	friend constexpr bool operator==(
		const ClassicMainlWinContext &,
		const ClassicMainlWinContext &
	) = default;
};

inline constexpr std::size_t classic_mainl_max_steps = 6U;

enum class ClassicMainlPlanErrorCode : std::uint8_t {
	invalid_story_start,
	invalid_terminal_state,
	invalid_transition,
	invalid_continue_transition,
};

struct ClassicMainlPlanError {
	ClassicMainlPlanErrorCode code =
		ClassicMainlPlanErrorCode::invalid_story_start;
};

// Source-private policy result. Public Story callers reach this only through
// the campaign owner, which supplies the retained MainProcess values; keeping
// the raw planner private prevents independently forged transition triples
// from becoming an application boundary.
class ClassicMainlPlan final {
public:
	[[nodiscard]] std::span<const ClassicMainlStep> steps() const noexcept;
	[[nodiscard]] const GameInit &resident() const noexcept;
	[[nodiscard]] const std::optional<GameInit> &high_score_registration()
		const noexcept;
	[[nodiscard]] const std::optional<ClassicMainlWinContext> &win()
		const noexcept;
	[[nodiscard]] std::uint8_t completed_story_stage() const noexcept;
	[[nodiscard]] std::int8_t winner() const noexcept;

private:
	friend std::expected<ClassicMainlPlan, ClassicMainlPlanError>
	classic_story_start_plan(const MainStoryConfig &) noexcept;
	friend std::expected<ClassicMainlPlan, ClassicMainlPlanError>
	classic_story_mainl_plan(
		const GameInit &,
		const CanonicalState &,
		const SessionTransition &
	) noexcept;
	friend std::expected<ClassicMainlPlan, ClassicMainlPlanError>
	classic_story_continue_plan(
		ContinueChoice,
		const SessionTransition &
	) noexcept;
	[[nodiscard]] bool append(
		ClassicMainlScene scene,
		ClassicInterstageCutscene cutscene =
			ClassicInterstageCutscene::none
	) noexcept;

	GameInit resident_{};
	std::optional<GameInit> high_score_registration_{};
	std::optional<ClassicMainlWinContext> win_{};
	std::array<ClassicMainlStep, classic_mainl_max_steps> steps_{};
	std::size_t step_count_ = 0U;
	std::uint8_t completed_story_stage_ = 0U;
	std::int8_t winner_ = 0;
};

[[nodiscard]] std::expected<ClassicMainlPlan, ClassicMainlPlanError>
classic_story_start_plan(const MainStoryConfig &config) noexcept;

[[nodiscard]] std::expected<ClassicMainlPlan, ClassicMainlPlanError>
classic_story_mainl_plan(
	const GameInit &process_start,
	const CanonicalState &terminal,
	const SessionTransition &transition
) noexcept;

[[nodiscard]] std::expected<ClassicMainlPlan, ClassicMainlPlanError>
classic_story_continue_plan(
	ContinueChoice choice,
	const SessionTransition &transition
) noexcept;

} // namespace th03::app

#endif
