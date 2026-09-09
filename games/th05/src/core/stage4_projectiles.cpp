#include "th05/core/stage4_projectiles.hpp"

#include "th05/core/b6ball.hpp"
#include "th05/core/bullet.hpp"
#include "th05/core/shot_collision.hpp"
#include "th05/core/state.hpp"
#include "pc98/masterlib_iatan2.hpp"
#include "pc98/masterlib_vector.hpp"
#include "pc98/th0405_motion.hpp"
#include "pc98/th0405_sparks.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace th05::core {
namespace {

constexpr std::int16_t subpixel_factor = 16;
constexpr std::int16_t ball_half_extent = 16 * subpixel_factor;
constexpr std::int16_t playfield_width = 384 * subpixel_factor;
constexpr std::int16_t playfield_height = 368 * subpixel_factor;
constexpr std::int16_t cheeto_trail_killbox = 12 * subpixel_factor;
constexpr std::int16_t cheeto_head_killbox = 16 * subpixel_factor;
constexpr std::int16_t b4ball_shot_radius = 8 * subpixel_factor;
constexpr std::int16_t pat_b4ball_snow = 212;
constexpr std::int16_t pat_b4ball_snow_hit = 220;
constexpr std::int16_t pat_decay_b4ball = 224;
constexpr std::int16_t pat_decay_b4ball_end = pat_decay_b4ball + 4;

[[nodiscard]] constexpr std::int16_t word(std::uint16_t bits) noexcept
{
	return std::bit_cast<std::int16_t>(bits);
}

[[nodiscard]] constexpr std::int16_t word_sub(
	std::int16_t left, std::int16_t right
) noexcept
{
	return word(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) - static_cast<std::uint16_t>(right)
	));
}

[[nodiscard]] constexpr std::int16_t word_sub(
	std::int16_t left, std::uint16_t right
) noexcept
{
	return word(static_cast<std::uint16_t>(static_cast<std::uint16_t>(left) - right));
}

[[nodiscard]] constexpr std::int16_t word_add(std::int16_t left, std::int16_t right) noexcept
{
	return word(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) + static_cast<std::uint16_t>(right)
	));
}

[[nodiscard]] constexpr std::uint8_t byte_add(std::uint8_t left, std::uint8_t right) noexcept
{
	return static_cast<std::uint8_t>(static_cast<std::uint16_t>(left) + right);
}

[[nodiscard]] constexpr bool overlap(std::int16_t delta, std::int16_t extent) noexcept
{
	return static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(delta) + static_cast<std::uint16_t>(extent / 2)
	) <= static_cast<std::uint16_t>(extent);
}

[[nodiscard]] constexpr bool encloses_32px(pc98::th0405::SubpixelPoint point) noexcept
{
	return point.x > -ball_half_extent && point.x < playfield_width + ball_half_extent &&
		point.y > -ball_half_extent && point.y < playfield_height + ball_half_extent;
}

[[nodiscard]] constexpr std::int16_t cheeto_sprite_for_angle(std::uint8_t angle) noexcept
{
	// th05/main/bullet/patnum.cpp, with base patnum 0: add seven in a byte,
	// retain the directional half-turn, then divide by eight.
	return static_cast<std::int16_t>((byte_add(angle, 7U) & 0x7FU) >> 3U);
}

[[nodiscard]] bool hittest_b4ball(
	State &state, pc98::th0405::SubpixelPoint center, std::uint16_t &damage
) noexcept
{
	const auto hit = shots_hittest(
		state.shots,
		{center, {b4ball_shot_radius, b4ball_shot_radius}},
		state.shot_hit_spark_parity,
		{
			state.bombing,
			false,
			static_cast<ShotPlaychar>(state.playchar),
			state.stage_id,
			static_cast<std::uint8_t>(state.stage_frame & 3U),
			state.bomb_frame,
			state.player_pos.cur,
		},
		pc98::th0405::SparkPool{state.sparks, state.spark_ring_offset},
		state.random,
		state.score
	);
	if(!hit) return false;
	damage = *hit;
	return true;
}

[[nodiscard]] std::optional<std::uint8_t> cheeto_player_angle(
	const State &state, pc98::th0405::SubpixelPoint origin
) noexcept
{
	return pc98::masterlib::iatan2_source(
		word_sub(state.player_pos.cur.y, origin.y),
		word_sub(state.player_pos.cur.x, origin.x)
	);
}

