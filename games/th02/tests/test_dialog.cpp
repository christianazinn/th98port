#include "th02/core/dialog.hpp"
#include "th02/core/stage1_state.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <vector>

namespace {
using namespace th02::core;
void require(bool condition, std::source_location at = std::source_location::current()) {
	if (!condition) { std::fprintf(stderr, "dialog assertion failed: %u\n", at.line()); std::abort(); }
}
struct Text { std::uint8_t line; std::size_t count; };
struct Sink final : DialogBoxPresentation {
	unsigned wipes{}, faces{};
	std::int16_t face{};
	std::vector<Text> text;
	void box_wipe() noexcept override { ++wipes; }
	void face_put(std::int16_t value) noexcept override { ++faces; face = value; }
	void text_put(std::uint8_t line, std::span<const std::uint8_t> prefix) noexcept override {
		text.push_back({line, prefix.size()});
	}
};

DialogBoxPhase pair(DialogState &state, DialogBoxState &box, bool first, bool second)
{
	require(dialog_box_acknowledgement(state, box, first) == DialogBoxPhase::acknowledgement_delay_two_frames);
	// A second poll cannot be accepted before its required two-frame delay.
	require(dialog_box_acknowledgement(state, box, !first) == DialogBoxPhase::acknowledgement_delay_two_frames);
	require(dialog_box_after_delay(box) == DialogBoxPhase::acknowledgement_second_sample);
	return dialog_box_acknowledgement(state, box, second);
}

void typing(bool fast)
{
	DialogState state{};
	DialogBoxState box{};
	Sink sink{};
	dialog_box_begin(state, box, 99, sink);
	unsigned samples = 0, delays = 0;
	while (box.phase == DialogBoxPhase::input_sample) {
		require(++samples < 200);
		const auto before_cursor = box.cursor;
		const auto phase = dialog_box_input(state, box, fast ? 0x40U : 0U, sink);
		if (!fast) {
			require(phase == DialogBoxPhase::delay_one_frame && box.cursor == before_cursor);
			++delays;
			(void)dialog_box_after_delay(box);
		}
	}
	require(box.phase == DialogBoxPhase::acknowledgement_first_sample && box.cursor == 82);
	require(samples == (fast ? 38U : 114U) && delays == (fast ? 0U : 114U));
	require(sink.faces == samples && sink.wipes == 1 && sink.face == 99);
	require(sink.text.size() == (fast ? 34U : 102U));
	require(sink.text.front().line == 0 && sink.text.front().count == 6);
	require(sink.text.back().line == 1 && sink.text.back().count == 36);
	require(state.box_current == 0);
	require(pair(state, box, true, false) == DialogBoxPhase::acknowledgement_first_sample);
	require(!box.acknowledgement_released);
	require(pair(state, box, false, false) == DialogBoxPhase::acknowledgement_first_sample);
	require(box.acknowledgement_released);
	require(pair(state, box, false, false) == DialogBoxPhase::acknowledgement_first_sample);
	require(pair(state, box, false, true) == DialogBoxPhase::complete && state.box_current == 1);
	require(dialog_box_acknowledgement(state, box, true) == DialogBoxPhase::complete && state.box_current == 1);
}

void mixed_input_and_loader()
{
	DialogState state{};
	state.text.fill(0xA5);
	state.box_current = 7;
	const std::array<std::uint8_t, 3> bytes{1, 2, 3};
	require(dialog_load_and_init(state, bytes));
	require(state.text[2] == 3 && state.text[3] == 0xA5 && state.box_current == 0);
	std::array<std::uint8_t, 5121> too_large{};
	state.box_current = 7;
	require(!dialog_load_and_init(state, too_large) && state.box_current == 7 && state.text[2] == 3);
	state.box_current = 0;
	DialogBoxState box{};
	Sink sink{};
	dialog_box_begin(state, box, 0, sink);
	require(dialog_box_input(state, box, 1, sink) == DialogBoxPhase::input_sample && box.cursor == 8);
	require(dialog_box_input(state, box, 0, sink) == DialogBoxPhase::delay_one_frame);
	require(dialog_box_after_delay(box) == DialogBoxPhase::input_sample && box.cursor == 8 && box.loop_count == 2);
	require(dialog_box_input(state, box, 0, sink) == DialogBoxPhase::delay_one_frame);
	require(dialog_box_after_delay(box) == DialogBoxPhase::input_sample && box.cursor == 10 && box.loop_count == 3);
	state.box_current = 64;
	dialog_box_begin(state, box, 4, sink);
	const auto prior_faces = sink.faces;
	require(dialog_box_input(state, box, 1, sink) == DialogBoxPhase::invalid_text_reference);
	require(sink.faces == prior_faces + 1 && box.loop_count == 0 && state.box_current == 64);
}

void slides()
{
	struct SlideSink final : DialogSlidePresentation {
		unsigned wipes{}, scores{}, tiles{}, boxes{}, scrolls{};
		std::vector<std::size_t> pages;
		std::int16_t last_left{};
		void overlay_wipe() noexcept override { ++wipes; }
		void score_events(const ScoreEvents &) noexcept override { ++scores; }
		void graph_scrollup(std::int16_t) noexcept override { ++scrolls; }
		void palette_show() noexcept override {}
		void page_access(std::size_t page) noexcept override { pages.push_back(page); }
		void egc_begin() noexcept override {}
		void box_area_invalidate() noexcept override {}
		void tiles_render(const StageState &) noexcept override { ++tiles; }
		void egc_end() noexcept override {}
		void player_put(const StageState &) noexcept override {}
		void box_put(std::int16_t left, std::int16_t top, std::int16_t bottom) noexcept override {
			require(top == 319 && bottom == 351);
			last_left = left;
			++boxes;
		}
	} sink;
	StageState state{};
	DialogState dialog{};
	DialogSlideState slide{};
	state.scroll.line = 399;
	state.score.score_delta = 100;
	state.tile_mode = 2;
	state.tiles_egc_render_all = true;
	state.frame.page_front = 1;
	state.frame.page_back = 0;
	require(dialog_pre_begin(state, dialog, slide, sink) == DialogSlidePhase::delay_one_frame);
	require(slide.left == 384 && sink.boxes == 0 && sink.wipes == 2 && sink.scores == 1);
	require(state.score.score == 100 && state.score.score_delta == 100);
	require(state.tile_mode == 0 && dialog.restore_tile_mode_none_at_post);
	unsigned frames = 0;
	while (slide.phase == DialogSlidePhase::delay_one_frame) {
		require(++frames < 30);
		(void)dialog_slide_after_delay(state, dialog, slide, sink);
	}
	require(frames == 23 && sink.boxes == 23 && sink.last_left == 32 && !state.tiles_egc_render_all);
	require(dialog_post_begin(state, slide, sink) == DialogSlidePhase::delay_one_frame && slide.left == 16);
	frames = 0;
	while (slide.phase == DialogSlidePhase::delay_one_frame) {
		require(++frames < 30);
		(void)dialog_slide_after_delay(state, dialog, slide, sink);
	}
	require(frames == 24 && sink.boxes == 47 && sink.last_left == -352);
	require(state.tile_mode == 2 && !dialog.restore_tile_mode_none_at_post);
	require(sink.pages == std::vector<std::size_t>({1, 0}) && state.stage_frame == 0 && state.resident_frame == 0);
	require(dialog_slide_after_delay(state, dialog, slide, sink) == DialogSlidePhase::complete && sink.boxes == 47);
	state.score.extends_gained = 65535;
	const auto wipes = sink.wipes;
	const auto pages = sink.pages.size();
	require(dialog_pre_begin(state, dialog, slide, sink) == DialogSlidePhase::score_refused);
	require(sink.wipes == wipes + 1 && sink.pages.size() == pages && state.tile_mode == 2);
}
} // namespace

int main() { typing(true); typing(false); mixed_input_and_loader(); slides(); }
