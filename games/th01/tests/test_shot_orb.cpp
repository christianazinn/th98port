#include "th01/core/shot_orb.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace {
using namespace th01::core;

ShotOrbState orb_at(std::int16_t left = 100, std::int16_t top = 100) noexcept
{
	return {{left, OrbVelocityX::stationary}, {0.0, 0.0, 0}, top, false};
}

bool is(const ShotRenderOp &operation, ShotRenderKind kind, std::size_t slot,
	std::int16_t left, std::int16_t top, std::uint8_t quarter) noexcept
{
	return operation == ShotRenderOp{kind, slot, left, top, quarter};
}

bool strict_overlap_boundaries()
{
	for(const int delta : {-16, -15, 31, 32}) {
		for(const bool test_x : {false, true}) {
			Shots shots;
			const auto x = static_cast<std::int16_t>(100 + (test_x ? delta : 0));
			const auto y = static_cast<std::int16_t>(112 + (test_x ? 0 : delta));
			shots.slots[0] = {x, y, 0, true, 0};
			auto orb = orb_at();
			std::uint32_t seed = 0x12345678;
			const auto result = update_shots_with_orb(shots, orb, seed);
			const bool overlaps = delta > -16 && delta < 32;
			if(result.status != ShotOrbUpdateStatus::complete ||
				shots.slots[0].moving == overlaps || result.render.count == 0 ||
				seed != 0x12345678) return false;
		}
	}
	return true;
}

bool response_bands_and_scan_order()
{
	for(const auto &[delta, expected_velocity] : {
		std::pair{7, OrbVelocityX::right4},
		std::pair{8, OrbVelocityX::stationary},
		std::pair{9, OrbVelocityX::left4},
	}) {
		Shots shots;
		shots.slots[0] = {static_cast<std::int16_t>(100 + delta), 112, 0, true, 0};
		auto orb = orb_at();
		std::uint32_t seed = 9;
		const auto result = update_shots_with_orb(shots, orb, seed);
		if(result.status != ShotOrbUpdateStatus::complete || orb.horizontal.velocity != expected_velocity ||
			orb.vertical != OrbVertical{-10.0, -10.0, 0} || orb.top != 90 || seed != 9) return false;
	}
	for(std::size_t selected = 0; selected < 8; ++selected) {
		Shots shots;
		shots.slots[selected] = {109, 112, 0, true, 0};
		auto orb = orb_at();
		std::uint32_t seed = 1;
		const auto result = update_shots_with_orb(shots, orb, seed);
		if(result.status != ShotOrbUpdateStatus::complete || shots.slots[selected].moving ||
			orb.horizontal.left != 96 || result.render.count != 1 ||
			!is(result.render.operations[0], ShotRenderKind::moving_unput, selected, 109, 112, 0)) {
			return false;
		}
	}
	Shots shots;
	shots.slots[0] = {109, 112, 0, true, 0};
	shots.slots[1] = {105, 102, 0, true, 0};
	auto orb = orb_at();
	std::uint32_t seed = 2;
	const auto result = update_shots_with_orb(shots, orb, seed);
	return result.status == ShotOrbUpdateStatus::complete && !shots.slots[0].moving &&
		!shots.slots[1].moving && orb.horizontal.left == 92 && orb.top == 75 &&
		orb.vertical == OrbVertical{-15.0, -15.0, 0} && result.render.count == 2 &&
		is(result.render.operations[0], ShotRenderKind::moving_unput, 0, 109, 112, 0) &&
		is(result.render.operations[1], ShotRenderKind::moving_unput, 1, 105, 102, 0) && seed == 2;
}

bool portal_clip_decay_and_trace()
{
	Shots shots;
	shots.slots[0] = {10, 100, 0, true, 0};
	shots.slots[1] = {20, 120, 0, false, 1};
	shots.slots[2] = {30, 130, 0, false, 7};
	shots.slots[3] = {40, 140, 0, false, 255};
	auto orb = orb_at();
	orb.in_portal = true;
	std::uint32_t seed = 3;
	const auto result = update_shots_with_orb(shots, orb, seed);
	if(result.status != ShotOrbUpdateStatus::complete || shots.slots[0] != Shot{10, 88, 0, true, 0} ||
		shots.slots[1].decay_frame != 2 || shots.slots[2].decay_frame != 0 ||
		shots.slots[3].decay_frame != 0 || result.render.count != 7 || seed != 3) return false;
	return is(result.render.operations[0], ShotRenderKind::moving_unput, 0, 10, 100, 0) &&
		is(result.render.operations[1], ShotRenderKind::moving_put, 0, 10, 88, 0) &&
		is(result.render.operations[2], ShotRenderKind::decay_unput, 1, 20, 120, 1) &&
		is(result.render.operations[3], ShotRenderKind::decay_put, 1, 20, 120, 1) &&
		is(result.render.operations[4], ShotRenderKind::decay_unput, 2, 30, 130, 2) &&
		is(result.render.operations[5], ShotRenderKind::decay_unput, 3, 40, 140, 64) &&
		is(result.render.operations[6], ShotRenderKind::decay_put, 3, 40, 140, 1);
}

