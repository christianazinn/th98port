#ifndef TH03_APP_CLASSIC_OP_SESSION_INTERNAL_HPP
#define TH03_APP_CLASSIC_OP_SESSION_INTERNAL_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "classic_mainl_stage_internal.hpp"
#include "classic_mainl_win_internal.hpp"
#include "classic_op_music_room_internal.hpp"
#include "classic_op_options_scene_internal.hpp"
#include "classic_op_scene_internal.hpp"
#include "classic_op_selection_internal.hpp"
#include "classic_registration_scene_internal.hpp"
#include "classic_story_session_internal.hpp"
#include "mainl_compiled_text_internal.hpp"
#include "th03/app/authentic_text.hpp"
#include "th03/app/classic_config_store.hpp"
#include "th03/app/classic_score_store.hpp"
#include "th03/app/main_compiled_content.hpp"
#include "th03/app/new_classic_frontend.hpp"
#include "th03/app/main_process.hpp"
#include "th03/app/new_classic_language.hpp"
#include "th03/app/new_classic_joy_config_store.hpp"
#include "th03/app/new_classic_device_store.hpp"
#include "th03/app/new_classic_practice.hpp"
#include "th03/app/new_classic_settings_store.hpp"
#include "th03/app/pixel_oracle_identity.hpp"
#include "th03/present/source_surface.hpp"

