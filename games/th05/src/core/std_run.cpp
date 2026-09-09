#include "th05/core/state.hpp"
#include "th05/core/enemy.hpp"
#include "th05/core/midboss_extra.hpp"

namespace th05::core {
namespace {
std::uint16_t read_word(const std::vector<std::uint8_t> &data, std::size_t offset) noexcept
{
	return static_cast<std::uint16_t>(static_cast<unsigned>(data[offset]) |
		(static_cast<unsigned>(data[offset + 1]) << 8));
}
}
bool std_run(State &state, Presentation &) noexcept
{
	auto &s = state.std_data;
	if (!s.active) return true;
	// TH05 activates the midboss inside the still-installed STD callback.
	if (state.stage_id == 0) stage1_midboss_activate(state);
	else if (state.stage_id == 1) stage2_midboss_activate(state);
	else if (state.stage_id == 2) stage3_midboss_activate(state);
	else if (state.stage_id == 3) stage4_midboss_activate(state);
	else if (state.stage_id == 4) stage5_midboss_activate(state);
	else if (state.stage_id == 5) stage6_midboss_activate(state);
	else if (state.stage_id == 6) extra_midboss_activate(state);
	else return false;
	if (static_cast<std::size_t>(s.ip) + 1 >= s.data.size()) return false;
	if (read_word(s.data, s.ip) != state.stage_frame) return true;
	s.ip = static_cast<std::uint16_t>(s.ip + 2);
	if (s.ip >= s.data.size()) return false;
	auto remaining = s.data[s.ip];
	s.ip = static_cast<std::uint16_t>(s.ip + 1);
	do {
		if (!state.midboss_active && !enemies_add(state)) return false;
		s.ip = static_cast<std::uint16_t>(s.ip + 8);
		--remaining;
	} while (remaining != 0);
	if (static_cast<std::size_t>(s.ip) + 1 >= s.data.size()) return false;
	if (read_word(s.data, s.ip) == 0) s.active = false;
	return true;
}
}
