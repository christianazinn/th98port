#pragma once

#include "th02/core/stage1_run.hpp"
#include "th02/core/stage4_prepare.hpp"
#include "th02/core/stage4_entities.hpp"
#include "th02/core/stage1_frame.hpp"
#include "th02/core/boss_intro.hpp"
#include "th02/core/stage1_end.hpp"

namespace th02::core {
struct Stage4RunState {
	Stage1RunPhase phase{Stage1RunPhase::not_started};
	Stage1FrameSchedule frame{};
	BossIntroState intro{};
	Stage1EndState ending{};
	bool stage_title_enabled{true};
	bool demo{};
	// Typed reached results remain available for first-failure diagnosis.
	Stage1PreparationResult preparation{};
	PlayerBombUpdateOutcome player{};
	Stage4EntitiesStepResult entities{};
};
struct Stage4RunPresentation {
	Stage1NotificationPresentation &notifications;
	Stage4InvalidationPresentation &invalidation;
	Stage1PreparationPresentation &preparation;
	BombAnimationCallbacks &bomb_animation;
	BombPresentationCallbacks &bomb;
	Stage4EntitiesPresentation &entities;
	Stage1FramePresentation &frame;
	BossIntroPresentation &intro;
	Stage1EndPresentation &ending;
};

// Call after actual stage setup, including assets. This owner does not load
// assets or create valid state. First entry initializes the source's two local
// old-page scroll lines. Gameplay input is the already-sensed (or replay/demo
// substituted) mask. Pause UI remains an explicit external boundary.
[[nodiscard]] Stage1RunWait stage4_run_wait(const Stage4RunState &run) noexcept;
[[nodiscard]] Stage1RunWait stage4_run_prepare(Stage1State &state,
	Stage4RunState &run, Stage4RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage4_run_input(Stage1State &state,
	Stage4RunState &run, std::uint16_t input, Stage4RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage4_run_after_delay(Stage1State &state,
	Stage4RunState &run, Stage4RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage4_run_acknowledgement(Stage1State &state,
	Stage4RunState &run, bool pressed, Stage4RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage4_run_after_pause(Stage1State &state,
	Stage4RunState &run, bool quit_selected, Stage4RunPresentation &presentation) noexcept;
} // namespace th02::core
