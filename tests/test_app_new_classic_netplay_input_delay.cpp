#include "th03/app/new_classic_netplay_input_delay.hpp"

#include <cstddef>
#include <iostream>
#include <limits>
#include <string_view>

namespace {

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

void test_delay_and_retry()
{
	using namespace th03::app;
	expect(!NewClassicNetplayInputDelay::create(7U),
		"delay rejects values above the negotiated maximum");
	auto delay = NewClassicNetplayInputDelay::create(3U);
	expect(delay && delay->frames() == 3U, "three-frame delay creates");
	if(!delay) {
		return;
	}
	const NewClassicNetplayOwnedInput first{.word = 0x0020U, .charge = 1U};
	const NewClassicNetplayOwnedInput second{.word = 0x0004U};
	for(std::uint64_t frame = 0U; frame < 3U; ++frame) {
		const auto effective = delay->stage(frame, frame == 0U ? first : second);
		expect(effective && *effective == NewClassicNetplayOwnedInput{},
			"new gameplay domain begins with neutral delayed frames");
	}
	const auto delayed_first = delay->stage(3U, second);
	expect(delayed_first && *delayed_first == first,
		"sampled input appears at its exact delayed frame");
	const auto retried = delay->stage(3U, second);
	expect(retried && *retried == first,
		"prediction-stalled frame retry is idempotent");
	const auto conflicting = delay->stage(3U, first);
	expect(!conflicting && conflicting.error() ==
		NewClassicNetplayInputDelayError::conflicting_retry,
		"stalled-frame retry cannot replace its physical sample");
	const auto skipped = delay->stage(5U, second);
	expect(!skipped && skipped.error() ==
		NewClassicNetplayInputDelayError::frame_regressed,
		"delay requires contiguous source input frames");
}

void test_zero_and_boundary_reset()
{
	using namespace th03::app;
	auto zero = NewClassicNetplayInputDelay::create(0U);
	const NewClassicNetplayOwnedInput input{.word = 0x0010U};
	const auto immediate = zero ? zero->stage(42U, input) :
		std::expected<NewClassicNetplayOwnedInput,
			NewClassicNetplayInputDelayError>{
				std::unexpected(NewClassicNetplayInputDelayError::invalid_delay)
			};
	expect(immediate && *immediate == input,
		"zero delay preserves the current gameplay input");

	auto delayed = NewClassicNetplayInputDelay::create(2U);
	expect(delayed && delayed->stage(100U, input) &&
		delayed->stage(101U, {}), "boundary fixture stages a pending suffix");
	if(!delayed) {
		return;
	}
	delayed->reset();
	const auto after = delayed->stage(900U, {});
	expect(after && *after == NewClassicNetplayOwnedInput{},
		"boundary reset drops old-domain pending inputs and accepts a new frame base");

	auto boundary = NewClassicNetplayInputDelay::create(0U);
	const auto overflow = boundary->stage(
		std::numeric_limits<std::uint64_t>::max(), {}
	);
	expect(!overflow && overflow.error() ==
		NewClassicNetplayInputDelayError::frame_overflow,
		"delay rejects the terminal frame whose next-frame cursor would wrap");
}

} // namespace

int main()
{
	test_delay_and_retry();
	test_zero_and_boundary_reset();
	if(failures != 0U) {
		std::cerr << "New Classic netplay input-delay checks: " << checks
			<< ", failures: " << failures << '\n';
		return 1;
	}
	std::cout << "New Classic netplay input-delay checks: " << checks << '\n';
	return 0;
}
