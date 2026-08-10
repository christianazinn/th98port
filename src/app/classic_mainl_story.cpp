#include "classic_mainl_story_internal.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace th03::app {
namespace {

constexpr std::uint8_t story_stage_decisive = 6U;
constexpr std::uint8_t story_stage_chiyuri = 7U;
constexpr std::uint8_t story_stage_yumemi = 8U;
constexpr std::uint8_t story_stage_ending = 99U;
constexpr std::uint8_t paletted_chiyuri_first = 15U;

[[nodiscard]] ClassicMainlPlanError error(
	ClassicMainlPlanErrorCode code
) noexcept
{
	return ClassicMainlPlanError{.code = code};
}

[[nodiscard]] bool valid_paletted_character(std::int8_t value) noexcept
{
	return value >= 1 && value <= 18;
}

[[nodiscard]] std::uint8_t base_character(std::int8_t paletted) noexcept
{
	return static_cast<std::uint8_t>(
		(static_cast<std::uint8_t>(paletted) - 1U) / 2U
	);
}

[[nodiscard]] bool story_process_start(const GameInit &resident) noexcept
{
	return resident.game_mode == game_mode::story &&
		resident.story_stage <= story_stage_yumemi &&
		resident.remaining_credits <= 3U &&
		valid_paletted_character(resident.paletted_character[0U]) &&
		valid_paletted_character(resident.paletted_character[1U]) &&
		!resident.runtime_snapshot && !resident.compact_runtime_snapshot &&
		!resident.round_start_snapshot && !resident.practice_rules;
}

[[nodiscard]] bool story_transition_resident(const GameInit &resident) noexcept
{
	return resident.game_mode == game_mode::story &&
		resident.remaining_credits <= 3U &&
		valid_paletted_character(resident.paletted_character[0U]) &&
		valid_paletted_character(resident.paletted_character[1U]) &&
		!resident.runtime_snapshot && !resident.compact_runtime_snapshot &&
		!resident.round_start_snapshot && !resident.practice_rules;
}

[[nodiscard]] std::optional<ClassicMainlWinContext> win_context(
	const GameInit &process_start,
	const CanonicalState &terminal,
	const SessionTransition &transition
) noexcept
{
	const auto winner_index = static_cast<std::size_t>(terminal.winner);
	if(winner_index >= process_start.paletted_character.size()) {
		return std::nullopt;
	}
	const auto loser_index = 1U - winner_index;
	std::uint8_t message = base_character(
		process_start.paletted_character[loser_index]
	);
	if(terminal.winner == 0) {
		if(transition.resident.story_stage == story_stage_decisive) {
			message = 9U;
		} else if(transition.resident.story_stage == story_stage_chiyuri) {
			message = 10U;
		}
	}
	return ClassicMainlWinContext{
		.winner_paletted_character =
			process_start.paletted_character[winner_index],
		.loser_paletted_character =
			process_start.paletted_character[loser_index],
		.message_block = message,
	};
}

} // namespace

bool ClassicMainlPlan::append(
	ClassicMainlScene scene,
	ClassicInterstageCutscene cutscene
) noexcept
{
	if(step_count_ >= steps_.size()) {
		return false;
	}
	steps_[step_count_++] = {scene, cutscene};
	return true;
}

std::span<const ClassicMainlStep> ClassicMainlPlan::steps() const noexcept
{
	return std::span<const ClassicMainlStep>{steps_}.first(step_count_);
}

const GameInit &ClassicMainlPlan::resident() const noexcept
{
	return resident_;
}

const std::optional<GameInit> &ClassicMainlPlan::high_score_registration()
	const noexcept
{
	return high_score_registration_;
}

const std::optional<ClassicMainlWinContext> &ClassicMainlPlan::win()
	const noexcept
{
	return win_;
}

std::uint8_t ClassicMainlPlan::completed_story_stage() const noexcept
{
	return completed_story_stage_;
}

std::int8_t ClassicMainlPlan::winner() const noexcept
{
	return winner_;
}

std::expected<ClassicMainlPlan, ClassicMainlPlanError>
classic_story_start_plan(const MainStoryConfig &config) noexcept
{
	if(!valid_main_story_config(config) || config.resident.story_stage != 0U) {
		return std::unexpected(error(
			ClassicMainlPlanErrorCode::invalid_story_start
		));
	}
	ClassicMainlPlan plan{};
	plan.resident_ = config.resident;
	if(!plan.append(ClassicMainlScene::stage_splash) ||
		!plan.append(ClassicMainlScene::start_main)) {
		return std::unexpected(error(
			ClassicMainlPlanErrorCode::invalid_story_start
		));
	}
	return plan;
}

