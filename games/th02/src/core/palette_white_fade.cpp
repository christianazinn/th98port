#include "th02/core/palette_white_fade.hpp"

namespace th02::core {
namespace {

bool step(PaletteWhiteFadeState &fade, std::uint16_t &tone,
	PaletteWhiteFadePresentation &presentation) noexcept
{
	if (fade.direction == PaletteWhiteFadeDirection::out) {
		tone = static_cast<std::uint16_t>(tone + 6);
		if (tone < 200) return false;
		tone = 200;
	} else {
		tone = static_cast<std::uint16_t>(tone - 6);
		if (tone > 100) return false;
		tone = 100;
	}
	presentation.palette_show();
	fade.phase = PaletteWhiteFadePhase::complete;
	return true;
}

} // namespace

void palette_white_fade_begin(PaletteWhiteFadeState &fade,
	PaletteWhiteFadeDirection direction, std::int16_t speed, std::uint16_t &tone) noexcept
{
	fade = {PaletteWhiteFadePhase::synchronize, direction, speed, 0};
	tone = direction == PaletteWhiteFadeDirection::out ? 100 : 200;
}

void palette_white_fade_after_vsync(PaletteWhiteFadeState &fade,
	std::uint16_t &tone, PaletteWhiteFadePresentation &presentation) noexcept
{
	if (fade.phase == PaletteWhiteFadePhase::delay) {
		if (--fade.remaining != 0) return;
		if (step(fade, tone, presentation)) return;
	} else if (fade.phase != PaletteWhiteFadePhase::synchronize) {
		return;
	}
	for (;;) {
		presentation.palette_show();
		if (fade.speed > 0) {
			fade.remaining = static_cast<std::uint16_t>(fade.speed);
			fade.phase = PaletteWhiteFadePhase::delay;
			return;
		}
		if (step(fade, tone, presentation)) return;
	}
}

} // namespace th02::core
