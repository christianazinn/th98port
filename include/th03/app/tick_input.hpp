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

// Nominal PC-9801 24 kHz display timing. DOSBox-X 2026.05.02 and NP2kai
// independently model a 21,052,600 Hz dot clock divided by 8 across 106
// horizontal character periods and 440 total lines:
//
//     21,052,600 / (8 * 106 * 440) = 526315 / 9328 Hz
//
// This is exact for that nominal timing model, not a claim of zero tolerance
// in a physical oscillator. TickInputScheduler remains rate-neutral; the
// application CLIs select this value unless the caller supplies an override.
inline constexpr RationalTickRate pc98_24khz_nominal_tick_rate{
	.ticks = 526'315U,
	.seconds = 9'328U,
};

enum class TickInputCatchUpPolicy : std::uint8_t {
	// Deterministic, replay, and headless callers observe every deadline.
	preserve_all,
	// Interactive PC-98-style frame waits lose elapsed retraces. Rebase one
	// late sample to the current host time instead of repaying a backlog.
	discard_missed_deadlines,
};

inline constexpr std::size_t physical_input_source_count = 16U;
inline constexpr std::size_t pending_physical_event_capacity = 256U;

// The complete contribution currently held by one keyboard/controller mapping
// slot. Host bindings are recomputed into this snapshot after each physical
// edge, so multiple buttons bound to one logical action cannot false-release
// each other. Player masks accept the donor cardinal/button bits 0x003F and
// legacy diagonal bits 0x0F00; charge_mask accepts only bits 0 and 1.
// [system] is the donor's complete source-global input_sp mask: cardinal and
// legacy diagonal directions, Bomb/Shot, Cancel/OK, and Q (0x7F3F). Application
// quit remains a separate event and never aliases any source input bit.
struct PhysicalInputSourceState {
	std::uint16_t p1 = 0;
	std::uint16_t p2 = 0;
	std::uint16_t system = 0;
	std::uint8_t charge_mask = 0;
	// Meaningful for controller sources. Unlike held contributions, device
	// presence survives focus release and a controller remap.
	bool device_present = false;
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

// Live-only source domains sampled at one logical boundary. Replay and
// scripted callers intentionally do not construct this host-facing detail.
struct PhysicalInputDomains final {
	CoreInput keyboard{};
	std::uint16_t first_joystick = 0;
	bool first_joystick_present = false;
};

enum class TickInputError : std::uint8_t {
	invalid_rate,
	invalid_catch_up_policy,
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
	// Present on scheduler-produced physical samples and absent from scripted
	// or replay samples that already contain their post-mode CoreInput words.
	std::optional<PhysicalInputDomains> physical_domains{};
	// Sticky once a due app_quit event is applied. This never aliases or sets
	// the gameplay Cancel bit in [input.system].
	bool app_quit_requested = false;
	// Host-only Replay Patch shortcut sampled at the logical boundary. It is
	// intentionally outside CoreInput and never enters deterministic gameplay.
	bool restart_requested = false;
	// Replay Patch's staff/replay accelerator is bound to physical Z rather
	// than the configurable Shot action. It is presentation pacing only and is
	// therefore kept out of CoreInput and replay serialization.
	bool fast_forward_requested = false;
	// Raw keyboard capture is consumed only by Replay Patch KeyConfig. It is
	// outside deterministic gameplay and replay serialization.
	std::optional<std::uint16_t> scancode_pressed{};
	bool any_scancode_held = false;
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
		TickTimestampNs first_deadline_ns,
		TickInputCatchUpPolicy catch_up_policy =
			TickInputCatchUpPolicy::preserve_all
	) noexcept;

	// Timestamps may be equal and then retain injection order. If the fixed
	// history fills entirely with events due at the next logical boundary,
	// those events are folded into that boundary's pending source state before
	// accepting more; no intermediate sample exists to observe them. Rejected
	// events leave the queue, source states, and monotonic timestamp guard
	// unchanged.
	[[nodiscard]] std::expected<void, TickInputError> push_event(
		PhysicalInputEvent event
	) noexcept;

	// Live host queues can publish an edge after its original timestamp's
	// logical boundary was already emitted. Preserve polling order and schedule
	// such an edge at the earliest still-unemitted boundary instead of rewriting
	// history or terminating live play. Scripted/replay injection should keep
	// using strict push_event().
	[[nodiscard]] std::expected<void, TickInputError> push_live_event(
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
	// A modal host UI owns a separate scheduler. Rebase the live source clock
	// after it returns so the gameplay scheduler neither repays the modal's wall
	// time nor leaks held input across the ownership boundary.
	[[nodiscard]] std::expected<void, TickInputError> rebase_after_modal(
		TickTimestampNs now_ns
	) noexcept;

private:
	TickInputScheduler(
		RationalTickRate rate,
		TickTimestampNs first_deadline_ns,
		std::uint64_t period_numerator_ns,
		TickInputCatchUpPolicy catch_up_policy
	) noexcept;

	void apply_event(const PhysicalInputEvent &event) noexcept;
	[[nodiscard]] CoreInput latched_input() const noexcept;
	[[nodiscard]] PhysicalInputDomains
	latched_physical_domains() const noexcept;
	void advance_deadline() noexcept;

	RationalTickRate rate_{};
	std::uint64_t period_numerator_ns_ = 0;
	TickInputCatchUpPolicy catch_up_policy_ =
		TickInputCatchUpPolicy::preserve_all;
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
