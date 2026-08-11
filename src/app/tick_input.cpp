#include "th03/app/tick_input.hpp"

#include <limits>

namespace th03::app {
namespace {

constexpr std::uint64_t nanoseconds_per_second = 1'000'000'000ULL;
constexpr std::uint8_t last_event_kind = static_cast<std::uint8_t>(
	PhysicalInputEventKind::app_quit
);
constexpr std::uint16_t valid_player_input_bits = 0x0F3FU;
constexpr std::uint16_t valid_system_input_bits = 0x7F3FU;
constexpr std::uint8_t valid_charge_mask_bits = 0x03U;

constexpr bool valid_event_kind(PhysicalInputEventKind kind) noexcept
{
	return static_cast<std::uint8_t>(kind) <= last_event_kind;
}

constexpr bool valid_source_state(const PhysicalInputSourceState &state) noexcept
{
	return (state.p1 & static_cast<std::uint16_t>(~valid_player_input_bits)) == 0U &&
		(state.p2 & static_cast<std::uint16_t>(~valid_player_input_bits)) == 0U &&
		(state.system & static_cast<std::uint16_t>(~valid_system_input_bits)) == 0U &&
		(state.charge_mask & static_cast<std::uint8_t>(~valid_charge_mask_bits)) == 0U;
}

constexpr bool event_uses_source(PhysicalInputEventKind kind) noexcept
{
	return kind == PhysicalInputEventKind::source_state_changed ||
		kind == PhysicalInputEventKind::source_released;
}

constexpr bool event_uses_state(PhysicalInputEventKind kind) noexcept
{
	return kind == PhysicalInputEventKind::source_state_changed;
}

} // namespace

TickInputScheduler::TickInputScheduler(
	RationalTickRate rate,
	TickTimestampNs first_deadline_ns,
	std::uint64_t period_numerator_ns,
	TickInputCatchUpPolicy catch_up_policy
) noexcept :
	rate_(rate),
	period_numerator_ns_(period_numerator_ns),
	catch_up_policy_(catch_up_policy),
	fractional_accumulator_(static_cast<std::uint64_t>(rate.ticks) - 1U),
	next_deadline_ns_(first_deadline_ns)
{}

std::expected<TickInputScheduler, TickInputError> TickInputScheduler::create(
	RationalTickRate rate,
	TickTimestampNs first_deadline_ns,
	TickInputCatchUpPolicy catch_up_policy
) noexcept
{
	if(rate.ticks == 0U || rate.seconds == 0U) {
		return std::unexpected(TickInputError::invalid_rate);
	}
	if(catch_up_policy != TickInputCatchUpPolicy::preserve_all &&
		catch_up_policy != TickInputCatchUpPolicy::discard_missed_deadlines) {
		return std::unexpected(TickInputError::invalid_catch_up_policy);
	}
	const auto period_numerator_ns =
		static_cast<std::uint64_t>(rate.seconds) * nanoseconds_per_second;
	// Integer nanoseconds cannot represent distinct boundaries above 1 GHz.
	if(period_numerator_ns < static_cast<std::uint64_t>(rate.ticks)) {
		return std::unexpected(TickInputError::invalid_rate);
	}
	return TickInputScheduler(
		rate,
		first_deadline_ns,
		period_numerator_ns,
		catch_up_policy
	);
}

std::expected<void, TickInputError> TickInputScheduler::push_event(
	PhysicalInputEvent event
) noexcept
{
	if(!valid_event_kind(event.kind)) {
		return std::unexpected(TickInputError::invalid_event_kind);
	}
	if(
		event_uses_source(event.kind) &&
		event.source >= physical_input_source_count
	) {
		return std::unexpected(TickInputError::invalid_source);
	}
	if(event_uses_state(event.kind) && !valid_source_state(event.state)) {
		return std::unexpected(TickInputError::invalid_input_bits);
	}
	if(
		last_event_timestamp_ &&
		event.timestamp_ns < *last_event_timestamp_
	) {
		return std::unexpected(TickInputError::timestamp_regressed);
	}
	if(
		last_emitted_deadline_ &&
		event.timestamp_ns <= *last_emitted_deadline_
	) {
		return std::unexpected(TickInputError::event_already_due);
	}
	// A host event queue can contain more edges for the upcoming logical
	// boundary than fit in this fixed history. Events at or before that next
	// boundary can be folded into the latched state without changing any
	// observable sample: no earlier logical tick remains to be emitted.
	if(pending_count_ >= pending_events_.size()) {
		while(
			pending_count_ != 0U &&
			pending_events_[pending_head_].timestamp_ns <= next_deadline_ns_
		) {
			apply_event(pending_events_[pending_head_]);
			pending_head_ = (pending_head_ + 1U) % pending_events_.size();
			pending_count_--;
		}
	}
	if(pending_count_ >= pending_events_.size()) {
		return std::unexpected(TickInputError::event_queue_full);
	}

	const auto tail = (pending_head_ + pending_count_) % pending_events_.size();
	pending_events_[tail] = event;
	pending_count_++;
	last_event_timestamp_ = event.timestamp_ns;
	return {};
}

std::expected<void, TickInputError> TickInputScheduler::push_live_event(
	PhysicalInputEvent event
) noexcept
{
	// SDL timestamps describe when the host produced an event, but delivery to
	// its polled queue can lag behind that event's logical deadline. Once a tick
	// is emitted, poll order is the only remaining causal order we can preserve.
	if(last_emitted_deadline_ && event.timestamp_ns <= *last_emitted_deadline_) {
		if(!deadline_valid_) {
			return std::unexpected(TickInputError::deadline_overflow);
		}
		event.timestamp_ns = next_deadline_ns_;
	}
	if(last_event_timestamp_ && event.timestamp_ns < *last_event_timestamp_) {
		event.timestamp_ns = *last_event_timestamp_;
	}
	return push_event(event);
}

std::expected<std::optional<LogicalTickInput>, TickInputError>
TickInputScheduler::pop_due(TickTimestampNs now_ns) noexcept
{
	if(last_observed_now_ && now_ns < *last_observed_now_) {
		return std::unexpected(TickInputError::timestamp_regressed);
	}
	last_observed_now_ = now_ns;
	if(!deadline_valid_) {
		return std::unexpected(TickInputError::deadline_overflow);
	}
	if(catch_up_policy_ ==
			TickInputCatchUpPolicy::discard_missed_deadlines &&
		now_ns > next_deadline_ns_) {
		// frame_delay(1) cannot repay retraces that elapsed while a PC-98 frame
		// was being drawn. Keep one current sample and begin the next rational
		// period from this observation. Logical tick indices count updates that
		// actually ran, not discarded wall-clock deadline positions.
		next_deadline_ns_ = now_ns;
		fractional_accumulator_ =
			static_cast<std::uint64_t>(rate_.ticks) - 1U;
	}
	if(now_ns < next_deadline_ns_) {
		return std::optional<LogicalTickInput>{};
	}

	while(
		pending_count_ != 0U &&
		pending_events_[pending_head_].timestamp_ns <= next_deadline_ns_
	) {
		apply_event(pending_events_[pending_head_]);
		pending_head_ = (pending_head_ + 1U) % pending_events_.size();
		pending_count_--;
	}

	const auto sample = LogicalTickInput{
		.tick_index = tick_index_,
		.deadline_ns = next_deadline_ns_,
		.input = latched_input(),
		.physical_domains = latched_physical_domains(),
		.app_quit_requested = app_quit_requested_,
	};
	last_emitted_deadline_ = next_deadline_ns_;
	tick_index_++;
	advance_deadline();
	return std::optional<LogicalTickInput>{sample};
}

std::optional<TickTimestampNs> TickInputScheduler::next_deadline_ns() const noexcept
{
	if(!deadline_valid_) {
		return std::nullopt;
	}
	return next_deadline_ns_;
}

std::size_t TickInputScheduler::pending_event_count() const noexcept
{
	return pending_count_;
}

bool TickInputScheduler::app_quit_requested() const noexcept
{
	return app_quit_requested_;
}

void TickInputScheduler::apply_event(const PhysicalInputEvent &event) noexcept
{
	switch(event.kind) {
	case PhysicalInputEventKind::source_state_changed:
		source_states_[event.source] = event.state;
		break;
	case PhysicalInputEventKind::source_released:
		source_states_[event.source] = {};
		break;
	case PhysicalInputEventKind::focus_released:
		for(auto &source : source_states_) {
			source = {.device_present = source.device_present};
		}
		break;
	case PhysicalInputEventKind::app_quit:
		app_quit_requested_ = true;
		break;
	}
}

CoreInput TickInputScheduler::latched_input() const noexcept
{
	CoreInput input{};
	for(const auto &source : source_states_) {
		input.p1 = static_cast<std::uint16_t>(input.p1 | source.p1);
		input.p2 = static_cast<std::uint16_t>(input.p2 | source.p2);
		input.system = static_cast<std::uint16_t>(
			input.system | source.system
		);
		input.charge_mask = static_cast<std::uint8_t>(
			input.charge_mask | source.charge_mask
		);
	}
	return input;
}

PhysicalInputDomains
TickInputScheduler::latched_physical_domains() const noexcept
{
	const auto &keyboard_source = source_states_[0U];
	PhysicalInputDomains domains{
		.keyboard = {
			.p1 = keyboard_source.p1,
			.p2 = keyboard_source.p2,
			.system = keyboard_source.system,
			.charge_mask = keyboard_source.charge_mask,
		},
	};
	for(std::size_t source = 1U; source < source_states_.size(); ++source) {
		const auto &candidate = source_states_[source];
		if(!candidate.device_present) {
			continue;
		}
		domains.first_joystick = static_cast<std::uint16_t>(
			(candidate.p1 | candidate.p2 | candidate.system) & 0x003FU
		);
		domains.first_joystick_present = true;
		break;
	}
	return domains;
}

void TickInputScheduler::advance_deadline() noexcept
{
	if(
		fractional_accumulator_ >
		std::numeric_limits<std::uint64_t>::max() - period_numerator_ns_
	) {
		deadline_valid_ = false;
		return;
	}
	const auto accumulated = fractional_accumulator_ + period_numerator_ns_;
	const auto delta = accumulated / static_cast<std::uint64_t>(rate_.ticks);
	const auto remainder = accumulated % static_cast<std::uint64_t>(rate_.ticks);
	if(next_deadline_ns_ > std::numeric_limits<TickTimestampNs>::max() - delta) {
		deadline_valid_ = false;
		return;
	}
	next_deadline_ns_ += delta;
	fractional_accumulator_ = remainder;
}

} // namespace th03::app