bool portal_bypass_and_offscreen()
{
	Shots shots;
	shots.slots[0] = {109, 112, 0, true, 0};
	auto orb = orb_at();
	orb.in_portal = true;
	std::uint32_t seed = 4;
	const auto result = update_shots_with_orb(shots, orb, seed);
	if(result.status != ShotOrbUpdateStatus::complete || !shots.slots[0].moving ||
		shots.slots[0].top != 100 || orb.horizontal.left != 100 ||
		orb.horizontal.velocity != OrbVelocityX::stationary || orb.vertical != OrbVertical{} ||
		orb.top != 100 || !orb.in_portal || result.render.count != 2 || seed != 4) return false;
	shots = {};
	shots.slots[0] = {9, 75, 0, true, 0};
	orb = orb_at();
	orb.in_portal = true;
	const auto clipped = update_shots_with_orb(shots, orb, seed);
	return clipped.status == ShotOrbUpdateStatus::complete && !shots.slots[0].moving &&
		shots.slots[0].top == 63 && clipped.render.count == 1 &&
		is(clipped.render.operations[0], ShotRenderKind::moving_unput, 0, 9, 75, 0);
}

bool word_wrap_and_trace_capacity()
{
	Shots shots;
	shots.slots[0] = {-32767, -32764, 0, true, 0};
	auto orb = orb_at(32760, 32760);
	std::uint32_t seed = 7;
	const auto hit = update_shots_with_orb(shots, orb, seed);
	if(hit.status != ShotOrbUpdateStatus::complete || shots.slots[0].moving ||
		shots.slots[0].top != 32760 || orb.horizontal.left != 32756 || orb.top != 32750 ||
		hit.render.count != 1 ||
		!is(hit.render.operations[0], ShotRenderKind::moving_unput, 0, -32767, -32764, 0)) {
		return false;
	}
	shots = {};
	for(std::size_t i = 0; i < shots.slots.size(); ++i) {
		shots.slots[i] = {static_cast<std::int16_t>(10 + i), 100, 0, true, 0};
	}
	orb = orb_at();
	orb.in_portal = true;
	const auto trace = update_shots_with_orb(shots, orb, seed);
	return trace.status == ShotOrbUpdateStatus::complete && trace.render.count == 16 &&
		is(trace.render.operations[0], ShotRenderKind::moving_unput, 0, 10, 100, 0) &&
		is(trace.render.operations[1], ShotRenderKind::moving_put, 0, 10, 88, 0) &&
		is(trace.render.operations[14], ShotRenderKind::moving_unput, 7, 17, 100, 0) &&
		is(trace.render.operations[15], ShotRenderKind::moving_put, 7, 17, 88, 0);
}

bool prefix_preserving_refusals()
{
	Shots shots;
	shots.slots[0] = {109, 112, 0, true, 0};
	auto orb = orb_at();
	orb.vertical = {0.0, std::numeric_limits<double>::quiet_NaN(), 5};
	std::uint32_t seed = 5;
	const auto force = update_shots_with_orb(shots, orb, seed);
	if(force.status != ShotOrbUpdateStatus::unsupported_force || !shots.slots[0].moving ||
		shots.slots[0].top != 100 || orb.horizontal.velocity != OrbVelocityX::left4 ||
		orb.vertical.force_frame != 5 || force.render.count != 1 || seed != 5) return false;
	shots = {};
	shots.slots[0] = {109, 112, 0, true, 0};
	orb = orb_at();
	orb.vertical = {0.0, 70000.0, 5};
	const auto vertical = update_shots_with_orb(shots, orb, seed);
	return vertical.status == ShotOrbUpdateStatus::unsupported_vertical && !shots.slots[0].moving &&
		shots.slots[0].top == 100 && orb.horizontal.left == 96 &&
		orb.vertical == OrbVertical{34990.0, 70000.0, 0} && orb.top == 100 &&
		vertical.render.count == 1 && seed == 5;
}

} // namespace

int main()
{
	if(!strict_overlap_boundaries() || !response_bands_and_scan_order() ||
		!portal_clip_decay_and_trace() || !portal_bypass_and_offscreen() ||
		!word_wrap_and_trace_capacity() ||
		!prefix_preserving_refusals()) {
		std::cerr << "TH01 shot-Orb composition test failed\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
