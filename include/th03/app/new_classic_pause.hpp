#ifndef TH03_APP_NEW_CLASSIC_PAUSE_HPP
#define TH03_APP_NEW_CLASSIC_PAUSE_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "th03/core/game.hpp"
#include "th03/present/source_surface.hpp"

namespace th03::app {

struct NewClassicReplayPatchAssets;

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
	explicit NewClassicPauseMenu(
		bool save_enabled,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets = {}
	) noexcept;

	[[nodiscard]] NewClassicPauseStatus consume(
		NewClassicPauseInput input
	) noexcept;
	[[nodiscard]] present::SourceSurface compose_over(
		present::SourceSurface base
	) noexcept;

	[[nodiscard]] NewClassicPauseChoice choice() const noexcept;
	[[nodiscard]] bool save_enabled() const noexcept;
	[[nodiscard]] bool take_rejected_save_effect() noexcept;

private:
	friend struct NewClassicPauseTestAccess;
	enum class Phase : std::uint8_t {
		release_before_choice,
		wait_for_choice,
		release_after_resume,
	};

	void move(int delta) noexcept;
	void draw() noexcept;
	void fill_text_black(
		std::size_t left,
		std::size_t top,
		std::size_t width,
		std::size_t height
	) noexcept;
	void put_gaiji(
		std::size_t cell_left,
		std::size_t cell_top,
		std::size_t tile,
		std::size_t count,
		present::Index4 graphics_color
	) noexcept;

	bool save_enabled_ = false;
	NewClassicPauseChoice choice_ = NewClassicPauseChoice::resume;
	NewClassicPauseStatus status_ = NewClassicPauseStatus::running;
	Phase phase_ = Phase::release_before_choice;
	bool visible_ = false;
	bool rejected_save_effect_ = false;
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_{};
	std::vector<present::ComposedPixel> pixels_{};
};

struct NewClassicPauseTestAccess final {
	static void select(
		NewClassicPauseMenu &menu,
		NewClassicPauseChoice choice
	) noexcept
	{
		menu.choice_ = choice;
	}
};

} // namespace th03::app

#endif
