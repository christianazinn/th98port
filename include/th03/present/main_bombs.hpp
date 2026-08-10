#ifndef TH03_PRESENT_MAIN_BOMBS_HPP
#define TH03_PRESENT_MAIN_BOMBS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "th03/assets/types.hpp"
#include "th03/core/observer.hpp"
#include "th03/present/main_bomb_palette.hpp"
#include "th03/present/sprite16.hpp"
#include "th03/present/staff_flakes.hpp"

namespace th03::present {

inline constexpr std::size_t main_bomb_player_count = 2U;
inline constexpr std::size_t main_reimu_bomb_record_count = 32U;
inline constexpr std::size_t main_mima_bomb_column_count = 8U;
inline constexpr std::size_t main_ellen_bomb_vector_count = 8U;

// Semantic regions in MAIN's committed Sprite16 source atlas. Repeated Mima
// entries are intentional: the source selects three, three, and two columns
// from its three cels.
struct MainBombAtlasLayout {
	std::array<Sprite16AtlasRect, main_bomb_player_count> reimu_falling{};
	std::array<
		std::array<Sprite16AtlasRect, main_mima_bomb_column_count>,
		main_bomb_player_count
	> mima_columns{};
	std::array<
		std::array<Sprite16AtlasRect, 2U>,
		main_bomb_player_count
	> marisa_flashes{};
	std::array<Sprite16AtlasRect, main_bomb_player_count> ellen_vectors{};
};

[[nodiscard]] MainBombAtlasLayout authentic_main_bomb_atlas_layout() noexcept;

// BM.MRS is loaded once into two source slots. The second source slot has
// already undergone the source's persistent mrs_hflip() transformation. Both
// immutable images must outlive MainBombsPresenter.
struct MainBombMrsAssets {
	std::array<const assets::MrsImage *, main_bomb_player_count> images{};
};

enum class MainBombsStatus : std::uint8_t {
	ready,
	invalid_layout,
	invalid_assets,
	invalid_observation,
	staging_not_committed,
	draw_failed,
	palette_failed,
};

// Process-scoped reconstruction of the visual portions of MAIN's nine Bomb
// callbacks. The object owns the original private visual records and the
// source-shared Marisa/Rikako/Ellen values. It never stores a gameplay view or
// mutates gameplay state.
//
// CommonEffectSpawned and BombCommonEffectStep must be fanned to
// MainEffectsPresenter before this observer. This presenter then performs the
// source work that followed the allocation call, such as MRS and dependent
// private-sprite draws.
// A frame-1 BombFrameStep re-arms per-activation presentation records even if
// the preceding round ended before BombFinished. Process-shared visual state
// remains intact until this presenter is destroyed with the MAIN process.
class MainBombsPresenter final : public GameplayObserver {
public:
	MainBombsPresenter(
		Raster &raster,
		const Sprite16Staging &staging,
		MainBombAtlasLayout layout,
		MainBombMrsAssets mrs,
		StaffFlakeTrigTables trig,
		MainPaletteState &palette,
		KotohimeBombFadeCompatibility kotohime_compatibility = {}
	) noexcept;

	void bomb_compatibility_read(
		const BombCompatibilityRead &read
	) noexcept override;
	void bomb_finished(const BombFinished &finished) noexcept override;
	void bomb_frame_step(const BombFrameStep &step) noexcept override;
	void common_effect_spawned(
		const CommonEffectSpawned &spawn
	) noexcept override;
	void bomb_common_effect_step(
		const BombCommonEffectStep &step
	) noexcept override;

	[[nodiscard]] MainBombsStatus status() const noexcept;
	[[nodiscard]] bool ready() const noexcept;

private:
	enum class Character : std::uint8_t {
		reimu,
		mima,
		marisa,
		ellen,
		kotohime,
		kana,
		rikako,
		chiyuri,
		yumemi,
	};

	enum class PendingKind : std::uint8_t {
		none,
		mima_circle,
		marisa_circle,
		ellen_circle,
		chiyuri_early_circle,
		chiyuri_squares,
		yumemi_square,
		kana_step,
		kotohime_step,
	};

	struct Activation {
		bool active = false;
		std::int8_t paletted_character = 0;
		Character character = Character::reimu;
		std::uint8_t frame = 0U;
		std::int16_t entry_shift = 0;
	};

