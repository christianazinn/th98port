#ifndef TH03_APP_NEW_CLASSIC_DEVICE_SCENE_HPP
#define TH03_APP_NEW_CLASSIC_DEVICE_SCENE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include "th03/app/new_classic_assets.hpp"
#include "th03/app/new_classic_controller.hpp"
#include "th03/app/new_classic_frontend.hpp"
#include "th03/core/game.hpp"

namespace th03::app {

enum class NewClassicDeviceSceneMode : std::uint8_t {
	assignment,
	input_test,
};

enum class NewClassicDeviceSceneStatus : std::uint8_t {
	running,
	apply_requested,
	applied,
	back,
	application_quit,
	surface_failed,
};

struct NewClassicDeviceSceneResult final {
	bool surface_changed = false;
	NewClassicDeviceSceneStatus status = NewClassicDeviceSceneStatus::running;
};

class NewClassicDeviceScene final {
public:
	NewClassicDeviceScene(
		NewClassicDeviceSceneMode mode,
		NewClassicDeviceAssignment assignment,
		NewClassicDeviceAssignment defaults,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
		NewClassicLanguageAssetProfile language_profile =
			NewClassicLanguageAssetProfile::english
	);

	[[nodiscard]] NewClassicDeviceSceneResult consume(
		NewClassicUiInput input,
		const NewClassicControllerCatalog &catalog,
		CoreInput logical_input
	) noexcept;
	void report_commit(bool success) noexcept;
	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] const NewClassicDeviceAssignment &assignment() const noexcept;
	[[nodiscard]] NewClassicDeviceSceneStatus status() const noexcept;
	[[nodiscard]] bool valid() const noexcept;
	[[nodiscard]] NewClassicDeviceSceneMode mode() const noexcept;

private:
	enum class Phase : std::uint8_t { fade_in, menu, discard, fade_out };
	[[nodiscard]] bool compose() noexcept;
	[[nodiscard]] bool compose_assignment() noexcept;
	[[nodiscard]] bool compose_input_test() noexcept;
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
	[[nodiscard]] std::string_view assignment_value(
		std::size_t player,
		std::array<char, 64U> &storage
	) const noexcept;
	void begin_fade(NewClassicDeviceSceneStatus terminal) noexcept;
	[[nodiscard]] NewClassicDeviceSceneResult advance_fade(
		const NewClassicUiInput &input
	) noexcept;
	[[nodiscard]] NewClassicDeviceSceneResult result(bool changed = false)
		const noexcept;
	void fail() noexcept;
	[[nodiscard]] static bool any(const NewClassicUiInput &input) noexcept;

	NewClassicDeviceSceneMode mode_ = NewClassicDeviceSceneMode::assignment;
	NewClassicDeviceAssignment original_{};
	NewClassicDeviceAssignment assignment_{};
	NewClassicDeviceAssignment defaults_{};
	NewClassicControllerCatalog catalog_{};
	CoreInput logical_input_{};
	NewClassicLanguageAssetProfile language_profile_ =
		NewClassicLanguageAssetProfile::english;
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_{};
	NewClassicPatchSurface patch_surface_{};
	present::Page front_page_ = present::Page::zero;
	Phase phase_ = Phase::fade_in;
	std::uint8_t selected_ = 0U;
	std::uint8_t fade_boundary_ = 0U;
	bool input_previous_none_ = false;
	NewClassicDeviceSceneStatus status_ = NewClassicDeviceSceneStatus::running;
	NewClassicDeviceSceneStatus fade_terminal_ = NewClassicDeviceSceneStatus::back;
	bool valid_ = false;
};

} // namespace th03::app

#endif
