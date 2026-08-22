#include "rpy_input.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
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

std::expected<EncodedRpyInput, Error> encode_rpy_input(
	std::span<const replay::SessionRecord> records,
	std::uint32_t sample_count,
	std::span<const std::uint32_t> checkpoint_samples,
	std::string_view format_name
)
{
	if(checkpoint_samples.empty() || checkpoint_samples.front() != 0U) {
		return std::unexpected(error(
			ErrorCode::invalid_value, format_name,
			"checkpoint sample directory lacks its zero anchor"
		));
	}
	for(std::size_t index = 0U; index < checkpoint_samples.size(); ++index) {
		if(checkpoint_samples[index] > sample_count ||
			(index > 0U && checkpoint_samples[index] <= checkpoint_samples[index - 1U])) {
			return std::unexpected(error(
				ErrorCode::invalid_value, format_name,
				"checkpoint sample cursors are not strictly increasing"
			));
		}
	}

	struct PendingRun {
		replay::InputPhase phase = replay::InputPhase::gameplay;
		CoreInput input{};
		std::uint8_t count = 0U;
	};
	EncodedRpyInput result{};
	result.bytes.reserve(records.size() * 2U);
	result.checkpoints.resize(checkpoint_samples.size());
	result.checkpoints[0] = {};
	std::size_t next_checkpoint = 1U;
	std::uint32_t samples = 0U;
	CoreInput previous{};
	bool full_keyframe = true;
	std::optional<PendingRun> pending{};

	const auto append_u16 = [&](std::uint16_t value) {
		result.bytes.push_back(static_cast<std::byte>(value));
		result.bytes.push_back(static_cast<std::byte>(value >> 8U));
	};
	const auto flush = [&]() -> std::expected<void, Error> {
		if(!pending) {
			return {};
		}
		if(pending->count == 0U || pending->count > 64U ||
			pending->input.charge_mask > 3U) {
			return std::unexpected(error(
				ErrorCode::invalid_value, format_name,
				"input run is not representable in V14 RLE"
			));
		}
		const auto phase = static_cast<std::uint8_t>(pending->phase);
		if(phase > 1U) {
			return std::unexpected(error(
				ErrorCode::invalid_value, format_name,
				"input run has an unknown phase"
			));
		}
		result.bytes.push_back(static_cast<std::byte>(
			(phase << 6U) | (pending->count - 1U)
		));
		std::uint8_t change = full_keyframe ? 0x0FU : 0U;
		if(pending->input.p1 != previous.p1) {
			change |= 0x01U;
		}
		if(pending->input.p2 != previous.p2) {
			change |= 0x02U;
		}
		if(pending->input.system != previous.system) {
			change |= 0x04U;
		}
		if(pending->input.charge_mask != previous.charge_mask) {
			change |= 0x08U;
		}
		result.bytes.push_back(static_cast<std::byte>(change));
		if((change & 0x01U) != 0U) {
			append_u16(pending->input.p1);
		}
		if((change & 0x02U) != 0U) {
			append_u16(pending->input.p2);
		}
		if((change & 0x04U) != 0U) {
			append_u16(pending->input.system);
		}
		if((change & 0x08U) != 0U) {
			result.bytes.push_back(
				static_cast<std::byte>(pending->input.charge_mask)
			);
		}
		previous = pending->input;
		full_keyframe = false;
		pending.reset();
		return {};
	};
	const auto mark_checkpoint = [&]() -> std::expected<void, Error> {
		if(auto flushed = flush(); !flushed) {
			return flushed;
		}
		if(result.bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
			return std::unexpected(error(
				ErrorCode::invalid_value, format_name,
				"compressed input cursor exceeds V14"
			));
		}
		result.checkpoints[next_checkpoint] = {
			.sample_count = samples,
			.input_size = static_cast<std::uint32_t>(result.bytes.size()),
		};
		++next_checkpoint;
		full_keyframe = true;
		return {};
	};

	for(const auto &record : records) {
		if(const auto *boundary = std::get_if<replay::ProcessBoundary>(&record)) {
			if(boundary->sample_index != samples) {
				return std::unexpected(error(
					ErrorCode::count_mismatch, format_name,
					"process boundary disagrees with expanded sample cursor"
				));
			}
			if(auto flushed = flush(); !flushed) {
				return std::unexpected(flushed.error());
			}
			const auto tag = (boundary->completed_process == replay::ProcessId::main)
				? 0x80U : ((boundary->completed_process == replay::ProcessId::mainl)
					? 0x81U : 0xFFU);
			if(tag == 0xFFU) {
				return std::unexpected(error(
					ErrorCode::invalid_value, format_name,
					"process boundary has an unknown process"
				));
			}
			result.bytes.push_back(static_cast<std::byte>(tag));
			result.bytes.push_back(std::byte{0xA5U});
			previous = {};
			full_keyframe = true;
			continue;
		}

		const auto *sample = std::get_if<replay::InputSample>(&record);
		if(!sample || sample->frame_index != samples) {
			return std::unexpected(error(
				ErrorCode::count_mismatch, format_name,
				"input record disagrees with expanded sample cursor"
			));
		}
		while(next_checkpoint < checkpoint_samples.size() &&
			checkpoint_samples[next_checkpoint] == samples) {
			if(auto marked = mark_checkpoint(); !marked) {
				return std::unexpected(marked.error());
			}
		}
		if(next_checkpoint < checkpoint_samples.size() &&
			checkpoint_samples[next_checkpoint] < samples) {
			return std::unexpected(error(
				ErrorCode::count_mismatch, format_name,
				"checkpoint sample was not a record boundary"
			));
		}
		const bool same = pending && pending->phase == sample->phase &&
			pending->input.p1 == sample->input.p1 &&
			pending->input.p2 == sample->input.p2 &&
			pending->input.system == sample->input.system &&
			pending->input.charge_mask == sample->input.charge_mask &&
			pending->count < 64U;
		if(same) {
			++pending->count;
		} else {
			if(auto flushed = flush(); !flushed) {
				return std::unexpected(flushed.error());
			}
			pending = PendingRun{
				.phase = sample->phase,
				.input = sample->input,
				.count = 1U,
			};
		}
		++samples;
	}
	if(auto flushed = flush(); !flushed) {
		return std::unexpected(flushed.error());
	}
	while(next_checkpoint < checkpoint_samples.size() &&
		checkpoint_samples[next_checkpoint] == samples) {
		if(auto marked = mark_checkpoint(); !marked) {
			return std::unexpected(marked.error());
		}
	}
	if(samples != sample_count || next_checkpoint != checkpoint_samples.size()) {
		return std::unexpected(error(
			ErrorCode::count_mismatch, format_name,
			"expanded input or checkpoint count disagrees with document"
		));
	}
	return result;
}

} // namespace th03::formats::detail
