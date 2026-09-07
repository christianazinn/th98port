#ifndef TH03RV_SERVICE_PROTOCOL_HPP
#define TH03RV_SERVICE_PROTOCOL_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "th03/formats/replay.hpp"
#include "th03/replay/run.hpp"

namespace th03rv {

inline constexpr std::string_view service_protocol = "T3RV-SERVICE-1";

struct ServiceAcceptedReport final {
	std::string display_name{};
	std::string mode = "story";
	std::uint8_t rank = 0U;
	std::uint8_t character = 0U;
	std::string result_kind{};
	std::uint64_t score = 0U;
	std::uint8_t end_stage = 0U;
	std::uint8_t continues_used = 0U;
	std::uint8_t autofire = 0U;
	std::uint32_t timed_frames = 0U;
	std::uint32_t slow_frames = 0U;
	std::uint16_t dos_date = 0U;
	std::optional<std::string> recorder_user_id{};
	std::optional<std::string> opponent_user_id{};
};

// This projection accepts a complete verified stock Story run or a completed
// stock V14 netplay match. Scores and terminal facts come from simulated state,
// never ReplayMetadata::final_score or its optional terminal fields. Netplay
// additionally projects the recorder-side UUID so the service can bind the
// authenticated uploader without exposing UUIDs in the retained UI.
[[nodiscard]] std::optional<ServiceAcceptedReport>
make_service_accepted_report(
	const th03::formats::ReplayDocument &document,
	const th03::replay::RunResult &run,
	const th03::GameContent &content,
	bool splits_match,
	bool checkpoints_match
);

[[nodiscard]] std::string service_accepted_json(
	const ServiceAcceptedReport &report
);

[[nodiscard]] std::string service_rejected_json(std::string_view reason);
[[nodiscard]] std::string service_error_json();

} // namespace th03rv

#endif
