#ifndef TH03_APP_CLASSIC_MAINL_CONTINUE_INTERNAL_HPP
#define TH03_APP_CLASSIC_MAINL_CONTINUE_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "th03/app/classic_audio.hpp"
#include "th03/app/resources.hpp"
#include "th03/assets/types.hpp"
#include "th03/core/game.hpp"
#include "th03/present/graphics_text.hpp"
#include "th03/present/raster.hpp"

namespace th03::app {

class ClassicRegistrationCompletion;

struct ClassicMainlContinueInput final {
	bool left = false;
	bool right = false;
	bool ok = false;
	bool shot = false;
	bool cancel = false;
};

enum class ClassicMainlContinuePhase : std::uint8_t {
	black_in,
	waiting_for_input,
	black_out,
	await_gameover_preload,
	ready,
	consumed,
	failed,
};

enum class ClassicMainlContinueBoundary : std::uint8_t {
	immediate,
	vsync,
};

enum class ClassicMainlContinueErrorCode : std::uint8_t {
	invalid_context,
	resource_set_mismatch,
	resource_bytes_missing,
	resource_integrity_failed,
	asset_decode_failed,
	invalid_asset_geometry,
	font_prepare_failed,
	draw_failed,
	audio_command_pending,
	invalid_state,
};

struct ClassicMainlContinueError final {
	ClassicMainlContinueErrorCode code =
		ClassicMainlContinueErrorCode::invalid_context;
	std::optional<MainResourceKey> resource{};
	std::optional<assets::AssetError> asset_error{};
	std::optional<present::GraphicsTextError> text_error{};
};

struct ClassicMainlContinueTick final {
	ClassicMainlContinuePhase phase = ClassicMainlContinuePhase::failed;
	ClassicMainlContinueBoundary boundary =
		ClassicMainlContinueBoundary::immediate;
	bool surface_changed = false;
	bool input_sampled = false;
	bool choice_changed = false;
	bool choice_committed = false;
	bool ready = false;
	bool gameover_preload_requested = false;
};

class ClassicMainlContinueCompletion final {
public:
	ClassicMainlContinueCompletion(
		const ClassicMainlContinueCompletion &
	) = delete;
	ClassicMainlContinueCompletion &operator=(
		const ClassicMainlContinueCompletion &
	) = delete;
	ClassicMainlContinueCompletion(
		ClassicMainlContinueCompletion &&
	) noexcept = default;
	ClassicMainlContinueCompletion &operator=(
		ClassicMainlContinueCompletion &&
	) noexcept = default;

	[[nodiscard]] ContinueChoice choice() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;

private:
	friend class ClassicMainlContinueScene;
	friend class ClassicMainlGameOverScene;
	explicit ClassicMainlContinueCompletion(
		ContinueChoice choice,
		present::Raster raster
	) noexcept;

	ContinueChoice choice_ = ContinueChoice::decline;
	present::Raster raster_{};
};

// Source-private equivalent of continue_menu(), beginning at the exact
// registration next-screen asset boundary. The retained MainProcess remains the
// sole authority for score clearing and Continue resident mutation; this scene
// publishes only the choice and the black OVER raster.
class ClassicMainlContinueScene final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlContinueScene>,
		ClassicMainlContinueError
	> create(
		std::uint8_t remaining_credits,
		std::span<const LoadedMainResource> resources,
		const present::AnkGlyphProvider &font
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlContinueScene>,
		ClassicMainlContinueError
	> create_from_registration(
		ClassicRegistrationCompletion &&completion,
		std::uint8_t remaining_credits,
		const present::AnkGlyphProvider &font
	);
	~ClassicMainlContinueScene();

	ClassicMainlContinueScene(const ClassicMainlContinueScene &) = delete;
	ClassicMainlContinueScene &operator=(
		const ClassicMainlContinueScene &
	) = delete;
	ClassicMainlContinueScene(ClassicMainlContinueScene &&) = delete;
	ClassicMainlContinueScene &operator=(ClassicMainlContinueScene &&) = delete;

	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command() noexcept;
	[[nodiscard]] std::expected<
		ClassicMainlContinueTick,
		ClassicMainlContinueError
	> advance(ClassicMainlContinueInput input) noexcept;
	[[nodiscard]] std::expected<
		ClassicMainlContinueTick,
		ClassicMainlContinueError
	> preload_gameover(std::span<const LoadedMainResource> resources);

	[[nodiscard]] ClassicMainlContinuePhase phase() const noexcept;
	[[nodiscard]] ContinueChoice selected_choice() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] std::optional<ClassicMainlContinueCompletion>
	take_completion() noexcept;

private:
	friend struct ClassicMainlContinueTestAccess;

	struct Prepared final {
		std::optional<assets::PiImage> continue_background{};
		std::array<std::optional<assets::PlanarCel4>, 4U> choice_cels{};
		std::optional<assets::PiImage> gameover_background{};
		std::array<
			std::optional<present::PreparedGraphicsText8x16>,
			4U
		> credit_digits{};
	};

