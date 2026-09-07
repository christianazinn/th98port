// TH01's renderer model, pinned.
//
// WHAT THIS COVERS. The three donor behaviours that a native port is most
// likely to "clean up" and thereby change:
//
//   1. the 16-dot outward snap of the EGC restore blit's X range, which
//      unblits up to eight extra pixels on each side;
//   2. its deliberate absence of clipping, which wraps an overhanging X range
//      into the next or previous VRAM line -- `th01/hardware/egcwave.cpp`
//      exists to exercise exactly that, and its header says so; and
//   3. two address-arithmetic spellings the donor keeps distinct and that
//      disagree for negative X.
//
// Each of these is a place where `th03-port`'s `Raster` cannot stand in, and
// each is small enough to state as an equality rather than as prose.
//
// WHAT IT DOES NOT COVER. Any claim about the real hardware past the end of a
// plane. `Vram` refuses those accesses; the refusal is a guard, not a
// measurement, and the test pins the guard rather than a behaviour.

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>
#include <vector>

#include "th01/present/vram.hpp"

namespace {

using namespace th01::present;

unsigned checks = 0U;
unsigned failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

void expect_eq(long long actual, long long expected, const char *message)
{
	++checks;
	if(actual != expected) {
		++failures;
		std::cerr
			<< "FAIL: " << message << " (expected " << expected
			<< ", got " << actual << ")\n";
	}
}

// The donor's `pc98.h:100-106` and `:228-231`, restated as numbers so that a
// future edit to the header is a test failure rather than a silent redefinition
// of the machine.
void test_geometry_matches_the_donor()
{
	expect_eq(res_x, 640, "RES_X");
	expect_eq(res_y, 400, "RES_Y");
	expect_eq(byte_dots, 8, "BYTE_DOTS");
	expect_eq(row_size, 80, "ROW_SIZE");
	expect_eq(plane_size, 32000, "PLANE_SIZE");
	expect_eq(egc_register_dots, 16, "EGC_REGISTER_DOTS");
	expect_eq(egc_register_size, 2, "EGC_REGISTER_SIZE");
	expect_eq(static_cast<long long>(page_pixels), 256000, "page pixels");
	expect_eq(static_cast<long long>(plane_count), 4, "planes");
	expect_eq(static_cast<long long>(page_count), 2, "pages");
}

// `egcrect.cpp:13` is `x_floor &= 0xFFF0`. Under Turbo C++'s 16-bit `int` that
// literal IS -16 and the statement is an arithmetic floor. On this host the
// same literal is 65520 and the statement is not. The two are compared here so
// that the hazard is demonstrated on the machine the port runs on, rather than
// asserted about a compiler nobody here has.
void test_the_sixteen_dot_floor_survives_a_32_bit_int()
{
	expect_eq(floor_to_egc_register(0), 0, "floor(0)");
	expect_eq(floor_to_egc_register(15), 0, "floor(15)");
	expect_eq(floor_to_egc_register(16), 16, "floor(16)");
	expect_eq(floor_to_egc_register(639), 624, "floor(639)");
	expect_eq(floor_to_egc_register(-1), -16, "floor(-1)");
	expect_eq(floor_to_egc_register(-8), -16, "floor(-8)");
	expect_eq(floor_to_egc_register(-16), -16, "floor(-16)");
	expect_eq(floor_to_egc_register(-17), -32, "floor(-17)");

	// The transcription trap, shown rather than described.
	const int naive = -8 & 0xFFF0;
	expect(
		naive != floor_to_egc_register(-8),
		"a literal `& 0xFFF0` disagrees with the 16-dot floor for negative X "
		"on this host, which is why the header does not spell it that way"
	);
	expect(
		(-8 & 0xFFF0) == 65520,
		"and it disagrees by becoming a large positive offset, not by being "
		"off by one"
	);
}

// `planar.h:105` shifts, `planar.h:128` divides. The donor uses both. For
// negative X an arithmetic shift floors and integer division truncates toward
// zero, so the two disagree -- by one plane byte, i.e. eight pixels.
void test_the_two_offset_spellings_disagree_for_negative_x()
{
	// Agreement everywhere it is safe to assume it.
	expect_eq(vram_offset_shift(0, 0), 0, "shift(0,0)");
	expect_eq(vram_offset_muldiv(0, 0), 0, "muldiv(0,0)");
	expect_eq(vram_offset_shift(632, 399), 31999, "shift(632,399)");
	expect_eq(vram_offset_muldiv(632, 399), 31999, "muldiv(632,399)");
	expect_eq(vram_offset_shift(-16, 0), -2, "shift(-16,0)");
	expect_eq(vram_offset_muldiv(-16, 0), -2, "muldiv(-16,0)");

	// And the disagreement, at the smallest X that produces one.
	expect_eq(vram_offset_shift(-1, 0), -1, "shift(-1,0) floors");
	expect_eq(vram_offset_muldiv(-1, 0), 0, "muldiv(-1,0) truncates");
	expect(
		vram_offset_shift(-1, 0) != vram_offset_muldiv(-1, 0),
		"the shift and divide spellings of the VRAM offset are not the same "
		"function; a port that unifies them moves pixels for negative X"
	);
}

void fill_page_1_plane_bytes(Vram &vram, std::uint8_t value)
{
	for(int offset = 0; offset < plane_size; ++offset) {
		for(const auto plane : {Plane::b, Plane::r, Plane::g, Plane::e}) {
			if(!vram.set_plane_byte(Page::one, plane, offset, value)) {
				expect(false, "filling page 1 should never be refused");
				return;
			}
		}
	}
}

std::uint8_t byte_at(const Vram &vram, Page page, Plane plane, int offset)
{
	std::uint8_t value = 0U;
	if(!vram.plane_byte(page, plane, offset, value)) {
		expect(false, "reading an in-plane byte should never be refused");
		return 0U;
	}
	return value;
}

// One EGC access moves 16 dots on ALL FOUR planes, because `egc_setup_copy()`
// enables all four. The donor expresses it as a single 16-bit write to plane B
// (`egc_chunk()`, `planar.h:248`); a port that took that literally would move a
// quarter of the pixels.
void test_one_chunk_moves_four_planes()
{
	Vram vram;
	for(const auto plane : {Plane::b, Plane::r, Plane::g, Plane::e}) {
		expect(
			vram.set_plane_byte(Page::one, plane, 10, 0xA5U),
			"seed page 1 low byte"
		);
		expect(
			vram.set_plane_byte(Page::one, plane, 11, 0x5AU),
			"seed page 1 high byte"
		);
	}

	expect(vram.egc_copy_chunk_1_to_0(10), "the chunk copy runs");

	for(const auto plane : {Plane::b, Plane::r, Plane::g, Plane::e}) {
		expect_eq(byte_at(vram, Page::zero, plane, 10), 0xA5, "plane low byte");
		expect_eq(byte_at(vram, Page::zero, plane, 11), 0x5A, "plane high byte");
	}
	expect_eq(
		byte_at(vram, Page::zero, Plane::b, 9), 0x00,
		"the byte before the chunk is untouched"
	);
	expect_eq(
		byte_at(vram, Page::zero, Plane::b, 12), 0x00,
		"the byte after the chunk is untouched"
	);
}

// The documented ZUN behaviour: the blit covers
// (floor(left/16)*16 .. ceil((left+w)/16)*16), so asking for four pixels at
// x=20 restores sixteen, from x=16 to x=31.
void test_the_x_range_snaps_outward_to_sixteen_dots()
{
	Vram vram;
	fill_page_1_plane_bytes(vram, 0xFFU);

	expect(
		vram.egc_copy_rect_1_to_0_16(20, 0, 4, 1),
		"a four-pixel-wide restore at x=20 runs"
	);

	// x=16..31 is plane bytes 2 and 3 of row 0.
	expect_eq(byte_at(vram, Page::zero, Plane::b, 2), 0xFF, "byte 2 restored");
	expect_eq(byte_at(vram, Page::zero, Plane::b, 3), 0xFF, "byte 3 restored");
	expect_eq(
		byte_at(vram, Page::zero, Plane::b, 1), 0x00,
		"x=8..15 was NOT restored, so the snap is outward only on the "
		"16-dot register the request falls in"
	);
	expect_eq(
		byte_at(vram, Page::zero, Plane::b, 4), 0x00,
		"x=32..39 was not restored"
	);
}

// `th01/hardware/egc.h`: "Does not clamp the X coordinate ... the blit
// operation will wrap around into the next or previous line."
// `th03-port`'s `Raster::copy_page()` clips instead
// (`clipped_mapping()`, `th03-port/src/present/raster.cpp:122`), which is the
// one measured reason it cannot serve as TH01's restore primitive.
void test_an_overhanging_blit_wraps_into_the_next_line()
{
	Vram vram;
	fill_page_1_plane_bytes(vram, 0xFFU);

	// x_floor = 624, x_end = 648: registers at x=624 (bytes 78-79 of row 0)
	// and at x=640, which is not a coordinate -- it is bytes 80-81, the first
	// two bytes of row 1.
	expect(
		vram.egc_copy_rect_1_to_0_16(632, 0, 16, 1),
		"a blit overhanging the right edge runs rather than being clipped away"
	);

	expect_eq(byte_at(vram, Page::zero, Plane::b, 78), 0xFF, "row 0 byte 78");
	expect_eq(byte_at(vram, Page::zero, Plane::b, 79), 0xFF, "row 0 byte 79");
	expect_eq(
		byte_at(vram, Page::zero, Plane::b, 80), 0xFF,
		"the overhang landed on row 1 byte 0 -- it wrapped, it was not clipped"
	);
	expect_eq(byte_at(vram, Page::zero, Plane::b, 81), 0xFF, "row 1 byte 1");
	expect_eq(
		byte_at(vram, Page::zero, Plane::b, 82), 0x00,
		"and it stopped after one wrapped register"
	);
}

// The same mechanism in the other direction: a negative X floors to a negative
// 16-dot register, whose offset is a byte at the END of the previous line.
void test_a_negative_x_wraps_into_the_previous_line()
{
	Vram vram;
	fill_page_1_plane_bytes(vram, 0xFFU);

	// x_floor = -16 -> offset -2; row 5 starts at 400, so the access is at
	// 398: bytes 78-79 of row 4, i.e. x=624..639 one line up.
	expect(
		vram.egc_copy_rect_1_to_0_16(-16, 5, 16, 1),
		"a blit starting left of the screen runs"
	);
	expect_eq(byte_at(vram, Page::zero, Plane::b, 398), 0xFF, "row 4 byte 78");
	expect_eq(byte_at(vram, Page::zero, Plane::b, 399), 0xFF, "row 4 byte 79");
	expect_eq(
		byte_at(vram, Page::zero, Plane::b, 400), 0x00,
		"row 5 byte 0 is not part of the wrapped register"
	);
}

// The guard, pinned as a guard: leaving the plane is refused, and refused
// before anything is written.
void test_leaving_the_plane_is_refused_atomically()
{
	Vram vram;
	fill_page_1_plane_bytes(vram, 0xFFU);

	expect(
		!vram.egc_copy_rect_1_to_0_16(0, 399, 640, 2),
		"a two-row blit starting on the last row leaves the plane and is "
		"refused"
	);
	expect_eq(
		byte_at(vram, Page::zero, Plane::b, 31920), 0x00,
		"and the first row of that blit was not written either, so the "
		"refusal is atomic"
	);
}

// The host seam. PC-98 colour order is B | R<<1 | G<<2 | E<<3, and dots are
// MSB-first inside a plane byte.
void test_indexed_readout_is_the_pc98_colour_order()
{
	Vram vram;
	expect(vram.set_plane_byte(Page::zero, Plane::b, 0, 0x80U), "seed B");
	expect(vram.set_plane_byte(Page::zero, Plane::g, 0, 0xC0U), "seed G");
	expect(vram.set_plane_byte(Page::zero, Plane::e, 0, 0x40U), "seed E");

	std::vector<Index4> out(page_pixels, 0U);
	expect(
		vram.to_indexed(Page::zero, std::span<Index4>(out)),
		"the readout runs at the exact host size"
	);

	expect_eq(out[0], 0x1 | 0x4, "pixel 0 is B+G");
	expect_eq(out[1], 0x4 | 0x8, "pixel 1 is G+E");
	expect_eq(out[2], 0, "pixel 2 is background");
	expect_eq(out[page_pixels - 1U], 0, "the last pixel exists and is clear");

	std::vector<Index4> wrong_size(page_pixels - 1U, 0U);
	expect(
		!vram.to_indexed(Page::zero, std::span<Index4>(wrong_size)),
		"a target of the wrong size is refused"
	);
}

// Page 1 is a source, never a scanout target, but TH01 does write it during
// gameplay: `b15m.cpp`'s `ent_put_both()` draws the same cel to page 1 and
// page 0 so the restore source tracks a moving entity. The two pages are
// therefore independent storage, which is what this pins.
void test_the_two_pages_are_independent()
{
	Vram vram;
	expect(vram.set_plane_byte(Page::zero, Plane::b, 100, 0x11U), "write p0");
	expect(vram.set_plane_byte(Page::one, Plane::b, 100, 0x22U), "write p1");
	expect_eq(byte_at(vram, Page::zero, Plane::b, 100), 0x11, "page 0 kept");
	expect_eq(byte_at(vram, Page::one, Plane::b, 100), 0x22, "page 1 kept");

	// And the access page is state the donor threads globally, so it is state
	// here too -- but it is not what the blits above address.
	expect(vram.access_page() == Page::zero, "access page starts at 0");
	vram.set_access_page(Page::one);
	expect(vram.access_page() == Page::one, "access page is settable");
}

// `egcrect.cpp` selects page 0 as the last act of its INNER loop, so a
// rectangle that copies nothing leaves the selector alone. `restore_rect()`
// sets it unconditionally; the difference only becomes observable once the
// clipping is gone, which is why it is pinned rather than inherited.
void test_the_access_page_follows_the_donors_spelling()
{
	Vram vram;
	vram.set_access_page(Page::one);
	expect(
		vram.egc_copy_rect_1_to_0_16(0, 0, 16, 1),
		"a one-register blit runs"
	);
	expect(
		vram.access_page() == Page::zero,
		"a blit that copied something leaves the access page at 0"
	);

	vram.set_access_page(Page::one);
	expect(
		vram.egc_copy_rect_1_to_0_16(0, 0, 16, 0),
		"a zero-height blit is not an error"
	);
	expect(
		vram.access_page() == Page::one,
		"but it copies nothing and therefore does not touch the selector"
	);

	vram.set_access_page(Page::one);
	expect(
		vram.egc_copy_rect_1_to_0_16(0, 0, 0, 4),
		"a zero-width blit is not an error"
	);
	expect(
		vram.access_page() == Page::one,
		"and it too leaves the selector alone"
	);
}

} // namespace

int main()
{
	test_geometry_matches_the_donor();
	test_the_sixteen_dot_floor_survives_a_32_bit_int();
	test_the_two_offset_spellings_disagree_for_negative_x();
	test_one_chunk_moves_four_planes();
	test_the_x_range_snaps_outward_to_sixteen_dots();
	test_an_overhanging_blit_wraps_into_the_next_line();
	test_a_negative_x_wraps_into_the_previous_line();
	test_leaving_the_plane_is_refused_atomically();
	test_indexed_readout_is_the_pc98_colour_order();
	test_the_two_pages_are_independent();
	test_the_access_page_follows_the_donors_spelling();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return (failures == 0U) ? 0 : 1;
}
