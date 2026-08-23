#include "th03/formats/v14.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/core/gameplay_ruleset.hpp"

#include "bytes.hpp"
#include "rpy_checkpoint.hpp"
#include "rpy_input.hpp"

namespace th03::formats {
namespace {

constexpr std::size_t base_header_size = 128U;
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
constexpr std::size_t ruleset_offset = 0x26EU;
constexpr std::size_t recording_flags_offset = 0x26FU;
constexpr std::size_t recorder_role_offset = 0x270U;
constexpr std::size_t recorder_source_offset = 0x271U;
constexpr std::size_t player_one_uuid_offset = 0x272U;
constexpr std::size_t player_two_uuid_offset = 0x282U;
constexpr std::size_t match_id_offset = 0x292U;
constexpr std::size_t player_one_nametag_length_offset = 0x2A2U;
constexpr std::size_t player_two_nametag_length_offset = 0x2A3U;
constexpr std::size_t player_one_nametag_offset = 0x2A4U;
constexpr std::size_t player_two_nametag_offset = 0x2DDU;
constexpr std::size_t reserved_extension_offset = 0x316U;
constexpr std::size_t accelerator_footer_size = 80U;
constexpr std::size_t accelerator_descriptor_size = 16U;
constexpr std::size_t accelerator_raw_size = 29'286U;
constexpr std::uint32_t accelerator_packed_size_max = 0xFFF0U;
// T3RACC1 stores release MAIN DGROUP bytes beginning at palette_1F2F4
// (0x1D94). The first 48 bytes are the saved 16-entry RGB8 palette;
// player_velocity.x and page_back/page_front are fixed at
// 0x6586 and 0x658E/0x658F in that frozen process image, hence these
// raw-image offsets.
constexpr std::size_t accelerator_palette_offset = 0U;
constexpr std::size_t accelerator_shared_velocity_x_offset = 0x47F2U;
constexpr std::size_t accelerator_page_back_offset = 0x47FAU;
constexpr std::size_t accelerator_page_front_offset = 0x47FBU;
constexpr std::size_t accelerator_background_offset = 0x1C38U;
constexpr std::size_t accelerator_background_record_size = 16U;
constexpr std::size_t accelerator_background_magnitude_offset = 0x2138U;
constexpr std::uint16_t accelerator_background_variant_one_offset = 0x238EU;
constexpr std::uint16_t replay_flags_known = 0x0007U;
constexpr std::uint16_t replay_flags_required = 0x0003U;
constexpr std::uint16_t summary_flags_current = 0x007FU;

Error error(ErrorCode code, std::string message, std::size_t offset = 0U)
{
	return Error{code, std::move(message), offset};
}

bool game_mode_valid(std::uint8_t mode) noexcept
{
	return mode == game_mode::story || mode == game_mode::vs_1p_cpu ||
		mode == game_mode::vs_1p_2p || mode == game_mode::vs_cpu_cpu;
}

std::size_t checkpoint_capacity(std::uint8_t mode, bool practice) noexcept
{
	if(mode == game_mode::story) {
		return 15U;
	}
	return practice ? 5U : 3U;
}

bool packed_score_valid(const std::array<std::uint8_t, 4> &score) noexcept
{
	for(const auto byte : score) {
		if((byte & 0x0FU) > 9U || (byte >> 4U) > 9U) {
			return false;
		}
	}
	return true;
}

std::array<std::uint8_t, 4> read_packed_score(
	std::span<const std::byte> bytes,
	std::size_t offset
)
{
	std::array<std::uint8_t, 4> score{};
	for(std::size_t index = 0U; index < score.size(); ++index) {
		score[index] = detail::u8(bytes, offset + index);
	}
	return score;
}

std::array<std::uint8_t, 4> pack_decimal(std::uint32_t value) noexcept
{
	std::array<std::uint8_t, 4> packed{};
	for(auto &byte : packed) {
		byte = static_cast<std::uint8_t>(
			(value % 10U) | (((value / 10U) % 10U) << 4U)
		);
		value /= 100U;
	}
	return packed;
}

bool leap_year(unsigned int year) noexcept
{
	return (year % 4U) == 0U && ((year % 100U) != 0U || (year % 400U) == 0U);
}

bool dos_date_valid(std::uint16_t value) noexcept
{
	if(value == 0U) {
		return true;
	}
	const auto year = 1980U + static_cast<unsigned int>((value >> 9U) & 0x7FU);
	const auto month = static_cast<unsigned int>((value >> 5U) & 0x0FU);
	const auto day = static_cast<unsigned int>(value & 0x1FU);
	if(month == 0U || month > 12U || day == 0U) {
		return false;
	}
	constexpr std::array<unsigned int, 12> days{
		31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U,
	};
	auto maximum = days[month - 1U];
	if(month == 2U && leap_year(year)) {
		maximum = 29U;
	}
	return day <= maximum;
}

bool replay_name_byte_valid(std::uint8_t value) noexcept
{
	return (value >= 'A' && value <= 'Z') ||
		(value >= '0' && value <= '9') || value == ' ' || value == '.' ||
		value == ',' || value == '!' || value == '?';
}

bool fallback_nametag_valid(std::string_view nametag) noexcept
{
	if(nametag.empty()) {
		return true;
	}
	if(nametag.size() > replay_v14_nametag_max_bytes ||
		nametag.front() == ' ' || nametag.back() == ' ' ||
		nametag.find("  ") != std::string_view::npos) {
		return false;
	}
	for(const auto character : nametag) {
		const auto byte = static_cast<unsigned char>(character);
		if(byte < 0x20U || byte > 0x7EU) {
			return false;
		}
	}
	std::size_t line = 0U;
	std::size_t line_size = 0U;
	std::size_t offset = 0U;
	while(offset < nametag.size()) {
		const auto separator = nametag.find(' ', offset);
		const auto end = separator == std::string_view::npos ?
			nametag.size() : separator;
		const auto word_size = end - offset;
		if(word_size > 28U) {
			return false;
		}
		const auto required = line_size == 0U ?
			word_size : line_size + 1U + word_size;
		if(required > 28U) {
			++line;
			line_size = word_size;
			if(line >= 2U) {
				return false;
			}
		} else {
			line_size = required;
		}
		offset = end == nametag.size() ? end : end + 1U;
	}
	return true;
}

std::optional<std::uint8_t> optional_u8(std::uint8_t value) noexcept
{
	return (value == 0xFFU) ? std::nullopt : std::optional{value};
}

std::optional<std::vector<std::byte>> decompress_accelerator(
	std::span<const std::byte> packed
)
{
	std::vector<std::byte> raw(accelerator_raw_size);
	std::size_t input = 0U;
	std::size_t output = 0U;
	while(output < raw.size()) {
		if(input >= packed.size()) {
			return std::nullopt;
		}
		const auto flags = std::to_integer<std::uint8_t>(packed[input++]);
		for(std::uint8_t token = 0U; token < 8U && output < raw.size(); ++token) {
			if((flags & static_cast<std::uint8_t>(1U << token)) != 0U) {
				if(input >= packed.size()) {
					return std::nullopt;
				}
				raw[output++] = packed[input++];
				continue;
			}
			if((input + 1U) >= packed.size()) {
				return std::nullopt;
			}
			const auto lo = std::to_integer<std::uint8_t>(packed[input++]);
			const auto hi = std::to_integer<std::uint8_t>(packed[input++]);
			const auto distance = static_cast<std::size_t>(
				lo | ((static_cast<std::uint16_t>(hi) >> 4U) << 8U)
			);
			const auto length = static_cast<std::size_t>((hi & 0x0FU) + 3U);
			if(distance == 0U || distance > output || length > (raw.size() - output)) {
				return std::nullopt;
			}
			for(std::size_t copied = 0U; copied < length; ++copied) {
				raw[output] = raw[output - distance];
				++output;
			}
		}
	}
	if(input != packed.size()) {
		return std::nullopt;
	}
	return raw;
}

std::uint32_t accelerator_hash(std::span<const std::byte> raw) noexcept
{
	std::uint32_t hash = 5381U;
	for(const auto value : raw) {
		hash = (hash * 33U) ^ std::to_integer<std::uint8_t>(value);
	}
	return hash;
}

std::vector<ReplayAcceleratorPresentation> parse_accelerators(
	std::span<const std::byte> bytes,
	std::uint32_t logical_end,
	std::span<const std::uint8_t> checkpoint_directory
)
{
	std::vector<ReplayAcceleratorPresentation> accepted;
	if(bytes.size() < accelerator_footer_size) {
		return accepted;
	}
	const auto footer = bytes.size() - accelerator_footer_size;
	constexpr std::array<std::uint8_t, 8U> magic{
		'T', '3', 'R', 'A', 'C', 'C', '1', 0U,
	};
	if(!detail::has_magic(bytes.subspan(footer), magic) ||
		detail::u16(bytes, footer + 8U) != 1U ||
		detail::u16(bytes, footer + 10U) != accelerator_footer_size) {
		return accepted;
	}
	const auto count = detail::u8(bytes, footer + 12U);
	if(count == 0U || count > 4U || detail::u8(bytes, footer + 13U) != 0U ||
		detail::u8(bytes, footer + 14U) != 0U ||
		detail::u8(bytes, footer + 15U) != 0U) {
		return accepted;
	}
	accepted.reserve(count);
	for(std::size_t index = 0U; index < count; ++index) {
		const auto descriptor = footer + 16U + (index * accelerator_descriptor_size);
		const auto checkpoint = detail::u8(bytes, descriptor);
		const auto codec = detail::u8(bytes, descriptor + 1U);
		const auto raw_size = detail::u16(bytes, descriptor + 2U);
		const auto offset = detail::u32(bytes, descriptor + 4U);
		const auto packed_size = detail::u32(bytes, descriptor + 8U);
		const auto expected_hash = detail::u32(bytes, descriptor + 12U);
		if(checkpoint >= checkpoint_directory.size() ||
			(checkpoint_directory[checkpoint] >> 4U) == 0U || codec != 1U ||
			raw_size != accelerator_raw_size || packed_size == 0U ||
			packed_size > accelerator_packed_size_max || offset < logical_end ||
			packed_size > footer || offset > (footer - packed_size) ||
			std::find_if(
				accepted.begin(), accepted.end(),
				[checkpoint](const ReplayAcceleratorPresentation &candidate) {
					return candidate.checkpoint == checkpoint;
				}
			) != accepted.end()) {
			continue;
		}
		const auto packed = bytes.subspan(
			static_cast<std::size_t>(offset), static_cast<std::size_t>(packed_size)
		);
		auto raw = decompress_accelerator(packed);
		if(!raw || accelerator_hash(*raw) != expected_hash) {
			continue;
		}
		const auto page_back = std::to_integer<std::uint8_t>(
			(*raw)[accelerator_page_back_offset]
		);
		const auto page_front = std::to_integer<std::uint8_t>(
			(*raw)[accelerator_page_front_offset]
		);
		if(page_back > 1U || page_front > 1U || page_back == page_front) {
			continue;
		}
		ReplayAcceleratorPresentation presentation{
			.checkpoint = checkpoint,
			.page_back = page_back,
			.page_front = page_front,
			.shared_velocity_x_q4_4 = std::bit_cast<std::int8_t>(
				std::to_integer<std::uint8_t>(
					(*raw)[accelerator_shared_velocity_x_offset]
				)
			),
			.background_magnitude = std::to_integer<std::uint8_t>(
				(*raw)[accelerator_background_magnitude_offset]
			),
		};
		for(std::size_t color = 0U; color < presentation.palette.size(); ++color) {
			const auto base = accelerator_palette_offset + (color * 3U);
			presentation.palette[color] = {
				.r = detail::u8(*raw, base),
				.g = detail::u8(*raw, base + 1U),
				.b = detail::u8(*raw, base + 2U),
			};
		}
		for(std::size_t slot = 0U; slot < presentation.background.size(); ++slot) {
			const auto base = accelerator_background_offset +
				(slot * accelerator_background_record_size);
			presentation.background[slot] = {
				.x_q12_4 = std::bit_cast<std::int16_t>(
					detail::u16(*raw, base)
				),
				.y_q12_4 = std::bit_cast<std::int16_t>(
					detail::u16(*raw, base + 2U)
				),
				.velocity_x_q12_4 = std::bit_cast<std::int16_t>(
					detail::u16(*raw, base + 4U)
				),
				.velocity_y_q12_4 = std::bit_cast<std::int16_t>(
					detail::u16(*raw, base + 6U)
				),
				.angle = detail::u8(*raw, base + 8U),
				.sprite_variant = static_cast<std::uint8_t>(
					detail::u16(*raw, base + 10U) ==
						accelerator_background_variant_one_offset
				),
			};
		}
		accepted.push_back(std::move(presentation));
	}
	return accepted;
}

std::expected<ReplayPracticeConfig, Error> parse_practice_config(
	std::span<const std::byte> bytes
)
{
	ReplayPracticeConfig config{
		.preset = detail::u8(bytes, 0x44U),
		.stage = detail::u8(bytes, 0x45U),
		.round = detail::u8(bytes, 0x46U),
		.stock = detail::u8(bytes, 0x47U),
		.extends_gained = detail::u8(bytes, 0x48U),
		.cpu_timer = detail::u8(bytes, 0x49U),
		.round_speed = detail::u8(bytes, 0x4AU),
		.bullet_speed = detail::u8(bytes, 0x4BU),
		.p1_spell = detail::u8(bytes, 0x4CU),
		.cpu_spell = detail::u8(bytes, 0x4DU),
		.boss_level = detail::u8(bytes, 0x4EU),
		.cpu_damage = detail::u8(bytes, 0x4FU),
		.initial_cpu_safety_frames = detail::u16(bytes, 0x50U),
		.p1_gauge = detail::u8(bytes, 0x52U),
		.cpu_gauge = detail::u8(bytes, 0x53U),
	};
	for(std::size_t offset = 0x54U; offset <= 0x70U; ++offset) {
		if(detail::u8(bytes, offset) != 0U) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				"V14 Practice reserved byte is nonzero",
				offset
			));
		}
	}
	const bool base_valid =
		config.preset <= 1U && config.cpu_timer <= 2U &&
		config.round_speed <= 0x7FU && config.bullet_speed <= 0x7FU &&
		config.p1_spell >= 1U && config.p1_spell <= 16U &&
		config.cpu_spell >= 1U && config.cpu_spell <= 16U &&
		config.boss_level <= 15U && config.cpu_damage <= 3U &&
		config.p1_gauge >= 64U && config.cpu_gauge >= 64U;
	const bool scenario_valid = (config.preset == 0U)
		? (
			config.stage == 0U && config.round == 0U &&
			config.stock == 0xFFU && config.extends_gained == 0U
		)
		: (
			config.stage < 9U && config.round <= 5U && config.stock <= 4U &&
			config.extends_gained <= 2U &&
			config.stock <= static_cast<std::uint8_t>(2U + config.extends_gained)
		);
	if(!base_valid || !scenario_valid) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 Practice configuration is invalid",
			0x44U
		));
	}
	return config;
}

