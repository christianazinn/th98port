#include "th02/core/meira.hpp"

#include <bit>

namespace th02::core {
namespace {
std::int16_t word_add(std::int16_t a, std::int16_t b) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::int32_t>(a) + b));
}
std::int16_t scroll_y(std::int16_t y, std::int16_t line) noexcept
{
	y = word_add(y, line);
	return y >= 400 ? word_add(y, -400) : y;
}
} // namespace

void meira_slashes_add(MeiraState &meira, ScreenPoint origin,
	std::uint8_t angle, std::uint8_t group, std::uint8_t speed,
	std::uint8_t trail_frames) noexcept
{
	for (auto &slash : meira.slashes) {
		if (slash.flag != 0) continue;
		slash.flag = 1;
		slash.group = group;
		slash.left = origin.x;
		slash.top = origin.y;
		slash.angle = angle;
		slash.age = 0;
		slash.speed = speed;
		slash.trail_patnum = meira.slash_trail_patnum;
		slash.trail_frames = trail_frames;
		return;
	}
}

bool meira_slash_step(BossState &boss, MeiraState &meira,
	std::size_t page_back, std::uint8_t mirrored,
	std::int16_t delta_x, std::int16_t delta_y) noexcept
{
	if (page_back >= boss.topleft_on_page.size()) return false;
	auto &point = boss.topleft_on_page[page_back];
	if (mirrored == 0) {
		point.x = word_add(point.x, delta_x);
		if (point.x > 368) point.x = 368;
	} else {
		point.x = word_add(point.x, word_add(static_cast<std::int16_t>(~delta_x), 1));
		if (point.x < 16) point.x = 16;
	}
	point.y = word_add(point.y, delta_y);
	meira_slashes_add(meira, {word_add(point.x, 12), word_add(point.y, 12)},
		0, meira.burst_group, meira.burst_speed,
		static_cast<std::uint8_t>(meira.slash_trail_frames));
	return true;
}

void meira_slashes_invalidate(MeiraState &meira, MeiraCallbacks &callbacks) noexcept
{
	for (auto &slash : meira.slashes) {
		if (slash.flag == 0) continue;
		callbacks.invalidate_meira({{slash.left, slash.top}, 32, 32});
		if (slash.flag == 2) slash.flag = 0;
	}
}

bool meira_bg_render(BossState &boss, MeiraState &meira,
	const PlayerFrameState &frame, MeiraCallbacks &callbacks) noexcept
{
	if (frame.page_back >= boss.topleft_on_page.size()) return false;
	auto &back = boss.topleft_on_page[frame.page_back];
	callbacks.invalidate_meira({back, 64, 64});
	if (meira.afterimages_active != 0) {
		for (std::size_t i = 0; i < 3; ++i) {
			const ScreenPoint trail{meira.afterimage_left[frame.page_back][i],
				meira.afterimage_top[frame.page_back][i]};
			if (trail.x != back.x && trail.y != back.y)
				callbacks.invalidate_meira({trail, 64, 64});
			if (frame.page_front >= boss.topleft_on_page.size()) return false;
			meira.afterimage_left[frame.page_back][i] = meira.afterimage_left[frame.page_front][i];
			meira.afterimage_top[frame.page_back][i] = meira.afterimage_top[frame.page_front][i];
		}
	}
	if (frame.page_front >= boss.topleft_on_page.size()) return false;
	back = boss.topleft_on_page[frame.page_front];
	meira_slashes_invalidate(meira, callbacks);
	return true;
}

