#include "th05/core/state.hpp"
#include "pc98/masterlib_vector.hpp"

namespace th05::core {
namespace {
bool emit(State &s, Stage2Particle &particle, int angle_base, int origin, bool respawn) noexcept
{
	const auto random = pc98::th0405::randring_next16_mod(s.random, 32);
	if (!random) return false;
	particle.angle = static_cast<std::uint8_t>(*random + angle_base);
	particle.pos.cur.x = static_cast<std::int16_t>((angle_base + 32 - particle.angle) * 128 + origin);
	if (respawn) {
		particle.pos.cur.x = static_cast<std::int16_t>(particle.pos.cur.x + (pc98::irand(s.random.seed) % 8) * 16);
		particle.zoom = 8;
	}
	particle.pos.cur.y = 0;
	const auto velocity = pc98::masterlib::vector2(particle.angle, 128);
	particle.pos.velocity = {velocity.x, velocity.y};
	return true;
}
bool respawn(State &s, Stage2Particle &particle) noexcept
{
	int cycle = s.stage_frame % 4096;
	if (cycle < 1000) return emit(s, particle, 48, 1024, true);
	if (cycle < 1128) { cycle -= 1000; return emit(s, particle, 48 + cycle / 8, 1024 + cycle * 8, true); }
	if (cycle < 2000) return emit(s, particle, 64, 2048, true);
	if (cycle < 2128) { cycle -= 2000; return emit(s, particle, 64 - cycle / 8, 2048 - cycle * 8, true); }
	if (cycle < 3000) return emit(s, particle, 48, 1024, true);
	if (cycle < 3128) { cycle -= 3000; return emit(s, particle, 48 - cycle / 8, 1024 - cycle * 8, true); }
	if (cycle < 3968) return emit(s, particle, 32, 0, true);
	cycle -= 3968;
	return emit(s, particle, 32 + cycle / 8, cycle * 8, true);
}
}
bool stage2_scenery_update(State &s, Presentation &p) noexcept
{
	if ((s.boss.phase > 1 && s.boss.phase < 254) ||
		(s.boss.phase == 1 && s.boss.phase_frame >= 32)) return true;
	auto &scene = s.stage2_scenery;
	if (s.stage_frame < 304) {
		s.scroll_active = false;
		if (s.stage_frame >= 256) {
			const auto level = static_cast<std::uint8_t>(303 - s.stage_frame);
			s.palette[0] = {level, level, static_cast<std::uint8_t>(level * 5)};
			s.palette_tone = static_cast<std::uint16_t>(scene.flash_tone + 100);
			s.palette_changed = true;
			scene.flash_tone = static_cast<std::uint8_t>(scene.flash_tone + 2);
			if (s.stage_frame % 4 == 0) {
				const auto cel = static_cast<std::size_t>((s.stage_frame - 256) / 4);
				for (std::size_t row = 0; row < 25; ++row) {
					s.tile_ring[row * 32 + 11 - cel] = 20552; // 72 + 256 * 80
					s.tile_ring[row * 32 + 12 + cel] = 20552;
				}
			}
		}
		return true;
	}
	if (s.stage_frame < 306) {
		p.graph_scroll(s.scroll_line);
		for (std::size_t row = 0; row < 25; ++row)
			for (std::size_t x = 0; x < 24; ++x) s.tile_ring[row * 32 + x] = 72;
		s.scroll_active = true;
		scene.spawned = 0;
		scene.bg_pulse = 0;
		scene.pulse_direction = 0;
		return true;
	}
	if (scene.flash_tone != 0) {
		scene.flash_tone = scene.flash_tone > 4 ? static_cast<std::uint8_t>(scene.flash_tone - 4) : 0;
		s.palette_tone = static_cast<std::uint16_t>(scene.flash_tone + 100);
	} else {
		if (scene.spawned < 64 && s.stage_frame % 2 == 0) {
			if (scene.spawned < 0) return false;
			auto &particle = scene.particles[static_cast<std::size_t>(scene.spawned)];
			particle.flag = 1;
			if (!emit(s, particle, 48, 1024, false)) return false;
			++scene.spawned;
		}
		for (std::size_t i = 0; i < scene.particles.size(); ++i) {
			auto &particle = scene.particles[i];
			if (!particle.flag) continue;
			const auto pos = pc98::th0405::update_motion(particle.pos);
			if (pos.x <= -128 || pos.x >= 6272 || pos.y <= -128 || pos.y >= 6016) {
				if (!respawn(s, particle)) return false;
			} else if (i & 3) ++particle.zoom;
		}
		if (s.stage_frame % 4 != 0) return true;
		if (scene.pulse_direction == 0) {
			++scene.bg_pulse;
			if (scene.bg_pulse >= 63) ++scene.pulse_direction;
		} else {
			--scene.bg_pulse;
			if (scene.bg_pulse <= 32) --scene.pulse_direction;
		}
		s.palette[0] = {static_cast<std::uint8_t>(scene.bg_pulse * 2),
			static_cast<std::uint8_t>(scene.bg_pulse * 2), static_cast<std::uint8_t>(scene.bg_pulse * 4)};
	}
	s.palette_changed = true;
	return true;
}
}
