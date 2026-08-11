#ifndef TH03_APP_CLASSIC_OP_SELECTION_INTERNAL_HPP
#define TH03_APP_CLASSIC_OP_SELECTION_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "classic_op_scene_internal.hpp"
#include "th03/app/classic_audio.hpp"
#include "th03/app/resources.hpp"
#include "th03/assets/types.hpp"
#include "th03/core/game.hpp"
#include "th03/present/raster.hpp"
#include "th03/present/superimpose.hpp"
#include "th03/present/tram.hpp"

namespace th03::app {

enum class ClassicOpSelectionMode : std::uint8_t {
	story,
	vs_1p_cpu,
	vs_1p_2p,
	vs_cpu_cpu,
};

struct ClassicOpSelectionInput final {
	std::uint16_t single_player = 0U;
	std::uint16_t player_one = 0U;
	std::uint16_t player_two = 0U;
};

struct ClassicOpSelectionCarry final {
	std::array<bool, 2U> input_locked{};
	std::uint8_t curve_cycle = 0U;

	friend constexpr bool operator==(
		const ClassicOpSelectionCarry &,
		const ClassicOpSelectionCarry &
	) = default;
};

enum class ClassicOpSelectionPhase : std::uint8_t {
	initial_blank,
	initial_delay,
	draw_and_input,
	confirm_white_in,
	frame_wait,
	ready,
	consumed,
	failed,
};

enum class ClassicOpSelectionBoundary : std::uint8_t {
	immediate,
	vsync,
	input_poll,
};

enum class ClassicOpSelectionOutcome : std::uint8_t {
	canceled,
	selected,
};

enum class ClassicOpSelectionErrorCode : std::uint8_t {
	invalid_config,
	invalid_input,
	resource_set_mismatch,
	resource_bytes_missing,
	resource_integrity_failed,
	asset_decode_failed,
	invalid_asset_geometry,
	draw_failed,
	audio_command_pending,
	invalid_state,
};

struct ClassicOpSelectionError final {
	ClassicOpSelectionErrorCode code =
		ClassicOpSelectionErrorCode::invalid_state;
	std::optional<MainResourceKey> resource{};
	std::optional<assets::AssetError> asset_error{};
};

struct ClassicOpSelectionTick final {
	ClassicOpSelectionPhase phase = ClassicOpSelectionPhase::failed;
	ClassicOpSelectionBoundary boundary =
		ClassicOpSelectionBoundary::immediate;
	bool surface_changed = false;
	bool input_sampled = false;
	bool page_flipped = false;
	bool resident_changed = false;
	std::optional<std::uint8_t> confirming_player{};
	bool ready = false;
};

class ClassicOpSelectionCompletion final {
public:
	ClassicOpSelectionCompletion(const ClassicOpSelectionCompletion &) = delete;
	ClassicOpSelectionCompletion &operator=(
		const ClassicOpSelectionCompletion &
	) = delete;
	ClassicOpSelectionCompletion(ClassicOpSelectionCompletion &&) noexcept =
		default;
	ClassicOpSelectionCompletion &operator=(
		ClassicOpSelectionCompletion &&
	) noexcept = default;

	[[nodiscard]] ClassicOpSelectionOutcome outcome() const noexcept;
	[[nodiscard]] ClassicOpSelectionMode mode() const noexcept;
	[[nodiscard]] const GameInit &resident() const noexcept;
	[[nodiscard]] const ClassicOpSelectionCarry &carry() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] const present::Tram &tram() const noexcept;

private:
	friend class ClassicOpCharacterSelection;
	friend struct ClassicOpSelectionTestAccess;
	ClassicOpSelectionCompletion(
		ClassicOpSelectionOutcome outcome,
		ClassicOpSelectionMode mode,
		GameInit resident,
		ClassicOpSelectionCarry carry,
		present::Raster raster,
		present::Tram tram
	) noexcept;

	ClassicOpSelectionOutcome outcome_ =
		ClassicOpSelectionOutcome::canceled;
	ClassicOpSelectionMode mode_ = ClassicOpSelectionMode::story;
	GameInit resident_{};
	ClassicOpSelectionCarry carry_{};
	present::Raster raster_{};
	present::Tram tram_{};
};

// Source-private OP.EXE character-selection owner. A fresh OP process starts
// with a zero carry; canceled selection re-entry reuses the returned carry.
class ClassicOpCharacterSelection final {
public:
	// The typed OP completion proves that authenticated MIKOFT gaiji remain
	// installed in the carried Tram. Raw construction is test-private.
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpCharacterSelection>,
		ClassicOpSelectionError
	> create_from_op(
		const ClassicOpCompletion &completion,
		ClassicOpSelectionMode mode,
		std::uint8_t available_character_count,
		ClassicOpSelectionCarry carry,
		std::span<const LoadedMainResource> resources
	);

