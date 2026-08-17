#include "th03/app/new_classic_netplay_simulator.hpp"

#include <algorithm>
#include <array>
#include <limits>

namespace th03::app {
namespace {

[[nodiscard]] bool valid_player(NewClassicNetplayPlayer player) noexcept
{
	return player == NewClassicNetplayPlayer::player_one ||
		player == NewClassicNetplayPlayer::player_two;
}

[[nodiscard]] NewClassicNetplayPlayer other_player(
	NewClassicNetplayPlayer player
) noexcept
{
	return player == NewClassicNetplayPlayer::player_one ?
		NewClassicNetplayPlayer::player_two :
		NewClassicNetplayPlayer::player_one;
}

[[nodiscard]] NewClassicNetplaySimulatorError simulator_error(
	NewClassicNetplaySimulatorErrorCode code
) noexcept
{
	return NewClassicNetplaySimulatorError{.code = code};
}

} // namespace

struct NewClassicNetplayTransportSimulator::Impl final {
	struct Slot final {
		bool valid = false;
		bool duplicate = false;
		NewClassicNetplayPlayer sender =
			NewClassicNetplayPlayer::player_one;
		NewClassicNetplayPlayer recipient =
			NewClassicNetplayPlayer::player_two;
		std::uint64_t datagram_id = 0U;
		std::uint64_t delivery_frame = 0U;
		std::uint64_t enqueue_ordinal = 0U;
		std::size_t payload_size = 0U;
		std::array<std::byte, new_classic_netplay_simulated_mtu> payload{};
	};

	std::array<Slot, new_classic_netplay_simulated_queue> slots{};
	std::uint64_t next_datagram_id = 1U;
	std::uint64_t next_enqueue_ordinal = 0U;
	std::size_t queued = 0U;
	NewClassicNetplaySimulatorStats stats{};

	[[nodiscard]] std::expected<std::uint64_t,
		NewClassicNetplaySimulatorError> delivery_frame(
		std::uint64_t current,
		std::uint8_t delay
	) const noexcept
	{
		if(current > std::numeric_limits<std::uint64_t>::max() - delay) {
			return std::unexpected(simulator_error(
				NewClassicNetplaySimulatorErrorCode::frame_overflow
			));
		}
		return current + delay;
	}

	[[nodiscard]] Slot *free_slot() noexcept
	{
		const auto found = std::find_if(slots.begin(), slots.end(),
			[](const Slot &slot) { return !slot.valid; });
		return found == slots.end() ? nullptr : &*found;
	}

