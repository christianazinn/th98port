#include "th02/core/midboss2.hpp"

#include <bit>
#include <cstdint>

namespace th02::core {
namespace {

constexpr std::int16_t playfield_top = 16;
constexpr std::int16_t playfield_left = 32;
constexpr std::int16_t playfield_width = 384;
constexpr std::int16_t screen_height = 400;
constexpr std::int16_t midboss_width = 64;
constexpr std::int16_t midboss_height = 64;
constexpr std::int16_t midboss_enter_left =
	playfield_left + (playfield_width / 2) - 32;
constexpr std::int16_t midboss_enter_top = playfield_top - 32;
constexpr std::int16_t midboss_patnum = 137;
constexpr std::int16_t midboss_patnum_hit = 136;
constexpr std::int16_t midboss_cels = 4;
constexpr std::int16_t midboss_entry_frames = 320;
constexpr std::int16_t midboss_damage_max = 380;
constexpr std::int16_t midboss_defeat_vram_y_max = 304;
constexpr std::int16_t midboss_leave_frame = 1600;
constexpr std::int16_t midboss_defeat_frames = 64;
constexpr std::int16_t midboss_defeat_spark_until = 48;
constexpr std::int16_t midboss_defeat_spark_count = 24;
constexpr std::int16_t midboss_defeat_zoom_patnum = 10;
constexpr std::int16_t midboss_bullet_patnum_noroi = 84;
constexpr std::int16_t midboss_bullet_speed = 30;

[[nodiscard]] std::int16_t word_from_i32(std::int32_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

[[nodiscard]] std::int16_t word_add(std::int16_t lhs,
	std::int16_t rhs) noexcept
{
	return word_from_i32(static_cast<std::int32_t>(lhs) + rhs);
}

[[nodiscard]] std::int32_t dword_add(std::int32_t lhs,
	std::int32_t rhs) noexcept
{
	return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(
		static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(lhs)) +
		static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(rhs))
	));
}

