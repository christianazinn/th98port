#include "th03/present/main_entities.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

#include "th03/present/main_static.hpp"

namespace th03::present {
namespace {

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
	const MainEntityAtlasLayout &layout
) noexcept
{
	for(const auto &player : layout.shot_cels) {
		for(const auto rect : player) {
			if(!valid_rect(rect, 16U, 8U)) {
				return false;
			}
		}
	}
	for(std::size_t family = 0U; family < main_enemy_family_count; ++family) {
		const auto enemy_size = static_cast<std::uint16_t>((family + 1U) * 16U);
		if(!valid_rect(
			layout.living_enemy_undamaged[family],
			enemy_size,
			static_cast<std::uint16_t>(enemy_size / 2U)
		)) {
			return false;
		}
		const auto explosion_size = static_cast<std::uint16_t>(enemy_size + 16U);
		for(const auto rect : layout.explosion_cels[family]) {
			if(!valid_rect(
				rect,
				explosion_size,
				static_cast<std::uint16_t>(explosion_size / 2U)
			)) {
				return false;
			}
		}
	}
	for(const auto &variant : layout.falling_fireball_cels) {
		for(const auto rect : variant) {
			if(!valid_rect(rect, 32U, 16U)) {
				return false;
			}
		}
	}
	for(const auto &variant : layout.transferring_fireball_cels) {
		for(const auto rect : variant) {
			if(!valid_rect(rect, 16U, 8U)) {
				return false;
			}
		}
	}
	return true;
}

[[nodiscard]] constexpr std::int32_t signed_shift_right_4(
	std::int16_t value
) noexcept
{
	auto result = static_cast<std::int32_t>(value) / 16;
	if(value < 0 && (value % 16) != 0) {
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
		signed_shift_right_4(raw_x) +
		static_cast<std::int32_t>(main_playfield_border) +
		static_cast<std::int32_t>(player * main_playfield_bordered_width) +
		context.playfield_fg_shift_x[player]
	);
}

[[nodiscard]] constexpr std::int32_t foreground_y(
	std::int16_t raw_y
) noexcept
{
	return wrap_signed_word(
		signed_shift_right_4(raw_y) +
		static_cast<std::int32_t>(main_playfield_top)
	);
}

[[nodiscard]] constexpr Point centered_position(
	const MainPhaseContext &context,
	std::int16_t raw_x,
	std::int16_t raw_y,
	std::size_t player,
	std::uint16_t size
) noexcept
{
	const auto radius = static_cast<std::int32_t>(size / 2U);
	return {
		wrap_signed_word(foreground_x(context, raw_x, player) - radius),
		wrap_signed_word(foreground_y(raw_y) - radius),
	};
}

[[nodiscard]] constexpr std::optional<Sprite16AtlasRect> enemy_damage_rect(
	const MainEntityAtlasLayout &layout,
	std::uint8_t size_words,
	std::uint8_t hp
) noexcept
{
	if(size_words == 0U || size_words > main_enemy_family_count) {
		return std::nullopt;
	}
	const auto base = layout.living_enemy_undamaged[size_words - 1U];
	const auto base_pixel =
		(static_cast<std::uint32_t>(base.raw_top) * sprite16_source_width) +
		base.left;
	const auto damage_shift = static_cast<std::uint32_t>(hp) * base.width;
	if(damage_shift > base_pixel) {
		return std::nullopt;
	}
	const auto pixel = base_pixel - damage_shift;
	const auto rect = Sprite16AtlasRect{
		static_cast<std::uint16_t>(pixel % sprite16_source_width),
		static_cast<std::uint16_t>(pixel / sprite16_source_width),
		base.width,
		base.raw_height,
	};
	return valid_rect(rect, base.width, base.raw_height) ?
		std::optional<Sprite16AtlasRect>{rect} : std::nullopt;
}

[[nodiscard]] constexpr std::optional<std::size_t> enemy_family(
	const EnemyFact &enemy
) noexcept
{
	if(enemy.explosion_max_enemy_hits_half == 0U ||
		enemy.explosion_max_enemy_hits_half > main_enemy_family_count) {
		return std::nullopt;
	}
	const auto family = static_cast<std::size_t>(
		enemy.explosion_max_enemy_hits_half - 1U
	);
	const auto expected_size = static_cast<std::uint8_t>((family + 1U) * 16U);
	return (enemy.size_pixels == expected_size) ?
		std::optional<std::size_t>{family} : std::nullopt;
}

[[nodiscard]] constexpr std::optional<std::size_t> enemy_explosion_cel(
	std::uint8_t frame
) noexcept
{
	if(frame < 10U) {
		return 0U;
	}
	if(frame < 32U) {
		return 1U + ((frame / 4U) & 1U);
	}
	if(frame < 36U) {
		return 3U;
	}
	return std::nullopt;
}

[[nodiscard]] constexpr std::optional<std::size_t> fireball_explosion_cel(
	std::uint8_t frame
) noexcept
{
	if(frame < 12U) {
		return 0U;
	}
	if(frame < 32U) {
		return 1U + ((frame / 4U) & 1U);
	}
	if(frame < 36U) {
		return 3U;
	}
	return std::nullopt;
}

[[nodiscard]] constexpr std::optional<std::size_t> fireball_variant_index(
	FireballVariant variant
) noexcept
{
	if(variant == FireballVariant::blue) {
		return 0U;
	}
	if(variant == FireballVariant::red) {
		return 1U;
	}
	return std::nullopt;
}

} // namespace

