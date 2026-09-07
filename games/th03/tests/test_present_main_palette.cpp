#include "th03/present/main_palette.hpp"

#include <cstddef>
#include <cstdio>
#include <string_view>
#include <type_traits>

namespace {

using th03::assets::Rgb4;
using th03::present::MainPaletteState;
using th03::present::Raster;
using th03::present::Rgb8;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::fprintf(
			stderr,
			"FAIL: %.*s\n",
			static_cast<int>(message.size()),
			message.data()
		);
	}
}

void test_logical_rgb8_baseline_and_entry_restore()
{
	Raster raster;
	MainPaletteState palette;
	expect(!palette.baseline_ready(), "baseline starts unavailable");
	expect(!palette.dirty(), "palette starts clean");
	const auto before_missing_baseline = raster.pending_palette();
	expect(
		!palette.restore_entry_deferred(raster, 0U) &&
		!palette.restore_baseline_and_show(raster),
		"restore operations reject a missing baseline"
	);
	expect(
		raster.pending_palette() == before_missing_baseline && !palette.dirty(),
		"missing-baseline rejection leaves palette and latch untouched"
	);
	expect(
		raster.set_pending_palette_entry_rgb8(3U, {0x7FU, 0x80U, 0xFFU}),
		"seed RGB8 palette entry"
	);
	expect(palette.capture_baseline(raster), "capture RGB8 baseline once");
	expect(palette.baseline_ready(), "RGB8 baseline is captured");
	expect(
		!palette.capture_baseline(raster),
		"process baseline cannot be replaced after capture"
	);
	expect(
		palette.set_entry_deferred(raster, 3U, {0x12U, 0x34U, 0x56U}),
		"deferred RGB8 entry write succeeds"
	);
	expect(
		raster.pending_palette().colors[3U] == Rgb8{0x12U, 0x34U, 0x56U},
		"pending RGB8 keeps all low nibbles"
	);
	expect(palette.dirty(), "deferred entry write marks changed");
	expect(
		palette.restore_entry_deferred(raster, 3U),
		"saved entry restore succeeds"
	);
	expect(
		raster.pending_palette().colors[3U] == Rgb8{0x7FU, 0x80U, 0xFFU},
		"saved RGB8 entry restores byte-exactly"
	);

	const auto before_invalid = raster.pending_palette();
	expect(
		!palette.set_entry_deferred(raster, 16U, {1U, 2U, 3U}),
		"out-of-range deferred entry is rejected"
	);
	expect(
		raster.pending_palette() == before_invalid && palette.dirty(),
		"invalid entry leaves logical palette and latch unchanged"
	);
	expect(palette.apply_if_changed(raster), "clean latch before atomicity probe");
	const auto clean_before_invalid = raster.pending_palette();
	expect(
		!palette.set_entry_deferred(raster, 16U, {1U, 2U, 3U}),
		"out-of-range write is also rejected from a clean state"
	);
	expect(
		raster.pending_palette() == clean_before_invalid && !palette.dirty(),
		"invalid write does not spuriously set a clean latch"
	);
}

void test_immediate_show_keeps_dirty_latch()
{
	Raster raster;
	MainPaletteState palette;
	expect(
		palette.set_entry_deferred(raster, 2U, {0xFFU, 0x80U, 0x7FU}),
		"deferred RGB8 write succeeds"
	);
	palette.set_tone_and_show_now(raster, 100);
	expect(
		raster.applied_palette().colors[2U] == Rgb4{15U, 8U, 7U},
		"immediate show delegates RGB8 high-nibble conversion to Raster"
	);
	expect(palette.dirty(), "immediate show does not clear a deferred latch");
	palette.show_now(raster);
	expect(palette.dirty(), "plain immediate show also keeps the latch");
	expect(
		palette.apply_if_changed(raster),
		"deferred footer shows a pending palette"
	);
	expect(!palette.dirty(), "deferred footer clears latch after showing");
	expect(
		!palette.apply_if_changed(raster),
		"clean deferred footer performs no show"
	);
}

