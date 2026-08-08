#include "th03/formats/t3case.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <string>
#include <utility>

#include "bytes.hpp"

namespace th03::formats {
namespace {

constexpr std::uint16_t flag_advisory_positions = 0x0001U;
constexpr std::uint16_t flag_charge_in_control = 0x0002U;
constexpr std::uint16_t flag_snapshot = 0x0008U;
constexpr std::uint16_t known_flags =
	flag_advisory_positions | flag_charge_in_control | flag_snapshot;

Error error(ErrorCode code, std::string message, std::size_t offset = 0)
{
	return Error{code, std::move(message), offset};
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

GameInit parse_init(
	std::span<const std::byte> bytes,
	bool has_snapshot
)
{
	constexpr std::size_t startup = t3case_header_size;
	GameInit init{};
	init.resident_rand = detail::i32(bytes, startup + 0U);
	init.random_seed = detail::u32(bytes, startup + 4U);
	init.game_mode = detail::u8(bytes, startup + 8U);
	init.rank = detail::u8(bytes, startup + 9U);
	init.key_mode = detail::u8(bytes, startup + 10U);
	init.story_stage = detail::u8(bytes, startup + 11U);
	init.story_lives = detail::u8(bytes, startup + 12U);
	init.remaining_credits = detail::u8(bytes, startup + 13U);
	init.skill = detail::u8(bytes, startup + 14U);
	init.demo_number = detail::u8(bytes, startup + 15U);
	for(std::size_t player = 0; player < 2U; ++player) {
		init.is_cpu[player] = detail::u8(bytes, startup + 16U + player);
		init.paletted_character[player] = detail::i8(bytes, startup + 18U + player);
		for(std::size_t digit = 0; digit < 8U; ++digit) {
			init.last_score[player][digit] = detail::u8(
				bytes, startup + 32U + (player * 8U) + digit
			);
		}
	}
	for(std::size_t stage = 0; stage < 9U; ++stage) {
		init.story_opponents[stage] = detail::i8(bytes, startup + 20U + stage);
	}
	init.winner = detail::i8(bytes, startup + 29U);
	init.show_score_menu = detail::u8(bytes, startup + 30U);
	init.fast_op_animation = detail::u8(bytes, startup + 31U);
	init.autofire_mask = detail::u8(bytes, startup + 51U);

	if(has_snapshot) {
		constexpr std::size_t snapshot =
			t3case_header_size + t3case_startup_size;
		RuntimeSnapshot runtime{};
		runtime.random_seed = detail::u32(bytes, snapshot + 0U);
		copy_bytes(runtime.randring, bytes, snapshot + 4U);
		runtime.randring_cursor = detail::u8(bytes, snapshot + 260U);
		copy_bytes(runtime.formation_type_ring, bytes, snapshot + 261U);
		copy_bytes(runtime.formation_position_ring, bytes, snapshot + 517U);
		for(std::size_t player = 0; player < 2U; ++player) {
			runtime.formation_cursor[player] = detail::u8(
				bytes, snapshot + 773U + player
			);
			copy_bytes(
				runtime.cpu_charge_ring[player],
				bytes,
				snapshot + 775U + (player * 64U)
			);
			runtime.cpu_charge_cursor[player] = detail::u8(
				bytes, snapshot + 903U + player
			);
			auto &target = runtime.players[player];
			target.center_x = exact::Subpixel::from_raw_bits(
				detail::u16(bytes, snapshot + 905U + (player * 2U))
			);
			target.center_y = exact::Subpixel::from_raw_bits(
				detail::u16(bytes, snapshot + 909U + (player * 2U))
			);
			target.halfhearts = detail::u8(bytes, snapshot + 913U + player);
			target.invincibility_time = detail::u8(bytes, snapshot + 915U + player);
			target.gauge_charge_speed = detail::u8(bytes, snapshot + 917U + player);
			target.gauge_charged = detail::u16(
				bytes, snapshot + 919U + (player * 2U)
			);
			target.gauge_avail = detail::u16(
				bytes, snapshot + 923U + (player * 2U)
			);
			target.bombs = detail::u8(bytes, snapshot + 927U + player);
			target.shot_active = detail::u8(bytes, snapshot + 929U + player);
			target.cpu_frame = detail::u16(
				bytes, snapshot + 931U + (player * 2U)
			);
		}
		init.runtime_snapshot = std::move(runtime);
	}
	return init;
}

} // namespace

std::uint32_t fnv1a32(std::span<const std::byte> bytes, std::uint32_t seed) noexcept
{
	auto hash = seed;
	for(const auto value : bytes) {
		hash ^= std::to_integer<std::uint8_t>(value);
		hash *= 0x01000193U;
	}
	return hash;
}

std::expected<T3Case, Error> parse_t3case(std::span<const std::byte> bytes)
{
	constexpr std::array<std::uint8_t, 8> magic{
		'T', '3', 'C', 'A', 'S', 'E', '1', 0,
	};
	constexpr std::size_t prefix_size = t3case_header_size + t3case_startup_size;
	if(bytes.size() < prefix_size) {
		return std::unexpected(error(
			ErrorCode::truncated,
			"T3CASE is shorter than its 128-byte header/startup prefix"
		));
	}
	if(!detail::has_magic(bytes, magic)) {
		return std::unexpected(error(ErrorCode::bad_magic, "T3CASE magic is not T3CASE1"));
	}

	T3Case result{};
	auto &header = result.header;
	header.version = detail::u16(bytes, 8U);
	const auto header_size = detail::u16(bytes, 10U);
	const auto startup_size = detail::u16(bytes, 12U);
	const auto record_size = detail::u16(bytes, 14U);
	header.payload_offset = detail::u32(bytes, 16U);
	header.payload_size = detail::u32(bytes, 20U);
	header.sample_count = detail::u32(bytes, 24U);
	header.record_count = detail::u32(bytes, 28U);
	header.source_kind = static_cast<T3CaseSourceKind>(detail::u8(bytes, 32U));
	header.input_semantics = detail::u8(bytes, 33U);
	header.ruleset_id = detail::u8(bytes, 34U);
	header.scenario_id = detail::u8(bytes, 35U);
	header.first_process = static_cast<replay::ProcessId>(detail::u8(bytes, 36U));
	header.producer = detail::u8(bytes, 37U);
	header.flags = detail::u16(bytes, 38U);
	header.case_id = detail::u32(bytes, 40U);
	header.source_digest = detail::u32(bytes, 44U);
	header.source_commit = detail::u32(bytes, 48U);
	header.payload_checksum = detail::u32(bytes, 52U);
	header.header_checksum = detail::u32(bytes, 56U);
	header.total_size = detail::u32(bytes, 60U);

	if(header.version != 1U) {
		return std::unexpected(error(ErrorCode::unsupported_version, "unsupported T3CASE version"));
	}
	if(
		(header_size != t3case_header_size) ||
		(startup_size != t3case_startup_size) ||
		(record_size != t3case_record_size)
	) {
		return std::unexpected(error(ErrorCode::unsupported_layout, "unsupported T3CASE layout"));
	}
	if((header.flags & static_cast<std::uint16_t>(~known_flags)) != 0U) {
		return std::unexpected(error(ErrorCode::invalid_value, "unknown T3CASE flags"));
	}
	const bool has_snapshot = (header.flags & flag_snapshot) != 0U;
	const auto expected_payload_offset = static_cast<std::uint32_t>(
		prefix_size + (has_snapshot ? t3case_v11_snapshot_size : 0U)
	);
	if(header.payload_offset != expected_payload_offset) {
		return std::unexpected(error(ErrorCode::unsupported_layout, "T3CASE payload offset disagrees with flags"));
	}
	if(header.source_kind == T3CaseSourceKind::master_direct) {
		if(has_snapshot) {
			return std::unexpected(error(ErrorCode::invalid_value, "master-direct T3CASE carries a snapshot"));
		}
	} else if(header.source_kind == T3CaseSourceKind::normalized_v11) {
		if(!has_snapshot) {
			return std::unexpected(error(ErrorCode::invalid_value, "normalized V11 T3CASE has no snapshot"));
		}
	} else {
		return std::unexpected(error(ErrorCode::unsupported_version, "unsupported T3CASE source kind"));
	}
	if(header.input_semantics != 1U || header.ruleset_id != 1U) {
		return std::unexpected(error(ErrorCode::unsupported_version, "unsupported T3CASE input semantics or ruleset"));
	}
	if(
		(header.first_process != replay::ProcessId::main) &&
		(header.first_process != replay::ProcessId::mainl)
	) {
		return std::unexpected(error(ErrorCode::invalid_value, "unknown T3CASE first process"));
	}

	const auto expected_payload_size =
		static_cast<std::uint64_t>(header.record_count) * t3case_record_size;
	if(expected_payload_size != header.payload_size) {
		return std::unexpected(error(ErrorCode::count_mismatch, "T3CASE payload size disagrees with record count"));
	}
	if(header.sample_count > header.record_count) {
		return std::unexpected(error(ErrorCode::count_mismatch, "T3CASE sample count exceeds record count"));
	}
	const auto expected_total =
		static_cast<std::uint64_t>(header.payload_offset) + header.payload_size;
	if(expected_total != header.total_size) {
		return std::unexpected(error(ErrorCode::count_mismatch, "T3CASE total size disagrees with layout"));
	}
	if(bytes.size() < expected_total) {
		return std::unexpected(error(ErrorCode::truncated, "T3CASE is truncated"));
	}
	if(bytes.size() > expected_total) {
		return std::unexpected(error(ErrorCode::trailing_data, "T3CASE carries trailing data"));
	}

	std::array<std::byte, t3case_header_size> header_copy{};
	std::copy_n(bytes.begin(), t3case_header_size, header_copy.begin());
	std::fill(header_copy.begin() + 56, header_copy.begin() + 60, std::byte{0});
	auto computed_header = fnv1a32(header_copy);
	computed_header = fnv1a32(bytes.subspan(t3case_header_size, t3case_startup_size), computed_header);
	if(has_snapshot) {
		computed_header = fnv1a32(bytes.subspan(prefix_size, t3case_v11_snapshot_size), computed_header);
	}
	if(computed_header != header.header_checksum) {
		return std::unexpected(error(ErrorCode::checksum_mismatch, "T3CASE header checksum mismatch", 56U));
	}
	if(fnv1a32(bytes.subspan(header.payload_offset, header.payload_size)) != header.payload_checksum) {
		return std::unexpected(error(ErrorCode::checksum_mismatch, "T3CASE payload checksum mismatch", header.payload_offset));
	}

	constexpr std::size_t startup = t3case_header_size;
	if(detail::u16(bytes, startup + 48U) != 0U || detail::u8(bytes, startup + 50U) != 0U) {
		return std::unexpected(error(ErrorCode::invalid_value, "T3CASE startup carries unsupported post-init restore data", startup + 48U));
	}
	if((detail::u8(bytes, startup + 51U) & 0xFCU) != 0U) {
		return std::unexpected(error(ErrorCode::invalid_value, "T3CASE startup Autofire is not a two-player mask", startup + 51U));
	}
	for(std::size_t offset = startup + 52U; offset < prefix_size; ++offset) {
		if(detail::u8(bytes, offset) != 0U) {
			return std::unexpected(error(ErrorCode::invalid_value, "T3CASE startup reserved bytes must be zero", offset));
		}
	}
	result.init = parse_init(bytes, has_snapshot);
	if(header.scenario_id != result.init.game_mode) {
		return std::unexpected(error(ErrorCode::invalid_value, "T3CASE scenario ID disagrees with startup game mode", 35U));
	}

	result.records.reserve(header.record_count);
	std::uint32_t samples = 0;
	auto expected_process = header.first_process;
	for(std::uint32_t index = 0; index < header.record_count; ++index) {
		const auto offset = static_cast<std::size_t>(header.payload_offset) +
			(static_cast<std::size_t>(index) * t3case_record_size);
		T3CaseRecord record{};
		record.kind = static_cast<T3CaseRecordKind>(detail::u8(bytes, offset));
		record.phase = detail::u8(bytes, offset + 1U);
		record.round_or_result_frame = detail::u16(bytes, offset + 2U);
		record.frame_index = detail::u32(bytes, offset + 4U);
		record.round_frame = detail::u32(bytes, offset + 8U);
		record.input_p1 = detail::u16(bytes, offset + 12U);
		record.input_p2 = detail::u16(bytes, offset + 14U);
		record.input_system = detail::u16(bytes, offset + 16U);
		record.control = detail::u16(bytes, offset + 18U);

		if(record.frame_index != samples) {
			return std::unexpected(error(ErrorCode::malformed_stream, "T3CASE record has a non-dense frame index", offset + 4U));
		}
		if(record.kind == T3CaseRecordKind::input) {
			if(record.phase > 1U) {
				return std::unexpected(error(ErrorCode::malformed_stream, "T3CASE input record has an unsupported phase", offset + 1U));
			}
			if(
				record.phase == 1U &&
				(record.round_frame != std::numeric_limits<std::uint32_t>::max() ||
				 record.round_or_result_frame != std::numeric_limits<std::uint16_t>::max())
			) {
				return std::unexpected(error(ErrorCode::malformed_stream, "T3CASE interstitial counters are not sentinels", offset + 2U));
			}
			if((header.flags & flag_charge_in_control) != 0U) {
				if(record.control > 3U) {
					return std::unexpected(error(ErrorCode::malformed_stream, "T3CASE Charge mask uses undefined bits", offset + 18U));
				}
			} else if(record.control != 0U) {
				return std::unexpected(error(ErrorCode::malformed_stream, "T3CASE input record carries a control value", offset + 18U));
			}
			++samples;
		} else if(record.kind == T3CaseRecordKind::control) {
			if(record.phase != 2U || (record.control != 1U && record.control != 2U)) {
				return std::unexpected(error(ErrorCode::malformed_stream, "T3CASE process control is malformed", offset));
			}
			if(record.control != static_cast<std::uint16_t>(expected_process)) {
				return std::unexpected(error(ErrorCode::malformed_stream, "T3CASE process controls do not alternate from first_process", offset + 18U));
			}
			expected_process = (expected_process == replay::ProcessId::main) ?
				replay::ProcessId::mainl : replay::ProcessId::main;
		} else {
			return std::unexpected(error(ErrorCode::malformed_stream, "T3CASE record has an unknown kind", offset));
		}
		result.records.push_back(record);
	}
	if(samples != header.sample_count) {
		return std::unexpected(error(ErrorCode::count_mismatch, "T3CASE counted samples disagree with header"));
	}
	return result;
}

std::expected<replay::ReplaySession, Error> make_replay_session(const T3Case &input)
{
	replay::ReplaySession session{};
	session.source_version = (input.header.source_kind == T3CaseSourceKind::normalized_v11) ? 11U : 0U;
	session.init = input.init;
	session.sample_count = input.header.sample_count;
	session.records.reserve(input.records.size());
	const bool advisory = (input.header.flags & flag_advisory_positions) != 0U;

	for(const auto &record : input.records) {
		if(record.kind == T3CaseRecordKind::input) {
			replay::InputSample sample{};
			sample.frame_index = record.frame_index;
			sample.phase = (record.phase == 0U) ?
				replay::InputPhase::gameplay : replay::InputPhase::interstitial;
			sample.input = CoreInput{
				record.input_p1,
				record.input_p2,
				record.input_system,
				static_cast<std::uint8_t>(record.control),
			};
			if(!advisory && sample.phase == replay::InputPhase::gameplay) {
				sample.round_frame = record.round_frame;
				sample.round_or_result_frame = record.round_or_result_frame;
			}
			session.records.emplace_back(std::move(sample));
		} else {
			session.records.emplace_back(replay::ProcessBoundary{
				record.frame_index,
				static_cast<replay::ProcessId>(record.control),
			});
		}
	}
	return session;
}

} // namespace th03::formats
