#ifndef TH03_APP_CLASSIC_STAFF_SCENE_INTERNAL_HPP
#define TH03_APP_CLASSIC_STAFF_SCENE_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "classic_mainl_stage_internal.hpp"
#include "mainl_compiled_text_internal.hpp"
#include "th03/app/classic_audio.hpp"
#include "th03/app/new_classic_game_text.hpp"
#include "th03/app/resources.hpp"
#include "th03/assets/types.hpp"
#include "th03/core/game.hpp"
#include "th03/present/graphics_text.hpp"
#include "th03/present/raster.hpp"
#include "th03/present/staff_flakes.hpp"

namespace th03::app {

enum class ClassicStaffPhase : std::uint8_t {
	prelude,
	wait_volume,
	presentation,
	verdict,
	ready,
	consumed,
	failed,
};

enum class ClassicStaffBoundary : std::uint8_t {
	immediate,
	vsync,
	audio_volume,
	audio_measure,
};

enum class ClassicStaffErrorCode : std::uint8_t {
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

struct ClassicStaffError final {
	ClassicStaffErrorCode code = ClassicStaffErrorCode::invalid_context;
	std::optional<MainResourceKey> resource{};
	std::optional<assets::AssetError> asset_error{};
	std::optional<present::GraphicsTextError> text_error{};
};

struct ClassicStaffInput final {
	ClassicAudioStatus audio{};
	bool any_input = false;
	// Physical Z, sampled outside the interface/remapping word. The scene
	// applies it only when its entry-time score snapshot unlocked acceleration.
	bool fast_forward_held = false;
	// Already sampled for the VSync boundary consumed by this tick.
	bool vsync_count_above_one = false;
};

struct ClassicStaffTick final {
	ClassicStaffPhase phase = ClassicStaffPhase::failed;
	ClassicStaffBoundary boundary = ClassicStaffBoundary::immediate;
	bool surface_changed = false;
	bool input_sampled = false;
	bool ready = false;
};

class ClassicStaffCompletion final {
public:
	ClassicStaffCompletion(const ClassicStaffCompletion &) = delete;
	ClassicStaffCompletion &operator=(const ClassicStaffCompletion &) = delete;
	ClassicStaffCompletion(ClassicStaffCompletion &&) noexcept = default;
	ClassicStaffCompletion &operator=(ClassicStaffCompletion &&) noexcept = default;

	[[nodiscard]] const present::Raster &raster() const noexcept;

private:
	friend class ClassicStaffScene;
	friend struct ClassicStoryBackend;
	explicit ClassicStaffCompletion(present::Raster raster) noexcept;
	present::Raster raster_{};
};

// Source-private owner of staffroll_and_verdict_animate(). Construction
// authenticates the complete file closure and prepares every compiled string;
// advance() consumes at most one source VSync or input poll.
class ClassicStaffScene final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicStaffScene>,
		ClassicStaffError
	> create(
		const GameInit &resident,
		std::span<const LoadedMainResource> resources,
		const MainlStageTextAssets &stage_text,
		const MainlStaffCompiledAssets &compiled,
		const present::AnkGlyphProvider &font,
		present::StaffFlakeTrigTables trig,
		present::Raster initial_raster,
		bool fast_forward_unlocked = false,
		const NewClassicGameTextCatalog *game_text = nullptr
	);
	~ClassicStaffScene();

	ClassicStaffScene(const ClassicStaffScene &) = delete;
	ClassicStaffScene &operator=(const ClassicStaffScene &) = delete;
	ClassicStaffScene(ClassicStaffScene &&) = delete;
	ClassicStaffScene &operator=(ClassicStaffScene &&) = delete;

	[[nodiscard]] std::expected<ClassicStaffTick, ClassicStaffError>
	advance(const ClassicStaffInput &input);
	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command() noexcept;
	[[nodiscard]] std::optional<ClassicStaffCompletion>
	take_completion() noexcept;

	[[nodiscard]] ClassicStaffPhase phase() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;