std::expected<ReplayMetadata, Error> parse_metadata(
	std::span<const std::byte> bytes,
	bool practice
)
{
	const auto status = detail::u8(bytes, 0x10U);
	const auto end_reason = detail::u8(bytes, 0x11U);
	const auto mode = detail::u8(bytes, 0x12U);
	if(status > static_cast<std::uint8_t>(ReplayStatus::error)) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "V14 replay status is invalid", 0x10U
		));
	}
	if(
		status == static_cast<std::uint8_t>(ReplayStatus::empty) ||
		status == static_cast<std::uint8_t>(ReplayStatus::error)
	) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 status does not describe a written replay artifact",
			0x10U
		));
	}
	if(end_reason > static_cast<std::uint8_t>(ReplayEndReason::game_over)) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "V14 replay end reason is invalid", 0x11U
		));
	}
	if(!game_mode_valid(mode)) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "V14 game mode is not Story or VS", 0x12U
		));
	}
	if(detail::u8(bytes, 0x13U) > 3U) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "V14 rank is outside 0..3", 0x13U
		));
	}
	if(detail::u8(bytes, 0x14U) > 2U) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "V14 key mode is outside 0..2", 0x14U
		));
	}
	for(std::size_t player = 0U; player < 2U; ++player) {
		const auto character = detail::u8(bytes, 0x15U + player);
		if(character < 1U || character > 18U) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				"V14 paletted character is outside 1..18",
				0x15U + player
			));
		}
		if(detail::u8(bytes, 0x18U + player) > 1U) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				"V14 CPU flag is not Boolean",
				0x18U + player
			));
		}
	}
	const auto expected_cpu = (mode == game_mode::vs_1p_2p)
		? std::array<std::uint8_t, 2>{0U, 0U}
		: ((mode == game_mode::vs_cpu_cpu)
			? std::array<std::uint8_t, 2>{1U, 1U}
			: std::array<std::uint8_t, 2>{0U, 1U});
	if(
		std::array<std::uint8_t, 2>{
			detail::u8(bytes, 0x18U), detail::u8(bytes, 0x19U),
		} != expected_cpu
	) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 CPU flags disagree with the game mode",
			0x18U
		));
	}
	if(practice && mode != game_mode::vs_1p_cpu) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 Practice must use GM_VS_1P_CPU",
			0x12U
		));
	}
	if((mode == game_mode::story || practice) && detail::u8(bytes, 0x17U) >= 9U) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "V14 initial stage is outside 0..8", 0x17U
		));
	}
	const auto stage_reached_count = detail::u8(bytes, 0x43U);
	if(stage_reached_count > 9U || (mode != game_mode::story && stage_reached_count != 0U)) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 stage-reached count is inconsistent with its scenario",
			0x43U
		));
	}
	const auto summary_flags = detail::u16(bytes, 0x3AU);
	// V14 is a frozen pre-1.0 contract. New capability bits require a new
	// magic/version rather than silently changing the meaning of this layout.
	if(summary_flags != summary_flags_current) {
		return std::unexpected(error(
			ErrorCode::unsupported_version,
			"V14 lacks the finalized summary capabilities",
			0x3AU
		));
	}
	const auto final_route_raw = detail::u8(bytes, 0x3CU);
	if(final_route_raw != 0xFFU && final_route_raw > 2U) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "V14 final route is invalid", 0x3CU
		));
	}
	const auto final_mode_raw = detail::u8(bytes, 0x3DU);
	if(final_mode_raw != 0xFFU && !game_mode_valid(final_mode_raw)) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "V14 final game mode is invalid", 0x3DU
		));
	}
	const auto final_stage_raw = detail::u8(bytes, 0x3EU);
	if(final_stage_raw != 0xFFU && final_stage_raw > 9U) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "V14 final Story stage is invalid", 0x3EU
		));
	}
	const auto final_winner_raw = detail::u8(bytes, 0x40U);
	if(final_winner_raw != 0xFFU && final_winner_raw > 1U) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "V14 final winner is invalid", 0x40U
		));
	}
	const auto final_score = read_packed_score(bytes, 0x71U);
	if(!packed_score_valid(final_score)) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "V14 final score is not packed decimal", 0x71U
		));
	}
	const auto autofire = detail::u8(bytes, 0x75U);
	if(autofire > 3U) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 Autofire mask contains undefined bits",
			0x75U
		));
	}
	const auto dos_date = detail::u16(bytes, 0x76U);
	if(!dos_date_valid(dos_date)) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "V14 recording date is invalid", 0x76U
		));
	}

	ReplayMetadata metadata{};
	metadata.flags = detail::u16(bytes, 0x0EU);
	metadata.status = static_cast<ReplayStatus>(status);
	metadata.end_reason = static_cast<ReplayEndReason>(end_reason);
	metadata.game_mode = mode;
	metadata.rank = detail::u8(bytes, 0x13U);
	metadata.key_mode = detail::u8(bytes, 0x14U);
	metadata.paletted_character = {
		detail::i8(bytes, 0x15U), detail::i8(bytes, 0x16U),
	};
	metadata.story_stage = detail::u8(bytes, 0x17U);
	metadata.is_cpu = {
		detail::u8(bytes, 0x18U), detail::u8(bytes, 0x19U),
	};
	metadata.sample_count = detail::u32(bytes, 0x1AU);
	metadata.final_frame_count = detail::u32(bytes, 0x1EU);
	metadata.resident_rand = detail::i32(bytes, 0x22U);
	metadata.random_seed = detail::u32(bytes, 0x26U);
	metadata.summary_flags = summary_flags;
	metadata.final_route = optional_u8(final_route_raw);
	metadata.final_game_mode = optional_u8(final_mode_raw);
	metadata.final_story_stage = optional_u8(final_stage_raw);
	metadata.final_round_id = optional_u8(detail::u8(bytes, 0x3FU));
	metadata.final_winner = (final_winner_raw == 0xFFU)
		? std::nullopt
		: std::optional{static_cast<std::int8_t>(final_winner_raw)};
	metadata.final_story_lives = detail::u8(bytes, 0x41U);
	metadata.final_misses = optional_u8(detail::u8(bytes, 0x42U));
	metadata.stage_reached_count = stage_reached_count;
	metadata.final_score = final_score;
	metadata.autofire = autofire;
	metadata.dos_date = dos_date;
	bool name_is_unset = true;
	for(std::size_t index = 0U; index < metadata.name.size(); ++index) {
		const auto value = detail::u8(bytes, 0x78U + index);
		name_is_unset = name_is_unset && value == 0U;
	metadata.name[index] = static_cast<char>(value);
	}
	const auto ruleset = detail::u8(bytes, ruleset_offset);
	if(ruleset > static_cast<std::uint8_t>(ReplayRuleset::dopamine_test_v1)) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "V14 ruleset is unknown", ruleset_offset
		));
	}
	const auto recording_flags = detail::u8(bytes, recording_flags_offset);
	if((recording_flags & static_cast<std::uint8_t>(
		~replay_recording_flags_known
	)) != 0U) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 recording flags contain undefined bits",
			recording_flags_offset
		));
	}
	const auto recorder_role = detail::u8(bytes, recorder_role_offset);
	const auto recorder_source = detail::u8(bytes, recorder_source_offset);
	if(recorder_role > static_cast<std::uint8_t>(ReplayRecorderRole::player_two)) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "V14 recorder role is invalid",
			recorder_role_offset
		));
	}
	if(recorder_source > static_cast<std::uint8_t>(ReplayRecorderSource::imported)) {
		return std::unexpected(error(
			ErrorCode::invalid_value, "V14 recorder source is invalid",
			recorder_source_offset
		));
	}
	const bool netplay = (recording_flags & replay_recording_flag_netplay) != 0U;
	if(netplay != (recorder_role !=
		static_cast<std::uint8_t>(ReplayRecorderRole::unknown)) ||
		(netplay && mode != game_mode::vs_1p_2p)) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 netplay flag, recorder role, and game mode disagree",
			recording_flags_offset
		));
	}
	metadata.ruleset = static_cast<ReplayRuleset>(ruleset);
	metadata.recording_flags = recording_flags;
	metadata.recorder_role = static_cast<ReplayRecorderRole>(recorder_role);
	metadata.recorder_source = static_cast<ReplayRecorderSource>(recorder_source);
	for(std::size_t index = 0U; index < metadata.player_uuid[0U].size(); ++index) {
		metadata.player_uuid[0U][index] = detail::u8(
			bytes, player_one_uuid_offset + index
		);
		metadata.player_uuid[1U][index] = detail::u8(
			bytes, player_two_uuid_offset + index
		);
		metadata.match_id[index] = detail::u8(bytes, match_id_offset + index);
	}
	constexpr std::array<std::size_t, 2U> nametag_length_offsets{
		player_one_nametag_length_offset, player_two_nametag_length_offset,
	};
	constexpr std::array<std::size_t, 2U> nametag_offsets{
		player_one_nametag_offset, player_two_nametag_offset,
	};
	for(std::size_t player = 0U; player < 2U; ++player) {
		const auto length = detail::u8(bytes, nametag_length_offsets[player]);
		if(length > replay_v14_nametag_max_bytes) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				"V14 fallback nametag length exceeds 57 bytes",
				nametag_length_offsets[player]
			));
		}
		auto &nametag = metadata.player_nametag_snapshot[player];
		nametag.reserve(length);
		for(std::size_t index = 0U; index < replay_v14_nametag_max_bytes; ++index) {
			const auto value = detail::u8(bytes, nametag_offsets[player] + index);
			if(index < length) {
				nametag.push_back(static_cast<char>(value));
			} else if(value != 0U) {
				return std::unexpected(error(
					ErrorCode::invalid_value,
					"V14 fallback nametag padding is nonzero",
					nametag_offsets[player] + index
				));
			}
		}
		if(!fallback_nametag_valid(nametag)) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				"V14 fallback nametag does not fit the title geometry",
				nametag_offsets[player]
			));
		}
	}
	for(std::size_t offset = reserved_extension_offset;
		offset < replay_v14_header_size; ++offset) {
		if(detail::u8(bytes, offset) != 0U) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				"V14 reserved extension byte is nonzero", offset
			));
		}
	}
	if(!name_is_unset) {
		for(std::size_t index = 0U; index < metadata.name.size(); ++index) {
			if(!replay_name_byte_valid(
				static_cast<std::uint8_t>(metadata.name[index])
			)) {
				return std::unexpected(error(
					ErrorCode::invalid_value,
					"V14 replay name is neither unset nor space-padded ASCII",
					0x78U + index
				));
			}
		}
	}

	if(practice) {
		auto config = parse_practice_config(bytes);
		if(!config) {
			return std::unexpected(config.error());
		}
		metadata.practice = *config;
	} else if(mode == game_mode::story) {
		ReplayStorySummary story{};
		for(std::size_t stage = 0U; stage < stage_count; ++stage) {
			story.stage_opponents[stage] = detail::u8(bytes, 0x44U + stage);
			if(
				story.stage_opponents[stage] > 18U ||
				(stage < stage_reached_count && story.stage_opponents[stage] == 0U)
			) {
				return std::unexpected(error(
					ErrorCode::invalid_value,
					"V14 Story opponent is outside the paletted-character domain",
					0x44U + stage
				));
			}
			story.stage_scores[stage] = read_packed_score(
				bytes, 0x4DU + (stage * 4U)
			);
			if(!packed_score_valid(story.stage_scores[stage])) {
				return std::unexpected(error(
					ErrorCode::invalid_value,
					"V14 Story stage score is not packed decimal",
					0x4DU + (stage * 4U)
				));
			}
		}
		if(
			std::bit_cast<std::uint8_t>(metadata.paletted_character[1]) !=
			story.stage_opponents[metadata.story_stage]
		) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				"V14 initial Story opponent disagrees with P2",
				0x16U
			));
		}
		metadata.story = story;
	}
	return metadata;
}

