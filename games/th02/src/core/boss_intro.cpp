#include "th02/core/boss_intro.hpp"
#include "th02/core/stage_state.hpp"
#include <array>
#include <bit>

namespace th02::core {
namespace {
constexpr std::array<std::uint8_t, 35> faces{
	57, 96, 96, 57, 96, 57, 96, 57,
	105, 6, 96, 12, 6, 6, 105, 105, 12, 96, 6, 6, 105, 105, 105, 105,
	57, 144, 6, 144, 144, 144, 6, 144, 96, 0, 96};
constexpr std::array<std::uint8_t, 22> continued_faces{
	105, 57, 105, 6, 6, 105, 105, 6, 105, 6, 6, 96, 6, 105, 57, 105, 0, 96, 6, 6, 105, 105};
constexpr std::array<std::uint8_t, 17> not_continued_faces{
	105, 0, 105, 6, 105, 105, 105, 105, 105, 6, 105, 105, 105, 6, 6, 105, 105};

void begin_box(StageState &s, BossIntroState &i, BossIntroPresentation &p) noexcept
{
	i.phase = BossIntroPhase::boxes;
	auto face = std::uint8_t{105};
	if (i.kind == BossIntroKind::marisa) face = faces[i.sequence_index];
	else if (i.sequence_index < 4) face = (i.sequence_index & 1) == 0 ? 6 : 105;
	else if (s.continues_used && i.sequence_index < 26) face = continued_faces[i.sequence_index - 4];
	else if (!s.continues_used && i.sequence_index < 21) face = not_continued_faces[i.sequence_index - 4];
	dialog_box_begin(s.dialog, i.box, face, p);
}

Stage1IntroWait advance(StageState &s, BossIntroState &i, BossIntroPresentation &p) noexcept
{
	if (i.phase == BossIntroPhase::pre_slide && i.slide.phase == DialogSlidePhase::complete) {
		p.scene(BossIntroScene::load_portrait, 0, s);
		begin_box(s, i, p);
	} else if (i.phase == BossIntroPhase::boxes && i.box.phase == DialogBoxPhase::complete) {
		++i.sequence_index;
		if (i.kind == BossIntroKind::mima) {
			if (i.sequence_index == 2) {
				i.remaining = 10; i.phase = BossIntroPhase::entrance_wait;
			} else if (i.sequence_index == (s.continues_used ? 26 : 21)) {
				if (s.continues_used) s.dialog.box_current += 17;
				palette_white_fade_begin(i.fade, PaletteWhiteFadeDirection::out, 1, s.bomb_animation.palette_tone);
				i.phase = BossIntroPhase::white_out;
			} else if (i.sequence_index == (s.continues_used ? 27 : 22)) {
				(void)dialog_post_begin(s, i.slide, p); i.phase = BossIntroPhase::post_slide;
			} else {
				if (i.sequence_index == 4 && !s.continues_used) s.dialog.box_current += 22;
				begin_box(s, i, p);
			}
		} else if (i.sequence_index == 8) {
			i.remaining = 10; i.phase = BossIntroPhase::entrance_wait;
		} else if (i.sequence_index == 24) {
			p.scene(BossIntroScene::load_marisa, 0, s);
			s.boss.topleft_on_page.fill({176, 64}); s.marisa.topleft = {176, 64}; s.boss.patnum = 128;
			palette_white_fade_begin(i.fade, PaletteWhiteFadeDirection::out, 1, s.bomb_animation.palette_tone);
			i.phase = BossIntroPhase::white_out;
		} else if (i.sequence_index == faces.size()) {
			p.box_wipe(); p.scene(BossIntroScene::clear_page, -1, s);
			p.scene(BossIntroScene::marisa_and_player, 0, s);
			p.scene(BossIntroScene::copy_and_show, 0, s);
			s.tile_mode = 2;
			shots_free_all(s.player_shots);
			marisa_combat_reset(s.boss, s.marisa, s.rank);
			s.background_particles.color = 9;
			s.background_particles.edge_step = 24;
			s.background_particles.speed_initial = 24;
			s.background_particles.speed_delta = 4;
			i.phase = BossIntroPhase::complete;
		} else begin_box(s, i, p);
	} else if (i.phase == BossIntroPhase::white_out && i.fade.phase == PaletteWhiteFadePhase::complete) {
		p.scene(BossIntroScene::clear_page, -1, s);
		p.scene(i.kind == BossIntroKind::mima ? BossIntroScene::mima_and_player : BossIntroScene::marisa_and_player, 0, s);
		p.sound_effect(10); p.sound_effects_update(); p.scene(BossIntroScene::background_rows, 0, s);
		palette_white_fade_begin(i.fade, PaletteWhiteFadeDirection::in, i.kind == BossIntroKind::mima ? 3 : 1, s.bomb_animation.palette_tone);
		i.phase = BossIntroPhase::white_in;
	} else if (i.phase == BossIntroPhase::white_in && i.fade.phase == PaletteWhiteFadePhase::complete) {
		p.music_stop(); p.music_load(i.kind == BossIntroKind::mima ? "mima.m" : "boss3.m"); p.music_play(); begin_box(s, i, p);
	} else if (i.phase == BossIntroPhase::post_slide && i.slide.phase == DialogSlidePhase::complete) {
		mima_combat_reset(s);
		p.page_access(s.frame.page_front); p.scene(BossIntroScene::clear_page, -1, s);
		p.page_access(s.frame.page_back); p.scene(BossIntroScene::clear_page, -1, s);
		p.scene(BossIntroScene::mima_side_columns, 11, s);
		i.phase = BossIntroPhase::complete;
	}
	if (i.slide.phase == DialogSlidePhase::score_refused || i.box.phase == DialogBoxPhase::invalid_text_reference)
		i.phase = BossIntroPhase::refused;
	return boss_intro_wait(i);
}
} // namespace

Stage1IntroWait boss_intro_wait(const BossIntroState &i) noexcept
{
	if (i.phase == BossIntroPhase::idle) return Stage1IntroWait::idle;
	if (i.phase == BossIntroPhase::complete) return Stage1IntroWait::complete;
	if (i.phase == BossIntroPhase::refused) return Stage1IntroWait::refused;
	if (i.phase != BossIntroPhase::boxes) return Stage1IntroWait::delay_one_frame;
	Stage1IntroState proxy; proxy.phase = Stage1IntroPhase::boxes; proxy.box = i.box;
	return stage1_intro_wait(proxy);
}

Stage1IntroWait boss_intro_begin(StageState &s, BossIntroState &i, BossIntroPresentation &p, BossIntroKind kind) noexcept
{
	i = {};
	i.kind = kind;
	if (s.frame.page_front >= 2 || s.frame.page_back >= 2) {
		i.phase = BossIntroPhase::refused; return Stage1IntroWait::refused;
	}
	s.frame.player.on_page[s.frame.page_front].y = s.frame.player.current.y;
	s.scroll.line = 0; s.scroll.sad = 0;
	p.page_access(0); p.scene(BossIntroScene::clear_page, 0, s);
	p.page_access(1); p.scene(BossIntroScene::clear_page, 1, s);
	i.phase = BossIntroPhase::reset_wait;
	return Stage1IntroWait::delay_one_frame;
}

Stage1IntroWait boss_intro_after_delay(StageState &s, BossIntroState &i, BossIntroPresentation &p) noexcept
{
	if (i.phase == BossIntroPhase::reset_wait) {
		p.graph_scrollup(0); p.page_access(s.frame.page_front);
		const ScreenPoint option{
			std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(s.frame.player.current.x - 16)),
			std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(s.frame.player.current.y + 16))};
		s.frame.option_left_on_page[s.frame.page_back] = option;
		s.frame.option_left_on_page[s.frame.page_front] = option;
		p.player_put(s);
		(void)dialog_pre_begin(s, s.dialog, i.slide, p); i.phase = BossIntroPhase::pre_slide;
	} else if (i.phase == BossIntroPhase::pre_slide || i.phase == BossIntroPhase::post_slide) {
		(void)dialog_slide_after_delay(s, s.dialog, i.slide, p);
	} else if (i.phase == BossIntroPhase::boxes) {
		(void)dialog_box_after_delay(i.box);
	} else if (i.phase == BossIntroPhase::entrance_wait) {
		if (--i.remaining == 0) {
			i.phase = BossIntroPhase::rings; i.radius = 150;
			p.sound_effect(9); p.scene(BossIntroScene::ring_put, i.radius, s);
		}
	} else if (i.phase == BossIntroPhase::rings) {
		p.scene(BossIntroScene::ring_erase, i.radius, s);
		s.bomb_animation.palette_tone = static_cast<std::uint16_t>(((150 - i.radius) / 3) * 2 + 100);
		p.palette_show(); p.sound_effects_update();
		if (--i.radius != 0) p.scene(BossIntroScene::ring_put, i.radius, s);
		else {
			p.scene(BossIntroScene::clear_page, -1, s);
			p.scene(BossIntroScene::portrait_and_player, 0, s); p.scene(BossIntroScene::background_rows, 0, s);
			i.phase = BossIntroPhase::portrait_fade; i.remaining = 30;
			s.bomb_animation.palette_tone = 200; p.palette_show();
		}
	} else if (i.phase == BossIntroPhase::portrait_fade) {
		if (--i.remaining == 0) {
			s.bomb_animation.palette_tone = 100; p.palette_show();
			if (i.kind == BossIntroKind::mima) {
				p.scene(BossIntroScene::load_mima, 0, s);
				s.boss.topleft_on_page.fill({144, 64}); s.boss.patnum = 128;
			}
			begin_box(s, i, p);
		} else if (i.remaining % 3 == 0) {
			s.bomb_animation.palette_tone = static_cast<std::uint16_t>(200 - ((30 - i.remaining) / 3) * 10);
			p.palette_show();
		}
	} else if (i.phase == BossIntroPhase::white_out || i.phase == BossIntroPhase::white_in) {
		palette_white_fade_after_vsync(i.fade, s.bomb_animation.palette_tone, p);
	}
	return advance(s, i, p);
}
Stage1IntroWait boss_intro_input(StageState &s, BossIntroState &i,
	std::uint16_t input, BossIntroPresentation &p) noexcept
{
	if (i.phase == BossIntroPhase::boxes && i.box.phase == DialogBoxPhase::input_sample) {
		s.input = input; (void)dialog_box_input(s.dialog, i.box, input, p);
	}
	return advance(s, i, p);
}
Stage1IntroWait boss_intro_acknowledgement(StageState &s, BossIntroState &i,
	bool pressed, BossIntroPresentation &p) noexcept
{
	if (i.phase == BossIntroPhase::boxes) (void)dialog_box_acknowledgement(s.dialog, i.box, pressed);
	return advance(s, i, p);
}
} // namespace th02::core
