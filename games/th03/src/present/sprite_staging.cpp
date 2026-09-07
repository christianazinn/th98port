#include "th03/present/sprite_staging.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

#include "th03/present/main_static.hpp"
#include "th03/present/pi.hpp"

namespace th03::present {
namespace {

[[nodiscard]] BitSource alpha_source(
	std::span<const std::uint8_t> bits
) noexcept
{
	return {
		bits,
		sprite16_source_width,
		sprite16_source_height,
		sprite16_source_stride,
		PackedRowOrder::top_to_bottom,
	};
}

[[nodiscard]] bool valid_pi_pixels(const assets::PiImage &image) noexcept
{
	const auto expected_size = static_cast<std::size_t>(image.image.width) *
		image.image.height;
	if(image.image.width == 0U || image.image.height == 0U ||
		image.image.indices.size() != expected_size) {
		return false;
	}
	for(const auto color : image.image.indices) {
		if(color > 0x0FU) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool valid_pi_geometry(
	const assets::PiImage &image,
	std::uint16_t width,
	std::uint16_t height
) noexcept
{
	return image.image.width == width && image.image.height == height &&
		valid_pi_pixels(image);
}

[[nodiscard]] bool valid_extra_pi(const assets::PiImage &image) noexcept
{
	return image.image.width == 320U &&
		(image.image.height == 128U || image.image.height == 144U) &&
		valid_pi_pixels(image);
}

} // namespace

bool main_prepare_sprite16_pi_source_sheet(
	Raster &raster,
	MainSprite16PiAssets assets
)
{
	for(std::size_t player = 0U; player < main_sprite16_pi_player_count; ++player) {
		if(assets.player_shots[player] == nullptr ||
			assets.player_extra_shots[player] == nullptr ||
			!valid_pi_geometry(*assets.player_shots[player], 320U, 16U) ||
			!valid_extra_pi(*assets.player_extra_shots[player])) {
			return false;
		}
	}
	if(assets.enemy_center == nullptr || assets.opponent_enemies == nullptr ||
		!valid_pi_geometry(*assets.enemy_center, 640U, 48U) ||
		!valid_pi_geometry(*assets.opponent_enemies, 640U, 192U)) {
		return false;
	}

	if(!raster.set_access_page(Page::one) || !raster.clear(
		{
			0,
			0,
			sprite16_source_width,
			graphics_height,
		},
		0U
	)) {
		return false;
	}
	for(std::size_t player = 0U; player < main_sprite16_pi_player_count; ++player) {
		const auto left = static_cast<std::int32_t>(player * 320U);
		if(!pi_put_lineskip_8(
			raster,
			left,
			200,
			*assets.player_shots[player]
		) || !pi_put_lineskip_8(
			raster,
			left,
			208,
			*assets.player_extra_shots[player]
		)) {
			return false;
		}
	}
	return pi_put_lineskip_8(raster, 0, 280, *assets.enemy_center) &&
		pi_put_lineskip_8(raster, 0, 304, *assets.opponent_enemies);
}

bool Sprite16Staging::stage_main_shots(
	Raster &raster,
	MainPlayerShotSheets sheets
)
{
	if(!main_prepare_shot_source_sheet(raster, sheets)) {
		return false;
	}
	committed_ = false;
	return true;
}

bool Sprite16Staging::commit(Raster &raster)
{
	if(!raster.copy_page(
		Page::zero,
		{
			0,
			sprite16_source_top,
			sprite16_source_width,
			sprite16_source_height,
		},
		Page::one,
		{0, sprite16_source_top}
	) || !raster.set_access_page(Page::zero)) {
		return false;
	}

	committed_ = false;
	positive_alpha_.fill(0U);
	negative_alpha_.fill(0U);
	const auto source_page = raster.graphics(Page::zero);
	for(std::size_t y = 0U; y < sprite16_source_height; ++y) {
		const auto source_row =
			(y + sprite16_source_top) * sprite16_source_width;
		const auto mask_row = y * sprite16_source_stride;
		for(std::size_t byte_x = 0U; byte_x < sprite16_source_stride; ++byte_x) {
			std::uint8_t opaque = 0U;
			const auto pixel_x = byte_x * 8U;
			for(std::size_t bit = 0U; bit < 8U; ++bit) {
				if(source_page[source_row + pixel_x + bit] != 0U) {
					opaque = static_cast<std::uint8_t>(
						opaque | (0x80U >> bit)
					);
				}
			}
			const auto offset = mask_row + byte_x;
			positive_alpha_[offset] = opaque;
			negative_alpha_[offset] = static_cast<std::uint8_t>(~opaque);
		}
	}

	committed_ = true;
	return true;
}

bool Sprite16Staging::committed() const noexcept
{
	return committed_;
}

std::optional<BitSource> Sprite16Staging::positive_alpha() const noexcept
{
	if(!committed_) {
		return std::nullopt;
	}
	return alpha_source(positive_alpha_);
}

std::optional<BitSource> Sprite16Staging::negative_alpha() const noexcept
{
	if(!committed_) {
		return std::nullopt;
	}
	return alpha_source(negative_alpha_);
}

} // namespace th03::present
