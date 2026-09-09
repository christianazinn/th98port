#include "th04/core/midboss_extra.hpp"
#include "th04/core/state.hpp"
#include "th04/core/stage.hpp"
#include <iostream>

using namespace th04::core;
int main()
{
	State s;
	Presentation p;
	if (!gameplay_init(s, 4, 0, 0, 3, 3, false)) return 1;
	s.stage_id = 6;
	s.random.seed = 318;
	stage_init(s, p);
	extra_midboss_setup(s);
	s.stage_frame = 5399;
	extra_midboss_activate(s);
	if (s.midboss_active) return 2;
	++s.stage_frame;
	extra_midboss_activate(s);
	if (!s.midboss_active || s.midboss.hp != 4096 || s.midboss.angle != 0x60) return 3;
	const auto initial = s.midboss.pos.cur;
	if (!extra_midboss_update(s, p) || s.bullets.template_.origin != initial ||
		s.midboss.pos.prev != initial || s.midboss.hp != 4064 ||
		s.midboss.angle != 0x5E || s.midboss.phase_frame != 1) return 4;
	unsigned ticks = 1;
	while (s.midboss_active && ticks < 3000) {
		++s.stage_frame;
		if (!extra_midboss_update(s, p)) return 5;
		++ticks;
	}
	// Existing original-execution audit: 2594 updates, 2593 rendered frames.
	if (ticks != 2594 || s.midboss_active || s.midboss.hp != 0) {
		std::cerr << "fly-by ticks=" << ticks << " x=" << s.midboss.pos.cur.x << '\n';
		return 6;
	}
	// A phase-5 gift does not consume a shot or HP; HP remains orbit radius.
	extra_midboss_setup(s);
	s.midboss.phase = 5;
	s.midboss.phase_frame = 450;
	s.midboss.hp = 768;
	s.items.items.fill({});
	if (!extra_midboss_update(s, p) || s.midboss.hp != 768 ||
		s.midboss.phase_frame != 451) return 7;
	bool one_up = false;
	for (const auto &item : s.items.items) {
		if (item.type == static_cast<std::uint8_t>(ItemType::one_up)) one_up = true;
	}
	if (!one_up) return 8;
	return 0;
}
