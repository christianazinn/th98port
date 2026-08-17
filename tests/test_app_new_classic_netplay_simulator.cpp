#include "th03/app/new_classic_netplay_simulator.hpp"
#include "th03/app/new_classic_netplay_timeline.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string_view>

namespace {

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::fprintf(stderr, "FAIL: %.*s\n",
			static_cast<int>(message.size()), message.data());
	}
}

bool same_core_input(const th03::CoreInput &left, const th03::CoreInput &right)
{
	return left.p1 == right.p1 && left.p2 == right.p2 &&
		left.system == right.system && left.charge_mask == right.charge_mask;
}

void test_transport_order_and_bounds()
{
	using namespace th03::app;
	NewClassicNetplayTransportSimulator transport;
	constexpr std::array first{std::byte{1U}};
	constexpr std::array second{std::byte{2U}};
	constexpr std::array third{std::byte{3U}};
	const auto id_first = transport.send(
		0U, NewClassicNetplayPlayer::player_one, first,
		{.primary_delay_frames = 3U}
	);
	const auto id_second = transport.send(
		0U, NewClassicNetplayPlayer::player_one, second,
		{.primary_delay_frames = 1U}
	);
	const auto id_third = transport.send(
		0U, NewClassicNetplayPlayer::player_one, third,
		{
			.primary_delay_frames = 1U,
			.drop_primary = true,
			.duplicate_delay_frames = std::uint8_t{0U},
		}
	);
	expect(id_first && id_second && id_third &&
		*id_first < *id_second && *id_second < *id_third,
		"simulator assigns stable logical datagram IDs before reordering");

	std::array<std::byte, new_classic_netplay_simulated_mtu> output{};
	auto received = transport.receive(
		0U, NewClassicNetplayPlayer::player_two, output
	);
	expect(received && *received && (*received)->datagram_id == *id_third &&
		(*received)->duplicate && output[0U] == third[0U],
		"earliest duplicate arrives before later primary deliveries");
	received = transport.receive(
		1U, NewClassicNetplayPlayer::player_two, output
	);
	expect(received && *received && (*received)->datagram_id == *id_second &&
		!(*received)->duplicate && output[0U] == second[0U],
		"equal-frame delivery retains enqueue order");
	received = transport.receive(
		3U, NewClassicNetplayPlayer::player_two, output
	);
	expect(received && *received && (*received)->datagram_id == *id_first &&
		output[0U] == first[0U] && transport.queued() == 0U,
		"reordered primary eventually drains exactly once");

	constexpr std::array<std::byte, new_classic_netplay_simulated_mtu + 1U>
		oversized{};
	const auto rejected = transport.send(
		0U, NewClassicNetplayPlayer::player_two, oversized, {}
	);
	expect(!rejected && rejected.error().code ==
		NewClassicNetplaySimulatorErrorCode::payload_too_large,
		"simulator rejects datagrams above the no-fragmentation budget");
	const auto stats = transport.stats();
	expect(stats.sends == 3U && stats.primary_drops == 1U &&
		stats.duplicate_copies == 1U && stats.enqueued_copies == 3U &&
		stats.delivered_copies == 3U,
		"simulator reports deterministic drop, duplicate, and delivery counts");
}

