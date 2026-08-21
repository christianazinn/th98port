#ifndef TH03_APP_CLASSIC_RUNNER_HPP
#define TH03_APP_CLASSIC_RUNNER_HPP

#include <cstdint>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "th03/app/classic_audio.hpp"
#include "th03/app/experience_profile.hpp"
#include "th03/app/main_compiled_content.hpp"
#include "th03/app/main_process.hpp"
#include "th03/app/new_classic_assets.hpp"
#include "th03/app/new_classic_device_store.hpp"
#include "th03/app/new_classic_frontend.hpp"
#include "th03/app/new_classic_key_config_store.hpp"
#include "th03/app/new_classic_joy_config_store.hpp"
#include "th03/app/new_classic_messages.hpp"
#include "th03/app/new_classic_settings.hpp"
#include "th03/app/pixel_oracle_identity.hpp"
#include "th03/formats/replay.hpp"
#include "th03/app/resources.hpp"
#include "th03/app/tick_input.hpp"
#include "th03/app/update_ui.hpp"
#include "th03/present/main_bomb_palette.hpp"
#include "th03/present/source_surface.hpp"

namespace th03::app {

// Host presentation may be slower than the nominal PC-98 display. Classic
// retains every source deadline and publishes only the newest completed frame;
// this keeps gameplay and the PMD service on the PC-98 timeline.
inline constexpr TickInputCatchUpPolicy classic_interactive_catch_up_policy =
	TickInputCatchUpPolicy::preserve_all;

struct ClassicContentPaths final {
	std::filesystem::path data_root{};
	std::filesystem::path main_executable{};
	std::filesystem::path mainl_executable{};
	std::filesystem::path op_executable{};
	std::filesystem::path font_rom{};
	// Optional authenticated Replay Patch 0.4.12 TH3EN.DAT. Classic ignores
	// this profile; New Classic may select it through the persisted language.
	std::filesystem::path language_overlay{};
	// Optional T3LNG001 semantic UI package. New Classic validates it against
	// the authenticated MNUFONT; Classic never opens it.
	std::filesystem::path language_pack{};
	// Optional T3GTL001 source-constrained graphics-text catalog. It is applied
	// only with New Classic's authenticated English asset profile.
	std::filesystem::path game_text_pack{};
	// Optional complete T3CTP001 scripted-dialogue catalog. New Classic applies
	// it only after all 28 selected source scripts validate atomically.
	std::filesystem::path cutscene_text_pack{};
	// Complete authenticated Replay Patch 0.4.12 AZINN.DAT. Classic ignores
	// this profile. An empty New Classic path derives AZINN.DAT under data_root.
	std::filesystem::path replay_patch_assets{};
	// Repo-owned retained-menu artwork. The SDL product derives this as PODDUI
	// below its executable/install root, independent of the launch directory.
	std::filesystem::path new_classic_menu_assets{};
	// Replay Patch's native REPLAY/ directory. New Classic derives this under
	// data_root when omitted; Classic never opens it.
	std::filesystem::path replay_root{};
	std::filesystem::path config{"YUME.CFG"};
	// Native New Classic settings. An empty value derives PODD.CFG beside the
	// selected Classic config without changing that source-compatible file.
	std::filesystem::path new_classic_settings{};
	std::filesystem::path score{"YUME.NEM"};
	// New Classic's versioned per-character store. An empty value derives
	// YUME.NEW.NEM beside [score], keeping Classic's 824-byte file untouched.
	std::filesystem::path new_classic_score{};
};

enum class ClassicRunnerInitialRoute : std::uint8_t {
	title,
	new_classic_practice,
	new_classic_replay,
};

struct ClassicRunnerInitialReplay final {
	formats::ReplayDocument document{};
	std::size_t checkpoint_index = 0U;
};

struct ClassicRunnerRequest final {
	ClassicContentPaths content{};
	// RES_YUME.COM zero-fills the original resident structure on power-on.
	std::uint32_t seed = 0U;
	// False selects the source's sound-unavailable fallback paths while still
	// publishing and requiring the caller to drain every semantic command.
	bool fm_available = false;
	present::KotohimeBombFadeCompatibility kotohime_compatibility{};
	// Host/application policy only. Deterministic gameplay never receives this
	// identity; New Classic adapters apply recorded transforms at the boundary.
	ExperienceProfile experience = ExperienceProfile::classic;
	// Named deterministic mechanics contract. Classic accepts only stock; the
	// initial Arrange prototype is New Classic-only and deliberately cannot
	// enter replay, online-upload, or netplay routes.
	GameplayRuleset gameplay_ruleset = stock_gameplay_ruleset;
	// Replay Patch Autofire selection. Classic must leave this at zero.
	std::uint8_t autofire_mask = 0U;
	// Replay Patch's MAIN-only warning-flash reduction (tone 150 -> 100).
	bool reduced_flash = false;
	std::optional<NewClassicKeyConfig> new_classic_key_config{};
	NewClassicKeyConfig new_classic_key_defaults{};
	std::optional<NewClassicJoyConfig> new_classic_joy_config{};
	NewClassicJoyConfig new_classic_joy_defaults{};
	std::optional<NewClassicDeviceAssignment> new_classic_device_assignment{};
	NewClassicDeviceAssignment new_classic_device_defaults{};
	NewClassicControllerCatalogRead new_classic_controller_catalog = nullptr;
	void *new_classic_controller_context = nullptr;
	std::uint16_t new_classic_maximum_key = 0U;
	std::uint16_t new_classic_capture_cancel_key = 0U;
	NewClassicHostKeyName new_classic_key_name = nullptr;
	void *new_classic_key_name_context = nullptr;
	// Validated per-board snapshots loaded by the platform before session
	// construction. The source-time owner treats them as immutable read-only
	// input and never performs network or filesystem work while advancing.
	std::vector<NewClassicOnlineCacheDocument> new_classic_online_boards{};
	// Authenticated accepted submissions for the current player. This private
	// view is memory-only and is never merged into public best-per-profile rows.
	std::vector<NewClassicOnlineCacheDocument>
		new_classic_online_my_records{};
	// Host capability only. When true, Replay exposes an explicit Upload
	// destination whose immutable document request is completed outside source
	// time. No endpoint or credential crosses into the retained owner.
	bool new_classic_online_upload_available = false;
	// Immutable host capability only. The retained Options owner never reads a
	// credential; it may merely emit one confirmed reset request when true.
	bool new_classic_online_identity_reset_available = false;
	// Immutable host capability. The retained Options owner only exposes a
	// typed check request; trust, network, storage, and installation remain
	// outside source time.
	bool new_classic_update_available = false;
	// New Classic may enter one Replay Patch-owned route without first exposing
	// Classic's title. The shared authenticated session still owns every child.
	ClassicRunnerInitialRoute initial_route = ClassicRunnerInitialRoute::title;
	std::optional<ClassicRunnerInitialReplay> initial_replay{};
};

enum class ClassicContentFile : std::uint8_t {
	none,
	main_executable,
	mainl_executable,
	op_executable,
	font_rom,
	replay_patch_assets,
	language_pack,
	game_text_pack,
	cutscene_text_pack,
};

enum class ClassicRunnerErrorCode : std::uint8_t {
	invalid_request,
	file_missing,
	file_not_regular,
	file_size_failed,
	file_size_mismatch,
	file_open_failed,
	file_read_failed,
	compiled_content_failed,
	replay_patch_assets_failed,
	language_pack_failed,
	game_text_pack_failed,
	cutscene_text_pack_failed,
	session_failed,
	invalid_sample_order,
	runner_not_running,
	unexpected_session_status,
	surface_failed,
};

// The private scene/session error hierarchy never crosses this public runner
// boundary. [detail_code] retains its stable top-level numeric code for a
// diagnostic, while public nested resource/content/process/surface failures
// remain typed and contain no original path or bytes.
struct ClassicRunnerError final {
	ClassicRunnerErrorCode code = ClassicRunnerErrorCode::invalid_request;
	ClassicContentFile file = ClassicContentFile::none;
	std::uint8_t detail_code = 0U;
	std::optional<ResourceLoadError> resource_error{};
	std::optional<MainCompiledContentError> compiled_content_error{};
	std::optional<MainProcessError> process_error{};
	std::optional<NewClassicAssetsError> replay_patch_assets_error{};
	std::optional<NewClassicMessagePackError> language_pack_error{};
	std::optional<NewClassicGameTextPackError> game_text_pack_error{};
	std::optional<NewClassicCutsceneTextPackError> cutscene_text_pack_error{};
	std::optional<present::SourceSurfaceError> surface_error{};
};

enum class ClassicRunnerStatus : std::uint8_t {
	running,
	quit_requested,
	session_complete,
	switch_to_classic,
	netplay_requested,
	failed,
};

struct ClassicRunnerConsumeResult final {
	std::uint64_t tick_index = 0U;
	TickTimestampNs deadline_ns = 0U;
	bool surface_changed = false;
	bool input_sampled = false;
	bool process_advanced = false;
	bool round_started = false;
	// A synchronous source-owned reconstruction completed behind a black
	// boundary. The platform must discard elapsed wall-clock debt before it
	// publishes or advances the reconstructed owner.
	bool clock_rebase_requested = false;
	PixelOracleIdentity pixel_oracle_identity{};
	ClassicRunnerStatus status = ClassicRunnerStatus::failed;
};

// Platform-neutral owner of the complete Classic application lifecycle. It
// authenticates caller-supplied process/content bytes once, keeps every source
// scene private, and consumes one scheduler sample per call. Audio commands
// must be drained before the following sample; an audio engine supplies the
// status observed by source measure/volume waits.
class ClassicRunner final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicRunner>,
		ClassicRunnerError
	> load(ClassicRunnerRequest request);

