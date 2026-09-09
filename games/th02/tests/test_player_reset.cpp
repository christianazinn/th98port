#include "th02/core/reset.hpp"

#include <cstdio>
#include <cstdlib>

namespace {

using namespace th02::core;

struct ResetFixture {
	PlayerResetState reset{0xA1U, 0xA2U, 0xA3U, 0xA4U};
	bool quit{true};
	PlayerHitState hit{0xFFU, 7U, true, true};
	PlayerMissState miss{26U, 52U, 0, -5, 8U, 9U, 4U};
	PlayerShotPoolState shots{};
	PlayerShotLifetimeState lifetime{};
	PlayerShotStreamState stream{3U, 5U, 7U, 8U, 9U, 0xAAU, 0xBBU,
		0xCCU, 0xDDU, 22};
	PlayerShotVolleyContext volley{};
	MovementSpeeds speeds{12, 13, 14, 15};
};

void warm(ResetFixture &fixture) noexcept
{
	fixture.lifetime.option_shots_alive = 5U;
	fixture.lifetime.option_decay_interval = -8;
	for(std::size_t i = 0; i < fixture.shots.shots.size(); ++i) {
		auto &shot = fixture.shots.shots[i];
		shot.flag = static_cast<std::uint8_t>((i % 2U) + 1U);
		shot.decay_cel = static_cast<std::uint8_t>(i + 3U);
		shot.pos_on_page[0] = {
			static_cast<std::int16_t>(i + 100U),
			static_cast<std::int16_t>(i + 200U),
		};
		shot.pos_on_page[1] = {
			static_cast<std::int16_t>(i + 300U),
			static_cast<std::int16_t>(i + 400U),
		};
		shot.velocity = {
			static_cast<std::int16_t>(i + 500U),
			static_cast<std::int16_t>(i + 600U),
		};
		shot.patnum = static_cast<std::uint8_t>(i + 7U);
		shot.from_option = ((i % 2U) != 0U);
		fixture.lifetime.animation_frames[i] = static_cast<std::uint8_t>(i + 11U);
	}
	fixture.shots.current_slot = 17;
	fixture.shots.spawn_top = -123;
	fixture.shots.patnum = 0xCAU;
	fixture.shots.option_patnum = 0xCBU;
	fixture.volley.type = PlayerShotType::invalid;
	fixture.volley.positions = {{101, 102}, {103, 104}};
	fixture.volley.back_page = 1U;
	fixture.volley.boss_position = {55, 56};
	fixture.volley.boss_pos_x_unused = 57;
	fixture.volley.shot_c_cycle = 58U;
}

[[nodiscard]] bool same_speeds(MovementSpeeds left,
	MovementSpeeds right) noexcept
{
	return left.aligned_x == right.aligned_x &&
		left.aligned_y == right.aligned_y &&
		left.diagonal_x == right.diagonal_x &&
		left.diagonal_y == right.diagonal_y;
}

[[nodiscard]] bool only_shot_flags_freed(const PlayerShotPoolState &before,
	const PlayerShotPoolState &after) noexcept
{
	for(std::size_t i = 0; i < before.shots.size(); ++i) {
		auto expected = before.shots[i];
		expected.flag = player_shot_free;
		if(after.shots[i] != expected) return false;
	}
	return after.current_slot == before.current_slot &&
		after.spawn_top == before.spawn_top && after.patnum == before.patnum &&
		after.option_patnum == before.option_patnum;
}

[[nodiscard]] bool common_prefix_is_exact(const ResetFixture &fixture,
	const ResetFixture &before) noexcept
{
	return !fixture.quit &&
		fixture.reset.shot_unused == 8U && fixture.reset.shot_unused_2 == 0U &&
		fixture.reset.player_unused == 0U && fixture.hit.raw_hit == 0U &&
		fixture.hit.invincibility_time == 0U && !fixture.hit.invincible_via_bomb &&
		!fixture.hit.miss_active && fixture.miss.miss_frame == 0U &&
		fixture.miss.stage_miss_count == 0U &&
		fixture.miss.power == before.miss.power &&
		fixture.miss.playperf == before.miss.playperf &&
		fixture.miss.total_miss_count == before.miss.total_miss_count &&
		fixture.miss.start_bombs == before.miss.start_bombs &&
		fixture.lifetime.option_shots_alive == 0U &&
		fixture.lifetime.animation_frames == before.lifetime.animation_frames &&
		fixture.stream.stream_a_phase == before.stream.stream_a_phase &&
		fixture.stream.stream_b_phase == before.stream.stream_b_phase &&
		fixture.stream.stream_a_cooldown == before.stream.stream_a_cooldown &&
		fixture.stream.stream_b_cooldown == before.stream.stream_b_cooldown &&
		fixture.stream.shot_level == before.stream.shot_level &&
		fixture.stream.shot_patnum == before.stream.shot_patnum &&
		fixture.stream.shot_option_patnum == before.stream.shot_option_patnum &&
		fixture.stream.spread_angle_delta == 0 &&
		fixture.volley.positions == before.volley.positions &&
		fixture.volley.back_page == before.volley.back_page &&
		fixture.volley.shot_c_cycle == before.volley.shot_c_cycle &&
		fixture.volley.boss_position == ShotPixelPoint{-1, -1} &&
		fixture.volley.boss_pos_x_unused == -1 &&
		only_shot_flags_freed(before.shots, fixture.shots);
}

[[nodiscard]] bool reset_a_writes_its_partial_selection() noexcept
{
	ResetFixture fixture;
	warm(fixture);
	const auto before = fixture;
	player_reset(fixture.reset, fixture.quit, fixture.hit, fixture.miss,
		fixture.shots, fixture.lifetime, fixture.stream, fixture.volley,
		fixture.speeds, 0U);

	return common_prefix_is_exact(fixture, before) &&
		fixture.reset.option_patnum == 72U && fixture.volley.type == PlayerShotType::a &&
		same_speeds(fixture.speeds, {5, 5, 4, 4}) &&
		fixture.stream.shot_patnum_powered == 0x30U &&
		fixture.stream.shot_option_patnum_powered == before.stream.shot_option_patnum_powered &&
		fixture.lifetime.option_decay_interval == before.lifetime.option_decay_interval;
}

[[nodiscard]] bool reset_b_writes_its_partial_selection() noexcept
{
	ResetFixture fixture;
	warm(fixture);
	const auto before = fixture;
	player_reset(fixture.reset, fixture.quit, fixture.hit, fixture.miss,
		fixture.shots, fixture.lifetime, fixture.stream, fixture.volley,
		fixture.speeds, 1U);

	return common_prefix_is_exact(fixture, before) &&
		fixture.reset.option_patnum == 73U && fixture.volley.type == PlayerShotType::b &&
		same_speeds(fixture.speeds, {4, 4, 3, 3}) &&
		fixture.stream.shot_patnum_powered == 0x34U &&
		fixture.stream.shot_option_patnum_powered == 0x37U &&
		fixture.lifetime.option_decay_interval == before.lifetime.option_decay_interval;
}

[[nodiscard]] bool reset_c_writes_its_partial_selection() noexcept
{
	ResetFixture fixture;
	warm(fixture);
	const auto before = fixture;
	player_reset(fixture.reset, fixture.quit, fixture.hit, fixture.miss,
		fixture.shots, fixture.lifetime, fixture.stream, fixture.volley,
		fixture.speeds, 2U);

	return common_prefix_is_exact(fixture, before) &&
		fixture.reset.option_patnum == 74U && fixture.volley.type == PlayerShotType::c &&
		same_speeds(fixture.speeds, {3, 3, 3, 3}) &&
		fixture.stream.shot_patnum_powered == 0x7DU &&
		fixture.stream.shot_option_patnum_powered == 0x3BU &&
		fixture.lifetime.option_decay_interval == 3;
}

[[nodiscard]] bool invalid_shottype_keeps_selection_state() noexcept
{
	for(const auto shottype : {3U, 0xFFU}) {
		ResetFixture fixture;
		warm(fixture);
		fixture.volley.type = PlayerShotType::b;
		const auto before = fixture;
		player_reset(fixture.reset, fixture.quit, fixture.hit, fixture.miss,
			fixture.shots, fixture.lifetime, fixture.stream, fixture.volley,
			fixture.speeds, static_cast<std::uint8_t>(shottype));

		if(!common_prefix_is_exact(fixture, before) ||
			fixture.reset.option_patnum != 72U ||
			fixture.volley.type != before.volley.type ||
			!same_speeds(fixture.speeds, before.speeds) ||
			fixture.stream.shot_patnum_powered != before.stream.shot_patnum_powered ||
			fixture.stream.shot_option_patnum_powered !=
				before.stream.shot_option_patnum_powered ||
			fixture.lifetime.option_decay_interval != before.lifetime.option_decay_interval) {
			return false;
		}
	}
	return true;
}

}

int main()
{
	if(!reset_a_writes_its_partial_selection()) {
		std::fputs("FAIL reset A partial writes\n", stderr);
		return EXIT_FAILURE;
	}
	if(!reset_b_writes_its_partial_selection()) {
		std::fputs("FAIL reset B partial writes\n", stderr);
		return EXIT_FAILURE;
	}
	if(!reset_c_writes_its_partial_selection()) {
		std::fputs("FAIL reset C partial writes\n", stderr);
		return EXIT_FAILURE;
	}
	if(!invalid_shottype_keeps_selection_state()) {
		std::fputs("FAIL invalid reset selection preservation\n", stderr);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
