#include "th03/app/tick_input.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>

namespace {

using th03::app::LogicalTickInput;
using th03::app::PhysicalInputEvent;
using th03::app::PhysicalInputEventKind;
using th03::app::PhysicalInputSourceState;
using th03::app::RationalTickRate;
using th03::app::TickInputError;
using th03::app::TickInputCatchUpPolicy;
using th03::app::TickInputScheduler;
using th03::app::pc98_24khz_nominal_tick_rate;

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

PhysicalInputSourceState state(
	std::uint16_t p1 = 0U,
	std::uint16_t p2 = 0U,
	std::uint8_t charge_mask = 0U,
	std::uint16_t system = 0U,
	bool device_present = false
)
{
	return PhysicalInputSourceState{
		.p1 = p1,
		.p2 = p2,
		.system = system,
		.charge_mask = charge_mask,
		.device_present = device_present,
	};
}

PhysicalInputEvent event(
	std::uint64_t timestamp_ns,
	PhysicalInputEventKind kind,
	std::uint8_t source = 0U,
	PhysicalInputSourceState source_state = {}
)
{
	return PhysicalInputEvent{
		.timestamp_ns = timestamp_ns,
		.kind = kind,
		.source = source,
		.state = source_state,
	};
}

void expect_push(
	TickInputScheduler &scheduler,
	PhysicalInputEvent physical_event,
	const char *message
)
{
	expect(
		static_cast<bool>(scheduler.push_event(physical_event)),
		message
	);
}

LogicalTickInput expect_tick(
	TickInputScheduler &scheduler,
	std::uint64_t now_ns,
	const char *message
)
{
	const auto due = scheduler.pop_due(now_ns);
	expect(due && due->has_value(), message);
	if(!due || !due->has_value()) {
		return {};
	}
	return **due;
}

bool same_input(const th03::CoreInput &left, const th03::CoreInput &right)
{
	return left.p1 == right.p1 && left.p2 == right.p2 &&
		left.system == right.system && left.charge_mask == right.charge_mask;
}

void test_rate_validation_and_exact_rational_deadlines()
{
	expect(
		pc98_24khz_nominal_tick_rate.ticks == 526'315U &&
			pc98_24khz_nominal_tick_rate.seconds == 9'328U,
		"nominal PC-9801 24 kHz timing retains its reduced exact rational"
	);
	auto nominal_created = TickInputScheduler::create(
		pc98_24khz_nominal_tick_rate, 777U
	);
	expect(static_cast<bool>(nominal_created),
		"nominal PC-9801 cadence is representable by the scheduler");
	if(nominal_created) {
		auto nominal = std::move(*nominal_created);
		const auto first = expect_tick(
			nominal, 777U, "nominal PC-9801 first phase is immediately due"
		);
		expect(
			first.tick_index == 0U && first.deadline_ns == 777U &&
				nominal.next_deadline_ns() == 17'724'004U,
			"nominal first deadline is immediate and the next uses exact ceiling"
		);
	}
	for(const auto invalid : {
		RationalTickRate{0U, 1U},
		RationalTickRate{1U, 0U},
		RationalTickRate{1'000'000'001U, 1U},
	}) {
		const auto scheduler = TickInputScheduler::create(invalid, 0U);
		expect(
			!scheduler && scheduler.error() == TickInputError::invalid_rate,
			"invalid or sub-nanosecond rates are rejected"
		);
	}
	expect(
		static_cast<bool>(TickInputScheduler::create({1'000'000'000U, 1U}, 0U)),
		"one logical tick per nanosecond remains representable"
	);
	const auto invalid_policy = TickInputScheduler::create(
		{1U, 1U},
		0U,
		static_cast<TickInputCatchUpPolicy>(99U)
	);
	expect(
		!invalid_policy && invalid_policy.error() ==
			TickInputError::invalid_catch_up_policy,
		"unknown catch-up policies are rejected"
	);

	auto created = TickInputScheduler::create({3U, 2U}, 100U);
	expect(static_cast<bool>(created), "rational scheduler is created");
	if(!created) {
		return;
	}
	auto scheduler = std::move(*created);
	expect(
		scheduler.next_deadline_ns() == 100U,
		"first logical deadline is explicit"
	);
	const auto early = scheduler.pop_due(99U);
	expect(early && !early->has_value(), "a deadline is not sampled early");

	const auto tick0 = expect_tick(scheduler, 100U, "first deadline is due");
	expect(
		tick0.tick_index == 0U && tick0.deadline_ns == 100U,
		"first sample carries its index and exact deadline"
	);
	const auto tick1 = expect_tick(
		scheduler, 2'000'000'100ULL, "first overdue rational tick is retained"
	);
	const auto tick2 = expect_tick(
		scheduler, 2'000'000'100ULL, "second overdue rational tick is retained"
	);
	const auto tick3 = expect_tick(
		scheduler, 2'000'000'100ULL, "third overdue rational tick is retained"
	);
	expect(
		tick1.tick_index == 1U && tick1.deadline_ns == 666'666'767ULL &&
			tick2.tick_index == 2U && tick2.deadline_ns == 1'333'333'434ULL &&
			tick3.tick_index == 3U && tick3.deadline_ns == 2'000'000'100ULL,
		"rational deadlines use exact cumulative ceiling without drift"
	);
	const auto caught_up = scheduler.pop_due(2'000'000'100ULL);
	expect(
		caught_up && !caught_up->has_value() &&
			scheduler.rebase_after_modal(3'000'000'000ULL).has_value(),
		"a modal boundary rebases an idle live timeline"
	);
	const auto stale_modal_cutoff = scheduler.pop_due(2'000'000'100ULL);
	expect(
		!stale_modal_cutoff && stale_modal_cutoff.error() ==
			TickInputError::timestamp_regressed,
		"a host catch-up batch must end after a modal clock rebase"
	);
	const auto rebased = expect_tick(
		scheduler, 3'000'000'000ULL, "the rebased deadline is immediately due"
	);
	expect(
		rebased.tick_index == 4U && rebased.deadline_ns == 3'000'000'000ULL &&
			same_input(rebased.input, {}),
		"modal rebasing preserves logical order while releasing held input"
	);
	expect(
		caught_up && !caught_up->has_value(),
		"overdue polling stops only after every due boundary was returned"
	);

	auto long_created = TickInputScheduler::create({7U, 3U}, 123U);
	expect(static_cast<bool>(long_created), "long rational scheduler is created");
	if(!long_created) {
		return;
	}
	auto long_schedule = std::move(*long_created);
	constexpr std::uint64_t long_tick_count = 10'000U;
	constexpr std::uint64_t numerator_ns = 3'000'000'000ULL;
	for(std::uint64_t index = 0U; index < long_tick_count; ++index) {
		const auto expected_deadline = 123U +
			((index * numerator_ns) + 6U) / 7U;
		const auto absolute = expect_tick(
			long_schedule,
			std::numeric_limits<std::uint64_t>::max(),
			"long rational deadline remains due"
		);
		expect(
			absolute.tick_index == index &&
				absolute.deadline_ns == expected_deadline,
			"long rational schedule retains every absolute ceiling deadline"
		);
	}
}

void test_overdue_ticks_latch_each_boundary_independently()
{
	auto created = TickInputScheduler::create({100'000'000U, 1U}, 10U);
	expect(static_cast<bool>(created), "overdue test scheduler is created");
	if(!created) {
		return;
	}
	auto scheduler = std::move(*created);
	expect_push(scheduler, event(5U,
		PhysicalInputEventKind::source_state_changed, 0U, state(0x0008U)),
		"source zero initially contributes P1 Right");
	expect_push(scheduler, event(15U,
		PhysicalInputEventKind::source_state_changed, 0U, state(0x0028U)),
		"source zero recomputes P1 Right and Shot");
	expect_push(scheduler, event(25U,
		PhysicalInputEventKind::source_state_changed, 0U, state(0x0020U)),
		"source zero recomputes P1 Shot alone");
	expect_push(scheduler, event(25U,
		PhysicalInputEventKind::source_state_changed, 1U, state(0U, 0x0004U)),
		"equal-timestamp source one contributes P2 Left");
	expect_push(scheduler, event(35U,
		PhysicalInputEventKind::source_state_changed, 0U, state()),
		"source zero recomputes an empty state");
	expect_push(scheduler, event(45U,
		PhysicalInputEventKind::source_state_changed, 1U, state()),
		"source one recomputes an empty state");

	std::array<LogicalTickInput, 5U> ticks{};
	for(std::size_t index = 0U; index < ticks.size(); ++index) {
		ticks[index] = expect_tick(
			scheduler, 50U, "every overdue deadline is returned independently"
		);
	}
	const std::array expected_inputs{
		th03::CoreInput{.p1 = 0x0008U},
		th03::CoreInput{.p1 = 0x0028U},
		th03::CoreInput{.p1 = 0x0020U, .p2 = 0x0004U},
		th03::CoreInput{.p2 = 0x0004U},
		th03::CoreInput{},
	};
	bool full_sequence_matches = true;
	for(std::size_t index = 0U; index < ticks.size(); ++index) {
		full_sequence_matches = full_sequence_matches &&
			ticks[index].deadline_ns == (10U * (index + 1U)) &&
			same_input(ticks[index].input, expected_inputs[index]);
	}
	expect(
		full_sequence_matches,
		"overdue polling preserves the complete CoreInput timeline"
	);
	expect(
		scheduler.pending_event_count() == 0U,
		"all history through the last returned deadline is consumed"
	);
}

void test_live_policy_discards_missed_deadlines()
{
	auto created = TickInputScheduler::create(
		{100'000'000U, 1U},
		10U,
		TickInputCatchUpPolicy::discard_missed_deadlines
	);
	expect(static_cast<bool>(created), "live catch-up scheduler is created");
	if(!created) {
		return;
	}
	auto scheduler = std::move(*created);
	expect_push(scheduler, event(15U,
		PhysicalInputEventKind::source_state_changed, 0U, state(0x0008U)),
		"late live history retains its press");
	expect_push(scheduler, event(35U,
		PhysicalInputEventKind::source_state_changed, 0U, state(0x0020U)),
		"late live history retains its newest state");
	const auto rebased = expect_tick(
		scheduler, 50U, "late live observation emits one current sample"
	);
	expect(
		rebased.tick_index == 0U && rebased.deadline_ns == 50U &&
			rebased.input.p1 == 0x0020U &&
			scheduler.next_deadline_ns() == 60U &&
			scheduler.pending_event_count() == 0U,
		"live resync collapses only elapsed deadlines, not input history"
	);
	const auto no_backlog = scheduler.pop_due(50U);
	expect(
		no_backlog && !no_backlog->has_value(),
		"live resync leaves no overdue repayment burst"
	);
	const auto on_time = expect_tick(
		scheduler, 60U, "live cadence resumes at the next rational period"
	);
	expect(
		on_time.tick_index == 1U && on_time.deadline_ns == 60U &&
			on_time.input.p1 == 0x0020U,
		"live resync preserves logical indices and latched input"
	);
}

void test_same_source_duplicate_binding_snapshots()
{
	auto created = TickInputScheduler::create({100'000'000U, 1U}, 10U);
	expect(static_cast<bool>(created), "duplicate-binding scheduler is created");
	if(!created) {
		return;
	}
	auto scheduler = std::move(*created);
	// The platform mapper recomputes the entire source contribution after each
	// raw event. A and B both bind P1 Shot in this synthetic input sequence.
	expect_push(scheduler, event(1U,
		PhysicalInputEventKind::source_state_changed, 0U, state(0x0020U)),
		"A-down snapshot contributes Shot");
	expect_push(scheduler, event(11U,
		PhysicalInputEventKind::source_state_changed, 0U, state(0x0020U)),
		"A+B-down recomputation still contributes one Shot bit");
	expect_push(scheduler, event(21U,
		PhysicalInputEventKind::source_state_changed, 0U, state(0x0020U)),
		"A-up recomputation retains Shot while B remains down");
	expect_push(scheduler, event(31U,
		PhysicalInputEventKind::source_state_changed, 0U, state()),
		"B-up recomputation clears Shot");

	const auto a_down = expect_tick(scheduler, 10U, "A-down reaches its deadline");
	const auto both_down = expect_tick(
		scheduler, 20U, "A+B-down reaches its deadline"
	);
	const auto only_b = expect_tick(scheduler, 30U, "A-up reaches its deadline");
	const auto neither = expect_tick(scheduler, 40U, "B-up reaches its deadline");
	expect(
		a_down.input.p1 == 0x0020U && both_down.input.p1 == 0x0020U &&
			only_b.input.p1 == 0x0020U && neither.input.p1 == 0U,
		"same-device duplicate bindings cannot false-release a logical action"
	);
}

void test_input_domain_and_equal_timestamp_order()
{
	auto created = TickInputScheduler::create({1U, 1U}, 100U);
	expect(static_cast<bool>(created), "input-domain scheduler is created");
	if(!created) {
		return;
	}
	auto scheduler = std::move(*created);
	expect_push(scheduler, event(100U,
		PhysicalInputEventKind::source_state_changed, 0U,
		state(0x0F3FU, 0x0F3FU, 0x03U, 0x1000U)),
		"all donor player, system, and charge bits are accepted at the boundary");
	const auto tick = expect_tick(
		scheduler, 100U, "events exactly at a deadline are sampled"
	);
	expect(
		tick.input.p1 == 0x0F3FU && tick.input.p2 == 0x0F3FU &&
			tick.input.system == 0x1000U && tick.input.charge_mask == 0x03U,
		"cardinals, buttons, system controls, legacy diagonals, and charge bits are preserved"
	);
	expect_push(scheduler, event(101U,
		PhysicalInputEventKind::source_state_changed, 1U,
		state(0U, 0U, 0U, 0x2000U)),
		"a second source contributes the source-global OK bit");
	const auto combined_system = expect_tick(
		scheduler, 1'000'000'100U, "global system bits combine across sources"
	);
	expect(combined_system.input.system == 0x3000U,
		"Cancel and OK source contributions are ORed independently");

	auto ordered_created = TickInputScheduler::create({1U, 1U}, 100U);
	expect(static_cast<bool>(ordered_created), "ordering scheduler is created");
	if(!ordered_created) {
		return;
	}
	auto ordered = std::move(*ordered_created);
	expect_push(ordered, event(100U,
		PhysicalInputEventKind::source_state_changed, 0U, state(0x0020U)),
		"same-time held snapshot is queued first");
	expect_push(ordered, event(100U,
		PhysicalInputEventKind::source_state_changed, 0U, state()),
		"same-time empty snapshot is queued second");
	const auto ordered_tick = expect_tick(
		ordered, 100U, "same-time ordered snapshots are due"
	);
	expect(
		ordered_tick.input.p1 == 0U,
		"equal timestamps retain injection order through the final latch"
	);
}

void test_source_device_and_focus_release()
{
	auto created = TickInputScheduler::create({100'000'000U, 1U}, 10U);
	expect(static_cast<bool>(created), "release test scheduler is created");
	if(!created) {
		return;
	}
	auto scheduler = std::move(*created);
	expect_push(scheduler, event(1U,
		PhysicalInputEventKind::source_state_changed, 0U, state(0x0020U)),
		"source zero contributes P1 Shot");
	expect_push(scheduler, event(2U,
		PhysicalInputEventKind::source_state_changed, 1U, state(0x0020U)),
		"source one also contributes P1 Shot");
	expect(
		expect_tick(scheduler, 10U, "both sources are due").input.p1 == 0x0020U,
		"source contributions are ORed without duplicating logical bits"
	);
	expect_push(scheduler, event(11U,
		PhysicalInputEventKind::source_released, 0U),
		"source zero device removal is queued");
	expect(
		expect_tick(scheduler, 20U, "source zero removal is due").input.p1 ==
			0x0020U,
		"device removal clears only the named source"
	);
	expect_push(scheduler, event(21U,
		PhysicalInputEventKind::source_released, 1U),
		"source one device removal is queued");
	expect_push(scheduler, event(22U,
		PhysicalInputEventKind::source_state_changed, 0U,
		state(0x0004U, 0U, 0x01U, 0x0020U)),
		"source zero contributes P1 Left, source Shot, and charge before focus loss");
	expect_push(scheduler, event(23U,
		PhysicalInputEventKind::source_state_changed, 1U,
		state(0U, 0x0010U, 0x02U, 0x4000U)),
		"source one contributes P2 Bomb, source Q, and charge before focus loss");
	const auto before_focus = expect_tick(
		scheduler, 30U, "replacement source states are due"
	);
	expect(
		before_focus.input.p1 == 0x0004U &&
			before_focus.input.p2 == 0x0010U &&
			before_focus.input.system == 0x4020U &&
			before_focus.input.charge_mask == 0x03U,
		"independent source snapshots combine both players, input_sp, and charge bits"
	);
	expect_push(scheduler, event(31U,
		PhysicalInputEventKind::focus_released,
		std::numeric_limits<std::uint8_t>::max(), state(0xFFFFU, 0xFFFFU, 0xFFU)),
		"focus release ignores source-state payload fields");
	const auto after_focus = expect_tick(scheduler, 40U, "focus loss is due");
	expect(
		after_focus.input.p1 == 0U && after_focus.input.p2 == 0U &&
			after_focus.input.system == 0U && after_focus.input.charge_mask == 0U,
		"focus release clears every physical source contribution"
	);
}

void test_physical_source_domains_and_neutral_presence()
{
	auto created = TickInputScheduler::create({100'000'000U, 1U}, 10U);
	expect(static_cast<bool>(created), "physical-domain scheduler is created");
	if(!created) {
		return;
	}
	auto scheduler = std::move(*created);
	expect_push(scheduler, event(1U,
		PhysicalInputEventKind::source_state_changed, 0U,
		state(0x0104U, 0x0208U, 0x03U, 0x6032U)),
		"keyboard source retains all three donor words");
	expect_push(scheduler, event(2U,
		PhysicalInputEventKind::source_state_changed, 1U,
		state(0x0001U, 0U, 0U, 0U, false)),
		"held non-controller source does not imply joystick presence");
	expect_push(scheduler, event(3U,
		PhysicalInputEventKind::source_state_changed, 2U,
		state(0U, 0x0128U, 0U, 0x1000U, true)),
		"first connected controller source is published");
	expect_push(scheduler, event(4U,
		PhysicalInputEventKind::source_state_changed, 3U,
		state(0x0010U, 0U, 0U, 0U, true)),
		"second connected controller source is published");

	const auto held = expect_tick(
		scheduler, 10U, "physical source domains are due"
	);
	expect(held.physical_domains.has_value(),
		"scheduler-produced ticks identify their physical source domains");
	if(held.physical_domains) {
		expect(same_input(held.physical_domains->keyboard, {
			.p1 = 0x0104U,
			.p2 = 0x0208U,
			.system = 0x6032U,
			.charge_mask = 0x03U,
		}), "physical keyboard snapshot remains separate from controllers");
		expect(held.physical_domains->first_joystick_present &&
			held.physical_domains->first_joystick == 0x0028U,
			"lowest connected controller becomes masked PC-98 joystick zero");
	}
	expect(!LogicalTickInput{}.physical_domains.has_value(),
		"scripted and replay samples omit host physical domains by default");

	expect_push(scheduler, event(11U,
		PhysicalInputEventKind::focus_released),
		"focus release clears held words without disconnecting devices");
	const auto neutral = expect_tick(scheduler, 20U, "focus release is due");
	expect(neutral.input.p1 == 0U && neutral.input.p2 == 0U &&
		neutral.input.system == 0U && neutral.physical_domains &&
		neutral.physical_domains->first_joystick_present &&
		neutral.physical_domains->first_joystick == 0U,
		"neutral joystick presence survives focus release");

	expect_push(scheduler, event(21U,
		PhysicalInputEventKind::source_released, 2U),
		"first controller is physically removed");
	const auto promoted = expect_tick(
		scheduler, 30U, "controller removal is due"
	);
	expect(promoted.physical_domains &&
		promoted.physical_domains->first_joystick_present &&
		promoted.physical_domains->first_joystick == 0U,
		"next connected neutral controller is promoted to joystick zero");
}

void test_app_quit_is_separate_and_sticky()
{
	auto created = TickInputScheduler::create({100'000'000U, 1U}, 10U);
	expect(static_cast<bool>(created), "quit test scheduler is created");
	if(!created) {
		return;
	}
	auto scheduler = std::move(*created);
	expect_push(scheduler, event(1U,
		PhysicalInputEventKind::source_state_changed, 0U, state(0x0001U)),
		"gameplay source state is queued");
	const auto gameplay = expect_tick(scheduler, 10U, "gameplay state is due");
	expect(
		gameplay.input.p1 == 0x0001U && gameplay.input.system == 0U &&
			!gameplay.app_quit_requested && !scheduler.app_quit_requested(),
		"ordinary player input cannot synthesize application quit"
	);
	expect_push(scheduler, event(11U, PhysicalInputEventKind::app_quit,
		std::numeric_limits<std::uint8_t>::max(),
		state(0xFFFFU, 0xFFFFU, 0xFFU)),
		"application quit ignores source-state payload fields");
	const auto quit = expect_tick(scheduler, 20U, "application quit is due");
	expect(
		quit.input.p1 == 0x0001U && quit.input.system == 0U &&
			quit.app_quit_requested && scheduler.app_quit_requested(),
		"application quit is exposed separately from CoreInput"
	);
	const auto later = expect_tick(scheduler, 30U, "post-quit tick is due");
	expect(
		later.app_quit_requested && later.input.system == 0U,
		"application quit remains sticky without mutating gameplay input"
	);
}

void test_public_event_validation_is_atomic()
{
	auto created = TickInputScheduler::create({1U, 1U}, 1'000U);
	expect(static_cast<bool>(created), "validation scheduler is created");
	if(!created) {
		return;
	}
	auto scheduler = std::move(*created);
	const auto invalid_kind = scheduler.push_event(event(
		100U, static_cast<PhysicalInputEventKind>(0xFFU)
	));
	expect(
		!invalid_kind && invalid_kind.error() == TickInputError::invalid_event_kind &&
			scheduler.pending_event_count() == 0U,
		"malformed public event kinds are rejected without queue mutation"
	);
	expect_push(scheduler, event(10U,
		PhysicalInputEventKind::source_state_changed, 0U, state(0x0001U)),
		"invalid-kind rejection does not advance the timestamp guard");

	const auto invalid_source = scheduler.push_event(event(
		100U,
		PhysicalInputEventKind::source_state_changed,
		static_cast<std::uint8_t>(th03::app::physical_input_source_count),
		state(0x0002U)
	));
	expect(
		!invalid_source && invalid_source.error() == TickInputError::invalid_source &&
			scheduler.pending_event_count() == 1U,
		"out-of-range source rejection is atomic"
	);
	expect_push(scheduler, event(20U,
		PhysicalInputEventKind::source_state_changed, 0U, state(0x0002U)),
		"invalid-source rejection does not advance the timestamp guard");

	for(const auto invalid_state : {
		state(0x0040U),
		state(0U, 0x0080U),
		state(0U, 0U, 0x04U),
		state(0U, 0U, 0U, 0x8000U),
	}) {
		const auto invalid_bits = scheduler.push_event(event(
			100U,
			PhysicalInputEventKind::source_state_changed,
			0U,
			invalid_state
		));
		expect(
			!invalid_bits &&
				invalid_bits.error() == TickInputError::invalid_input_bits &&
				scheduler.pending_event_count() == 2U,
			"unsupported player, system, or charge bits are rejected atomically"
		);
	}
	expect_push(scheduler, event(30U,
		PhysicalInputEventKind::source_state_changed, 0U, state()),
		"invalid-bit rejection does not advance the timestamp guard");

	const auto regressed = scheduler.push_event(event(
		29U, PhysicalInputEventKind::source_released, 0U
	));
	expect(
		!regressed && regressed.error() == TickInputError::timestamp_regressed &&
			scheduler.pending_event_count() == 3U,
		"accepted physical-event timestamps are monotonically guarded"
	);

	auto full_created = TickInputScheduler::create({1U, 1U}, 1'000U);
	expect(static_cast<bool>(full_created), "capacity scheduler is created");
	if(!full_created) {
		return;
	}
	auto full = std::move(*full_created);
	for(std::size_t index = 0U;
		index < th03::app::pending_physical_event_capacity;
		++index) {
		expect_push(full, event(1'001U + static_cast<std::uint64_t>(index),
			PhysicalInputEventKind::source_state_changed, 0U, state(0x0001U)),
			"fixed event queue accepts every documented slot");
	}
	const auto queue_full = full.push_event(event(
		1'257U, PhysicalInputEventKind::source_state_changed, 0U, state()
	));
	expect(
		!queue_full && queue_full.error() == TickInputError::event_queue_full &&
			full.pending_event_count() == th03::app::pending_physical_event_capacity,
		"a full fixed queue reports an error instead of dropping an event"
	);
	const auto full_tick = expect_tick(
		full, 1'000U, "a full future history leaves the earlier deadline visible"
	);
	expect(
		full_tick.input.p1 == 0U &&
			full.pending_event_count() == th03::app::pending_physical_event_capacity,
		"a rejected future event cannot make later history arrive early"
	);
	const auto future_tick = expect_tick(
		full, 1'000'001'000ULL, "the full future history reaches its own deadline"
	);
	expect(
		future_tick.input.p1 == 0x0001U && full.pending_event_count() == 0U,
		"consuming a full ring preserves its final snapshot and frees every slot"
	);
	expect_push(full, event(1'000'001'001ULL,
		PhysicalInputEventKind::source_state_changed, 0U, state()),
		"the fixed event ring accepts events after wrapping its head");
	const auto wrapped_tick = expect_tick(
		full, 2'000'001'000ULL, "the reused ring event reaches the next deadline"
	);
	expect(
		wrapped_tick.input.p1 == 0U && full.pending_event_count() == 0U,
		"wrapped event storage retains the exact source snapshot"
	);

	auto folded_created = TickInputScheduler::create({1U, 1U}, 2'000U);
	expect(static_cast<bool>(folded_created),
		"same-boundary capacity scheduler is created");
	if(!folded_created) {
		return;
	}
	auto folded = std::move(*folded_created);
	for(std::size_t index = 0U;
		index < th03::app::pending_physical_event_capacity + 1U;
		++index) {
		expect_push(folded, event(
			2'000U,
			PhysicalInputEventKind::source_state_changed,
			0U,
			state((index % 2U) == 0U ? 0x0020U : 0U)
		), "same-boundary input burst is accepted without losing an edge");
	}
	const auto folded_tick = expect_tick(
		folded, 2'000U, "capacity-plus-one burst reaches one logical boundary"
	);
	expect(
		folded_tick.input.p1 == 0x0020U && folded.pending_event_count() == 0U,
		"same-boundary compaction preserves injection order and the final snapshot"
	);
}

void test_retroactive_event_clock_regression_and_deadline_overflow()
{
	auto clock_created = TickInputScheduler::create({1U, 1U}, 100U);
	expect(static_cast<bool>(clock_created), "clock validation scheduler is created");
	if(!clock_created) {
		return;
	}
	auto clock = std::move(*clock_created);
	const auto not_due = clock.pop_due(99U);
	expect(not_due && !not_due->has_value(), "initial host-clock observation is accepted");
	const auto clock_regressed = clock.pop_due(98U);
	expect(
		!clock_regressed &&
			clock_regressed.error() == TickInputError::timestamp_regressed &&
			clock.next_deadline_ns() == 100U,
		"regressed host-clock observation is rejected without scheduler mutation"
	);
	const auto clock_tick = expect_tick(
		clock, 100U, "clock can advance after a rejected regression"
	);
	expect(
		clock_tick.tick_index == 0U && clock_tick.deadline_ns == 100U,
		"clock regression neither consumes nor shifts a logical tick"
	);

	auto created = TickInputScheduler::create({1U, 1U}, 100U);
	expect(static_cast<bool>(created), "retroactive-event scheduler is created");
	if(!created) {
		return;
	}
	auto scheduler = std::move(*created);
	(void)expect_tick(scheduler, 100U, "initial tick is emitted");
	const auto retroactive = scheduler.push_event(event(
		100U, PhysicalInputEventKind::source_state_changed, 0U, state(0x0020U)
	));
	expect(
		!retroactive && retroactive.error() == TickInputError::event_already_due &&
			scheduler.pending_event_count() == 0U,
		"events at an emitted boundary are rejected rather than rewriting history"
	);
	expect_push(scheduler, event(101U,
		PhysicalInputEventKind::source_state_changed, 0U, state(0x0020U)),
		"event rejection leaves later valid injection available");

	auto live_created = TickInputScheduler::create(
		{100'000'000U, 1U}, 100U
	);
	expect(static_cast<bool>(live_created), "live-event scheduler is created");
	if(live_created) {
		auto live = std::move(*live_created);
		(void)expect_tick(live, 100U, "live scheduler emits its first boundary");
		expect(static_cast<bool>(live.push_live_event(event(
			99U, PhysicalInputEventKind::source_state_changed,
			0U, state(0x0020U)
		))), "late live press is deferred instead of rejected");
		const auto pressed = expect_tick(
			live, 110U, "deferred live press reaches the first unemitted boundary"
		);
		expect(pressed.input.p1 == 0x0020U,
			"late live press never changes the already emitted tick");
		expect(static_cast<bool>(live.push_live_event(event(
			100U, PhysicalInputEventKind::source_state_changed, 0U, state()
		))), "late live release is also deferred without timestamp failure");
		const auto released = expect_tick(
			live, 120U, "deferred live release reaches the following boundary"
		);
		expect(released.input.p1 == 0U,
			"rapid late press/release retains physical polling order");
	}

	constexpr auto maximum = std::numeric_limits<std::uint64_t>::max();
	auto overflow_created = TickInputScheduler::create(
		{1'000'000'000U, 1U}, maximum - 1U
	);
	expect(static_cast<bool>(overflow_created), "overflow scheduler is created");
	if(!overflow_created) {
		return;
	}
	auto overflow = std::move(*overflow_created);
	const auto penultimate = expect_tick(
		overflow, maximum, "penultimate timestamp is returned"
	);
	const auto terminal = expect_tick(
		overflow, maximum, "maximum timestamp is returned before overflow"
	);
	expect(
		penultimate.tick_index == 0U && penultimate.deadline_ns == maximum - 1U &&
			terminal.tick_index == 1U && terminal.deadline_ns == maximum,
		"deadline overflow never consumes or hides the due terminal tick"
	);
	expect(
		!overflow.next_deadline_ns().has_value(),
		"no nonexistent post-overflow deadline is exposed"
	);
	const auto exhausted = overflow.pop_due(maximum);
	expect(
		!exhausted && exhausted.error() == TickInputError::deadline_overflow,
		"the call after the returned terminal tick reports deadline overflow"
	);
}

} // namespace

int main()
{
	test_rate_validation_and_exact_rational_deadlines();
	test_overdue_ticks_latch_each_boundary_independently();
	test_live_policy_discards_missed_deadlines();
	test_same_source_duplicate_binding_snapshots();
	test_input_domain_and_equal_timestamp_order();
	test_source_device_and_focus_release();
	test_physical_source_domains_and_neutral_presence();
	test_app_quit_is_separate_and_sticky();
	test_public_event_validation_is_atomic();
	test_retroactive_event_clock_regression_and_deadline_overflow();

	if(failures != 0U) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << "All " << checks << " app tick/input checks passed\n";
	return 0;
}
