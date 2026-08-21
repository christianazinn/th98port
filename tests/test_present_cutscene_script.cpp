#include "th03/present/cutscene_script.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace th03::present;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

std::vector<std::uint8_t> bytes(std::string_view text)
{
	std::vector<std::uint8_t> result{};
	result.reserve(text.size());
	for(const auto value : text) {
		result.push_back(static_cast<std::uint8_t>(
			static_cast<unsigned char>(value)
		));
	}
	return result;
}

CutsceneScriptVm make_vm(std::span<const std::uint8_t> script)
{
	auto created = CutsceneScriptVm::create(script);
	expect(created.has_value(), "VM fixture constructs");
	if(!created) {
		std::terminate();
	}
	return std::move(*created);
}

CutsceneScriptVm make_started_vm(std::span<const std::uint8_t> script)
{
	auto vm = make_vm(script);
	auto begun = vm.begin();
	expect(begun.has_value(), "VM fixture begins");
	if(!begun) {
		std::terminate();
	}
	return vm;
}

CutsceneScriptBatch advance(
	CutsceneScriptVm &vm,
	CutsceneScriptInput input = {}
)
{
	auto result = vm.step(input);
	expect(result.has_value(), "VM step succeeds");
	if(!result) {
		std::terminate();
	}
	return std::move(*result);
}

template <typename Operation>
const Operation *operation(const CutsceneScriptBatch &batch, std::size_t index)
{
	const auto operations = batch.operations();
	if(index >= operations.size()) {
		return nullptr;
	}
	return std::get_if<Operation>(&operations[index]);
}

void expect_wait(
	const CutsceneScriptBatch &batch,
	std::size_t index,
	CutsceneWaitKind kind,
	std::uint16_t first,
	std::uint16_t second,
	std::string_view message
)
{
	const auto *wait = operation<CutsceneWait>(batch, index);
	expect(
		wait && wait->kind == kind && wait->first == first &&
			wait->second == second,
		message
	);
}

void test_lifecycle_ownership_separators_and_glyph()
{
	auto empty = CutsceneScriptVm::create({});
	expect(
		!empty && empty.error().code == CutsceneScriptErrorCode::empty_script,
		"empty script is rejected"
	);

	std::vector<std::uint8_t> script{
		0x0DU, 0x20U, 0x82U, 0x5CU,
		static_cast<std::uint8_t>('\\'), static_cast<std::uint8_t>('$'),
	};
	auto vm = make_vm(script);
	auto before_begin = vm.step({});
	expect(
		!before_begin && before_begin.error().code ==
			CutsceneScriptErrorCode::not_started,
		"step before begin is rejected"
	);
	auto begun = vm.begin();
	expect(
		begun && begun->operations().size() == 2U &&
			operation<CutsceneShowPage>(*begun, 0U) &&
			operation<CutsceneShowPage>(*begun, 0U)->page == 0U &&
			operation<CutsceneSnapshotBox>(*begun, 1U),
		"begin emits the caller/source page and box prologue"
	);
	expect(vm.started() && vm.byte_offset() == 0U, "begin consumes no script byte");
	auto begun_twice = vm.begin();
	expect(
		!begun_twice && begun_twice.error().code ==
			CutsceneScriptErrorCode::already_started,
		"begin is single-use"
	);
	script.assign(script.size(), 0U);

	auto batch = advance(vm);
	expect(
		batch.operations().empty() && vm.byte_offset() == 1U,
		"control separator retains its own source iteration"
	);
	batch = advance(vm, {.any_key_held = true});
	expect(
		batch.operations().empty() && vm.byte_offset() == 2U,
		"space retains its own source iteration"
	);
	batch = advance(vm);
	const auto *glyph = operation<CutsceneDrawGlyph>(batch, 0U);
	expect(
		glyph && glyph->position == CutsceneCursor{80, 320} &&
			glyph->shift_jis == std::array<std::uint8_t, 2U>{0x82U, 0x5CU} &&
			glyph->color == 15U && glyph->weight == GraphicsTextWeight::bold,
		"two-byte glyph owns a Shift-JIS trail byte equal to backslash"
	);
	expect_wait(
		batch, 1U, CutsceneWaitKind::frames, 1U, 0U,
		"ordinary glyph emits the default one-frame typing delay"
	);
	expect(vm.cursor() == CutsceneCursor{96, 320}, "glyph advances the cursor");

	batch = advance(vm);
	expect(
		batch.operations().size() == 3U &&
			operation<CutsceneRestoreBox>(batch, 0U) &&
			operation<CutsceneReleaseBox>(batch, 1U) &&
			operation<CutsceneFinish>(batch, 2U) && vm.finished(),
		"TH03 end restores and releases the box before finishing"
	);
	auto after = vm.step({});
	expect(
		!after && after.error().code == CutsceneScriptErrorCode::already_finished,
		"finished VM rejects another iteration"
	);
}