	~ClassicOpCharacterSelection() = default;
	ClassicOpCharacterSelection(const ClassicOpCharacterSelection &) = delete;
	ClassicOpCharacterSelection &operator=(
		const ClassicOpCharacterSelection &
	) = delete;
	ClassicOpCharacterSelection(ClassicOpCharacterSelection &&) = delete;
	ClassicOpCharacterSelection &operator=(
		ClassicOpCharacterSelection &&
	) = delete;

	[[nodiscard]] std::expected<
		ClassicOpSelectionTick,
		ClassicOpSelectionError
	> advance(ClassicOpSelectionInput input = {});
	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command() noexcept;
	[[nodiscard]] std::optional<ClassicOpSelectionCompletion>
	take_completion() noexcept;

	[[nodiscard]] ClassicOpSelectionPhase phase() const noexcept;
	[[nodiscard]] const GameInit &resident() const noexcept;
	[[nodiscard]] const ClassicOpSelectionCarry &carry() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] const present::Tram &tram() const noexcept;
	[[nodiscard]] std::array<std::uint8_t, 2U> local_selection() const noexcept;
	[[nodiscard]] std::array<bool, 2U> confirmed() const noexcept;

private:
	friend struct ClassicOpSelectionTestAccess;

	struct Prepared final {
		std::array<std::array<assets::PlanarCel4, 2U>, 9U> portraits{};
		assets::PlanarCel4 story_portrait{};
		assets::PlanarCel4 stats{};
		assets::PlanarCel4 extra_background{};
		std::array<assets::PlanarCel4, 9U> extra_icons{};
		std::optional<present::PreparedSuperimposeSheet> names{};
		assets::Palette16Rgb4 palette{};
	};

	enum class ResumePoint : std::uint8_t {
		player_one_bomb,
		player_two_start,
		player_two_bomb,
		after_players,
	};

	ClassicOpCharacterSelection(
		std::unique_ptr<Prepared> prepared,
		GameInit resident,
		ClassicOpSelectionMode mode,
		std::uint8_t available_character_count,
		ClassicOpSelectionCarry carry,
		present::Raster raster,
		present::Tram tram
	) noexcept;

	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpCharacterSelection>,
		ClassicOpSelectionError
	> create_impl(
		GameInit resident,
		ClassicOpSelectionMode mode,
		std::uint8_t available_character_count,
		ClassicOpSelectionCarry carry,
		present::Raster raster,
		present::Tram tram,
		std::span<const LoadedMainResource> resources,
		bool authenticate_resource_bytes
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpCharacterSelection>,
		ClassicOpSelectionError
	> create_prepared(
		std::unique_ptr<Prepared> prepared,
		GameInit resident,
		ClassicOpSelectionMode mode,
		std::uint8_t available_character_count,
		ClassicOpSelectionCarry carry,
		present::Raster raster,
		present::Tram tram
	);

	[[nodiscard]] bool initialize_after_blank() noexcept;
	[[nodiscard]] bool draw_frame() noexcept;
	[[nodiscard]] bool draw_curves() noexcept;
	[[nodiscard]] bool draw_portrait(std::size_t player) noexcept;
	[[nodiscard]] bool draw_stats_base(std::size_t player) noexcept;
	[[nodiscard]] bool draw_stat_row(
		std::size_t player,
		std::uint8_t row
	) noexcept;
	[[nodiscard]] bool draw_extra(std::size_t player) noexcept;
	[[nodiscard]] bool draw_player(std::size_t player) noexcept;
	[[nodiscard]] bool draw_cursor(std::size_t player) noexcept;
	[[nodiscard]] bool process_sample() noexcept;
	[[nodiscard]] bool process_player(
		std::size_t player,
		std::uint16_t input,
		ResumePoint after_shot,
		ResumePoint after_bomb
	) noexcept;
	[[nodiscard]] bool resume_sample() noexcept;
	[[nodiscard]] bool begin_confirmation(
		std::size_t player,
		MainPaletteVariant palette,
		ResumePoint resume
	) noexcept;
	[[nodiscard]] bool confirmation_vsync() noexcept;
	[[nodiscard]] bool finish_sample() noexcept;
	[[nodiscard]] bool publish_frame() noexcept;
	[[nodiscard]] bool finish_selected() noexcept;
	void finish_canceled() noexcept;
	void fail() noexcept;
	[[nodiscard]] bool push_audio_command(ClassicAudioCommand command) noexcept;
	[[nodiscard]] ClassicOpSelectionTick tick(
		ClassicOpSelectionBoundary boundary,
		bool surface_changed = false,
		bool input_sampled = false,
		bool page_flipped = false,
		bool resident_changed = false
	) const noexcept;

	std::unique_ptr<Prepared> prepared_{};
	GameInit resident_{};
	ClassicOpSelectionMode mode_ = ClassicOpSelectionMode::story;
	std::uint8_t available_character_count_ = 7U;
	ClassicOpSelectionCarry carry_{};
	present::Raster raster_{};
	present::Tram tram_{};
	std::array<std::uint8_t, 2U> selection_{};
	std::array<MainPaletteVariant, 2U> palette_{
		MainPaletteVariant::first, MainPaletteVariant::first,
	};
	std::array<bool, 2U> confirmed_{};
	std::uint8_t active_player_ = 0U;
	std::uint16_t cpu_quirk_frames_ = 0U;
	std::uint8_t trail_count_ = 8U;
	std::uint8_t fade_counter_ = 0U;
	std::uint16_t delay_elapsed_ = 0U;
	std::uint16_t frame_elapsed_ = 0U;
	std::uint16_t frame_wait_remaining_ = 0U;
	ClassicOpSelectionInput sampled_input_{};
	ResumePoint resume_point_ = ResumePoint::after_players;
	std::uint8_t confirming_player_ = 0U;
	std::uint8_t confirmation_frame_ = 0U;
	bool resident_changed_this_advance_ = false;
	bool page_flipped_this_advance_ = false;
	ClassicOpSelectionPhase phase_ = ClassicOpSelectionPhase::initial_blank;
	std::array<ClassicAudioCommand, 4U> audio_commands_{};
	std::size_t audio_head_ = 0U;
	std::size_t audio_count_ = 0U;
	std::optional<ClassicOpSelectionCompletion> completion_{};
};