EnemyBulletAddResult meira_slashes_update_and_render(MeiraState &meira,
	EnemyBulletPoolState &bullets, RandomState &random, const PlayerFrameState &frame,
	PlayerHitState &player, std::int16_t playperf, std::int8_t rank,
	bool bombing, std::int16_t scroll_line, MeiraCallbacks &callbacks) noexcept
{
	for (auto &slash : meira.slashes) {
		if (slash.flag != 1) continue;
		if (slash.trail_frames > 0) {
			callbacks.render_meira({MeiraSpriteKind::slash_trail,
				{slash.left, scroll_y(slash.top, scroll_line)}, slash.trail_patnum});
			--slash.trail_frames;
			const auto &position = frame.player.current;
			if (word_add(slash.left, -16) <= position.x && slash.left >= position.x &&
				word_add(slash.top, -16) < position.y && slash.top > position.y)
				player.raw_hit = 1;
		} else if (slash.age < 32) {
			if (slash.age == 0) {
				callbacks.sound_effect(10);
				slash.left = word_add(slash.left, -8);
				slash.top = word_add(slash.top, -8);
			}
			callbacks.render_meira({MeiraSpriteKind::slash_bloom,
				{slash.left, scroll_y(slash.top, scroll_line)},
				static_cast<std::int16_t>((slash.age >> 2) + 26)});
			++slash.age;
		} else {
			if ((meira.slash_burst_i & 1) <= rank) {
				const auto result = bullets_add_pellet(bullets, random, frame, playperf,
					rank, bombing, {word_add(slash.left, 12), word_add(slash.top, 12)},
					slash.angle, slash.group, slash.speed);
				if (result != EnemyBulletAddResult::completed) return result;
			}
			++meira.slash_burst_i;
			slash.flag = 2;
		}
	}
	return EnemyBulletAddResult::completed;
}
bool meira_dash_pattern_update(BossState &boss, MeiraState &meira,
	const PlayerFrameState &frame, MeiraDashPattern pattern, MeiraCallbacks &callbacks) noexcept
{
	if (frame.page_back >= boss.topleft_on_page.size()) return false;
	auto &point = boss.topleft_on_page[frame.page_back];
	const auto clock = boss.phase_frame;
	const auto mirrored = meira.player_is_right;
	const auto step = [&](std::int16_t x, std::int16_t y) {
		(void)meira_slash_step(boss, meira, frame.page_back, meira.player_is_right, x, y);
	};
	if (pattern == MeiraDashPattern::fall) {
		if (clock < 10) {
		} else if (clock == 10) { callbacks.sound_effect(9); boss.patnum = 142;
		} else if (clock < 50) {
		} else if (clock == 50) { callbacks.sound_effect(3); boss.patnum = 143;
		} else if (clock <= 90) { point.y = word_add(point.y, 6);
		} else { boss.patnum = 141; boss.phase_frame = 0; }
	} else if (pattern == MeiraDashPattern::rising_slashes) {
		if (clock < 20) return true;
		const auto old_top = point.y;
		if (clock == 20) {
			meira.player_is_right = static_cast<std::uint8_t>(word_add(point.x, 16) < frame.player.current.x);
			boss.patnum = static_cast<std::int16_t>(meira.player_is_right + 144);
			meira.slash_trail_frames = 16;
			meira.slash_trail_patnum = static_cast<std::uint8_t>(121 - meira.player_is_right);
			meira.burst_group = enemy_bullet_group::one_aimed;
			meira.burst_speed = 64;
		} else if (clock <= 32) {
			step(-8, -8); meira.slash_trail_frames = word_add(meira.slash_trail_frames, 2);
		} else if (clock < 56) { boss.patnum = 141;
		} else if (clock == 56) {
			boss.patnum = static_cast<std::int16_t>(145 - mirrored);
			meira.slash_trail_patnum = static_cast<std::uint8_t>(120 + mirrored);
			meira.slash_trail_frames = 16;
		} else if (clock == 57) {
			step(8, -8); meira.slash_trail_frames = word_add(meira.slash_trail_frames, 2);
			if (old_top > 48) boss.phase_frame = 56;
		} else if (clock < 80) { boss.patnum = 141;
		} else if (clock == 80) {
			boss.patnum = static_cast<std::int16_t>(149 - mirrored);
			meira.slash_trail_patnum = static_cast<std::uint8_t>(121 - mirrored);
			meira.slash_trail_frames = 16;
		} else if (point.x > 32 && point.x < 352) {
			step(8, 8); meira.slash_trail_frames = word_add(meira.slash_trail_frames, 2);
		} else if (old_top > 48) { point.y = word_add(point.y, -2); boss.patnum = 141;
		} else { boss.phase_frame = 0; }
	} else if (pattern == MeiraDashPattern::double_slash) {
		if (clock < 50) {
		} else if (clock == 50) {
			callbacks.sound_effect(9); boss.patnum = 142; meira.burst_group = boss.rank_param[0];
		} else if (clock < 99) {
		} else if (clock == 99) {
			callbacks.sound_effect(3);
			boss.patnum = static_cast<std::int16_t>(149 - mirrored); // Original mirrored sprite.
			meira.slash_trail_patnum = static_cast<std::uint8_t>(120 + mirrored);
			meira.slash_trail_frames = 80;
		} else if (clock < 120) {
			step(-8, 8); meira.slash_trail_frames = word_add(meira.slash_trail_frames, -1);
		} else if (clock < 136) {
			boss.patnum = 142;
			boss.patnum = static_cast<std::int16_t>(mirrored + 144);
			meira.slash_trail_patnum = static_cast<std::uint8_t>(121 - mirrored);
			meira.slash_trail_frames = word_add(meira.slash_trail_frames, -1);
			if (clock == 135) callbacks.sound_effect(3);
		} else if (clock < 156) {
			step(-8, -8); meira.slash_trail_frames = word_add(meira.slash_trail_frames, -1);
		} else if (point.x != 192) {
			boss.patnum = 141; point.x = word_add(point.x, mirrored == 0 ? 2 : -2);
		} else { boss.phase_frame = 0; }
	} else if (pattern == MeiraDashPattern::four_slashes) {
		if (clock < 50) {
		} else if (clock == 50) {
			meira.player_is_right = static_cast<std::uint8_t>(word_add(point.x, 16) < frame.player.current.x);
			callbacks.sound_effect(9); boss.patnum = 142;
			meira.burst_group = enemy_bullet_group::one_random_angle; meira.burst_speed = 48;
		} else if (clock < 99) {
		} else if (clock == 99) {
			callbacks.sound_effect(3); boss.patnum = static_cast<std::int16_t>(mirrored + 148);
			meira.slash_trail_patnum = static_cast<std::uint8_t>(120 + mirrored); meira.slash_trail_frames = 20;
		} else if (clock < 116) {
			step(-8, 8); meira.slash_trail_frames = word_add(meira.slash_trail_frames, 2);
		} else if (clock < 136) {
			boss.patnum = 142; boss.patnum = static_cast<std::int16_t>(149 - mirrored);
			meira.slash_trail_patnum = static_cast<std::uint8_t>(121 - mirrored); meira.slash_trail_frames = 20;
			if (clock == 135) callbacks.sound_effect(3);
		} else if (clock < 152) {
			step(8, 8); meira.slash_trail_frames = word_add(meira.slash_trail_frames, 2);
		} else if (clock < 172) {
			boss.patnum = 142; boss.patnum = static_cast<std::int16_t>(145 - mirrored);
			meira.slash_trail_patnum = static_cast<std::uint8_t>(120 + mirrored); meira.slash_trail_frames = 20;
			if (clock == 171) callbacks.sound_effect(3);
		} else if (clock < 188) {
			step(8, -8); meira.slash_trail_frames = word_add(meira.slash_trail_frames, 2);
		} else if (clock < 208) {
			boss.patnum = 142; boss.patnum = static_cast<std::int16_t>(mirrored + 144);
			meira.slash_trail_patnum = static_cast<std::uint8_t>(121 - mirrored); meira.slash_trail_frames = 20;
			if (clock == 207) callbacks.sound_effect(3);
		} else if (clock < 224) {
			step(-8, -8); meira.slash_trail_frames = word_add(meira.slash_trail_frames, 2);
		} else { boss.patnum = 141; boss.phase_frame = 0; }
	} else return false;
	return true;
}
namespace {
MeiraUpdateResult bullet_result(EnemyBulletAddResult result) noexcept
{
	if (result == EnemyBulletAddResult::invalid_page) return MeiraUpdateResult::bullet_invalid_page;
	if (result == EnemyBulletAddResult::source_iatan2_fault) return MeiraUpdateResult::bullet_iatan2_fault;
	return MeiraUpdateResult::boss;
}
MeiraUpdateResult pellet(MeiraCombatContext &c, ScreenPoint point,
	std::uint8_t angle, std::uint8_t group, std::int16_t speed) noexcept
{
	return bullet_result(bullets_add_pellet(c.bullets, c.random, c.frame,
		c.playperf, c.rank, c.bombing, point, angle, group, speed));
}
MeiraUpdateResult balls(MeiraCombatContext &c, ScreenPoint point, unsigned count) noexcept
{
	for (unsigned i = 0; i < count; ++i) {
		const auto angle = randring_next8(c.random);
		const auto result = bullet_result(bullets_add_16x16(c.bullets, c.random, c.frame,
			c.playperf, c.rank, c.bombing, {word_add(point.x, 24), word_add(point.y, 24)},
			angle, enemy_bullet_special_motion::bounce_left_right_top_bottom,
			static_cast<std::int16_t>(122U + (i & 1U)), 54));
		if (result != MeiraUpdateResult::boss) return result;
	}
	return MeiraUpdateResult::boss;
}

MeiraUpdateResult stream(BossState &boss, MeiraState &meira, MeiraCombatContext &c,
	MeiraCallbacks &callbacks, bool returning) noexcept
{
	auto &speed = returning ? meira.return_stream_speed : meira.down_stream_speed;
	auto &point = boss.topleft_on_page[c.frame.page_back];
	const auto clock = boss.phase_frame;
	if (clock < 50) {
	} else if (clock == 50) {
		callbacks.sound_effect(9); boss.patnum = 142;
		if (returning) meira.burst_group = enemy_bullet_group::one_random_angle;
	} else if (clock < 99) {
	} else if (clock == 99) {
		callbacks.sound_effect(3); boss.patnum = 143; speed = 32;
	} else if (clock < (returning ? 120 : 116)) {
		if ((clock & 1) != 0) {
			const auto result = pellet(c, {word_add(point.x, 24), word_add(point.y, 32)},
				returning ? 0U : 64U, boss.rank_param[returning ? 2U : 1U], speed);
			if (result != MeiraUpdateResult::boss) return result;
			speed = word_add(speed, 11);
		}
		if (returning && point.x != 192) point.x = word_add(point.x, point.x < 192 ? 8 : -8);
	} else { boss.patnum = 141; boss.phase_frame = 0; }
	return MeiraUpdateResult::boss;
}

MeiraUpdateResult triple_spread(BossState &boss, MeiraState &meira,
	MeiraCombatContext &c, MeiraCallbacks &callbacks) noexcept
{
	const auto clock = boss.phase_frame;
	if (clock < 50) {
	} else if (clock == 50) {
		callbacks.sound_effect(9); boss.patnum = 142; meira.burst_group = enemy_bullet_group::one_random_angle;
	} else if (clock < 99) {
	} else if (clock == 99 || clock == 108 || clock == 116) {
		callbacks.sound_effect(3); boss.patnum = 143;
		const auto point = boss.topleft_on_page[c.frame.page_back];
		return pellet(c, {word_add(point.x, 24), word_add(point.y, 32)}, 0, boss.rank_param[2], 74);
	} else if (clock < 116) { boss.patnum = 142;
	} else { boss.patnum = 141; boss.phase_frame = 0; }
	return MeiraUpdateResult::boss;
}

MeiraUpdateResult teleport(BossState &boss, MeiraState &meira, MeiraCombatContext &c,
	MeiraCallbacks &callbacks) noexcept
{
	const auto clock = boss.phase_frame;
	if (clock < 50) return MeiraUpdateResult::boss;
	auto &point = boss.topleft_on_page[c.frame.page_back];
	auto &left = meira.afterimage_left[c.frame.page_back];
	auto &top = meira.afterimage_top[c.frame.page_back];
	if ((clock & 7) == 0) {
		left[0] = left[1]; top[0] = top[1];
		left[1] = left[2]; top[1] = top[2];
		left[2] = point.x; top[2] = point.y;
	}
	if (clock == 50) {
		left.fill(point.x); top.fill(point.y);
		meira.dash_target.x = static_cast<std::int16_t>((randring_next16(c.random) % 320U) + 32U);
		meira.dash_target.y = static_cast<std::int16_t>(randring_next8(c.random) + 16);
		meira.dash_origin = point;
		meira.dash_step = 1;
	} else if (clock <= 114) {
		const auto interpolate = [&](std::int16_t origin, std::int16_t target) {
			const auto difference = word_add(target, word_add(static_cast<std::int16_t>(~origin), 1));
			const auto product = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
				static_cast<std::int32_t>(difference) * meira.dash_step));
			return word_add(static_cast<std::int16_t>(product / 64), origin);
		};
		point.x = interpolate(meira.dash_origin.x, meira.dash_target.x);
		point.y = interpolate(meira.dash_origin.y, meira.dash_target.y);
		meira.dash_step = word_add(meira.dash_step, 1);
		// Source's subsequent clock==114 sound arm is unreachable.
	} else if (clock >= 164) {
		boss.patnum = 146;
		const auto result = balls(c, point, 2);
		if (result != MeiraUpdateResult::boss) return result;
		boss.phase_frame = 0;
	} else return MeiraUpdateResult::boss;
	for (std::size_t i = 0; i < 3; ++i) {
		if (left[i] != point.x && top[i] != point.y)
			callbacks.render_meira({MeiraSpriteKind::afterimage,
				{left[i], scroll_y(top[i], c.scroll_line)}, 147, 0, 14});
	}
	return MeiraUpdateResult::boss;
}