[[nodiscard]] std::int16_t arithmetic_shift_right(std::int16_t value,
	std::uint8_t count) noexcept
{
	auto bits = std::bit_cast<std::uint16_t>(value);
	bits = static_cast<std::uint16_t>(bits >> count);
	if(value < 0) {
		bits = static_cast<std::uint16_t>(bits |
			(static_cast<std::uint16_t>(0xFFFFU) << (16U - count)));
	}
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int16_t vram_top_living(std::int16_t top,
	std::int16_t scroll_line) noexcept
{
	top = word_add(top, scroll_line);
	if(top < 0) {
		top = word_add(top, screen_height);
	} else if(top >= screen_height) {
		top = word_add(top, -screen_height);
	}
	return top;
}

[[nodiscard]] std::int16_t vram_top_defeat(std::int16_t top,
	std::int16_t scroll_line) noexcept
{
	top = word_add(top, scroll_line);
	if(top >= screen_height) top = word_add(top, -screen_height);
	return top;
}

[[nodiscard]] Midboss2UpdateResult map_bullet_result(
	EnemyBulletAddResult result
) noexcept
{
	switch(result) {
	case EnemyBulletAddResult::completed:
		return Midboss2UpdateResult::completed;
	case EnemyBulletAddResult::invalid_page:
		return Midboss2UpdateResult::bullet_invalid_page;
	case EnemyBulletAddResult::source_iatan2_fault:
		return Midboss2UpdateResult::bullet_source_iatan2_fault;
	}
	return Midboss2UpdateResult::bullet_source_iatan2_fault;
}

[[nodiscard]] Midboss2UpdateResult fire_pattern(
	const BossState &boss, Midboss2Context &context
) noexcept
{
	if((boss.phase_frame % 8) != 0 || (boss.phase_frame % 192) >= 64) {
		return Midboss2UpdateResult::completed;
	}
	const auto &back_topleft = boss.topleft_on_page[context.player_frame.page_back];
	const auto angle = static_cast<std::uint8_t>(
		static_cast<std::uint16_t>(std::bit_cast<std::uint16_t>(boss.phase_frame)) << 2U
	);
	return map_bullet_result(bullets_add_16x16(context.bullets, context.random,
		context.player_frame, context.playperf, context.rank, context.bombing,
		{word_add(back_topleft.x, 28), word_add(back_topleft.y, 32)}, angle,
		enemy_bullet_special_motion::chase, midboss_bullet_patnum_noroi,
		midboss_bullet_speed));
}

[[nodiscard]] Midboss2UpdateResult defeat_update_and_render(BossState &boss,
	Midboss2State &state, Midboss2Context &context,
	Midboss2Callbacks &callbacks
) noexcept
{
	const auto &back_topleft = boss.topleft_on_page[context.player_frame.page_back];
	if(state.defeat_frame < midboss_defeat_spark_until &&
		(state.defeat_frame & 15) == 5) {
		if(!sparks_add(context.sparks, context.player_frame.page_back,
			word_add(back_topleft.x, 32),
			static_cast<std::uint16_t>(word_add(back_topleft.y, 32)), 8,
			midboss_defeat_spark_count, true)) {
			return Midboss2UpdateResult::defeat_spark_allocation_refused;
		}
	}
	const auto zoom_patnum = word_add(midboss_defeat_zoom_patnum,
		arithmetic_shift_right(state.defeat_frame, 3U));
	state.defeat_frame = word_add(state.defeat_frame, 1);
	if(state.defeat_frame >= midboss_defeat_frames) {
		state.defeat_frame = 0;
		boss.phase_frame = 0;
		boss.phase = 0U;
		state.active = 0;
		boss.target = {-1, -1};
		return Midboss2UpdateResult::completed;
	}
	callbacks.render_midboss({Midboss2SpriteKind::defeat_zoom,
		{back_topleft.x, vram_top_defeat(back_topleft.y, context.scroll_line)},
		zoom_patnum, 2});
	return Midboss2UpdateResult::completed;
}

[[nodiscard]] Midboss2UpdateResult hit_test_entry(BossState &boss,
	Midboss2Context &context, Midboss2Callbacks &callbacks,
	std::int16_t vram_top, std::int16_t patnum
) noexcept
{
	const auto &back_topleft = boss.topleft_on_page[context.player_frame.page_back];
	PlayerShotSparkAllocatorCallbacks shot_sparks(context.sparks,
		context.player_frame.page_back);
	const auto damage = shots_hittest(context.shots, context.score,
		context.player_frame.page_back, context.shot_level, context.stage_frame,
		context.bombing, context.bomb_damage_frame_mask,
		{{back_topleft.x, back_topleft.y}, midboss_width, 58}, shot_sparks);
	if(!damage) return Midboss2UpdateResult::invalid_shot_hittest;
	if(!shot_sparks.all_allocations_succeeded()) {
		return Midboss2UpdateResult::shot_spark_allocation_refused;
	}
	if(*damage != 0) {
		callbacks.sound_effect(4U);
		callbacks.render_midboss({Midboss2SpriteKind::hit_white,
			{back_topleft.x, vram_top}, midboss_patnum_hit, 0});
		boss.damage = word_add(boss.damage, 1);
	} else {
		callbacks.render_midboss({Midboss2SpriteKind::normal,
			{back_topleft.x, vram_top}, patnum, 0});
	}
	return Midboss2UpdateResult::completed;
}

[[nodiscard]] Midboss2UpdateResult hit_test_living(BossState &boss,
	Midboss2Context &context, Midboss2Callbacks &callbacks,
	std::int16_t vram_top, std::int16_t patnum
) noexcept
{
	const auto &back_topleft = boss.topleft_on_page[context.player_frame.page_back];
	PlayerShotSparkAllocatorCallbacks shot_sparks(context.sparks,
		context.player_frame.page_back);
	const auto damage = shots_hittest(context.shots, context.score,
		context.player_frame.page_back, context.shot_level, context.stage_frame,
		context.bombing, context.bomb_damage_frame_mask,
		{{back_topleft.x, back_topleft.y}, midboss_width, 58}, shot_sparks);
	if(!damage) return Midboss2UpdateResult::invalid_shot_hittest;
	if(!shot_sparks.all_allocations_succeeded()) {
		return Midboss2UpdateResult::shot_spark_allocation_refused;
	}
	if(*damage != 0) {
		boss.damage = word_add(boss.damage, *damage);
		if(boss.damage <= midboss_damage_max || vram_top >= midboss_defeat_vram_y_max) {
			callbacks.sound_effect(4U);
			callbacks.render_midboss({Midboss2SpriteKind::hit_white,
				{back_topleft.x, vram_top}, midboss_patnum_hit, 0});
		} else {
			callbacks.sound_effect(2U);
			boss.phase = 1U;
			context.score.score_delta = dword_add(context.score.score_delta, 20000);
		}
		return Midboss2UpdateResult::completed;
	}
	if(boss.phase_frame > midboss_leave_frame) boss.phase = 1U;
	callbacks.render_midboss({Midboss2SpriteKind::normal,
		{back_topleft.x, vram_top}, patnum, 0});
	return Midboss2UpdateResult::completed;
}

} // namespace

