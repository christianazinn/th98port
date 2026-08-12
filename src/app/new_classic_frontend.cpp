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
	for(const auto value : packed) {
		result.push_back(static_cast<char>('0' + ((value / 10U) % 10U)));
		result.push_back(static_cast<char>('0' + (value % 10U)));
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
		canvas_.draw_text(
			"SLOT " + slot_number(selected_slot_), 72U, 96U, 2U, 14U
		);
		canvas_.draw_text(fixed_name(entry.name), 270U, 96U, 2U, 15U);
		canvas_.draw_text(
			mode_label(entry.game_mode, document.practice), 72U, 124U, 2U, 15U
		);
		canvas_.draw_text(
			"SCORE " + decimal_score(entry.final_score), 270U, 124U, 2U, 15U
		);
		canvas_.draw_text("SELECT CHECKPOINT", 72U, 160U, 1U, 14U);
		const auto first = selected_checkpoint_ >= 10U ?
			selected_checkpoint_ - 9U : 0U;
		const auto last = std::min(first + 10U, document.checkpoints.size());
		for(std::size_t index = first; index < last; ++index) {
			const auto &checkpoint = document.checkpoints[index];
			std::string label = "CHECKPOINT " + std::to_string(index + 1U) +
				"  STAGE " + std::to_string(
					checkpoint.stage == 0x0FU ? 0U : checkpoint.stage + 1U
				) + "  ROUND " + std::to_string(checkpoint.round + 1U);
			draw_row(
				canvas_, label, 180U + ((index - first) * 20U),
				index == selected_checkpoint_
			);
		}
		canvas_.draw_text_centered(
			"Z PLAY  ESC BACK", screen_center, 378U, 1U, 6U
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
		if((input.up || input.down) && count != 0U) {
			selected_checkpoint_ = input.up ?
				(selected_checkpoint_ + count - 1U) % count :
				(selected_checkpoint_ + 1U) % count;
			draw();
			return NewClassicReplayBrowserResult{
				.surface_changed = true, .status = status_
			};
		}
		if(input.confirm && count != 0U) {
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
