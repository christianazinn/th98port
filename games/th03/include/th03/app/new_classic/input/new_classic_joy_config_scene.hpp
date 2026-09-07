#ifndef TH03_APP_NEW_CLASSIC_JOY_CONFIG_SCENE_HPP
#define TH03_APP_NEW_CLASSIC_JOY_CONFIG_SCENE_HPP

#include <cstdint>
#include <memory>
#include <string_view>

#include "th03/app/new_classic/content/new_classic_assets.hpp"
#include "th03/app/new_classic/new_classic_frontend.hpp"
#include "th03/app/new_classic/input/new_classic_joy_config_store.hpp"

namespace th03::app {

enum class NewClassicJoyConfigSceneStatus : std::uint8_t {
	running,
	apply_requested,
	applied,
	back,
	application_quit,
	surface_failed,
};

struct NewClassicJoyConfigSceneResult final {
	bool surface_changed = false;
	NewClassicJoyConfigSceneStatus status =
		NewClassicJoyConfigSceneStatus::running;
};

// Native source-styled extension over the project-owned neutral background and
// Replay Patch's MNUFONT asset. The persisted policy is platform-neutral;
// conversion to SDL buttons/axes remains outside this scene.
class NewClassicJoyConfigScene final {
public:
	NewClassicJoyConfigScene(
		NewClassicJoyConfig config,
		NewClassicJoyConfig defaults,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
		NewClassicLanguageAssetProfile language_profile =
			NewClassicLanguageAssetProfile::english
	);

	[[nodiscard]] NewClassicJoyConfigSceneResult consume(
		NewClassicUiInput input
	) noexcept;
	void report_commit(bool success) noexcept;
	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] const NewClassicJoyConfig &config() const noexcept;
	[[nodiscard]] NewClassicJoyConfigSceneStatus status() const noexcept;
	[[nodiscard]] bool valid() const noexcept;

private:
	friend struct NewClassicJoyConfigSceneTestAccess;
	enum class Page : std::uint8_t { buttons, axes };
	enum class Phase : std::uint8_t { fade_in, menu, discard, fade_out };

	[[nodiscard]] bool compose_screen() noexcept;
	[[nodiscard]] bool draw_row(std::uint8_t row, bool selected) noexcept;
	[[nodiscard]] bool draw_value(
		std::string_view label,
		std::string_view value,
		std::int32_t top,
		bool selected
	) noexcept;
	[[nodiscard]] bool draw_command(
		std::string_view label,
		std::int32_t top,
		bool selected
	) noexcept;
	[[nodiscard]] bool show_overlay(std::string_view text) noexcept;
	[[nodiscard]] bool adjust(int direction) noexcept;
	void restore_defaults() noexcept;
	void begin_fade_out(NewClassicJoyConfigSceneStatus terminal) noexcept;
	[[nodiscard]] NewClassicJoyConfigSceneResult advance_fade(
		const NewClassicUiInput &input
	) noexcept;
	[[nodiscard]] NewClassicJoyConfigSceneResult result(
		bool surface_changed = false
	) const noexcept;
	void fail_surface() noexcept;
	[[nodiscard]] static bool any_interface_input(
		const NewClassicUiInput &input
	) noexcept;

	NewClassicJoyConfig original_{};
	NewClassicJoyConfig config_{};
	NewClassicJoyConfig defaults_{};
	NewClassicLanguageAssetProfile language_profile_ =
		NewClassicLanguageAssetProfile::english;
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_{};
	NewClassicPatchSurface patch_surface_{};
	present::Page front_page_ = present::Page::zero;
	Page page_ = Page::buttons;
	Phase phase_ = Phase::fade_in;
	std::uint8_t selected_ = 0U;
	std::uint8_t fade_boundary_ = 0U;
	bool input_previous_none_ = false;
	NewClassicJoyConfigSceneStatus status_ =
		NewClassicJoyConfigSceneStatus::running;
	NewClassicJoyConfigSceneStatus fade_out_terminal_ =
		NewClassicJoyConfigSceneStatus::back;
	bool valid_ = false;
};

struct NewClassicJoyConfigSceneTestAccess final {
	[[nodiscard]] static std::uint8_t page(
		const NewClassicJoyConfigScene &scene
	) noexcept;
	[[nodiscard]] static std::uint8_t selected_row(
		const NewClassicJoyConfigScene &scene
	) noexcept;
	[[nodiscard]] static std::uint8_t phase(
		const NewClassicJoyConfigScene &scene
	) noexcept;
};

} // namespace th03::app

#endif
