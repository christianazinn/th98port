#ifndef TH03_APP_MAIN_PROCESS_INTERNAL_HPP
#define TH03_APP_MAIN_PROCESS_INTERNAL_HPP

#include <cstddef>
#include <expected>
#include <memory>
#include <span>
#include <utility>

#include "th03/app/main/main_process.hpp"
#include "th03/present/main_rollback.hpp"

#include "classic/mainl/classic_mainl_stage_internal.hpp"
#include "main/main_process_static_content_internal.hpp"

namespace th03::app {

enum class MainFeedbackScheduling : std::uint8_t {
	request,
	reset_then_request,
	force,
};

struct MainFeedbackPolicyFact final {
	std::uint8_t effect_id = 0U;
	MainFeedbackScheduling scheduling = MainFeedbackScheduling::request;

	friend constexpr bool operator==(
		const MainFeedbackPolicyFact &,
		const MainFeedbackPolicyFact &
	) = default;
};

// Hermetic tests need structurally valid synthetic asset bytes that cannot
// match the authenticated original-resource fingerprints. Production callers
// have no access to this source-private seam.
struct MainProcessTestAccess final {
	[[nodiscard]] static std::expected<
		std::unique_ptr<MainProcess>,
		MainProcessError
	> create_with_synthetic_resources(
		const MainDirectVsConfig &config,
		std::span<const LoadedMainResource> resources,
		MainCompiledContent compiled_content,
		std::span<const std::byte> font_rom
	)
	{
		return MainProcess::create_impl(
			config,
			resources,
			std::move(compiled_content),
			font_rom,
			false
		);
	}

	static void fail_presentation(MainProcess &process) noexcept
	{
		process.fail_presentation_for_test();
	}

	[[nodiscard]] static const assets::MrsImage *portrait(
		const MainProcess &process,
		bool bomb,
		std::size_t player
	) noexcept;

	[[nodiscard]] static present::MainBombsStatus bombs_status(
		const MainProcess &process
	) noexcept;

	[[nodiscard]] static std::uint8_t continues_used_digit(
		const MainProcess &process
	) noexcept;

	[[nodiscard]] static bool presenter_rollback_ready(
		const MainProcess &process
	) noexcept;

	[[nodiscard]] static bool capture_presenter_rollback(
		const MainProcess &process,
		present::MainPresenterRollbackState &state
	) noexcept;

	static void restore_presenter_rollback(
		MainProcess &process,
		const present::MainPresenterRollbackState &state
	) noexcept;

	[[nodiscard]] static MainFeedbackPolicyFact feedback_policy(
		MainFeedbackCueSite site
	) noexcept;

	static void request_feedback(
		MainProcess &process,
		MainFeedbackCueRequested request
	) noexcept;

	static void feedback_dispatch_boundary(MainProcess &process) noexcept;

	[[nodiscard]] static std::expected<
		std::shared_ptr<const MainProcessStaticContent>,
		MainProcessError
	> prepare_static_with_synthetic_resources(
		std::span<const LoadedMainResource> resources,
		MainCompiledContent compiled_content,
		std::span<const std::byte> font_rom,
		const NewClassicGameTextCatalog *localized_hud_text = nullptr
	)
	{
		return MainProcessStaticContentAccess::create(
			resources,
			std::move(compiled_content),
			font_rom,
			false,
			localized_hud_text
		);
	}
};

struct MainProcessStoryAccess final {
	[[nodiscard]] static std::expected<
		std::unique_ptr<MainProcess>,
		MainProcessError
	> create_story(
		const MainStoryConfig &config,
		ClassicMainlStageTransition &transition,
		std::span<const LoadedMainResource> resources,
		std::shared_ptr<const MainProcessStaticContent> static_content,
		bool authenticate_resource_bytes = true
	);
};

// Source-private entry for a completed stock OP VS selection. The stage owns
// the exact resident/options/callback tuple and its move-only Raster handoff;
// no independent GameInit is accepted here.
struct MainProcessClassicVsAccess final {
	[[nodiscard]] static std::expected<
		std::unique_ptr<MainProcess>,
		MainProcessError
	> create_vs(
		ClassicMainlStageTransition &transition,
		std::span<const LoadedMainResource> resources,
		std::shared_ptr<const MainProcessStaticContent> static_content,
		bool authenticate_resource_bytes = true
	);
};

// Source-private entry for one completed stock OP automatic-Demo route. The
// MAINL stage retains the exact typed resident and Raster handoff; callers
// cannot substitute a second Demo recipe or bypass the visible splash.
struct MainProcessDemoAccess final {
	[[nodiscard]] static std::expected<
		std::unique_ptr<MainProcess>,
		MainProcessError
	> create_demo(
		ClassicMainlStageTransition &transition,
		std::span<const LoadedMainResource> resources,
		std::shared_ptr<const MainProcessStaticContent> static_content,
		bool authenticate_resource_bytes = true
	);
};

// Replay Patch MAIN owns one late native presentation hook after the ordinary
// round presenters. Keep mutable Raster/TRAM access source-private so replay
// UI cannot leak into the shared deterministic process API.
struct MainProcessReplayPresentationAccess final {
	struct AcceleratorAnchor final {
		present::Raster raster{};
	};

	[[nodiscard]] static present::Raster &raster(MainProcess &process) noexcept;
	[[nodiscard]] static present::Tram &tram(MainProcess &process) noexcept;
	static void set_preroll_simulating(
		MainProcess &process,
		bool simulating
	) noexcept;
	[[nodiscard]] static AcceleratorAnchor accelerator_anchor(
		const MainProcess &process
	);
	[[nodiscard]] static bool restore_accelerator_publication(
		MainProcess &process,
		AcceleratorAnchor &&anchor,
		const RoundStartObservation &anchor_round_start,
		present::MainPageRoles source_pages,
		const present::Palette16Rgb8 &palette,
		std::int8_t shared_velocity_x_q4_4,
		const std::array<
			present::MainBackgroundRecordState,
			present::main_background_record_count
		> &background,
		std::uint8_t background_direction
	) noexcept;
};

} // namespace th03::app

#endif