void test_state_palette_and_persistent_default()
{
	auto script = bytes(
		"\\c15\\b3\\v2\\t80\\wi4\\wo5\\fi6\\fo7\\fm8"
		"\\e\\vp1\\b9\\$"
	);
	auto vm = make_started_vm(script);

	auto batch = advance(vm);
	expect(
		operation<CutsceneSetTextColor>(batch, 0U) &&
			operation<CutsceneSetTextColor>(batch, 0U)->color == 15U &&
			vm.text_color() == 15U && vm.number_default() == 15U,
		"color command updates style and installs white as numeric default"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneSetTextWeight>(batch, 0U) &&
			vm.text_weight() == GraphicsTextWeight::black &&
			vm.number_default() == 2U,
		"weight command selects black and leaves bold as numeric default"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneSetTextInterval>(batch, 0U) &&
			vm.text_interval() == 2U && vm.number_default() == 1U,
		"typing interval command updates state"
	);
	batch = advance(vm);
	expect_wait(
		batch, 0U, CutsceneWaitKind::frames, 1U, 0U,
		"tone waits one frame before application"
	);
	expect(
		operation<CutsceneSetTone>(batch, 1U) &&
			operation<CutsceneSetTone>(batch, 1U)->tone == 80U,
		"tone operation follows its wait"
	);

	constexpr std::array fade_colors{
		CutsceneFadeColor::white,
		CutsceneFadeColor::white,
		CutsceneFadeColor::black,
		CutsceneFadeColor::black,
	};
	constexpr std::array fade_directions{
		CutsceneFadeDirection::in,
		CutsceneFadeDirection::out,
		CutsceneFadeDirection::in,
		CutsceneFadeDirection::out,
	};
	constexpr std::array<std::uint16_t, 4U> fade_speeds{4U, 5U, 6U, 7U};
	for(std::size_t index = 0U; index < fade_colors.size(); ++index) {
		batch = advance(vm);
		const auto *fade = operation<CutscenePaletteFade>(batch, 0U);
		expect(
			fade && fade->color == fade_colors[index] &&
				fade->direction == fade_directions[index] &&
				fade->speed == fade_speeds[index],
			"white/black fade maps exactly"
		);
	}

	batch = advance(vm);
	expect(
		operation<CutsceneAudioFade>(batch, 0U) &&
			operation<CutsceneAudioFade>(batch, 0U)->parameter == 8U &&
			vm.number_default() == 1U,
		"song fade preserves the full parsed parameter and default"
	);
	batch = advance(vm);
	expect(
		operation<CutscenePlaySoundEffect>(batch, 0U) &&
			operation<CutscenePlaySoundEffect>(batch, 0U)->id == 1U,
		"argument-less effect reuses the persistent numeric default"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneShowPage>(batch, 0U) &&
			operation<CutsceneShowPage>(batch, 0U)->page == 1U &&
			vm.number_default() == 0U,
		"page command selects the page and installs default zero"
	);
	batch = advance(vm);
	expect(
		batch.operations().empty() &&
			vm.text_weight() == GraphicsTextWeight::black &&
			vm.number_default() == 2U,
		"out-of-domain TH03 weight leaves the prior effect unchanged"
	);
	(void)advance(vm);
}

void test_waits_fast_forward_and_box_commands()
{
	auto script = bytes(
		"\\w12\\wk9\\wm4,32\\wmk5,24\\k128\\s12\\s-\\$"
	);
	auto vm = make_started_vm(script);
	auto batch = advance(vm);
	expect_wait(batch, 0U, CutsceneWaitKind::frames, 12U, 0U, "frame wait");
	batch = advance(vm);
	expect_wait(batch, 0U, CutsceneWaitKind::input_ok, 9U, 0U, "OK wait");
	batch = advance(vm);
	expect_wait(batch, 0U, CutsceneWaitKind::measure, 4U, 32U, "measure wait");
	batch = advance(vm);
	expect_wait(
		batch, 0U, CutsceneWaitKind::input_ok_or_measure, 5U, 24U,
		"OK-or-measure wait"
	);
	batch = advance(vm);
	expect_wait(
		batch, 0U, CutsceneWaitKind::input_change, 0U, 0U,
		"TH03 k command parses but ignores its parameter"
	);
	batch = advance(vm);
	expect_wait(
		batch, 0U, CutsceneWaitKind::input_change, 12U, 0U,
		"box command retains its parameter"
	);
	expect(
		operation<CutsceneRestoreBox>(batch, 1U) &&
			vm.cursor() == CutsceneCursor{80, 320},
		"box wait restores the saved box and cursor"
	);
	batch = advance(vm);
	expect(
		batch.operations().size() == 1U &&
			operation<CutsceneRestoreBox>(batch, 0U),
		"s-minus skips the wait but restores the box"
	);
	(void)advance(vm);

	vm = make_started_vm(script);
	for(std::size_t index = 0U; index < 5U; ++index) {
		batch = advance(vm, {.cancel_held = true, .any_key_held = true});
		expect(batch.operations().empty(), "Cancel suppresses wait command");
	}
	batch = advance(vm, {.cancel_held = true, .any_key_held = true});
	expect(
		batch.operations().size() == 1U &&
			operation<CutsceneRestoreBox>(batch, 0U),
		"Cancel suppresses box wait but retains restoration"
	);
}