	~ClassicRunner();
	ClassicRunner(const ClassicRunner &) = delete;
	ClassicRunner &operator=(const ClassicRunner &) = delete;
	ClassicRunner(ClassicRunner &&) = delete;
	ClassicRunner &operator=(ClassicRunner &&) = delete;

	[[nodiscard]] std::expected<
		ClassicRunnerConsumeResult,
		ClassicRunnerError
	> consume(
		const LogicalTickInput &sample,
		ClassicAudioStatus audio = {.volume = 255U}
	);
	[[nodiscard]] std::optional<ClassicAudioCommand> take_audio_command();
	[[nodiscard]] std::expected<
		present::SourceSurface,
		ClassicRunnerError
	> compose_shown_surface(
		std::span<present::ComposedPixel, present::graphics_page_pixels> output
	) const;
	[[nodiscard]] ClassicRunnerStatus status() const noexcept;
	// Resume the retained patched OP owner after the host-owned Online Match
	// root returns. This emits no title-song restart command.
	[[nodiscard]] std::expected<void, ClassicRunnerError>
	resume_from_netplay();
	[[nodiscard]] PixelOracleIdentity pixel_oracle_bootstrap_identity();
	// Present only for New Classic after a complete recorded route. Taking the
	// document transfers it to the save/browser owner and enables a later run.
	[[nodiscard]] std::optional<formats::ReplayDocument>
	take_completed_replay();
	void classify_pending_replay_frame(bool deadline_missed) noexcept;
	[[nodiscard]] std::optional<NewClassicKeyConfig>
	take_new_classic_key_config_update() noexcept;
	[[nodiscard]] std::optional<NewClassicJoyConfig>
	take_new_classic_joy_config_update() noexcept;
	[[nodiscard]] std::optional<NewClassicDeviceAssignment>
	take_new_classic_device_assignment_update() noexcept;
	[[nodiscard]] std::optional<std::uint8_t>
	take_new_classic_master_volume_update() noexcept;
	[[nodiscard]] std::optional<bool>
	take_new_classic_mute_when_unfocused_update() noexcept;
	[[nodiscard]] std::optional<NewClassicWindowPresentation>
	take_new_classic_window_presentation_update() noexcept;
	[[nodiscard]] std::optional<std::filesystem::path>
	take_new_classic_open_folder_request() noexcept;
	[[nodiscard]] bool
	take_new_classic_online_identity_reset_request() noexcept;
	[[nodiscard]] bool take_new_classic_update_check_request() noexcept;
	[[nodiscard]] std::expected<void, ClassicRunnerError>
	begin_new_classic_update_check();
	[[nodiscard]] std::expected<void, ClassicRunnerError>
	report_new_classic_update_check(NewClassicUpdateCheckOutcome outcome);
	// Host-owned online work may complete between logical ticks. This replaces
	// one validated immutable board for a later HiScore entry; it never mutates
	// an already-visible menu and performs no filesystem or network work.
	[[nodiscard]] std::expected<void, ClassicRunnerError>
	merge_new_classic_online_board(NewClassicOnlineCacheDocument document);
	[[nodiscard]] std::expected<void, ClassicRunnerError>
	merge_new_classic_online_my_records(
		NewClassicOnlineCacheDocument document
	);
	[[nodiscard]] std::expected<void, ClassicRunnerError>
	clear_new_classic_online_my_records();
	[[nodiscard]] std::optional<NewClassicOnlineReplayRecord>
	take_new_classic_online_replay_download_request() noexcept;
	[[nodiscard]] std::expected<void, ClassicRunnerError>
	report_new_classic_online_replay_download(
		const NewClassicOnlineReplayId &replay_id,
		NewClassicOnlineReplayDownloadOutcome outcome
	);
	[[nodiscard]] std::optional<NewClassicReplayUploadRequest>
	take_new_classic_online_replay_upload_request() noexcept;
	[[nodiscard]] std::expected<void, ClassicRunnerError>
	report_new_classic_online_replay_upload(
		std::uint8_t slot,
		NewClassicReplayUploadOutcome outcome
	);
	[[nodiscard]] std::optional<NewClassicOnlineReplayRecord>
	take_new_classic_online_replay_watch_request() noexcept;
	[[nodiscard]] std::expected<void, ClassicRunnerError>
	report_new_classic_online_replay_watch(
		const NewClassicOnlineReplayId &replay_id,
		std::optional<formats::ReplayDocument> document
	);
	[[nodiscard]] std::shared_ptr<const NewClassicReplaySaveArt>
	new_classic_replay_save_art() const noexcept;

private:
	friend struct ClassicRunnerTestAccess;
	struct Impl;
	explicit ClassicRunner(std::unique_ptr<Impl> impl) noexcept;
	std::unique_ptr<Impl> impl_;
};

} // namespace th03::app

#endif
