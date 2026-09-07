#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_TIMELINE_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_TIMELINE_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>

#include "th03/app/new_classic/netplay/new_classic_netplay_protocol.hpp"

namespace th03::app {

inline constexpr std::size_t new_classic_netplay_timeline_slots = 64U;
inline constexpr std::uint8_t new_classic_netplay_max_prediction = 12U;

enum class NewClassicNetplayTimelineErrorCode : std::uint8_t {
	invalid_player,
	invalid_input,
	frame_too_old,
	frame_too_far,
	conflicting_input,
	local_input_unavailable,
	remote_input_unavailable,
	prediction_limit,
	rollback_required,
	rollback_unavailable,
};

struct NewClassicNetplayTimelineError final {
	NewClassicNetplayTimelineErrorCode code =
		NewClassicNetplayTimelineErrorCode::invalid_player;
	std::optional<NewClassicNetplayInputError> input{};
};

enum class NewClassicNetplaySubmissionDisposition : std::uint8_t {
	accepted,
	duplicate,
	stale_ignored,
};

struct NewClassicNetplayRemoteSubmission final {
	NewClassicNetplaySubmissionDisposition disposition =
		NewClassicNetplaySubmissionDisposition::accepted;
	std::optional<std::uint64_t> rollback_frame{};
	std::uint64_t confirmed_frame_count = 0U;
};

struct NewClassicNetplayResolvedFrame final {
	std::uint64_t frame = 0U;
	NewClassicNetplayOwnedInput local{};
	NewClassicNetplayOwnedInput remote{};
	CoreInput combined{};
	bool pause_requested = false;
	bool remote_predicted = false;
	std::uint8_t prediction_depth = 0U;
};

struct NewClassicNetplayTimelineStats final {
	std::uint64_t resolved_frames = 0U;
	std::uint64_t predicted_frames = 0U;
	std::uint64_t rollback_requests = 0U;
	std::uint64_t rewinds = 0U;
};

// Bounded, backend-neutral input history for one peer. Local/remote ownership
// is fixed at construction. A missing remote value repeats the prior resolved
// remote value (neutral at frame zero) until the twelve-frame prediction cap.
class NewClassicNetplayInputTimeline final {
public:
	[[nodiscard]] static std::expected<
		std::unique_ptr<NewClassicNetplayInputTimeline>,
		NewClassicNetplayTimelineError
	> create(NewClassicNetplayPlayer local_player);

	~NewClassicNetplayInputTimeline();
	NewClassicNetplayInputTimeline(
		const NewClassicNetplayInputTimeline &
	) = delete;
	NewClassicNetplayInputTimeline &operator=(
		const NewClassicNetplayInputTimeline &
	) = delete;
	NewClassicNetplayInputTimeline(NewClassicNetplayInputTimeline &&) = delete;
	NewClassicNetplayInputTimeline &operator=(
		NewClassicNetplayInputTimeline &&
	) = delete;

	[[nodiscard]] std::expected<
		NewClassicNetplaySubmissionDisposition,
		NewClassicNetplayTimelineError
	> submit_local(
		std::uint64_t frame,
		NewClassicNetplayOwnedInput input
	) noexcept;

	[[nodiscard]] std::expected<
		NewClassicNetplayRemoteSubmission,
		NewClassicNetplayTimelineError
	> submit_remote(
		std::uint64_t frame,
		NewClassicNetplayOwnedInput input
	) noexcept;

	[[nodiscard]] std::expected<
		NewClassicNetplayResolvedFrame,
		NewClassicNetplayTimelineError
	> resolve_next() noexcept;
	[[nodiscard]] std::expected<
		NewClassicNetplayResolvedFrame,
		NewClassicNetplayTimelineError
	> resolve_next_exact() noexcept;

	// Rewind exactly to the earliest requested correction. Actual local and
	// remote inputs remain retained; discarded predictions are recomputed.
	[[nodiscard]] std::expected<void, NewClassicNetplayTimelineError>
	rewind_to_rollback() noexcept;
	// Inputs sampled after a corrected Pause marker belong to a discarded
	// speculative suffix and must not survive into the resumed gameplay domain.
	[[nodiscard]] std::expected<void, NewClassicNetplayTimelineError>
	discard_unconfirmed_suffix() noexcept;

	[[nodiscard]] std::optional<std::uint64_t>
	rollback_frame() const noexcept;
	[[nodiscard]] std::uint64_t next_frame() const noexcept;
	[[nodiscard]] std::uint64_t confirmed_frame_count() const noexcept;
	[[nodiscard]] std::optional<NewClassicNetplayResolvedFrame>
	resolved_frame(std::uint64_t frame) const noexcept;
	[[nodiscard]] std::optional<NewClassicNetplayOwnedInput>
	local_input(std::uint64_t frame) const noexcept;
	[[nodiscard]] NewClassicNetplayTimelineStats stats() const noexcept;

private:
	struct Impl;
	explicit NewClassicNetplayInputTimeline(
		std::unique_ptr<Impl> impl
	) noexcept;
	std::unique_ptr<Impl> impl_;
};

} // namespace th03::app

#endif