MeiraUpdateResult hittest(BossState &boss, MeiraCombatContext &c, MeiraCallbacks &callbacks) noexcept
{
	const auto point = boss.topleft_on_page[c.frame.page_back];
	const auto player = c.frame.player.current;
	if (point.x <= player.x && word_add(point.x, 32) > player.x &&
		word_add(point.y, -16) < player.y && word_add(point.y, 32) > player.y) c.player.raw_hit = 1;
	PlayerShotSparkAllocatorCallbacks sparks(c.sparks, c.frame.page_back);
	const auto damage = shots_hittest(c.shots, c.score, c.frame.page_back, c.shot_level,
		c.stage_frame, c.bombing, c.bomb_damage_frame_mask, {{word_add(point.x, 8), point.y}, 48, 48}, sparks);
	if (!damage) return MeiraUpdateResult::invalid_shot_hittest;
	if (!sparks.all_allocations_succeeded()) return MeiraUpdateResult::shot_spark_refused;
	const ScreenPoint screen{point.x, scroll_y(point.y, c.scroll_line)};
	if (*damage != 0) {
		boss.damage = word_add(boss.damage, *damage);
		callbacks.sound_effect(4);
		callbacks.render_meira({MeiraSpriteKind::hit_white, screen, boss.patnum});
		if (boss.damage >= 2400) {
			boss.phase = 1;
			c.score.score_delta = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(c.score.score_delta) + 30000U);
			c.player.invincibility_time = 200;
		}
	} else callbacks.render_meira({MeiraSpriteKind::normal, screen, boss.patnum});
	return MeiraUpdateResult::boss;
}