void test_opcode_defaults()
{
	auto script = bytes(
		"\\c\\b\\v\\t\\wi\\wo\\fi\\fo\\fm\\g\\ga\\k\\s"
		"\\w\\wk\\wm,\\=\\==,\\e\\$"
	);
	auto vm = make_started_vm(script);
	auto batch = advance(vm);
	expect(
		operation<CutsceneSetTextColor>(batch, 0U) &&
			operation<CutsceneSetTextColor>(batch, 0U)->color == 15U,
		"color defaults to V_WHITE"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneSetTextWeight>(batch, 0U) &&
			operation<CutsceneSetTextWeight>(batch, 0U)->weight ==
				GraphicsTextWeight::bold,
		"weight defaults to bold"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneSetTextInterval>(batch, 0U) &&
			operation<CutsceneSetTextInterval>(batch, 0U)->frames == 1U,
		"typing interval defaults to one"
	);
	batch = advance(vm);
	expect_wait(batch, 0U, CutsceneWaitKind::frames, 1U, 0U, "default tone delay");
	expect(
		operation<CutsceneSetTone>(batch, 1U) &&
			operation<CutsceneSetTone>(batch, 1U)->tone == 100U,
		"tone defaults to 100 percent"
	);
	for(std::size_t index = 0U; index < 4U; ++index) {
		batch = advance(vm);
		expect(
			operation<CutscenePaletteFade>(batch, 0U) &&
				operation<CutscenePaletteFade>(batch, 0U)->speed == 1U,
			"palette fades default to speed one"
		);
	}
	batch = advance(vm);
	expect(
		operation<CutsceneAudioFade>(batch, 0U) &&
			operation<CutsceneAudioFade>(batch, 0U)->parameter == 1U,
		"audio fade defaults to one"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneShake>(batch, 0U) &&
			operation<CutsceneShake>(batch, 0U)->parameter == 8U,
		"shake defaults to eight"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneDrawGaiji>(batch, 0U) &&
			operation<CutsceneDrawGaiji>(batch, 0U)->code == 0U,
		"gaiji defaults to zero"
	);
	batch = advance(vm);
	expect_wait(
		batch, 0U, CutsceneWaitKind::input_change, 0U, 0U,
		"k defaults to its ignored zero parameter"
	);
	batch = advance(vm);
	expect_wait(
		batch, 0U, CutsceneWaitKind::input_change, 0U, 0U,
		"box wait defaults to zero"
	);
	batch = advance(vm);
	expect_wait(batch, 0U, CutsceneWaitKind::frames, 64U, 0U, "w defaults to 64");
	batch = advance(vm);
	expect_wait(batch, 0U, CutsceneWaitKind::input_ok, 64U, 0U, "wk defaults to 64");
	batch = advance(vm);
	expect_wait(
		batch, 0U, CutsceneWaitKind::measure, 64U, 64U,
		"wm defaults both measure parameters to 64"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneClearPicture>(batch, 0U),
		"immediate picture command defaults to the clearing quarter"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneCrossfadePicture>(batch, 0U) &&
			operation<CutsceneCrossfadePicture>(batch, 0U)->quarter == 4U &&
			operation<CutsceneCrossfadePicture>(batch, 0U)->frames_per_mask == 1U,
		"crossfade defaults to quarter four and one frame per mask"
	);
	batch = advance(vm);
	expect(
		operation<CutscenePlaySoundEffect>(batch, 0U) &&
			operation<CutscenePlaySoundEffect>(batch, 0U)->id == 1U,
		"effect inherits the final crossfade default"
	);
	(void)advance(vm);
}

