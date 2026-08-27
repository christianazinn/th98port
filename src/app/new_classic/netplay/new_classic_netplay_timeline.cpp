#include "th03/app/new_classic/netplay/new_classic_netplay_timeline.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace th03::app {
namespace {

[[nodiscard]] bool valid_player(NewClassicNetplayPlayer player) noexcept
{
	return player == NewClassicNetplayPlayer::player_one ||
		player == NewClassicNetplayPlayer::player_two;
}

[[nodiscard]] NewClassicNetplayTimelineError timeline_error(
	NewClassicNetplayTimelineErrorCode code
) noexcept
{
	return NewClassicNetplayTimelineError{.code = code};
}

[[nodiscard]] std::expected<void, NewClassicNetplayTimelineError>
validate_input(NewClassicNetplayOwnedInput input) noexcept
{
	const auto encoded = encode_new_classic_netplay_input(input);
	if(!encoded) {
		return std::unexpected(NewClassicNetplayTimelineError{
			.code = NewClassicNetplayTimelineErrorCode::invalid_input,
			.input = encoded.error(),
		});
	}
	return {};
}

} // namespace

struct NewClassicNetplayInputTimeline::Impl final {
	struct Slot final {
		bool valid = false;
		bool executed = false;
		bool remote_predicted = false;
		std::uint64_t frame = 0U;
		std::optional<NewClassicNetplayOwnedInput> local{};
		std::optional<NewClassicNetplayOwnedInput> remote{};
		std::optional<NewClassicNetplayOwnedInput> resolved_remote{};
		CoreInput combined{};
		std::uint8_t prediction_depth = 0U;
	};

	NewClassicNetplayPlayer local_player =
		NewClassicNetplayPlayer::player_one;
	std::array<Slot, new_classic_netplay_timeline_slots> slots{};
	std::uint64_t next_frame = 0U;
	std::uint64_t confirmed_frame_count = 0U;
	std::optional<std::uint64_t> rollback_frame{};
	NewClassicNetplayTimelineStats stats{};

	[[nodiscard]] std::expected<Slot *, NewClassicNetplayTimelineError>
	slot_for_write(std::uint64_t frame) noexcept
	{
		if(frame < confirmed_frame_count) {
			auto &retained = slots[frame % slots.size()];
			if(retained.valid && retained.frame == frame) {
				return &retained;
			}
			return std::unexpected(timeline_error(
				NewClassicNetplayTimelineErrorCode::frame_too_old
			));
		}
		if(frame >= next_frame && frame - next_frame >= slots.size()) {
			return std::unexpected(timeline_error(
				NewClassicNetplayTimelineErrorCode::frame_too_far
			));
		}
		auto &slot = slots[frame % slots.size()];
		if(slot.valid && slot.frame != frame &&
			slot.frame >= confirmed_frame_count) {
			return std::unexpected(timeline_error(
				NewClassicNetplayTimelineErrorCode::frame_too_far
			));
		}
		if(!slot.valid || slot.frame != frame) {
			slot = Slot{.valid = true, .frame = frame};
		}
		return &slot;
	}

	[[nodiscard]] Slot *slot_if_retained(std::uint64_t frame) noexcept
	{
		auto &slot = slots[frame % slots.size()];
		return slot.valid && slot.frame == frame ? &slot : nullptr;
	}

	[[nodiscard]] const Slot *slot_if_retained(
		std::uint64_t frame
	) const noexcept
	{
		const auto &slot = slots[frame % slots.size()];
		return slot.valid && slot.frame == frame ? &slot : nullptr;
	}

	void advance_confirmation() noexcept
	{
		for(;;) {
			const auto *slot = slot_if_retained(confirmed_frame_count);
			if(!slot || !slot->executed || !slot->local || !slot->remote ||
				!slot->resolved_remote ||
				*slot->remote != *slot->resolved_remote) {
				return;
			}
			++confirmed_frame_count;
		}
	}
};

std::expected<
	std::unique_ptr<NewClassicNetplayInputTimeline>,
	NewClassicNetplayTimelineError
> NewClassicNetplayInputTimeline::create(
	NewClassicNetplayPlayer local_player
)
{
	if(!valid_player(local_player)) {
		return std::unexpected(timeline_error(
			NewClassicNetplayTimelineErrorCode::invalid_player
		));
	}
	auto impl = std::make_unique<Impl>();
	impl->local_player = local_player;
	return std::unique_ptr<NewClassicNetplayInputTimeline>(
		new NewClassicNetplayInputTimeline(std::move(impl))
	);
}

NewClassicNetplayInputTimeline::NewClassicNetplayInputTimeline(
	std::unique_ptr<Impl> impl
) noexcept :
	impl_(std::move(impl))
{}

NewClassicNetplayInputTimeline::~NewClassicNetplayInputTimeline() = default;

std::expected<
	NewClassicNetplaySubmissionDisposition,
	NewClassicNetplayTimelineError