std::expected<std::vector<ReplayRoundSplit>, Error> parse_round_splits(
	std::span<const std::byte> bytes,
	const ReplayMetadata &metadata,
	bool practice
)
{
	if(detail::u8(bytes, 0x80U) != summary_flags_current) {
		return std::unexpected(error(
			ErrorCode::unsupported_version,
			"V14 round summary lacks the finalized capabilities",
			0x80U
		));
	}
	const auto count = detail::u8(bytes, 0x81U);
	if(count > round_split_count) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 round-split count exceeds 27",
			0x81U
		));
	}
	std::vector<ReplayRoundSplit> result;
	result.reserve(count);
	for(std::size_t index = 0U; index < count; ++index) {
		const auto offset = round_split_offset + (index * round_split_size);
		const auto stage_round = detail::u8(bytes, offset + 0U);
		const auto route_winner = detail::u8(bytes, offset + 1U);
		const auto stage = static_cast<std::uint8_t>(stage_round & 0x0FU);
		const auto winner = static_cast<std::uint8_t>(route_winner & 0x0FU);
		const bool stage_valid = (metadata.game_mode == game_mode::story || practice)
			? stage < 9U : stage == 0x0FU;
		ReplayRoundSplit split{};
		split.story_stage = stage;
		split.round_id = static_cast<std::uint8_t>(stage_round >> 4U);
		split.route = static_cast<std::uint8_t>(route_winner >> 4U);
		split.winner = (winner == 0x0FU) ? -1 : static_cast<std::int8_t>(winner);
		split.p1_score = read_packed_score(bytes, offset + 2U);
		split.p2_score = read_packed_score(bytes, offset + 6U);
		split.real_frames = detail::u32(bytes, offset + 10U);
		if(
			!stage_valid || (practice && stage != metadata.practice->stage) ||
			split.route > 2U || (winner > 1U && winner != 0x0FU) ||
			!packed_score_valid(split.p1_score) ||
			!packed_score_valid(split.p2_score)
		) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				"V14 contains an invalid round split",
				offset
			));
		}
		result.push_back(split);
	}
	return result;
}

