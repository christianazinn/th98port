#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_POST_MATCH_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_POST_MATCH_HPP

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "th03/app/content/authentic_text.hpp"
#include "th03/app/classic/classic_audio.hpp"
#include "th03/app/direct_vs/direct_vs_launch.hpp"
#include "th03/app/new_classic/content/new_classic_game_text.hpp"
#include "th03/app/new_classic/content/new_classic_language.hpp"
#include "th03/app/content/resources.hpp"
#include "th03/core/game.hpp"
#include "th03/present/source_surface.hpp"

namespace th03::app {

enum class NewClassicNetplayPostMatchStatus : std::uint8_t {
	running,
	complete,
	quit_requested,
};

enum class NewClassicNetplayPostMatchBoundary : std::uint8_t {
	immediate,
	vsync,
	audio_measure,
};

enum class NewClassicNetplayPostMatchErrorCode : std::uint8_t {
	invalid_request,
	file_missing,
	file_not_regular,
	file_size_failed,
	file_size_mismatch,
	file_open_failed,
	file_read_failed,
	font_invalid,
	language_overlay_failed,
	game_text_pack_failed,
	resource_load_failed,
	scene_failed,
	surface_failed,
	not_running,
};

struct NewClassicNetplayPostMatchError final {
	NewClassicNetplayPostMatchErrorCode code =
		NewClassicNetplayPostMatchErrorCode::invalid_request;
	std::optional<AuthenticTextError> font_error{};
	std::optional<NewClassicLanguageOverlayError> language_overlay_error{};
	std::optional<NewClassicGameTextPackError> game_text_pack_error{};
	std::optional<ResourceLoadError> resource_error{};
	std::optional<present::SourceSurfaceError> surface_error{};
};

struct NewClassicNetplayPostMatchRequest final {
	DirectVsContentPaths content{};
	GameInit process_start{};
	CanonicalState terminal{};
	SessionTransition transition{};
};

struct NewClassicNetplayPostMatchTick final {
	NewClassicNetplayPostMatchStatus status =
		NewClassicNetplayPostMatchStatus::running;
	NewClassicNetplayPostMatchBoundary boundary =
		NewClassicNetplayPostMatchBoundary::immediate;
	bool surface_changed = false;
	bool input_sampled = false;
};

// The terminal simulation proof is mutually acknowledged before this owner is
// constructed. From that point onward, peer departure is transport cleanup,
// not a failed match: the local winner scene and in-memory replay transaction
// must remain usable at either player's pace.
[[nodiscard]] constexpr bool
new_classic_netplay_peer_departure_is_fatal(bool post_match_owned) noexcept
{
	return !post_match_owned;
}

// Source-faithful VS MAINL winner owner for a mutually confirmed direct match.
// Networking and rollback end before construction; this owner advances only
// the ordinary local WIN scene and publishes completion after its final fade.
class NewClassicNetplayPostMatch final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<NewClassicNetplayPostMatch>,
		NewClassicNetplayPostMatchError
	> create(NewClassicNetplayPostMatchRequest request);

	~NewClassicNetplayPostMatch();
	NewClassicNetplayPostMatch(const NewClassicNetplayPostMatch &) = delete;
	NewClassicNetplayPostMatch &operator=(
		const NewClassicNetplayPostMatch &
	) = delete;
	NewClassicNetplayPostMatch(NewClassicNetplayPostMatch &&) = delete;
	NewClassicNetplayPostMatch &operator=(
		NewClassicNetplayPostMatch &&
	) = delete;

	[[nodiscard]] std::expected<
		NewClassicNetplayPostMatchTick,
		NewClassicNetplayPostMatchError
	> advance(
		ClassicAudioStatus audio,
		bool dismiss_pressed,
		bool app_quit_requested = false
	) noexcept;
	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command() noexcept;
	[[nodiscard]] std::expected<
		present::SourceSurface,
		NewClassicNetplayPostMatchError
	> compose_shown_surface(
		std::span<
			present::ComposedPixel,
			present::graphics_page_pixels
		> output
	) const;
	[[nodiscard]] NewClassicNetplayPostMatchStatus status() const noexcept;

private:
	friend struct NewClassicNetplayPostMatchTestAccess;
	struct Impl;
	explicit NewClassicNetplayPostMatch(std::unique_ptr<Impl> impl) noexcept;
	[[nodiscard]] static std::expected<
		std::unique_ptr<NewClassicNetplayPostMatch>,
		NewClassicNetplayPostMatchError
	> create_prepared(
		NewClassicNetplayPostMatchRequest request,
		std::span<const LoadedMainResource> resources,
		const present::AnkGlyphProvider &font,
		bool authenticate_resources,
		std::vector<std::byte> owned_font_rom = {},
		std::optional<NewClassicGameTextCatalog> owned_game_text = {},
		bool use_builtin_english_text = false
	);

	std::unique_ptr<Impl> impl_;
};

} // namespace th03::app

#endif
