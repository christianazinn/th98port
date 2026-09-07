#include "th03/app/update/update_task.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <span>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace th03::app {
namespace {

struct OwnedPinnedKey final {
	std::array<std::uint8_t, 16U> key_id{};
	std::vector<std::byte> subject_public_key_info{};
};

struct OwnedTaskInput final {
	std::string manifest_url{};
	UpdatePlatform platform = UpdatePlatform::windows_standalone;
	UpdateChannel channel = UpdateChannel::beta;
	std::uint64_t installed_sequence = 0U;
	std::vector<OwnedPinnedKey> pinned_keys{};
	UpdateRuntimeContext runtime{};
	UpdateTransport transport{};
};

[[nodiscard]] OwnedTaskInput own_input(
	const UpdateReleaseConfiguration &configuration,
	UpdateRuntimeContext runtime,
	UpdateTransport transport
)
{
	OwnedTaskInput result{
		.manifest_url = std::string(configuration.manifest_url),
		.platform = configuration.platform,
		.channel = configuration.channel,
		.installed_sequence = configuration.installed_sequence,
		.runtime = std::move(runtime),
		.transport = transport,
	};
	result.pinned_keys.reserve(configuration.pinned_keys.size());
	for(const auto &pin : configuration.pinned_keys) {
		result.pinned_keys.push_back(OwnedPinnedKey{
			.key_id = pin.key_id,
			.subject_public_key_info = std::vector<std::byte>(
				pin.subject_public_key_info.begin(),
				pin.subject_public_key_info.end()
			),
		});
	}
	return result;
}

} // namespace

struct UpdateInstallTask::Impl final {
	std::mutex mutex{};
	std::optional<UpdateInstallTaskResult> completed{};
	std::atomic<bool> finished = false;
	std::atomic<bool> worker_failed = false;
	std::jthread worker{};
};

bool valid_update_runtime_context(
	const UpdateRuntimeContext &context
) noexcept
{
	return !context.activation_state.empty() &&
		!context.generation_root.empty();
}

std::expected<std::optional<UpdateRuntimeContext>, UpdateRuntimeContextError>
parse_update_runtime_context(
	std::optional<std::string_view> activation_state,
	std::optional<std::string_view> generation_root
)
{
	if(!activation_state && !generation_root) {
		return std::optional<UpdateRuntimeContext>{};
	}
	if(!activation_state || !generation_root || activation_state->empty() ||
		generation_root->empty()) {
		return std::unexpected(UpdateRuntimeContextError::incomplete);
	}
	return std::optional<UpdateRuntimeContext>{UpdateRuntimeContext{
		.activation_state = std::filesystem::path(*activation_state),
		.generation_root = std::filesystem::path(*generation_root),
	}};
}

bool new_classic_update_available(
	bool new_classic,
	bool source_capture,
	bool publication_oracle,
	const std::optional<UpdateReleaseConfiguration> &configuration,
	const std::optional<UpdateRuntimeContext> &runtime,
	UpdateTransport transport
) noexcept
{
	return new_classic && !source_capture && !publication_oracle &&
		configuration && valid_update_release_configuration(*configuration) &&
		runtime && valid_update_runtime_context(*runtime) &&
		transport.perform != nullptr;
}

bool claim_new_classic_automatic_update_attempt(
	bool available,
	bool enabled,
	bool &attempted
) noexcept
{
	if(!available || !enabled || attempted) {
		return false;
	}
	attempted = true;
	return true;
}

UpdateInstallTask::UpdateInstallTask(Impl *impl) noexcept : impl_(impl)
{
}

UpdateInstallTask::UpdateInstallTask(UpdateInstallTask &&other) noexcept :
	impl_(std::exchange(other.impl_, nullptr))
{
}

UpdateInstallTask &UpdateInstallTask::operator=(
	UpdateInstallTask &&other
) noexcept
{
	if(this != &other) {
		delete impl_;
		impl_ = std::exchange(other.impl_, nullptr);
	}
	return *this;
}

UpdateInstallTask::~UpdateInstallTask()
{
	delete impl_;
}

std::optional<UpdateInstallTaskResult> UpdateInstallTask::take_completed()
{
	if(impl_ == nullptr) {
		return std::nullopt;
	}
	const std::scoped_lock lock(impl_->mutex);
	return std::exchange(impl_->completed, std::nullopt);
}

bool UpdateInstallTask::finished() const noexcept
{
	return impl_ == nullptr || impl_->finished.load(std::memory_order_acquire);
}

bool UpdateInstallTask::worker_failed() const noexcept
{
	return impl_ != nullptr &&
		impl_->worker_failed.load(std::memory_order_acquire);
}

std::expected<UpdateInstallTask, UpdateTaskStartError>
start_update_install_task(
	const UpdateReleaseConfiguration &configuration,
	UpdateRuntimeContext runtime,
	UpdateTransport transport
)
{
	if(!valid_update_release_configuration(configuration)) {
		return std::unexpected(UpdateTaskStartError{
			.code = UpdateTaskStartErrorCode::invalid_configuration,
		});
	}
	if(!valid_update_runtime_context(runtime)) {
		return std::unexpected(UpdateTaskStartError{
			.code = UpdateTaskStartErrorCode::invalid_runtime_context,
		});
	}
	if(transport.perform == nullptr) {
		return std::unexpected(UpdateTaskStartError{
			.code = UpdateTaskStartErrorCode::invalid_transport,
		});
	}
	auto input = own_input(configuration, std::move(runtime), transport);
	auto impl = std::unique_ptr<UpdateInstallTask::Impl>(
		new(std::nothrow) UpdateInstallTask::Impl()
	);
	if(!impl) {
		return std::unexpected(UpdateTaskStartError{
			.code = UpdateTaskStartErrorCode::worker_start,
		});
	}
	try {
		auto *context = impl.get();
		impl->worker = std::jthread(
			[context, input = std::move(input)](std::stop_token stop) {
				try {
					if(!stop.stop_requested()) {
						std::vector<UpdatePinnedKey> pins{};
						pins.reserve(input.pinned_keys.size());
						for(const auto &pin : input.pinned_keys) {
							pins.push_back(UpdatePinnedKey{
								.key_id = pin.key_id,
								.subject_public_key_info =
									pin.subject_public_key_info,
							});
						}
						auto installed = install_update_for_next_launch(
							input.manifest_url, pins, input.platform,
							input.channel, input.installed_sequence,
							input.runtime.activation_state,
							input.runtime.generation_root, input.transport
						);
						const std::scoped_lock lock(context->mutex);
						context->completed.emplace(std::move(installed));
					}
				} catch(...) {
					context->worker_failed.store(
						true, std::memory_order_release
					);
				}
				context->finished.store(true, std::memory_order_release);
			}
		);
	} catch(...) {
		return std::unexpected(UpdateTaskStartError{
			.code = UpdateTaskStartErrorCode::worker_start,
		});
	}
	return UpdateInstallTask(impl.release());
}

} // namespace th03::app