void test_full_baseline_restore_and_tone_clamp()
{
	Raster raster;
	MainPaletteState palette;
	expect(
		raster.set_pending_palette_entry_rgb8(0U, {0xF1U, 0x22U, 0x43U}),
		"seed first baseline entry"
	);
	expect(
		raster.set_pending_palette_entry_rgb8(15U, {0x10U, 0x20U, 0x30U}),
		"seed final baseline entry"
	);
	expect(palette.capture_baseline(raster), "capture full restore baseline");
	expect(
		palette.set_entry_deferred(raster, 0U, {0U, 0U, 0U}) &&
			palette.set_entry_deferred(raster, 15U, {0U, 0U, 0U}),
		"overwrite baseline entries through deferred writes"
	);
	palette.set_tone_deferred(raster, -1);
	expect(palette.dirty(), "deferred raw tone marks changed");
	expect(
		palette.restore_baseline_and_show(raster),
		"full baseline restore succeeds"
	);
	expect(
		raster.pending_palette().colors[0U] == Rgb8{0xF1U, 0x22U, 0x43U} &&
			raster.pending_palette().colors[15U] == Rgb8{0x10U, 0x20U, 0x30U},
		"full baseline restore replaces every logical source entry"
	);
	expect(
		raster.palette_tone() == -1 &&
			raster.applied_palette().colors[0U] == Rgb4{},
		"raw low tone and local clamp remain Raster behavior"
	);
	expect(
		palette.dirty(),
		"baseline restore's immediate show does not clear prior dirty state"
	);
	palette.set_tone_and_show_now(raster, 201);
	expect(
		raster.palette_tone() == 201 &&
			raster.applied_palette().colors[0U] == Rgb4{15U, 15U, 15U},
		"raw high tone and local clamp remain Raster behavior"
	);
	expect(palette.dirty(), "immediate high-tone show retains dirty state");
}

void test_accelerator_palette_restore()
{
	Raster raster;
	MainPaletteState palette;
	expect(
		raster.set_pending_palette_entry_rgb8(0U, {0xF0U, 0xF0U, 0xF0U}) &&
			palette.capture_baseline(raster) &&
			palette.set_entry_deferred(raster, 1U, {0xFFU, 0xFFU, 0xFFU}),
		"prepare a dirty process palette before accelerator restore"
	);
	raster.set_palette_tone(37);
	th03::present::Palette16Rgb8 restored{};
	restored.colors[0U] = {0x22U, 0x44U, 0x66U};
	restored.colors[15U] = {0x88U, 0xAAU, 0xCCU};
	palette.restore_accelerator_palette(raster, restored);
	expect(
		raster.pending_palette() == restored && raster.palette_tone() == 100 &&
			raster.applied_palette().colors[0U] == Rgb4{2U, 4U, 6U} &&
			raster.applied_palette().colors[15U] == Rgb4{8U, 10U, 12U},
		"accelerator restore replaces logical/applied RGB8 at source tone 100"
	);
	expect(!palette.dirty(), "accelerator restore clears the deferred latch");
	expect(
		palette.set_entry_deferred(raster, 0U, {}) &&
			palette.restore_entry_deferred(raster, 0U) &&
			raster.pending_palette().colors[0U] == restored.colors[0U],
		"later palette effects restore the accelerator's baseline"
	);
}

static_assert(
	std::is_trivially_destructible_v<MainPaletteState>,
	"MainPaletteState has no owning heap storage"
);
static_assert(
	sizeof(MainPaletteState) <= sizeof(th03::present::Palette16Rgb8) + 2U,
	"MainPaletteState is only the fixed palette snapshot and two latches"
);

} // namespace

int main()
{
	test_logical_rgb8_baseline_and_entry_restore();
	test_immediate_show_keeps_dirty_latch();
	test_full_baseline_restore_and_tone_clamp();
	test_accelerator_palette_restore();
	if(failures != 0U) {
		std::fprintf(
			stderr,
			"%zu of %zu checks failed\n",
			failures,
			checks
		);
		return 1;
	}
	std::printf("All %zu MAIN palette checks passed\n", checks);
	return 0;
}
