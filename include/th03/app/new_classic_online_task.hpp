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

} // namespace th03::app

#endif
