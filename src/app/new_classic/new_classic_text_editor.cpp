#include "th03/app/new_classic/new_classic_frontend.hpp"

namespace th03::app {

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
