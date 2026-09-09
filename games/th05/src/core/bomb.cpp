#include "th05/core/bomb.hpp"

#include "th05/core/state.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_sparks.hpp"

#include <bit>
#include <cstdint>
#include <span>

namespace th05::core {
namespace {

constexpr std::int16_t word(std::int32_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

constexpr std::int16_t word_add(std::int16_t left, std::int16_t right) noexcept
{
	return word(static_cast<std::int32_t>(left) + right);
}

constexpr std::int16_t word_sub(std::int16_t left, std::int16_t right) noexcept
{
	return word(static_cast<std::int32_t>(left) - right);
}

constexpr std::int16_t word_negate(std::int16_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		0U - static_cast<std::uint16_t>(value)
	));
}

constexpr std::int16_t to_subpixels(std::int16_t pixels) noexcept
{
	return word(static_cast<std::int32_t>(pixels) * 16);
}

constexpr std::int16_t to_pixel_slow(std::int16_t subpixels) noexcept
{
	// The source uses CWD/IDIV 16 rather than SAR 4.
	return static_cast<std::int16_t>(subpixels / 16);
}

void tone_deferred(State &state, std::int32_t tone) noexcept
{
	state.palette_tone = std::bit_cast<std::uint16_t>(word(tone));
	state.palette_changed = true;
}

void emit_geometry(
	Presentation &presentation,
	BombGeometryKind kind,
	pc98::th0405::SubpixelPoint subpixel,
	BombPixelPoint pixel,
	std::int16_t extent,
	std::uint8_t color
) noexcept
{
	presentation.bomb_geometry({kind, subpixel, pixel, extent, color});
}

[[nodiscard]] constexpr bool stage_scrolls(const State &state) noexcept
{
	return state.stage_id != 5U;
}

void bomb_dream_decay(State &state, Presentation &presentation) noexcept
{
	const auto frame = static_cast<int>(state.bomb_frame);
	if((frame % 2) != 0) return;
	if((state.boss_phase != 0U) && ((frame % 4) == 0)) return;
	if(state.items.dream > 1U) --state.items.dream;
	presentation.hud(HudField::dream);
}

[[nodiscard]] bool reimu_stars_update_and_render(
	State &state,
	Presentation &presentation
) noexcept
{
	auto &storage = state.bomb.animation;
	if(state.bomb_frame == 32U) {
		std::size_t head_index = 0U;
		std::size_t trail_index = 7U;
		for(std::size_t trail = 0; trail < bomb_reimu_star_trails; ++trail) {
			auto head = bomb_reimu_star_load(storage, head_index);
			const auto x = pc98::th0405::randring_next16_mod(state.random, 5120U);
			if(!x) return false;
			head.topleft.x = word(512 + *x);
			const auto y = pc98::th0405::randring_next16_mod(state.random, 5888U);
			if(!y) return false;
			head.topleft.y = word(*y);
			bomb_reimu_star_store(storage, head_index, head);

			// Keep the source's intentionally non-row-local trail pointer:
			// 7..1, 11..5, 15..9, ... rather than six independent rows.
			for(std::size_t copies = 0U; copies < 7U; ++copies) {
				auto copied = bomb_reimu_star_load(storage, trail_index - copies);
				copied.topleft = head.topleft;
				bomb_reimu_star_store(storage, trail_index - copies, copied);
			}

			const auto speed_bits = pc98::th0405::randring_next16_and(
				state.random, 127U
			);
			if(!speed_bits) return false;
			const auto angle_offset = pc98::th0405::randring_next16_mod(
				state.random, 48U
			);
			if(!angle_offset) return false;
			const auto velocity = pc98::masterlib::vector2(
				static_cast<std::uint8_t>(0x40U + *angle_offset - 24U),
				word(160 + *speed_bits)
			);
			head.velocity = {velocity.x, velocity.y};
			bomb_reimu_star_store(storage, head_index, head);
			head_index += bomb_reimu_star_nodes;
			trail_index += 4U;
		}
	}

	for(std::size_t trail = 0; trail < bomb_reimu_star_trails; ++trail) {
		const auto base = trail * bomb_reimu_star_nodes;
		for(std::size_t node = bomb_reimu_star_nodes; node-- > 1U;) {
			auto destination = bomb_reimu_star_load(storage, base + node);
			const auto source = bomb_reimu_star_load(storage, base + node - 1U);
			destination.topleft = source.topleft;
			bomb_reimu_star_store(storage, base + node, destination);
		}

		auto head = bomb_reimu_star_load(storage, base);
		head.topleft.x = word_add(head.topleft.x, head.velocity.x);
		head.topleft.y = word_add(head.topleft.y, head.velocity.y);
		if((head.topleft.x <= 512) || (head.topleft.x >= 5632)) {
			head.velocity.x = word_negate(head.velocity.x);
		}
		if(head.topleft.y >= 6400) {
			head.topleft.y = word_sub(head.topleft.y, 6400);
		}
		bomb_reimu_star_store(storage, base, head);

		for(const auto node : {6U, 4U, 2U}) {
			const auto star = bomb_reimu_star_load(storage, base + node);
			emit_geometry(
				presentation, BombGeometryKind::reimu_star, star.topleft, {}, 0, 6U
			);
		}
		emit_geometry(
			presentation, BombGeometryKind::reimu_star, head.topleft, {}, 0, 7U
		);

		const auto next_index = 14U + (trail * 8U);
		if(
			(next_index < (bomb_reimu_star_trails * bomb_reimu_star_nodes)) &&
			(state.bomb_frame <= 112U) && ((state.stage_frame % 8U) == trail)
		) {
			const auto next_head = bomb_reimu_star_load(storage, next_index);
			emit_geometry(
				presentation,
				BombGeometryKind::growing_circle,
				{next_head.topleft.x, word_add(next_head.topleft.y, 256)},
				{},
				0,
				state.circles_color
			);
		}
	}
	return true;
}

[[nodiscard]] bool marisa_lasers_update_and_render(
	State &state,
	Presentation &presentation
) noexcept
{
	auto &storage = state.bomb.animation;
	const auto frame = static_cast<int>(state.bomb_frame);
	if(state.bomb_frame == 16U) {
		tone_deferred(state, 100);
		for(std::size_t index = 0; index < bomb_marisa_lasers; ++index) {
			auto laser = bomb_animation_record_load<BombMarisaLaser>(storage, index);
			laser.center.x = -999;
			bomb_animation_record_store(storage, index, laser);
		}
	}

	if((frame % 4) == 0) {
		const auto index = static_cast<std::size_t>(((frame - 16) / 4) & 7);
		auto laser = bomb_animation_record_load<BombMarisaLaser>(storage, index);
		// The source performs this otherwise dead pair copy before spark setup.
		laser.center = state.player_pos.cur;
		bomb_animation_record_store(storage, index, laser);
		auto pool = pc98::th0405::SparkPool{
			std::span<pc98::th0405::Spark>{state.sparks}, state.spark_ring_offset
		};
		if(!pc98::th0405::sparks_add_circle(
			pool, state.player_pos.cur, to_subpixels(12), 24U
		)) {
			return false;
		}
		laser = bomb_animation_record_load<BombMarisaLaser>(storage, index);
		laser.center = state.player_pos.cur;
		laser.radius = 24;
		bomb_animation_record_store(storage, index, laser);
		presentation.sound(15U);
		state.palette_tone = 170U;
	} else {
		state.palette_tone = 100U;
	}
	presentation.palette_show(state.palette_tone);

	for(std::size_t index = 0; index < bomb_marisa_lasers; ++index) {
		auto laser = bomb_animation_record_load<BombMarisaLaser>(storage, index);
		if(laser.center.x == -999) continue;
		const BombPixelPoint position{
			word(to_pixel_slow(laser.center.x) + 32),
			word(to_pixel_slow(laser.center.y) + 16),
		};
		emit_geometry(
			presentation, BombGeometryKind::marisa_laser, laser.center, position,
			laser.radius, 8U
		);
		laser.radius = word_sub(laser.radius, 1);
		if(laser.radius < 4) laser.center.x = -999;
		bomb_animation_record_store(storage, index, laser);
	}
	return true;
}

[[nodiscard]] bool mima_circles_update_and_render(
	State &state,
	Presentation &presentation
) noexcept
{
	auto &storage = state.bomb.animation;
	const auto frame = static_cast<int>(state.bomb_frame);
	if(state.bomb_frame == 1U) {
		tone_deferred(state, 100);
		for(std::size_t index = 0; index < bomb_mima_circles; ++index) {
			auto circle = bomb_animation_record_load<BombMimaCircle>(storage, index);
			circle.center.x = -999;
			bomb_animation_record_store(storage, index, circle);
		}
	}

	if((frame % 4) == 0) {
		const auto index = static_cast<std::size_t>(((frame - 16) / 4) & 7);
		auto circle = bomb_animation_record_load<BombMimaCircle>(storage, index);
		circle.center = {state.player_pos.cur.x, state.player_pos.cur.y};
		circle.distance = 256;
		bomb_animation_record_store(storage, index, circle);
		const auto angle = pc98::th0405::randring_next16(state.random);
		if(!angle) return false;
		circle = bomb_animation_record_load<BombMimaCircle>(storage, index);
		circle.angle = static_cast<std::uint8_t>(*angle);
		bomb_animation_record_store(storage, index, circle);
	}

	for(std::size_t index = 0; index < bomb_mima_circles; ++index) {
		auto circle = bomb_animation_record_load<BombMimaCircle>(storage, index);
		if(circle.center.x == -999) continue;
		const auto offset = pc98::masterlib::vector2(circle.angle, circle.distance);
		circle.center = {word(224 + offset.x), word(117 + offset.y)};
		emit_geometry(
			presentation,
			BombGeometryKind::mima_circle,
			{},
			circle.center,
			static_cast<std::int16_t>(circle.distance / 2),
			15U
		);
		circle.distance = word_sub(circle.distance, 16);
		if(circle.distance < 4) circle.center.x = -999;
		bomb_animation_record_store(storage, index, circle);
	}
	return true;
}

void yuuka_hearts_update_and_render(State &state, Presentation &presentation) noexcept
{
	auto heart = bomb_animation_record_load<BombYuukaHeart>(state.bomb.animation, 0U);
	if(state.bomb_frame == 32U) heart.distance = 16;
	for(std::size_t index = 0; index < 16U; ++index) {
		const auto offset = pc98::masterlib::vector2(heart.angle, heart.distance);
		heart.topleft = {word(192 + offset.x), word(160 + offset.y)};
		if(
			(heart.topleft.x >= 0) && (heart.topleft.x <= 384) &&
			(heart.topleft.y >= 0) && (heart.topleft.y <= 336)
		) {
			emit_geometry(
				presentation, BombGeometryKind::yuuka_heart, {}, heart.topleft, 0, 15U
			);
		}
		heart.angle = static_cast<std::uint8_t>(heart.angle + 16U);
	}
	heart.angle = static_cast<std::uint8_t>(heart.angle + 2U);
	heart.distance = word_add(heart.distance, 2);
	bomb_animation_record_store(state.bomb.animation, 0U, heart);
}

[[nodiscard]] bool reimu_bomb_update_and_render(
	State &state,
	Presentation &presentation
) noexcept
{
	presentation.bomb_background_put(0U);
	if(!reimu_stars_update_and_render(state, presentation)) return false;
	state.circles_color = 9U;
	if(state.bomb_frame <= 64U) {
		tone_deferred(state, 196 - ((static_cast<int>(state.bomb_frame) - 32) * 3));
	} else if((state.bomb_frame <= 112U) && ((state.stage_frame % 4U) == 0U)) {
		presentation.sound(9U);
	}
	return true;
}

[[nodiscard]] bool marisa_bomb_update_and_render(
	State &state,
	Presentation &presentation
) noexcept
{
	presentation.bomb_background_put(1U);
	if(state.bomb_frame < 16U) {
		tone_deferred(state, 196 - (static_cast<int>(state.bomb_frame) * 6));
		return true;
	}
	if(!marisa_lasers_update_and_render(state, presentation)) return false;
	state.shot_spawn.shot_time = 0xFFU;
	return true;
}

[[nodiscard]] bool mima_bomb_update_and_render(
	State &state,
	Presentation &presentation
) noexcept
{
	presentation.bomb_background_put(2U);
	for(const auto entry : {
		BombGeometry{BombGeometryKind::mima_beam, {}, {209, 16}, 48, 7U},
		BombGeometry{BombGeometryKind::mima_beam, {}, {209, 336}, 48, 7U},
		BombGeometry{BombGeometryKind::mima_beam, {}, {238, 16}, 48, 7U},
		BombGeometry{BombGeometryKind::mima_beam, {}, {238, 336}, 48, 7U},
		BombGeometry{BombGeometryKind::mima_beam, {}, {208, 16}, 48, 6U},
		BombGeometry{BombGeometryKind::mima_beam, {}, {208, 336}, 48, 6U},
		BombGeometry{BombGeometryKind::mima_beam, {}, {239, 16}, 48, 6U},
		BombGeometry{BombGeometryKind::mima_beam, {}, {239, 336}, 48, 6U},
	}) {
		presentation.bomb_geometry(entry);
	}
	if(!mima_circles_update_and_render(state, presentation)) return false;
	const auto ring_radius = word(224 - static_cast<int>(state.bomb_frame));
	if(ring_radius > 0) {
		emit_geometry(
			presentation, BombGeometryKind::mima_ring, {}, {224, 117}, ring_radius, 15U
		);
	}
	tone_deferred(state, static_cast<int>(state.bomb_frame) + 100);
	return true;
}

void yuuka_bomb_update_and_render(State &state, Presentation &presentation) noexcept
{
	presentation.bomb_background_put(3U);
	yuuka_hearts_update_and_render(state, presentation);
	state.circles_color = 9U;
	const auto frame = static_cast<int>(state.bomb_frame);
	if(state.bomb_frame <= 64U) {
		tone_deferred(state, 196 - ((frame - 32) * 3));
	} else if((frame % 4) == 0) {
		presentation.sound(9U);
		state.palette_tone = 150U;
		presentation.palette_show(state.palette_tone);
	} else if((frame % 4) == 2) {
		state.palette_tone = 100U;
		presentation.palette_show(state.palette_tone);
	}
}

[[nodiscard]] bool bomb_reimu(State &state, Presentation &presentation) noexcept
{
	const auto frame = state.bomb_frame;
	if(frame < 32U) {
		state.tiles_bb_color = 15U;
		presentation.bomb_cel(static_cast<std::uint8_t>(frame / 2U));
	} else if(frame == 32U) {
		state.scroll_active = false;
		presentation.graph_scroll(0);
		state.bomb_background = false;
		if(!reimu_bomb_update_and_render(state, presentation)) return false;
		bomb_dream_decay(state, presentation);
	} else if(frame < 128U) {
		if(!reimu_bomb_update_and_render(state, presentation)) return false;
		bomb_dream_decay(state, presentation);
	} else if(frame == 128U) {
		presentation.sound(15U);
		if(stage_scrolls(state)) state.scroll_active = true;
		state.items.items_pull_to_player = false;
		state.bomb_background = true;
		tone_deferred(state, 192);
	} else if(frame < 160U) {
		state.bomb_background = true;
		tone_deferred(state, 192 - ((static_cast<int>(frame) - 128) * 3));
		if((frame == 130U) && stage_scrolls(state)) {
			presentation.graph_scroll(state.scroll_line);
		}
	} else {
		state.bombing = false;
		tone_deferred(state, 100);
	}
	state.bomb_frame = static_cast<std::uint8_t>(state.bomb_frame + 1U);
	return true;
}

[[nodiscard]] bool bomb_marisa(State &state, Presentation &presentation) noexcept
{
	const auto frame = state.bomb_frame;
	if(frame < 80U) {
		if(frame == 2U) {
			state.scroll_active = false;
			presentation.graph_scroll(0);
			state.bomb_background = false;
		}
		if(!marisa_bomb_update_and_render(state, presentation)) return false;
		bomb_dream_decay(state, presentation);
	} else if(frame == 80U) {
		presentation.sound(15U);
		if(stage_scrolls(state)) state.scroll_active = true;
		state.items.items_pull_to_player = false;
		state.bomb_background = true;
		tone_deferred(state, 192);
	} else if(frame < 96U) {
		state.bomb_background = true;
		tone_deferred(state, 192 - ((static_cast<int>(frame) - 80) * 6));
		if((frame == 82U) && stage_scrolls(state)) {
			presentation.graph_scroll(state.scroll_line);
		}
	} else {
		state.bombing = false;
		tone_deferred(state, 100);
	}
	state.bomb_frame = static_cast<std::uint8_t>(state.bomb_frame + 1U);
	return true;
}

[[nodiscard]] bool bomb_mima(State &state, Presentation &presentation) noexcept
{
	const auto frame = state.bomb_frame;
	if(frame < 64U) {
		if(frame == 2U) {
			state.scroll_active = false;
			presentation.graph_scroll(0);
			state.bomb_background = false;
		}
		if(!mima_bomb_update_and_render(state, presentation)) return false;
		bomb_dream_decay(state, presentation);
	} else if(frame == 64U) {
		presentation.sound(15U);
		if(stage_scrolls(state)) state.scroll_active = true;
		state.items.items_pull_to_player = false;
		state.bomb_background = true;
		tone_deferred(state, 192);
	} else if(frame < 80U) {
		state.bomb_background = true;
		tone_deferred(state, 192 - ((static_cast<int>(frame) - 64) * 6));
		if((frame == 66U) && stage_scrolls(state)) {
			presentation.graph_scroll(state.scroll_line);
		}
	} else {
		state.bombing = false;
		tone_deferred(state, 100);
	}
	state.bomb_frame = static_cast<std::uint8_t>(state.bomb_frame + 1U);
	return true;
}

[[nodiscard]] bool bomb_yuuka(State &state, Presentation &presentation) noexcept
{
	const auto frame = state.bomb_frame;
	if(frame < 32U) {
		state.tiles_bb_color = 15U;
		presentation.bomb_cel(static_cast<std::uint8_t>(frame / 2U));
	} else if(frame == 32U) {
		state.scroll_active = false;
		presentation.graph_scroll(0);
		state.bomb_background = false;
		yuuka_bomb_update_and_render(state, presentation);
		bomb_dream_decay(state, presentation);
	} else if(frame < 160U) {
		yuuka_bomb_update_and_render(state, presentation);
		bomb_dream_decay(state, presentation);
	} else if(frame == 160U) {
		presentation.sound(15U);
		if(stage_scrolls(state)) state.scroll_active = true;
		state.items.items_pull_to_player = false;
		state.bomb_background = true;
		tone_deferred(state, 192);
	} else if(frame < 192U) {
		state.bomb_background = true;
		tone_deferred(state, 192 - ((static_cast<int>(frame) - 160) * 3));
		if((frame == 161U) && stage_scrolls(state)) {
			presentation.graph_scroll(state.scroll_line);
		}
	} else {
		state.bombing = false;
		tone_deferred(state, 100);
	}
	state.bomb_frame = static_cast<std::uint8_t>(state.bomb_frame + 1U);
	return true;
}

} // namespace

