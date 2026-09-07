#include "th03/present/main_effects.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "th03/present/main_static.hpp"
#include "th03/present/vectors.hpp"

namespace th03::present {
namespace {

constexpr std::uint16_t hit_circle_width = 48U;
constexpr std::uint16_t hit_circle_raw_height = 24U;
constexpr std::uint8_t hit_circle_frames_per_cel = 4U;
constexpr std::uint8_t hit_circle_frame_count = 16U;
constexpr std::uint8_t common_effect_frame_count = 16U;
constexpr std::uint8_t common_effect_type_circle = 1U;
constexpr std::uint8_t common_effect_type_square = 2U;
constexpr Index4 enemy_hit_circle_color_base = 11U;
constexpr Index4 player_hit_circle_color = 15U;
constexpr Index4 common_effect_color = 13U;
constexpr std::int16_t playfield_center_x = 144;
constexpr std::int16_t playfield_center_y = 184;

constexpr std::array<Sprite16HorizontalClip, 2U> hit_circle_clips{{
	{0, 319},
	{320, 639},
}};
constexpr std::array<GrcgClipRect, 2U> common_effect_clips{{
	{16, 8, 303, 191},
	{336, 8, 623, 191},
}};

[[nodiscard]] constexpr std::int16_t wrap_word(
	std::int32_t value
) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(value) & 0xFFFFU
	);
	return (bits <= 0x7FFFU) ?
		static_cast<std::int16_t>(bits) :
		static_cast<std::int16_t>(static_cast<std::int32_t>(bits) - 0x10000);
}

[[nodiscard]] constexpr std::int32_t signed_shift_right(
	std::int32_t value,
	unsigned int count
) noexcept
{
	const auto divisor = static_cast<std::int64_t>(1) << count;
	if(value >= 0) {
		return static_cast<std::int32_t>(value / divisor);
	}
	return static_cast<std::int32_t>(
		-(((-static_cast<std::int64_t>(value)) + divisor - 1) / divisor)
	);
}

[[nodiscard]] constexpr std::optional<std::size_t> player_index(
	PlayerId player
) noexcept
{
	if(player == PlayerId::p1) {
		return 0U;
	}
	if(player == PlayerId::p2) {
		return 1U;
	}
	return std::nullopt;
}

[[nodiscard]] constexpr bool valid_rect(Sprite16AtlasRect rect) noexcept
{
	return rect.width == hit_circle_width &&
		rect.raw_height == hit_circle_raw_height &&
		(rect.left % 16U) == 0U &&
		(static_cast<std::uint32_t>(rect.left) + rect.width <=
			sprite16_source_width) &&
		(static_cast<std::uint32_t>(rect.raw_top) + rect.raw_height <=
			sprite16_source_height);
}

