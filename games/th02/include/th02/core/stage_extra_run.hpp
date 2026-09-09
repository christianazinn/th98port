#pragma once

#include "th02/core/stage1_run.hpp"
#include "th02/core/stage_extra_prepare.hpp"
#include "th02/core/stage_extra_entities.hpp"
#include "th02/core/stage1_frame.hpp"
#include "th02/core/boss_intro.hpp"
#include "th02/core/stage1_end.hpp"

namespace th02::core {
struct StageExtraRunState {
	Stage1RunPhase phase{Stage1RunPhase::not_started};
	Stage1FrameSchedule frame{};
	BossIntroState intro{};
	Stage1EndState ending{};
	bool stage_title_enabled{true};
	bool demo{};
	// Typed reached results remain available for first-failure diagnosis.
	Stage1PreparationResult preparation{};
	PlayerBombUpdateOutcome player{};
	StageExtraEntitiesStepResult entities{};
};
struct StageExtraRunPresentation {
	Stage1NotificationPresentation &notifications;
	StageExtraInvalidationPresentation &invalidation;
	Stage1PreparationPresentation &preparation;
	BombAnimationCallbacks &bomb_animation;
	BombPresentationCallbacks &bomb;
	StageExtraEntitiesPresentation &entities;
	Stage1FramePresentation &frame;
	BossIntroPresentation &intro;
	Stage1EndPresentation &ending;
};

// Call after actual stage setup, including assets. This owner does not load
// assets or create valid state. First entry initializes the source's two local
// old-page scroll lines. Gameplay input is the already-sensed (or replay/demo
// substituted) mask. Pause UI remains an explicit external boundary.
[[nodiscard]] Stage1RunWait stage_extra_run_wait(const StageExtraRunState &run) noexcept;
[[nodiscard]] Stage1RunWait stage_extra_run_prepare(Stage1State &state,
	StageExtraRunState &run, StageExtraRunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage_extra_run_input(Stage1State &state,
	StageExtraRunState &run, std::uint16_t input, StageExtraRunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage_extra_run_after_delay(Stage1State &state,
	StageExtraRunState &run, StageExtraRunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage_extra_run_acknowledgement(Stage1State &state,
	StageExtraRunState &run, bool pressed, StageExtraRunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage_extra_run_after_pause(Stage1State &state,
	StageExtraRunState &run, bool quit_selected, StageExtraRunPresentation &presentation) noexcept;
} // namespace th02::core