MeiraUpdateResult defeat(BossState &boss, MeiraState &meira, MeiraCombatContext &c,
	MeiraCallbacks &callbacks) noexcept
{
	const auto point = boss.topleft_on_page[c.frame.page_back];
	const ScreenPoint center{word_add(point.x, 24), word_add(point.y, 24)};
	boss_explode_render(center, meira.defeat_frame, boss.explode_angle_offset,
		c.scroll_line, c.slowdown_factor, c.palette_tone, callbacks);
	boss_explode_render(center, word_add(meira.defeat_frame, -24), boss.explode_angle_offset,
		c.scroll_line, c.slowdown_factor, c.palette_tone, callbacks);
	if (!sparks_add(c.sparks, c.frame.page_back, word_add(point.x, 48),
		static_cast<std::uint16_t>(word_add(point.y, 48)), 60, 1, false)) return MeiraUpdateResult::defeat_spark_refused;
	const auto zoom_patnum = word_add(10, static_cast<std::int16_t>(word_add(meira.defeat_frame, -32) / 10));
	meira.defeat_frame = word_add(meira.defeat_frame, 1);
	if (meira.defeat_frame >= 96) { meira.defeat_frame = 0; return MeiraUpdateResult::stage_clear; }
	const ScreenPoint screen{point.x, scroll_y(point.y, c.scroll_line)};
	if (boss.phase_frame < 32) callbacks.render_meira({MeiraSpriteKind::normal, screen, boss.patnum});
	else callbacks.render_meira({MeiraSpriteKind::defeat_zoom, screen, zoom_patnum, 2});
	return MeiraUpdateResult::boss;
}
} // namespace

