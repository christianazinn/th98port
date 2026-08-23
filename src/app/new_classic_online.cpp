#include "th03/app/new_classic_online.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/core/utf8.hpp"

namespace th03::app {
namespace {

constexpr std::array<std::byte, 8U> cache_magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'O'}, std::byte{'N'},
	std::byte{'L'}, std::byte{'0'}, std::byte{'0'}, std::byte{'2'},
};
constexpr std::uint16_t cache_version = 2U;
constexpr std::uint64_t maximum_score = 999999990ULL;

[[nodiscard]] NewClassicOnlineError error(
	NewClassicOnlineErrorCode code,
	std::size_t record = 0U,
	std::size_t offset = 0U
) noexcept
{
	return {code, record, offset};
}

[[nodiscard]] constexpr bool valid_mode(NewClassicOnlineMode mode) noexcept
{
	return mode >= NewClassicOnlineMode::story &&
		mode <= NewClassicOnlineMode::vs_1p_2p;
}

[[nodiscard]] constexpr bool valid_result_kind(
	NewClassicOnlineResultKind kind
) noexcept
{
	return kind >= NewClassicOnlineResultKind::game_over &&
		kind <= NewClassicOnlineResultKind::versus_complete;
}

[[nodiscard]] bool valid_utf8(std::string_view text) noexcept
{
	for(std::size_t offset = 0U; offset < text.size();) {
		const auto scalar = next_utf8_scalar(text, offset);
		if(!scalar || !unicode_printable_scalar(scalar->value)) {
			return false;
		}
		offset += scalar->bytes;
	}
	return true;
}

[[nodiscard]] constexpr bool valid_category(
	const NewClassicOnlineCategory &category
) noexcept
{
	if(!valid_mode(category.mode) || category.rank >= 4U ||
		category.character >= 9U || category.start_stage >= 9U ||
		category.start_round >= 4U) {
		return false;
	}
	if(category.mode != NewClassicOnlineMode::practice &&
		(category.start_stage != 0U || category.start_round != 0U)) {
		return false;
	}
	return true;
}

[[nodiscard]] constexpr bool valid_board_category(
	const NewClassicOnlineCategory &category
) noexcept
{
	if(category.character != new_classic_online_overall_character) {
		return valid_category(category);
	}
	return (category.mode == NewClassicOnlineMode::story ||
		category.mode == NewClassicOnlineMode::vs_1p_2p) &&
		category.rank < 4U &&
		category.start_stage == 0U && category.start_round == 0U;
}

[[nodiscard]] constexpr bool category_belongs_to_board(
	const NewClassicOnlineCategory &category,
	const NewClassicOnlineCategory &board
) noexcept
{
	return valid_category(category) && valid_board_category(board) &&
		category.mode == board.mode && category.rank == board.rank &&
		category.start_stage == board.start_stage &&
		category.start_round == board.start_round &&
		(board.character == new_classic_online_overall_character ||
		 category.character == board.character);
}

[[nodiscard]] constexpr bool valid_result(
	const NewClassicOnlineCategory &category,
	const NewClassicOnlineResult &result
) noexcept
{
	if(!valid_result_kind(result.kind) || result.score > maximum_score ||
		(result.score % 10U) != 0U || result.end_stage >= 9U) {
		return false;
	}
	switch(category.mode) {
	case NewClassicOnlineMode::story:
		return result.kind == NewClassicOnlineResultKind::game_over ||
			result.kind == NewClassicOnlineResultKind::clear;
	case NewClassicOnlineMode::practice:
		return result.kind == NewClassicOnlineResultKind::practice_complete;
	case NewClassicOnlineMode::vs_1p_cpu:
	case NewClassicOnlineMode::vs_1p_2p:
		return result.kind == NewClassicOnlineResultKind::versus_complete &&
			result.end_stage == 0U && result.continues_used == 0U;
	}
	return false;
}

[[nodiscard]] std::uint32_t fnv1a32(std::span<const std::byte> bytes) noexcept
{
	std::uint32_t hash = 2166136261U;
	for(const auto byte : bytes) {
		hash ^= std::to_integer<std::uint8_t>(byte);
		hash *= 16777619U;
	}
	return hash;
}

