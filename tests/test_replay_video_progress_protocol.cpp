#include "progress_protocol.hpp"

#include <iostream>
#include <string>

namespace {

int failures = 0;
int checks = 0;

void expect(bool condition, const char *label)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << label << '\n';
	}
}

} // namespace

int main()
{
	using namespace th03::replay_video;
	expect(progress_event_json({
		.kind = ProgressEventKind::progress,
		.pass = "audio",
		.pass_index = 1U,
		.pass_count = 2U,
		.completed_permyriad = 1250U,
		.sample = 150U,
		.first_sample = 100U,
		.total_samples = 500U,
		.frames = 50U,
		.elapsed_milliseconds = 250U,
		.eta_milliseconds = 1750U,
		.speed_milli = 3546U,
	}) ==
		"{\"protocol\":\"T3RVP001\",\"event\":\"progress\","
		"\"pass\":\"audio\",\"passIndex\":1,\"passCount\":2,"
		"\"completedPermyriad\":1250,\"sample\":150,"
		"\"firstSample\":100,\"totalSamples\":500,\"frames\":50,"
		"\"elapsedMilliseconds\":250,\"etaMilliseconds\":1750,"
		"\"speedMilli\":3546}",
		"T3RVP001 progress serialization is byte-stable");
	expect(progress_event_json({
		.kind = ProgressEventKind::pass_complete,
		.pass = "video",
		.pass_index = 2U,
		.completed_permyriad = 10'000U,
	}) ==
		"{\"protocol\":\"T3RVP001\",\"event\":\"pass-complete\","
		"\"pass\":\"video\",\"passIndex\":2,\"passCount\":2,"
		"\"completedPermyriad\":10000,\"sample\":0,"
		"\"firstSample\":0,\"totalSamples\":0,\"frames\":0,"
		"\"elapsedMilliseconds\":0,\"etaMilliseconds\":0,"
		"\"speedMilli\":0}",
		"T3RVP001 completion serialization is byte-stable");
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0 ? 0 : 1;
}
