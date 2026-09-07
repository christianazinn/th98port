#include "th03/present/main_boss.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "th03/core/numeric.hpp"
#include "th03/present/main_static.hpp"
#include "th03/present/vectors.hpp"

namespace th03::present {
namespace {

constexpr std::array<std::uint16_t, 9U> boss_base_left{{
	96U, 96U, 0U, 0U, 64U, 64U, 48U, 64U, 64U,
}};
constexpr std::array<std::uint16_t, 9U> boss_base_raw_top{{
	8U, 8U, 8U, 24U, 8U, 8U, 8U, 8U, 8U,
}};
constexpr std::array<std::uint16_t, 9U> boss_body_width{{
	96U, 144U, 176U, 64U, 128U, 128U, 96U, 128U, 112U,
}};
constexpr std::array<std::uint16_t, 9U> boss_body_raw_height{{
	48U, 56U, 48U, 48U, 48U, 40U, 48U, 64U, 56U,
}};
constexpr std::array<std::int16_t, 9U> boss_body_left_radius{{
	48, 72, 88, 32, 64, 64, 48, 64, 56,
}};
constexpr std::array<std::int16_t, 9U> boss_body_top_radius{{
	32, 40, 32, 32, 32, 24, 32, 48, 40,
}};

constexpr std::array<std::int16_t, 9U> ellen_trail_offset{{
	108, 96, 80, 64, 48, 32, 16, 8, 4,
}};
constexpr std::array<std::uint16_t, 9U> ellen_trail_screen_height{{
	2U, 8U, 16U, 16U, 16U, 16U, 16U, 8U, 2U,
}};

constexpr std::array<Sprite16HorizontalClip, 2U> target_clips{{
	{0, 319},
	{320, 639},
}};
constexpr std::array<GrcgClipRect, 2U> target_inner_clips{{
	{16, 8, 303, 191},
	{336, 8, 623, 191},
}};

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

[[nodiscard]] constexpr std::optional<std::uint8_t> character_index(
	std::int8_t paletted_character
) noexcept
{
	if(paletted_character < 1 || paletted_character > 18) {
		return std::nullopt;
	}
	return static_cast<std::uint8_t>((paletted_character - 1) / 2);
}

template <typename T>
[[nodiscard]] constexpr T wrap_add(T left, T right) noexcept
{
	return exact::wrap_add(left, right);
}

[[nodiscard]] constexpr std::int16_t wrap_word(std::int32_t value) noexcept
{
	return exact::from_bits<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] constexpr std::int16_t signed_shift_right(
	std::int16_t value,
	unsigned count
) noexcept
{
	return exact::arithmetic_shift_right(value, count);
}

[[nodiscard]] constexpr std::int16_t polar(
	std::int16_t center,
	std::int16_t radius,
	std::int16_t ratio
) noexcept
{
	return wrap_word(
		static_cast<std::int32_t>(center) + exact::arithmetic_shift_right(
			static_cast<std::int32_t>(radius) * ratio,
			8U
		)
	);
}

[[nodiscard]] constexpr std::int16_t foreground_x(
	const MainPhaseContext &context,
	std::int16_t raw_x,
	std::uint8_t target
) noexcept
{
	return wrap_word(
		static_cast<std::int32_t>(signed_shift_right(raw_x, 4U)) +
		static_cast<std::int32_t>(main_playfield_border) +
		static_cast<std::int32_t>(target * main_playfield_bordered_width) +
		context.playfield_fg_shift_x[target]
	);
}

[[nodiscard]] constexpr std::int16_t screen_polar(
	std::int16_t center,
	std::int16_t radius,
	std::int16_t ratio
) noexcept
{
	return polar(center, radius, ratio);
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

[[nodiscard]] constexpr Sprite16AtlasRect body_rect(
	std::uint8_t owner,
	std::uint8_t character
) noexcept
{
	return source_rect(
		owner,
		boss_base_left[character],
		boss_base_raw_top[character],
		boss_body_width[character],
		boss_body_raw_height[character]
	);
}

[[nodiscard]] constexpr Point body_position(
	const MainPhaseContext &context,
	const MainBossFact &boss,
	std::uint8_t character,
	std::uint8_t target
) noexcept
{
	return {
		wrap_word(
			static_cast<std::int32_t>(foreground_x(context, boss.center_x, target)) -
			boss_body_left_radius[character]
		),
		wrap_word(
			static_cast<std::int32_t>(signed_shift_right(boss.center_y, 4U)) -
			boss_body_top_radius[character]
		),
	};
}

[[nodiscard]] constexpr std::int16_t material_radius_q4(
	std::uint16_t frame
) noexcept
{
	return wrap_word(
		(static_cast<std::int32_t>(200) -
			(static_cast<std::int32_t>(frame) * 2)) * 16
	);
}

[[nodiscard]] constexpr Sprite16DrawStyle striped(
	std::uint16_t mask
) noexcept
{
	return {.write_mask = mask};
}

[[nodiscard]] constexpr bool state_in(
	std::uint8_t state,
	std::uint8_t first,
	std::uint8_t last
) noexcept
{
	return state >= first && state <= last;
}

} // namespace

MainBossPresenter::MainBossPresenter(
	Raster &raster,
	const Sprite16Staging &staging,
	StaffFlakeTrigTables trig,
	MainPaletteState &palette
) noexcept :
	raster_(&raster),
	staging_(&staging),
	trig_(trig),
	palette_(&palette)
{
	if(trig_.sine_q8.size() != 256U || trig_.cosine_q8.size() != 256U ||
		!palette_->baseline_ready()) {
		status_ = MainBossStatus::invalid_assets;
		return;
	}
	if(!staging_->committed()) {
		status_ = MainBossStatus::staging_not_committed;
	}
}

bool MainBossPresenter::fail(MainBossStatus status) noexcept
{
	if(status_ == MainBossStatus::ready) {
		status_ = status;
	}
	return false;
}

bool MainBossPresenter::put(
	Sprite16AtlasRect source,
	Point position,
	Sprite16HorizontalClip clip,
	Sprite16DrawStyle style
) noexcept
{
	return sprite16_put(
		*raster_, *staging_, source, position, clip, style
	) || fail(MainBossStatus::draw_failed);
}

bool MainBossPresenter::set_chiyuri_gray(
	std::uint8_t value,
	std::uint8_t target_player
) noexcept
{
	if(!palette_->set_entry_deferred(
		*raster_, target_player, Rgb8{value, value, value}
	)) {
		return fail(MainBossStatus::palette_failed);
	}
	return true;
}

void MainBossPresenter::boss_activated(
	const BossActivated &activated
) noexcept
{
	if(!ready()) {
		return;
	}
	const auto owner = player_index(activated.owner);
	const auto character = character_index(activated.paletted_character);
	if(!owner.has_value() || !character.has_value()) {
		(void)fail(MainBossStatus::invalid_observation);
		return;
	}
	activation_ = {
		true,
		activated.owner,
		activated.paletted_character,
		static_cast<Character>(*character),
	};
	ellen_phase_ = 0U;
	rikako_angle_ = 0U;
	rikako_angle_delta_ = 4;
	chiyuri_phase_ = 0U;
}

void MainBossPresenter::boss_animation_step(
	const BossAnimationStep &step
) noexcept
{
	if(!ready()) {
		return;
	}
	const auto owner = player_index(step.owner);
	if(!owner.has_value() || !activation_.active ||
		activation_.owner != step.owner) {
		(void)fail(MainBossStatus::invalid_observation);
		return;
	}

	switch(activation_.character) {
	case Character::ellen:
		ellen_phase_ = static_cast<std::uint8_t>(ellen_phase_ + 1U);
		if(state_in(step.state_at_dispatch, 6U, 10U)) {
			ellen_phase_ = static_cast<std::uint8_t>(ellen_phase_ + 3U);
		}
		return;

	case Character::rikako:
		if(state_in(step.state_at_dispatch, 2U, 5U)) {
			if((step.pattern_frame_at_dispatch % 8U) == 0U &&
				rikako_angle_delta_ <= 8) {
				rikako_angle_delta_++;
			}
		} else if(state_in(step.state_at_dispatch, 7U, 12U)) {
			if((step.pattern_frame_at_dispatch % 8U) == 0U &&
				rikako_angle_delta_ >= -8) {
				rikako_angle_delta_--;
			}
		} else if(step.state_at_dispatch == 6U ||
			state_in(step.state_at_dispatch, 13U, 17U)) {
			if((step.pattern_frame_at_dispatch % 8U) == 0U) {
				if(rikako_angle_delta_ < 2) {
					rikako_angle_delta_++;
				} else if(rikako_angle_delta_ > 2) {
					rikako_angle_delta_--;
				}
			}
		}
		rikako_angle_ = static_cast<std::uint8_t>(
			rikako_angle_ + rikako_angle_delta_
		);
		return;

	case Character::chiyuri: {
		const auto target = static_cast<std::uint8_t>(1U - *owner);
		if(step.state_at_dispatch == 0U &&
			step.pattern_frame_at_dispatch == 0x60U) {
			if(!set_chiyuri_gray(0xFFU, target)) {
				return;
			}
			chiyuri_phase_ = 0x10U;
		}
		if(step.state_at_dispatch == 1U &&
			step.render_state_at_dispatch == 0x10U) {
			chiyuri_phase_ = 0x10U;
		}
		if(chiyuri_phase_ != 0U) {
			chiyuri_phase_--;
			if(chiyuri_phase_ != 0U) {
				(void)set_chiyuri_gray(
					static_cast<std::uint8_t>(chiyuri_phase_ << 4U),
					target
				);
			}
		}
		return;
	}

	default:
		(void)fail(MainBossStatus::invalid_observation);
		return;
	}
}

void MainBossPresenter::boss_palette_baseline_restored(
	const BossPaletteBaselineRestored &restored
) noexcept
{
	if(!ready()) {
		return;
	}
	const auto player = player_index(restored.player);
	if(!player.has_value()) {
		(void)fail(MainBossStatus::invalid_observation);
		return;
	}
	if(!palette_->restore_entry_deferred(*raster_, *player)) {
		(void)fail(MainBossStatus::palette_failed);
	}
}

void MainBossPresenter::observe(const MainBossView &view) noexcept
{
	if(!ready()) {
		return;
	}
	const auto boss = view.boss();
	if(!boss.has_value()) {
		return;
	}
	std::array<RawPoint, MainBossView::reimu_orbit_point_count> points{};
	const auto character = character_index(boss->paletted_character);
	if(!character.has_value()) {
		(void)fail(MainBossStatus::invalid_observation);
		return;
	}
	if(*character == static_cast<std::uint8_t>(Character::reimu)) {
		for(std::size_t point = 0U; point < points.size(); ++point) {
			const auto value = view.reimu_orbit_point(
				static_cast<ReimuBossOrbitPoint>(point)
			);
			if(!value.has_value()) {
				(void)fail(MainBossStatus::invalid_observation);
				return;
			}
			points[point] = *value;
		}
	}
	(void)present_boss(
		view.context(), view.round_frame_is_odd(), *boss, points
	);
}

bool MainBossPresenter::present_boss(
	const MainPhaseContext &context,
	bool round_frame_is_odd,
	const MainBossFact &boss,
	const std::array<
		RawPoint,
		MainBossView::reimu_orbit_point_count
	> &reimu_orbit_points
) noexcept
{
	if(!ready()) {
		return false;
	}
	const auto owner = player_index(boss.owner);
	const auto character_raw = character_index(boss.paletted_character);
	if(!owner.has_value() || !character_raw.has_value() ||
		!activation_.active || activation_.owner != boss.owner ||
		activation_.paletted_character != boss.paletted_character ||
		static_cast<std::uint8_t>(activation_.character) != *character_raw) {
		return fail(MainBossStatus::invalid_observation);
	}
	const auto character = static_cast<Character>(*character_raw);
	if((character == Character::reimu &&
		boss.reimu_orbit_count > reimu_orbit_points.size()) ||
		(character == Character::kotohime && boss.state != 0U &&
			boss.state != 0xFFU && boss.render_state != 0U &&
			boss.kotohime_orbit_count == 0U) ||
		(character == Character::chiyuri && boss.render_frame > 3U) ||
		(character == Character::kana && boss.state != 0U &&
			boss.state != 0xFFU && boss.render_state > 2U) ||
		(character == Character::yumemi && boss.yumemi_radius > 255U) ||
		(boss.state == 0xFFU && boss.pattern_frame > 25U)) {
		return fail(MainBossStatus::invalid_observation);
	}

	const auto target = static_cast<std::uint8_t>(1U - *owner);
	const auto clip = target_clips[target];
	if(boss.state == 0U) {
		return render_materializing(
			context,
			round_frame_is_odd,
			boss,
			character,
			*owner,
			target,
			clip
		);
	}
	if(boss.state == 0xFFU) {
		return render_terminal(context, boss, target, clip);
	}
	return render_active(
		context,
		boss,
		character,
		*owner,
		target,
		clip,
		reimu_orbit_points
	);
}

bool MainBossPresenter::render_active(
	const MainPhaseContext &context,
	const MainBossFact &boss,
	Character character,
	std::uint8_t owner,
	std::uint8_t target,
	Sprite16HorizontalClip clip,
	const std::array<
		RawPoint,
		MainBossView::reimu_orbit_point_count
	> &reimu_orbit_points
) noexcept
{
	const auto character_raw = static_cast<std::uint8_t>(character);
	const auto base = body_rect(owner, character_raw);
	auto position = body_position(context, boss, character_raw, target);

	switch(character) {
	case Character::marisa:
		if(!put(base, position, clip)) {
			return false;
		}
		if(boss.hit != 0U) {
			return put(
				source_rect(owner, 176U, 8U, 48U, 40U),
				{wrap_word(position.x + 64), wrap_word(position.y + 16)},
				clip
			);
		}
		if(boss.render_state != 1U) {
			return true;
		}
		if((context.round_or_result_frame & 3U) < 2U) {
			if(!put(
				source_rect(owner, 224U, 8U, 48U, 40U),
				{wrap_word(position.x + 64), wrap_word(position.y + 16)},
				clip
			)) {
				return false;
			}
			return put(
				source_rect(owner, 0U, 56U, 112U, 8U),
				{wrap_word(position.x + 32), wrap_word(position.y + 48)},
				clip
			);
		}
		return put(
			source_rect(owner, 0U, 64U, 112U, 8U),
			{wrap_word(position.x + 32), wrap_word(position.y + 48)},
			clip
		);

	case Character::mima:
		if(!put(base, position, clip)) {
			return false;
		}
		if(boss.hit != 0U) {
			return put(
				source_rect(owner, 240U, 8U, 64U, 40U),
				{wrap_word(position.x + 32), wrap_word(position.y + 16)},
				clip
			);
		}
		if(boss.render_state == 2U || boss.render_state == 5U) {
			if(!put(
				source_rect(owner, 304U, 8U, 16U, 8U),
				{wrap_word(position.x + 48), wrap_word(position.y + 32)},
				clip
			)) {
				return false;
			}
			const auto frame = (boss.render_state == 5U) ? 0U : boss.render_frame;
			return put(
				source_rect(owner, (frame >= 0x10U) ? 32U : 0U, 56U, 32U, 16U),
				{wrap_word(position.x + 80), position.y},
				clip
			);
		}
		if(boss.render_state == 3U) {
			return put(
				source_rect(owner, 240U, 48U, 80U, 24U),
				{wrap_word(position.x + 32), wrap_word(position.y + 16)},
				clip
			);
		}
		if(boss.render_state == 4U) {
			return put(
				source_rect(owner, 304U, 8U, 16U, 8U),
				{wrap_word(position.x + 48), wrap_word(position.y + 32)},
				clip
			);
		}
		return true;

	case Character::yumemi:
		if(!put(base, position, clip)) {
			return false;
		}
		if(boss.render_state != 0U) {
			const auto raw_top = static_cast<std::uint16_t>(
				8U + ((boss.render_state == 2U) ? 32U : 0U)
			);
			if(!put(
				source_rect(owner, 176U, raw_top, 64U, 32U),
				{wrap_word(position.x + 32), position.y},
				clip
			)) {
				return false;
			}
		} else if(boss.hit != 0U) {
			if(!put(
				source_rect(owner, 240U, 8U, 80U, 48U),
				{wrap_word(position.x + 16), position.y},
				clip
			)) {
				return false;
			}
		}
		if(boss.yumemi_radius == 0U && boss.render_frame == 0U) {
			return true;
		}
		{
			const auto center = GrcgPoint{
				foreground_x(context, boss.yumemi_muzzle.x, target),
				wrap_word(
					static_cast<std::int32_t>(signed_shift_right(
						boss.yumemi_muzzle.y, 5U
					)) + 8
				),
			};
			bool drawn = false;
			if(boss.render_frame == 0U) {
				drawn = grcg_circle(
					*raster_,
					center,
					static_cast<std::uint8_t>(boss.yumemi_radius),
					target_inner_clips[target],
					10U
				);
			} else {
				drawn = grcg_line(
					*raster_,
					center,
					{
						foreground_x(context, boss.yumemi_target.x, target),
						wrap_word(
							static_cast<std::int32_t>(signed_shift_right(
								boss.yumemi_target.y, 5U
							)) + 8
						),
					},
					target_inner_clips[target],
					10U
				);
			}
			return drawn || fail(MainBossStatus::draw_failed);
		}

	case Character::reimu:
		if(boss.hit != 0U) {
			const auto hit = source_rect(owner, 192U, 8U, 96U, 48U);
			if(!put(hit, position, clip)) {
				return false;
			}
		} else if(!put(base, position, clip)) {
			return false;
		}
		if(boss.render_state != 1U) {
			return true;
		}
		{
			const auto cel = static_cast<std::uint8_t>(
				(boss.pattern_frame >> 2U) & 3U
			);
			const auto source = source_rect(
				owner,
				static_cast<std::uint16_t>((cel & 1U) * 48U),
				static_cast<std::uint16_t>(8U + ((cel >> 1U) * 24U)),
				48U,
				24U
			);
			for(std::uint8_t point = 0U; point < boss.reimu_orbit_count; ++point) {
				const auto &orbit = reimu_orbit_points[point];
				if(!put(
					source,
					{
						wrap_word(
							static_cast<std::int32_t>(foreground_x(
								context, orbit.x, target
							)) - 24
						),
						wrap_word(
							static_cast<std::int32_t>(signed_shift_right(
								orbit.y, 4U
							)) - 8
						),
					},
					clip
				)) {
					return false;
				}
			}
		}
		return true;

	case Character::ellen: {
		auto body_source = base;
		if(boss.hit != 0U) {
			body_source.left = static_cast<std::uint16_t>(body_source.left + 64U);
		}
		if(!put(body_source, position, clip)) {
			return false;
		}
		auto trail = static_cast<std::uint8_t>((ellen_phase_ / 4U) % 9U);
		for(std::uint8_t draw = 0U; draw < 3U; ++draw) {
			const auto offset = ellen_trail_offset[trail];
			if(!put(
				source_rect(
					owner,
					128U,
					static_cast<std::uint16_t>(8 + (offset / 2)),
					96U,
					static_cast<std::uint16_t>(
						ellen_trail_screen_height[trail] / 2U
					)
				),
				{
					wrap_word(
						static_cast<std::int32_t>(foreground_x(
							context, boss.center_x, target
						)) - 48
					),
					wrap_word(
						static_cast<std::int32_t>(signed_shift_right(
							boss.center_y, 4U
						)) + offset - 40
					),
				},
				clip
			)) {
				return false;
			}
			trail = static_cast<std::uint8_t>((trail == 0U) ? 8U : trail - 1U);
		}
		return true;
	}

	case Character::kotohime: {
		auto body_source = base;
		if(boss.hit != 0U) {
			body_source.left = static_cast<std::uint16_t>(body_source.left + 128U);
		}
		if(!put(body_source, position, clip)) {
			return false;
		}
		if(boss.render_state == 0U) {
			return true;
		}
		const auto source = source_rect(
			owner,
			16U,
			static_cast<std::uint16_t>(
				(boss.render_state == 2U &&
					(boss.pattern_frame & 3U) < 2U) ? 48U : 16U
			),
			32U,
			16U
		);
		for(std::uint8_t point = 0U; point < boss.kotohime_orbit_count; ++point) {
			const auto angle = static_cast<std::uint8_t>(
				(static_cast<std::uint16_t>(point) << 8U) /
				boss.kotohime_orbit_count + boss.pattern_angle
			);
			const auto x = polar(
				boss.center_x, boss.kotohime_radius, trig_.cosine_q8[angle]
			);
			const auto y = polar(
				boss.center_y, boss.kotohime_radius, trig_.sine_q8[angle]
			);
			if(!put(
				source,
				{
					wrap_word(
						static_cast<std::int32_t>(foreground_x(
							context, x, target
						)) - 16
					),
					signed_shift_right(y, 4U),
				},
				clip
			)) {
				return false;
			}
		}
		return true;
	}

	case Character::kana: {
		auto body_source = base;
		if(boss.hit != 0U) {
			body_source.left = static_cast<std::uint16_t>(body_source.left + 128U);
		}
		if(!put(body_source, position, clip)) {
			return false;
		}
		if(boss.render_state == 0U) {
			return true;
		}
		auto cel = std::uint16_t{0U};
		if(boss.render_state == 2U) {
			cel = (boss.render_frame < 8U) ? 0U :
				((boss.render_frame < 16U) ? 1U :
					((boss.render_frame < 24U) ? 2U : 3U));
		}
		return put(
			source_rect(
				owner,
				static_cast<std::uint16_t>(64U + cel * 64U),
				48U,
				64U,
				32U
			),
			{wrap_word(position.x + 32), position.y},
			clip
		);
	}

	case Character::rikako: {
		auto body_source = base;
		if(boss.hit != 0U) {
			body_source.left = static_cast<std::uint16_t>(body_source.left + 96U);
		}
		if(!put(body_source, position, clip)) {
			return false;
		}
		const auto orbit_center_x = wrap_word(position.x + 40);
		const auto orbit_center_y = wrap_word(position.y + 40);
		auto angle = rikako_angle_;
		for(std::uint8_t point = 0U; point < 8U; ++point) {
			const auto cel = static_cast<std::uint8_t>(point & 3U);
			const auto source = (cel == 0U) ?
				source_rect(owner, 32U, 8U, 16U, 8U) :
				source_rect(
					owner,
					static_cast<std::uint16_t>((cel - 1U) * 16U),
					72U,
					16U,
					8U
				);
			if(!put(
				source,
				{
					screen_polar(
						orbit_center_x, 48, trig_.cosine_q8[angle]
					),
					screen_polar(
						orbit_center_y, 48, trig_.sine_q8[angle]
					),
				},
				clip
			)) {
				return false;
			}
			angle = static_cast<std::uint8_t>(angle + 0x20U);
		}
		return true;
	}

	case Character::chiyuri:
		if(boss.render_state != 0U) {
			const auto color = (boss.render_state >= 0x18U) ? 6U :
				((boss.render_state >= 0x14U) ? 5U :
					((boss.render_state >= 0x0CU) ? 2U :
						((boss.render_state >= 8U) ? 5U :
							((boss.render_state >= 4U) ? 6U : 15U))));
			return put(
				base,
				position,
				clip,
				{.monochrome_color = static_cast<Index4>(color)}
			);
		}
		if(!put(base, position, clip)) {
			return false;
		}
		if(boss.hit != 0U) {
			return put(
				source_rect(owner, 0U, 32U, 48U, 40U),
				{wrap_word(position.x + 48), wrap_word(position.y + 32)},
				clip
			);
		}
		if(boss.render_frame == 0U) {
			return true;
		}
		return put(
			source_rect(
				owner,
				static_cast<std::uint16_t>(160U + boss.render_frame * 32U),
				8U,
				32U,
				24U
			),
			{wrap_word(position.x + 48), wrap_word(position.y + 32)},
			clip
		);
	}
	return fail(MainBossStatus::invalid_observation);
}

bool MainBossPresenter::render_materializing(
	const MainPhaseContext &context,
	bool round_frame_is_odd,
	const MainBossFact &boss,
	Character character,
	std::uint8_t owner,
	std::uint8_t target,
	Sprite16HorizontalClip clip
) noexcept
{
	const auto character_raw = static_cast<std::uint8_t>(character);
	const auto base = body_rect(owner, character_raw);
	const auto body = body_position(context, boss, character_raw, target);
	const auto frame = boss.pattern_frame;

	if(character == Character::marisa) {
		const auto distance = wrap_word(
			static_cast<std::int32_t>(200) - static_cast<std::int32_t>(frame * 2U)
		);
		return put(
			base,
			{wrap_word(body.x - distance), body.y},
			clip,
			striped(0xAAAAU)
		) && put(
			base,
			{wrap_word(body.x + distance), body.y},
			clip,
			striped(0x5555U)
		);
	}
	if(character == Character::mima) {
		const auto radius = wrap_word(
			static_cast<std::int32_t>(200) - static_cast<std::int32_t>(frame * 2U)
		);
		auto angle = static_cast<std::uint8_t>(frame * 3U);
		const auto first = Point{
			screen_polar(wrap_word(body.x), radius, trig_.cosine_q8[angle]),
			screen_polar(wrap_word(body.y), radius, trig_.sine_q8[angle]),
		};
		angle = static_cast<std::uint8_t>(angle + 0x80U);
		const auto second = Point{
			screen_polar(wrap_word(body.x), radius, trig_.cosine_q8[angle]),
			screen_polar(wrap_word(body.y), radius, trig_.sine_q8[angle]),
		};
		return put(base, first, clip, striped(0xAAAAU)) &&
			put(base, second, clip, striped(0x5555U));
	}
	if(character == Character::yumemi) {
		const auto style = Sprite16DrawStyle{
			.overlap = Sprite16Overlap::or_planes,
		};
		const auto top = body.y;
		const auto draw_forward = [this, base, top, clip, style](
			std::int16_t left,
			std::int16_t edge
		) noexcept {
			while(left <= edge) {
				if(!put(base, {left, top}, clip, style)) {
					return false;
				}
				left = wrap_word(left + 8);
			}
			return true;
		};
		const auto draw_backward = [this, base, top, clip, style](
			std::int16_t left,
			std::int16_t edge
		) noexcept {
			while(left >= edge) {
				if(!put(base, {left, top}, clip, style)) {
					return false;
				}
				left = wrap_word(left - 8);
			}
			return true;
		};
		const auto screen_from_pixel = [&context, target](std::int16_t pixel) {
			return foreground_x(
				context, wrap_word(static_cast<std::int32_t>(pixel) * 16), target
			);
		};
		if(frame < 24U) {
			return draw_forward(
				screen_from_pixel(-104),
				screen_from_pixel(static_cast<std::int16_t>(frame * 8U - 104U))
			);
		}
		if(frame < 48U) {
			return draw_forward(
				screen_from_pixel(static_cast<std::int16_t>((frame - 24U) * 8U - 104U)),
				screen_from_pixel(88)
			);
		}
		if(frame < 72U) {
			if(!draw_backward(
				screen_from_pixel(272),
				screen_from_pixel(static_cast<std::int16_t>(272U - (frame - 48U) * 8U))
			)) {
				return false;
			}
			return put(base, {screen_from_pixel(88), top}, clip, style);
		}
		if(frame < 96U) {
			return draw_backward(
				screen_from_pixel(static_cast<std::int16_t>(272U - (frame - 72U) * 8U)),
				screen_from_pixel(88)
			);
		}
		return true;
	}

	if((character == Character::reimu || character == Character::ellen ||
		character == Character::kotohime || character == Character::chiyuri) &&
		round_frame_is_odd && frame < 64U) {
		if(character == Character::ellen) {
			// Ellen's three materialization trails precede the parity return.
		} else {
			return true;
		}
	}
	if(character == Character::kana && round_frame_is_odd && frame < 32U) {
		return true;
	}
	if(character == Character::rikako &&
		(context.round_or_result_frame & 1U) != 0U && frame < 64U) {
		return true;
	}

	if(character == Character::reimu) {
		const auto cel = static_cast<std::uint8_t>((frame >> 2U) & 3U);
		const auto source = source_rect(
			owner,
			static_cast<std::uint16_t>((cel & 1U) * 48U),
			static_cast<std::uint16_t>(8U + ((cel >> 1U) * 24U)),
			48U,
			24U
		);
		auto angle = static_cast<std::uint8_t>(frame);
		const auto radius = wrap_word(
			static_cast<std::int32_t>(3200) - static_cast<std::int32_t>(frame * 32U)
		);
		for(std::uint8_t point = 0U; point < 8U; ++point) {
			const auto x = polar(boss.center_x, radius, trig_.cosine_q8[angle]);
			const auto y = polar(boss.center_y, radius, trig_.sine_q8[angle]);
			if(!put(
				source,
				{
					wrap_word(
						static_cast<std::int32_t>(foreground_x(context, x, target)) - 24
					),
					wrap_word(
						static_cast<std::int32_t>(signed_shift_right(y, 4U)) - 8
					),
				},
				clip
			)) {
				return false;
			}
			angle = static_cast<std::uint8_t>(angle + 0x20U);
		}
		return true;
	}
	if(character == Character::ellen) {
		auto trail = static_cast<std::uint8_t>(frame % 9U);
		for(std::uint8_t draw = 0U; draw < 3U; ++draw) {
			const auto offset = ellen_trail_offset[trail];
			if(!put(
				source_rect(
					owner,
					128U,
					static_cast<std::uint16_t>(8 + (offset / 2)),
					96U,
					static_cast<std::uint16_t>(
						ellen_trail_screen_height[trail] / 2U
					)
				),
				{
					wrap_word(
						static_cast<std::int32_t>(foreground_x(
							context, boss.center_x, target
						)) - 48
					),
					wrap_word(
						static_cast<std::int32_t>(signed_shift_right(
							boss.center_y, 4U
						)) + offset - 40
					),
				},
				clip
			)) {
				return false;
			}
			trail = static_cast<std::uint8_t>((trail == 0U) ? 8U : trail - 1U);
		}
		if(round_frame_is_odd && frame < 64U) {
			return true;
		}
		auto angle = static_cast<std::uint8_t>(frame * 3U);
		const auto radius = material_radius_q4(frame);
		for(std::uint8_t ring = 0U; ring < 4U; ++ring) {
			const auto source = source_rect(
				owner,
				static_cast<std::uint16_t>(96U - ring * 32U),
				8U,
				32U,
				16U
			);
			for(std::uint8_t point = 0U; point < 8U; ++point) {
				const auto x = polar(
					boss.center_x, radius, trig_.cosine_q8[angle]
				);
				const auto y = polar(
					boss.center_y, radius, trig_.sine_q8[angle]
				);
				if(!put(
					source,
					{
						wrap_word(
							static_cast<std::int32_t>(foreground_x(
								context, x, target
							)) - 16
						),
						signed_shift_right(y, 4U),
					},
					clip
				)) {
					return false;
				}
				angle = static_cast<std::uint8_t>(angle + 0x20U);
			}
			angle = static_cast<std::uint8_t>(angle + 3U);
		}
		return true;
	}
	if(character == Character::kotohime) {
		const auto angle_start = static_cast<std::uint8_t>(boss.pattern_angle + 2U);
		const auto radius = material_radius_q4(frame);
		const auto source = source_rect(owner, 16U, 16U, 32U, 16U);
		for(std::uint8_t point = 0U; point < 8U; ++point) {
			const auto angle = static_cast<std::uint8_t>(
				(static_cast<std::uint16_t>(point) << 8U) / 8U + angle_start
			);
			const auto x = polar(boss.center_x, radius, trig_.cosine_q8[angle]);
			const auto y = polar(boss.center_y, radius, trig_.sine_q8[angle]);
			if(!put(
				source,
				{
					wrap_word(
						static_cast<std::int32_t>(foreground_x(context, x, target)) - 16
					),
					signed_shift_right(y, 4U),
				},
				clip
			)) {
				return false;
			}
		}
		return true;
	}
	if(character == Character::kana) {
		auto angle = static_cast<std::uint8_t>(0U - boss.pattern_angle);
		const auto source = source_rect(
			owner,
			32U,
			static_cast<std::uint16_t>(8U + (((frame >> 2U) & 3U) * 16U)),
			32U,
			16U
		);
		const auto radius = material_radius_q4(frame);
		for(std::uint8_t point = 0U; point < 16U; ++point) {
			const auto x = polar(boss.center_x, radius, trig_.cosine_q8[angle]);
			const auto y = polar(boss.center_y, radius, trig_.sine_q8[angle]);
			if(!put(
				source,
				{
					wrap_word(
						static_cast<std::int32_t>(foreground_x(context, x, target)) - 16
					),
					signed_shift_right(y, 4U),
				},
				clip
			)) {
				return false;
			}
			angle = static_cast<std::uint8_t>(angle + 0x10U);
		}
		return true;
	}
	if(character == Character::rikako) {
		auto angle = static_cast<std::uint8_t>(
			0U - static_cast<std::uint8_t>(frame * 4U)
		);
		const auto source = source_rect(
			owner, 0U, ((frame & 1U) != 0U) ? 48U : 24U, 48U, 24U
		);
		const auto radius = material_radius_q4(frame);
		for(std::uint8_t point = 0U; point < 16U; ++point) {
			const auto x = polar(boss.center_x, radius, trig_.cosine_q8[angle]);
			const auto y = polar(boss.center_y, radius, trig_.sine_q8[angle]);
			if(!put(
				source,
				{
					wrap_word(
						static_cast<std::int32_t>(foreground_x(context, x, target)) - 24
					),
					wrap_word(
						static_cast<std::int32_t>(signed_shift_right(y, 4U)) - 8
					),
				},
				clip
			)) {
				return false;
			}
			angle = static_cast<std::uint8_t>(angle + 0x10U);
		}
		return true;
	}
	if(character == Character::chiyuri) {
		std::int16_t radius_pixels = 0;
		if(frame < 32U) {
			radius_pixels = wrap_word(256 - static_cast<std::int32_t>(frame * 8U));
			chiyuri_phase_ = static_cast<std::uint8_t>(chiyuri_phase_ + 4U);
		} else if(frame < 64U) {
			radius_pixels = wrap_word(512 - static_cast<std::int32_t>(frame * 8U));
			chiyuri_phase_ = static_cast<std::uint8_t>(chiyuri_phase_ - 4U);
		} else {
			radius_pixels = wrap_word(768 - static_cast<std::int32_t>(frame * 8U));
			chiyuri_phase_ = static_cast<std::uint8_t>(chiyuri_phase_ + 4U);
		}
		const auto radius = wrap_word(static_cast<std::int32_t>(radius_pixels) * 16);
		auto angle = chiyuri_phase_;
		const auto source = source_rect(
			owner,
			static_cast<std::uint16_t>(192U + boss.render_frame * 32U),
			32U,
			32U,
			16U
		);
		for(std::uint8_t point = 0U; point < 16U; ++point) {
			const auto x = polar(boss.center_x, radius, trig_.cosine_q8[angle]);
			const auto y = polar(boss.center_y, radius, trig_.sine_q8[angle]);
			if(!put(
				source,
				{
					wrap_word(
						static_cast<std::int32_t>(foreground_x(context, x, target)) - 16
					),
					signed_shift_right(y, 4U),
				},
				clip
			)) {
				return false;
			}
			angle = static_cast<std::uint8_t>(angle + 0x10U);
		}
		return true;
	}
	return fail(MainBossStatus::invalid_observation);
}

bool MainBossPresenter::render_terminal(
	const MainPhaseContext &context,
	const MainBossFact &boss,
	std::uint8_t target,
	Sprite16HorizontalClip clip
) noexcept
{
	const auto source = Sprite16AtlasRect{384U, 80U, 48U, 24U};
	const auto center_x = foreground_x(context, boss.center_x, target);
	const auto center_y = wrap_word(
		static_cast<std::int32_t>(signed_shift_right(boss.center_y, 4U)) + 16
	);
	const auto radius = static_cast<std::int16_t>(boss.pattern_frame * 8U);
	auto angle = static_cast<std::uint8_t>(radius);
	for(std::uint8_t point = 0U; point < 16U; ++point) {
		if(!put(
			source,
			{
				wrap_word(
					static_cast<std::int32_t>(screen_polar(
						center_x, radius, trig_.cosine_q8[angle]
					)) - 24
				),
				wrap_word(
					static_cast<std::int32_t>(screen_polar(
						center_y, radius, trig_.sine_q8[angle]
					)) - 24
				),
			},
			clip
		)) {
			return false;
		}
		angle = static_cast<std::uint8_t>(angle + 0x10U);
	}
	angle = static_cast<std::uint8_t>(0U - static_cast<std::uint8_t>(radius));
	const auto inner_radius = wrap_word(static_cast<std::int32_t>(radius) * 2);
	for(std::uint8_t point = 0U; point < 8U; ++point) {
		if(!put(
			source,
			{
				wrap_word(
					static_cast<std::int32_t>(screen_polar(
						center_x, inner_radius, trig_.cosine_q8[angle]
					)) - 24
				),
				wrap_word(
					static_cast<std::int32_t>(screen_polar(
						center_y, inner_radius, trig_.sine_q8[angle]
					)) - 24
				),
			},
			clip
		)) {
			return false;
		}
		angle = static_cast<std::uint8_t>(angle + 0x20U);
	}
	return true;
}

MainBossStatus MainBossPresenter::status() const noexcept
{
	return status_;
}

bool MainBossPresenter::ready() const noexcept
{
	return status_ == MainBossStatus::ready;
}

} // namespace th03::present
