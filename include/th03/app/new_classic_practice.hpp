#ifndef TH03_APP_NEW_CLASSIC_PRACTICE_HPP
#define TH03_APP_NEW_CLASSIC_PRACTICE_HPP

#include <cstdint>
#include <expected>

#include "th03/app/host_menu_canvas.hpp"
#include "th03/core/game.hpp"
#include "th03/formats/replay.hpp"

namespace th03::app {

enum class NewClassicPracticeStatus : std::uint8_t {
	running,
	started,
	canceled,
};

struct NewClassicPracticeInput final {
	bool raw_any = false;
	bool up = false;
	bool down = false;
	bool left = false;
	bool right = false;
	bool confirm = false;
	bool cancel = false;
};

// True only for the exact configuration for which Replay Patch clears its
// Practice resident block and starts an ordinary VS game.
[[nodiscard]] bool new_classic_practice_is_exact_vs_default(
	const formats::ReplayPracticeConfig &config,
	std::uint8_t rank
) noexcept;

// Build the patch's post-fresh-initialization Practice checkpoint. The result
// is accepted by the same Game factory used for playback and carries no
// presentation or filesystem state.
[[nodiscard]] std::expected<GameInit, Error>
prepare_new_classic_practice_init(
	GameInit selected_resident,
	const formats::ReplayPracticeConfig &config,
	const GameContent &content
);

// Replay Patch 0.4.10's two-page Practice setup. It is host-rendered over the
// completed authentic selector, but every field and step rule remains source-
// shaped and the output is the frozen V13 Practice configuration.
class NewClassicPracticeMenu final {
public:
	explicit NewClassicPracticeMenu(GameInit selected_resident) noexcept;

	[[nodiscard]] NewClassicPracticeStatus consume(
		NewClassicPracticeInput input
	) noexcept;
	[[nodiscard]] present::SourceSurface compose_over(
		present::SourceSurface base
	) noexcept;

	[[nodiscard]] NewClassicPracticeStatus status() const noexcept;
	[[nodiscard]] const formats::ReplayPracticeConfig &config() const noexcept;
	[[nodiscard]] bool practice_active() const noexcept;

private:
	enum class Page : std::uint8_t { main, advanced };

	void reset_defaults() noexcept;
	void step_value(bool forward) noexcept;
	void draw() noexcept;
	void refresh_safety() noexcept;
	[[nodiscard]] std::uint8_t fixed_stage() const noexcept;
	[[nodiscard]] std::uint8_t row_count() const noexcept;

	GameInit selected_resident_{};
	formats::ReplayPracticeConfig config_{};
	Page page_ = Page::main;
	std::uint8_t selected_ = 0U;
	bool input_locked_ = true;
	NewClassicPracticeStatus status_ = NewClassicPracticeStatus::running;
	HostMenuCanvas canvas_{};
};

} // namespace th03::app

#endif
