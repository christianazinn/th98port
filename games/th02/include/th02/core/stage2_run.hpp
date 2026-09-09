#pragma once

#include "th02/core/stage1_run.hpp"
#include "th02/core/stage2_prepare.hpp"
#include "th02/core/stage2_entities.hpp"
#include "th02/core/stage1_frame.hpp"
#include "th02/core/stage2_intro.hpp"
#include "th02/core/stage1_end.hpp"

namespace th02::core {
struct Stage2RunState {
	Stage1RunPhase phase{Stage1RunPhase::not_started};
	Stage1FrameSchedule frame{};
	Stage2IntroState intro{};
	Stage1EndState ending{};
	bool stage_title_enabled{true};
	bool demo{};
	// Typed reached results remain available for first-failure diagnosis.
	Stage1PreparationResult preparation{};
	PlayerBombUpdateOutcome player{};
	Stage2EntitiesStepResult entities{};
};
struct Stage2RunPresentation {
	Stage1NotificationPresentation &notifications;
	Stage2InvalidationPresentation &invalidation;
	Stage1PreparationPresentation &preparation;
	BombAnimationCallbacks &bomb_animation;
	BombPresentationCallbacks &bomb;
	Stage2EntitiesPresentation &entities;
	Stage1FramePresentation &frame;
	Stage2IntroPresentation &intro;
	Stage1EndPresentation &ending;
};

// Call after actual stage setup, including assets. This owner does not load
// assets or create valid state. First entry initializes the source's two local
// old-page scroll lines. Gameplay input is the already-sensed (or replay/demo
// substituted) mask. Pause UI remains an explicit external boundary.
[[nodiscard]] Stage1RunWait stage2_run_wait(const Stage2RunState &run) noexcept;
[[nodiscard]] Stage1RunWait stage2_run_prepare(Stage1State &state,
	Stage2RunState &run, Stage2RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage2_run_input(Stage1State &state,
	Stage2RunState &run, std::uint16_t input, Stage2RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage2_run_after_delay(Stage1State &state,
	Stage2RunState &run, Stage2RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage2_run_acknowledgement(Stage1State &state,
	Stage2RunState &run, bool pressed, Stage2RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage2_run_after_pause(Stage1State &state,
	Stage2RunState &run, bool quit_selected, Stage2RunPresentation &presentation) noexcept;
} // namespace th02::core