	explicit ClassicMainlContinueScene(
		std::unique_ptr<Prepared> prepared,
		std::uint8_t remaining_credits
	) noexcept;
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlContinueScene>,
		ClassicMainlContinueError
	> create_impl(
		std::uint8_t remaining_credits,
		std::span<const LoadedMainResource> resources,
		const present::AnkGlyphProvider &font,
		bool authenticate_resource_bytes
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlContinueScene>,
		ClassicMainlContinueError
	> create_from_registration_impl(
		ClassicRegistrationCompletion &&completion,
		std::uint8_t remaining_credits,
		const present::AnkGlyphProvider &font
	);
	[[nodiscard]] std::expected<
		ClassicMainlContinueTick,
		ClassicMainlContinueError
	> preload_gameover_impl(
		std::span<const LoadedMainResource> resources,
		bool authenticate_resource_bytes
	);
	[[nodiscard]] bool draw_continue() noexcept;
	[[nodiscard]] bool draw_choices() noexcept;
	[[nodiscard]] bool draw_credit(std::uint8_t value) noexcept;
	[[nodiscard]] bool draw_gameover() noexcept;
	[[nodiscard]] bool push_audio_command(ClassicAudioCommand command) noexcept;
	void fail() noexcept;

	std::unique_ptr<Prepared> prepared_{};
	std::uint8_t remaining_credits_ = 0U;
	present::Raster raster_{};
	std::array<ClassicAudioCommand, 1U> audio_commands_{};
	std::size_t audio_count_ = 0U;
	ClassicMainlContinuePhase phase_ = ClassicMainlContinuePhase::black_in;
	ContinueChoice choice_ = ContinueChoice::accept;
	std::uint8_t fade_count_ = 0U;
	bool input_locked_ = false;
	std::optional<ClassicMainlContinueCompletion> completion_{};
};

enum class ClassicMainlGameOverPhase : std::uint8_t {
	black_in,
	wait_measure_3,
	black_out,
	ready,
	consumed,
	failed,
};

enum class ClassicMainlGameOverBoundary : std::uint8_t {
	immediate,
	vsync,
	audio_measure,
};

struct ClassicMainlGameOverTick final {
	ClassicMainlGameOverPhase phase = ClassicMainlGameOverPhase::failed;
	ClassicMainlGameOverBoundary boundary =
		ClassicMainlGameOverBoundary::immediate;
	bool surface_changed = false;
	bool ready = false;
};

class ClassicMainlGameOverCompletion final {
public:
	ClassicMainlGameOverCompletion(
		const ClassicMainlGameOverCompletion &
	) = delete;
	ClassicMainlGameOverCompletion &operator=(
		const ClassicMainlGameOverCompletion &
	) = delete;
	ClassicMainlGameOverCompletion(
		ClassicMainlGameOverCompletion &&
	) noexcept = default;
	ClassicMainlGameOverCompletion &operator=(
		ClassicMainlGameOverCompletion &&
	) noexcept = default;

	[[nodiscard]] const present::Raster &raster() const noexcept;

private:
	friend class ClassicMainlGameOverScene;
	explicit ClassicMainlGameOverCompletion(
		present::Raster raster
	) noexcept;

	present::Raster raster_{};
};

// Exact gameover_bgm_play_and_fade() timeline over the OVER.PI capability
// published by Continue. PMD state is injected and host audio is never read.
class ClassicMainlGameOverScene final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlGameOverScene>,
		ClassicMainlContinueError
	> create(ClassicMainlContinueCompletion &&continue_completion);
	~ClassicMainlGameOverScene() = default;

	ClassicMainlGameOverScene(const ClassicMainlGameOverScene &) = delete;
	ClassicMainlGameOverScene &operator=(
		const ClassicMainlGameOverScene &
	) = delete;
	ClassicMainlGameOverScene(ClassicMainlGameOverScene &&) = delete;
	ClassicMainlGameOverScene &operator=(ClassicMainlGameOverScene &&) = delete;

	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command() noexcept;
	[[nodiscard]] std::expected<
		ClassicMainlGameOverTick,
		ClassicMainlContinueError
	> advance(ClassicAudioStatus audio) noexcept;

	[[nodiscard]] ClassicMainlGameOverPhase phase() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] std::optional<ClassicMainlGameOverCompletion>
	take_completion() noexcept;

private:
	explicit ClassicMainlGameOverScene(
		present::Raster raster
	) noexcept;
	[[nodiscard]] bool push_audio_command(ClassicAudioCommand command) noexcept;
	void begin_measure_wait(ClassicAudioStatus audio) noexcept;
	[[nodiscard]] bool measure_wait_done(
		ClassicAudioStatus audio,
		ClassicMainlGameOverBoundary &boundary
	) noexcept;
	void fail() noexcept;

	present::Raster raster_{};
	std::array<ClassicAudioCommand, 3U> audio_commands_{};
	std::size_t audio_head_ = 0U;
	std::size_t audio_count_ = 0U;
	ClassicMainlGameOverPhase phase_ = ClassicMainlGameOverPhase::black_in;
	std::uint8_t fade_count_ = 0U;
	std::uint16_t wait_count_ = 0U;
	bool wait_uses_measure_ = false;
	std::optional<ClassicMainlGameOverCompletion> completion_{};
};

struct ClassicMainlContinueTestAccess final {
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicMainlContinueScene>,
		ClassicMainlContinueError
	> create_with_synthetic_resources(
		std::uint8_t remaining_credits,
		std::span<const LoadedMainResource> resources,
		const present::AnkGlyphProvider &font
	)
	{
		return ClassicMainlContinueScene::create_impl(
			remaining_credits, resources, font, false
		);
	}

	[[nodiscard]] static std::expected<
		ClassicMainlContinueTick,
		ClassicMainlContinueError
	> preload_gameover_with_synthetic_resources(
		ClassicMainlContinueScene &scene,
		std::span<const LoadedMainResource> resources
	)
	{
		return scene.preload_gameover_impl(resources, false);
	}
};

} // namespace th03::app

#endif
