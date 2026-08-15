#ifndef TH03_APP_CLASSIC_RUNNER_INTERNAL_HPP
#define TH03_APP_CLASSIC_RUNNER_INTERNAL_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include "classic_op_session_internal.hpp"
#include "th03/app/classic_runner.hpp"

namespace th03::app {

struct ClassicRunnerOperations final {
	void *context = nullptr;
	std::expected<ClassicOpSessionTick, ClassicOpSessionError> (*consume)(
		void *, const ClassicOpSessionInput &
	) = nullptr;
	std::optional<ClassicAudioCommand> (*take_audio_command)(void *) = nullptr;
	std::optional<NewClassicKeyConfig> (*take_key_config_update)(void *) = nullptr;
	std::optional<bool> (*take_reduced_flash_update)(void *) = nullptr;
	std::optional<std::uint8_t> (*take_master_volume_update)(void *) = nullptr;
	std::optional<NewClassicWindowPresentation>
		(*take_window_presentation_update)(void *) = nullptr;
	PixelOracleSourceIdentity (*pixel_oracle_source_identity)(void *) = nullptr;
	std::expected<present::SourceSurface, ClassicOpSessionError>
	(*compose_shown_surface)(
		void *,
		std::span<present::ComposedPixel, present::graphics_page_pixels>
	) = nullptr;
};

struct ClassicRunnerTestAccess final {
	[[nodiscard]] static GameInit initial_resident(std::uint32_t seed) noexcept;

	[[nodiscard]] static std::expected<
		std::unique_ptr<ClassicRunner>,
		ClassicRunnerError
	> create(
		std::shared_ptr<void> owner,
		ClassicRunnerOperations operations,
		ExperienceProfile experience = ExperienceProfile::classic,
		bool reduced_flash = false
	);
	[[nodiscard]] static ExperienceProfile experience(
		const ClassicRunner &runner
	) noexcept;

	[[nodiscard]] static ClassicOpSessionInput project_input(
		const LogicalTickInput &sample,
		ClassicAudioStatus audio = {.volume = 255U}
	) noexcept;
	[[nodiscard]] static std::optional<ClassicOpSessionScene> scene(
		const ClassicRunner &runner
	) noexcept;
	[[nodiscard]] static std::optional<ClassicStorySessionScene> story_scene(
		const ClassicRunner &runner
	) noexcept;
	[[nodiscard]] static std::optional<std::uint32_t>
	story_source_sample_count(const ClassicRunner &runner) noexcept;
	[[nodiscard]] static std::optional<ClassicStorySessionErrorCode>
	story_failure_code(const ClassicRunner &runner) noexcept;
	[[nodiscard]] static std::optional<std::uint8_t>
	story_failure_detail_code(const ClassicRunner &runner) noexcept;
	[[nodiscard]] static std::expected<void, ClassicRunnerError>
	force_story_main_outcome(
		ClassicRunner &runner,
		ClassicStoryRouteTestOutcome outcome
	);
	[[nodiscard]] static std::expected<void, ClassicRunnerError>
	force_vs_main_outcome(
		ClassicRunner &runner,
		ClassicOpVsRouteTestOutcome outcome
	);
	[[nodiscard]] static std::optional<GameInit> vs_selection_resident(
		const ClassicRunner &runner
	) noexcept;
	[[nodiscard]] static std::optional<GameInit> vs_terminal_resident(
		const ClassicRunner &runner
	) noexcept;

	[[nodiscard]] static std::expected<
		std::vector<std::byte>,
		ClassicRunnerError
	> read_file(
		const std::filesystem::path &path,
		ClassicContentFile file,
		std::optional<std::uintmax_t> exact_size = std::nullopt
	);
};

} // namespace th03::app

#endif
