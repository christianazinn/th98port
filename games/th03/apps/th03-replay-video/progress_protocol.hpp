#ifndef TH03_REPLAY_VIDEO_PROGRESS_PROTOCOL_HPP
#define TH03_REPLAY_VIDEO_PROGRESS_PROTOCOL_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace th03::replay_video {

enum class ProgressEventKind : std::uint8_t {
	progress,
	pass_complete,
};

// T3RVP001 is a newline-delimited machine protocol. Values are integral so
// locale and floating-point formatting cannot change the wire representation.
struct ProgressEvent final {
	ProgressEventKind kind = ProgressEventKind::progress;
	std::string_view pass{};
	std::uint8_t pass_index = 0U;
	std::uint8_t pass_count = 2U;
	std::uint32_t completed_permyriad = 0U;
	std::uint32_t sample = 0U;
	std::uint32_t first_sample = 0U;
	std::uint32_t total_samples = 0U;
	std::uint64_t frames = 0U;
	std::uint64_t elapsed_milliseconds = 0U;
	std::uint64_t eta_milliseconds = 0U;
	std::uint64_t speed_milli = 0U;
};

[[nodiscard]] std::string progress_event_json(const ProgressEvent &event);

} // namespace th03::replay_video

#endif