struct ClassicOpSelectionTestAccess final {
	[[nodiscard]] static ClassicOpSelectionCompletion make_completion(
		ClassicOpSelectionOutcome outcome,
		ClassicOpSelectionMode mode,
		GameInit resident,
		ClassicOpSelectionCarry carry = {},
		present::Raster raster = {},
		present::Tram tram = {}
	) noexcept
	{
		return ClassicOpSelectionCompletion(
			outcome,
			mode,
			std::move(resident),
			carry,
			std::move(raster),
			std::move(tram)
		);
	}

	[[nodiscard]] static bool draw_curves_only(
		ClassicOpCharacterSelection &scene
	) noexcept
	{
		return scene.raster_.clear({
			0, 0, present::graphics_width, present::graphics_height,
		}, 0U) && scene.draw_curves();
	}

	[[nodiscard]] static bool draw_player_only(
		ClassicOpCharacterSelection &scene,
		std::size_t player
	) noexcept
	{
		return scene.raster_.clear({
			0, 0, present::graphics_width, present::graphics_height,
		}, 0U) && scene.draw_player(player);
	}

	[[nodiscard]] static bool draw_cursor_only(
		ClassicOpCharacterSelection &scene,
		std::size_t player
	) noexcept
	{
		return scene.raster_.clear({
			0, 0, present::graphics_width, present::graphics_height,
		}, 0U) && scene.draw_cursor(player);
	}

	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpCharacterSelection>,
		ClassicOpSelectionError
	> create_with_predecoded_assets(
		GameInit resident,
		ClassicOpSelectionMode mode,
		std::uint8_t available_character_count,
		ClassicOpSelectionCarry carry,
		present::Raster raster,
		present::Tram tram,
		std::array<assets::CdgFile, 9U> portraits,
		assets::CdgFile story_portrait,
		assets::CdgFile stats,
		assets::CdgFile extra_background,
		assets::CdgFile extra_icons,
		assets::BfntSheet names,
		assets::Palette16Rgb4 palette
	);

	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpCharacterSelection>,
		ClassicOpSelectionError
	> create_with_synthetic_resources(
		GameInit resident,
		ClassicOpSelectionMode mode,
		std::uint8_t available_character_count,
		ClassicOpSelectionCarry carry,
		present::Raster raster,
		present::Tram tram,
		std::span<const LoadedMainResource> resources
	)
	{
		return ClassicOpCharacterSelection::create_impl(
			std::move(resident), mode, available_character_count, carry,
			std::move(raster), std::move(tram), resources, false
		);
	}
};

} // namespace th03::app

#endif
