#include "th03/present/main_bombs.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "th03/present/main_static.hpp"
#include "th03/present/triangles.hpp"
#include "th03/present/vectors.hpp"

namespace th03::present {
namespace {

constexpr std::int16_t playfield_center_x_q4 = 144 << 4;
constexpr std::int16_t playfield_center_y_q4 = 184 << 4;
constexpr std::int16_t ellen_vector_free = 9999;
constexpr std::int16_t ellen_vector_inactive = 19999;

constexpr std::array<Sprite16HorizontalClip, main_bomb_player_count>
	playfield_sprite_clips{{
		{0, 319},
		{320, 639},
	}};
constexpr Sprite16HorizontalClip both_playfields_clip{0, 639};
constexpr std::array<GrcgClipRect, main_bomb_player_count>
	playfield_grcg_clips{{
		{16, 8, 303, 191},
		{336, 8, 623, 191},
	}};
constexpr GrcgClipRect full_zoomed_grcg_clip{0, 0, 639, 199};
constexpr std::array<std::uint8_t, 6U> yumemi_angles{{
	0xC0U, 0x15U, 0x6BU, 0x40U, 0xEBU, 0x95U,
}};

[[nodiscard]] constexpr std::int16_t wrap_word(std::int32_t value) noexcept
{
	const auto bits = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(value) & 0xFFFFU
	);
	return (bits <= 0x7FFFU) ?
		static_cast<std::int16_t>(bits) :
		static_cast<std::int16_t>(static_cast<std::int32_t>(bits) - 0x10000);
}

[[nodiscard]] constexpr std::int32_t arithmetic_shift_right(
	std::int32_t value,
	unsigned int count
) noexcept
{
	const auto divisor = static_cast<std::int64_t>(1) << count;
	if(value >= 0) {
		return static_cast<std::int32_t>(value / divisor);
	}
	return static_cast<std::int32_t>(
		-(((-static_cast<std::int64_t>(value)) + divisor - 1) / divisor)
	);
}

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

[[nodiscard]] constexpr std::optional<std::uint8_t> character_index(
	std::int8_t paletted_character
) noexcept
{
	if(paletted_character < 1 || paletted_character > 18) {
		return std::nullopt;
	}
	return static_cast<std::uint8_t>((paletted_character - 1) / 2);
}

[[nodiscard]] constexpr std::int16_t foreground_x(
	std::int16_t raw_x,
	std::uint8_t player,
	std::int16_t shift
) noexcept
{
	return wrap_word(
		arithmetic_shift_right(raw_x, 4U) +
		static_cast<std::int32_t>(main_playfield_border) +
		static_cast<std::int32_t>(player * main_playfield_bordered_width) +
		shift
	);
}

[[nodiscard]] constexpr std::int16_t shake(std::uint8_t frame) noexcept
{
	return ((frame & 3U) < 2U) ? std::int16_t{4} : std::int16_t{-4};
}

[[nodiscard]] constexpr std::int16_t polar(
	std::int16_t center,
	std::int16_t radius,
	std::int16_t ratio
) noexcept
{
	return wrap_word(
		static_cast<std::int32_t>(center) + arithmetic_shift_right(
			static_cast<std::int32_t>(radius) * ratio, 8U
		)
	);
}

[[nodiscard]] constexpr bool circle_fact(
	const CommonEffectSpawned &spawn,
	CommonEffectKind kind,
	std::int16_t center_x,
	std::int16_t center_y,
	std::int16_t shift
) noexcept
{
	return spawn.kind == kind && spawn.center_x == center_x &&
		spawn.center_y == center_y && spawn.fg_shift_x_at_spawn == shift &&
		spawn.initial_angle == 0U;
}

} // namespace

void MainBombsPresenter::update_reimu(std::uint8_t player) noexcept
{
	for(auto &record : reimu_[player]) {
		if(!record.active) {
			continue;
		}
		record.y = wrap_word(
			static_cast<std::int32_t>(record.y) + record.velocity_y
		);
		if(record.y <= -128) {
			record.y = 384 << 4;
			record.velocity_y = wrap_word(
				static_cast<std::int32_t>(record.velocity_y) + 8
			);
		}
	}
}