// cheeto_head_t overlays both b4ball_t and b6ball_t, but the C++ owners use
// deliberately distinct types. Keep the byte mapping explicit instead of
// reinterpreting either object as a CheetoHead.
struct CheetoHeadFields {
	CheetoFlag flag{};
	std::uint8_t angle{};
	pc98::th0405::Motion pos{};
	std::uint16_t age{};
	std::int16_t sprite{};
	std::uint8_t speed{};
};

static_assert(sizeof(B4Ball) == sizeof(B6Ball));
static_assert(offsetof(B4Ball, flag) == offsetof(B6Ball, flag));
static_assert(offsetof(B4Ball, angle) == offsetof(B6Ball, unused_1));
static_assert(offsetof(B4Ball, pos) == offsetof(B6Ball, pos));
static_assert(offsetof(B4Ball, age) == offsetof(B6Ball, age));
static_assert(offsetof(B4Ball, patnum_tiny_base) == offsetof(B6Ball, patnum_tiny));
static_assert(offsetof(B4Ball, speed) == offsetof(B6Ball, unused_2) + 2U);

[[nodiscard]] CheetoHeadFields cheeto_head_load(const B4Ball &ball) noexcept
{
	return {
		static_cast<CheetoFlag>(ball.flag), ball.angle, ball.pos, ball.age,
		ball.patnum_tiny_base, ball.speed,
	};
}

void cheeto_head_store(B4Ball &ball, const CheetoHeadFields &head) noexcept
{
	ball.flag = static_cast<std::uint8_t>(head.flag);
	ball.angle = head.angle;
	ball.pos = head.pos;
	ball.age = head.age;
	ball.patnum_tiny_base = head.sprite;
	ball.speed = head.speed;
}

[[nodiscard]] CheetoHeadFields cheeto_head_load(const B6Ball &ball) noexcept
{
	return {
		static_cast<CheetoFlag>(static_cast<std::uint8_t>(ball.flag)),
		static_cast<std::uint8_t>(ball.unused_1), ball.pos, ball.age,
		ball.patnum_tiny, static_cast<std::uint8_t>(ball.unused_2[2]),
	};
}

void cheeto_head_store(B6Ball &ball, const CheetoHeadFields &head) noexcept
{
	ball.flag = static_cast<B6BallFlag>(static_cast<std::uint8_t>(head.flag));
	ball.unused_1 = head.angle;
	ball.pos = head.pos;
	ball.age = head.age;
	ball.patnum_tiny = head.sprite;
	ball.unused_2[2] = std::bit_cast<std::int8_t>(head.speed);
}

void cheeto_template_store(B4BallTemplate &destination, const CheetoTemplate &source) noexcept
{
	destination.angle = source.angle;
	destination.origin = source.origin;
	const auto patnum = static_cast<std::uint16_t>(destination.patnum_tiny_base);
	destination.patnum_tiny_base = word(static_cast<std::uint16_t>(
		(patnum & 0xFF00U) | static_cast<std::uint8_t>(source.color)
	));
	destination.speed = source.speed;
}

void cheeto_template_store(B6BallTemplate &destination, const CheetoTemplate &source) noexcept
{
	destination.angle = source.angle;
	destination.origin = source.origin;
	const auto patnum = static_cast<std::uint16_t>(destination.patnum_tiny);
	destination.patnum_tiny = word(static_cast<std::uint16_t>(
		(patnum & 0xFF00U) | static_cast<std::uint8_t>(source.color)
	));
	destination.speed = source.speed;
}

void cheeto_head_spawn(
	CheetoHeadFields &head, CheetoTrail &trail, const CheetoTemplate &template_
) noexcept
{
	trail.flag = CheetoFlag::decelerate;
	head.angle = template_.angle;
	head.speed = template_.speed;
	const auto velocity = pc98::masterlib::vector2(
		head.angle, static_cast<std::int16_t>(head.speed)
	);
	head.pos.velocity = {velocity.x, velocity.y};
	trail.color = template_.color;
	head.sprite = cheeto_sprite_for_angle(head.angle);
	head.pos.cur = template_.origin;
	for(std::size_t node = 0; node < trail.nodes.size(); ++node) {
		trail.nodes[node] = template_.origin;
		trail.sprites[node] = static_cast<std::uint8_t>(head.sprite);
	}
}

