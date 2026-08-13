#ifndef TH03_APP_MAIN_PROCESS_HPP
#define TH03_APP_MAIN_PROCESS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

#include "th03/app/main_compiled_content.hpp"
#include "th03/app/classic_audio.hpp"
#include "th03/app/resources.hpp"
#include "th03/assets/types.hpp"
#include "th03/core/game.hpp"
#include "th03/present/main_bomb_palette.hpp"
#include "th03/present/main_round.hpp"

namespace th03::app {

class MainProcessStaticContent;
class MainStageHandoff;
struct MainProcessStoryAccess;
struct MainProcessClassicVsAccess;
struct MainProcessDemoAccess;
struct MainProcessReplayPresentationAccess;

// One fresh, human P1-vs-P2 MAIN process. Story and automatic-Demo entry are
// source-private and require completed MAINL handoffs; arbitrary CPU and
// snapshot-backed entry remain separate process policies because accepting
// them here would permit late attachment to presentation-private source state.
struct MainDirectVsConfig {
	MainPlayerSelection player_one{};
	MainPlayerSelection player_two{
		.character = MainCharacter::reimu,
		.palette = MainPaletteVariant::second,
	};
	std::uint32_t seed = 0U;
	// Source rank values are Easy=0, Normal=1, Hard=2, Lunatic=3.
	std::uint8_t rank = 1U;
	std::uint8_t key_mode = 0U;
	GameOptions game_options{};
	// Optional Kotohime research override. When supplied, the callback and its
	// context must remain valid for this process's complete lifetime.
	present::KotohimeBombFadeCompatibility kotohime_compatibility{};
};

// Checked conversion shared by process policy, resource selection, and Story
// scenario validation. Paletted source IDs are exactly 1..18.
[[nodiscard]] std::optional<MainPlayerSelection>
main_player_selection_from_paletted(std::int8_t paletted) noexcept;
[[nodiscard]] std::optional<std::int8_t> main_paletted_character(
	MainPlayerSelection selection
) noexcept;

// One source-fresh Story MAIN entry. Snapshot-backed replay/debug entry stays
// on the replay runner because it cannot reconstruct presentation-private
// process state. The source-private Classic session path pairs this value with
// one completed MAINL Raster handoff and campaign-owned static content.
struct MainStoryConfig {
	GameInit resident{};
	GameOptions game_options{};
	present::KotohimeBombFadeCompatibility kotohime_compatibility{};
};

// One explicitly recorded V13 checkpoint. This is the only public late-entry
// presentation path: the checkpoint must carry the replay format's complete
// compact round/runtime snapshots and is always run under recorded input.
struct MainReplayConfig final {
	GameInit checkpoint{};
	GameOptions game_options{
		.input_protocol = InputProtocol::recorded_replay,
	};
	// Playback starts each MAIN process from its process-entry checkpoint and
	// pre-rolls repeat-round checkpoints. Fresh process entry must replay Game
	// initialization so presentation observers receive their private pool facts.
	bool initialize_fresh = true;
	present::KotohimeBombFadeCompatibility kotohime_compatibility{};
};

[[nodiscard]] bool valid_main_replay_config(
	const MainReplayConfig &config
) noexcept;

[[nodiscard]] bool valid_main_story_config(
	const MainStoryConfig &config
) noexcept;

// Pure preflight shared by the external loader and the process factory. This
// prevents malformed application requests from touching supplied files before
// the same config would be rejected by MainProcess::create().
[[nodiscard]] bool valid_main_direct_vs_config(
	const MainDirectVsConfig &config
) noexcept;

enum class MainProcessErrorCode : std::uint8_t {
	invalid_config,
	invalid_runtime_argument,
	resource_set_mismatch,
	resource_bytes_missing,
	resource_integrity_failed,
	asset_decode_failed,
	enedat_decode_failed,
	font_rom_failed,
	gameft_prepare_failed,
	pi_palette_missing,
	pi_staging_failed,
	static_startup_failed,
	background_initialization_failed,
	presenter_setup_failed,
	game_create_failed,
	presentation_failed,
	repeat_startup_failed,
	core_operation_failed,
	audio_command_pending,
};

struct MainProcessError {
	MainProcessErrorCode code = MainProcessErrorCode::invalid_config;
	std::optional<MainResourceKey> resource{};
	std::optional<assets::AssetError> asset_error{};
	std::optional<Error> core_error{};
	present::MainRoundStatus presentation_status =
		present::MainRoundStatus::ready;
};

// Stable-address owner/coordinator for one complete MAIN process. The public
// factory is the authenticated direct-VS entry; source-private Classic Story,
// selected VS, and automatic-Demo factories supply an already completed MAINL
// Raster handoff. All paths publish no partially initialized Raster/TRAM state
// and expose no mutable Game, Raster, Tram, palette, or child presenter.
class MainProcess final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<MainProcess>,
		MainProcessError
	> create(
		const MainDirectVsConfig &config,
		std::span<const LoadedMainResource> resources,
		MainCompiledContent compiled_content,
		std::span<const std::byte> font_rom
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<MainProcess>,
		MainProcessError
	> create_replay(
		const MainReplayConfig &config,
		std::span<const LoadedMainResource> resources,
		MainCompiledContent compiled_content,
		std::span<const std::byte> font_rom
	);

	~MainProcess();
	MainProcess(const MainProcess &) = delete;
	MainProcess &operator=(const MainProcess &) = delete;
	MainProcess(MainProcess &&) = delete;
	MainProcess &operator=(MainProcess &&) = delete;

	// Every gameplay and interstitial sample is synchronously observed by the
	// retained MainRound presenter. A presentation failure is sticky/fatal.
	[[nodiscard]] std::expected<StepResult, MainProcessError> step(
		CoreInput input,
		InputContext context = InputContext::gameplay
	) noexcept;
	[[nodiscard]] std::expected<StepResult, MainProcessError>
	finish_deferred_frame(DeferredFrameAction action) noexcept;
	[[nodiscard]] bool deferred_frame_pending() const noexcept;

	// Exact route-1 pair: core reset first, then the process-scoped static/HUD
	// repeat transaction. All child private pools remain alive.
	[[nodiscard]] std::expected<void, MainProcessError>
	start_next_round() noexcept;
	[[nodiscard]] std::expected<void, MainProcessError>
	finish_main_process() noexcept;
	[[nodiscard]] std::expected<SessionTransition, MainProcessError>
	session_transition(
		std::optional<ContinueChoice> continue_choice = std::nullopt
	) const;

	[[nodiscard]] CanonicalState canonical_state() const;
	[[nodiscard]] std::uint32_t live_random_seed() const noexcept;
	[[nodiscard]] RoundStartObservation round_start_observation() const;
	[[nodiscard]] InputProtocol input_protocol() const noexcept;
	[[nodiscard]] std::expected<
		present::SourceSurface,
		present::SourceSurfaceError
	> compose_shown_surface(
		std::span<
			present::ComposedPixel,
			present::graphics_page_pixels
		> output
	) const;

	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] const present::Tram &tram() const noexcept;
	[[nodiscard]] present::MainRoundStatus presentation_status() const noexcept;
	[[nodiscard]] const std::optional<MainResultState> &last_result_state()
		const noexcept;
	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command() noexcept;

private:
	friend struct MainProcessTestAccess;
	friend struct MainProcessStoryAccess;
	friend struct MainProcessClassicVsAccess;
	friend struct MainProcessDemoAccess;
	friend struct MainProcessReplayPresentationAccess;
	struct Impl;
	explicit MainProcess(std::unique_ptr<Impl> impl) noexcept;
	[[nodiscard]] static std::expected<
		std::unique_ptr<MainProcess>,
		MainProcessError
	> create_impl(
		const MainDirectVsConfig &config,
		std::span<const LoadedMainResource> resources,
		MainCompiledContent compiled_content,
		std::span<const std::byte> font_rom,
		bool authenticate_resource_bytes
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<MainProcess>,
		MainProcessError
	> create_common(
		const GameInit &init,
		GameOptions game_options,
		present::KotohimeBombFadeCompatibility kotohime_compatibility,
		std::array<MainPlayerSelection, 2U> selections,
		const MainStageHandoff &handoff,
		std::span<const LoadedMainResource> resources,
		std::shared_ptr<const MainProcessStaticContent> static_content,
		std::uint8_t continues_used_digit,
		std::optional<ClassicAudioCommand> gameplay_song
	);
	void fail_presentation_for_test() noexcept;
	[[nodiscard]] std::optional<MainProcessError>
	presentation_precheck() const noexcept;

	std::unique_ptr<Impl> impl_;
};

} // namespace th03::app

#endif
