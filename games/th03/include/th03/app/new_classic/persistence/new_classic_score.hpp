#ifndef TH03_APP_NEW_CLASSIC_SCORE_HPP
#define TH03_APP_NEW_CLASSIC_SCORE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

#include "th03/app/classic/persistence/classic_score.hpp"

namespace th03::app {

inline constexpr std::size_t new_classic_score_rank_count = 4U;
inline constexpr std::size_t new_classic_score_character_count = 9U;
inline constexpr std::size_t new_classic_score_locked_character_count = 7U;
inline constexpr std::size_t new_classic_score_place_count = 10U;
inline constexpr std::size_t new_classic_score_name_length = 8U;
inline constexpr std::size_t new_classic_score_digit_count = 8U;
inline constexpr std::size_t new_classic_score_entry_bytes = 20U;
inline constexpr std::size_t new_classic_score_board_bytes = 216U;
inline constexpr std::size_t new_classic_score_header_bytes = 36U;
inline constexpr std::size_t new_classic_score_file_bytes = 7812U;
inline constexpr std::uint8_t new_classic_score_all_characters = 0xFFU;
inline constexpr std::uint8_t new_classic_score_empty_stage = 0xFFU;

struct NewClassicScoreEntry final {
	std::array<char, new_classic_score_name_length> name{};
	// Source score digits are least-significant first.
	std::array<std::uint8_t, new_classic_score_digit_count> score{};
	std::uint8_t stage = new_classic_score_empty_stage;
	std::uint8_t continues_used = 0U;
	bool valid = false;

	friend constexpr bool operator==(
		const NewClassicScoreEntry &,
		const NewClassicScoreEntry &
	) = default;
};

struct NewClassicScoreStats final {
	std::uint32_t play_frames = 0U;
	std::uint32_t one_ccs = 0U;
	std::uint32_t continues_used = 0U;

	friend constexpr bool operator==(
		const NewClassicScoreStats &,
		const NewClassicScoreStats &
	) = default;
};

struct NewClassicScoreBoard final {
	std::array<NewClassicScoreEntry, new_classic_score_place_count> entries{};
	NewClassicScoreStats stats{};

	friend constexpr bool operator==(
		const NewClassicScoreBoard &,
		const NewClassicScoreBoard &
	) = default;
};

struct NewClassicScoreDocument final {
	std::uint32_t generation = 0U;
	std::uint32_t last_run_id = 0U;
	bool extra_unlocked = false;
	std::array<
		std::array<NewClassicScoreBoard, new_classic_score_character_count>,
		new_classic_score_rank_count
	> boards{};

	friend constexpr bool operator==(
		const NewClassicScoreDocument &,
		const NewClassicScoreDocument &
	) = default;
};

struct NewClassicScoreRun final {
	std::uint32_t run_id = 0U;
	std::uint32_t play_frames = 0U;
	std::uint32_t continues_used = 0U;
	bool one_cc = false;
};

enum class NewClassicScoreBoardPolicy : std::uint8_t {
	reject,
	replace_invalid,
};

enum class NewClassicScoreErrorCode : std::uint8_t {
	input_size,
	header_shape,
	header_flags,
	header_checksum,
	board_checksum,
	invalid_entry_flags,
	invalid_entry_order,
	invalid_name,
	invalid_score_digit,
	invalid_stage,
	invalid_continues,
	invalid_rank,
	invalid_character,
	invalid_page,
	invalid_candidate,
	invalid_place,
	invalid_name_glyph,
	invalid_run,
};

struct NewClassicScoreError final {
	NewClassicScoreErrorCode code = NewClassicScoreErrorCode::input_size;
	std::size_t rank = new_classic_score_rank_count;
	std::size_t character = new_classic_score_character_count;
	std::size_t place = new_classic_score_place_count;
	std::size_t cell = new_classic_score_digit_count;
	std::size_t offset = new_classic_score_file_bytes;
};

struct NewClassicScoreDecodeResult final {
	NewClassicScoreDocument document{};
	std::array<
		std::array<bool, new_classic_score_character_count>,
		new_classic_score_rank_count
	> repaired_boards{};
};

struct NewClassicScoreView final {
	ClassicScoreBoard board{};
	NewClassicScoreStats stats{};
	std::array<bool, new_classic_score_place_count> valid{};
	std::array<bool, new_classic_score_place_count> total{};
	std::uint8_t page = new_classic_score_all_characters;
};

[[nodiscard]] NewClassicScoreDocument new_classic_score_empty() noexcept;

[[nodiscard]] std::expected<
	NewClassicScoreDecodeResult,
	NewClassicScoreError
> decode_new_classic_score_file(
	std::span<const std::byte> bytes,
	NewClassicScoreBoardPolicy policy = NewClassicScoreBoardPolicy::reject
) noexcept;

[[nodiscard]] std::expected<
	std::array<std::byte, new_classic_score_file_bytes>,
	NewClassicScoreError
> encode_new_classic_score_file(
	const NewClassicScoreDocument &document
) noexcept;

[[nodiscard]] std::expected<
	NewClassicScoreDocument,
	NewClassicScoreError
> migrate_classic_score_document(
	const ClassicScoreDocument &legacy
) noexcept;

[[nodiscard]] std::expected<std::optional<std::size_t>, NewClassicScoreError>
new_classic_score_insert(
	NewClassicScoreDocument &document,
	std::uint8_t rank,
	std::uint8_t character,
	const ClassicScoreCandidate &candidate
) noexcept;

[[nodiscard]] std::expected<void, NewClassicScoreError>
new_classic_score_set_name(
	NewClassicScoreDocument &document,
	std::uint8_t rank,
	std::uint8_t character,
	std::size_t place,
	std::span<const ClassicRegistrationGlyph, classic_score_name_length>
		name_forward
) noexcept;

// Accounts a completed live Story run exactly once. Stale/already committed
// run IDs are harmless no-ops, matching the Replay Patch journal recovery
// guard without exposing its multi-process DOS transport to the native host.
[[nodiscard]] std::expected<bool, NewClassicScoreError>
new_classic_score_apply_run(
	NewClassicScoreDocument &document,
	std::uint8_t rank,
	std::uint8_t character,
	NewClassicScoreRun run
) noexcept;

[[nodiscard]] std::expected<NewClassicScoreView, NewClassicScoreError>
new_classic_score_view(
	const NewClassicScoreDocument &document,
	std::uint8_t rank,
	std::uint8_t page
) noexcept;

[[nodiscard]] constexpr std::uint32_t new_classic_score_add_saturating(
	std::uint32_t lhs,
	std::uint32_t rhs
) noexcept
{
	return (static_cast<std::uint64_t>(lhs) + rhs > 0xFFFFFFFFULL) ?
		0xFFFFFFFFU : static_cast<std::uint32_t>(lhs + rhs);
}

} // namespace th03::app

#endif
