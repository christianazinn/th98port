#include "input_catalog.hpp"

#include <string_view>

#include <SDL3/SDL_keyboard.h>

#include "th03/platform/sdl_input.hpp"

namespace th03::app::launcher_support {

[[nodiscard]] std::string_view sdl_scancode_name(
	void *,
	std::uint16_t key
) noexcept
{
	if(key == 0U || key >= static_cast<std::uint16_t>(SDL_SCANCODE_COUNT)) {
		return {};
	}
	// Keep the Replay Patch's visible PC-98 vocabulary for native keys with a
	// direct host equivalent. SDL's platform-local names are only a fallback
	// for host-only extensions that could not exist in TH3KEY.CFG.
	switch(static_cast<SDL_Scancode>(key)) {
	case SDL_SCANCODE_1: return "1";
	case SDL_SCANCODE_2: return "2";
	case SDL_SCANCODE_3: return "3";
	case SDL_SCANCODE_4: return "4";
	case SDL_SCANCODE_5: return "5";
	case SDL_SCANCODE_6: return "6";
	case SDL_SCANCODE_7: return "7";
	case SDL_SCANCODE_8: return "8";
	case SDL_SCANCODE_9: return "9";
	case SDL_SCANCODE_0: return "0";
	case SDL_SCANCODE_A: return "A";
	case SDL_SCANCODE_B: return "B";
	case SDL_SCANCODE_C: return "C";
	case SDL_SCANCODE_D: return "D";
	case SDL_SCANCODE_E: return "E";
	case SDL_SCANCODE_F: return "F";
	case SDL_SCANCODE_G: return "G";
	case SDL_SCANCODE_H: return "H";
	case SDL_SCANCODE_I: return "I";
	case SDL_SCANCODE_J: return "J";
	case SDL_SCANCODE_K: return "K";
	case SDL_SCANCODE_L: return "L";
	case SDL_SCANCODE_M: return "M";
	case SDL_SCANCODE_N: return "N";
	case SDL_SCANCODE_O: return "O";
	case SDL_SCANCODE_P: return "P";
	case SDL_SCANCODE_Q: return "Q";
	case SDL_SCANCODE_R: return "R";
	case SDL_SCANCODE_S: return "S";
	case SDL_SCANCODE_T: return "T";
	case SDL_SCANCODE_U: return "U";
	case SDL_SCANCODE_V: return "V";
	case SDL_SCANCODE_W: return "W";
	case SDL_SCANCODE_X: return "X";
	case SDL_SCANCODE_Y: return "Y";
	case SDL_SCANCODE_Z: return "Z";
	case SDL_SCANCODE_MINUS: return "Minus";
	case SDL_SCANCODE_EQUALS: return "Plus";
	case SDL_SCANCODE_BACKSPACE: return "Backspace";
	case SDL_SCANCODE_TAB: return "Tab";
	case SDL_SCANCODE_LEFTBRACKET: return "LBracket";
	case SDL_SCANCODE_RIGHTBRACKET: return "RBracket";
	case SDL_SCANCODE_RETURN: return "Return";
	case SDL_SCANCODE_COMMA: return "Comma";
	case SDL_SCANCODE_PERIOD: return "Period";
	case SDL_SCANCODE_SLASH: return "Slash";
	case SDL_SCANCODE_SPACE: return "Space";
	case SDL_SCANCODE_PAGEUP: return "Roll Up";
	case SDL_SCANCODE_PAGEDOWN: return "Roll Down";
	case SDL_SCANCODE_INSERT: return "Insert";
	case SDL_SCANCODE_DELETE: return "Delete";
	case SDL_SCANCODE_UP: return "Up Arrow";
	case SDL_SCANCODE_LEFT: return "Left Arrow";
	case SDL_SCANCODE_RIGHT: return "Right Arrow";
	case SDL_SCANCODE_DOWN: return "Down Arrow";
	case SDL_SCANCODE_HOME: return "Home/Clear";
	case SDL_SCANCODE_END: return "End";
	case SDL_SCANCODE_KP_0: return "Num 0";
	case SDL_SCANCODE_KP_1: return "Num 1";
	case SDL_SCANCODE_KP_2: return "Num 2";
	case SDL_SCANCODE_KP_3: return "Num 3";
	case SDL_SCANCODE_KP_4: return "Num 4";
	case SDL_SCANCODE_KP_5: return "Num 5";
	case SDL_SCANCODE_KP_6: return "Num 6";
	case SDL_SCANCODE_KP_7: return "Num 7";
	case SDL_SCANCODE_KP_8: return "Num 8";
	case SDL_SCANCODE_KP_9: return "Num 9";
	case SDL_SCANCODE_KP_MINUS: return "Num Minus";
	case SDL_SCANCODE_KP_DIVIDE: return "Num Divide";
	case SDL_SCANCODE_KP_MULTIPLY: return "Num Multiply";
	case SDL_SCANCODE_KP_PLUS: return "Num Plus";
	case SDL_SCANCODE_KP_EQUALS: return "Num Equals";
	case SDL_SCANCODE_KP_COMMA: return "Num Comma";
	case SDL_SCANCODE_KP_PERIOD: return "Num Period";
	case SDL_SCANCODE_F1: return "F1";
	case SDL_SCANCODE_F2: return "F2";
	case SDL_SCANCODE_F3: return "F3";
	case SDL_SCANCODE_F4: return "F4";
	case SDL_SCANCODE_F5: return "F5";
	case SDL_SCANCODE_F6: return "F6";
	case SDL_SCANCODE_F7: return "F7";
	case SDL_SCANCODE_F8: return "F8";
	case SDL_SCANCODE_F9: return "F9";
	case SDL_SCANCODE_F10: return "F10";
	case SDL_SCANCODE_LSHIFT:
	case SDL_SCANCODE_RSHIFT: return "Shift";
	case SDL_SCANCODE_LALT:
	case SDL_SCANCODE_RALT: return "GRPH";
	case SDL_SCANCODE_LCTRL:
	case SDL_SCANCODE_RCTRL: return "Ctrl";
	default: break;
	}
	const auto *name = SDL_GetScancodeName(static_cast<SDL_Scancode>(key));
	return name != nullptr ? std::string_view{name} : std::string_view{};
}

[[nodiscard]] bool sdl_controller_catalog(
	void *context,
	th03::app::NewClassicControllerCatalog &catalog
) noexcept
{
	if(context == nullptr) {
		return false;
	}
	catalog = static_cast<th03::platform::SdlInputMapper *>(context)->
		controller_catalog();
	return true;
}

} // namespace th03::app::launcher_support
