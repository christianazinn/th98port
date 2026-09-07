#ifndef TH03_APP_NEW_CLASSIC_LANGUAGE_INTERNAL_HPP
#define TH03_APP_NEW_CLASSIC_LANGUAGE_INTERNAL_HPP

#include <expected>
#include <span>

#include "classic/mainl/mainl_compiled_text_internal.hpp"
#include "classic/op/op_compiled_text_internal.hpp"
#include "th03/app/content/main_compiled_content.hpp"
#include "th03/app/new_classic/content/new_classic_game_text.hpp"
#include "th03/app/new_classic/content/new_classic_language.hpp"

namespace th03::app {

void apply_new_classic_english_text(
	MainCompiledContent &compiled,
	MainlStageTextAssets &stage,
	OpMusicRoomTextImport &music,
	const NewClassicGameTextCatalog &text
);
void apply_new_classic_english_stage_text(
	MainlStageTextAssets &stage,
	const NewClassicGameTextCatalog &text
);

namespace detail {

[[nodiscard]] std::expected<
	NewClassicLanguageOverlay,
	NewClassicLanguageOverlayError
> parse_new_classic_language_overlay_for_test(
	std::span<const std::byte> bytes,
	const MainExecutableSha256 &accepted_digest
);

} // namespace detail
} // namespace th03::app

#endif
