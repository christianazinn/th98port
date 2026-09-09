#include "th02/core/midboss3.hpp"
#include <bit>

namespace th02::core {
namespace {
std::int16_t word(int x) noexcept { return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(x)); }
void render(ScreenPoint point, std::int16_t scroll, std::int16_t patnum,
	bool white, Midboss2Callbacks &p) noexcept
{
	auto y = word(point.y + scroll);
	if (y >= 400) y = word(y - 400);
	if (white) p.sound_effect(4);
	p.render_midboss({white ? Midboss2SpriteKind::hit_white : Midboss2SpriteKind::normal,
		{point.x, y}, patnum, 0});
}
Midboss2UpdateResult pattern(BossState &boss, Midboss3State &s, std::size_t i,
	Midboss3Context &c) noexcept
{
	auto &point = s.topleft_on_page[i][c.frame.page_back];
	const int step = i == 0 ? 8 : -8;
	const auto top = word(point.y + 12);
	const auto fire = [&](std::uint8_t angle, std::uint8_t group, std::int16_t speed) {
		const auto result = bullets_add_pellet(c.bullets, c.random, c.frame, c.playperf,
			c.rank, c.bombing, {word(point.x + 28), top}, angle, group, speed);
		return result == EnemyBulletAddResult::completed ? Midboss2UpdateResult::completed :
			result == EnemyBulletAddResult::invalid_page ? Midboss2UpdateResult::bullet_invalid_page :
			Midboss2UpdateResult::bullet_source_iatan2_fault;
	};
	if (boss.phase_frame < 152) {
		if (boss.phase_frame == 128) s.angle = {0, 128};
		if (!(boss.phase_frame & 3)) {
			s.angle[i] = static_cast<std::uint8_t>(s.angle[i] + step);
			return fire(s.angle[i], enemy_bullet_group::one, 40);
		}
	} else if (boss.phase_frame < 192) {
		point.x = word(point.x + step);
		if (!(boss.phase_frame & 7)) {
			s.angle[i] = static_cast<std::uint8_t>(s.angle[i] + step * 2);
			return fire(static_cast<std::uint8_t>(128 - s.angle[i]), c.rank != 0 ?
				enemy_bullet_group::spread_4_medium : enemy_bullet_group::spread_2_medium, 40);
		}
	} else if (boss.phase_frame < 352) {
		point.y = word(point.y + 2);
		if (point.y >= 400) point.y = word(point.y - 400);
	} else if (boss.phase_frame < 376) {
		if (!(boss.phase_frame & 3)) {
			s.angle[i] = static_cast<std::uint8_t>(s.angle[i] - step);
			return fire(s.angle[i], enemy_bullet_group::one, 50);
		}
	} else if (boss.phase_frame < 416) {
		point.x = word(point.x - step);
		if (!(boss.phase_frame & 3)) {
			s.angle[i] = static_cast<std::uint8_t>(s.angle[i] - step);
			return fire(s.angle[i], enemy_bullet_group::one, 50);
		}
	} else if (boss.phase_frame < 576) {
		point.y = word(point.y - 2);
		if (point.y < 0) point.y = word(point.y + 400);
		if (boss.phase_frame % 60 == 0) return fire(0, enemy_bullet_group::one_aimed, 40);
	} else boss.phase_frame = 127;
	return Midboss2UpdateResult::completed;
}
Midboss2UpdateResult defeat(BossState &boss, Midboss3State &s,
	Midboss3SharedState shared, std::size_t i, Midboss3Context &c, Midboss2Callbacks &p) noexcept
{
	const auto point = s.topleft_on_page[i][c.frame.page_back];
	if (!sparks_add(c.sparks, c.frame.page_back, word(point.x + 32),
		static_cast<std::uint16_t>(point.y + 16), 60, 1, false))
		return Midboss2UpdateResult::defeat_spark_allocation_refused;
	const auto patnum = word(10 + s.kill_frame[i] / 6);
	s.kill_frame[i] = word(s.kill_frame[i] + 1);
	if (s.kill_frame[i] >= 48) {
		if (!sparks_add(c.sparks, c.frame.page_back, word(point.x + 32),
			static_cast<std::uint16_t>(point.y + 16), 128, 32, true))
			return Midboss2UpdateResult::defeat_spark_allocation_refused;
		s.kill_frame[i] = 0;
		shared.flag[i] = 2;
		boss.target = {-1, -1};
	} else {
		render(point, c.scroll_line, patnum, false, p);
		render({word(point.x + 32), point.y}, c.scroll_line, patnum, false, p);
	}
	return Midboss2UpdateResult::completed;
}
Midboss2UpdateResult hit(Midboss3State &s, Midboss3SharedState shared,
	std::size_t i, Midboss3Context &c, Midboss2Callbacks &p, bool entering) noexcept
{
	const auto point = s.topleft_on_page[i][c.frame.page_back];
	PlayerShotSparkAllocatorCallbacks allocator(c.sparks, c.frame.page_back);
	const auto damage = shots_hittest(c.shots, c.score, c.frame.page_back,
		c.shot_level, c.stage_frame, c.bombing, c.bomb_damage_frame_mask,
		{{word(point.x + 12), point.y}, 40, 30}, allocator);
	if (!damage) return Midboss2UpdateResult::invalid_shot_hittest;
	if (!allocator.all_allocations_succeeded()) return Midboss2UpdateResult::shot_spark_allocation_refused;
	if (*damage != 0) {
		shared.damage[i] = word(shared.damage[i] + *damage);
		if (entering || shared.damage[i] <= 280) render(point, c.scroll_line, shared.patnum[0], true, p);
		else {
			p.sound_effect(2);
			shared.flag[i] = 1;
			c.score.score_delta = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(c.score.score_delta) + 20000U);
		}
	} else render(point, c.scroll_line, shared.patnum[0], false, p);
	return Midboss2UpdateResult::completed;
}
}