std::expected<std::array<ReplayStageClearBonus, 9>, Error> parse_bonuses(
	std::span<const std::byte> bytes
)
{
	std::array<ReplayStageClearBonus, 9> bonuses{};
	for(std::size_t stage = 0U; stage < bonuses.size(); ++stage) {
		const auto offset = stage_bonus_offset + (stage * stage_bonus_size);
		auto &bonus = bonuses[stage];
		bonus.total = read_packed_score(bytes, offset);
		bonus.max_combo = detail::u8(bytes, offset + 4U);
		bonus.gauge_attacks = detail::u8(bytes, offset + 5U);
		bonus.boss_attacks = detail::u8(bytes, offset + 6U);
		bonus.boss_reversals = detail::u8(bytes, offset + 7U);
		bonus.boss_panics = detail::u8(bytes, offset + 8U);
		bonus.remaining_lives = detail::u8(bytes, offset + 9U);
		if(!packed_score_valid(bonus.total)) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				"V14 clear-bonus total is not packed decimal",
				offset
			));
		}
		if(stage != 8U && bonus.remaining_lives != 0U) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				"V14 non-final clear bonus contains remaining lives",
				offset + 9U
			));
		}
		const auto total =
			(static_cast<std::uint32_t>(bonus.max_combo) * 1'000U) +
			(static_cast<std::uint32_t>(bonus.gauge_attacks) * 10'000U) +
			(static_cast<std::uint32_t>(bonus.boss_attacks) * 15'000U) +
			(static_cast<std::uint32_t>(bonus.boss_reversals) * 20'000U) +
			(static_cast<std::uint32_t>(bonus.boss_panics) * 30'000U) +
			(static_cast<std::uint32_t>(bonus.remaining_lives) * 100'000U);
		if(bonus.total != pack_decimal(total)) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				"V14 clear-bonus total disagrees with its components",
				offset
			));
		}
	}
	return bonuses;
}

std::expected<PracticeRules, Error> validate_practice_anchor(
	const ReplayPracticeConfig &config,
	const GameInit &init
)
{
	const auto &state = init.round_start_snapshot->state;
	const auto &runtime = *init.compact_runtime_snapshot;
	const auto expected_lives = (config.stock == 0xFFU) ? 0U : config.stock;
	const auto expected_extends = (config.extends_gained >= 2U)
		? 0xFFU : config.extends_gained;
	if(
		init.story_stage != config.stage || init.story_lives != expected_lives ||
		state.round_id != config.round || state.round_speed != config.round_speed ||
		state.bullet_base_speed != config.bullet_speed ||
		state.gauge_attack_level !=
			std::array<std::uint8_t, 2>{config.p1_spell, config.cpu_spell} ||
		state.boss_attack_level != config.boss_level ||
		state.cpu_hit_damage_additional != config.cpu_damage ||
		state.extends_gained != expected_extends ||
		state.cpu_safety_frames != std::array<std::uint16_t, 2>{
			config.initial_cpu_safety_frames,
			config.initial_cpu_safety_frames,
		} ||
		runtime.players[0].gauge_avail !=
			static_cast<std::uint16_t>(config.p1_gauge) * 16U ||
		runtime.players[1].gauge_avail !=
			static_cast<std::uint16_t>(config.cpu_gauge) * 16U
	) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 Practice configuration disagrees with checkpoint 0",
			0x44U
		));
	}
	return PracticeRules{
		.uses_stock = config.stock != 0xFFU,
		.retry_cpu_timer = static_cast<CpuTimerPolicy>(config.cpu_timer),
		.retry_cpu_damage = config.cpu_damage,
	};
}

