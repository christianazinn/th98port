#ifndef TH03_APP_NEW_CLASSIC_PLAYBACK_HPP
#define TH03_APP_NEW_CLASSIC_PLAYBACK_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

#include "th03/app/classic_runner.hpp"
#include "th03/app/main_process.hpp"
#include "th03/formats/replay.hpp"

namespace th03::app {

struct NewClassicPlaybackRequest final {
	ClassicContentPaths content{};
	formats::ReplayDocument document{};
	std::size_t checkpoint_index = 0U;
	present::KotohimeBombFadeCompatibility kotohime_compatibility{};
};

enum class NewClassicPlaybackErrorCode : std::uint8_t {
	invalid_request,
	file_missing,
	file_read_failed,
	compiled_content_failed,
	resource_failed,
	process_failed,
	driver_failed,
	replay_contract_failed,
	surface_failed,
};

struct NewClassicPlaybackError final {
	NewClassicPlaybackErrorCode code =
		NewClassicPlaybackErrorCode::invalid_request;
	std::optional<MainCompiledContentError> compiled_content{};
	std::optional<ResourceLoadError> resource{};
	std::optional<MainProcessError> process{};
	std::optional<present::SourceSurfaceError> surface{};
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
};

// Visual V13 player over the same authenticated MainProcess/driver used by
// live Classic. Later checkpoint starts reconstruct directly from the stored
// compact snapshot; host abort/fast-forward state never enters the replay.
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
	> advance(bool abort_requested, bool application_quit_requested);
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
	[[nodiscard]] NewClassicPlaybackStatus status() const noexcept;
	[[nodiscard]] std::uint32_t sample_index() const noexcept;

private:
	struct Impl;
	explicit NewClassicReplayPlayback(std::unique_ptr<Impl> impl) noexcept;
	std::unique_ptr<Impl> impl_;
};

} // namespace th03::app

#endif
