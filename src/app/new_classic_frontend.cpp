#include "th03/app/new_classic_frontend.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace th03::app {
namespace {

constexpr std::size_t screen_center = present::graphics_width / 2U;
constexpr std::size_t browser_rows = 15U;
constexpr std::array<std::string_view, 6U> landing_labels{
	"PLAY GAME", "REPLAY", "PRACTICE", "KEY CONFIG", "OPTIONS", "BACK",
};
constexpr std::string_view keyboard_symbols =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 <E";
constexpr std::size_t keyboard_columns = 13U;
constexpr std::size_t keyboard_end = 38U;
constexpr std::size_t keyboard_back = 37U;

[[nodiscard]] constexpr bool any(NewClassicUiInput input) noexcept
{
	return input.up || input.down || input.left || input.right ||
		input.confirm || input.cancel;
}

void draw_header(HostMenuCanvas &canvas, std::string_view subtitle) noexcept
{
	canvas.clear();
	canvas.fill_rect(32U, 20U, 576U, 58U, 1U);
	canvas.fill_rect(36U, 24U, 568U, 50U, 3U);
	canvas.draw_text_centered("NEW CLASSIC", screen_center, 33U, 3U, 15U);
	canvas.draw_text_centered(subtitle, screen_center, 62U, 1U, 14U);
}

void draw_row(
	HostMenuCanvas &canvas,
	std::string_view label,
	std::size_t y,
	bool selected,
	std::uint8_t normal_color = 15U
) noexcept
{
	canvas.fill_rect(104U, y, 432U, 32U, selected ? 4U : 1U);
	canvas.fill_rect(108U, y + 3U, 424U, 26U, selected ? 3U : 2U);
	canvas.draw_text_centered(
		label, screen_center, y + 7U, 2U, selected ? 14U : normal_color
	);
}

[[nodiscard]] std::string fixed_name(const std::array<char, 8U> &name)
{
	std::string result(name.begin(), name.end());
	while(!result.empty() && (result.back() == ' ' || result.back() == '\0')) {
		result.pop_back();
	}
	return result.empty() ? std::string{"NO NAME"} : result;
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
	result.reserve(8U);
	for(auto cursor = packed.rbegin(); cursor != packed.rend(); ++cursor) {
		const auto value = *cursor;
		result.push_back(static_cast<char>('0' + (value >> 4U)));
		result.push_back(static_cast<char>('0' + (value & 0x0FU)));
	}
	return result;
}

[[nodiscard]] std::string mode_label(std::uint8_t mode, bool practice)
{
	if(practice) {
		return "PRACTICE";
	}
	if(mode == game_mode::story) {
		return "STORY";
	}
	if(mode == game_mode::vs_1p_cpu) {
		return "1P CPU";
	}
	if(mode == game_mode::vs_1p_2p) {
		return "1P 2P";
	}
	if(mode == game_mode::vs_cpu_cpu) {
		return "CPU CPU";
	}
	return "UNKNOWN";
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
		buffer, sizeof(buffer), "%04u-%02u-%02u",
		static_cast<unsigned>(1980U + (packed >> 9U)),
		static_cast<unsigned>((packed >> 5U) & 0x0FU),
		static_cast<unsigned>(packed & 0x1FU)
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

[[nodiscard]] std::string winner_label(std::int8_t winner)
{
	if(winner == 0) {
		return "P1";
	}
	if(winner == 1) {
		return "P2";
	}
	return "-";
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

void draw_compact_row(
	HostMenuCanvas &canvas,
	std::string_view label,
	std::size_t y,
	bool selected
) noexcept
{
	canvas.fill_rect(48U, y, 544U, 17U, selected ? 4U : 1U);
	canvas.draw_text(label, 58U, y + 3U, 1U, selected ? 14U : 15U);
}

void draw_detail_tabs(
	HostMenuCanvas &canvas,
	NewClassicReplayDetailPage selected
) noexcept
{
	constexpr std::array<std::string_view, 3U> labels{
		"SPLITS", "CLEARS", "TIMERS",
	};
	for(std::size_t index = 0U; index < labels.size(); ++index) {
		const auto active = index == static_cast<std::size_t>(selected);
		const auto x = 104U + (index * 144U);
		canvas.fill_rect(x, 164U, 128U, 19U, active ? 4U : 1U);
		canvas.draw_text_centered(
			labels[index], x + 64U, 168U, 1U, active ? 14U : 6U
		);
	}
}

[[nodiscard]] ReplayCatalogSlot catalog_slot(
	const formats::ReplayDocument &document
) noexcept
{
	ReplayCatalogSlot slot{
		.state = ReplayCatalogSlotState::ready,
		.checkpoint_count = static_cast<std::uint8_t>(std::min<std::size_t>(
			document.checkpoints.size(), 0xFFU
		)),
	};
	if(document.metadata) {
		slot.name = document.metadata->name;
		slot.game_mode = document.metadata->game_mode;
		slot.rank = document.metadata->rank;
		slot.stage_reached = document.metadata->stage_reached_count;
		slot.final_score = document.metadata->final_score;
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

NewClassicLandingMenu::NewClassicLandingMenu()
{
	draw();
}

void NewClassicLandingMenu::draw() noexcept
{
	draw_header(canvas_, "REPLAY PATCH 0.4.10 EXPERIENCE");
	for(std::size_t index = 0U; index < landing_labels.size(); ++index) {
		draw_row(
			canvas_, landing_labels[index], 98U + (index * 46U),
			index == static_cast<std::size_t>(choice_)
		);
	}
}

NewClassicLandingResult NewClassicLandingMenu::consume(
	NewClassicUiInput input
) noexcept
{
	if(status_ != NewClassicLandingStatus::running) {
		return {.status = status_, .choice = choice_};
	}
	if(input.app_quit_requested) {
		status_ = NewClassicLandingStatus::application_quit;
		return {.status = status_, .choice = choice_};
	}
	if(!input_allowed_) {
		if(!any(input)) {
			input_allowed_ = true;
		}
		return {.status = status_, .choice = choice_};
	}
	if(!any(input)) {
		return {.status = status_, .choice = choice_};
	}
	input_allowed_ = false;
	if(input.cancel) {
		choice_ = NewClassicLandingChoice::back;
		status_ = NewClassicLandingStatus::selected;
		return {.status = status_, .choice = choice_};
	}
	if(input.up || input.down) {
		const auto count = landing_labels.size();
		const auto old = static_cast<std::size_t>(choice_);
		choice_ = static_cast<NewClassicLandingChoice>(
			input.up ? (old + count - 1U) % count : (old + 1U) % count
		);
		draw();
		return {.surface_changed = true, .status = status_, .choice = choice_};
	}
	if(input.confirm) {
		status_ = NewClassicLandingStatus::selected;
	}
	return {.status = status_, .choice = choice_};
}

present::SourceSurface NewClassicLandingMenu::surface() const noexcept
{
	return canvas_.surface();
}

NewClassicLandingChoice NewClassicLandingMenu::choice() const noexcept
{
	return choice_;
}

NewClassicOptionsMenu::NewClassicOptionsMenu(
	ClassicConfigOptions config,
	bool english_available
) noexcept : config_(config), english_available_(english_available)
{
	if(config_.bgm_mode == ClassicBgmMode::midi) {
		config_.bgm_mode = ClassicBgmMode::off;
	}
	if(!english_available_ && config_.language != ClassicLanguage::japanese) {
		config_.language = ClassicLanguage::japanese;
		english_unavailable_notice_ = true;
	}
	draw();
}

void NewClassicOptionsMenu::draw() noexcept
{
	draw_header(canvas_, commit_failure_ ? "OPTIONS SAVE FAILED" :
		"NEW CLASSIC OPTIONS");
	if(commit_failure_) {
		canvas_.draw_text_centered(
			"COULD NOT SAVE YUME.CFG", screen_center, 170U, 2U, 14U
		);
		canvas_.draw_text_centered(
			"Z: RETRY   ESC: RETURN", screen_center, 220U, 2U, 15U
		);
		return;
	}
	constexpr std::array<std::string_view, 7U> labels{
		"Rank", "Music", "Sound Effects", "Language", "KeyConfig",
		"Flash FX", "Back",
	};
	constexpr std::array<std::string_view, 4U> ranks{
		"Easy", "Normal", "Hard", "Lunatic",
	};
	for(std::size_t row = 0U; row < labels.size(); ++row) {
		std::string value{};
		switch(row) {
		case 0U:
			value = ranks[static_cast<std::size_t>(config_.rank)];
			break;
		case 1U:
			value = config_.bgm_mode == ClassicBgmMode::off ? "Off" : "FM";
			break;
		case 2U: value = config_.sfx_enabled ? "On" : "Off"; break;
		case 3U: value = config_.language == ClassicLanguage::english ?
			"English" : "Japanese"; break;
		case 5U: value = config_.reduced_flash ? "Reduced" : "Full"; break;
		default: break;
		}
		const auto y = 100U + (row * 38U);
		if(row == selected_) {
			canvas_.fill_rect(96U, y - 5U, 448U, 29U, 3U);
		}
		canvas_.draw_text(
			(row == selected_ ? "> " : "  ") + std::string(labels[row]),
			112U, y, 2U, row == selected_ ? 14U : 15U
		);
		if(!value.empty()) {
			canvas_.draw_text(value, 410U, y, 2U,
				row == selected_ ? 14U : 7U);
		}
	}
	canvas_.draw_text_centered(
		english_unavailable_notice_ ?
			"English resource overlay is not installed." :
			"Arrows: Change   Z: Select   Esc: Save/Back",
		screen_center, 382U, 1U,
		english_unavailable_notice_ ? 14U : 7U
	);
}

NewClassicOptionsResult NewClassicOptionsMenu::consume(
	NewClassicUiInput input
) noexcept
{
	if(status_ != NewClassicOptionsStatus::running) {
		return {.status = status_};
	}
	if(input.app_quit_requested) {
		status_ = NewClassicOptionsStatus::application_quit;
		return {.status = status_};
	}
	const bool any = input.up || input.down || input.left || input.right ||
		input.confirm || input.cancel;
	if(!input_allowed_) {
		if(!any) {
			input_allowed_ = true;
		}
		return {.status = status_};
	}
	if(!any) {
		return {.status = status_};
	}
	input_allowed_ = false;
	if(commit_failure_) {
		if(input.confirm) {
			status_ = NewClassicOptionsStatus::apply_requested;
		} else if(input.cancel) {
			commit_failure_ = false;
			input_allowed_ = false;
			draw();
			return {.surface_changed = true, .status = status_};
		}
		return {.status = status_};
	}
	if(input.cancel) {
		status_ = NewClassicOptionsStatus::apply_requested;
		return {.status = status_};
	}
	if(input.up || input.down) {
		constexpr std::size_t row_count = 7U;
		selected_ = input.up ? (selected_ + row_count - 1U) % row_count :
			(selected_ + 1U) % row_count;
		english_unavailable_notice_ = false;
		draw();
		return {.surface_changed = true, .status = status_};
	}
	if(selected_ == 4U && input.confirm) {
		status_ = NewClassicOptionsStatus::key_config_requested;
		return {.status = status_};
	}
	if(selected_ == 6U && input.confirm) {
		status_ = NewClassicOptionsStatus::apply_requested;
		return {.status = status_};
	}
	if(selected_ == 3U && (input.confirm || input.left || input.right)) {
		if(english_available_) {
			config_.language = config_.language == ClassicLanguage::japanese ?
				ClassicLanguage::english : ClassicLanguage::japanese;
			english_unavailable_notice_ = false;
		} else {
			english_unavailable_notice_ = true;
		}
		draw();
		return {.surface_changed = true, .status = status_};
	}
	if(!(input.confirm || input.left || input.right)) {
		return {.status = status_};
	}
	switch(selected_) {
	case 0U: {
		constexpr auto count = 4U;
		const auto rank = static_cast<std::uint8_t>(config_.rank);
		config_.rank = static_cast<ClassicRank>(input.left ?
			(rank + count - 1U) % count : (rank + 1U) % count);
		break;
	}
	case 1U:
		config_.bgm_mode = config_.bgm_mode == ClassicBgmMode::off ?
			ClassicBgmMode::fm : ClassicBgmMode::off;
		break;
	case 2U: config_.sfx_enabled = !config_.sfx_enabled; break;
	case 5U: config_.reduced_flash = !config_.reduced_flash; break;
	default: return {.status = status_};
	}
	english_unavailable_notice_ = false;
	draw();
	return {.surface_changed = true, .status = status_};
}

void NewClassicOptionsMenu::report_commit(bool success) noexcept
{
	if(status_ != NewClassicOptionsStatus::apply_requested) {
		return;
	}
	if(success) {
		status_ = NewClassicOptionsStatus::applied;
		return;
	}
	status_ = NewClassicOptionsStatus::running;
	commit_failure_ = true;
	input_allowed_ = false;
	draw();
}

void NewClassicOptionsMenu::resume_after_key_config() noexcept
{
	if(status_ != NewClassicOptionsStatus::key_config_requested) {
		return;
	}
	status_ = NewClassicOptionsStatus::running;
	input_allowed_ = false;
	draw();
}

present::SourceSurface NewClassicOptionsMenu::surface() const noexcept
{
	return canvas_.surface();
}

const ClassicConfigOptions &NewClassicOptionsMenu::config() const noexcept
{
	return config_;
}

NewClassicOptionsStatus NewClassicOptionsMenu::status() const noexcept
{
	return status_;
}

namespace {

constexpr std::array<std::string_view, new_classic_player_action_count>
	key_player_actions{
		"Up-Left", "Up", "Up-Right", "Left", "Right", "Down-Left",
		"Down", "Down-Right", "Shot", "Bomb", "Charge",
	};
constexpr std::array<std::string_view, new_classic_story_action_count>
	key_story_actions{"Up", "Left", "Right", "Down"};

[[nodiscard]] std::string host_key_label(
	NewClassicHostKeyName resolver,
	void *context,
	std::uint16_t key
)
{
	if(key == new_classic_host_key_unbound) {
		return "Unbound";
	}
	if(resolver) {
		const auto resolved = resolver(context, key);
		if(!resolved.empty()) {
			return std::string(resolved);
		}
	}
	return "Key " + std::to_string(key);
}

} // namespace

NewClassicKeyConfigMenu::NewClassicKeyConfigMenu(
	NewClassicKeyConfig config,
	NewClassicKeyConfig defaults,
	std::uint16_t maximum_key,
	std::uint16_t capture_cancel_key,
	NewClassicHostKeyName key_name,
	void *key_name_context
) noexcept :
	original_(config),
	config_(std::move(config)),
	defaults_(std::move(defaults)),
	maximum_key_(maximum_key),
	capture_cancel_key_(capture_cancel_key),
	key_name_(key_name),
	key_name_context_(key_name_context)
{
	draw();
}

std::size_t NewClassicKeyConfigMenu::row_count() const noexcept
{
	return page_ == Page::story ? 11U : 15U;
}

void NewClassicKeyConfigMenu::set_page(Page page) noexcept
{
	page_ = page;
	selected_ = page_ == Page::story ? 0U : 9U;
}

std::optional<std::size_t>
NewClassicKeyConfigMenu::binding_index() const noexcept
{
	if(page_ == Page::story) {
		if(selected_ < new_classic_story_action_count) {
			return (2U * new_classic_player_action_count) + selected_;
		}
		if(selected_ >= 4U && selected_ <= 6U) {
			return static_cast<std::size_t>(NewClassicPlayerAction::shot) +
				(selected_ - 4U);
		}
		return std::nullopt;
	}
	if(selected_ >= 1U && selected_ <= new_classic_player_action_count) {
		const auto player = page_ == Page::p1 ? 0U : 1U;
		return (player * new_classic_player_action_count) + (selected_ - 1U);
	}
	return std::nullopt;
}

void NewClassicKeyConfigMenu::restore_defaults() noexcept
{
	if(page_ == Page::story) {
		for(std::size_t index = 0U; index < new_classic_story_action_count;
			++index) {
			config_.bindings[(2U * new_classic_player_action_count) + index] =
				defaults_.bindings[(2U * new_classic_player_action_count) + index];
		}
		for(std::size_t action = static_cast<std::size_t>(
			NewClassicPlayerAction::shot
		); action <= static_cast<std::size_t>(NewClassicPlayerAction::charge);
			++action) {
			config_.bindings[action] = defaults_.bindings[action];
		}
		config_.autofire = static_cast<std::uint8_t>(config_.autofire & ~1U);
		return;
	}
	const auto player = page_ == Page::p1 ? 0U : 1U;
	for(std::size_t action = 0U; action < new_classic_player_action_count;
		++action) {
		const auto index = (player * new_classic_player_action_count) + action;
		config_.bindings[index] = defaults_.bindings[index];
	}
	config_.autofire = static_cast<std::uint8_t>(
		config_.autofire & ~(1U << player)
	);
}

void NewClassicKeyConfigMenu::assign(std::uint16_t key) noexcept
{
	if(!capture_binding_ || key == 0U || key >= maximum_key_) {
		return;
	}
	const auto index = *capture_binding_;
	const auto old = config_.bindings[index];
	const auto first = index < 2U * new_classic_player_action_count ?
		0U : 2U * new_classic_player_action_count;
	const auto end = index < 2U * new_classic_player_action_count ?
		2U * new_classic_player_action_count : new_classic_key_binding_count;
	for(std::size_t other = first; other < end; ++other) {
		if(other != index && config_.bindings[other] == key) {
			config_.bindings[other] = old;
			break;
		}
	}
	config_.bindings[index] = key;
}

void NewClassicKeyConfigMenu::draw() noexcept
{
	draw_header(canvas_, phase_ == Phase::discard ? "DISCARD CHANGES?" :
		phase_ == Phase::commit_failure ? "SAVE FAILED" : "KEY CONFIGURATION");
	if(phase_ == Phase::discard) {
		canvas_.draw_text_centered(
			"Z / ENTER: DISCARD", screen_center, 172U, 2U, 14U
		);
		canvas_.draw_text_centered(
			"ESC: KEEP EDITING", screen_center, 218U, 2U, 15U
		);
		return;
	}
	if(phase_ == Phase::commit_failure) {
		canvas_.draw_text_centered(
			"COULD NOT SAVE TH3KEY.CFG", screen_center, 168U, 2U, 14U
		);
		canvas_.draw_text_centered(
			"Z: RETRY   ESC: RETURN", screen_center, 218U, 2U, 15U
		);
		return;
	}
	const auto title = page_ == Page::p1 ? "[ P1 ]" :
		page_ == Page::p2 ? "[ P2 ]" : "[ STORY ]";
	canvas_.draw_text_centered(title, screen_center, 82U, 2U, 14U);
	const auto put = [&](std::size_t row, std::string label, std::string value) {
		const auto y = 108U + (row * 18U);
		const bool selected = row == selected_;
		if(selected) {
			canvas_.fill_rect(72U, y - 1U, 496U, 17U, 3U);
		}
		canvas_.draw_text(selected ? "> " + label : "  " + label,
			88U, y, 1U, selected ? 14U : 15U);
		if(!value.empty()) {
			canvas_.draw_text(value, 376U, y, 1U, selected ? 14U : 7U);
		}
	};
	if(page_ == Page::story) {
		for(std::size_t row = 0U; row < key_story_actions.size(); ++row) {
			put(row, std::string(key_story_actions[row]), host_key_label(
				key_name_, key_name_context_,
				config_.bindings[(2U * new_classic_player_action_count) + row]
			));
		}
		for(std::size_t row = 4U; row <= 6U; ++row) {
			const auto action = static_cast<std::size_t>(
				NewClassicPlayerAction::shot
			) + (row - 4U);
			put(row, std::string(key_player_actions[action]), host_key_label(
				key_name_, key_name_context_, config_.bindings[action]
			));
		}
		put(7U, "Autofire", (config_.autofire & 1U) ? "On" : "Off");
		put(8U, "Defaults for Story", {});
		put(9U, "Apply and return", {});
		put(10U, "Cancel", {});
	} else {
		const auto player = page_ == Page::p1 ? 0U : 1U;
		put(0U, "Autofire", (config_.autofire & (1U << player)) ? "On" : "Off");
		for(std::size_t action = 0U; action < key_player_actions.size(); ++action) {
			put(action + 1U, std::string(key_player_actions[action]), host_key_label(
				key_name_, key_name_context_, config_.bindings[
					(player * new_classic_player_action_count) + action
				]
			));
		}
		put(12U, "Defaults for this player", {});
		put(13U, "Apply and return", {});
		put(14U, "Cancel", {});
	}
	const bool capture = phase_ == Phase::capture_wait_release ||
		phase_ == Phase::capture_wait_press ||
		phase_ == Phase::capture_wait_final_release;
	canvas_.draw_text_centered(
		capture ? "Release, then press a key. Esc cancels." :
			"Left/Right: Page   Z: Edit   Esc: Cancel",
		screen_center, 382U, 1U, capture ? 14U : 7U
	);
}

NewClassicKeyConfigResult NewClassicKeyConfigMenu::consume(
	NewClassicKeyConfigInput input
) noexcept
{
	if(status_ != NewClassicKeyConfigStatus::running) {
		return {.status = status_};
	}
	if(input.ui.app_quit_requested) {
		status_ = NewClassicKeyConfigStatus::application_quit;
		return {.status = status_};
	}
	if(phase_ == Phase::capture_wait_release) {
		if(!input.any_scancode_held) {
			phase_ = Phase::capture_wait_press;
		}
		return {.status = status_};
	}
	if(phase_ == Phase::capture_wait_press) {
		if(!input.scancode_pressed) {
			return {.status = status_};
		}
		if(*input.scancode_pressed != capture_cancel_key_) {
			assign(*input.scancode_pressed);
		}
		phase_ = Phase::capture_wait_final_release;
		draw();
		return {.surface_changed = true, .status = status_};
	}
	if(phase_ == Phase::capture_wait_final_release) {
		if(!input.any_scancode_held) {
			phase_ = Phase::menu;
			capture_binding_.reset();
			input_allowed_ = false;
			draw();
			return {.surface_changed = true, .status = status_};
		}
		return {.status = status_};
	}
	if(phase_ == Phase::discard) {
		if(input.ui.confirm) {
			config_ = original_;
			status_ = NewClassicKeyConfigStatus::back;
		} else if(input.ui.cancel) {
			phase_ = Phase::menu;
			input_allowed_ = false;
			draw();
			return {.surface_changed = true, .status = status_};
		}
		return {.status = status_};
	}
	if(phase_ == Phase::commit_failure) {
		if(input.ui.confirm) {
			status_ = NewClassicKeyConfigStatus::apply_requested;
		} else if(input.ui.cancel) {
			phase_ = Phase::menu;
			input_allowed_ = false;
			draw();
			return {.surface_changed = true, .status = status_};
		}
		return {.status = status_};
	}
	const bool any = input.ui.up || input.ui.down || input.ui.left ||
		input.ui.right || input.ui.confirm || input.ui.cancel;
	if(!input_allowed_) {
		if(!any) {
			input_allowed_ = true;
		}
		return {.status = status_};
	}
	if(!any) {
		return {.status = status_};
	}
	input_allowed_ = false;
	if(input.ui.cancel) {
		if(config_ == original_) {
			status_ = NewClassicKeyConfigStatus::back;
		} else {
			phase_ = Phase::discard;
			draw();
			return {.surface_changed = true, .status = status_};
		}
		return {.status = status_};
	}
	if(input.ui.left || input.ui.right) {
		const auto current = static_cast<std::size_t>(page_);
		set_page(static_cast<Page>(input.ui.left ? (current + 2U) % 3U :
			(current + 1U) % 3U));
		draw();
		return {.surface_changed = true, .status = status_};
	}
	if(input.ui.up || input.ui.down) {
		const auto count = row_count();
		selected_ = input.ui.up ? (selected_ + count - 1U) % count :
			(selected_ + 1U) % count;
		draw();
		return {.surface_changed = true, .status = status_};
	}
	if(!input.ui.confirm) {
		return {.status = status_};
	}
	if(const auto binding = binding_index()) {
		capture_binding_ = binding;
		phase_ = Phase::capture_wait_release;
		draw();
		return {.surface_changed = true, .status = status_};
	}
	const auto autofire_row = page_ == Page::story ? 7U : 0U;
	const auto defaults_row = page_ == Page::story ? 8U : 12U;
	const auto apply_row = page_ == Page::story ? 9U : 13U;
	if(selected_ == autofire_row) {
		const auto player = page_ == Page::p2 ? 1U : 0U;
		config_.autofire = static_cast<std::uint8_t>(
			config_.autofire ^ (1U << player)
		);
		draw();
		return {.surface_changed = true, .status = status_};
	}
	if(selected_ == defaults_row) {
		restore_defaults();
		draw();
		return {.surface_changed = true, .status = status_};
	}
	if(selected_ == apply_row) {
		status_ = NewClassicKeyConfigStatus::apply_requested;
		return {.status = status_};
	}
	if(config_ == original_) {
		status_ = NewClassicKeyConfigStatus::back;
	} else {
		phase_ = Phase::discard;
		draw();
		return {.surface_changed = true, .status = status_};
	}
	return {.status = status_};
}

void NewClassicKeyConfigMenu::report_commit(bool success) noexcept
{
	if(status_ != NewClassicKeyConfigStatus::apply_requested) {
		return;
	}
	if(success) {
		original_ = config_;
		status_ = NewClassicKeyConfigStatus::applied;
		return;
	}
	status_ = NewClassicKeyConfigStatus::running;
	phase_ = Phase::commit_failure;
	input_allowed_ = false;
	draw();
}

present::SourceSurface NewClassicKeyConfigMenu::surface() const noexcept
{
	return canvas_.surface();
}

const NewClassicKeyConfig &NewClassicKeyConfigMenu::config() const noexcept
{
	return config_;
}

NewClassicKeyConfigStatus NewClassicKeyConfigMenu::status() const noexcept
{
	return status_;
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
			result.slots[slot] = catalog_slot(*loaded->document);
		}
	}
	return result;
}

std::expected<std::unique_ptr<NewClassicReplayBrowser>, ReplaySlotStoreError>
NewClassicReplayBrowser::load(std::filesystem::path replay_root)
{
	auto catalog = scan_new_classic_replays(replay_root);
	if(!catalog) {
		return std::unexpected(std::move(catalog.error()));
	}
	return std::unique_ptr<NewClassicReplayBrowser>(
		new NewClassicReplayBrowser(std::move(replay_root), std::move(*catalog))
	);
}

NewClassicReplayBrowser::NewClassicReplayBrowser(
	std::filesystem::path replay_root,
	NewClassicReplayCatalog catalog
) : replay_root_(std::move(replay_root)), catalog_(std::move(catalog))
{
	draw();
}

void NewClassicReplayBrowser::draw() noexcept
{
	draw_header(canvas_, detail_document_ ? "REPLAY DETAILS" : "REPLAY BROWSER");
	if(detail_document_) {
		const auto &document = *detail_document_;
		const auto &entry = catalog_.slots[selected_slot_];
		const auto *metadata = document.metadata ? &*document.metadata : nullptr;
		canvas_.draw_text(
			"SLOT " + slot_number(selected_slot_) + "  " + fixed_name(entry.name) +
				"  " + mode_label(entry.game_mode, document.practice),
			52U, 88U, 1U, 14U
		);
		canvas_.draw_text(
			"RESULT " + std::string(metadata ? end_reason_label(metadata->end_reason) :
				std::string_view{"UNKNOWN"}) + "  SCORE " +
				decimal_score(entry.final_score) + "  DATE " +
				(metadata ? dos_date_label(metadata->dos_date) : std::string{"-"}),
			52U, 106U, 1U, 15U
		);
		if(metadata) {
			canvas_.draw_text(
				"RANK " + std::string(rank_label(metadata->rank)) + "  P1 " +
					std::string(character_label(metadata->paletted_character[0U])) +
					"  P2 " +
					std::string(character_label(metadata->paletted_character[1U])) +
					"  AF " + ((metadata->autofire & 1U) ? "ON" : "OFF") +
					"/" + ((metadata->autofire & 2U) ? "ON" : "OFF"),
				52U, 124U, 1U, 15U
			);
		}
		std::string context = "SLOWDOWN " + slowdown_label(document) +
			"  CHECKPOINTS " + std::to_string(document.checkpoints.size());
		if(document.practice && metadata && metadata->practice) {
			const auto &practice = *metadata->practice;
			context += "  START S" + std::to_string(practice.stage + 1U) +
				" R" + std::to_string(practice.round + 1U) +
				" STOCK " + std::to_string(practice.stock);
		}
		canvas_.draw_text(context, 52U, 142U, 1U, 15U);
		draw_detail_tabs(canvas_, detail_page_);

		if(detail_page_ == NewClassicReplayDetailPage::clear_bonuses) {
			if(!document.stage_clear_bonuses) {
				draw_compact_row(
					canvas_, "NO CLEAR-BONUS TELEMETRY.", 188U, false
				);
			} else if(entry.game_mode != game_mode::story && !document.practice) {
				draw_compact_row(
					canvas_, "NO STAGE CLEAR BONUSES IN VS MODE.", 188U, false
				);
			} else {
				const auto first_stage = document.practice && metadata &&
					metadata->practice ? metadata->practice->stage : 0U;
				const auto last_stage = document.practice ?
					std::min<std::size_t>(first_stage + 1U, 9U) : 9U;
				for(std::size_t stage = first_stage; stage < last_stage; ++stage) {
					const auto &bonus = (*document.stage_clear_bonuses)[stage];
					std::string label = "S" + std::to_string(stage + 1U) + "  ";
					if(metadata && metadata->story &&
						stage < metadata->stage_reached_count) {
						label += std::string(character_label(static_cast<std::int8_t>(
							metadata->story->stage_opponents[stage]
						))) + "  ";
					}
					if(!clear_bonus_valid(bonus)) {
						label += "-";
					} else {
						label += decimal_score(bonus.total) +
							"  MAX " + std::to_string(bonus.max_combo) +
							"  GA " + std::to_string(bonus.gauge_attacks) +
							"  BA " + std::to_string(bonus.boss_attacks) +
							"  RV " + std::to_string(bonus.boss_reversals) +
							"  PN " + std::to_string(bonus.boss_panics) +
							"  LIFE " + std::to_string(bonus.remaining_lives);
					}
					draw_compact_row(
						canvas_, label, 188U + ((stage - first_stage) * 19U), false
					);
				}
			}
			canvas_.draw_text_centered(
				"LEFT RIGHT TAB  ESC BACK", screen_center, 378U, 1U, 6U
			);
			return;
		}

		const auto first = selected_checkpoint_ >= 10U ?
			selected_checkpoint_ - 9U : 0U;
		const auto last = std::min(first + 10U, document.checkpoints.size());
		for(std::size_t index = first; index < last; ++index) {
			const auto &checkpoint = document.checkpoints[index];
			const auto *split = round_split(document, checkpoint);
			const auto stage = checkpoint.stage == 0x0FU ?
				std::string{"VS"} : "S" + std::to_string(checkpoint.stage + 1U);
			std::string label = "CP " + std::to_string(index + 1U) + "  " +
				stage + " R" + std::to_string(checkpoint.round + 1U);
			if(detail_page_ == NewClassicReplayDetailPage::timers) {
				label += "  TIME " + (split && split->real_frames ?
					new_classic_replay_timer_label(*split->real_frames) :
					std::string{"-"});
			} else if(split) {
				label += "  P1 " + decimal_score(split->p1_score) +
					"  P2 " + decimal_score(split->p2_score) +
					"  WIN " + winner_label(split->winner);
			} else {
				label += "  SPLIT -";
			}
			draw_compact_row(
				canvas_, label, 188U + ((index - first) * 19U),
				index == selected_checkpoint_
			);
		}
		if(document.checkpoints.empty()) {
			draw_compact_row(canvas_, "NO PLAYABLE CHECKPOINTS.", 188U, false);
		}
		canvas_.draw_text_centered(
			"UP DOWN CHECKPOINT  Z PLAY  LEFT RIGHT TAB  ESC BACK",
			screen_center, 378U, 1U, 6U
		);
		return;
	}

	const auto page = static_cast<std::size_t>(selected_slot_) / browser_rows;
	const auto first = page * browser_rows;
	const auto last = std::min(first + browser_rows, replay_slot_count);
	for(std::size_t slot = first; slot < last; ++slot) {
		const auto &entry = catalog_.slots[slot];
		std::string label = "SLOT " + slot_number(slot) + "  ";
		if(entry.state == ReplayCatalogSlotState::empty) {
			label += "EMPTY";
		} else if(entry.state == ReplayCatalogSlotState::invalid) {
			label += "INVALID";
		} else {
			label += fixed_name(entry.name) + "  " +
				mode_label(entry.game_mode, false) + "  " +
				decimal_score(entry.final_score);
		}
		const auto y = 86U + ((slot - first) * 19U);
		canvas_.fill_rect(
			48U, y, 544U, 17U,
			slot == selected_slot_ ? 4U : 1U
		);
		canvas_.draw_text(
			label, 58U, y + 3U, 1U,
			slot == selected_slot_ ? 14U : 15U
		);
	}
	canvas_.draw_text_centered(
		"UP DOWN SELECT  LEFT RIGHT PAGE  ESC BACK",
		screen_center, 378U, 1U, 6U
	);
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
	if(input.cancel) {
		if(detail_document_) {
			detail_document_.reset();
			selected_checkpoint_ = 0U;
			detail_page_ = NewClassicReplayDetailPage::splits;
			draw();
			return NewClassicReplayBrowserResult{
				.surface_changed = true, .status = status_
			};
		}
		status_ = NewClassicReplayBrowserStatus::back;
		return NewClassicReplayBrowserResult{.status = status_};
	}
	if(detail_document_) {
		const auto count = detail_document_->checkpoints.size();
		if(input.left || input.right) {
			const auto old = static_cast<std::size_t>(detail_page_);
			const auto next = input.left ? (old == 0U ? 0U : old - 1U) :
				std::min(old + 1U, static_cast<std::size_t>(
					NewClassicReplayDetailPage::timers
				));
			if(next != old) {
				detail_page_ = static_cast<NewClassicReplayDetailPage>(next);
				draw();
				return NewClassicReplayBrowserResult{
					.surface_changed = true, .status = status_
				};
			}
			return NewClassicReplayBrowserResult{.status = status_};
		}
		if((input.up || input.down) && count != 0U &&
			detail_page_ != NewClassicReplayDetailPage::clear_bonuses) {
			selected_checkpoint_ = input.up ?
				(selected_checkpoint_ + count - 1U) % count :
				(selected_checkpoint_ + 1U) % count;
			draw();
			return NewClassicReplayBrowserResult{
				.surface_changed = true, .status = status_
			};
		}
		if(input.confirm && count != 0U &&
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
			(selected_slot_ + replay_slot_count - browser_rows) % replay_slot_count
		);
	} else if(input.right) {
		selected_slot_ = static_cast<std::uint8_t>(
			(selected_slot_ + browser_rows) % replay_slot_count
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
		return NewClassicReplayBrowserResult{.status = status_};
	}
	draw();
	return NewClassicReplayBrowserResult{
		.surface_changed = true, .status = status_
	};
}

present::SourceSurface NewClassicReplayBrowser::surface() const noexcept
{
	return canvas_.surface();
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

std::optional<formats::ReplayDocument>
NewClassicReplayBrowser::take_selected_document()
{
	if(status_ != NewClassicReplayBrowserStatus::play_selected) {
		return std::nullopt;
	}
	return std::move(detail_document_);
}

NewClassicReplaySaveMenu::NewClassicReplaySaveMenu(
	formats::ReplayDocument document
) : document_(std::move(document)), keyboard_position_(keyboard_end)
{
	NewClassicReplayCatalog empty{};
	draw(empty);
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
	if(phase_ == Phase::prompt) {
		draw_header(canvas_, "RECORDING COMPLETE");
		canvas_.draw_text_centered("SAVE REPLAY?", screen_center, 150U, 3U, 15U);
		draw_row(canvas_, "YES", 230U, yes_);
		draw_row(canvas_, "NO", 278U, !yes_);
		return;
	}
	if(phase_ == Phase::name) {
		draw_header(canvas_, "REPLAY NAME");
		std::string shown(name_.begin(), name_.end());
		canvas_.draw_text_centered(shown, screen_center, 100U, 3U, 15U);
		canvas_.fill_rect(104U + (name_position_ * 54U), 126U, 42U, 4U, 14U);
		for(std::size_t index = 0U; index < keyboard_symbols.size(); ++index) {
			const auto x = 48U + ((index % keyboard_columns) * 43U);
			const auto y = 180U + ((index / keyboard_columns) * 48U);
			if(index == keyboard_position_) {
				canvas_.fill_rect(x - 8U, y - 7U, 34U, 34U, 4U);
			}
			const char value = keyboard_symbols[index];
			const std::string label = index == keyboard_back ? "BS" :
				(index == keyboard_end ? "END" : std::string(1U, value));
			canvas_.draw_text(
				label, x, y, 2U, index == keyboard_position_ ? 14U : 15U
			);
		}
		canvas_.draw_text_centered("ESC BACK", screen_center, 370U, 1U, 6U);
		return;
	}
	if(phase_ == Phase::overwrite) {
		draw_header(canvas_, "REPLACE EXISTING REPLAY?");
		canvas_.draw_text_centered(
			"SLOT " + slot_number(selected_slot_), screen_center, 150U, 3U, 15U
		);
		draw_row(canvas_, "YES", 230U, yes_);
		draw_row(canvas_, "NO", 278U, !yes_);
		return;
	}
	if(phase_ == Phase::failure) {
		draw_header(canvas_, "SAVE FAILED");
		canvas_.draw_text_centered("REPLAY WAS NOT LOST", screen_center, 170U, 2U, 15U);
		canvas_.draw_text_centered("Z RETRY SLOT LIST", screen_center, 230U, 2U, 14U);
		return;
	}

	draw_header(canvas_, "SAVE REPLAY");
	const auto page = static_cast<std::size_t>(selected_slot_) / browser_rows;
	const auto first = page * browser_rows;
	const auto last = std::min(first + browser_rows, replay_slot_count);
	for(std::size_t slot = first; slot < last; ++slot) {
		std::string label = "SLOT " + slot_number(slot) + "  ";
		const auto &entry = catalog.slots[slot];
		label += entry.state == ReplayCatalogSlotState::empty ? "EMPTY" :
			(entry.state == ReplayCatalogSlotState::invalid ? "INVALID" :
			 fixed_name(entry.name));
		const auto y = 86U + ((slot - first) * 19U);
		canvas_.fill_rect(72U, y, 496U, 17U,
			slot == selected_slot_ ? 4U : 1U);
		canvas_.draw_text(label, 82U, y + 3U, 1U,
			slot == selected_slot_ ? 14U : 15U);
	}
	canvas_.draw_text_centered(
		"Z SAVE  LEFT RIGHT PAGE  ESC BACK", screen_center, 378U, 1U, 6U
	);
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
		return {.status = status_};
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
	if(phase_ == Phase::failure) {
		if(input.confirm || input.cancel) {
			phase_ = Phase::slots;
			draw(catalog);
			return {.surface_changed = true, .status = status_};
		}
		return {.status = status_};
	}
	if(phase_ == Phase::prompt || phase_ == Phase::overwrite) {
		if(input.cancel) {
			if(phase_ == Phase::prompt) {
				status_ = NewClassicReplaySaveStatus::discarded;
			} else {
				phase_ = Phase::slots;
				draw(catalog);
				return {.surface_changed = true, .status = status_};
			}
			return {.status = status_};
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
				} else {
					phase_ = Phase::name;
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
			if(name_position_ != 0U) {
				--name_position_;
				name_[name_position_] = ' ';
			} else {
				phase_ = Phase::prompt;
			}
			draw(catalog);
			return {.surface_changed = true, .status = status_};
		}
		const auto count = keyboard_symbols.size();
		if(input.left) {
			keyboard_position_ = (keyboard_position_ + count - 1U) % count;
		} else if(input.right) {
			keyboard_position_ = (keyboard_position_ + 1U) % count;
		} else if(input.up) {
			keyboard_position_ =
				(keyboard_position_ + count - keyboard_columns) % count;
		} else if(input.down) {
			keyboard_position_ =
				(keyboard_position_ + keyboard_columns) % count;
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
				name_[name_position_] = keyboard_symbols[keyboard_position_];
				name_position_ = std::min(name_position_ + 1U, name_.size() - 1U);
			}
		}
		draw(catalog);
		return {.surface_changed = true, .status = status_};
	}
	if(input.cancel) {
		phase_ = Phase::name;
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
			(selected_slot_ + replay_slot_count - browser_rows) % replay_slot_count);
	} else if(input.right) {
		selected_slot_ = static_cast<std::uint8_t>(
			(selected_slot_ + browser_rows) % replay_slot_count);
	} else if(input.confirm) {
		if(catalog.slots[selected_slot_].state == ReplayCatalogSlotState::ready) {
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
	draw(catalog);
	return {.surface_changed = true, .status = status_};
}

void NewClassicReplaySaveMenu::report_commit(bool success) noexcept
{
	if(status_ != NewClassicReplaySaveStatus::save_requested) {
		return;
	}
	if(success) {
		status_ = NewClassicReplaySaveStatus::saved;
		return;
	}
	status_ = NewClassicReplaySaveStatus::running;
	phase_ = Phase::failure;
	NewClassicReplayCatalog empty{};
	draw(empty);
}

present::SourceSurface NewClassicReplaySaveMenu::surface() const noexcept
{
	return canvas_.surface();
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
	return phase_ == Phase::failure;
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
}

} // namespace th03::app
