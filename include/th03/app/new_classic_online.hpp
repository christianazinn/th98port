#ifndef TH03_APP_NEW_CLASSIC_ONLINE_HPP
#define TH03_APP_NEW_CLASSIC_ONLINE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace th03::app {

inline constexpr std::size_t new_classic_online_identity_bytes = 32U;
inline constexpr std::size_t new_classic_online_player_bytes = 48U;
inline constexpr std::size_t new_classic_online_evidence_bytes = 160U;
inline constexpr std::size_t new_classic_online_note_bytes = 160U;
inline constexpr std::size_t new_classic_online_title_bytes = 80U;
inline constexpr std::size_t new_classic_online_entry_limit = 256U;
inline constexpr std::size_t new_classic_online_replay_limit = 256U;
inline constexpr std::size_t new_classic_online_cache_max_bytes = 256U * 1024U;
inline constexpr std::size_t new_classic_online_cache_header_bytes = 64U;
inline constexpr std::size_t new_classic_online_cache_footer_bytes = 4U;

template<typename Tag>
struct NewClassicOnlineIdentity final {
	// Every ID is the SHA-256 digest of a type-specific canonical document:
	// exact V13 bytes for ReplayId, pinned manifests for content/ruleset/verifier,
	// and canonical entry/snapshot payloads for their respective IDs. The tag
	// prevents unrelated 32-byte identities from being mixed by the client.
	std::array<std::uint8_t, new_classic_online_identity_bytes> bytes{};

	friend constexpr bool operator==(
		const NewClassicOnlineIdentity &,
		const NewClassicOnlineIdentity &
	) = default;
};

struct NewClassicOnlineEntryTag;
struct NewClassicOnlineReplayTag;
struct NewClassicOnlineContentTag;
struct NewClassicOnlineRulesetTag;
struct NewClassicOnlineVerifierTag;
struct NewClassicOnlineSnapshotTag;

using NewClassicOnlineEntryId =
	NewClassicOnlineIdentity<NewClassicOnlineEntryTag>;
using NewClassicOnlineReplayId =
	NewClassicOnlineIdentity<NewClassicOnlineReplayTag>;
using NewClassicOnlineContentId =
	NewClassicOnlineIdentity<NewClassicOnlineContentTag>;
using NewClassicOnlineRulesetId =
	NewClassicOnlineIdentity<NewClassicOnlineRulesetTag>;
using NewClassicOnlineVerifierId =
	NewClassicOnlineIdentity<NewClassicOnlineVerifierTag>;
using NewClassicOnlineSnapshotId =
	NewClassicOnlineIdentity<NewClassicOnlineSnapshotTag>;

enum class NewClassicOnlineMode : std::uint8_t {
	story,
	practice,
	vs_1p_cpu,
	vs_1p_2p,
};

enum class NewClassicOnlineProvenance : std::uint8_t {
	replay_verified,
	curated_legacy,
};

enum class NewClassicOnlineResultKind : std::uint8_t {
	game_over,
	clear,
	practice_complete,
	versus_complete,
};

enum class NewClassicOnlineReplayAvailability : std::uint8_t {
	metadata_only,
	downloaded,
};

struct NewClassicOnlineCategory final {
	NewClassicOnlineMode mode = NewClassicOnlineMode::story;
	std::uint8_t rank = 0U;
	std::uint8_t character = 0U;
	std::uint8_t start_stage = 0U;
	std::uint8_t start_round = 0U;

	friend constexpr bool operator==(
		const NewClassicOnlineCategory &,
		const NewClassicOnlineCategory &
	) = default;
};

struct NewClassicOnlineResult final {
	NewClassicOnlineResultKind kind = NewClassicOnlineResultKind::game_over;
	std::uint64_t score = 0U;
	std::uint8_t end_stage = 0U;
	std::uint8_t continues_used = 0U;

	friend constexpr bool operator==(
		const NewClassicOnlineResult &,
		const NewClassicOnlineResult &
	) = default;
};

struct NewClassicLeaderboardEntry final {
	NewClassicOnlineEntryId entry_id{};
	NewClassicOnlineContentId content_id{};
	NewClassicOnlineRulesetId ruleset_id{};
	std::optional<NewClassicOnlineReplayId> replay_id{};
	std::optional<NewClassicOnlineVerifierId> verifier_id{};
	NewClassicOnlineCategory category{};
	NewClassicOnlineResult result{};
	NewClassicOnlineProvenance provenance =
		NewClassicOnlineProvenance::replay_verified;
	std::uint16_t dos_date = 0U;
	std::uint64_t published_unix_seconds = 0U;
	std::string player{};
	std::string evidence_source{};
	std::string curator_note{};

	friend bool operator==(
		const NewClassicLeaderboardEntry &,
		const NewClassicLeaderboardEntry &
	) = default;
};

struct NewClassicOnlineReplayRecord final {
	NewClassicOnlineReplayId replay_id{};
	NewClassicOnlineContentId content_id{};
	NewClassicOnlineRulesetId ruleset_id{};
	NewClassicOnlineCategory category{};
	NewClassicOnlineResult result{};
	NewClassicOnlineReplayAvailability availability =
		NewClassicOnlineReplayAvailability::metadata_only;
	std::uint16_t dos_date = 0U;
	std::uint32_t payload_bytes = 0U;
	std::string title{};

	friend bool operator==(
		const NewClassicOnlineReplayRecord &,
		const NewClassicOnlineReplayRecord &
	) = default;
};

struct NewClassicOnlineCacheDocument final {
	std::uint32_t generation = 0U;
	NewClassicOnlineSnapshotId snapshot_id{};
	std::uint64_t fetched_unix_seconds = 0U;
	std::vector<NewClassicLeaderboardEntry> leaderboard_entries{};
	std::vector<NewClassicOnlineReplayRecord> replay_records{};

	friend bool operator==(
		const NewClassicOnlineCacheDocument &,
		const NewClassicOnlineCacheDocument &
	) = default;
};

enum class NewClassicOnlineErrorCode : std::uint8_t {
	input_size,
	magic,
	version,
	header_shape,
	count,
	checksum,
	record_shape,
	identity,
	category,
	result,
	provenance,
	text,
	duplicate_identity,
	document_shape,
	trailing_data,
};

struct NewClassicOnlineError final {
	NewClassicOnlineErrorCode code = NewClassicOnlineErrorCode::input_size;
	std::size_t record = 0U;
	std::size_t offset = 0U;
};

template<typename Tag>
[[nodiscard]] constexpr bool new_classic_online_identity_present(
	const NewClassicOnlineIdentity<Tag> &identity
) noexcept
{
	for(const auto byte : identity.bytes) {
		if(byte != 0U) {
			return true;
		}
	}
	return false;
}

[[nodiscard]] std::expected<void, NewClassicOnlineError>
validate_new_classic_online_cache(
	const NewClassicOnlineCacheDocument &document
) noexcept;

[[nodiscard]] std::expected<std::vector<std::byte>, NewClassicOnlineError>
encode_new_classic_online_cache(
	const NewClassicOnlineCacheDocument &document
);

[[nodiscard]] std::expected<
	NewClassicOnlineCacheDocument,
	NewClassicOnlineError
> decode_new_classic_online_cache(std::span<const std::byte> bytes);

// Stable content-addressed filename for a downloaded replay payload. This is
// a local cache name, not a service URL or an authority over replay validity.
[[nodiscard]] std::string new_classic_online_replay_filename(
	const NewClassicOnlineReplayId &replay_id
);

} // namespace th03::app

#endif