void player_bomb(State &state, Presentation &presentation) noexcept
{
	if(state.playchar > 3U) return;
	if(state.bombing || (state.bombs == 0U) || state.bombing_disabled) return;
	if(state.miss_time != 0U) {
		if(state.miss_time <= 32U) return;
		state.miss_time = 0U;
		state.player_is_hit = false;
		state.miss_move_lock_time = 0U;
	}

	--state.bombs;
	presentation.hud(HudField::bombs);
	state.bombing = true;
	state.bomb_frame = 0U;
	switch(state.playchar) {
	case 0U:
		state.player_invincibility_time = 192U;
		state.resources.bullet_clear_time = 192U;
		break;
	case 1U:
		state.player_invincibility_time = 160U;
		state.resources.bullet_clear_time = 128U;
		break;
	case 2U:
		state.player_invincibility_time = 144U;
		state.resources.bullet_clear_time = 96U;
		break;
	case 3U:
		state.player_invincibility_time = 224U;
		state.resources.bullet_clear_time = 192U;
		break;
	default:
		return;
	}
	state.bomb_background = true;
	presentation.sound(13U);
	state.items.items_pull_to_player = true;
	state.bombs_used = static_cast<std::uint8_t>(state.bombs_used + 1U);
	playperf_lower(state, 1U);
}

bool bomb_update(State &state, Presentation &presentation) noexcept
{
	if(!state.bombing) return true;
	switch(state.playchar) {
	case 0U: return bomb_reimu(state, presentation);
	case 1U: return bomb_marisa(state, presentation);
	case 2U: return bomb_mima(state, presentation);
	case 3U: return bomb_yuuka(state, presentation);
	default: return false;
	}
}

} // namespace th05::core
