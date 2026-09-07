#include "th03/present/main_players_bullets.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "th03/present/main_static.hpp"

namespace th03::present {
namespace {

constexpr std::uint16_t bullet16_size = 16U;
constexpr std::uint16_t cloud_size = 32U;
constexpr std::uint8_t cloud_frames_per_cel = 4U;
constexpr std::uint8_t ordinary_pellet_flag = 1U;
constexpr Index4 main_white = 15U;
constexpr std::array<std::size_t, 3U> trail_point_order{5U, 3U, 1U};
constexpr std::array<std::size_t, 3U> trail_cel_order{3U, 2U, 1U};
constexpr Sprite16HorizontalClip full_clip{
	0,
	static_cast<std::int32_t>(graphics_width - 1U),
};

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

[[nodiscard]] constexpr bool valid_construction(
	Bullet16ConstructionKind construction
) noexcept
{
	return construction == Bullet16ConstructionKind::default_attack ||
		construction == Bullet16ConstructionKind::kana_extra_attack;
}

[[nodiscard]] constexpr bool is_kana(
	MainPalettedCharacter character
) noexcept
{
	return character == MainPalettedCharacter::kana_primary ||
		character == MainPalettedCharacter::kana_alternate;
}

[[nodiscard]] constexpr std::int32_t signed_shift_right(
	std::int32_t value,
	unsigned int bits
) noexcept
{
	const auto divisor = static_cast<std::int32_t>(1U << bits);
	auto result = value / divisor;
	if(value < 0 && (value % divisor) != 0) {
		--result;
	}
	return result;
}

[[nodiscard]] constexpr std::int32_t wrap_signed_word(
	std::int32_t value
) noexcept
{
	const auto wrapped = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(value) & 0xFFFFU
	);
	return (wrapped <= 0x7FFFU) ?
		static_cast<std::int32_t>(wrapped) :
		(static_cast<std::int32_t>(wrapped) - 0x10000);
}

[[nodiscard]] constexpr std::int32_t foreground_x(
	const MainPhaseContext &context,
	std::int16_t raw_x,
	std::size_t player
) noexcept
{
	return wrap_signed_word(
		signed_shift_right(raw_x, 4U) +
		static_cast<std::int32_t>(main_playfield_border) +
		static_cast<std::int32_t>(player * main_playfield_bordered_width) +
		context.playfield_fg_shift_x[player]
	);
}

[[nodiscard]] constexpr Point centered_position(
	const MainPhaseContext &context,
	std::int16_t raw_x,
	std::int16_t raw_y,
	std::size_t player,
	std::uint16_t size
) noexcept
{
	const auto radius = static_cast<std::int32_t>(size / 2U);
	return {
		wrap_signed_word(foreground_x(context, raw_x, player) - radius),
		wrap_signed_word(
			signed_shift_right(raw_y, 4U) +
			static_cast<std::int32_t>(main_playfield_top) - radius
		),
	};
}

} // namespace

void MainPlayersBulletsPresenter::bullet16_spawned(
	const Bullet16Spawned &spawn
) noexcept
{
	if(!ready()) {
		return;
	}
	const auto slot = static_cast<std::size_t>(spawn.slot);
	const auto source_player = player_index(spawn.source_player);
	if(slot >= bullet16_styles_.size() || !source_player.has_value() ||
		!valid_construction(spawn.construction)) {
		status_ = MainPlayersBulletsStatus::invalid_observation;
		return;
	}
	if(spawn.construction == Bullet16ConstructionKind::kana_extra_attack &&
		!is_kana(loaded_characters_[*source_player])) {
		status_ = MainPlayersBulletsStatus::invalid_observation;
		return;
	}
	bullet16_styles_[slot] = {
		.source_player = spawn.source_player,
		.construction = spawn.construction,
		.animated = spawn.source_template_animation_enabled,
		.observed = true,
	};
}

