#include "th01/present/vram.hpp"

namespace th01::present {

namespace {

// The donor writes one 16-bit `egc_chunk()`; this is that access as the two
// plane bytes it really is, at `offset` and `offset + 1`.
constexpr int chunk_bytes = egc_register_size;

} // namespace

Vram::Vram() = default;

bool Vram::offset_in_plane(int offset) noexcept
{
	return (offset >= 0) && (offset < plane_size);
}

void Vram::set_access_page(Page page) noexcept
{
	access_page_ = page;
}

Page Vram::access_page() const noexcept
{
	return access_page_;
}

bool Vram::plane_byte(
	Page page, Plane plane, int offset, std::uint8_t &out
) const noexcept
{
	if(!offset_in_plane(offset)) {
		return false;
	}
	out = planes_
		[static_cast<std::size_t>(static_cast<std::uint8_t>(page))]
		[static_cast<std::size_t>(static_cast<std::uint8_t>(plane))]
		[static_cast<std::size_t>(offset)];
	return true;
}

bool Vram::set_plane_byte(
	Page page, Plane plane, int offset, std::uint8_t value
) noexcept
{
	if(!offset_in_plane(offset)) {
		return false;
	}
	planes_
		[static_cast<std::size_t>(static_cast<std::uint8_t>(page))]
		[static_cast<std::size_t>(static_cast<std::uint8_t>(plane))]
		[static_cast<std::size_t>(offset)] = value;
	return true;
}

bool Vram::egc_copy_chunk_1_to_0(int offset) noexcept
{
	if(!offset_in_plane(offset) || !offset_in_plane(offset + chunk_bytes - 1)) {
		return false;
	}
	for(std::size_t plane = 0U; plane < plane_count; ++plane) {
		for(int byte = 0; byte < chunk_bytes; ++byte) {
			const auto at = static_cast<std::size_t>(offset + byte);
			planes_[0U][plane][at] = planes_[1U][plane][at];
		}
	}
	return true;
}

bool Vram::egc_copy_rect_1_to_0_16(int left, int top, int w, int h) noexcept
{
	// `egcrect.cpp` verbatim, including the two things that make it not a
	// `copy_page()`: the X range is snapped outward to 16-dot registers, and
	// nothing is clipped.
	const int x_end = left + w;
	const int x_floor = floor_to_egc_register(left);
	int vo_row = vram_offset_shift(x_floor, top);

	// Validate the whole traversal before touching a byte, so a rectangle that
	// leaves a plane fails without leaving a half-finished blit behind.
	{
		int probe_row = vo_row;
		for(int row = 0; row < h; ++row) {
			int vo = probe_row;
			for(int column = x_floor; column < x_end;
				vo += egc_register_size, column += egc_register_dots
			) {
				if(
					!offset_in_plane(vo) ||
					!offset_in_plane(vo + chunk_bytes - 1)
				) {
					return false;
				}
			}
			probe_row += row_size;
		}
	}

	bool copied_any = false;
	for(int row = 0; row < h; ++row) {
		int vo = vo_row;
		for(int column = x_floor; column < x_end;
			vo += egc_register_size, column += egc_register_dots
		) {
			if(!egc_copy_chunk_1_to_0(vo)) {
				return false;
			}
			copied_any = true;
		}
		vo_row += row_size;
	}
	if(copied_any) {
		// `egcrect.cpp:24` leaves the accessed page at 0, because the last
		// thing the inner loop does is select it. An empty rectangle never
		// enters the loop and therefore never touches the selector.
		access_page_ = Page::zero;
	}
	return true;
}

bool Vram::to_indexed(Page page, std::span<Index4> out) const noexcept
{
	if(out.size() != page_pixels) {
		return false;
	}
	const auto &page_planes =
		planes_[static_cast<std::size_t>(static_cast<std::uint8_t>(page))];
	std::size_t pixel = 0U;
	for(int y = 0; y < res_y; ++y) {
		for(int byte_x = 0; byte_x < row_size; ++byte_x) {
			const auto at =
				static_cast<std::size_t>((y * row_size) + byte_x);
			const std::uint8_t b = page_planes[0U][at];
			const std::uint8_t r = page_planes[1U][at];
			const std::uint8_t g = page_planes[2U][at];
			const std::uint8_t e = page_planes[3U][at];
			for(int bit = byte_dots - 1; bit >= 0; --bit) {
				const auto shift = static_cast<unsigned>(bit);
				const auto value = static_cast<Index4>(
					((b >> shift) & 1U) |
					(((r >> shift) & 1U) << 1U) |
					(((g >> shift) & 1U) << 2U) |
					(((e >> shift) & 1U) << 3U)
				);
				out[pixel] = value;
				++pixel;
			}
		}
	}
	return true;
}

} // namespace th01::present
