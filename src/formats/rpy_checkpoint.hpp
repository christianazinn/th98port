#ifndef TH03_FORMATS_RPY_CHECKPOINT_HPP
#define TH03_FORMATS_RPY_CHECKPOINT_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>

#include "th03/core/game.hpp"
#include "th03/error.hpp"

namespace th03::formats::detail {

inline constexpr std::size_t compact_checkpoint_size = 289U;
inline constexpr std::size_t compact_checkpoint_prefix_size = 12U;
inline constexpr std::size_t compact_snapshot_size = 84U;
inline constexpr std::size_t compact_round_state_size = 30U;
inline constexpr std::size_t compact_round_carry_size = 163U;

struct CompactCheckpoint {
	std::uint32_t sample_count = 0U;
	std::uint32_t global_frame = 0U;
	std::uint32_t input_size = 0U;
	GameInit init{};
};

bool range_fits(std::size_t offset, std::size_t size, std::size_t total) noexcept;

std::expected<CompactCheckpoint, Error> parse_compact_checkpoint(
	std::span<const std::byte> bytes,
	std::size_t offset,
	std::string_view format_name
);

std::expected<void, Error> write_compact_checkpoint(
	std::span<std::byte> bytes,
	std::size_t offset,
	const CompactCheckpoint &checkpoint,
	std::string_view format_name
);

} // namespace th03::formats::detail

#endif
