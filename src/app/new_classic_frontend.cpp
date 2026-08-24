#include "th03/app/new_classic_frontend.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/assets/decode.hpp"

namespace th03::app {
namespace {

constexpr std::size_t screen_center = present::graphics_width / 2U;
constexpr std::size_t browser_rows = 15U;
constexpr std::size_t replay_name_grid_columns = 16U;
constexpr std::size_t replay_name_grid_count = 48U;
constexpr std::size_t keyboard_space = 14U;
constexpr std::size_t keyboard_back = 30U;
constexpr std::size_t keyboard_question = 45U;
constexpr std::size_t keyboard_end = 46U;
constexpr std::uint8_t palette_fade_boundary_count = 19U;

constexpr std::array<std::array<std::uint8_t, 6U>, 9U>
replay_name_playchar_sjis{{
	{{0xE8U, 0xCBU, 0x96U, 0xB2U, 0x00U, 0x00U}},
	{{0x96U, 0xA3U, 0x96U, 0x82U, 0x00U, 0x00U}},
	{{0x96U, 0x82U, 0x97U, 0x9DU, 0x8DU, 0xB9U}},
	{{0x83U, 0x47U, 0x83U, 0x8CU, 0x83U, 0x93U}},
	{{0x8FU, 0xACU, 0x93U, 0x65U, 0x95U, 0x50U}},
	{{0x83U, 0x4AU, 0x83U, 0x69U, 0x00U, 0x00U}},
	{{0x97U, 0x9DU, 0x8DU, 0x81U, 0x8EU, 0x71U}},
	{{0x82U, 0xBFU, 0x82U, 0xE4U, 0x82U, 0xE8U}},
	{{0x96U, 0xB2U, 0x94U, 0xFCU, 0x00U, 0x00U}},
}};

[[nodiscard]] constexpr std::size_t playchar_sjis_size(
	std::size_t character
) noexcept
{
	return character == 0U || character == 1U || character == 5U ||
		character == 8U ? 4U : 6U;
}

[[nodiscard]] constexpr bool replay_name_grid_skip(
	std::size_t position
) noexcept
{
	return position == 13U || position == 15U || position == 29U ||
		position == 31U || position == 47U;
}

[[nodiscard]] constexpr char replay_name_ascii(
	std::size_t position
) noexcept
{
	if(position <= 12U) {
		return static_cast<char>('A' + position);
	}
	if(position >= 16U && position <= 28U) {
		return static_cast<char>('N' + (position - 16U));
	}
	if(position >= 32U && position <= 41U) {
		return static_cast<char>('0' + (position - 32U));
	}
	switch(position) {
	case 14U: return ' ';
	case 42U: return '.';
	case 43U: return ',';
	case 44U: return '!';
	case 45U: return '?';
	default: return '\0';
	}
}

[[nodiscard]] std::string replay_name_grid_label(std::size_t position)
{
	if(position == keyboard_space) {
		return "SP";
	}
	if(position == keyboard_back) {
		return "BS";
	}
	if(position == keyboard_end) {
		return "END";
	}
	const auto value = replay_name_ascii(position);
	return value == '\0' ? std::string{} : std::string(1U, value);
}

bool patch_put_fixed(
	const NewClassicMenuFont &font,
	present::Raster &raster,
	std::int32_t left,
	std::int32_t top,
	std::string_view text,
	std::int32_t cell_width,
	present::Index4 color
) noexcept
{
	for(const char character : text) {
		auto glyph_left = left;
		if(character == '1') {
			glyph_left += 2;
		} else if(character == 'I' && cell_width == 13) {
			glyph_left += 5;
		}
		if(!font.put_n(raster, glyph_left, top,
			std::string_view(&character, 1U), 1U, color)) {
			return false;
		}
		left += cell_width;
	}
	return true;
}

bool patch_put_timer_right(
	const NewClassicMenuFont &font,
	present::Raster &raster,
	std::int32_t right,
	std::int32_t top,
	std::string_view text,
	present::Index4 color
) noexcept
{
	if(text == "-") {
		return font.put_right(raster, right, top, text, color);
	}
	const auto width = static_cast<std::int32_t>(
		(text.size() - 2U) * 12U
	) + font.width(":.");
	auto left = right - width;
	for(const char character : text) {
		auto glyph_left = left;
		if(character == '1') {
			glyph_left += 2;
		}
		if(!font.put_n(raster, glyph_left, top,
			std::string_view(&character, 1U), 1U, color)) {
			return false;
		}
		left += (character >= '0' && character <= '9') ?
			std::int32_t{12} :
			font.width_n(std::string_view(&character, 1U), 1U);
	}
	return true;
}

[[nodiscard]] constexpr std::string_view patch_winner_label(
	std::int8_t winner
) noexcept
{
	return winner == 0 ? "1" : winner == 1 ? "2" : "-";
}

std::string source_fixed_name(const std::array<char, 8U> &name)
{
	std::string result(name.begin(), name.end());
	for(char &value : result) {
		if(value == '\0') {
			value = ' ';
		}
	}
	return result;
}

std::string source_trimmed_name(const std::array<char, 8U> &name)
{
	auto result = source_fixed_name(name);
	while(!result.empty() && result.back() == ' ') {
		result.pop_back();
	}
	return result;
}

[[nodiscard]] constexpr bool any(NewClassicUiInput input) noexcept
{
	return input.up || input.down || input.left || input.right ||
		input.confirm || input.bomb || input.cancel;
}

[[nodiscard]] std::string slot_number(std::size_t slot)
{
	char buffer[3U]{};
	static_cast<void>(std::snprintf(buffer, sizeof(buffer), "%02u",
		static_cast<unsigned>(slot)));
	return buffer;
}

[[nodiscard]] std::string decimal_score(
	const std::array<std::uint8_t, 4U> &packed
)
{
	std::string result;
	result.reserve(9U);
	bool significant = false;
	for(auto cursor = packed.rbegin(); cursor != packed.rend(); ++cursor) {
		const auto value = *cursor;
		const std::array<std::uint8_t, 2U> digits{
			static_cast<std::uint8_t>(value >> 4U),
			static_cast<std::uint8_t>(value & 0x0FU),
		};
		for(const auto digit : digits) {
			if(digit >= 10U) {
				significant = true;
				result.push_back('?');
				continue;
			}
			significant = significant || digit != 0U;
			result.push_back(significant ? static_cast<char>('0' + digit) : ' ');
		}
	}
	result.push_back('0');
	return result;
}

[[nodiscard]] int packed_score_compare(
	const std::array<std::uint8_t, 4U> &left,
	const std::array<std::uint8_t, 4U> &right
) noexcept
{
	for(std::size_t index = left.size(); index-- != 0U;) {
		if(left[index] != right[index]) {
			return left[index] < right[index] ? -1 : 1;
		}
	}
	return 0;
}

[[nodiscard]] std::array<std::uint8_t, 4U> replay_list_score(
	const formats::ReplayDocument &document,
	std::optional<std::uint8_t> local_player = std::nullopt
) noexcept
{
	if(!document.metadata || document.practice ||
		document.metadata->game_mode == game_mode::story) {
		return document.metadata ? document.metadata->final_score :
			std::array<std::uint8_t, 4U>{};
	}
	const auto found = std::find_if(
		document.expected_round_splits.rbegin(),
		document.expected_round_splits.rend(),
		[](const formats::ReplayRoundSplit &split) {
			return split.story_stage == 0x0FU;
		}
	);
	if(found == document.expected_round_splits.rend()) {
		return document.metadata->final_score;
	}
	if(local_player && document.metadata->game_mode == game_mode::vs_1p_2p &&
		*local_player <= 1U) {
		return *local_player == 0U ? found->p1_score : found->p2_score;
	}
	if(document.metadata->game_mode == game_mode::vs_1p_cpu) {
		return found->p1_score;
	}
	if(document.metadata->game_mode == game_mode::vs_1p_2p &&
		document.metadata->final_winner &&
		*document.metadata->final_winner >= 0 &&
		*document.metadata->final_winner <= 1) {
		return *document.metadata->final_winner == 0 ?
			found->p1_score : found->p2_score;
	}
	return packed_score_compare(found->p2_score, found->p1_score) > 0 ?
		found->p2_score : found->p1_score;
}

[[nodiscard]] std::string catalog_marker(
	const ReplayCatalogSlot &slot,
	const NewClassicMessageCatalog &messages
)
{
	if(slot.practice) {
		const auto stage = std::to_string(slot.practice_stage + 1U);
		const auto round = std::to_string(slot.practice_round + 1U);
		const std::array arguments{
			NewClassicMessageArgument{"stage", stage},
			NewClassicMessageArgument{"round", round},
		};
		return messages.format(
			NewClassicMessageId::replay_marker_practice, arguments
		);
	}
	if(slot.game_mode != game_mode::story) {
		return std::string{
			messages.text(NewClassicMessageId::replay_marker_vs)
		};
	}
	if(slot.end_reason == formats::ReplayEndReason::complete) {
		return std::string{
			messages.text(NewClassicMessageId::replay_marker_all)
		};
	}
	return slot.stage_reached == 0U ? "-" :
		std::to_string(slot.stage_reached);
}

[[nodiscard]] constexpr std::string_view rank_label(std::uint8_t rank) noexcept
{
	constexpr std::array<std::string_view, 4U> labels{
		"EASY", "NORMAL", "HARD", "LUNATIC",
	};
	return rank < labels.size() ? labels[rank] : "UNKNOWN";
}

[[nodiscard]] constexpr std::string_view character_label(
	std::int8_t paletted_character
) noexcept
{
	constexpr std::array<std::string_view, 9U> labels{
		"REIMU", "MIMA", "MARISA", "ELLEN", "KOTOHIME",
		"KANA", "RIKAKO", "CHIYURI", "YUMEMI",
	};
	if(paletted_character < 1 || paletted_character > 18) {
		return "UNKNOWN";
	}
	return labels[static_cast<std::size_t>(paletted_character - 1) / 2U];
}

[[nodiscard]] std::string_view patch_rank_label(
	std::uint8_t rank,
	const NewClassicMessageCatalog &messages
) noexcept
{
	constexpr std::array<NewClassicMessageId, 4U> labels{
		NewClassicMessageId::value_easy,
		NewClassicMessageId::value_normal,
		NewClassicMessageId::value_hard,
		NewClassicMessageId::value_lunatic,
	};
	return rank < labels.size() ? messages.text(labels[rank]) : "?";
}

[[nodiscard]] std::string_view patch_character_label(
	std::int8_t paletted_character,
	const NewClassicMessageCatalog &messages
) noexcept
{
	constexpr std::array<NewClassicMessageId, 9U> labels{
		NewClassicMessageId::replay_character_reimu,
		NewClassicMessageId::replay_character_mima,
		NewClassicMessageId::replay_character_marisa,
		NewClassicMessageId::replay_character_ellen,
		NewClassicMessageId::replay_character_kotohime,
		NewClassicMessageId::replay_character_kana,
		NewClassicMessageId::replay_character_rikako,
		NewClassicMessageId::replay_character_chiyuri,
		NewClassicMessageId::replay_character_yumemi,
	};
	if(paletted_character < 1 || paletted_character > 18) {
		return "-";
	}
	return messages.text(
		labels[static_cast<std::size_t>(paletted_character - 1) / 2U]
	);
}

[[nodiscard]] std::optional<std::int32_t> patch_put_online_category(
	const NewClassicMenuFont &font,
	present::Raster &raster,
	const NewClassicMessageCatalog &messages,
	std::uint8_t rank,
	std::uint8_t character,
	std::int32_t top,
	present::Index4 navigation_color = 9U
) noexcept
{
	if(rank >= 4U || character > new_classic_online_overall_character) {
		return std::nullopt;
	}
	std::int32_t rank_width = 0;
	for(std::uint8_t value = 0U; value < 4U; ++value) {
		rank_width = std::max(
			rank_width, font.width(patch_rank_label(value, messages))
		);
	}
	std::int32_t character_width = 0;
	for(std::uint8_t value = 0U;
		value <= new_classic_online_overall_character; ++value) {
		const auto label = value == new_classic_online_overall_character ?
			messages.text(NewClassicMessageId::hiscore_online_overall) :
			patch_character_label(
				static_cast<std::int8_t>((value * 2U) + 1U), messages
			);
		character_width = std::max(character_width, font.width(
			label
		));
	}
	const auto scope = messages.text(NewClassicMessageId::hiscore_online_scope);
	constexpr std::string_view separator{" - "};
	constexpr std::string_view spacing{" "};
	const auto total_width = font.width(scope) +
		(font.width(separator) * 2) + rank_width +
		font.width("<") + (font.width(spacing) * 2) +
		character_width + font.width(">");
	auto left = static_cast<std::int32_t>(screen_center) - (total_width / 2);
	if(!font.put(raster, left, top, scope, 15U)) {
		return std::nullopt;
	}
	left += font.width(scope);
	if(!font.put(raster, left, top, separator, 15U)) {
		return std::nullopt;
	}
	left += font.width(separator);
	const auto rank_center = left + (rank_width / 2);
	const auto rank_label = patch_rank_label(rank, messages);
	if(!font.put(raster, rank_center - (font.width(rank_label) / 2), top,
		rank_label, 15U)) {
		return std::nullopt;
	}
	left += rank_width;
	if(!font.put(raster, left, top, separator, 15U)) {
		return std::nullopt;
	}
	left += font.width(separator);
	if(!font.put(raster, left, top, "<", navigation_color)) {
		return std::nullopt;
	}
	left += font.width("<") + font.width(spacing);
	const auto character_label = character == new_classic_online_overall_character ?
		messages.text(NewClassicMessageId::hiscore_online_overall) :
		patch_character_label(
			static_cast<std::int8_t>((character * 2U) + 1U), messages
		);
	if(!font.put(raster,
		left + ((character_width - font.width(character_label)) / 2), top,
		character_label, 15U)) {
		return std::nullopt;
	}
	left += character_width + font.width(spacing);
	if(!font.put(raster, left, top, ">", navigation_color)) {
		return std::nullopt;
	}
	return rank_center;
}

[[nodiscard]] std::string_view patch_end_reason_label(
	formats::ReplayEndReason reason,
	const NewClassicMessageCatalog &messages
) noexcept
{
	switch(reason) {
	case formats::ReplayEndReason::complete:
		return messages.text(NewClassicMessageId::replay_end_clear);
	case formats::ReplayEndReason::game_over:
		return messages.text(NewClassicMessageId::replay_end_game_over);
	case formats::ReplayEndReason::menu_return:
		return messages.text(NewClassicMessageId::replay_end_menu_return);
	case formats::ReplayEndReason::input_end:
		return messages.text(NewClassicMessageId::replay_end_input_end);
	case formats::ReplayEndReason::partial:
		return messages.text(NewClassicMessageId::replay_end_partial);
	case formats::ReplayEndReason::error:
		return messages.text(NewClassicMessageId::replay_end_error);
	case formats::ReplayEndReason::none:
		return messages.text(NewClassicMessageId::replay_end_none);
	}
	return messages.text(NewClassicMessageId::replay_end_none);
}

[[nodiscard]] bool replay_upload_supported(
	const formats::ReplayDocument &document
) noexcept
{
	if(!document.metadata || document.practice ||
		document.metadata->status != formats::ReplayStatus::finalized ||
		document.metadata->ruleset != formats::ReplayRuleset::stock) {
		return false;
	}
	if(document.metadata->game_mode == game_mode::story) {
		return true;
	}
	return document.metadata->game_mode == game_mode::vs_1p_2p &&
		document.metadata->recording_flags ==
			formats::replay_recording_flag_netplay &&
		document.metadata->recorder_source ==
			formats::ReplayRecorderSource::native &&
		(document.metadata->recorder_role ==
			formats::ReplayRecorderRole::player_one ||
		 document.metadata->recorder_role ==
			formats::ReplayRecorderRole::player_two);
}

[[nodiscard]] constexpr std::string_view end_reason_label(
	formats::ReplayEndReason reason
) noexcept
{
	switch(reason) {
	case formats::ReplayEndReason::complete: return "COMPLETE";
	case formats::ReplayEndReason::menu_return: return "MENU RETURN";
	case formats::ReplayEndReason::input_end: return "INPUT END";
	case formats::ReplayEndReason::partial: return "PARTIAL";
	case formats::ReplayEndReason::error: return "ERROR";
	case formats::ReplayEndReason::game_over: return "GAME OVER";
	case formats::ReplayEndReason::none: return "IN PROGRESS";
	}
	return "UNKNOWN";
}

[[nodiscard]] std::string dos_date_label(std::uint16_t packed)
{
	if(packed == 0U) {
		return "-";
	}
	char buffer[11U]{};
	static_cast<void>(std::snprintf(
		buffer, sizeof(buffer), "%02u-%02u-%04u",
		static_cast<unsigned>((packed >> 5U) & 0x0FU),
		static_cast<unsigned>(packed & 0x1FU),
		static_cast<unsigned>(1980U + (packed >> 9U))
	));
	return buffer;
}

[[nodiscard]] std::string slowdown_label(
	const formats::ReplayDocument &document
)
{
	if(!document.slowdown || document.slowdown->timed_frames == 0U) {
		return "-";
	}
	const auto percent = std::min<std::uint64_t>(
		100U,
		(static_cast<std::uint64_t>(document.slowdown->slow_frames) * 100U) /
		document.slowdown->timed_frames
	);
	return std::to_string(percent) + "%";
}

[[nodiscard]] const formats::ReplayRoundSplit *round_split(
	const formats::ReplayDocument &document,
	const formats::ReplayCheckpoint &checkpoint
) noexcept
{
	const auto found = std::find_if(
		document.expected_round_splits.begin(),
		document.expected_round_splits.end(),
		[&checkpoint](const formats::ReplayRoundSplit &split) {
			return split.story_stage == checkpoint.stage &&
				split.round_id == checkpoint.round;
		}
	);
	return found == document.expected_round_splits.end() ? nullptr : &*found;
}

[[nodiscard]] std::string q4_label(std::uint8_t value)
{
	const auto fraction = static_cast<std::uint16_t>((value & 0x0FU) * 625U);
	auto result = std::to_string(value >> 4U) + ".";
	result += static_cast<char>('0' + ((fraction / 1000U) % 10U));
	result += static_cast<char>('0' + ((fraction / 100U) % 10U));
	result += static_cast<char>('0' + ((fraction / 10U) % 10U));
	result += static_cast<char>('0' + (fraction % 10U));
	return result;
}

enum class ReplayDetailRowKind : std::uint8_t {
	stage,
	round,
};

struct ReplayDetailRow final {
	std::string label{};
	std::optional<std::size_t> checkpoint{};
	ReplayDetailRowKind kind = ReplayDetailRowKind::stage;
	std::uint8_t stage = 0U;
	std::uint8_t stage_checkpoint_count = 0U;
};

[[nodiscard]] std::vector<ReplayDetailRow> story_detail_rows(
	const formats::ReplayDocument &document,
	NewClassicReplayDetailPage page
)
{
	std::vector<ReplayDetailRow> rows;
	const auto *metadata = document.metadata ? &*document.metadata : nullptr;
	for(std::size_t stage = 0U; stage < 9U; ++stage) {
		std::vector<std::size_t> checkpoints;
		for(std::size_t index = 0U; index < document.checkpoints.size(); ++index) {
			if(document.checkpoints[index].stage == stage) {
				checkpoints.push_back(index);
			}
		}
		const bool reached = metadata && metadata->story &&
			stage < metadata->stage_reached_count;
		const auto opponent = reached &&
			metadata->story->stage_opponents[stage] != 0U ?
			std::string(character_label(static_cast<std::int8_t>(
				metadata->story->stage_opponents[stage]
			))) : std::string{"-"};
		const auto stage_prefix = "S" + std::to_string(stage + 1U) + " " +
			opponent + " ";
		if(page == NewClassicReplayDetailPage::timers) {
			std::uint32_t total = 0U;
			bool valid = false;
			for(const auto index : checkpoints) {
				if(const auto *split = round_split(document, document.checkpoints[index]);
					split && split->real_frames) {
					total += *split->real_frames;
					valid = true;
				}
			}
			if(checkpoints.size() != 1U) {
				rows.push_back({
					.label = stage_prefix + (valid ?
						new_classic_replay_timer_label(total) : "-"),
					.kind = ReplayDetailRowKind::stage,
					.stage = static_cast<std::uint8_t>(stage),
					.stage_checkpoint_count = static_cast<std::uint8_t>(
						checkpoints.size()
					),
				});
			}
			for(const auto index : checkpoints) {
				const auto *split = round_split(document, document.checkpoints[index]);
				const auto value = split && split->real_frames ?
					new_classic_replay_timer_label(*split->real_frames) : "-";
				rows.push_back({
					.label = checkpoints.size() == 1U ? stage_prefix + value :
						"   ROUND " + std::to_string(
							document.checkpoints[index].round + 1U
						) + " " + value,
					.checkpoint = index,
					.kind = checkpoints.size() == 1U ?
						ReplayDetailRowKind::stage : ReplayDetailRowKind::round,
					.stage = static_cast<std::uint8_t>(stage),
					.stage_checkpoint_count = static_cast<std::uint8_t>(
						checkpoints.size()
					),
				});
			}
			continue;
		}
		if(checkpoints.empty()) {
			rows.push_back({
				.label = stage_prefix + "-",
				.kind = ReplayDetailRowKind::stage,
				.stage = static_cast<std::uint8_t>(stage),
				.stage_checkpoint_count = 0U,
			});
			continue;
		}
		if(checkpoints.size() > 1U) {
			rows.push_back({
				.label = stage_prefix + "-",
				.kind = ReplayDetailRowKind::stage,
				.stage = static_cast<std::uint8_t>(stage),
				.stage_checkpoint_count = static_cast<std::uint8_t>(
					checkpoints.size()
				),
			});
		}
		for(const auto index : checkpoints) {
			const auto *split = round_split(document, document.checkpoints[index]);
			const auto value = checkpoints.size() == 1U && reached ?
				decimal_score(metadata->story->stage_scores[stage]) :
				split ? decimal_score(split->p1_score) : "-";
			rows.push_back({
				.label = checkpoints.size() == 1U ? stage_prefix + value :
					"   ROUND " + std::to_string(
						document.checkpoints[index].round + 1U
					) + " " + value,
				.checkpoint = index,
				.kind = checkpoints.size() == 1U ?
					ReplayDetailRowKind::stage : ReplayDetailRowKind::round,
				.stage = static_cast<std::uint8_t>(stage),
				.stage_checkpoint_count = static_cast<std::uint8_t>(
					checkpoints.size()
				),
			});
		}
	}
	return rows;
}

[[nodiscard]] bool clear_bonus_valid(
	const formats::ReplayStageClearBonus &bonus
) noexcept
{
	return std::any_of(bonus.total.begin(), bonus.total.end(),
		[](std::uint8_t value) { return value != 0U; }) ||
		bonus.max_combo != 0U || bonus.gauge_attacks != 0U ||
		bonus.boss_attacks != 0U || bonus.boss_reversals != 0U ||
		bonus.boss_panics != 0U || bonus.remaining_lives != 0U;
}

[[nodiscard]] ReplayCatalogSlot catalog_slot(
	const formats::ReplayDocument &document,
	std::optional<ReplaySlotLocalView> local_view
) noexcept
{
	ReplayCatalogSlot slot{
		.state = ReplayCatalogSlotState::ready,
		.checkpoint_count = static_cast<std::uint8_t>(std::min<std::size_t>(
			document.checkpoints.size(), 0xFFU
		)),
	};
	slot.practice = document.practice;
	slot.netplay = local_view &&
		local_view->origin == ReplaySlotLocalOrigin::netplay;
	slot.local_player = slot.netplay ? local_view->local_player : 0U;
	slot.display_paletted_character = document.session.init.paletted_character[
		slot.local_player
	];
	if(document.metadata) {
		slot.name = document.metadata->name;
		slot.game_mode = document.metadata->game_mode;
		slot.rank = document.metadata->rank;
		slot.stage_reached = document.metadata->stage_reached_count;
		slot.final_score = replay_list_score(
			document,
			slot.netplay ? std::optional<std::uint8_t>{slot.local_player} :
				std::nullopt
		);
		slot.end_reason = document.metadata->end_reason;
		if(document.metadata->practice) {
			slot.practice_stage = document.metadata->practice->stage;
			slot.practice_round = document.metadata->practice->round;
		}
	} else {
		slot.game_mode = document.session.init.game_mode;
		slot.rank = document.session.init.rank;
	}
	return slot;
}

} // namespace

NewClassicUiInput new_classic_text_editor_ui(
	const LogicalTickInput &sample,
	NewClassicTextEditorCommand command
) noexcept
{
	return {
		.up = command == NewClassicTextEditorCommand::up,
		.down = command == NewClassicTextEditorCommand::down,
		.left = command == NewClassicTextEditorCommand::left,
		.right = command == NewClassicTextEditorCommand::right,
		.confirm = command == NewClassicTextEditorCommand::confirm,
		.cancel = command == NewClassicTextEditorCommand::cancel,
		.app_quit_requested = sample.app_quit_requested,
	};
}

NewClassicReplaySaveArt::NewClassicReplaySaveArt(
	present::PreparedSuperimposeSheet normal,
	present::PreparedSuperimposeSheet highlight
) noexcept :
	normal_(std::move(normal)),
	highlight_(std::move(highlight))
{}

std::expected<
	std::shared_ptr<const NewClassicReplaySaveArt>,
	NewClassicReplaySaveArtError
> NewClassicReplaySaveArt::prepare(
	std::span<const std::byte> normal_bfnt,
	std::span<const std::byte> highlight_bfnt,
	std::span<const std::byte> font_rom
)
{
	auto normal = assets::decode_bfnt(
		normal_bfnt, assets::BfntConsumerPolicy::superimpose
	);
	if(!normal) {
		return std::unexpected(NewClassicReplaySaveArtError{
			.code = NewClassicReplaySaveArtErrorCode::normal_glyphs_failed,
			.asset_error = std::move(normal.error()),
		});
	}
	auto highlight = assets::decode_bfnt(
		highlight_bfnt, assets::BfntConsumerPolicy::superimpose
	);
	if(!highlight) {
		return std::unexpected(NewClassicReplaySaveArtError{
			.code = NewClassicReplaySaveArtErrorCode::highlight_glyphs_failed,
			.asset_error = std::move(highlight.error()),
		});
	}
	auto normal_prepared = present::prepare_superimpose_sheet(*normal);
	auto highlight_prepared = present::prepare_superimpose_sheet(*highlight);
	const auto valid = [](const present::PreparedSuperimposeSheet &sheet) {
		return sheet.first_code() == 0U && sheet.last_code() == 48U &&
			sheet.pixel_width() == 32U && sheet.pixel_height() == 32U;
	};
	if(!normal_prepared || !highlight_prepared || !valid(*normal_prepared) ||
		!valid(*highlight_prepared) ||
		!present::superimpose_remap_color_preserve_opacity(
			*normal_prepared, 10U, 15U
		) || !present::superimpose_remap_color_preserve_opacity(
			*highlight_prepared, 15U, 0U
		) || !present::superimpose_fill_glyph_rect_preserve_opacity(
			*normal_prepared, 13U, {8, 13, 16U, 1U}, 3U
		) || !present::superimpose_fill_glyph_rect_preserve_opacity(
			*normal_prepared, 13U, {8, 14, 16U, 4U}, 15U
		) || !present::superimpose_fill_glyph_rect_preserve_opacity(
			*normal_prepared, 13U, {8, 18, 16U, 1U}, 3U
		)) {
		return std::unexpected(NewClassicReplaySaveArtError{
			.code = NewClassicReplaySaveArtErrorCode::invalid_glyph_geometry,
		});
	}
	auto font = Pc98FontRomProvider::from_exact_font_rom(font_rom);
	if(!font) {
		return std::unexpected(NewClassicReplaySaveArtError{
			.code = NewClassicReplaySaveArtErrorCode::font_failed,
			.font_error = std::move(font.error()),
		});
	}
	auto art = std::shared_ptr<NewClassicReplaySaveArt>(
		new NewClassicReplaySaveArt(
			std::move(*normal_prepared), std::move(*highlight_prepared)
		)
	);
	for(std::size_t character = 0U;
		character < replay_name_playchar_sjis.size(); ++character) {
		auto prepared = present::prepare_graphics_text_8x16(
			std::span<const std::uint8_t>(replay_name_playchar_sjis[character])
				.first(playchar_sjis_size(character)),
			*font
		);
		if(!prepared) {
			return std::unexpected(NewClassicReplaySaveArtError{
				.code = NewClassicReplaySaveArtErrorCode::character_name_failed,
				.text_error = std::move(prepared.error()),
			});
		}
		art->character_names_[character].emplace(std::move(*prepared));
	}
	constexpr std::array<std::uint8_t, 1U> question{{'?'}};
	auto unknown = present::prepare_graphics_text_8x16(question, *font);
	if(!unknown) {
		return std::unexpected(NewClassicReplaySaveArtError{
			.code = NewClassicReplaySaveArtErrorCode::character_name_failed,
			.text_error = std::move(unknown.error()),
		});
	}
	art->character_names_[9U].emplace(std::move(*unknown));
	return std::shared_ptr<const NewClassicReplaySaveArt>{std::move(art)};
}

std::string new_classic_replay_timer_label(std::uint32_t real_frames)
{
	const auto minutes = real_frames / 3'384U;
	const auto remainder = static_cast<std::uint16_t>(real_frames % 3'384U);
	const auto centiseconds = static_cast<std::uint16_t>(
		(static_cast<std::uint32_t>(remainder) * 1'000U) / 564U
	);
	char buffer[24U]{};
	static_cast<void>(std::snprintf(
		buffer, sizeof(buffer), "%u:%02u.%02u",
		static_cast<unsigned>(minutes),
		static_cast<unsigned>(centiseconds / 100U),
		static_cast<unsigned>(centiseconds % 100U)
	));
	return buffer;
}

std::expected<NewClassicReplayCatalog, ReplaySlotStoreError>
scan_new_classic_replays(const std::filesystem::path &replay_root)
{
	if(replay_root.empty()) {
		return std::unexpected(ReplaySlotStoreError{
			.code = ReplaySlotStoreErrorCode::invalid_root,
		});
	}
	NewClassicReplayCatalog result{};
	for(std::size_t slot = 0U; slot < replay_slot_count; ++slot) {
		auto loaded = load_replay_slot(
			replay_root, static_cast<std::uint8_t>(slot)
		);
		if(!loaded) {
			result.slots[slot].state = ReplayCatalogSlotState::invalid;
			continue;
		}
		if(loaded->document) {
			result.slots[slot] = catalog_slot(
				*loaded->document, loaded->local_view
			);
		}
	}
	return result;
}

std::expected<std::unique_ptr<NewClassicReplayBrowser>, ReplaySlotStoreError>
NewClassicReplayBrowser::load(
	std::filesystem::path replay_root,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	NewClassicReplayBrowserMode mode
)
{
	auto catalog = scan_new_classic_replays(replay_root);
	if(!catalog) {
		return std::unexpected(std::move(catalog.error()));
	}
	return std::unique_ptr<NewClassicReplayBrowser>(
		new NewClassicReplayBrowser(
			std::move(replay_root), std::move(*catalog), std::move(patch_assets),
			mode
		)
	);
}

std::unique_ptr<NewClassicReplayBrowser> NewClassicReplayBrowser::detail(
	formats::ReplayDocument document,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets
)
{
	auto browser = std::unique_ptr<NewClassicReplayBrowser>(
		new NewClassicReplayBrowser(
			{}, NewClassicReplayCatalog{}, std::move(patch_assets),
			NewClassicReplayBrowserMode::playback
		)
	);
	browser->catalog_.slots[0U] = catalog_slot(document, std::nullopt);
	browser->selected_slot_ = 0U;
	browser->top_slot_ = 0U;
	browser->selected_checkpoint_ = 0U;
	browser->detail_page_ = NewClassicReplayDetailPage::splits;
	browser->detail_document_ = std::move(document);
	browser->checkpoint_focus_ = true;
	browser->fade_in_active_ = false;
	browser->fade_boundary_ = 0U;
	browser->detail_only_ = true;
	browser->input_allowed_ = true;
	browser->draw();
	browser->patch_surface_.raster().set_palette_tone(100);
	browser->patch_surface_.raster().apply_pending_palette();
	return browser;
}

NewClassicReplayBrowser::NewClassicReplayBrowser(
	std::filesystem::path replay_root,
	NewClassicReplayCatalog catalog,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	NewClassicReplayBrowserMode mode
) :
	replay_root_(std::move(replay_root)),
	catalog_(std::move(catalog)),
	mode_(mode),
	patch_assets_(std::move(patch_assets))
{
	const auto first = std::find_if(
		catalog_.slots.begin(), catalog_.slots.end(),
		[](const ReplayCatalogSlot &slot) {
			return slot.state == ReplayCatalogSlotState::ready;
		}
	);
	if(first != catalog_.slots.end()) {
		selected_slot_ = static_cast<std::uint8_t>(
			first - catalog_.slots.begin()
		);
		top_slot_ = static_cast<std::uint8_t>(std::min<std::size_t>(
			selected_slot_, replay_slot_count - browser_rows
		));
	}
	draw();
	patch_surface_.raster().set_palette_tone(0);
	patch_surface_.raster().apply_pending_palette();
	fade_in_active_ = true;
}

void NewClassicReplayBrowser::draw() noexcept
{
	draw_patch();
}
void NewClassicReplayBrowser::draw_patch() noexcept
{
	if(!patch_assets_ ||
		!patch_surface_.begin_hidden_frame(
			patch_assets_->replay_list_background
		)) {
		return;
	}
	auto &raster = patch_surface_.raster();
	static_cast<void>(raster.set_pending_palette_entry(12U, {15U, 15U, 0U}));
	raster.apply_pending_palette();
	const auto &font = patch_assets_->menu_font;
	const auto &messages = patch_assets_->messages();
	const auto put = [&](std::int32_t left, std::int32_t top,
		std::string_view text, present::Index4 color) {
		return font.put(raster, left, top, text, color);
	};
	const auto put_right = [&](std::int32_t right, std::int32_t top,
		std::string_view text, present::Index4 color) {
		return font.put_right(raster, right, top, text, color);
	};
	if(upload_pending_ || upload_feedback_) {
		bool drawn = raster.clear({104, 144, 432U, 96U}, 0U);
		NewClassicMessageId message = NewClassicMessageId::replay_upload_pending;
		if(upload_feedback_) {
			switch(*upload_feedback_) {
			case NewClassicReplayUploadOutcome::accepted:
				message = NewClassicMessageId::replay_upload_accepted;
				break;
			case NewClassicReplayUploadOutcome::duplicate:
				message = NewClassicMessageId::replay_upload_duplicate;
				break;
			case NewClassicReplayUploadOutcome::slowdown:
				message = NewClassicMessageId::replay_upload_slowdown;
				break;
			case NewClassicReplayUploadOutcome::verification_failed:
				message = NewClassicMessageId::replay_upload_verification_failed;
				break;
			case NewClassicReplayUploadOutcome::ineligible:
				message = NewClassicMessageId::replay_upload_ineligible;
				break;
			case NewClassicReplayUploadOutcome::failed:
				message = NewClassicMessageId::replay_upload_failed;
				break;
			case NewClassicReplayUploadOutcome::unavailable:
				message = NewClassicMessageId::replay_upload_unavailable;
				break;
			}
		}
		drawn = drawn && font.put_centered(
			raster, 320, 176, messages.text(message), upload_pending_ ? 9U : 13U
		);
		if(!upload_pending_) {
			drawn = drawn && font.put_centered(
				raster, 320, 208,
				messages.text(NewClassicMessageId::replay_upload_dismiss), 15U
			);
		}
		static_cast<void>(drawn);
		static_cast<void>(patch_surface_.publish(raster.access_page()));
		return;
	}
	if(!detail_document_) {
		bool drawn = true;
		drawn = drawn && put(64, 80, messages.text(
			NewClassicMessageId::replay_header_slot), 9U) &&
			put(104, 80, messages.text(
				NewClassicMessageId::replay_header_name), 9U) &&
			put_right(340, 80, messages.text(
				NewClassicMessageId::replay_header_score), 9U) &&
			put(364, 80, messages.text(
				NewClassicMessageId::replay_header_character), 9U) &&
			put(476, 80, messages.text(
				NewClassicMessageId::replay_header_rank), 9U) &&
			put(560, 80, messages.text(
				NewClassicMessageId::replay_header_stage), 9U);
		for(std::size_t row = 0U; drawn && row < browser_rows; ++row) {
			const auto slot = static_cast<std::size_t>(top_slot_) + row;
			if(slot >= replay_slot_count) {
				break;
			}
			const auto &entry = catalog_.slots[slot];
			const auto color = static_cast<present::Index4>(
				slot == selected_slot_ ? 12U : 15U);
			const auto top = static_cast<std::int32_t>(96U + (row * 16U));
			if(slot == selected_slot_) {
				drawn = put(40, top, ">", 12U);
			}
			drawn = drawn && patch_put_fixed(
				font, raster, 64, top, slot_number(slot), 12, color
			);
			if(entry.state != ReplayCatalogSlotState::ready) {
				drawn = drawn && put(104, top, messages.text(
					NewClassicMessageId::replay_none), color);
				continue;
			}
			drawn = drawn && patch_put_fixed(
				font, raster, 104, top, source_fixed_name(entry.name), 13, color
			);
			const auto score = decimal_score(entry.final_score);
			drawn = drawn && patch_put_fixed(
				font, raster,
				340 - static_cast<std::int32_t>(score.size() * 12U), top,
				score, 12, color
			) && put(364, top,
				patch_character_label(
					entry.display_paletted_character, messages
				), color) &&
				put(476, top, patch_rank_label(entry.rank, messages), color) &&
				patch_put_fixed(
					font, raster, 560, top, catalog_marker(entry, messages), 12, color
				);
		}
		static_cast<void>(drawn);
		static_cast<void>(patch_surface_.publish(raster.access_page()));
		return;
	}

	const auto &document = *detail_document_;
	const auto &entry = catalog_.slots[selected_slot_];
	const auto *metadata = document.metadata ? &*document.metadata : nullptr;
	const auto trimmed_name = source_trimmed_name(entry.name);
	static_cast<void>(put(48, 80, trimmed_name, 15U));
	const auto slot = slot_number(selected_slot_);
	const std::array slot_argument{NewClassicMessageArgument{"slot", slot}};
	static_cast<void>(put_right(304, 80, messages.format(
		NewClassicMessageId::replay_detail_slot, slot_argument
	), 9U));
	std::int32_t tab_left = 328;
	auto tab = [&](std::string_view label, bool selected) {
		const auto ok = put(tab_left, 80, label, selected ? 12U : 9U);
		tab_left += font.width(label);
		return ok;
	};
	static_cast<void>(tab(messages.text(
		NewClassicMessageId::replay_tab_splits), detail_page_ ==
		NewClassicReplayDetailPage::splits));
	static_cast<void>(tab(" / ", false));
	static_cast<void>(tab(messages.text(
		NewClassicMessageId::replay_tab_clears), detail_page_ ==
		NewClassicReplayDetailPage::clear_bonuses));
	static_cast<void>(tab(" / ", false));
	static_cast<void>(tab(messages.text(
		NewClassicMessageId::replay_tab_timers), detail_page_ ==
		NewClassicReplayDetailPage::timers));
	if(metadata) {
		const auto story = metadata->game_mode == game_mode::story &&
			!document.practice;
		const auto score_label = metadata->game_mode == game_mode::vs_1p_cpu ?
			messages.text(NewClassicMessageId::replay_label_p1_score) :
			(story || document.practice || entry.netplay) ? messages.text(
				NewClassicMessageId::replay_label_final_score) : messages.text(
					NewClassicMessageId::replay_label_win_score);
		std::string mode;
		if(document.practice) {
			mode = messages.text(NewClassicMessageId::replay_mode_practice);
		} else if(story) {
			const auto reason = patch_end_reason_label(
				metadata->end_reason, messages
			);
			const std::array arguments{
				NewClassicMessageArgument{"reason", reason},
			};
			mode = messages.format(
				NewClassicMessageId::replay_mode_story, arguments
			);
		} else {
			const auto vs_mode = messages.text(
				entry.netplay ? NewClassicMessageId::replay_mode_netplay :
				metadata->game_mode == game_mode::vs_1p_cpu ?
					NewClassicMessageId::replay_mode_1p_cpu :
				metadata->game_mode == game_mode::vs_1p_2p ?
					NewClassicMessageId::replay_mode_1p_2p :
					NewClassicMessageId::replay_mode_cpu_cpu
			);
			const std::array arguments{
				NewClassicMessageArgument{"mode", vs_mode},
			};
			mode = messages.format(
				NewClassicMessageId::replay_mode_vs, arguments
			);
		}
		static_cast<void>(put(48, 96, mode, 9U));
		static_cast<void>(put(48, 112, score_label, 15U));
		const auto score = decimal_score(entry.final_score);
		static_cast<void>(patch_put_fixed(
			font, raster, 304 - static_cast<std::int32_t>(score.size() * 12U),
			112, score, 12, 15U
		));
		static_cast<void>(put(48, 128, messages.text(
			NewClassicMessageId::replay_label_date), 15U));
		static_cast<void>(put_right(304, 128,
			dos_date_label(metadata->dos_date), 15U));
		static_cast<void>(put(
			48, 144,
			std::string(patch_rank_label(metadata->rank, messages)) + " " +
			std::string(patch_character_label(
				metadata->paletted_character[0U], messages
			)) +
			(story ? "" : std::string(messages.text(
				NewClassicMessageId::replay_summary_vs)) +
				std::string(patch_character_label(
					metadata->paletted_character[1U], messages
				))),
			15U
		));
		std::string context;
		if(document.practice && metadata->practice) {
			const auto stage = std::to_string(metadata->practice->stage + 1U);
			const auto round = std::to_string(metadata->practice->round + 1U);
			const auto plus = metadata->practice->round == 5U ? "+" : "";
			const std::array arguments{
				NewClassicMessageArgument{"stage", stage},
				NewClassicMessageArgument{"round", round},
				NewClassicMessageArgument{"plus", plus},
			};
			context = messages.format(
				NewClassicMessageId::replay_practice_start, arguments
			);
		} else {
			context = (metadata->autofire & 0x01U) ?
				messages.text(NewClassicMessageId::replay_autofire_on) :
				messages.text(NewClassicMessageId::replay_autofire_off);
			if(!story) {
				context += " ";
				context += (metadata->autofire & 0x02U) ?
					messages.text(NewClassicMessageId::replay_autofire_on) :
					messages.text(NewClassicMessageId::replay_autofire_off);
			}
		}
		static_cast<void>(put(48, 160, context, 15U));
		static_cast<void>(put(48, 176, messages.text(
			NewClassicMessageId::replay_label_slowdown), 15U));
		static_cast<void>(put_right(
			304, 176, slowdown_label(document), 15U
		));
		if(document.practice) {
			if(!checkpoint_focus_) {
				static_cast<void>(put(32, 192, ">", 12U));
			}
			static_cast<void>(put(
				48, 192, messages.text(
					NewClassicMessageId::replay_label_view_settings
				), checkpoint_focus_ ? 15U : 12U
			));
		}
		if(mode_ == NewClassicReplayBrowserMode::upload &&
			replay_upload_supported(document)) {
			if(upload_action_focus_) {
				static_cast<void>(put(32, 208, ">", 9U));
			}
			static_cast<void>(put(
				48, 208,
				messages.text(NewClassicMessageId::replay_upload_title),
				9U
			));
		}
	}
	const auto story_detail = metadata &&
		metadata->game_mode == game_mode::story && !document.practice;
	if(detail_page_ == NewClassicReplayDetailPage::clear_bonuses ||
		story_detail) {
		static_cast<void>(put(344, 112, messages.text(
			NewClassicMessageId::replay_column_stage), 9U));
		static_cast<void>(put(376, 112, messages.text(
			NewClassicMessageId::replay_column_opponent), 9U));
		static_cast<void>(put_right(
			592, 112,
			detail_page_ == NewClassicReplayDetailPage::clear_bonuses ?
				messages.text(NewClassicMessageId::replay_column_bonus) :
				detail_page_ == NewClassicReplayDetailPage::timers ?
					messages.text(NewClassicMessageId::replay_column_time) :
					messages.text(NewClassicMessageId::replay_column_score),
			9U
		));
	} else {
		static_cast<void>(put(328, 112, messages.text(
			NewClassicMessageId::replay_column_round), 9U));
		if(detail_page_ == NewClassicReplayDetailPage::timers) {
			static_cast<void>(put_right(568, 112, messages.text(
				NewClassicMessageId::replay_column_time), 9U));
		} else {
			static_cast<void>(put_right(452, 112, messages.text(
				NewClassicMessageId::replay_label_p1_score), 9U));
			static_cast<void>(put_right(568, 112, messages.text(
				NewClassicMessageId::replay_label_p2_score), 9U));
			static_cast<void>(put(584, 112, messages.text(
				NewClassicMessageId::replay_column_winner), 9U));
		}
	}
	if(practice_settings_modal_ && metadata && metadata->practice) {
		const auto &cfg = *metadata->practice;
		static_cast<void>(raster.clear({72, 104, 496U, 192U}, 0U));
		const auto timer = messages.text(cfg.cpu_timer == 0U ?
			NewClassicMessageId::replay_timer_vs_default :
			cfg.cpu_timer == 1U ?
				NewClassicMessageId::replay_timer_story_native :
				NewClassicMessageId::replay_timer_infinite);
		const auto format_one = [&](NewClassicMessageId id,
			std::string_view name, std::string_view value) {
			const std::array arguments{NewClassicMessageArgument{name, value}};
			return messages.format(id, arguments);
		};
		const auto format_two = [&](NewClassicMessageId id,
			std::string_view first_name, std::string_view first_value,
			std::string_view second_name, std::string_view second_value) {
			const std::array arguments{
				NewClassicMessageArgument{first_name, first_value},
				NewClassicMessageArgument{second_name, second_value},
			};
			return messages.format(id, arguments);
		};
		const auto stock = std::to_string(cfg.stock);
		const auto extends = std::to_string(cfg.extends_gained);
		const auto safety = std::to_string(cfg.initial_cpu_safety_frames);
		const auto round_speed = q4_label(cfg.round_speed);
		const auto bullet_speed = q4_label(cfg.bullet_speed);
		const auto p1_spell = std::to_string(cfg.p1_spell);
		const auto cpu_spell = std::to_string(cfg.cpu_spell);
		const auto p1_gauge = std::to_string(cfg.p1_gauge);
		const auto cpu_gauge = std::to_string(cfg.cpu_gauge);
		const auto boss_rank = std::to_string(cfg.boss_level + 1U);
		const auto cpu_damage = std::to_string(cfg.cpu_damage);
		std::array<std::string, 8U> rows{{
			cfg.stock == 0xFFU ? std::string{messages.text(
				NewClassicMessageId::replay_practice_life_vs)} : format_two(
				NewClassicMessageId::replay_practice_life,
				"stock", stock, "extends", extends),
			format_one(NewClassicMessageId::replay_practice_cpu_timer,
				"timer", timer),
			format_one(NewClassicMessageId::replay_practice_cpu_safety,
				"frames", safety),
			format_two(NewClassicMessageId::replay_practice_speeds,
				"round", round_speed, "bullet", bullet_speed),
			format_two(NewClassicMessageId::replay_practice_spell_rank,
				"p1", p1_spell, "cpu", cpu_spell),
			format_two(NewClassicMessageId::replay_practice_start_gauge,
				"p1", p1_gauge, "cpu", cpu_gauge),
			format_one(NewClassicMessageId::replay_practice_boss_rank,
				"rank", boss_rank),
			format_one(NewClassicMessageId::replay_practice_cpu_damage,
				"damage", cpu_damage),
		}};
		for(std::size_t row = 0U; row < rows.size(); ++row) {
			static_cast<void>(put(
				96, 128 + static_cast<std::int32_t>(row * 16U), rows[row], 15U
			));
		}
		static_cast<void>(patch_surface_.publish(raster.access_page()));
		return;
	}
	if(detail_page_ == NewClassicReplayDetailPage::clear_bonuses) {
		if(metadata && metadata->game_mode != game_mode::story &&
			!document.practice) {
			static_cast<void>(put(344, 128, messages.text(
				NewClassicMessageId::replay_no_clear_bonuses), 15U));
		} else {
			const auto first_stage = document.practice && metadata &&
				metadata->practice ? metadata->practice->stage : 0U;
			const auto last_stage = document.practice ?
				std::min<std::size_t>(first_stage + 1U, 9U) : 9U;
			for(std::size_t stage = first_stage; stage < last_stage; ++stage) {
				const auto top = static_cast<std::int32_t>(
					128U + ((stage - first_stage) * 16U)
				);
				static_cast<void>(patch_put_fixed(
					font, raster, 344, top, std::to_string(stage + 1U),
					12, 15U
				));
				const bool reached = metadata && metadata->story &&
					stage < metadata->stage_reached_count;
				static_cast<void>(put(
					376, top,
					reached ? patch_character_label(static_cast<std::int8_t>(
						metadata->story->stage_opponents[stage]
					), messages) : "-", 15U
				));
				const auto valid = document.stage_clear_bonuses &&
					clear_bonus_valid((*document.stage_clear_bonuses)[stage]);
				const auto score = valid ? decimal_score(
					(*document.stage_clear_bonuses)[stage].total
				) : std::string(9U, '-');
				static_cast<void>(patch_put_fixed(
					font, raster, 592 - static_cast<std::int32_t>(score.size() * 12U),
					top, score, 12, 15U
				));
			}
		}
		static_cast<void>(patch_surface_.publish(raster.access_page()));
		return;
	}
	if(story_detail) {
		const auto rows = story_detail_rows(document, detail_page_);
		auto selected_row = std::size_t{0U};
		for(std::size_t row = 0U; row < rows.size(); ++row) {
			if(rows[row].checkpoint == selected_checkpoint_) {
				selected_row = row;
				break;
			}
		}
		const auto first = selected_row >= 14U ? selected_row - 13U : 0U;
		const auto last = std::min(first + 14U, rows.size());
		for(std::size_t index = first; index < last; ++index) {
			const auto &row = rows[index];
			const auto top = static_cast<std::int32_t>(
				128U + ((index - first) * 16U)
			);
			const auto selected = checkpoint_focus_ &&
				row.checkpoint == selected_checkpoint_;
			const auto color = static_cast<present::Index4>(selected ? 12U : 15U);
			if(selected) {
				static_cast<void>(put(328, top, ">", 12U));
			}
			if(row.kind == ReplayDetailRowKind::round && row.checkpoint) {
				const auto &checkpoint = document.checkpoints[*row.checkpoint];
				const auto round = std::to_string(checkpoint.round + 1U);
				const std::array arguments{
					NewClassicMessageArgument{"round", round},
				};
				static_cast<void>(put(
					400, top, messages.format(
						NewClassicMessageId::replay_round, arguments
					),
					color
				));
				const auto *split = round_split(document, checkpoint);
				if(detail_page_ == NewClassicReplayDetailPage::timers) {
					static_cast<void>(patch_put_timer_right(
						font, raster, 592, top,
						split && split->real_frames ?
							new_classic_replay_timer_label(*split->real_frames) : "-",
						color
					));
				} else {
					const auto score = split ? decimal_score(split->p1_score) :
						std::string(9U, '-');
					static_cast<void>(patch_put_fixed(
						font, raster,
						592 - static_cast<std::int32_t>(score.size() * 12U),
						top, score, 12, color
					));
				}
				continue;
			}

			static_cast<void>(patch_put_fixed(
				font, raster, 344, top, std::to_string(row.stage + 1U),
				12, color
			));
			const auto reached = row.stage < metadata->stage_reached_count &&
				metadata->story.has_value();
			static_cast<void>(put(
				376, top,
				reached ? patch_character_label(static_cast<std::int8_t>(
					metadata->story->stage_opponents[row.stage]
				), messages) : "-",
				color
			));
			if(detail_page_ == NewClassicReplayDetailPage::timers) {
				std::uint32_t total = 0U;
				bool valid = false;
				for(const auto &checkpoint : document.checkpoints) {
					if(checkpoint.stage != row.stage) {
						continue;
					}
					if(const auto *split = round_split(document, checkpoint);
						split && split->real_frames) {
						total += *split->real_frames;
						valid = true;
					}
				}
				static_cast<void>(patch_put_timer_right(
					font, raster, 592, top,
					valid ? new_classic_replay_timer_label(total) : "-", color
				));
			} else {
				const auto score = reached && row.stage_checkpoint_count == 1U ?
					decimal_score(metadata->story->stage_scores[row.stage]) :
					std::string(9U, '-');
				static_cast<void>(patch_put_fixed(
					font, raster,
					592 - static_cast<std::int32_t>(score.size() * 12U),
					top, score, 12, color
				));
			}
		}
		static_cast<void>(patch_surface_.publish(raster.access_page()));
		return;
	}
	const auto count = document.checkpoints.size();
	const auto first = selected_checkpoint_ >= 14U ?
		selected_checkpoint_ - 13U : 0U;
	const auto last = std::min(first + 14U, count);
	for(std::size_t index = first; index < last; ++index) {
		const auto top = static_cast<std::int32_t>(128U + ((index - first) * 16U));
		const auto color = static_cast<present::Index4>(
			checkpoint_focus_ && index == selected_checkpoint_ ? 12U : 15U);
		if(checkpoint_focus_ && index == selected_checkpoint_) {
			static_cast<void>(put(304, top, ">", 12U));
		}
		const auto &checkpoint = document.checkpoints[index];
		static_cast<void>(put(328, top,
			std::to_string(checkpoint.round + 1U), color));
		if(detail_page_ == NewClassicReplayDetailPage::timers) {
			const auto *split = round_split(document, checkpoint);
			static_cast<void>(patch_put_timer_right(font, raster, 568, top,
				split && split->real_frames ?
					new_classic_replay_timer_label(*split->real_frames) : "-",
				color));
		} else if(detail_page_ == NewClassicReplayDetailPage::splits) {
			const auto *split = round_split(document, checkpoint);
			const auto p1 = split ? decimal_score(split->p1_score) :
				std::string(9U, '-');
			const auto p2 = split ? decimal_score(split->p2_score) :
				std::string(9U, '-');
			static_cast<void>(patch_put_fixed(
				font, raster, 452 - static_cast<std::int32_t>(p1.size() * 12U),
				top, p1, 12, color
			));
			static_cast<void>(patch_put_fixed(
				font, raster, 568 - static_cast<std::int32_t>(p2.size() * 12U),
				top, p2, 12, color
			));
			static_cast<void>(put(
				584, top, split ? patch_winner_label(split->winner) : "-", color
			));
		}
	}
	static_cast<void>(patch_surface_.publish(raster.access_page()));
}

std::expected<void, ReplaySlotStoreError>
NewClassicReplayBrowser::open_selected()
{
	auto loaded = load_replay_slot(replay_root_, selected_slot_);
	if(!loaded) {
		catalog_.slots[selected_slot_].state = ReplayCatalogSlotState::invalid;
		draw();
		return {};
	}
	if(!loaded->document) {
		catalog_.slots[selected_slot_] = {};
		draw();
		return {};
	}
	detail_document_ = std::move(*loaded->document);
	selected_checkpoint_ = 0U;
	detail_page_ = NewClassicReplayDetailPage::splits;
	checkpoint_focus_ = true;
	practice_settings_modal_ = false;
	upload_action_focus_ = false;
	draw();
	return {};
}

std::expected<NewClassicReplayBrowserResult, ReplaySlotStoreError>
NewClassicReplayBrowser::consume(NewClassicUiInput input)
{
	if(status_ != NewClassicReplayBrowserStatus::running) {
		return NewClassicReplayBrowserResult{.status = status_};
	}
	if(input.app_quit_requested) {
		status_ = NewClassicReplayBrowserStatus::application_quit;
		return NewClassicReplayBrowserResult{.status = status_};
	}
	if(host_surface_changed_) {
		host_surface_changed_ = false;
		return NewClassicReplayBrowserResult{
			.surface_changed = true,
			.status = status_,
		};
	}
	if(fade_in_active_) {
		auto &raster = patch_surface_.raster();
		bool changed = false;
		if(fade_boundary_ != 0U && fade_boundary_ <= 17U) {
			const auto pair = static_cast<std::int16_t>(fade_boundary_ - 1U);
			raster.set_palette_tone(static_cast<std::int16_t>(pair * 6));
			raster.apply_pending_palette();
			changed = true;
		}
		++fade_boundary_;
		if(fade_boundary_ >= palette_fade_boundary_count) {
			raster.set_palette_tone(100);
			raster.apply_pending_palette();
			fade_in_active_ = false;
			input_allowed_ = !any(input);
			changed = true;
		}
		return NewClassicReplayBrowserResult{
			.surface_changed = changed,
			.status = status_,
		};
	}
	if(upload_pending_) {
		return NewClassicReplayBrowserResult{.status = status_};
	}
	if(!input_allowed_) {
		if(!any(input)) {
			input_allowed_ = true;
		}
		return NewClassicReplayBrowserResult{.status = status_};
	}
	if(!any(input)) {
		return NewClassicReplayBrowserResult{.status = status_};
	}
	input_allowed_ = false;
	if(upload_feedback_) {
		upload_feedback_.reset();
		active_upload_slot_.reset();
		draw();
		return NewClassicReplayBrowserResult{
			.surface_changed = true, .status = status_
		};
	}
	if(practice_settings_modal_) {
		practice_settings_modal_ = false;
		draw();
		return NewClassicReplayBrowserResult{
			.surface_changed = true, .status = status_
		};
	}
	if(detail_document_ && (input.cancel || input.bomb)) {
		if(detail_only_) {
			status_ = NewClassicReplayBrowserStatus::back;
			return NewClassicReplayBrowserResult{.status = status_};
		}
		if(detail_document_) {
			detail_document_.reset();
			selected_checkpoint_ = 0U;
			detail_page_ = NewClassicReplayDetailPage::splits;
			checkpoint_focus_ = true;
			upload_action_focus_ = false;
			draw();
			return NewClassicReplayBrowserResult{
				.surface_changed = true, .status = status_
			};
		}
	}
	if(!detail_document_ && input.cancel) {
		status_ = NewClassicReplayBrowserStatus::back;
		return NewClassicReplayBrowserResult{.status = status_};
	}
	if(detail_document_) {
		const auto count = detail_document_->checkpoints.size();
		if(input.left) {
			if(mode_ == NewClassicReplayBrowserMode::upload &&
				replay_upload_supported(*detail_document_) &&
				detail_page_ == NewClassicReplayDetailPage::splits &&
				checkpoint_focus_) {
				upload_action_focus_ = true;
				checkpoint_focus_ = false;
				draw();
				return NewClassicReplayBrowserResult{
					.surface_changed = true, .status = status_
				};
			}
			if(detail_document_->practice && !checkpoint_focus_) {
				return NewClassicReplayBrowserResult{.status = status_};
			}
			if(detail_document_->practice &&
				detail_page_ == NewClassicReplayDetailPage::splits) {
				checkpoint_focus_ = false;
				draw();
				return NewClassicReplayBrowserResult{
					.surface_changed = true, .status = status_
				};
			}
			if(detail_page_ != NewClassicReplayDetailPage::splits) {
				detail_page_ = static_cast<NewClassicReplayDetailPage>(
					static_cast<std::size_t>(detail_page_) - 1U
				);
				draw();
				return NewClassicReplayBrowserResult{
					.surface_changed = true, .status = status_
				};
			}
			return NewClassicReplayBrowserResult{.status = status_};
		}
		if(input.right) {
			if(upload_action_focus_) {
				upload_action_focus_ = false;
				checkpoint_focus_ = true;
				draw();
				return NewClassicReplayBrowserResult{
					.surface_changed = true, .status = status_
				};
			}
			if(detail_document_->practice && !checkpoint_focus_) {
				checkpoint_focus_ = true;
				detail_page_ = NewClassicReplayDetailPage::splits;
				draw();
				return NewClassicReplayBrowserResult{
					.surface_changed = true, .status = status_
				};
			}
			if(detail_page_ != NewClassicReplayDetailPage::timers) {
				detail_page_ = static_cast<NewClassicReplayDetailPage>(
					static_cast<std::size_t>(detail_page_) + 1U
				);
				draw();
				return NewClassicReplayBrowserResult{
					.surface_changed = true, .status = status_
				};
			}
			return NewClassicReplayBrowserResult{.status = status_};
		}
		if((input.up || input.down) && checkpoint_focus_ && count != 0U &&
			detail_page_ != NewClassicReplayDetailPage::clear_bonuses) {
			selected_checkpoint_ = input.up ?
				(selected_checkpoint_ + count - 1U) % count :
				(selected_checkpoint_ + 1U) % count;
			draw();
			return NewClassicReplayBrowserResult{
				.surface_changed = true, .status = status_
			};
		}
		if(input.confirm && detail_document_->practice && !checkpoint_focus_) {
			practice_settings_modal_ = true;
			draw();
			return NewClassicReplayBrowserResult{
				.surface_changed = true, .status = status_
			};
		}
		if(input.confirm && upload_action_focus_) {
			if(!detail_document_->slowdown ||
				detail_document_->slowdown->timed_frames == 0U) {
				upload_feedback_ = NewClassicReplayUploadOutcome::ineligible;
			} else if((static_cast<std::uint64_t>(
				detail_document_->slowdown->slow_frames) * 100U) >
				(static_cast<std::uint64_t>(
					detail_document_->slowdown->timed_frames) * 5U)) {
				upload_feedback_ = NewClassicReplayUploadOutcome::slowdown;
			} else {
				upload_request_ = NewClassicReplayUploadRequest{
					.slot = selected_slot_,
					.document = *detail_document_,
				};
				active_upload_slot_ = selected_slot_;
				upload_pending_ = true;
			}
			draw();
			return NewClassicReplayBrowserResult{
				.surface_changed = true, .status = status_
			};
		}
		if(input.confirm && checkpoint_focus_ && count != 0U &&
			detail_page_ != NewClassicReplayDetailPage::clear_bonuses) {
			status_ = NewClassicReplayBrowserStatus::play_selected;
		}
		return NewClassicReplayBrowserResult{.status = status_};
	}
	if(input.up) {
		selected_slot_ = static_cast<std::uint8_t>(
			(selected_slot_ + replay_slot_count - 1U) % replay_slot_count
		);
	} else if(input.down) {
		selected_slot_ = static_cast<std::uint8_t>(
			(selected_slot_ + 1U) % replay_slot_count
		);
	} else if(input.left) {
		selected_slot_ = static_cast<std::uint8_t>(
			selected_slot_ < browser_rows ? 0U : selected_slot_ - browser_rows
		);
	} else if(input.right) {
		selected_slot_ = static_cast<std::uint8_t>(
			std::min<std::size_t>(selected_slot_ + browser_rows,
				replay_slot_count - 1U)
		);
	} else if(input.confirm &&
		catalog_.slots[selected_slot_].state == ReplayCatalogSlotState::ready) {
		if(auto opened = open_selected(); !opened) {
			return std::unexpected(std::move(opened.error()));
		}
		return NewClassicReplayBrowserResult{
			.surface_changed = true, .status = status_
		};
	} else {
		// Replay Patch only maps Cancel in the catalog. Bomb is a detail-view
		// back action and is deliberately inert here.
		return NewClassicReplayBrowserResult{.status = status_};
	}
	if(selected_slot_ < top_slot_) {
		top_slot_ = selected_slot_;
	} else if(static_cast<std::size_t>(selected_slot_) >=
		static_cast<std::size_t>(top_slot_) + browser_rows) {
		top_slot_ = static_cast<std::uint8_t>(
			selected_slot_ - (browser_rows - 1U)
		);
	}
	if(top_slot_ > replay_slot_count - browser_rows) {
		top_slot_ = static_cast<std::uint8_t>(
			replay_slot_count - browser_rows
		);
	}
	draw();
	return NewClassicReplayBrowserResult{
		.surface_changed = true, .status = status_
	};
}

present::SourceSurface NewClassicReplayBrowser::surface() const noexcept
{
	return patch_surface_.surface();
}

std::uint8_t NewClassicReplayBrowser::selected_slot() const noexcept
{
	return selected_slot_;
}

std::size_t NewClassicReplayBrowser::selected_checkpoint() const noexcept
{
	return selected_checkpoint_;
}

NewClassicReplayDetailPage
NewClassicReplayBrowser::detail_page() const noexcept
{
	return detail_page_;
}

bool NewClassicReplayBrowser::prepare_playback_transition() noexcept
{
	return status_ == NewClassicReplayBrowserStatus::play_selected &&
		patch_surface_.terminal_black_clear();
}

std::optional<formats::ReplayDocument>
NewClassicReplayBrowser::take_selected_document()
{
	if(status_ != NewClassicReplayBrowserStatus::play_selected) {
		return std::nullopt;
	}
	return std::move(detail_document_);
}

std::optional<NewClassicReplayUploadRequest>
NewClassicReplayBrowser::take_upload_request() noexcept
{
	return std::exchange(upload_request_, std::nullopt);
}

bool NewClassicReplayBrowser::report_upload(
	std::uint8_t slot,
	NewClassicReplayUploadOutcome outcome
) noexcept
{
	if(!upload_pending_ || !active_upload_slot_ || *active_upload_slot_ != slot ||
		upload_request_) {
		return false;
	}
	upload_pending_ = false;
	upload_feedback_ = outcome;
	input_allowed_ = false;
	draw();
	host_surface_changed_ = true;
	return true;
}

bool NewClassicReplayBrowserTestAccess::checkpoint_focus(
	const NewClassicReplayBrowser &browser
) noexcept
{
	return browser.checkpoint_focus_;
}

bool NewClassicReplayBrowserTestAccess::practice_settings_visible(
	const NewClassicReplayBrowser &browser
) noexcept
{
	return browser.practice_settings_modal_;
}

std::size_t NewClassicReplayBrowserTestAccess::physical_row_count(
	const NewClassicReplayBrowser &browser
) noexcept
{
	if(!browser.detail_document_ || browser.detail_document_->practice ||
		!browser.detail_document_->metadata ||
		browser.detail_document_->metadata->game_mode != game_mode::story) {
		return browser.detail_document_ ?
			browser.detail_document_->checkpoints.size() : 0U;
	}
	return story_detail_rows(
		*browser.detail_document_, browser.detail_page_
	).size();
}

NewClassicReplayHub::NewClassicReplayHub(
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	bool libraries_available,
	bool upload_available,
	bool my_records_available
) :
	patch_assets_(std::move(patch_assets))
{
	valid_ = static_cast<bool>(patch_assets_);
	static_cast<void>(refresh_capabilities(
		libraries_available, upload_available, my_records_available
	));
	if(valid_) {
		patch_surface_.raster().set_palette_tone(0);
		patch_surface_.raster().apply_pending_palette();
	}
}

bool NewClassicReplayHub::valid() const noexcept
{
	return valid_;
}

void NewClassicReplayHub::draw() noexcept
{
	if(!valid_ || !patch_assets_ || !patch_surface_.begin_hidden_frame(
		patch_assets_->menu_background(NewClassicMenuBackground::replay)
	)) {
		valid_ = false;
		return;
	}
	auto &raster = patch_surface_.raster();
	static_cast<void>(raster.set_pending_palette_entry(12U, {15U, 15U, 0U}));
	raster.apply_pending_palette();
	const auto &font = patch_assets_->menu_font;
	const auto &messages = patch_assets_->messages();
	const auto message = [](NewClassicReplayHubStatus row) noexcept {
		switch(row) {
		case NewClassicReplayHubStatus::local_selected:
			return NewClassicMessageId::replay_command_local;
		case NewClassicReplayHubStatus::upload_selected:
			return NewClassicMessageId::replay_command_upload;
		case NewClassicReplayHubStatus::my_records_selected:
			return NewClassicMessageId::replay_command_my_records;
		case NewClassicReplayHubStatus::online_selected:
			return NewClassicMessageId::replay_command_online;
		case NewClassicReplayHubStatus::downloaded_selected:
			return NewClassicMessageId::replay_command_downloaded;
		case NewClassicReplayHubStatus::back:
			return NewClassicMessageId::replay_command_return;
		case NewClassicReplayHubStatus::running:
		case NewClassicReplayHubStatus::application_quit:
			break;
		}
		return NewClassicMessageId::replay_command_return;
	};
	valid_ = font.put_centered(
		raster, 320, 80, messages.text(NewClassicMessageId::title_replay), 9U
	);
	for(std::size_t row = 0U; valid_ && row < rows_.size(); ++row) {
		const auto top = static_cast<std::int32_t>(144U + (row * 32U));
		const auto color = static_cast<present::Index4>(
			row == selection_ ? 12U : 15U
		);
		valid_ = font.put_centered(
			raster, 320, top, messages.text(message(rows_[row])), color
		);
		if(valid_ && row == selection_) {
			valid_ = font.put(raster, 160, top, ">", 12U);
		}
	}
	if(valid_ && !patch_surface_.publish(raster.access_page())) {
		valid_ = false;
	}
}

NewClassicReplayHubResult NewClassicReplayHub::consume(
	NewClassicUiInput input
) noexcept
{
	if(!valid_ || status_ != NewClassicReplayHubStatus::running) {
		return {.status = status_};
	}
	if(input.app_quit_requested) {
		status_ = NewClassicReplayHubStatus::application_quit;
		return {.status = status_};
	}
	if(fade_in_active_) {
		auto &raster = patch_surface_.raster();
		bool changed = false;
		if(fade_boundary_ != 0U && fade_boundary_ <= 17U) {
			const auto pair = static_cast<std::int16_t>(fade_boundary_ - 1U);
			raster.set_palette_tone(static_cast<std::int16_t>(pair * 6));
			raster.apply_pending_palette();
			changed = true;
		}
		++fade_boundary_;
		if(fade_boundary_ >= palette_fade_boundary_count) {
			raster.set_palette_tone(100);
			raster.apply_pending_palette();
			fade_in_active_ = false;
			input_allowed_ = !any(input);
			changed = true;
		}
		return {.surface_changed = changed, .status = status_};
	}
	if(!input_allowed_) {
		if(!any(input)) {
			input_allowed_ = true;
		}
		return {.status = status_};
	}
	if(!any(input)) {
		return {.status = status_};
	}
	input_allowed_ = false;
	if(input.cancel || input.bomb) {
		status_ = NewClassicReplayHubStatus::back;
		return {.status = status_};
	}
	if(input.up) {
		selection_ = (selection_ + rows_.size() - 1U) % rows_.size();
	} else if(input.down) {
		selection_ = (selection_ + 1U) % rows_.size();
	} else if(input.confirm) {
		status_ = rows_[selection_];
		return {.status = status_};
	} else {
		return {.status = status_};
	}
	draw();
	return {.surface_changed = valid_, .status = status_};
}

[[nodiscard]] std::string dos_date_short_label(std::uint16_t packed)
{
	if(packed == 0U) {
		return "-";
	}
	char buffer[9U]{};
	static_cast<void>(std::snprintf(
		buffer, sizeof(buffer), "%02u-%02u-%02u",
		static_cast<unsigned>((packed >> 5U) & 0x0FU),
		static_cast<unsigned>(packed & 0x1FU),
		static_cast<unsigned>((1980U + (packed >> 9U)) % 100U)
	));
	return buffer;
}

bool NewClassicReplayHub::refresh_capabilities(
	bool libraries_available,
	bool upload_available,
	bool my_records_available
) noexcept
{
	if(!valid_ || status_ != NewClassicReplayHubStatus::running) {
		return false;
	}
	std::vector<NewClassicReplayHubStatus> rows{
		NewClassicReplayHubStatus::local_selected,
	};
	static_cast<void>(upload_available);
	if(my_records_available) {
		rows.push_back(NewClassicReplayHubStatus::my_records_selected);
	}
	if(libraries_available) {
		rows.push_back(NewClassicReplayHubStatus::online_selected);
		rows.push_back(NewClassicReplayHubStatus::downloaded_selected);
	}
	rows.push_back(NewClassicReplayHubStatus::back);
	if(rows == rows_) {
		return false;
	}
	const auto selected = selection_ < rows_.size() ?
		rows_[selection_] : NewClassicReplayHubStatus::local_selected;
	rows_ = std::move(rows);
	const auto retained = std::ranges::find(rows_, selected);
	selection_ = retained == rows_.end() ?
		std::min(selection_, rows_.size() - 1U) :
		static_cast<std::size_t>(retained - rows_.begin());
	draw();
	return valid_;
}

present::SourceSurface NewClassicReplayHub::surface() const noexcept
{
	return patch_surface_.surface();
}

NewClassicReplayHubStatus NewClassicReplayHub::status() const noexcept
{
	return status_;
}

std::size_t NewClassicReplayHub::selection() const noexcept
{
	return selection_;
}

namespace {

[[nodiscard]] std::string online_text(
	std::string_view text,
	const NewClassicMenuFont &font,
	std::int32_t maximum_width
)
{
	if(text.empty() || !valid_new_classic_utf8(text)) {
		return "-";
	}
	std::size_t accepted = 0U;
	for(std::size_t offset = 0U; offset < text.size();) {
		const auto scalar = next_new_classic_utf8_scalar(text, offset);
		if(!scalar || !new_classic_unicode_printable_scalar(scalar->value)) {
			return "-";
		}
		const auto next = offset + scalar->bytes;
		if(font.width(text.substr(0U, next)) > maximum_width) {
			break;
		}
		accepted = next;
		offset = next;
	}
	return accepted == 0U ? std::string{"-"} :
		std::string{text.substr(0U, accepted)};
}

[[nodiscard]] std::string online_score(std::uint64_t score)
{
	return std::to_string(score);
}

[[nodiscard]] std::string online_place(std::size_t place)
{
	return std::to_string(place + 1U);
}

[[nodiscard]] std::string online_result(
	const NewClassicLeaderboardEntry &entry,
	const NewClassicMessageCatalog &messages
)
{
	if(entry.result.kind == NewClassicOnlineResultKind::clear) {
		return std::string(messages.text(
			NewClassicMessageId::hiscore_online_result_clear
		));
	}
	if(entry.result.kind == NewClassicOnlineResultKind::versus_complete) {
		return std::string(messages.text(
			NewClassicMessageId::replay_mode_netplay
		));
	}
	const auto label = messages.text(
		NewClassicMessageId::hiscore_online_result_game_over
	);
	return std::string(label) + " " +
		std::to_string(static_cast<unsigned>(entry.result.end_stage) + 1U);
}

[[nodiscard]] std::string online_slowdown(
	const NewClassicOnlineReplayRecord &record
)
{
	if(record.timed_frames == 0U) {
		return "-";
	}
	const auto percent = std::min<std::uint64_t>(
		100U,
		(static_cast<std::uint64_t>(record.slow_frames) * 100U) /
			record.timed_frames
	);
	return std::to_string(percent) + "%";
}

} // namespace

NewClassicHighScoreMenu::NewClassicHighScoreMenu(
	std::vector<NewClassicOnlineCacheDocument> boards,
	std::uint8_t rank,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	NewClassicOnlineMenuMode mode,
	std::optional<NewClassicScoreDocument> personal_bests,
	bool fade_in
) :
	patch_assets_(std::move(patch_assets)),
	personal_bests_(std::move(personal_bests)),
	mode_(mode),
	rank_(rank)
{
	for(auto &board : boards) {
		const bool accepted_mode = board.board_category &&
			(board.board_category->mode == NewClassicOnlineMode::story ||
			 (mode_ == NewClassicOnlineMenuMode::my_records &&
			  board.board_category->mode == NewClassicOnlineMode::vs_1p_2p));
		if(accepted_mode &&
			(mode_ == NewClassicOnlineMenuMode::high_scores ||
			 board.board_category->character !=
				new_classic_online_overall_character) &&
			validate_new_classic_online_cache(board)) {
			if(mode_ != NewClassicOnlineMenuMode::high_scores) {
				std::erase_if(board.leaderboard_entries,
					[&board, this](const auto &entry) {
						if(!entry.replay_id) {
							return true;
						}
						const auto replay = std::ranges::find(
							board.replay_records, *entry.replay_id,
							&NewClassicOnlineReplayRecord::replay_id
						);
						return replay == board.replay_records.end() ||
							(mode_ == NewClassicOnlineMenuMode::downloaded_replays &&
							 replay->availability !=
								NewClassicOnlineReplayAvailability::downloaded);
					}
				);
			}
			boards_.push_back(std::move(board));
		}
	}
	std::ranges::sort(boards_, {}, [](const auto &board) {
		const auto character = board.board_category->character ==
			new_classic_online_overall_character ? 0U :
			static_cast<std::uint16_t>(board.board_category->character) + 1U;
		return (static_cast<std::uint16_t>(board.board_category->rank) << 8U) |
			character;
	});
	const auto duplicate = std::adjacent_find(
		boards_.begin(), boards_.end(), [](const auto &left, const auto &right) {
			return left.board_category == right.board_category;
		}
	);
	if(!patch_assets_ || rank_ >= 4U || duplicate != boards_.end() ||
		(mode_ != NewClassicOnlineMenuMode::high_scores && boards_.empty())) {
		return;
	}
	const auto initial = std::ranges::find_if(boards_, [this](const auto &board) {
		return board.board_category->rank == rank_ &&
			(mode_ != NewClassicOnlineMenuMode::high_scores ||
			 board.board_category->character ==
				new_classic_online_overall_character);
	});
	const auto fallback = initial != boards_.end() ? initial :
		std::ranges::find_if(boards_, [this](const auto &board) {
			return board.board_category->rank == rank_;
		});
	if(fallback != boards_.end()) {
		board_index_ = static_cast<std::size_t>(fallback - boards_.begin());
	}
	if(mode_ == NewClassicOnlineMenuMode::my_records ||
		mode_ == NewClassicOnlineMenuMode::downloaded_replays) {
		for(std::size_t board = 0U; board < boards_.size(); ++board) {
			for(std::size_t entry = 0U;
				entry < boards_[board].leaderboard_entries.size(); ++entry) {
				flat_entries_.emplace_back(board, entry);
			}
		}
		std::ranges::stable_sort(flat_entries_, [this](const auto &left,
			const auto &right) {
			const auto &left_entry =
				boards_[left.first].leaderboard_entries[left.second];
			const auto &right_entry =
				boards_[right.first].leaderboard_entries[right.second];
			return left_entry.dos_date > right_entry.dos_date;
		});
	}
	if(mode_ != NewClassicOnlineMenuMode::high_scores) {
		page_ = Page::board;
	}
	valid_ = true;
	draw();
	if(valid_) {
		fade_in_active_ = fade_in;
		input_allowed_ = !fade_in;
		patch_surface_.raster().set_palette_tone(fade_in ? 0 : 100);
		patch_surface_.raster().apply_pending_palette();
	}
}

bool NewClassicHighScoreMenu::valid() const noexcept
{
	return valid_;
}

void NewClassicHighScoreMenu::draw() noexcept
{
	if(!valid_ || !patch_assets_) {
		valid_ = false;
		return;
	}
	const auto &background = mode_ == NewClassicOnlineMenuMode::high_scores ?
		patch_assets_->menu_background(NewClassicMenuBackground::high_score) :
		patch_assets_->replay_list_background;
	if(!patch_surface_.begin_hidden_frame(background)) {
		valid_ = false;
		return;
	}
	auto &raster = patch_surface_.raster();
	static_cast<void>(raster.set_pending_palette_entry(12U, {15U, 15U, 0U}));
	raster.apply_pending_palette();
	switch(page_) {
	case Page::hub: draw_hub(); break;
	case Page::board: draw_board(); break;
	case Page::detail: draw_detail(); break;
	case Page::personal_bests: draw_personal_bests(); break;
	}
	if(valid_ && !patch_surface_.publish(raster.access_page())) {
		valid_ = false;
	}
}

void NewClassicHighScoreMenu::draw_hub() noexcept
{
	const auto &font = patch_assets_->menu_font;
	const auto &messages = patch_assets_->messages();
	const std::array ids{
		NewClassicMessageId::hiscore_command_local,
		NewClassicMessageId::hiscore_command_online,
		NewClassicMessageId::hiscore_command_personal,
		NewClassicMessageId::hiscore_command_return,
	};
	auto &raster = patch_surface_.raster();
	valid_ = font.put_centered(
		raster, 320, 96, messages.text(NewClassicMessageId::title_hiscore), 9U
	);
	for(std::size_t row = 0U; valid_ && row < ids.size(); ++row) {
		const auto top = static_cast<std::int32_t>(160U + (row * 32U));
		const bool enabled = row != 1U || !boards_.empty();
		const auto color = static_cast<present::Index4>(
			!enabled ? 8U : (row == hub_selection_ ? 12U : 15U)
		);
		valid_ = font.put_centered(
			raster, 320, top, messages.text(ids[row]), color
		);
		if(valid_ && row == hub_selection_) {
			valid_ = font.put(raster, 176, top, ">", 12U);
		}
	}
}

void NewClassicHighScoreMenu::draw_personal_bests() noexcept
{
	if(rank_ >= new_classic_score_rank_count) {
		valid_ = false;
		return;
	}
	const auto &font = patch_assets_->menu_font;
	const auto &messages = patch_assets_->messages();
	auto &raster = patch_surface_.raster();
	const auto rank = patch_rank_label(rank_, messages);
	const std::array title_arguments{
		NewClassicMessageArgument{"rank", rank},
	};
	valid_ = font.put_centered(raster, 320, 48, messages.format(
		NewClassicMessageId::hiscore_personal_title, title_arguments
	), 9U) &&
		font.put(raster, 40, 80, messages.text(
			NewClassicMessageId::replay_header_character), 9U) &&
		font.put_right(raster, 316, 80, messages.text(
			NewClassicMessageId::replay_header_score), 9U) &&
		font.put(raster, 340, 80, messages.text(
			NewClassicMessageId::replay_header_stage), 9U) &&
		font.put(raster, 408, 80, messages.text(
			NewClassicMessageId::hiscore_personal_header_one_cc), 9U) &&
		font.put(raster, 488, 80, messages.text(
			NewClassicMessageId::hiscore_personal_header_time), 9U);
	const auto empty = new_classic_score_empty();
	const auto &document = personal_bests_ ? *personal_bests_ : empty;
	const auto character_count = document.extra_unlocked ?
		new_classic_score_character_count : new_classic_score_locked_character_count;
	const auto fixed = [](std::uint32_t value, std::size_t digits) {
		std::string result(digits, '0');
		while(digits != 0U) {
			result[--digits] = static_cast<char>('0' + (value % 10U));
			value /= 10U;
		}
		return result;
	};
	for(std::size_t character = 0U;
		valid_ && character < character_count; ++character) {
		const auto &board = document.boards[rank_][character];
		const auto &entry = board.entries.front();
		const auto top = static_cast<std::int32_t>(96U + (character * 24U));
		std::string score{"-"};
		std::string stage{"-"};
		if(entry.valid) {
			score.resize(entry.score.size());
			for(std::size_t digit = 0U; digit < entry.score.size(); ++digit) {
				score[entry.score.size() - digit - 1U] = static_cast<char>(
					'0' + entry.score[digit]
				);
			}
			stage = entry.stage == 99U ?
				std::string(messages.text(NewClassicMessageId::replay_marker_all)) :
				std::to_string(entry.stage);
		}
		const auto seconds = static_cast<std::uint32_t>(
			(board.stats.play_frames / 282U) * 5U +
			((board.stats.play_frames % 282U) * 5U) / 282U
		);
		const auto hours = std::min<std::uint32_t>(seconds / 3600U, 9999U);
		const auto playtime = std::to_string(hours) + ":" +
			fixed((seconds / 60U) % 60U, 2U) + ":" +
			fixed(seconds % 60U, 2U);
		valid_ = font.put(raster, 40, top, patch_character_label(
			static_cast<std::int8_t>((character * 2U) + 1U), messages
		), 15U) &&
			font.put_right(raster, 316, top, score, 15U) &&
			font.put(raster, 340, top, stage, 15U) &&
			font.put_right(raster, 464, top,
				std::to_string(board.stats.one_ccs), 15U) &&
			font.put(raster, 488, top, playtime, 15U);
	}
	valid_ = valid_ && font.put_centered(raster, 320, 352, messages.text(
		NewClassicMessageId::hiscore_online_detail_footer
	), 9U);
}

void NewClassicHighScoreMenu::draw_board() noexcept
{
	if(board_index_ >= boards_.size()) {
		valid_ = false;
		return;
	}
	const auto &board = boards_[board_index_];
	const auto &category = *board.board_category;
	const auto &font = patch_assets_->menu_font;
	const auto &messages = patch_assets_->messages();
	auto &raster = patch_surface_.raster();
	const bool flat = mode_ == NewClassicOnlineMenuMode::my_records ||
		mode_ == NewClassicOnlineMenuMode::downloaded_replays;
	std::optional<std::int32_t> rank_center;
	if(mode_ == NewClassicOnlineMenuMode::high_scores) {
		rank_center = patch_put_online_category(
			font, raster, messages, category.rank, category.character, 48
		);
		valid_ = rank_center.has_value();
	} else if(flat) {
		const auto title_id = mode_ ==
			NewClassicOnlineMenuMode::downloaded_replays ?
			NewClassicMessageId::replay_downloaded_title :
			NewClassicMessageId::replay_my_records_title;
		valid_ = font.put_centered(
			raster, 320, 64, messages.text(title_id), 9U
		);
	} else {
		rank_center = patch_put_online_category(
			font, raster, messages, category.rank, category.character, 80, 9U
		);
		valid_ = rank_center.has_value();
	}
	const auto header_top = mode_ == NewClassicOnlineMenuMode::online_replays ?
		128 : 96;
	const auto rows_top = header_top + 16;
	valid_ = valid_ && (flat || font.put(
		raster, 32, header_top, messages.text(
			NewClassicMessageId::hiscore_online_header_place), 9U
	)) && font.put(
		raster, flat ? 32 : 80, header_top, messages.text(mode_ ==
			NewClassicOnlineMenuMode::high_scores ?
			NewClassicMessageId::hiscore_online_label_player :
			NewClassicMessageId::replay_header_name), 9U
	) &&
		font.put_right(raster, 324, header_top, messages.text(
			NewClassicMessageId::replay_header_score), 9U) &&
		font.put(raster, 348, header_top, messages.text(
			NewClassicMessageId::replay_header_character), 9U) &&
		font.put(raster, 464, header_top, messages.text(
			flat ? NewClassicMessageId::replay_header_rank :
				NewClassicMessageId::replay_label_date), 9U) &&
		font.put(raster, 576, header_top, messages.text(
			NewClassicMessageId::replay_header_stage), 9U);
	if((flat && flat_entries_.empty()) ||
		(!flat && board.leaderboard_entries.empty())) {
		valid_ = valid_ && font.put(
			raster, flat ? 32 : 80, rows_top,
			messages.text(NewClassicMessageId::replay_none), 15U
		);
	}
	for(std::size_t row = 0U; valid_ && row < browser_rows; ++row) {
		const auto list_index = (flat ? flat_top_ : top_entry_) + row;
		const auto list_size = flat ? flat_entries_.size() :
			board.leaderboard_entries.size();
		if(list_index >= list_size) {
			break;
		}
		const auto location = flat ? flat_entries_[list_index] :
			std::pair<std::size_t, std::size_t>{board_index_, list_index};
		const auto &row_board = boards_[location.first];
		const auto &entry = row_board.leaderboard_entries[location.second];
		const auto &row_category = *row_board.board_category;
		const auto top = rows_top + static_cast<std::int32_t>(row * 16U);
		const auto selected = list_index ==
			(flat ? flat_selection_ : selected_entry_);
		const auto color = static_cast<present::Index4>(
			selected ? 12U : 15U
		);
		if(selected) {
			valid_ = font.put(raster, 20, top, ">", 12U);
		}
		const auto replay_it = entry.replay_id ? std::ranges::find(
			row_board.replay_records, *entry.replay_id,
			&NewClassicOnlineReplayRecord::replay_id
		) : row_board.replay_records.end();
		const auto *replay = replay_it == row_board.replay_records.end() ?
			nullptr : &*replay_it;
		auto name = online_text(
			mode_ == NewClassicOnlineMenuMode::high_scores || !replay ?
				std::string_view{entry.player} : std::string_view{replay->title},
			font, 208
		);
		const auto score = online_score(entry.result.score);
		const auto stage = entry.result.kind ==
			NewClassicOnlineResultKind::versus_complete ? std::string{"-"} :
			(entry.result.kind == NewClassicOnlineResultKind::clear ?
			std::string(messages.text(NewClassicMessageId::replay_marker_all)) :
			std::to_string(static_cast<unsigned>(entry.result.end_stage) + 1U));
		const auto category_value = flat ? std::string(patch_rank_label(
			row_category.rank, messages
		)) : dos_date_short_label(entry.dos_date);
		const auto row_character = patch_character_label(
			static_cast<std::int8_t>((row_category.character * 2U) + 1U), messages
		);
		valid_ = valid_ && (flat || patch_put_fixed(
			font, raster, 32, top, online_place(list_index), 12, color
		)) && font.put(raster, flat ? 32 : 80, top, name, color) &&
			patch_put_fixed(font, raster,
				324 - static_cast<std::int32_t>(score.size() * 12U), top,
				score, 12, color) &&
			font.put(raster, 348, top, row_character, color) &&
			(flat ? font.put(raster, 464, top, category_value, color) :
				patch_put_fixed(font, raster, 464, top, category_value, 11, color)) &&
			font.put(raster, 576, top, stage, color);
	}
}

const NewClassicOnlineReplayRecord *
NewClassicHighScoreMenu::selected_replay() const noexcept
{
	if(board_index_ >= boards_.size()) {
		return nullptr;
	}
	const auto &board = boards_[board_index_];
	if(selected_entry_ >= board.leaderboard_entries.size()) {
		return nullptr;
	}
	const auto &entry = board.leaderboard_entries[selected_entry_];
	if(!entry.replay_id) {
		return nullptr;
	}
	const auto found = std::ranges::find_if(
		board.replay_records, [&entry](const auto &record) {
			return record.replay_id == *entry.replay_id;
		}
	);
	return found == board.replay_records.end() ? nullptr : &*found;
}

void NewClassicHighScoreMenu::request_replay_load() noexcept
{
	const auto *replay = selected_replay();
	if(!replay || replay->availability !=
		NewClassicOnlineReplayAvailability::downloaded ||
		watch_state_ != WatchState::idle) {
		return;
	}
	watch_target_ = replay->replay_id;
	watch_state_ = WatchState::pending;
	watch_request_ = *replay;
}

void NewClassicHighScoreMenu::draw_detail() noexcept
{
	if(board_index_ >= boards_.size() || selected_entry_ >=
		boards_[board_index_].leaderboard_entries.size()) {
		valid_ = false;
		return;
	}
	const auto &entry =
		boards_[board_index_].leaderboard_entries[selected_entry_];
	const auto &font = patch_assets_->menu_font;
	const auto &messages = patch_assets_->messages();
	auto &raster = patch_surface_.raster();
	const auto put_row = [&](std::int32_t top, NewClassicMessageId label,
		std::string_view value) {
		return font.put(raster, 48, top, messages.text(label), 15U) &&
			font.put_right(raster, 304, top, value, 15U);
	};
	const auto *replay = selected_replay();
	auto name = online_text(
		mode_ == NewClassicOnlineMenuMode::high_scores || !replay ?
			std::string_view{entry.player} : std::string_view{replay->title},
		font, 244
	);
	const auto score = online_score(entry.result.score);
	const auto result = online_result(entry, messages);
	const auto &category = *boards_[board_index_].board_category;
	const auto rank = patch_rank_label(category.rank, messages);
	const auto character = patch_character_label(
		static_cast<std::int8_t>((category.character * 2U) + 1U), messages
	);
	valid_ = font.put_centered(raster, 320, 64, messages.text(
			mode_ == NewClassicOnlineMenuMode::high_scores ?
				NewClassicMessageId::hiscore_command_online :
				(mode_ == NewClassicOnlineMenuMode::downloaded_replays ?
					NewClassicMessageId::replay_downloaded_title :
					(mode_ == NewClassicOnlineMenuMode::my_records ?
						NewClassicMessageId::replay_my_records_title :
						NewClassicMessageId::replay_online_title))
		), 9U) && font.put(raster, 48, 80, name, 15U) &&
		font.put(raster, 48, 96, result, 9U) &&
		put_row(112, NewClassicMessageId::replay_header_score, score) &&
		put_row(128, NewClassicMessageId::replay_label_date,
			dos_date_label(entry.dos_date)) &&
		font.put(raster, 48, 144,
			std::string(rank) + " " + std::string(character), 15U);
	if(replay) {
		const auto displayed_autofire = entry.autofire.value_or(
			(replay->autofire & 0x01U) != 0U
		);
		const auto autofire = messages.text(displayed_autofire ?
			NewClassicMessageId::replay_autofire_on :
			NewClassicMessageId::replay_autofire_off);
		const bool target = download_target_ &&
			*download_target_ == replay->replay_id;
		auto replay_message =
			replay->availability == NewClassicOnlineReplayAvailability::downloaded ?
				NewClassicMessageId::hiscore_online_replay_downloaded :
				NewClassicMessageId::hiscore_online_replay_online;
		if(target) {
			switch(download_state_) {
			case DownloadState::idle: break;
			case DownloadState::pending:
				replay_message =
					NewClassicMessageId::hiscore_online_replay_downloading;
				break;
			case DownloadState::failed:
				replay_message = NewClassicMessageId::hiscore_online_replay_failed;
				break;
			case DownloadState::unavailable:
				replay_message =
					NewClassicMessageId::hiscore_online_replay_unavailable;
				break;
			}
		}
		const auto replay_state = messages.text(replay_message);
		valid_ = valid_ &&
			font.put(raster, 48, 160, autofire, 15U) &&
			put_row(176, NewClassicMessageId::replay_label_slowdown,
				online_slowdown(*replay)) &&
			font.put(raster, 344, 112, messages.text(
				NewClassicMessageId::hiscore_online_label_replay), 9U) &&
			font.put(raster, 344, 128, replay_state, 15U);
	} else {
		const auto autofire = entry.autofire ? std::string(messages.text(
			*entry.autofire ? NewClassicMessageId::replay_autofire_on :
				NewClassicMessageId::replay_autofire_off
		)) : std::string(messages.text(
			NewClassicMessageId::hiscore_online_label_autofire
		)) + " -";
		valid_ = valid_ &&
			font.put(raster, 48, 160, autofire, 15U) &&
			font.put(raster, 344, 112, messages.text(
				NewClassicMessageId::hiscore_online_label_replay), 9U) &&
			font.put(raster, 344, 128, messages.text(
				NewClassicMessageId::hiscore_online_replay_historical), 15U);
	}
	if(replay && watch_state_ == WatchState::pending) {
		valid_ = valid_ && font.put_centered(
			raster, 320, 208, messages.text(
				NewClassicMessageId::hiscore_online_watching_footer
			), 9U
		);
	} else if(replay && replay->availability ==
		NewClassicOnlineReplayAvailability::metadata_only) {
		const bool pending = download_target_ &&
			*download_target_ == replay->replay_id &&
			download_state_ == DownloadState::pending;
		if(pending) {
			valid_ = valid_ && font.put(
				raster, 48, 208, messages.text(
					NewClassicMessageId::hiscore_online_downloading_footer
				), 9U
			);
		} else {
			valid_ = valid_ && font.put(raster, 32, 208, ">", 12U) &&
				font.put(raster, 48, 208, messages.text(
					NewClassicMessageId::hiscore_online_download_action
				), 12U);
		}
	}
}

NewClassicHighScoreMenuResult NewClassicHighScoreMenu::consume(
	NewClassicUiInput input
) noexcept
{
	if(!valid_ || status_ != NewClassicHighScoreMenuStatus::running) {
		return {.status = status_};
	}
	if(input.app_quit_requested) {
		status_ = NewClassicHighScoreMenuStatus::application_quit;
		return {.status = status_};
	}
	if(redraw_pending_) {
		redraw_pending_ = false;
		draw();
		return {.surface_changed = valid_, .status = status_};
	}
	if(fade_in_active_) {
		auto &raster = patch_surface_.raster();
		bool changed = false;
		if(fade_boundary_ != 0U && fade_boundary_ <= 17U) {
			const auto pair = static_cast<std::int16_t>(fade_boundary_ - 1U);
			raster.set_palette_tone(static_cast<std::int16_t>(pair * 6));
			raster.apply_pending_palette();
			changed = true;
		}
		++fade_boundary_;
		if(fade_boundary_ >= palette_fade_boundary_count) {
			raster.set_palette_tone(100);
			raster.apply_pending_palette();
			fade_in_active_ = false;
			input_allowed_ = !any(input);
			changed = true;
		}
		return {.surface_changed = changed, .status = status_};
	}
	if(page_ == Page::detail && replay_detail_browser_) {
		auto result = replay_detail_browser_->consume(input);
		if(!result) {
			valid_ = false;
			return {.status = status_};
		}
		if(result->status == NewClassicReplayBrowserStatus::application_quit) {
			status_ = NewClassicHighScoreMenuStatus::application_quit;
			return {.status = status_};
		}
		if(result->status == NewClassicReplayBrowserStatus::back) {
			replay_detail_browser_.reset();
			page_ = Page::board;
			draw();
			return {.surface_changed = valid_, .status = status_};
		}
		if(result->status == NewClassicReplayBrowserStatus::play_selected) {
			auto document = replay_detail_browser_->take_selected_document();
			if(!document) {
				valid_ = false;
				return {.status = status_};
			}
			playback_request_ = NewClassicOnlineReplayPlaybackRequest{
				.document = std::move(*document),
				.checkpoint_index =
					replay_detail_browser_->selected_checkpoint(),
			};
		}
		return {
			.surface_changed = result->surface_changed,
			.status = status_,
		};
	}
	if(!input_allowed_) {
		if(!any(input)) {
			input_allowed_ = true;
		}
		return {.status = status_};
	}
	if(watch_state_ == WatchState::pending) {
		return {.status = status_};
	}
	if(!any(input)) {
		return {.status = status_};
	}
	input_allowed_ = false;
	if(page_ == Page::hub) {
		if(input.cancel) {
			status_ = NewClassicHighScoreMenuStatus::back;
			return {.status = status_};
		}
		if(input.up || input.down) {
			do {
				hub_selection_ = input.up ?
					(hub_selection_ + 3U) % 4U :
					(hub_selection_ + 1U) % 4U;
			} while(hub_selection_ == 1U && boards_.empty());
		} else if(input.confirm) {
			if(hub_selection_ == 0U) {
				status_ = NewClassicHighScoreMenuStatus::local_selected;
				return {.status = status_};
			}
			if(hub_selection_ == 1U && !boards_.empty()) {
				page_ = Page::board;
				selected_entry_ = 0U;
				top_entry_ = 0U;
			} else if(hub_selection_ == 2U) {
				page_ = Page::personal_bests;
			} else {
				status_ = NewClassicHighScoreMenuStatus::back;
				return {.status = status_};
			}
		} else {
			return {.status = status_};
		}
		draw();
		return {.surface_changed = valid_, .status = status_};
	}
	if(page_ == Page::personal_bests) {
		page_ = Page::hub;
		draw();
		return {.surface_changed = valid_, .status = status_};
	}
	if(page_ == Page::detail) {
		if(input.cancel || input.bomb) {
			replay_detail_browser_.reset();
			watch_state_ = WatchState::idle;
			watch_target_.reset();
			page_ = Page::board;
			draw();
			return {.surface_changed = valid_, .status = status_};
		}
		if(input.confirm) {
			const auto *replay = selected_replay();
			if(replay && replay->availability ==
				NewClassicOnlineReplayAvailability::metadata_only &&
				download_state_ != DownloadState::pending) {
				download_target_ = replay->replay_id;
				download_state_ = DownloadState::pending;
				download_request_ = *replay;
				draw();
				return {.surface_changed = valid_, .status = status_};
			}
		}
		return {.status = status_};
	}
	const bool flat = mode_ == NewClassicOnlineMenuMode::my_records ||
		mode_ == NewClassicOnlineMenuMode::downloaded_replays;
	if(input.cancel || input.bomb) {
		if(mode_ == NewClassicOnlineMenuMode::high_scores) {
			page_ = Page::hub;
			draw();
			return {.surface_changed = valid_, .status = status_};
		}
		status_ = NewClassicHighScoreMenuStatus::back;
		return {.status = status_};
	}
	if(flat) {
		const auto count = flat_entries_.size();
		if(input.up && count != 0U) {
			flat_selection_ = (flat_selection_ + count - 1U) % count;
		} else if(input.down && count != 0U) {
			flat_selection_ = (flat_selection_ + 1U) % count;
		} else if(input.confirm && count != 0U) {
			const auto location = flat_entries_[flat_selection_];
			board_index_ = location.first;
			selected_entry_ = location.second;
			page_ = Page::detail;
			request_replay_load();
		} else {
			return {.status = status_};
		}
		if(flat_selection_ < flat_top_) {
			flat_top_ = flat_selection_;
		} else if(flat_selection_ >= flat_top_ + browser_rows) {
			flat_top_ = flat_selection_ - (browser_rows - 1U);
		}
		draw();
		return {.surface_changed = valid_, .status = status_};
	}
	if(input.left || input.right) {
		const auto &current = *boards_[board_index_].board_category;
		const auto target_character = static_cast<std::uint8_t>(input.left ?
			(current.character == new_classic_online_overall_character ? 8U :
			 (current.character == 0U ? new_classic_online_overall_character :
			  current.character - 1U)) :
			(current.character == new_classic_online_overall_character ? 0U :
			 (current.character == 8U ? new_classic_online_overall_character :
			  current.character + 1U))
		);
		const auto found = std::ranges::find_if(
			boards_, [&](const auto &candidate) {
				return candidate.board_category->rank == current.rank &&
					candidate.board_category->character == target_character;
			}
		);
		if(found == boards_.end()) {
			return {.status = status_};
		}
		board_index_ = static_cast<std::size_t>(found - boards_.begin());
		selected_entry_ = 0U;
		top_entry_ = 0U;
	} else {
		const auto count = boards_[board_index_].leaderboard_entries.size();
		if(input.up && count != 0U) {
			selected_entry_ = (selected_entry_ + count - 1U) % count;
		} else if(input.down && count != 0U) {
			selected_entry_ = (selected_entry_ + 1U) % count;
		} else if(input.confirm && count != 0U) {
			page_ = Page::detail;
			request_replay_load();
		} else {
			return {.status = status_};
		}
		if(selected_entry_ < top_entry_) {
			top_entry_ = selected_entry_;
		} else if(selected_entry_ >= top_entry_ + browser_rows) {
			top_entry_ = selected_entry_ - (browser_rows - 1U);
		}
	}
	draw();
	return {.surface_changed = valid_, .status = status_};
}

present::SourceSurface NewClassicHighScoreMenu::surface() const noexcept
{
	if(page_ == Page::detail && replay_detail_browser_) {
		return replay_detail_browser_->surface();
	}
	return patch_surface_.surface();
}

NewClassicHighScoreMenuStatus NewClassicHighScoreMenu::status() const noexcept
{
	return status_;
}

bool NewClassicHighScoreMenu::viewing_online() const noexcept
{
	return page_ == Page::board || page_ == Page::detail;
}

bool NewClassicHighScoreMenu::viewing_detail() const noexcept
{
	return page_ == Page::detail;
}

bool NewClassicHighScoreMenu::viewing_personal_bests() const noexcept
{
	return page_ == Page::personal_bests;
}

std::size_t NewClassicHighScoreMenu::selected_board() const noexcept
{
	if((mode_ == NewClassicOnlineMenuMode::my_records ||
		mode_ == NewClassicOnlineMenuMode::downloaded_replays) &&
		page_ == Page::board && flat_selection_ < flat_entries_.size()) {
		return flat_entries_[flat_selection_].first;
	}
	return board_index_;
}

std::size_t NewClassicHighScoreMenu::selected_entry() const noexcept
{
	if((mode_ == NewClassicOnlineMenuMode::my_records ||
		mode_ == NewClassicOnlineMenuMode::downloaded_replays) &&
		page_ == Page::board) {
		return flat_selection_;
	}
	return selected_entry_;
}

NewClassicOnlineMenuMode NewClassicHighScoreMenu::mode() const noexcept
{
	return mode_;
}

std::optional<NewClassicOnlineReplayRecord>
NewClassicHighScoreMenu::take_replay_download_request() noexcept
{
	return std::exchange(download_request_, std::nullopt);
}

bool NewClassicHighScoreMenu::report_replay_download(
	const NewClassicOnlineReplayId &replay_id,
	NewClassicOnlineReplayDownloadOutcome outcome
) noexcept
{
	if(download_state_ != DownloadState::pending || !download_target_ ||
		*download_target_ != replay_id) {
		return false;
	}
	if(outcome == NewClassicOnlineReplayDownloadOutcome::downloaded) {
		auto found = std::ranges::find_if(
			boards_, [&replay_id](const NewClassicOnlineCacheDocument &board) {
				return std::ranges::any_of(
					board.replay_records, [&replay_id](const auto &record) {
						return record.replay_id == replay_id;
					}
				);
			}
		);
		if(found == boards_.end()) {
			return false;
		}
		auto record = std::ranges::find(
			found->replay_records, replay_id,
			&NewClassicOnlineReplayRecord::replay_id
		);
		if(record == found->replay_records.end()) {
			return false;
		}
		record->availability =
			NewClassicOnlineReplayAvailability::downloaded;
		download_state_ = DownloadState::idle;
		download_target_.reset();
		request_replay_load();
	} else {
		download_state_ = outcome ==
			NewClassicOnlineReplayDownloadOutcome::unavailable ?
			DownloadState::unavailable : DownloadState::failed;
	}
	redraw_pending_ = true;
	return true;
}

std::optional<NewClassicOnlineReplayRecord>
NewClassicHighScoreMenu::take_replay_watch_request() noexcept
{
	return std::exchange(watch_request_, std::nullopt);
}

bool NewClassicHighScoreMenu::report_replay_watch(
	const NewClassicOnlineReplayId &replay_id,
	std::optional<formats::ReplayDocument> document
) noexcept
{
	if(watch_state_ != WatchState::pending || !watch_target_ ||
		*watch_target_ != replay_id) {
		return false;
	}
	if(document) {
		try {
			replay_detail_browser_ = NewClassicReplayBrowser::detail(
				std::move(*document), patch_assets_
			);
		} catch(const std::bad_alloc &) {
			return false;
		}
		watch_state_ = WatchState::idle;
		watch_target_.reset();
		redraw_pending_ = true;
		return true;
	}
	for(auto &board : boards_) {
		auto record = std::ranges::find(
			board.replay_records, replay_id,
			&NewClassicOnlineReplayRecord::replay_id
		);
		if(record != board.replay_records.end()) {
			record->availability =
				NewClassicOnlineReplayAvailability::metadata_only;
		}
	}
	download_target_ = replay_id;
	download_state_ = DownloadState::failed;
	watch_state_ = WatchState::idle;
	watch_target_.reset();
	redraw_pending_ = true;
	return true;
}

std::optional<NewClassicOnlineReplayPlaybackRequest>
NewClassicHighScoreMenu::take_replay_playback_request() noexcept
{
	return std::exchange(playback_request_, std::nullopt);
}

bool NewClassicHighScoreMenu::prepare_playback_transition() noexcept
{
	return page_ == Page::detail && replay_detail_browser_ &&
		replay_detail_browser_->prepare_playback_transition();
}

NewClassicReplaySaveMenu::NewClassicReplaySaveMenu(
	formats::ReplayDocument document,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	std::shared_ptr<const NewClassicReplaySaveArt> save_art,
	NewClassicDosDateProvider date_provider,
	void *date_provider_context,
	std::optional<ReplaySlotLocalView> local_view
) :
	document_(std::move(document)),
	patch_assets_(std::move(patch_assets)),
	save_art_(std::move(save_art)),
	date_provider_(date_provider),
	date_provider_context_(date_provider_context),
	local_view_(local_view)
{
	if(document_.metadata && document_.metadata->end_reason ==
		formats::ReplayEndReason::menu_return) {
		phase_ = Phase::name;
		capture_date();
	}
	NewClassicReplayCatalog empty{};
	draw(empty);
	patch_surface_.raster().set_palette_tone(0);
	patch_surface_.raster().apply_pending_palette();
	fade_in_active_ = true;
}

void NewClassicReplaySaveMenu::capture_date() noexcept
{
	if(document_.metadata && date_provider_) {
		document_.metadata->dos_date = date_provider_(date_provider_context_);
	}
}

void NewClassicReplaySaveMenu::apply_name() noexcept
{
	if(document_.metadata) {
		document_.metadata->name = name_;
	}
}

void NewClassicReplaySaveMenu::draw(
	const NewClassicReplayCatalog &catalog
) noexcept
{
	draw_patch(catalog);
}
void NewClassicReplaySaveMenu::draw_patch(
	const NewClassicReplayCatalog &catalog
) noexcept
{
	if(!patch_assets_) {
		return;
	}
	const auto name_background = phase_ == Phase::prompt ||
		phase_ == Phase::name ||
		(phase_ == Phase::quit_confirm &&
			(return_phase_ == Phase::prompt || return_phase_ == Phase::name));
	const auto &background = name_background ?
		patch_assets_->replay_name_background :
		patch_assets_->replay_list_background;
	const auto dialog = phase_ == Phase::prompt ||
		phase_ == Phase::overwrite || phase_ == Phase::quit_confirm;
	const auto previous_dialog = drawn_phase_ &&
		(*drawn_phase_ == Phase::prompt || *drawn_phase_ == Phase::overwrite ||
		 *drawn_phase_ == Phase::quit_confirm);
	if(previous_dialog && !dialog && dialog_restore_page_) {
		if(!patch_surface_.publish(*dialog_restore_page_)) {
			return;
		}
		dialog_restore_page_.reset();
		if(phase_ == Phase::name && *drawn_phase_ != Phase::prompt) {
			drawn_phase_ = phase_;
			return;
		}
	}
	if(previous_dialog && !dialog && phase_ == Phase::name &&
		*drawn_phase_ == Phase::prompt) {
		if(!patch_surface_.reset(background)) {
			return;
		}
	}
	const auto changed_dialog = dialog && previous_dialog && drawn_phase_ &&
		*drawn_phase_ != phase_;
	if(changed_dialog) {
		// Each source yes/no transaction ends by showing its saved copy. A
		// nested Quit confirmation therefore closes the first dialog before
		// copying and drawing the second one.
		if(!dialog_restore_page_ ||
			!patch_surface_.publish(*dialog_restore_page_)) {
			return;
		}
		dialog_restore_page_.reset();
	}
	if(dialog && (!previous_dialog || changed_dialog)) {
		if(!drawn_phase_ && !patch_surface_.reset(background)) {
			return;
		}
		dialog_restore_page_ = patch_surface_.copy_shown_to_hidden();
		if(!dialog_restore_page_) {
			return;
		}
	}
	// Replay Patch dialogs copy the complete visible page to the hidden page,
	// then clear and redraw only the dialog rectangle on the still-visible
	// page. Reblitting SLB1B/SLB1 here would erase the name grid or slot list
	// underneath the dialog until the restore-page flip.
	const auto frame_ready = dialog ?
		patch_surface_.raster().set_access_page(
			patch_surface_.raster().shown_page()
		) :
		(name_background ? patch_surface_.begin_shown_frame(background) :
			patch_surface_.begin_hidden_frame(background));
	if(!frame_ready) {
		return;
	}
	auto &raster = patch_surface_.raster();
	if(!name_background) {
		static_cast<void>(raster.set_pending_palette_entry(12U, {15U, 15U, 0U}));
		raster.apply_pending_palette();
	}
	const auto &font = patch_assets_->menu_font;
	const auto &messages = patch_assets_->messages();
	const auto put = [&](std::int32_t left, std::int32_t top,
		std::string_view text, present::Index4 color) {
		return font.put(raster, left, top, text, color);
	};
	const auto put_right = [&](std::int32_t right, std::int32_t top,
		std::string_view text, present::Index4 color) {
		return font.put_right(raster, right, top, text, color);
	};
	const auto put_regi = [&](std::int32_t left, std::int32_t top,
		ClassicRegistrationGlyph glyph, bool selected) {
		if(!save_art_ || !classic_registration_glyph_valid(glyph)) {
			return false;
		}
		const auto raw = static_cast<std::uint16_t>(glyph);
		const auto &sheet = selected ? save_art_->highlight_ : save_art_->normal_;
		if(!present::superimpose_put(raster, sheet, {left, top}, raw)) {
			return false;
		}
		return glyph != ClassicRegistrationGlyph::space &&
			glyph != ClassicRegistrationGlyph::backspace &&
			glyph != ClassicRegistrationGlyph::end ? true :
			present::superimpose_put(raster, sheet, {left + 32, top}, raw + 1U);
	};
	const auto ascii_regi = [](char character) ->
		std::optional<ClassicRegistrationGlyph> {
		if(character >= 'a' && character <= 'z') {
			character = static_cast<char>('A' + (character - 'a'));
		}
		if(character >= 'A' && character <= 'M') {
			return static_cast<ClassicRegistrationGlyph>(character - 'A');
		}
		if(character >= 'N' && character <= 'Z') {
			return static_cast<ClassicRegistrationGlyph>(
				static_cast<std::uint8_t>(ClassicRegistrationGlyph::n) +
				(character - 'N')
			);
		}
		if(character >= '0' && character <= '9') {
			return static_cast<ClassicRegistrationGlyph>(
				static_cast<std::uint8_t>(ClassicRegistrationGlyph::digit_0) +
				(character - '0')
			);
		}
		switch(character) {
		case '.': return ClassicRegistrationGlyph::period;
		case ',': return ClassicRegistrationGlyph::comma;
		case '!': return ClassicRegistrationGlyph::exclamation;
		case '?': return ClassicRegistrationGlyph::question;
		case '-': return ClassicRegistrationGlyph::blank_1;
		default: return std::nullopt;
		}
	};
	const auto put_regi_text = [&](std::int32_t left, std::int32_t top,
		std::string_view text, std::int32_t spacing) {
		if(!save_art_) {
			return false;
		}
		for(const auto character : text) {
			if(const auto glyph = ascii_regi(character)) {
				if(!put_regi(left, top, *glyph, false)) {
					return false;
				}
			}
			left += spacing;
		}
		return true;
	};

	const auto draw_slots = [&]() {
		static_cast<void>(put(64, 80, messages.text(
			NewClassicMessageId::replay_header_slot
		), 9U));
		static_cast<void>(put(104, 80, messages.text(
			NewClassicMessageId::replay_header_name
		), 9U));
		static_cast<void>(put_right(340, 80, messages.text(
			NewClassicMessageId::replay_header_score
		), 9U));
		static_cast<void>(put(364, 80, messages.text(
			NewClassicMessageId::replay_header_character
		), 9U));
		static_cast<void>(put(476, 80, messages.text(
			NewClassicMessageId::replay_header_rank
		), 9U));
		static_cast<void>(put(560, 80, messages.text(
			NewClassicMessageId::replay_header_stage
		), 9U));
		for(std::size_t row = 0U; row < browser_rows; ++row) {
			const auto slot = static_cast<std::size_t>(top_slot_) + row;
			if(slot >= replay_slot_count) {
				break;
			}
			const auto &entry = catalog.slots[slot];
			const auto color = static_cast<present::Index4>(
				slot == selected_slot_ ? 12U : 15U
			);
			const auto top = static_cast<std::int32_t>(96U + (row * 16U));
			if(slot == selected_slot_) {
				static_cast<void>(put(40, top, ">", 12U));
			}
			static_cast<void>(patch_put_fixed(
				font, raster, 64, top, slot_number(slot), 12, color
			));
			if(entry.state != ReplayCatalogSlotState::ready) {
				static_cast<void>(put(104, top, messages.text(
					NewClassicMessageId::replay_none
				), color));
				continue;
			}
			static_cast<void>(patch_put_fixed(
				font, raster, 104, top, source_fixed_name(entry.name), 13, color
			));
			const auto score = decimal_score(entry.final_score);
			static_cast<void>(patch_put_fixed(
				font, raster,
				340 - static_cast<std::int32_t>(score.size() * 12U), top,
				score, 12, color
			));
			static_cast<void>(put(
				364, top, patch_character_label(
					entry.display_paletted_character, messages
				), color
			));
			static_cast<void>(put(
				476, top, patch_rank_label(entry.rank, messages), color
			));
			static_cast<void>(patch_put_fixed(
				font, raster, 560, top, catalog_marker(entry, messages), 12, color
			));
		}
	};

	if(phase_ == Phase::name) {
		const auto fixed_name = source_fixed_name(name_);
		const auto score = decimal_score(replay_list_score(
			document_, local_view_ ?
				std::optional<std::uint8_t>{local_view_->local_player} : std::nullopt
		));
		if(save_art_) {
			static_cast<void>(put_regi_text(80, 96, fixed_name, 24));
			static_cast<void>(put_regi_text(
				400 + static_cast<std::int32_t>((9U - score.size()) * 16U),
				96, score, 16
			));
		} else {
			static_cast<void>(patch_put_fixed(
				font, raster, 80, 96, fixed_name, 24, 15U
			));
			static_cast<void>(patch_put_fixed(
				font, raster,
				400 + static_cast<std::int32_t>((9U - score.size()) * 16U),
				96, score, 16, 15U
			));
		}
		if(document_.metadata) {
			const auto date = dos_date_label(document_.metadata->dos_date);
			if(save_art_) {
				static_cast<void>(put_regi_text(80, 160, date, 20));
			} else {
				static_cast<void>(patch_put_fixed(
					font, raster, 80, 160, date, 20, 15U
				));
			}
			const auto rank = patch_rank_label(
				document_.metadata->rank, messages
			);
			const auto rank_initial = rank.empty() ? std::string{"?"} :
				rank.substr(0U, 1U);
			const auto player = local_view_ ? local_view_->local_player : 0U;
			if(save_art_) {
				static_cast<void>(put_regi_text(344, 160, rank_initial, 24));
				const auto paletted = document_.metadata->paletted_character[player];
				const auto character = paletted >= 1 && paletted <= 18 ?
					static_cast<std::size_t>((paletted - 1) / 2) : 9U;
				if(save_art_->character_names_[character]) {
					const auto &name = *save_art_->character_names_[character];
					const auto left = 416 + static_cast<std::int32_t>(
						(96U - name.pixel_width()) / 2U
					);
					static_cast<void>(present::graphics_text_put_8x16(
						raster, {left, 168}, name,
						{.color = 15U,
						 .weight = present::GraphicsTextWeight::bold}
					));
				}
			} else {
				static_cast<void>(put(344, 160, rank_initial, 15U));
				static_cast<void>(font.put_centered(
					raster, 464, 168,
					patch_character_label(
						document_.metadata->paletted_character[player], messages
					),
					15U
				));
			}
			std::string marker;
			if(document_.metadata->practice) {
				marker = messages.text(
					NewClassicMessageId::replay_marker_practice_short
				);
			} else if(document_.metadata->game_mode != game_mode::story) {
				marker = messages.text(
					NewClassicMessageId::replay_marker_vs
				);
			} else if(document_.metadata->end_reason ==
				formats::ReplayEndReason::complete) {
				if(save_art_) {
					static_cast<void>(put_regi(
						528, 160, ClassicRegistrationGlyph::all, false
					));
				}
			} else if(document_.metadata->stage_reached_count >= 1U &&
				document_.metadata->stage_reached_count <= 9U) {
				marker = std::to_string(document_.metadata->stage_reached_count);
			} else {
				marker = "-";
			}
			if(!marker.empty()) {
				const auto left = 560 - static_cast<std::int32_t>(marker.size() * 24U);
				if(save_art_) {
					static_cast<void>(put_regi_text(left, 160, marker, 24));
				} else {
					static_cast<void>(patch_put_fixed(
						font, raster, left, 160, marker, 24, 15U
					));
				}
			}
		}
		for(std::size_t position = 0U; position < replay_name_grid_count;
			++position) {
			if(replay_name_grid_skip(position)) {
				continue;
			}
			const auto left = 64 + static_cast<std::int32_t>(
				(position % replay_name_grid_columns) * 32U
			);
			const auto top = 256 + static_cast<std::int32_t>(
				(position / replay_name_grid_columns) * 24U
			);
			if(save_art_) {
				static_cast<void>(put_regi(
					left, top, static_cast<ClassicRegistrationGlyph>(position),
					position == keyboard_position_
				));
			} else {
				static_cast<void>(put(
					left, top, replay_name_grid_label(position),
					position == keyboard_position_ ? 12U : 15U
				));
			}
		}
	} else if(!name_background) {
		draw_slots();
		if(phase_ == Phase::saved_wait) {
			static_cast<void>(font.put_centered(
				raster, 320, 352, messages.text(
					NewClassicMessageId::replay_save_saved
				), 13U
			));
		}
	}

	if(phase_ == Phase::prompt || phase_ == Phase::overwrite ||
		phase_ == Phase::quit_confirm) {
		static_cast<void>(raster.clear({208, 160, 224U, 80U}, 0U));
		std::string question;
		if(phase_ == Phase::prompt) {
			question = messages.text(NewClassicMessageId::replay_save_prompt);
		} else if(phase_ == Phase::overwrite) {
			const auto slot = slot_number(selected_slot_);
			const std::array arguments{
				NewClassicMessageArgument{"slot", slot},
			};
			question = messages.format(
				NewClassicMessageId::replay_save_overwrite, arguments
			);
		} else {
			question = messages.text(NewClassicMessageId::replay_save_quit);
		}
		static_cast<void>(font.put_centered(raster, 320, 176, question, 9U));
		static_cast<void>(put(264, 208, messages.text(
			NewClassicMessageId::replay_save_yes
		), yes_ ? 12U : 15U));
		static_cast<void>(put(360, 208, messages.text(
			NewClassicMessageId::replay_save_no
		), yes_ ? 15U : 12U));
	}
	static_cast<void>(patch_surface_.publish(raster.access_page()));
	drawn_phase_ = phase_;
}

NewClassicReplaySaveResult NewClassicReplaySaveMenu::consume(
	NewClassicUiInput input,
	const NewClassicReplayCatalog &catalog
) noexcept
{
	if(status_ != NewClassicReplaySaveStatus::running) {
		return {.status = status_};
	}
	if(input.app_quit_requested) {
		status_ = NewClassicReplaySaveStatus::application_quit;
		return {
			.surface_changed = patch_surface_.terminal_black_clear(),
			.status = status_,
		};
	}
	if(fade_in_active_) {
		auto &raster = patch_surface_.raster();
		bool changed = false;
		if(fade_boundary_ != 0U && fade_boundary_ <= 17U) {
			const auto pair = static_cast<std::int16_t>(fade_boundary_ - 1U);
			raster.set_palette_tone(static_cast<std::int16_t>(pair * 6));
			raster.apply_pending_palette();
			changed = true;
		}
		++fade_boundary_;
		if(fade_boundary_ >= palette_fade_boundary_count) {
			raster.set_palette_tone(100);
			raster.apply_pending_palette();
			fade_in_active_ = false;
			input_allowed_ = !any(input);
			changed = true;
		}
		return {
			.surface_changed = changed,
			.status = status_,
		};
	}
	if(!input_allowed_) {
		if(!any(input)) {
			input_allowed_ = true;
		}
		return {.status = status_};
	}
	if(!any(input)) {
		return {.status = status_};
	}
	input_allowed_ = false;
	if(phase_ == Phase::saved_wait) {
		status_ = NewClassicReplaySaveStatus::saved;
		return {
			.surface_changed = patch_surface_.terminal_black_clear(),
			.status = status_,
		};
	}
	if(phase_ == Phase::quit_confirm) {
		if(input.cancel) {
			phase_ = return_phase_;
			yes_ = return_yes_;
			draw(catalog);
			return {.surface_changed = true, .status = status_};
		}
		if(input.up || input.down || input.left || input.right) {
			yes_ = !yes_;
			draw(catalog);
			return {.surface_changed = true, .status = status_};
		}
		if(input.confirm) {
			if(yes_) {
				status_ = NewClassicReplaySaveStatus::discarded;
				return {
					.surface_changed = patch_surface_.terminal_black_clear(),
					.status = status_,
				};
			}
			phase_ = return_phase_;
			yes_ = return_yes_;
			draw(catalog);
			return {.surface_changed = true, .status = status_};
		}
		return {.status = status_};
	}
	if(phase_ == Phase::prompt || phase_ == Phase::overwrite) {
		if(input.cancel) {
			return_phase_ = phase_ == Phase::overwrite ? Phase::slots : phase_;
			return_yes_ = phase_ == Phase::overwrite ? false : yes_;
			phase_ = Phase::quit_confirm;
			yes_ = false;
			draw(catalog);
			return {.surface_changed = true, .status = status_};
		}
		if(input.up || input.down || input.left || input.right) {
			yes_ = !yes_;
			draw(catalog);
			return {.surface_changed = true, .status = status_};
		}
		if(input.confirm) {
			if(phase_ == Phase::prompt) {
				if(!yes_) {
					status_ = NewClassicReplaySaveStatus::discarded;
					return {
						.surface_changed = patch_surface_.terminal_black_clear(),
						.status = status_,
					};
				} else {
					phase_ = Phase::name;
					capture_date();
					draw(catalog);
					return {.surface_changed = true, .status = status_};
				}
			} else if(yes_) {
				status_ = NewClassicReplaySaveStatus::save_requested;
			} else {
				phase_ = Phase::slots;
				draw(catalog);
				return {.surface_changed = true, .status = status_};
			}
		}
		return {.status = status_};
	}
	if(phase_ == Phase::name) {
		if(input.cancel) {
			return_phase_ = phase_;
			return_yes_ = yes_;
			phase_ = Phase::quit_confirm;
			yes_ = false;
			draw(catalog);
			return {.surface_changed = true, .status = status_};
		}
		if(input.bomb) {
			if(name_position_ != 0U) {
				--name_position_;
				name_[name_position_] = ' ';
			}
			draw(catalog);
			return {.surface_changed = true, .status = status_};
		}
		if(input.left) {
			if((keyboard_position_ % replay_name_grid_columns) == 0U) {
				keyboard_position_ += keyboard_space;
			} else if(keyboard_position_ == keyboard_back ||
				keyboard_position_ == keyboard_space) {
				keyboard_position_ -= 2U;
			} else {
				--keyboard_position_;
			}
		} else if(input.right) {
			if((keyboard_position_ % replay_name_grid_columns) == keyboard_space) {
				keyboard_position_ -= keyboard_space;
			} else if(keyboard_position_ == 12U || keyboard_position_ == 28U) {
				keyboard_position_ += 2U;
			} else {
				++keyboard_position_;
			}
		} else if(input.up) {
			if(keyboard_position_ != keyboard_question) {
				keyboard_position_ = (keyboard_position_ +
					replay_name_grid_count - replay_name_grid_columns) %
					replay_name_grid_count;
			}
		} else if(input.down) {
			if(keyboard_position_ != keyboard_question) {
				keyboard_position_ = (keyboard_position_ +
					replay_name_grid_columns) % replay_name_grid_count;
			}
		} else if(input.confirm) {
			if(keyboard_position_ == keyboard_end) {
				apply_name();
				phase_ = Phase::slots;
			} else if(keyboard_position_ == keyboard_back) {
				if(name_position_ != 0U) {
					--name_position_;
					name_[name_position_] = ' ';
				}
			} else {
				const auto value = replay_name_ascii(keyboard_position_);
				if(value != '\0' && name_position_ < name_.size()) {
					name_[name_position_++] = value;
					if(name_position_ == name_.size()) {
						keyboard_position_ = keyboard_end;
					}
				}
			}
		}
		draw(catalog);
		return {.surface_changed = true, .status = status_};
	}
	if(input.cancel) {
		return_phase_ = phase_;
		return_yes_ = yes_;
		phase_ = Phase::quit_confirm;
		yes_ = false;
		draw(catalog);
		return {.surface_changed = true, .status = status_};
	}
	if(input.up) {
		selected_slot_ = static_cast<std::uint8_t>(
			(selected_slot_ + replay_slot_count - 1U) % replay_slot_count);
	} else if(input.down) {
		selected_slot_ = static_cast<std::uint8_t>(
			(selected_slot_ + 1U) % replay_slot_count);
	} else if(input.left) {
		selected_slot_ = static_cast<std::uint8_t>(
			selected_slot_ < browser_rows ? 0U : selected_slot_ - browser_rows);
	} else if(input.right) {
		selected_slot_ = static_cast<std::uint8_t>(
			std::min<std::size_t>(selected_slot_ + browser_rows,
				replay_slot_count - 1U));
	} else if(input.confirm) {
		if(catalog.slots[selected_slot_].state != ReplayCatalogSlotState::empty) {
			yes_ = false;
			phase_ = Phase::overwrite;
			draw(catalog);
			return {.surface_changed = true, .status = status_};
		}
		status_ = NewClassicReplaySaveStatus::save_requested;
		return {.status = status_};
	} else {
		return {.status = status_};
	}
	if(selected_slot_ < top_slot_) {
		top_slot_ = selected_slot_;
	} else if(static_cast<std::size_t>(selected_slot_) >=
		static_cast<std::size_t>(top_slot_) + browser_rows) {
		top_slot_ = static_cast<std::uint8_t>(
			selected_slot_ - (browser_rows - 1U)
		);
	}
	if(top_slot_ > replay_slot_count - browser_rows) {
		top_slot_ = static_cast<std::uint8_t>(
			replay_slot_count - browser_rows
		);
	}
	draw(catalog);
	return {.surface_changed = true, .status = status_};
}

void NewClassicReplaySaveMenu::report_commit(
	bool success,
	const NewClassicReplayCatalog &catalog
) noexcept
{
	if(status_ != NewClassicReplaySaveStatus::save_requested) {
		return;
	}
	if(success) {
		commit_failed_ = false;
		status_ = NewClassicReplaySaveStatus::running;
		phase_ = Phase::saved_wait;
		draw(catalog);
		return;
	}
	commit_failed_ = true;
	status_ = NewClassicReplaySaveStatus::running;
	phase_ = Phase::slots;
	draw(catalog);
}

present::SourceSurface NewClassicReplaySaveMenu::surface() const noexcept
{
	return patch_surface_.surface();
}

std::uint8_t NewClassicReplaySaveMenu::selected_slot() const noexcept
{
	return selected_slot_;
}

const formats::ReplayDocument &NewClassicReplaySaveMenu::document() const noexcept
{
	return document_;
}

formats::ReplayDocument NewClassicReplaySaveMenu::take_document()
{
	return std::move(document_);
}

bool NewClassicReplaySaveMenu::commit_failed() const noexcept
{
	return commit_failed_;
}

std::array<char, 8U> NewClassicReplaySaveMenu::edited_name() const noexcept
{
	return name_;
}

void NewClassicReplaySaveTestAccess::select_end(
	NewClassicReplaySaveMenu &menu
) noexcept
{
	menu.keyboard_position_ = keyboard_end;
}

void NewClassicReplaySaveTestAccess::set_slot(
	NewClassicReplaySaveMenu &menu,
	std::uint8_t slot
) noexcept
{
	menu.selected_slot_ = slot < replay_slot_count ? slot : 0U;
	menu.top_slot_ = static_cast<std::uint8_t>(std::min<std::size_t>(
		menu.selected_slot_, replay_slot_count - browser_rows
	));
}

bool NewClassicReplaySaveTestAccess::in_name(
	const NewClassicReplaySaveMenu &menu
) noexcept
{
	return menu.phase_ == NewClassicReplaySaveMenu::Phase::name;
}

bool NewClassicReplaySaveTestAccess::in_slots(
	const NewClassicReplaySaveMenu &menu
) noexcept
{
	return menu.phase_ == NewClassicReplaySaveMenu::Phase::slots;
}

bool NewClassicReplaySaveTestAccess::in_overwrite(
	const NewClassicReplaySaveMenu &menu
) noexcept
{
	return menu.phase_ == NewClassicReplaySaveMenu::Phase::overwrite;
}

bool NewClassicReplaySaveTestAccess::in_quit_confirm(
	const NewClassicReplaySaveMenu &menu
) noexcept
{
	return menu.phase_ == NewClassicReplaySaveMenu::Phase::quit_confirm;
}

std::size_t NewClassicReplaySaveTestAccess::name_position(
	const NewClassicReplaySaveMenu &menu
) noexcept
{
	return menu.name_position_;
}

} // namespace th03::app
