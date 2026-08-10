#ifndef TH03_APP_TICK_INPUT_HPP
#define TH03_APP_TICK_INPUT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>

#include "th03/core/game.hpp"

namespace th03::app {

// Absolute timestamps from a monotonic host clock, expressed in nanoseconds.
using TickTimestampNs = std::uint64_t;

// Logical ticks per [seconds]. No source cadence is assumed by this neutral
// boundary; the application must select an evidence-backed rate explicitly.
struct RationalTickRate {
	std::uint32_t ticks = 0;
	std::uint32_t seconds = 0;
};

inline constexpr std::size_t physical_input_source_count = 16U;
inline constexpr std::size_t pending_physical_event_capacity = 256U;

// The complete contribution currently held by one keyboard/controller mapping
// slot. Host bindings are recomputed into this snapshot after each physical
// edge, so multiple buttons bound to one logical action cannot false-release
// each other. Player masks accept the donor cardinal/button bits 0x003F and
// legacy diagonal bits 0x0F00; charge_mask accepts only bits 0 and 1.
// CoreInput::system is intentionally not a physical contribution.
struct PhysicalInputSourceState {
	std::uint16_t p1 = 0;
	std::uint16_t p2 = 0;
	std::uint8_t charge_mask = 0;
};

enum class PhysicalInputEventKind : std::uint8_t {
	source_state_changed,
	source_released,
	focus_released,
	app_quit,
};

// [source] identifies one independently held keyboard/controller mapping slot.
// It is required by source_state_changed/source_released and ignored by the two
// process-wide event kinds. [state] is used only by source_state_changed.
struct PhysicalInputEvent {
	TickTimestampNs timestamp_ns = 0;
	PhysicalInputEventKind kind = PhysicalInputEventKind::source_released;
	std::uint8_t source = 0;
	PhysicalInputSourceState state{};
};

enum class TickInputError : std::uint8_t {
	invalid_rate,
	// Either an injected event timestamp or a pop_due() clock observation
	// preceded the last accepted timestamp in its respective sequence.
	timestamp_regressed,
	event_already_due,
	event_queue_full,
	invalid_event_kind,
	invalid_source,
	invalid_input_bits,
	deadline_overflow,
};

struct LogicalTickInput {
	std::uint64_t tick_index = 0;
	TickTimestampNs deadline_ns = 0;
	CoreInput input{};
	// Sticky once a due app_quit event is applied. This never aliases or sets
	// the gameplay Cancel bit in [input.system].
	bool app_quit_requested = false;
};

// Fixed-capacity event history plus a rational fake-clock-friendly scheduler.
// pop_due() returns at most one logical tick; callers loop until nullopt. This
// makes every overdue deadline visible without allocating, dropping, or
// combining samples.
class TickInputScheduler {
public:
	[[nodiscard]] static std::expected<TickInputScheduler, TickInputError>
	create(
		RationalTickRate rate,
		TickTimestampNs first_deadline_ns
	) noexcept;

	// Timestamps may be equal and then retain injection order. Rejected events
	// leave the queue, source states, and monotonic timestamp guard unchanged.
	[[nodiscard]] std::expected<void, TickInputError> push_event(
		PhysicalInputEvent event
	) noexcept;

	// Events at or before the returned deadline are applied before sampling.
	// If advancing after a valid due tick would overflow, that tick is returned
	// normally; the following call reports deadline_overflow without consuming
	// any event or logical tick. [now_ns] observations must be monotonic; equal
	// timestamps are accepted, and a rejected regression changes no state.
	[[nodiscard]] std::expected<
		std::optional<LogicalTickInput>,
		TickInputError
	> pop_due(TickTimestampNs now_ns) noexcept;

	[[nodiscard]] std::optional<TickTimestampNs> next_deadline_ns() const noexcept;
	[[nodiscard]] std::size_t pending_event_count() const noexcept;
	[[nodiscard]] bool app_quit_requested() const noexcept;

private:
	TickInputScheduler(
		RationalTickRate rate,
		TickTimestampNs first_deadline_ns,
		std::uint64_t period_numerator_ns
	) noexcept;

	void apply_event(const PhysicalInputEvent &event) noexcept;
	[[nodiscard]] CoreInput latched_input() const noexcept;
	void advance_deadline() noexcept;

	RationalTickRate rate_{};
	std::uint64_t period_numerator_ns_ = 0;
	// Initialized to ticks-1 so each integer timestamp is the ceiling of the
	// exact rational boundary rather than an early floor approximation.
	std::uint64_t fractional_accumulator_ = 0;
	TickTimestampNs next_deadline_ns_ = 0;
	std::uint64_t tick_index_ = 0;
	bool deadline_valid_ = true;
	bool app_quit_requested_ = false;
	std::array<PhysicalInputSourceState, physical_input_source_count>
		source_states_{};
	std::array<PhysicalInputEvent, pending_physical_event_capacity>
		pending_events_{};
	std::size_t pending_head_ = 0;
	std::size_t pending_count_ = 0;
	std::optional<TickTimestampNs> last_event_timestamp_{};
	std::optional<TickTimestampNs> last_observed_now_{};
	std::optional<TickTimestampNs> last_emitted_deadline_{};
};

} // namespace th03::app

#endif