void test_typing_cadence()
{
	for(std::uint16_t interval = 1U; interval <= 16U; ++interval) {
		const auto command = std::string("\\v") + std::to_string(interval);
		auto script = bytes(command);
		script.insert(script.end(), {0x82U, 0xA0U, 0x82U, 0xA2U, '\\', '$'});

		auto vm = make_started_vm(script);
		(void)advance(vm);
		auto batch = advance(vm);
		expect_wait(
			batch, 1U, CutsceneWaitKind::frames, interval, 0U,
			"unaccelerated glyph waits the configured interval"
		);

		vm = make_started_vm(script);
		(void)advance(vm);
		batch = advance(vm, {.any_key_held = true});
		const auto third = static_cast<std::uint16_t>(interval / 3U);
		if(third == 0U) {
			expect(
				batch.operations().size() == 1U,
				"first held-key glyph at interval one or two has no delay"
			);
		} else {
			expect_wait(
				batch, 1U, CutsceneWaitKind::frames, third, 0U,
				"held key uses the truncated one-third interval"
			);
		}
		batch = advance(vm, {.any_key_held = true});
		expect_wait(
			batch, 1U, CutsceneWaitKind::frames,
			(third == 0U) ? 1U : third, 0U,
			"held-key speedup parity delays the second glyph"
		);
		expect(vm.speedup_cycle() == 2U, "held glyphs advance speedup parity");

		vm = make_started_vm(script);
		(void)advance(vm);
		batch = advance(vm, {.cancel_held = true, .any_key_held = true});
		expect(
			batch.operations().size() == 1U && vm.fast_forward() &&
				vm.speedup_cycle() == 0U,
			"Cancel removes typing delay without changing speedup parity"
		);
	}
}

std::vector<std::uint8_t> glyph_script(std::size_t glyph_count)
{
	std::vector<std::uint8_t> script{};
	script.reserve((glyph_count * 2U) + 2U);
	for(std::size_t index = 0U; index < glyph_count; ++index) {
		script.push_back(0x82U);
		script.push_back(0xA0U);
	}
	script.push_back(static_cast<std::uint8_t>('\\'));
	script.push_back(static_cast<std::uint8_t>('$'));
	return script;
}

void test_cursor_transitions()
{
	auto script = glyph_script(108U);
	auto vm = make_started_vm(script);
	CutsceneScriptBatch batch{};
	for(std::size_t index = 0U; index < 29U; ++index) {
		batch = advance(vm);
	}
	expect(vm.cursor() == CutsceneCursor{544, 320}, "first row reaches final glyph cell");
	batch = advance(vm);
	expect(
		vm.cursor() == CutsceneCursor{144, 336} &&
			batch.operations().size() == 2U,
		"ordinary wrap advances subsequent lines to x=144"
	);
	for(std::size_t index = 30U; index < 107U; ++index) {
		batch = advance(vm);
	}
	batch = advance(vm);
	expect(
		batch.operations().size() == cutscene_batch_capacity &&
			operation<CutsceneDrawGlyph>(batch, 0U) &&
			vm.cursor() == CutsceneCursor{80, 320},
		"full box fits the fixed-capacity maximal glyph batch"
	);
	expect_wait(
		batch, 1U, CutsceneWaitKind::input_change, 0U, 0U,
		"automatic full box waits for input change"
	);
	expect(
		operation<CutsceneRestoreBox>(batch, 2U),
		"automatic full box restores its saved pixels"
	);
	expect_wait(
		batch, 3U, CutsceneWaitKind::frames, 1U, 0U,
		"automatic box transition retains typing delay"
	);

	auto newline_script = bytes("\\n\\n\\n\\n\\$");
	vm = make_started_vm(newline_script);
	for(std::size_t index = 0U; index < 3U; ++index) {
		batch = advance(vm);
		expect(batch.operations().empty(), "nonterminal explicit newline is silent");
	}
	batch = advance(vm);
	expect_wait(
		batch, 0U, CutsceneWaitKind::input_change, 0U, 0U,
		"fourth explicit newline falls through to an implicit box wait"
	);
	expect(operation<CutsceneRestoreBox>(batch, 1U), "newline restores full box");
}

