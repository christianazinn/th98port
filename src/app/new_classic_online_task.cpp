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

struct NewClassicOnlineMyRecordsRefreshTask::Impl final {
	std::mutex mutex{};
	std::deque<NewClassicOnlineBoardRefreshResult> completed{};
	std::optional<NewClassicOnlineCredentialAcquireError> credential_error{};
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

struct NewClassicOnlineReplayUploadTask::Impl final {
	std::mutex mutex{};
	std::optional<NewClassicOnlineReplayUploadResult> completed{};
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

NewClassicOnlineMyRecordsRefreshTask::NewClassicOnlineMyRecordsRefreshTask(
	Impl *impl
) noexcept : impl_(impl)
{
}

NewClassicOnlineMyRecordsRefreshTask::NewClassicOnlineMyRecordsRefreshTask(
	NewClassicOnlineMyRecordsRefreshTask &&other
) noexcept : impl_(std::exchange(other.impl_, nullptr))
{
}

NewClassicOnlineMyRecordsRefreshTask &
NewClassicOnlineMyRecordsRefreshTask::operator=(
	NewClassicOnlineMyRecordsRefreshTask &&other
) noexcept
{
	if(this != &other) {
		delete impl_;
		impl_ = std::exchange(other.impl_, nullptr);
	}
	return *this;
}

NewClassicOnlineMyRecordsRefreshTask::~NewClassicOnlineMyRecordsRefreshTask()
{
	delete impl_;
}

std::optional<NewClassicOnlineBoardRefreshResult>
NewClassicOnlineMyRecordsRefreshTask::take_completed()
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

bool NewClassicOnlineMyRecordsRefreshTask::finished() const noexcept
{
	return impl_ == nullptr || impl_->finished.load(std::memory_order_acquire);
}

bool NewClassicOnlineMyRecordsRefreshTask::worker_failed() const noexcept
{
	return impl_ != nullptr &&
		impl_->worker_failed.load(std::memory_order_acquire);
}

std::optional<NewClassicOnlineCredentialAcquireError>
NewClassicOnlineMyRecordsRefreshTask::take_credential_error() noexcept
{
	if(impl_ == nullptr) {
		return std::nullopt;
	}
	const std::scoped_lock lock(impl_->mutex);
	return std::exchange(impl_->credential_error, std::nullopt);
}

std::expected<NewClassicOnlineMyRecordsRefreshTask, NewClassicOnlineTaskError>
start_new_classic_online_my_records_refresh(
	std::vector<NewClassicOnlineCategory> categories,
	NewClassicOnlineCredentialSource credential_source,
	NewClassicOnlineTransport transport
)
{
	if(transport.context == nullptr || transport.perform == nullptr) {
		return std::unexpected(NewClassicOnlineTaskError{
			.code = NewClassicOnlineTaskErrorCode::invalid_transport,
		});
	}
	if(credential_source.context == nullptr ||
		credential_source.acquire == nullptr) {
		return std::unexpected(NewClassicOnlineTaskError{
			.code = NewClassicOnlineTaskErrorCode::invalid_credential_source,
		});
	}
	if(categories.empty()) {
		return std::unexpected(NewClassicOnlineTaskError{
			.code = NewClassicOnlineTaskErrorCode::empty_request,
		});
	}
	auto impl = std::unique_ptr<NewClassicOnlineMyRecordsRefreshTask::Impl>(
		new(std::nothrow) NewClassicOnlineMyRecordsRefreshTask::Impl()
	);
	if(!impl) {
		return std::unexpected(NewClassicOnlineTaskError{
			.code = NewClassicOnlineTaskErrorCode::worker_start,
		});
	}
	try {
		auto *context = impl.get();
		impl->worker = std::jthread(
			[context, categories = std::move(categories), credential_source,
			 transport](std::stop_token stop) {
				try {
					auto credentials = credential_source.acquire(
						credential_source.context
					);
					if(!credentials) {
						const std::scoped_lock lock(context->mutex);
						context->credential_error = credentials.error();
					} else {
						for(const auto &category : categories) {
							if(stop.stop_requested()) {
								break;
							}
							auto fetched = fetch_new_classic_online_records(
								category, *credentials, transport
							);
							NewClassicOnlineBoardRefreshResult result{
								.category = category,
							};
							if(fetched) {
								result.document = std::move(*fetched);
							} else {
								result.error = std::move(fetched.error());
							}
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
	return NewClassicOnlineMyRecordsRefreshTask(impl.release());
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

NewClassicOnlineReplayUploadTask::NewClassicOnlineReplayUploadTask(
	Impl *impl
) noexcept : impl_(impl)
{
}

NewClassicOnlineReplayUploadTask::NewClassicOnlineReplayUploadTask(
	NewClassicOnlineReplayUploadTask &&other
) noexcept : impl_(std::exchange(other.impl_, nullptr))
{
}

NewClassicOnlineReplayUploadTask &
NewClassicOnlineReplayUploadTask::operator=(
	NewClassicOnlineReplayUploadTask &&other
) noexcept
{
	if(this != &other) {
		delete impl_;
		impl_ = std::exchange(other.impl_, nullptr);
	}
	return *this;
}

NewClassicOnlineReplayUploadTask::~NewClassicOnlineReplayUploadTask()
{
	delete impl_;
}

std::optional<NewClassicOnlineReplayUploadResult>
NewClassicOnlineReplayUploadTask::take_completed()
{
	if(impl_ == nullptr) {
		return std::nullopt;
	}
	const std::scoped_lock lock(impl_->mutex);
	return std::exchange(impl_->completed, std::nullopt);
}

bool NewClassicOnlineReplayUploadTask::finished() const noexcept
{
	return impl_ == nullptr || impl_->finished.load(std::memory_order_acquire);
}

bool NewClassicOnlineReplayUploadTask::worker_failed() const noexcept
{
	return impl_ != nullptr &&
		impl_->worker_failed.load(std::memory_order_acquire);
}

std::expected<NewClassicOnlineReplayUploadTask, NewClassicOnlineTaskError>
start_new_classic_online_replay_upload(
	formats::ReplayDocument document,
	std::uint8_t slot,
	NewClassicOnlineCredentialSource credential_source,
	NewClassicOnlineTransport transport
)
{
	if(transport.context == nullptr || transport.perform == nullptr) {
		return std::unexpected(NewClassicOnlineTaskError{
			.code = NewClassicOnlineTaskErrorCode::invalid_transport,
		});
	}
	if(credential_source.context == nullptr ||
		credential_source.acquire == nullptr) {
		return std::unexpected(NewClassicOnlineTaskError{
			.code = NewClassicOnlineTaskErrorCode::invalid_credential_source,
		});
	}
	if(slot >= replay_slot_count || !document.metadata) {
		return std::unexpected(NewClassicOnlineTaskError{
			.code = NewClassicOnlineTaskErrorCode::empty_request,
		});
	}
	auto impl = std::unique_ptr<NewClassicOnlineReplayUploadTask::Impl>(
		new(std::nothrow) NewClassicOnlineReplayUploadTask::Impl()
	);
	if(!impl) {
		return std::unexpected(NewClassicOnlineTaskError{
			.code = NewClassicOnlineTaskErrorCode::worker_start,
		});
	}
	try {
		auto *context = impl.get();
		impl->worker = std::jthread(
			[context, document = std::move(document), slot,
			 credential_source, transport](std::stop_token stop) {
				try {
					NewClassicOnlineReplayUploadResult result{.slot = slot};
					if(!stop.stop_requested()) {
						auto credentials = credential_source.acquire(
							credential_source.context
						);
						if(!credentials) {
							result.credential_error = credentials.error();
						} else if(!stop.stop_requested()) {
							auto uploaded = upload_new_classic_online_replay(
								document, *credentials, transport
							);
							if(uploaded) {
								result.upload = std::move(*uploaded);
							} else {
								result.error = std::move(uploaded.error());
							}
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
	return NewClassicOnlineReplayUploadTask(impl.release());
}

} // namespace th03::app