std::expected<void, Error> validate_checkpoint_identity(
	const detail::CompactCheckpoint &checkpoint,
	std::uint8_t stage_round,
	const ReplayMetadata &metadata,
	bool practice,
	std::size_t offset
)
{
	const auto &init = checkpoint.init;
	const auto &state = init.round_start_snapshot->state;
	const auto stage = static_cast<std::uint8_t>(stage_round & 0x0FU);
	const auto round = static_cast<std::uint8_t>(stage_round >> 4U);
	const bool stage_valid = (metadata.game_mode == game_mode::story || practice)
		? stage < 9U : stage == 0x0FU;
	if(!stage_valid || (practice && stage != metadata.practice->stage)) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 checkpoint directory contains an invalid stage",
			offset
		));
	}
	if(
		init.game_mode != metadata.game_mode || init.rank != metadata.rank ||
		init.key_mode != metadata.key_mode ||
		init.is_cpu != metadata.is_cpu || init.autofire_mask != metadata.autofire ||
		state.round_id != round
	) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 checkpoint identity disagrees with the header or directory",
			offset
		));
	}
	const auto expected_p2 = metadata.story
		? std::bit_cast<std::int8_t>(metadata.story->stage_opponents[stage])
		: metadata.paletted_character[1];
	if(
		init.paletted_character[0] != metadata.paletted_character[0] ||
		init.paletted_character[1] != expected_p2
	) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 checkpoint characters disagree with the scenario",
			offset
		));
	}
	if(
		(metadata.game_mode == game_mode::story || practice) &&
		init.story_stage != stage
	) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 checkpoint stage disagrees with its directory",
			offset
		));
	}
	if(metadata.story) {
		for(std::size_t index = 0U; index < stage_count; ++index) {
			if(
				std::bit_cast<std::uint8_t>(init.story_opponents[index]) !=
				metadata.story->stage_opponents[index]
			) {
				return std::unexpected(error(
					ErrorCode::invalid_value,
					"V14 checkpoint Story opponents disagree with the header",
					offset
				));
			}
		}
	}
	return {};
}

} // namespace

