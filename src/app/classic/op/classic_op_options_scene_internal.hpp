#ifndef TH03_APP_CLASSIC_OP_OPTIONS_SCENE_INTERNAL_HPP
#define TH03_APP_CLASSIC_OP_OPTIONS_SCENE_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

#include "classic/op/classic_op_scene_internal.hpp"
#include "th03/app/classic/persistence/classic_config.hpp"
#include "th03/app/new_classic/content/new_classic_assets.hpp"
#include "th03/app/new_classic/persistence/new_classic_settings.hpp"
#include "th03/app/update/update_ui.hpp"
#include "th03/assets/types.hpp"
#include "th03/present/graphics_text.hpp"
#include "th03/present/raster.hpp"
#include "th03/present/superimpose.hpp"
#include "th03/present/tram.hpp"

namespace th03::app {

enum class ClassicOpOptionsChoice : std::uint8_t {
	rank,
	arrange,
	master_volume,
	window_mode,
	window_scale,
	aspect_ratio,
	filtering,
	bgm,
	key_mode,
	quit,
	sfx,
	mute_when_unfocused,
	language,
	photosensitivity,
	category_gameplay,
	category_audio,
	category_input,
	category_video,
	category_language,
	category_system,
	key_config,
	joy_config,
	device_assignment,
	input_test,
	return_page,
	switch_classic,
	data_storage,
	online_privacy,
	credits,
	diagnostics,
	updates,
	automatic_updates,
	open_user_data,
	open_replay_folder,
	open_log_folder,
	forget_room_identity,
	check_for_updates,
	confirm_switch_classic,
	cancel_switch_classic,
	confirm_forget_room_identity,
	cancel_forget_room_identity,
};

enum class ClassicOpOptionsPage : std::uint8_t {
	categories,
	gameplay,
	audio,
	input,
	video,
	language,
	system,
	data_storage,
	online_privacy,
	credits,
	diagnostics,
	updates,
	switch_confirmation,
	forget_identity_confirmation,
};

enum class ClassicOpOptionsOpenFolder : std::uint8_t {
	user_data,
	replays,
};

enum class ClassicOpOptionsOutcome : std::uint8_t {
	return_main,
	key_config,
	joy_config,
	device_assignment,
	input_test,
	switch_classic,
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
	bool forget_online_identity = false;
	bool check_for_updates = false;
	std::optional<ClassicOpOptionsOpenFolder> open_folder{};
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
	[[nodiscard]] std::uint8_t master_volume_percent() const noexcept;
	[[nodiscard]] std::uint8_t music_volume_percent() const noexcept;
	[[nodiscard]] std::uint8_t effect_volume_percent() const noexcept;
	[[nodiscard]] bool mute_when_unfocused() const noexcept;
	[[nodiscard]] NewClassicWindowPresentation window_presentation() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] const present::Tram &tram() const noexcept;
	[[nodiscard]] bool main_input_allowed() const noexcept;
	[[nodiscard]] ClassicOpOptionsOutcome outcome() const noexcept;

private:
	friend class ClassicOpOptionsScene;
	ClassicOpOptionsCompletion(
		GameInit resident,
		ClassicConfigOptions config,
		std::uint8_t master_volume_percent,
		NewClassicWindowPresentation window_presentation,
		std::uint8_t music_volume_percent,
		std::uint8_t effect_volume_percent,
		bool mute_when_unfocused,
		present::Raster raster,
		present::Tram tram,
		bool main_input_allowed,
		ClassicOpOptionsOutcome outcome
	) noexcept;