std::expected<ClassicMainlPlan, ClassicMainlPlanError>
classic_story_mainl_plan(
	const GameInit &process_start,
	const CanonicalState &terminal,
	const SessionTransition &transition
) noexcept
{
	if(!story_process_start(process_start) ||
		terminal.game_mode != game_mode::story ||
		terminal.story_stage != process_start.story_stage ||
		(terminal.winner != 0 && terminal.winner != 1) ||
		(terminal.route != 0U && terminal.route != 2U)) {
		return std::unexpected(error(
			ClassicMainlPlanErrorCode::invalid_terminal_state
		));
	}
	if(!story_transition_resident(transition.resident)) {
		return std::unexpected(error(
			ClassicMainlPlanErrorCode::invalid_transition
		));
	}

	ClassicMainlPlan plan{};
	plan.resident_ = transition.resident;
	plan.high_score_registration_ = transition.high_score_registration;
	plan.completed_story_stage_ = terminal.story_stage;
	plan.winner_ = terminal.winner;

	if(terminal.route == 0U) {
		if(transition.action != SessionAction::exit_to_operator ||
			transition.high_score_registration ||
			transition.resident.story_stage != process_start.story_stage ||
			!plan.append(ClassicMainlScene::exit_to_operator)) {
			return std::unexpected(error(
				ClassicMainlPlanErrorCode::invalid_transition
			));
		}
		return plan;
	}

	if(terminal.winner == 0) {
		if(terminal.story_stage == story_stage_yumemi) {
			const bool extra =
				transition.resident.remaining_credits == 3U &&
				static_cast<std::uint8_t>(
					transition.resident.paletted_character[0U]
				) < paletted_chiyuri_first;
			if(transition.action != SessionAction::ending ||
				transition.resident.story_stage != story_stage_ending ||
				!transition.high_score_registration ||
				!plan.append(ClassicMainlScene::ending) ||
				!plan.append(ClassicMainlScene::high_score_registration) ||
				(extra && !plan.append(ClassicMainlScene::extra_ending)) ||
				!plan.append(ClassicMainlScene::exit_to_operator)) {
				return std::unexpected(error(
					ClassicMainlPlanErrorCode::invalid_transition
				));
			}
			return plan;
		}

		const auto next_stage = static_cast<std::uint8_t>(
			terminal.story_stage + 1U
		);
		if(transition.action != SessionAction::start_gameplay ||
			transition.high_score_registration ||
			transition.resident.story_stage != next_stage) {
			return std::unexpected(error(
				ClassicMainlPlanErrorCode::invalid_transition
			));
		}
		if(next_stage != story_stage_yumemi) {
			plan.win_ = win_context(process_start, terminal, transition);
			if(!plan.win_ || !plan.append(ClassicMainlScene::win)) {
				return std::unexpected(error(
					ClassicMainlPlanErrorCode::invalid_transition
				));
			}
		}
		if(next_stage == story_stage_chiyuri && !plan.append(
			ClassicMainlScene::interstage_cutscene,
			ClassicInterstageCutscene::chiyuri
		)) {
			return std::unexpected(error(
				ClassicMainlPlanErrorCode::invalid_transition
			));
		}
		if(next_stage == story_stage_yumemi && !plan.append(
			ClassicMainlScene::interstage_cutscene,
			ClassicInterstageCutscene::yumemi
		)) {
			return std::unexpected(error(
				ClassicMainlPlanErrorCode::invalid_transition
			));
		}
		if(!plan.append(ClassicMainlScene::stage_splash) ||
			!plan.append(ClassicMainlScene::start_main)) {
			return std::unexpected(error(
				ClassicMainlPlanErrorCode::invalid_transition
			));
		}
		return plan;
	}

	plan.win_ = win_context(process_start, terminal, transition);
	if(!plan.win_ || !transition.high_score_registration ||
		!plan.append(ClassicMainlScene::win) ||
		!plan.append(ClassicMainlScene::high_score_registration)) {
		return std::unexpected(error(
			ClassicMainlPlanErrorCode::invalid_transition
		));
	}
	if(transition.action == SessionAction::request_continue) {
		if(!plan.append(ClassicMainlScene::continue_menu)) {
			return std::unexpected(error(
				ClassicMainlPlanErrorCode::invalid_transition
			));
		}
		return plan;
	}
	if(transition.action == SessionAction::exit_to_operator &&
		transition.resident.remaining_credits == 0U &&
		plan.append(ClassicMainlScene::continue_menu) &&
		plan.append(ClassicMainlScene::game_over) &&
		plan.append(ClassicMainlScene::exit_to_operator)) {
		return plan;
	}
	return std::unexpected(error(
		ClassicMainlPlanErrorCode::invalid_transition
	));
}

std::expected<ClassicMainlPlan, ClassicMainlPlanError>
classic_story_continue_plan(
	ContinueChoice choice,
	const SessionTransition &transition
) noexcept
{
	if(!story_transition_resident(transition.resident) ||
		!transition.high_score_registration) {
		return std::unexpected(error(
			ClassicMainlPlanErrorCode::invalid_continue_transition
		));
	}
	ClassicMainlPlan plan{};
	plan.resident_ = transition.resident;
	if(choice == ContinueChoice::accept &&
		transition.action == SessionAction::start_gameplay &&
		plan.append(ClassicMainlScene::stage_splash) &&
		plan.append(ClassicMainlScene::start_main)) {
		return plan;
	}
	if(choice == ContinueChoice::decline &&
		transition.action == SessionAction::exit_to_operator &&
		plan.append(ClassicMainlScene::game_over) &&
		plan.append(ClassicMainlScene::exit_to_operator)) {
		return plan;
	}
	return std::unexpected(error(
		ClassicMainlPlanErrorCode::invalid_continue_transition
	));
}

} // namespace th03::app
