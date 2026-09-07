#ifndef TH03_REPLAY_SESSION_HPP
#define TH03_REPLAY_SESSION_HPP

#include <cstdint>
#include <optional>
#include <variant>
#include <vector>

#include "th03/core/game.hpp"

namespace th03::replay {

enum class InputPhase : std::uint8_t {
	gameplay = 0,
	interstitial = 1,
};

enum class ProcessId : std::uint8_t {
	main = 1,
	mainl = 2,
};

struct InputSample {
	std::uint32_t frame_index = 0;
	InputPhase phase = InputPhase::gameplay;
	CoreInput input{};
	std::optional<std::uint32_t> round_frame;
	std::optional<std::uint16_t> round_or_result_frame;
};

struct ProcessBoundary {
	std::uint32_t sample_index = 0;
	ProcessId completed_process = ProcessId::main;
};

using SessionRecord = std::variant<InputSample, ProcessBoundary>;

struct ReplaySession {
	std::uint16_t source_version = 0;
	GameInit init{};
	std::vector<SessionRecord> records;
	std::uint32_t sample_count = 0;
};

} // namespace th03::replay

#endif
