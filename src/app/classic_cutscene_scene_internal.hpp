#ifndef TH03_APP_CLASSIC_CUTSCENE_SCENE_INTERNAL_HPP
#define TH03_APP_CLASSIC_CUTSCENE_SCENE_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "th03/app/authentic_text.hpp"
#include "th03/app/classic_audio.hpp"
#include "th03/app/resources.hpp"
#include "th03/app/new_classic_cutscene_text.hpp"
#include "th03/assets/types.hpp"
#include "th03/present/cutscene_raster.hpp"
#include "th03/present/cutscene_script.hpp"
#include "th03/present/raster.hpp"
#include "th03/present/tram.hpp"

namespace th03::app {

enum class ClassicCutscenePhase : std::uint8_t {
	prelude,
	running,
	ready,
	consumed,
	failed,
};

enum class ClassicCutsceneBoundary : std::uint8_t {
	immediate,
	vsync,
	audio_measure,
};

enum class ClassicCutsceneErrorCode : std::uint8_t {
	invalid_request,
	resource_set_mismatch,
	resource_bytes_missing,
	resource_integrity_failed,
	asset_decode_failed,
	invalid_asset_geometry,
	font_prepare_failed,
	script_failed,
	localized_script_failed,
	raster_failed,
	unknown_picture,
	unknown_music,
	audio_command_pending,
	invalid_state,
};

struct ClassicCutsceneError final {
	ClassicCutsceneErrorCode code = ClassicCutsceneErrorCode::invalid_request;
	std::optional<MainResourceKey> resource{};
	std::optional<assets::AssetError> asset_error{};
	std::optional<present::CutsceneScriptError> script_error{};
	std::optional<NewClassicCutsceneTextPackError> cutscene_text_error{};
	std::optional<present::CutsceneRasterError> raster_error{};
};

struct ClassicCutsceneInput final {
	present::CutsceneScriptInput script{};
	bool shot_or_ok_held = false;
	ClassicAudioStatus audio{};
};

struct ClassicCutsceneTick final {
	ClassicCutscenePhase phase = ClassicCutscenePhase::failed;
	ClassicCutsceneBoundary boundary = ClassicCutsceneBoundary::immediate;
	bool surface_changed = false;
	bool input_sampled = false;
	bool ready = false;
};

class ClassicCutsceneCompletion final {
public:
	ClassicCutsceneCompletion(const ClassicCutsceneCompletion &) = delete;
	ClassicCutsceneCompletion &operator=(const ClassicCutsceneCompletion &) = delete;
	ClassicCutsceneCompletion(ClassicCutsceneCompletion &&) noexcept = default;
	ClassicCutsceneCompletion &operator=(ClassicCutsceneCompletion &&) noexcept =
		default;

	[[nodiscard]] const present::Raster &raster() const noexcept;

private:
	friend class ClassicCutsceneScene;
	friend struct ClassicStoryBackend;
	explicit ClassicCutsceneCompletion(present::Raster raster) noexcept;
	present::Raster raster_{};
};

// Source-private owner for one of TH03's 28 plaintext cutscenes. Construction
// validates and decodes the complete stock closure before publishing any
// mutable scene state. advance() stops at each source input poll, VSync/audio
// boundary, or script completion. Immediate input-only ticks are intentional:
// cutscene_animate() polls once for every command, separator, or text pair.
class ClassicCutsceneScene final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicCutsceneScene>,
		ClassicCutsceneError
	> create(
		MainlCutsceneResourceRequest request,
		std::span<const LoadedMainResource> resources,
		const present::AnkGlyphProvider &font,
		present::Raster initial_raster,
		const NewClassicCutsceneTextCatalog *localized_text = nullptr
	);
	~ClassicCutsceneScene();

	ClassicCutsceneScene(const ClassicCutsceneScene &) = delete;
	ClassicCutsceneScene &operator=(const ClassicCutsceneScene &) = delete;
	ClassicCutsceneScene(ClassicCutsceneScene &&) = delete;
	ClassicCutsceneScene &operator=(ClassicCutsceneScene &&) = delete;

	[[nodiscard]] std::expected<ClassicCutsceneTick, ClassicCutsceneError>
	advance(const ClassicCutsceneInput &input);
	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command() noexcept;
	[[nodiscard]] bool audio_command_pending() const noexcept;
	[[nodiscard]] std::optional<ClassicCutsceneCompletion>
	take_completion() noexcept;

	[[nodiscard]] ClassicCutscenePhase phase() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] const present::Tram &tram() const noexcept;