> NewClassicNetplayInputTimeline::submit_local(
	std::uint64_t frame,
	NewClassicNetplayOwnedInput input
) noexcept
{
	if(const auto valid = validate_input(input); !valid) {
		return std::unexpected(valid.error());
	}
	auto slot = impl_->slot_for_write(frame);
	if(!slot) {
		if(slot.error().code ==
			NewClassicNetplayTimelineErrorCode::frame_too_old) {
			return NewClassicNetplaySubmissionDisposition::stale_ignored;
		}
		return std::unexpected(slot.error());
	}
	if((*slot)->local) {
		if(*(*slot)->local != input) {
			return std::unexpected(timeline_error(
				NewClassicNetplayTimelineErrorCode::conflicting_input
			));
		}
		return NewClassicNetplaySubmissionDisposition::duplicate;
	}
	(*slot)->local = input;
	impl_->advance_confirmation();
	return NewClassicNetplaySubmissionDisposition::accepted;
}

std::expected<
	NewClassicNetplayRemoteSubmission,
	NewClassicNetplayTimelineError
> NewClassicNetplayInputTimeline::submit_remote(
	std::uint64_t frame,
	NewClassicNetplayOwnedInput input
) noexcept
{
	if(const auto valid = validate_input(input); !valid) {
		return std::unexpected(valid.error());
	}
	auto slot = impl_->slot_for_write(frame);
	if(!slot) {
		if(slot.error().code ==
			NewClassicNetplayTimelineErrorCode::frame_too_old) {
			return NewClassicNetplayRemoteSubmission{
				.disposition =
					NewClassicNetplaySubmissionDisposition::stale_ignored,
				.rollback_frame = impl_->rollback_frame,
				.confirmed_frame_count = impl_->confirmed_frame_count,
			};
		}
		return std::unexpected(slot.error());
	}
	auto disposition = NewClassicNetplaySubmissionDisposition::accepted;
	if((*slot)->remote) {
		if(*(*slot)->remote != input) {
			return std::unexpected(timeline_error(
				NewClassicNetplayTimelineErrorCode::conflicting_input
			));
		}
		disposition = NewClassicNetplaySubmissionDisposition::duplicate;
	} else {
		(*slot)->remote = input;
	}
	if((*slot)->executed && (*slot)->resolved_remote &&
		*(*slot)->resolved_remote != input) {
		if(!impl_->rollback_frame || frame < *impl_->rollback_frame) {
			impl_->rollback_frame = frame;
			++impl_->stats.rollback_requests;
		}
	}
	impl_->advance_confirmation();
	return NewClassicNetplayRemoteSubmission{
		.disposition = disposition,
		.rollback_frame = impl_->rollback_frame,
		.confirmed_frame_count = impl_->confirmed_frame_count,
	};
}

std::expected<
	NewClassicNetplayResolvedFrame,
	NewClassicNetplayTimelineError
> NewClassicNetplayInputTimeline::resolve_next() noexcept
{
	if(impl_->rollback_frame) {
		return std::unexpected(timeline_error(
			NewClassicNetplayTimelineErrorCode::rollback_required
		));
	}
	auto slot = impl_->slot_for_write(impl_->next_frame);
	if(!slot) {
		return std::unexpected(slot.error());
	}
	if(!(*slot)->local) {
		return std::unexpected(timeline_error(
			NewClassicNetplayTimelineErrorCode::local_input_unavailable
		));
	}
	const auto depth = impl_->next_frame - impl_->confirmed_frame_count;
	if(depth >= new_classic_netplay_max_prediction) {
		return std::unexpected(timeline_error(
			NewClassicNetplayTimelineErrorCode::prediction_limit
		));
	}

	NewClassicNetplayOwnedInput remote{};
	const auto predicted = !(*slot)->remote.has_value();
	if((*slot)->remote) {
		remote = *(*slot)->remote;
	} else if(impl_->next_frame != 0U) {
		const auto *previous = impl_->slot_if_retained(impl_->next_frame - 1U);
		if(previous && previous->executed && previous->resolved_remote) {
			remote = *previous->resolved_remote;
			// Control markers are edges. Repeating one as a held-input
			// prediction would invent another pause after Resume.
			remote.control = NewClassicNetplayControl::none;
		}
	}
	const auto combined = impl_->local_player ==
		NewClassicNetplayPlayer::player_one ?
		combine_new_classic_netplay_inputs(*(*slot)->local, remote) :
		combine_new_classic_netplay_inputs(remote, *(*slot)->local);
	if(!combined) {
		return std::unexpected(NewClassicNetplayTimelineError{
			.code = NewClassicNetplayTimelineErrorCode::invalid_input,
			.input = combined.error(),
		});
	}
	(*slot)->executed = true;
	(*slot)->remote_predicted = predicted;
	(*slot)->resolved_remote = remote;
	(*slot)->combined = *combined;
	(*slot)->prediction_depth = static_cast<std::uint8_t>(depth);
	const NewClassicNetplayResolvedFrame result{
		.frame = impl_->next_frame,
		.local = *(*slot)->local,
		.remote = remote,
		.combined = *combined,
		.pause_requested =
			(*slot)->local->control ==
				NewClassicNetplayControl::pause_request ||
			remote.control == NewClassicNetplayControl::pause_request,
		.remote_predicted = predicted,
		.prediction_depth = static_cast<std::uint8_t>(depth),
	};
	++impl_->next_frame;
	++impl_->stats.resolved_frames;
	if(predicted) {
		++impl_->stats.predicted_frames;
	}
	impl_->advance_confirmation();
	return result;
}

