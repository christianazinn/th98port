#include "th01/core/projectiles.hpp"
#include "th01/core/stage.hpp"
#include "th01/core/state.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>

namespace {
using namespace th01::core;

int failures{};

void check(bool condition, const char *message)
{
	if(!condition) {
		std::cerr << message << '\n';
		++failures;
	}
}

void put_le32(std::span<std::uint8_t> bytes, std::size_t offset,
	std::uint32_t value)
{
	bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
	bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
	bytes[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
	bytes[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

template <std::size_t Size>
void put_archive_name(std::array<std::uint8_t, Size> &archive,
	std::size_t entry, const char (&name)[11])
{
	for(std::size_t index = 0U; index < 11U; ++index) {
		if(name[index] == '\0') { archive[(entry * 32U) + 3U + index] = 0; break; }
		archive[(entry * 32U) + 3U + index] = static_cast<std::uint8_t>(
			~static_cast<std::uint8_t>(name[index])
		);
	}
}

template <std::size_t Size>
void put_raw_stage_entry(std::array<std::uint8_t, Size> &archive,
	std::span<const std::uint8_t> stage_data)
{
	constexpr std::size_t payload_offset = 64U * 32U;
	archive[0U] = static_cast<std::uint8_t>('R');
	put_archive_name(archive, 0U, "STAGE0.DAT");
	put_le32(archive, 16U, static_cast<std::uint32_t>(stage_data.size()));
	put_le32(archive, 20U, static_cast<std::uint32_t>(stage_data.size()));
	put_le32(archive, 24U, payload_offset);
	for(std::size_t index = 0U; index < stage_data.size(); ++index) {
		archive[payload_offset + index] = static_cast<std::uint8_t>(stage_data[index] ^ 0x76U);
	}
}

void archive_tests()
{
	constexpr std::size_t stage_bytes = 22U + (5U * 250U);
	std::array<std::uint8_t, stage_bytes> stage_data{};
	stage_data[22U] = 0x80U;
	stage_data[22U + 50U] = static_cast<std::uint8_t>(ObstacleType::bumper);
	std::array<std::uint8_t, (64U * 32U) + stage_bytes> archive{};
	put_raw_stage_entry(archive, stage_data);
	StageState stage{};
	check(stage_scene_load_from_archive(archive, 0U, stage) ==
		StageArchiveResult::completed, "PF raw STAGE0.DAT loads");
	check(stage.scene[0U].cards[0U][0U] == 0x80U &&
		stage.scene[0U].obstacles[0U][0U] ==
		static_cast<std::int8_t>(ObstacleType::bumper),
		"PF loader retains the source card and obstacle bytes");
	check(stage_scene_load_from_archive(archive, 10U, stage) ==
		StageArchiveResult::invalid_scene_id, "PF loader rejects source-invalid scene id");

	// A 1,272-byte all-zero STAGE payload encoded with the source's byte RLE.
	// The final run deliberately ends at the requested output boundary.
	constexpr std::array<std::uint8_t, 11U> rle_plain{
		0U, 0U, 255U, 0U, 255U, 0U, 255U, 0U, 255U, 0U, 246U,
	};
	std::array<std::uint8_t, (64U * 32U) + rle_plain.size()> compressed{};
	compressed[0U] = 0x95U;
	compressed[1U] = 0x95U;
	put_archive_name(compressed, 0U, "STAGE0.DAT");
	put_le32(compressed, 16U, static_cast<std::uint32_t>(rle_plain.size()));
	put_le32(compressed, 20U, static_cast<std::uint32_t>(stage_bytes));
	put_le32(compressed, 24U, 64U * 32U);
	for(std::size_t index = 0U; index < rle_plain.size(); ++index) {
		compressed[(64U * 32U) + index] = static_cast<std::uint8_t>(rle_plain[index] ^ 0x76U);
	}
	stage = {};
	check(stage_scene_load_from_archive(compressed, 0U, stage) ==
		StageArchiveResult::completed && stage.scene[4U].cards[9U][4U] == 0U,
		"PF compressed RLE STAGE0.DAT loads at the source output boundary");
}

void stage_setup_and_cards_tests()
{
	State state{};
	state.random_seed = 7U;
	state.stage.scene[0U].cards[0U][0U] = 0x0CU;
	state.stage.scene[0U].obstacles[0U][0U] = static_cast<std::int8_t>(
		ObstacleType::card_two_flip
	);
	state.stage.scene[0U].obstacles[0U][2U] = static_cast<std::int8_t>(
		ObstacleType::turret_quick_1_aimed
	);
	state.stage.scene[0U].obstacles[1U][0U] = static_cast<std::int8_t>(
		ObstacleType::bumper
	);
	check(stage_setup(state, true) == StageSetupResult::completed,
		"stage setup accepts a source-shaped regular stage");
	check(state.stage.card_count == 2U && state.stage.obstacle_count == 2U &&
		state.stage.cards[0U].left == 0 && state.stage.cards[1U].left == 32 &&
		state.stage.cards[0U].hp == 1U,
		"stage setup preserves packed-card ordering, coordinates, and HP overlay");

	state.orb.horizontal.left = 0;
	state.orb.top = 64;
	state.cardcombo_cur = 0;
	state.stage.card_flip_cycle = 0U;
	check(stage_cards_hittest_and_obstacles_update(state),
		"card/obstacle source update completes");
	check(state.stage.cards[0U].flag == CardFlag::flipping &&
		state.stage.cards[0U].score == 100U && state.score == 100U &&
		state.cardcombo_cur == 1 && state.stage.points[0U].flag == ItemFlag::splash,
		"card hit awards source score/combo and first-cycle point item");
	for(int frame = 0; frame < 30; ++frame) stage_cards_update(state);
	check(state.stage.cards[0U].flag == CardFlag::alive && state.stage.cards[0U].hp == 0U &&
		state.stage.cards[0U].score == 0U,
		"two-flip card returns alive after the first complete flip and score tail");

	state.orb.horizontal.left = 32;
	state.orb.top = 64;
	state.stage.card_flip_cycle = 139U;
	state.bomb_damaging = false;
	state.test_damage = true;
	check(stage_cards_hittest_and_obstacles_update(state), "debug card-damage path completes");
	check(state.stage.card_flip_cycle == 1U && state.stage.bombs[0U].flag == ItemFlag::splash,
		"cycle 140 resets and creates a bomb outside a damaging bomb");
}

void item_timer_and_obstacle_tests()
{
	State state{};
	state.stage.bombs[0U] = {
		.left = 304, .top = 360, .velocity_y = 2, .flag = ItemFlag::fall,
	};
	stage_items_update(state);
	check(state.rem_bombs == 2 && state.stage.bombs[0U].flag == ItemFlag::collected &&
		state.stage.bombs[0U].top == 358 && state.stage.bombs[0U].state == 15U,
		"bomb item tests before motion then enters its sixteen-frame popup");
	state.stage.points[0U] = {
		.left = 304, .top = 360, .velocity_y = 2, .flag = ItemFlag::fall,
	};
	stage_items_update(state);
	check(state.point_value == 1000U && state.score == 1000U &&
		state.stage.points[0U].flag == ItemFlag::collected_over_cap,
		"point item increments resident point value before score");

	state.stage_id = 16;
	state.route = 1;
	check(stage_timer_init(state) && state.stage.timer == 1800U,
		"timer table selects the Jigoku Stage 17 value");
	state.stage_id = 9;
	check(stage_timer_init(state) && state.stage.timer == 2000U,
		"timer table selects the Jigoku Stage 10 value");
	state.stage_id = 17;
	check(stage_timer_init(state) && state.stage.timer == 1800U,
		"timer table selects the Jigoku Stage 18 value");
	state.stage_id = 19;
	check(stage_timer_init(state) && state.stage.timer == 8000U,
		"timer table selects the Jigoku Stage 20 value");
	state.stage_id = 16;
	check(stage_timer_init(state), "timer restores the HARRY UP fixture stage");
	state.stage.timer = 2U;
	stage_timer_tick(state);
	check(state.stage.timer == 0U && state.projectiles.pellet_speed_x40 == 2,
		"timer zero raises pellet speed by source 0.05");
	state.stage.harryup_cycle = 4U;
	state.rank = 0;
	stage_timer_tick(state);
	check(state.stage.frames_since_harryup == 1U && state.stage.harryup_cycle == 5U &&
		state.projectiles.pellet_alive_count == 1,
		"HARRY UP fires its first random-wide pellet on cycle five");

	state = {};
	state.stage_id = 0;
	state.stage.obstacle_count = 1U;
	state.stage.obstacles[0U] = {
		.left = 0, .top = 64, .type = ObstacleType::bumper,
	};
	state.orb.horizontal.left = 0;
	state.orb.horizontal.velocity = OrbVelocityX::right4;
	state.orb.top = 64;
	check(stage_cards_hittest_and_obstacles_update(state) && state.orb.top == 40 &&
		state.stage.obstacles[0U].frame == 2 && state.orb.vertical.force == 0.0,
		"bumper teleports before setting its source surface bounce force");
}

} // namespace

int main()
{
	archive_tests();
	stage_setup_and_cards_tests();
	item_timer_and_obstacle_tests();
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
