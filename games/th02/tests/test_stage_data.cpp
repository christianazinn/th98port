#include "th02/core/stage_data.hpp"
#include "th02/core/random.hpp"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <vector>

namespace {

using namespace th02::core;

class StageWriter {
public:
	void byte(std::uint8_t value) { payload_.push_back(value); }

	void word(std::int16_t value)
	{
		const auto bits = std::bit_cast<std::uint16_t>(value);
		byte(static_cast<std::uint8_t>(bits & 0x00FFU));
		byte(static_cast<std::uint8_t>(bits >> 8U));
	}

	void dword(std::int32_t value)
	{
		const auto bits = std::bit_cast<std::uint32_t>(value);
		byte(static_cast<std::uint8_t>(bits & 0x000000FFU));
		byte(static_cast<std::uint8_t>((bits >> 8U) & 0x000000FFU));
		byte(static_cast<std::uint8_t>((bits >> 16U) & 0x000000FFU));
		byte(static_cast<std::uint8_t>(bits >> 24U));
	}

	void script(std::initializer_list<std::uint8_t> values)
	{
		for(const auto value : values) byte(value);
	}

	void template_record(std::int16_t patnum, std::int16_t autofire)
	{
		word(0x0102);
		word(-20);
		word(3);
		word(-4);
		word(patnum);
		word(5);
		word(6);
		word(7);
		byte(0xA1U);
		byte(0xA2U);
		dword(0x10203040);
		word(0x1234);
		byte(0xFEU);
		byte(0xA3U);
		word(autofire);
	}

