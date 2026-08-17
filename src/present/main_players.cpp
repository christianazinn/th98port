#include "th03/present/main_players_bullets.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

#include "th03/present/main_static.hpp"

namespace th03::present {
namespace {

[[nodiscard]] bool no_middle_pass(
	void *,
	const MainPhaseContext &
) noexcept
{
	return true;
}

constexpr std::uint16_t player_sprite_width = 32U;
// player_render() assigns sprite16_put_size.h directly rather than calling
// set(), so this value is already in raw 200-line rows.
constexpr std::uint16_t player_sprite_raw_height = 32U;
constexpr std::uint16_t defeat_sprite_width = 48U;
constexpr std::uint16_t defeat_sprite_raw_height = 24U;
constexpr std::uint16_t bullet16_width = 16U;
constexpr std::uint16_t bullet16_raw_height = 8U;
constexpr std::uint16_t cloud_width = 32U;
constexpr std::uint16_t cloud_raw_height = 16U;
constexpr std::uint8_t character_glow_glyph_count = 9U;
constexpr std::uint8_t gauge_glyph_count = 16U;
constexpr std::uint16_t first_sheet_glyph = 0U;
constexpr std::uint16_t q12_4_one_pixel = 16U;
constexpr std::uint16_t gauge_visible_min = 0x0100U;
constexpr std::uint16_t gauge_64 = 64U * q12_4_one_pixel;
constexpr std::uint16_t gauge_128 = 128U * q12_4_one_pixel;
constexpr std::uint16_t gauge_max = 0x00FFU * q12_4_one_pixel;
constexpr std::uint8_t defeat_initial_time = 0x30U;
constexpr std::uint8_t defeated_time = 0xFFU;
constexpr std::size_t trig_entry_count = 256U;

constexpr Sprite16HorizontalClip full_clip{
	0,
	static_cast<std::int32_t>(graphics_width - 1U),
};
constexpr std::array<Sprite16HorizontalClip, 2U> playfield_clips{{
	{0, static_cast<std::int32_t>(main_playfield_bordered_width - 1U)},
	{
		static_cast<std::int32_t>(main_playfield_bordered_width),
		static_cast<std::int32_t>(
			(main_playfield_bordered_width * 2U) - 1U
		),
	},
}};

enum class PlayerGlow : std::uint8_t {
	none = 0U,
	invincible = 1U,
	hyper = 2U,
};

enum class GaugeRing : std::uint8_t {
	below_64,
	below_128,
	below_maximum,
	maximum,
};

[[nodiscard]] constexpr std::optional<std::size_t> player_index(
	PlayerId player
) noexcept
{
	if(player == PlayerId::p1) {
		return 0U;
	}
	if(player == PlayerId::p2) {
		return 1U;
	}
	return std::nullopt;
}

[[nodiscard]] constexpr bool valid_rect(
	Sprite16AtlasRect rect,
	std::uint16_t width,
	std::uint16_t raw_height
) noexcept
{
	return rect.width == width && rect.raw_height == raw_height &&
		(rect.left % 16U) == 0U &&
		(static_cast<std::uint32_t>(rect.left) + rect.width <=
			sprite16_source_width) &&
		(static_cast<std::uint32_t>(rect.raw_top) + rect.raw_height <=
			sprite16_source_height);
}

[[nodiscard]] constexpr bool valid_layout(
	const MainPlayersBulletsAtlasLayout &layout
) noexcept
{
	for(const auto &player : layout.player_pose_cels) {
		for(const auto rect : player) {
			if(!valid_rect(
				rect, player_sprite_width, player_sprite_raw_height
			)) {
				return false;
			}
		}
	}
	if(!valid_rect(
		layout.defeat_ring, defeat_sprite_width, defeat_sprite_raw_height
	)) {
		return false;
	}
	for(const auto &player : layout.default_bullet16_cels) {
		for(const auto rect : player) {
			if(!valid_rect(rect, bullet16_width, bullet16_raw_height)) {
				return false;
			}
		}
	}
	for(const auto &player : layout.kana_bullet16_cels) {
		for(const auto rect : player) {
			if(!valid_rect(rect, bullet16_width, bullet16_raw_height)) {
				return false;
			}
		}
	}
	for(const auto rect : layout.pellet_cloud_cels) {
		if(!valid_rect(rect, cloud_width, cloud_raw_height)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] constexpr bool valid_character(
	MainPalettedCharacter character
) noexcept
{
	return character >= MainPalettedCharacter::reimu_primary &&
		character <= MainPalettedCharacter::yumemi_alternate;
}

[[nodiscard]] bool exact_sheet_geometry(
	const assets::BfntSheet &sheet,
	std::uint16_t width,
	std::uint16_t height,
	std::uint16_t glyph_count
) noexcept
{
	return sheet.consumer == assets::BfntConsumerPolicy::superimpose &&
		sheet.header.pixel_width == width &&
		sheet.header.pixel_height == height &&
		sheet.header.first_code == first_sheet_glyph &&
		sheet.header.last_code == static_cast<std::uint16_t>(glyph_count - 1U) &&
		sheet.glyphs.size() == glyph_count;
}

[[nodiscard]] constexpr std::int32_t signed_shift_right(
	std::int32_t value,
	unsigned int bits
) noexcept
{
	const auto divisor = static_cast<std::int32_t>(1U << bits);
	auto result = value / divisor;
	if(value < 0 && (value % divisor) != 0) {
		--result;
	}
	return result;
}

[[nodiscard]] constexpr std::int32_t wrap_signed_word(
	std::int32_t value
) noexcept
{
	const auto wrapped = static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(value) & 0xFFFFU
	);
	return (wrapped <= 0x7FFFU) ?
		static_cast<std::int32_t>(wrapped) :
		(static_cast<std::int32_t>(wrapped) - 0x10000);
}

[[nodiscard]] constexpr std::int32_t foreground_x(
	const MainPhaseContext &context,
	std::int16_t raw_x,
	std::size_t player
) noexcept
{
	return wrap_signed_word(
		signed_shift_right(raw_x, 4U) +
		static_cast<std::int32_t>(main_playfield_border) +
		static_cast<std::int32_t>(player * main_playfield_bordered_width) +
		context.playfield_fg_shift_x[player]
	);
}

[[nodiscard]] constexpr std::int16_t wrapped_word(std::int32_t value) noexcept
{
	return static_cast<std::int16_t>(wrap_signed_word(value));
}

[[nodiscard]] constexpr std::int32_t polar(
	std::int16_t center,
	std::int16_t radius,
	std::int16_t ratio
) noexcept
{
	const auto product = static_cast<std::int32_t>(radius) * ratio;
	return wrap_signed_word(center + signed_shift_right(product, 8U));
}

[[nodiscard]] PlayerGlow player_glow(
	Page source_page_back,
	const PlayerFact &player
) noexcept
{
	if(source_page_back == Page::zero) {
		return PlayerGlow::none;
	}
	if(player.hyper_active != 0U) {
		return PlayerGlow::hyper;
	}
	return (player.invincibility_time != 0U) ?
		PlayerGlow::invincible : PlayerGlow::none;
}

[[nodiscard]] constexpr GaugeRing gauge_ring(std::uint16_t gauge) noexcept
{
	if(gauge < gauge_64) {
		return GaugeRing::below_64;
	}
	if(gauge < gauge_128) {
		return GaugeRing::below_128;
	}
	if(gauge < gauge_max) {
		return GaugeRing::below_maximum;
	}
	return GaugeRing::maximum;
}

} // namespace

MainPlayersBulletsAtlasLayout
authentic_main_players_bullets_atlas_layout() noexcept
{
	MainPlayersBulletsAtlasLayout layout{};
	for(std::size_t pose = 0U; pose < main_player_pose_count; ++pose) {
		layout.player_pose_cels[0U][pose] = {
			static_cast<std::uint16_t>(544U + (pose * player_sprite_width)),
			128U,
			player_sprite_width,
			player_sprite_raw_height,
		};
		layout.player_pose_cels[1U][pose] = {
			static_cast<std::uint16_t>(544U + (pose * player_sprite_width)),
			160U,
			player_sprite_width,
			player_sprite_raw_height,
		};
	}
	layout.defeat_ring = {
		416U, 168U, defeat_sprite_width, defeat_sprite_raw_height,
	};
	for(std::size_t player = 0U;
		player < PlayersBulletsView::player_count;
		++player) {
		for(std::size_t cel = 0U; cel < main_bullet16_cel_count; ++cel) {
			layout.default_bullet16_cels[player][cel] = {
				static_cast<std::uint16_t>(
					64U + (player * main_playfield_bordered_width) +
					(cel * bullet16_width)
				),
				0U,
				bullet16_width,
				bullet16_raw_height,
			};
			layout.kana_bullet16_cels[player][cel] = {
				static_cast<std::uint16_t>(
					(player * main_playfield_bordered_width) +
					(cel * bullet16_width)
				),
				72U,
				bullet16_width,
				bullet16_raw_height,
			};
		}
	}
	for(std::size_t cel = 0U; cel < main_pellet_cloud_cel_count; ++cel) {
		layout.pellet_cloud_cels[cel] = {
			static_cast<std::uint16_t>(528U + (cel * cloud_width)),
			80U,
			cloud_width,
			cloud_raw_height,
		};
	}
	return layout;
}

MainPlayersBulletsPresenter::MainPlayersBulletsPresenter(
	Raster &raster,
	const Sprite16Staging &staging,
	MainPlayersBulletsAtlasLayout layout,
	MainPlayerOverlayAssets overlays,
	PelletMasks pellet_masks,
	StaffFlakeTrigTables trig
) :
	raster_(&raster),
	staging_(&staging),
	layout_(layout),
	loaded_characters_(overlays.loaded_characters),
	pellet_masks_(pellet_masks),
	trig_(trig)
{
	if(!valid_layout(layout_)) {
		status_ = MainPlayersBulletsStatus::invalid_layout;
		return;
	}
	if(overlays.gauge_sheet == nullptr ||
		!exact_sheet_geometry(
			*overlays.gauge_sheet, 32U, 16U, gauge_glyph_count
		) || pellet_masks_.rows.size() != pellet_mask_bytes ||
		pellet_masks_.row_order != PackedRowOrder::top_to_bottom ||
		trig_.sine_q8.size() != trig_entry_count ||
		trig_.cosine_q8.size() != trig_entry_count) {
		status_ = MainPlayersBulletsStatus::invalid_assets;
		return;
	}
	for(std::size_t player = 0U; player < overlays.character_sheets.size();
		++player) {
		const auto *sheet = overlays.character_sheets[player];
		if(sheet == nullptr || !valid_character(loaded_characters_[player]) ||
			!exact_sheet_geometry(
				*sheet, 32U, 32U, character_glow_glyph_count
			)) {
			status_ = MainPlayersBulletsStatus::invalid_assets;
			return;
		}
		prepared_character_sheets_[player] = prepare_superimpose_sheet(*sheet);
		if(!prepared_character_sheets_[player].has_value()) {
			status_ = MainPlayersBulletsStatus::invalid_assets;
			return;
		}
	}
	prepared_gauge_sheet_ = prepare_superimpose_sheet(*overlays.gauge_sheet);
	if(!prepared_gauge_sheet_.has_value()) {
		status_ = MainPlayersBulletsStatus::invalid_assets;
	}
}

void MainPlayersBulletsPresenter::player_pose_committed(
	const PlayerPoseCommitted &pose
) noexcept
{
	if(!ready()) {
		return;
	}
	const auto player = player_index(pose.player);
	if(!player.has_value()) {
		status_ = MainPlayersBulletsStatus::invalid_observation;
		return;
	}
	if(pose.final_velocity_x_write_q4_4.has_value()) {
		shared_velocity_x_q4_4_ = *pose.final_velocity_x_write_q4_4;
	}
	if(shared_velocity_x_q4_4_ == 0) {
		player_poses_[*player] = PlayerPose::neutral;
	} else if(shared_velocity_x_q4_4_ < 0) {
		player_poses_[*player] = PlayerPose::left;
	} else {
		player_poses_[*player] = PlayerPose::right;
	}
}

void MainPlayersBulletsPresenter::accelerator_shared_velocity_restore(
	std::int8_t velocity_x_q4_4
) noexcept
{
	shared_velocity_x_q4_4_ = velocity_x_q4_4;
}

void MainPlayersBulletsPresenter::player_defeat_step(
	const PlayerDefeatStep &step
) noexcept
{
	if(!ready()) {
		return;
	}
	const auto player = player_index(step.player);
	if(!player.has_value() || step.lose_anim_time_before == 0U ||
		step.lose_anim_time_before > defeat_initial_time) {
		status_ = MainPlayersBulletsStatus::invalid_observation;
		return;
	}

	if(step.lose_anim_time_before == defeat_initial_time) {
		defeat_.anchor_left = wrapped_word(
			signed_shift_right(step.center_x, 4U) +
			static_cast<std::int32_t>(main_playfield_border) +
			static_cast<std::int32_t>(
				*player * main_playfield_bordered_width
			) + step.fg_shift_x_before_defeat_shake - 24
		);
		defeat_.anchor_top = wrapped_word(
			signed_shift_right(step.center_y, 4U) - 8
		);
		defeat_.radius = 0;
	} else {
		defeat_.radius = wrapped_word(defeat_.radius + 9);
	}

	// The source clears the one shared record on the decrement-to-zero tick.
	if(step.lose_anim_time_before == 1U) {
		defeat_ = {};
	}
}

void MainPlayersBulletsPresenter::observe(
	const PlayersBulletsView &view
) noexcept
{
	if(!ready()) {
		return;
	}
	MainPlayerFacts players{};
	MainBulletFacts bullets{};
	MainBulletTrails trails{};
	for(std::size_t player = 0U; player < players.size(); ++player) {
		const auto fact = view.player(static_cast<PlayerId>(player));
		if(!fact.has_value()) {
			status_ = MainPlayersBulletsStatus::invalid_observation;
			return;
		}
		players[player] = *fact;
	}
	for(std::size_t slot = 0U; slot < bullets.size(); ++slot) {
		const auto fact = view.bullet(static_cast<BulletSlot>(slot));
		if(!fact.has_value()) {
			status_ = MainPlayersBulletsStatus::invalid_observation;
			return;
		}
		bullets[slot] = *fact;
		if(!fact->flag.is_bullet16() || !fact->has_trail) {
			continue;
		}
		for(std::size_t point = 0U; point < trails[slot].size(); ++point) {
			const auto position = view.trail_point(
				static_cast<BulletSlot>(slot),
				static_cast<TrailPoint>(point)
			);
			if(!position.has_value()) {
				status_ = MainPlayersBulletsStatus::invalid_observation;
				return;
			}
			trails[slot][point] = *position;
		}
	}
	(void)present_phase(view.context(), players, bullets, trails);
}

bool MainPlayersBulletsPresenter::validate_players(
	const MainPlayerFacts &players
) noexcept
{
	for(std::size_t player = 0U; player < players.size(); ++player) {
		const auto &fact = players[player];
		const bool valid_time = fact.lose_anim_time <= defeat_initial_time ||
			fact.lose_anim_time == defeated_time;
		if(static_cast<std::size_t>(fact.player_id) != player || !valid_time ||
			fact.paletted_character != static_cast<std::int8_t>(
				loaded_characters_[player]
			)) {
			status_ = MainPlayersBulletsStatus::invalid_observation;
			return false;
		}
	}
	return true;
}

bool MainPlayersBulletsPresenter::draw_players(
	const MainPhaseContext &context,
	const MainPlayerFacts &players,
	std::optional<Page> source_page_back
) noexcept
{
	const auto page_back = source_page_back.value_or(raster_->access_page());
	auto draw_defeat_ring = [this, &context](std::size_t player) noexcept {
		auto draw = [this, &context, player](
			std::uint8_t angle_cos,
			std::uint8_t angle_sin
		) noexcept {
			const auto left = wrap_signed_word(
				polar(
					defeat_.anchor_left,
					defeat_.radius,
					trig_.cosine_q8[angle_cos]
				) + context.playfield_fg_shift_x[player]
			);
			const auto top = polar(
				defeat_.anchor_top,
				defeat_.radius,
				trig_.sine_q8[angle_sin]
			);
			return sprite16_put(
				*raster_,
				*staging_,
				layout_.defeat_ring,
				{left, top},
				playfield_clips[player]
			);
		};

		const auto base = static_cast<std::uint8_t>(defeat_.radius / 4);
		auto angle = base;
		for(std::size_t sprite = 0U; sprite < 16U; ++sprite) {
			if(!draw(angle, static_cast<std::uint8_t>(angle + 0x20U))) {
				return false;
			}
			angle = static_cast<std::uint8_t>(angle + 0x10U);
		}
		angle = static_cast<std::uint8_t>(0U - base);
		for(std::size_t sprite = 0U; sprite < 16U; ++sprite) {
			if(!draw(angle, static_cast<std::uint8_t>(angle + 224U))) {
				return false;
			}
			angle = static_cast<std::uint8_t>(angle + 0x10U);
		}
		angle = base;
		for(std::size_t sprite = 0U; sprite < 16U; ++sprite) {
			if(!draw(angle, angle)) {
				return false;
			}
			angle = static_cast<std::uint8_t>(angle + 0x10U);
		}
		return true;
	};

	// player_render(): P1 base/defeat, then P2 base/defeat.
	for(std::size_t player = 0U; player < players.size(); ++player) {
		const auto &fact = players[player];
		if(fact.lose_anim_time != 0U) {
			if(fact.lose_anim_time != defeated_time &&
				!draw_defeat_ring(player)) {
				return false;
			}
			continue;
		}
		if(player_glow(page_back, fact) != PlayerGlow::none) {
			continue;
		}
		const auto pose = static_cast<std::size_t>(player_poses_[player]);
		const Point position{
			wrap_signed_word(foreground_x(context, fact.center_x, player) - 16),
			wrap_signed_word(signed_shift_right(fact.center_y, 4U) - 16),
		};
		if(!sprite16_put(
			*raster_,
			*staging_,
			layout_.player_pose_cels[player][pose],
			position,
			full_clip
		)) {
			return false;
		}
	}

	// player_overlay_render(): P1 glow/gauge, then P2 glow/gauge.
	for(std::size_t player = 0U; player < players.size(); ++player) {
		const auto &fact = players[player];
		if(fact.lose_anim_time != 0U) {
			continue;
		}
		const auto left = wrap_signed_word(
			foreground_x(context, fact.center_x, player) - 16
		);
		const auto glow = player_glow(page_back, fact);
		if(glow != PlayerGlow::none) {
			const auto code = static_cast<std::uint16_t>(
				(static_cast<std::size_t>(player_poses_[player]) * 3U) +
				static_cast<std::size_t>(glow)
			);
			if(!superimpose_put(
				*raster_,
				*prepared_character_sheets_[player],
				{left, signed_shift_right(fact.center_y, 5U) - 8},
				code
			)) {
				return false;
			}
		}
		if(fact.gauge_charged < gauge_visible_min) {
			continue;
		}
		const auto code = static_cast<std::uint16_t>(
			(static_cast<std::size_t>(gauge_ring(fact.gauge_charged)) * 4U) +
			((context.round_or_result_frame >> 2U) & 3U)
		);
		if(!superimpose_put(
			*raster_,
			*prepared_gauge_sheet_,
			{left, signed_shift_right(fact.center_y, 5U) - 15},
			code
		)) {
			return false;
		}
	}
	return true;
}

bool MainPlayersBulletsPresenter::present_players(
	const MainPhaseContext &context,
	const MainPlayerFacts &players
) noexcept
{
	if(!ready() || !validate_players(players)) {
		return false;
	}
	if(!staging_->committed()) {
		status_ = MainPlayersBulletsStatus::staging_not_committed;
		return false;
	}
	if(!draw_players(context, players, std::nullopt)) {
		status_ = MainPlayersBulletsStatus::draw_failed;
		return false;
	}
	return true;
}

bool MainPlayersBulletsPresenter::present_bullets(
	const MainPhaseContext &context,
	const MainBulletFacts &bullets,
	const MainBulletTrails &trails
) noexcept
{
	if(!ready() || !validate_bullets(bullets, trails)) {
		return false;
	}
	if(!staging_->committed()) {
		status_ = MainPlayersBulletsStatus::staging_not_committed;
		return false;
	}
	if(!draw_bullets(context, bullets, trails)) {
		status_ = MainPlayersBulletsStatus::draw_failed;
		return false;
	}
	return true;
}

bool MainPlayersBulletsPresenter::present_phase(
	const MainPhaseContext &context,
	const MainPlayerFacts &players,
	const MainBulletFacts &bullets,
	const MainBulletTrails &trails
) noexcept
{
	return present_phase_with_middle(
		context,
		players,
		bullets,
		trails,
		no_middle_pass,
		nullptr
	);
}

bool MainPlayersBulletsPresenter::present_phase_with_middle(
	const MainPhaseContext &context,
	const MainPlayerFacts &players,
	const MainBulletFacts &bullets,
	const MainBulletTrails &trails,
	MainPlayersBulletsMiddlePass middle,
	void *middle_context,
	std::optional<Page> source_page_back
) noexcept
{
	if(!ready()) {
		return false;
	}
	if(middle == nullptr) {
		status_ = MainPlayersBulletsStatus::invalid_observation;
		return false;
	}
	if(!validate_players(players) || !validate_bullets(bullets, trails)) {
		return false;
	}
	if(!staging_->committed()) {
		status_ = MainPlayersBulletsStatus::staging_not_committed;
		return false;
	}
	if(!draw_players(context, players, source_page_back)) {
		status_ = MainPlayersBulletsStatus::draw_failed;
		return false;
	}
	// A failed inserted owner reports its own status; do not misdiagnose this
	// child as a raster failure.
	if(!middle(middle_context, context)) {
		return false;
	}
	if(!draw_bullets(context, bullets, trails)) {
		status_ = MainPlayersBulletsStatus::draw_failed;
		return false;
	}
	return true;
}

MainPlayersBulletsStatus MainPlayersBulletsPresenter::status() const noexcept
{
	return status_;
}

bool MainPlayersBulletsPresenter::ready() const noexcept
{
	return status_ == MainPlayersBulletsStatus::ready;
}

} // namespace th03::present
