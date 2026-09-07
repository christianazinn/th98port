#ifndef TH03_PRESENT_MAIN_STATIC_HPP
#define TH03_PRESENT_MAIN_STATIC_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "th03/present/main_palette.hpp"
#include "th03/present/raster.hpp"
#include "th03/present/sprite_staging.hpp"
#include "th03/present/tram.hpp"

namespace th03::present {

inline constexpr std::size_t main_player_count = 2U;
inline constexpr std::uint16_t main_playfield_width = 288U;
inline constexpr std::uint16_t main_playfield_height = 368U;
inline constexpr std::uint16_t main_playfield_border = 16U;
inline constexpr std::uint16_t main_playfield_bordered_width = 320U;
inline constexpr std::uint16_t main_playfield_top = 16U;
inline constexpr std::uint16_t main_playfield_bottom = 384U;
inline constexpr std::uint16_t main_source_next_page_clear_bottom = 183U;
// Native visual bugfix: source sub_9778() stops at raw row 183 even though
// unclipped background sprites can write through row 193. Clear the complete
// zoomed-200 graphics page behind the lower TRAM rows to prevent retained
// sprite trails without changing any text-layer cells.
inline constexpr std::uint16_t main_next_page_clear_bottom =
	(graphics_height / 2U) - 1U;

inline constexpr std::uint8_t main_halfhearts_max = 10U;
inline constexpr std::uint8_t main_gauge_level_min = 1U;
inline constexpr std::uint8_t main_gauge_level_max = 16U;
inline constexpr std::uint8_t main_boss_level_max = 16U;

// Exact TH03 gaiji IDs from th03/gaiji/gaiji.h.
inline constexpr std::uint8_t gaiji_yinyang = 0x02U;
inline constexpr std::uint8_t gaiji_heart_empty = 0x04U;
inline constexpr std::uint8_t gaiji_heart_full = 0x05U;
inline constexpr std::uint8_t gaiji_heart_half = 0x06U;
inline constexpr std::uint8_t gaiji_bomb = 0x07U;
inline constexpr std::uint8_t gaiji_thalia = 0x09U;
inline constexpr std::uint8_t gaiji_proportional_digit_1 = 0x20U;
inline constexpr std::uint8_t gaiji_space = 0xCFU;

enum class MainGameMode : std::uint8_t {
	story,
	versus,
	demo,
};

struct MainHudPlayerState {
	std::uint8_t halfhearts = main_halfhearts_max;
	std::uint8_t bombs = 0U;
	std::uint8_t rounds_won = 0U;
	std::uint8_t gauge_level = main_gauge_level_min;
};

// Presentation-owned input. These are copied gameplay facts; this type has no
// dependency on th03_core and is not canonical gameplay state.
struct MainHudState {
	MainGameMode game_mode = MainGameMode::versus;
	std::uint8_t story_lives = 0U;
	std::array<MainHudPlayerState, main_player_count> players{};
	std::uint8_t boss_level = 0U;
};

struct MainPageRoles {
	Page back = Page::zero;
	Page front = Page::one;

	[[nodiscard]] friend constexpr bool operator==(
		const MainPageRoles &,
		const MainPageRoles &
	) = default;
};

// Portable equivalents of hud_wipe() and hud_static_put(). Invalid caller
// state is rejected before any cell is changed.
void main_hud_wipe(Tram &tram) noexcept;
[[nodiscard]] bool main_hud_static_put(
	Tram &tram,
	const MainHudState &state
) noexcept;

// Exact reusable gauge-level pair written by hud_static_gauge_levels_put().
// Warning startup redraws both players through this same source helper.
[[nodiscard]] bool main_hud_gauge_levels_put(
	Tram &tram,
	std::size_t player_id,
	std::uint8_t gauge_level,
	std::uint8_t boss_level
) noexcept;

[[nodiscard]] bool main_hud_halfhearts_put(
	Tram &tram,
	std::size_t player_id,
	std::uint8_t halfhearts
) noexcept;
[[nodiscard]] bool main_hud_bombs_put(
	Tram &tram,
	std::size_t player_id,
	std::uint8_t bombs
) noexcept;
[[nodiscard]] bool main_hud_rounds_won_put(
	Tram &tram,
	std::size_t player_id,
	std::uint8_t rounds_won
) noexcept;
[[nodiscard]] bool main_hud_story_lives_put(
	Tram &tram,
	std::uint8_t lives
) noexcept;

// Draws the exact six patterns selected by sub_E266() to page 1 and then page
// 0. The decoded sheets must have the exact 32x32, 9-glyph character layout.
[[nodiscard]] bool main_prepare_shot_source_sheet(
	Raster &raster,
	MainPlayerShotSheets sheets
);

// Owns MAIN's page-role state while Raster and Tram remain reusable primitive
// stores. This slice deliberately has no gameplay observer or application
// scene policy.
class MainStaticPresenter {
public:
	// Begins with the MAINL-to-MAIN handoff already present in raster and an
	// uncaptured process-scoped palette state: source
	// graph_copy_page(0) copies page 1 to page 0. The method then prepares the
	// shot sheet, restores the full logical RGB8 handoff palette, selects the
	// upper 200-line
	// mode, commits the lower-half Sprite16 source and masks, selects shown page
	// 1, and writes the static HUD.
	[[nodiscard]] bool round_startup(
		Raster &raster,
		Tram &tram,
		const MainHudState &hud,
		MainPlayerShotSheets sheets,
		MainPaletteState &palette
	);

	// Source sub_A21F() visual reset for a route-1 repeat inside the same MAIN
	// process. It clears both upper 200-line pages to color 2 (page 1, then page
	// 0), shows page 1 while accessing page 0, wipes/rewrites the static HUD,
	// and restores the process baseline palette. Sprite16 staging and all other
	// process-scoped presentation state remain intact. The caller performs
	// entry.cpp's following Tone=100 immediate show.
	[[nodiscard]] bool repeat_round_startup(
		Raster &raster,
		Tram &tram,
		const MainHudState &hud,
		MainPaletteState &palette
	) noexcept;

	// Late-frame page transaction from sub_9778(): access the old front, show
	// the old back, and exchange roles. Source clears P1/P2 x 288 through raw
	// y=183; the native renderer deliberately extends this through raw y=199
	// to remove source-observable sprite trails behind the lower TRAM rows.
	[[nodiscard]] bool end_frame_swap_and_clear(Raster &raster) noexcept;

	// A Replay Patch accelerator restores page_back/page_front after repeat
	// startup has already left hardware access on page 0. The BSS roles and the
	// hardware selectors can therefore be crossed for exactly one frame.
	// Preserve them separately until the next ordinary page transaction rejoins
	// them.
	[[nodiscard]] bool accelerator_publication_restore(
		MainPageRoles source_pages
	) noexcept;

	[[nodiscard]] MainPageRoles page_roles() const noexcept;
	[[nodiscard]] bool source_page_ready(const Raster &raster) const noexcept;
	[[nodiscard]] const Sprite16Staging &sprite_staging() const noexcept;
	[[nodiscard]] bool started() const noexcept;

private:
	friend class MainPresenterRollbackAccess;
	MainPageRoles pages_{};
	Sprite16Staging sprite_staging_{};
	bool started_ = false;
	bool accelerator_roles_crossed_ = false;
};

} // namespace th03::present

#endif
