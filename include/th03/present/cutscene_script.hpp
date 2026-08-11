#ifndef TH03_PRESENT_CUTSCENE_SCRIPT_HPP
#define TH03_PRESENT_CUTSCENE_SCRIPT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <variant>
#include <vector>

#include "th03/present/graphics_text.hpp"

namespace th03::present {

inline constexpr std::size_t cutscene_filename_capacity = 13U;
inline constexpr std::size_t cutscene_batch_capacity = 4U;

struct CutsceneCursor {
	std::int32_t x = 80;
	std::int32_t y = 320;

	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneCursor &,
		const CutsceneCursor &
	) = default;
};

struct CutsceneScriptInput {
	bool cancel_held = false;
	bool any_key_held = false;
};

enum class CutsceneWaitKind : std::uint8_t {
	frames,
	input_change,
	input_ok,
	measure,
	input_ok_or_measure,
};

enum class CutsceneFadeColor : std::uint8_t {
	white,
	black,
};

enum class CutsceneFadeDirection : std::uint8_t {
	in,
	out,
};

enum class CutsceneBgmAction : std::uint8_t {
	stop,
	play,
	load_and_play,
};

struct CutsceneFilename {
	std::array<char, cutscene_filename_capacity> bytes{};
	std::uint8_t length = 0U;

	[[nodiscard]] std::span<const char> view() const noexcept {
		return {bytes.data(), length};
	}

	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneFilename &,
		const CutsceneFilename &
	) = default;
};

struct CutsceneDrawGlyph {
	CutsceneCursor position{};
	std::array<std::uint8_t, 2U> shift_jis{};
	std::uint16_t color = 15U;
	GraphicsTextWeight weight = GraphicsTextWeight::bold;

	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneDrawGlyph &,
		const CutsceneDrawGlyph &
	) = default;
};

struct CutsceneDrawGaiji {
	CutsceneCursor position{};
	std::uint16_t code = 0U;
	std::uint16_t color = 15U;

	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneDrawGaiji &,
		const CutsceneDrawGaiji &
	) = default;
};

struct CutsceneWait {
	CutsceneWaitKind kind = CutsceneWaitKind::frames;
	std::uint16_t first = 0U;
	std::uint16_t second = 0U;

	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneWait &,
		const CutsceneWait &
	) = default;
};

struct CutsceneRestoreBox {
	[[nodiscard]] friend constexpr bool operator==(
		CutsceneRestoreBox,
		CutsceneRestoreBox
	) = default;
};

struct CutsceneSnapshotBox {
	[[nodiscard]] friend constexpr bool operator==(
		CutsceneSnapshotBox,
		CutsceneSnapshotBox
	) = default;
};

struct CutsceneReleaseBox {
	[[nodiscard]] friend constexpr bool operator==(
		CutsceneReleaseBox,
		CutsceneReleaseBox
	) = default;
};

struct CutsceneSetTextColor {
	std::uint16_t color = 15U;
	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneSetTextColor &,
		const CutsceneSetTextColor &
	) = default;
};

struct CutsceneSetTextWeight {
	GraphicsTextWeight weight = GraphicsTextWeight::bold;
	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneSetTextWeight &,
		const CutsceneSetTextWeight &
	) = default;
};

struct CutsceneSetTextInterval {
	std::uint16_t frames = 1U;
	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneSetTextInterval &,
		const CutsceneSetTextInterval &
	) = default;
};

struct CutsceneShowPage {
	std::uint16_t page = 0U;
	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneShowPage &,
		const CutsceneShowPage &
	) = default;
};

struct CutsceneSetTone {
	std::uint16_t tone = 100U;
	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneSetTone &,
		const CutsceneSetTone &
	) = default;
};

struct CutscenePaletteFade {
	CutsceneFadeColor color = CutsceneFadeColor::white;
	CutsceneFadeDirection direction = CutsceneFadeDirection::in;
	std::uint16_t speed = 1U;

	[[nodiscard]] friend constexpr bool operator==(
		const CutscenePaletteFade &,
		const CutscenePaletteFade &
	) = default;
};

struct CutsceneAudioFade {
	// Intentionally not narrowed to eight bits. The donor passes the complete
	// parsed value into the packed PMD interrupt command.
	std::uint16_t parameter = 1U;
	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneAudioFade &,
		const CutsceneAudioFade &
	) = default;
};

