#include "th04/core/state.hpp"
#include "th04/core/enemy.hpp"

namespace th04::core {
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
	if (static_cast<std::size_t>(s.ip) + 1 >= s.data.size()) return false;
	if (read_word(s.data, s.ip) != state.stage_frame) return true;
	s.ip = static_cast<std::uint16_t>(s.ip + 2);
	if (s.ip >= s.data.size()) return false;
	auto remaining = s.data[s.ip];
	s.ip = static_cast<std::uint16_t>(s.ip + 1);
	do {
		if (!state.midboss_active) {
			if (static_cast<std::size_t>(s.ip) + 5 >= s.data.size()) return false;
			if (!enemies_add(state, s.data[s.ip],
				std::bit_cast<std::int16_t>(read_word(s.data, s.ip + 1U)),
				std::bit_cast<std::int16_t>(read_word(s.data, s.ip + 3U)),
				s.data[s.ip + 5U])) return false;
		}
		s.ip = static_cast<std::uint16_t>(s.ip + 8);
		--remaining;
	} while (remaining != 0);
	if (static_cast<std::size_t>(s.ip) + 1 >= s.data.size()) return false;
	if (read_word(s.data, s.ip) == 0) s.active = false;
	return true;
}
} // namespace th04::core
