#pragma once

#include "th02/core/enemy.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace th02::core {

// These are the source storage ceilings, not claims that a stage uses every
// slot. The native parser refuses a count that would overrun the corresponding
// source array instead of reproducing the loader's unchecked DOS write.
inline constexpr std::size_t stage_map_length_max = 320U;
inline constexpr std::size_t stage_template_capacity = 128U;
inline constexpr std::size_t stage_script_capacity = 40U;
inline constexpr std::size_t stage_script_size = 64U;
inline constexpr std::size_t stage_template_file_size = 0x1CU;

// Caller-supplied master.lib sprite metrics. Each entry is the raw
// super_patsize word: the source derives width as its high byte times
// BYTE_DOTS (8) and height from its low byte.
using EnemySpriteSizeWord = std::uint16_t;

// Owned result of the byte-loader boundary. The source far script pointers are
// represented by script_index, so this state has no host pointers or mutable
// global aliases. spawn_rows is the existing 49-column EnemySpawnRow record.
struct EnemyStageData {
	std::array<std::uint8_t, stage_map_length_max> map{};
	std::int16_t map_length{};
	std::array<EnemyTemplate, stage_template_capacity> templates{};
	std::int16_t template_count{};
	std::array<std::array<std::uint8_t, stage_script_size>,
		stage_script_capacity> scripts{};
	// A length accompanies each copied script so zero-filled native storage
	// beyond the source terminator is never mistaken for source bytes.
	std::array<std::uint8_t, stage_script_capacity> script_lengths{};
	std::int16_t script_count{};
	std::vector<EnemySpawnRow> spawn_rows{};
	std::int16_t spawn_row_count{};
	bool operator==(const EnemyStageData &) const = default;
};

enum class EnemyStageDataResult : std::uint8_t {
	completed,
	invalid_file_size,
	invalid_map_length,
	truncated_map,
	invalid_template_count,
	truncated_template,
	invalid_sprite_index,
	script_pool_full,
	truncated_script,
	script_too_long,
	truncated_script_reuse,
	invalid_script_reference,
	invalid_spawn_row_count,
	truncated_spawn_rows,
	allocation_failed,
};

// Decode the bytes read by th02/main/enemy/enemy.cpp::enemy_stagedata_load.
// `bytes` includes the four-byte little-endian payload size that precedes the
// data read by the original. The declared size must equal bytes.size()-4.
// Rank values follow th01/rank.h: 0 doubles autofire, 2 and 3 halve it, and
// all other values preserve the source word. Fresh scripts follow the source's
// do-copy-then-peek loop: they contain at least one body byte and a following
// FF terminator, while FE records consume the marker, index, and one skipped
// byte. On any refusal, `out` remains unchanged. Bytes after the declared
// records are harmless source-compatible trailing payload and are not
// interpreted as another format.
[[nodiscard]] EnemyStageDataResult enemy_stage_data_decode(
	std::span<const std::uint8_t> bytes,
	std::span<const EnemySpriteSizeWord> sprite_sizes,
	std::int8_t rank,
	EnemyStageData &out
) noexcept;

} // namespace th02::core
