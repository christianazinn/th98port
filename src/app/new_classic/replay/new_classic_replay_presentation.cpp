#include "new_classic/replay/new_classic_replay_presentation_internal.hpp"

#include <array>
#include <cstdio>

namespace th03::app::replay_presentation {

bool put_fixed(
	const NewClassicMenuFont &font,
	present::Raster &raster,
	std::int32_t left,
	std::int32_t top,
	std::string_view text,
	std::int32_t cell_width,
	present::Index4 color
) noexcept
{
	for(const char character : text) {
		auto glyph_left = left;
		if(character == '1') {
			glyph_left += 2;
		} else if(character == 'I' && cell_width == 13) {
			glyph_left += 5;
		}
		if(!font.put_n(raster, glyph_left, top,
			std::string_view(&character, 1U), 1U, color)) {
			return false;
		}
		left += cell_width;
	}
	return true;
}

[[nodiscard]] bool ui_input_present(NewClassicUiInput input) noexcept
{
	return input.up || input.down || input.left || input.right ||
		input.confirm || input.bomb || input.cancel;
}

[[nodiscard]] std::string_view localized_rank_label(
	std::uint8_t rank,
	const NewClassicMessageCatalog &messages
) noexcept
{
	constexpr std::array<NewClassicMessageId, 4U> labels{
		NewClassicMessageId::value_easy,
		NewClassicMessageId::value_normal,
		NewClassicMessageId::value_hard,
		NewClassicMessageId::value_lunatic,
	};
	return rank < labels.size() ? messages.text(labels[rank]) : "?";
}

[[nodiscard]] std::string_view localized_character_label(
	std::int8_t paletted_character,
	const NewClassicMessageCatalog &messages
) noexcept
{
	constexpr std::array<NewClassicMessageId, 9U> labels{
		NewClassicMessageId::replay_character_reimu,
		NewClassicMessageId::replay_character_mima,
		NewClassicMessageId::replay_character_marisa,
		NewClassicMessageId::replay_character_ellen,
		NewClassicMessageId::replay_character_kotohime,
		NewClassicMessageId::replay_character_kana,
		NewClassicMessageId::replay_character_rikako,
		NewClassicMessageId::replay_character_chiyuri,
		NewClassicMessageId::replay_character_yumemi,
	};
	if(paletted_character < 1 || paletted_character > 18) {
		return "-";
	}
	return messages.text(
		labels[static_cast<std::size_t>(paletted_character - 1) / 2U]
	);
}

[[nodiscard]] std::string dos_date_label(std::uint16_t packed)
{
	if(packed == 0U) {
		return "-";
	}
	char buffer[11U]{};
	static_cast<void>(std::snprintf(
		buffer, sizeof(buffer), "%02u-%02u-%04u",
		static_cast<unsigned>((packed >> 5U) & 0x0FU),
		static_cast<unsigned>(packed & 0x1FU),
		static_cast<unsigned>(1980U + (packed >> 9U))
	));
	return buffer;
}

} // namespace th03::app::replay_presentation
