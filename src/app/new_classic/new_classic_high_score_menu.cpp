#include "th03/app/new_classic/new_classic_frontend.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "new_classic/replay/new_classic_replay_presentation_internal.hpp"

namespace th03::app {
namespace {

constexpr std::size_t screen_center = present::graphics_width / 2U;
constexpr std::size_t browser_rows = 10U;
constexpr std::uint8_t palette_fade_boundary_count = 19U;

[[nodiscard]] std::size_t adjacent_page_selection(
	std::size_t selection,
	std::size_t entries,
	bool forward
) noexcept
{
	if(entries == 0U) {
		return 0U;
	}
	const auto pages = replay_presentation::page_count(entries, browser_rows);
	const auto current_page = selection / browser_rows;
	const auto target_page = forward ? (current_page + 1U) % pages :
		(current_page + pages - 1U) % pages;
	return std::min(
		(target_page * browser_rows) + (selection % browser_rows),
		entries - 1U
	);
}

[[nodiscard]] std::optional<std::int32_t> put_online_category(
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
			rank_width, font.width(replay_presentation::localized_rank_label(value, messages))
		);
	}
	std::int32_t character_width = 0;
	for(std::uint8_t value = 0U;
		value <= new_classic_online_overall_character; ++value) {
		const auto label = value == new_classic_online_overall_character ?
			messages.text(NewClassicMessageId::hiscore_online_overall) :
			replay_presentation::localized_character_label(
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
	const auto rank_label = replay_presentation::localized_rank_label(rank, messages);
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
		replay_presentation::localized_character_label(
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

} // namespace

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
	const auto rank = replay_presentation::localized_rank_label(rank_, messages);
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
		valid_ = font.put(raster, 40, top, replay_presentation::localized_character_label(
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
		rank_center = put_online_category(
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
		rank_center = put_online_category(
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
		const auto category_value = flat ? std::string(replay_presentation::localized_rank_label(
			row_category.rank, messages
		)) : dos_date_short_label(entry.dos_date);
		const auto row_character = replay_presentation::localized_character_label(
			static_cast<std::int8_t>((row_category.character * 2U) + 1U), messages
		);
		valid_ = valid_ && (flat || replay_presentation::put_fixed(
			font, raster, 32, top, online_place(list_index), 12, color
		)) && font.put(raster, flat ? 32 : 80, top, name, color) &&
			replay_presentation::put_fixed(font, raster,
				324 - static_cast<std::int32_t>(score.size() * 12U), top,
				score, 12, color) &&
			font.put(raster, 348, top, row_character, color) &&
			(flat ? font.put(raster, 464, top, category_value, color) :
				replay_presentation::put_fixed(font, raster, 464, top, category_value, 11, color)) &&
			font.put(raster, 576, top, stage, color);
	}
	const auto list_size = flat ? flat_entries_.size() :
		board.leaderboard_entries.size();
	const auto selection = flat ? flat_selection_ : selected_entry_;
	valid_ = valid_ && font.put_centered(
		raster, 320, 368,
		replay_presentation::page_label(
			selection, list_size, browser_rows, messages
		),
		15U
	);
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
	const auto rank = replay_presentation::localized_rank_label(category.rank, messages);
	const auto character = replay_presentation::localized_character_label(
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
			replay_presentation::dos_date_label(entry.dos_date)) &&
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
			input_allowed_ = !replay_presentation::ui_input_present(input);
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
		if(!replay_presentation::ui_input_present(input)) {
			input_allowed_ = true;
		}
		return {.status = status_};
	}
	if(watch_state_ == WatchState::pending) {
		return {.status = status_};
	}
	if(!replay_presentation::ui_input_present(input)) {
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
		} else if((input.left || input.right) && count != 0U) {
			flat_selection_ = adjacent_page_selection(
				flat_selection_, count, input.right
			);
		} else if(input.confirm && count != 0U) {
			const auto location = flat_entries_[flat_selection_];
			board_index_ = location.first;
			selected_entry_ = location.second;
			page_ = Page::detail;
			request_replay_load();
		} else {
			return {.status = status_};
		}
		flat_top_ = replay_presentation::page_top(
			flat_selection_, browser_rows
		);
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
		top_entry_ = replay_presentation::page_top(
			selected_entry_, browser_rows
		);
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

} // namespace th03::app
