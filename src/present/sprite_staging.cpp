#include "th03/present/sprite_staging.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

#include "th03/present/main_static.hpp"

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

} // namespace

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
