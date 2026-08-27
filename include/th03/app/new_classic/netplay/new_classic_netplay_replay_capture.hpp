#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_REPLAY_CAPTURE_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_REPLAY_CAPTURE_HPP

#include <cstdint>
#include <expected>
#include <optional>

#include "th03/app/new_classic/netplay/new_classic_netplay_main_session.hpp"
#include "th03/app/new_classic/replay/new_classic_replay.hpp"

namespace th03::app {

enum class NewClassicNetplayReplayCaptureErrorCode : std::uint8_t {
	invalid_sequence,
	replay_failed,
};

struct NewClassicNetplayReplayCaptureError final {
	NewClassicNetplayReplayCaptureErrorCode code =
		NewClassicNetplayReplayCaptureErrorCode::invalid_sequence;
	std::optional<NewClassicReplayError> replay{};
};

// App-layer adapter from the corrected contiguous-confirmed netplay journal to
// the ordinary in-memory V14 recorder. It owns no path or storage policy.
class NewClassicNetplayReplayCapture final {
public:
	[[nodiscard]] static std::expected<
		NewClassicNetplayReplayCapture,
		NewClassicNetplayReplayCaptureError
	> create(
		const RoundStartObservation &initial_round,
		std::uint16_t dos_date = 0U,
		NewClassicReplayIdentity identity = {}
	);

	[[nodiscard]] std::expected<void, NewClassicNetplayReplayCaptureError>
	append_confirmed_frame(
		const NewClassicNetplayConfirmedInputFrame &frame,
		bool deadline_missed
	);
	// Commit the fixed three-poll source Pause -> Resume transaction after both
	// peers leave the negotiated wait. No wall-clock network poll is recorded.
	[[nodiscard]] std::expected<void, NewClassicNetplayReplayCaptureError>
	pause_resumed(const StepResult &completed_frame);
	[[nodiscard]] std::expected<void, NewClassicNetplayReplayCaptureError>
	round_started(const RoundStartObservation &observation);
	[[nodiscard]] std::expected<
		formats::ReplayDocument,
		NewClassicNetplayReplayCaptureError
	> finish_mainl(const GameInit &resident);

	[[nodiscard]] std::uint64_t frame_count() const noexcept;
	[[nodiscard]] const formats::ReplayDocument *document() const noexcept;
	void discard() noexcept;

private:
	explicit NewClassicNetplayReplayCapture(
		NewClassicReplayRecorder recorder
	) noexcept;

	NewClassicReplayRecorder recorder_{};
	std::uint64_t next_frame_ = 0U;
	std::uint8_t source_vsyncs_before_next_frame_ = 1U;
	bool round_start_pending_ = false;
	bool pause_pending_ = false;
	bool main_complete_ = false;
	bool failed_ = false;
	bool finished_ = false;
};

} // namespace th03::app

#endif