	void enqueue(
		Slot &slot,
		std::uint64_t datagram_id,
		std::uint64_t delivery_frame,
		NewClassicNetplayPlayer sender,
		std::span<const std::byte> payload,
		bool duplicate
	) noexcept
	{
		slot.valid = true;
		slot.duplicate = duplicate;
		slot.sender = sender;
		slot.recipient = other_player(sender);
		slot.datagram_id = datagram_id;
		slot.delivery_frame = delivery_frame;
		slot.enqueue_ordinal = next_enqueue_ordinal++;
		slot.payload_size = payload.size();
		std::copy(payload.begin(), payload.end(), slot.payload.begin());
		++queued;
		++stats.enqueued_copies;
	}
};

NewClassicNetplayTransportSimulator::NewClassicNetplayTransportSimulator() :
	impl_(std::make_unique<Impl>())
{}

NewClassicNetplayTransportSimulator::~NewClassicNetplayTransportSimulator() =
	default;

std::expected<std::uint64_t, NewClassicNetplaySimulatorError>
NewClassicNetplayTransportSimulator::send(
	std::uint64_t current_frame,
	NewClassicNetplayPlayer sender,
	std::span<const std::byte> payload,
	NewClassicNetplayDeliveryPlan delivery
) noexcept
{
	if(!valid_player(sender)) {
		return std::unexpected(simulator_error(
			NewClassicNetplaySimulatorErrorCode::invalid_endpoint
		));
	}
	if(payload.empty()) {
		return std::unexpected(simulator_error(
			NewClassicNetplaySimulatorErrorCode::empty_payload
		));
	}
	if(payload.size() > new_classic_netplay_simulated_mtu) {
		return std::unexpected(simulator_error(
			NewClassicNetplaySimulatorErrorCode::payload_too_large
		));
	}

	const auto primary_frame = impl_->delivery_frame(
		current_frame, delivery.primary_delay_frames
	);
	if(!primary_frame) {
		return std::unexpected(primary_frame.error());
	}
	std::optional<std::uint64_t> duplicate_frame{};
	if(delivery.duplicate_delay_frames) {
		auto frame = impl_->delivery_frame(
			current_frame, *delivery.duplicate_delay_frames
		);
		if(!frame) {
			return std::unexpected(frame.error());
		}
		duplicate_frame = *frame;
	}

	const auto copies = static_cast<std::size_t>(!delivery.drop_primary) +
		static_cast<std::size_t>(duplicate_frame.has_value());
	if(copies > impl_->slots.size() - impl_->queued) {
		return std::unexpected(simulator_error(
			NewClassicNetplaySimulatorErrorCode::queue_full
		));
	}
	const auto datagram_id = impl_->next_datagram_id++;
	++impl_->stats.sends;
	if(delivery.drop_primary) {
		++impl_->stats.primary_drops;
	} else {
		auto *slot = impl_->free_slot();
		impl_->enqueue(
			*slot, datagram_id, *primary_frame, sender, payload, false
		);
	}
	if(duplicate_frame) {
		auto *slot = impl_->free_slot();
		impl_->enqueue(
			*slot, datagram_id, *duplicate_frame, sender, payload, true
		);
		++impl_->stats.duplicate_copies;
	}
	return datagram_id;
}

std::expected<
	std::optional<NewClassicNetplaySimulatedDatagram>,
	NewClassicNetplaySimulatorError
> NewClassicNetplayTransportSimulator::receive(
	std::uint64_t current_frame,
	NewClassicNetplayPlayer recipient,
	std::span<std::byte> output
) noexcept
{
	if(!valid_player(recipient)) {
		return std::unexpected(simulator_error(
			NewClassicNetplaySimulatorErrorCode::invalid_endpoint
		));
	}
	Impl::Slot *selected = nullptr;
	for(auto &slot : impl_->slots) {
		if(!slot.valid || slot.recipient != recipient ||
			slot.delivery_frame > current_frame) {
			continue;
		}
		if(!selected || slot.delivery_frame < selected->delivery_frame ||
			(slot.delivery_frame == selected->delivery_frame &&
				slot.enqueue_ordinal < selected->enqueue_ordinal)) {
			selected = &slot;
		}
	}
	if(!selected) {
		return std::optional<NewClassicNetplaySimulatedDatagram>{};
	}
	if(output.size() < selected->payload_size) {
		return std::unexpected(simulator_error(
			NewClassicNetplaySimulatorErrorCode::output_too_small
		));
	}
	std::copy_n(
		selected->payload.begin(), selected->payload_size, output.begin()
	);
	const NewClassicNetplaySimulatedDatagram result{
		.datagram_id = selected->datagram_id,
		.delivery_frame = selected->delivery_frame,
		.sender = selected->sender,
		.payload_size = selected->payload_size,
		.duplicate = selected->duplicate,
	};
	selected->valid = false;
	--impl_->queued;
	++impl_->stats.delivered_copies;
	return std::optional<NewClassicNetplaySimulatedDatagram>{result};
}

NewClassicNetplaySimulatorStats
NewClassicNetplayTransportSimulator::stats() const noexcept
{
	return impl_->stats;
}

std::size_t NewClassicNetplayTransportSimulator::queued() const noexcept
{
	return impl_->queued;
}

} // namespace th03::app
