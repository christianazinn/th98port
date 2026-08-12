#ifndef TH03_APP_NEW_CLASSIC_PAUSE_HPP
#define TH03_APP_NEW_CLASSIC_PAUSE_HPP

#include <cstdint>

#include "th03/app/host_menu_canvas.hpp"
#include "th03/core/game.hpp"

namespace th03::app {

enum class NewClassicPauseChoice : std::uint8_t {
	resume,
	restart,
	save_exit,
	discard_exit,
};

enum class NewClassicPauseStatus : std::uint8_t {
	running,
	resume,
	restart,
	save_exit,
	discard_exit,
	application_quit,
};

struct NewClassicPauseInput final {
	bool up = false;
	bool down = false;
	bool confirm = false;
	bool cancel = false;
	bool quit = false;
	bool restart = false;
	bool raw_any = false;
	bool application_quit = false;
};

[[nodiscard]] NewClassicPauseInput project_new_classic_pause_input(
	CoreInput input,
	bool restart_requested = false,
	bool application_quit_requested = false
) noexcept;

// Application owner for the Replay Patch pause loop. The deterministic MAIN
// frame remains deferred while this state machine consumes interstitial
// samples; callers complete that frame exactly once after a terminal result.
class NewClassicPauseMenu final {
public:
	explicit NewClassicPauseMenu(bool save_enabled) noexcept;

	[[nodiscard]] NewClassicPauseStatus consume(
		NewClassicPauseInput input
	) noexcept;
	[[nodiscard]] present::SourceSurface compose_over(
		present::SourceSurface base
	) noexcept;

	[[nodiscard]] NewClassicPauseChoice choice() const noexcept;
	[[nodiscard]] bool save_enabled() const noexcept;

private:
	enum class Phase : std::uint8_t {
		release_before_choice,
		wait_for_choice,
		release_after_resume,
	};

	void move(int delta) noexcept;
	void draw() noexcept;

	bool save_enabled_ = false;
	NewClassicPauseChoice choice_ = NewClassicPauseChoice::resume;
	NewClassicPauseStatus status_ = NewClassicPauseStatus::running;
	Phase phase_ = Phase::release_before_choice;
	HostMenuCanvas canvas_{};
};

} // namespace th03::app

#endif
