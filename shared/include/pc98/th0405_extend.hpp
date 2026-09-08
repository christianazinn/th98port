#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace pc98::th0405 {

struct ExtendState {
	std::uint8_t playperf{}, playperf_max{}, rem_lives{}, bullet_clear_time{};
	bool operator==(const ExtendState &) const = default;
};

enum class ExtendEventKind { lives, popup, sound };
struct ExtendEvent {
	ExtendEventKind kind{};
	std::uint8_t value{};
	bool operator==(const ExtendEvent &) const = default;
};
struct ExtendEvents {
	std::array<ExtendEvent, 3> entries{};
	std::size_t count{};
	bool operator==(const ExtendEvents &) const = default;
};

// Identical award order in th04/main/score_extend.cpp and
// th05/main/item/collect.cpp. Their admission predicates and life limits
// differ; callers supply 100 for TH04 and 99 for TH05. Presentation requests
// replace HUD drawing, popup installation and sound playback, in source order.
[[nodiscard]] inline ExtendEvents grant_extend(
	std::uint8_t &extends_gained, ExtendState &state, std::uint8_t life_limit) noexcept
{
	// The shared PLAYPERF_RAISE adds in AL before its unsigned cap comparison.
	const auto raised = static_cast<std::uint8_t>(static_cast<unsigned>(state.playperf) + 4U);
	state.playperf = raised <= state.playperf_max ? raised : state.playperf_max;
	++extends_gained;
	if(state.rem_lives >= life_limit) return {};
	++state.rem_lives;
	if(state.bullet_clear_time < 20) state.bullet_clear_time = 20;
	return {{{{ExtendEventKind::lives, state.rem_lives},
		{ExtendEventKind::popup, 1}, {ExtendEventKind::sound, 7}}}, 3};
}

} // namespace pc98::th0405
