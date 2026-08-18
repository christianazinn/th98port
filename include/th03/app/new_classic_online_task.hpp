#ifndef TH03_APP_NEW_CLASSIC_ONLINE_TASK_HPP
#define TH03_APP_NEW_CLASSIC_ONLINE_TASK_HPP

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include "th03/app/new_classic_online_client.hpp"

namespace th03::app {

enum class NewClassicOnlineTaskErrorCode : std::uint8_t {
	invalid_transport,
	invalid_credential_source,
	empty_request,
	worker_start,
};

struct NewClassicOnlineTaskError final {
	NewClassicOnlineTaskErrorCode code =
		NewClassicOnlineTaskErrorCode::invalid_transport;
};

struct NewClassicOnlineBoardRefreshResult final {
	NewClassicOnlineCategory category{};
	std::optional<NewClassicOnlineCacheDocument> document{};
	std::optional<NewClassicOnlineClientError> error{};
};

struct NewClassicOnlineReplayDownloadResult final {
	NewClassicOnlineReplayId replay_id{};
	std::optional<std::filesystem::path> path{};
	std::optional<NewClassicOnlineClientError> error{};
};

struct NewClassicOnlineCredentialAcquireError final {
	std::uint8_t detail_code = 0U;
};

// The platform owns credential persistence and protection. The worker invokes
// this seam off the logical thread before uploading; its context must outlive
// the task, just like the transport context.
struct NewClassicOnlineCredentialSource final {
	void *context = nullptr;
	std::expected<
		NewClassicOnlineCredentials,
		NewClassicOnlineCredentialAcquireError
	> (*acquire)(void *) = nullptr;
};

struct NewClassicOnlineReplayUploadResult final {
	std::uint8_t slot = 0U;
	std::optional<NewClassicOnlineUploadResult> upload{};
	std::optional<NewClassicOnlineClientError> error{};
	std::optional<NewClassicOnlineCredentialAcquireError> credential_error{};
};

// Owns one sequential background worker. The supplied transport context must
// outlive this object. Destruction requests cancellation between requests and
// joins the worker; the platform transport's bounded request deadline owns the
// maximum wait while one request is already in flight.
class NewClassicOnlineBoardRefreshTask final {
public:
	NewClassicOnlineBoardRefreshTask(
		NewClassicOnlineBoardRefreshTask &&
	) noexcept;
	NewClassicOnlineBoardRefreshTask &operator=(
		NewClassicOnlineBoardRefreshTask &&
	) noexcept;
	~NewClassicOnlineBoardRefreshTask();

	NewClassicOnlineBoardRefreshTask(
		const NewClassicOnlineBoardRefreshTask &
	) = delete;
	NewClassicOnlineBoardRefreshTask &operator=(
		const NewClassicOnlineBoardRefreshTask &
	) = delete;

	[[nodiscard]] std::optional<NewClassicOnlineBoardRefreshResult>
	take_completed();
	[[nodiscard]] bool finished() const noexcept;
	[[nodiscard]] bool worker_failed() const noexcept;

private:
	struct Impl;
	explicit NewClassicOnlineBoardRefreshTask(Impl *impl) noexcept;
	Impl *impl_ = nullptr;

	friend std::expected<
		NewClassicOnlineBoardRefreshTask,
		NewClassicOnlineTaskError
	> start_new_classic_online_board_refresh(
		std::vector<NewClassicOnlineCategory>,
		std::filesystem::path,
		NewClassicOnlineTransport
	);
};

[[nodiscard]] std::expected<
	NewClassicOnlineBoardRefreshTask,
	NewClassicOnlineTaskError
> start_new_classic_online_board_refresh(
	std::vector<NewClassicOnlineCategory> categories,
	std::filesystem::path cache_root,
	NewClassicOnlineTransport transport
);

// One host-owned replay download. Like the refresh task, the transport context
// must outlive this object and owns the bounded request deadline in flight.
class NewClassicOnlineReplayDownloadTask final {
public:
	NewClassicOnlineReplayDownloadTask(
		NewClassicOnlineReplayDownloadTask &&
	) noexcept;
	NewClassicOnlineReplayDownloadTask &operator=(
		NewClassicOnlineReplayDownloadTask &&
	) noexcept;
	~NewClassicOnlineReplayDownloadTask();

	NewClassicOnlineReplayDownloadTask(
		const NewClassicOnlineReplayDownloadTask &
	) = delete;
	NewClassicOnlineReplayDownloadTask &operator=(
		const NewClassicOnlineReplayDownloadTask &
	) = delete;

	[[nodiscard]] std::optional<NewClassicOnlineReplayDownloadResult>
	take_completed();
	[[nodiscard]] bool finished() const noexcept;
	[[nodiscard]] bool worker_failed() const noexcept;

private:
	struct Impl;
	explicit NewClassicOnlineReplayDownloadTask(Impl *impl) noexcept;
	Impl *impl_ = nullptr;

	friend std::expected<
		NewClassicOnlineReplayDownloadTask,
		NewClassicOnlineTaskError
	> start_new_classic_online_replay_download(
		NewClassicOnlineReplayRecord,
		std::filesystem::path,
		NewClassicOnlineTransport
	);
};

[[nodiscard]] std::expected<
	NewClassicOnlineReplayDownloadTask,
	NewClassicOnlineTaskError
> start_new_classic_online_replay_download(
	NewClassicOnlineReplayRecord record,
	std::filesystem::path replay_root,
	NewClassicOnlineTransport transport
);

// One explicit upload of an immutable finalized replay. Canonical V13 encoding
// stays in memory; this owner never creates an upload candidate on disk.
class NewClassicOnlineReplayUploadTask final {
public:
	NewClassicOnlineReplayUploadTask(
		NewClassicOnlineReplayUploadTask &&
	) noexcept;
	NewClassicOnlineReplayUploadTask &operator=(
		NewClassicOnlineReplayUploadTask &&
	) noexcept;
	~NewClassicOnlineReplayUploadTask();

	NewClassicOnlineReplayUploadTask(
		const NewClassicOnlineReplayUploadTask &
	) = delete;
	NewClassicOnlineReplayUploadTask &operator=(
		const NewClassicOnlineReplayUploadTask &
	) = delete;

	[[nodiscard]] std::optional<NewClassicOnlineReplayUploadResult>
	take_completed();
	[[nodiscard]] bool finished() const noexcept;
	[[nodiscard]] bool worker_failed() const noexcept;

private:
	struct Impl;
	explicit NewClassicOnlineReplayUploadTask(Impl *impl) noexcept;
	Impl *impl_ = nullptr;

	friend std::expected<
		NewClassicOnlineReplayUploadTask,
		NewClassicOnlineTaskError
	> start_new_classic_online_replay_upload(
		formats::ReplayDocument,
		std::uint8_t,
		NewClassicOnlineCredentialSource,
		NewClassicOnlineTransport
	);
};

[[nodiscard]] std::expected<
	NewClassicOnlineReplayUploadTask,
	NewClassicOnlineTaskError
> start_new_classic_online_replay_upload(
	formats::ReplayDocument document,
	std::uint8_t slot,
	NewClassicOnlineCredentialSource credential_source,
	NewClassicOnlineTransport transport
);

} // namespace th03::app

#endif
