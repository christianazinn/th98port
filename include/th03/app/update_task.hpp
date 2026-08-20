#ifndef TH03_APP_UPDATE_TASK_HPP
#define TH03_APP_UPDATE_TASK_HPP

#include "th03/app/update_install.hpp"
#include "th03/app/update_release_config.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <string_view>

namespace th03::app {

struct UpdateRuntimeContext final {
	std::filesystem::path activation_state{};
	std::filesystem::path generation_root{};
};

[[nodiscard]] bool valid_update_runtime_context(
	const UpdateRuntimeContext &context
) noexcept;

enum class UpdateRuntimeContextError : std::uint8_t {
	incomplete,
};

[[nodiscard]] std::expected<
	std::optional<UpdateRuntimeContext>,
	UpdateRuntimeContextError
> parse_update_runtime_context(
	std::optional<std::string_view> activation_state,
	std::optional<std::string_view> generation_root
);

// One capability test shared by the host and focused tests. A source capture,
// direct/default build, partial runtime handoff, missing trust record, or
// missing HTTPS adapter must omit the Updates row completely.
[[nodiscard]] bool new_classic_update_available(
	bool new_classic,
	bool source_capture,
	bool publication_oracle,
	const std::optional<UpdateReleaseConfiguration> &configuration,
	const std::optional<UpdateRuntimeContext> &runtime,
	UpdateTransport transport
) noexcept;

enum class UpdateTaskStartErrorCode : std::uint8_t {
	invalid_configuration,
	invalid_runtime_context,
	invalid_transport,
	worker_start,
};

struct UpdateTaskStartError final {
	UpdateTaskStartErrorCode code =
		UpdateTaskStartErrorCode::invalid_configuration;
};

using UpdateInstallTaskResult = std::expected<
	UpdateInstallResult,
	UpdateInstallError
>;

// Owns one background install transaction. Configuration strings and pinned
// key bytes are copied before the worker starts. The transport context must
// outlive the task; destruction requests cancellation before the transaction
// begins and otherwise joins the transport's bounded in-flight request.
class UpdateInstallTask final {
public:
	UpdateInstallTask(UpdateInstallTask &&) noexcept;
	UpdateInstallTask &operator=(UpdateInstallTask &&) noexcept;
	~UpdateInstallTask();

	UpdateInstallTask(const UpdateInstallTask &) = delete;
	UpdateInstallTask &operator=(const UpdateInstallTask &) = delete;

	[[nodiscard]] std::optional<UpdateInstallTaskResult> take_completed();
	[[nodiscard]] bool finished() const noexcept;
	[[nodiscard]] bool worker_failed() const noexcept;

private:
	struct Impl;
	explicit UpdateInstallTask(Impl *impl) noexcept;
	Impl *impl_ = nullptr;

	friend std::expected<UpdateInstallTask, UpdateTaskStartError>
	start_update_install_task(
		const UpdateReleaseConfiguration &,
		UpdateRuntimeContext,
		UpdateTransport
	);
};

[[nodiscard]] std::expected<UpdateInstallTask, UpdateTaskStartError>
start_update_install_task(
	const UpdateReleaseConfiguration &configuration,
	UpdateRuntimeContext runtime,
	UpdateTransport transport
);

} // namespace th03::app

#endif
