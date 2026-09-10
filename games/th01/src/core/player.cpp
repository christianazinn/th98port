#include "th01/core/player.hpp"
#include "th01/core/state.hpp"
#include "th01/core/motion.hpp"
#include "th01/core/projectiles.hpp"
#include "pc98/random.hpp"

#include <bit>
#include <cstdlib>

namespace th01::core {
namespace {
std::int8_t increment(std::int8_t v) noexcept
{
	return std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(static_cast<int>(v) + 1));
}

void shot(State &s, int offset = 0) noexcept
{
	(void)add_shot(s.shots, static_cast<std::int16_t>(s.player.left + 8 + offset), 368);
}

void choose_special(State &s, int direction) noexcept
{
	const int same = direction == 0 ? Direction::right : Direction::left;
	const int other = direction == 0 ? Direction::left : Direction::right;
	s.player.submode = static_cast<std::int8_t>((s.input.direction == other ? 2 :
		(s.input.direction == same ? 4 : 0)) | direction);
}

void special_start(State &s, int direction, bool shooting) noexcept
{
	if(shooting) s.player.submode = static_cast<std::int8_t>(6 | direction);
	else choose_special(s, direction);
	s.player.mode = 3;
	if(shooting || s.player.submode < 4) s.player.deflecting = true;
	s.player.combo_enabled = false;
}

bool bomb_update(State &s) noexcept
{
	// bomb.cpp receives a 16-bit frame even though its source counter is u32.
	const auto frame = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(s.bomb_frame));
	s.player.invincible = true;
	if(frame == 0 || (frame < 50 && frame % 10 == 0)) pellets_decay(s);
	else if(frame == 50) s.bomb_damaging = true;
	else if(frame == 140) {
		s.player.invincibility_time = 100;
		s.bomb_damaging = false;
		return true;
	}
	return false;
}
} // namespace

void player_reset(State &s) noexcept
{
	auto &p = s.player;
	p.dash_cycle = p.mode_frame = p.mode = 0;
	p.submode = -1;
	p.dash_direction = 1;
	p.bomb_flag = 0;
	s.bomb_damaging = false;
	p.sliding = p.deflecting = false;
	p.swing_deflection_frames = s.rank == 0 ? 15 : s.rank == 2 ? 10 : s.rank == 3 ? 8 : 12;
	p.bombing = p.combo_enabled = s.input.bomb = p.invincible_against_orb = false;
}

void orb_player_hittest(State &s, std::int16_t friction) noexcept
{
	auto &p = s.player;
	auto &o = s.orb;
	const auto dx = static_cast<int>(o.horizontal.left) - p.left;
	const auto dy = static_cast<int>(o.top) - 368;
	if(friction == 0) {
		if(!p.invincible && !p.invincible_against_orb && std::abs(dx) < 24 && std::abs(dy) < 16)
			s.input.player_is_hit = true;
		return;
	}
	if(std::abs(dx) >= 40 || std::abs(dy) >= 32) return;
	if(friction < 100) {
		if(dx < 0) o.horizontal.velocity = OrbVelocityX::left4;
		else if(dx > 0) o.horizontal.velocity = OrbVelocityX::right4;
		else {
			o.horizontal.velocity = OrbVelocityX::stationary;
			if(pc98::irand(s.random_seed) % 8 == 0) o.horizontal.velocity = OrbVelocityX::left4;
			if(pc98::irand(s.random_seed) % 8 == 4) o.horizontal.velocity = OrbVelocityX::left4;
		}
		(void)apply_orb_force(o.vertical, o.horizontal.velocity, s.random_seed,
			static_cast<double>(friction / 2 - 13), OrbForce::immediate);
	} else {
		switch(friction) {
		case 101: o.horizontal.velocity = OrbVelocityX::left4; break;
		case 102: o.horizontal.velocity = OrbVelocityX::right4; break;
		case 103: o.horizontal.velocity = OrbVelocityX::right8; break;
		case 104: o.horizontal.velocity = OrbVelocityX::left8; break;
		default: break;
		}
		(void)apply_orb_force(o.vertical, o.horizontal.velocity, s.random_seed, -10.0, OrbForce::immediate);
	}
}