Midboss2InvalidateResult midboss3_invalidate(Midboss3State &s,
	Midboss3SharedState shared, const PlayerFrameState &frame, Midboss2Callbacks &p) noexcept
{
	unsigned removed = 0;
	for (std::size_t i = 0; i < 2; ++i) {
		if (shared.flag[i] == 2) { ++removed; continue; }
		if (frame.page_back >= 2) return Midboss2InvalidateResult::invalid_page;
		p.invalidate_midboss({s.topleft_on_page[i][frame.page_back], 64, 32});
		if (frame.page_front >= 2) return Midboss2InvalidateResult::invalid_page;
		s.topleft_on_page[i][frame.page_back] = s.topleft_on_page[i][frame.page_front];
	}
	return removed == 2 ? Midboss2InvalidateResult::inactive : Midboss2InvalidateResult::active;
}

Midboss2UpdateResult midboss3_update_and_render(BossState &boss, Midboss3State &s,
	Midboss3SharedState shared, Midboss3Context &c, Midboss2Callbacks &p) noexcept
{
	if (c.frame.page_back >= 2) return Midboss2UpdateResult::invalid_page;
	const auto target = s.topleft_on_page[shared.flag[0] == 0 ? 0 : 1][c.frame.page_back];
	boss.target = {word(target.x + 24), word(target.y + 16)};
	boss.phase_frame = word(boss.phase_frame + 1);
	if (boss.phase_frame == 1) {
		s.topleft_on_page[0] = {ScreenPoint{32, -16}, ScreenPoint{32, -16}};
		s.topleft_on_page[1] = {ScreenPoint{352, -16}, ScreenPoint{352, -16}};
		shared.damage[0] = shared.damage[1] = 0;
		s.loops = 0;
		return Midboss2UpdateResult::completed;
	}
	shared.patnum[0] = word((c.scroll_line & 3) + 144);
	if (boss.phase_frame < 128) {
		for (std::size_t i = 0; i < 2; ++i) {
			s.topleft_on_page[i][c.frame.page_back].y = word(s.topleft_on_page[i][c.frame.page_back].y + c.scroll_delta);
			const auto result = hit(s, shared, i, c, p, true);
			if (result != Midboss2UpdateResult::completed) return result;
		}
	} else {
		if (boss.phase_frame == 128) ++s.loops;
		for (std::size_t i = 0; i < 2; ++i) {
			if (shared.flag[i] == 1) {
				const auto result = defeat(boss, s, shared, i, c, p);
				if (result != Midboss2UpdateResult::completed) return result;
			} else if (shared.flag[i] == 0) {
				auto result = pattern(boss, s, i, c);
				if (result != Midboss2UpdateResult::completed) return result;
				result = hit(s, shared, i, c, p, false);
				if (result != Midboss2UpdateResult::completed) return result;
				if (s.loops >= 4) shared.flag[i] = 1;
			}
		}
	}
	return Midboss2UpdateResult::completed;
}
} // namespace th02::core