[[nodiscard]] bool cheeto_head_update(
	State &state, CheetoTrail &trail, CheetoHeadFields &head
) noexcept
{
	++head.age;
	for(std::size_t node = trail.nodes.size() - 1U; node > 0U; --node) {
		trail.nodes[node] = trail.nodes[node - 1U];
		if(overlap(word_sub(trail.nodes[node].x, state.player_pos.cur.x), cheeto_trail_killbox) &&
			overlap(word_sub(trail.nodes[node].y, state.player_pos.cur.y), cheeto_trail_killbox)) {
			state.player_is_hit = true;
		}
		trail.sprites[node] = trail.sprites[node - 1U];
	}
	trail.nodes[0] = head.pos.cur;
	trail.sprites[0] = static_cast<std::uint8_t>(head.sprite);
	if(!encloses_32px(trail.nodes.back())) {
		trail.flag = CheetoFlag::free;
		head.flag = CheetoFlag::free;
		return true;
	}

	const auto position = pc98::th0405::update_motion(head.pos);
	if(overlap(word_sub(position.x, state.player_pos.cur.x), cheeto_head_killbox) &&
		overlap(word_sub(position.y, state.player_pos.cur.y), cheeto_head_killbox)) {
		state.player_is_hit = true;
	}

	std::uint8_t friction_factor{};
	if(trail.flag == CheetoFlag::decelerate) {
		head.speed = byte_add(head.speed, 0xFFU);
		if(head.speed <= 4U) {
			trail.flag = static_cast<CheetoFlag>(byte_add(
				static_cast<std::uint8_t>(trail.flag), 1U
			));
		}
		friction_factor = 0x10U;
	} else {
		head.speed = byte_add(head.speed, static_cast<std::uint8_t>((state.stage_frame & 1U) + 1U));
		friction_factor = byte_add(head.speed, 0x20U);
	}

	auto target = cheeto_player_angle(state, position);
	if(!target) return false;
	auto angle_delta = static_cast<std::uint8_t>(head.angle - *target);
	if(angle_delta >= 0x80U) {
		if(angle_delta < 0xFEU) {
			if(friction_factor == 0U) return false;
			angle_delta = static_cast<std::uint8_t>((0x100U - angle_delta) / friction_factor);
			if(angle_delta < friction_factor) angle_delta = 0x01U;
			head.angle = byte_add(head.angle, angle_delta);
		} else {
			target = cheeto_player_angle(state, position);
			if(!target) return false;
			head.angle = *target;
		}
	} else if(angle_delta <= 0x02U) {
		target = cheeto_player_angle(state, position);
		if(!target) return false;
		head.angle = *target;
	} else {
		if(friction_factor == 0U) return false;
		angle_delta = static_cast<std::uint8_t>(angle_delta / friction_factor);
		if(angle_delta < friction_factor) angle_delta = 0x01U;
		head.angle = byte_add(head.angle, static_cast<std::uint8_t>(0U - angle_delta));
	}

	const auto velocity = pc98::masterlib::vector2(
		head.angle, static_cast<std::int16_t>(head.speed)
	);
	head.pos.velocity = {velocity.x, velocity.y};
	head.sprite = cheeto_sprite_for_angle(head.angle);
	return true;
}

} // namespace

void stage4_cheetos_add(State &state, const CheetoTemplate &template_) noexcept
{
	auto &custom = state.stage4.custom;
	cheeto_template_store(custom.b4ball_template, template_);
	for(std::size_t index = 0; index < state.stage4.cheeto_trails.size(); ++index) {
		auto &ball = custom.b4balls[index];
		auto &trail = state.stage4.cheeto_trails[index];
		auto head = cheeto_head_load(ball);
		if(head.flag != CheetoFlag::free || trail.flag != CheetoFlag::free) continue;
		cheeto_head_spawn(head, trail, template_);
		cheeto_head_store(ball, head);
		return;
	}
}

bool stage4_cheetos_update(State &state) noexcept
{
	for(std::size_t index = 0; index < state.stage4.cheeto_trails.size(); ++index) {
		auto &trail = state.stage4.cheeto_trails[index];
		if(trail.flag == CheetoFlag::free) continue;
		auto &ball = state.stage4.custom.b4balls[index];
		auto head = cheeto_head_load(ball);
		const auto updated = cheeto_head_update(state, trail, head);
		cheeto_head_store(ball, head);
		if(!updated) return false;
	}
	return true;
}

