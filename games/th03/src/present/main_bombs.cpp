#include "th03/present/main_bombs.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

#include "th03/present/main_static.hpp"
#include "th03/present/planar.hpp"

namespace th03::present {
namespace {

constexpr std::uint16_t mrs_width = 288U;
constexpr std::uint16_t mrs_height = 184U;
constexpr std::size_t mrs_stride = mrs_width / 8U;
constexpr std::size_t mrs_plane_size = mrs_stride * mrs_height;
constexpr std::int16_t mrs_bottom_offset = 183 * 80;

[[nodiscard]] constexpr bool valid_rect(
	Sprite16AtlasRect rect,
	std::uint16_t width,
	std::uint16_t raw_height
) noexcept
{
	return rect.width == width && rect.raw_height == raw_height &&
		(rect.left % 16U) == 0U &&
		(static_cast<std::uint32_t>(rect.left) + rect.width <=
			sprite16_source_width) &&
		(static_cast<std::uint32_t>(rect.raw_top) + rect.raw_height <=
			sprite16_source_height);
}

[[nodiscard]] constexpr bool valid_layout(
	const MainBombAtlasLayout &layout
) noexcept
{
	for(const auto rect : layout.reimu_falling) {
		if(!valid_rect(rect, 16U, 8U)) {
			return false;
		}
	}
	for(const auto &player : layout.mima_columns) {
		for(const auto rect : player) {
			if(!valid_rect(rect, 16U, 8U)) {
				return false;
			}
		}
	}
	for(const auto &player : layout.marisa_flashes) {
		for(const auto rect : player) {
			if(!valid_rect(rect, 48U, 24U)) {
				return false;
			}
		}
	}
	for(const auto rect : layout.ellen_vectors) {
		if(!valid_rect(rect, 64U, 32U)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool valid_mrs(
	const assets::MrsImage &image,
	assets::MrsOrientation orientation
) noexcept
{
	const auto &cel = image.cel;
	if(image.orientation != orientation || cel.pixel_width != mrs_width ||
		cel.pixel_height != mrs_height || cel.bitplane_size != mrs_plane_size ||
		cel.offset_at_bottom_left != mrs_bottom_offset ||
		cel.vram_dword_width != (mrs_width / 32U) ||
		cel.row_order != assets::PlanarRowOrder::bottom_to_top ||
		cel.bit_order != assets::PlanarBitOrder::most_significant_bit_left ||
		!cel.alpha || cel.alpha->size() != mrs_plane_size) {
		return false;
	}
	for(const auto &plane : cel.color_planes) {
		if(plane.size() != mrs_plane_size) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] constexpr std::int32_t align_left_8(
	std::int16_t left
) noexcept
{
	const auto wide = static_cast<std::int32_t>(left);
	auto remainder = wide % 8;
	if(remainder < 0) {
		remainder += 8;
	}
	return wide - remainder;
}

} // namespace

MainBombAtlasLayout authentic_main_bomb_atlas_layout() noexcept
{
	MainBombAtlasLayout layout{};
	for(std::size_t player = 0U; player < main_bomb_player_count; ++player) {
		const auto base = static_cast<std::uint16_t>(
			player * main_playfield_bordered_width
		);
		layout.reimu_falling[player] = {
			static_cast<std::uint16_t>(base + 192U), 0U, 16U, 8U,
		};
		for(std::size_t column = 0U; column < main_mima_bomb_column_count;
			++column) {
			const auto cel = (column < 3U) ? 0U : ((column < 6U) ? 1U : 2U);
			layout.mima_columns[player][column] = {
				static_cast<std::uint16_t>(base + 224U + (cel * 16U)),
				0U,
				16U,
				8U,
			};
		}
		layout.marisa_flashes[player][0U] = {
			static_cast<std::uint16_t>(base + 176U), 48U, 48U, 24U,
		};
		layout.marisa_flashes[player][1U] = {
			static_cast<std::uint16_t>(base + 224U), 48U, 48U, 24U,
		};
		layout.ellen_vectors[player] = {
			static_cast<std::uint16_t>(base + 224U), 8U, 64U, 32U,
		};
	}
	return layout;
}

MainBombsPresenter::MainBombsPresenter(
	Raster &raster,
	const Sprite16Staging &staging,
	MainBombAtlasLayout layout,
	MainBombMrsAssets mrs,
	StaffFlakeTrigTables trig,
	MainPaletteState &palette,
	KotohimeBombFadeCompatibility kotohime_compatibility
) noexcept :
	raster_(&raster),
	staging_(&staging),
	layout_(layout),
	mrs_(mrs),
	trig_(trig),
	palette_interpreter_(raster, palette, kotohime_compatibility)
{
	if(!valid_layout(layout_)) {
		status_ = MainBombsStatus::invalid_layout;
		return;
	}
	if(mrs_.images[0U] == nullptr || mrs_.images[1U] == nullptr ||
		!valid_mrs(
			*mrs_.images[0U], assets::MrsOrientation::as_stored
		) || !valid_mrs(
			*mrs_.images[1U], assets::MrsOrientation::horizontally_flipped
		) || trig_.sine_q8.size() != 256U ||
		trig_.cosine_q8.size() != 256U) {
		status_ = MainBombsStatus::invalid_assets;
		return;
	}
	if(!staging_->committed()) {
		status_ = MainBombsStatus::staging_not_committed;
		return;
	}
	const auto &p2 = *mrs_.images[1U];
	const auto &blue = p2.cel.color_planes[
		static_cast<std::size_t>(assets::PlanarColorPlane::blue)
	];
	for(std::size_t offset = 0U; offset < altered_p2_blue_.size(); ++offset) {
		altered_p2_blue_[offset] = static_cast<std::uint8_t>(
			blue[offset] | static_cast<std::uint8_t>(~(*p2.cel.alpha)[offset])
		);
	}
}

bool MainBombsPresenter::fail(MainBombsStatus status) noexcept
{
	if(status_ == MainBombsStatus::ready) {
		status_ = status;
	}
	return false;
}

bool MainBombsPresenter::apply_palette(const BombFrameStep &step) noexcept
{
	palette_interpreter_.bomb_frame_step(step);
	if(palette_interpreter_.status() != MainBombPaletteStatus::ready) {
		return fail(MainBombsStatus::palette_failed);
	}
	return true;
}

bool MainBombsPresenter::begin_pending(
	PendingKind kind,
	std::uint8_t player,
	std::uint8_t frame
) noexcept
{
	if(pending_.kind != PendingKind::none) {
		return fail(MainBombsStatus::invalid_observation);
	}
	pending_ = {kind, player, frame, 0U};
	return true;
}

void MainBombsPresenter::clear_pending() noexcept
{
	pending_ = {};
}

bool MainBombsPresenter::fill_playfield(std::uint8_t player) noexcept
{
	return raster_->clear(
		{
			static_cast<std::int32_t>(
				main_playfield_border + player * main_playfield_bordered_width
			),
			0,
			mrs_width,
			mrs_height,
		},
		static_cast<Index4>(player)
	) || fail(MainBombsStatus::draw_failed);
}

bool MainBombsPresenter::draw_mrs(
	std::uint8_t player,
	std::int16_t left
) noexcept
{
	const auto &image = *mrs_.images[player];
	PlanarSource source{};
	for(std::size_t plane = 0U; plane < planar_plane_count; ++plane) {
		source.planes[plane] = image.cel.color_planes[plane];
	}
	if(player == 1U) {
		source.planes[static_cast<std::size_t>(PlanarPlane::blue)] =
			altered_p2_blue_;
	}
	source.width = mrs_width;
	source.height = mrs_height;
	source.stride = mrs_stride;
	source.row_order = PackedRowOrder::bottom_to_top;
	return raster_->blit_planar(
		source, {align_left_8(left), 8}, PlanarWriteMode::replace
	) || fail(MainBombsStatus::draw_failed);
}

} // namespace th03::present