private:
	friend struct ClassicCutsceneSceneTestAccess;

	enum class PendingKind : std::uint8_t {
		none,
		frames,
		input_change_release,
		input_change_press,
		input_ok,
		measure,
		input_ok_or_measure,
		palette_fade_initial,
		palette_fade_wait,
		crossfade_wait,
		shake_wait,
	};

	struct Prepared final {
		MainlCutsceneResourceRequest request{};
		std::optional<PreparedGameftGaiji> gaiji{};
		std::optional<present::CutsceneScriptVm> vm{};
		std::vector<std::array<std::uint8_t, 2U>> localized_glyphs{};
		std::size_t localized_script_index = new_classic_cutscene_script_count;
		std::array<assets::PiImage, 24U> pictures{};
	};

	ClassicCutsceneScene(
		std::unique_ptr<Prepared> prepared,
		const present::AnkGlyphProvider &font,
		present::Raster raster
	) noexcept;
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicCutsceneScene>,
		ClassicCutsceneError
	> create_impl(
		MainlCutsceneResourceRequest request,
		std::span<const LoadedMainResource> resources,
		const present::AnkGlyphProvider &font,
		present::Raster initial_raster,
		bool authenticate_resource_bytes,
		const NewClassicCutsceneTextCatalog *localized_text
	);

	[[nodiscard]] std::expected<void, ClassicCutsceneError>
	begin_prelude(bool &surface_changed);
	[[nodiscard]] std::expected<std::optional<ClassicCutsceneTick>, ClassicCutsceneError>
	advance_pending(
		const ClassicCutsceneInput &input,
		bool &surface_changed,
		bool &input_sampled
	);
	[[nodiscard]] std::expected<void, ClassicCutsceneError>
	load_next_batch(const ClassicCutsceneInput &input, bool &input_sampled);
	[[nodiscard]] std::expected<std::optional<ClassicCutsceneTick>, ClassicCutsceneError>
	execute_operation(
		const present::CutsceneOperation &operation,
		const ClassicCutsceneInput &input,
		bool &surface_changed,
		bool &input_sampled
	);
	[[nodiscard]] std::expected<void, ClassicCutsceneError>
	load_picture(const present::CutsceneFilename &filename);
	[[nodiscard]] std::optional<ClassicMusicCue>
	music_cue(const present::CutsceneFilename &filename) const noexcept;
	[[nodiscard]] bool push_audio_command(ClassicAudioCommand command) noexcept;
	void begin_wait(const present::CutsceneWait &operation, const ClassicCutsceneInput &input);
	void fail() noexcept;

	std::unique_ptr<Prepared> prepared_{};
	const present::AnkGlyphProvider *font_ = nullptr;
	present::Raster raster_{};
	present::Tram tram_{};
	std::optional<present::CutsceneRasterPresenter> presenter_{};
	present::CutsceneScriptBatch batch_{};
	std::uint8_t batch_index_ = 0U;
	std::size_t localized_glyph_index_ = 0U;
	bool batch_active_ = false;
	bool began_vm_ = false;
	bool prelude_started_ = false;
	PendingKind pending_ = PendingKind::none;
	std::uint16_t wait_remaining_ = 0U;
	std::uint16_t wait_target_ = 0U;
	bool wait_audio_active_ = false;
	bool wait_poll_due_ = false;
	std::optional<present::CutscenePaletteFade> palette_fade_{};
	std::uint8_t palette_fade_sample_ = 0U;
	std::optional<present::CutsceneCrossfadePicture> crossfade_{};
	std::uint8_t crossfade_step_ = 0U;
	std::optional<present::CutsceneShake> shake_{};
	std::uint32_t shake_position_ = 0U;
	std::array<ClassicAudioCommand, 3U> audio_commands_{};
	std::size_t audio_head_ = 0U;
	std::size_t audio_count_ = 0U;
	ClassicMusicCue loaded_music_ = ClassicMusicCue::none;
	std::optional<ClassicCutsceneCompletion> completion_{};
	ClassicCutscenePhase phase_ = ClassicCutscenePhase::prelude;
};

struct ClassicCutsceneSceneTestAccess final {
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicCutsceneScene>,
		ClassicCutsceneError
	> create_with_synthetic_resources(
		MainlCutsceneResourceRequest request,
		std::span<const LoadedMainResource> resources,
		const present::AnkGlyphProvider &font,
		present::Raster initial_raster,
		const NewClassicCutsceneTextCatalog *localized_text = nullptr
	)
	{
		return ClassicCutsceneScene::create_impl(
			request, resources, font, std::move(initial_raster), false,
			localized_text
		);
	}

	// Narrow hermetic scheduler seam. Production construction always validates
	// and decodes the complete authenticated closure above; this route supplies
	// blank, already-decoded PI slots so timing/error tests need no copied game
	// bytes or test-only PI/BFNT encoders.
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicCutsceneScene>,
		ClassicCutsceneError
	> create_with_synthetic_script(
		std::span<const std::uint8_t> script,
		const present::AnkGlyphProvider &font,
		present::Raster initial_raster,
		MainlCutsceneKind kind = MainlCutsceneKind::interstage_chiyuri
	);
};

} // namespace th03::app

#endif
