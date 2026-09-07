#include "th03/present/main_palette.hpp"

namespace th03::present {

bool MainPaletteState::capture_baseline(const Raster &raster) noexcept
{
	if(baseline_ready_) {
		return false;
	}
	baseline_ = raster.pending_palette();
	baseline_ready_ = true;
	return true;
}

bool MainPaletteState::baseline_ready() const noexcept
{
	return baseline_ready_;
}

bool MainPaletteState::set_entry_deferred(
	Raster &raster,
	std::uint8_t index,
	Rgb8 color
) noexcept
{
	if(!raster.set_pending_palette_entry_rgb8(index, color)) {
		return false;
	}
	dirty_ = true;
	return true;
}

bool MainPaletteState::restore_entry_deferred(
	Raster &raster,
	std::uint8_t index
) noexcept
{
	if(!baseline_ready_ || index >= baseline_.colors.size()) {
		return false;
	}
	return set_entry_deferred(raster, index, baseline_.colors[index]);
}

void MainPaletteState::set_tone_deferred(
	Raster &raster,
	std::int16_t tone
) noexcept
{
	raster.set_palette_tone(tone);
	dirty_ = true;
}

bool MainPaletteState::restore_baseline_and_show(Raster &raster) noexcept
{
	if(!baseline_ready_) {
		return false;
	}
	for(std::size_t index = 0U; index < baseline_.colors.size(); ++index) {
		const auto restored = raster.set_pending_palette_entry_rgb8(
			static_cast<std::uint8_t>(index), baseline_.colors[index]
		);
		if(!restored) {
			return false;
		}
	}
	show_now(raster);
	return true;
}

void MainPaletteState::restore_accelerator_palette(
	Raster &raster,
	const Palette16Rgb8 &palette
) noexcept
{
	baseline_ = palette;
	baseline_ready_ = true;
	for(std::size_t index = 0U; index < palette.colors.size(); ++index) {
		(void)raster.set_pending_palette_entry_rgb8(
			static_cast<std::uint8_t>(index), palette.colors[index]
		);
	}
	raster.set_palette_tone(100);
	raster.apply_pending_palette();
	dirty_ = false;
}

void MainPaletteState::set_tone_and_show_now(
	Raster &raster,
	std::int16_t tone
) noexcept
{
	raster.set_palette_tone(tone);
	show_now(raster);
}

void MainPaletteState::show_now(Raster &raster) noexcept
{
	raster.apply_pending_palette();
}

bool MainPaletteState::apply_if_changed(Raster &raster) noexcept
{
	if(!dirty_) {
		return false;
	}
	show_now(raster);
	dirty_ = false;
	return true;
}

bool MainPaletteState::dirty() const noexcept
{
	return dirty_;
}

} // namespace th03::present