void meira_combat_setup_after_dialogue(BossState &boss, MeiraState &meira,
	EnemyBulletSpecialState &special, std::int8_t rank) noexcept
{
	boss.damage = 0; boss.phase = 0;
	special.turns_or_drift_frames = 2;
	boss.explode_angle_offset = 0;
	meira.phase = 0; meira.pattern = 0; meira.afterimages_active = 0;
	boss.rank_param[0] = enemy_bullet_group::one_random_angle;
	boss.rank_param[1] = rank != 0 ? enemy_bullet_group::spread_5_wide : enemy_bullet_group::spread_3_wide;
	boss.rank_param[2] = rank != 0 ? enemy_bullet_group::spread_5_medium_aimed : enemy_bullet_group::spread_3_medium_aimed;
}

MeiraUpdateResult meira_update(BossState &boss, MeiraState &meira,
	MeiraCombatContext &c, MeiraCallbacks &callbacks) noexcept
{
	boss.phase_frame = word_add(boss.phase_frame, 1);
	if (c.frame.page_back >= boss.topleft_on_page.size()) return MeiraUpdateResult::invalid_page;
	const auto point = boss.topleft_on_page[c.frame.page_back];
	boss.target = {word_add(point.x, 40), word_add(point.y, 40)};
	MeiraUpdateResult result = MeiraUpdateResult::boss;
	if (boss.phase != 0) {
		result = defeat(boss, meira, c, callbacks);
		if (result != MeiraUpdateResult::boss) return result;
	} else {
		if (meira.phase == 0) {
			switch (meira.pattern) {
			case 0: (void)meira_dash_pattern_update(boss, meira, c.frame, MeiraDashPattern::fall, callbacks); break;
			case 1: (void)meira_dash_pattern_update(boss, meira, c.frame, MeiraDashPattern::rising_slashes, callbacks); break;
			case 2: (void)meira_dash_pattern_update(boss, meira, c.frame, MeiraDashPattern::double_slash, callbacks); break;
			case 3: result = stream(boss, meira, c, callbacks, false); break;
			default: break;
			}
			if (result != MeiraUpdateResult::boss) return result;
			if (boss.phase_frame == 0) {
				if (++meira.pattern > 3) meira.pattern = 0;
				if (boss.damage > 700 && meira.pattern != 1) { ++meira.phase; meira.pattern = 0; }
			}
		} else if (meira.phase == 1) {
			switch (meira.pattern) {
			case 0: result = stream(boss, meira, c, callbacks, true); break;
			case 1: (void)meira_dash_pattern_update(boss, meira, c.frame, MeiraDashPattern::four_slashes, callbacks); break;
			case 2: result = triple_spread(boss, meira, c, callbacks); break;
			default: break;
			}
			if (result != MeiraUpdateResult::boss) return result;
			if (boss.phase_frame == 0) {
				if (++meira.pattern > 2) meira.pattern = 0;
				if (boss.damage > 1500) { ++meira.phase; meira.pattern = 0; }
			}
		} else if (meira.phase == 2) {
			if (meira.pattern == 0) {
				if (boss.phase_frame == 50) { callbacks.sound_effect(9); boss.patnum = 142; c.special.turns_or_drift_frames = 5;
				} else if (boss.phase_frame >= 99) {
					boss.patnum = 146; result = balls(c, point, 3);
					if (result != MeiraUpdateResult::boss) return result;
					boss.phase_frame = 0; meira.afterimages_active = 1;
				}
			} else if (meira.pattern == 1) result = teleport(boss, meira, c, callbacks);
			if (result != MeiraUpdateResult::boss) return result;
			if (boss.phase_frame == 0) meira.pattern = 1;
		}
		result = hittest(boss, c, callbacks);
		if (result != MeiraUpdateResult::boss) return result;
	}
	return bullet_result(meira_slashes_update_and_render(meira, c.bullets, c.random,
		c.frame, c.player, c.playperf, c.rank, c.bombing, c.scroll_line, callbacks));
}
} // namespace th02::core