struct CutsceneShake {
	std::uint16_t parameter = 8U;
	bool delay_each_position = true;

	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneShake &,
		const CutsceneShake &
	) = default;
};

struct CutsceneClearPages {
	// TH03 deliberately does not refresh the saved box background here.
	bool snapshot_box_after_clear = false;
	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneClearPages &,
		const CutsceneClearPages &
	) = default;
};

struct CutsceneLoadBackground {
	CutsceneFilename filename{};
	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneLoadBackground &,
		const CutsceneLoadBackground &
	) = default;
};

struct CutscenePresentBackground {
	bool apply_palette = true;
	// Presentation also copies the result to the other page and refreshes the
	// saved text-box background.
	[[nodiscard]] friend constexpr bool operator==(
		const CutscenePresentBackground &,
		const CutscenePresentBackground &
	) = default;
};

struct CutsceneFreeBackground {
	[[nodiscard]] friend constexpr bool operator==(
		CutsceneFreeBackground,
		CutsceneFreeBackground
	) = default;
};

struct CutsceneApplyBackgroundPalette {
	[[nodiscard]] friend constexpr bool operator==(
		CutsceneApplyBackgroundPalette,
		CutsceneApplyBackgroundPalette
	) = default;
};

struct CutsceneShowPictureQuarter {
	std::uint16_t quarter = 0U;
	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneShowPictureQuarter &,
		const CutsceneShowPictureQuarter &
	) = default;
};

struct CutsceneClearPicture {
	[[nodiscard]] friend constexpr bool operator==(
		CutsceneClearPicture,
		CutsceneClearPicture
	) = default;
};

struct CutsceneCrossfadePicture {
	std::uint16_t quarter = 0U;
	std::uint16_t frames_per_mask = 1U;
	bool animate_masks = true;

	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneCrossfadePicture &,
		const CutsceneCrossfadePicture &
	) = default;
};

struct CutsceneBgm {
	CutsceneBgmAction action = CutsceneBgmAction::stop;
	CutsceneFilename filename{};

	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneBgm &,
		const CutsceneBgm &
	) = default;
};

struct CutscenePlaySoundEffect {
	std::uint16_t id = 0U;
	[[nodiscard]] friend constexpr bool operator==(
		const CutscenePlaySoundEffect &,
		const CutscenePlaySoundEffect &
	) = default;
};

struct CutsceneFinish {
	[[nodiscard]] friend constexpr bool operator==(
		CutsceneFinish,
		CutsceneFinish
	) = default;
};

using CutsceneOperation = std::variant<
	CutsceneDrawGlyph,
	CutsceneDrawGaiji,
	CutsceneWait,
	CutsceneRestoreBox,
	CutsceneSnapshotBox,
	CutsceneReleaseBox,
	CutsceneSetTextColor,
	CutsceneSetTextWeight,
	CutsceneSetTextInterval,
	CutsceneShowPage,
	CutsceneSetTone,
	CutscenePaletteFade,
	CutsceneAudioFade,
	CutsceneShake,
	CutsceneClearPages,
	CutsceneLoadBackground,
	CutscenePresentBackground,
	CutsceneFreeBackground,
	CutsceneApplyBackgroundPalette,
	CutsceneShowPictureQuarter,
	CutsceneClearPicture,
	CutsceneCrossfadePicture,
	CutsceneBgm,
	CutscenePlaySoundEffect,
	CutsceneFinish
>;

struct CutsceneScriptBatch {
	std::array<CutsceneOperation, cutscene_batch_capacity> storage{};
	std::uint8_t count = 0U;

	[[nodiscard]] std::span<const CutsceneOperation> operations() const noexcept {
		return {storage.data(), count};
	}
};

enum class CutsceneScriptErrorCode : std::uint8_t {
	empty_script,
	unexpected_end,
	not_started,
	already_started,
	already_finished,
	operation_overflow,
};

struct CutsceneScriptError {
	CutsceneScriptErrorCode code = CutsceneScriptErrorCode::empty_script;
	std::size_t byte_offset = 0U;

	[[nodiscard]] friend constexpr bool operator==(
		const CutsceneScriptError &,
		const CutsceneScriptError &
	) = default;
};

// Source-exact TH03 plaintext cutscene interpreter. The VM owns its bytes and
// performs no I/O. A caller must execute each returned semantic batch in order
// before requesting the next top-level source iteration.
class CutsceneScriptVm final {
public:
	[[nodiscard]] static std::expected<CutsceneScriptVm, CutsceneScriptError>
	create(std::span<const std::uint8_t> script);

