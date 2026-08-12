#include "th03/formats/v13.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "bytes.hpp"
#include "rpy_checkpoint.hpp"
#include "rpy_input.hpp"

namespace th03::formats {
namespace {

constexpr std::size_t round_split_offset = 0x82U;
constexpr std::size_t round_split_size = 14U;
constexpr std::size_t round_split_count = 27U;
constexpr std::size_t stage_bonus_offset = 0x1FCU;
constexpr std::size_t stage_bonus_size = 10U;
constexpr std::size_t stage_count = 9U;
constexpr std::size_t timed_frames_offset = 0x256U;
constexpr std::size_t slow_frames_offset = 0x25AU;
constexpr std::size_t checkpoint_count_offset = 0x25EU;
constexpr std::size_t checkpoint_directory_offset = 0x25FU;
constexpr std::uint8_t summary_flags_current = 0x7FU;

Error writer_error(std::string message)
{
	return Error{ErrorCode::invalid_value, std::move(message), 0U};
}

std::size_t checkpoint_capacity(std::uint8_t mode, bool practice) noexcept
{
	if(mode == game_mode::story) {
		return 15U;
	}
	return practice ? 5U : 3U;
}

void put_i8(std::span<std::byte> bytes, std::size_t offset, std::int8_t value)
{
	detail::put_u8(bytes, offset, std::bit_cast<std::uint8_t>(value));
}

void put_score(
	std::span<std::byte> bytes,
	std::size_t offset,
	const std::array<std::uint8_t, 4U> &score
)
{
	for(std::size_t index = 0U; index < score.size(); ++index) {
		detail::put_u8(bytes, offset + index, score[index]);
	}
}

void write_metadata(
	std::span<std::byte> bytes,
	const ReplayMetadata &metadata,
	bool practice
)
{
	detail::put_u16(bytes, 0x0EU, metadata.flags);
	detail::put_u8(bytes, 0x10U, static_cast<std::uint8_t>(metadata.status));
	detail::put_u8(bytes, 0x11U, static_cast<std::uint8_t>(metadata.end_reason));
	detail::put_u8(bytes, 0x12U, metadata.game_mode);
	detail::put_u8(bytes, 0x13U, metadata.rank);
	detail::put_u8(bytes, 0x14U, metadata.key_mode);
	put_i8(bytes, 0x15U, metadata.paletted_character[0]);
	put_i8(bytes, 0x16U, metadata.paletted_character[1]);
	detail::put_u8(bytes, 0x17U, metadata.story_stage);
	detail::put_u8(bytes, 0x18U, metadata.is_cpu[0]);
	detail::put_u8(bytes, 0x19U, metadata.is_cpu[1]);
	detail::put_u32(bytes, 0x1AU, metadata.sample_count);
	detail::put_u32(bytes, 0x1EU, metadata.final_frame_count);
	detail::put_i32(bytes, 0x22U, metadata.resident_rand);
	detail::put_u32(bytes, 0x26U, metadata.random_seed);
	detail::put_u16(bytes, 0x3AU, metadata.summary_flags);
	const auto optional_byte = [](const auto &value) noexcept {
		return value ? static_cast<std::uint8_t>(*value) :
			static_cast<std::uint8_t>(0xFFU);
	};
	detail::put_u8(bytes, 0x3CU, optional_byte(metadata.final_route));
	detail::put_u8(bytes, 0x3DU, optional_byte(metadata.final_game_mode));
	detail::put_u8(bytes, 0x3EU, optional_byte(metadata.final_story_stage));
	detail::put_u8(bytes, 0x3FU, optional_byte(metadata.final_round_id));
	detail::put_u8(bytes, 0x40U, metadata.final_winner ?
		std::bit_cast<std::uint8_t>(*metadata.final_winner) :
		static_cast<std::uint8_t>(0xFFU));
	detail::put_u8(bytes, 0x41U, metadata.final_story_lives);
	detail::put_u8(bytes, 0x42U, optional_byte(metadata.final_misses));
	detail::put_u8(bytes, 0x43U, metadata.stage_reached_count);

	if(practice && metadata.practice) {
		const auto &config = *metadata.practice;
		detail::put_u8(bytes, 0x44U, config.preset);
		detail::put_u8(bytes, 0x45U, config.stage);
		detail::put_u8(bytes, 0x46U, config.round);
		detail::put_u8(bytes, 0x47U, config.stock);
		detail::put_u8(bytes, 0x48U, config.extends_gained);
		detail::put_u8(bytes, 0x49U, config.cpu_timer);
		detail::put_u8(bytes, 0x4AU, config.round_speed);
		detail::put_u8(bytes, 0x4BU, config.bullet_speed);
		detail::put_u8(bytes, 0x4CU, config.p1_spell);
		detail::put_u8(bytes, 0x4DU, config.cpu_spell);
		detail::put_u8(bytes, 0x4EU, config.boss_level);
		detail::put_u8(bytes, 0x4FU, config.cpu_damage);
		detail::put_u16(bytes, 0x50U, config.initial_cpu_safety_frames);
		detail::put_u8(bytes, 0x52U, config.p1_gauge);
		detail::put_u8(bytes, 0x53U, config.cpu_gauge);
	} else if(!practice && metadata.story) {
		for(std::size_t stage = 0U; stage < stage_count; ++stage) {
			detail::put_u8(
				bytes, 0x44U + stage, metadata.story->stage_opponents[stage]
			);
			put_score(
				bytes, 0x4DU + (stage * 4U),
				metadata.story->stage_scores[stage]
			);
		}
	}
	put_score(bytes, 0x71U, metadata.final_score);
	detail::put_u8(bytes, 0x75U, metadata.autofire);
	detail::put_u16(bytes, 0x76U, metadata.dos_date);
	for(std::size_t index = 0U; index < metadata.name.size(); ++index) {
		detail::put_u8(
			bytes, 0x78U + index,
			static_cast<std::uint8_t>(metadata.name[index])
		);
	}
}

} // namespace

std::expected<std::vector<std::byte>, Error> write_v13_replay(
	const ReplayDocument &document
)
{
	if(!document.metadata) {
		return std::unexpected(writer_error(
			"V13 writer requires metadata"
		));
	}
	const auto &metadata = *document.metadata;
	const auto capacity = checkpoint_capacity(metadata.game_mode, document.practice);
	if(document.checkpoints.empty() || document.checkpoints.size() > capacity ||
		document.expected_round_splits.size() > round_split_count) {
		return std::unexpected(writer_error(
			"V13 writer checkpoint or round-summary count is invalid"
		));
	}
	std::vector<std::uint32_t> checkpoint_samples;
	checkpoint_samples.reserve(document.checkpoints.size());
	for(const auto &checkpoint : document.checkpoints) {
		checkpoint_samples.push_back(checkpoint.sample_index);
	}
	auto encoded_input = detail::encode_rpy_input(
		document.session.records,
		document.session.sample_count,
		checkpoint_samples,
		"V13 writer"
	);
	if(!encoded_input) {
		return std::unexpected(encoded_input.error());
	}
	if(encoded_input->bytes.size() > std::numeric_limits<std::uint32_t>::max()) {
		return std::unexpected(writer_error(
			"V13 writer input stream exceeds 32-bit size"
		));
	}
	const auto input_offset = replay_v13_header_size +
		(capacity * replay_v13_checkpoint_size);
	if(encoded_input->bytes.size() >
		std::numeric_limits<std::size_t>::max() - input_offset) {
		return std::unexpected(writer_error(
			"V13 writer output size overflows"
		));
	}
	std::vector<std::byte> bytes(
		input_offset + encoded_input->bytes.size(), std::byte{0}
	);
	const auto output = std::span<std::byte>(bytes);
	constexpr std::array<std::uint8_t, 8U> magic{
		'T', '3', 'R', 'P', 'L', 'Y', '1', '3',
	};
	for(std::size_t index = 0U; index < magic.size(); ++index) {
		detail::put_u8(output, index, magic[index]);
	}
	detail::put_u16(output, 0x08U, replay_v13_version);
	detail::put_u16(output, 0x0AU, replay_v13_header_size);
	detail::put_u16(output, 0x0CU, 0U);
	write_metadata(output, metadata, document.practice);
	detail::put_u32(output, 0x2AU, static_cast<std::uint32_t>(input_offset));
	detail::put_u32(
		output, 0x2EU, static_cast<std::uint32_t>(encoded_input->bytes.size())
	);
	detail::put_u32(output, 0x32U, replay_v13_header_size);
	detail::put_u32(output, 0x36U, replay_v13_checkpoint_size);

	detail::put_u8(output, 0x80U, summary_flags_current);
	detail::put_u8(
		output, 0x81U,
		static_cast<std::uint8_t>(document.expected_round_splits.size())
	);
	for(std::size_t index = 0U;
		index < document.expected_round_splits.size(); ++index) {
		const auto &split = document.expected_round_splits[index];
		const auto offset = round_split_offset + (index * round_split_size);
		detail::put_u8(output, offset,
			static_cast<std::uint8_t>((split.round_id << 4U) | split.story_stage));
		const auto winner = (split.winner < 0) ? 0x0FU :
			static_cast<std::uint8_t>(split.winner);
		detail::put_u8(output, offset + 1U,
			static_cast<std::uint8_t>((split.route << 4U) | winner));
		put_score(output, offset + 2U, split.p1_score);
		put_score(output, offset + 6U, split.p2_score);
		detail::put_u32(output, offset + 10U, split.real_frames.value_or(0U));
	}
	if(document.stage_clear_bonuses) {
		for(std::size_t stage = 0U; stage < stage_count; ++stage) {
			const auto &bonus = (*document.stage_clear_bonuses)[stage];
			const auto offset = stage_bonus_offset + (stage * stage_bonus_size);
			put_score(output, offset, bonus.total);
			detail::put_u8(output, offset + 4U, bonus.max_combo);
			detail::put_u8(output, offset + 5U, bonus.gauge_attacks);
			detail::put_u8(output, offset + 6U, bonus.boss_attacks);
			detail::put_u8(output, offset + 7U, bonus.boss_reversals);
			detail::put_u8(output, offset + 8U, bonus.boss_panics);
			detail::put_u8(output, offset + 9U, bonus.remaining_lives);
		}
	}
	const auto slowdown = document.slowdown.value_or(ReplaySlowdown{});
	detail::put_u32(output, timed_frames_offset, slowdown.timed_frames);
	detail::put_u32(output, slow_frames_offset, slowdown.slow_frames);
	detail::put_u8(
		output, checkpoint_count_offset,
		static_cast<std::uint8_t>(document.checkpoints.size())
	);
	for(std::size_t index = 0U; index < document.checkpoints.size(); ++index) {
		const auto &checkpoint = document.checkpoints[index];
		detail::put_u8(
			output, checkpoint_directory_offset + index,
			static_cast<std::uint8_t>(
				(checkpoint.round << 4U) | checkpoint.stage
			)
		);
		const detail::CompactCheckpoint compact{
			.sample_count = checkpoint.sample_index,
			.global_frame = checkpoint.global_frame,
			.input_size = encoded_input->checkpoints[index].input_size,
			.init = checkpoint.init,
		};
		const auto offset = replay_v13_header_size +
			(index * replay_v13_checkpoint_size);
		if(auto written = detail::write_compact_checkpoint(
			output, offset, compact, "V13 writer"
		); !written) {
			return std::unexpected(written.error());
		}
	}
	std::copy(
		encoded_input->bytes.begin(), encoded_input->bytes.end(),
		bytes.begin() + static_cast<std::ptrdiff_t>(input_offset)
	);
	if(auto validated = parse_v13_replay(bytes); !validated) {
		return std::unexpected(validated.error());
	}
	return bytes;
}

} // namespace th03::formats