bool MainBombsPresenter::draw_reimu(
	std::uint8_t player,
	std::int16_t shift
) noexcept
{
	for(const auto &record : reimu_[player]) {
		if(!record.active) {
			continue;
		}
		const Point position{
			wrap_word(static_cast<std::int32_t>(
				foreground_x(record.x, player, shift)
			) - 8),
			wrap_word(arithmetic_shift_right(record.y, 4U) + 8),
		};
		if(!sprite16_put(
			*raster_, *staging_, layout_.reimu_falling[player], position,
			both_playfields_clip
		)) {
			return fail(MainBombsStatus::draw_failed);
		}
	}
	return true;
}

bool MainBombsPresenter::draw_mima(
	std::uint8_t player,
	std::int16_t shift
) noexcept
{
	for(std::size_t column = 0U; column < mima_[player].size(); ++column) {
		if(!mima_[player][column].active) {
			break;
		}
		if(!sprite16_put_downwards_column(
			*raster_,
			*staging_,
			layout_.mima_columns[player][column],
			{foreground_x(mima_[player][column].x, player, shift), 16},
			both_playfields_clip
		)) {
			return fail(MainBombsStatus::draw_failed);
		}
	}
	return true;
}

bool MainBombsPresenter::draw_marisa(
	std::uint8_t player,
	std::uint8_t frame
) noexcept
{
	if(!marisa_point_.initialized) {
		return fail(MainBombsStatus::invalid_observation);
	}
	const auto cel = ((frame & 3U) < 2U) ? 1U : 0U;
	return sprite16_put(
		*raster_,
		*staging_,
		layout_.marisa_flashes[player][cel],
		{
			wrap_word(static_cast<std::int32_t>(marisa_point_.screen_x) - 24),
			wrap_word(static_cast<std::int32_t>(marisa_point_.screen_y) - 24),
		},
		both_playfields_clip
	) || fail(MainBombsStatus::draw_failed);
}

bool MainBombsPresenter::draw_ellen(std::uint8_t player) noexcept
{
	for(const auto &vector : ellen_[player]) {
		if(vector.x == ellen_vector_inactive || vector.x == ellen_vector_free) {
			continue;
		}
		if(!sprite16_put(
			*raster_,
			*staging_,
			layout_.ellen_vectors[player],
			{
				wrap_word(static_cast<std::int32_t>(
					foreground_x(vector.x, player, 0)
				) - 32),
				wrap_word(arithmetic_shift_right(vector.y, 4U) - 16),
			},
			playfield_sprite_clips[player]
		)) {
			return fail(MainBombsStatus::draw_failed);
		}
	}
	return true;
}

bool MainBombsPresenter::draw_rikako(
	std::uint8_t player,
	std::uint8_t frame
) noexcept
{
	if(!draw_mrs(
		player,
		static_cast<std::int16_t>(
			main_playfield_border + player * main_playfield_bordered_width
		)
	)) {
		return false;
	}
	const auto twice_radius = wrap_word(
		static_cast<std::int32_t>(rikako_radius_) * 2
	);
	const std::array<std::int16_t, 4U> raw_x{{
		wrap_word(playfield_center_x_q4 - rikako_radius_),
		wrap_word(playfield_center_x_q4 + rikako_radius_),
		wrap_word(playfield_center_x_q4 - twice_radius),
		wrap_word(playfield_center_x_q4 + twice_radius),
	}};
	for(const auto x : raw_x) {
		if(!grcg_vline(
			*raster_, foreground_x(x, player, shake(frame)), 8, 192,
			full_zoomed_grcg_clip, 15U
		)) {
			return fail(MainBombsStatus::draw_failed);
		}
	}
	rikako_radius_ = wrap_word(static_cast<std::int32_t>(rikako_radius_) + 0x41);
	if(rikako_radius_ >= (72 << 4)) {
		rikako_radius_ = 0;
	}
	return true;
}

