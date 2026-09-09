#pragma once

#include <cstdint>

namespace th02::core {

enum class PaletteWhiteFadeDirection : std::uint8_t { out, in };
enum class PaletteWhiteFadePhase : std::uint8_t { idle, synchronize, delay, complete };
struct PaletteWhiteFadeState {
	PaletteWhiteFadePhase phase{PaletteWhiteFadePhase::idle};
	PaletteWhiteFadeDirection direction{};
	std::int16_t speed{};
	std::uint16_t remaining{};
};
class PaletteWhiteFadePresentation {
public:
	virtual ~PaletteWhiteFadePresentation() = default;
	virtual void palette_show() noexcept = 0;
};

// master.lib palette_white_{out,in}: the initial sync occurs even at speed <= 0.
// Each after_vsync completes exactly one requested wait, not a gameplay tick.
void palette_white_fade_begin(PaletteWhiteFadeState &fade,
	PaletteWhiteFadeDirection direction, std::int16_t speed, std::uint16_t &tone) noexcept;
void palette_white_fade_after_vsync(PaletteWhiteFadeState &fade,
	std::uint16_t &tone, PaletteWhiteFadePresentation &presentation) noexcept;

} // namespace th02::core