namespace th03::app {

class NewClassicReplayRecorder;
struct ClassicRunnerRequest;

enum class ClassicOpSessionScene : std::uint8_t {
	title_menu,
	options,
	key_config,
	replay_browser,
	replay_playback,
	music_room,
	high_scores,
	character_selection,
	story,
	demo_stage_splash,
	demo_main,
	vs_stage_splash,
	vs_main,
	vs_win,
	complete,
	failed,
};

enum class ClassicOpSessionStatus : std::uint8_t {
	running,
	complete,
	switch_to_classic,
	netplay_requested,
	quit_requested,
	failed,
};

enum class ClassicOpSessionErrorCode : std::uint8_t {
	invalid_config,
	invalid_backend,
	resource_load_failed,
	config_store_failed,
	score_store_failed,
	font_failed,
	mainl_text_failed,
	static_content_failed,
	title_failed,
	options_failed,
	replay_browser_failed,
	replay_playback_failed,
	music_room_failed,
	high_scores_failed,
	selection_failed,
	stage_failed,
	main_failed,
	story_failed,
	win_failed,
	backend_contract_failed,
	audio_command_pending,
	session_not_running,
	surface_failed,
	op_text_failed,
	language_overlay_failed,
};

struct ClassicOpSessionError final {
	ClassicOpSessionErrorCode code = ClassicOpSessionErrorCode::invalid_config;
	std::optional<ResourceLoadError> resource_error{};
	std::optional<ClassicConfigStoreError> config_store_error{};
	std::optional<NewClassicSettingsStoreError> new_settings_store_error{};
	std::optional<ClassicScoreStoreError> score_store_error{};
	std::optional<NewClassicScoreStoreError> new_score_store_error{};
	std::optional<MainlCompiledTextError> mainl_text_error{};
	std::optional<OpCompiledTextError> op_text_error{};
	std::optional<NewClassicLanguageOverlayError> language_overlay_error{};
	std::optional<AuthenticTextError> font_error{};
	std::optional<MainProcessError> main_error{};
	std::optional<ClassicOpError> title_error{};
	std::optional<ClassicOpMusicRoomError> music_room_error{};
	std::optional<ClassicRegistrationError> registration_error{};
	std::optional<ClassicOpSelectionError> selection_error{};
	std::optional<ClassicMainlStageError> stage_error{};
	std::optional<ClassicStorySessionError> story_error{};
	std::optional<ClassicMainlWinError> win_error{};
	std::optional<present::SourceSurfaceError> surface_error{};
};

struct ClassicOpSessionInput final {
	ClassicOpInput op{};
	ClassicOpMusicRoomInput music_room{};
	ClassicRegistrationInput high_scores{};
	ClassicOpSelectionInput selection{};
	ClassicStorySessionInput story{};
	ClassicMainInputSample main{};
	ClassicAudioStatus audio{};
	NewClassicUiInput replay_ui{};
	bool replay_fast_forward = false;
	std::optional<std::uint16_t> scancode_pressed{};
	bool any_scancode_held = false;
	bool dismiss_pressed = false;
	bool app_quit_requested = false;
};

struct ClassicOpSessionTick final {
	ClassicOpSessionScene scene = ClassicOpSessionScene::failed;
	ClassicOpSessionStatus status = ClassicOpSessionStatus::failed;
	bool surface_changed = false;
	bool input_sampled = false;
	bool process_advanced = false;
	bool round_started = false;
	bool clock_rebase_requested = false;
	PixelOracleSourceIdentity pixel_oracle_source_identity{};
};

struct ClassicOpSessionBackendTick final {
	bool surface_changed = false;
	bool input_sampled = false;
	bool ready = false;
	bool process_advanced = false;
	bool round_started = false;
	bool return_to_selection = false;
	std::optional<PixelOracleSourceIdentity> pixel_oracle_source_identity{};
};

struct ClassicOpSessionTitleResult final {
	ClassicOpRoute route = ClassicOpRoute::quit;
	std::optional<ClassicOpSelectionMode> selection_mode{};
};

struct ClassicOpSessionSelectionResult final {
	ClassicOpSelectionOutcome outcome = ClassicOpSelectionOutcome::canceled;
	ClassicOpSelectionMode mode = ClassicOpSelectionMode::story;
};

enum class ClassicOpSessionReplayResult : std::uint8_t {
	back,
	continue_browser,
	preparing_playback,
	play,
	application_quit,
};

struct ClassicOpSessionVsTerminal final {
	GameInit process_start{};
	CanonicalState terminal{};
	SessionTransition transition{};
	bool restart_requested = false;
};

enum class ClassicOpVsRouteTestOutcome : std::uint8_t {
	exit_to_operator,
	player_one_win,
	player_two_win,
};

enum class ClassicOpVsSurfaceSource : std::uint8_t {
	none,
	win_completion,
	win,
	main_process,
};

// Source-private dependency seam. Production binds this to one concrete owner
// of the loader, authenticated content, persistence files, scenes, and MAIN
// process. Tests can substitute deterministic facts without forging any child
// completion capability.
struct ClassicOpSessionBackendOperations final {
	void *context = nullptr;
	std::expected<ClassicOpSessionBackendTick, ClassicOpSessionError>
	(*advance_title)(void *, ClassicOpInput) = nullptr;
	std::expected<ClassicOpSessionTitleResult, ClassicOpSessionError>
	(*finish_title)(void *) = nullptr;
	std::expected<void, ClassicOpSessionError> (*start_options)(void *) = nullptr;
	std::expected<ClassicOpSessionBackendTick, ClassicOpSessionError>
	(*advance_options)(void *, ClassicOpInput) = nullptr;
	std::expected<bool, ClassicOpSessionError> (*finish_options)(void *) = nullptr;
	bool (*key_config_active)(void *) = nullptr;
	std::expected<ClassicOpSessionBackendTick, ClassicOpSessionError>
	(*advance_key_config)(
		void *, NewClassicUiInput, CoreInput, std::optional<std::uint16_t>, bool
	) = nullptr;
	std::expected<bool, ClassicOpSessionError>
	(*finish_key_config)(void *) = nullptr;
	std::expected<void, ClassicOpSessionError>
	(*start_replay_browser)(void *) = nullptr;
	std::expected<ClassicOpSessionBackendTick, ClassicOpSessionError>
	(*advance_replay_browser)(void *, NewClassicUiInput) = nullptr;
	std::expected<ClassicOpSessionReplayResult, ClassicOpSessionError>
	(*finish_replay_browser)(void *) = nullptr;
	std::expected<ClassicOpSessionBackendTick, ClassicOpSessionError>
	(*advance_replay_playback)(
		void *, ClassicAudioStatus, NewClassicUiInput, CoreInput, bool
	) = nullptr;
	std::expected<bool, ClassicOpSessionError>
	(*finish_replay_playback)(void *) = nullptr;
	std::expected<void, ClassicOpSessionError> (*start_music_room)(void *) = nullptr;
	std::expected<ClassicOpSessionBackendTick, ClassicOpSessionError>
	(*advance_music_room)(void *, const ClassicOpMusicRoomInput &) = nullptr;
	std::expected<void, ClassicOpSessionError> (*finish_music_room)(void *) = nullptr;
	std::expected<void, ClassicOpSessionError> (*start_high_scores)(void *) = nullptr;
	std::expected<ClassicOpSessionBackendTick, ClassicOpSessionError>
	(*advance_high_scores)(
		void *, ClassicRegistrationInput, ClassicAudioStatus
	) = nullptr;
	std::expected<ClassicOpSessionReplayResult, ClassicOpSessionError>
	(*finish_high_scores)(void *) = nullptr;
	std::expected<void, ClassicOpSessionError> (*start_selection)(
		void *, ClassicOpSelectionMode
	) = nullptr;
	std::expected<ClassicOpSessionBackendTick, ClassicOpSessionError>
	(*advance_selection)(void *, ClassicOpSelectionInput) = nullptr;
	std::expected<ClassicOpSessionSelectionResult, ClassicOpSessionError>
	(*finish_selection)(void *) = nullptr;
	std::expected<void, ClassicOpSessionError>
	(*return_from_canceled_selection)(void *) = nullptr;
	std::expected<void, ClassicOpSessionError> (*commit_config)(void *) = nullptr;
	std::expected<void, ClassicOpSessionError> (*start_story)(void *) = nullptr;
	std::expected<ClassicOpSessionBackendTick, ClassicOpSessionError>
	(*advance_story)(void *, const ClassicStorySessionInput &) = nullptr;
	std::expected<bool, ClassicOpSessionError> (*finish_story)(void *) = nullptr;
	std::expected<void, ClassicOpSessionError> (*start_demo_stage)(void *) = nullptr;
	std::expected<ClassicOpSessionBackendTick, ClassicOpSessionError>
	(*advance_demo_stage)(void *, bool) = nullptr;
	std::expected<void, ClassicOpSessionError> (*start_demo_main)(void *) = nullptr;
	std::expected<ClassicOpSessionBackendTick, ClassicOpSessionError>
	(*advance_demo_main)(void *, const ClassicMainInputSample &) = nullptr;
	std::expected<void, ClassicOpSessionError> (*finish_demo_main)(void *) = nullptr;
	std::expected<void, ClassicOpSessionError> (*start_vs_stage)(void *) = nullptr;
	std::expected<ClassicOpSessionBackendTick, ClassicOpSessionError>
	(*advance_vs_stage)(void *, ClassicOpInput) = nullptr;
	std::expected<void, ClassicOpSessionError> (*start_vs_main)(void *) = nullptr;
	std::expected<ClassicOpSessionBackendTick, ClassicOpSessionError>
	(*advance_vs_main)(void *, const ClassicMainInputSample &) = nullptr;
	std::expected<ClassicOpSessionVsTerminal, ClassicOpSessionError>
	(*finish_vs_main)(void *) = nullptr;
	std::expected<void, ClassicOpSessionError> (*restart_vs_stage)(
		void *, const ClassicOpSessionVsTerminal &
	) = nullptr;
	std::expected<void, ClassicOpSessionError> (*start_vs_win)(
		void *, const ClassicOpSessionVsTerminal &
	) = nullptr;
	std::expected<ClassicOpSessionBackendTick, ClassicOpSessionError>
	(*advance_vs_win)(void *, ClassicAudioStatus, bool) = nullptr;
	std::expected<void, ClassicOpSessionError> (*finish_vs_win)(void *) = nullptr;
	std::expected<void, ClassicOpSessionError>
	(*start_direct_vs_selection)(void *) = nullptr;
	std::expected<void, ClassicOpSessionError>
	(*merge_online_board)(void *, NewClassicOnlineCacheDocument) = nullptr;
	bool (*audio_pending)(void *) = nullptr;
	std::optional<ClassicAudioCommand> (*take_audio_command)(void *) = nullptr;
	std::optional<PixelOracleSourceIdentity>
	(*pixel_oracle_source_identity)(void *) = nullptr;
	std::expected<present::SourceSurface, ClassicOpSessionError>
	(*compose_shown_surface)(
		void *,
		std::span<present::ComposedPixel, present::graphics_page_pixels>
	) = nullptr;
};

// Internal production-backend observation/control used only by the supplied-
// resource route gate. The ordinary source-private operation table remains
// unable to synthesize child completion.
struct ClassicOpSessionTestHooks final {
	void *context = nullptr;
	std::optional<ClassicStorySessionScene> (*story_scene)(void *) = nullptr;
	std::optional<std::uint32_t> (*story_source_sample_count)(void *) = nullptr;
	std::optional<ClassicStorySessionErrorCode> (*story_failure_code)(void *) =
		nullptr;
	std::optional<std::uint8_t> (*story_failure_detail_code)(void *) = nullptr;
	std::expected<void, ClassicOpSessionError>
	(*force_story_main_outcome)(void *, ClassicStoryRouteTestOutcome) = nullptr;
	std::expected<void, ClassicOpSessionError>
	(*force_vs_main_outcome)(void *, ClassicOpVsRouteTestOutcome) = nullptr;
	std::optional<GameInit> (*vs_selection_resident)(void *) = nullptr;
	std::optional<GameInit> (*vs_terminal_resident)(void *) = nullptr;
	bool (*online_high_score_menu)(void *) = nullptr;
};

class ClassicOpSession final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpSession>,
		ClassicOpSessionError
	> create(
		GameInit resident,
		std::unique_ptr<MainResourceLoader> loader,
		MainCompiledContent compiled_content,
		std::span<const std::byte> font_rom,
		std::span<const std::byte> mainl_executable,
		std::span<const std::byte> op_executable,
		std::filesystem::path config_path = "YUME.CFG",
		std::filesystem::path score_path = "YUME.NEM",
		bool fm_available = true,
		present::KotohimeBombFadeCompatibility kotohime_compatibility = {},
		NewClassicReplayRecorder *replay_recorder = nullptr,
		bool new_classic_practice_route = false,
		std::filesystem::path new_classic_score_path = {},
		std::filesystem::path language_overlay_path = {},
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets = {},
		const ClassicRunnerRequest *runner_request = nullptr
	);
	~ClassicOpSession();