void test_prediction_correction_and_limit()
{
	using namespace th03::app;
	auto timeline = NewClassicNetplayInputTimeline::create(
		NewClassicNetplayPlayer::player_one
	);
	expect(static_cast<bool>(timeline), "valid local owner creates a timeline");
	if(!timeline) {
		return;
	}
	expect((*timeline)->submit_local(0U, {.word = 0x0001U}).has_value(),
		"timeline accepts frame-zero local input");
	const auto predicted = (*timeline)->resolve_next();
	expect(predicted && predicted->frame == 0U &&
		predicted->remote_predicted && predicted->remote.word == 0U &&
		predicted->combined.p1 == 0x0001U && predicted->combined.p2 == 0U,
		"first missing remote input predicts neutral");
	const auto corrected = (*timeline)->submit_remote(
		0U, {.word = 0x0020U, .charge = 1U}
	);
	expect(corrected && corrected->rollback_frame == 0U &&
		(*timeline)->confirmed_frame_count() == 0U,
		"different late input requests rollback without false confirmation");
	expect((*timeline)->rewind_to_rollback().has_value(),
		"timeline rewinds to its earliest requested correction");
	const auto replayed = (*timeline)->resolve_next();
	expect(replayed && !replayed->remote_predicted &&
		replayed->combined.p2 == 0x0020U &&
		replayed->combined.charge_mask == 0x02U &&
		(*timeline)->confirmed_frame_count() == 1U,
		"corrected execution advances the contiguous confirmation frontier");

	auto exact = NewClassicNetplayInputTimeline::create(
		NewClassicNetplayPlayer::player_one
	);
	expect(exact && (*exact)->submit_local(0U, {.word = 0x0002U}),
		"reliable-phase fixture stages its local input");
	if(exact) {
		const auto waiting = (*exact)->resolve_next_exact();
		expect(!waiting && waiting.error().code ==
			NewClassicNetplayTimelineErrorCode::remote_input_unavailable &&
			(*exact)->next_frame() == 0U,
			"reliable resolution waits without predicting or consuming frame zero");
		expect((*exact)->submit_remote(0U, {.word = 0x0008U}).has_value(),
			"reliable fixture accepts the matching remote value");
		const auto resolved = (*exact)->resolve_next_exact();
		expect(resolved && !resolved->remote_predicted &&
			resolved->combined.p1 == 0x0002U &&
			resolved->combined.p2 == 0x0008U &&
			(*exact)->confirmed_frame_count() == 1U,
			"reliable resolution consumes only two real values and confirms them");
	}

	auto capped = NewClassicNetplayInputTimeline::create(
		NewClassicNetplayPlayer::player_two
	);
	if(!capped) {
		expect(false, "second valid timeline constructs");
		return;
	}
	for(std::uint64_t frame = 0U;
		frame < new_classic_netplay_max_prediction; ++frame) {
		expect((*capped)->submit_local(frame, {}).has_value(),
			"prediction-limit fixture accepts local input");
		expect((*capped)->resolve_next().has_value(),
			"prediction-limit fixture advances within the cap");
	}
	expect((*capped)->submit_local(new_classic_netplay_max_prediction, {})
		.has_value(), "prediction-limit fixture stages the boundary input");
	expect((*capped)->submit_remote(
		new_classic_netplay_max_prediction, {.word = 0x0004U}
	).has_value(), "an exact newer input may arrive beyond an older gap");
	const auto stalled = (*capped)->resolve_next();
	expect(!stalled && stalled.error().code ==
		NewClassicNetplayTimelineErrorCode::prediction_limit,
		"timeline caps the whole unconfirmed window despite a newer exact input");

	auto pause = NewClassicNetplayInputTimeline::create(
		NewClassicNetplayPlayer::player_one
	);
	expect(static_cast<bool>(pause),
		"pause-marker correction fixture constructs");
	if(pause) {
		expect((*pause)->submit_local(0U, {}).has_value() &&
			(*pause)->submit_remote(0U, {}).has_value() &&
			(*pause)->resolve_next().has_value(),
			"pause fixture confirms its neutral prefix");
		expect((*pause)->submit_local(1U, {}).has_value() &&
			(*pause)->resolve_next().has_value() &&
			(*pause)->submit_local(2U, {.word = 0x0001U}).has_value() &&
			(*pause)->resolve_next().has_value(),
			"pause fixture predicts a two-frame neutral remote suffix");
		const auto marker = (*pause)->submit_remote(1U, {
			.control = NewClassicNetplayControl::pause_request,
		});
		expect(marker && marker->rollback_frame == 1U &&
			(*pause)->confirmed_frame_count() == 1U,
			"late Pause marker requests correction at its exact frame");
		expect((*pause)->rewind_to_rollback().has_value(),
			"late Pause marker rewinds the speculative suffix");
		const auto corrected_pause = (*pause)->resolve_next();
		expect(corrected_pause && corrected_pause->pause_requested &&
			!corrected_pause->remote_predicted &&
			corrected_pause->combined.system == 0U &&
			(*pause)->confirmed_frame_count() == 2U,
			"corrected Pause remains typed beside CoreInput until session projection");
		expect((*pause)->discard_unconfirmed_suffix().has_value() &&
			!(*pause)->local_input(2U),
			"Pause correction discards inputs sampled after the frozen boundary");
		expect((*pause)->submit_local(2U, {.word = 0x0004U}).has_value(),
			"Resume opens a fresh local slot after the discarded suffix");
		const auto after_resume = (*pause)->resolve_next();
		expect(after_resume && after_resume->remote_predicted &&
			!after_resume->pause_requested &&
			after_resume->remote.control == NewClassicNetplayControl::none,
			"prediction repeats held gameplay input but never repeats Pause");
	}
}

