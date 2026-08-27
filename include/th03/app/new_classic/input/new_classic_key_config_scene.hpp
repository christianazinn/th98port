#ifndef TH03_APP_NEW_CLASSIC_KEY_CONFIG_SCENE_HPP
#define TH03_APP_NEW_CLASSIC_KEY_CONFIG_SCENE_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "th03/app/new_classic/content/new_classic_assets.hpp"
#include "th03/app/new_classic/new_classic_frontend.hpp"
#include "th03/app/new_classic/input/new_classic_key_config_store.hpp"

namespace th03::app {

enum class NewClassicKeyConfigSceneStatus : std::uint8_t {
	running,
	apply_requested,
	applied,
	back,
	application_quit,
	surface_failed,
};

struct NewClassicKeyConfigSceneInput final {
	NewClassicUiInput ui{};
	std::optional<std::uint16_t> scancode_pressed{};
	bool any_scancode_held = false;
};

struct NewClassicKeyConfigSceneResult final {
	bool surface_changed = false;
	NewClassicKeyConfigSceneStatus status =
		NewClassicKeyConfigSceneStatus::running;
};

// Native Replay Patch 0.4.12 KeyConfig scene. NewClassicKeyConfig remains the
// host-scancode persistence boundary, but the scene's pages, rendering, page
// flips, palette cadence, and release barriers follow th03/keycfg_o.cpp.
class NewClassicKeyConfigScene final {
public:
	NewClassicKeyConfigScene(
		NewClassicKeyConfig config,
		NewClassicKeyConfig defaults,
		std::uint16_t maximum_key,
		std::uint16_t capture_cancel_key,
		NewClassicHostKeyName key_name,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
		void *key_name_context = nullptr,
		NewClassicLanguageAssetProfile language_profile =
			NewClassicLanguageAssetProfile::english
	);

	[[nodiscard]] NewClassicKeyConfigSceneResult consume(
		NewClassicKeyConfigSceneInput input
	) noexcept;
	void report_commit(bool success) noexcept;
	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] const NewClassicKeyConfig &config() const noexcept;
	[[nodiscard]] NewClassicKeyConfigSceneStatus status() const noexcept;
	[[nodiscard]] bool valid() const noexcept;

private:
	friend struct NewClassicKeyConfigSceneTestAccess;

	enum class Page : std::uint8_t { p1, p2, story };
	enum class Phase : std::uint8_t {
		fade_in,
		menu,
		capture_wait_release,
		capture_wait_press,
		capture_wait_final_release,
		discard,
		fade_out,
	};

	[[nodiscard]] bool compose_screen() noexcept;
	[[nodiscard]] bool draw_player_row(
		std::uint8_t player,
		std::uint8_t row,
		bool selected
	) noexcept;
	[[nodiscard]] bool draw_story_row(
		std::uint8_t row,
		bool selected
	) noexcept;
	[[nodiscard]] bool draw_row(
		std::string_view label,
		std::string_view value,
		std::int32_t label_left,
		std::int32_t value_left,
		std::int32_t top,
		bool selected
	) noexcept;
	[[nodiscard]] bool draw_command(
		std::string_view label,
		std::int32_t top,
		bool selected
	) noexcept;
	[[nodiscard]] bool show_overlay(std::string_view text) noexcept;
	[[nodiscard]] std::string_view key_label(
		std::uint16_t key,
		std::string &fallback
	) const noexcept;
	[[nodiscard]] NewClassicKeyConfigSceneResult advance_fade(
		const NewClassicKeyConfigSceneInput &input
	) noexcept;
	void begin_fade_out(
		NewClassicKeyConfigSceneStatus terminal
	) noexcept;
	void restore_defaults() noexcept;
	void assign(std::uint16_t key) noexcept;
	void set_page(Page page) noexcept;
	[[nodiscard]] std::optional<std::size_t> binding_index() const noexcept;
	[[nodiscard]] static std::uint8_t player_selected_row(
		bool movement_column,
		std::uint8_t at
	) noexcept;
	[[nodiscard]] static bool any_interface_input(
		const NewClassicUiInput &input
	) noexcept;
	[[nodiscard]] NewClassicKeyConfigSceneResult result(
		bool surface_changed = false
	) const noexcept;
	void fail_surface() noexcept;

	NewClassicKeyConfig original_{};
	NewClassicKeyConfig config_{};
	NewClassicKeyConfig defaults_{};
	std::uint16_t maximum_key_ = 0U;
	std::uint16_t capture_cancel_key_ = 0U;
	NewClassicHostKeyName key_name_ = nullptr;
	void *key_name_context_ = nullptr;
	NewClassicLanguageAssetProfile language_profile_ =
		NewClassicLanguageAssetProfile::english;
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_{};
	NewClassicPatchSurface patch_surface_{};
	present::Page front_page_ = present::Page::zero;
	Page page_ = Page::story;
	Phase phase_ = Phase::fade_in;
	std::uint8_t selected_ = 0U;
	std::uint8_t player_at_ = 0U;
	std::uint8_t fade_boundary_ = 0U;
	bool movement_column_ = false;
	bool input_previous_none_ = false;
	std::optional<std::size_t> capture_binding_{};
	NewClassicKeyConfigSceneStatus status_ =
		NewClassicKeyConfigSceneStatus::running;
	NewClassicKeyConfigSceneStatus fade_out_terminal_ =
		NewClassicKeyConfigSceneStatus::back;
	bool valid_ = false;
};

struct NewClassicKeyConfigSceneTestAccess final {
	[[nodiscard]] static std::uint8_t page(
		const NewClassicKeyConfigScene &scene
	) noexcept;
	[[nodiscard]] static std::uint8_t selected_row(
		const NewClassicKeyConfigScene &scene
	) noexcept;
	[[nodiscard]] static bool movement_column(
		const NewClassicKeyConfigScene &scene
	) noexcept;
	[[nodiscard]] static std::uint8_t phase(
		const NewClassicKeyConfigScene &scene
	) noexcept;
};

} // namespace th03::app

#endif
