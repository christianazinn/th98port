#ifndef TH03_APP_CLASSIC_STORY_SESSION_INTERNAL_HPP
#define TH03_APP_CLASSIC_STORY_SESSION_INTERNAL_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>

#include "classic_cutscene_scene_internal.hpp"
#include "classic_mainl_continue_internal.hpp"
#include "classic_mainl_stage_internal.hpp"
#include "classic_mainl_story_internal.hpp"
#include "classic_mainl_win_internal.hpp"
#include "classic_registration_scene_internal.hpp"
#include "classic_staff_scene_internal.hpp"
#include "mainl_compiled_text_internal.hpp"
#include "main_process_driver_internal.hpp"
#include "th03/app/resources.hpp"
#include "th03/app/tick_input.hpp"
#include "th03/present/source_surface.hpp"

namespace th03::app {

class NewClassicReplayRecorder;

struct ClassicOpSessionBackend;
class MainProcessStaticContent;

enum class ClassicStorySessionScene : std::uint8_t {
	stage_splash,
	main,
	win,
	cutscene,
	staff_verdict,
	high_score_registration,
	continue_menu,
	game_over,
	complete,
	failed,
};

enum class ClassicStorySessionStatus : std::uint8_t {
	running,
	complete,
	quit_requested,
	failed,
};

enum class ClassicStoryRouteTestOutcome : std::uint8_t {
	player_win,
	player_loss,
	exit_to_operator,
};

enum class ClassicStorySessionErrorCode : std::uint8_t {
	invalid_config,
	invalid_backend,
	resource_load_failed,
	static_content_failed,
	stage_failed,
	main_failed,
	mainl_text_failed,
	win_failed,
	cutscene_failed,
	staff_failed,
	registration_failed,
	score_store_failed,
	continue_failed,
	game_over_failed,
	plan_failed,
	backend_contract_failed,
	session_not_running,
	surface_unavailable,
	surface_failed,
};

struct ClassicStorySessionError final {
	ClassicStorySessionErrorCode code =
		ClassicStorySessionErrorCode::invalid_config;
	std::optional<ResourceLoadError> resource_error{};
	std::optional<MainProcessError> main_error{};
	std::optional<MainlCompiledTextError> mainl_text_error{};
	std::optional<ClassicMainlStageError> stage_error{};
	std::optional<ClassicMainlWinError> win_error{};
	std::optional<ClassicCutsceneError> cutscene_error{};
	std::optional<ClassicStaffError> staff_error{};
	std::optional<ClassicRegistrationError> registration_error{};
	std::optional<NewClassicScoreStoreError> new_score_store_error{};
	std::optional<ClassicMainlContinueError> continue_error{};
	std::optional<ClassicMainlPlanError> plan_error{};
	std::optional<present::SourceSurfaceError> surface_error{};
};

struct ClassicMainInputSample final {
	CoreInput aggregate{};
	std::optional<PhysicalInputDomains> physical_domains{};
	bool restart_requested = false;
};

struct ClassicStorySessionInput final {
	ClassicMainInputSample main{};
	ClassicCutsceneInput cutscene{};
	ClassicStaffInput staff{};
	ClassicRegistrationInput registration{};
	ClassicMainlContinueInput continue_menu{};
	ClassicAudioStatus audio{};
	bool dismiss_pressed = false;
	bool app_quit_requested = false;
};

struct ClassicStorySessionTick final {
	ClassicStorySessionScene scene = ClassicStorySessionScene::failed;
	ClassicStorySessionStatus status = ClassicStorySessionStatus::failed;
	std::uint32_t source_sample_count = 0U;
	bool surface_changed = false;
	bool input_sampled = false;
	bool process_advanced = false;
	bool round_started = false;
	StepResult step_result{};
};

struct ClassicStoryBackendTick final {
	bool surface_changed = false;
	bool input_sampled = false;
	bool ready = false;
	bool stage_preload_requested = false;
	bool process_advanced = false;
	bool round_started = false;
	bool restart_requested = false;
	StepResult step_result{};
};

struct ClassicStoryTerminal final {
	GameInit process_start{};
	CanonicalState terminal{};
	SessionTransition transition{};
};

// Source-private operation seam. Production binds this table to the concrete
// backend below, which owns the loader, scenes, retained MainProcess, and its
// neutral driver. Only test access can supply another table.
struct ClassicStorySessionBackendOperations final {
	void *context = nullptr;
	std::expected<void, ClassicStorySessionError> (*start_stage)(
		void *, const MainStoryConfig &
	) = nullptr;
	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	(*advance_stage)(void *, bool) = nullptr;
	std::expected<void, ClassicStorySessionError> (*start_main)(
		void *, const MainStoryConfig &
	) = nullptr;
	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	(*advance_main)(void *, const ClassicMainInputSample &) = nullptr;
	std::expected<ClassicStoryTerminal, ClassicStorySessionError>
	(*main_terminal)(void *) = nullptr;
	std::expected<void, ClassicStorySessionError> (*start_win)(
		void *, const ClassicMainlWinContext &
	) = nullptr;
	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	(*advance_win)(void *, ClassicAudioStatus, bool) = nullptr;
	std::expected<void, ClassicStorySessionError> (*preload_stage)(
		void *, const MainStoryConfig &
	) = nullptr;
	std::expected<void, ClassicStorySessionError> (*finish_win)(
		void *
	) = nullptr;
	std::expected<void, ClassicStorySessionError> (*start_cutscene)(
		void *, MainlCutsceneResourceRequest
	) = nullptr;
	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	(*advance_cutscene)(void *, const ClassicCutsceneInput &) = nullptr;
	std::expected<void, ClassicStorySessionError> (*finish_cutscene)(
		void *
	) = nullptr;
	std::expected<void, ClassicStorySessionError> (*start_staff)(
		void *, const GameInit &
	) = nullptr;
	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	(*advance_staff)(void *, const ClassicStaffInput &) = nullptr;
	std::expected<void, ClassicStorySessionError> (*finish_staff)(
		void *
	) = nullptr;
	std::expected<void, ClassicStorySessionError> (*start_registration)(
		void *, const GameInit &
	) = nullptr;
	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	(*advance_registration)(
		void *, ClassicRegistrationInput, ClassicAudioStatus
	) = nullptr;
	std::expected<void, ClassicStorySessionError> (*finish_registration)(
		void *
	) = nullptr;
	std::expected<bool, ClassicStorySessionError> (*start_continue)(
		void *, std::uint8_t
	) = nullptr;
	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	(*advance_continue)(void *, ClassicMainlContinueInput) = nullptr;
	std::expected<ContinueChoice, ClassicStorySessionError>
	(*finish_continue)(void *) = nullptr;
	std::expected<SessionTransition, ClassicStorySessionError>
	(*resolve_continue)(void *, ContinueChoice) = nullptr;
	std::expected<void, ClassicStorySessionError> (*start_game_over)(
		void *
	) = nullptr;
	std::expected<ClassicStoryBackendTick, ClassicStorySessionError>
	(*advance_game_over)(void *, ClassicAudioStatus) = nullptr;
	std::expected<void, ClassicStorySessionError> (*finish_game_over)(
		void *
	) = nullptr;
	// Native single-process equivalent of the Replay Patch score-time journal
	// checkpoint. Optional for hermetic backends without persistence ownership.
	std::expected<void, ClassicStorySessionError> (*checkpoint_score_run)(
		void *
	) = nullptr;
	std::optional<ClassicAudioCommand> (*take_audio_command)(void *) =
		nullptr;
	std::optional<CanonicalState> (*main_canonical_state)(void *) = nullptr;
	std::expected<present::SourceSurface, ClassicStorySessionError>
	(*compose_shown_surface)(
		void *,
		std::span<
			present::ComposedPixel,
			present::graphics_page_pixels
		>
	) = nullptr;
};

// Internal integration-test capability. Production construction binds this to
// the concrete resource/scene backend; substituted unit-test operation tables
// do not gain a way to forge a child completion.
struct ClassicStorySessionTestHooks final {
	void *context = nullptr;
	std::expected<void, ClassicStorySessionError>
	(*force_main_outcome)(void *, ClassicStoryRouteTestOutcome) = nullptr;
};

class ClassicStorySession final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicStorySession>,
		ClassicStorySessionError
	> create(
		const MainStoryConfig &config,
		std::unique_ptr<MainResourceLoader> loader,
		MainCompiledContent compiled_content,
		std::span<const std::byte> font_rom,
		MainlStageTextAssets stage_text,
		MainlStaffCompiledAssets staff_compiled,
		std::filesystem::path score_path = "YUME.NEM",
		NewClassicReplayRecorder *replay_recorder = nullptr
	);
	// Replay-only continuation from a completed retained MAIN owner. The
	// session runs the exact typed MAINL plan but stops before constructing the
	// next MAIN, whose state must come from the next V13 checkpoint.
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicStorySession>,
		ClassicStorySessionError
	> create_replay_interstitial(
		const MainStoryConfig &process_config,
		std::shared_ptr<MainResourceLoader> loader,
		std::shared_ptr<const MainProcessStaticContent> static_content,
		MainlStageTextAssets stage_text,
		MainlStaffCompiledAssets staff_compiled,
		std::filesystem::path score_path,
		std::filesystem::path legacy_score_path,
		std::unique_ptr<MainProcess> process,
		std::unique_ptr<MainProcessDriver> driver
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicStorySession>,
		ClassicStorySessionError
	> create(
		const MainStoryConfig &config,
		std::unique_ptr<MainResourceLoader> loader,
		MainCompiledContent compiled_content,
		std::span<const std::byte> font_rom,
		std::span<const std::byte> mainl_executable,
		std::filesystem::path score_path = "YUME.NEM",
		NewClassicReplayRecorder *replay_recorder = nullptr
	);
	~ClassicStorySession();

	ClassicStorySession(const ClassicStorySession &) = delete;
	ClassicStorySession &operator=(const ClassicStorySession &) = delete;
	ClassicStorySession(ClassicStorySession &&) = delete;
	ClassicStorySession &operator=(ClassicStorySession &&) = delete;

	[[nodiscard]] std::expected<
		ClassicStorySessionTick,
		ClassicStorySessionError
	> consume(const ClassicStorySessionInput &input);
	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command();
	[[nodiscard]] std::expected<
		present::SourceSurface,
		ClassicStorySessionError
	> compose_shown_surface(
		std::span<
			present::ComposedPixel,
			present::graphics_page_pixels
		> output
	) const;

	[[nodiscard]] ClassicStorySessionScene scene() const noexcept;
	[[nodiscard]] ClassicStorySessionStatus status() const noexcept;
	[[nodiscard]] std::uint32_t source_sample_count() const noexcept;
	[[nodiscard]] std::optional<CanonicalState>
	main_canonical_state() const noexcept;
	[[nodiscard]] const GameInit &resident() const noexcept;
	[[nodiscard]] bool restart_requested() const noexcept;
private:
	friend struct ClassicStorySessionTestAccess;
	friend struct ClassicOpSessionBackend;
	struct Impl;
	explicit ClassicStorySession(std::unique_ptr<Impl> impl) noexcept;
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicStorySession>,
		ClassicStorySessionError
	> create_with_backend(
		const MainStoryConfig &config,
		std::shared_ptr<void> backend_owner,
		ClassicStorySessionBackendOperations operations,
		ClassicStorySessionTestHooks test_hooks = {},
		NewClassicReplayRecorder *replay_recorder = nullptr,
		bool stop_before_main = false,
		std::optional<ClassicMainlPlan> initial_plan = std::nullopt,
		std::optional<std::uint32_t> initial_source_sample_count = std::nullopt
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicStorySession>,
		ClassicStorySessionError
	> create_with_prepared_content(
		const MainStoryConfig &config,
		std::shared_ptr<MainResourceLoader> loader,
		std::shared_ptr<const MainProcessStaticContent> static_content,
		MainlStageTextAssets stage_text,
		MainlStaffCompiledAssets staff_compiled,
		std::filesystem::path score_path,
		NewClassicReplayRecorder *replay_recorder,
		std::filesystem::path legacy_score_path = {},
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets = {}
	);

	std::unique_ptr<Impl> impl_;
};

struct ClassicStorySessionTestAccess final {
	[[nodiscard]] static present::Raster normalize_main_cutscene_seed(
		const present::Raster &raster
	);
	[[nodiscard]] static ClassicCutsceneInput replay_cutscene_input(
		const ClassicCutsceneInput &current,
		const ClassicCutsceneInput &latched,
		bool input_due
	) noexcept;
	[[nodiscard]] static CoreInput story_main_input(
		const ClassicMainInputSample &input,
		InputContext context,
		bool new_classic = false
	) noexcept;
	[[nodiscard]] static std::expected<void, ClassicStorySessionError>
	force_main_outcome(
		ClassicStorySession &session,
		ClassicStoryRouteTestOutcome outcome
	);
	[[nodiscard]] static std::optional<ClassicStorySessionErrorCode>
	failure_code(const ClassicStorySession &session) noexcept;
	[[nodiscard]] static std::optional<std::uint8_t>
	failure_detail_code(const ClassicStorySession &session) noexcept;
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicStorySession>,
		ClassicStorySessionError
	> create(
		const MainStoryConfig &config,
		std::shared_ptr<void> backend_owner,
		ClassicStorySessionBackendOperations operations
	)
	{
		return ClassicStorySession::create_with_backend(
			config, std::move(backend_owner), operations
		);
	}
};

} // namespace th03::app

#endif