struct DatagramOwner final {
	std::uint64_t frame = 0U;
	th03::app::NewClassicNetplayPlayer sender =
		th03::app::NewClassicNetplayPlayer::player_one;
};

th03::app::NewClassicNetplayOwnedInput p1_input(std::uint64_t frame)
{
	return {
		.word = static_cast<std::uint16_t>(
			(frame % 2U ? 0x0001U : 0x0004U) |
			(frame % 5U == 0U ? 0x0020U : 0U)
		),
		.charge = static_cast<std::uint8_t>(frame % 3U == 0U),
	};
}

th03::app::NewClassicNetplayOwnedInput p2_input(std::uint64_t frame)
{
	return {
		.word = static_cast<std::uint16_t>(
			(frame % 3U ? 0x0002U : 0x0008U) |
			(frame % 7U == 0U ? 0x0010U : 0U)
		),
		.charge = static_cast<std::uint8_t>(frame % 4U == 1U),
	};
}

void test_two_peer_convergence()
{
	using namespace th03::app;
	constexpr std::uint64_t frames = 48U;
	NewClassicNetplayTransportSimulator transport;
	auto peer_one = NewClassicNetplayInputTimeline::create(
		NewClassicNetplayPlayer::player_one
	);
	auto peer_two = NewClassicNetplayInputTimeline::create(
		NewClassicNetplayPlayer::player_two
	);
	expect(peer_one && peer_two, "both peer timelines construct");
	if(!peer_one || !peer_two) {
		return;
	}
	std::array<std::optional<DatagramOwner>, 128U> owners{};
	std::array<std::byte, new_classic_netplay_simulated_mtu> payload{};

	auto send_input = [&](std::uint64_t host_frame,
		NewClassicNetplayPlayer sender,
		std::uint64_t input_frame,
		NewClassicNetplayOwnedInput input,
		NewClassicNetplayDeliveryPlan plan) {
		const auto encoded = encode_new_classic_netplay_input(input);
		if(!encoded) {
			expect(false, "generated peer input encodes");
			return;
		}
		const auto sent = transport.send(host_frame, sender, *encoded, plan);
		if(!sent || *sent >= owners.size()) {
			expect(false, "scripted peer input enters bounded transport");
			return;
		}
		owners[*sent] = DatagramOwner{input_frame, sender};
	};

	auto repair = [&](NewClassicNetplayInputTimeline &timeline) {
		if(!timeline.rollback_frame()) {
			return;
		}
		const auto target = timeline.next_frame();
		const auto rewound = timeline.rewind_to_rollback();
		expect(static_cast<bool>(rewound),
			"late delivery rewinds the affected peer timeline");
		if(!rewound) {
			return;
		}
		while(timeline.next_frame() < target) {
			expect(timeline.resolve_next().has_value(),
				"peer recomputes every discarded predicted input");
		}
	};

	auto deliver = [&](std::uint64_t host_frame,
		NewClassicNetplayPlayer recipient,
		NewClassicNetplayInputTimeline &timeline) {
		for(;;) {
			auto received = transport.receive(host_frame, recipient, payload);
			if(!received) {
				expect(false, "bounded simulator receive succeeds");
				return;
			}
			if(!*received) {
				return;
			}
			const auto &metadata = **received;
			if(metadata.datagram_id >= owners.size() ||
				!owners[metadata.datagram_id]) {
				expect(false, "delivered datagram retains its frame owner");
				return;
			}
			const auto decoded = decode_new_classic_netplay_input(
				std::span(payload).first(metadata.payload_size)
			);
			if(!decoded) {
				expect(false, "delivered T3NET002 value decodes");
				return;
			}
			const auto submitted = timeline.submit_remote(
				owners[metadata.datagram_id]->frame, *decoded
			);
			expect(static_cast<bool>(submitted),
				"duplicate/reordered remote input remains idempotent");
			if(!submitted) {
				return;
			}
			repair(timeline);
		}
	};

	for(std::uint64_t frame = 0U; frame < frames; ++frame) {
		expect((*peer_one)->submit_local(frame, p1_input(frame)).has_value() &&
			(*peer_two)->submit_local(frame, p2_input(frame)).has_value(),
			"both peers stage their independently owned input");
		const auto p1_drop = frame % 13U == 5U;
		const auto p2_drop = frame % 11U == 3U;
		send_input(frame, NewClassicNetplayPlayer::player_one, frame,
			p1_input(frame), {
				.primary_delay_frames = static_cast<std::uint8_t>((frame * 3U) % 6U),
				.drop_primary = p1_drop,
				.duplicate_delay_frames = p1_drop ?
					std::optional<std::uint8_t>{std::uint8_t{7U}} :
					(frame % 7U == 0U ?
						std::optional<std::uint8_t>{std::uint8_t{6U}} :
						std::nullopt),
			});
		send_input(frame, NewClassicNetplayPlayer::player_two, frame,
			p2_input(frame), {
				.primary_delay_frames = static_cast<std::uint8_t>((frame * 5U) % 7U),
				.drop_primary = p2_drop,
				.duplicate_delay_frames = p2_drop ?
					std::optional<std::uint8_t>{std::uint8_t{8U}} :
					(frame % 9U == 0U ?
						std::optional<std::uint8_t>{std::uint8_t{5U}} :
						std::nullopt),
			});
		deliver(frame, NewClassicNetplayPlayer::player_one, **peer_one);
		deliver(frame, NewClassicNetplayPlayer::player_two, **peer_two);
		while((*peer_one)->next_frame() <= frame) {
			expect((*peer_one)->resolve_next().has_value(),
				"peer one advances within the prediction window");
		}
		while((*peer_two)->next_frame() <= frame) {
			expect((*peer_two)->resolve_next().has_value(),
				"peer two advances within the prediction window");
		}
	}
	for(std::uint64_t host_frame = frames; host_frame < frames + 12U;
		++host_frame) {
		deliver(host_frame, NewClassicNetplayPlayer::player_one, **peer_one);
		deliver(host_frame, NewClassicNetplayPlayer::player_two, **peer_two);
	}
	expect(transport.queued() == 0U,
		"all delayed and duplicated datagrams drain after the bounded tail");
	expect((*peer_one)->confirmed_frame_count() == frames &&
		(*peer_two)->confirmed_frame_count() == frames,
		"both peers confirm the complete contiguous history");
	for(std::uint64_t frame = 0U; frame < frames; ++frame) {
		const auto one = (*peer_one)->resolved_frame(frame);
		const auto two = (*peer_two)->resolved_frame(frame);
		const auto expected = combine_new_classic_netplay_inputs(
			p1_input(frame), p2_input(frame)
		);
		expect(one && two && expected &&
			same_core_input(one->combined, *expected) &&
			same_core_input(two->combined, *expected),
			"asymmetric delivery resolves the same exact CoreInput on both peers");
	}
	const auto transport_stats = transport.stats();
	const auto one_stats = (*peer_one)->stats();
	const auto two_stats = (*peer_two)->stats();
	expect(transport_stats.primary_drops > 0U &&
		transport_stats.duplicate_copies > 0U &&
		one_stats.predicted_frames > 0U && two_stats.predicted_frames > 0U &&
		one_stats.rewinds > 0U && two_stats.rewinds > 0U,
		"fixture actually exercises loss, duplication, prediction, and rollback");
}

} // namespace

int main()
{
	test_transport_order_and_bounds();
	test_prediction_correction_and_limit();
	test_two_peer_convergence();
	if(failures != 0U) {
		std::fprintf(stderr, "%zu of %zu checks failed\n", failures, checks);
		return 1;
	}
	std::printf("all %zu New Classic simulated netplay checks passed\n", checks);
	return 0;
}