MainEntityAtlasLayout authentic_main_entity_atlas_layout() noexcept
{
	MainEntityAtlasLayout layout{};
	for(std::size_t cel = 0U; cel < main_shot_cel_count; ++cel) {
		layout.shot_cels[0U][cel] = {
			static_cast<std::uint16_t>(cel * 16U), 0U, 16U, 8U,
		};
		layout.shot_cels[1U][cel] = {
			static_cast<std::uint16_t>(320U + (cel * 16U)), 0U, 16U, 8U,
		};
	}

	layout.living_enemy_undamaged = {{
		{48U, 104U, 16U, 8U},
		{96U, 112U, 32U, 16U},
		{272U, 104U, 48U, 24U},
		{192U, 128U, 64U, 32U},
	}};
	layout.explosion_cels = {{
		{{
			{256U, 128U, 32U, 16U},
			{288U, 128U, 32U, 16U},
			{256U, 144U, 32U, 16U},
			{288U, 144U, 32U, 16U},
		}},
		{{
			{320U, 104U, 48U, 24U},
			{320U, 128U, 48U, 24U},
			{320U, 152U, 48U, 24U},
			{368U, 168U, 48U, 24U},
		}},
		{{
			{368U, 104U, 64U, 32U},
			{368U, 136U, 64U, 32U},
			{432U, 104U, 64U, 32U},
			{432U, 136U, 64U, 32U},
		}},
		{{
			{0U, 160U, 80U, 40U},
			{80U, 160U, 80U, 40U},
			{160U, 160U, 80U, 40U},
			{240U, 160U, 80U, 40U},
		}},
	}};
	layout.falling_fireball_cels = {{
		{{{0U, 80U, 32U, 16U}, {32U, 80U, 32U, 16U}}},
		{{{64U, 80U, 32U, 16U}, {96U, 80U, 32U, 16U}}},
	}};
	layout.transferring_fireball_cels = {{
		{{{0U, 96U, 16U, 8U}, {16U, 96U, 16U, 8U}}},
		{{{32U, 96U, 16U, 8U}, {48U, 96U, 16U, 8U}}},
	}};
	return layout;
}

MainEntitiesPresenter::MainEntitiesPresenter(
	Raster &raster,
	const Sprite16Staging &staging,
	MainEntityAtlasLayout layout
) noexcept :
	raster_(&raster),
	staging_(&staging),
	layout_(layout)
{
	if(!valid_layout(layout_)) {
		status_ = MainEntitiesStatus::invalid_layout;
	}
}

void MainEntitiesPresenter::shot_pair_spawned(
	const ShotPairSpawned &spawn
) noexcept
{
	if(!ready()) {
		return;
	}
	const auto slot = static_cast<std::size_t>(spawn.slot);
	const auto player = player_index(spawn.player_id);
	if(slot >= shot_cels_.size() || !player.has_value()) {
		status_ = MainEntitiesStatus::invalid_observation;
		return;
	}
	shot_cels_[slot] = 0U;
	shot_players_[slot] = spawn.player_id;
	shot_spawn_observed_[slot] = true;
}

