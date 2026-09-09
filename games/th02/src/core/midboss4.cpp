#include "th02/core/midboss4.hpp"
#include "th02/core/stage_state.hpp"
#include <bit>

namespace th02::core {
namespace {
std::int16_t word(std::int32_t v) noexcept
{ return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

Midboss2UpdateResult bullet_result(EnemyBulletAddResult r) noexcept
{
	if (r == EnemyBulletAddResult::completed) return Midboss2UpdateResult::completed;
	return r == EnemyBulletAddResult::invalid_page ? Midboss2UpdateResult::bullet_invalid_page :
		Midboss2UpdateResult::bullet_source_iatan2_fault;
}
Midboss2UpdateResult pellet(StageState &s, std::int16_t y, std::uint8_t angle,
	std::uint8_t group, std::int16_t speed) noexcept
{
	return bullet_result(bullets_add_pellet(s.bullets, s.random, s.frame,
		s.player_miss.playperf, s.rank, s.bomb.bombing,
		{word(s.marisa.intro_direction * 16 + s.marisa.topleft.x + 28), y},
		angle, group, speed));
}
Midboss2UpdateResult pattern(StageState &s, Midboss2Callbacks &p,
	LaserSpawnCallbacks &lp) noexcept
{
	const auto f = s.boss.phase_frame;
	switch (s.midboss4.pattern) {
	case 0:
		if (f % 16 == 0) { p.sound_effect(3); return pellet(s, 96, 64, enemy_bullet_group::one, 80); }
		break;
	case 1:
		if (f % 16 == 0) {
			p.sound_effect(3);
			return pellet(s, 96, 64, s.rank == 0 ? enemy_bullet_group::spread_4_wide :
				enemy_bullet_group::spread_5_medium, word(f - 20));
		}
		break;
	case 2:
		if (f % 8 == 0) return bullet_result(bullets_add_16x16(s.bullets, s.random,
			s.frame, s.player_miss.playperf, s.rank, s.bomb.bombing,
			{word(s.marisa.intro_direction * 16 + s.marisa.topleft.x + 28), 96},
			64, enemy_bullet_special_motion::chase, 80, 8));
		break;
	case 3:
		if (f % (16 - s.rank * 2) == 0) {
			s.lasers.wait_frames = 16;
			(void)lasers_add(s.lasers,
				{word(s.marisa.intro_direction * 16 + s.marisa.topleft.x + 28), 80}, 6, 111, lp);
		}
		break;
	case 4:
		if (f % 16 == 0) {
			p.sound_effect(7);
			for (int i = 0; i < 4; ++i) {
				const auto a = static_cast<std::uint8_t>((randring_next8(s.random) & 63U) + 32U);
				const auto r = pellet(s, s.midboss4.pellet_top, a, enemy_bullet_group::one, 70);
				if (r != Midboss2UpdateResult::completed) return r;
			}
		}
		break;
	}
	return Midboss2UpdateResult::completed;
}
} // namespace

Midboss2InvalidateResult midboss4_invalidate(StageState &s, Midboss2Callbacks &p) noexcept
{
	if (s.frame.page_back >= 2) return Midboss2InvalidateResult::invalid_page;
	s.midboss4.position_page = static_cast<std::uint8_t>(s.frame.page_back);
	auto &back = s.boss.topleft_on_page[s.frame.page_back];
	p.invalidate_midboss({back, 64, 64});
	if (s.frame.page_front >= 2) return Midboss2InvalidateResult::invalid_page;
	back = s.boss.topleft_on_page[s.frame.page_front];
	return s.midboss4.active != 0 ? Midboss2InvalidateResult::active : Midboss2InvalidateResult::inactive;
}

Midboss2UpdateResult midboss4_update_and_render(StageState &s,
	Midboss2Callbacks &p, LaserSpawnCallbacks &lp) noexcept
{
	auto &b = s.boss; auto &m = s.marisa; auto &mid = s.midboss4;
	b.target = {word(m.topleft.x + 24), word(m.topleft.y + 24)};
	b.phase_frame = word(b.phase_frame + 1);
	if (b.phase_frame == 1) {
		b.topleft_on_page.fill({static_cast<std::int16_t>(m.intro_direction == 0 ? 64 : 320), -16});
		b.patnum = m.intro_direction == 0 ? 150 : 149;
		mid.position_page = 0;
		b.damage = 0; b.phase = 0; m.hit_flash = false; m.intro_step = 0;
		m.intro_direction = m.intro_direction == 0 ? 1 : -1;
		mid.patterns_seen = 0; mid.pattern = 0; mid.active = 1;
	}
	if (mid.position_page >= 2) return Midboss2UpdateResult::invalid_page;
	auto &pos = b.topleft_on_page[mid.position_page];
	mid.pellet_top = word(pos.y + 48);
	m.topleft.y = pos.y;
	if (b.phase == 0) {
		if (m.intro_step == 0) {
			pos.x = word(pos.x + m.intro_direction * 8); pos.y = word(pos.y + 2);
			m.topleft = pos;
			if ((b.phase_frame & 7) == 0) {
				p.sound_effect(7);
				const auto r = pellet(s, mid.pellet_top, 64, enemy_bullet_group::one, 80);
				if (r != Midboss2UpdateResult::completed) return r;
			}
			if (b.phase_frame >= 32) {
				m.intro_step = 1; b.phase_frame = 2; b.patnum = 148;
				m.intro_direction = word(-m.intro_direction);
				mid.pattern = static_cast<std::uint8_t>(randring_next8(s.random) % 5U);
			}
		} else if (m.intro_step == 1 && b.phase_frame > 50) {
			b.patnum = m.intro_direction == -1 ? 149 : 150;
			pos.x = word(pos.x + m.intro_direction * 4); m.topleft.x = pos.x;
			const auto r = pattern(s, p, lp);
			if (r != Midboss2UpdateResult::completed) return r;
			if (mid.patterns_seen < 12) {
				if (b.phase_frame >= 114) {
					b.phase_frame = 2; b.patnum = 148; m.intro_direction = word(-m.intro_direction);
					mid.pattern = static_cast<std::uint8_t>(randring_next8(s.random) % 5U);
					++mid.patterns_seen;
				}
			} else if (m.topleft.x <= 0 || m.topleft.x >= 384) {
				b.phase_frame = 0; m.intro_direction = -1; s.midboss_scroll_step = 1632;
				mid.active = 0; return Midboss2UpdateResult::completed;
			}
		}
	}
	if (b.phase != 0) {
		if (b.phase_frame % 24 == 0 && !sparks_add(s.sparks, s.frame.page_back,
			word(m.topleft.x + 32), static_cast<std::uint16_t>(word(m.topleft.y + 32)), 128, 24, true))
			return Midboss2UpdateResult::defeat_spark_allocation_refused;
		const auto zoom = word(10 + (mid.defeat_frame >> 3));
		mid.defeat_frame = word(mid.defeat_frame + 1);
		if (mid.defeat_frame >= 64) {
			mid.defeat_frame = 0; b.phase_frame = 0; b.phase = 2; b.target = {-1, -1};
			mid.active = 0; m.intro_direction = -1; s.midboss_scroll_step = 1632;
		} else {
			auto y = word(m.topleft.y + s.scroll.line); if (y >= 400) y = word(y - 400);
			p.render_midboss({Midboss2SpriteKind::defeat_zoom, {m.topleft.x, y}, zoom, 2});
		}
		return Midboss2UpdateResult::completed;
	}
	PlayerShotSparkAllocatorCallbacks sparks(s.sparks, s.frame.page_back);
	const auto damage = shots_hittest(s.player_shots, s.score, s.frame.page_back,
		s.player_stream.shot_level, s.stage_frame, s.bomb.bombing, s.bomb_damage_frame_mask,
		{{word(m.topleft.x + 8), m.topleft.y}, 48, 48}, sparks);
	if (!damage) return Midboss2UpdateResult::invalid_shot_hittest;
	if (!sparks.all_allocations_succeeded()) return Midboss2UpdateResult::shot_spark_allocation_refused;
	if (*damage != 0) {
		m.hit_flash = true; b.damage = word(b.damage + *damage);
		auto y = word(m.topleft.y + s.scroll.line); if (y >= 400) y = word(y - 400);
		if (b.damage >= 400 && y < 336 && y > 96) {
			p.sound_effect(2); b.phase = 1;
			s.score.score_delta = std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(s.score.score_delta) + 50000U);
			items_add(s.items, s.score, word(m.topleft.x + 24), m.topleft.y,
				static_cast<std::int16_t>(s.midboss_scroll_step == 1632 ? ItemType::one_up : ItemType::bomb));
		}
	}
	auto y = word(m.topleft.y + s.scroll.line);
	if (y < 0) y = word(y + 400); else if (y >= 400) y = word(y - 400);
	if (m.hit_flash) {
		p.sound_effect(4); p.render_midboss({Midboss2SpriteKind::hit_white, {m.topleft.x, y}, b.patnum, 0});
		m.hit_flash = false;
	} else p.render_midboss({Midboss2SpriteKind::normal, {m.topleft.x, y}, b.patnum, 0});
	return Midboss2UpdateResult::completed;
}
} // namespace th02::core
