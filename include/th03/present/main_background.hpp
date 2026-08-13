#ifndef TH03_PRESENT_MAIN_BACKGROUND_HPP
#define TH03_PRESENT_MAIN_BACKGROUND_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "th03/core/observer.hpp"
#include "th03/present/sprite16.hpp"

namespace th03::present {

inline constexpr std::size_t main_background_record_count = 50U;
inline constexpr std::size_t main_background_type_a_record_count = 48U;
inline constexpr std::size_t main_background_dot_count = 24U;
inline constexpr std::size_t main_background_type_b_static_count = 40U;
inline constexpr std::size_t main_background_type_b_animation_cel_count = 4U;

// Typed view of the five distinct source regions addressed by D092, D2E8,
// D135, D1E7, D340, and D3F9 in MAIN's committed Sprite16 atlas.
struct MainBackgroundAtlasLayout {
	std::array<Sprite16AtlasRect, 2U> type_a_variants{};
	Sprite16AtlasRect type_b_static{};
	std::array<
		Sprite16AtlasRect,
		main_background_type_b_animation_cel_count
	> type_b_animation{};
};

[[nodiscard]] MainBackgroundAtlasLayout
	authentic_main_background_atlas_layout() noexcept;

enum class MainBackgroundStatus : std::uint8_t {
	ready,
	invalid_layout,
	invalid_observation,
	staging_not_committed,
	draw_failed,
};

// Process-scoped owner of byte_20F2C's live MAIN background prefix. The
// source reserves 80 records, but its two callbacks address only the first 48
// or 50; this fixed-capacity native pool therefore stores every observable
// record without preserving unreachable BSS. Initialization and initial-frame
// IRAND results arrive as ordered observer facts, while all visual state stays
// presentation-private and survives repeat-round startup.
//
// This presenter must receive the fresh Game::create() initialization stream
// for its MAIN process. Snapshot-backed creation intentionally emits no D092/
// D2E8 facts and cannot reconstruct this presentation-private source pool.
class MainBackgroundPresenter final : public GameplayObserver {
public:
	MainBackgroundPresenter(
		Raster &raster,
		const Sprite16Staging &staging,
		MainBackgroundAtlasLayout layout
	) noexcept;
	MainBackgroundPresenter(const MainBackgroundPresenter &) = delete;
	MainBackgroundPresenter &operator=(const MainBackgroundPresenter &) = delete;
	MainBackgroundPresenter(MainBackgroundPresenter &&) = delete;
	MainBackgroundPresenter &operator=(MainBackgroundPresenter &&) = delete;

	void main_background_initialize(
		const MainBackgroundInitialize &initialize
	) noexcept override;
	void main_background_frame(
		const MainBackgroundFrame &frame
	) noexcept override;
	void main_background_random_read(
		const MainBackgroundRandomRead &read
	) noexcept override;

	[[nodiscard]] MainBackgroundStatus status() const noexcept;
	[[nodiscard]] bool ready() const noexcept;
	// True only after the complete fresh D092/D2E8 initialization fact stream
	// has been consumed. `ready()` alone also describes the valid pre-stream
	// state used while an application assembles a MAIN process.
	[[nodiscard]] bool initialization_complete() const noexcept;
	// Replay Patch RC11/RC12 advances this process-private pool while its
	// hardware put entry points are suppressed. Native later-checkpoint preroll
	// uses the same split: positions, velocities, phases, and RNG observations
	// advance, but hidden frames do not touch the Raster.
	void set_rendering_enabled(bool enabled) noexcept;

private:
	struct Record {
		std::int16_t x_q12_4 = 0;
		std::int16_t y_q12_4 = 0;
		std::int16_t velocity_x_q12_4 = 0;
		std::int16_t velocity_y_q12_4 = 0;
		std::uint8_t angle = 0U;
		std::uint8_t sprite_variant = 0U;
	};

	enum class ReadStream : std::uint8_t {
		none,
		initialize,
		initial_frame,
	};

	void fail(MainBackgroundStatus status) noexcept;
	void consume_initialize_read(std::uint16_t raw_value) noexcept;
	void consume_initial_frame_read(std::uint16_t raw_value) noexcept;
	void finish_initial_frame() noexcept;
	void repeat_velocity_reset() noexcept;
	void move(Record &record) noexcept;
	[[nodiscard]] bool draw_dot(const Record &record) noexcept;
	[[nodiscard]] bool draw_sprite(
		const Record &record,
		std::size_t slot,
		std::uint16_t frame_mod_4096,
		bool clipped_type_a
	) noexcept;
	[[nodiscard]] bool draw_all(std::uint16_t frame_mod_4096) noexcept;
	void steady_frame(std::uint16_t frame_mod_4096) noexcept;

	Raster *raster_;
	const Sprite16Staging *staging_;
	MainBackgroundAtlasLayout layout_{};
	std::array<Record, main_background_record_count> records_{};
	MainBackgroundKind kind_ = MainBackgroundKind::type_a;
	ReadStream read_stream_ = ReadStream::none;
	std::size_t read_slot_ = 0U;
	std::uint8_t read_field_ = 0U;
	std::uint16_t initial_frames_completed_ = 0U;
	std::uint8_t magnitude_ = 0x40U;
	std::uint8_t round_id_ = 0U;
	bool initialized_ = false;
	bool have_round_id_ = false;
	// D1E7/D3F9 leave their automatic flag untouched in two schedule gaps.
	// Retaining its preceding value makes that source stack dependency explicit.
	bool vector_latch_ = false;
	bool rendering_enabled_ = true;
	MainBackgroundStatus status_ = MainBackgroundStatus::ready;
};

} // namespace th03::present

#endif
