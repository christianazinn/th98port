#pragma once

#include "pc98/score_digits.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace th04::core {

using ScoreDigits = pc98::ScoreDigits;

struct ScoreState {
	ScoreDigits score{};
	std::uint32_t score_delta{}, score_delta_frame{};
	std::uint8_t score_unused{}, extends_gained{}, hiscore_popup_shown{};
	bool operator==(const ScoreState &) const = default;
};

struct ExtendState {
	std::uint8_t playperf{}, playperf_max{}, rem_lives{}, bullet_clear_time{};
	bool operator==(const ExtendState &) const = default;
};

enum class ScoreEventKind { lives, popup, sound };
struct ScoreEvent {
	ScoreEventKind kind{};
	std::uint8_t value{};
	bool operator==(const ScoreEvent &) const = default;
};
struct ScoreEvents {
	std::array<ScoreEvent, 3> entries{};
	std::size_t count{};
	bool operator==(const ScoreEvents &) const = default;
};

// State effects of th04/main/score_extend.cpp, including playperf_raise.
// A lives request redraws the HUD; popup 1 installs the popup overlay; sound
// 7 plays the extend effect. Consume requests in order before later gameplay.
// The caller invokes this after a score transfer, not on every idle frame.
// Actual HUD/audio rendering and the caller's score-transfer loop are separate.
[[nodiscard]] inline ScoreEvents score_extend_update(ScoreState &score, ExtendState &state) noexcept
{
	bool extend = false;
	switch(score.extends_gained) {
	case 0: extend = score.score[6] >= 3; break;
	case 1: extend = score.score[6] >= 8; break;
	case 2: extend = score.score[7] >= 1 && score.score[6] >= 5; break;
	case 3: extend = score.score[7] >= 2 && score.score[6] >= 2; break;
	case 4: extend = score.score[7] >= 3; break;
	default: break;
	}
	if(!extend) return {};

	// PLAYPERF_RAISE adds in AL, then compares unsigned against the cap.
	const auto raised = static_cast<std::uint8_t>(static_cast<unsigned>(state.playperf) + 4U);
	state.playperf = raised <= state.playperf_max ? raised : state.playperf_max;
	++score.extends_gained;
	if(state.rem_lives > 99) return {};
	++state.rem_lives;
	if(state.bullet_clear_time < 20) state.bullet_clear_time = 20;
	return {{{{ScoreEventKind::lives, state.rem_lives},
		{ScoreEventKind::popup, 1}, {ScoreEventKind::sound, 7}}}, 3};
}

// th04/main/score_reset.cpp. Unlike TH05 this never commits a high score.
inline void score_reset(ScoreState &state)
{
	for(std::size_t i = 1; i < state.score.size(); ++i) {
		state.score[i] = 0;
	}
	state.score_delta = 0;
	state.score_delta_frame = 0;
	state.score_unused = 0;
	state.extends_gained = 0;
	state.hiscore_popup_shown = 0;
}

} // namespace th04::core