void test_picture_resource_audio_gaiji_and_shake()
{
	auto script = bytes(
		"\\p,DM1A.PI\r\n\\p=\\p@\\pp\\p-\\=2\\=9\\==3,2\\$"
	);
	auto vm = make_started_vm(script);
	auto batch = advance(vm);
	const auto *load = operation<CutsceneLoadBackground>(batch, 0U);
	expect(
		load && std::string_view(load->filename.view()) == "DM1A.PI",
		"background filename consumes its control sentinel"
	);
	batch = advance(vm);
	expect(batch.operations().empty(), "remaining LF keeps its own iteration");
	batch = advance(vm);
	expect(
		operation<CutscenePresentBackground>(batch, 0U) &&
			operation<CutscenePresentBackground>(batch, 0U)->apply_palette,
		"p-equals presents with palette"
	);
	batch = advance(vm);
	expect(
		operation<CutscenePresentBackground>(batch, 0U) &&
			!operation<CutscenePresentBackground>(batch, 0U)->apply_palette,
		"p-at presents without palette"
	);
	batch = advance(vm);
	expect(operation<CutsceneApplyBackgroundPalette>(batch, 0U), "pp applies palette only");
	batch = advance(vm);
	expect(operation<CutsceneFreeBackground>(batch, 0U), "p-minus frees PI background");
	batch = advance(vm);
	expect(
		operation<CutsceneShowPictureQuarter>(batch, 0U) &&
			operation<CutsceneShowPictureQuarter>(batch, 0U)->quarter == 2U,
		"valid picture quarter is shown"
	);
	batch = advance(vm);
	expect(operation<CutsceneClearPicture>(batch, 0U), "out-of-range quarter clears picture");
	batch = advance(vm);
	expect(
		operation<CutsceneCrossfadePicture>(batch, 0U) &&
			operation<CutsceneCrossfadePicture>(batch, 0U)->quarter == 3U &&
			operation<CutsceneCrossfadePicture>(batch, 0U)->frames_per_mask == 2U &&
			operation<CutsceneCrossfadePicture>(batch, 0U)->animate_masks,
		"crossfade preserves quarter, per-mask cadence, and final copy semantics"
	);
	(void)advance(vm);

	script = bytes("\\==1,3\\$");
	vm = make_started_vm(script);
	batch = advance(vm, {.cancel_held = true, .any_key_held = true});
	expect(
		operation<CutsceneCrossfadePicture>(batch, 0U) &&
			!operation<CutsceneCrossfadePicture>(batch, 0U)->animate_masks,
		"Cancel keeps crossfade presentation but suppresses mask waits"
	);

	script = bytes(
		"\\m,DEMO1.M\r\\m$\\m*\\e5\\@\\ga31\\g2\\$"
	);
	vm = make_started_vm(script);
	batch = advance(vm);
	const auto *bgm = operation<CutsceneBgm>(batch, 0U);
	expect(
		bgm && bgm->action == CutsceneBgmAction::load_and_play &&
			std::string_view(bgm->filename.view()) == "DEMO1.M",
		"TH03 song load retains stop-before-load semantics"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneBgm>(batch, 0U) &&
			operation<CutsceneBgm>(batch, 0U)->action == CutsceneBgmAction::stop,
		"song stop command"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneBgm>(batch, 0U) &&
			operation<CutsceneBgm>(batch, 0U)->action == CutsceneBgmAction::play,
		"song play command"
	);
	batch = advance(vm);
	expect(
		operation<CutscenePlaySoundEffect>(batch, 0U) &&
			operation<CutscenePlaySoundEffect>(batch, 0U)->id == 5U,
		"sound effect command"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneClearPages>(batch, 0U) &&
			!operation<CutsceneClearPages>(batch, 0U)->snapshot_box_after_clear,
		"clear command preserves the missing TH03 box resnapshot"
	);
	batch = advance(vm);
	const auto *gaiji = operation<CutsceneDrawGaiji>(batch, 0U);
	expect(
		gaiji && gaiji->code == 31U && gaiji->position == CutsceneCursor{80, 320} &&
			gaiji->color == 15U && batch.operations().size() == 1U &&
			vm.cursor() == CutsceneCursor{96, 320},
		"gaiji advances without a typing delay"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneShake>(batch, 0U) &&
			operation<CutsceneShake>(batch, 0U)->parameter == 2U &&
			operation<CutsceneShake>(batch, 0U)->delay_each_position,
		"shake preserves its inclusive source loop contract"
	);

	const auto long_name = bytes("\\p,ABCDEFGHIJKLXY\\$");
	vm = make_started_vm(long_name);
	batch = advance(vm);
	load = operation<CutsceneLoadBackground>(batch, 0U);
	expect(
		load && load->filename.length == 12U &&
			std::string_view(load->filename.view()) == "ABCDEFGHIJKL",
		"filename parser stops at PF_FN_LEN minus terminator"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneDrawGlyph>(batch, 0U) &&
			operation<CutsceneDrawGlyph>(batch, 0U)->shift_jis ==
				std::array<std::uint8_t, 2U>{'X', 'Y'},
		"twelve-byte filename leaves the next source byte unconsumed"
	);
}

