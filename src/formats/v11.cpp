#include "th03/formats/v11.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>

#include "bytes.hpp"

namespace th03::formats {
namespace {

constexpr std::size_t v11_header_size = 400;
constexpr std::size_t v11_checkpoint_size = 994;
constexpr std::size_t v11_checkpoint_prefix_size = 12;
constexpr std::size_t v11_snapshot_size = 982;

Error error(ErrorCode code, std::string message, std::size_t offset = 0)
{
	return Error{code, std::move(message), offset};
}

bool range_fits(std::size_t offset, std::size_t size, std::size_t total)
{
	return offset <= total && size <= (total - offset);
}

template <std::size_t Size>
void copy_bytes(
	std::array<std::uint8_t, Size> &destination,
	std::span<const std::byte> source,
	std::size_t offset
)
{
	for(std::size_t index = 0; index < Size; ++index) {
		destination[index] = detail::u8(source, offset + index);
	}
}

GameInit parse_snapshot(std::span<const std::byte> bytes, std::size_t snapshot)
{
	GameInit init{};
	init.resident_rand = detail::i32(bytes, snapshot + 0U);
	init.random_seed = detail::u32(bytes, snapshot + 4U);
	init.rank = detail::u8(bytes, snapshot + 8U);
	init.key_mode = detail::u8(bytes, snapshot + 9U);
	init.game_mode = detail::u8(bytes, snapshot + 10U);
	init.story_stage = detail::u8(bytes, snapshot + 11U);
	init.story_lives = detail::u8(bytes, snapshot + 12U);
	init.remaining_credits = detail::u8(bytes, snapshot + 13U);
	init.skill = detail::u8(bytes, snapshot + 14U);
	init.demo_number = detail::u8(bytes, snapshot + 15U);
	init.winner = detail::i8(bytes, snapshot + 16U);
	init.show_score_menu = detail::u8(bytes, snapshot + 17U);
	init.fast_op_animation = detail::u8(bytes, snapshot + 18U);
	for(std::size_t player = 0; player < 2U; ++player) {
		init.is_cpu[player] = detail::u8(bytes, snapshot + 19U + player);
		init.paletted_character[player] = detail::i8(bytes, snapshot + 21U + player);
		for(std::size_t digit = 0; digit < 8U; ++digit) {
			init.last_score[player][digit] = detail::u8(
				bytes, snapshot + 32U + (player * 8U) + digit
			);
		}
	}
	for(std::size_t stage = 0; stage < 9U; ++stage) {
		init.story_opponents[stage] = detail::i8(bytes, snapshot + 23U + stage);
	}
	init.autofire_mask = detail::u8(bytes, snapshot + 53U);

	RuntimeSnapshot runtime{};
	runtime.random_seed = init.random_seed;
	copy_bytes(runtime.randring, bytes, snapshot + 55U);
	runtime.randring_cursor = detail::u8(bytes, snapshot + 48U);
	copy_bytes(runtime.formation_type_ring, bytes, snapshot + 311U);
	copy_bytes(runtime.formation_position_ring, bytes, snapshot + 567U);
	for(std::size_t player = 0; player < 2U; ++player) {
		runtime.formation_cursor[player] = detail::u8(bytes, snapshot + 49U + player);
		copy_bytes(
			runtime.cpu_charge_ring[player],
			bytes,
			snapshot + 823U + (player * 64U)
		);
		runtime.cpu_charge_cursor[player] = detail::u8(bytes, snapshot + 51U + player);
		auto &target = runtime.players[player];
		target.center_x = exact::Subpixel::from_raw_bits(
			detail::u16(bytes, snapshot + 951U + (player * 2U))
		);
		target.center_y = exact::Subpixel::from_raw_bits(
			detail::u16(bytes, snapshot + 955U + (player * 2U))
		);
		target.halfhearts = detail::u8(bytes, snapshot + 959U + player);
		target.invincibility_time = detail::u8(bytes, snapshot + 961U + player);
		target.gauge_charge_speed = detail::u8(bytes, snapshot + 963U + player);
		target.gauge_charged = detail::u16(bytes, snapshot + 965U + (player * 2U));
		target.gauge_avail = detail::u16(bytes, snapshot + 969U + (player * 2U));
		target.bombs = detail::u8(bytes, snapshot + 973U + player);
		target.shot_active = detail::u8(bytes, snapshot + 975U + player);
		target.cpu_frame = detail::u16(bytes, snapshot + 977U + (player * 2U));
	}
	init.runtime_snapshot = std::move(runtime);
	return init;
}

} // namespace

std::expected<replay::ReplaySession, Error> parse_v11_replay(
	std::span<const std::byte> bytes
)
{
	constexpr std::array<std::uint8_t, 8> magic{
		'T', '3', 'R', 'P', 'L', 'Y', '1', '1',
	};
	if(bytes.size() < v11_header_size) {
		return std::unexpected(error(ErrorCode::truncated, "V11 replay is shorter than its 400-byte header"));
	}
	if(!detail::has_magic(bytes, magic)) {
		return std::unexpected(error(ErrorCode::bad_magic, "replay magic is not T3RPLY11"));
	}
	if(detail::u16(bytes, 8U) != replay_v11_version) {
		return std::unexpected(error(ErrorCode::unsupported_version, "replay magic/version mismatch", 8U));
	}
	if(detail::u16(bytes, 10U) != v11_header_size || detail::u16(bytes, 12U) != 0U) {
		return std::unexpected(error(ErrorCode::unsupported_layout, "unsupported V11 header or sample layout", 10U));
	}
	const auto flags = detail::u16(bytes, 14U);
	if((flags & 0x0001U) == 0U) {
		return std::unexpected(error(ErrorCode::unsupported_layout, "V11 replay is not an RLE input stream", 14U));
	}
	if((flags & 0x0004U) != 0U) {
		return std::unexpected(error(ErrorCode::unsupported_version, "V11 Practice replay is outside the calibrated oracle"));
	}
	if(detail::u8(bytes, 18U) != 1U) {
		return std::unexpected(error(ErrorCode::unsupported_version, "only V11 Story replays are calibrated", 18U));
	}
	if((detail::u16(bytes, 58U) & 0x0001U) == 0U) {
		return std::unexpected(error(ErrorCode::invalid_value, "V11 summary-valid flag is clear", 58U));
	}
	const auto autofire = detail::u8(bytes, 117U);
	if((autofire & 0xFCU) != 0U) {
		return std::unexpected(error(ErrorCode::invalid_value, "V11 Autofire mask contains undefined bits", 117U));
	}

	const auto sample_count = detail::u32(bytes, 26U);
	const auto input_offset = detail::u32(bytes, 42U);
	const auto input_size = detail::u32(bytes, 46U);
	const auto snapshot_offset = detail::u32(bytes, 50U);
	const auto snapshot_size = detail::u32(bytes, 54U);
	if(snapshot_offset != v11_header_size || snapshot_size != v11_checkpoint_size) {
		return std::unexpected(error(ErrorCode::unsupported_layout, "V11 checkpoint layout is not 400/994", 50U));
	}
	if(!range_fits(input_offset, input_size, bytes.size())) {
		return std::unexpected(error(ErrorCode::truncated, "V11 input stream extends past end of file", input_offset));
	}
	if(static_cast<std::uint64_t>(sample_count) > static_cast<std::uint64_t>(input_size) * 64U) {
		return std::unexpected(error(ErrorCode::count_mismatch, "V11 sample count cannot fit its RLE input stream"));
	}

	const auto story_stage = detail::u8(bytes, 23U);
	const auto stage_reached_count = detail::u8(bytes, 67U);
	if(story_stage >= 9U || story_stage >= stage_reached_count) {
		return std::unexpected(error(ErrorCode::invalid_value, "V11 selected Story stage has no populated checkpoint", 23U));
	}
	const auto checkpoint = static_cast<std::size_t>(snapshot_offset) +
		(static_cast<std::size_t>(story_stage) * v11_checkpoint_size);
	if(!range_fits(checkpoint, v11_checkpoint_size, input_offset)) {
		return std::unexpected(error(ErrorCode::truncated, "V11 selected checkpoint overlaps the input stream", checkpoint));
	}
	if(
		detail::u32(bytes, checkpoint + 0U) != 0U ||
		detail::u32(bytes, checkpoint + 4U) != 0U ||
		detail::u32(bytes, checkpoint + 8U) != 0U
	) {
		return std::unexpected(error(ErrorCode::invalid_value, "V11 oracle checkpoint does not begin at cursor zero", checkpoint));
	}
	const auto snapshot = checkpoint + v11_checkpoint_prefix_size;
	if(!range_fits(snapshot, v11_snapshot_size, bytes.size())) {
		return std::unexpected(error(ErrorCode::truncated, "V11 snapshot extends past end of file", snapshot));
	}

	auto init = parse_snapshot(bytes, snapshot);
	struct Check {
		std::uint32_t header;
		std::uint32_t snapshot;
		const char *name;
	};
	const std::array checks{
		Check{detail::u32(bytes, 34U), static_cast<std::uint32_t>(init.resident_rand), "resident_rand"},
		Check{detail::u32(bytes, 38U), init.random_seed, "random_seed"},
		Check{detail::u8(bytes, 18U), init.game_mode, "game_mode"},
		Check{detail::u8(bytes, 19U), init.rank, "rank"},
		Check{detail::u8(bytes, 20U), init.key_mode, "key_mode"},
		Check{detail::u8(bytes, 21U), std::bit_cast<std::uint8_t>(init.paletted_character[0]), "playchar_p1"},
		Check{detail::u8(bytes, 22U), std::bit_cast<std::uint8_t>(init.paletted_character[1]), "playchar_p2"},
		Check{story_stage, init.story_stage, "story_stage"},
		Check{detail::u8(bytes, 24U), init.is_cpu[0], "is_cpu_p1"},
		Check{detail::u8(bytes, 25U), init.is_cpu[1], "is_cpu_p2"},
		Check{autofire, init.autofire_mask, "autofire"},
	};
	for(const auto &check : checks) {
		if(check.header != check.snapshot) {
			return std::unexpected(error(ErrorCode::invalid_value, std::string("V11 header/checkpoint mismatch: ") + check.name));
		}
	}

	replay::ReplaySession session{};
	session.source_version = replay_v11_version;
	session.init = std::move(init);
	session.sample_count = sample_count;
	session.records.reserve(static_cast<std::size_t>(sample_count) + 32U);

	auto cursor = static_cast<std::size_t>(input_offset);
	const auto end = cursor + static_cast<std::size_t>(input_size);
	std::uint16_t p1 = 0;
	std::uint16_t p2 = 0;
	std::uint16_t system = 0;
	std::uint8_t charge = 0;
	std::uint32_t frame = 0;
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
				return std::unexpected(error(ErrorCode::truncated, "truncated V11 control packet", packet_offset));
			}
			if(detail::u8(bytes, cursor++) != 0xA5U) {
				return std::unexpected(error(ErrorCode::malformed_stream, "V11 control packet lacks marker", packet_offset));
			}
			replay::ProcessId process{};
			if(tag == 0x80U) {
				process = replay::ProcessId::main;
			} else if(tag == 0x81U) {
				process = replay::ProcessId::mainl;
			} else {
				return std::unexpected(error(ErrorCode::malformed_stream, "unknown V11 control tag", packet_offset));
			}
			if(process != expected_process) {
				return std::unexpected(error(ErrorCode::malformed_stream, "V11 controls do not alternate from MAIN", packet_offset));
			}
			session.records.emplace_back(replay::ProcessBoundary{frame, process});
			expected_process = (process == replay::ProcessId::main) ?
				replay::ProcessId::mainl : replay::ProcessId::main;
			keyframed = false;
			last_was_control = true;
			p1 = 0;
			p2 = 0;
			system = 0;
			charge = 0;
			continue;
		}
		if(phase > 1U) {
			return std::unexpected(error(ErrorCode::malformed_stream, "unsupported V11 packet phase", packet_offset));
		}
		if(cursor >= end) {
			return std::unexpected(error(ErrorCode::truncated, "truncated V11 packet header", packet_offset));
		}
		const auto change = detail::u8(bytes, cursor++);
		if((change & 0xF0U) != 0U) {
			return std::unexpected(error(ErrorCode::malformed_stream, "V11 packet uses an unknown change bit", packet_offset + 1U));
		}
		if(!keyframed && (change & 0x0FU) != 0x0FU) {
			return std::unexpected(error(ErrorCode::malformed_stream, "first V11 packet after a process boundary is not a full keyframe", packet_offset + 1U));
		}
		auto read_changed_u16 = [&](std::uint8_t bit, std::uint16_t &target) -> bool {
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
			return std::unexpected(error(ErrorCode::truncated, "V11 packet values extend past the input stream", packet_offset));
		}
		if((change & 0x08U) != 0U) {
			if(cursor >= end) {
				return std::unexpected(error(ErrorCode::truncated, "V11 Charge value extends past the input stream", packet_offset));
			}
			charge = detail::u8(bytes, cursor++);
		}
		if(charge > 3U) {
			return std::unexpected(error(ErrorCode::malformed_stream, "V11 Charge mask uses undefined bits", packet_offset));
		}
		if(static_cast<std::uint64_t>(frame) + run > sample_count) {
			return std::unexpected(error(ErrorCode::count_mismatch, "V11 packet expands past sample_count", packet_offset));
		}
		keyframed = true;
		last_was_control = false;
		for(std::uint8_t count = 0; count < run; ++count) {
			replay::InputSample sample{};
			sample.frame_index = frame++;
			sample.phase = (phase == 0U) ?
				replay::InputPhase::gameplay : replay::InputPhase::interstitial;
			sample.input = CoreInput{p1, p2, system, charge};
			session.records.emplace_back(std::move(sample));
		}
	}

	if(frame != sample_count) {
		return std::unexpected(error(ErrorCode::count_mismatch, "V11 expanded sample count disagrees with header"));
	}
	if(cursor != end) {
		return std::unexpected(error(ErrorCode::trailing_data, "V11 decoder did not consume the complete input stream", cursor));
	}
	if(!last_was_control) {
		return std::unexpected(error(ErrorCode::malformed_stream, "V11 finalized input stream does not end in a process control", end));
	}
	return session;
}

} // namespace th03::formats
