#include "th03/app/new_classic_netplay_pause_barrier.hpp"

namespace th03::app {
namespace {

constexpr std::uint16_t input_cancel = 0x1000U;

[[nodiscard]] NewClassicNetplayPauseBarrierError barrier_error(
	NewClassicNetplayPauseBarrierErrorCode code
) noexcept
{
	return NewClassicNetplayPauseBarrierError{.code = code};
}

} // namespace

std::expected<
	NewClassicNetplayPauseBarrier,
	NewClassicNetplayPauseBarrierError
> NewClassicNetplayPauseBarrier::create(
	std::uint64_t frame,
	std::uint64_t confirmed_gameplay_frame_count
) noexcept
{
	if(frame == 0U || confirmed_gameplay_frame_count == 0U) {
		return std::unexpected(barrier_error(
			NewClassicNetplayPauseBarrierErrorCode::invalid_identity
		));
	}
	NewClassicNetplayPauseBarrier result{};
	result.frame_ = frame;
	result.confirmed_gameplay_frame_count_ = confirmed_gameplay_frame_count;
	return result;
}

void NewClassicNetplayPauseBarrier::consume_local(CoreInput input) noexcept
{
	if(local_ready_) {
		return;
	}
	const bool raw_any = input.system != 0U || input.p1 != 0U ||
		input.p2 != 0U;
	if(!released_) {
		released_ = !raw_any;
		return;
	}
	if((input.system & input_cancel) != 0U) {
		choose_local_resume();
	}
}

void NewClassicNetplayPauseBarrier::choose_local_resume() noexcept
{
	if(!local_forfeit_ && !remote_forfeit_) {
		released_ = true;
		local_ready_ = true;
	}
}

void NewClassicNetplayPauseBarrier::choose_local_forfeit() noexcept
{
	released_ = true;
	local_forfeit_ = true;
	local_ready_ = false;
}

std::expected<void, NewClassicNetplayPauseBarrierError>
NewClassicNetplayPauseBarrier::receive(
	NewClassicNetplayDatagramType signal,
	std::uint64_t frame,
	std::uint64_t confirmed_gameplay_frame_count
) noexcept
{
	if(frame != frame_ || confirmed_gameplay_frame_count !=
		confirmed_gameplay_frame_count_) {
		return std::unexpected(barrier_error(
			NewClassicNetplayPauseBarrierErrorCode::invalid_identity
		));
	}
	if(signal == NewClassicNetplayDatagramType::pause_waiting) {
		return {};
	}
	if(signal == NewClassicNetplayDatagramType::forfeit_request) {
		remote_forfeit_ = true;
		remote_ready_ = false;
		return {};
	}
	if(signal == NewClassicNetplayDatagramType::forfeit_ack) {
		if(!local_forfeit_) {
			return std::unexpected(barrier_error(
				NewClassicNetplayPauseBarrierErrorCode::invalid_signal
			));
		}
		forfeit_acknowledged_ = true;
		return {};
	}
	if(signal != NewClassicNetplayDatagramType::resume_ready) {
		return std::unexpected(barrier_error(
			NewClassicNetplayPauseBarrierErrorCode::invalid_signal
		));
	}
	remote_ready_ = true;
	return {};
}

std::expected<void, NewClassicNetplayPauseBarrierError>
NewClassicNetplayPauseBarrier::observe_peer_gameplay(
	std::uint64_t frame
) noexcept
{
	if(frame < frame_) {
		return {};
	}
	if(local_forfeit_ || remote_forfeit_) {
		return std::unexpected(barrier_error(
			NewClassicNetplayPauseBarrierErrorCode::invalid_signal
		));
	}
	if(!local_ready_) {
		return std::unexpected(barrier_error(
			NewClassicNetplayPauseBarrierErrorCode::
				peer_resumed_before_local_ready
		));
	}
	remote_ready_ = true;
	return {};
}

NewClassicNetplayDatagramType
NewClassicNetplayPauseBarrier::outbound_signal() const noexcept
{
	if(local_forfeit_) {
		return NewClassicNetplayDatagramType::forfeit_request;
	}
	if(remote_forfeit_) {
		return NewClassicNetplayDatagramType::forfeit_ack;
	}
	return local_ready_ ? NewClassicNetplayDatagramType::resume_ready :
		NewClassicNetplayDatagramType::pause_waiting;
}

NewClassicNetplayPauseBarrierStatus
NewClassicNetplayPauseBarrier::status() const noexcept
{
	if(local_forfeit_ && remote_forfeit_) {
		return NewClassicNetplayPauseBarrierStatus::mutual_forfeit;
	}
	if(local_forfeit_ && forfeit_acknowledged_) {
		return NewClassicNetplayPauseBarrierStatus::
			local_forfeit_acknowledged;
	}
	if(local_forfeit_) {
		return NewClassicNetplayPauseBarrierStatus::local_forfeit_waiting;
	}
	if(remote_forfeit_) {
		return NewClassicNetplayPauseBarrierStatus::remote_forfeit;
	}
	if(!released_) {
		return NewClassicNetplayPauseBarrierStatus::waiting_release;
	}
	if(!local_ready_) {
		return NewClassicNetplayPauseBarrierStatus::waiting_local_ready;
	}
	if(!remote_ready_) {
		return NewClassicNetplayPauseBarrierStatus::waiting_remote_ready;
	}
	return NewClassicNetplayPauseBarrierStatus::ready;
}

std::uint64_t NewClassicNetplayPauseBarrier::frame() const noexcept
{
	return frame_;
}

std::uint64_t
NewClassicNetplayPauseBarrier::confirmed_gameplay_frame_count() const noexcept
{
	return confirmed_gameplay_frame_count_;
}

bool NewClassicNetplayPauseBarrier::ready() const noexcept
{
	return !local_forfeit_ && !remote_forfeit_ && local_ready_ && remote_ready_;
}

bool NewClassicNetplayPauseBarrier::local_ready() const noexcept
{
	return local_ready_;
}

bool NewClassicNetplayPauseBarrier::remote_ready() const noexcept
{
	return remote_ready_;
}

bool NewClassicNetplayPauseBarrier::local_forfeit() const noexcept
{
	return local_forfeit_;
}

bool NewClassicNetplayPauseBarrier::remote_forfeit() const noexcept
{
	return remote_forfeit_;
}

bool NewClassicNetplayPauseBarrier::forfeit_acknowledged() const noexcept
{
	return forfeit_acknowledged_;
}

} // namespace th03::app
