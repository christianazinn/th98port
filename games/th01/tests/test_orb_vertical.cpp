#include "th01/core/orb.hpp"
#include "pc98/random.hpp"

#include <array>
#include <bit>
#include <cfenv>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <utility>

namespace {
using namespace th01::core;
struct Row {
	int operation, type;
	std::int16_t frame;
	double force, velocity, immediate;
	std::int16_t returned;
	std::uint64_t force_after, velocity_after;
};

// Measured 38-row MODEL_LARGE guest probe, source 11f83ab511e0c3e6481d7416e4b9e3f520bc5e39.
// T1ORB.BIN SHA256 ba30d1c78dcd6c753e19131c1cb7f7bdeb28aa8b341a15539361ebed1c5ae130.
// Raw output doubles, not recomputed expected values. Surface probes used nonzero X velocity.
constexpr std::array<Row, 38> rows{{
{0,255,0,-8.0,0.0,0.0,-8,0xC020000000000000ULL,0xC020000000000000ULL},
{0,255,4,-8.0,0.0,0.0,-8,0xC020000000000000ULL,0xC020000000000000ULL},
{0,255,5,-8.0,0.0,0.0,-7,0xC020000000000000ULL,0xC01C000000000000ULL},
{0,255,16,-8.0,0.0,0.0,-5,0xC020000000000000ULL,0xC014000000000000ULL},
{0,255,17,-8.0,0.0,0.0,-5,0xC020000000000000ULL,0xC014000000000000ULL},
{0,255,0,-16.25,-3.5,0.0,-16,0xC030400000000000ULL,0xC030000000000000ULL},
{0,255,5,-16.0,-2.5,0.0,-15,0xC030000000000000ULL,0xC02E000000000000ULL},
{0,255,0,-15.75,-1.5,0.0,-15,0xC02F800000000000ULL,0xC02F800000000000ULL},
{0,255,5,-0.5,-0.5,0.0,0,0xBFE0000000000000ULL,0x3FE0000000000000ULL},
{0,255,0,0.5,0.5,0.0,0,0x3FE0000000000000ULL,0x3FE0000000000000ULL},
{0,255,5,15.75,1.5,0.0,16,0x402F800000000000ULL,0x4030000000000000ULL},
{0,255,0,16.0,2.5,0.0,16,0x4030000000000000ULL,0x4030000000000000ULL},
{0,255,5,16.25,3.5,0.0,17,0x4030400000000000ULL,0x4030000000000000ULL},
{0,255,0,-1.5,13.0,0.0,-1,0xBFF8000000000000ULL,0xBFF8000000000000ULL},
{0,255,0,-0.5,13.0,0.0,0,0xBFE0000000000000ULL,0xBFE0000000000000ULL},
{0,255,0,0.5,13.0,0.0,0,0x3FE0000000000000ULL,0x3FE0000000000000ULL},
{0,255,0,1.5,13.0,0.0,1,0x3FF8000000000000ULL,0x3FF8000000000000ULL},
{1,0,1,2.25,-20.0,0.0,0,0x402F333333333334ULL,0xC034000000000000ULL},
{1,0,1,2.25,-16.0,0.0,0,0x4028F5C28F5C28F6ULL,0xC030000000000000ULL},
{1,0,1,2.25,-0.5,0.0,0,0x3FD8F5C28F5C28F6ULL,0xBFE0000000000000ULL},
{1,0,1,2.25,0.5,0.0,0,0xBFD8F5C28F5C28F6ULL,0x3FE0000000000000ULL},
{1,0,1,2.25,16.0,0.0,0,0xC028F5C28F5C28F6ULL,0x4030000000000000ULL},
{1,0,1,2.25,20.0,0.0,0,0xC02F333333333334ULL,0x4034000000000000ULL},
{1,1,3,2.25,3.25,0.0,0,0xC00A000000000000ULL,0x400A000000000000ULL},
{1,1,4,2.25,3.25,0.0,0,0xC011000000000000ULL,0x400A000000000000ULL},
{1,1,7,2.25,3.25,0.0,0,0xC011000000000000ULL,0x400A000000000000ULL},
{1,1,8,2.25,3.25,0.0,0,0xC015000000000000ULL,0x400A000000000000ULL},
{1,2,17,2.25,-3.0,0.0,0,0xC027000000000000ULL,0xC008000000000000ULL},
{1,2,17,2.25,-1.0,0.0,0,0xC025000000000000ULL,0xBFF0000000000000ULL},
{1,2,17,2.25,-0.5,0.0,0,0xC024800000000000ULL,0xBFE0000000000000ULL},
{1,2,17,2.25,0.5,0.0,0,0xC023800000000000ULL,0x3FE0000000000000ULL},
{1,2,17,2.25,1.0,0.0,0,0xC023000000000000ULL,0x3FF0000000000000ULL},
{1,2,17,2.25,3.0,0.0,0,0xC021000000000000ULL,0x4008000000000000ULL},
{1,3,9,2.25,-7.5,-16.25,0,0xC030400000000000ULL,0xC01E000000000000ULL},
{1,3,9,2.25,-7.5,-0.5,0,0xBFE0000000000000ULL,0xC01E000000000000ULL},
{1,3,9,2.25,-7.5,0.0,0,0x0000000000000000ULL,0xC01E000000000000ULL},
{1,3,9,2.25,-7.5,0.5,0,0x3FE0000000000000ULL,0xC01E000000000000ULL},
{1,3,9,2.25,-7.5,16.25,0,0x4030400000000000ULL,0xC01E000000000000ULL},
}};

bool guest_rows()
{
	for(std::size_t i = 0; i < rows.size(); ++i) {
		const auto &r = rows[i];
		OrbVertical orb{r.force, r.velocity, r.frame};
		auto vx = OrbVelocityX::left4;
		std::uint32_t seed = 123;
		if(r.operation == 0) {
			if(update_orb_velocity_y(orb) != r.returned) return false;
		} else if(!apply_orb_force(orb, vx, seed, r.immediate, static_cast<OrbForce>(r.type))) return false;
		if(std::bit_cast<std::uint64_t>(orb.force) != r.force_after ||
			std::bit_cast<std::uint64_t>(orb.velocity) != r.velocity_after ||
			orb.force_frame != (r.operation == 0 ? r.frame : 0) || vx != OrbVelocityX::left4 || seed != 123) {
			std::cerr << "Guest row " << i << " differs\n";
			return false;
		}
	}
	return true;
}

bool source_random_branch()
{
	bool saw_override = false;
	for(std::uint32_t initial = 0; initial < 65536; ++initial) {
		auto expected_seed = initial;
		const auto first = pc98::irand(expected_seed);
		const auto second = pc98::irand(expected_seed);
		auto expected_vx = OrbVelocityX::stationary;
		if(first % 50 == 0) expected_vx = OrbVelocityX::left4;
		if(second % 50 == 1) expected_vx = OrbVelocityX::right4;
		saw_override |= first % 50 == 0 && second % 50 == 1;
		OrbVertical orb{1, 8, 16};
		auto vx = OrbVelocityX::stationary;
		auto seed = initial;
		if(!apply_orb_force(orb, vx, seed, 99, OrbForce::surface) ||
			seed != expected_seed || vx != expected_vx || orb.force_frame != 0) return false;
		orb.force_frame = 17;
		vx = OrbVelocityX::stationary;
		if(!apply_orb_force(orb, vx, seed, 99, OrbForce::surface) ||
			seed != expected_seed || vx != OrbVelocityX::stationary) return false;
	}
	return saw_override;
}

bool refused_conversions()
{
	for(double force : {std::numeric_limits<double>::infinity(),
		-std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(),
		32768.0, -32769.0}) {
		OrbVertical orb{force, 3, 0};
		const auto bits = std::bit_cast<std::uint64_t>(orb.force);
		if(update_orb_velocity_y(orb) || orb.velocity != 3 || orb.force_frame != 0 ||
			std::bit_cast<std::uint64_t>(orb.force) != bits) return false;
	}
	for(const auto &[force, expected] : std::array{
		std::pair{32767.75, std::int16_t{32767}}, std::pair{-32768.75, std::int16_t{-32768}}}) {
		OrbVertical orb{force, 0, 0};
		if(update_orb_velocity_y(orb) != expected) return false;
	}
	const int saved = std::fegetround();
	if(std::fesetround(FE_DOWNWARD) != 0) return false;
	OrbVertical orb{1, 2, 3};
	auto vx = OrbVelocityX::stationary;
	std::uint32_t seed = 7;
	const bool refused = !update_orb_velocity_y(orb) &&
		!apply_orb_force(orb, vx, seed, 8, OrbForce::surface) &&
		orb == OrbVertical{1, 2, 3} && vx == OrbVelocityX::stationary && seed == 7;
	const int restored = std::fesetround(saved);
	return refused && restored == 0;
}
bool frame_and_force_rules()
{
	for(int frame = -32768; frame < 32768; ++frame) {
		OrbVertical orb{-8, 99, static_cast<std::int16_t>(frame)};
		const auto expected = static_cast<std::int16_t>(frame / 5 - 8);
		if(update_orb_velocity_y(orb) != expected || orb.force_frame != frame || orb.force != -8 ||
			orb.velocity != (expected < -16 ? -16 : (expected > 16 ? 16 : expected))) return false;
		auto vx = OrbVelocityX::right8;
		std::uint32_t seed = 5;
		orb.velocity = 3.25;
		if(!apply_orb_force(orb, vx, seed, 1000, OrbForce::top) ||
			orb.force != -3.25 - frame / 4 || orb.velocity != 3.25 || orb.force_frame != 0 ||
			vx != OrbVelocityX::right8 || seed != 5) return false;
	}
	OrbVertical orb{2, 7, 9};
	auto vx = OrbVelocityX::left8;
	std::uint32_t seed = 5;
	if(!apply_orb_force(orb, vx, seed, 1000, static_cast<OrbForce>(-1)) ||
		orb != OrbVertical{2, 7, 0}) return false;
	if(!apply_orb_force(orb, vx, seed, -0.0, OrbForce::immediate) ||
		std::bit_cast<std::uint64_t>(orb.force) != 0x8000000000000000ULL) return false;
	const auto before = orb;
	if(apply_orb_force(orb, vx, seed, std::numeric_limits<double>::infinity(), OrbForce::immediate) ||
		orb != before || seed != 5 || vx != OrbVelocityX::left8) return false;
	return true;
}
} // namespace

int main()
{
	if(!guest_rows() || !source_random_branch() || !refused_conversions() || !frame_and_force_rules()) {
		std::cerr << "TH01 Orb scalar test failed\n";
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
