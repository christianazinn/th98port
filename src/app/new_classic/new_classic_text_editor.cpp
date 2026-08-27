#include "th03/app/new_classic/new_classic_frontend.hpp"

namespace th03::app {

NewClassicUiInput new_classic_menu_ui(
	const LogicalTickInput &sample,
	bool accept_player_words
) noexcept
{
	constexpr std::uint16_t input_up = 0x0001U;
	constexpr std::uint16_t input_down = 0x0002U;
	constexpr std::uint16_t input_left = 0x0004U;
	constexpr std::uint16_t input_right = 0x0008U;
	constexpr std::uint16_t input_bomb = 0x0010U;
	constexpr std::uint16_t input_shot = 0x0020U;
	constexpr std::uint16_t input_cancel = 0x1000U;
	constexpr std::uint16_t input_ok = 0x2000U;
	const auto word = accept_player_words ?
		new_classic_interface_input_word(sample) : sample.input.system;
	const auto pressed = [word](std::uint16_t bit) noexcept {
		return (word & bit) != 0U;
	};
	return {
		.up = pressed(input_up),
		.down = pressed(input_down),
		.left = pressed(input_left),
		.right = pressed(input_right),
		.confirm = pressed(input_shot) || pressed(input_ok),
		.bomb = pressed(input_bomb),
		.cancel = pressed(input_cancel),
		.app_quit_requested = sample.app_quit_requested,
	};
}

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

} // namespace th03::app
