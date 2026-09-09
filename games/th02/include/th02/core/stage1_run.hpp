#pragma once

#include "th02/core/stage1_prepare.hpp"
#include "th02/core/stage1_frame.hpp"
#include "th02/core/stage1_intro.hpp"
#include "th02/core/stage1_end.hpp"

namespace th02::core {
enum class Stage1RunPhase : std::uint8_t {
	not_started, ready, input, paused, intro, ending, cleared, quit, refused,
};
enum class Stage1RunWait : std::uint8_t {
	ready_for_frame, input_sample, paused, delay_one_frame, delay_two_frames,
	acknowledgement_first_sample, acknowledgement_second_sample, cleared, quit, refused,
	maine,
};
struct Stage1RunState {
	Stage1RunPhase phase{Stage1RunPhase::not_started};
	Stage1FrameSchedule frame{};
	Stage1IntroState intro{};
	Stage1EndState ending{};
	bool stage_title_enabled{true};
	bool demo{};
	// Typed reached results remain available for first-failure diagnosis.
	Stage1PreparationResult preparation{};
	PlayerBombUpdateOutcome player{};
	Stage1EntitiesStepResult entities{};
};
struct Stage1RunPresentation {
	Stage1NotificationPresentation &notifications;
	Stage1InvalidationPresentation &invalidation;
	Stage1PreparationPresentation &preparation;
	BombAnimationCallbacks &bomb_animation;
	BombPresentationCallbacks &bomb;
	Stage1EntitiesPresentation &entities;
	Stage1FramePresentation &frame;
	Stage1IntroPresentation &intro;
	Stage1EndPresentation &ending;
};

// Call after actual stage setup, including assets. This owner does not load
// assets or create valid state. First entry initializes the source's two local
// old-page scroll lines. Gameplay input is the already-sensed (or replay/demo
// substituted) mask. Pause UI remains an explicit external boundary.
[[nodiscard]] Stage1RunWait stage1_run_wait(const Stage1RunState &run) noexcept;
[[nodiscard]] Stage1RunWait stage1_run_prepare(Stage1State &state,
	Stage1RunState &run, Stage1RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage1_run_input(Stage1State &state,
	Stage1RunState &run, std::uint16_t input, Stage1RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage1_run_after_delay(Stage1State &state,
	Stage1RunState &run, Stage1RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage1_run_acknowledgement(Stage1State &state,
	Stage1RunState &run, bool pressed, Stage1RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage1_run_after_pause(Stage1State &state,
	Stage1RunState &run, bool quit_selected, Stage1RunPresentation &presentation) noexcept;
} // namespace th02::core