std::expected<ReplayDocument, Error> parse_v14_replay(
	std::span<const std::byte> bytes
)
{
	constexpr std::array<std::uint8_t, 8> magic{
		'T', '3', 'R', 'P', 'L', 'Y', '1', '4',
	};
	if(bytes.size() < replay_v14_header_size) {
		return std::unexpected(error(
			ErrorCode::truncated,
			"V14 replay is shorter than its 896-byte header"
		));
	}
	if(!detail::has_magic(bytes, magic)) {
		return std::unexpected(error(
			ErrorCode::bad_magic, "replay magic is not T3RPLY14"
		));
	}
	if(detail::u16(bytes, 0x08U) != replay_v14_version) {
		return std::unexpected(error(
			ErrorCode::unsupported_version,
			"V14 magic/version mismatch",
			0x08U
		));
	}
	if(
		detail::u16(bytes, 0x0AU) != replay_v14_header_size ||
		detail::u16(bytes, 0x0CU) != 0U
	) {
		return std::unexpected(error(
			ErrorCode::unsupported_layout,
			"unsupported V14 header or sample layout",
			0x0AU
		));
	}
	const auto flags = detail::u16(bytes, 0x0EU);
	if(
		(flags & replay_flags_required) != replay_flags_required ||
		(flags & static_cast<std::uint16_t>(~replay_flags_known)) != 0U
	) {
		return std::unexpected(error(
			ErrorCode::unsupported_layout,
			"V14 flags do not describe the finalized RLE/Charge layout",
			0x0EU
		));
	}
	const bool practice = (flags & 0x0004U) != 0U;
	auto metadata = parse_metadata(bytes, practice);
	if(!metadata) {
		return std::unexpected(metadata.error());
	}
	if(metadata->sample_count == 0U) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 replay has no logical input samples",
			0x1AU
		));
	}

	const auto snapshot_offset = detail::u32(bytes, 0x32U);
	const auto snapshot_size = detail::u32(bytes, 0x36U);
	const auto capacity = checkpoint_capacity(metadata->game_mode, practice);
	const auto input_offset = detail::u32(bytes, 0x2AU);
	const auto input_size = detail::u32(bytes, 0x2EU);
	const auto expected_input_offset = replay_v14_header_size +
		(capacity * replay_v14_checkpoint_size);
	if(
		snapshot_offset != replay_v14_header_size ||
		snapshot_size != replay_v14_checkpoint_size
	) {
		return std::unexpected(error(
			ErrorCode::unsupported_layout,
			"V14 checkpoint layout is not 896/289",
			0x32U
		));
	}
	if(input_offset != expected_input_offset) {
		return std::unexpected(error(
			ErrorCode::unsupported_layout,
			"V14 input offset disagrees with its scenario reservation",
			0x2AU
		));
	}
	if(!detail::range_fits(input_offset, input_size, bytes.size())) {
		return std::unexpected(error(
			ErrorCode::truncated,
			"V14 input stream extends past end of file",
			input_offset
		));
	}

	auto round_splits = parse_round_splits(bytes, *metadata, practice);
	if(!round_splits) {
		return std::unexpected(round_splits.error());
	}
	auto bonuses = parse_bonuses(bytes);
	if(!bonuses) {
		return std::unexpected(bonuses.error());
	}
	const ReplaySlowdown slowdown{
		.timed_frames = detail::u32(bytes, timed_frames_offset),
		.slow_frames = detail::u32(bytes, slow_frames_offset),
	};
	if(slowdown.slow_frames > slowdown.timed_frames) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 slowdown frames exceed timed frames",
			slow_frames_offset
		));
	}

	const auto reached_count = detail::u8(bytes, checkpoint_count_offset);
	if(reached_count == 0U || reached_count > capacity) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 reached-checkpoint count is invalid",
			checkpoint_count_offset
		));
	}
	std::vector<detail::CompactCheckpoint> checkpoints;
	std::vector<std::uint8_t> checkpoint_directory;
	std::vector<detail::RpyCheckpointCursor> cursors;
	checkpoints.reserve(reached_count);
	checkpoint_directory.reserve(reached_count);
	cursors.reserve(reached_count);
	for(std::size_t index = 0U; index < reached_count; ++index) {
		const auto directory_offset = checkpoint_directory_offset + index;
		const auto stage_round = detail::u8(bytes, directory_offset);
		const auto offset = static_cast<std::size_t>(snapshot_offset) +
			(index * replay_v14_checkpoint_size);
		auto checkpoint = detail::parse_compact_checkpoint(
			bytes, offset, "V14"
		);
		if(!checkpoint) {
			return std::unexpected(checkpoint.error());
		}
		if(
			checkpoint->sample_count > metadata->sample_count ||
			checkpoint->global_frame > metadata->final_frame_count ||
			checkpoint->input_size > input_size
		) {
			return std::unexpected(error(
				ErrorCode::invalid_value,
				"V14 checkpoint cursor exceeds the finalized replay",
				offset
			));
		}
		if(index == 0U) {
			if(
				checkpoint->sample_count != 0U || checkpoint->global_frame != 0U ||
				checkpoint->input_size != 0U
			) {
				return std::unexpected(error(
					ErrorCode::invalid_value,
					"V14 process anchor does not begin at cursor zero",
					offset
				));
			}
		} else {
			const auto &previous = checkpoints.back();
			if(
				checkpoint->sample_count <= previous.sample_count ||
				checkpoint->global_frame <= previous.global_frame ||
				checkpoint->input_size <= previous.input_size
			) {
				return std::unexpected(error(
					ErrorCode::invalid_value,
					"V14 checkpoint cursors are not strictly increasing",
					offset
				));
			}
		}
		auto identity = validate_checkpoint_identity(
			*checkpoint, stage_round, *metadata, practice, directory_offset
		);
		if(!identity) {
			return std::unexpected(identity.error());
		}
		checkpoints.push_back(std::move(*checkpoint));
		checkpoint_directory.push_back(stage_round);
		cursors.push_back(detail::RpyCheckpointCursor{
			.sample_count = checkpoints.back().sample_count,
			.input_size = checkpoints.back().input_size,
		});
	}

	const auto &anchor = checkpoints.front().init;
	if(
		std::bit_cast<std::uint32_t>(anchor.resident_rand) !=
			std::bit_cast<std::uint32_t>(metadata->resident_rand) ||
		anchor.random_seed != metadata->random_seed ||
		anchor.story_stage != metadata->story_stage
	) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 header and checkpoint 0 deterministic state disagree",
			snapshot_offset + detail::compact_checkpoint_prefix_size
		));
	}
	if(practice) {
		auto rules = validate_practice_anchor(*metadata->practice, anchor);
		if(!rules) {
			return std::unexpected(rules.error());
		}
		for(auto &checkpoint : checkpoints) {
			checkpoint.init.practice_rules = *rules;
		}
	} else if(anchor.round_start_snapshot->state.round_id != 0U) {
		return std::unexpected(error(
			ErrorCode::invalid_value,
			"V14 Story/VS process anchor must begin at round 0",
			snapshot_offset + detail::compact_checkpoint_prefix_size +
				detail::compact_snapshot_size
		));
	}

	auto records = detail::decode_rpy_input(
		bytes,
		static_cast<std::size_t>(input_offset),
		static_cast<std::size_t>(input_size),
		metadata->sample_count,
		"V14",
		(metadata->status == ReplayStatus::recording)
			? detail::RpyInputTermination::allow_input_packet
			: detail::RpyInputTermination::require_process_control
	);
	if(!records) {
		return std::unexpected(records.error());
	}
	auto cursor_validation = detail::validate_rpy_checkpoint_cursors(
		bytes,
		static_cast<std::size_t>(input_offset),
		static_cast<std::size_t>(input_size),
		cursors,
		"V14"
	);
	if(!cursor_validation) {
		return std::unexpected(cursor_validation.error());
	}

	ReplayDocument document{};
	document.session.source_version = replay_v14_version;
	document.session.init = checkpoints.front().init;
	document.session.records = std::move(*records);
	document.session.sample_count = metadata->sample_count;
	document.practice = practice;
	document.metadata = std::move(*metadata);
	const auto gameplay_ruleset =
		document.metadata->ruleset == ReplayRuleset::dopamine_test_v1 ?
			dopamine_test_v1_ruleset : stock_gameplay_ruleset;
	document.session.init.gameplay_ruleset = gameplay_ruleset;
	document.checkpoints.reserve(checkpoints.size());
	for(std::size_t index = 0U; index < checkpoints.size(); ++index) {
		const auto stage_round = checkpoint_directory[index];
		auto &checkpoint = checkpoints[index];
		document.checkpoints.push_back(ReplayCheckpoint{
			.stage = static_cast<std::uint8_t>(stage_round & 0x0FU),
			.round = static_cast<std::uint8_t>(stage_round >> 4U),
			.sample_index = checkpoint.sample_count,
			.global_frame = checkpoint.global_frame,
			.input_byte_cursor = checkpoint.input_size,
			.init = std::move(checkpoint.init),
		});
		document.checkpoints.back().init.gameplay_ruleset = gameplay_ruleset;
	}
	document.accelerators = parse_accelerators(
		bytes,
		static_cast<std::uint32_t>(input_offset + input_size),
		checkpoint_directory
	);
	document.expected_round_splits = std::move(*round_splits);
	document.stage_clear_bonuses = std::move(*bonuses);
	document.slowdown = slowdown;
	return document;
}

} // namespace th03::formats
