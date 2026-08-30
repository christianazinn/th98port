#ifndef TH03_APP_NEW_CLASSIC_REPLAY_PRESENTATION_INTERNAL_HPP
#define TH03_APP_NEW_CLASSIC_REPLAY_PRESENTATION_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "th03/app/new_classic/new_classic_frontend.hpp"

namespace th03::app::replay_presentation {

[[nodiscard]] std::string source_fixed_name(
	const std::array<char, 8U> &name
);
[[nodiscard]] std::string slot_number(std::size_t slot);
[[nodiscard]] std::string decimal_score(
	const std::array<std::uint8_t, 4U> &packed
);
[[nodiscard]] std::array<std::uint8_t, 4U> replay_list_score(
	const formats::ReplayDocument &document,
	std::optional<std::uint8_t> local_player = std::nullopt
) noexcept;
[[nodiscard]] std::string catalog_marker(
	const ReplayCatalogSlot &slot,
	const NewClassicMessageCatalog &messages
);

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
[[nodiscard]] std::size_t page_top(
	std::size_t selection, std::size_t rows
) noexcept;
[[nodiscard]] std::size_t page_count(
	std::size_t entries, std::size_t rows
) noexcept;
[[nodiscard]] std::string page_label(
	std::size_t selection,
	std::size_t entries,
	std::size_t rows,
	const NewClassicMessageCatalog &messages
);

} // namespace th03::app::replay_presentation

#endif