	// Public only because the source-private schedule table is a
	// namespace-scope constant.
	enum class DriverPhase : std::uint8_t;
	enum class SequenceKind : std::uint8_t;
	struct Sequence;

private:
	friend struct ClassicStaffSceneTestAccess;

	struct Prepared final {
		assets::Palette16Rgb4 palette{};
		std::array<assets::PlanarCel4, 12U> cels{};
		std::array<
			std::uint8_t,
			present::staff_flake_mask_bytes
		> flake_masks{};
		std::optional<present::PreparedGraphicsText8x16> playchar{};
		std::optional<present::PreparedGraphicsText8x16> rank{};
		std::array<
			std::optional<present::PreparedGraphicsText8x16>,
			10U
		> numbers{};
		std::optional<present::PreparedGraphicsText8x16> point{};
		std::array<std::uint8_t, 8U> score{};
		std::uint8_t continues_used = 0U;
		std::uint8_t skill = 0U;

		[[nodiscard]] present::StaffFlakeMasks masks() const noexcept
		{
			return {
				flake_masks,
				present::PackedRowOrder::top_to_bottom,
			};
		}
	};

	ClassicStaffScene(
		std::unique_ptr<Prepared> &&prepared,
		GameInit resident,
		present::StaffFlakeTrigTables trig,
		present::Raster raster,
		bool fast_forward_unlocked
	) noexcept;
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicStaffScene>,
		ClassicStaffError
	> create_impl(
		const GameInit &resident,
		std::span<const LoadedMainResource> resources,
		const MainlStageTextAssets &stage_text,
		const MainlStaffCompiledAssets &compiled,
		const present::AnkGlyphProvider &font,
		present::StaffFlakeTrigTables trig,
		present::Raster initial_raster,
		bool authenticate_resource_bytes,
		bool fast_forward_unlocked,
		const NewClassicGameTextCatalog *game_text
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicStaffScene>,
		ClassicStaffError
	> create_predecoded(
		const GameInit &resident,
		std::array<assets::PlanarCel4, 12U> cels,
		assets::Palette16Rgb4 palette,
		const MainlStageTextAssets &stage_text,
		const MainlStaffCompiledAssets &compiled,
		const present::AnkGlyphProvider &font,
		present::StaffFlakeTrigTables trig,
		present::Raster initial_raster,
		bool fast_forward_unlocked,
		const NewClassicGameTextCatalog *game_text
	);
	[[nodiscard]] std::expected<ClassicStaffTick, ClassicStaffError>
	advance_one(const ClassicStaffInput &input);
	[[nodiscard]] bool fast_forward_step() const noexcept;

	[[nodiscard]] bool push_audio_command(ClassicAudioCommand command) noexcept;
	[[nodiscard]] bool initialize_visuals() noexcept;
	[[nodiscard]] bool initialize_sequence() noexcept;
	[[nodiscard]] bool tick_flakes(bool overrun) noexcept;
	[[nodiscard]] bool phase_done(
		ClassicAudioStatus audio,
		std::uint16_t measure,
		bool fast_forward_held
	) const noexcept;
	[[nodiscard]] bool slide_up() noexcept;
	[[nodiscard]] bool slide_out() noexcept;
	[[nodiscard]] bool overlay_up() noexcept;
	[[nodiscard]] bool overlay_out() noexcept;
	[[nodiscard]] bool dissolve() noexcept;
	[[nodiscard]] bool draw_verdict() noexcept;
	[[nodiscard]] bool clear_inner() noexcept;
	[[nodiscard]] ClassicStaffTick tick(
		ClassicStaffBoundary boundary,
		bool surface_changed = false,
		bool input_sampled = false
	) const noexcept;
	void fail() noexcept;

	std::unique_ptr<Prepared> prepared_{};
	GameInit resident_{};
	present::StaffFlakeTrigTables trig_{};
	present::Raster raster_{};
	std::optional<present::StaffFlakeState> flakes_{};
	std::array<std::int16_t, 2U> center_y_{};
	std::int16_t overlay_y_ = 0;
	std::size_t sequence_index_ = 0U;
	DriverPhase driver_phase_{};
	ClassicStaffPhase phase_ = ClassicStaffPhase::prelude;
	std::uint8_t fade_wait_remaining_ = 0U;
	std::int16_t fade_tone_ = 100;
	bool fade_tone_applied_ = false;
	std::uint8_t cleanup_ticks_ = 0U;
	std::int16_t verdict_tone_ = 0;
	bool fast_forward_unlocked_ = false;
	std::array<ClassicAudioCommand, 2U> audio_commands_{};
	std::size_t audio_head_ = 0U;
	std::size_t audio_count_ = 0U;
	std::optional<ClassicStaffCompletion> completion_{};
};

struct ClassicStaffSceneTestAccess final {
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicStaffScene>,
		ClassicStaffError
	> create_with_synthetic_resources(
		const GameInit &resident,
		std::span<const LoadedMainResource> resources,
		const MainlStageTextAssets &stage_text,
		const MainlStaffCompiledAssets &compiled,
		const present::AnkGlyphProvider &font,
		present::StaffFlakeTrigTables trig,
		present::Raster initial_raster,
		bool fast_forward_unlocked = false,
		const NewClassicGameTextCatalog *game_text = nullptr
	)
	{
		return ClassicStaffScene::create_impl(
			resident, resources, stage_text, compiled, font, trig,
			std::move(initial_raster), false, fast_forward_unlocked, game_text
		);
	}

	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicStaffScene>,
		ClassicStaffError
	> create_with_predecoded_assets(
		const GameInit &resident,
		std::array<assets::PlanarCel4, 12U> cels,
		assets::Palette16Rgb4 palette,
		const MainlStageTextAssets &stage_text,
		const MainlStaffCompiledAssets &compiled,
		const present::AnkGlyphProvider &font,
		present::StaffFlakeTrigTables trig,
		present::Raster initial_raster,
		bool fast_forward_unlocked = false,
		const NewClassicGameTextCatalog *game_text = nullptr
	)
	{
		return ClassicStaffScene::create_predecoded(
			resident, std::move(cels), palette, stage_text, compiled, font,
			trig, std::move(initial_raster), fast_forward_unlocked, game_text
		);
	}