	GameInit resident_{};
	ClassicConfigOptions config_{};
	std::uint8_t master_volume_percent_ = 100U;
	std::uint8_t music_volume_percent_ = 100U;
	std::uint8_t effect_volume_percent_ = 100U;
	bool mute_when_unfocused_ = false;
	NewClassicWindowPresentation window_presentation_{};
	present::Raster raster_{};
	present::Tram tram_{};
	bool main_input_allowed_ = false;
	ClassicOpOptionsOutcome outcome_ = ClassicOpOptionsOutcome::return_main;
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
		std::span<const LoadedMainResource> resources,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets = {},
		const present::AnkGlyphProvider *font = nullptr,
		bool language_available = false,
		std::uint8_t master_volume_percent = 100U,
		NewClassicWindowPresentation window_presentation = {},
		std::uint8_t music_volume_percent = 100U,
		std::uint8_t effect_volume_percent = 100U,
		bool mute_when_unfocused = false,
		bool online_identity_reset_available = false,
		NewClassicUpdateUiState update = {}
	);

	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpOptionsScene>,
		ClassicOpError
	> resume_after_key_config(
		const ClassicOpOptionsCompletion &completion,
		bool fm_available,
		std::span<const LoadedMainResource> resources,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
		const present::AnkGlyphProvider *font,
		bool language_available,
		std::uint8_t master_volume_percent = 100U,
		NewClassicWindowPresentation window_presentation = {},
		std::uint8_t music_volume_percent = 100U,
		std::uint8_t effect_volume_percent = 100U,
		bool mute_when_unfocused = false,
		bool online_identity_reset_available = false,
		NewClassicUpdateUiState update = {}
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpOptionsScene>,
		ClassicOpError
	> resume_after_joy_config(
		const ClassicOpOptionsCompletion &completion,
		bool fm_available,
		std::span<const LoadedMainResource> resources,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
		const present::AnkGlyphProvider *font,
		bool language_available,
		std::uint8_t master_volume_percent = 100U,
		NewClassicWindowPresentation window_presentation = {},
		std::uint8_t music_volume_percent = 100U,
		std::uint8_t effect_volume_percent = 100U,
		bool mute_when_unfocused = false,
		bool online_identity_reset_available = false,
		NewClassicUpdateUiState update = {}
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpOptionsScene>,
		ClassicOpError
	> resume_after_device_scene(
		const ClassicOpOptionsCompletion &completion,
		bool fm_available,
		std::span<const LoadedMainResource> resources,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
		const present::AnkGlyphProvider *font,
		bool language_available,
		std::uint8_t master_volume_percent = 100U,
		NewClassicWindowPresentation window_presentation = {},
		std::uint8_t music_volume_percent = 100U,
		std::uint8_t effect_volume_percent = 100U,
		bool mute_when_unfocused = false,
		bool online_identity_reset_available = false,
		NewClassicUpdateUiState update = {}
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
		assets::BfntSheet window_sheet,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets = {},
		const present::AnkGlyphProvider *font = nullptr,
		bool language_available = false,
		std::uint8_t master_volume_percent = 100U,
		NewClassicWindowPresentation window_presentation = {},
		std::uint8_t music_volume_percent = 100U,
		std::uint8_t effect_volume_percent = 100U,
		bool mute_when_unfocused = false,
		bool online_identity_reset_available = false,
		NewClassicUpdateUiState update = {}
	);
	[[nodiscard]] std::expected<void, ClassicOpError>
	begin_update_check();
	[[nodiscard]] std::expected<void, ClassicOpError>
	report_update_check(NewClassicUpdateCheckOutcome outcome);
	[[nodiscard]] std::expected<void, ClassicOpError>
	refresh_patch_language_background(
		std::span<const LoadedMainResource> resources
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
	[[nodiscard]] ClassicOpOptionsPage page() const noexcept;
	[[nodiscard]] ClassicOpOptionsChoice selected_choice() const noexcept;
	[[nodiscard]] ClassicConfigOptions config() const noexcept;
	[[nodiscard]] std::uint8_t master_volume_percent() const noexcept;
	[[nodiscard]] std::uint8_t music_volume_percent() const noexcept;
	[[nodiscard]] std::uint8_t effect_volume_percent() const noexcept;
	[[nodiscard]] bool mute_when_unfocused() const noexcept;
	[[nodiscard]] bool automatic_updates() const noexcept;
	[[nodiscard]] NewClassicWindowPresentation window_presentation() const noexcept;
	[[nodiscard]] const GameInit &resident() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] const present::Tram &tram() const noexcept;
	[[nodiscard]] bool input_allowed() const noexcept;

private:
	struct Prepared final {
		assets::PiImage menu_background{};
		std::optional<present::PreparedSuperimposeSheet> window_sheet{};
		std::optional<present::PreparedGraphicsText8x16> japanese{};
	};

	ClassicOpOptionsScene(
		std::unique_ptr<Prepared> prepared,
		GameInit resident,
		ClassicConfigOptions config,
		bool fm_available,
		present::Raster raster,
		present::Tram tram,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
		bool language_available,
		std::uint8_t master_volume_percent,
		NewClassicWindowPresentation window_presentation,
		std::uint8_t music_volume_percent,
		std::uint8_t effect_volume_percent,
		bool mute_when_unfocused,
		bool online_identity_reset_available,
		NewClassicUpdateUiState update
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
	[[nodiscard]] bool restore_patch_rows(
		std::uint8_t first,
		std::uint8_t count,
		std::int32_t box_right
	) noexcept;
	[[nodiscard]] bool draw_patch_choice(
		ClassicOpOptionsChoice choice,
		present::Index4 color
	) noexcept;
	[[nodiscard]] bool draw_patch_credits() noexcept;
	[[nodiscard]] bool draw_patch_online_identity_status() noexcept;
	[[nodiscard]] bool draw_patch_update_status() noexcept;
	[[nodiscard]] std::span<const ClassicOpOptionsChoice>
	active_choices() const noexcept;
	[[nodiscard]] std::optional<std::uint8_t>
	choice_line(ClassicOpOptionsChoice choice) const noexcept;
	[[nodiscard]] bool open_patch_page(ClassicOpOptionsPage page) noexcept;
	[[nodiscard]] bool return_to_patch_categories() noexcept;
	[[nodiscard]] bool update_selection(int direction) noexcept;
	[[nodiscard]] bool update_horizontal(
		ClassicOpInput input,
		bool &config_changed
	) noexcept;
	[[nodiscard]] bool push_audio_command(
		ClassicAudioCommand command
	) noexcept;
	void finish(
		ClassicOpOptionsOutcome outcome = ClassicOpOptionsOutcome::return_main
	) noexcept;
	void fail() noexcept;
	[[nodiscard]] ClassicOpOptionsTick tick(
		ClassicOpBoundary boundary,
		bool surface_changed = false,
		bool tram_changed = false,
		bool input_sampled = false,
		bool selection_changed = false,
		bool config_changed = false,
		bool forget_online_identity = false,
		bool check_for_updates = false,
		std::optional<ClassicOpOptionsOpenFolder> open_folder = std::nullopt
	) const noexcept;

	std::unique_ptr<Prepared> prepared_{};
	GameInit resident_{};
	ClassicConfigOptions config_{};
	std::uint8_t master_volume_percent_ = 100U;
	std::uint8_t music_volume_percent_ = 100U;
	std::uint8_t effect_volume_percent_ = 100U;
	bool mute_when_unfocused_ = false;
	bool online_identity_reset_available_ = false;
	NewClassicUpdateUiState update_{};
	bool external_surface_changed_ = false;
	NewClassicWindowPresentation window_presentation_{};
	present::Raster raster_{};
	present::Tram tram_{};
	std::array<ClassicAudioCommand, 8U> audio_commands_{};
	std::size_t audio_head_ = 0U;
	std::size_t audio_count_ = 0U;
	ClassicOpOptionsPhase phase_ = ClassicOpOptionsPhase::failed;
	ClassicOpOptionsChoice selected_ = ClassicOpOptionsChoice::rank;
	ClassicOpOptionsPage page_ = ClassicOpOptionsPage::categories;
	ClassicOpOptionsOutcome pending_outcome_ =
		ClassicOpOptionsOutcome::return_main;
	ClassicOpInput transition_input_{};
	std::uint16_t phase_frame_ = 0U;
	std::int32_t box_right_left_ = 0;
	bool transition_input_latched_ = false;
	bool menu_input_allowed_ = false;
	bool fm_available_ = false;
	bool language_available_ = false;
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_{};
	std::optional<ClassicOpOptionsCompletion> completion_{};
};

} // namespace th03::app

#endif
