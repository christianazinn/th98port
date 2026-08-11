#ifndef TH03_APP_CLASSIC_OP_OPTIONS_SCENE_INTERNAL_HPP
#define TH03_APP_CLASSIC_OP_OPTIONS_SCENE_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

#include "classic_op_scene_internal.hpp"
#include "th03/app/classic_config.hpp"
#include "th03/assets/types.hpp"
#include "th03/present/raster.hpp"
#include "th03/present/superimpose.hpp"
#include "th03/present/tram.hpp"

namespace th03::app {

enum class ClassicOpOptionsChoice : std::uint8_t {
	rank,
	bgm,
	key_mode,
	quit,
};

enum class ClassicOpOptionsPhase : std::uint8_t {
	grow_window,
	enter_options,
	options_menu,
	shrink_window,
	return_main,
	ready,
	consumed,
	failed,
};

struct ClassicOpOptionsTick final {
	ClassicOpOptionsPhase phase = ClassicOpOptionsPhase::failed;
	ClassicOpBoundary boundary = ClassicOpBoundary::immediate;
	bool surface_changed = false;
	bool tram_changed = false;
	bool input_sampled = false;
	bool selection_changed = false;
	bool config_changed = false;
	bool ready = false;
};

class ClassicOpOptionsCompletion final {
public:
	ClassicOpOptionsCompletion(const ClassicOpOptionsCompletion &) = delete;
	ClassicOpOptionsCompletion &operator=(
		const ClassicOpOptionsCompletion &
	) = delete;
	ClassicOpOptionsCompletion(ClassicOpOptionsCompletion &&) noexcept = default;
	ClassicOpOptionsCompletion &operator=(
		ClassicOpOptionsCompletion &&
	) noexcept = default;

	[[nodiscard]] const GameInit &resident() const noexcept;
	[[nodiscard]] ClassicConfigOptions config() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] const present::Tram &tram() const noexcept;
	[[nodiscard]] bool main_input_allowed() const noexcept;

private:
	friend class ClassicOpOptionsScene;
	ClassicOpOptionsCompletion(
		GameInit resident,
		ClassicConfigOptions config,
		present::Raster raster,
		present::Tram tram,
		bool main_input_allowed
	) noexcept;

	GameInit resident_{};
	ClassicConfigOptions config_{};
	present::Raster raster_{};
	present::Tram tram_{};
	bool main_input_allowed_ = false;
};

// Source-private owner of OP.EXE's in-process Options child. The caller has
// already authenticated and decoded the process-resident title resources;
// this scene performs no file I/O and returns to the retained main menu.
class ClassicOpOptionsScene final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpOptionsScene>,
		ClassicOpError
	> create_from_op(
		const ClassicOpCompletion &completion,
		ClassicConfigOptions config,
		bool fm_available,
		std::span<const LoadedMainResource> resources
	);

	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpOptionsScene>,
		ClassicOpError
	> create(
		GameInit resident,
		ClassicConfigOptions config,
		bool fm_available,
		present::Raster raster,
		present::Tram tram,
		assets::PiImage menu_background,
		assets::BfntSheet window_sheet
	);
	~ClassicOpOptionsScene() = default;

	ClassicOpOptionsScene(const ClassicOpOptionsScene &) = delete;
	ClassicOpOptionsScene &operator=(const ClassicOpOptionsScene &) = delete;
	ClassicOpOptionsScene(ClassicOpOptionsScene &&) = delete;
	ClassicOpOptionsScene &operator=(ClassicOpOptionsScene &&) = delete;

	[[nodiscard]] std::expected<ClassicOpOptionsTick, ClassicOpError>
	advance(ClassicOpInput input = {});
	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command() noexcept;
	[[nodiscard]] std::optional<ClassicOpOptionsCompletion>
	take_completion() noexcept;

	[[nodiscard]] ClassicOpOptionsPhase phase() const noexcept;
	[[nodiscard]] ClassicOpOptionsChoice selected_choice() const noexcept;
	[[nodiscard]] ClassicConfigOptions config() const noexcept;
	[[nodiscard]] const GameInit &resident() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] const present::Tram &tram() const noexcept;
	[[nodiscard]] bool input_allowed() const noexcept;

private:
	struct Prepared final {
		assets::PiImage menu_background{};
		std::optional<present::PreparedSuperimposeSheet> window_sheet{};
	};

	ClassicOpOptionsScene(
		std::unique_ptr<Prepared> prepared,
		GameInit resident,
		ClassicConfigOptions config,
		bool fm_available,
		present::Raster raster,
		present::Tram tram
	) noexcept;

	[[nodiscard]] bool restore_menu_background(
		std::int32_t left,
		std::uint16_t width
	) noexcept;
	[[nodiscard]] bool draw_window_glyph(
		std::int32_t left,
		std::uint16_t glyph
	) noexcept;
	[[nodiscard]] bool grow_step() noexcept;
	[[nodiscard]] bool shrink_step() noexcept;
	[[nodiscard]] bool draw_option_choice(
		ClassicOpOptionsChoice choice,
		present::TramAttribute attribute
	) noexcept;
	[[nodiscard]] bool draw_all_option_choices() noexcept;
	[[nodiscard]] bool draw_main_choices() noexcept;
	[[nodiscard]] bool update_selection(int direction) noexcept;
	[[nodiscard]] bool update_horizontal(
		ClassicOpInput input,
		bool &config_changed
	) noexcept;
	[[nodiscard]] bool push_audio_command(
		ClassicAudioCommand command
	) noexcept;
	void finish() noexcept;
	void fail() noexcept;
	[[nodiscard]] ClassicOpOptionsTick tick(
		ClassicOpBoundary boundary,
		bool surface_changed = false,
		bool tram_changed = false,
		bool input_sampled = false,
		bool selection_changed = false,
		bool config_changed = false
	) const noexcept;

	std::unique_ptr<Prepared> prepared_{};
	GameInit resident_{};
	ClassicConfigOptions config_{};
	present::Raster raster_{};
	present::Tram tram_{};
	std::array<ClassicAudioCommand, 3U> audio_commands_{};
	std::size_t audio_head_ = 0U;
	std::size_t audio_count_ = 0U;
	ClassicOpOptionsPhase phase_ = ClassicOpOptionsPhase::failed;
	ClassicOpOptionsChoice selected_ = ClassicOpOptionsChoice::rank;
	ClassicOpInput transition_input_{};
	std::uint16_t phase_frame_ = 0U;
	std::int32_t box_right_left_ = 0;
	bool transition_input_latched_ = false;
	bool menu_input_allowed_ = false;
	bool fm_available_ = false;
	std::optional<ClassicOpOptionsCompletion> completion_{};
};

} // namespace th03::app

#endif
