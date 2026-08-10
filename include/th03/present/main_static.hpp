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
inline constexpr std::uint16_t main_next_page_clear_bottom = 183U;

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

	// Exact late-frame page transaction from sub_9778(): access the old front,
	// show the old back, exchange roles, then clear P1/P2 x 288 through y=183
	// on the newly accessed page with colors 0 and 1.
	[[nodiscard]] bool end_frame_swap_and_clear(Raster &raster) noexcept;

	[[nodiscard]] MainPageRoles page_roles() const noexcept;
	[[nodiscard]] const Sprite16Staging &sprite_staging() const noexcept;
	[[nodiscard]] bool started() const noexcept;

private:
	MainPageRoles pages_{};
	Sprite16Staging sprite_staging_{};
	bool started_ = false;
};

} // namespace th03::present

#endif
