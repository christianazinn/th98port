#ifndef TH03_APP_CLASSIC_MAINL_WIN_INTERNAL_HPP
#define TH03_APP_CLASSIC_MAINL_WIN_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>

#include "classic_mainl_story_internal.hpp"
#include "th03/app/classic_audio.hpp"
#include "th03/app/new_classic_game_text.hpp"
#include "th03/app/resources.hpp"
#include "th03/assets/types.hpp"
#include "th03/present/graphics_text.hpp"
#include "th03/present/raster.hpp"

namespace th03::app {

[[nodiscard]] constexpr NewClassicGameTextId classic_mainl_win_text_id(
	MainCharacter character,
	std::size_t block,
	std::size_t line
) noexcept
{
	const auto value = static_cast<std::size_t>(character);
	const auto first = static_cast<std::size_t>(MainCharacter::reimu);
	if(value < first) {
		return NewClassicGameTextId::count;
	}
	return new_classic_win_quote_text_id(value - first, block, line);
}

enum class ClassicMainlWinPhase : std::uint8_t {
	black_in,
	wait_measure_6,
	logo_fade,
	wait_measure_10,
	wait_measure_11,
	white_in,
	post_reveal_delay,
	stage_preload,
	waiting_for_input,
	black_out,
	ready,
	consumed,
	failed,
};

enum class ClassicMainlWinBoundary : std::uint8_t {
	// Source code advanced without waiting for a VSync.
	immediate,
	// Exactly one source VSync/frame_delay boundary was consumed.
	vsync,
	// snd_delay_until_measure() is busy-polling an active PMD driver.
	audio_measure,
	// The owning session must perform stage_splash_load() on this Raster.
	stage_preload,
};

enum class ClassicMainlWinErrorCode : std::uint8_t {
	invalid_context,
	resource_set_mismatch,
	resource_bytes_missing,
	resource_integrity_failed,
	asset_decode_failed,
	invalid_asset_geometry,
	message_prepare_failed,
	draw_failed,
	audio_command_pending,
	invalid_state,
};

struct ClassicMainlWinError final {
	ClassicMainlWinErrorCode code =
		ClassicMainlWinErrorCode::invalid_context;
	std::optional<MainResourceKey> resource{};
	std::optional<assets::AssetError> asset_error{};
	std::optional<present::GraphicsTextError> text_error{};
};

struct ClassicMainlWinTick final {
	ClassicMainlWinPhase phase = ClassicMainlWinPhase::failed;
	ClassicMainlWinBoundary boundary = ClassicMainlWinBoundary::immediate;
	bool surface_changed = false;
	bool input_sampled = false;
	bool dismissed = false;
	bool stage_preload_requested = false;
	bool ready = false;
};

// Source-private completion capability. It is published only after the final
// black-out and cannot be consumed until the queued WIN-song stop is drained.
class ClassicMainlWinCompletion final {
public:
	ClassicMainlWinCompletion(const ClassicMainlWinCompletion &) = delete;
	ClassicMainlWinCompletion &operator=(
		const ClassicMainlWinCompletion &
	) = delete;
	ClassicMainlWinCompletion(ClassicMainlWinCompletion &&) noexcept = default;
	ClassicMainlWinCompletion &operator=(
		ClassicMainlWinCompletion &&
	) noexcept = default;

	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] bool stage_splash_preloaded() const noexcept;

private:
	friend class ClassicMainlWinScene;
	friend struct ClassicStoryBackend;
	explicit ClassicMainlWinCompletion(
		present::Raster raster,
		bool stage_splash_preloaded
	) noexcept;

	present::Raster raster_{};
	bool stage_splash_preloaded_ = false;
};