bool MainBombsPresenter::draw_yumemi_triangles(
	std::uint8_t player,
	std::uint8_t frame
) noexcept
{
	const auto base = static_cast<std::int16_t>(
		main_playfield_border + player * main_playfield_bordered_width
	);
	const auto draw = [this, player, base](
		std::size_t first_angle,
		std::int16_t radius,
		Index4 color
	) noexcept {
		std::array<GrcgPoint, 3U> vertices{};
		for(std::size_t vertex = 0U; vertex < vertices.size(); ++vertex) {
			const auto angle = yumemi_angles[first_angle + vertex];
			vertices[vertex] = {
				wrap_word(static_cast<std::int32_t>(base) + polar(
					144, radius, trig_.cosine_q8[angle]
				)),
				static_cast<std::int16_t>(
					polar(200, radius, trig_.sine_q8[angle]) / 2
				),
			};
		}
		return grcg_triangle(
			*raster_, vertices[0U], vertices[1U], vertices[2U],
			playfield_grcg_clips[player], {color, 0xFFFFU}
		);
	};
	const auto first_radius = (frame < 32U) ?
		static_cast<std::int16_t>(224 - frame * 3U) : std::int16_t{128};
	if(!draw(0U, first_radius, (frame < 32U) ? 8U : 7U)) {
		return fail(MainBombsStatus::draw_failed);
	}
	if(frame > 32U && !draw(
		3U, static_cast<std::int16_t>(320 - frame * 3U), 8U
	)) {
		return fail(MainBombsStatus::draw_failed);
	}
	return true;
}

bool MainBombsPresenter::finish_ellen_prefix(
	std::uint8_t player,
	std::uint8_t frame
) noexcept
{
	auto vectors = ellen_[player];
	auto prefix = ellen_prefix_[player];
	if(!prefix.active) {
		prefix = {true, frame, 0U};
	} else if(prefix.frame != frame) {
		return fail(MainBombsStatus::invalid_observation);
	}
	for(std::size_t slot = prefix.next_slot; slot < vectors.size(); ++slot) {
		auto &vector = vectors[slot];
		bool respawn = (vector.x == ellen_vector_free);
		if(vector.x == ellen_vector_inactive) {
			continue;
		}
		if(!respawn) {
			vector.x = wrap_word(
				static_cast<std::int32_t>(vector.x) + vector.velocity_x
			);
			vector.y = wrap_word(
				static_cast<std::int32_t>(vector.y) + vector.velocity_y
			);
			respawn = vector.x <= -(32 << 4) || vector.x >= (320 << 4) ||
				vector.y <= -(32 << 4) || vector.y >= (400 << 4);
		}
		if(respawn) {
			return fail(MainBombsStatus::invalid_observation);
		}
	}
	ellen_[player] = vectors;
	ellen_prefix_[player] = {};
	return true;
}

