#ifndef TH03_APP_NEW_CLASSIC_ONLINE_CLIENT_HPP
#define TH03_APP_NEW_CLASSIC_ONLINE_CLIENT_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "th03/app/new_classic_online.hpp"
#include "th03/app/new_classic_online_store.hpp"
#include "th03/app/replay_slot_store.hpp"
#include "th03/formats/replay.hpp"

namespace th03::app {

enum class NewClassicOnlineHttpMethod : std::uint8_t {
	get,
	post,
};

struct NewClassicOnlineHttpHeader final {
	std::string name{};
	std::string value{};
};

struct NewClassicOnlineHttpRequest final {
	NewClassicOnlineHttpMethod method = NewClassicOnlineHttpMethod::get;
	std::string target{};
	std::vector<NewClassicOnlineHttpHeader> headers{};
	std::span<const std::byte> body{};
	std::size_t maximum_response_bytes = 0U;
};

struct NewClassicOnlineHttpResponse final {
	std::uint16_t status = 0U;
	std::string content_type{};
	std::vector<std::byte> body{};
};

enum class NewClassicOnlineTransportErrorCode : std::uint8_t {
	unavailable,
	timeout,
	tls,
	request,
	response_limit,
	canceled,
};

struct NewClassicOnlineTransportError final {
	NewClassicOnlineTransportErrorCode code =
		NewClassicOnlineTransportErrorCode::unavailable;
};

// The transaction layer owns all request bytes for the duration of this
// synchronous call. A platform adapter owns HTTPS, timeouts, and cancellation;
// gameplay never calls this seam directly.
struct NewClassicOnlineTransport final {
	void *context = nullptr;
	std::expected<NewClassicOnlineHttpResponse, NewClassicOnlineTransportError>
		(*perform)(void *, const NewClassicOnlineHttpRequest &) = nullptr;
};

struct NewClassicOnlineCredentials final {
	std::string user_id{};
	std::string token{};

	friend bool operator==(
		const NewClassicOnlineCredentials &,
		const NewClassicOnlineCredentials &
	) = default;
};

[[nodiscard]] bool validate_new_classic_online_credentials(
	const NewClassicOnlineCredentials &credentials
) noexcept;

enum class NewClassicOnlineUploadStatus : std::uint8_t {
	accepted,
	rejected,
};

struct NewClassicOnlineUploadResult final {
	NewClassicOnlineUploadStatus status = NewClassicOnlineUploadStatus::rejected;
	std::string replay_id{};
	std::string entry_id{};
	std::string reason{};
};

enum class NewClassicOnlineClientErrorCode : std::uint8_t {
	invalid_category,
	invalid_credentials,
	invalid_document,
	transport,
	http_status,
	content_type,
	response_shape,
	response_limit,
	cache_codec,
	cache_mismatch,
	cache_store,
	replay_digest,
	replay_format,
	replay_metadata,
	replay_store,
};

struct NewClassicOnlineClientError final {
	NewClassicOnlineClientErrorCode code =
		NewClassicOnlineClientErrorCode::transport;
	std::uint16_t http_status = 0U;
	std::optional<NewClassicOnlineTransportError> transport{};
	std::optional<NewClassicOnlineError> cache_codec{};
	std::optional<NewClassicOnlineStoreError> cache_store{};
	std::optional<Error> replay_format{};
	std::optional<ReplaySlotStoreError> replay_store{};
};

[[nodiscard]] std::filesystem::path new_classic_online_board_cache_path(
	const std::filesystem::path &cache_root,
	const NewClassicOnlineCategory &category
);

[[nodiscard]] std::expected<
	NewClassicOnlineCacheDocument,
	NewClassicOnlineClientError
> refresh_new_classic_online_board(
	const NewClassicOnlineCategory &category,
	const std::filesystem::path &cache_root,
	const NewClassicOnlineTransport &transport
);

[[nodiscard]] std::expected<
	NewClassicOnlineCredentials,
	NewClassicOnlineClientError
> enroll_new_classic_online_user(const NewClassicOnlineTransport &transport);

[[nodiscard]] std::expected<
	NewClassicOnlineUploadResult,
	NewClassicOnlineClientError
> upload_new_classic_online_replay(
	const formats::ReplayDocument &document,
	const NewClassicOnlineCredentials &credentials,
	const NewClassicOnlineTransport &transport
);

[[nodiscard]] std::expected<
	std::filesystem::path,
	NewClassicOnlineClientError
> download_new_classic_online_replay(
	const NewClassicOnlineReplayRecord &record,
	const std::filesystem::path &replay_root,
	const NewClassicOnlineTransport &transport
);

// Revalidates a content-addressed local download against the cached record.
// Missing files are ordinary false; malformed or inaccessible existing files
// are typed failures and are never advertised as downloaded.
[[nodiscard]] std::expected<bool, NewClassicOnlineClientError>
probe_new_classic_online_replay_download(
	const NewClassicOnlineReplayRecord &record,
	const std::filesystem::path &replay_root
);

// Re-reads and revalidates a content-addressed local download for immediate
// playback. Callers own this filesystem work outside deterministic source
// ticks; the returned document contains the exact stored V13 bytes.
[[nodiscard]] std::expected<formats::ReplayDocument, NewClassicOnlineClientError>
load_new_classic_online_replay(
	const NewClassicOnlineReplayRecord &record,
	const std::filesystem::path &replay_root
);

} // namespace th03::app

#endif
