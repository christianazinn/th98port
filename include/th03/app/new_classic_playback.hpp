#ifndef TH03_APP_NEW_CLASSIC_PLAYBACK_HPP
#define TH03_APP_NEW_CLASSIC_PLAYBACK_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

#include "th03/app/classic_runner.hpp"
#include "th03/app/classic_config.hpp"
#include "th03/app/main_process.hpp"
#include "th03/app/new_classic_language.hpp"
#include "th03/app/pixel_oracle_identity.hpp"
#include "th03/formats/replay.hpp"

namespace th03::app {

struct NewClassicReplayPlaybackTestAccess;

struct NewClassicPlaybackRequest final {
	ClassicContentPaths content{};
	formats::ReplayDocument document{};
	std::size_t checkpoint_index = 0U;
	present::KotohimeBombFadeCompatibility kotohime_compatibility{};
	bool reduced_flash = false;
	// Presentation-only viewer choice. It is deliberately not read from V13.
	ClassicLanguage language = ClassicLanguage::japanese;
};

enum class NewClassicPlaybackErrorCode : std::uint8_t {
	invalid_request,
	file_missing,
	file_read_failed,
	compiled_content_failed,
	mainl_content_failed,
	language_overlay_failed,
	replay_patch_assets_failed,
	static_content_failed,
	resource_failed,
	process_failed,
	driver_failed,
	interstitial_failed,
	replay_contract_failed,
	surface_failed,
};

struct NewClassicPlaybackError final {
	NewClassicPlaybackErrorCode code =
		NewClassicPlaybackErrorCode::invalid_request;
	std::optional<MainCompiledContentError> compiled_content{};
	std::optional<NewClassicLanguageOverlayError> language_overlay{};
	std::optional<NewClassicAssetsError> replay_patch_assets{};
	std::optional<ResourceLoadError> resource{};
	std::optional<MainProcessError> process{};
	std::optional<present::SourceSurfaceError> surface{};
	std::optional<std::uint8_t> interstitial_detail{};
};

enum class NewClassicPlaybackStatus : std::uint8_t {
	running,
	complete,
	aborted,
	application_quit,
	failed,
};

struct NewClassicPlaybackTick final {
	bool surface_changed = false;
	bool process_advanced = false;
	NewClassicPlaybackStatus status = NewClassicPlaybackStatus::failed;
	std::optional<PixelOracleSourceIdentity> pixel_oracle_source_identity{};
};

// Visual V13 player over the same authenticated MAIN and typed MAINL owners
// used by live Classic. Later checkpoint starts reconstruct directly from the
// stored compact snapshot; host abort/fast-forward state never enters the
// replay. Ordinary clear/interstitial owners retain their donor input domains;
// 0.4.12 Game Over playback bypasses registration and Continue entirely.
// Viewing never publishes score persistence.
class NewClassicReplayPlayback final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<NewClassicReplayPlayback>,
		NewClassicPlaybackError
	> load(NewClassicPlaybackRequest request);

	~NewClassicReplayPlayback();
	NewClassicReplayPlayback(const NewClassicReplayPlayback &) = delete;
	NewClassicReplayPlayback &operator=(const NewClassicReplayPlayback &) = delete;
	NewClassicReplayPlayback(NewClassicReplayPlayback &&) = delete;
	NewClassicReplayPlayback &operator=(NewClassicReplayPlayback &&) = delete;

	[[nodiscard]] std::expected<
		NewClassicPlaybackTick,
		NewClassicPlaybackError
	> advance(
		ClassicAudioStatus audio,
		bool abort_requested,
		bool application_quit_requested,
		CoreInput live_interface_input = {}
	);
	[[nodiscard]] std::optional<ClassicAudioCommand> take_audio_command();
	[[nodiscard]] std::expected<
		present::SourceSurface,
		NewClassicPlaybackError
	> compose_shown_surface(
		std::span<
			present::ComposedPixel,
			present::graphics_page_pixels
		> output
	) const;
	void set_fast_forward(bool enabled) noexcept;
	[[nodiscard]] bool fast_forward() const noexcept;
	[[nodiscard]] std::size_t fast_forward_step_count() const noexcept;
	[[nodiscard]] NewClassicPlaybackStatus status() const noexcept;
	[[nodiscard]] std::uint32_t sample_index() const noexcept;
	[[nodiscard]] PixelOracleSourceIdentity
	pixel_oracle_source_identity() const noexcept;
	[[nodiscard]] std::optional<GameInit> take_exit_resident() noexcept;

private:
	friend struct NewClassicReplayPlaybackTestAccess;
	struct Impl;
	[[nodiscard]] static std::expected<
		std::unique_ptr<NewClassicReplayPlayback>,
		NewClassicPlaybackError
	> load_internal(
		NewClassicPlaybackRequest request,
		bool suppress_preroll_presentation
	);
	explicit NewClassicReplayPlayback(std::unique_ptr<Impl> impl) noexcept;
	std::unique_ptr<Impl> impl_;
};

} // namespace th03::app

#endif