	CutsceneScriptVm(CutsceneScriptVm &&) noexcept = default;
	CutsceneScriptVm &operator=(CutsceneScriptVm &&) noexcept = default;
	CutsceneScriptVm(const CutsceneScriptVm &) = delete;
	CutsceneScriptVm &operator=(const CutsceneScriptVm &) = delete;

	// Emits the source prologue: show page 0 and snapshot the initial text-box
	// background. No input poll belongs to this batch.
	[[nodiscard]] std::expected<CutsceneScriptBatch, CutsceneScriptError>
	begin() noexcept;

	// Consumes exactly one source interpreter iteration and therefore exactly
	// one caller-owned input poll. A malformed iteration is atomic.
	[[nodiscard]] std::expected<CutsceneScriptBatch, CutsceneScriptError>
	step(CutsceneScriptInput input) noexcept;

	[[nodiscard]] std::size_t byte_offset() const noexcept;
	[[nodiscard]] CutsceneCursor cursor() const noexcept;
	[[nodiscard]] std::uint16_t text_interval() const noexcept;
	[[nodiscard]] std::uint16_t text_color() const noexcept;
	[[nodiscard]] GraphicsTextWeight text_weight() const noexcept;
	[[nodiscard]] std::uint16_t number_default() const noexcept;
	[[nodiscard]] std::uint8_t speedup_cycle() const noexcept;
	[[nodiscard]] bool fast_forward() const noexcept;
	[[nodiscard]] bool started() const noexcept;
	[[nodiscard]] bool finished() const noexcept;

private:
	explicit CutsceneScriptVm(std::vector<std::uint8_t> script) noexcept;

	struct IterationState {
		std::size_t byte_offset = 0U;
		CutsceneCursor cursor{};
		std::uint16_t text_interval = 1U;
		std::uint16_t text_color = 15U;
		GraphicsTextWeight text_weight = GraphicsTextWeight::bold;
		std::uint16_t number_default = 0U;
		std::uint8_t speedup_cycle = 0U;
		bool fast_forward = false;
		bool finished = false;
	};

	[[nodiscard]] IterationState iteration_state() const noexcept;
	void restore_iteration_state(IterationState state) noexcept;

	[[nodiscard]] std::expected<CutsceneScriptBatch, CutsceneScriptError>
	step_impl(CutsceneScriptInput input) noexcept;
	[[nodiscard]] std::expected<void, CutsceneScriptError> execute_opcode(
		std::uint8_t opcode,
		CutsceneScriptBatch &batch
	) noexcept;
	[[nodiscard]] std::expected<void, CutsceneScriptError> advance_cursor(
		CutsceneScriptBatch &batch
	) noexcept;
	[[nodiscard]] std::expected<void, CutsceneScriptError> append_typing_delay(
		CutsceneScriptInput input,
		CutsceneScriptBatch &batch
	) noexcept;
	[[nodiscard]] std::expected<void, CutsceneScriptError> append(
		CutsceneScriptBatch &batch,
		CutsceneOperation operation
	) const noexcept;
	[[nodiscard]] std::expected<std::uint8_t, CutsceneScriptError>
	read_byte() noexcept;
	[[nodiscard]] std::expected<std::uint8_t, CutsceneScriptError>
	peek_byte() const noexcept;
	[[nodiscard]] std::expected<std::uint16_t, CutsceneScriptError>
	read_number_first() noexcept;
	[[nodiscard]] std::expected<std::uint16_t, CutsceneScriptError>
	read_number_first(std::uint16_t default_value) noexcept;
	[[nodiscard]] std::expected<std::uint16_t, CutsceneScriptError>
	read_number_second() noexcept;
	[[nodiscard]] std::expected<CutsceneFilename, CutsceneScriptError>
	read_filename() noexcept;

	std::vector<std::uint8_t> script_{};
	std::size_t byte_offset_ = 0U;
	CutsceneCursor cursor_{};
	std::uint16_t text_interval_ = 1U;
	std::uint16_t text_color_ = 15U;
	GraphicsTextWeight text_weight_ = GraphicsTextWeight::bold;
	std::uint16_t number_default_ = 0U;
	std::uint8_t speedup_cycle_ = 0U;
	bool fast_forward_ = false;
	bool started_ = false;
	bool finished_ = false;
};

} // namespace th03::present

#endif
