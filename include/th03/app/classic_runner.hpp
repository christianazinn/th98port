#ifndef TH03_APP_CLASSIC_RUNNER_HPP
#define TH03_APP_CLASSIC_RUNNER_HPP

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>

#include "th03/app/classic_audio.hpp"
#include "th03/app/experience_menu.hpp"
#include "th03/app/main_compiled_content.hpp"
#include "th03/app/main_process.hpp"
#include "th03/app/new_classic_assets.hpp"
#include "th03/app/new_classic_frontend.hpp"
#include "th03/app/new_classic_key_config_store.hpp"
#include "th03/formats/replay.hpp"
#include "th03/app/resources.hpp"
#include "th03/app/tick_input.hpp"
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
	// Complete authenticated Replay Patch 0.4.12 AZINN.DAT. Classic ignores
	// this profile. An empty New Classic path derives AZINN.DAT under data_root.
	std::filesystem::path replay_patch_assets{};
	// Replay Patch's native REPLAY/ directory. New Classic derives this under
	// data_root when omitted; Classic never opens it.
	std::filesystem::path replay_root{};
	std::filesystem::path config{"YUME.CFG"};
	std::filesystem::path score{"YUME.NEM"};
	// New Classic's versioned per-character store. An empty value derives
	// YUME.NEW.NEM beside [score], keeping Classic's 824-byte file untouched.
	std::filesystem::path new_classic_score{};
};

enum class ClassicRunnerInitialRoute : std::uint8_t {
	title,
	new_classic_practice,
};

struct ClassicRunnerRequest final {
	ClassicContentPaths content{};
	std::uint32_t seed = 1U;
	// False selects the source's sound-unavailable fallback paths while still
	// publishing and requiring the caller to drain every semantic command.
	bool fm_available = false;
	present::KotohimeBombFadeCompatibility kotohime_compatibility{};
	// Host/application policy only. Deterministic gameplay never receives this
	// identity; New Classic adapters apply recorded transforms at the boundary.
	ExperienceProfile experience = ExperienceProfile::classic;
	// Replay Patch Autofire selection. Classic must leave this at zero.
	std::uint8_t autofire_mask = 0U;
	// Replay Patch's MAIN-only warning-flash reduction (tone 150 -> 100).
	bool reduced_flash = false;
	std::optional<NewClassicKeyConfig> new_classic_key_config{};
	NewClassicKeyConfig new_classic_key_defaults{};
	std::uint16_t new_classic_maximum_key = 0U;
	std::uint16_t new_classic_capture_cancel_key = 0U;
	NewClassicHostKeyName new_classic_key_name = nullptr;
	void *new_classic_key_name_context = nullptr;
	// New Classic may enter one Replay Patch-owned route without first exposing
	// Classic's title. The shared authenticated session still owns every child.
	ClassicRunnerInitialRoute initial_route = ClassicRunnerInitialRoute::title;
};

enum class ClassicContentFile : std::uint8_t {
	none,
	main_executable,
	mainl_executable,
	op_executable,
	font_rom,
	replay_patch_assets,
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
	std::optional<present::SourceSurfaceError> surface_error{};
};

enum class ClassicRunnerStatus : std::uint8_t {
	running,
	quit_requested,
	session_complete,
	failed,
};

struct ClassicRunnerConsumeResult final {
	std::uint64_t tick_index = 0U;
	TickTimestampNs deadline_ns = 0U;
	bool surface_changed = false;
	bool input_sampled = false;
	bool process_advanced = false;
	bool round_started = false;
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
	// Present only for New Classic after a complete recorded route. Taking the
	// document transfers it to the save/browser owner and enables a later run.
	[[nodiscard]] std::optional<formats::ReplayDocument>
	take_completed_replay();
	void classify_pending_replay_frame(bool deadline_missed) noexcept;
	[[nodiscard]] std::optional<NewClassicKeyConfig>
	take_new_classic_key_config_update() noexcept;
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
