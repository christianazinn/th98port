#include "progress_protocol.hpp"

#include <string>

namespace th03::replay_video {

std::string progress_event_json(const ProgressEvent &event)
{
	const auto kind = event.kind == ProgressEventKind::pass_complete ?
		"pass-complete" : "progress";
	return "{\"protocol\":\"T3RVP001\",\"event\":\"" +
		std::string(kind) + "\",\"pass\":\"" + std::string(event.pass) +
		"\",\"passIndex\":" + std::to_string(event.pass_index) +
		",\"passCount\":" + std::to_string(event.pass_count) +
		",\"completedPermyriad\":" +
		std::to_string(event.completed_permyriad) +
		",\"sample\":" + std::to_string(event.sample) +
		",\"firstSample\":" + std::to_string(event.first_sample) +
		",\"totalSamples\":" + std::to_string(event.total_samples) +
		",\"frames\":" + std::to_string(event.frames) +
		",\"elapsedMilliseconds\":" +
		std::to_string(event.elapsed_milliseconds) +
		",\"etaMilliseconds\":" +
		std::to_string(event.eta_milliseconds) +
		",\"speedMilli\":" + std::to_string(event.speed_milli) + "}";
}

} // namespace th03::replay_video