void test_every_default_path_and_unconsumed_subcommands()
{
	auto script = bytes(
		"\\c\\b\\wi\\wo\\w\\wk\\wm\\wmk\\v\\vp\\t"
		"\\fi\\fo\\fm\\ga\\g\\k\\s\\=\\==,\\e\\$"
	);
	auto vm = make_started_vm(script);
	auto batch = advance(vm);
	expect(
		operation<CutsceneSetTextColor>(batch, 0U) &&
			operation<CutsceneSetTextColor>(batch, 0U)->color == 15U,
		"color default"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneSetTextWeight>(batch, 0U) &&
			operation<CutsceneSetTextWeight>(batch, 0U)->weight ==
				GraphicsTextWeight::bold,
		"weight default"
	);
	batch = advance(vm);
	expect(
		operation<CutscenePaletteFade>(batch, 0U) &&
			operation<CutscenePaletteFade>(batch, 0U)->speed == 1U &&
			operation<CutscenePaletteFade>(batch, 0U)->color ==
				CutsceneFadeColor::white &&
			operation<CutscenePaletteFade>(batch, 0U)->direction ==
				CutsceneFadeDirection::in,
		"white-in default"
	);
	batch = advance(vm);
	expect(
		operation<CutscenePaletteFade>(batch, 0U) &&
			operation<CutscenePaletteFade>(batch, 0U)->speed == 1U &&
			operation<CutscenePaletteFade>(batch, 0U)->direction ==
				CutsceneFadeDirection::out,
		"white-out default"
	);
	batch = advance(vm);
	expect_wait(batch, 0U, CutsceneWaitKind::frames, 64U, 0U,
		"plain wait default");
	batch = advance(vm);
	expect_wait(batch, 0U, CutsceneWaitKind::input_ok, 64U, 0U,
		"interruptible wait default");
	batch = advance(vm);
	expect_wait(batch, 0U, CutsceneWaitKind::measure, 64U, 64U,
		"measure defaults");
	batch = advance(vm);
	expect_wait(batch, 0U, CutsceneWaitKind::input_ok_or_measure, 64U, 64U,
		"interruptible measure defaults");
	batch = advance(vm);
	expect(
		operation<CutsceneSetTextInterval>(batch, 0U) &&
			operation<CutsceneSetTextInterval>(batch, 0U)->frames == 1U,
		"text interval default"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneShowPage>(batch, 0U) &&
			operation<CutsceneShowPage>(batch, 0U)->page == 0U,
		"shown-page default"
	);
	batch = advance(vm);
	expect_wait(batch, 0U, CutsceneWaitKind::frames, 1U, 0U,
		"tone default pre-delay");
	expect(
		operation<CutsceneSetTone>(batch, 1U) &&
			operation<CutsceneSetTone>(batch, 1U)->tone == 100U,
		"tone default"
	);
	batch = advance(vm);
	expect(
		operation<CutscenePaletteFade>(batch, 0U) &&
			operation<CutscenePaletteFade>(batch, 0U)->color ==
				CutsceneFadeColor::black &&
			operation<CutscenePaletteFade>(batch, 0U)->direction ==
				CutsceneFadeDirection::in &&
			operation<CutscenePaletteFade>(batch, 0U)->speed == 1U,
		"black-in default"
	);
	batch = advance(vm);
	expect(
		operation<CutscenePaletteFade>(batch, 0U) &&
			operation<CutscenePaletteFade>(batch, 0U)->direction ==
				CutsceneFadeDirection::out &&
			operation<CutscenePaletteFade>(batch, 0U)->speed == 1U,
		"black-out default"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneAudioFade>(batch, 0U) &&
			operation<CutsceneAudioFade>(batch, 0U)->parameter == 1U,
		"audio-fade default"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneDrawGaiji>(batch, 0U) &&
			operation<CutsceneDrawGaiji>(batch, 0U)->code == 0U,
		"gaiji default"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneShake>(batch, 0U) &&
			operation<CutsceneShake>(batch, 0U)->parameter == 8U,
		"shake default"
	);
	batch = advance(vm);
	expect_wait(batch, 0U, CutsceneWaitKind::input_change, 0U, 0U,
		"mid-box wait default and ignored parameter");
	batch = advance(vm);
	expect_wait(batch, 0U, CutsceneWaitKind::input_change, 0U, 0U,
		"new-box default");
	expect(operation<CutsceneRestoreBox>(batch, 1U), "new-box default restores");
	batch = advance(vm);
	expect(operation<CutsceneClearPicture>(batch, 0U),
		"quarter default selects clear branch");
	batch = advance(vm);
	expect(
		operation<CutsceneCrossfadePicture>(batch, 0U) &&
			operation<CutsceneCrossfadePicture>(batch, 0U)->quarter == 4U &&
			operation<CutsceneCrossfadePicture>(batch, 0U)->frames_per_mask == 1U,
		"crossfade defaults"
	);
	batch = advance(vm);
	expect(
		operation<CutscenePlaySoundEffect>(batch, 0U) &&
			operation<CutscenePlaySoundEffect>(batch, 0U)->id == 1U,
		"effect inherits crossfade second-parameter default"
	);
	(void)advance(vm);

	script = bytes("\\fI \\gA \\pAB\\mC \\$");
	vm = make_started_vm(script);
	batch = advance(vm);
	expect(batch.operations().empty(), "uppercase black-fade subcommand is unconsumed");
	batch = advance(vm);
	expect(
		operation<CutsceneDrawGlyph>(batch, 0U) &&
			operation<CutsceneDrawGlyph>(batch, 0U)->shift_jis ==
				std::array<std::uint8_t, 2U>{'I', ' '},
		"unconsumed black-fade subcommand becomes the next glyph"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneShake>(batch, 0U) &&
			operation<CutsceneShake>(batch, 0U)->parameter == 8U,
		"uppercase ga subcommand selects shake"
	);
	batch = advance(vm);
	expect(
		operation<CutsceneDrawGlyph>(batch, 0U) &&
			operation<CutsceneDrawGlyph>(batch, 0U)->shift_jis ==
				std::array<std::uint8_t, 2U>{'A', ' '},
		"unconsumed ga byte becomes the next glyph"
	);
	batch = advance(vm);
	expect(batch.operations().empty(), "unknown picture subcommand is unconsumed");
	batch = advance(vm);
	expect(
		operation<CutsceneDrawGlyph>(batch, 0U) &&
			operation<CutsceneDrawGlyph>(batch, 0U)->shift_jis ==
				std::array<std::uint8_t, 2U>{'A', 'B'},
		"unconsumed picture subcommand becomes the next glyph"
	);
	batch = advance(vm);
	expect(batch.operations().empty(), "unknown BGM subcommand is unconsumed");
	batch = advance(vm);
	expect(
		operation<CutsceneDrawGlyph>(batch, 0U) &&
			operation<CutsceneDrawGlyph>(batch, 0U)->shift_jis ==
				std::array<std::uint8_t, 2U>{'C', ' '},
		"unconsumed BGM subcommand becomes the next glyph"
	);
}

