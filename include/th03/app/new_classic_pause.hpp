#ifndef TH03_APP_NEW_CLASSIC_PAUSE_HPP
#define TH03_APP_NEW_CLASSIC_PAUSE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "th03/core/game.hpp"
#include "th03/present/source_surface.hpp"

namespace th03::app {

struct NewClassicReplayPatchAssets;

struct NewClassicPauseNetplayText final {
	static constexpr std::size_t width = 256U;
	static constexpr std::size_t height = 16U;
	// The mask starts at the modal's x=192. Replay Patch pause choices start
	// at cell 28 (x=224), leaving this exact 32-pixel inset after the cursor.
	static constexpr std::size_t choice_text_left = 32U;
	using Mask = std::array<std::uint8_t, width * height>;
	Mask forfeit{};
	Mask waiting_for_opponent{};
	std::array<Mask, 3U> resume_countdown{};
};

inline constexpr std::uint64_t new_classic_netplay_resume_countdown_second_ns =
	1'000'000'000U;
inline constexpr std::uint8_t new_classic_netplay_resume_countdown_seconds = 3U;

[[nodiscard]] constexpr std::optional<std::uint8_t>
new_classic_netplay_resume_countdown_remaining(
	std::uint64_t elapsed_ns
) noexcept
{
	const auto elapsed_seconds =
		elapsed_ns / new_classic_netplay_resume_countdown_second_ns;
	if(elapsed_seconds >= new_classic_netplay_resume_countdown_seconds) {
		return std::nullopt;
	}
	return static_cast<std::uint8_t>(
		new_classic_netplay_resume_countdown_seconds - elapsed_seconds
	);
}

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

enum class NewClassicPausePresentation : std::uint8_t {
	replay_patch,
	netplay_wait,
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

// Netplay intentionally gives each machine its ordinary local Story controls
// in P1 while Esc/Q remain source-global system actions.
[[nodiscard]] NewClassicPauseInput project_new_classic_netplay_pause_input(
	CoreInput input,
	bool application_quit_requested = false
) noexcept;

// Application owner for the Replay Patch pause loop. The deterministic MAIN
// frame remains deferred while this state machine consumes interstitial
// samples; callers complete that frame exactly once after a terminal result.
class NewClassicPauseMenu final {
public:
	explicit NewClassicPauseMenu(
		bool save_enabled,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets = {},
		NewClassicPausePresentation presentation =
			NewClassicPausePresentation::replay_patch,
		std::shared_ptr<const NewClassicPauseNetplayText> netplay_text = {}
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
	void set_resume_countdown(std::optional<std::uint8_t> seconds) noexcept;
	[[nodiscard]] std::optional<std::uint8_t>
	resume_countdown() const noexcept;
	void set_waiting_for_opponent(bool waiting) noexcept;
	[[nodiscard]] bool waiting_for_opponent() const noexcept;

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
	void put_menu_text_mask(
		std::int32_t top,
		const NewClassicPauseNetplayText::Mask &mask,
		present::Index4 graphics_color
	) noexcept;

	bool save_enabled_ = false;
	NewClassicPauseChoice choice_ = NewClassicPauseChoice::resume;
	NewClassicPauseStatus status_ = NewClassicPauseStatus::running;
	Phase phase_ = Phase::release_before_choice;
	bool visible_ = false;
	bool rejected_save_effect_ = false;
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_{};
	NewClassicPausePresentation presentation_ =
		NewClassicPausePresentation::replay_patch;
	std::shared_ptr<const NewClassicPauseNetplayText> netplay_text_{};
	std::optional<std::uint8_t> resume_countdown_{};
	bool waiting_for_opponent_ = false;
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