	[[nodiscard]] std::vector<std::uint8_t> finish() const
	{
		std::vector<std::uint8_t> result;
		result.reserve(payload_.size() + 4U);
		const auto size = static_cast<std::uint32_t>(payload_.size());
		result.push_back(static_cast<std::uint8_t>(size & 0x000000FFU));
		result.push_back(static_cast<std::uint8_t>((size >> 8U) & 0x000000FFU));
		result.push_back(static_cast<std::uint8_t>((size >> 16U) & 0x000000FFU));
		result.push_back(static_cast<std::uint8_t>(size >> 24U));
		result.insert(result.end(), payload_.begin(), payload_.end());
		return result;
	}

private:
	std::vector<std::uint8_t> payload_{};
};

[[nodiscard]] bool valid_templates_scripts_and_rows() noexcept
{
	StageWriter writer;
	writer.word(2);
	writer.byte(0x11U);
	writer.byte(0x22U);
	writer.word(2);
	writer.template_record(1, 10);
	writer.script({0x12U, 0x34U, 0xFFU});
	writer.template_record(2, -3);
	writer.script({0xFEU, 0x00U, 0xAAU});
	writer.word(1);
	writer.word(-7);
	writer.byte(0xFFU);
	writer.byte(2U);
	for(std::size_t column = 3U; column < enemy_spawn_column_count; ++column) {
		writer.byte(static_cast<std::uint8_t>(column));
	}
	const auto bytes = writer.finish();
	const std::array<EnemySpriteSizeWord, 3U> sizes{
		0x0102U, 0x0203U, 0x0304U};
	EnemyStageData decoded{};
	if(enemy_stage_data_decode(bytes, sizes, 0, decoded) !=
		EnemyStageDataResult::completed) {
		return false;
	}
	const auto &first = decoded.templates[0U];
	const auto &second = decoded.templates[1U];
	return decoded.map_length == 2 && decoded.map[0U] == 0x11U &&
		decoded.map[1U] == 0x22U && decoded.template_count == 2 &&
		first.patnum == 1 && first.width == 16 && first.height == 3 &&
		first.unused == 0x0102 && first.spawn_top == -20 &&
		first.bullet_origin_x == 3 && first.bullet_origin_y == -4 &&
		first.anim_cels == 5 && first.anim_frames_per_cel == 6 &&
		first.explode_sprite == 7 && first.item == 0xA1U &&
		first.unused_2 == 0xA2U && first.autofire_interval == 20 &&
		first.score == 0x10203040 && first.hp == 0x1234 &&
		first.no_player_collision == 0xFEU && first.unused_3 == 0xA3U &&
		second.patnum == 2 &&
		second.width == 24 && second.height == 4 && second.autofire_interval == -6 &&
		second.script_index == 0U && decoded.script_count == 1 &&
		decoded.scripts[0U][0U] == 0x12U && decoded.scripts[0U][1U] == 0x34U &&
		decoded.scripts[0U][2U] == 0U && decoded.script_lengths[0U] == 2U &&
		decoded.spawn_row_count == 1 &&
		decoded.spawn_rows.size() == 1U &&
		decoded.spawn_rows[0U].column[0U] == -7 &&
		decoded.spawn_rows[0U].column[1U] == -1 &&
		decoded.spawn_rows[0U].column[2U] == 2 &&
		decoded.spawn_rows[0U].column[48U] == 48;
}

[[nodiscard]] bool rank_scaling_and_sprite_derivation() noexcept
{
	StageWriter writer;
	writer.word(0);
	writer.word(1);
	writer.template_record(0, -3);
	writer.script({0xFFU, 0xFFU});
	writer.word(0);
	const auto bytes = writer.finish();
	const std::array<EnemySpriteSizeWord, 1U> sizes{0x0304U};
	const std::array<std::int8_t, 5U> ranks{0, 1, 2, 3, 4};
	const std::array<std::int16_t, 5U> expected{-6, -3, -2, -2, -3};
	for(std::size_t i = 0U; i < ranks.size(); ++i) {
		EnemyStageData decoded{};
		if(enemy_stage_data_decode(bytes, sizes, ranks[i], decoded) !=
			EnemyStageDataResult::completed ||
			decoded.templates[0U].autofire_interval != expected[i] ||
			decoded.templates[0U].width != 24 ||
			decoded.templates[0U].height != 4 ||
			decoded.script_lengths[0U] != 1U ||
			decoded.scripts[0U][0U] != 0xFFU) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool malformed_counts_and_atomic_failure() noexcept
{
	StageWriter writer;
	writer.word(0);
	writer.word(0);
	writer.word(0);
	const auto valid = writer.finish();
	const std::array<EnemySpriteSizeWord, 1U> sizes{0x0101U};
	EnemyStageData before{};
	before.map_length = 9;
	before.map[0U] = 0x5AU;
	before.template_count = 3;
	before.spawn_row_count = 4;
	const auto expected = before;
	for(const auto result : {
		EnemyStageDataResult::invalid_file_size,
		EnemyStageDataResult::invalid_map_length,
		EnemyStageDataResult::invalid_template_count}) {
		auto malformed = valid;
		if(result == EnemyStageDataResult::invalid_file_size) {
			malformed[0U] ^= 1U;
		} else if(result == EnemyStageDataResult::invalid_map_length) {
			malformed[4U] = 0xFFU;
			malformed[5U] = 0xFFU;
		} else {
			malformed[6U] = 0xFFU;
			malformed[7U] = 0xFFU;
		}
		if(enemy_stage_data_decode(malformed, sizes, 1, before) != result ||
			before != expected) {
			return false;
		}
	}

	StageWriter map_truncated;
	map_truncated.word(2);
	map_truncated.byte(0xAAU);
	if(enemy_stage_data_decode(map_truncated.finish(), sizes, 1, before) !=
		EnemyStageDataResult::truncated_map || before != expected) {
		return false;
	}
	StageWriter template_truncated;
	template_truncated.word(0);
	template_truncated.word(1);
	template_truncated.byte(0x11U);
	if(enemy_stage_data_decode(template_truncated.finish(), sizes, 1, before) !=
		EnemyStageDataResult::truncated_template || before != expected) {
		return false;
	}
	return true;
}

[[nodiscard]] bool malformed_scripts_and_rows_are_refused() noexcept
{
	const std::array<EnemySpriteSizeWord, 1U> sizes{0x0101U};
	EnemyStageData unchanged{};
	unchanged.map_length = 8;
	const auto expected = unchanged;

	StageWriter no_terminator;
	no_terminator.word(0);
	no_terminator.word(1);
	no_terminator.template_record(0, 1);
	no_terminator.byte(0x01U);
	const auto no_terminator_result = enemy_stage_data_decode(
		no_terminator.finish(), sizes, 1, unchanged);
	if(no_terminator_result != EnemyStageDataResult::truncated_script ||
		unchanged != expected) {
		return false;
	}

	StageWriter invalid_alias;
	invalid_alias.word(0);
	invalid_alias.word(2);
	invalid_alias.template_record(0, 1);
	invalid_alias.script({0x00U, 0xFFU});
	invalid_alias.template_record(0, 1);
	invalid_alias.script({0xFEU, 0x01U, 0x00U});
	const auto invalid_alias_result = enemy_stage_data_decode(
		invalid_alias.finish(), sizes, 1, unchanged);
	if(invalid_alias_result != EnemyStageDataResult::invalid_script_reference ||
		unchanged != expected) {
		return false;
	}

	StageWriter long_script;
	long_script.word(0);
	long_script.word(1);
	long_script.template_record(0, 1);
	for(std::size_t i = 0U; i < stage_script_size + 1U; ++i) {
		long_script.byte(1U);
	}
	long_script.byte(0xFFU);
	const auto long_script_result = enemy_stage_data_decode(
		long_script.finish(), sizes, 1, unchanged);
	if(long_script_result != EnemyStageDataResult::script_too_long ||
		unchanged != expected) {
		return false;
	}

	StageWriter short_alias;
	short_alias.word(0);
	short_alias.word(1);
	short_alias.template_record(0, 1);
	short_alias.script({0xFEU, 0x00U});
	const auto short_alias_result = enemy_stage_data_decode(
		short_alias.finish(), sizes, 1, unchanged);
	if(short_alias_result != EnemyStageDataResult::truncated_script_reuse ||
		unchanged != expected) {
		return false;
	}

	StageWriter short_rows;
	short_rows.word(0);
	short_rows.word(0);
	short_rows.word(1);
	short_rows.word(12);
	for(std::size_t i = 0U; i < enemy_spawn_column_count - 2U; ++i) {
		short_rows.byte(0U);
	}
	const auto short_rows_result = enemy_stage_data_decode(
		short_rows.finish(), sizes, 1, unchanged);
	if(short_rows_result != EnemyStageDataResult::truncated_spawn_rows ||
		unchanged != expected) {
		return false;
	}
	return true;
}

[[nodiscard]] bool complete_spawn_tail_policy_is_explicit() noexcept
{
	const std::array<EnemySpriteSizeWord, 1U> sizes{0x0101U};
	StageWriter writer;
	writer.word(0);
	writer.word(0);
	writer.word(2);
	writer.word(123);
	for(std::size_t column = 1U; column < enemy_spawn_column_count; ++column) {
		writer.byte(static_cast<std::uint8_t>(column));
	}
	const auto bytes = writer.finish();
	EnemyStageData decoded{};
	if(enemy_stage_data_decode(bytes, sizes, 1, decoded) !=
		EnemyStageDataResult::truncated_spawn_rows) {
		return false;
	}
	if(enemy_stage_data_decode(bytes, sizes, 1, decoded,
		EnemyStageDataTailPolicy::materialize_complete_spawn_rows) !=
		EnemyStageDataResult::completed) {
		return false;
	}
	if(decoded.spawn_row_count != 2 || decoded.spawn_rows.size() != 1U ||
		decoded.spawn_rows[0U].column[0U] != 123 ||
		decoded.spawn_rows[0U].column[1U] != 1 ||
		decoded.spawn_rows[0U].column[48U] != 48) {
		return false;
	}
	EnemyPoolState enemies{};
	enemies.spawn_row_cur = 1;
	RandomState random{};
	const EnemySpawnGrid grid{decoded.spawn_rows, decoded.spawn_row_count};
	return enemies_spawn(enemies, random, grid, decoded.templates,
		{0, false, 0U}) == EnemySpawnResult::invalid_row;
}

[[nodiscard]] bool sprite_and_capacity_boundaries() noexcept
{
	StageWriter bad_sprite;
	bad_sprite.word(0);
	bad_sprite.word(1);
	bad_sprite.template_record(1, 1);
	bad_sprite.script({0xFFU});
	bad_sprite.word(0);
	const std::array<EnemySpriteSizeWord, 1U> short_sizes{0x0101U};
	EnemyStageData output{};
	output.map_length = 6;
	const auto expected = output;
	if(enemy_stage_data_decode(bad_sprite.finish(), short_sizes, 1, output) !=
		EnemyStageDataResult::invalid_sprite_index || output != expected) {
		return false;
	}

	StageWriter too_many_templates;
	too_many_templates.word(0);
	too_many_templates.word(129);
	if(enemy_stage_data_decode(too_many_templates.finish(), short_sizes, 1,
		output) != EnemyStageDataResult::invalid_template_count ||
		output != expected) {
		return false;
	}
	StageWriter too_long_map;
	too_long_map.word(321);
	if(enemy_stage_data_decode(too_long_map.finish(), short_sizes, 1, output) !=
		EnemyStageDataResult::invalid_map_length || output != expected) {
		return false;
	}
	return true;
}

} // namespace

int main()
{
	const std::array checks{
		valid_templates_scripts_and_rows(),
		rank_scaling_and_sprite_derivation(),
		malformed_counts_and_atomic_failure(),
		malformed_scripts_and_rows_are_refused(),
		complete_spawn_tail_policy_is_explicit(),
		sprite_and_capacity_boundaries(),
	};
	for(std::size_t i = 0U; i < checks.size(); ++i) {
		const bool passed = checks[i];
		if(!passed) {
			std::fprintf(stderr, "TH02 stage-data test %zu failed\n", i);
			return EXIT_FAILURE;
		}
	}
	std::fputs("TH02 stage-data tests passed\n", stdout);
	return EXIT_SUCCESS;
}
