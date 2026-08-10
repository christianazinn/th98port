#include "rpy_input.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "bytes.hpp"

namespace th03::formats::detail {
namespace {

Error error(
	ErrorCode code,
	std::string_view format_name,
	std::string message,
	std::size_t offset = 0U
)
{
	return Error{
		code,
		std::string(format_name) + " " + std::move(message),
		offset,
	};
}

bool range_fits(std::size_t offset, std::size_t size, std::size_t total)
{
	return offset <= total && size <= (total - offset);
}

} // namespace

std::expected<std::vector<replay::SessionRecord>, Error> decode_rpy_input(
	std::span<const std::byte> bytes,
	std::size_t input_offset,
	std::size_t input_size,
	std::uint32_t sample_count,
	std::string_view format_name,
	RpyInputTermination termination
)
{
	if(!range_fits(input_offset, input_size, bytes.size())) {
		return std::unexpected(error(
			ErrorCode::truncated, format_name,
			"input stream extends past end of file", input_offset
		));
	}
	if(
		static_cast<std::uint64_t>(sample_count) >
		static_cast<std::uint64_t>(input_size) * 64U
	) {
		return std::unexpected(error(
			ErrorCode::count_mismatch, format_name,
			"sample count cannot fit its RLE input stream"
		));
	}

	std::vector<replay::SessionRecord> records;
	records.reserve(static_cast<std::size_t>(sample_count) + 32U);
	auto cursor = input_offset;
	const auto end = cursor + input_size;
	std::uint16_t p1 = 0U;
	std::uint16_t p2 = 0U;
	std::uint16_t system = 0U;
	std::uint8_t charge = 0U;
	std::uint32_t frame = 0U;
	auto expected_process = replay::ProcessId::main;
	bool keyframed = false;
	bool last_was_control = false;

	while(cursor < end) {
		const auto packet_offset = cursor;
		const auto tag = detail::u8(bytes, cursor++);
		const auto phase = static_cast<std::uint8_t>((tag >> 6U) & 0x03U);
		const auto run = static_cast<std::uint8_t>((tag & 0x3FU) + 1U);
		if(phase == 2U) {
			if(cursor >= end) {
				return std::unexpected(error(
					ErrorCode::truncated, format_name,
					"control packet is truncated", packet_offset
				));
			}
			if(detail::u8(bytes, cursor++) != 0xA5U) {
				return std::unexpected(error(
					ErrorCode::malformed_stream, format_name,
					"control packet lacks marker", packet_offset
				));
			}
			replay::ProcessId process{};
			if(tag == 0x80U) {
				process = replay::ProcessId::main;
			} else if(tag == 0x81U) {
				process = replay::ProcessId::mainl;
			} else {
				return std::unexpected(error(
					ErrorCode::malformed_stream, format_name,
					"control packet has an unknown tag", packet_offset
				));
			}
			if(process != expected_process) {
				return std::unexpected(error(
					ErrorCode::malformed_stream, format_name,
					"process controls do not alternate from MAIN", packet_offset
				));
			}
			records.emplace_back(replay::ProcessBoundary{frame, process});
			expected_process = (process == replay::ProcessId::main) ?
				replay::ProcessId::mainl : replay::ProcessId::main;
			keyframed = false;
			last_was_control = true;
			p1 = 0U;
			p2 = 0U;
			system = 0U;
			charge = 0U;
			continue;
		}
		if(phase > 1U) {
			return std::unexpected(error(
				ErrorCode::malformed_stream, format_name,
				"packet uses an unsupported input phase", packet_offset
			));
		}
		if(cursor >= end) {
			return std::unexpected(error(
				ErrorCode::truncated, format_name,
				"packet header is truncated", packet_offset
			));
		}
		const auto change = detail::u8(bytes, cursor++);
		if((change & 0xF0U) != 0U) {
			return std::unexpected(error(
				ErrorCode::malformed_stream, format_name,
				"packet uses an unknown change bit", packet_offset + 1U
			));
		}
		if(!keyframed && (change & 0x0FU) != 0x0FU) {
			return std::unexpected(error(
				ErrorCode::malformed_stream, format_name,
				"first packet after a process boundary is not a full keyframe",
				packet_offset + 1U
			));
		}
		auto read_changed_u16 = [&, end](
			std::uint8_t bit, std::uint16_t &target
		) -> bool {
			if((change & bit) == 0U) {
				return true;
			}
			if(!range_fits(cursor, 2U, end)) {
				return false;
			}
			target = detail::u16(bytes, cursor);
			cursor += 2U;
			return true;
		};
		if(
			!read_changed_u16(0x01U, p1) ||
			!read_changed_u16(0x02U, p2) ||
			!read_changed_u16(0x04U, system)
		) {
			return std::unexpected(error(
				ErrorCode::truncated, format_name,
				"packet values extend past the input stream", packet_offset
			));
		}
		if((change & 0x08U) != 0U) {
			if(cursor >= end) {
				return std::unexpected(error(
					ErrorCode::truncated, format_name,
					"Charge value extends past the input stream", packet_offset
				));
			}
			charge = detail::u8(bytes, cursor++);
		}
		if(charge > 3U) {
			return std::unexpected(error(
				ErrorCode::malformed_stream, format_name,
				"Charge mask uses undefined bits", packet_offset
			));
		}
		if(static_cast<std::uint64_t>(frame) + run > sample_count) {
			return std::unexpected(error(
				ErrorCode::count_mismatch, format_name,
				"packet expands past sample count", packet_offset
			));
		}
		keyframed = true;
		last_was_control = false;
		for(std::uint8_t count = 0U; count < run; ++count) {
			replay::InputSample sample{};
			sample.frame_index = frame++;
			sample.phase = (phase == 0U) ?
				replay::InputPhase::gameplay : replay::InputPhase::interstitial;
			sample.input = CoreInput{p1, p2, system, charge};
			records.emplace_back(std::move(sample));
		}
	}

	if(frame != sample_count) {
		return std::unexpected(error(
			ErrorCode::count_mismatch, format_name,
			"expanded sample count disagrees with header"
		));
	}
	if(cursor != end) {
		return std::unexpected(error(
			ErrorCode::trailing_data, format_name,
			"decoder did not consume the complete input stream", cursor
		));
	}
	if(
		!last_was_control &&
		termination == RpyInputTermination::require_process_control
	) {
		return std::unexpected(error(
			ErrorCode::malformed_stream, format_name,
			"finalized input stream does not end in a process control", end
		));
	}
	return records;
}

std::expected<void, Error> validate_rpy_checkpoint_cursors(
	std::span<const std::byte> bytes,
	std::size_t input_offset,
	std::size_t input_size,
	std::span<const RpyCheckpointCursor> checkpoints,
	std::string_view format_name
)
{
	if(!range_fits(input_offset, input_size, bytes.size())) {
		return std::unexpected(error(
			ErrorCode::truncated,
			format_name,
			"input stream extends past end of file",
			input_offset
		));
	}
	std::vector<bool> found(checkpoints.size(), false);
	auto cursor = input_offset;
	const auto end = input_offset + input_size;
	std::uint64_t sample = 0U;
	while(true) {
		const auto relative = cursor - input_offset;
		for(std::size_t index = 0U; index < checkpoints.size(); ++index) {
			if(checkpoints[index].input_size != relative) {
				continue;
			}
			if(static_cast<std::uint64_t>(checkpoints[index].sample_count) != sample) {
				return std::unexpected(error(
					ErrorCode::count_mismatch,
					format_name,
					"checkpoint sample and byte cursors disagree",
					cursor
				));
			}
			if(cursor < end) {
				if(!range_fits(cursor, 2U, end)) {
					return std::unexpected(error(
						ErrorCode::truncated,
						format_name,
						"packet header after checkpoint is truncated",
						cursor
					));
				}
				const auto phase = static_cast<std::uint8_t>(u8(bytes, cursor) >> 6U);
				if(phase > 1U || (u8(bytes, cursor + 1U) & 0x0FU) != 0x0FU) {
					return std::unexpected(error(
						ErrorCode::malformed_stream,
						format_name,
						"checkpoint is not followed by a full input keyframe",
						cursor
					));
				}
			}
			found[index] = true;
		}
		if(cursor == end) {
			break;
		}
		if(!range_fits(cursor, 2U, end)) {
			return std::unexpected(error(
				ErrorCode::truncated,
				format_name,
				"packet header is truncated while validating checkpoints",
				cursor
			));
		}
		const auto tag = u8(bytes, cursor++);
		const auto change = u8(bytes, cursor++);
		const auto phase = static_cast<std::uint8_t>(tag >> 6U);
		if(phase == 2U) {
			continue;
		}
		std::size_t field_size = 0U;
		field_size += ((change & 0x01U) != 0U) ? 2U : 0U;
		field_size += ((change & 0x02U) != 0U) ? 2U : 0U;
		field_size += ((change & 0x04U) != 0U) ? 2U : 0U;
		field_size += ((change & 0x08U) != 0U) ? 1U : 0U;
		if(!range_fits(cursor, field_size, end)) {
			return std::unexpected(error(
				ErrorCode::truncated,
				format_name,
				"packet values are truncated while validating checkpoints",
				cursor
			));
		}
		cursor += field_size;
		sample += static_cast<std::uint64_t>((tag & 0x3FU) + 1U);
	}
	for(std::size_t index = 0U; index < found.size(); ++index) {
		if(!found[index]) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				format_name,
				"checkpoint byte cursor is not a packet boundary",
				checkpoints[index].input_size
			));
		}
	}
	return {};
}

} // namespace th03::formats::detail
