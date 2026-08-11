#ifndef TH03_APP_CLASSIC_STORY_SESSION_INTERNAL_HPP
#define TH03_APP_CLASSIC_STORY_SESSION_INTERNAL_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

#include "classic_cutscene_scene_internal.hpp"
#include "classic_mainl_continue_internal.hpp"
#include "classic_mainl_stage_internal.hpp"
#include "classic_mainl_story_internal.hpp"
#include "classic_mainl_win_internal.hpp"
#include "mainl_compiled_text_internal.hpp"
#include "main_process_driver_internal.hpp"
#include "th03/app/resources.hpp"
#include "th03/present/source_surface.hpp"

namespace th03::app {

enum class ClassicStorySessionScene : std::uint8_t {
	stage_splash,
	main,
	win,
	cutscene,
	continue_menu,
	game_over,
	external_request,
	complete,
	failed,
};

enum class ClassicStoryExternalKind : std::uint8_t {
	interstage_chiyuri,
	interstage_yumemi,
	high_score_registration,
	ending,
	extra_ending,
};

struct ClassicStoryExternalRequest final {
	std::uint64_t token = 0U;
	ClassicStoryExternalKind kind =
		ClassicStoryExternalKind::high_score_registration;
	GameInit resident{};
	std::optional<GameInit> high_score_registration{};
};

enum class ClassicStorySessionStatus : std::uint8_t {
	running,
	waiting_for_external,
	complete,
	quit_requested,
	failed,
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
	continue_failed,
	game_over_failed,
	plan_failed,
	backend_contract_failed,
	session_not_running,
	invalid_external_token,
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
	std::optional<ClassicMainlContinueError> continue_error{};
	std::optional<ClassicMainlPlanError> plan_error{};
	std::optional<present::SourceSurfaceError> surface_error{};
};

struct ClassicStorySessionInput final {
	CoreInput main{};
	ClassicCutsceneInput cutscene{};
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
	(*advance_main)(void *, CoreInput) = nullptr;
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
	std::optional<ClassicAudioCommand> (*take_audio_command)(void *) =
		nullptr;
	std::expected<present::SourceSurface, ClassicStorySessionError>
	(*compose_shown_surface)(
		void *,
		std::span<
			present::ComposedPixel,
			present::graphics_page_pixels
		>
	) = nullptr;
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
		MainlStageTextAssets stage_text
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicStorySession>,
		ClassicStorySessionError
	> create(
		const MainStoryConfig &config,
		std::unique_ptr<MainResourceLoader> loader,
		MainCompiledContent compiled_content,
		std::span<const std::byte> font_rom,
		std::span<const std::byte> mainl_executable
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
	[[nodiscard]] std::expected<void, ClassicStorySessionError>
	complete_external(std::uint64_t token);
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
	[[nodiscard]] const std::optional<ClassicStoryExternalRequest> &
	external_request() const noexcept;

private:
	friend struct ClassicStorySessionTestAccess;
	struct Impl;
	explicit ClassicStorySession(std::unique_ptr<Impl> impl) noexcept;
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicStorySession>,
		ClassicStorySessionError
	> create_with_backend(
		const MainStoryConfig &config,
		std::shared_ptr<void> backend_owner,
		ClassicStorySessionBackendOperations operations
	);

	std::unique_ptr<Impl> impl_;
};

struct ClassicStorySessionTestAccess final {
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
