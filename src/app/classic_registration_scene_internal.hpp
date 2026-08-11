#ifndef TH03_APP_CLASSIC_REGISTRATION_SCENE_INTERNAL_HPP
#define TH03_APP_CLASSIC_REGISTRATION_SCENE_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "classic_mainl_stage_internal.hpp"
#include "th03/app/classic_audio.hpp"
#include "th03/app/classic_score.hpp"
#include "th03/app/classic_score_store.hpp"
#include "th03/app/resources.hpp"
#include "th03/assets/types.hpp"
#include "th03/core/game.hpp"
#include "th03/present/graphics_text.hpp"
#include "th03/present/raster.hpp"
#include "th03/present/superimpose.hpp"

namespace th03::app {

struct ClassicRegistrationInput final {
	bool up = false;
	bool down = false;
	bool left = false;
	bool right = false;
	bool ok = false;
	bool shot = false;
	bool bomb = false;

	[[nodiscard]] bool any() const noexcept
	{
		return up || down || left || right || ok || shot || bomb;
	}
};

enum class ClassicRegistrationPhase : std::uint8_t {
	black_in,
	name_entry,
	wait_release,
	wait_press,
	black_out,
	ready,
	consumed,
	failed,
};

enum class ClassicRegistrationBoundary : std::uint8_t {
	immediate,
	vsync,
	input_poll,
};

enum class ClassicRegistrationErrorCode : std::uint8_t {
	invalid_context,
	resource_set_mismatch,
	resource_bytes_missing,
	resource_integrity_failed,
	asset_decode_failed,
	invalid_asset_geometry,
	font_prepare_failed,
	store_load_failed,
	score_mutation_failed,
	draw_failed,
	audio_command_pending,
	store_commit_failed,
	invalid_state,
};

struct ClassicRegistrationError final {
	ClassicRegistrationErrorCode code =
		ClassicRegistrationErrorCode::invalid_context;
	std::optional<MainResourceKey> resource{};
	std::optional<assets::AssetError> asset_error{};
	std::optional<present::GraphicsTextError> text_error{};
	std::optional<ClassicScoreStoreError> store_error{};
	std::optional<ClassicScoreError> score_error{};
};

struct ClassicRegistrationTick final {
	ClassicRegistrationPhase phase = ClassicRegistrationPhase::failed;
	ClassicRegistrationBoundary boundary =
		ClassicRegistrationBoundary::immediate;
	bool surface_changed = false;
	bool input_sampled = false;
	bool selection_changed = false;
	bool name_changed = false;
	bool ready = false;
};

class ClassicRegistrationCompletion final {
public:
	ClassicRegistrationCompletion(const ClassicRegistrationCompletion &) = delete;
	ClassicRegistrationCompletion &operator=(
		const ClassicRegistrationCompletion &
	) = delete;
	ClassicRegistrationCompletion(ClassicRegistrationCompletion &&) noexcept =
		default;
	ClassicRegistrationCompletion &operator=(
		ClassicRegistrationCompletion &&
	) noexcept = default;

	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] std::optional<std::size_t> entered_place() const noexcept;
	[[nodiscard]] ClassicScoreStoreDisposition load_disposition() const noexcept;

private:
	friend class ClassicRegistrationScene;
	friend struct ClassicStoryBackend;
	ClassicRegistrationCompletion(
		present::Raster raster,
		std::optional<std::size_t> entered_place,
		ClassicScoreStoreDisposition load_disposition
	) noexcept;

	present::Raster raster_{};
	std::optional<std::size_t> entered_place_{};
	ClassicScoreStoreDisposition load_disposition_ =
		ClassicScoreStoreDisposition::loaded;
};

// Source-private owner of MAINL's Story registration route. OP score viewing
// and replay pages are deliberately excluded. Construction authenticates and
// prepares the full visual closure before touching the score store; advance()
// consumes at most one source input poll or VSync boundary.
class ClassicRegistrationScene final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicRegistrationScene>,
		ClassicRegistrationError
	> create(
		const GameInit &resident,
		const std::filesystem::path &score_path,
		std::span<const LoadedMainResource> resources,
		const MainlStageTextAssets &stage_text,
		const present::AnkGlyphProvider &font,
		present::Raster initial_raster
	);
	~ClassicRegistrationScene();

	ClassicRegistrationScene(const ClassicRegistrationScene &) = delete;
	ClassicRegistrationScene &operator=(const ClassicRegistrationScene &) = delete;
	ClassicRegistrationScene(ClassicRegistrationScene &&) = delete;
	ClassicRegistrationScene &operator=(ClassicRegistrationScene &&) = delete;

	[[nodiscard]] std::expected<ClassicRegistrationTick, ClassicRegistrationError>
	advance(ClassicRegistrationInput input);
	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command() noexcept;
	[[nodiscard]] std::optional<ClassicRegistrationCompletion>
	take_completion() noexcept;

	[[nodiscard]] ClassicRegistrationPhase phase() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] std::optional<std::size_t> entered_place() const noexcept;

