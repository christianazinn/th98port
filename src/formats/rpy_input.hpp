#ifndef TH03_FORMATS_RPY_INPUT_HPP
#define TH03_FORMATS_RPY_INPUT_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

#include "th03/error.hpp"
#include "th03/replay/session.hpp"

namespace th03::formats::detail {

struct RpyCheckpointCursor {
	std::uint32_t sample_count = 0U;
	std::uint32_t input_size = 0U;
};

enum class RpyInputTermination {
	require_process_control,
	allow_input_packet,
};

std::expected<std::vector<replay::SessionRecord>, Error> decode_rpy_input(
	std::span<const std::byte> bytes,
	std::size_t input_offset,
	std::size_t input_size,
	std::uint32_t sample_count,
	std::string_view format_name,
	RpyInputTermination termination =
		RpyInputTermination::require_process_control
);

// V13 seals the current RLE packet before capturing a checkpoint. Each stored
// byte cursor must therefore land at a packet boundary whose expanded sample
// cursor agrees exactly, with a full keyframe following when input remains.
std::expected<void, Error> validate_rpy_checkpoint_cursors(
	std::span<const std::byte> bytes,
	std::size_t input_offset,
	std::size_t input_size,
	std::span<const RpyCheckpointCursor> checkpoints,
	std::string_view format_name
);

} // namespace th03::formats::detail

#endif
