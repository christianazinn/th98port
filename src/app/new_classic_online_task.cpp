#include "th03/app/new_classic_online_task.hpp"

#include <atomic>
#include <deque>
#include <mutex>
#include <new>
#include <stop_token>
#include <thread>
#include <utility>

namespace th03::app {

struct NewClassicOnlineBoardRefreshTask::Impl final {
	std::mutex mutex{};
	std::deque<NewClassicOnlineBoardRefreshResult> completed{};
	std::atomic<bool> finished = false;
	std::atomic<bool> worker_failed = false;
	std::jthread worker{};
};

struct NewClassicOnlineReplayDownloadTask::Impl final {
	std::mutex mutex{};
	std::optional<NewClassicOnlineReplayDownloadResult> completed{};
	std::atomic<bool> finished = false;
	std::atomic<bool> worker_failed = false;
	std::jthread worker{};
};

NewClassicOnlineBoardRefreshTask::NewClassicOnlineBoardRefreshTask(
	Impl *impl
) noexcept : impl_(impl)
{
}

NewClassicOnlineBoardRefreshTask::NewClassicOnlineBoardRefreshTask(
	NewClassicOnlineBoardRefreshTask &&other
) noexcept : impl_(std::exchange(other.impl_, nullptr))
{
}

NewClassicOnlineBoardRefreshTask &
NewClassicOnlineBoardRefreshTask::operator=(
	NewClassicOnlineBoardRefreshTask &&other
) noexcept
{
	if(this != &other) {
		delete impl_;
		impl_ = std::exchange(other.impl_, nullptr);
	}
	return *this;
}

NewClassicOnlineBoardRefreshTask::~NewClassicOnlineBoardRefreshTask()
{
	delete impl_;
}

std::optional<NewClassicOnlineBoardRefreshResult>
NewClassicOnlineBoardRefreshTask::take_completed()
{
	if(impl_ == nullptr) {
		return std::nullopt;
	}
	const std::scoped_lock lock(impl_->mutex);
	if(impl_->completed.empty()) {
		return std::nullopt;
	}
	auto result = std::move(impl_->completed.front());
	impl_->completed.pop_front();
	return result;
}

bool NewClassicOnlineBoardRefreshTask::finished() const noexcept
{
	return impl_ == nullptr || impl_->finished.load(std::memory_order_acquire);
}

bool NewClassicOnlineBoardRefreshTask::worker_failed() const noexcept
{
	return impl_ != nullptr &&
		impl_->worker_failed.load(std::memory_order_acquire);
}

std::expected<NewClassicOnlineBoardRefreshTask, NewClassicOnlineTaskError>
start_new_classic_online_board_refresh(
	std::vector<NewClassicOnlineCategory> categories,
	std::filesystem::path cache_root,
	NewClassicOnlineTransport transport
)
{
	if(transport.context == nullptr || transport.perform == nullptr) {
		return std::unexpected(NewClassicOnlineTaskError{
			.code = NewClassicOnlineTaskErrorCode::invalid_transport,
		});
	}
	if(categories.empty()) {
		return std::unexpected(NewClassicOnlineTaskError{
			.code = NewClassicOnlineTaskErrorCode::empty_request,
		});
	}

	auto impl = std::unique_ptr<NewClassicOnlineBoardRefreshTask::Impl>(
		new(std::nothrow) NewClassicOnlineBoardRefreshTask::Impl()
	);
	if(!impl) {
		return std::unexpected(NewClassicOnlineTaskError{
			.code = NewClassicOnlineTaskErrorCode::worker_start,
		});
	}
	try {
		auto *context = impl.get();
		impl->worker = std::jthread(
			[context, categories = std::move(categories),
			 cache_root = std::move(cache_root), transport](std::stop_token stop) {
				try {
					for(const auto &category : categories) {
						if(stop.stop_requested()) {
							break;
						}
						auto refreshed = refresh_new_classic_online_board(
							category, cache_root, transport
						);
						NewClassicOnlineBoardRefreshResult result{
							.category = category,
						};
						if(refreshed) {
							result.document = std::move(*refreshed);
						} else {
							result.error = std::move(refreshed.error());
						}
						{
							const std::scoped_lock lock(context->mutex);
							context->completed.push_back(std::move(result));
						}
					}
				} catch(...) {
					context->worker_failed.store(true, std::memory_order_release);
				}
				context->finished.store(true, std::memory_order_release);
			}
		);
	} catch(...) {
		return std::unexpected(NewClassicOnlineTaskError{
			.code = NewClassicOnlineTaskErrorCode::worker_start,
		});
	}
	return NewClassicOnlineBoardRefreshTask(impl.release());
}

NewClassicOnlineReplayDownloadTask::NewClassicOnlineReplayDownloadTask(
	Impl *impl
) noexcept : impl_(impl)
{
}

NewClassicOnlineReplayDownloadTask::NewClassicOnlineReplayDownloadTask(
	NewClassicOnlineReplayDownloadTask &&other
) noexcept : impl_(std::exchange(other.impl_, nullptr))
{
}

NewClassicOnlineReplayDownloadTask &
NewClassicOnlineReplayDownloadTask::operator=(
	NewClassicOnlineReplayDownloadTask &&other
) noexcept
{
	if(this != &other) {
		delete impl_;
		impl_ = std::exchange(other.impl_, nullptr);
	}
	return *this;
}

NewClassicOnlineReplayDownloadTask::~NewClassicOnlineReplayDownloadTask()
{
	delete impl_;
}

std::optional<NewClassicOnlineReplayDownloadResult>
NewClassicOnlineReplayDownloadTask::take_completed()
{
	if(impl_ == nullptr) {
		return std::nullopt;
	}
	const std::scoped_lock lock(impl_->mutex);
	return std::exchange(impl_->completed, std::nullopt);
}

bool NewClassicOnlineReplayDownloadTask::finished() const noexcept
{
	return impl_ == nullptr || impl_->finished.load(std::memory_order_acquire);
}

bool NewClassicOnlineReplayDownloadTask::worker_failed() const noexcept
{
	return impl_ != nullptr &&
		impl_->worker_failed.load(std::memory_order_acquire);
}

std::expected<NewClassicOnlineReplayDownloadTask, NewClassicOnlineTaskError>
start_new_classic_online_replay_download(
	NewClassicOnlineReplayRecord record,
	std::filesystem::path replay_root,
	NewClassicOnlineTransport transport
)
{
	if(transport.context == nullptr || transport.perform == nullptr) {
		return std::unexpected(NewClassicOnlineTaskError{
			.code = NewClassicOnlineTaskErrorCode::invalid_transport,
		});
	}
	if(replay_root.empty() ||
		!new_classic_online_identity_present(record.replay_id)) {
		return std::unexpected(NewClassicOnlineTaskError{
			.code = NewClassicOnlineTaskErrorCode::empty_request,
		});
	}
	auto impl = std::unique_ptr<NewClassicOnlineReplayDownloadTask::Impl>(
		new(std::nothrow) NewClassicOnlineReplayDownloadTask::Impl()
	);
	if(!impl) {
		return std::unexpected(NewClassicOnlineTaskError{
			.code = NewClassicOnlineTaskErrorCode::worker_start,
		});
	}
	try {
		auto *context = impl.get();
		impl->worker = std::jthread(
			[context, record = std::move(record),
			 replay_root = std::move(replay_root), transport](std::stop_token stop) {
				try {
					if(!stop.stop_requested()) {
						NewClassicOnlineReplayDownloadResult result{
							.replay_id = record.replay_id,
						};
						auto downloaded = download_new_classic_online_replay(
							record, replay_root, transport
						);
						if(downloaded) {
							result.path = std::move(*downloaded);
						} else {
							result.error = std::move(downloaded.error());
						}
						const std::scoped_lock lock(context->mutex);
						context->completed = std::move(result);
					}
				} catch(...) {
					context->worker_failed.store(true, std::memory_order_release);
				}
				context->finished.store(true, std::memory_order_release);
			}
		);
	} catch(...) {
		return std::unexpected(NewClassicOnlineTaskError{
			.code = NewClassicOnlineTaskErrorCode::worker_start,
		});
	}
	return NewClassicOnlineReplayDownloadTask(impl.release());
}

} // namespace th03::app
