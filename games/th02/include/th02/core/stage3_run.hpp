#pragma once

#include "th02/core/stage1_run.hpp"
#include "th02/core/stage3_prepare.hpp"
#include "th02/core/stage3_entities.hpp"
#include "th02/core/stage1_frame.hpp"
#include "th02/core/stage1_intro.hpp"
#include "th02/core/stage1_end.hpp"

namespace th02::core {
struct Stage3RunState {
	Stage1RunPhase phase{Stage1RunPhase::not_started};
	Stage1FrameSchedule frame{};
	Stage1IntroState intro{};
	Stage1EndState ending{};
	bool stage_title_enabled{true};
	bool demo{};
	// Typed reached results remain available for first-failure diagnosis.
	Stage1PreparationResult preparation{};
	PlayerBombUpdateOutcome player{};
	Stage3EntitiesStepResult entities{};
};
struct Stage3RunPresentation {
	Stage1NotificationPresentation &notifications;
	Stage3InvalidationPresentation &invalidation;
	Stage1PreparationPresentation &preparation;
	BombAnimationCallbacks &bomb_animation;
	BombPresentationCallbacks &bomb;
	Stage3EntitiesPresentation &entities;
	Stage1FramePresentation &frame;
	Stage1IntroPresentation &intro;
	Stage1EndPresentation &ending;
};

// Call after actual stage setup, including assets. This owner does not load
// assets or create valid state. First entry initializes the source's two local
// old-page scroll lines. Gameplay input is the already-sensed (or replay/demo
// substituted) mask. Pause UI remains an explicit external boundary.
[[nodiscard]] Stage1RunWait stage3_run_wait(const Stage3RunState &run) noexcept;
[[nodiscard]] Stage1RunWait stage3_run_prepare(Stage1State &state,
	Stage3RunState &run, Stage3RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage3_run_input(Stage1State &state,
	Stage3RunState &run, std::uint16_t input, Stage3RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage3_run_after_delay(Stage1State &state,
	Stage3RunState &run, Stage3RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage3_run_acknowledgement(Stage1State &state,
	Stage3RunState &run, bool pressed, Stage3RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage3_run_after_pause(Stage1State &state,
	Stage3RunState &run, bool quit_selected, Stage3RunPresentation &presentation) noexcept;
} // namespace th02::core