// Fixed-storage, source-ordered MAINL win_animate_and_wait() equivalent. The
// caller must drain the initial load/play commands before the first advance.
// advance() represents one application presentation tick; it never consumes a
// gameplay/V13 input sample.
class ClassicMainlWinScene final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlWinScene>,
		ClassicMainlWinError
	> create(
		const ClassicMainlWinContext &context,
		std::span<const LoadedMainResource> resources,
		const present::AnkGlyphProvider &font,
		const NewClassicGameTextCatalog *localized_text = nullptr
	);
	~ClassicMainlWinScene();

	ClassicMainlWinScene(const ClassicMainlWinScene &) = delete;
	ClassicMainlWinScene &operator=(const ClassicMainlWinScene &) = delete;
	ClassicMainlWinScene(ClassicMainlWinScene &&) = delete;
	ClassicMainlWinScene &operator=(ClassicMainlWinScene &&) = delete;

	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command() noexcept;
	[[nodiscard]] std::expected<
		ClassicMainlWinTick,
		ClassicMainlWinError
	> advance(
		ClassicAudioStatus audio,
		bool dismiss_pressed
	) noexcept;

	[[nodiscard]] ClassicMainlWinPhase phase() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	// Only valid while phase() == stage_preload. The owning Classic session
	// performs the already-audited stage-splash load on this exact Raster, then
	// calls acknowledge_stage_preload() before any dismissal sample.
	[[nodiscard]] present::Raster &stage_preload_raster() noexcept;
	[[nodiscard]] std::expected<void, ClassicMainlWinError>
	acknowledge_stage_preload() noexcept;
	[[nodiscard]] std::optional<ClassicMainlWinCompletion>
	take_completion() noexcept;

private:
	friend struct ClassicMainlWinTestAccess;

	struct Prepared final {
		assets::Palette16Rgb4 palette_pre{};
		std::array<assets::PlanarCel4, 5U> logo_fade{};
		assets::PlanarCel4 logo_final{};
		assets::PlanarCel4 portrait{};
		assets::Palette16Rgb4 palette_post{};
		std::array<
			std::optional<present::PreparedGraphicsText8x16>,
			3U
		> message{};
	};

	explicit ClassicMainlWinScene(
		std::unique_ptr<Prepared> prepared
	) noexcept;
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlWinScene>,
		ClassicMainlWinError
	> create_impl(
		const ClassicMainlWinContext &context,
		std::span<const LoadedMainResource> resources,
		const present::AnkGlyphProvider &font,
		bool authenticate_resource_bytes,
		const NewClassicGameTextCatalog *localized_text
	);

	[[nodiscard]] bool install_palette(
		const assets::Palette16Rgb4 &palette
	) noexcept;
	[[nodiscard]] bool draw_logo(std::size_t cel) noexcept;
	[[nodiscard]] bool draw_reveal() noexcept;
	[[nodiscard]] bool draw_message() noexcept;
	void begin_measure_wait(
		ClassicMainlWinPhase phase,
		ClassicAudioStatus audio,
		std::uint16_t measure,
		std::uint16_t fallback_frames
	) noexcept;
	[[nodiscard]] bool measure_wait_done(
		ClassicAudioStatus audio,
		ClassicMainlWinBoundary &boundary
	) noexcept;
	[[nodiscard]] bool push_audio_command(ClassicAudioCommand command) noexcept;
	void fail() noexcept;

	std::unique_ptr<Prepared> prepared_{};
	present::Raster raster_{};
	std::array<ClassicAudioCommand, 2U> audio_commands_{};
	std::size_t audio_head_ = 0U;
	std::size_t audio_count_ = 0U;
	ClassicMainlWinPhase phase_ = ClassicMainlWinPhase::black_in;
	std::uint8_t logo_cel_ = 0U;
	std::uint16_t delay_count_ = 0U;
	std::uint16_t wait_measure_ = 0U;
	std::uint16_t wait_fallback_frames_ = 0U;
	bool wait_uses_measure_ = false;
	bool preload_stage_splash_ = false;
	bool stage_splash_preloaded_ = false;
	std::optional<ClassicMainlWinCompletion> completion_{};
};

struct ClassicMainlWinTestAccess final {
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlWinScene>,
		ClassicMainlWinError
	> create_with_synthetic_resources(
		const ClassicMainlWinContext &context,
		std::span<const LoadedMainResource> resources,
		const present::AnkGlyphProvider &font,
		const NewClassicGameTextCatalog *localized_text = nullptr
	)
	{
		return ClassicMainlWinScene::create_impl(
			context,
			resources,
			font,
			false,
			localized_text
		);
	}
};

} // namespace th03::app

#endif
