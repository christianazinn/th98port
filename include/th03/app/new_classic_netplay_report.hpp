#ifndef TH03_APP_NEW_CLASSIC_NETPLAY_REPORT_HPP
#define TH03_APP_NEW_CLASSIC_NETPLAY_REPORT_HPP

#include <cstdint>
#include <memory>
#include <optional>

#include "th03/app/new_classic_assets.hpp"
#include "th03/app/new_classic_frontend.hpp"
#include "th03/app/new_classic_netplay_main_session.hpp"

namespace th03::app {

enum class NewClassicNetplayReportKind : std::uint8_t {
	match_complete,
	forfeit_loss,
	forfeit_win,
	forfeit_mutual,
	peer_left,
	peer_timeout,
	desync,
	connection_failed,
};

struct NewClassicNetplayReportMetrics final {
	std::uint64_t confirmed_frames = 0U;
	NewClassicNetplayMainSessionStats session{};
	std::uint64_t missed_deadlines = 0U;
	std::optional<std::uint64_t> round_trip_time_ms{};
};

enum class NewClassicNetplayReportStatus : std::uint8_t {
	running,
	complete,
	application_quit,
};

struct NewClassicNetplayReportResult final {
	bool surface_changed = false;
	NewClassicNetplayReportStatus status =
		NewClassicNetplayReportStatus::running;
};

// New Classic's local-only netplay terminal. The report observes diagnostics
// after peer agreement has ended; it never feeds simulation, proof, or V14.
class NewClassicNetplayReport final {
public:
	NewClassicNetplayReport(
		NewClassicNetplayReportKind kind,
		NewClassicNetplayReportMetrics metrics,
		std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
		NewClassicLanguageAssetProfile language =
			NewClassicLanguageAssetProfile::english
	);

	[[nodiscard]] bool valid() const noexcept;
	[[nodiscard]] NewClassicNetplayReportResult consume(
		NewClassicUiInput input
	) noexcept;
	[[nodiscard]] present::SourceSurface surface() const noexcept;
	[[nodiscard]] NewClassicNetplayReportStatus status() const noexcept;
	[[nodiscard]] NewClassicNetplayReportKind kind() const noexcept;
	[[nodiscard]] const NewClassicNetplayReportMetrics &metrics() const noexcept;

private:
	[[nodiscard]] bool draw() noexcept;

	NewClassicNetplayReportKind kind_ =
		NewClassicNetplayReportKind::match_complete;
	NewClassicNetplayReportMetrics metrics_{};
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets_{};
	NewClassicLanguageAssetProfile language_ =
		NewClassicLanguageAssetProfile::english;
	NewClassicPatchSurface patch_surface_{};
	NewClassicNetplayReportStatus status_ =
		NewClassicNetplayReportStatus::running;
	bool release_required_ = true;
	bool valid_ = false;
};

} // namespace th03::app

#endif