private:
	friend struct ClassicRegistrationSceneTestAccess;

	struct Prepared final {
		assets::PiImage background{};
		assets::PlanarCel4 rank{};
		std::optional<present::PreparedSuperimposeSheet> normal{};
		std::optional<present::PreparedSuperimposeSheet> highlight{};
		std::array<
			std::optional<present::PreparedGraphicsText8x16>,
			10U
		> playchars{};
	};

	ClassicRegistrationScene(
		std::unique_ptr<Prepared> prepared,
		GameInit resident,
		std::filesystem::path score_path,
		ClassicScoreDocument document,
		ClassicScoreStoreDisposition load_disposition,
		ClassicScoreCodecFields commit_codec,
		std::optional<std::size_t> entered_place,
		present::Raster raster
	) noexcept;
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicRegistrationScene>,
		ClassicRegistrationError
	> create_impl(
		const GameInit &resident,
		const std::filesystem::path &score_path,
		std::span<const LoadedMainResource> resources,
		const MainlStageTextAssets &stage_text,
		const present::AnkGlyphProvider &font,
		present::Raster initial_raster,
		bool authenticate_resource_bytes
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicRegistrationScene>,
		ClassicRegistrationError
	> create_predecoded(
		const GameInit &resident,
		const std::filesystem::path &score_path,
		assets::PiImage background,
		assets::PlanarCel4 rank,
		assets::BfntSheet normal,
		assets::BfntSheet highlight,
		const MainlStageTextAssets &stage_text,
		const present::AnkGlyphProvider &font,
		present::Raster initial_raster
	);

	[[nodiscard]] bool initialize_visuals() noexcept;
	[[nodiscard]] bool draw_glyph(
		present::Point position,
		ClassicRegistrationGlyph glyph,
		bool highlight
	) noexcept;
	[[nodiscard]] bool draw_alphabet_glyph(
		ClassicRegistrationGlyph glyph,
		bool highlight
	) noexcept;
	[[nodiscard]] bool draw_alphabet() noexcept;
	[[nodiscard]] bool draw_row(std::size_t place) noexcept;
	[[nodiscard]] bool draw_rows(bool restore_background) noexcept;
	[[nodiscard]] bool blank_cursor() noexcept;
	[[nodiscard]] bool rerender_name_cursor() noexcept;
	[[nodiscard]] std::expected<void, ClassicRegistrationError>
	finish_name();
	[[nodiscard]] bool direction_trigger(
		bool pressed,
		std::uint32_t &hold
	) noexcept;
	[[nodiscard]] bool push_audio_command(ClassicAudioCommand command) noexcept;
	[[nodiscard]] ClassicRegistrationTick tick(
		ClassicRegistrationBoundary boundary,
		bool surface_changed = false,
		bool input_sampled = false,
		bool selection_changed = false,
		bool name_changed = false
	) const noexcept;
	[[nodiscard]] std::expected<ClassicRegistrationTick, ClassicRegistrationError>
	fade_tick(bool black_in);
	void fail() noexcept;

	std::unique_ptr<Prepared> prepared_{};
	GameInit resident_{};
	std::filesystem::path score_path_{};
	ClassicScoreDocument document_{};
	ClassicScoreStoreDisposition load_disposition_ =
		ClassicScoreStoreDisposition::loaded;
	ClassicScoreCodecFields commit_codec_{};
	std::optional<std::size_t> entered_place_{};
	present::Raster raster_{};
	ClassicRegistrationPhase phase_ = ClassicRegistrationPhase::black_in;
	std::uint8_t fade_boundary_ = 0U;
	ClassicRegistrationGlyph selected_ = ClassicRegistrationGlyph::a;
	std::uint8_t name_cursor_backwards_ =
		static_cast<std::uint8_t>(classic_score_name_length - 1U);
	std::array<std::uint32_t, 4U> direction_hold_{};
	bool enter_locked_ = false;
	bool back_locked_ = false;
	bool name_rerender_ = true;
	std::array<ClassicAudioCommand, 3U> audio_commands_{};
	std::size_t audio_head_ = 0U;
	std::size_t audio_count_ = 0U;
};

struct ClassicRegistrationSceneTestAccess final {
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicRegistrationScene>,
		ClassicRegistrationError
	> create_with_synthetic_resources(
		const GameInit &resident,
		const std::filesystem::path &score_path,
		std::span<const LoadedMainResource> resources,
		const MainlStageTextAssets &stage_text,
		const present::AnkGlyphProvider &font,
		present::Raster initial_raster
	)
	{
		return ClassicRegistrationScene::create_impl(
			resident, score_path, resources, stage_text, font,
			std::move(initial_raster), false
		);
	}

	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicRegistrationScene>,
		ClassicRegistrationError
	> create_with_predecoded_assets(
		const GameInit &resident,
		const std::filesystem::path &score_path,
		assets::PiImage background,
		assets::PlanarCel4 rank,
		assets::BfntSheet normal,
		assets::BfntSheet highlight,
		const MainlStageTextAssets &stage_text,
		const present::AnkGlyphProvider &font,
		present::Raster initial_raster
	)
	{
		return ClassicRegistrationScene::create_predecoded(
			resident, score_path, std::move(background), std::move(rank),
			std::move(normal), std::move(highlight), stage_text, font,
			std::move(initial_raster)
		);
	}

	[[nodiscard]] static const ClassicScoreDocument &document(
		const ClassicRegistrationScene &scene
	) noexcept
	{
		return scene.document_;
	}

	[[nodiscard]] static ClassicRegistrationGlyph selected(
		const ClassicRegistrationScene &scene
	) noexcept
	{
		return scene.selected_;
	}

	[[nodiscard]] static std::uint8_t name_cursor_backwards(
		const ClassicRegistrationScene &scene
	) noexcept
	{
		return scene.name_cursor_backwards_;
	}

	[[nodiscard]] static const std::array<std::uint32_t, 4U> &direction_hold(
		const ClassicRegistrationScene &scene
	) noexcept
	{
		return scene.direction_hold_;
	}
};

} // namespace th03::app

#endif