void append_u8(std::vector<std::byte> &output, std::uint8_t value)
{
	output.push_back(static_cast<std::byte>(value));
}

void append_u16(std::vector<std::byte> &output, std::uint16_t value)
{
	append_u8(output, static_cast<std::uint8_t>(value));
	append_u8(output, static_cast<std::uint8_t>(value >> 8U));
}

void append_u32(std::vector<std::byte> &output, std::uint32_t value)
{
	for(std::size_t byte = 0U; byte < 4U; ++byte) {
		append_u8(output, static_cast<std::uint8_t>(value >> (byte * 8U)));
	}
}

void append_u64(std::vector<std::byte> &output, std::uint64_t value)
{
	for(std::size_t byte = 0U; byte < 8U; ++byte) {
		append_u8(output, static_cast<std::uint8_t>(value >> (byte * 8U)));
	}
}

void set_u16(std::vector<std::byte> &output, std::size_t offset, std::uint16_t value)
{
	output[offset] = static_cast<std::byte>(value & 0xFFU);
	output[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

void set_u32(std::vector<std::byte> &output, std::size_t offset, std::uint32_t value)
{
	for(std::size_t byte = 0U; byte < 4U; ++byte) {
		output[offset + byte] = static_cast<std::byte>(value >> (byte * 8U));
	}
}

template<typename Tag>
void append_identity(
	std::vector<std::byte> &output,
	const NewClassicOnlineIdentity<Tag> &identity
)
{
	for(const auto byte : identity.bytes) {
		append_u8(output, byte);
	}
}

template<typename Tag>
void append_optional_identity(
	std::vector<std::byte> &output,
	const std::optional<NewClassicOnlineIdentity<Tag>> &identity
)
{
	append_identity(output, identity.value_or(NewClassicOnlineIdentity<Tag>{}));
}

void append_text(std::vector<std::byte> &output, const std::string &text)
{
	append_u16(output, static_cast<std::uint16_t>(text.size()));
	output.insert(
		output.end(),
		reinterpret_cast<const std::byte *>(text.data()),
		reinterpret_cast<const std::byte *>(text.data() + text.size())
	);
}

void append_category_result(
	std::vector<std::byte> &output,
	const NewClassicOnlineCategory &category,
	const NewClassicOnlineResult &result,
	std::uint8_t policy,
	std::uint8_t displayed_autofire = 0U
)
{
	append_u8(output, static_cast<std::uint8_t>(category.mode));
	append_u8(output, category.rank);
	append_u8(output, category.character);
	append_u8(output, category.start_stage);
	append_u8(output, category.start_round);
	append_u8(output, static_cast<std::uint8_t>(result.kind));
	append_u8(output, result.end_stage);
	append_u8(output, result.continues_used);
	append_u8(output, policy);
	append_u8(output, displayed_autofire);
	append_u64(output, result.score);
}

class Reader final {
public:
	explicit Reader(std::span<const std::byte> bytes) noexcept : bytes_(bytes) {}

	[[nodiscard]] std::size_t offset() const noexcept { return offset_; }
	[[nodiscard]] std::size_t remaining() const noexcept {
		return bytes_.size() - std::min(offset_, bytes_.size());
	}
	[[nodiscard]] bool seek(std::size_t offset) noexcept {
		if(offset > bytes_.size()) {
			return false;
		}
		offset_ = offset;
		return true;
	}
	[[nodiscard]] std::optional<std::uint8_t> u8() noexcept {
		if(remaining() < 1U) {
			return std::nullopt;
		}
		return std::to_integer<std::uint8_t>(bytes_[offset_++]);
	}
	[[nodiscard]] std::optional<std::uint16_t> u16() noexcept {
		if(remaining() < 2U) {
			return std::nullopt;
		}
		const auto result = static_cast<std::uint16_t>(
			std::to_integer<std::uint8_t>(bytes_[offset_]) |
			(std::to_integer<std::uint8_t>(bytes_[offset_ + 1U]) << 8U)
		);
		offset_ += 2U;
		return result;
	}
	[[nodiscard]] std::optional<std::uint32_t> u32() noexcept {
		if(remaining() < 4U) {
			return std::nullopt;
		}
		std::uint32_t result = 0U;
		for(std::size_t byte = 0U; byte < 4U; ++byte) {
			result |= static_cast<std::uint32_t>(
				std::to_integer<std::uint8_t>(bytes_[offset_ + byte])
			) << (byte * 8U);
		}
		offset_ += 4U;
		return result;
	}
	[[nodiscard]] std::optional<std::uint64_t> u64() noexcept {
		if(remaining() < 8U) {
			return std::nullopt;
		}
		std::uint64_t result = 0U;
		for(std::size_t byte = 0U; byte < 8U; ++byte) {
			result |= static_cast<std::uint64_t>(
				std::to_integer<std::uint8_t>(bytes_[offset_ + byte])
			) << (byte * 8U);
		}
		offset_ += 8U;
		return result;
	}
	template<typename Tag>
	[[nodiscard]] std::optional<NewClassicOnlineIdentity<Tag>> identity() noexcept {
		if(remaining() < new_classic_online_identity_bytes) {
			return std::nullopt;
		}
		NewClassicOnlineIdentity<Tag> result{};
		for(auto &byte : result.bytes) {
			byte = std::to_integer<std::uint8_t>(bytes_[offset_++]);
		}
		return result;
	}
	[[nodiscard]] std::optional<std::string> text() {
		const auto size = u16();
		if(!size || remaining() < *size) {
			return std::nullopt;
		}
		std::string result(
			reinterpret_cast<const char *>(bytes_.data() + offset_), *size
		);
		offset_ += *size;
		return result;
	}

private:
	std::span<const std::byte> bytes_{};
	std::size_t offset_ = 0U;
};

[[nodiscard]] std::optional<NewClassicOnlineCategory> read_category(
	Reader &reader
) noexcept
{
	const auto mode = reader.u8();
	const auto rank = reader.u8();
	const auto character = reader.u8();
	const auto stage = reader.u8();
	const auto round = reader.u8();
	if(!mode || !rank || !character || !stage || !round) {
		return std::nullopt;
	}
	return NewClassicOnlineCategory{
		static_cast<NewClassicOnlineMode>(*mode), *rank, *character, *stage, *round
	};
}

struct ReadResultPolicy final {
	NewClassicOnlineResult result{};
	std::uint8_t policy = 0U;
	std::uint8_t displayed_autofire = 0U;
};

[[nodiscard]] std::optional<ReadResultPolicy> read_result_policy(
	Reader &reader
) noexcept
{
	const auto kind = reader.u8();
	const auto stage = reader.u8();
	const auto continues = reader.u8();
	const auto policy = reader.u8();
	const auto displayed_autofire = reader.u8();
	const auto score = reader.u64();
	if(!kind || !stage || !continues || !policy || !displayed_autofire ||
		*displayed_autofire > 2U ||
		!score) {
		return std::nullopt;
	}
	return ReadResultPolicy{{
		static_cast<NewClassicOnlineResultKind>(*kind), *score, *stage, *continues
	}, *policy, *displayed_autofire};
}

template<typename Tag>
[[nodiscard]] std::optional<NewClassicOnlineIdentity<Tag>> optional_identity(
	NewClassicOnlineIdentity<Tag> identity
) noexcept
{
	if(!new_classic_online_identity_present(identity)) {
		return std::nullopt;
	}
	return identity;
}

} // namespace

std::expected<void, NewClassicOnlineError> validate_new_classic_online_cache(
	const NewClassicOnlineCacheDocument &document
) noexcept
{
	if(document.leaderboard_entries.size() > new_classic_online_entry_limit ||
		document.replay_records.size() > new_classic_online_replay_limit) {
		return std::unexpected(error(NewClassicOnlineErrorCode::count));
	}
	const bool no_records = document.leaderboard_entries.empty() &&
		document.replay_records.empty();
	if(!document.board_category) {
		if(!no_records ||
			new_classic_online_identity_present(document.snapshot_id) ||
			document.fetched_unix_seconds != 0U) {
			return std::unexpected(error(NewClassicOnlineErrorCode::document_shape));
		}
	} else if(!valid_board_category(*document.board_category) ||
		!new_classic_online_identity_present(document.snapshot_id) ||
		document.fetched_unix_seconds == 0U) {
		return std::unexpected(error(NewClassicOnlineErrorCode::document_shape));
	}
	for(std::size_t index = 0U; index < document.replay_records.size(); ++index) {
		const auto &record = document.replay_records[index];
		if(!new_classic_online_identity_present(record.replay_id) ||
			!new_classic_online_identity_present(record.content_id) ||
			!new_classic_online_identity_present(record.ruleset_id)) {
			return std::unexpected(error(NewClassicOnlineErrorCode::identity, index));
		}
		for(std::size_t earlier = 0U; earlier < index; ++earlier) {
			if(document.replay_records[earlier].replay_id == record.replay_id) {
				return std::unexpected(error(
					NewClassicOnlineErrorCode::duplicate_identity, index
				));
			}
		}
		if(!document.board_category || !category_belongs_to_board(
			record.category, *document.board_category
		)) {
			return std::unexpected(error(NewClassicOnlineErrorCode::category, index));
		}
		if(!valid_result(record.category, record.result)) {
			return std::unexpected(error(NewClassicOnlineErrorCode::result, index));
		}
		if(record.availability > NewClassicOnlineReplayAvailability::downloaded ||
			record.payload_bytes == 0U ||
			record.payload_bytes > new_classic_online_replay_max_bytes ||
			record.autofire > 3U ||
			record.timed_frames == 0U ||
			record.slow_frames > record.timed_frames ||
			(static_cast<std::uint64_t>(record.slow_frames) * 100U) >
				(static_cast<std::uint64_t>(record.timed_frames) * 5U)) {
			return std::unexpected(error(
				NewClassicOnlineErrorCode::telemetry, index
			));
		}
		if(record.title.size() > new_classic_online_title_bytes ||
			(!record.title.empty() && !valid_utf8(record.title))) {
			return std::unexpected(error(NewClassicOnlineErrorCode::text, index));
		}
	}

	for(std::size_t index = 0U; index < document.leaderboard_entries.size(); ++index) {
		const auto &entry = document.leaderboard_entries[index];
		if(!new_classic_online_identity_present(entry.entry_id) ||
			!new_classic_online_identity_present(entry.content_id) ||
			!new_classic_online_identity_present(entry.ruleset_id)) {
			return std::unexpected(error(NewClassicOnlineErrorCode::identity, index));
		}
		for(std::size_t earlier = 0U; earlier < index; ++earlier) {
			if(document.leaderboard_entries[earlier].entry_id == entry.entry_id) {
				return std::unexpected(error(
					NewClassicOnlineErrorCode::duplicate_identity, index
				));
			}
		}
		if(!document.board_category || !category_belongs_to_board(
			entry.category, *document.board_category
		)) {
			return std::unexpected(error(NewClassicOnlineErrorCode::category, index));
		}
		if(!valid_result(entry.category, entry.result)) {
			return std::unexpected(error(NewClassicOnlineErrorCode::result, index));
		}
		if(entry.player.size() > new_classic_online_player_bytes ||
			(!entry.player.empty() && !valid_utf8(entry.player)) ||
			entry.evidence_source.size() > new_classic_online_evidence_bytes ||
			entry.curator_note.size() > new_classic_online_note_bytes ||
			(!entry.evidence_source.empty() && !valid_utf8(entry.evidence_source)) ||
			(!entry.curator_note.empty() && !valid_utf8(entry.curator_note))) {
			return std::unexpected(error(NewClassicOnlineErrorCode::text, index));
		}
		switch(entry.provenance) {
		case NewClassicOnlineProvenance::replay_verified:
			if(!entry.replay_id || !entry.verifier_id ||
				!new_classic_online_identity_present(*entry.replay_id) ||
				!new_classic_online_identity_present(*entry.verifier_id) ||
				!entry.evidence_source.empty() || !entry.curator_note.empty()) {
				return std::unexpected(error(
					NewClassicOnlineErrorCode::provenance, index
				));
			}
			break;
		case NewClassicOnlineProvenance::curated_legacy:
			if(entry.replay_id || entry.verifier_id) {
				return std::unexpected(error(
					NewClassicOnlineErrorCode::provenance, index
				));
			}
			break;
		default:
			return std::unexpected(error(
				NewClassicOnlineErrorCode::provenance, index
			));
		}
		if(entry.replay_id) {
			const auto found = std::ranges::find_if(
				document.replay_records,
				[&entry](const NewClassicOnlineReplayRecord &record) {
					return record.replay_id == *entry.replay_id;
				}
			);
			if(found != document.replay_records.end() &&
				(found->content_id != entry.content_id ||
				 found->ruleset_id != entry.ruleset_id ||
				 found->category != entry.category ||
				 found->result != entry.result)) {
				return std::unexpected(error(
					NewClassicOnlineErrorCode::provenance, index
				));
			}
		}
	}
	return {};
}

std::expected<std::vector<std::byte>, NewClassicOnlineError>
encode_new_classic_online_cache(const NewClassicOnlineCacheDocument &document)
{
	if(auto valid = validate_new_classic_online_cache(document); !valid) {
		return std::unexpected(valid.error());
	}
	std::vector<std::byte> output(new_classic_online_cache_header_bytes);
	std::ranges::copy(cache_magic, output.begin());
	set_u16(output, 8U, cache_version);
	set_u16(output, 10U,
		static_cast<std::uint16_t>(new_classic_online_cache_header_bytes));
	set_u32(output, 16U, document.generation);
	set_u16(output, 20U,
		static_cast<std::uint16_t>(document.leaderboard_entries.size()));
	set_u16(output, 22U,
		static_cast<std::uint16_t>(document.replay_records.size()));
	for(std::size_t byte = 0U; byte < 8U; ++byte) {
		output[24U + byte] = static_cast<std::byte>(
			document.fetched_unix_seconds >> (byte * 8U)
		);
	}
	for(std::size_t byte = 0U; byte < document.snapshot_id.bytes.size(); ++byte) {
		output[32U + byte] = static_cast<std::byte>(
			document.snapshot_id.bytes[byte]
		);
	}
	if(document.board_category) {
		output[64U] = static_cast<std::byte>(document.board_category->mode);
		output[65U] = static_cast<std::byte>(document.board_category->rank);
		output[66U] = static_cast<std::byte>(document.board_category->character);
		output[67U] = static_cast<std::byte>(document.board_category->start_stage);
		output[68U] = static_cast<std::byte>(document.board_category->start_round);
		output[69U] = std::byte{1U};
	}

	for(const auto &entry : document.leaderboard_entries) {
		const auto begin = output.size();
		append_u16(output, 0U);
		append_identity(output, entry.entry_id);
		append_identity(output, entry.content_id);
		append_identity(output, entry.ruleset_id);
		append_optional_identity(output, entry.replay_id);
		append_optional_identity(output, entry.verifier_id);
		append_category_result(output, entry.category, entry.result,
			static_cast<std::uint8_t>(entry.provenance),
			entry.autofire ? static_cast<std::uint8_t>(*entry.autofire ? 2U : 1U) : 0U);
		append_u16(output, entry.dos_date);
		append_u64(output, entry.published_unix_seconds);
		append_text(output, entry.player);
		append_text(output, entry.evidence_source);
		append_text(output, entry.curator_note);
		set_u16(output, begin, static_cast<std::uint16_t>(output.size() - begin));
	}

	for(const auto &record : document.replay_records) {
		const auto begin = output.size();
		append_u16(output, 0U);
		append_identity(output, record.replay_id);
		append_identity(output, record.content_id);
		append_identity(output, record.ruleset_id);
		append_category_result(output, record.category, record.result,
			static_cast<std::uint8_t>(record.availability));
		append_u16(output, record.dos_date);
		append_u32(output, record.payload_bytes);
		append_u8(output, record.autofire);
		append_u8(output, 0U);
		append_u8(output, 0U);
		append_u8(output, 0U);
		append_u32(output, record.timed_frames);
		append_u32(output, record.slow_frames);
		append_text(output, record.title);
		set_u16(output, begin, static_cast<std::uint16_t>(output.size() - begin));
	}

	if(output.size() + new_classic_online_cache_footer_bytes >
		new_classic_online_cache_max_bytes) {
		return std::unexpected(error(NewClassicOnlineErrorCode::input_size));
	}
	set_u32(output, 12U, static_cast<std::uint32_t>(
		output.size() + new_classic_online_cache_footer_bytes
	));
	append_u32(output, fnv1a32(output));
	return output;
}

std::expected<NewClassicOnlineCacheDocument, NewClassicOnlineError>
decode_new_classic_online_cache(std::span<const std::byte> bytes)
{
	if(bytes.size() < new_classic_online_cache_header_bytes +
		new_classic_online_cache_footer_bytes ||
		bytes.size() > new_classic_online_cache_max_bytes) {
		return std::unexpected(error(NewClassicOnlineErrorCode::input_size));
	}
	if(!std::ranges::equal(bytes.first(cache_magic.size()), cache_magic)) {
		return std::unexpected(error(NewClassicOnlineErrorCode::magic));
	}
	Reader reader(bytes);
	if(!reader.seek(8U)) {
		return std::unexpected(error(NewClassicOnlineErrorCode::header_shape));
	}
	const auto version = reader.u16();
	const auto header = reader.u16();
	const auto total = reader.u32();
	const auto generation = reader.u32();
	const auto entry_count = reader.u16();
	const auto replay_count = reader.u16();
	const auto fetched = reader.u64();
	const auto snapshot = reader.identity<NewClassicOnlineSnapshotTag>();
	const auto board_mode = reader.u8();
	const auto board_rank = reader.u8();
	const auto board_character = reader.u8();
	const auto board_stage = reader.u8();
	const auto board_round = reader.u8();
	const auto board_present = reader.u8();
	std::array<std::optional<std::uint8_t>, 10U> reserved{};
	for(auto &byte : reserved) {
		byte = reader.u8();
	}
	if(!version || *version != cache_version) {
		return std::unexpected(error(NewClassicOnlineErrorCode::version, 0U, 8U));
	}
	if(!header || *header != new_classic_online_cache_header_bytes || !total ||
		*total != bytes.size() || !generation || !entry_count || !replay_count ||
		!fetched || !snapshot || !board_mode || !board_rank || !board_character ||
		!board_stage || !board_round || !board_present || *board_present > 1U ||
		std::ranges::any_of(reserved, [](const auto &byte) {
			return !byte || *byte != 0U;
		}) || *entry_count > new_classic_online_entry_limit ||
		*replay_count > new_classic_online_replay_limit) {
		return std::unexpected(error(NewClassicOnlineErrorCode::header_shape));
	}
	const auto stored_checksum_offset = bytes.size() -
		new_classic_online_cache_footer_bytes;
	Reader footer(bytes.subspan(stored_checksum_offset));
	const auto stored_checksum = footer.u32();
	if(!stored_checksum || *stored_checksum !=
		fnv1a32(bytes.first(stored_checksum_offset))) {
		return std::unexpected(error(NewClassicOnlineErrorCode::checksum));
	}
	NewClassicOnlineCacheDocument document{};
	document.generation = *generation;
	document.snapshot_id = *snapshot;
	document.fetched_unix_seconds = *fetched;
	if(*board_present != 0U) {
		document.board_category = NewClassicOnlineCategory{
			static_cast<NewClassicOnlineMode>(*board_mode),
			*board_rank,
			*board_character,
			*board_stage,
			*board_round,
		};
	} else if(*board_mode != 0U || *board_rank != 0U ||
		*board_character != 0U || *board_stage != 0U || *board_round != 0U) {
		return std::unexpected(error(NewClassicOnlineErrorCode::header_shape));
	}
	document.leaderboard_entries.reserve(*entry_count);
	document.replay_records.reserve(*replay_count);
	if(!reader.seek(new_classic_online_cache_header_bytes)) {
		return std::unexpected(error(NewClassicOnlineErrorCode::header_shape));
	}

	for(std::size_t index = 0U; index < *entry_count; ++index) {
		const auto record_start = reader.offset();
		const auto record_size = reader.u16();
		const auto entry_id = reader.identity<NewClassicOnlineEntryTag>();
		const auto content_id = reader.identity<NewClassicOnlineContentTag>();
		const auto ruleset_id = reader.identity<NewClassicOnlineRulesetTag>();
		const auto replay_id = reader.identity<NewClassicOnlineReplayTag>();
		const auto verifier_id = reader.identity<NewClassicOnlineVerifierTag>();
		const auto category = read_category(reader);
		const auto result_policy = read_result_policy(reader);
		const auto date = reader.u16();
		const auto published = reader.u64();
		const auto player = reader.text();
		const auto evidence = reader.text();
		const auto note = reader.text();
		if(!record_size || *record_size < 2U ||
			record_start + *record_size > stored_checksum_offset ||
			!entry_id || !content_id || !ruleset_id || !replay_id ||
			!verifier_id || !category || !result_policy || !date || !published ||
			!player || !evidence || !note ||
			reader.offset() != record_start + *record_size) {
			return std::unexpected(error(
				NewClassicOnlineErrorCode::record_shape, index, record_start
			));
		}
		document.leaderboard_entries.push_back(NewClassicLeaderboardEntry{
			*entry_id,
			*content_id,
			*ruleset_id,
			optional_identity(*replay_id),
			optional_identity(*verifier_id),
			*category,
			result_policy->result,
			static_cast<NewClassicOnlineProvenance>(result_policy->policy),
			*date,
			*published,
			std::move(*player),
			std::move(*evidence),
			std::move(*note),
			result_policy->displayed_autofire == 0U ? std::nullopt :
				std::optional<bool>{result_policy->displayed_autofire == 2U},
		});
	}

	for(std::size_t index = 0U; index < *replay_count; ++index) {
		const auto record_start = reader.offset();
		const auto record_size = reader.u16();
		const auto replay_id = reader.identity<NewClassicOnlineReplayTag>();
		const auto content_id = reader.identity<NewClassicOnlineContentTag>();
		const auto ruleset_id = reader.identity<NewClassicOnlineRulesetTag>();
		const auto category = read_category(reader);
		const auto result_policy = read_result_policy(reader);
		const auto date = reader.u16();
		const auto payload_bytes = reader.u32();
		const auto autofire = reader.u8();
		const auto reserved_0 = reader.u8();
		const auto reserved_1 = reader.u8();
		const auto reserved_2 = reader.u8();
		const auto timed_frames = reader.u32();
		const auto slow_frames = reader.u32();
		const auto title = reader.text();
		if(!record_size || *record_size < 2U ||
			record_start + *record_size > stored_checksum_offset ||
			!replay_id || !content_id || !ruleset_id || !category ||
			!result_policy || !date || !payload_bytes || !autofire ||
			!reserved_0 || *reserved_0 != 0U || !reserved_1 ||
			*reserved_1 != 0U || !reserved_2 || *reserved_2 != 0U ||
			!timed_frames || !slow_frames || !title ||
			reader.offset() != record_start + *record_size) {
			return std::unexpected(error(
				NewClassicOnlineErrorCode::record_shape, index, record_start
			));
		}
		document.replay_records.push_back(NewClassicOnlineReplayRecord{
			*replay_id,
			*content_id,
			*ruleset_id,
			*category,
			result_policy->result,
			static_cast<NewClassicOnlineReplayAvailability>(result_policy->policy),
			*date,
			*payload_bytes,
			*autofire,
			*timed_frames,
			*slow_frames,
			std::move(*title),
		});
	}
	if(reader.offset() != stored_checksum_offset) {
		return std::unexpected(error(
			NewClassicOnlineErrorCode::trailing_data, 0U, reader.offset()
		));
	}
	if(auto valid = validate_new_classic_online_cache(document); !valid) {
		return std::unexpected(valid.error());
	}
	return document;
}

std::string new_classic_online_replay_filename(
	const NewClassicOnlineReplayId &replay_id
)
{
	constexpr std::array digits{
		'0', '1', '2', '3', '4', '5', '6', '7',
		'8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
	};
	std::string result;
	result.reserve((new_classic_online_identity_bytes * 2U) + 4U);
	for(const auto byte : replay_id.bytes) {
		result.push_back(digits[byte >> 4U]);
		result.push_back(digits[byte & 0x0FU]);
	}
	result += ".RPY";
	return result;
}

} // namespace th03::app
