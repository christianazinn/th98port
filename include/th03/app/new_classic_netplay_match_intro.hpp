#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_MATCH_INTRO_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_MATCH_INTRO_HPP

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

#include "th03/app/authentic_text.hpp"
#include "th03/app/classic_audio.hpp"
#include "th03/app/direct_vs_launch.hpp"
#include "th03/app/new_classic_language.hpp"
#include "th03/app/new_classic_messages.hpp"
#include "th03/app/resources.hpp"
#include "th03/present/source_surface.hpp"

namespace th03::app {

enum class NewClassicNetplayMatchIntroStatus : std::uint8_t {
	running,
	ready,
};

enum class NewClassicNetplayMatchIntroErrorCode : std::uint8_t {
	invalid_request,
	file_missing,
	file_not_regular,
	file_size_failed,
	file_size_mismatch,
	file_open_failed,
	file_read_failed,
	mainl_text_failed,
	font_invalid,
	language_overlay_failed,
	resource_load_failed,
	scene_failed,
	surface_failed,
	not_running,
};

struct NewClassicNetplayMatchIntroError final {
	NewClassicNetplayMatchIntroErrorCode code =
		NewClassicNetplayMatchIntroErrorCode::invalid_request;
	std::optional<AuthenticTextError> font_error{};
	std::optional<NewClassicLanguageOverlayError> language_overlay_error{};
	std::optional<ResourceLoadError> resource_error{};
	std::optional<present::SourceSurfaceError> surface_error{};
};

struct NewClassicNetplayMatchIntroRequest final {
	DirectVsContentPaths content{};
	MainDirectVsConfig process{};
	NewClassicLanguageAssetProfile language =
		NewClassicLanguageAssetProfile::english;
};

struct NewClassicNetplayMatchIntroTick final {
	NewClassicNetplayMatchIntroStatus status =
		NewClassicNetplayMatchIntroStatus::running;
	bool surface_changed = false;
	bool input_sampled = false;
};

// Local presentation owner for the ordinary VS MAINL character/stage splash.
// It finishes before frame-zero network proof and therefore never enters the
// deterministic input, rollback, replay, or simulation-digest domains.
class NewClassicNetplayMatchIntro final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<NewClassicNetplayMatchIntro>,
		NewClassicNetplayMatchIntroError
	> create(NewClassicNetplayMatchIntroRequest request);

	~NewClassicNetplayMatchIntro();
	NewClassicNetplayMatchIntro(const NewClassicNetplayMatchIntro &) = delete;
	NewClassicNetplayMatchIntro &operator=(
		const NewClassicNetplayMatchIntro &
	) = delete;
	NewClassicNetplayMatchIntro(NewClassicNetplayMatchIntro &&) = delete;
	NewClassicNetplayMatchIntro &operator=(
		NewClassicNetplayMatchIntro &&
	) = delete;

	[[nodiscard]] std::expected<
		NewClassicNetplayMatchIntroTick,
		NewClassicNetplayMatchIntroError
	> advance(bool dismiss_pressed = false) noexcept;
	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command() noexcept;
	[[nodiscard]] std::expected<
		present::SourceSurface,
		NewClassicNetplayMatchIntroError
	> compose_shown_surface(
		std::span<
			present::ComposedPixel,
			present::graphics_page_pixels
		> output
	) const;
	[[nodiscard]] NewClassicNetplayMatchIntroStatus status() const noexcept;

private:
	struct Impl;
	explicit NewClassicNetplayMatchIntro(std::unique_ptr<Impl> impl) noexcept;
	std::unique_ptr<Impl> impl_;
};

} // namespace th03::app

#endif