bool MainBombsPresenter::render_frame(
	const BombFrameStep &step,
	std::uint8_t player,
	Character character
) noexcept
{
	const auto frame = step.bomb_frame;
	const auto base = static_cast<std::int16_t>(
		main_playfield_border + player * main_playfield_bordered_width
	);
	switch(character) {
	case Character::reimu:
		if(frame < 64U) {
			if(!fill_playfield(player)) {
				return false;
			}
			if(frame < 32U) {
				reimu_reads_[player] = {
					ReimuReadPhase::velocity, frame, 0,
				};
				return true;
			}
			return apply_palette(step) &&
				draw_reimu(player, step.fg_shift_x_at_entry);
		}
		if(!apply_palette(step)) {
			return false;
		}
		if(frame <= 96U && !draw_mrs(player, base)) {
			return false;
		}
		return (frame >= 96U) || draw_reimu(player, shake(frame));

	case Character::mima:
		if(frame <= 80U) {
			return fill_playfield(player) && apply_palette(step) &&
				draw_mima(player, step.fg_shift_x_at_entry);
		}
		if(!apply_palette(step)) {
			return false;
		}
		if(frame < 144U) {
			if((frame % 8U) == 0U) {
				return begin_pending(PendingKind::mima_circle, player, frame);
			}
			return draw_mrs(player, wrap_word(base + shake(frame)));
		}
		return true;

	case Character::marisa:
		if(frame < 64U) {
			return fill_playfield(player) && apply_palette(step);
		}
		if(!apply_palette(step)) {
			return false;
		}
		if(frame < 128U) {
			const auto current_shake = shake(frame);
			const auto left = static_cast<std::int16_t>(
				player * main_playfield_bordered_width +
				((current_shake > 0) ? 16 : 8)
			);
			if(!draw_mrs(player, left)) {
				return false;
			}
			if((frame % 8U) == 0U) {
				return begin_pending(PendingKind::marisa_circle, player, frame);
			}
			return draw_marisa(player, frame);
		}
		return true;

	case Character::ellen:
		if(frame < 64U) {
			if(!fill_playfield(player) || !apply_palette(step)) {
				return false;
			}
			ellen_release_cursor_ = 0U;
			return true;
		}
		if(!apply_palette(step)) {
			return false;
		}
		if(frame < 128U) {
			if(!draw_mrs(player, base)) {
				return false;
			}
			if((frame % 4U) == 0U) {
				return begin_pending(PendingKind::ellen_circle, player, frame);
			}
			return draw_ellen(player);
		}
		return true;

	case Character::kotohime:
		if(frame < 64U && !fill_playfield(player)) {
			return false;
		}
		return apply_palette(step) && begin_pending(
			PendingKind::kotohime_step, player, frame
		);

	case Character::kana:
		return apply_palette(step) && begin_pending(
			PendingKind::kana_step, player, frame
		);

	case Character::rikako:
		if(frame < 64U) {
			if(!fill_playfield(player) || !apply_palette(step)) {
				return false;
			}
			rikako_radius_ = 0;
			return true;
		}
		if(!apply_palette(step)) {
			return false;
		}
		return (frame >= 128U) || draw_rikako(player, frame);

	case Character::chiyuri:
		if(frame < 64U) {
			if(!fill_playfield(player) || !apply_palette(step)) {
				return false;
			}
			return ((frame % 8U) != 0U) || begin_pending(
				PendingKind::chiyuri_early_circle, player, frame
			);
		}
		if(!apply_palette(step)) {
			return false;
		}
		if(frame < 144U) {
			if((frame % 16U) == 0U) {
				return begin_pending(PendingKind::chiyuri_squares, player, frame);
			}
			return draw_mrs(player, base);
		}
		return true;

	case Character::yumemi:
		if(frame <= 64U) {
			return draw_yumemi_triangles(player, frame) && apply_palette(step);
		}
		if(!apply_palette(step)) {
			return false;
		}
		if(frame < 144U) {
			if((frame % 8U) == 0U) {
				return begin_pending(PendingKind::yumemi_square, player, frame);
			}
			return draw_mrs(player, base);
		}
		return true;
	}
	return fail(MainBombsStatus::invalid_observation);
}

void MainBombsPresenter::bomb_frame_step(const BombFrameStep &step) noexcept
{
	if(!ready()) {
		return;
	}
	const auto player = player_index(step.player);
	const auto character_raw = character_index(step.paletted_character);
	if(!player || !character_raw || step.bomb_frame == 0U ||
		step.bomb_frame >= 180U || pending_.kind != PendingKind::none) {
		(void)fail(MainBombsStatus::invalid_observation);
		return;
	}
	for(const auto &read : reimu_reads_) {
		if(read.phase != ReimuReadPhase::none) {
			(void)fail(MainBombsStatus::invalid_observation);
			return;
		}
	}
	const auto character = static_cast<Character>(*character_raw);
	auto &activation = activations_[*player];
	// A round can end while a Bomb is still active. Source round preparation
	// clears the gameplay Bomb flag without running the 179-to-180 completion
	// callback, so frame 1 is the exact presentation re-arm signal as well as
	// the ordinary first activation frame.
	const bool first = step.bomb_frame == 1U;
	if((!activation.active && !first) || (activation.active && !first && (
		activation.paletted_character != step.paletted_character ||
		step.bomb_frame != static_cast<std::uint8_t>(activation.frame + 1U)
	))) {
		(void)fail(MainBombsStatus::invalid_observation);
		return;
	}
	if(!first && activation.character == Character::mima) {
		const bool expected = step.bomb_frame <= 64U &&
			(step.bomb_frame % 8U) == 0U;
		if(expected != (mima_read_frame_[*player] == step.bomb_frame)) {
			(void)fail(MainBombsStatus::invalid_observation);
			return;
		}
	}
	if(!first && activation.character == Character::ellen &&
		!finish_ellen_prefix(*player, step.bomb_frame)) {
		return;
	}
	if(first) {
		if(character == Character::reimu) {
			for(auto &record : reimu_[*player]) {
				record = {};
			}
		} else if(character == Character::mima) {
			for(auto &column : mima_[*player]) {
				column = {};
			}
			mima_read_frame_[*player] = 0U;
		} else if(character == Character::ellen) {
			for(auto &vector : ellen_[*player]) {
				vector = {};
				vector.x = ellen_vector_inactive;
			}
			ellen_prefix_[*player] = {};
		}
	}
	activation = {
		true, step.paletted_character, character, step.bomb_frame,
		step.fg_shift_x_at_entry,
	};
	if(character == Character::reimu && !first) {
		update_reimu(*player);
	}
	(void)render_frame(step, *player, character);
}

