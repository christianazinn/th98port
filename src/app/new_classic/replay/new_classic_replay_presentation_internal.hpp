#ifndef TH03_APP_NEW_CLASSIC_REPLAY_PRESENTATION_INTERNAL_HPP
#define TH03_APP_NEW_CLASSIC_REPLAY_PRESENTATION_INTERNAL_HPP

#include <cstdint>
#include <string>
#include <string_view>

#include "th03/app/new_classic/new_classic_frontend.hpp"

namespace th03::app::replay_presentation {

[[nodiscard]] bool put_fixed(
	const NewClassicMenuFont &font,
	present::Raster &raster,
	std::int32_t left,
	std::int32_t top,
	std::string_view text,
	std::int32_t cell_width,
	present::Index4 color
) noexcept;
[[nodiscard]] bool ui_input_present(NewClassicUiInput input) noexcept;
[[nodiscard]] std::string_view localized_rank_label(
	std::uint8_t rank,
	const NewClassicMessageCatalog &messages
) noexcept;
[[nodiscard]] std::string_view localized_character_label(
	std::int8_t paletted_character,
	const NewClassicMessageCatalog &messages
) noexcept;
[[nodiscard]] std::string dos_date_label(std::uint16_t packed);

} // namespace th03::app::replay_presentation

#endif
