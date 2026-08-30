#include "new_classic/replay/new_classic_replay_presentation_internal.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string>

namespace th03::app::replay_presentation {

bool put_fixed(
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

[[nodiscard]] bool ui_input_present(NewClassicUiInput input) noexcept
{
	return input.up || input.down || input.left || input.right ||
		input.confirm || input.bomb || input.cancel;
}

[[nodiscard]] std::string_view localized_rank_label(
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

[[nodiscard]] std::string_view localized_character_label(
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

[[nodiscard]] std::size_t page_top(
	std::size_t selection, std::size_t rows
) noexcept
{
	return rows == 0U ? 0U : (selection / rows) * rows;
}

[[nodiscard]] std::size_t page_count(
	std::size_t entries, std::size_t rows
) noexcept
{
	return rows == 0U ? 1U : std::max<std::size_t>(
		1U, (entries + rows - 1U) / rows
	);
}

[[nodiscard]] std::string page_label(
	std::size_t selection,
	std::size_t entries,
	std::size_t rows,
	const NewClassicMessageCatalog &messages
)
{
	const auto pages = page_count(entries, rows);
	const auto page = rows == 0U ? 1U :
		std::min((selection / rows) + 1U, pages);
	const auto page_value = std::to_string(page);
	const auto pages_value = std::to_string(pages);
	const std::array arguments{
		NewClassicMessageArgument{"page", page_value},
		NewClassicMessageArgument{"pages", pages_value},
	};
	return messages.format(NewClassicMessageId::replay_page, arguments);
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

namespace {

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

} // namespace

[[nodiscard]] std::array<std::uint8_t, 4U> replay_list_score(
	const formats::ReplayDocument &document,
	std::optional<std::uint8_t> local_player
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

} // namespace th03::app::replay_presentation