void player_update(State &s) noexcept
{
	auto &p = s.player;
	auto &input = s.input;
	p.dash_cycle = static_cast<std::int8_t>((static_cast<int>(p.dash_cycle) + 1) & 7);
	if((s.rem_bombs != 0 || p.bombing) && p.bomb_flag >= 2) {
		if(p.bomb_flag == 2) {
			s.bomb_frame = 0;
			p.bomb_flag = 3;
			p.deflecting = p.bombing = true;
			s.rem_bombs = static_cast<std::int8_t>(s.rem_bombs - 1);
		}
		orb_player_hittest(s, 1);
		const bool done = bomb_update(s);
		p.bomb_flag = done ? 0 : 3;
		if(done) p.bombing = input.bomb = false;
		input.shot = input.strike = false;
		p.mode = 0;
		if(p.bomb_flag == 0) p.deflecting = false;
	} else if(input.bomb && p.mode != 3 && s.rem_bombs != 0) {
		p.bomb_flag = 2;
		input.bomb = false;
		pellet_speed_raise(s, 1);
	} else if(p.mode == 0) {
		switch(input.direction) {
		case Direction::right:
			p.dash_direction = 0; p.left = move_player(p.left, 4); p.submode = 1; break;
		case Direction::left:
			p.dash_direction = 1; p.left = move_player(p.left, -4); p.submode = 2; break;
		default: p.submode = 0; break;
		}
		if(input.shot) {
			p.mode = 1; p.mode_frame = 0;
			p.submode = static_cast<std::int8_t>(input.direction == Direction::both ? 0 : input.direction);
		}
		if(input.strike) {
			p.mode_frame = 0; p.mode = 2;
			if(input.direction == Direction::none || input.direction == Direction::both) {
				p.submode = 0; p.deflecting = true;
			} else {
				p.submode = static_cast<std::int8_t>(input.direction % 3);
				p.sliding = true; p.combo_enabled = false;
			}
			p.invincible_against_orb = true;
		}
	} else if(p.mode == 1) {
		if(p.submode == 0) {
			if(p.mode_frame == 0) shot(s);
			else if(p.mode_frame >= 2) {
				s.bomb_frame = 0; p.mode = 0; input.shot = false; p.submode = 3;
			}
		} else if(p.submode == 1 || p.submode == 2) {
			p.dash_direction = static_cast<std::int8_t>(p.submode == 1 ? 0 : 1);
			p.left = move_player(p.left, static_cast<std::int16_t>(p.submode == 1 ? 4 : -4));
			if(p.mode_frame >= 1) {
				shot(s); s.bomb_frame = 0; input.shot = false; p.mode = 0;
			}
		}
		p.mode_frame = increment(p.mode_frame);
	} else if(p.mode == 2) {
		if(p.submode == 0) {
			if(p.mode_frame < 1) {
				p.submode = static_cast<std::int8_t>(input.direction % 3);
				if(p.submode != 0) {
					p.mode_frame = 0; p.deflecting = false; p.sliding = true;
				}
			} else {
				if(p.mode_frame == p.swing_deflection_frames) p.deflecting = false;
				if(p.mode_frame >= 23) {
					p.mode = 0; p.submode = 3; p.mode_frame = 0;
					input.strike = false; s.bomb_frame = 0; p.invincible_against_orb = false;
				}
			}
		} else if(p.submode == 1 || p.submode == 2) {
			const int direction = p.submode == 1 ? 0 : 1;
			p.left = move_player(p.left, static_cast<std::int16_t>(direction == 0 ? 6 : -6));
			if(p.mode_frame >= 5 && !input.strike && !input.shot) p.combo_enabled = true;
			if(p.mode_frame >= 12) {
				p.mode = 0; p.submode = 3; p.mode_frame = 0; p.sliding = false;
				if(p.combo_enabled && (input.strike || input.shot)) {
					special_start(s, direction, !input.strike);
					return;
				}
				input.strike = false; s.bomb_frame = 0;
				if(direction == 0) p.combo_enabled = false;
				p.invincible_against_orb = false;
			}
		}
		p.mode_frame = increment(p.mode_frame);
		if(p.submode == 0 && p.mode_frame < 21) orb_player_hittest(s, p.mode_frame);
		else if(p.submode == 1) orb_player_hittest(s, 102);
		else if(p.submode == 2) orb_player_hittest(s, 101);
	} else if(p.mode >= 3 && p.mode <= 5) {
		const int kind = p.submode & ~1;
		const int direction = p.submode & 1;
		if(kind == 0 || kind == 4)
			p.left = move_player(p.left, static_cast<std::int16_t>((direction == 0 ? 1 : -1) * (kind == 0 ? 2 : 6)));
		int duration = 28;
		if(kind == 0 || kind == 2) {
			if(p.mode_frame == 20) p.deflecting = false;
			if(p.mode_frame >= 20 && !input.strike && !input.shot) p.combo_enabled = true;
		} else if(kind == 4) duration = 13;
		else if(kind == 6) {
			switch(p.mode_frame) {
			case 1: shot(s, -8); break;
			case 4: shot(s, 8); break;
			case 7: case 13: case 19: shot(s); break;
			case 10: shot(s, -16); break;
			case 16: shot(s, 16); break;
			default: break;
			}
			if(p.mode_frame == 4) p.deflecting = false;
			duration = 19;
		}
		p.mode_frame = increment(p.mode_frame);
		if(p.mode_frame > duration) {
			s.bomb_frame = 0; p.mode_frame = 0; p.sliding = p.deflecting = false;
			if(p.mode < 5 && p.combo_enabled && (input.strike || input.shot)) {
				if(kind == 0 || kind == 2) {
					if(input.strike) choose_special(s, direction);
					else p.submode = static_cast<std::int8_t>(6 | direction);
				}
				p.mode = increment(p.mode);
				if(!input.strike || p.submode < 4) p.deflecting = true;
				p.combo_enabled = false;
				return;
			}
			p.invincible_against_orb = false; p.mode = 0;
		} else if(kind == 0 || kind == 2) orb_player_hittest(s, static_cast<std::int16_t>(p.mode_frame / 2));
		else if(kind == 4) orb_player_hittest(s, static_cast<std::int16_t>(direction == 0 ? 103 : 104));
		else if(kind == 6 && p.mode_frame < 16) orb_player_hittest(s, p.mode_frame);
	}
}

void player_invincibility_update(State &s) noexcept
{
	if(s.player.invincibility_time > 1) {
		--s.player.invincibility_time;
		s.player.invincible = true;
	} else if(s.player.invincibility_time == 1) {
		s.player.invincible = false;
		s.player.invincibility_time = 0;
	}
}

void player_invincibility_particles_update(State &s) noexcept
{
	auto &frames = s.player.invincibility_sprite_frames;
	if(!s.player.invincible) { frames[0] = 255; return; }
	if(frames[0] == 255) frames.fill(0);
	for(auto &frame : frames) {
		if(frame == 9 || frame == 0) {
			(void)pc98::irand(s.random_seed);
			(void)pc98::irand(s.random_seed);
			frame = static_cast<std::int16_t>(pc98::irand(s.random_seed) % 7 + 1);
		}
		++frame;
	}
}
} // namespace th01::core