void test_malformed_atomicity_unknown_and_boundary_default()
{
	struct Case {
		std::vector<std::uint8_t> script;
		std::size_t error_offset;
	};
	const std::array cases{
		Case{{'\\'}, 1U},
		Case{{0x82U}, 1U},
		Case{{'\\', 'p'}, 2U},
		Case{{'\\', 'p', ',', 'A'}, 4U},
		Case{{'\\', 'w', 'm', '4', ','}, 5U},
	};
	for(const auto &test : cases) {
		auto vm = make_started_vm(test.script);
		auto result = vm.step({.cancel_held = true, .any_key_held = true});
		expect(
			!result && result.error().code ==
				CutsceneScriptErrorCode::unexpected_end &&
				result.error().byte_offset == test.error_offset,
			"truncated input reports its bounded source offset"
		);
		expect(
			vm.byte_offset() == 0U && vm.number_default() == 0U &&
				!vm.fast_forward() && !vm.finished(),
			"malformed iteration rolls all VM state back atomically"
		);
	}

	auto script = bytes("\\z\\$");
	auto vm = make_started_vm(script);
	auto batch = advance(vm);
	expect(batch.operations().empty(), "unknown opcode is ignored like source dispatcher");
	batch = advance(vm);
	expect(operation<CutsceneFinish>(batch, 2U), "unknown opcode does not hide terminator");

	script = bytes("\\fm\\e\\$");
	vm = make_started_vm(script);
	batch = advance(vm);
	expect(
		operation<CutsceneAudioFade>(batch, 0U) &&
			operation<CutsceneAudioFade>(batch, 0U)->parameter == 1U,
		"argument-less fade is valid immediately before another opcode"
	);
	batch = advance(vm);
	expect(
		operation<CutscenePlaySoundEffect>(batch, 0U) &&
			operation<CutscenePlaySoundEffect>(batch, 0U)->id == 1U,
		"argument-less effect is safe immediately before the final opcode"
	);

	script = bytes(" \n");
	vm = make_started_vm(script);
	(void)advance(vm);
	(void)advance(vm);
	auto missing_end = vm.step({});
	expect(
		!missing_end && missing_end.error().code ==
			CutsceneScriptErrorCode::unexpected_end &&
			missing_end.error().byte_offset == 2U && vm.byte_offset() == 2U,
		"missing terminator becomes a bounded error at end of input"
	);
}