[[nodiscard]] constexpr bool valid_layout(
	const MainEffectsAtlasLayout &layout
) noexcept
{
	for(const auto rect : layout.hit_circle_cels) {
		if(!valid_rect(rect)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] constexpr bool valid_trig(
	const StaffFlakeTrigTables &trig
) noexcept
{
	return trig.sine_q8.size() == 256U && trig.cosine_q8.size() == 256U;
}

[[nodiscard]] constexpr bool valid_hit_circle_kind(
	HitCircleKind kind
) noexcept
{
	return kind == HitCircleKind::enemy || kind == HitCircleKind::player;
}

[[nodiscard]] constexpr bool valid_common_effect_kind(
	CommonEffectKind kind
) noexcept
{
	return kind == CommonEffectKind::expanding_circle ||
		kind == CommonEffectKind::contracting_circle ||
		kind == CommonEffectKind::contracting_square;
}

[[nodiscard]] constexpr bool valid_bomb_effect_kind(
	BombCommonEffectKind kind
) noexcept
{
	return kind == BombCommonEffectKind::kana ||
		kind == BombCommonEffectKind::kotohime;
}

[[nodiscard]] constexpr std::int16_t foreground_x(
	std::int16_t center_x,
	std::size_t player,
	std::int16_t shift_x
) noexcept
{
	return wrap_word(
		signed_shift_right(center_x, 4U) +
		static_cast<std::int32_t>(main_playfield_border) +
		static_cast<std::int32_t>(player * main_playfield_bordered_width) +
		shift_x
	);
}

[[nodiscard]] constexpr std::int16_t foreground_y(
	std::int16_t center_y
) noexcept
{
	return wrap_word(
		signed_shift_right(center_y, 4U) +
		static_cast<std::int32_t>(main_playfield_border)
	);
}

[[nodiscard]] constexpr std::int16_t polar(
	std::int16_t center,
	std::int16_t radius,
	std::int16_t ratio
) noexcept
{
	const auto product = static_cast<std::int32_t>(radius) * ratio;
	return wrap_word(
		static_cast<std::int32_t>(center) +
		signed_shift_right(product, 8U)
	);
}

[[nodiscard]] constexpr std::int16_t pixel_to_q12_4(
	std::int16_t pixel
) noexcept
{
	return wrap_word(static_cast<std::int32_t>(pixel) * 16);
}

[[nodiscard]] constexpr std::int16_t half_toward_zero(
	std::int16_t value
) noexcept
{
	return static_cast<std::int16_t>(value / 2);
}

} // namespace

MainEffectsAtlasLayout authentic_main_effects_atlas_layout() noexcept
{
	MainEffectsAtlasLayout layout{};
	for(std::size_t cel = 0U; cel < layout.hit_circle_cels.size(); ++cel) {
		layout.hit_circle_cels[cel] = {
			static_cast<std::uint16_t>(128U + (cel * hit_circle_width)),
			80U,
			hit_circle_width,
			hit_circle_raw_height,
		};
	}
	return layout;
}

MainEffectsPresenter::MainEffectsPresenter(
	Raster &raster,
	const Sprite16Staging &staging,
	MainEffectsAtlasLayout layout,
	StaffFlakeTrigTables trig
) noexcept :
	raster_(&raster),
	staging_(&staging),
	layout_(layout),
	trig_(trig)
{
	if(!valid_layout(layout_)) {
		status_ = MainEffectsStatus::invalid_layout;
		return;
	}
	if(!valid_trig(trig_)) {
		status_ = MainEffectsStatus::invalid_assets;
	}
}

void MainEffectsPresenter::hit_circles_advanced() noexcept
{
	if(!ready()) {
		return;
	}
	for(auto &circle : hit_circles_) {
		if(circle.age == 0U) {
			continue;
		}
		circle.age = static_cast<std::uint8_t>(circle.age + 1U);
		if(circle.age > hit_circle_frame_count) {
			circle.age = 0U;
		}
	}
}

void MainEffectsPresenter::hit_circle_spawned(
	const HitCircleSpawned &spawn
) noexcept
{
	if(!ready()) {
		return;
	}
	const auto player = player_index(spawn.player);
	if(!player.has_value() || !valid_hit_circle_kind(spawn.kind)) {
		status_ = MainEffectsStatus::invalid_observation;
		return;
	}
	std::size_t slot = main_enemy_hit_circle_count;
	if(spawn.kind == HitCircleKind::enemy) {
		enemy_hit_circle_cursor_ = static_cast<std::uint8_t>(
			enemy_hit_circle_cursor_ + 1U
		);
		if(enemy_hit_circle_cursor_ >= main_enemy_hit_circle_count) {
			enemy_hit_circle_cursor_ = 0U;
		}
		slot = enemy_hit_circle_cursor_;
	}
	auto &circle = hit_circles_[slot];
	circle.age = 1U;
	circle.player = spawn.player;
	circle.left = wrap_word(
		static_cast<std::int32_t>(foreground_x(
			spawn.center_x, *player, spawn.fg_shift_x_at_spawn
		)) - static_cast<std::int32_t>(hit_circle_width / 2U)
	);
	circle.top = wrap_word(
		static_cast<std::int32_t>(foreground_y(spawn.center_y)) -
		static_cast<std::int32_t>(hit_circle_width / 2U)
	);
}

void MainEffectsPresenter::common_effects_advanced() noexcept
{
	if(!ready()) {
		return;
	}
	for(auto &effect : common_effects_) {
		if(effect.type == 0U) {
			continue;
		}
		effect.age = static_cast<std::uint8_t>(effect.age + 1U);
		if(effect.age > common_effect_frame_count) {
			effect.type = 0U;
		}
		effect.radius = static_cast<std::uint8_t>(
			static_cast<std::int16_t>(effect.radius) + effect.radius_delta
		);
	}
}

void MainEffectsPresenter::spawn_common_effect(
	CommonEffectKind kind,
	PlayerId player,
	std::int16_t center_x,
	std::int16_t center_y,
	std::int16_t fg_shift_x_at_spawn,
	std::uint8_t initial_angle
) noexcept
{
	const auto player_id = *player_index(player);
	common_effect_cursor_ = static_cast<std::uint8_t>(
		common_effect_cursor_ + 1U
	);
	if(common_effect_cursor_ >= main_common_effect_count) {
		common_effect_cursor_ = 0U;
	}
	auto &effect = common_effects_[common_effect_cursor_];
	effect.age = 0U;
	effect.center_x = foreground_x(
		center_x, player_id, fg_shift_x_at_spawn
	);
	effect.center_y = foreground_y(center_y);
	if(kind == CommonEffectKind::expanding_circle) {
		effect.type = common_effect_type_circle;
		effect.radius = 8U;
		effect.radius_delta = 8;
		return;
	}
	if(kind == CommonEffectKind::contracting_circle) {
		effect.type = common_effect_type_circle;
		effect.radius = 136U;
		effect.radius_delta = -8;
		return;
	}
	effect.type = common_effect_type_square;
	effect.radius = 162U;
	effect.radius_delta = -10;
	effect.angle = initial_angle;
}

void MainEffectsPresenter::common_effect_spawned(
	const CommonEffectSpawned &spawn
) noexcept
{
	if(!ready()) {
		return;
	}
	if(!player_index(spawn.player).has_value() ||
		!valid_common_effect_kind(spawn.kind)) {
		status_ = MainEffectsStatus::invalid_observation;
		return;
	}
	spawn_common_effect(
		spawn.kind,
		spawn.player,
		spawn.center_x,
		spawn.center_y,
		spawn.fg_shift_x_at_spawn,
		spawn.initial_angle
	);
}

void MainEffectsPresenter::bomb_common_effect_step(
	const BombCommonEffectStep &step
) noexcept
{
	if(!ready()) {
		return;
	}
	if(!player_index(step.player).has_value() ||
		!valid_bomb_effect_kind(step.kind)) {
		status_ = MainEffectsStatus::invalid_observation;
		return;
	}

	const auto spawn = [this, &step](
		CommonEffectKind kind,
		std::int16_t center_x,
		std::int16_t center_y
	) noexcept {
		spawn_common_effect(
			kind,
			step.player,
			center_x,
			center_y,
			step.fg_shift_x_at_effects,
			0U
		);
	};

	const auto frame = step.bomb_frame;
	if(step.kind == BombCommonEffectKind::kana) {
		if(frame < 64U) {
			if((frame % 8U) == 0U) {
				spawn(
					CommonEffectKind::contracting_circle,
					pixel_to_q12_4(playfield_center_x),
					pixel_to_q12_4(playfield_center_y)
				);
			}
			// Shared across both players and written on every such callback.
			kana_bomb_angle_ = 0U;
			return;
		}
		if(frame >= 128U || (frame % 4U) != 0U) {
			return;
		}

		const auto spawn_at_angle = [this, &spawn](
			std::uint8_t angle
		) noexcept {
			const auto left = polar(
				playfield_center_x,
				playfield_center_x,
				trig_.cosine_q8[angle]
			);
			const auto top = polar(
				playfield_center_y,
				playfield_center_x,
				trig_.sine_q8[angle]
			);
			spawn(
				CommonEffectKind::expanding_circle,
				pixel_to_q12_4(left),
				pixel_to_q12_4(top)
			);
		};
		spawn_at_angle(kana_bomb_angle_);
		kana_bomb_angle_ = static_cast<std::uint8_t>(
			0x80U - kana_bomb_angle_
		);
		spawn_at_angle(kana_bomb_angle_);
		kana_bomb_angle_ = static_cast<std::uint8_t>(
			0x80U - kana_bomb_angle_
		);
		kana_bomb_angle_ = static_cast<std::uint8_t>(
			kana_bomb_angle_ + 0x10U
		);
		return;
	}

	if(frame < 64U) {
		if((frame % 8U) != 0U) {
			return;
		}
		const auto center_x = wrap_word(
			(288 * 16) - 72 - (static_cast<std::int32_t>(frame) * 0x48)
		);
		kotohime_bomb_y_ = wrap_word(
			(static_cast<std::int32_t>(frame) * 0x5C) + 0x5C
		);
		spawn(CommonEffectKind::expanding_circle, center_x, kotohime_bomb_y_);
		return;
	}
	if(frame >= 128U || (frame % 8U) != 0U) {
		return;
	}
	constexpr std::array<std::int16_t, 4U> centers_x{{
		0,
		96 << 4,
		192 << 4,
		288 << 4,
	}};
	for(const auto center_x : centers_x) {
		spawn(
			CommonEffectKind::expanding_circle,
			center_x,
			kotohime_bomb_y_
		);
	}
	kotohime_bomb_y_ = wrap_word(
		static_cast<std::int32_t>(kotohime_bomb_y_) - (46 * 16)
	);
}

bool MainEffectsPresenter::present_hit_circles(
	const MainPhaseContext &context
) noexcept
{
	if(!ready()) {
		return false;
	}
	if(!staging_->committed()) {
		status_ = MainEffectsStatus::staging_not_committed;
		return false;
	}
	const auto enemy_color = static_cast<Index4>(
		enemy_hit_circle_color_base + (context.round_or_result_frame & 1U)
	);
	for(std::size_t slot = 0U; slot < main_enemy_hit_circle_count; ++slot) {
		const auto &circle = hit_circles_[slot];
		if(circle.age == 0U) {
			continue;
		}
		const auto player = *player_index(circle.player);
		const auto cel = static_cast<std::size_t>(
			(circle.age - 1U) / hit_circle_frames_per_cel
		);
		Sprite16DrawStyle style{};
		style.monochrome_color = enemy_color;
		if(!sprite16_put(
			*raster_,
			*staging_,
			layout_.hit_circle_cels[cel],
			{circle.left, circle.top},
			hit_circle_clips[player],
			style
		)) {
			status_ = MainEffectsStatus::draw_failed;
			return false;
		}
	}

	const auto &player_circle = hit_circles_[main_enemy_hit_circle_count];
	if(player_circle.age != 0U) {
		const auto player = *player_index(player_circle.player);
		const auto cel = static_cast<std::size_t>(
			(player_circle.age - 1U) / hit_circle_frames_per_cel
		);
		Sprite16DrawStyle style{};
		style.monochrome_color = player_hit_circle_color;
		if(!sprite16_put(
			*raster_,
			*staging_,
			layout_.hit_circle_cels[cel],
			{player_circle.left, player_circle.top},
			hit_circle_clips[player],
			style
		)) {
			status_ = MainEffectsStatus::draw_failed;
			return false;
		}
	}
	return true;
}

bool MainEffectsPresenter::present_common_effects(
	const MainPhaseContext &context
) noexcept
{
	(void)context;
	if(!ready()) {
		return false;
	}
	for(std::size_t slot = 0U; slot < common_effects_.size(); ++slot) {
		auto &effect = common_effects_[slot];
		if(effect.type == 0U) {
			continue;
		}
		const auto clip = common_effect_clips[
			(effect.center_x < 320) ? 0U : 1U
		];
		if(effect.type == common_effect_type_circle) {
			if(!grcg_circle(
				*raster_,
				{effect.center_x, wrap_word(signed_shift_right(
					effect.center_y, 1U
				))},
				effect.radius,
				clip,
				common_effect_color
			)) {
				status_ = MainEffectsStatus::draw_failed;
				return false;
			}
			continue;
		}

		if((slot & 1U) == 0U) {
			effect.angle = static_cast<std::uint8_t>(effect.angle + 8U);
		} else {
			effect.angle = static_cast<std::uint8_t>(effect.angle - 8U);
		}
		std::array<GrcgPoint, 4U> vertices{};
		auto angle = effect.angle;
		for(auto &vertex : vertices) {
			vertex.x = polar(
				effect.center_x,
				static_cast<std::int16_t>(effect.radius),
				trig_.cosine_q8[angle]
			);
			vertex.y = half_toward_zero(polar(
				effect.center_y,
				static_cast<std::int16_t>(effect.radius),
				trig_.sine_q8[angle]
			));
			angle = static_cast<std::uint8_t>(angle + 0x40U);
		}
		for(std::size_t edge = 0U; edge < vertices.size(); ++edge) {
			if(!grcg_line(
				*raster_,
				vertices[edge],
				vertices[(edge + 1U) % vertices.size()],
				clip,
				common_effect_color
			)) {
				status_ = MainEffectsStatus::draw_failed;
				return false;
			}
		}
	}
	return true;
}

MainEffectsStatus MainEffectsPresenter::status() const noexcept
{
	return status_;
}

bool MainEffectsPresenter::ready() const noexcept
{
	return status_ == MainEffectsStatus::ready;
}

} // namespace th03::present
