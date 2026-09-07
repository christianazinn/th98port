#include "th03/app/new_classic/online/new_classic_online_profile_scene.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string_view>
#include <utility>

#include "th03/app/new_classic/netplay/new_classic_nametag.hpp"
#include "th03/app/new_classic/content/new_classic_unicode.hpp"

namespace th03::app {
namespace {

constexpr std::int32_t center_x = 320;
constexpr std::int32_t title_top = 64;
constexpr present::Index4 title_color = 9U;
constexpr present::Index4 selected_color = 12U;
constexpr present::Index4 normal_color = 15U;
constexpr present::Index4 detail_color = 13U;
constexpr present::Index4 error_color = 11U;
constexpr std::uint8_t fade_boundary_count = 19U;
constexpr std::uint8_t cursor_blink_interval = 28U;

[[nodiscard]] bool raw_any(NewClassicUiInput input) noexcept
{
	return input.up || input.down || input.left || input.right ||
		input.confirm || input.bomb || input.cancel;
}

[[nodiscard]] bool link_code_character(char character) noexcept
{
	return (character >= '2' && character <= '9') ||
		(character >= 'A' && character <= 'H') ||
		(character >= 'J' && character <= 'N') ||
		(character >= 'P' && character <= 'Z');
}

[[nodiscard]] std::size_t previous_utf8_offset(
	std::string_view text, std::size_t offset
) noexcept
{
	offset = std::min(offset, text.size());
	if(offset == 0U) {
		return 0U;
	}
	--offset;
	while(offset != 0U &&
		(static_cast<std::uint8_t>(text[offset]) & 0xC0U) == 0x80U) {
		--offset;
	}
	return offset;
}

[[nodiscard]] std::size_t next_utf8_offset(
	std::string_view text, std::size_t offset
) noexcept
{
	const auto scalar = next_new_classic_utf8_scalar(text, offset);
	return scalar ? offset + scalar->bytes : text.size();
}

[[nodiscard]] std::size_t utf8_cells(
	std::string_view text, std::size_t end = std::string_view::npos
) noexcept
{
	end = std::min(end, text.size());
	std::size_t cells = 0U;
	for(std::size_t offset = 0U; offset < end;) {
		const auto scalar = next_new_classic_utf8_scalar(text, offset);
		if(!scalar || offset + scalar->bytes > end) {
			break;
		}
		cells += new_classic_unicode_cell_width(scalar->value);
		offset += scalar->bytes;
	}
	return cells;
}

[[nodiscard]] std::size_t utf8_offset_for_cells(
	std::string_view text, std::size_t wanted
) noexcept
{
	std::size_t cells = 0U;
	std::size_t offset = 0U;
	while(offset < text.size()) {
		const auto scalar = next_new_classic_utf8_scalar(text, offset);
		if(!scalar || cells + new_classic_unicode_cell_width(scalar->value) >
			wanted) {
			break;
		}
		cells += new_classic_unicode_cell_width(scalar->value);
		offset += scalar->bytes;
	}
	return offset;
}

[[nodiscard]] bool put_editor_line(
	const NewClassicMenuFont &font,
	present::Raster &raster,
	std::int32_t center,
	std::int32_t top,
	std::string_view text,
	std::size_t cursor,
	bool cursor_visible,
	present::Index4 color
) noexcept
{
	if(!font.put_centered(raster, center, top, text, color)) {
		return false;
	}
	if(!cursor_visible) {
		return true;
	}
	const auto bounded_cursor = std::min(cursor, text.size());
	const auto left = center - (font.width(text) / 2);
	return font.put(
		raster, left + font.width_n(text, bounded_cursor), top, "_", color
	);
}

} // namespace

NewClassicOnlineProfileScene::NewClassicOnlineProfileScene(
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	NewClassicLanguageAssetProfile language,
	std::optional<std::string> nametag
) :
	patch_assets_(std::move(patch_assets)),
	language_(language),
	nametag_(std::move(nametag))
{
	if((nametag_ && !layout_new_classic_nametag(*nametag_)) ||
		!patch_assets_ || !patch_surface_.reset(
			patch_assets_->menu_background(NewClassicMenuBackground::netplay)
		)) {
		return;
	}
	if(!nametag_) {
		screen_ = Screen::name_entry;
	}
	valid_ = draw();
	if(valid_) {
		phase_ = Phase::menu;
		patch_surface_.raster().set_palette_tone(100);
		patch_surface_.raster().apply_pending_palette();
		require_release();
	}
}

bool NewClassicOnlineProfileScene::draw() noexcept
{
	if(!patch_assets_ || !patch_surface_.begin_hidden_frame(
		patch_assets_->menu_background(NewClassicMenuBackground::netplay)
	)) {
		return false;
	}
	const auto &font = patch_assets_->menu_font;
	const auto &messages = patch_assets_->messages(language_);
	auto &raster = patch_surface_.raster();
	if(!font.put_centered(raster, center_x, title_top,
		messages.text(NewClassicMessageId::profile_title), title_color)) {
		return false;
	}

	if(screen_ == Screen::menu) {
		if(!font.put_centered(raster, center_x, 112,
			messages.text(NewClassicMessageId::profile_current), normal_color)) {
			return false;
		}
		const auto layout = nametag_ ? layout_new_classic_nametag(*nametag_) :
			std::expected<NewClassicNametagLayout, NewClassicNametagError>{
				std::unexpected(NewClassicNametagError::empty)
			};
		if(!layout || !font.put_centered(
			raster, center_x, 136, layout->lines[0U], detail_color
		) || (layout->line_count == 2U && !font.put_centered(
			raster, center_x, 160, layout->lines[1U], detail_color
		))) {
			return false;
		}
		constexpr std::array rows{
			NewClassicMessageId::profile_change_name,
			NewClassicMessageId::profile_link_another,
			NewClassicMessageId::profile_link_this,
			NewClassicMessageId::profile_return,
		};
		for(std::size_t row = 0U; row < rows.size(); ++row) {
			if(!font.put_centered(
				raster, center_x, 208 + static_cast<std::int32_t>(row) * 32,
				messages.text(rows[row]), row == selection_ ?
					selected_color : normal_color
			)) {
				return false;
			}
		}
	} else if(screen_ == Screen::name_entry) {
		if(!font.put_centered(raster, center_x, 112,
			messages.text(NewClassicMessageId::profile_enter_name), normal_color)) {
			return false;
		}
		const auto entry_color = invalid_entry_ ? error_color : selected_color;
		if(!put_editor_line(
			font, raster, center_x, 160, name_entry_lines_[0U],
			name_entry_column_, cursor_visible_ && name_entry_row_ == 0U,
			entry_color
		) || !put_editor_line(
			font, raster, center_x, 184, name_entry_lines_[1U],
			name_entry_column_, cursor_visible_ && name_entry_row_ == 1U,
			entry_color
		) || !font.put_centered(raster, center_x, 240,
			messages.text(invalid_entry_ ?
				NewClassicMessageId::profile_name_invalid :
				NewClassicMessageId::profile_name_hint),
			invalid_entry_ ? error_color : title_color)) {
			return false;
		}
	} else if(screen_ == Screen::link_entry) {
		const auto shown = entry_.empty() ? std::string_view{"----------"} :
			std::string_view{entry_};
		if(!font.put_centered(raster, center_x, 128,
			messages.text(NewClassicMessageId::profile_link_this), normal_color) ||
			!font.put_centered(raster, center_x, 176, shown,
				invalid_entry_ ? error_color : selected_color) ||
			!font.put_centered(raster, center_x, 240,
				messages.text(NewClassicMessageId::netplay_room_footer_edit),
				title_color)) {
			return false;
		}
	} else if(screen_ == Screen::link_code) {
		if(!font.put_centered(raster, center_x, 128,
			messages.text(NewClassicMessageId::profile_link_code), normal_color) ||
			!font.put_centered(raster, center_x, 176, link_code_, selected_color) ||
			!font.put_centered(raster, center_x, 224,
				messages.text(NewClassicMessageId::profile_link_code_hint),
				title_color)) {
			return false;
		}
	} else {
		const auto text = screen_ == Screen::linked ?
			NewClassicMessageId::profile_linked :
			NewClassicMessageId::profile_failed;
		if(!font.put_centered(raster, center_x, 160, messages.text(text),
			screen_ == Screen::linked ? detail_color : error_color) ||
			!font.put_centered(raster, center_x, 240,
				messages.text(NewClassicMessageId::netplay_report_footer),
				title_color)) {
			return false;
		}
	}
	return patch_surface_.publish(raster.access_page());
}

void NewClassicOnlineProfileScene::begin_name_entry() noexcept
{
	name_entry_lines_ = {};
	if(nametag_) {
		if(const auto layout = layout_new_classic_nametag(*nametag_); layout) {
			name_entry_lines_ = layout->lines;
			name_entry_row_ = layout->line_count > 1U ? 1U : 0U;
			name_entry_column_ = name_entry_lines_[name_entry_row_].size();
		}
	} else {
		name_entry_row_ = 0U;
		name_entry_column_ = 0U;
	}
	reset_cursor_blink();
}

std::string NewClassicOnlineProfileScene::name_entry_text() const
{
	std::string result = name_entry_lines_[0U];
	if(!name_entry_lines_[1U].empty()) {
		if(!result.empty()) {
			result.push_back(' ');
		}
		result += name_entry_lines_[1U];
	}
	return result;
}

void NewClassicOnlineProfileScene::reset_cursor_blink() noexcept
{
	cursor_blink_ticks_ = 0U;
	cursor_visible_ = true;
}

void NewClassicOnlineProfileScene::require_release() noexcept
{
	release_required_ = true;
}

void NewClassicOnlineProfileScene::begin_fade_out(
	NewClassicOnlineProfileAction action
) noexcept
{
	action_ = std::move(action);
}

bool NewClassicOnlineProfileScene::consume_fade() noexcept
{
	const bool black_in = phase_ == Phase::fade_in;
	auto &raster = patch_surface_.raster();
	bool changed = false;
	if(fade_boundary_ != 0U && fade_boundary_ <= 17U) {
		const auto pair = static_cast<std::int16_t>(fade_boundary_ - 1U);
		raster.set_palette_tone(static_cast<std::int16_t>(
			black_in ? (pair * 6) : (100 - (pair * 6))
		));
		raster.apply_pending_palette();
		changed = true;
	}
	++fade_boundary_;
	if(fade_boundary_ < fade_boundary_count) {
		return changed;
	}
	raster.set_palette_tone(black_in ? 100 : 0);
	raster.apply_pending_palette();
	fade_boundary_ = 0U;
	if(black_in) {
		phase_ = Phase::menu;
		require_release();
	} else {
		action_ = std::move(fade_out_action_);
	}
	return true;
}

bool NewClassicOnlineProfileScene::consume(
	NewClassicOnlineProfileInput input
) noexcept
{
	if(!valid_ || action_ || request_pending_) {
		return false;
	}
	if(input.ui.app_quit_requested) {
		action_ = NewClassicOnlineProfileAction{
			NewClassicOnlineProfileActionType::application_quit, {}
		};
		return false;
	}
	if(phase_ != Phase::menu) {
		return consume_fade();
	}
	const bool any = raw_any(input.ui) || input.text_character ||
		!input.text_utf8.empty() || input.erase ||
		input.delete_forward || input.home || input.end;
	if(release_required_) {
		if(!any) {
			release_required_ = false;
		}
		return false;
	}
	if(screen_ == Screen::menu) {
		if(input.ui.up || input.ui.down) {
			selection_ = static_cast<std::uint8_t>((selection_ +
				(input.ui.up ? 3U : 1U)) % 4U);
			require_release();
			return draw();
		}
		if(input.ui.cancel || (input.ui.confirm && selection_ == 3U)) {
			begin_fade_out(NewClassicOnlineProfileAction{
				NewClassicOnlineProfileActionType::back, {}
			});
			return false;
		}
		if(input.ui.confirm) {
			if(selection_ == 0U) {
				screen_ = Screen::name_entry;
				begin_name_entry();
			} else if(selection_ == 1U) {
				action_ = NewClassicOnlineProfileAction{
					NewClassicOnlineProfileActionType::create_link_code, {}
				};
				request_pending_ = true;
			} else {
				screen_ = Screen::link_entry;
				entry_.clear();
				entry_cursor_ = 0U;
				reset_cursor_blink();
			}
			invalid_entry_ = false;
			require_release();
			return action_ ? false : draw();
		}
		return false;
	}

	if(screen_ == Screen::name_entry || screen_ == Screen::link_entry) {
		bool changed = false;
		std::string typed = std::move(input.text_utf8);
		if(input.text_character) {
			typed.push_back(*input.text_character);
		}
		if(screen_ == Screen::name_entry) {
			auto &line = name_entry_lines_[name_entry_row_];
			if(input.erase) {
				if(name_entry_column_ != 0U) {
					const auto previous = previous_utf8_offset(
						line, name_entry_column_
					);
					line.erase(previous, name_entry_column_ - previous);
					name_entry_column_ = previous;
					changed = true;
				} else if(name_entry_row_ != 0U) {
					name_entry_row_ = 0U;
					name_entry_column_ = name_entry_lines_[0U].size();
					changed = true;
				}
			} else if(input.delete_forward && name_entry_column_ < line.size()) {
				line.erase(name_entry_column_,
					next_utf8_offset(line, name_entry_column_) - name_entry_column_);
				changed = true;
			} else if(input.home) {
				name_entry_column_ = 0U;
				changed = true;
			} else if(input.end) {
				name_entry_column_ = line.size();
				changed = true;
			} else if(input.ui.up || input.ui.down) {
				const auto cells = utf8_cells(line, name_entry_column_);
				name_entry_row_ = input.ui.up ? 0U : 1U;
				name_entry_column_ = utf8_offset_for_cells(
					name_entry_lines_[name_entry_row_], cells
				);
				changed = true;
			} else if(input.ui.left) {
				if(name_entry_column_ != 0U) {
					name_entry_column_ = previous_utf8_offset(
						line, name_entry_column_
					);
				} else if(name_entry_row_ != 0U) {
					name_entry_row_ = 0U;
					name_entry_column_ = name_entry_lines_[0U].size();
				}
				changed = true;
			} else if(input.ui.right) {
				if(name_entry_column_ < line.size()) {
					name_entry_column_ = next_utf8_offset(
						line, name_entry_column_
					);
				} else if(name_entry_row_ == 0U) {
					name_entry_row_ = 1U;
					name_entry_column_ = 0U;
				}
				changed = true;
			} else if(!typed.empty() && valid_new_classic_utf8(typed)) {
				for(std::size_t offset = 0U; offset < typed.size();) {
					const auto scalar = next_new_classic_utf8_scalar(typed, offset);
					if(!scalar ||
						!new_classic_unicode_printable_scalar(scalar->value)) {
						break;
					}
					const auto bytes = typed.substr(offset, scalar->bytes);
					auto &target = name_entry_lines_[name_entry_row_];
					const auto cells = utf8_cells(target) +
						new_classic_unicode_cell_width(scalar->value);
					if(cells <= new_classic_nametag_line_cells) {
						target.insert(name_entry_column_, bytes);
						name_entry_column_ += scalar->bytes;
						if(name_entry_text().size() <=
							new_classic_nametag_max_bytes) {
							changed = true;
						} else {
							target.erase(name_entry_column_ - scalar->bytes,
								scalar->bytes);
							name_entry_column_ -= scalar->bytes;
						}
					} else if(name_entry_row_ == 0U &&
						name_entry_column_ == target.size() &&
						name_entry_lines_[1U].empty()) {
						if(scalar->value == static_cast<std::uint32_t>(' ')) {
							name_entry_row_ = 1U;
							name_entry_column_ = 0U;
							changed = true;
							offset += scalar->bytes;
							continue;
						}
						const auto separator = target.find_last_of(' ');
						if(separator != std::string::npos) {
							auto &second_line = name_entry_lines_[1U];
							second_line = target.substr(separator + 1U);
							second_line.append(bytes);
							if(utf8_cells(second_line) <=
								new_classic_nametag_line_cells) {
								target.erase(separator);
								name_entry_row_ = 1U;
								name_entry_column_ = second_line.size();
								if(name_entry_text().size() <=
									new_classic_nametag_max_bytes) {
									changed = true;
								} else {
									target.push_back(' ');
									target += second_line.substr(
										0U, second_line.size() - scalar->bytes
									);
									second_line.clear();
									name_entry_row_ = 0U;
									name_entry_column_ = target.size();
								}
							} else {
								second_line.clear();
							}
						}
					}
					offset += scalar->bytes;
				}
			}
		} else {
			if(input.erase && entry_cursor_ != 0U) {
				entry_.erase(entry_cursor_ - 1U, 1U);
				--entry_cursor_;
				changed = true;
			} else if(input.delete_forward && entry_cursor_ < entry_.size()) {
				entry_.erase(entry_cursor_, 1U);
				changed = true;
			} else if(input.home) {
				entry_cursor_ = 0U;
				changed = true;
			} else if(input.end) {
				entry_cursor_ = entry_.size();
				changed = true;
			} else if(input.ui.left) {
				if(entry_cursor_ != 0U) {
					--entry_cursor_;
				}
				changed = true;
			} else if(input.ui.right) {
				if(entry_cursor_ < entry_.size()) {
					++entry_cursor_;
				}
				changed = true;
			} else if(!typed.empty()) {
				for(const auto source : typed) {
					const auto character = static_cast<char>(std::toupper(
						static_cast<unsigned char>(source)
					));
					if(link_code_character(character) && entry_.size() < 10U) {
						entry_.insert(entry_cursor_, 1U, character);
						++entry_cursor_;
						changed = true;
					}
				}
			}
		}
		if(changed) {
			invalid_entry_ = false;
			reset_cursor_blink();
			return draw();
		}
		if(input.ui.cancel) {
			if(!nametag_) {
				begin_fade_out(NewClassicOnlineProfileAction{
					NewClassicOnlineProfileActionType::back, {}
				});
				return false;
			}
			screen_ = Screen::menu;
			entry_.clear();
			name_entry_lines_ = {};
			invalid_entry_ = false;
			require_release();
			return draw();
		}
		if(input.ui.confirm) {
			const auto submitted = screen_ == Screen::name_entry ?
				name_entry_text() : entry_;
			const bool valid = screen_ == Screen::name_entry ?
				layout_new_classic_nametag(submitted).has_value() :
				(submitted.size() == 10U && std::ranges::all_of(
					submitted, link_code_character
				));
			if(!valid) {
				invalid_entry_ = true;
				reset_cursor_blink();
				require_release();
				return draw();
			}
			action_ = NewClassicOnlineProfileAction{
				screen_ == Screen::name_entry ?
					NewClassicOnlineProfileActionType::update_nametag :
					NewClassicOnlineProfileActionType::redeem_link_code,
				submitted,
			};
			request_pending_ = true;
			return false;
		}
		if(++cursor_blink_ticks_ >= cursor_blink_interval) {
			cursor_blink_ticks_ = 0U;
			cursor_visible_ = !cursor_visible_;
			return draw();
		}
		return false;
	}

	if(any) {
		screen_ = nametag_ ? Screen::menu : Screen::name_entry;
		link_code_.clear();
		entry_.clear();
		invalid_entry_ = false;
		require_release();
		return draw();
	}
	return false;
}

std::optional<NewClassicOnlineProfileAction>
NewClassicOnlineProfileScene::take_action() noexcept
{
	return std::exchange(action_, std::nullopt);
}

bool NewClassicOnlineProfileScene::report_profile(
	NewClassicOnlineProfile profile
) noexcept
{
	if(!request_pending_ || !profile.nametag ||
		!layout_new_classic_nametag(*profile.nametag)) {
		return false;
	}
	nametag_ = std::move(profile.nametag);
	entry_.clear();
	name_entry_lines_ = {};
	screen_ = Screen::menu;
	request_pending_ = false;
	require_release();
	return draw();
}

bool NewClassicOnlineProfileScene::report_link_code(std::string code) noexcept
{
	if(!request_pending_ || code.size() != 10U ||
		!std::ranges::all_of(code, link_code_character)) {
		return false;
	}
	link_code_ = std::move(code);
	screen_ = Screen::link_code;
	request_pending_ = false;
	require_release();
	return draw();
}

bool NewClassicOnlineProfileScene::report_linked(
	NewClassicOnlineProfile profile
) noexcept
{
	if(!request_pending_ || (profile.nametag &&
		!layout_new_classic_nametag(*profile.nametag))) {
		return false;
	}
	nametag_ = std::move(profile.nametag);
	entry_.clear();
	screen_ = nametag_ ? Screen::linked : Screen::name_entry;
	if(screen_ == Screen::name_entry) {
		begin_name_entry();
	}
	request_pending_ = false;
	require_release();
	return draw();
}

bool NewClassicOnlineProfileScene::report_failure() noexcept
{
	if(!request_pending_) {
		return false;
	}
	request_pending_ = false;
	screen_ = Screen::failed;
	require_release();
	return draw();
}

bool NewClassicOnlineProfileScene::valid() const noexcept
{
	return valid_;
}

bool NewClassicOnlineProfileScene::text_entry_active() const noexcept
{
	return screen_ == Screen::name_entry || screen_ == Screen::link_entry;
}

present::SourceSurface NewClassicOnlineProfileScene::surface() const noexcept
{
	return patch_surface_.surface();
}

} // namespace th03::app