	[[nodiscard]] static const assets::PlanarCel4 &cel(
		const ClassicStaffScene &scene,
		std::size_t index
	) noexcept
	{
		return scene.prepared_->cels[index];
	}

	[[nodiscard]] static std::uint32_t playchar_width(
		const ClassicStaffScene &scene
	) noexcept
	{
		return scene.prepared_->playchar->pixel_width();
	}

	[[nodiscard]] static std::uint32_t rank_width(
		const ClassicStaffScene &scene
	) noexcept
	{
		return scene.prepared_->rank->pixel_width();
	}

	[[nodiscard]] static std::size_t point_glyph_count(
		const ClassicStaffScene &scene
	) noexcept
	{
		return scene.prepared_->point->glyph_count();
	}

	[[nodiscard]] static std::uint8_t active_flake_count(
		const ClassicStaffScene &scene
	) noexcept
	{
		return scene.flakes_ ? scene.flakes_->active_count : 0U;
	}

	[[nodiscard]] static bool overrun_reset_pending(
		const ClassicStaffScene &scene
	) noexcept
	{
		return scene.flakes_ && scene.flakes_->reset_pending;
	}

	[[nodiscard]] static bool cdg_put_alpha(
		const ClassicStaffScene &scene
	) noexcept
	{
		return scene.flakes_ && scene.flakes_->cdg_put_alpha;
	}

	[[nodiscard]] static std::size_t sequence_index(
		const ClassicStaffScene &scene
	) noexcept
	{
		return scene.sequence_index_;
	}

	[[nodiscard]] static bool forced_overlay_alpha_probe(
		ClassicStaffScene &scene
	) noexcept;
	[[nodiscard]] static std::int16_t frame(
		const ClassicStaffScene &scene
	) noexcept;
};

} // namespace th03::app

#endif
