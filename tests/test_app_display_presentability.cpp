#include "th03/app/runtime/display_presentability.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>

namespace {

using th03::app::SourcePresentationMailbox;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

struct ScheduleResult {
	std::uint64_t source_ticks = 0U;
	std::uint64_t runner_steps = 0U;
	std::uint64_t audio_ticks = 0U;
	std::uint64_t replay_samples = 0U;
	std::uint64_t oracle_publications = 0U;
	std::uint64_t physical_scanouts = 0U;
	std::uint64_t repeated_scanouts = 0U;
	std::uint64_t host_submissions = 0U;
};

[[nodiscard]] ScheduleResult run_one_second_schedule(
	std::uint32_t display_hz
)
{
	constexpr std::uint64_t source_ticks_per_period = 526'315U;
	constexpr std::uint64_t source_seconds_per_period = 9'328U;
	SourcePresentationMailbox mailbox{};
	ScheduleResult result{};
	std::uint64_t source_index = 0U;
	std::optional<std::uint64_t> prior_scanout{};

	const auto publish_source = [&]() {
		const auto requested = mailbox.publish_source(true);
		++result.source_ticks;
		++result.runner_steps;
		++result.audio_ticks;
		++result.replay_samples;
		++result.oracle_publications;
		if(requested) {
			(void)mailbox.acknowledge_host_submission(*requested);
		}
		++source_index;
	};

	for(std::uint64_t scanout = 0U; scanout < display_hz; ++scanout) {
		// source_index * 9328 / 526315 <= scanout / display_hz
		while(source_index * source_seconds_per_period * display_hz <=
			scanout * source_ticks_per_period) {
			publish_source();
		}
		const auto held = mailbox.submitted_publication();
		if(held == prior_scanout) {
			++result.repeated_scanouts;
		}
		prior_scanout = held;
		++result.physical_scanouts;
	}
	while(source_index * source_seconds_per_period < source_ticks_per_period) {
		publish_source();
	}
	result.host_submissions = mailbox.host_submission_count();
	return result;
}

void test_cross_refresh_sample_and_hold()
{
	const std::array rates{60U, 120U, 144U};
	std::optional<ScheduleResult> baseline{};
	for(const auto rate : rates) {
		const auto result = run_one_second_schedule(rate);
		expect(
			result.source_ticks == 57U && result.runner_steps == 57U &&
			result.audio_ticks == 57U && result.replay_samples == 57U &&
			result.oracle_publications == 57U &&
			result.host_submissions == 57U,
			"60/120/144 Hz output preserves identical source-domain work"
		);
		expect(
			result.physical_scanouts == rate && result.repeated_scanouts > 0U,
			"physical scanouts sample-and-hold the latest submitted source image"
		);
		if(!baseline) {
			baseline = result;
		} else {
			expect(
				result.source_ticks == baseline->source_ticks &&
				result.runner_steps == baseline->runner_steps &&
				result.audio_ticks == baseline->audio_ticks &&
				result.replay_samples == baseline->replay_samples &&
				result.oracle_publications == baseline->oracle_publications &&
				result.host_submissions == baseline->host_submissions,
				"display refresh changes only physical repetition"
			);
		}
	}
}

void test_minimize_retain_and_single_restore()
{
	SourcePresentationMailbox mailbox{};
	const auto initial = mailbox.publish_source(true);
	expect(
		initial == 0U && mailbox.acknowledge_host_submission(*initial) &&
		mailbox.submitted_publication() == 0U,
		"first complete source image is submitted while presentable"
	);

	expect(!mailbox.refresh_host(false),
		"becoming unpresentable does not invent a source publication");
	for(std::uint64_t expected = 1U; expected <= 3U; ++expected) {
		expect(
			!mailbox.publish_source(false) &&
			mailbox.newest_source_publication() == expected &&
			mailbox.submitted_publication() == 0U,
			"unpresentable source publications replace only the retained newest image"
		);
	}
	expect(
		mailbox.source_publication_count() == 4U &&
		mailbox.host_submission_count() == 1U && mailbox.submission_pending(),
		"minimization suppresses only host submission"
	);

	const auto restored = mailbox.refresh_host(true);
	expect(
		restored == 3U && mailbox.acknowledge_host_submission(*restored) &&
		mailbox.submitted_publication() == 3U &&
		mailbox.host_submission_count() == 2U,
		"restore submits exactly the newest complete retained image"
	);
	expect(
		!mailbox.refresh_host(true) && !mailbox.submission_pending() &&
		!mailbox.acknowledge_host_submission(3U) &&
		!mailbox.acknowledge_host_submission(2U) &&
		mailbox.host_submission_count() == 2U,
		"steady presentability and repeated or stale acknowledgements cannot duplicate a submit"
	);
}

} // namespace

int main()
{
	test_cross_refresh_sample_and_hold();
	test_minimize_retain_and_single_restore();

	if(failures != 0U) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << "All " << checks
		<< " app display-presentability checks passed\n";
	return 0;
}
