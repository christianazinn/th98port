#pragma once

#include "th02/core/stage1_run.hpp"
#include "th02/core/stage5_prepare.hpp"
#include "th02/core/stage5_entities.hpp"
#include "th02/core/stage1_frame.hpp"
#include "th02/core/boss_intro.hpp"
#include "th02/core/stage5_end.hpp"

namespace th02::core {
enum class Stage5RunPhase : std::uint8_t {
	not_started, ready, input, paused, intro, ending, maine, quit, refused,
};
struct Stage5RunState {
	Stage5RunPhase phase{Stage5RunPhase::not_started};
	Stage1FrameSchedule frame{};
	BossIntroState intro{};
	Stage5EndState ending{};
	bool stage_title_enabled{true};
	bool demo{};
	// Typed reached results remain available for first-failure diagnosis.
	Stage1PreparationResult preparation{};
	PlayerBombUpdateOutcome player{};
	Stage5EntitiesStepResult entities{};
};
struct Stage5RunPresentation {
	Stage1NotificationPresentation &notifications;
	Stage5InvalidationPresentation &invalidation;
	Stage1PreparationPresentation &preparation;
	BombAnimationCallbacks &bomb_animation;
	BombPresentationCallbacks &bomb;
	Stage5EntitiesPresentation &entities;
	Stage1FramePresentation &frame;
	BossIntroPresentation &intro;
	Stage5EndPresentation &ending;
};

// Call after actual stage setup, including assets. This owner does not load
// assets or create valid state. First entry initializes the source's two local
// old-page scroll lines. Gameplay input is the already-sensed (or replay/demo
// substituted) mask. Pause UI remains an explicit external boundary.
[[nodiscard]] Stage1RunWait stage5_run_wait(const Stage5RunState &run) noexcept;
[[nodiscard]] Stage1RunWait stage5_run_prepare(StageState &state,
	Stage5RunState &run, Stage5RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage5_run_input(StageState &state,
	Stage5RunState &run, std::uint16_t input, Stage5RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage5_run_after_delay(StageState &state,
	Stage5RunState &run, Stage5RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage5_run_acknowledgement(StageState &state,
	Stage5RunState &run, bool pressed, Stage5RunPresentation &presentation) noexcept;
[[nodiscard]] Stage1RunWait stage5_run_after_pause(StageState &state,
	Stage5RunState &run, bool quit_selected, Stage5RunPresentation &presentation) noexcept;
} // namespace th02::core