bool MainPlayersBulletsPresenter::validate_bullets(
	const MainBulletFacts &bullets,
	const MainBulletTrails &
) noexcept
{
	for(std::size_t slot = 0U; slot < bullets.size(); ++slot) {
		const auto &bullet = bullets[slot];
		if(static_cast<std::size_t>(bullet.slot) != slot) {
			status_ = MainPlayersBulletsStatus::invalid_observation;
			return false;
		}
		const bool rendered = bullet.flag.is_bullet16() ||
			bullet.flag.is_pellet_cloud() ||
			bullet.flag.is_rendered_as_pellet();
		if(rendered && !player_index(bullet.player_id).has_value()) {
			status_ = MainPlayersBulletsStatus::invalid_observation;
			return false;
		}
		if(bullet.flag.is_bullet16() && !bullet16_styles_[slot].observed) {
			status_ = MainPlayersBulletsStatus::invalid_observation;
			return false;
		}
		if(bullet.flag.is_pellet_cloud() &&
			(bullet.age / cloud_frames_per_cel) >=
				main_pellet_cloud_cel_count) {
			status_ = MainPlayersBulletsStatus::invalid_observation;
			return false;
		}
	}
	// Retire style only after the complete phase validates. A spawn that is
	// killed before this final view must not authorize a later slot reuse.
	for(std::size_t slot = 0U; slot < bullets.size(); ++slot) {
		if(!bullets[slot].flag.is_bullet16()) {
			bullet16_styles_[slot].observed = false;
		}
	}
	return true;
}

bool MainPlayersBulletsPresenter::draw_bullets(
	const MainPhaseContext &context,
	const MainBulletFacts &bullets,
	const MainBulletTrails &trails
) noexcept
{
	// bullets_render() first walks every Bullet16 in ascending slot order.
	for(std::size_t slot = 0U; slot < bullets.size(); ++slot) {
		const auto &bullet = bullets[slot];
		if(!bullet.flag.is_bullet16()) {
			continue;
		}
		const auto player = *player_index(bullet.player_id);
		const auto &style = bullet16_styles_[slot];
		const auto source_player = *player_index(style.source_player);
		const auto &cels =
			(style.construction == Bullet16ConstructionKind::default_attack) ?
				layout_.default_bullet16_cels[source_player] :
				layout_.kana_bullet16_cels[source_player];
		if(bullet.has_trail) {
			for(std::size_t trail = 0U; trail < trail_point_order.size(); ++trail) {
				const auto point = trails[slot][trail_point_order[trail]];
				if(!sprite16_put_noclip(
					*raster_,
					*staging_,
					cels[trail_cel_order[trail]],
					centered_position(
						context,
						point.x,
						point.y,
						player,
						bullet16_size
					)
				)) {
					return false;
				}
			}
		}
		const auto main_cel = static_cast<std::size_t>(
			style.animated ? (bullet.age % main_bullet16_cel_count) : 0U
		);
		if(!sprite16_put_noclip(
			*raster_,
			*staging_,
			cels[main_cel],
			centered_position(
				context,
				bullet.center_x,
				bullet.center_y,
				player,
				bullet16_size
			)
		)) {
			return false;
		}
	}

	// The source's optional second pass draws all delay clouds in white.
	for(const auto &bullet : bullets) {
		if(!bullet.flag.is_pellet_cloud()) {
			continue;
		}
		const auto player = *player_index(bullet.player_id);
		const auto cel = static_cast<std::size_t>(
			bullet.age / cloud_frames_per_cel
		);
		if(!sprite16_put(
			*raster_,
			*staging_,
			layout_.pellet_cloud_cels[cel],
			centered_position(
				context,
				bullet.center_x,
				bullet.center_y,
				player,
				cloud_size
			),
			full_clip,
			Sprite16DrawStyle{.monochrome_color = main_white}
		)) {
			return false;
		}
	}

	// The final source pass treats every odd raw flag as a pellet. Only exact
	// flag 1 selects the ordinary token; every other odd value transfers.
	for(const auto &bullet : bullets) {
		if(!bullet.flag.is_rendered_as_pellet()) {
			continue;
		}
		const auto player = *player_index(bullet.player_id);
		const auto cel = (bullet.flag.raw == ordinary_pellet_flag) ?
			PelletCel::normal : PelletCel::transfer;
		const auto left = wrap_signed_word(
			foreground_x(context, bullet.center_x, player) - 4
		);
		const auto top = wrap_signed_word(
			signed_shift_right(bullet.center_y, 5U) + 6
		);
		if(!pellet_put(
			*raster_, left, top, pellet_masks_, cel, main_white
		)) {
			return false;
		}
	}
	return true;
}

} // namespace th03::present
