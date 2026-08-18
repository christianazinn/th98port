#include "th03/app/new_classic_netplay_input_delay.hpp"

#include <limits>

namespace th03::app {

NewClassicNetplayInputDelay::NewClassicNetplayInputDelay(
	std::uint8_t frames
) noexcept :
	frames_(frames)
{}

std::expected<NewClassicNetplayInputDelay, NewClassicNetplayInputDelayError>
NewClassicNetplayInputDelay::create(std::uint8_t frames) noexcept
{
	if(frames > new_classic_netplay_max_input_delay) {
		return std::unexpected(
			NewClassicNetplayInputDelayError::invalid_delay
		);
	}
	return NewClassicNetplayInputDelay(frames);
}

std::expected<NewClassicNetplayOwnedInput, NewClassicNetplayInputDelayError>
NewClassicNetplayInputDelay::stage(
	std::uint64_t frame,
	NewClassicNetplayOwnedInput sampled
) noexcept
{
	if(retry_frame_ && *retry_frame_ == frame) {
		if(sampled != retry_sampled_) {
			return std::unexpected(
				NewClassicNetplayInputDelayError::conflicting_retry
			);
		}
		return retry_effective_;
	}
	if(next_frame_ && frame != *next_frame_) {
		return std::unexpected(NewClassicNetplayInputDelayError::frame_regressed);
	}
	if(
		frame == std::numeric_limits<std::uint64_t>::max() ||
		frame > std::numeric_limits<std::uint64_t>::max() - frames_
	) {
		return std::unexpected(NewClassicNetplayInputDelayError::frame_overflow);
	}
	const auto target = frame + frames_;
	auto &future = slots_[target % slots_.size()];
	if(future.valid) {
		return std::unexpected(NewClassicNetplayInputDelayError::frame_regressed);
	}
	future = Slot{.valid = true, .frame = target, .input = sampled};

	NewClassicNetplayOwnedInput effective{};
	auto &current = slots_[frame % slots_.size()];
	if(current.valid) {
		if(current.frame != frame) {
			return std::unexpected(NewClassicNetplayInputDelayError::frame_regressed);
		}
		effective = current.input;
		current = {};
	}
	retry_frame_ = frame;
	retry_sampled_ = sampled;
	retry_effective_ = effective;
	next_frame_ = frame + 1U;
	return effective;
}

void NewClassicNetplayInputDelay::reset() noexcept
{
	slots_ = {};
	next_frame_.reset();
	retry_frame_.reset();
	retry_sampled_ = {};
	retry_effective_ = {};
}

std::uint8_t NewClassicNetplayInputDelay::frames() const noexcept
{
	return frames_;
}

} // namespace th03::app