Midboss2InvalidateResult midboss2_invalidate(BossState &boss,
	const PlayerFrameState &frame, const Midboss2State &state,
	Midboss2Callbacks &callbacks
) noexcept
{
	if(frame.page_back >= boss.topleft_on_page.size() ||
		frame.page_front >= boss.topleft_on_page.size()) {
		return Midboss2InvalidateResult::invalid_page;
	}
	const auto old_back = boss.topleft_on_page[frame.page_back];
	callbacks.invalidate_midboss({old_back, midboss_width, midboss_height});
	boss.topleft_on_page[frame.page_back] = boss.topleft_on_page[frame.page_front];
	return (state.active != 0) ? Midboss2InvalidateResult::active :
		Midboss2InvalidateResult::inactive;
}

Midboss2UpdateResult midboss2_update_and_render(BossState &boss,
	Midboss2State &state, Midboss2Context &context,
	Midboss2Callbacks &callbacks
) noexcept
{
	if(context.player_frame.page_back >= boss.topleft_on_page.size()) {
		return Midboss2UpdateResult::invalid_page;
	}
	auto &back_topleft = boss.topleft_on_page[context.player_frame.page_back];
	boss.target = {216, word_add(back_topleft.y, 24)};
	boss.phase_frame = word_add(boss.phase_frame, 1);
	if(boss.phase_frame == 1) {
		boss.topleft_on_page[0U] = {midboss_enter_left, midboss_enter_top};
		boss.topleft_on_page[1U] = {midboss_enter_left, midboss_enter_top};
		boss.damage = 0;
		state.active = 1;
		context.flash.frame = 1;
		return Midboss2UpdateResult::completed;
	}

	const auto cel = static_cast<std::int16_t>(
		arithmetic_shift_right(context.scroll_line, 1U) & (midboss_cels - 1)
	);
	const auto patnum = word_add(midboss_patnum, cel);
	if(boss.phase_frame < midboss_entry_frames) {
		if((std::bit_cast<std::uint16_t>(boss.phase_frame) & 3U) == 0U) {
			back_topleft.y = word_add(back_topleft.y, 1);
		}
		return hit_test_entry(boss, context, callbacks,
			vram_top_living(back_topleft.y, context.scroll_line), patnum);
	}
	if(boss.phase != 0U) {
		return defeat_update_and_render(boss, state, context, callbacks);
	}
	const auto fire_result = fire_pattern(boss, context);
	if(fire_result != Midboss2UpdateResult::completed) return fire_result;
	return hit_test_living(boss, context, callbacks,
		vram_top_living(back_topleft.y, context.scroll_line), patnum);
}

} // namespace th02::core
