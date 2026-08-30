#include "th03/app/new_classic/new_classic_frontend.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "new_classic/replay/new_classic_replay_presentation_internal.hpp"
#include "th03/assets/decode.hpp"

namespace th03::app {
namespace {

constexpr std::size_t browser_rows = 10U;
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

} // namespace

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
	const auto name_background = phase_ == Phase::name ||
		(phase_ == Phase::quit_confirm &&
			return_phase_ == Phase::name);
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
			static_cast<void>(replay_presentation::put_fixed(
				font, raster, 64, top, replay_presentation::slot_number(slot), 12, color
			));
			if(entry.state != ReplayCatalogSlotState::ready) {
				static_cast<void>(put(104, top, messages.text(
					NewClassicMessageId::replay_none
				), color));
				continue;
			}
			static_cast<void>(replay_presentation::put_fixed(
				font, raster, 104, top, replay_presentation::source_fixed_name(entry.name), 13, color
			));
			const auto score = replay_presentation::decimal_score(entry.final_score);
			static_cast<void>(replay_presentation::put_fixed(
				font, raster,
				340 - static_cast<std::int32_t>(score.size() * 12U), top,
				score, 12, color
			));
			static_cast<void>(put(
				364, top, replay_presentation::localized_character_label(
					entry.display_paletted_character, messages
				), color
			));
			static_cast<void>(put(
				476, top, replay_presentation::localized_rank_label(entry.rank, messages), color
			));
			static_cast<void>(replay_presentation::put_fixed(
				font, raster, 560, top, replay_presentation::catalog_marker(entry, messages), 12, color
			));
		}
		static_cast<void>(font.put_centered(
			raster, 320, 368,
			replay_presentation::page_label(
				selected_slot_, replay_slot_count, browser_rows, messages
			),
			15U
		));
	};

	if(phase_ == Phase::name) {
		const auto fixed_name = replay_presentation::source_fixed_name(name_);
		const auto score = replay_presentation::decimal_score(replay_presentation::replay_list_score(
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
			static_cast<void>(replay_presentation::put_fixed(
				font, raster, 80, 96, fixed_name, 24, 15U
			));
			static_cast<void>(replay_presentation::put_fixed(
				font, raster,
				400 + static_cast<std::int32_t>((9U - score.size()) * 16U),
				96, score, 16, 15U
			));
		}
		if(document_.metadata) {
			const auto date = replay_presentation::dos_date_label(document_.metadata->dos_date);
			if(save_art_) {
				static_cast<void>(put_regi_text(80, 160, date, 20));
			} else {
				static_cast<void>(replay_presentation::put_fixed(
					font, raster, 80, 160, date, 20, 15U
				));
			}
			const auto rank = replay_presentation::localized_rank_label(
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
					replay_presentation::localized_character_label(
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
					static_cast<void>(replay_presentation::put_fixed(
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
		static_cast<void>(raster.clear({184, 160, 272U, 80U}, 0U));
		std::string question;
		if(phase_ == Phase::prompt) {
			question = messages.text(NewClassicMessageId::replay_save_prompt);
		} else if(phase_ == Phase::overwrite) {
			const auto slot = replay_presentation::slot_number(selected_slot_);
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
			input_allowed_ = !replay_presentation::ui_input_present(input);
			changed = true;
		}
		return {
			.surface_changed = changed,
			.status = status_,
		};
	}
	if(!input_allowed_) {
		if(!replay_presentation::ui_input_present(input)) {
			input_allowed_ = true;
		}
		return {.status = status_};
	}
	if(!replay_presentation::ui_input_present(input)) {
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
			selected_slot_ < browser_rows ?
				selected_slot_ + replay_slot_count - browser_rows :
				selected_slot_ - browser_rows);
	} else if(input.right) {
		selected_slot_ = static_cast<std::uint8_t>(
			selected_slot_ >= replay_slot_count - browser_rows ?
				selected_slot_ - replay_slot_count + browser_rows :
				selected_slot_ + browser_rows);
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
	top_slot_ = static_cast<std::uint8_t>(
		replay_presentation::page_top(selected_slot_, browser_rows)
	);
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
	menu.top_slot_ = static_cast<std::uint8_t>(
		replay_presentation::page_top(menu.selected_slot_, browser_rows)
	);
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

std::uint8_t NewClassicReplaySaveTestAccess::top_slot(
	const NewClassicReplaySaveMenu &menu
) noexcept
{
	return menu.top_slot_;
}

} // namespace th03::app
