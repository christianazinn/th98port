#include "th03/app/new_classic_netplay_replay_capture.hpp"

#include <utility>

namespace th03::app {
namespace {

[[nodiscard]] NewClassicNetplayReplayCaptureError capture_error(
	NewClassicNetplayReplayCaptureErrorCode code
) noexcept
{
	return NewClassicNetplayReplayCaptureError{.code = code};
}

[[nodiscard]] NewClassicNetplayReplayCaptureError replay_error(
	NewClassicReplayError error
) noexcept
{
	return NewClassicNetplayReplayCaptureError{
		.code = NewClassicNetplayReplayCaptureErrorCode::replay_failed,
		.replay = std::move(error),
	};
}

} // namespace

NewClassicNetplayReplayCapture::NewClassicNetplayReplayCapture(
	NewClassicReplayRecorder recorder
) noexcept :
	recorder_(std::move(recorder))
{}

std::expected<
	NewClassicNetplayReplayCapture,
	NewClassicNetplayReplayCaptureError
> NewClassicNetplayReplayCapture::create(
	const RoundStartObservation &initial_round,
	std::uint16_t dos_date
)
{
	NewClassicReplayRecorder recorder(dos_date);
	if(auto started = recorder.round_started(initial_round); !started) {
		return std::unexpected(replay_error(std::move(started.error())));
	}
	return NewClassicNetplayReplayCapture(std::move(recorder));
}

std::expected<void, NewClassicNetplayReplayCaptureError>
NewClassicNetplayReplayCapture::append_confirmed_frame(
	const NewClassicNetplayConfirmedInputFrame &frame,
	bool deadline_missed
)
{
	if(failed_ || finished_ || round_start_pending_ || main_complete_ ||
		frame.frame != next_frame_ || frame.source_vsyncs == 0U ||
		frame.step_result.status == StepStatus::paused ||
		frame.step_result.status == StepStatus::invalid_input_context ||
		frame.step_result.status == StepStatus::simulation_not_implemented) {
		failed_ = true;
		return std::unexpected(capture_error(
			NewClassicNetplayReplayCaptureErrorCode::invalid_sequence
		));
	}
	// `source_vsyncs` is the wait *after* this process execution. The live
	// recorder observes those VSyncs before the following replay_frame_io(), so
	// carry each confirmed fact's wait forward instead of charging it backward.
	for(std::uint8_t vsync = 0U;
		vsync < source_vsyncs_before_next_frame_; ++vsync) {
		recorder_.source_vsync();
	}
	if(frame.phase == replay::InputPhase::gameplay) {
		recorder_.frame_io(frame.step_result);
		recorder_.classify_pending_frame(deadline_missed);
	}
	if(auto sampled = recorder_.sample(frame.phase, frame.input); !sampled) {
		failed_ = true;
		return std::unexpected(replay_error(std::move(sampled.error())));
	}

	if(frame.step_result.status == StepStatus::round_transition ||
		frame.step_result.status == StepStatus::terminal) {
		if(auto split = recorder_.round_completed(
			frame.step_result.input_boundary_state
		); !split) {
			failed_ = true;
			return std::unexpected(replay_error(std::move(split.error())));
		}
		if(frame.step_result.status == StepStatus::round_transition) {
			round_start_pending_ = true;
		} else {
			if(auto boundary = recorder_.process_completed(
				replay::ProcessId::main
			); !boundary) {
				failed_ = true;
				return std::unexpected(replay_error(
					std::move(boundary.error())
				));
			}
			main_complete_ = true;
		}
	}
	source_vsyncs_before_next_frame_ = frame.source_vsyncs;
	++next_frame_;
	return {};
}

std::expected<void, NewClassicNetplayReplayCaptureError>
NewClassicNetplayReplayCapture::round_started(
	const RoundStartObservation &observation
)
{
	if(failed_ || finished_ || main_complete_ || !round_start_pending_) {
		failed_ = true;
		return std::unexpected(capture_error(
			NewClassicNetplayReplayCaptureErrorCode::invalid_sequence
		));
	}
	if(auto started = recorder_.round_started(observation); !started) {
		failed_ = true;
		return std::unexpected(replay_error(std::move(started.error())));
	}
	round_start_pending_ = false;
	return {};
}

std::expected<formats::ReplayDocument, NewClassicNetplayReplayCaptureError>
NewClassicNetplayReplayCapture::finish_mainl(const GameInit &resident)
{
	if(failed_ || finished_ || round_start_pending_ || !main_complete_) {
		failed_ = true;
		return std::unexpected(capture_error(
			NewClassicNetplayReplayCaptureErrorCode::invalid_sequence
		));
	}
	if(auto boundary = recorder_.process_completed(replay::ProcessId::mainl);
		!boundary) {
		failed_ = true;
		return std::unexpected(replay_error(std::move(boundary.error())));
	}
	if(auto finished = recorder_.finish(
		formats::ReplayEndReason::game_over, resident
	); !finished) {
		failed_ = true;
		return std::unexpected(replay_error(std::move(finished.error())));
	}
	auto document = recorder_.take_completed();
	if(!document) {
		failed_ = true;
		return std::unexpected(capture_error(
			NewClassicNetplayReplayCaptureErrorCode::invalid_sequence
		));
	}
	finished_ = true;
	return std::move(*document);
}

std::uint64_t NewClassicNetplayReplayCapture::frame_count() const noexcept
{
	return next_frame_;
}

const formats::ReplayDocument *
NewClassicNetplayReplayCapture::document() const noexcept
{
	return recorder_.document();
}

void NewClassicNetplayReplayCapture::discard() noexcept
{
	recorder_.discard();
	source_vsyncs_before_next_frame_ = 1U;
	round_start_pending_ = false;
	main_complete_ = false;
	failed_ = true;
	finished_ = false;
}

} // namespace th03::app
