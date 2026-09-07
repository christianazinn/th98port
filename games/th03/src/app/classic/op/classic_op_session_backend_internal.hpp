#ifndef TH03_APP_CLASSIC_OP_SESSION_BACKEND_INTERNAL_HPP
#define TH03_APP_CLASSIC_OP_SESSION_BACKEND_INTERNAL_HPP

#include "classic/op/classic_op_session_internal.hpp"

namespace th03::app::detail {

struct ClassicOpSessionProductionBackend final {
	std::shared_ptr<void> owner{};
	ClassicOpSessionBackendOperations operations{};
	ClassicOpSessionHostOperations host_operations{};
	ClassicOpSessionTestHooks test_hooks{};
	ClassicOpSessionScene initial_scene = ClassicOpSessionScene::title_menu;
};

[[nodiscard]] ClassicOpSessionError session_error(
	ClassicOpSessionErrorCode code
) noexcept;
[[nodiscard]] ClassicOpSessionError replay_capture_error() noexcept;
void apply_key_config_autofire(
	GameInit &resident,
	const std::optional<NewClassicKeyConfig> &key_config
) noexcept;
[[nodiscard]] CoreInput normalize_vs_cpu_main_input(
	const GameInit &resident,
	const ClassicMainInputSample &sample,
	InputContext context,
	bool new_classic = false
) noexcept;
[[nodiscard]] bool attract_input_present(
	const ClassicMainInputSample &input
) noexcept;
[[nodiscard]] CoreInput normalize_replay_interstitial_input(
	const ClassicMainInputSample &sample
) noexcept;
[[nodiscard]] ClassicOpVsSurfaceSource vs_surface_source(
	bool win,
	bool win_completion,
	bool main_process
) noexcept;

[[nodiscard]] std::expected<
	ClassicOpSessionProductionBackend,
	ClassicOpSessionError
> create_classic_op_session_production_backend(
	GameInit resident,
	std::unique_ptr<MainResourceLoader> loader,
	MainCompiledContent compiled_content,
	std::span<const std::byte> font_rom,
	std::span<const std::byte> mainl_executable,
	std::span<const std::byte> op_executable,
	std::filesystem::path config_path,
	std::filesystem::path score_path,
	bool fm_available,
	present::KotohimeBombFadeCompatibility kotohime_compatibility,
	NewClassicReplayRecorder *replay_recorder,
	bool new_classic_practice_route,
	std::filesystem::path new_classic_score_path,
	std::filesystem::path language_overlay_path,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	const ClassicRunnerRequest *runner_request
);

} // namespace th03::app::detail

#endif
