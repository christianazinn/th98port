#include "th02/core/stage_data.hpp"

#include <bit>
#include <cstdint>
#include <limits>
#include <utility>

namespace th02::core {
namespace {

inline constexpr std::uint8_t script_reuse = 0xFEU;
inline constexpr std::uint8_t script_end = 0xFFU;
inline constexpr std::uint16_t byte_dots = 8U;
inline constexpr std::size_t file_size_prefix = 4U;
inline constexpr std::size_t spawn_row_bytes = 2U +
	(enemy_spawn_column_count - 1U);

[[nodiscard]] bool has_bytes(std::span<const std::uint8_t> bytes,
	std::size_t offset, std::size_t count) noexcept
{
	return offset <= bytes.size() && count <= bytes.size() - offset;
}

[[nodiscard]] std::uint16_t word_at(
	std::span<const std::uint8_t> bytes, std::size_t offset
) noexcept
{
	return static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(bytes[offset]) |
		static_cast<std::uint16_t>(
			static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U));
}

[[nodiscard]] std::uint32_t dword_at(
	std::span<const std::uint8_t> bytes, std::size_t offset
) noexcept
{
	return static_cast<std::uint32_t>(
		static_cast<std::uint32_t>(bytes[offset]) |
		(static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U) |
		(static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U) |
		(static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U));
}

[[nodiscard]] std::int16_t signed_word(std::uint16_t bits) noexcept
{
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] std::int32_t signed_dword(std::uint32_t bits) noexcept
{
	return std::bit_cast<std::int32_t>(bits);
}

[[nodiscard]] std::int16_t rank_scaled_autofire(
	std::int16_t interval, std::int8_t rank
) noexcept
{
	const auto bits = std::bit_cast<std::uint16_t>(interval);
	if(rank == 0) {
		// The source shifts its signed int word. Make the 16-bit wrap explicit.
		return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
			static_cast<std::uint32_t>(bits) << 1U));
	}
	if(rank == 2 || rank == 3) {
		// The source compiler performs an arithmetic right shift on its x86 word.
		const auto shifted = static_cast<std::uint16_t>(bits >> 1U);
		const auto sign = (bits & 0x8000U) != 0U ? 0x8000U : 0U;
		return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
			shifted | static_cast<std::uint16_t>(sign)));
	}
	return interval;
}

void decode_template_fields(
	EnemyTemplate &target, std::span<const std::uint8_t> bytes,
	std::size_t offset, EnemySpriteSizeWord sprite_size,
	std::int8_t rank
) noexcept
{
	target.unused = signed_word(word_at(bytes, offset + 0x00U));
	target.spawn_top = signed_word(word_at(bytes, offset + 0x02U));
	target.bullet_origin_x = signed_word(word_at(bytes, offset + 0x04U));
	target.bullet_origin_y = signed_word(word_at(bytes, offset + 0x06U));
	target.patnum = signed_word(word_at(bytes, offset + 0x08U));
	target.anim_cels = signed_word(word_at(bytes, offset + 0x0AU));
	target.anim_frames_per_cel = signed_word(word_at(bytes, offset + 0x0CU));
	target.explode_sprite = signed_word(word_at(bytes, offset + 0x0EU));
	target.item = bytes[offset + 0x10U];
	target.unused_2 = bytes[offset + 0x11U];
	target.score = signed_dword(dword_at(bytes, offset + 0x12U));
	target.hp = signed_word(word_at(bytes, offset + 0x16U));
	target.no_player_collision = bytes[offset + 0x18U];
	target.unused_3 = bytes[offset + 0x19U];
	target.autofire_interval = rank_scaled_autofire(
		signed_word(word_at(bytes, offset + 0x1AU)), rank);
	target.width = static_cast<std::int16_t>(
		static_cast<std::uint16_t>(sprite_size >> 8U) * byte_dots);
	target.height = static_cast<std::int16_t>(
		static_cast<std::uint8_t>(sprite_size & 0x00FFU));
}

} // namespace

