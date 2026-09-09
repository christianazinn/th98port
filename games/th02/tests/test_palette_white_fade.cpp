#include "th02/core/palette_white_fade.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>

using namespace th02::core;

namespace {
void require(bool condition) { if (!condition) std::abort(); }
struct Trace final : PaletteWhiteFadePresentation {
	std::uint16_t &tone;
	std::array<std::uint16_t, 18> values{};
	std::size_t count{};
	explicit Trace(std::uint16_t &value) : tone(value) {}
	void palette_show() noexcept override {
		require(count < values.size());
		values[count++] = tone;
	}
};
}

int main()
{
	for (const auto direction : {PaletteWhiteFadeDirection::out, PaletteWhiteFadeDirection::in}) {
		for (const std::int16_t speed : std::array<std::int16_t, 5>{-32768, -1, 0, 1, 3}) {
			std::uint16_t tone = 42;
			Trace trace(tone);
			PaletteWhiteFadeState fade;
			palette_white_fade_after_vsync(fade, tone, trace);
			require(tone == 42 && trace.count == 0);
			palette_white_fade_begin(fade, direction, speed, tone);
			require(trace.count == 0 && fade.phase == PaletteWhiteFadePhase::synchronize);
			int waits = 0;
			while (fade.phase != PaletteWhiteFadePhase::complete) {
				palette_white_fade_after_vsync(fade, tone, trace);
				++waits;
				require(waits <= 52);
			}
			require(waits == 1 + (speed > 0 ? 17 * speed : 0));
			require(trace.count == 18);
			for (std::size_t i = 0; i < 17; ++i) {
				const auto expected = direction == PaletteWhiteFadeDirection::out ? 100 + 6 * i : 200 - 6 * i;
				require(trace.values[i] == expected);
			}
			require(trace.values[17] == (direction == PaletteWhiteFadeDirection::out ? 200 : 100));
			palette_white_fade_after_vsync(fade, tone, trace);
			require(trace.count == 18);
		}
	}
	std::puts("palette white fade source timing passed");
}
