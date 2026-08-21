#include "th03/present/main_warning_gauge.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "th03/present/cels.hpp"
#include "th03/present/main_static.hpp"

namespace th03::present {
namespace {

constexpr std::uint16_t mrs_width = 288U;
constexpr std::uint16_t mrs_height = 184U;
constexpr std::size_t mrs_stride = mrs_width / 8U;
constexpr std::size_t mrs_plane_size = mrs_stride * mrs_height;
constexpr std::int16_t mrs_bottom_offset = 183 * 80;
constexpr std::uint8_t warning_step_first = 3U;
constexpr std::uint8_t warning_step_end = 33U;
constexpr std::uint8_t warning_level_glyph_base = 0x1FU;
constexpr std::int32_t warning_first_row = 11;
constexpr std::int32_t warning_last_row = 16;
constexpr std::int32_t warning_clear_width = 32;
constexpr std::uint8_t ramp_peak = 144U;
constexpr std::uint8_t ramp_step = 4U;
constexpr std::uint16_t gauge_max = 0x0FF0U;
constexpr std::int32_t gauge_source_left = 320;
constexpr std::int32_t gauge_source_top = 392;
constexpr std::int32_t gauge_p1_left = 16;
constexpr std::int32_t gauge_background_top = 192;
constexpr std::int32_t gauge_bar_top = 194;
constexpr std::uint16_t gauge_background_width = 288U;
constexpr std::uint16_t gauge_background_height = 8U;
constexpr std::uint16_t gauge_bar_height = 4U;

constexpr std::array<std::uint8_t, 16U> warning_row_one{
	0x50U, 0x51U, 0x52U, 0x53U, 0x54U, 0x55U, 0x56U, 0x57U,
	0x58U, 0x59U, 0x5AU, 0x5BU, 0x5CU, 0x5DU, 0x5EU, 0x5FU,
};
constexpr std::array<std::uint8_t, 16U> warning_row_two{
	0x60U, 0x61U, 0x62U, 0x63U, 0x64U, 0x65U, 0x66U, 0x67U,
	0x68U, 0x69U, 0x6AU, 0x6BU, 0x6CU, 0x6DU, 0x6EU, 0x6FU,
};
constexpr std::array<std::uint8_t, 16U> warning_row_three{
	0x70U, 0x71U, 0x72U, 0x73U, 0x74U, 0x75U, 0x76U, 0x77U,
	0x78U, 0x79U, 0x7AU, 0x7BU, 0x7CU, 0x7DU, 0x7EU, 0x7FU,
};
constexpr std::array<std::uint8_t, 11U> warning_you{
	0x82U, 0x83U, 0x84U, 0x85U, 0x86U, 0x87U,
	0x88U, 0x89U, 0x8AU, 0x8BU, 0x8CU,
};
constexpr std::array<std::uint8_t, 8U> warning_gauge{
	0xC7U, 0xC8U, 0xC9U, 0xCAU, 0xCBU, 0xCCU, 0xCDU, 0xCEU,
};
constexpr std::array<std::uint8_t, 8U> warning_boss{
	0xD0U, 0xD1U, 0xD2U, 0xCAU, 0xCBU, 0xCCU, 0xCDU, 0xCEU,
};
constexpr std::array<std::uint8_t, 14U> warning_peril{
	0x8DU, 0x8EU, 0x8FU, 0x92U, 0x93U, 0x94U, 0x95U,
	0x96U, 0x97U, 0x98U, 0x99U, 0x9AU, 0x9BU, 0x9CU,
};

[[nodiscard]] constexpr bool valid_player(PlayerId player) noexcept
{
	return static_cast<std::size_t>(player) < main_warning_player_count;
}

[[nodiscard]] constexpr bool valid_attack(GaugeBossAttackKind attack) noexcept
{
	return attack == GaugeBossAttackKind::none ||
		attack == GaugeBossAttackKind::gauge_pellet ||
		attack == GaugeBossAttackKind::gauge_bullet ||
		attack == GaugeBossAttackKind::boss;
}

[[nodiscard]] constexpr bool valid_warning_lifecycle(
	const WarningPlayerLifecycle &lifecycle
) noexcept
{
	if(!valid_attack(lifecycle.next_attack) ||
		!valid_attack(lifecycle.active_attack)) {
		return false;
	}
	if(lifecycle.warning_step_before < warning_step_first) {
		return lifecycle.warning_step_after == lifecycle.warning_step_before &&
			(lifecycle.warning_step_before == 0U ||
				lifecycle.next_attack != GaugeBossAttackKind::none);
	}
	if(lifecycle.warning_step_before < warning_step_end) {
		return lifecycle.warning_step_after == static_cast<std::uint8_t>(
			lifecycle.warning_step_before + 1U
		) && lifecycle.next_attack != GaugeBossAttackKind::none;
	}
	return lifecycle.warning_step_before == warning_step_end &&
		lifecycle.warning_step_after == 0U &&
		lifecycle.next_attack != GaugeBossAttackKind::none;
}

[[nodiscard]] bool valid_mrs(
	const assets::MrsImage &image,
	assets::MrsOrientation orientation
) noexcept
{
	const auto &cel = image.cel;
	if(image.orientation != orientation || cel.pixel_width != mrs_width ||
		cel.pixel_height != mrs_height || cel.bitplane_size != mrs_plane_size ||
		cel.offset_at_bottom_left != mrs_bottom_offset ||
		cel.vram_dword_width != (mrs_width / 32U) ||
		cel.row_order != assets::PlanarRowOrder::bottom_to_top ||
		cel.bit_order != assets::PlanarBitOrder::most_significant_bit_left ||
		!cel.alpha || cel.alpha->size() != mrs_plane_size) {
		return false;
	}
	for(const auto &plane : cel.color_planes) {
		if(plane.size() != mrs_plane_size) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] constexpr std::int32_t warning_left(std::size_t player) noexcept
{
	return (player == 0U) ? 44 : 4;
}

[[nodiscard]] constexpr std::uint8_t warning_level(
	GaugeBossAttackKind attack,
	std::uint8_t gauge_level,
	std::uint8_t boss_level
) noexcept
{
	return (attack == GaugeBossAttackKind::boss) ? boss_level : gauge_level;
}

[[nodiscard]] constexpr TramAttribute attack_attribute(
	GaugeBossAttackKind attack
) noexcept
{
	switch(attack) {
	case GaugeBossAttackKind::gauge_pellet:
		return tram_cyan;
	case GaugeBossAttackKind::gauge_bullet:
		return tram_magenta;
	case GaugeBossAttackKind::boss:
		return tram_red;
	case GaugeBossAttackKind::none:
		break;
	}
	return tram_black;
}

template <std::size_t Size>
[[nodiscard]] bool put_gaiji_string(
	Tram &tram,
	TramPoint left,
	const std::array<std::uint8_t, Size> &glyphs,
	TramAttribute attribute
) noexcept
{
	for(const auto glyph : glyphs) {
		if(!tram.put_gaiji(left, glyph, attribute)) {
			return false;
		}
		left.x += 2;
	}
	return true;
}

[[nodiscard]] constexpr Index4 charged_color(std::uint16_t charged) noexcept
{
	if(charged < 64U) {
		return 15U;
	}
	if(charged < 128U) {
		return 10U;
	}
	if(charged < 192U) {
		return 8U;
	}
	if(charged < 255U) {
		return 6U;
	}
	return 12U;
}

} // namespace

MainWarningGaugePresenter::MainWarningGaugePresenter(
	Raster &raster,
	Tram &tram,
	const Sprite16Staging &staging,
	MainPaletteState &palette,
	MainWarningMrsAssets portraits
) noexcept :
	raster_(&raster),
	tram_(&tram),
	palette_(&palette),
	portraits_(portraits)
{
	if(portraits_.images[0U] == nullptr || portraits_.images[1U] == nullptr ||
		!valid_mrs(
			*portraits_.images[0U], assets::MrsOrientation::as_stored
		) || !valid_mrs(
			*portraits_.images[1U],
			assets::MrsOrientation::horizontally_flipped
		) || !palette_->baseline_ready()) {
		status_ = MainWarningGaugeStatus::invalid_assets;
		return;
	}
	if(!staging.committed()) {
		status_ = MainWarningGaugeStatus::staging_not_committed;
	}
}

bool MainWarningGaugePresenter::fail(MainWarningGaugeStatus status) noexcept
{
	if(ready()) {
		status_ = status;
	}
	return false;
}

bool MainWarningGaugePresenter::put_warning_text(
	std::size_t player,
	GaugeBossAttackKind attack,
	std::uint8_t gauge_level,
	std::uint8_t boss_level,
	TramAttribute attribute
) noexcept
{
	const auto left = warning_left(player);
	const auto &label = (attack == GaugeBossAttackKind::boss) ?
		warning_boss : warning_gauge;
	return put_gaiji_string(
		*tram_, {left, 11}, warning_row_one, attribute
	) && put_gaiji_string(
		*tram_, {left, 12}, warning_row_two, attribute
	) && put_gaiji_string(
		*tram_, {left, 13}, warning_row_three, attribute
	) && put_gaiji_string(
		*tram_, {left + 4, 14}, warning_you, attribute
	) && put_gaiji_string(
		*tram_, {left + 6, 15}, label, attribute
	) && tram_->put_gaiji(
		{left + 23, 15},
		static_cast<std::uint8_t>(
			warning_level_glyph_base +
			warning_level(attack, gauge_level, boss_level)
		),
		tram_white
	) && put_gaiji_string(
		*tram_, {left + 4, 16}, warning_peril, attribute
	);
}

bool MainWarningGaugePresenter::clear_warning_text(std::size_t player) noexcept
{
	const auto left = warning_left(player);
	for(auto row = warning_first_row; row <= warning_last_row; ++row) {
		for(auto column = 0; column < warning_clear_width; ++column) {
			if(!tram_->put_cell(
				{left + column, row}, {' ', tram_white}
			)) {
				return false;
			}
		}
	}
	return true;
}

bool MainWarningGaugePresenter::restore_opponent(std::size_t player) noexcept
{
	return palette_->restore_entry_deferred(
		*raster_, static_cast<std::uint8_t>(1U - player)
	);
}

bool MainWarningGaugePresenter::set_warning_partial_palette(
	std::size_t player,
	GaugeBossAttackKind attack
) noexcept
{
	const auto opponent = static_cast<std::uint8_t>(1U - player);
	auto color = raster_->pending_palette().colors[opponent];
	switch(attack) {
	case GaugeBossAttackKind::gauge_pellet:
		color.b = 120U;
		break;
	case GaugeBossAttackKind::gauge_bullet:
		color.r = 120U;
		color.b = 120U;
		break;
	case GaugeBossAttackKind::boss:
		color.r = 120U;
		break;
	case GaugeBossAttackKind::none:
		return false;
	}
	return palette_->set_entry_deferred(*raster_, opponent, color);
}

bool MainWarningGaugePresenter::apply_active_ramp(
	std::size_t player,
	GaugeBossAttackKind active_attack
) noexcept
{
	if(active_attack == GaugeBossAttackKind::none) {
		return true;
	}
	if(!valid_attack(active_attack)) {
		return false;
	}
	if(!ramp_decreasing_[player]) {
		ramp_value_[player] = static_cast<std::uint8_t>(
			ramp_value_[player] + ramp_step
		);
		if(ramp_value_[player] >= ramp_peak) {
			ramp_decreasing_[player] = true;
		}
	} else {
		ramp_value_[player] = static_cast<std::uint8_t>(
			ramp_value_[player] - ramp_step
		);
		if(ramp_value_[player] == 0U) {
			ramp_decreasing_[player] = false;
		}
	}
	const auto opponent = static_cast<std::uint8_t>(1U - player);
	auto color = raster_->pending_palette().colors[opponent];
	color.r = ramp_value_[player];
	return palette_->set_entry_deferred(*raster_, opponent, color);
}

bool MainWarningGaugePresenter::apply_warning_lifecycle(
	std::size_t player,
	const WarningPlayerLifecycle &lifecycle
) noexcept
{
	const auto before = lifecycle.warning_step_before;
	if(before >= warning_step_first) {
		if(before == warning_step_first) {
			// SUB_CB81 resets only the shared accumulator on the first red step.
			// The active-ramp direction byte retains its process value.
			ramp_value_[player] = 0U;
		}
		if(before >= warning_step_end) {
			if(!clear_warning_text(player) || !restore_opponent(player)) {
				return false;
			}
			ramp_value_[player] = 0U;
			ramp_decreasing_[player] = false;
		} else {
			// The red warning and the active attack share byte_20E98. Source
			// applies this signed byte delta before the active +/-4 ramp below.
			ramp_value_[player] = static_cast<std::uint8_t>(
				ramp_value_[player] +
				((before < 18U) ? std::uint8_t{13U} : std::uint8_t{243U})
			);
		}
		if(before < warning_step_end &&
			(lifecycle.warning_step_after & 1U) != 0U) {
			if(!restore_opponent(player) || !put_warning_text(
				player,
				lifecycle.next_attack,
				warning_gauge_level_[player],
				warning_boss_level_,
				attack_attribute(lifecycle.next_attack)
			)) {
				return false;
			}
		} else if(before < warning_step_end && (
			!set_warning_partial_palette(player, lifecycle.next_attack) ||
			!put_warning_text(
				player,
				lifecycle.next_attack,
				warning_gauge_level_[player],
				warning_boss_level_,
				tram_black
			)
		)) {
			return false;
		}
	}
	return apply_active_ramp(player, lifecycle.active_attack);
}

void MainWarningGaugePresenter::warning_interstitial_started(
	const WarningInterstitialStarted &started
) noexcept
{
	if(!ready()) {
		return;
	}
	if(interstitial_active_ ||
		(!started.flashing_players[0U] && !started.flashing_players[1U]) ||
		started.boss_level > main_boss_level_max ||
		(started.sample_count != 0U && started.sample_count != 27U)) {
		(void)fail(MainWarningGaugeStatus::invalid_observation);
		return;
	}
	for(std::size_t player = 0U; player < main_warning_player_count; ++player) {
		if(started.gauge_levels[player] < main_gauge_level_min ||
			started.gauge_levels[player] > main_gauge_level_max ||
			!valid_attack(started.next_attacks[player]) ||
			(started.flashing_players[player] &&
				started.next_attacks[player] == GaugeBossAttackKind::none)) {
			(void)fail(MainWarningGaugeStatus::invalid_observation);
			return;
		}
	}
	warning_boss_level_ = started.boss_level;
	for(std::size_t player = 0U; player < main_warning_player_count; ++player) {
		warning_gauge_level_[player] = started.gauge_levels[player];
		if(started.flashing_players[player]) {
			warning_level_valid_[player] = true;
		}
	}
	for(std::size_t player = 0U; player < main_warning_player_count; ++player) {
		if(!main_hud_gauge_levels_put(
			*tram_, player, started.gauge_levels[player], started.boss_level
		)) {
			(void)fail(MainWarningGaugeStatus::draw_failed);
			return;
		}
	}
	for(std::size_t player = 0U; player < main_warning_player_count; ++player) {
		if(started.flashing_players[player] && !put_warning_text(
			player,
			started.next_attacks[player],
			started.gauge_levels[player],
			started.boss_level,
			tram_white
		)) {
			(void)fail(MainWarningGaugeStatus::draw_failed);
			return;
		}
	}
	interstitial_active_ = started.sample_count != 0U;
	next_interstitial_sample_ = 0U;
}

void MainWarningGaugePresenter::warning_interstitial_sample(
	const WarningInterstitialSample &sample
) noexcept
{
	if(!ready()) {
		return;
	}
	if(!interstitial_active_ || sample.ordinal > 26U ||
		sample.ordinal != next_interstitial_sample_) {
		(void)fail(MainWarningGaugeStatus::invalid_observation);
		return;
	}
	palette_->set_tone_and_show_now(
		*raster_,
		static_cast<std::int16_t>(100 + (50 * (sample.ordinal & 1U)))
	);
	if(sample.ordinal == 26U) {
		interstitial_active_ = false;
		next_interstitial_sample_ = 0U;
		return;
	}
	next_interstitial_sample_++;
}

void MainWarningGaugePresenter::warning_frame_lifecycle(
	const WarningFrameLifecycle &lifecycle
) noexcept
{
	if(!ready()) {
		return;
	}
	if(interstitial_active_ ||
		!valid_warning_lifecycle(lifecycle.players[0U]) ||
		!valid_warning_lifecycle(lifecycle.players[1U]) ||
		lifecycle.gauge_levels[0U] < main_gauge_level_min ||
		lifecycle.gauge_levels[0U] > main_gauge_level_max ||
		lifecycle.gauge_levels[1U] < main_gauge_level_min ||
		lifecycle.gauge_levels[1U] > main_gauge_level_max ||
		lifecycle.boss_level > main_boss_level_max ||
		(lifecycle.players[0U].warning_step_before >= warning_step_first &&
			!warning_level_valid_[0U]) ||
		(lifecycle.players[1U].warning_step_before >= warning_step_first &&
			!warning_level_valid_[1U])) {
		(void)fail(MainWarningGaugeStatus::invalid_observation);
		return;
	}
	warning_gauge_level_ = lifecycle.gauge_levels;
	warning_boss_level_ = lifecycle.boss_level;
	for(std::size_t player = 0U; player < main_warning_player_count; ++player) {
		if(!apply_warning_lifecycle(player, lifecycle.players[player])) {
			(void)fail(MainWarningGaugeStatus::palette_failed);
			return;
		}
		if(lifecycle.players[player].warning_step_before == warning_step_end) {
			warning_level_valid_[player] = false;
		}
	}
}

void MainWarningGaugePresenter::gauge_attack_finished(
	const GaugeAttackFinished &finished
) noexcept
{
	if(!ready()) {
		return;
	}
	if(!valid_player(finished.attacker) || !valid_player(finished.target) ||
		static_cast<std::size_t>(finished.attacker) ==
			static_cast<std::size_t>(finished.target) ||
		static_cast<std::size_t>(finished.target) !=
			(1U - static_cast<std::size_t>(finished.attacker))) {
		(void)fail(MainWarningGaugeStatus::invalid_observation);
		return;
	}
	if(!palette_->restore_entry_deferred(
		*raster_, static_cast<std::uint8_t>(finished.target)
	)) {
		(void)fail(MainWarningGaugeStatus::palette_failed);
	}
}

void MainWarningGaugePresenter::warning_portrait_transition(
	const WarningPortraitTransition &transition
) noexcept
{
	if(!ready()) {
		return;
	}
	if(!valid_player(transition.player)) {
		(void)fail(MainWarningGaugeStatus::invalid_observation);
		return;
	}
	const auto player = static_cast<std::size_t>(transition.player);
	if(!mrs_put_8(
		*raster_,
		static_cast<std::int32_t>(16U + (player * 320U)),
		16U,
		*portraits_.images[player]
	)) {
		(void)fail(MainWarningGaugeStatus::draw_failed);
	}
}

bool MainWarningGaugePresenter::draw_gauge(const MainGaugeState &state) noexcept
{
	for(const auto &player : state.players) {
		if(player.gauge_charged > gauge_max || player.gauge_avail > gauge_max ||
			player.gauge_charged > player.gauge_avail) {
			return false;
		}
	}
	const auto page = raster_->access_page();
	for(std::int32_t y = 0; y < gauge_background_height; ++y) {
		for(std::int32_t x = 0; x < gauge_background_width; ++x) {
			const auto color = raster_->pixel(
				page, {gauge_source_left + x, gauge_source_top + y}
			);
			if(!color.has_value() || !raster_->plot(
				{gauge_p1_left + x, gauge_background_top + y}, *color
			)) {
				return false;
			}
		}
		for(std::int32_t x = 0; x < gauge_background_width; ++x) {
			const auto color = raster_->pixel(
				page, {gauge_source_left + x, gauge_source_top + y}
			);
			if(!color.has_value() || !raster_->plot(
				{gauge_p1_left + 320 + x, gauge_background_top + y}, *color
			)) {
				return false;
			}
		}
	}

	const bool odd_frame = (state.context.round_or_result_frame & 1U) != 0U;
	for(std::size_t player = 0U; player < main_warning_player_count; ++player) {
		const auto &gauge = state.players[player];
		const auto playfield_left = static_cast<std::int32_t>(
			gauge_p1_left + (player * 320U)
		);
		if(gauge.hyper_active != 0U && odd_frame) {
			const auto tier = static_cast<std::uint16_t>(gauge.gauge_avail >> 10U);
			const auto color = static_cast<Index4>(
				(tier == 1U) ? 8U : ((tier == 2U) ? 12U : 6U)
			);
			if(!raster_->clear(
				{
					playfield_left,
					gauge_background_top,
					gauge_background_width,
					gauge_background_height,
				},
				color
			)) {
				return false;
			}
		}
	}

	for(std::size_t player = 0U; player < main_warning_player_count; ++player) {
		const auto &gauge = state.players[player];
		const auto charged = static_cast<std::uint16_t>(
			gauge.gauge_charged >> 4U
		);
		const auto available = static_cast<std::uint16_t>(
			gauge.gauge_avail >> 4U
		);
		const auto uncharged = static_cast<std::uint16_t>(available - charged);
		const auto bar_left = static_cast<std::int32_t>(32U + (player * 320U));
		if(uncharged != 0U) {
			const auto color = static_cast<Index4>(
				(gauge.hyper_active == 0U || odd_frame) ? 9U : 15U
			);
			if(!raster_->clear(
				{
					bar_left + charged,
					gauge_bar_top,
					uncharged,
					gauge_bar_height,
				},
				color
			)) {
				return false;
			}
		}
	}
	for(std::size_t player = 0U; player < main_warning_player_count; ++player) {
		const auto charged = static_cast<std::uint16_t>(
			state.players[player].gauge_charged >> 4U
		);
		if(charged != 0U && !raster_->clear(
			{
				static_cast<std::int32_t>(32U + (player * 320U)),
				gauge_bar_top,
				charged,
				gauge_bar_height,
			},
			charged_color(charged)
		)) {
			return false;
		}
	}
	return true;
}

void MainWarningGaugePresenter::main_gauge_state(
	const MainGaugeState &state
) noexcept
{
	if(!ready()) {
		return;
	}
	if(!draw_gauge(state)) {
		(void)fail(MainWarningGaugeStatus::invalid_observation);
	}
}

MainWarningGaugeStatus MainWarningGaugePresenter::status() const noexcept
{
	return status_;
}

bool MainWarningGaugePresenter::ready() const noexcept
{
	return status_ == MainWarningGaugeStatus::ready;
}

} // namespace th03::present