void MainBombsPresenter::bomb_compatibility_read(
	const BombCompatibilityRead &read
) noexcept
{
	if(!ready()) {
		return;
	}
	const auto player = player_index(read.player);
	if(!player) {
		(void)fail(MainBombsStatus::invalid_observation);
		return;
	}
	auto &activation = activations_[*player];
	if(!activation.active) {
		(void)fail(MainBombsStatus::invalid_observation);
		return;
	}
	if(activation.character == Character::reimu) {
		auto &state = reimu_reads_[*player];
		if(read.bomb_frame != activation.frame || read.bomb_frame >= 32U) {
			(void)fail(MainBombsStatus::invalid_observation);
			return;
		}
		if(state.phase == ReimuReadPhase::velocity) {
			if(read.site != BombCompatibilityReadSite::reimu_falling_velocity ||
				state.frame != read.bomb_frame) {
				(void)fail(MainBombsStatus::invalid_observation);
				return;
			}
			state.velocity_y = static_cast<std::int16_t>(
				-96 - static_cast<std::int16_t>(read.raw_value & 0x7FU)
			);
			state.phase = ReimuReadPhase::x;
			return;
		}
		if(state.phase != ReimuReadPhase::x ||
			read.site != BombCompatibilityReadSite::reimu_falling_x ||
			state.frame != read.bomb_frame) {
			(void)fail(MainBombsStatus::invalid_observation);
			return;
		}
		auto &record = reimu_[*player][read.bomb_frame];
		record = {
			true,
			static_cast<std::int16_t>(read.raw_value % (288U << 4U)),
			static_cast<std::int16_t>(384 << 4),
			state.velocity_y,
		};
		state = {};
		const BombFrameStep step{
			read.player,
			activation.paletted_character,
			activation.frame,
			activation.entry_shift,
		};
		(void)(apply_palette(step) && draw_reimu(*player, activation.entry_shift));
		return;
	}

	if(activation.character == Character::mima) {
		const auto expected_frame = static_cast<std::uint8_t>(activation.frame + 1U);
		if(read.site != BombCompatibilityReadSite::mima_column_x ||
			read.bomb_frame != expected_frame || read.bomb_frame > 64U ||
			(read.bomb_frame % 8U) != 0U ||
			mima_read_frame_[*player] == read.bomb_frame) {
			(void)fail(MainBombsStatus::invalid_observation);
			return;
		}
		const auto column = static_cast<std::size_t>(read.bomb_frame / 8U - 1U);
		mima_[*player][column] = {
			true,
			static_cast<std::int16_t>(read.raw_value % (288U << 4U)),
		};
		mima_read_frame_[*player] = read.bomb_frame;
		return;
	}

	if(activation.character != Character::ellen ||
		read.site != BombCompatibilityReadSite::ellen_vector_angle ||
		read.bomb_frame != static_cast<std::uint8_t>(activation.frame + 1U) ||
		read.bomb_frame > 180U) {
		(void)fail(MainBombsStatus::invalid_observation);
		return;
	}
	auto vectors = ellen_[*player];
	auto prefix = ellen_prefix_[*player];
	if(!prefix.active) {
		prefix = {true, read.bomb_frame, 0U};
	} else if(prefix.frame != read.bomb_frame) {
		(void)fail(MainBombsStatus::invalid_observation);
		return;
	}
	for(std::size_t slot = prefix.next_slot; slot < vectors.size(); ++slot) {
		auto &vector = vectors[slot];
		bool respawn = (vector.x == ellen_vector_free);
		if(vector.x == ellen_vector_inactive) {
			continue;
		}
		if(!respawn) {
			vector.x = wrap_word(
				static_cast<std::int32_t>(vector.x) + vector.velocity_x
			);
			vector.y = wrap_word(
				static_cast<std::int32_t>(vector.y) + vector.velocity_y
			);
			respawn = vector.x <= -(32 << 4) || vector.x >= (320 << 4) ||
				vector.y <= -(32 << 4) || vector.y >= (400 << 4);
		}
		if(!respawn) {
			continue;
		}
		const auto angle = static_cast<std::uint8_t>(read.raw_value);
		vector.x = playfield_center_x_q4;
		vector.y = playfield_center_y_q4;
		vector.velocity_x = wrap_word(arithmetic_shift_right(
			static_cast<std::int32_t>(224) * trig_.cosine_q8[angle], 8U
		));
		vector.velocity_y = wrap_word(arithmetic_shift_right(
			static_cast<std::int32_t>(224) * trig_.sine_q8[angle], 8U
		));
		prefix.next_slot = static_cast<std::uint8_t>(slot + 1U);
		ellen_[*player] = vectors;
		ellen_prefix_[*player] = prefix;
		return;
	}
	(void)fail(MainBombsStatus::invalid_observation);
}