	struct PendingCallback {
		PendingKind kind = PendingKind::none;
		std::uint8_t player = 0U;
		std::uint8_t frame = 0U;
		std::uint8_t received = 0U;
	};

	struct ReimuRecord {
		bool active = false;
		std::int16_t x = 0;
		std::int16_t y = 0;
		std::int16_t velocity_y = 0;
	};

	enum class ReimuReadPhase : std::uint8_t {
		none,
		velocity,
		x,
	};

	struct ReimuReadState {
		ReimuReadPhase phase = ReimuReadPhase::none;
		std::uint8_t frame = 0U;
		std::int16_t velocity_y = 0;
	};

	struct MimaColumn {
		bool active = false;
		std::int16_t x = 0;
	};

	struct EllenVector {
		std::int16_t x = 19999;
		std::int16_t y = 0;
		std::int16_t velocity_x = 0;
		std::int16_t velocity_y = 0;
	};

	struct EllenPrefix {
		bool active = false;
		std::uint8_t frame = 0U;
		std::uint8_t next_slot = 0U;
	};

	struct MarisaPoint {
		bool initialized = false;
		std::int16_t screen_x = 0;
		std::int16_t screen_y = 0;
	};

	[[nodiscard]] bool fail(MainBombsStatus status) noexcept;
	[[nodiscard]] bool apply_palette(const BombFrameStep &step) noexcept;
	[[nodiscard]] bool begin_pending(
		PendingKind kind,
		std::uint8_t player,
		std::uint8_t frame
	) noexcept;
	void clear_pending() noexcept;
	[[nodiscard]] bool fill_playfield(std::uint8_t player) noexcept;
	[[nodiscard]] bool draw_mrs(
		std::uint8_t player,
		std::int16_t left
	) noexcept;
	[[nodiscard]] bool draw_reimu(
		std::uint8_t player,
		std::int16_t shift
	) noexcept;
	[[nodiscard]] bool draw_mima(
		std::uint8_t player,
		std::int16_t shift
	) noexcept;
	[[nodiscard]] bool draw_marisa(
		std::uint8_t player,
		std::uint8_t frame
	) noexcept;
	[[nodiscard]] bool draw_ellen(std::uint8_t player) noexcept;
	[[nodiscard]] bool draw_rikako(
		std::uint8_t player,
		std::uint8_t frame
	) noexcept;
	[[nodiscard]] bool draw_yumemi_triangles(
		std::uint8_t player,
		std::uint8_t frame
	) noexcept;
	void update_reimu(std::uint8_t player) noexcept;
	[[nodiscard]] bool finish_ellen_prefix(
		std::uint8_t player,
		std::uint8_t frame
	) noexcept;
	[[nodiscard]] bool render_frame(
		const BombFrameStep &step,
		std::uint8_t player,
		Character character
	) noexcept;

	Raster *raster_;
	const Sprite16Staging *staging_;
	MainBombAtlasLayout layout_{};
	MainBombMrsAssets mrs_{};
	StaffFlakeTrigTables trig_{};
	MainBombPaletteInterpreter palette_interpreter_;
	std::array<std::uint8_t, 288U / 8U * 184U> altered_p2_blue_{};
	std::array<Activation, main_bomb_player_count> activations_{};
	PendingCallback pending_{};
	std::array<
		std::array<ReimuRecord, main_reimu_bomb_record_count>,
		main_bomb_player_count
	> reimu_{};
	std::array<ReimuReadState, main_bomb_player_count> reimu_reads_{};
	std::array<
		std::array<MimaColumn, main_mima_bomb_column_count>,
		main_bomb_player_count
	> mima_{};
	std::array<std::uint8_t, main_bomb_player_count> mima_read_frame_{};
	std::array<
		std::array<EllenVector, main_ellen_bomb_vector_count>,
		main_bomb_player_count
	> ellen_{};
	std::array<EllenPrefix, main_bomb_player_count> ellen_prefix_{};
	std::uint8_t ellen_release_cursor_ = 0U;
	MarisaPoint marisa_point_{};
	std::int16_t rikako_radius_ = 0;
	MainBombsStatus status_ = MainBombsStatus::ready;
};

} // namespace th03::present

#endif
