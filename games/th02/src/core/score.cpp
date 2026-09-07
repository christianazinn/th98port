#include "th02/core/score.hpp"

#include <bit>

namespace th02::core {
namespace {
// th02_main.asm::_EXTEND_SCORES, in internal score units (not HUD digits).
constexpr std::array<std::int32_t, 6> extend_scores{
	100000, 200000, 300000, 500000, 800000, 99999999};

std::int32_t add_dword(std::int32_t a, std::int32_t b) noexcept
{
	return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a) +
		static_cast<std::uint32_t>(b));
}

ScoreEvents process_score(ScoreState &state, bool turn_capped_lives_into_bombs) noexcept
{
	ScoreEvents events{};
	auto emit = [&events](ScoreEventKind kind, std::int32_t value) {
		events.entries[events.count++] = {kind, value};
	};
	emit(ScoreEventKind::score, state.score);
	if(state.score > state.hiscore) {
		state.hiscore = state.score;
		emit(ScoreEventKind::hiscore, state.hiscore);
	}
	if(extend_scores[state.extends_gained] <= state.score) {
		if(state.lives < 5) {
			++state.lives;
			emit(ScoreEventKind::lives, state.lives);
			emit(ScoreEventKind::sound, 8);
		} else if(turn_capped_lives_into_bombs) {
			if(state.bombs < 5) {
				++state.bombs;
				emit(ScoreEventKind::bombs, state.bombs);
			}
			// The update path requests the sound even when both are capped.
			emit(ScoreEventKind::sound, 8);
		}
		++state.extends_gained; // At most one threshold per call, not a loop.
	}
	return events;
}
}

std::optional<ScoreEvents> score_update(ScoreState &state, bool reduce_effects) noexcept
{
	if(state.score_delta == 0) {
		state.score_delta_transferred_prev = 0;
		return ScoreEvents{};
	}
	if(state.extends_gained >= extend_scores.size()) return std::nullopt;
	if(!reduce_effects) {
		auto to_transfer = state.score_delta;
		if(to_transfer < 32) to_transfer = 32;
		to_transfer >>= 5;
		if(state.score_delta_transferred_prev < to_transfer) {
			if(to_transfer > 1111) to_transfer = 1111;
			state.score_delta_transferred_prev = static_cast<std::uint16_t>(to_transfer);
		}
	} else {
		state.score_delta_transferred_prev = 3111;
	}
	auto delta_rem = add_dword(state.score_delta, -static_cast<std::int32_t>(state.score_delta_transferred_prev));
	if(delta_rem < 0) {
		state.score_delta_transferred_prev = static_cast<std::uint16_t>(state.score_delta);
		delta_rem = 0;
	}
	state.score_delta = delta_rem;
	state.score = add_dword(state.score, state.score_delta_transferred_prev);
	return process_score(state, true);
}

std::optional<ScoreEvents> score_grant_current_delta_as_bonus(ScoreState &state) noexcept
{
	if(state.extends_gained >= extend_scores.size()) return std::nullopt;
	state.score = add_dword(state.score, state.score_delta);
	return process_score(state, false);
}

} // namespace th02::core