void MainBombsPresenter::common_effect_spawned(
	const CommonEffectSpawned &spawn
) noexcept
{
	if(!ready() || pending_.kind == PendingKind::none) {
		return;
	}
	const auto player = player_index(spawn.player);
	if(!player || *player != pending_.player) {
		(void)fail(MainBombsStatus::invalid_observation);
		return;
	}
	const auto &activation = activations_[*player];
	const auto frame = pending_.frame;
	bool valid = false;
	switch(pending_.kind) {
	case PendingKind::mima_circle:
		valid = circle_fact(
			spawn, CommonEffectKind::expanding_circle,
			playfield_center_x_q4, playfield_center_y_q4,
			activation.entry_shift
		);
		break;
	case PendingKind::marisa_circle:
		valid = spawn.kind == CommonEffectKind::expanding_circle &&
			spawn.center_x >= 0 && spawn.center_x < (288 << 4) &&
			spawn.center_y >= 0 && spawn.center_y < (368 << 4) &&
			spawn.fg_shift_x_at_spawn == shake(frame) &&
			spawn.initial_angle == 0U;
		break;
	case PendingKind::ellen_circle:
		valid = circle_fact(
			spawn, CommonEffectKind::expanding_circle,
			playfield_center_x_q4, playfield_center_y_q4, shake(frame)
		);
		break;
	case PendingKind::chiyuri_early_circle:
		valid = circle_fact(
			spawn, CommonEffectKind::contracting_circle,
			playfield_center_x_q4, playfield_center_y_q4,
			activation.entry_shift
		);
		break;
	case PendingKind::chiyuri_squares: {
		const auto radius = static_cast<std::int16_t>((frame - 64U) * 32U);
		const std::array<std::array<std::int16_t, 2U>, 4U> centers{{
			{{wrap_word(playfield_center_x_q4 - radius), playfield_center_y_q4}},
			{{wrap_word(playfield_center_x_q4 + radius), playfield_center_y_q4}},
			{{playfield_center_x_q4, wrap_word(playfield_center_y_q4 - radius)}},
			{{playfield_center_x_q4, wrap_word(playfield_center_y_q4 + radius)}},
		}};
		const auto expected = centers[pending_.received];
		valid = spawn.kind == CommonEffectKind::contracting_square &&
			spawn.center_x == expected[0U] && spawn.center_y == expected[1U] &&
			spawn.fg_shift_x_at_spawn == shake(frame);
		break;
	}
	case PendingKind::yumemi_square:
		valid = spawn.kind == CommonEffectKind::contracting_square &&
			spawn.center_x == playfield_center_x_q4 &&
			spawn.center_y == playfield_center_y_q4 &&
			spawn.fg_shift_x_at_spawn == activation.entry_shift;
		break;
	case PendingKind::kana_step:
	case PendingKind::kotohime_step:
	case PendingKind::none:
		break;
	}
	if(!valid) {
		(void)fail(MainBombsStatus::invalid_observation);
		return;
	}

	switch(pending_.kind) {
	case PendingKind::mima_circle:
		clear_pending();
		(void)draw_mrs(
			*player,
			wrap_word(
				static_cast<std::int32_t>(main_playfield_border) +
				*player * main_playfield_bordered_width + shake(frame)
			)
		);
		return;
	case PendingKind::marisa_circle:
		marisa_point_ = {
			true,
			foreground_x(spawn.center_x, *player, spawn.fg_shift_x_at_spawn),
			wrap_word(arithmetic_shift_right(spawn.center_y, 4U) + 16),
		};
		clear_pending();
		(void)draw_marisa(*player, frame);
		return;
	case PendingKind::ellen_circle:
		if(ellen_release_cursor_ < main_ellen_bomb_vector_count) {
			ellen_[*player][ellen_release_cursor_].x = ellen_vector_free;
			ellen_release_cursor_++;
		}
		clear_pending();
		(void)draw_ellen(*player);
		return;
	case PendingKind::chiyuri_early_circle:
		clear_pending();
		return;
	case PendingKind::chiyuri_squares:
		pending_.received++;
		if(pending_.received == 4U) {
			clear_pending();
			(void)draw_mrs(
				*player,
				static_cast<std::int16_t>(
					main_playfield_border +
					*player * main_playfield_bordered_width
				)
			);
		}
		return;
	case PendingKind::yumemi_square:
		clear_pending();
		(void)draw_mrs(
			*player,
			static_cast<std::int16_t>(
				main_playfield_border + *player * main_playfield_bordered_width
			)
		);
		return;
	case PendingKind::kana_step:
	case PendingKind::kotohime_step:
	case PendingKind::none:
		return;
	}
}