void test_text_run_extraction()
{
	const std::vector<std::uint8_t> script{
		0x82U, 0xA0U, 0x82U, 0xA2U,
		static_cast<std::uint8_t>('\\'), static_cast<std::uint8_t>('n'),
		0x82U, 0xA4U,
		static_cast<std::uint8_t>('\\'), static_cast<std::uint8_t>('g'),
		static_cast<std::uint8_t>('a'), static_cast<std::uint8_t>('3'),
		0x82U, 0xA6U,
		static_cast<std::uint8_t>('\\'), static_cast<std::uint8_t>('$'),
	};
	auto runs = extract_cutscene_text_runs(script);
	expect(runs.has_value(), "text-run extraction accepts a valid script");
	if(runs) {
		expect(
			runs->size() == 3U && (*runs)[0U].ordinal == 0U &&
				(*runs)[0U].source_offset == 0U &&
				(*runs)[0U].first_position == CutsceneCursor{80, 320} &&
				(*runs)[0U].last_position == CutsceneCursor{96, 320} &&
				(*runs)[0U].shift_jis ==
					std::vector<std::uint8_t>{0x82U, 0xA0U, 0x82U, 0xA2U},
			"text-run extraction groups adjacent source glyph iterations"
		);
		expect(
			(*runs)[1U].ordinal == 1U &&
				(*runs)[1U].source_offset == 6U &&
				(*runs)[1U].first_position == CutsceneCursor{80, 336} &&
				(*runs)[1U].shift_jis ==
					std::vector<std::uint8_t>{0x82U, 0xA4U} &&
				(*runs)[2U].source_offset == 12U &&
				(*runs)[2U].first_position == CutsceneCursor{112, 336},
			"commands and gaiji terminate source text runs without moving text"
		);
	}

	auto wrapped_script = glyph_script(109U);
	runs = extract_cutscene_text_runs(wrapped_script);
	expect(
		runs && runs->size() == 2U &&
			(*runs)[0U].shift_jis.size() == 216U &&
			(*runs)[1U].shift_jis.size() == 2U &&
			(*runs)[1U].first_position == CutsceneCursor{80, 320},
		"automatic full-box restoration terminates an otherwise adjacent run"
	);

	const std::array<std::uint8_t, 1U> malformed{0x82U};
	auto rejected = extract_cutscene_text_runs(malformed);
	expect(
		!rejected && rejected.error().code ==
			CutsceneScriptErrorCode::unexpected_end &&
			rejected.error().byte_offset == 1U,
		"text-run extraction preserves canonical VM parse errors"
	);
}

void test_local_original_scripts(const std::filesystem::path &directory)
{
	constexpr std::array names{
		"@00DM0.TXT", "@00DM1.TXT", "@00ED.TXT",
		"@01DM0.TXT", "@01DM1.TXT", "@01ED.TXT",
		"@02DM0.TXT", "@02DM1.TXT", "@02ED.TXT",
		"@03DM0.TXT", "@03DM1.TXT", "@03ED.TXT",
		"@04DM0.TXT", "@04DM1.TXT", "@04ED.TXT",
		"@05DM0.TXT", "@05DM1.TXT", "@05ED.TXT",
		"@06DM0.TXT", "@06DM1.TXT", "@06ED.TXT",
		"@07DM0.TXT", "@07DM1.TXT", "@07ED.TXT",
		"@08DM0.TXT", "@08DM1.TXT", "@08ED.TXT",
		"@99ED.TXT",
	};
	for(const auto name : names) {
		std::error_code size_error{};
		const auto source_size = std::filesystem::file_size(
			directory / name,
			size_error
		);
		expect(!size_error, "local script size is readable");
		if(size_error || source_size == 0U ||
			source_size > static_cast<std::uintmax_t>(
				std::numeric_limits<std::streamsize>::max()
			)) {
			expect(false, "local script size is supported");
			continue;
		}
		std::vector<std::uint8_t> script(static_cast<std::size_t>(source_size));
		std::ifstream input(directory / name, std::ios::binary);
		expect(input.is_open(), "local script opens");
		if(!input) {
			continue;
		}
		input.read(
			reinterpret_cast<char *>(script.data()),
			static_cast<std::streamsize>(script.size())
		);
		expect(
			input.gcount() == static_cast<std::streamsize>(script.size()),
			"local script reads completely"
		);
		if(input.gcount() != static_cast<std::streamsize>(script.size())) {
			continue;
		}

		auto created = CutsceneScriptVm::create(script);
		expect(created.has_value(), "local script VM constructs");
		if(!created) {
			continue;
		}
		auto vm = std::move(*created);
		auto begun = vm.begin();
		expect(begun.has_value(), "local script VM begins");
		if(!begun) {
			continue;
		}
		for(std::size_t iteration = 0U;
			iteration <= script.size() && !vm.finished(); ++iteration) {
			auto stepped = vm.step({.cancel_held = true, .any_key_held = true});
			expect(stepped.has_value(), "local script iteration is valid");
			if(!stepped) {
				break;
			}
		}
		expect(vm.finished(), "local script reaches its source terminator");
		auto runs = extract_cutscene_text_runs(script);
		expect(
			runs && !runs->empty(),
			"local script exposes at least one canonical text run"
		);
	}
}

} // namespace

int main(int argc, char **argv)
{
	test_lifecycle_ownership_separators_and_glyph();
	test_state_palette_and_persistent_default();
	test_waits_fast_forward_and_box_commands();
	test_opcode_defaults();
	test_typing_cadence();
	test_cursor_transitions();
	test_picture_resource_audio_gaiji_and_shake();
	test_every_default_path_and_unconsumed_subcommands();
	test_malformed_atomicity_unknown_and_boundary_default();
	test_text_run_extraction();
	if(argc == 2) {
		test_local_original_scripts(std::filesystem::path(argv[1]));
	} else {
		expect(argc == 1, "optional local gate accepts one directory argument");
	}

	if(failures != 0U) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << "cutscene script checks: " << checks << '\n';
	return 0;
}
