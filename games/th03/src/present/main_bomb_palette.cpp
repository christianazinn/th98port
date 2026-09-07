#include "th03/present/main_bomb_palette.hpp"

#include <cstdint>
#include <optional>

namespace th03::present {
namespace {

enum class BombCharacter : std::uint8_t {
	reimu = 0U,
	mima,
	marisa,
	ellen,
	kotohime,
	kana,
	rikako,
	chiyuri,
	yumemi,
};

[[nodiscard]] constexpr std::optional<std::uint8_t> player_index(
	PlayerId player
) noexcept
{
	if(player == PlayerId::p1) {
		return std::uint8_t{0U};
	}
	if(player == PlayerId::p2) {
		return std::uint8_t{1U};
	}
	return std::nullopt;
}

[[nodiscard]] constexpr std::optional<BombCharacter> bomb_character(
	std::int8_t paletted_character
) noexcept
{
	constexpr auto first = std::int8_t{1};
	constexpr auto last = std::int8_t{18};
	if(paletted_character < first || paletted_character > last) {
		return std::nullopt;
	}
	const auto offset = static_cast<std::uint8_t>(
		paletted_character - first
	);
	return static_cast<BombCharacter>(offset / 2U);
}

[[nodiscard]] constexpr std::uint8_t byte_add(
	std::uint8_t lhs,
	std::uint8_t rhs
) noexcept
{
	return static_cast<std::uint8_t>(
		static_cast<std::uint16_t>(lhs) + static_cast<std::uint16_t>(rhs)
	);
}

[[nodiscard]] constexpr std::uint8_t byte_subtract(
	std::uint8_t lhs,
	std::uint8_t rhs
) noexcept
{
	return static_cast<std::uint8_t>(
		static_cast<std::uint16_t>(lhs) - static_cast<std::uint16_t>(rhs)
	);
}

[[nodiscard]] constexpr std::uint8_t byte_multiply(
	std::uint8_t value,
	std::uint8_t multiplier
) noexcept
{
	return static_cast<std::uint8_t>(
		static_cast<std::uint16_t>(value) *
		static_cast<std::uint16_t>(multiplier)
	);
}

[[nodiscard]] constexpr std::uint8_t byte_shift_left(
	std::uint8_t value,
	unsigned int shift
) noexcept
{
	return static_cast<std::uint8_t>(
		static_cast<std::uint16_t>(value) << shift
	);
}

[[nodiscard]] constexpr std::uint8_t fade_after_shift(
	std::uint8_t frame
) noexcept
{
	return byte_subtract(0xFFU, byte_shift_left(frame, 3U));
}

void set_deferred_rgb(
	MainPaletteState &palette,
	Raster &raster,
	std::uint8_t player,
	Rgb8 color
) noexcept
{
	// player_index() proves 0..1 before this helper is reached.
	(void)palette.set_entry_deferred(raster, player, color);
}

} // namespace

MainBombPaletteInterpreter::MainBombPaletteInterpreter(
	Raster &raster,
	MainPaletteState &palette,
	KotohimeBombFadeCompatibility kotohime_compatibility
) noexcept :
	raster_(&raster),
	palette_(&palette),
	kotohime_compatibility_(kotohime_compatibility)
{}

void MainBombPaletteInterpreter::bomb_frame_step(
	const BombFrameStep &step
) noexcept
{
	const auto player = player_index(step.player);
	if(!player.has_value()) {
		status_ = MainBombPaletteStatus::invalid_player;
		return;
	}
	const auto character = bomb_character(step.paletted_character);
	if(!character.has_value()) {
		status_ = MainBombPaletteStatus::invalid_paletted_character;
		return;
	}

	const auto frame = step.bomb_frame;
	switch(*character) {
	case BombCharacter::reimu: {
		// th03/main/player/ch_shot.cpp::reimu_bomb()
		if(frame < 64U) {
			set_deferred_rgb(
				*palette_,
				*raster_,
				*player,
				{
					byte_shift_left(frame, 2U),
					byte_multiply(frame, 3U),
					byte_multiply(frame, 3U),
				}
			);
			return;
		}
		if(frame < 112U) {
			palette_->set_tone_deferred(
				*raster_,
				(frame <= 96U) ? static_cast<std::int16_t>(
					100 + ((frame & 1U) * 80U)
				) : 100
			);
			return;
		}
		const auto reimu_fade = fade_after_shift(frame);
		set_deferred_rgb(
			*palette_, *raster_, *player,
			{reimu_fade, reimu_fade, reimu_fade}
		);
		return;
	}

	case BombCharacter::mima: {
		// th03/main/bullet/bullet.cpp::mima_bomb()
		if(frame <= 80U) {
			const auto fade = byte_subtract(
				0U, byte_multiply(frame, 3U)
			);
			set_deferred_rgb(*palette_, *raster_, *player, {fade, fade, fade});
			return;
		}
		if(frame < 144U) {
			set_deferred_rgb(
				*palette_,
				*raster_,
				*player,
				{static_cast<std::uint8_t>(
					((frame & 1U) == 0U) ? 100U : 0U
				), 0U, 0U}
			);
			return;
		}
		const auto mima_fade = fade_after_shift(frame);
		set_deferred_rgb(
			*palette_, *raster_, *player,
			{mima_fade, mima_fade, mima_fade}
		);
		return;
	}

	case BombCharacter::marisa: {
		// th03/main/player/ch_shot.cpp::marisa_bomb()
		if(frame < 64U) {
			const auto fade = byte_subtract(0U, byte_shift_left(frame, 2U));
			set_deferred_rgb(*palette_, *raster_, *player, {fade, fade, fade});
			return;
		}
		if(frame < 128U) {
			return;
		}
		const auto marisa_fade = fade_after_shift(frame);
		set_deferred_rgb(
			*palette_, *raster_, *player,
			{marisa_fade, marisa_fade, marisa_fade}
		);
		return;
	}

	case BombCharacter::ellen: {
		// th03/main/player/chybomb.cpp::ellen_bomb()
		if(frame < 64U) {
			const auto color = byte_shift_left(frame, 1U);
			set_deferred_rgb(
				*palette_,
				*raster_,
				*player,
				{byte_shift_left(color, 1U), color, color}
			);
			return;
		}
		if(frame < 128U) {
			set_deferred_rgb(
				*palette_,
				*raster_,
				*player,
				((frame & 1U) != 0U) ?
					Rgb8{0xFFU, 0x80U, 0x80U} : Rgb8{0U, 0U, 0x20U}
			);
			return;
		}
		const auto ellen_fade = fade_after_shift(frame);
		set_deferred_rgb(
			*palette_,
			*raster_,
			*player,
			{byte_shift_left(ellen_fade, 1U), ellen_fade, ellen_fade}
		);
		return;
	}

	case BombCharacter::kotohime: {
		// th03/main/player/chybomb.cpp::kotohime_bomb()
		if(frame < 64U) {
			const auto color = byte_multiply(frame, 3U);
			set_deferred_rgb(
				*palette_,
				*raster_,
				*player,
				{byte_add(color, 64U), byte_add(color, 32U), color}
			);
			return;
		}
		if(frame < 128U) {
			palette_->set_tone_deferred(
				*raster_, ((frame & 3U) < 2U) ? 170 : 100
			);
			return;
		}
		const auto kotohime_fade = fade_after_shift(frame);
		auto kotohime_red_green = byte_shift_left(kotohime_fade, 1U);
		if(kotohime_compatibility_.component_provider != nullptr &&
			!kotohime_compatibility_.component_provider(
				kotohime_compatibility_.context,
				step,
				kotohime_fade,
				kotohime_red_green
			)) {
			// The source writes PaletteTone first, but preserving an unobservable
			// partial state would make this compatibility failure non-atomic.
			status_ = MainBombPaletteStatus::rejected_kotohime_component;
			return;
		}
		palette_->set_tone_deferred(*raster_, 100);
		set_deferred_rgb(
			*palette_,
			*raster_,
			*player,
			{kotohime_red_green, kotohime_red_green, kotohime_fade}
		);
		return;
	}

	case BombCharacter::kana: {
		// th03/main/player/chybomb.cpp::kana_bomb()
		if(frame < 64U) {
			const auto color = byte_subtract(64U, frame);
			set_deferred_rgb(*palette_, *raster_, *player, {color, color, color});
			return;
		}
		if(frame < 128U) {
			const auto color = static_cast<std::uint8_t>(
				((frame & 3U) < 2U) ? 160U : 0U
			);
			set_deferred_rgb(*palette_, *raster_, *player, {color, color, color});
			return;
		}
		const auto kana_fade = fade_after_shift(frame);
		set_deferred_rgb(
			*palette_,
			*raster_,
			*player,
			{
				byte_shift_left(kana_fade, 1U),
				byte_shift_left(kana_fade, 1U),
				kana_fade,
			}
		);
		return;
	}

	case BombCharacter::rikako: {
		// th03/main_05.cpp::rikako_bomb()
		if(frame < 64U) {
			const auto color = byte_shift_left(frame, 2U);
			set_deferred_rgb(*palette_, *raster_, *player, {color, color, color});
			return;
		}
		if(frame < 128U) {
			return;
		}
		const auto rikako_fade = fade_after_shift(frame);
		set_deferred_rgb(
			*palette_,
			*raster_,
			*player,
			{byte_shift_left(rikako_fade, 1U), rikako_fade, rikako_fade}
		);
		return;
	}

	case BombCharacter::chiyuri: {
		// th03/main/player/chybomb.cpp::chiyuri_bomb()
		if(frame < 64U) {
			const auto color = byte_subtract(0xFFU, byte_shift_left(frame, 2U));
			set_deferred_rgb(*palette_, *raster_, *player, {color, color, color});
			return;
		}
		if(frame < 144U) {
			palette_->set_tone_deferred(
				*raster_, ((frame & 3U) < 2U) ? 60 : 120
			);
			return;
		}
		const auto chiyuri_fade = fade_after_shift(frame);
		palette_->set_tone_deferred(*raster_, 100);
		set_deferred_rgb(
			*palette_,
			*raster_,
			*player,
			{byte_shift_left(chiyuri_fade, 1U), chiyuri_fade, chiyuri_fade}
		);
		return;
	}

	case BombCharacter::yumemi: {
		// th03/main/player/yumemi_bomb.cpp::yumemi_bomb(). The generated
		// assembly proves the early raw island makes all components frame << 2.
		if(frame <= 64U) {
			const auto color = byte_shift_left(frame, 2U);
			set_deferred_rgb(*palette_, *raster_, *player, {color, color, color});
			return;
		}
		if(frame < 144U) {
			palette_->set_tone_and_show_now(
				*raster_, ((frame & 7U) == 0U) ? 140 : 60
			);
			return;
		}
		palette_->set_tone_and_show_now(*raster_, 100);
		const auto yumemi_fade = fade_after_shift(frame);
		set_deferred_rgb(
			*palette_,
			*raster_,
			*player,
			{yumemi_fade, yumemi_fade, yumemi_fade}
		);
		return;
	}
	}
}

void MainBombPaletteInterpreter::bomb_finished(
	const BombFinished &finished
) noexcept
{
	const auto player = player_index(finished.player);
	if(!player.has_value()) {
		status_ = MainBombPaletteStatus::invalid_player;
		return;
	}
	if(!bomb_character(finished.paletted_character).has_value()) {
		status_ = MainBombPaletteStatus::invalid_paletted_character;
		return;
	}
	if(!palette_->restore_entry_deferred(*raster_, *player)) {
		status_ = MainBombPaletteStatus::missing_baseline;
		return;
	}
}

MainBombPaletteStatus MainBombPaletteInterpreter::status() const noexcept
{
	return status_;
}

} // namespace th03::present