void MainBombsPresenter::bomb_common_effect_step(
	const BombCommonEffectStep &step
) noexcept
{
	if(!ready()) {
		return;
	}
	const auto player = player_index(step.player);
	if(!player || *player != pending_.player || step.bomb_frame != pending_.frame) {
		(void)fail(MainBombsStatus::invalid_observation);
		return;
	}
	const bool kana = pending_.kind == PendingKind::kana_step;
	const bool kotohime = pending_.kind == PendingKind::kotohime_step;
	if((!kana && !kotohime) ||
		step.kind != (kana ? BombCommonEffectKind::kana :
			BombCommonEffectKind::kotohime)) {
		(void)fail(MainBombsStatus::invalid_observation);
		return;
	}
	const auto expected_shift = (step.bomb_frame < 64U) ?
		activations_[*player].entry_shift :
		((step.bomb_frame < 128U) ? shake(step.bomb_frame) : 0);
	if(step.fg_shift_x_at_effects != expected_shift) {
		(void)fail(MainBombsStatus::invalid_observation);
		return;
	}
	clear_pending();
	if(step.bomb_frame >= 64U && step.bomb_frame < 128U) {
		(void)draw_mrs(
			*player,
			static_cast<std::int16_t>(
				main_playfield_border + *player * main_playfield_bordered_width
			)
		);
	}
}

void MainBombsPresenter::bomb_finished(const BombFinished &finished) noexcept
{
	if(!ready()) {
		return;
	}
	const auto player = player_index(finished.player);
	if(!player || pending_.kind != PendingKind::none) {
		(void)fail(MainBombsStatus::invalid_observation);
		return;
	}
	auto &activation = activations_[*player];
	if(!activation.active || activation.frame != 179U ||
		activation.paletted_character != finished.paletted_character ||
		reimu_reads_[*player].phase != ReimuReadPhase::none) {
		(void)fail(MainBombsStatus::invalid_observation);
		return;
	}
	if(activation.character == Character::ellen &&
		!finish_ellen_prefix(*player, 180U)) {
		return;
	}
	palette_interpreter_.bomb_finished(finished);
	if(palette_interpreter_.status() != MainBombPaletteStatus::ready) {
		(void)fail(MainBombsStatus::palette_failed);
		return;
	}
	activation = {};
	ellen_prefix_[*player] = {};
}

MainBombsStatus MainBombsPresenter::status() const noexcept
{
	return status_;
}

bool MainBombsPresenter::ready() const noexcept
{
	return status_ == MainBombsStatus::ready;
}

} // namespace th03::present