void MainEntitiesPresenter::observe(const ShotsEnemiesView &view) noexcept
{
	if(!ready()) {
		return;
	}
	MainShotPairFacts shots{};
	MainEnemyFacts enemies{};
	for(std::size_t slot = 0U; slot < shots.size(); ++slot) {
		const auto fact = view.shot(static_cast<ShotPairSlot>(slot));
		if(!fact.has_value()) {
			status_ = MainEntitiesStatus::invalid_observation;
			return;
		}
		shots[slot] = *fact;
	}
	for(std::size_t slot = 0U; slot < enemies.size(); ++slot) {
		const auto fact = view.enemy(static_cast<EnemySlot>(slot));
		if(!fact.has_value()) {
			status_ = MainEntitiesStatus::invalid_observation;
			return;
		}
		enemies[slot] = *fact;
	}
	(void)present_shots_enemies(view.context(), shots, enemies);
}

void MainEntitiesPresenter::observe(const FireballsView &view) noexcept
{
	if(!ready()) {
		return;
	}
	MainFireballFacts fireballs{};
	for(std::size_t slot = 0U; slot < fireballs.size(); ++slot) {
		const auto fact = view.fireball(static_cast<FireballSlot>(slot));
		if(!fact.has_value()) {
			status_ = MainEntitiesStatus::invalid_observation;
			return;
		}
		fireballs[slot] = *fact;
	}
	(void)present_fireballs(view.context(), fireballs);
}

bool MainEntitiesPresenter::present_shots_enemies(
	const MainPhaseContext &context,
	const MainShotPairFacts &shots,
	const MainEnemyFacts &enemies
) noexcept
{
	if(!ready()) {
		return false;
	}
	for(std::size_t slot = 0U; slot < shots.size(); ++slot) {
		const auto &shot = shots[slot];
		const auto player = player_index(shot.player_id);
		if(static_cast<std::size_t>(shot.slot) != slot ||
			(shot.alive && !player.has_value()) ||
			(shot.alive && shot_spawn_observed_[slot] &&
				shot_players_[slot] != shot.player_id)) {
			status_ = MainEntitiesStatus::invalid_observation;
			return false;
		}
	}
	for(std::size_t slot = 0U; slot < enemies.size(); ++slot) {
		const auto &enemy = enemies[slot];
		if(static_cast<std::size_t>(enemy.slot) != slot) {
			status_ = MainEntitiesStatus::invalid_observation;
			return false;
		}
		const auto classification = enemy.classification();
		if(classification == EnemyClassification::free ||
			classification == EnemyClassification::running_unspawned) {
			continue;
		}
		const auto player = player_index(enemy.player_id);
		const auto family = enemy_family(enemy);
		if(!player.has_value() || !family.has_value()) {
			status_ = MainEntitiesStatus::invalid_observation;
			return false;
		}
		if(classification == EnemyClassification::running_spawned) {
			if(!enemy_damage_rect(
				layout_,
				enemy.explosion_max_enemy_hits_half,
				enemy.hp
			).has_value()) {
				status_ = MainEntitiesStatus::invalid_observation;
				return false;
			}
		} else if(!enemy_explosion_cel(enemy.frame).has_value()) {
			status_ = MainEntitiesStatus::invalid_observation;
			return false;
		}
	}
	if(!staging_->committed()) {
		status_ = MainEntitiesStatus::staging_not_committed;
		return false;
	}

	// shots_render(): ascending slots, left sprite, right sprite, then advance.
	for(std::size_t slot = 0U; slot < shots.size(); ++slot) {
		const auto &shot = shots[slot];
		if(!shot.alive) {
			continue;
		}
		const auto player = *player_index(shot.player_id);
		const auto source = layout_.shot_cels[player][shot_cels_[slot]];
		const auto left = foreground_x(context, shot.topleft_x, player);
		const auto top = foreground_y(shot.topleft_y);
		if(!sprite16_put(
			*raster_, *staging_, source, {left, top}, full_clip
		) || !sprite16_put(
			*raster_,
			*staging_,
			source,
			{wrap_signed_word(left + 16), top},
			full_clip
		)) {
			status_ = MainEntitiesStatus::draw_failed;
			return false;
		}
		shot_cels_[slot] = static_cast<std::uint8_t>(
			(shot_cels_[slot] + 1U) % main_shot_cel_count
		);
	}

	// enemies_render(): ascending slots after every shot pair.
	for(const auto &enemy : enemies) {
		const auto classification = enemy.classification();
		if(classification == EnemyClassification::free ||
			classification == EnemyClassification::running_unspawned) {
			continue;
		}
		const auto player = *player_index(enemy.player_id);
		const auto family = *enemy_family(enemy);
		Sprite16AtlasRect source{};
		std::uint16_t logical_size = enemy.size_pixels;
		if(classification == EnemyClassification::running_spawned) {
			source = *enemy_damage_rect(
				layout_,
				enemy.explosion_max_enemy_hits_half,
				enemy.hp
			);
		} else {
			logical_size = static_cast<std::uint16_t>(logical_size + 16U);
			source = layout_.explosion_cels[family][
				*enemy_explosion_cel(enemy.frame)
			];
		}
		if(!sprite16_put(
			*raster_,
			*staging_,
			source,
			centered_position(
				context,
				enemy.center_x,
				enemy.center_y,
				player,
				logical_size
			),
			playfield_clips[player]
		)) {
			status_ = MainEntitiesStatus::draw_failed;
			return false;
		}
	}
	return true;
}

