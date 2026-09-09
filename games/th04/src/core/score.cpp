#include "th04/core/state.hpp"

namespace th04::core {
// th04/main/scoreupd.asm. Keep the source's low-word stores into its dword
// frame delta and its unnormalized highest score digit.
void score_update(State &state, Presentation &p) noexcept
{
	auto &s = state.score;
	if (s.score_delta == 0) return;
	const auto store_frame_low = [&](std::uint16_t value) {
		s.score_delta_frame = (s.score_delta_frame & 0xFFFF0000U) | value;
	};
	if (s.score_delta_frame > s.score_delta)
		store_frame_low(static_cast<std::uint16_t>(s.score_delta));
	auto calculated = s.score_delta >> 5;
	if (calculated == 0) calculated = 1;
	if (calculated > 6111) calculated = 6111;
	if (static_cast<std::uint16_t>(s.score_delta_frame) < calculated)
		store_frame_low(static_cast<std::uint16_t>(calculated));
	auto remaining = static_cast<std::uint16_t>(s.score_delta_frame);
	constexpr std::array<std::uint16_t, 4> powers{10000, 1000, 100, 10};
	for (std::size_t i = 0; i < powers.size(); ++i) {
		s.temp_lebcd[4 - i] = static_cast<std::uint8_t>(remaining / powers[i]);
		remaining = static_cast<std::uint16_t>(remaining % powers[i]);
	}
	s.temp_lebcd[0] = static_cast<std::uint8_t>(remaining);
	for (std::size_t i = 1; i < 7; ++i) {
		const auto a = s.temp_lebcd[i - 1];
		const auto b = s.score[i];
		auto ax = static_cast<std::uint16_t>(static_cast<std::uint8_t>(a + b));
		if ((ax & 15U) > 9 || (a & 15U) + (b & 15U) > 15)
			ax = static_cast<std::uint16_t>(ax + 0x106);
		s.score[i] = static_cast<std::uint8_t>(ax & 15U);
		s.score[i + 1] = static_cast<std::uint8_t>(s.score[i + 1] + (ax >> 8));
	}
	s.score[7] = static_cast<std::uint8_t>(s.score[7] + s.temp_lebcd[6]);
	bool is_hiscore = true;
	if (s.hiscore_popup_shown == 0) {
		for (int i = 7; i >= 0; --i) {
			const auto n = static_cast<std::size_t>(i);
			if (s.hiscore[n] > s.score[n]) { is_hiscore = false; break; }
			if (s.hiscore[n] < s.score[n]) break;
		}
	}
	if (is_hiscore) {
		s.hiscore = s.score;
		if (s.hiscore_popup_shown == 0) {
			s.hiscore_popup_shown = 1;
			p.popup(0); // POPUP_ID_HISCORE_ENTRY
		}
	}
	s.score_delta -= s.score_delta_frame;
	p.hud(HudField::score);
	s.score_unused = 0;
	const auto events = score_extend_update(s, state.resources);
	for (std::size_t i = 0; i < events.count; ++i) {
		const auto &event = events.entries[i];
		switch (event.kind) {
		case ScoreEventKind::lives: p.hud(HudField::lives); break;
		case ScoreEventKind::sound: p.sound(static_cast<std::uint8_t>(event.value)); break;
		case ScoreEventKind::popup: p.popup(static_cast<std::uint8_t>(event.value)); break;
		}
	}
}
} // namespace th04::core
