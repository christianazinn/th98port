#ifndef TH03_APP_NEW_CLASSIC_PRACTICE_HPP
#define TH03_APP_NEW_CLASSIC_PRACTICE_HPP

#include <cstdint>
#include <expected>
#include <memory>
#include <vector>

#include "th03/app/new_classic_assets.hpp"
#include "th03/core/game.hpp"
#include "th03/formats/replay.hpp"

namespace th03::app {

enum class NewClassicPracticeStatus : std::uint8_t {
	running,
	cancel_frame,
	cancel_wait_release,
	start_fade,
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

// Replay Patch 0.4.12's two-page Practice setup. The patch-enabled constructor
// draws the exact MNUFONT overlay over the selector-owned curves-only frame.
// The one-argument constructor remains available for model-only tests and
// returns an unchanged base surface.
class NewClassicPracticeMenu final {
public:
	explicit NewClassicPracticeMenu(GameInit selected_resident) noexcept;
	NewClassicPracticeMenu(
		GameInit selected_resident,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets
	) noexcept;

	[[nodiscard]] NewClassicPracticeStatus consume(
		NewClassicPracticeInput input
	) noexcept;
	void mark_background_published() noexcept;
	[[nodiscard]] present::SourceSurface compose_over(
		present::SourceSurface base,
		bool base_is_retained_selection_frame = false
	) noexcept;

	[[nodiscard]] NewClassicPracticeStatus status() const noexcept;
	[[nodiscard]] const formats::ReplayPracticeConfig &config() const noexcept;
	[[nodiscard]] bool practice_active() const noexcept;
	[[nodiscard]] bool has_patch_assets() const noexcept;
	[[nodiscard]] bool consume_surface_changed() const noexcept;

private:
	enum class Page : std::uint8_t { main, advanced };

	void reset_defaults() noexcept;
	void step_value(bool forward) noexcept;
	void advance_start_fade() noexcept;
	[[nodiscard]] bool draw() noexcept;
	void refresh_safety() noexcept;
	[[nodiscard]] std::uint8_t fixed_stage() const noexcept;
	[[nodiscard]] std::uint8_t row_count() const noexcept;
	[[nodiscard]] bool row_enabled(std::uint8_t row) const noexcept;

	GameInit selected_resident_{};
	formats::ReplayPracticeConfig config_{};
	Page page_ = Page::main;
	std::uint8_t selected_ = 0U;
	bool input_locked_ = true;
	NewClassicPracticeStatus status_ = NewClassicPracticeStatus::running;
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_{};
	present::Raster overlay_raster_{};
	std::vector<present::ComposedPixel> composed_pixels_{};
	present::Page published_page_ = present::Page::zero;
	bool published_page_initialized_ = false;
	bool background_published_ = false;
	bool cancel_release_first_poll_ = false;
	bool consume_surface_changed_ = false;
	std::uint8_t fade_boundary_ = 0U;
	std::int16_t palette_tone_ = 100;
};

} // namespace th03::app

#endif
