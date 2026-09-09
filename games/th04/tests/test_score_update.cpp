#include "th04/core/state.hpp"
#include <vector>

using namespace th04::core;
struct Trace : Presentation {
	std::vector<unsigned> events;
	void sound(std::uint8_t v) noexcept override { events.push_back(v); }
	void popup(std::uint8_t v) noexcept override { events.push_back(200U + v); }
	void hud(HudField v) noexcept override { events.push_back(100U + static_cast<unsigned>(v)); }
};
int main()
{
	State s;
	Trace p;
	s.resources = {12, 32, 2, 0};
	s.score.score = {3, 9, 9, 9, 9, 9, 2, 0};
	s.score.score_delta = 1;
	score_update(s, p);
	if (s.score.score != ScoreDigits{3, 0, 0, 0, 0, 0, 3, 0} ||
		s.score.hiscore != s.score.score || s.resources.rem_lives != 3 ||
		s.resources.playperf != 16 || s.score.score_delta != 0 ||
		p.events != std::vector<unsigned>{200, 106, 101, 201, 7}) return 1;
	p.events.clear();
	score_update(s, p);
	if (!p.events.empty() || s.score.score_delta_frame != 1) return 2;
	s = {};
	s.score.score.fill(9);
	s.score.score_delta = 1;
	score_update(s, p);
	if (s.score.score[0] != 9 || s.score.score[7] != 10) return 3;
	for (std::size_t i = 1; i < 7; ++i) if (s.score.score[i] != 0) return 4;
	s = {};
	s.score.score_delta = 1000000;
	score_update(s, p);
	if (s.score.score_delta_frame != 6111 || s.score.score_delta != 993889 ||
		s.score.score != ScoreDigits{0, 1, 1, 1, 6, 0, 0, 0}) return 5;
	// Only the low word is rewritten, even on the frame-delta clamp path.
	s = {};
	s.score.score_delta = 1;
	s.score.score_delta_frame = 0x10001;
	score_update(s, p);
	if (s.score.score_delta_frame != 0x10001 || s.score.score_delta != 0xFFFF0000U)
		return 6;
}