	ClassicOpSession(const ClassicOpSession &) = delete;
	ClassicOpSession &operator=(const ClassicOpSession &) = delete;
	ClassicOpSession(ClassicOpSession &&) = delete;
	ClassicOpSession &operator=(ClassicOpSession &&) = delete;

	[[nodiscard]] std::expected<
		ClassicOpSessionTick,
		ClassicOpSessionError
	> consume(const ClassicOpSessionInput &input);
	[[nodiscard]] std::optional<ClassicAudioCommand> take_audio_command();
	[[nodiscard]] std::optional<NewClassicKeyConfig>
	take_key_config_update() noexcept;
	[[nodiscard]] std::optional<NewClassicJoyConfig>
	take_joy_config_update() noexcept;
	[[nodiscard]] std::optional<NewClassicDeviceAssignment>
	take_device_assignment_update() noexcept;
	[[nodiscard]] std::optional<bool> take_reduced_flash_update() noexcept;
	[[nodiscard]] std::optional<std::uint8_t>
	take_master_volume_update() noexcept;
	[[nodiscard]] std::optional<bool> take_mute_when_unfocused_update() noexcept;
	[[nodiscard]] std::optional<NewClassicWindowPresentation>
	take_window_presentation_update() noexcept;
	[[nodiscard]] std::optional<std::filesystem::path>
	take_open_folder_request() noexcept;
	[[nodiscard]] bool take_online_identity_reset_request() noexcept;
	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	merge_online_board(NewClassicOnlineCacheDocument document);
	[[nodiscard]] std::optional<NewClassicOnlineReplayRecord>
	take_online_replay_download_request() noexcept;
	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	report_online_replay_download(
		const NewClassicOnlineReplayId &replay_id,
		NewClassicOnlineReplayDownloadOutcome outcome
	);
	[[nodiscard]] std::optional<NewClassicReplayUploadRequest>
	take_online_replay_upload_request() noexcept;
	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	report_online_replay_upload(
		std::uint8_t slot,
		NewClassicReplayUploadOutcome outcome
	);
	[[nodiscard]] std::optional<NewClassicOnlineReplayRecord>
	take_online_replay_watch_request() noexcept;
	[[nodiscard]] std::expected<void, ClassicOpSessionError>
	report_online_replay_watch(
		const NewClassicOnlineReplayId &replay_id,
		std::optional<formats::ReplayDocument> document
	);
	[[nodiscard]] std::shared_ptr<const NewClassicReplaySaveArt>
	replay_save_art() const noexcept;
	[[nodiscard]] std::expected<
		present::SourceSurface,
		ClassicOpSessionError
	> compose_shown_surface(
		std::span<present::ComposedPixel, present::graphics_page_pixels> output
	) const;

