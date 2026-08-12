#ifndef TH03_APP_NEW_CLASSIC_REPLAY_HPP
#define TH03_APP_NEW_CLASSIC_REPLAY_HPP

#include <cstdint>
#include <expected>
#include <optional>

#include "th03/core/game.hpp"
#include "th03/formats/replay.hpp"
#include "th03/replay/session.hpp"

namespace th03::app {

enum class NewClassicReplayErrorCode : std::uint8_t {
	invalid_round_start,
	invalid_sequence,
	document_invalid,
};

struct NewClassicReplayError final {
	NewClassicReplayErrorCode code =
		NewClassicReplayErrorCode::invalid_sequence;
	std::optional<Error> format{};
};

// Application-only Replay Patch capture adapter. It observes accepted Classic
// process boundaries and builds a neutral ReplayDocument; neither experience
// identity nor storage/UI policy crosses into deterministic gameplay.
class NewClassicReplayRecorder final {
public:
	explicit NewClassicReplayRecorder(std::uint16_t recording_date = 0U) noexcept :
		recording_date_(recording_date)
	{}
	[[nodiscard]] std::expected<void, NewClassicReplayError> round_started(
		const RoundStartObservation &observation,
		std::uint16_t dos_date = 0U
	);
	[[nodiscard]] std::expected<void, NewClassicReplayError> sample(
		replay::InputPhase phase,
		CoreInput input
	);
	[[nodiscard]] std::expected<void, NewClassicReplayError> round_completed(
		const CanonicalState &state,
		std::optional<std::uint32_t> real_frames = std::nullopt
	);
	[[nodiscard]] std::expected<void, NewClassicReplayError> process_completed(
		replay::ProcessId process
	);
	[[nodiscard]] std::expected<void, NewClassicReplayError> finish(
		formats::ReplayEndReason reason,
		const GameInit &resident
	);

	[[nodiscard]] bool recording() const noexcept;
	[[nodiscard]] const formats::ReplayDocument *document() const noexcept;
	[[nodiscard]] const CanonicalState *last_terminal() const noexcept;
	[[nodiscard]] std::optional<formats::ReplayDocument> take_completed();
	void discard() noexcept;

private:
	std::optional<formats::ReplayDocument> document_{};
	std::optional<formats::ReplayDocument> completed_{};
	std::optional<CanonicalState> last_terminal_{};
	std::uint16_t recording_date_ = 0U;
};

} // namespace th03::app

#endif
