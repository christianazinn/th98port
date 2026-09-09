#include "th05/core/state.hpp"

namespace th05::core {
// TH05's shared scoreupd.asm body clears the upper temporary digits and has
// no score-based extend call. Preserve the source's low-word frame-delta stores.
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
	std::array<std::uint8_t, 8> digits{};
	constexpr std::array<std::uint16_t, 4> powers{10000, 1000, 100, 10};
	for (std::size_t i = 0; i < powers.size(); ++i) {
		digits[4 - i] = static_cast<std::uint8_t>(remaining / powers[i]);
		remaining = static_cast<std::uint16_t>(remaining % powers[i]);
	}
	digits[0] = static_cast<std::uint8_t>(remaining);
	for (std::size_t i = 1; i < 7; ++i) {
		const auto a = digits[i - 1];
		const auto b = s.score[i];
		auto ax = static_cast<std::uint16_t>(static_cast<std::uint8_t>(a + b));
		if ((ax & 15U) > 9 || (a & 15U) + (b & 15U) > 15)
			ax = static_cast<std::uint16_t>(ax + 0x106);
		s.score[i] = static_cast<std::uint8_t>(ax & 15U);
		s.score[i + 1] = static_cast<std::uint8_t>(s.score[i + 1] + (ax >> 8));
	}
	s.score[7] = static_cast<std::uint8_t>(s.score[7] + digits[6]);
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
			p.popup(0);
		}
	}
	s.score_delta -= s.score_delta_frame;
	p.hud(HudField::score);
}
}