	[[nodiscard]] ClassicOpSessionScene scene() const noexcept;
	[[nodiscard]] ClassicOpSessionStatus status() const noexcept;
	[[nodiscard]] PixelOracleSourceIdentity
	pixel_oracle_source_identity() const noexcept;

private:
	friend struct ClassicOpSessionTestAccess;
	struct Impl;
	explicit ClassicOpSession(std::unique_ptr<Impl> impl) noexcept;
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpSession>,
		ClassicOpSessionError
	> create_with_backend(
		std::shared_ptr<void> backend_owner,
		ClassicOpSessionBackendOperations operations,
		ClassicOpSessionTestHooks test_hooks = {},
		NewClassicReplayRecorder *replay_recorder = nullptr,
		bool new_classic_practice_route = false
	);

	std::unique_ptr<Impl> impl_;
};

struct ClassicOpSessionTestAccess final {
	[[nodiscard]] static ClassicOpVsSurfaceSource vs_surface_source(
		bool win,
		bool win_completion,
		bool main_process
	) noexcept;
	[[nodiscard]] static bool attract_input_present(
		const ClassicMainInputSample &input
	) noexcept;
	[[nodiscard]] static std::optional<ClassicStorySessionScene> story_scene(
		const ClassicOpSession &session
	) noexcept;
	[[nodiscard]] static std::optional<std::uint32_t>
	story_source_sample_count(const ClassicOpSession &session) noexcept;
	[[nodiscard]] static std::optional<ClassicStorySessionErrorCode>
	story_failure_code(const ClassicOpSession &session) noexcept;
	[[nodiscard]] static std::optional<std::uint8_t>
	story_failure_detail_code(const ClassicOpSession &session) noexcept;
	[[nodiscard]] static std::expected<void, ClassicOpSessionError>
	force_story_main_outcome(
		ClassicOpSession &session,
		ClassicStoryRouteTestOutcome outcome
	);
	[[nodiscard]] static std::expected<void, ClassicOpSessionError>
	force_vs_main_outcome(
		ClassicOpSession &session,
		ClassicOpVsRouteTestOutcome outcome
	);
	[[nodiscard]] static std::optional<GameInit> vs_selection_resident(
		const ClassicOpSession &session
	) noexcept;
	[[nodiscard]] static std::optional<GameInit> vs_terminal_resident(
		const ClassicOpSession &session
	) noexcept;
	[[nodiscard]] static bool online_high_score_menu(
		const ClassicOpSession &session
	) noexcept;
	[[nodiscard]] static CoreInput vs_cpu_main_input(
		const GameInit &resident,
		const ClassicMainInputSample &input,
		InputContext context,
		bool new_classic = false
	) noexcept;
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpSession>,
		ClassicOpSessionError
	> create(
		std::shared_ptr<void> backend_owner,
		ClassicOpSessionBackendOperations operations
	)
	{
		return ClassicOpSession::create_with_backend(
			std::move(backend_owner), operations
		);
	}
};

} // namespace th03::app

#endif
