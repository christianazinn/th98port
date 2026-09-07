#include "th03/core/game.hpp"

#include "game_internal.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

namespace th03 {
namespace {

constexpr std::array<std::byte, 8U> snapshot_magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'R'}, std::byte{'B'},
	std::byte{'K'}, std::byte{'0'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::uint16_t snapshot_version = 1U;
constexpr std::uint16_t snapshot_header_size = 32U;
constexpr std::uint32_t snapshot_simulation_flag = 1U;

template <typename Value>
using Bare = std::remove_cv_t<std::remove_reference_t<Value>>;

template <typename Value>
struct IsArray : std::false_type {};

template <typename Value, std::size_t Count>
struct IsArray<std::array<Value, Count>> : std::true_type {};

template <typename Value>
struct IsOptional : std::false_type {};

template <typename Value>
struct IsOptional<std::optional<Value>> : std::true_type {};

template <typename Value>
struct IsVector : std::false_type {};

template <typename Value, typename Allocator>
struct IsVector<std::vector<Value, Allocator>> : std::true_type {};

template <typename Value>
struct FixedTraits;

template <std::integral Rep, unsigned FractionBits>
struct FixedTraits<exact::Fixed<Rep, FractionBits>> {
	using rep = Rep;
};

template <typename Value>
concept FixedValue = requires(Value value) {
	typename FixedTraits<Bare<Value>>::rep;
	{ value.raw() } -> std::same_as<typename FixedTraits<Bare<Value>>::rep>;
};

#define TH03_RBK_CODEC(Type, ...) \
	template <typename Archive, typename Value> \
	requires std::same_as<Bare<Value>, Type> \
	bool codec(Archive &archive, Value &value) noexcept \
	{ \
		return archive(__VA_ARGS__); \
	}

TH03_RBK_CODEC(PlayerStart,
	value.center_x, value.center_y, value.halfhearts,
	value.invincibility_time, value.gauge_charge_speed,
	value.gauge_charged, value.gauge_avail, value.bombs,
	value.shot_active, value.cpu_frame
)

TH03_RBK_CODEC(RuntimeSnapshot,
	value.random_seed, value.randring, value.randring_cursor,
	value.formation_type_ring, value.formation_position_ring,
	value.formation_cursor, value.cpu_charge_ring,
	value.cpu_charge_cursor, value.players
)

TH03_RBK_CODEC(CompactRuntimeSnapshot,
	value.round_reset_seed, value.random_seed, value.first_formation,
	value.players
)

TH03_RBK_CODEC(RoundStateSnapshot,
	value.round_id, value.rounds_won, value.score, value.round_speed,
	value.bullet_base_speed, value.gauge_attack_level,
	value.boss_attack_level, value.cpu_hit_damage_additional,
	value.extends_gained, value.cpu_safety_frames
)

TH03_RBK_CODEC(ComboCarrySnapshot,
	value.time, value.hits_highest, value.bonus_total
)

TH03_RBK_CODEC(RoundCarrySnapshot,
	value.cpu_dodge_strategy, value.human_movement_last,
	value.spell_ready_frames, value.combo_bonus_max,
	value.combo_hits_max, value.shot_cycle, value.boss_panic_fired,
	value.gauge_attack_next, value.cpu_shot_decision,
	value.background_phase, value.combo, value.chain_cursor,
	value.chain_hits, value.chain_pellet_and_fireball_value,
	value.chain_charge_fireball, value.chain_charge_exatt,
	value.randring_cursor, value.result_phase,
	value.fireball_generation_previous
)

TH03_RBK_CODEC(RoundStartSnapshot, value.state, value.carry)

TH03_RBK_CODEC(PracticeRules,
	value.uses_stock, value.retry_cpu_timer, value.retry_cpu_damage
)

TH03_RBK_CODEC(GameInit,
	value.global_frame, value.resident_rand, value.random_seed,
	value.game_mode, value.rank, value.key_mode, value.story_stage,
	value.story_lives, value.remaining_credits, value.skill,
	value.demo_number, value.is_cpu, value.paletted_character,
	value.story_opponents, value.winner, value.show_score_menu,
	value.fast_op_animation, value.last_score, value.autofire_mask,
	value.runtime_snapshot, value.compact_runtime_snapshot,
	value.round_start_snapshot, value.practice_rules
)

TH03_RBK_CODEC(RoundStartObservation,
	value.start, value.effective_reset_seed, value.runtime, value.round
)

TH03_RBK_CODEC(CanonicalState,
	value.route, value.game_mode, value.story_stage, value.round_id,
	value.winner, value.round_speed, value.global_frame,
	value.round_frame, value.round_or_result_frame, value.p1_score,
	value.p2_score, value.resident_rand, value.state_hash
)

TH03_RBK_CODEC(detail::PlayerState,
	value.center_x, value.center_y, value.is_hit, value.halfhearts,
	value.invincibility_time, value.shot_mode, value.knockback_time,
	value.move_lock_time, value.knockback_angle, value.knockback_length,
	value.knockback_active, value.gauge_charged, value.gauge_avail,
	value.bombs, value.hyper_active, value.hyper_character_callback,
	value.hyper_effect_active_this_frame, value.lose_anim_time,
	value.rounds_won, value.cpu_frame, value.hit_damage_next,
	value.shot_active, value.cpu_dodge_strategy,
	value.human_movement_last, value.spell_ready_frames,
	value.combo_bonus_max, value.combo_hits_max,
	value.gauge_attacks_fired, value.boss_attacks_fired,
	value.boss_attacks_reversed, value.boss_panics_fired,
	value.is_cpu, value.paletted_character, value.gauge_charge_speed,
	value.cpu_safety_frames, value.speed_aligned_x,
	value.speed_aligned_y, value.speed_diagonal_x,
	value.speed_diagonal_y, value.reimu_chargeshot_active,
	value.reimu_chargeshot_age, value.cpu_charge_ring,
	value.cpu_charge_cursor
)

TH03_RBK_CODEC(detail::KotohimeChargeState,
	value.active, value.x, value.y, value.velocity_y
)

TH03_RBK_CODEC(detail::YumemiChargeState,
	value.state, value.frame, value.x, value.y
)

TH03_RBK_CODEC(detail::ComboState,
	value.time, value.hits_highest, value.bonus_total
)

TH03_RBK_CODEC(detail::ChainState,
	value.cursor, value.hits, value.pellet_and_fireball_value,
	value.charge_fireball, value.charge_exatt
)

TH03_RBK_CODEC(detail::EnemyState,
	value.flag, value.frame, value.center_x, value.center_y, value.hp,
	value.player_id, value.size_pixels, value.script_ip,
	value.script_op_frame, value.velocity_x, value.velocity_y,
	value.angle, value.angle_speed, value.formation_type,
	value.formation_index, value.speed, value.loop_index,
	value.position_type, value.chain_slot,
	value.explosion_max_enemy_hits_half
)

TH03_RBK_CODEC(detail::ShotPairState,
	value.alive, value.topleft_x, value.topleft_y, value.velocity_y,
	value.player_id
)

TH03_RBK_CODEC(detail::ReimuChargeGroup,
	value.state, value.frame, value.x, value.y, value.angle, value.speed
)

TH03_RBK_CODEC(detail::MimaChargeNode,
	value.active, value.visual_frame, value.x, value.y,
	value.velocity_x, value.velocity_y
)

TH03_RBK_CODEC(detail::MarisaChargeState,
	value.active, value.age, value.x, value.y
)

TH03_RBK_CODEC(detail::KanaChargeGroup,
	value.x, value.y, value.angle, value.speed
)

TH03_RBK_CODEC(detail::RikakoChargeNode,
	value.x, value.y, value.angle
)

TH03_RBK_CODEC(detail::ChiyuriChargeNode,
	value.state, value.frame, value.x, value.y
)

TH03_RBK_CODEC(detail::EllenChargeNode,
	value.state, value.phase, value.angle, value.speed, value.x,
	value.y, value.velocity_x, value.velocity_y
)

TH03_RBK_CODEC(detail::EllenBombVector,
	value.x, value.y, value.velocity_x, value.velocity_y
)

TH03_RBK_CODEC(detail::BulletState,
	value.flag, value.age, value.center_x, value.center_y,
	value.target_x_for_target_player, value.target_x_for_origin_player,
	value.velocity_x, value.velocity_y, value.speed_next,
	value.angle_next, value.player_id, value.group_next,
	value.accel_type, value.is_collidable, value.has_trail,
	value.trail_index
)

TH03_RBK_CODEC(detail::BulletTrailState, value.center_x, value.center_y)

TH03_RBK_CODEC(detail::FireballState,
	value.flag, value.frame, value.center_x, value.center_y,
	value.variant_as_explosion_target, value.hp, value.player_id,
	value.size_pixels, value.velocity_x, value.velocity_y,
	value.target_x_for_target_player, value.target_x_for_origin_player,
	value.fall_velocity_y, value.chain_slot, value.generation
)

TH03_RBK_CODEC(detail::ExtraAttackState,
	value.state, value.frame, value.center_x, value.center_y,
	value.velocity_x, value.velocity_y, value.target_x_for_target_player,
	value.target_x_for_origin_player, value.player_id, value.angle,
	value.speed, value.behavior_variant, value.mima_timer,
	value.yumemi_extent, value.yumemi_duration, value.kotohime_target_y,
	value.kotohime_velocity_y, value.kotohime_milestone,
	value.ellen_trail_x, value.ellen_trail_y, value.ellen_radius,
	value.ellen_angle_delta
)

TH03_RBK_CODEC(detail::BossInitialState,
	value.center_x, value.center_y, value.velocity_x, value.velocity_y,
	value.hp, value.state, value.motion_angle, value.pattern_index,
	value.render_state
)

TH03_RBK_CODEC(detail::BossState,
	value.center_x, value.center_y, value.velocity_x, value.velocity_y,
	value.hp, value.state, value.motion_angle, value.pattern_index,
	value.render_state, value.hit, value.pattern_previous,
	value.pattern_count, value.pattern_frame, value.anchor_x,
	value.anchor_y, value.orbit_x, value.orbit_y, value.params,
	value.render_frame, value.direction, value.angle_delta,
	value.pattern_angle
)

TH03_RBK_CODEC(detail::DeterministicState,
	value.game_mode, value.rank, value.story_stage, value.story_lives,
	value.skill, value.demo_number, value.demo_frame,
	value.extends_gained, value.practice_uses_stock,
	value.three_digit_combos, value.resident_rand, value.winner,
	value.last_fatal_player, value.round_id, value.round_frame,
	value.round_or_result_frame, value.round_speed, value.defeat_flag,
	value.input_player, value.input_system, value.score, value.players,
	value.random_seed, value.randring, value.randring_cursor,
	value.formation_type_ring, value.formation_position_ring,
	value.formation_cursor, value.autofire_shot_gate,
	value.cpu_target_kind_scratch, value.boss_panic_fired_in_combo,
	value.bomb_flag, value.bomb_frame, value.ellen_bomb_vectors,
	value.ellen_bomb_vector_cursor, value.playfield_fg_shift_x,
	value.damage_all_on, value.cpu_hit_damage_additional,
	value.warning_flag, value.gba_flag_next, value.gba_flag_active,
	value.gba_gauge_level, value.reimu_gauge_pattern_frame,
	value.mima_gauge_pattern_frame, value.mima_gauge_pattern_angle,
	value.mima_gauge_pattern_angle_delta,
	value.marisa_gauge_pattern_frame, value.marisa_gauge_pattern_y,
	value.ellen_gauge_pattern_frame, value.ellen_gauge_pattern_x,
	value.ellen_gauge_pattern_y, value.kotohime_gauge_pattern_frame,
	value.kotohime_gauge_pattern_type, value.kana_gauge_pattern_frame,
	value.kana_gauge_pattern_x, value.rikako_gauge_pattern_frame,
	value.chiyuri_gauge_pattern_frame, value.chiyuri_gauge_pattern_x,
	value.yumemi_gauge_pattern_frame, value.gauge_pattern_param_a,
	value.gauge_pattern_param_b, value.gba_boss_level,
	value.gba_boss_launched_by, value.combo_points_for_boss_attack,
	value.boss_initial, value.boss, value.mima_boss_spread_angle,
	value.mima_boss_pellet_angle, value.mima_boss_orbit_angle_delta,
	value.mima_boss_orbit_angle, value.marisa_boss_spread_angle,
	value.ellen_boss_spread_angle, value.ellen_boss_pellet_angle,
	value.chiyuri_boss_pattern_angle,
	value.chiyuri_boss_pattern_direction, value.yumemi_boss_speed_offset,
	value.yumemi_boss_angle_delta, value.yumemi_boss_angle,
	value.yumemi_boss_radius, value.yumemi_boss_muzzle_x,
	value.yumemi_boss_muzzle_y, value.yumemi_boss_target_x,
	value.yumemi_boss_target_y, value.boss_target_active,
	value.boss_target_x, value.boss_target_y, value.background_phase,
	value.background_initial_kind, value.result_phase,
	value.result_phase_frame, value.result_transition_complete,
	value.result_bonus_remaining, value.result_bonus_per_frame,
	value.combos, value.chains, value.global_frame, value.route,
	value.round_complete, value.session_exit_requested,
	value.hud_start_flag, value.hud_start_state, value.hud_start_frame,
	value.enemies, value.bullets, value.bullet_trails,
	value.bullet_trail_cursor, value.fireballs, value.extra_attacks,
	value.shotpairs, value.reimu_charge,
	value.reimu_chargeshot_primary, value.mima_charge,
	value.mima_charge_active, value.mima_charge_frames,
	value.marisa_charge, value.kana_charge, value.kana_charge_state,
	value.kana_charge_frames, value.rikako_charge,
	value.rikako_charge_state, value.rikako_charge_frames,
	value.rikako_charge_radius, value.rikako_charge_origin_x,
	value.rikako_charge_origin_y, value.chiyuri_charge,
	value.ellen_charge, value.kotohime_charge, value.yumemi_charge,
	value.enemies_alive, value.enemy_speed, value.bullet_base_speed,
	value.bullet_template_angle, value.fireball_generation_previous,
	value.collision_map
)

TH03_RBK_CODEC(EnemyFormation, value.enemy_scripts)
TH03_RBK_CODEC(GameContent, value.enemy_formations)

#undef TH03_RBK_CODEC

template <typename Derived>
class Encoder {
public:
	template <typename... Values>
	bool operator()(const Values &...values) noexcept
	{
		return (one(values) && ...);
	}

private:
	template <typename Value>
	bool one(const Value &value) noexcept
	{
		using Type = Bare<Value>;
		if constexpr(std::same_as<Type, bool>) {
			return derived().emit(static_cast<std::uint8_t>(value ? 1U : 0U));
		} else if constexpr(std::integral<Type>) {
			auto bits = exact::bits(value);
			for(std::size_t index = 0U; index < sizeof(Type); ++index) {
				if(!derived().emit(static_cast<std::uint8_t>(
					bits >> static_cast<unsigned>(index * 8U)
				))) {
					return false;
				}
			}
			return true;
		} else if constexpr(std::is_enum_v<Type>) {
			return one(static_cast<std::underlying_type_t<Type>>(value));
		} else if constexpr(FixedValue<Type>) {
			return one(value.raw());
		} else if constexpr(IsArray<Type>::value) {
			for(const auto &element : value) {
				if(!one(element)) {
					return false;
				}
			}
			return true;
		} else if constexpr(IsOptional<Type>::value) {
			if(!one(value.has_value())) {
				return false;
			}
			return !value || one(*value);
		} else if constexpr(IsVector<Type>::value) {
			if(value.size() > 0xFFFFFFFFULL ||
				!one(static_cast<std::uint32_t>(value.size()))) {
				return false;
			}
			for(const auto &element : value) {
				if(!one(element)) {
					return false;
				}
			}
			return true;
		} else {
			return codec(derived(), value);
		}
	}

	Derived &derived() noexcept { return static_cast<Derived &>(*this); }
};

class ByteWriter final : public Encoder<ByteWriter> {
public:
	ByteWriter(std::span<std::byte> bytes, std::size_t offset) noexcept :
		bytes_(bytes), offset_(offset)
	{}

	bool emit(std::uint8_t value) noexcept
	{
		if(offset_ >= bytes_.size()) {
			return false;
		}
		bytes_[offset_++] = static_cast<std::byte>(value);
		return true;
	}

	[[nodiscard]] std::size_t offset() const noexcept { return offset_; }

private:
	std::span<std::byte> bytes_{};
	std::size_t offset_ = 0U;
};

class HashWriter final : public Encoder<HashWriter> {
public:
	bool emit(std::uint8_t value) noexcept
	{
		hash_ = ((hash_ << 5U) + hash_) ^ value;
		return true;
	}

	[[nodiscard]] std::uint32_t hash() const noexcept { return hash_; }

private:
	std::uint32_t hash_ = 5381U;
};

class ByteReader final {
public:
	explicit ByteReader(std::span<const std::byte> bytes) noexcept : bytes_(bytes) {}

	template <typename... Values>
	bool operator()(Values &...values) noexcept
	{
		return (one(values) && ...);
	}

	[[nodiscard]] bool complete() const noexcept { return offset_ == bytes_.size(); }

private:
	template <typename Value>
	bool one(Value &value) noexcept
	{
		using Type = Bare<Value>;
		if constexpr(std::same_as<Type, bool>) {
			std::uint8_t encoded = 0U;
			if(!one(encoded) || encoded > 1U) {
				return false;
			}
			value = encoded != 0U;
			return true;
		} else if constexpr(std::integral<Type>) {
			using Unsigned = std::make_unsigned_t<Type>;
			if(offset_ + sizeof(Type) > bytes_.size()) {
				return false;
			}
			Unsigned bits = 0U;
			for(std::size_t index = 0U; index < sizeof(Type); ++index) {
				bits = static_cast<Unsigned>(bits | static_cast<Unsigned>(
					std::to_integer<Unsigned>(bytes_[offset_++]) <<
					static_cast<unsigned>(index * 8U)
				));
			}
			value = exact::from_bits<Type>(bits);
			return true;
		} else if constexpr(std::is_enum_v<Type>) {
			std::underlying_type_t<Type> encoded{};
			if(!one(encoded)) {
				return false;
			}
			value = static_cast<Type>(encoded);
			return true;
		} else if constexpr(FixedValue<Type>) {
			typename FixedTraits<Type>::rep encoded{};
			if(!one(encoded)) {
				return false;
			}
			value = Type::from_raw(encoded);
			return true;
		} else if constexpr(IsArray<Type>::value) {
			for(auto &element : value) {
				if(!one(element)) {
					return false;
				}
			}
			return true;
		} else if constexpr(IsOptional<Type>::value) {
			bool present = false;
			if(!one(present)) {
				return false;
			}
			if(!present) {
				value.reset();
				return true;
			}
			value.emplace();
			return one(*value);
		} else {
			return codec(*this, value);
		}
	}

	std::span<const std::byte> bytes_{};
	std::size_t offset_ = 0U;
};

std::uint16_t read_u16(std::span<const std::byte> bytes, std::size_t offset) noexcept
{
	return static_cast<std::uint16_t>(
		std::to_integer<std::uint16_t>(bytes[offset]) |
		(std::to_integer<std::uint16_t>(bytes[offset + 1U]) << 8U)
	);
}

std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) noexcept
{
	return (
		std::to_integer<std::uint32_t>(bytes[offset]) |
		(std::to_integer<std::uint32_t>(bytes[offset + 1U]) << 8U) |
		(std::to_integer<std::uint32_t>(bytes[offset + 2U]) << 16U) |
		(std::to_integer<std::uint32_t>(bytes[offset + 3U]) << 24U)
	);
}

void write_u16(std::span<std::byte> bytes, std::size_t offset, std::uint16_t value) noexcept
{
	bytes[offset] = static_cast<std::byte>(value);
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

void write_u32(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value) noexcept
{
	bytes[offset] = static_cast<std::byte>(value);
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
	bytes[offset + 2U] = static_cast<std::byte>(value >> 16U);
	bytes[offset + 3U] = static_cast<std::byte>(value >> 24U);
}

std::uint32_t fnv1a32(std::span<const std::byte> bytes) noexcept
{
	std::uint32_t hash = 2'166'136'261U;
	for(const auto byte : bytes) {
		hash ^= std::to_integer<std::uint8_t>(byte);
		hash *= 16'777'619U;
	}
	return hash;
}

std::uint32_t configuration_hash(
	const GameInit &initial_init,
	bool replay_pause_adapter,
	bool three_digit_combos,
	const GameContent &content
) noexcept
{
	HashWriter writer{};
	if(!writer(
		initial_init, replay_pause_adapter, three_digit_combos, content
	)) {
		return 0U;
	}
	return writer.hash();
}

Error snapshot_error(ErrorCode code, const char *message, std::size_t offset = 0U)
{
	return Error{code, message, offset};
}

} // namespace

std::expected<RollbackSimulationSnapshot, Error>
Game::capture_rollback_simulation() const noexcept
{
	RollbackSimulationSnapshot snapshot{};
	auto bytes = std::span<std::byte>(snapshot.bytes);
	ByteWriter writer(bytes, snapshot_header_size);
	if(!writer(
		impl_->state,
		impl_->warning_interstitial_samples_remaining,
		impl_->replay_pause_gameplay_input,
		impl_->replay_pause_input_boundary,
		impl_->defeat_none_before_frame,
		impl_->defeat_wait_before_frame,
		impl_->replay_pause_deferred,
		impl_->live_pause_phase,
		impl_->live_pause_return_context,
		impl_->live_pause_input_boundary,
		impl_->main_process_finished,
		impl_->effective_reset_seed,
		impl_->round_start
	)) {
		return std::unexpected(snapshot_error(
			ErrorCode::count_mismatch,
			"T3RBK001 simulation state exceeds its fixed capacity"
		));
	}

	snapshot.used_size = static_cast<std::uint32_t>(writer.offset());
	std::ranges::copy(snapshot_magic, bytes.begin());
	write_u16(bytes, 8U, snapshot_version);
	write_u16(bytes, 10U, snapshot_header_size);
	write_u32(bytes, 12U, snapshot.used_size);
	const auto payload = std::span<const std::byte>(snapshot.bytes).subspan(
		snapshot_header_size,
		snapshot.used_size - snapshot_header_size
	);
	write_u32(bytes, 16U, static_cast<std::uint32_t>(payload.size()));
	write_u32(bytes, 20U, fnv1a32(payload));
	write_u32(bytes, 24U, configuration_hash(
		impl_->initial_init, impl_->replay_pause_adapter,
		impl_->state.three_digit_combos, impl_->content
	));
	write_u32(bytes, 28U, snapshot_simulation_flag);
	return snapshot;
}

std::expected<void, Error> Game::restore_rollback_simulation(
	const RollbackSimulationSnapshot &snapshot
) noexcept
{
	if(
		snapshot.used_size < snapshot_header_size ||
		snapshot.used_size > snapshot.bytes.size()
	) {
		return std::unexpected(snapshot_error(
			ErrorCode::truncated, "T3RBK001 size is outside its fixed image"
		));
	}
	const auto bytes = std::span<const std::byte>(snapshot.bytes).first(
		snapshot.used_size
	);
	if(!std::ranges::equal(snapshot_magic, bytes.first(snapshot_magic.size()))) {
		return std::unexpected(snapshot_error(
			ErrorCode::bad_magic, "rollback image magic is not T3RBK001"
		));
	}
	if(read_u16(bytes, 8U) != snapshot_version) {
		return std::unexpected(snapshot_error(
			ErrorCode::unsupported_version,
			"rollback image version is not supported", 8U
		));
	}
	if(read_u16(bytes, 10U) != snapshot_header_size) {
		return std::unexpected(snapshot_error(
			ErrorCode::unsupported_layout,
			"rollback image header size is not supported", 10U
		));
	}
	if(read_u32(bytes, 12U) != snapshot.used_size ||
		read_u32(bytes, 16U) != snapshot.used_size - snapshot_header_size) {
		return std::unexpected(snapshot_error(
			ErrorCode::count_mismatch,
			"rollback image size fields disagree", 12U
		));
	}
	if(read_u32(bytes, 28U) != snapshot_simulation_flag) {
		return std::unexpected(snapshot_error(
			ErrorCode::unsupported_layout,
			"rollback image is not a simulation snapshot", 28U
		));
	}
	if(read_u32(bytes, 24U) != configuration_hash(
		impl_->initial_init, impl_->replay_pause_adapter,
		impl_->state.three_digit_combos, impl_->content
	)) {
		return std::unexpected(snapshot_error(
			ErrorCode::invalid_value,
			"rollback image belongs to a different gameplay configuration", 24U
		));
	}
	const auto payload = bytes.subspan(snapshot_header_size);
	if(read_u32(bytes, 20U) != fnv1a32(payload)) {
		return std::unexpected(snapshot_error(
			ErrorCode::checksum_mismatch,
			"rollback image payload checksum does not match", 20U
		));
	}

	detail::DeterministicState state{};
	std::uint8_t warning_interstitial_samples_remaining = 0U;
	std::array<std::uint16_t, 2U> replay_pause_gameplay_input{};
	CanonicalState replay_pause_input_boundary{};
	bool defeat_none_before_frame = true;
	bool defeat_wait_before_frame = false;
	bool replay_pause_deferred = false;
	LivePausePhase live_pause_phase = LivePausePhase::inactive;
	InputContext live_pause_return_context = InputContext::gameplay;
	CanonicalState live_pause_input_boundary{};
	bool main_process_finished = false;
	std::uint32_t effective_reset_seed = 0U;
	RoundStartObservation round_start{};
	ByteReader reader(payload);
	if(!reader(
		state,
		warning_interstitial_samples_remaining,
		replay_pause_gameplay_input,
		replay_pause_input_boundary,
		defeat_none_before_frame,
		defeat_wait_before_frame,
		replay_pause_deferred,
		live_pause_phase,
		live_pause_return_context,
		live_pause_input_boundary,
		main_process_finished,
		effective_reset_seed,
		round_start
	) || !reader.complete()) {
		return std::unexpected(snapshot_error(
			ErrorCode::malformed_stream,
			"rollback image payload is malformed", snapshot_header_size
		));
	}

	impl_->state = std::move(state);
	impl_->warning_interstitial_samples_remaining =
		warning_interstitial_samples_remaining;
	impl_->replay_pause_gameplay_input = replay_pause_gameplay_input;
	impl_->replay_pause_input_boundary = replay_pause_input_boundary;
	impl_->defeat_none_before_frame = defeat_none_before_frame;
	impl_->defeat_wait_before_frame = defeat_wait_before_frame;
	impl_->replay_pause_deferred = replay_pause_deferred;
	impl_->live_pause_phase = live_pause_phase;
	impl_->live_pause_return_context = live_pause_return_context;
	impl_->live_pause_input_boundary = live_pause_input_boundary;
	impl_->main_process_finished = main_process_finished;
	impl_->effective_reset_seed = effective_reset_seed;
	impl_->round_start = std::move(round_start);
	return {};
}

} // namespace th03
