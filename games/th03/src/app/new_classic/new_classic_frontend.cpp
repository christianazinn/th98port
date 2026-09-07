#include "th03/app/new_classic/new_classic_frontend.hpp"

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

#include "new_classic/replay/new_classic_replay_presentation_internal.hpp"

namespace th03::app {
namespace {

constexpr std::size_t browser_rows = 10U;
constexpr std::uint8_t palette_fade_boundary_count = 19U;

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

std::string source_trimmed_name(const std::array<char, 8U> &name)
{
	auto result = replay_presentation::source_fixed_name(name);
	while(!result.empty() && result.back() == ' ') {
		result.pop_back();
	}
	return result;
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
				replay_presentation::decimal_score(metadata->story->stage_scores[stage]) :
				split ? replay_presentation::decimal_score(split->p1_score) : "-";
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
		slot.final_score = replay_presentation::replay_list_score(
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
		top_slot_ = static_cast<std::uint8_t>(
			replay_presentation::page_top(selected_slot_, browser_rows)
		);
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
			const auto top = replay_presentation::browser_row_top +
				static_cast<std::int32_t>(
					row * replay_presentation::browser_row_height
				);
			if(slot == selected_slot_) {
				drawn = put(40, top, ">", 12U);
			}
			drawn = drawn && replay_presentation::put_fixed(
				font, raster, 64, top,
				replay_presentation::slot_list_number(slot), 12, color
			);
			if(entry.state != ReplayCatalogSlotState::ready) {
				drawn = drawn && put(104, top, messages.text(
					NewClassicMessageId::replay_none), color);
				continue;
			}
			drawn = drawn && replay_presentation::put_fixed(
				font, raster, 104, top, replay_presentation::source_fixed_name(entry.name), 13, color
			);
			const auto score = replay_presentation::decimal_score(entry.final_score);
			drawn = drawn && replay_presentation::put_fixed(
				font, raster,
				340 - static_cast<std::int32_t>(score.size() * 12U), top,
				score, 12, color
			) && put(364, top,
				replay_presentation::localized_character_label(
					entry.display_paletted_character, messages
				), color) &&
				put(476, top, replay_presentation::localized_rank_label(entry.rank, messages), color) &&
				replay_presentation::put_fixed(
					font, raster, 560, top, replay_presentation::catalog_marker(entry, messages), 12, color
				);
		}
		drawn = drawn && font.put_centered(
			raster, 320, replay_presentation::browser_footer_top,
			replay_presentation::page_label(
				selected_slot_, replay_slot_count, browser_rows, messages
			),
			15U
		);
		static_cast<void>(drawn);
		static_cast<void>(patch_surface_.publish(raster.access_page()));
		return;
	}

	const auto &document = *detail_document_;
	const auto &entry = catalog_.slots[selected_slot_];
	const auto *metadata = document.metadata ? &*document.metadata : nullptr;
	const auto trimmed_name = source_trimmed_name(entry.name);
	static_cast<void>(put(48, 80, trimmed_name, 15U));
	const auto slot = replay_presentation::slot_number(selected_slot_);
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
		const auto score = replay_presentation::decimal_score(entry.final_score);
		static_cast<void>(replay_presentation::put_fixed(
			font, raster, 304 - static_cast<std::int32_t>(score.size() * 12U),
			112, score, 12, 15U
		));
		static_cast<void>(put(48, 128, messages.text(
			NewClassicMessageId::replay_label_date), 15U));
		static_cast<void>(put_right(304, 128,
			replay_presentation::dos_date_label(metadata->dos_date), 15U));
		static_cast<void>(put(
			48, 144,
			std::string(replay_presentation::localized_rank_label(metadata->rank, messages)) + " " +
			std::string(replay_presentation::localized_character_label(
				metadata->paletted_character[0U], messages
			)) +
			(story ? "" : std::string(messages.text(
				NewClassicMessageId::replay_summary_vs)) +
				std::string(replay_presentation::localized_character_label(
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
				static_cast<void>(replay_presentation::put_fixed(
					font, raster, 344, top, std::to_string(stage + 1U),
					12, 15U
				));
				const bool reached = metadata && metadata->story &&
					stage < metadata->stage_reached_count;
				static_cast<void>(put(
					376, top,
					reached ? replay_presentation::localized_character_label(static_cast<std::int8_t>(
						metadata->story->stage_opponents[stage]
					), messages) : "-", 15U
				));
				const auto valid = document.stage_clear_bonuses &&
					clear_bonus_valid((*document.stage_clear_bonuses)[stage]);
				const auto score = valid ? replay_presentation::decimal_score(
					(*document.stage_clear_bonuses)[stage].total
				) : std::string(9U, '-');
				static_cast<void>(replay_presentation::put_fixed(
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
					const auto score = split ? replay_presentation::decimal_score(split->p1_score) :
						std::string(9U, '-');
					static_cast<void>(replay_presentation::put_fixed(
						font, raster,
						592 - static_cast<std::int32_t>(score.size() * 12U),
						top, score, 12, color
					));
				}
				continue;
			}

			static_cast<void>(replay_presentation::put_fixed(
				font, raster, 344, top, std::to_string(row.stage + 1U),
				12, color
			));
			const auto reached = row.stage < metadata->stage_reached_count &&
				metadata->story.has_value();
			static_cast<void>(put(
				376, top,
				reached ? replay_presentation::localized_character_label(static_cast<std::int8_t>(
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
					replay_presentation::decimal_score(metadata->story->stage_scores[row.stage]) :
					std::string(9U, '-');
				static_cast<void>(replay_presentation::put_fixed(
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
			const auto p1 = split ? replay_presentation::decimal_score(split->p1_score) :
				std::string(9U, '-');
			const auto p2 = split ? replay_presentation::decimal_score(split->p2_score) :
				std::string(9U, '-');
			static_cast<void>(replay_presentation::put_fixed(
				font, raster, 452 - static_cast<std::int32_t>(p1.size() * 12U),
				top, p1, 12, color
			));
			static_cast<void>(replay_presentation::put_fixed(
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
			input_allowed_ = !replay_presentation::ui_input_present(input);
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
		if(!replay_presentation::ui_input_present(input)) {
			input_allowed_ = true;
		}
		return NewClassicReplayBrowserResult{.status = status_};
	}
	if(!replay_presentation::ui_input_present(input)) {
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
			selected_slot_ < browser_rows ?
				selected_slot_ + replay_slot_count - browser_rows :
				selected_slot_ - browser_rows
		);
	} else if(input.right) {
		selected_slot_ = static_cast<std::uint8_t>(
			selected_slot_ >= replay_slot_count - browser_rows ?
				selected_slot_ - replay_slot_count + browser_rows :
				selected_slot_ + browser_rows
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
	top_slot_ = static_cast<std::uint8_t>(
		replay_presentation::page_top(selected_slot_, browser_rows)
	);
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

std::uint8_t NewClassicReplayBrowserTestAccess::top_slot(
	const NewClassicReplayBrowser &browser
) noexcept
{
	return browser.top_slot_;
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

} // namespace th03::app