EnemyStageDataResult enemy_stage_data_decode(
	std::span<const std::uint8_t> bytes,
	std::span<const EnemySpriteSizeWord> sprite_sizes, std::int8_t rank,
	EnemyStageData &out, EnemyStageDataTailPolicy tail_policy
) noexcept
{
	if(!has_bytes(bytes, 0U, file_size_prefix)) {
		return EnemyStageDataResult::invalid_file_size;
	}
	const auto declared_size = dword_at(bytes, 0U);
	const auto payload_size = bytes.size() - file_size_prefix;
	if(payload_size > std::numeric_limits<std::uint32_t>::max() ||
		declared_size != static_cast<std::uint32_t>(payload_size)) {
		return EnemyStageDataResult::invalid_file_size;
	}

	EnemyStageData decoded{};
	std::size_t cursor = file_size_prefix;
	const auto payload_end = bytes.size();
	if(!has_bytes(bytes, cursor, 2U)) {
		return EnemyStageDataResult::invalid_map_length;
	}
	const auto map_length = signed_word(word_at(bytes, cursor));
	cursor += 2U;
	if(map_length < 0 ||
		static_cast<std::size_t>(map_length) > stage_map_length_max) {
		return EnemyStageDataResult::invalid_map_length;
	}
	if(!has_bytes(bytes, cursor, static_cast<std::size_t>(map_length))) {
		return EnemyStageDataResult::truncated_map;
	}
	decoded.map_length = map_length;
	for(std::size_t i = 0U; i < static_cast<std::size_t>(map_length); ++i) {
		decoded.map[i] = bytes[cursor + i];
	}
	cursor += static_cast<std::size_t>(map_length);

	if(!has_bytes(bytes, cursor, 2U)) {
		return EnemyStageDataResult::invalid_template_count;
	}
	const auto template_count = signed_word(word_at(bytes, cursor));
	cursor += 2U;
	if(template_count < 0 ||
		static_cast<std::size_t>(template_count) > stage_template_capacity) {
		return EnemyStageDataResult::invalid_template_count;
	}

	for(std::size_t i = 0U; i < static_cast<std::size_t>(template_count); ++i) {
		if(!has_bytes(bytes, cursor, stage_template_file_size)) {
			return EnemyStageDataResult::truncated_template;
		}
		const auto patnum = signed_word(word_at(bytes, cursor + 0x08U));
		if(patnum < 0 || static_cast<std::size_t>(patnum) >= sprite_sizes.size()) {
			return EnemyStageDataResult::invalid_sprite_index;
		}
		const auto sprite_size = sprite_sizes[static_cast<std::size_t>(patnum)];
		decode_template_fields(decoded.templates[i], bytes, cursor,
			sprite_size, rank);
		cursor += stage_template_file_size;

		if(!has_bytes(bytes, cursor, 1U)) {
			return EnemyStageDataResult::truncated_script;
		}
		if(bytes[cursor] == script_reuse) {
			// The source advances over FE, the referenced index, and one
			// skipped byte; its second increment lands on the next record.
			if(!has_bytes(bytes, cursor, 3U)) {
				return EnemyStageDataResult::truncated_script_reuse;
			}
			const auto script_index = bytes[cursor + 1U];
			if(static_cast<std::size_t>(script_index) >=
				static_cast<std::size_t>(decoded.script_count)) {
				return EnemyStageDataResult::invalid_script_reference;
			}
			decoded.templates[i].script_index = script_index;
			cursor += 3U;
			continue;
		}

		if(static_cast<std::size_t>(decoded.script_count) >=
			stage_script_capacity) {
			return EnemyStageDataResult::script_pool_full;
		}
		const auto script_index = static_cast<std::size_t>(decoded.script_count);
		auto &script = decoded.scripts[script_index];
		std::size_t script_length = 0U;
		bool terminated = false;
		do {
			if(cursor >= payload_end) break;
			const auto value = bytes[cursor++];
			if(script_length >= stage_script_size) {
				return EnemyStageDataResult::script_too_long;
			}
			script[script_length++] = value;
			if(!has_bytes(bytes, cursor, 1U)) break;
			if(bytes[cursor] == script_end) {
				cursor++;
				terminated = true;
			}
		} while(!terminated);
		if(!terminated) {
			return EnemyStageDataResult::truncated_script;
		}
		decoded.templates[i].script_index =
			static_cast<std::uint32_t>(script_index);
		decoded.script_lengths[script_index] = static_cast<std::uint8_t>(
			script_length);
		decoded.script_count = static_cast<std::int16_t>(
			static_cast<std::size_t>(decoded.script_count) + 1U);
	}
	decoded.template_count = template_count;

	if(!has_bytes(bytes, cursor, 2U)) {
		return EnemyStageDataResult::invalid_spawn_row_count;
	}
	const auto spawn_row_count = signed_word(word_at(bytes, cursor));
	cursor += 2U;
	if(spawn_row_count < 0) {
		return EnemyStageDataResult::invalid_spawn_row_count;
	}
	auto row_count = static_cast<std::size_t>(spawn_row_count);
	const auto available_spawn_bytes = payload_end - cursor;
	if(row_count > available_spawn_bytes / spawn_row_bytes) {
		if(tail_policy != EnemyStageDataTailPolicy::materialize_complete_spawn_rows ||
			available_spawn_bytes % spawn_row_bytes != 0U) {
			return EnemyStageDataResult::truncated_spawn_rows;
		}
		row_count = available_spawn_bytes / spawn_row_bytes;
	}
	try {
		decoded.spawn_rows.resize(row_count);
	} catch(...) {
		return EnemyStageDataResult::allocation_failed;
	}
	for(std::size_t row = 0U; row < row_count; ++row) {
		auto &target = decoded.spawn_rows[row];
		target.column[0U] = signed_word(word_at(bytes, cursor));
		cursor += 2U;
		for(std::size_t column = 1U;
			column < enemy_spawn_column_count; ++column) {
			const auto value = bytes[cursor++];
			target.column[column] = (value == 0xFFU)
				? static_cast<std::int16_t>(-1)
				: static_cast<std::int16_t>(value);
		}
	}
	decoded.spawn_row_count = spawn_row_count;

	out = std::move(decoded);
	return EnemyStageDataResult::completed;
}

} // namespace th02::core