void stage6_cheetos_add(State &state, const CheetoTemplate &template_) noexcept
{
	auto &b6balls = state.b6balls;
	cheeto_template_store(b6balls.b6ball_template, template_);
	for(std::size_t index = 0; index < state.stage4.cheeto_trails.size(); ++index) {
		auto &ball = b6balls.pool[index];
		auto &trail = state.stage4.cheeto_trails[index];
		auto head = cheeto_head_load(ball);
		if(head.flag != CheetoFlag::free || trail.flag != CheetoFlag::free) continue;
		cheeto_head_spawn(head, trail, template_);
		cheeto_head_store(ball, head);
		return;
	}
}

bool stage6_cheetos_update(State &state) noexcept
{
	for(std::size_t index = 0; index < state.stage4.cheeto_trails.size(); ++index) {
		auto &trail = state.stage4.cheeto_trails[index];
		if(trail.flag == CheetoFlag::free) continue;
		auto &ball = state.b6balls.pool[index];
		auto head = cheeto_head_load(ball);
		const auto updated = cheeto_head_update(state, trail, head);
		cheeto_head_store(ball, head);
		if(!updated) return false;
	}
	return true;
}

void stage4_b4balls_reset(State &state) noexcept
{
	for(auto &ball : state.stage4.custom.b4balls) {
		ball.flag = 0U;
	}
}

void stage4_b4balls_add(State &state) noexcept
{
	auto &custom = state.stage4.custom;
	const auto speed = bullet_speed_tune_for_playperf(
		custom.b4ball_template.speed, state.resources.playperf
	);
	for(auto &ball : custom.b4balls) {
		if(ball.flag != 0U) continue;
		ball.flag = 1U;
		ball.pos.cur = custom.b4ball_template.origin;
		const auto velocity = pc98::masterlib::vector2(
			custom.b4ball_template.angle, static_cast<std::int16_t>(speed)
		);
		ball.pos.velocity = {velocity.x, velocity.y};
		ball.angle = custom.b4ball_template.angle;
		ball.speed = custom.b4ball_template.speed;
		ball.patnum_tiny_base = custom.b4ball_template.patnum_tiny_base;
		ball.hp = custom.b4ball_template.hp;
		ball.damage_this_frame = 0;
		ball.revenge = custom.b4ball_template.revenge;
		return;
	}
}

bool stage4_b4balls_update(State &state, Presentation &presentation) noexcept
{
	for(auto &ball : state.stage4.custom.b4balls) {
		if(ball.flag == 0U) continue;
		++ball.age;
		const auto position = pc98::th0405::update_motion(ball.pos);
		if(!encloses_32px(position)) {
			ball.flag = 0U;
			continue;
		}

		if(ball.flag == 2U) {
			if((ball.age & 3U) != 0U) continue;
			ball.patnum_tiny_base = word_add(ball.patnum_tiny_base, 1);
			if(ball.patnum_tiny_base >= pat_decay_b4ball_end) ball.flag = 0U;
			continue;
		}

		if(overlap(word_sub(position.x, state.player_pos.cur.x), cheeto_head_killbox) &&
			overlap(word_sub(position.y, state.player_pos.cur.y), cheeto_head_killbox)) {
			state.player_is_hit = true;
		}

		std::uint16_t damage{};
		if(!hittest_b4ball(state, position, damage)) return false;
		if(damage == 0U) continue;
		if(ball.patnum_tiny_base != pat_b4ball_snow) {
			presentation.sound(10U);
			continue;
		}

		ball.damage_this_frame = 1;
		ball.hp = word_sub(ball.hp, damage);
		presentation.sound(4U);
		if(ball.hp >= 0) continue;

		++ball.flag;
		ball.age = 0U;
		ball.patnum_tiny_base = pat_decay_b4ball;
		ball.pos.velocity.x = static_cast<std::int16_t>(ball.pos.velocity.x / 4);
		ball.pos.velocity.y = static_cast<std::int16_t>(ball.pos.velocity.y / 4);
		state.score.score_delta += 550U;
		if(ball.revenge && ball.pos.cur.y <= 240 * subpixel_factor) {
			state.bullets.bullet_template.origin = ball.pos.cur;
			if(!bullets_add_regular(state)) return false;
		}
		presentation.sound(3U);
	}
	return true;
}

void stage4_b4balls_render_tail(State &state) noexcept
{
	for(auto &ball : state.stage4.custom.b4balls) {
		if(ball.flag != 0U && ball.patnum_tiny_base < pat_b4ball_snow_hit) {
			ball.damage_this_frame = 0;
		}
	}
}

} // namespace th05::core
