#include "th03/present/main_bomb_palette.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <type_traits>

namespace {

using th03::BombFrameStep;
using th03::BombFinished;
using th03::GameplayObserver;
using th03::PlayerId;
using th03::assets::Rgb4;
using th03::present::KotohimeBombFadeCompatibility;
using th03::present::MainBombPaletteInterpreter;
using th03::present::MainBombPaletteStatus;
using th03::present::MainPaletteState;
using th03::present::Raster;
using th03::present::Rgb8;

// Source coverage: th03/main/player/ch_shot.cpp::{reimu_bomb,marisa_bomb},
// th03/main/bullet/bullet.cpp::mima_bomb, th03/main/player/chybomb.cpp::{
// chiyuri_bomb,ellen_bomb,kana_bomb,kotohime_bomb}, th03/main_05.cpp::
// rikako_bomb, and th03/main/player/yumemi_bomb.cpp::yumemi_bomb.

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

constexpr std::int8_t paletted_character_for(std::uint8_t character) noexcept
{
	return static_cast<std::int8_t>((character * 2U) + 1U);
}

BombFrameStep step(
	PlayerId player,
	std::uint8_t character,
	std::uint8_t frame
) noexcept
{
	return {
		.player = player,
		.paletted_character = paletted_character_for(character),
		.bomb_frame = frame,
	};
}

void expect_pending(
	const Raster &raster,
	std::uint8_t player,
	Rgb8 expected,
	std::string_view message
)
{
	expect(raster.pending_palette().colors[player] == expected, message);
}

void test_character_palette_formulas()
{
	Raster raster;
	MainPaletteState palette;
	MainBombPaletteInterpreter interpreter(raster, palette);

	interpreter.bomb_frame_step(step(PlayerId::p1, 0U, 63U));
	expect_pending(raster, 0U, {252U, 189U, 189U}, "Reimu early RGB8");
	expect(palette.dirty(), "Reimu early write is deferred");
	(void)palette.apply_if_changed(raster);
	interpreter.bomb_frame_step(step(PlayerId::p1, 0U, 65U));
	expect(
		raster.palette_tone() == 180 && palette.dirty(),
		"Reimu 64..96 uses the 100/180 tone alternation"
	);
	interpreter.bomb_frame_step(step(PlayerId::p1, 0U, 97U));
	expect(raster.palette_tone() == 100, "Reimu 97..111 restores tone 100");
	interpreter.bomb_frame_step(step(PlayerId::p1, 0U, 112U));
	expect_pending(raster, 0U, {127U, 127U, 127U}, "Reimu late fade");

	Raster mima_raster;
	MainPaletteState mima_palette;
	MainBombPaletteInterpreter mima(mima_raster, mima_palette);
	mima.bomb_frame_step(step(PlayerId::p1, 1U, 80U));
	expect_pending(mima_raster, 0U, {16U, 16U, 16U}, "Mima low-byte imul fade");
	mima.bomb_frame_step(step(PlayerId::p1, 1U, 82U));
	expect_pending(mima_raster, 0U, {100U, 0U, 0U}, "Mima even red flash");
	mima.bomb_frame_step(step(PlayerId::p1, 1U, 83U));
	expect_pending(mima_raster, 0U, {0U, 0U, 0U}, "Mima odd black flash");
	mima.bomb_frame_step(step(PlayerId::p1, 1U, 144U));
	expect_pending(mima_raster, 0U, {127U, 127U, 127U}, "Mima late fade");

	Raster marisa_raster;
	MainPaletteState marisa_palette;
	MainBombPaletteInterpreter marisa(marisa_raster, marisa_palette);
	marisa.bomb_frame_step(step(PlayerId::p1, 2U, 63U));
	expect_pending(marisa_raster, 0U, {4U, 4U, 4U}, "Marisa early wrapped fade");
	(void)marisa_palette.apply_if_changed(marisa_raster);
	const auto marisa_mid = marisa_raster.pending_palette();
	marisa.bomb_frame_step(step(PlayerId::p1, 2U, 64U));
	expect(
		marisa_raster.pending_palette() == marisa_mid && !marisa_palette.dirty(),
		"Marisa middle phase has no palette transaction"
	);
	marisa.bomb_frame_step(step(PlayerId::p1, 2U, 144U));
	expect_pending(marisa_raster, 0U, {127U, 127U, 127U}, "Marisa late fade");

	Raster ellen_raster;
	MainPaletteState ellen_palette;
	MainBombPaletteInterpreter ellen(ellen_raster, ellen_palette);
	ellen.bomb_frame_step(step(PlayerId::p1, 3U, 63U));
	expect_pending(ellen_raster, 0U, {252U, 126U, 126U}, "Ellen early red double");
	ellen.bomb_frame_step(step(PlayerId::p1, 3U, 64U));
	expect_pending(ellen_raster, 0U, {0U, 0U, 32U}, "Ellen even flash");
	ellen.bomb_frame_step(step(PlayerId::p1, 3U, 65U));
	expect_pending(ellen_raster, 0U, {255U, 128U, 128U}, "Ellen odd flash");
	ellen.bomb_frame_step(step(PlayerId::p1, 3U, 144U));
	expect_pending(ellen_raster, 0U, {254U, 127U, 127U}, "Ellen late red double");

	Raster kana_raster;
	MainPaletteState kana_palette;
	MainBombPaletteInterpreter kana(kana_raster, kana_palette);
	kana.bomb_frame_step(step(PlayerId::p1, 5U, 63U));
	expect_pending(kana_raster, 0U, {1U, 1U, 1U}, "Kana early scalar fade");
	kana.bomb_frame_step(step(PlayerId::p1, 5U, 64U));
	expect_pending(kana_raster, 0U, {160U, 160U, 160U}, "Kana bright flash");
	kana.bomb_frame_step(step(PlayerId::p1, 5U, 66U));
	expect_pending(kana_raster, 0U, {0U, 0U, 0U}, "Kana dark flash");
	kana.bomb_frame_step(step(PlayerId::p1, 5U, 144U));
	expect_pending(kana_raster, 0U, {254U, 127U, 127U}, "Kana late red double");

	Raster rikako_raster;
	MainPaletteState rikako_palette;
	MainBombPaletteInterpreter rikako(rikako_raster, rikako_palette);
	rikako.bomb_frame_step(step(PlayerId::p1, 6U, 63U));
	expect_pending(rikako_raster, 0U, {252U, 252U, 252U}, "Rikako early fade");
	(void)rikako_palette.apply_if_changed(rikako_raster);
	const auto rikako_mid = rikako_raster.pending_palette();
	rikako.bomb_frame_step(step(PlayerId::p1, 6U, 64U));
	expect(
		rikako_raster.pending_palette() == rikako_mid && !rikako_palette.dirty(),
		"Rikako middle phase has no palette transaction"
	);
	rikako.bomb_frame_step(step(PlayerId::p1, 6U, 144U));
	expect_pending(rikako_raster, 0U, {254U, 127U, 127U}, "Rikako late red double");

	Raster chiyuri_raster;
	MainPaletteState chiyuri_palette;
	MainBombPaletteInterpreter chiyuri(chiyuri_raster, chiyuri_palette);
	chiyuri.bomb_frame_step(step(PlayerId::p1, 7U, 63U));
	expect_pending(chiyuri_raster, 0U, {3U, 3U, 3U}, "Chiyuri early inverse fade");
	chiyuri.bomb_frame_step(step(PlayerId::p1, 7U, 64U));
	expect(
		chiyuri_raster.palette_tone() == 60,
		"Chiyuri first middle tone"
	);
	chiyuri.bomb_frame_step(step(PlayerId::p1, 7U, 66U));
	expect(chiyuri_raster.palette_tone() == 120, "Chiyuri second middle tone");
	chiyuri.bomb_frame_step(step(PlayerId::p1, 7U, 144U));
	expect(
		chiyuri_raster.palette_tone() == 100,
		"Chiyuri late phase restores tone before RGB"
	);
	expect_pending(chiyuri_raster, 0U, {254U, 127U, 127U}, "Chiyuri late red double");
}

bool kotohime_red_provider(
	void *context,
	const BombFrameStep &observed,
	std::uint8_t fade_green_blue,
	std::uint8_t &red_out
) noexcept
{
	const auto expected_frame = *static_cast<const std::uint8_t *>(context);
	if(observed.bomb_frame != expected_frame || fade_green_blue != 127U) {
		return false;
	}
	red_out = 0xA5U;
	return true;
}

void test_kotohime_compatibility_is_atomic()
{
	Raster early_raster;
	MainPaletteState early_palette;
	MainBombPaletteInterpreter early(early_raster, early_palette);
	early.bomb_frame_step(step(PlayerId::p1, 4U, 63U));
	expect_pending(
		early_raster,
		0U,
		{253U, 221U, 189U},
		"Kotohime early RGB8 additions wrap independently"
	);

	Raster raster;
	MainPaletteState palette;
	MainBombPaletteInterpreter default_interpreter(raster, palette);
	const auto before_palette = raster.pending_palette();
	const auto before_tone = raster.palette_tone();
	default_interpreter.bomb_frame_step(step(PlayerId::p1, 4U, 144U));
	expect(
		default_interpreter.status() ==
			MainBombPaletteStatus::unsupported_kotohime_fade,
		"Kotohime late fade explicitly reports unsupported without a provider"
	);
	expect(
		raster.pending_palette() == before_palette &&
			raster.palette_tone() == before_tone && !palette.dirty(),
		"unsupported Kotohime fade is palette-atomic and fail-closed"
	);

	std::uint8_t expected_frame = 144U;
	MainBombPaletteInterpreter compatible(
		raster,
		palette,
		KotohimeBombFadeCompatibility{kotohime_red_provider, &expected_frame}
	);
	compatible.bomb_frame_step(step(PlayerId::p1, 4U, 144U));
	expect(
		compatible.status() == MainBombPaletteStatus::ready &&
			raster.palette_tone() == 100 && palette.dirty(),
		"Kotohime provider supplies an accepted late fade"
	);
	expect_pending(
		raster,
		0U,
		{0xA5U, 127U, 127U},
		"Kotohime provider returns the final stored red byte"
	);

	compatible.bomb_frame_step(step(PlayerId::p2, 4U, 64U));
	expect(
		raster.palette_tone() == 170,
		"Kotohime middle tone uses the source two-frame cadence"
	);
}

void test_yumemi_immediate_show_and_footer()
{
	Raster raster;
	MainPaletteState palette;
	MainBombPaletteInterpreter yumemi(raster, palette);
	expect(
		palette.set_entry_deferred(raster, 0U, {0xF0U, 0xF0U, 0xF0U}),
		"seed a prior deferred palette state"
	);
	yumemi.bomb_frame_step(step(PlayerId::p1, 8U, 65U));
	expect(
		raster.palette_tone() == 60 &&
			raster.applied_palette().colors[0U] == Rgb4{9U, 9U, 9U} &&
			palette.dirty(),
		"Yumemi middle immediate show preserves an older dirty latch"
	);
	yumemi.bomb_frame_step(step(PlayerId::p1, 8U, 72U));
	expect(
		raster.palette_tone() == 140 && palette.dirty(),
		"Yumemi eighth-frame middle tone shows immediately without clearing dirty"
	);

	yumemi.bomb_frame_step(step(PlayerId::p1, 8U, 144U));
	expect(
		raster.palette_tone() == 100 &&
			raster.applied_palette().colors[0U] == Rgb4{15U, 15U, 15U} &&
			palette.dirty(),
		"Yumemi late immediate show precedes its deferred fade write"
	);
	expect_pending(raster, 0U, {127U, 127U, 127U}, "Yumemi late pending RGB fade");
	expect(palette.apply_if_changed(raster), "Yumemi late footer performs second show");
	expect(
		raster.applied_palette().colors[0U] == Rgb4{7U, 7U, 7U} &&
			!palette.dirty(),
		"Yumemi late footer applies deferred RGB after its immediate show"
	);

	Raster early_raster;
	MainPaletteState early_palette;
	MainBombPaletteInterpreter early(early_raster, early_palette);
	early.bomb_frame_step(step(PlayerId::p1, 8U, 64U));
	expect_pending(
		early_raster,
		0U,
		{0U, 0U, 0U},
		"Yumemi early raw assembly island writes equal wrapped RGB components"
	);
}

void test_bomb_completion_restores_saved_entry()
{
	Raster raster;
	MainPaletteState palette;
	expect(
		raster.set_pending_palette_entry_rgb8(0U, {0x21U, 0x43U, 0x65U}) &&
		palette.capture_baseline(raster),
		"capture the process Bomb palette baseline"
	);
	MainBombPaletteInterpreter interpreter(raster, palette);
	interpreter.bomb_frame_step(step(PlayerId::p1, 0U, 10U));
	(void)palette.apply_if_changed(raster);
	interpreter.bomb_finished(BombFinished{
		.player = PlayerId::p1,
		.paletted_character = paletted_character_for(0U),
	});
	expect(
		interpreter.status() == MainBombPaletteStatus::ready &&
		palette.dirty(),
		"Bomb completion restores its entry through the deferred latch"
	);
	expect_pending(
		raster, 0U, {0x21U, 0x43U, 0x65U},
		"Bomb completion restores the process-saved RGB8 entry"
	);

	Raster missing_raster;
	MainPaletteState missing_palette;
	MainBombPaletteInterpreter missing(missing_raster, missing_palette);
	const auto before = missing_raster.pending_palette();
	missing.bomb_finished(BombFinished{
		.player = PlayerId::p1,
		.paletted_character = paletted_character_for(0U),
	});
	expect(
		missing.status() == MainBombPaletteStatus::missing_baseline &&
		missing_raster.pending_palette() == before && !missing_palette.dirty(),
		"Bomb completion without a saved baseline fails atomically"
	);
}

void test_observer_order_and_invalid_atomicity()
{
	Raster raster;
	MainPaletteState palette;
	MainBombPaletteInterpreter interpreter(raster, palette);
	GameplayObserver &observer = interpreter;
	observer.bomb_frame_step(step(PlayerId::p1, 0U, 65U));
	observer.bomb_frame_step(step(PlayerId::p2, 4U, 64U));
	expect(
		raster.palette_tone() == 170 && palette.dirty(),
		"the observer preserves caller P1-then-P2 order without buffering"
	);

	const auto before_palette = raster.pending_palette();
	const auto before_tone = raster.palette_tone();
	const auto before_dirty = palette.dirty();
	interpreter.bomb_frame_step({
		.player = static_cast<PlayerId>(2U),
		.paletted_character = paletted_character_for(0U),
		.bomb_frame = 0U,
	});
	expect(
		interpreter.status() == MainBombPaletteStatus::invalid_player,
		"invalid player is reported"
	);
	expect(
		raster.pending_palette() == before_palette &&
			raster.palette_tone() == before_tone && palette.dirty() == before_dirty,
		"invalid player leaves palette state untouched"
	);
	interpreter.bomb_frame_step({
		.player = PlayerId::p1,
		.paletted_character = 0,
		.bomb_frame = 0U,
	});
	expect(
		interpreter.status() == MainBombPaletteStatus::invalid_paletted_character,
		"invalid paletted character is reported"
	);
	expect(
		raster.pending_palette() == before_palette &&
			raster.palette_tone() == before_tone && palette.dirty() == before_dirty,
		"invalid character leaves palette state untouched"
	);
	interpreter.bomb_frame_step(step(PlayerId::p1, 0U, 0U));
	expect(
		interpreter.status() == MainBombPaletteStatus::invalid_paletted_character,
		"a later valid Bomb step does not erase a prior presentation error"
	);
}

static_assert(
	std::is_nothrow_constructible_v<
		MainBombPaletteInterpreter,
		Raster &,
		MainPaletteState &
	>,
	"MAIN Bomb palette interpreter construction is no-throw"
);
static_assert(
	sizeof(MainBombPaletteInterpreter) <= (5U * sizeof(void *)) + 8U,
	"MAIN Bomb palette interpreter owns only fixed pointers, policy, and status"
);

} // namespace

int main()
{
	test_character_palette_formulas();
	test_kotohime_compatibility_is_atomic();
	test_yumemi_immediate_show_and_footer();
	test_bomb_completion_restores_saved_entry();
	test_observer_order_and_invalid_atomicity();
	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return 1;
	}
	std::printf("All %zu MAIN Bomb palette checks passed\n", checks);
	return 0;
}
