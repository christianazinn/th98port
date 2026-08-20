#ifndef TH03_APP_UPDATE_UI_HPP
#define TH03_APP_UPDATE_UI_HPP

#include <cstdint>

namespace th03::app {

// Host-owned updater results projected back into the retained New Classic
// Options owner. Remote text, versions, URLs, and errors never cross this UI
// boundary.
enum class NewClassicUpdateCheckOutcome : std::uint8_t {
	up_to_date,
	restart_to_apply,
	failed,
};

} // namespace th03::app

#endif
