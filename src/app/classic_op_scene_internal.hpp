#ifndef TH03_APP_CLASSIC_OP_SCENE_INTERNAL_HPP
#define TH03_APP_CLASSIC_OP_SCENE_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "th03/app/authentic_text.hpp"
#include "th03/app/classic_audio.hpp"
#include "th03/app/new_classic_assets.hpp"
#include "th03/app/resources.hpp"
#include "th03/assets/types.hpp"
#include "th03/core/game.hpp"
#include "th03/present/raster.hpp"
#include "th03/present/superimpose.hpp"
#include "th03/present/tram.hpp"

namespace th03::app {

class ClassicOpMusicRoomCompletion;
class ClassicOpOptionsCompletion;
class ClassicOpSelectionCompletion;
enum class ClassicOpSelectionMode : std::uint8_t;
struct ClassicOpSessionBackend;

struct ClassicOpInput final {
	// The donor uses the complete input_sp word for wake and release locks,
	// including Bomb, Q, and diagonal bits that have no action in this menu.
	bool raw_any = false;
	bool up = false;
	bool down = false;
	bool left = false;
	bool right = false;
	bool ok = false;
	bool shot = false;
	bool cancel = false;

	[[nodiscard]] bool any() const noexcept
	{
		return raw_any || up || down || left || right || ok || shot || cancel;
	}
};

enum class ClassicOpTitleMode : std::uint8_t {
	long_animation,
	fast_fade,
};

enum class ClassicOpMenuChoice : std::uint8_t {
	story,
	versus,
	music_room,
	high_scores,
	options,
	replay,
	quit,
};

enum class ClassicOpRoute : std::uint8_t {
	story,
	versus,
	music_room,
	high_scores,
	options,
	replay,
	quit,
	demo,
};

enum class ClassicOpVsChoice : std::uint8_t {
	one_player_cpu,
	one_player_two_player,
	cpu_cpu,
	quit,
};

enum class ClassicOpPhase : std::uint8_t {
	long_shift,
	long_background_fade,
	long_selection_pause,
	long_first_flash,
	long_menu_swap_bright,
	long_menu_swap_normal,
	long_second_flash,
	fast_black,
	fast_initial_pause,
	fast_fade,
	wait_title_input,
	box_to_main,
	main_menu,
	vs_grow_window,
	vs_menu,
	vs_shrink_window,
	vs_return_main,
	demo_black_out,
	ready,
	consumed,
	failed,
};

enum class ClassicOpBoundary : std::uint8_t {
	immediate,
	vsync,
	input_poll,
};

enum class ClassicOpErrorCode : std::uint8_t {
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

struct ClassicOpError final {
	ClassicOpErrorCode code = ClassicOpErrorCode::invalid_state;
	std::optional<MainResourceKey> resource{};
	std::optional<assets::AssetError> asset_error{};
	std::optional<AuthenticTextError> text_error{};
};

struct ClassicOpTick final {
	ClassicOpPhase phase = ClassicOpPhase::failed;
	ClassicOpBoundary boundary = ClassicOpBoundary::immediate;
	bool surface_changed = false;
	bool tram_changed = false;
	bool input_sampled = false;
	bool selection_changed = false;
	bool ready = false;
};

class ClassicOpCompletion final {
public:
	ClassicOpCompletion(const ClassicOpCompletion &) = delete;
	ClassicOpCompletion &operator=(const ClassicOpCompletion &) = delete;
	ClassicOpCompletion(ClassicOpCompletion &&) noexcept = default;
	ClassicOpCompletion &operator=(ClassicOpCompletion &&) noexcept = default;

	[[nodiscard]] ClassicOpRoute route() const noexcept;
	[[nodiscard]] std::optional<ClassicOpSelectionMode>
	selection_mode() const noexcept;
	[[nodiscard]] bool resets_vs_selection() const noexcept;
	[[nodiscard]] const GameInit &resident() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] const present::Tram &tram() const noexcept;

private:
	friend class ClassicOpTitleMenuScene;
	friend struct ClassicOpSessionBackend;
	friend struct ClassicOpSceneTestAccess;
	ClassicOpCompletion(
		ClassicOpRoute route,
		std::optional<ClassicOpSelectionMode> selection_mode,
		bool reset_vs_selection,
		GameInit resident,
		present::Raster raster,
		present::Tram tram
	) noexcept;

