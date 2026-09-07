#ifndef TH03_PRESENT_CUTSCENE_RASTER_HPP
#define TH03_PRESENT_CUTSCENE_RASTER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>

#include "th03/assets/types.hpp"
#include "th03/present/cutscene_script.hpp"
#include "th03/present/raster.hpp"
#include "th03/present/text.hpp"
#include "th03/present/tram.hpp"

namespace th03::present {

inline constexpr std::size_t cutscene_crossfade_step_count = 5U;
inline constexpr std::size_t cutscene_palette_fade_sample_count = 18U;

enum class CutsceneCrossfadeStep : std::uint8_t {
	mask_first = 0U,
	mask_second,
	mask_third,
	mask_fourth,
	complete,
};

enum class CutsceneRasterDispatch : std::uint8_t {
	applied,
	external,
	background_required,
	crossfade_required,
	shake_required,
	palette_fade_required,
};

enum class CutsceneRasterErrorCode : std::uint8_t {
	invalid_raster_mode,
	invalid_operation,
	invalid_text,
	invalid_background,
	missing_background,
	missing_background_palette,
	box_not_snapshotted,
	invalid_picture_quarter,
	invalid_crossfade_step,
	invalid_shake_step,
	invalid_palette_fade_sample,
	draw_failed,
};

struct CutsceneRasterError {
	CutsceneRasterErrorCode code = CutsceneRasterErrorCode::invalid_operation;
	std::optional<GraphicsTextError> text_error{};

	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneRasterError &,
		const CutsceneRasterError &
	) = default;
};

// Executes the graphics-facing subset of CutsceneOperation against TH03's
// original Master/upstream two-page VRAM model. The presenter owns the current
// decoded PI background and the page-0 text-box snapshot. Raster, FONT and the
// MIKOFT-backed gaiji table must outlive it.
//
// Blocking VM operations remain explicit: execute() returns one of the
// *_required results, and the scene driver calls the matching frame-step method
// at source timing boundaries. No host clock, input polling, filesystem I/O or
// audio execution enters this class.
class CutsceneRasterPresenter final {
public:
	[[nodiscard]] static std::expected<
		CutsceneRasterPresenter,
		CutsceneRasterError
	> create(
		Raster &raster,
		const AnkGlyphProvider &font,
		const Tram &gaiji
	) noexcept;

	CutsceneRasterPresenter(CutsceneRasterPresenter &&) noexcept = default;
	CutsceneRasterPresenter &operator=(
		CutsceneRasterPresenter &&
	) noexcept = default;
	CutsceneRasterPresenter(const CutsceneRasterPresenter &) = delete;
	CutsceneRasterPresenter &operator=(
		const CutsceneRasterPresenter &
	) = delete;

	// Applies every immediate graphics operation. External wait/audio/finish
	// operations and operations that require caller-supplied content or timed
	// stepping are classified; palette fades also set their source starting
	// tone before the caller schedules the initial alignment VSync.
	[[nodiscard]] std::expected<
		CutsceneRasterDispatch,
		CutsceneRasterError
	> execute(const CutsceneOperation &operation);

	// Transactionally replaces PI slot 0 after complete 640x400/index/palette
	// domain validation. Palette-less images are accepted for \p@, but later
	// palette application is rejected until an image with a palette is supplied.
	[[nodiscard]] std::expected<void, CutsceneRasterError> replace_background(
		assets::PiImage image
	);

	// Applies one of the four cumulative EGC mask states or the final unmasked
	// picture. The caller owns frames_per_mask timing between the first four
	// steps. Every step leaves page 0 shown/accessed and copies the exact result
	// to page 1, matching the original TH03 implementation.
	[[nodiscard]] std::expected<void, CutsceneRasterError> crossfade_step(
		const CutsceneCrossfadePicture &operation,
		CutsceneCrossfadeStep step
	);

	// Source shake positions are indices 0..parameter inclusive, followed by
	// finish_shake(). delay_each_position controls caller timing only.
	[[nodiscard]] std::expected<void, CutsceneRasterError> shake_step(
		const CutsceneShake &operation,
		std::uint16_t position_index
	) noexcept;
	void finish_shake() noexcept;

	// Applies one of the 17 loop palette samples or the final exact endpoint.
	// The caller owns the initial alignment VSync and [speed] waits after samples
	// 0..16. Sample 17 has no following wait in master.lib.
	[[nodiscard]] std::expected<void, CutsceneRasterError> palette_fade_sample(
		const CutscenePaletteFade &operation,
		std::uint8_t sample_index
	) noexcept;

	[[nodiscard]] bool has_background() const noexcept;
	[[nodiscard]] bool has_background_palette() const noexcept;
	[[nodiscard]] bool has_box_snapshot() const noexcept;

private:
	static constexpr std::int32_t box_left = 80;
	static constexpr std::int32_t box_top = 320;
	static constexpr std::uint16_t box_width = 480U;
	static constexpr std::uint16_t box_height = 64U;
	static constexpr std::size_t box_pixels =
		static_cast<std::size_t>(box_width) * box_height;

	CutsceneRasterPresenter(
		Raster &raster,
		const AnkGlyphProvider &font,
		const Tram &gaiji
	) noexcept;

	[[nodiscard]] std::expected<void, CutsceneRasterError> draw_glyph(
		const CutsceneDrawGlyph &operation
	);
	[[nodiscard]] std::expected<void, CutsceneRasterError> draw_gaiji(
		const CutsceneDrawGaiji &operation
	) noexcept;
	[[nodiscard]] std::expected<void, CutsceneRasterError> snapshot_box() noexcept;
	[[nodiscard]] std::expected<void, CutsceneRasterError> restore_box() noexcept;
	void release_box() noexcept;
	[[nodiscard]] std::expected<void, CutsceneRasterError> clear_pages(
		const CutsceneClearPages &operation
	) noexcept;
	[[nodiscard]] std::expected<void, CutsceneRasterError> present_background(
		const CutscenePresentBackground &operation
	);
	[[nodiscard]] std::expected<void, CutsceneRasterError>
	apply_background_palette() noexcept;
	[[nodiscard]] std::expected<void, CutsceneRasterError> show_picture_quarter(
		const CutsceneShowPictureQuarter &operation
	);
	[[nodiscard]] std::expected<void, CutsceneRasterError> clear_picture();
	[[nodiscard]] std::expected<void, CutsceneRasterError> copy_picture_to_page_one();
	[[nodiscard]] std::expected<void, CutsceneRasterError> begin_palette_fade(
		const CutscenePaletteFade &operation
	) noexcept;
	[[nodiscard]] bool valid_raster_mode() const noexcept;

	Raster *raster_ = nullptr;
	const AnkGlyphProvider *font_ = nullptr;
	const Tram *gaiji_ = nullptr;
	std::array<Index4, box_pixels> box_snapshot_{};
	std::optional<assets::PiImage> background_{};
	bool box_snapshotted_ = false;
};

} // namespace th03::present

#endif