std::expected<
	NewClassicNetplayResolvedFrame,
	NewClassicNetplayTimelineError
> NewClassicNetplayInputTimeline::resolve_next_exact() noexcept
{
	if(impl_->rollback_frame) {
		return std::unexpected(timeline_error(
			NewClassicNetplayTimelineErrorCode::rollback_required
		));
	}
	auto slot = impl_->slot_for_write(impl_->next_frame);
	if(!slot) {
		return std::unexpected(slot.error());
	}
	if(!(*slot)->local) {
		return std::unexpected(timeline_error(
			NewClassicNetplayTimelineErrorCode::local_input_unavailable
		));
	}
	if(!(*slot)->remote) {
		return std::unexpected(timeline_error(
			NewClassicNetplayTimelineErrorCode::remote_input_unavailable
		));
	}
	return resolve_next();
}

std::expected<void, NewClassicNetplayTimelineError>
NewClassicNetplayInputTimeline::rewind_to_rollback() noexcept
{
	if(!impl_->rollback_frame || *impl_->rollback_frame >= impl_->next_frame ||
		*impl_->rollback_frame < impl_->confirmed_frame_count) {
		return std::unexpected(timeline_error(
			NewClassicNetplayTimelineErrorCode::rollback_unavailable
		));
	}
	const auto frame = *impl_->rollback_frame;
	for(auto cursor = frame; cursor < impl_->next_frame; ++cursor) {
		auto *slot = impl_->slot_if_retained(cursor);
		if(!slot) {
			return std::unexpected(timeline_error(
				NewClassicNetplayTimelineErrorCode::rollback_unavailable
			));
		}
		slot->executed = false;
		slot->remote_predicted = false;
		slot->resolved_remote.reset();
		slot->combined = {};
		slot->prediction_depth = 0U;
	}
	impl_->next_frame = frame;
	impl_->rollback_frame.reset();
	++impl_->stats.rewinds;
	return {};
}

std::expected<void, NewClassicNetplayTimelineError>
NewClassicNetplayInputTimeline::discard_unconfirmed_suffix() noexcept
{
	if(impl_->rollback_frame ||
		impl_->confirmed_frame_count > impl_->next_frame) {
		return std::unexpected(timeline_error(
			NewClassicNetplayTimelineErrorCode::rollback_unavailable
		));
	}
	for(auto &slot : impl_->slots) {
		if(slot.valid && slot.frame >= impl_->next_frame) {
			slot = Impl::Slot{};
		}
	}
	return {};
}

std::optional<std::uint64_t>
NewClassicNetplayInputTimeline::rollback_frame() const noexcept
{
	return impl_->rollback_frame;
}

std::uint64_t NewClassicNetplayInputTimeline::next_frame() const noexcept
{
	return impl_->next_frame;
}

std::uint64_t
NewClassicNetplayInputTimeline::confirmed_frame_count() const noexcept
{
	return impl_->confirmed_frame_count;
}

std::optional<NewClassicNetplayResolvedFrame>
NewClassicNetplayInputTimeline::resolved_frame(std::uint64_t frame) const noexcept
{
	const auto *slot = impl_->slot_if_retained(frame);
	if(!slot || !slot->executed || !slot->local || !slot->resolved_remote) {
		return std::nullopt;
	}
	return NewClassicNetplayResolvedFrame{
		.frame = frame,
		.local = *slot->local,
		.remote = *slot->resolved_remote,
		.combined = slot->combined,
		.pause_requested = slot->local->control ==
				NewClassicNetplayControl::pause_request ||
			slot->resolved_remote->control ==
				NewClassicNetplayControl::pause_request,
		.remote_predicted = slot->remote_predicted,
		.prediction_depth = slot->prediction_depth,
	};
}

std::optional<NewClassicNetplayOwnedInput>
NewClassicNetplayInputTimeline::local_input(std::uint64_t frame) const noexcept
{
	const auto *slot = impl_->slot_if_retained(frame);
	return slot ? slot->local : std::nullopt;
}

NewClassicNetplayTimelineStats
NewClassicNetplayInputTimeline::stats() const noexcept
{
	return impl_->stats;
}

} // namespace th03::app
