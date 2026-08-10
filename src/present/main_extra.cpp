#include "th03/present/main_extra_charge.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "th03/core/numeric.hpp"
#include "th03/present/main_static.hpp"
#include "th03/present/triangles.hpp"
#include "th03/present/vectors.hpp"

namespace th03::present {
namespace {

enum class Character : std::uint8_t {
	reimu,
	mima,
	marisa,
	ellen,
	kotohime,
	kana,
	rikako,
	chiyuri,
	yumemi,
};

constexpr Sprite16HorizontalClip full_sprite_clip{16, 623};
constexpr std::array<Sprite16HorizontalClip, 2U> playfield_clips{{
	{0, 319},
	{320, 639},
}};
constexpr GrcgClipRect full_grcg_clip{0, 0, 639, 199};
constexpr std::array<GrcgClipRect, 2U> inner_playfield_clips{{
	{16, 8, 303, 191},
	{336, 8, 623, 191},
}};
constexpr std::array<std::size_t, 9U> extra_slot_counts{{
	8U, 6U, 14U, 12U, 10U, 8U, 14U, 16U, 16U,
}};

[[nodiscard]] constexpr bool valid_extra_fact(
	Character character,
	const MainExtraAttackFact &fact
) noexcept
{
	if(fact.state == 0U) {
		return true;
	}
	if(character == Character::kana) {
		// Kana reads the observed destination only while refreshing its
		// presentation-only cache. Every other visible state uses that cache.
		return (
			(fact.state != 2U && fact.state != 3U) ||
			fact.destination_raw <= 1U
		);
	}
	if(fact.destination_raw > 1U) {
		return false;
	}
	return !(
		character == Character::kotohime &&
		fact.state == 1U &&
		fact.kotohime_variant > 1U
	);
}

[[nodiscard]] constexpr std::optional<std::uint8_t> player_index(
	PlayerId player
) noexcept
{
	if(player == PlayerId::p1) {
		return std::uint8_t{0U};
	}
	if(player == PlayerId::p2) {
		return std::uint8_t{1U};
	}
	return std::nullopt;
}

[[nodiscard]] constexpr std::optional<Character> character_index(
	std::int8_t paletted_character
) noexcept
{
	if(paletted_character < 1 || paletted_character > 18) {
		return std::nullopt;
	}
	return static_cast<Character>((paletted_character - 1) / 2);
}

[[nodiscard]] constexpr std::int16_t wrap_word(std::int32_t value) noexcept
{
	return exact::from_bits<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] constexpr std::int16_t shift_right(
	std::int16_t value,
	unsigned count
) noexcept
{
	return exact::arithmetic_shift_right(value, count);
}

[[nodiscard]] constexpr std::int16_t foreground_x(
	const MainPhaseContext &context,
	std::int16_t raw_x,
	std::uint8_t destination_raw
) noexcept
{
	const auto target = static_cast<std::uint8_t>(destination_raw != 0U);
	return wrap_word(
		static_cast<std::int32_t>(shift_right(raw_x, 4U)) +
		static_cast<std::int32_t>(main_playfield_border) +
		static_cast<std::int32_t>(target * main_playfield_bordered_width) +
		context.playfield_fg_shift_x[target]
	);
}

[[nodiscard]] constexpr std::int16_t sprite_center_y(
	std::int16_t raw_y
) noexcept
{
	return wrap_word(static_cast<std::int32_t>(shift_right(raw_y, 4U)) + 16);
}

[[nodiscard]] constexpr Sprite16AtlasRect source_rect(
	std::uint8_t owner,
	std::uint16_t left,
	std::uint16_t raw_top,
	std::uint16_t width,
	std::uint16_t raw_height
) noexcept
{
	return {
		static_cast<std::uint16_t>(owner * 320U + left),
		raw_top,
		width,
		raw_height,
	};
}

[[nodiscard]] bool put(
	Raster &raster,
	const Sprite16Staging &staging,
	Sprite16AtlasRect source,
	Point position,
	Sprite16HorizontalClip clip,
	Sprite16DrawStyle style = {}
) noexcept
{
	return sprite16_put(raster, staging, source, position, clip, style);
}

[[nodiscard]] bool cloud(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	const MainExtraAttackFact &fact,
	std::uint8_t frame
) noexcept
{
	if((frame & 1U) != 0U) {
		return true;
	}
	const auto left = static_cast<std::uint16_t>(
		320U + (((frame & 7U) <= 3U) ? 32U : 0U)
	);
	return put(
		raster,
		staging,
		{left, 80U, 32U, 16U},
		{
			wrap_word(
				static_cast<std::int32_t>(foreground_x(
					context, fact.center_x, fact.destination_raw
				)) - 16
			),
			wrap_word(static_cast<std::int32_t>(sprite_center_y(fact.center_y)) - 16),
		},
		full_sprite_clip
	);
}

[[nodiscard]] bool explosion(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	const MainExtraAttackFact &fact,
	std::uint8_t frame
) noexcept
{
	const auto phase = static_cast<std::uint16_t>((frame / 4U) % 3U);
	return put(
		raster,
		staging,
		{
			static_cast<std::uint16_t>(384U + phase * 48U),
			80U,
			48U,
			24U,
		},
		{
			wrap_word(
				static_cast<std::int32_t>(foreground_x(
					context, fact.center_x, fact.destination_raw
				)) - 24
			),
			wrap_word(static_cast<std::int32_t>(sprite_center_y(fact.center_y)) - 24),
		},
		full_sprite_clip
	);
}

[[nodiscard]] bool render_reimu(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	const MainExtraAttackFact &fact,
	std::uint8_t owner
) noexcept
{
	if(fact.state == 2U) {
		return cloud(raster, staging, context, fact, fact.frame);
	}
	if(fact.state != 1U) {
		return explosion(raster, staging, context, fact, fact.state);
	}
	const auto phase = static_cast<std::uint8_t>((fact.frame / 4U) & 3U);
	std::uint16_t left = 0U;
	std::uint16_t raw_top = 8U;
	if(phase == 1U) {
		left = 48U;
	} else if(phase != 0U) {
		raw_top = 32U;
		left = static_cast<std::uint16_t>((phase & 1U) * 48U);
	}
	return put(
		raster,
		staging,
		source_rect(owner, left, raw_top, 48U, 24U),
		{
			wrap_word(static_cast<std::int32_t>(foreground_x(
				context, fact.center_x, fact.destination_raw
			)) - 24),
			wrap_word(static_cast<std::int32_t>(sprite_center_y(fact.center_y)) - 24),
		},
		playfield_clips[1U - owner]
	);
}

[[nodiscard]] bool render_mima(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	const MainExtraAttackFact &fact,
	std::uint8_t owner
) noexcept
{
	if(fact.state == 2U) {
		return cloud(raster, staging, context, fact, fact.mima_timer_low);
	}
	if(fact.state != 1U) {
		return explosion(raster, staging, context, fact, fact.state);
	}
	std::uint16_t left = 0U;
	std::uint16_t raw_top = 8U;
	if(fact.frame == 0U) {
		left = 48U;
		raw_top = 32U;
	} else if(fact.frame == 1U) {
		raw_top = 32U;
	} else if(fact.frame == 2U) {
		left = 48U;
	}
	return put(
		raster,
		staging,
		source_rect(owner, left, raw_top, 48U, 24U),
		{
			wrap_word(static_cast<std::int32_t>(foreground_x(
				context, fact.center_x, fact.destination_raw
			)) - 24),
			wrap_word(static_cast<std::int32_t>(sprite_center_y(fact.center_y)) - 24),
		},
		playfield_clips[1U - owner]
	);
}

[[nodiscard]] bool render_marisa(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	const MainExtraAttackFact &fact,
	std::uint8_t owner
) noexcept
{
	if(fact.state == 2U) {
		return cloud(raster, staging, context, fact, fact.frame);
	}
	if(fact.state != 1U) {
		return explosion(raster, staging, context, fact, fact.state);
	}
	const auto center_x = foreground_x(context, fact.center_x, fact.destination_raw);
	const auto center_y = sprite_center_y(fact.center_y);
	std::uint16_t cel_left = 208U;
	bool head = false;
	if(fact.frame < 48U) {
		head = true;
		cel_left = 224U;
	} else if(fact.frame < 96U) {
		cel_left = 224U;
	} else if(fact.frame < 102U) {
		cel_left = 240U;
	} else if(fact.frame < 108U) {
		cel_left = 256U;
	} else if(fact.frame < 114U) {
		cel_left = 272U;
	} else {
		cel_left = 288U;
	}
	const Point top{
		wrap_word(static_cast<std::int32_t>(center_x) - 8),
		wrap_word(static_cast<std::int32_t>(center_y) - 8),
	};
	if(head && !put(
		raster,
		staging,
		source_rect(owner, 208U, 0U, 16U, 8U),
		top,
		playfield_clips[1U - owner]
	)) {
		return false;
	}
	return sprite16_put_downwards_column(
		raster,
		staging,
		source_rect(owner, cel_left, 0U, 16U, 8U),
		{top.x, wrap_word(static_cast<std::int32_t>(top.y) + 16)},
		playfield_clips[1U - owner]
	);
}

[[nodiscard]] bool render_ellen(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	const MainExtraAttackFact &fact,
	std::uint8_t owner
) noexcept
{
	if(fact.state == 2U) {
		return cloud(raster, staging, context, fact, fact.frame);
	}
	if(fact.state != 1U) {
		return explosion(raster, staging, context, fact, fact.state);
	}
	constexpr std::array<std::size_t, 4U> trail_points{{6U, 4U, 2U, 0U}};
	std::uint16_t cel_left = 96U;
	const auto clip = playfield_clips[static_cast<std::size_t>(fact.destination_raw != 0U)];
	for(const auto point : trail_points) {
		if(!put(
			raster,
			staging,
			source_rect(owner, cel_left, 8U, 32U, 16U),
			{
				wrap_word(static_cast<std::int32_t>(foreground_x(
					context, fact.ellen_trail_x[point], fact.destination_raw
				)) - 16),
				wrap_word(static_cast<std::int32_t>(sprite_center_y(
					fact.ellen_trail_y[point]
				)) - 16),
			},
			clip
		)) {
			return false;
		}
		cel_left = static_cast<std::uint16_t>(cel_left - 32U);
	}
	return true;
}

[[nodiscard]] bool render_kana(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	const MainExtraAttackFact &fact,
	std::uint8_t owner,
	std::uint8_t destination
) noexcept
{
	MainExtraAttackFact visible = fact;
	visible.destination_raw = destination;
	if(fact.state == 2U) {
		return cloud(raster, staging, context, visible, fact.frame);
	}
	if(fact.state != 1U) {
		return explosion(raster, staging, context, visible, fact.frame);
	}
	const auto phase = static_cast<std::uint16_t>((fact.frame / 4U) & 3U);
	return put(
		raster,
		staging,
		source_rect(
			owner,
			32U,
			static_cast<std::uint16_t>(8U + phase * 16U),
			32U,
			16U
		),
		{
			wrap_word(static_cast<std::int32_t>(foreground_x(
				context, fact.center_x, destination
			)) - 16),
			wrap_word(static_cast<std::int32_t>(sprite_center_y(fact.center_y)) - 16),
		},
		playfield_clips[1U - owner]
	);
}

[[nodiscard]] bool render_rikako(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	const MainExtraAttackFact &fact,
	std::uint8_t owner
) noexcept
{
	if(fact.state == 2U) {
		return cloud(raster, staging, context, fact, fact.frame);
	}
	if(fact.state != 1U) {
		return explosion(raster, staging, context, fact, fact.frame);
	}
	return put(
		raster,
		staging,
		source_rect(
			owner,
			0U,
			static_cast<std::uint16_t>(24U + ((fact.frame & 1U) * 24U)),
			48U,
			24U
		),
		{
			wrap_word(static_cast<std::int32_t>(foreground_x(
				context, fact.center_x, fact.destination_raw
			)) - 24),
			wrap_word(static_cast<std::int32_t>(sprite_center_y(fact.center_y)) - 24),
		},
		playfield_clips[1U - owner]
	);
}

[[nodiscard]] bool render_kotohime(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	const MainExtraAttackFact &fact,
	std::uint8_t owner
) noexcept
{
	if(fact.state == 2U) {
		return cloud(raster, staging, context, fact, fact.frame);
	}
	if(fact.state != 1U) {
		return explosion(raster, staging, context, fact, fact.frame);
	}
	std::uint16_t raw_top = 8U;
	const auto distance = wrap_word(
		static_cast<std::int32_t>(fact.kotohime_target_y) - fact.center_y
	);
	if(distance <= static_cast<std::int16_t>(96 << 4) &&
		(fact.frame % 4U) < 2U) {
		raw_top = 40U;
	}
	std::uint16_t left = 0U;
	std::uint16_t width = 64U;
	std::uint16_t raw_height = 32U;
	std::int16_t radius = 32;
	if(fact.kotohime_variant == 0U) {
		left = 16U;
		raw_top = static_cast<std::uint16_t>(raw_top + 8U);
		width = 32U;
		raw_height = 16U;
		radius = 16;
	}
	return put(
		raster,
		staging,
		source_rect(owner, left, raw_top, width, raw_height),
		{
			wrap_word(static_cast<std::int32_t>(foreground_x(
				context, fact.center_x, fact.destination_raw
			)) - radius),
			wrap_word(static_cast<std::int32_t>(sprite_center_y(fact.center_y)) - radius),
		},
		full_sprite_clip
	);
}

[[nodiscard]] bool render_chiyuri_beam(
	Raster &raster,
	const MainPhaseContext &context,
	const MainExtraAttackFact &first,
	const MainExtraAttackFact &second
) noexcept
{
	const auto first_x = foreground_x(context, first.center_x, first.destination_raw);
	const auto second_x = foreground_x(context, second.center_x, second.destination_raw);
	const auto frame = first.frame;
	if(frame < 16U) {
		return grcg_line(
			raster, {first_x, 8}, {second_x, 192}, full_grcg_clip, 15U
		);
	}
	if(frame >= 136U) {
		return true;
	}
	if(frame >= 40U && frame < 112U) {
		return grcg_trapezoid(
			raster,
			8,
			wrap_word(static_cast<std::int32_t>(first_x) - 6),
			wrap_word(static_cast<std::int32_t>(first_x) + 6),
			192,
			wrap_word(static_cast<std::int32_t>(second_x) - 6),
			wrap_word(static_cast<std::int32_t>(second_x) + 6),
			full_grcg_clip,
			{9U, 0xFFFFU}
		) && grcg_trapezoid(
			raster,
			8,
			wrap_word(static_cast<std::int32_t>(first_x) - 3),
			wrap_word(static_cast<std::int32_t>(first_x) + 3),
			192,
			wrap_word(static_cast<std::int32_t>(second_x) - 3),
			wrap_word(static_cast<std::int32_t>(second_x) + 3),
			full_grcg_clip,
			{10U, 0xFFFFU}
		) && grcg_trapezoid(
			raster,
			8,
			wrap_word(static_cast<std::int32_t>(first_x) - 1),
			wrap_word(static_cast<std::int32_t>(first_x) + 1),
			192,
			wrap_word(static_cast<std::int32_t>(second_x) - 1),
			wrap_word(static_cast<std::int32_t>(second_x) + 1),
			full_grcg_clip,
			{15U, 0xFFFFU}
		);
	}
	const auto radius = static_cast<std::int16_t>(
		(frame < 40U) ? ((frame - 16U) / 4U) : ((136U - frame) / 4U)
	);
	const auto inner = static_cast<std::int16_t>(radius / 2);
	return grcg_trapezoid(
		raster,
		8,
		wrap_word(static_cast<std::int32_t>(first_x) - radius),
		wrap_word(static_cast<std::int32_t>(first_x) + radius),
		192,
		wrap_word(static_cast<std::int32_t>(second_x) - radius),
		wrap_word(static_cast<std::int32_t>(second_x) + radius),
		full_grcg_clip,
		{9U, 0xFFFFU}
	) && grcg_trapezoid(
		raster,
		8,
		wrap_word(static_cast<std::int32_t>(first_x) - inner),
		wrap_word(static_cast<std::int32_t>(first_x) + inner),
		192,
		wrap_word(static_cast<std::int32_t>(second_x) - inner),
		wrap_word(static_cast<std::int32_t>(second_x) + inner),
		full_grcg_clip,
		{10U, 0xFFFFU}
	) && grcg_line(
		raster, {first_x, 8}, {second_x, 192}, full_grcg_clip, 15U
	);
}

[[nodiscard]] bool render_chiyuri_pair(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	const MainExtraAttackFact &first,
	const MainExtraAttackFact &second
) noexcept
{
	if(first.state == 1U) {
		return render_chiyuri_beam(raster, context, first, second);
	}
	const bool first_ok = (first.state == 2U) ?
		cloud(raster, staging, context, first, first.frame) :
		explosion(raster, staging, context, first, first.frame);
	if(!first_ok) {
		return false;
	}
	return (second.state == 2U) ?
		cloud(raster, staging, context, second, second.frame) :
		explosion(raster, staging, context, second, second.frame);
}

[[nodiscard]] bool render_yumemi_active(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	const MainExtraAttackFact &fact,
	std::uint8_t owner
) noexcept
{
	std::int16_t left = foreground_x(context, fact.center_x, fact.destination_raw);
	std::int16_t top = sprite_center_y(fact.center_y);
	const auto target = static_cast<std::size_t>(1U - owner);
	const auto raw_frame = fact.frame;
	if(raw_frame <= 16U) {
		const auto frame = static_cast<std::int16_t>(raw_frame);
		const auto half = static_cast<std::int16_t>(frame / 2);
		const auto y = static_cast<std::int16_t>(top / 2);
		if(!grcg_hline(raster, wrap_word(left + frame - 32), wrap_word(left + 32 - frame), wrap_word(y + half - 40), inner_playfield_clips[target], 6U) ||
			!grcg_hline(raster, wrap_word(left + frame - 32), wrap_word(left + 32 - frame), wrap_word(y + 52 - half), inner_playfield_clips[target], 6U) ||
			!grcg_vline(raster, wrap_word(left + frame - 32), wrap_word(y + half - 40), wrap_word(y + 52 - half), inner_playfield_clips[target], 6U) ||
			!grcg_vline(raster, wrap_word(left + 32 - frame), wrap_word(y + half - 40), wrap_word(y + 52 - half), inner_playfield_clips[target], 6U) ||
			!grcg_hline(raster, wrap_word(left + frame - 80), wrap_word(left + 80 - frame), wrap_word(y + half - 16), inner_playfield_clips[target], 6U) ||
			!grcg_hline(raster, wrap_word(left + frame - 80), wrap_word(left + 80 - frame), wrap_word(y + 16 - half), inner_playfield_clips[target], 6U) ||
			!grcg_vline(raster, wrap_word(left + frame - 80), wrap_word(y + half - 16), wrap_word(y + 16 - half), inner_playfield_clips[target], 6U) ||
			!grcg_vline(raster, wrap_word(left + 80 - frame), wrap_word(y + half - 16), wrap_word(y + 16 - half), inner_playfield_clips[target], 6U)) {
			return false;
		}
	} else {
		const auto collapsed = static_cast<std::int8_t>(
			static_cast<std::uint8_t>(fact.yumemi_duration - 8U - raw_frame)
		);
		const auto frame = static_cast<std::int16_t>(collapsed);
		const auto half = static_cast<std::int16_t>(frame / 2);
		const auto y = static_cast<std::int16_t>(top / 2);
		const Index4 color = (collapsed > 16) ? 5U : 6U;
		if(!grcg_hline(raster, wrap_word(left + frame - 16), wrap_word(left + 16 - frame), wrap_word(y + half - 32), inner_playfield_clips[target], color) ||
			!grcg_hline(raster, wrap_word(left + frame - 16), wrap_word(left + 16 - frame), wrap_word(y + 44 - half), inner_playfield_clips[target], color) ||
			!grcg_vline(raster, wrap_word(left + frame - 16), wrap_word(y + half - 32), wrap_word(y + 44 - half), inner_playfield_clips[target], color) ||
			!grcg_vline(raster, wrap_word(left + 16 - frame), wrap_word(y + half - 32), wrap_word(y + 44 - half), inner_playfield_clips[target], color) ||
			!grcg_hline(raster, wrap_word(left + frame - 64), wrap_word(left + 64 - frame), wrap_word(y + half - 8), inner_playfield_clips[target], color) ||
			!grcg_hline(raster, wrap_word(left + frame - 64), wrap_word(left + 64 - frame), wrap_word(y + 8 - half), inner_playfield_clips[target], color) ||
			!grcg_vline(raster, wrap_word(left + frame - 64), wrap_word(y + half - 8), wrap_word(y + 8 - half), inner_playfield_clips[target], color) ||
			!grcg_vline(raster, wrap_word(left + 64 - frame), wrap_word(y + half - 8), wrap_word(y + 8 - half), inner_playfield_clips[target], color)) {
			return false;
		}
	}

	left = wrap_word(static_cast<std::int32_t>(left) - 16);
	const auto extent = fact.yumemi_extent;
	std::int16_t y = wrap_word(static_cast<std::int32_t>(top) - extent);
	const auto clip = playfield_clips[target];
	const auto base = source_rect(owner, 0U, 8U, 32U, 8U);
	if(!put(raster, staging, base, {left, y}, clip)) {
		return false;
	}
	y = wrap_word(static_cast<std::int32_t>(y) + 16);
	const auto upper_limit = wrap_word(static_cast<std::int32_t>(top) - 16);
	while(upper_limit > y) {
		if(!put(raster, staging, source_rect(owner, 16U, 24U, 32U, 8U), {left, y}, clip)) {
			return false;
		}
		y = wrap_word(static_cast<std::int32_t>(y) + 16);
	}
	y = wrap_word(
		static_cast<std::int32_t>(top) +
		static_cast<std::int32_t>(extent) +
		static_cast<std::int32_t>(extent / 2U) - 16
	);
	if(!put(raster, staging, source_rect(owner, 0U, 16U, 32U, 8U), {left, y}, clip)) {
		return false;
	}
	y = wrap_word(static_cast<std::int32_t>(y) - 16);
	while(y > top) {
		if(!put(raster, staging, source_rect(owner, 16U, 24U, 32U, 8U), {left, y}, clip)) {
			return false;
		}
		y = wrap_word(static_cast<std::int32_t>(y) - 16);
	}
	top = wrap_word(static_cast<std::int32_t>(top) - 16);
	std::int16_t strip_left = wrap_word(
		static_cast<std::int32_t>(left) - extent + 16
	);
	if(!put(raster, staging, source_rect(owner, 0U, 8U, 16U, 16U), {strip_left, top}, clip)) {
		return false;
	}
	strip_left = wrap_word(static_cast<std::int32_t>(strip_left) + 16);
	while(strip_left < left) {
		if(!put(raster, staging, source_rect(owner, 0U, 24U, 16U, 16U), {strip_left, top}, clip)) {
			return false;
		}
		strip_left = wrap_word(static_cast<std::int32_t>(strip_left) + 16);
	}
	strip_left = wrap_word(static_cast<std::int32_t>(left) + extent);
	if(!put(raster, staging, source_rect(owner, 16U, 8U, 16U, 16U), {strip_left, top}, clip)) {
		return false;
	}
	strip_left = wrap_word(static_cast<std::int32_t>(strip_left) - 16);
	while(wrap_word(static_cast<std::int32_t>(left) + 16) < strip_left) {
		if(!put(raster, staging, source_rect(owner, 0U, 24U, 16U, 16U), {strip_left, top}, clip)) {
			return false;
		}
		strip_left = wrap_word(static_cast<std::int32_t>(strip_left) - 16);
	}
	if(extent > 24U) {
		return put(
			raster,
			staging,
			source_rect(owner, 32U, 8U, 32U, 16U),
			{left, top},
			clip
		);
	}
	return true;
}

[[nodiscard]] bool render_yumemi(
	Raster &raster,
	const Sprite16Staging &staging,
	const MainPhaseContext &context,
	const MainExtraAttackFact &fact,
	std::uint8_t owner
) noexcept
{
	if(fact.state == 1U) {
		return render_yumemi_active(raster, staging, context, fact, owner);
	}
	if(fact.state == 2U) {
		return cloud(raster, staging, context, fact, fact.frame);
	}
	return explosion(raster, staging, context, fact, fact.frame);
}

} // namespace

MainExtraChargePresenter::MainExtraChargePresenter(
	Raster &raster,
	const Sprite16Staging &staging,
	StaffFlakeTrigTables trig
) noexcept :
	raster_(&raster),
	staging_(&staging),
	trig_(trig)
{
	if(trig_.sine_q8.size() != 256U || trig_.cosine_q8.size() != 256U) {
		status_ = MainExtraChargeStatus::invalid_assets;
		return;
	}
	if(!staging_->committed()) {
		status_ = MainExtraChargeStatus::staging_not_committed;
	}
}

MainExtraChargeStatus MainExtraChargePresenter::status() const noexcept
{
	return status_;
}

bool MainExtraChargePresenter::ready() const noexcept
{
	return status_ == MainExtraChargeStatus::ready;
}

void MainExtraChargePresenter::observe(const MainExtraAttackView &view) noexcept
{
	if(!ready()) {
		return;
	}
	const auto owner = player_index(view.player());
	const auto character = character_index(view.paletted_character());
	if(!owner || !character) {
		status_ = MainExtraChargeStatus::invalid_observation;
		return;
	}
	const auto character_raw = static_cast<std::size_t>(*character);
	const auto count = extra_slot_counts[character_raw];
	if(view.slot_count() != count) {
		status_ = MainExtraChargeStatus::invalid_observation;
		return;
	}
	std::array<MainExtraAttackFact, MainExtraAttackView::max_count> facts{};
	for(std::size_t slot = 0U; slot < count; ++slot) {
		const auto fact = view.attack(static_cast<ExtraAttackSlot>(slot));
		if(!fact || static_cast<std::size_t>(fact->slot) != slot) {
			status_ = MainExtraChargeStatus::invalid_observation;
			return;
		}
		facts[slot] = *fact;
	}
	for(std::size_t slot = 0U; slot < count; ++slot) {
		if(!valid_extra_fact(*character, facts[slot])) {
			status_ = MainExtraChargeStatus::invalid_observation;
			return;
		}
	}
	if(*character == Character::chiyuri) {
		for(std::size_t slot = 0U; slot < count; slot += 2U) {
			// A live first member causes the source to render both pair members.
			// The second member's free/stale payload is therefore render-visible.
			if(
				facts[slot].state != 0U &&
				facts[slot + 1U].destination_raw > 1U
			) {
				status_ = MainExtraChargeStatus::invalid_observation;
				return;
			}
		}
	}
	if(*character == Character::kana) {
		for(std::size_t slot = 0U; slot < count; ++slot) {
			if(facts[slot].state == 2U || facts[slot].state == 3U) {
				kana_destination_cache_[*owner][slot] = facts[slot].destination_raw;
			}
		}
	}

	const auto context = view.context();
	bool rendered = true;
	if(*character == Character::chiyuri) {
		for(std::size_t slot = 0U; slot < count && rendered; slot += 2U) {
			if(facts[slot].state != 0U) {
				rendered = render_chiyuri_pair(
					*raster_, *staging_, context, facts[slot], facts[slot + 1U]
				);
			}
		}
	} else {
		for(std::size_t slot = 0U; slot < count && rendered; ++slot) {
			const auto &fact = facts[slot];
			if(fact.state == 0U) {
				continue;
			}
			switch(*character) {
			case Character::reimu:
				rendered = render_reimu(*raster_, *staging_, context, fact, *owner);
				break;
			case Character::mima:
				rendered = render_mima(*raster_, *staging_, context, fact, *owner);
				break;
			case Character::marisa:
				rendered = render_marisa(*raster_, *staging_, context, fact, *owner);
				break;
			case Character::ellen:
				rendered = render_ellen(*raster_, *staging_, context, fact, *owner);
				break;
			case Character::kotohime:
				rendered = render_kotohime(*raster_, *staging_, context, fact, *owner);
				break;
			case Character::kana:
				rendered = render_kana(
					*raster_,
					*staging_,
					context,
					fact,
					*owner,
					kana_destination_cache_[*owner][slot]
				);
				break;
			case Character::rikako:
				rendered = render_rikako(*raster_, *staging_, context, fact, *owner);
				break;
			case Character::yumemi:
				rendered = render_yumemi(*raster_, *staging_, context, fact, *owner);
				break;
			case Character::chiyuri:
				break;
			}
		}
	}
	if(!rendered) {
		status_ = MainExtraChargeStatus::draw_failed;
	}
}

} // namespace th03::present
