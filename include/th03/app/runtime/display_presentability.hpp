#ifndef TH03_APP_DISPLAY_PRESENTABILITY_HPP
#define TH03_APP_DISPLAY_PRESENTABILITY_HPP

#include <cstdint>
#include <optional>

namespace th03::app {

// Platform-neutral ownership for non-interpolated output. A source publication
// represents one newest complete image already retained by the host adapter.
// Physical scanouts merely rescan [submitted_publication] and never call this
// owner, advance a source tick, or create another publication.
class SourcePresentationMailbox final {
public:
	// Retains one newer source publication. If the host is presentable, the
	// returned ordinal should be submitted synchronously and acknowledged.
	// Otherwise the ordinal remains pending and supersedes any older pending
	// publication.
	[[nodiscard]] std::optional<std::uint64_t> publish_source(
		bool host_presentable
	) noexcept;

	// Updates host presentability without publishing a source image. Returning
	// an ordinal means that exactly the newest retained image needs one host
	// submission. Repeated presentable observations return the same ordinal
	// until the successful submission is acknowledged.
	[[nodiscard]] std::optional<std::uint64_t> refresh_host(
		bool host_presentable
	) noexcept;

	// Acknowledges a successful submission of the requested newest ordinal.
	// Rejects stale or speculative ordinals without changing mailbox state.
	[[nodiscard]] bool acknowledge_host_submission(
		std::uint64_t publication
	) noexcept;

	[[nodiscard]] bool host_presentable() const noexcept;
	[[nodiscard]] bool submission_pending() const noexcept;
	[[nodiscard]] std::uint64_t source_publication_count() const noexcept;
	[[nodiscard]] std::uint64_t host_submission_count() const noexcept;
	[[nodiscard]] std::optional<std::uint64_t>
	newest_source_publication() const noexcept;
	[[nodiscard]] std::optional<std::uint64_t>
	submitted_publication() const noexcept;

private:
	bool host_presentable_ = true;
	std::uint64_t source_publication_count_ = 0U;
	std::uint64_t host_submission_count_ = 0U;
	std::optional<std::uint64_t> newest_source_publication_{};
	std::optional<std::uint64_t> submitted_publication_{};
};

} // namespace th03::app

#endif
