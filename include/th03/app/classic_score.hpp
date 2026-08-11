#ifndef TH03_APP_CLASSIC_SCORE_HPP
#define TH03_APP_CLASSIC_SCORE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

namespace th03::app {

inline constexpr std::size_t classic_score_rank_count = 4U;
inline constexpr std::size_t classic_score_place_count = 10U;
inline constexpr std::size_t classic_score_name_length = 8U;
inline constexpr std::size_t classic_score_digit_count = 8U;
inline constexpr std::size_t classic_score_cell_count = 10U;
inline constexpr std::size_t classic_score_section_bytes = 206U;
inline constexpr std::size_t classic_score_file_bytes =
	classic_score_rank_count * classic_score_section_bytes;

// Exact pattern numbers registered from REGI2.BFT followed by REGI1.BFT.
// Names remain stored backwards in ClassicScoreRow to keep the source format
// and the later registration presenter mechanically auditable.
enum class ClassicRegistrationGlyph : std::uint8_t {
	a,
	b,
	c,
	d,
	e,
	f,
	g,
	h,
	i,
	j,
	k,
	l,
	m,
	blank_1,
	space,
	space_last,
	n,
	o,
	p,
	q,
	r,
	s,
	t,
	u,
	v,
	w,
	x,
	y,
	z,
	blank_2,
	backspace,
	backspace_last,
	digit_0,
	digit_1,
	digit_2,
	digit_3,
	digit_4,
	digit_5,
	digit_6,
	digit_7,
	digit_8,
	digit_9,
	period,
	comma,
	exclamation,
	question,
	end,
	end_last,
	all,
	count,
};

enum class ClassicScoreCleared : std::uint8_t {
	not_cleared = 18U,
	cleared = 99U,
};

struct ClassicScoreRow final {
	std::array<
		ClassicRegistrationGlyph,
		classic_score_name_length
	> name_backwards{};
	// Cell 0 is continues used, 1..8 are the eight source score digits,
	// and cell 9 is source-dead storage that is nevertheless preserved.
	std::array<
		ClassicRegistrationGlyph,
		classic_score_cell_count
	> score{};
	// 0 means no entry; 1..9 are Reimu through Yumemi.
	std::uint8_t playchar_optional = 0U;
	ClassicRegistrationGlyph stage = ClassicRegistrationGlyph::digit_1;

	friend constexpr bool operator==(
		const ClassicScoreRow &,
		const ClassicScoreRow &
	) = default;
};

struct ClassicScoreBoard final {
	std::array<ClassicScoreRow, classic_score_place_count> rows{};
	ClassicScoreCleared cleared = ClassicScoreCleared::not_cleared;

	friend constexpr bool operator==(
		const ClassicScoreBoard &,
		const ClassicScoreBoard &
	) = default;
};

// These three bytes are deliberately explicit. The source draws them from
// gameplay RNG on every save, but the native codec must be deterministic.
struct ClassicScoreCodecFields final {
	std::uint8_t unknown = 0U;
	std::uint8_t key1 = 0U;
	std::uint8_t key2 = 0U;

	friend constexpr bool operator==(
		const ClassicScoreCodecFields &,
		const ClassicScoreCodecFields &
	) = default;
};

struct ClassicScoreDocument final {
	std::array<ClassicScoreBoard, classic_score_rank_count> boards{};
	std::array<ClassicScoreCodecFields, classic_score_rank_count> codec{};

	friend constexpr bool operator==(
		const ClassicScoreDocument &,
		const ClassicScoreDocument &
	) = default;
};

struct ClassicScoreCandidate final {
	std::array<std::uint8_t, classic_score_digit_count> digits{};
	std::uint8_t continues_used = 0U;
	// 1..9, or 99 for the source's STAGE_ALL value.
	std::uint8_t stage = 1U;
	// Base character ID, Reimu=0 through Yumemi=8.
	std::uint8_t playchar = 0U;

	friend constexpr bool operator==(
		const ClassicScoreCandidate &,
		const ClassicScoreCandidate &
	) = default;
};

enum class ClassicScoreErrorCode : std::uint8_t {
	input_size,
	checksum_mismatch,
	invalid_cleared,
	invalid_name_glyph,
	invalid_score_glyph,
	invalid_playchar,
	invalid_stage,
	invalid_candidate,
	invalid_place,
	invalid_default_name,
};

struct ClassicScoreError final {
	ClassicScoreErrorCode code = ClassicScoreErrorCode::input_size;
	// Rank 4, place 10, or cell 10 respectively mean "not applicable".
	std::size_t rank = classic_score_rank_count;
	std::size_t place = classic_score_place_count;
	std::size_t cell = classic_score_cell_count;
	// Absolute byte offset for byte-format errors; 824 otherwise.
	std::size_t offset = classic_score_file_bytes;
};

[[nodiscard]] constexpr bool classic_registration_glyph_valid(
	ClassicRegistrationGlyph glyph
) noexcept
{
	return static_cast<std::uint8_t>(glyph) <
		static_cast<std::uint8_t>(ClassicRegistrationGlyph::count);
}

[[nodiscard]] constexpr std::optional<ClassicRegistrationGlyph>
classic_registration_digit(std::uint8_t digit) noexcept
{
	if(digit > 9U) {
		return std::nullopt;
	}
	return static_cast<ClassicRegistrationGlyph>(
		static_cast<std::uint8_t>(ClassicRegistrationGlyph::digit_0) + digit
	);
}

// Decodes all four independently encrypted sections. Failure publishes no
// partially decoded document. Domain checks happen only after the source
// checksum has been verified for that section.
[[nodiscard]] std::expected<ClassicScoreDocument, ClassicScoreError>
decode_classic_score_file(std::span<const std::byte> bytes) noexcept;

// Validates every logical field, computes the source checksum over bytes
// 2..205, then encrypts bytes 0..203 using the explicit per-rank codec fields.
[[nodiscard]] std::expected<
	std::array<std::byte, classic_score_file_bytes>,
	ClassicScoreError
> encode_classic_score_file(const ClassicScoreDocument &document) noexcept;

// Reproduces scoredat_recreate(): all four boards contain dotted names,
// 10000 then 9000..1000, No Entry, Stage 1, and no clear flag.
[[nodiscard]] ClassicScoreDocument classic_score_stock_file(
	std::array<ClassicScoreCodecFields, classic_score_rank_count> codec = {}
) noexcept;

// Preserves ZUN's source comparator: continues are ignored and exact ties
// sink beneath all existing equal rows. Returns nullopt when tenth place is
// not beaten. Invalid input is rejected before the board can be modified.
[[nodiscard]] std::expected<std::optional<std::size_t>, ClassicScoreError>
classic_score_insert(
	ClassicScoreBoard &board,
	const ClassicScoreCandidate &candidate
) noexcept;

// The source replaces any uniform eight-glyph name (including all spaces)
// with the character default. The supplied default is forward-order and is
// reversed into the stored row. Returns true if replacement occurred.
[[nodiscard]] std::expected<bool, ClassicScoreError>
classic_score_replace_uniform_name(
	ClassicScoreBoard &board,
	std::size_t place,
	std::span<const ClassicRegistrationGlyph, classic_score_name_length>
		default_name_forward
) noexcept;

} // namespace th03::app

#endif