bool MainEntitiesPresenter::present_fireballs(
	const MainPhaseContext &context,
	const MainFireballFacts &fireballs
) noexcept
{
	if(!ready()) {
		return false;
	}
	for(std::size_t slot = 0U; slot < fireballs.size(); ++slot) {
		const auto &fireball = fireballs[slot];
		if(static_cast<std::size_t>(fireball.slot) != slot) {
			status_ = MainEntitiesStatus::invalid_observation;
			return false;
		}
		const auto classification = fireball.classification();
		if(classification == FireballClassification::free) {
			continue;
		}
		if(!player_index(fireball.player_id).has_value()) {
			status_ = MainEntitiesStatus::invalid_observation;
			return false;
		}
		if(classification == FireballClassification::explosion) {
			if(!fireball_explosion_cel(fireball.frame).has_value()) {
				status_ = MainEntitiesStatus::invalid_observation;
				return false;
			}
		} else if(!fireball_variant_index(fireball.variant).has_value()) {
			status_ = MainEntitiesStatus::invalid_observation;
			return false;
		}
	}
	if(!staging_->committed()) {
		status_ = MainEntitiesStatus::staging_not_committed;
		return false;
	}

	// fireballs_hittest_and_render(): hittest is already complete in core;
	// rendering returns to ascending slot order here.
	for(const auto &fireball : fireballs) {
		const auto classification = fireball.classification();
		if(classification == FireballClassification::free) {
			continue;
		}
		const auto player = *player_index(fireball.player_id);
		Sprite16AtlasRect source{};
		std::uint16_t logical_size = 0U;
		if(classification == FireballClassification::explosion) {
			logical_size = 64U;
			source = layout_.explosion_cels[2U][
				*fireball_explosion_cel(fireball.frame)
			];
		} else {
			const auto variant = *fireball_variant_index(fireball.variant);
			const auto cel = static_cast<std::size_t>(
				((fireball.frame % 8U) <= 3U) ? 1U : 0U
			);
			if(classification == FireballClassification::fall) {
				logical_size = 32U;
				source = layout_.falling_fireball_cels[variant][cel];
			} else {
				logical_size = 16U;
				source = layout_.transferring_fireball_cels[variant][cel];
			}
		}
		if(!sprite16_put(
			*raster_,
			*staging_,
			source,
			centered_position(
				context,
				fireball.center_x,
				fireball.center_y,
				player,
				logical_size
			),
			full_clip
		)) {
			status_ = MainEntitiesStatus::draw_failed;
			return false;
		}
	}
	return true;
}

MainEntitiesStatus MainEntitiesPresenter::status() const noexcept
{
	return status_;
}

bool MainEntitiesPresenter::ready() const noexcept
{
	return status_ == MainEntitiesStatus::ready;
}

} // namespace th03::present
