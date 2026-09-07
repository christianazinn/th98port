#ifndef TH03_APP_MAIN_DIRECT_VS_RUNNER_INTERNAL_HPP
#define TH03_APP_MAIN_DIRECT_VS_RUNNER_INTERNAL_HPP

#include <expected>
#include <memory>

#include "th03/app/main/main_direct_vs_runner.hpp"

namespace th03::app {

struct MainDirectVsRunnerOperations {
	void *context = nullptr;
	std::expected<StepResult, MainProcessError> (*step)(
		void *, CoreInput, InputContext
	) noexcept = nullptr;
	std::expected<void, MainProcessError> (*start_next_round)(void *) noexcept =
		nullptr;
	std::expected<void, MainProcessError> (*finish_main_process)(void *) noexcept =
		nullptr;
	std::expected<SessionTransition, MainProcessError> (*session_transition)(
		void *, std::optional<ContinueChoice>
	) noexcept = nullptr;
};

struct MainDirectVsRunnerTestAccess {
	[[nodiscard]] static std::expected<
		std::unique_ptr<MainDirectVsRunner>,
		MainDirectVsRunnerError
	> create(
		MainDirectVsRunnerOperations operations
	);
};

} // namespace th03::app

#endif