	ClassicOpRoute route_ = ClassicOpRoute::quit;
	std::optional<ClassicOpSelectionMode> selection_mode_{};
	bool reset_vs_selection_ = false;
	GameInit resident_{};
	present::Raster raster_{};
	present::Tram tram_{};
};

// Source-private owner of OP.EXE's startup title and six-entry main menu. The
// character-selection loads interleaved by the original title animation are a
// child-scene preload contract; this owner preserves their 16-VSync timing
// boundary but retains only the title/menu assets it actually draws.
class ClassicOpTitleMenuScene final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpTitleMenuScene>,
		ClassicOpError
	> create(
		GameInit resident,
		ClassicOpTitleMode title_mode,
		std::span<const LoadedMainResource> resources,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets = {}
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpTitleMenuScene>,
		ClassicOpError
	> resume_from_options(
		const ClassicOpOptionsCompletion &completion,
		std::span<const LoadedMainResource> resources,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpTitleMenuScene>,
		ClassicOpError
	> return_from_music_room(
		const ClassicOpMusicRoomCompletion &completion,
		std::span<const LoadedMainResource> resources,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpTitleMenuScene>,
		ClassicOpError
	> return_from_canceled_selection(
		const ClassicOpSelectionCompletion &completion,
		std::span<const LoadedMainResource> resources,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpTitleMenuScene>,
		ClassicOpError
	> return_from_other_screen(
		GameInit resident,
		std::span<const LoadedMainResource> resources,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
		bool restart_bgm
	);
	// A fresh OP process returning from non-demo VS bypasses the title and
	// enters character selection directly. This source-private factory
	// authenticates the OP startup closure and carries only its MIKOFT state.
	[[nodiscard]] static std::expected<ClassicOpCompletion, ClassicOpError>
	direct_vs_reentry(
		GameInit resident,
		ClassicOpSelectionMode mode,
		std::span<const LoadedMainResource> resources
	);
	~ClassicOpTitleMenuScene() = default;

	ClassicOpTitleMenuScene(const ClassicOpTitleMenuScene &) = delete;
	ClassicOpTitleMenuScene &operator=(
		const ClassicOpTitleMenuScene &
	) = delete;
	ClassicOpTitleMenuScene(ClassicOpTitleMenuScene &&) = delete;
	ClassicOpTitleMenuScene &operator=(ClassicOpTitleMenuScene &&) = delete;

	[[nodiscard]] std::expected<ClassicOpTick, ClassicOpError>
	advance(ClassicOpInput input = {});
	[[nodiscard]] std::optional<ClassicAudioCommand>
	take_audio_command() noexcept;
	[[nodiscard]] std::optional<ClassicOpCompletion>
	take_completion() noexcept;

	[[nodiscard]] ClassicOpPhase phase() const noexcept;
	[[nodiscard]] ClassicOpMenuChoice selected_choice() const noexcept;
	[[nodiscard]] ClassicOpVsChoice selected_vs_choice() const noexcept;
	[[nodiscard]] const GameInit &resident() const noexcept;
	[[nodiscard]] const present::Raster &raster() const noexcept;
	[[nodiscard]] const present::Tram &tram() const noexcept;

private:
	friend struct ClassicOpSceneTestAccess;
	friend struct ClassicOpSessionBackend;

	struct Prepared final {
		assets::PiImage long_background{};
		assets::PiImage menu_background{};
		std::optional<present::PreparedSuperimposeSheet> window_sheet{};
		std::optional<PreparedGameftGaiji> gaiji{};
	};

	enum class EntryPolicy : std::uint8_t {
		startup_long,
		startup_fast,
		fullscreen_return,
	};

	ClassicOpTitleMenuScene(
		std::unique_ptr<Prepared> prepared,
		GameInit resident,
		EntryPolicy policy,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets = {},
		bool restart_bgm = true
	) noexcept;
	ClassicOpTitleMenuScene(
		std::unique_ptr<Prepared> prepared,
		GameInit resident,
		present::Raster raster,
		present::Tram tram,
		bool input_allowed,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets = {}
	) noexcept;
	[[nodiscard]] static std::expected<
		std::unique_ptr<Prepared>,
		ClassicOpError
	> prepare_resources(
		std::span<const LoadedMainResource> resources,
		bool authenticate_resource_bytes
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpTitleMenuScene>,
		ClassicOpError
	> create_impl(
		GameInit resident,
		ClassicOpTitleMode title_mode,
		std::span<const LoadedMainResource> resources,
		bool authenticate_resource_bytes,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets = {}
	);
	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpTitleMenuScene>,
		ClassicOpError
	> create_predecoded(
		GameInit resident,
		ClassicOpTitleMode title_mode,
		assets::PiImage long_background,
		assets::PiImage menu_background,
		assets::BfntSheet window_sheet,
		assets::BfntSheet gaiji,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets = {}
	);

	[[nodiscard]] bool initialize_long_title() noexcept;
	[[nodiscard]] bool initialize_fast_title() noexcept;
	[[nodiscard]] bool apply_palette(
		const assets::Palette16Rgb4 &palette
	) noexcept;
	[[nodiscard]] bool shift_access_page_left(std::uint16_t pixels) noexcept;
	[[nodiscard]] bool draw_menu_background(bool populate_hidden) noexcept;
	[[nodiscard]] bool draw_hidden_menu_background() noexcept;
	[[nodiscard]] bool restore_menu_background(
		std::int32_t left,
		std::int32_t top,
		std::uint16_t width,
		std::uint16_t height
	) noexcept;
	[[nodiscard]] bool draw_window_glyph(
		std::int32_t left,
		std::uint16_t glyph
	) noexcept;
	[[nodiscard]] bool draw_main_choices() noexcept;
	[[nodiscard]] bool restore_patch_choice_rows(
		std::int32_t box_right
	) noexcept;
	[[nodiscard]] bool restore_patch_menu_rows(std::int32_t box_right) noexcept;
	[[nodiscard]] bool draw_title_credit() noexcept;
	[[nodiscard]] bool draw_choice(
		ClassicOpMenuChoice choice,
		present::TramAttribute attribute
	) noexcept;
	[[nodiscard]] bool update_selection(int direction) noexcept;
	[[nodiscard]] bool draw_vs_choice(
		ClassicOpVsChoice choice,
		present::TramAttribute attribute
	) noexcept;
	[[nodiscard]] bool draw_vs_choices() noexcept;
	[[nodiscard]] bool update_vs_selection(int direction) noexcept;
	[[nodiscard]] bool grow_box_step() noexcept;
	[[nodiscard]] bool shrink_box_step() noexcept;
	[[nodiscard]] bool begin_menu_swap() noexcept;
	[[nodiscard]] bool begin_box_animation() noexcept;
	[[nodiscard]] bool box_step() noexcept;
	[[nodiscard]] std::int32_t box_top_value() const noexcept;
	[[nodiscard]] bool push_audio_command(ClassicAudioCommand command) noexcept;
	void select_route(
		ClassicOpRoute route,
		std::optional<ClassicOpSelectionMode> selection_mode = std::nullopt
	) noexcept;
	void select_demo_route() noexcept;
	void fail() noexcept;
	[[nodiscard]] ClassicOpTick tick(
		ClassicOpBoundary boundary,
		bool surface_changed = false,
		bool tram_changed = false,
		bool input_sampled = false,
		bool selection_changed = false
	) const noexcept;

	std::unique_ptr<Prepared> prepared_{};
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_{};
	GameInit resident_{};
	present::Raster raster_{};
	present::Tram tram_{};
	std::vector<present::Index4> page_snapshot_{};
	std::array<ClassicAudioCommand, 3U> audio_commands_{};
	std::size_t audio_head_ = 0U;
	std::size_t audio_count_ = 0U;
	ClassicOpPhase phase_ = ClassicOpPhase::failed;
	ClassicOpMenuChoice selected_ = ClassicOpMenuChoice::story;
	ClassicOpVsChoice selected_vs_ = ClassicOpVsChoice::one_player_cpu;
	std::int16_t brightness_ = 0;
	std::uint16_t phase_frame_ = 0U;
	std::uint16_t title_wait_frame_ = 0U;
	std::int32_t box_right_left_ = 0;
	present::Page title_page_ = present::Page::zero;
	bool menu_input_allowed_ = false;
	bool increment_rand_after_box_ = false;
	bool restart_bgm_ = true;
	bool demo_route_pending_ = false;
	bool menu_base_snapshot_valid_ = false;
	std::optional<ClassicOpCompletion> completion_{};
};

struct ClassicOpSceneTestAccess final {
	[[nodiscard]] static ClassicOpCompletion completion(
		ClassicOpRoute route,
		GameInit resident,
		std::optional<ClassicOpSelectionMode> selection_mode = std::nullopt
	) noexcept
	{
		return ClassicOpCompletion(
			route, selection_mode, route == ClassicOpRoute::versus,
			std::move(resident), present::Raster{}, present::Tram{}
		);
	}

	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicOpTitleMenuScene>,
		ClassicOpError
	> create_with_predecoded_assets(
		GameInit resident,
		ClassicOpTitleMode title_mode,
		assets::PiImage long_background,
		assets::PiImage menu_background,
		assets::BfntSheet window_sheet,
		assets::BfntSheet gaiji,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets = {}
	)
	{
		return ClassicOpTitleMenuScene::create_predecoded(
			std::move(resident), title_mode, std::move(long_background),
			std::move(menu_background), std::move(window_sheet),
			std::move(gaiji), std::move(patch_assets)
		);
	}
};

} // namespace th03::app

#endif
