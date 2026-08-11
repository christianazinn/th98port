#include "th03/present/cutscene_script.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>
#include <vector>

namespace th03::present {
namespace {

constexpr std::int32_t box_left = 80;
constexpr std::int32_t box_top = 320;
constexpr std::int32_t box_right = 560;
constexpr std::int32_t box_bottom = 384;
constexpr std::int32_t glyph_width = 16;
constexpr std::int32_t glyph_height = 16;
constexpr std::int32_t wrapped_line_left = 144;
constexpr std::uint16_t picture_quarter_count = 4U;

[[nodiscard]] constexpr bool is_digit(std::uint8_t value) noexcept
{
	return value >= static_cast<std::uint8_t>('0') &&
		value <= static_cast<std::uint8_t>('9');
}

[[nodiscard]] constexpr bool is_separator(std::uint8_t value) noexcept
{
	return value < 0x20U || value == static_cast<std::uint8_t>(' ');
}

[[nodiscard]] constexpr std::uint8_t ascii_lower(std::uint8_t value) noexcept
{
	if(value >= static_cast<std::uint8_t>('A') &&
		value <= static_cast<std::uint8_t>('Z')) {
		return static_cast<std::uint8_t>(
			value + static_cast<std::uint8_t>('a' - 'A')
		);
	}
	return value;
}

} // namespace

std::expected<CutsceneScriptVm, CutsceneScriptError>
CutsceneScriptVm::create(std::span<const std::uint8_t> script)
{
	if(script.empty()) {
		return std::unexpected(CutsceneScriptError{
			CutsceneScriptErrorCode::empty_script,
			0U,
		});
	}
	return CutsceneScriptVm(std::vector<std::uint8_t>(script.begin(), script.end()));
}

CutsceneScriptVm::CutsceneScriptVm(std::vector<std::uint8_t> script) noexcept
	: script_(std::move(script))
{
}

std::expected<CutsceneScriptBatch, CutsceneScriptError>
CutsceneScriptVm::begin() noexcept
{
	if(started_) {
		return std::unexpected(CutsceneScriptError{
			CutsceneScriptErrorCode::already_started,
			byte_offset_,
		});
	}

	CutsceneScriptBatch batch{};
	auto shown = append(batch, CutsceneShowPage{0U});
	if(!shown) {
		return std::unexpected(shown.error());
	}
	auto snapped = append(batch, CutsceneSnapshotBox{});
	if(!snapped) {
		return std::unexpected(snapped.error());
	}
	started_ = true;
	return batch;
}

std::expected<CutsceneScriptBatch, CutsceneScriptError>
CutsceneScriptVm::step(CutsceneScriptInput input) noexcept
{
	if(!started_) {
		return std::unexpected(CutsceneScriptError{
			CutsceneScriptErrorCode::not_started,
			byte_offset_,
		});
	}
	if(finished_) {
		return std::unexpected(CutsceneScriptError{
			CutsceneScriptErrorCode::already_finished,
			byte_offset_,
		});
	}

	const auto before = iteration_state();
	auto result = step_impl(input);
	if(!result) {
		restore_iteration_state(before);
	}
	return result;
}

std::expected<CutsceneScriptBatch, CutsceneScriptError>
CutsceneScriptVm::step_impl(CutsceneScriptInput input) noexcept
{
	fast_forward_ = input.cancel_held;
	CutsceneScriptBatch batch{};

	auto first = read_byte();
	if(!first) {
		return std::unexpected(first.error());
	}
	if(is_separator(*first)) {
		return batch;
	}

	if(*first == static_cast<std::uint8_t>('\\')) {
		auto opcode = read_byte();
		if(!opcode) {
			return std::unexpected(opcode.error());
		}
		auto executed = execute_opcode(*opcode, batch);
		if(!executed) {
			return std::unexpected(executed.error());
		}
		return batch;
	}

	auto second = read_byte();
	if(!second) {
		return std::unexpected(second.error());
	}
	auto drawn = append(batch, CutsceneDrawGlyph{
		cursor_,
		{*first, *second},
		text_color_,
		text_weight_,
	});
	if(!drawn) {
		return std::unexpected(drawn.error());
	}
	auto advanced = advance_cursor(batch);
	if(!advanced) {
		return std::unexpected(advanced.error());
	}
	auto delayed = append_typing_delay(input, batch);
	if(!delayed) {
		return std::unexpected(delayed.error());
	}
	return batch;
}

std::expected<void, CutsceneScriptError> CutsceneScriptVm::execute_opcode(
	std::uint8_t opcode,
	CutsceneScriptBatch &batch
) noexcept
{
	const auto lowered = ascii_lower(opcode);
	switch(lowered) {
	case static_cast<std::uint8_t>('n'):
		cursor_.y += glyph_height;
		cursor_.x = box_left;
		if(cursor_.y < box_bottom) {
			return {};
		}
		[[fallthrough]];

	case static_cast<std::uint8_t>('s'): {
		auto next = peek_byte();
		if(!next) {
			return std::unexpected(next.error());
		}
		if(*next != static_cast<std::uint8_t>('-')) {
			auto frames = read_number_first(0U);
			if(!frames) {
				return std::unexpected(frames.error());
			}
			if(!fast_forward_) {
				auto waited = append(batch, CutsceneWait{
					CutsceneWaitKind::input_change,
					*frames,
					0U,
				});
				if(!waited) {
					return waited;
				}
			}
		} else {
			++byte_offset_;
		}
		cursor_ = {box_left, box_top};
		return append(batch, CutsceneRestoreBox{});
	}

	case static_cast<std::uint8_t>('c'): {
		auto color = read_number_first(15U);
		if(!color) {
			return std::unexpected(color.error());
		}
		text_color_ = *color;
		return append(batch, CutsceneSetTextColor{*color});
	}

	case static_cast<std::uint8_t>('b'): {
		auto weight = read_number_first(2U);
		if(!weight) {
			return std::unexpected(weight.error());
		}
		if(*weight > 3U) {
			return {};
		}
		text_weight_ = static_cast<GraphicsTextWeight>(*weight);
		return append(batch, CutsceneSetTextWeight{text_weight_});
	}

	case static_cast<std::uint8_t>('w'): {
		auto next = peek_byte();
		if(!next) {
			return std::unexpected(next.error());
		}
		const auto sub = ascii_lower(*next);
		if(sub == static_cast<std::uint8_t>('i') ||
			sub == static_cast<std::uint8_t>('o')) {
			++byte_offset_;
			auto speed = read_number_first(1U);
			if(!speed) {
				return std::unexpected(speed.error());
			}
			return append(batch, CutscenePaletteFade{
				CutsceneFadeColor::white,
				(sub == static_cast<std::uint8_t>('i'))
					? CutsceneFadeDirection::in
					: CutsceneFadeDirection::out,
				*speed,
			});
		}

		number_default_ = 64U;
		if(sub != static_cast<std::uint8_t>('m')) {
			const bool wait_for_ok = sub == static_cast<std::uint8_t>('k');
			if(wait_for_ok) {
				++byte_offset_;
			}
			auto frames = read_number_first();
			if(!frames) {
				return std::unexpected(frames.error());
			}
			if(fast_forward_) {
				return {};
			}
			return append(batch, CutsceneWait{
				wait_for_ok ? CutsceneWaitKind::input_ok :
					CutsceneWaitKind::frames,
				*frames,
				0U,
			});
		}

		++byte_offset_;
		auto after_measure = peek_byte();
		if(!after_measure) {
			return std::unexpected(after_measure.error());
		}
		const bool wait_for_ok = *after_measure == static_cast<std::uint8_t>('k');
		if(wait_for_ok) {
			++byte_offset_;
		}
		auto measure = read_number_first();
		if(!measure) {
			return std::unexpected(measure.error());
		}
		auto fraction = read_number_second();
		if(!fraction) {
			return std::unexpected(fraction.error());
		}
		if(fast_forward_) {
			return {};
		}
		return append(batch, CutsceneWait{
			wait_for_ok ? CutsceneWaitKind::input_ok_or_measure :
				CutsceneWaitKind::measure,
			*measure,
			*fraction,
		});
	}

	case static_cast<std::uint8_t>('v'): {
		auto next = peek_byte();
		if(!next) {
			return std::unexpected(next.error());
		}
		if(*next != static_cast<std::uint8_t>('p')) {
			auto interval = read_number_first(1U);
			if(!interval) {
				return std::unexpected(interval.error());
			}
			text_interval_ = *interval;
			return append(batch, CutsceneSetTextInterval{*interval});
		}
		++byte_offset_;
		auto page = read_number_first(0U);
		if(!page) {
			return std::unexpected(page.error());
		}
		return append(batch, CutsceneShowPage{*page});
	}

	case static_cast<std::uint8_t>('t'): {
		auto tone = read_number_first(100U);
		if(!tone) {
			return std::unexpected(tone.error());
		}
		if(!fast_forward_) {
			auto delayed = append(batch, CutsceneWait{
				CutsceneWaitKind::frames,
				1U,
				0U,
			});
			if(!delayed) {
				return delayed;
			}
		}
		return append(batch, CutsceneSetTone{*tone});
	}

	case static_cast<std::uint8_t>('f'): {
		auto next = peek_byte();
		if(!next) {
			return std::unexpected(next.error());
		}
		if(*next == static_cast<std::uint8_t>('i') ||
			*next == static_cast<std::uint8_t>('o')) {
			++byte_offset_;
			auto speed = read_number_first(1U);
			if(!speed) {
				return std::unexpected(speed.error());
			}
			return append(batch, CutscenePaletteFade{
				CutsceneFadeColor::black,
				(*next == static_cast<std::uint8_t>('i'))
					? CutsceneFadeDirection::in
					: CutsceneFadeDirection::out,
				*speed,
			});
		}
		if(*next == static_cast<std::uint8_t>('m')) {
			++byte_offset_;
			auto parameter = read_number_first(1U);
			if(!parameter) {
				return std::unexpected(parameter.error());
			}
			return append(batch, CutsceneAudioFade{*parameter});
		}
		return {};
	}

	case static_cast<std::uint8_t>('g'): {
		auto next = peek_byte();
		if(!next) {
			return std::unexpected(next.error());
		}
		if(*next != static_cast<std::uint8_t>('a')) {
			auto parameter = read_number_first(8U);
			if(!parameter) {
				return std::unexpected(parameter.error());
			}
			return append(batch, CutsceneShake{*parameter, !fast_forward_});
		}
		++byte_offset_;
		auto code = read_number_first(0U);
		if(!code) {
			return std::unexpected(code.error());
		}
		auto drawn = append(batch, CutsceneDrawGaiji{
			cursor_,
			*code,
			text_color_,
		});
		if(!drawn) {
			return drawn;
		}
		return advance_cursor(batch);
	}

	case static_cast<std::uint8_t>('k'): {
		// Parsed only for source pointer/default side effects. TH03 always waits
		// indefinitely here regardless of the supplied value.
		auto ignored = read_number_first(0U);
		if(!ignored) {
			return std::unexpected(ignored.error());
		}
		if(fast_forward_) {
			return {};
		}
		return append(batch, CutsceneWait{
			CutsceneWaitKind::input_change,
			0U,
			0U,
		});
	}

	case static_cast<std::uint8_t>('@'):
		return append(batch, CutsceneClearPages{false});

	case static_cast<std::uint8_t>('p'): {
		auto sub = read_byte();
		if(!sub) {
			return std::unexpected(sub.error());
		}
		if(*sub == static_cast<std::uint8_t>('=') ||
			*sub == static_cast<std::uint8_t>('@')) {
			return append(batch, CutscenePresentBackground{
				*sub == static_cast<std::uint8_t>('='),
			});
		}
		if(*sub == static_cast<std::uint8_t>('-')) {
			return append(batch, CutsceneFreeBackground{});
		}
		if(*sub == static_cast<std::uint8_t>('p')) {
			return append(batch, CutsceneApplyBackgroundPalette{});
		}
		if(*sub != static_cast<std::uint8_t>(',')) {
			--byte_offset_;
			return {};
		}
		auto filename = read_filename();
		if(!filename) {
			return std::unexpected(filename.error());
		}
		return append(batch, CutsceneLoadBackground{*filename});
	}

	case static_cast<std::uint8_t>('='): {
		number_default_ = picture_quarter_count;
		auto next = peek_byte();
		if(!next) {
			return std::unexpected(next.error());
		}
		if(*next != static_cast<std::uint8_t>('=')) {
			auto quarter = read_number_first();
			if(!quarter) {
				return std::unexpected(quarter.error());
			}
			if(*quarter < picture_quarter_count) {
				return append(batch, CutsceneShowPictureQuarter{*quarter});
			}
			return append(batch, CutsceneClearPicture{});
		}
		++byte_offset_;
		auto quarter = read_number_first();
		if(!quarter) {
			return std::unexpected(quarter.error());
		}
		number_default_ = 1U;
		auto frames = read_number_second();
		if(!frames) {
			return std::unexpected(frames.error());
		}
		return append(batch, CutsceneCrossfadePicture{
			*quarter,
			*frames,
			!fast_forward_,
		});
	}

	case static_cast<std::uint8_t>('m'): {
		auto sub = peek_byte();
		if(!sub) {
			return std::unexpected(sub.error());
		}
		if(*sub == static_cast<std::uint8_t>('$')) {
			++byte_offset_;
			return append(batch, CutsceneBgm{CutsceneBgmAction::stop, {}});
		}
		if(*sub == static_cast<std::uint8_t>('*')) {
			++byte_offset_;
			return append(batch, CutsceneBgm{CutsceneBgmAction::play, {}});
		}
		if(*sub == static_cast<std::uint8_t>(',')) {
			++byte_offset_;
			auto filename = read_filename();
			if(!filename) {
				return std::unexpected(filename.error());
			}
			return append(batch, CutsceneBgm{
				CutsceneBgmAction::load_and_play,
				*filename,
			});
		}
		return {};
	}

	case static_cast<std::uint8_t>('e'): {
		auto id = read_number_first();
		if(!id) {
			return std::unexpected(id.error());
		}
		return append(batch, CutscenePlaySoundEffect{*id});
	}

	case static_cast<std::uint8_t>('$'): {
		finished_ = true;
		auto restored = append(batch, CutsceneRestoreBox{});
		if(!restored) {
			return restored;
		}
		auto released = append(batch, CutsceneReleaseBox{});
		if(!released) {
			return released;
		}
		auto freed = append(batch, CutsceneFreeBackground{});
		if(!freed) {
			return freed;
		}
		return append(batch, CutsceneFinish{});
	}

	default:
		return {};
	}
}

std::expected<void, CutsceneScriptError> CutsceneScriptVm::advance_cursor(
	CutsceneScriptBatch &batch
) noexcept
{
	cursor_.x += glyph_width;
	if(cursor_.x < box_right) {
		return {};
	}
	cursor_.y += glyph_height;
	cursor_.x = wrapped_line_left;
	if(cursor_.y < box_bottom) {
		return {};
	}
	if(!fast_forward_) {
		auto waited = append(batch, CutsceneWait{
			CutsceneWaitKind::input_change,
			0U,
			0U,
		});
		if(!waited) {
			return waited;
		}
	}
	cursor_ = {box_left, box_top};
	return append(batch, CutsceneRestoreBox{});
}

std::expected<void, CutsceneScriptError>
CutsceneScriptVm::append_typing_delay(
	CutsceneScriptInput input,
	CutsceneScriptBatch &batch
) noexcept
{
	if(fast_forward_) {
		return {};
	}
	if(!input.any_key_held) {
		return append(batch, CutsceneWait{
			CutsceneWaitKind::frames,
			text_interval_,
			0U,
		});
	}

	auto speedup_interval = static_cast<std::uint16_t>(text_interval_ / 3U);
	if((speedup_cycle_ & 1U) != 0U || speedup_interval != 0U) {
		if(speedup_interval == 0U) {
			speedup_interval = 1U;
		}
		auto delayed = append(batch, CutsceneWait{
			CutsceneWaitKind::frames,
			speedup_interval,
			0U,
		});
		if(!delayed) {
			return delayed;
		}
	}
	++speedup_cycle_;
	return {};
}

std::expected<void, CutsceneScriptError> CutsceneScriptVm::append(
	CutsceneScriptBatch &batch,
	CutsceneOperation operation
) const noexcept
{
	if(batch.count >= cutscene_batch_capacity) {
		return std::unexpected(CutsceneScriptError{
			CutsceneScriptErrorCode::operation_overflow,
			byte_offset_,
		});
	}
	batch.storage[batch.count] = std::move(operation);
	++batch.count;
	return {};
}

std::expected<std::uint8_t, CutsceneScriptError>
CutsceneScriptVm::read_byte() noexcept
{
	if(byte_offset_ >= script_.size()) {
		return std::unexpected(CutsceneScriptError{
			CutsceneScriptErrorCode::unexpected_end,
			script_.size(),
		});
	}
	const auto result = script_[byte_offset_];
	++byte_offset_;
	return result;
}

std::expected<std::uint8_t, CutsceneScriptError>
CutsceneScriptVm::peek_byte() const noexcept
{
	if(byte_offset_ >= script_.size()) {
		return std::unexpected(CutsceneScriptError{
			CutsceneScriptErrorCode::unexpected_end,
			script_.size(),
		});
	}
	return script_[byte_offset_];
}

std::expected<std::uint16_t, CutsceneScriptError>
CutsceneScriptVm::read_number_first() noexcept
{
	// The source helper speculatively reads three bytes and then rewinds over
	// non-digits. Inspect only bytes that can affect the result so that a valid
	// argument-less command immediately before \$ remains safe at the boundary.
	if(byte_offset_ >= script_.size()) {
		return std::unexpected(CutsceneScriptError{
			CutsceneScriptErrorCode::unexpected_end,
			script_.size(),
		});
	}
	const auto c0 = script_[byte_offset_];
	if(!is_digit(c0)) {
		return number_default_;
	}
	++byte_offset_;
	if(byte_offset_ >= script_.size()) {
		return static_cast<std::uint16_t>(
			c0 - static_cast<std::uint8_t>('0')
		);
	}
	const auto c1 = script_[byte_offset_];
	if(!is_digit(c1)) {
		return static_cast<std::uint16_t>(c0 - static_cast<std::uint8_t>('0'));
	}
	++byte_offset_;
	if(byte_offset_ >= script_.size()) {
		return static_cast<std::uint16_t>(
			(static_cast<std::uint16_t>(c0 - static_cast<std::uint8_t>('0')) * 10U) +
			static_cast<std::uint16_t>(c1 - static_cast<std::uint8_t>('0'))
		);
	}
	const auto c2 = script_[byte_offset_];
	if(!is_digit(c2)) {
		return static_cast<std::uint16_t>(
			(static_cast<std::uint16_t>(c0 - static_cast<std::uint8_t>('0')) * 10U) +
			static_cast<std::uint16_t>(c1 - static_cast<std::uint8_t>('0'))
		);
	}
	++byte_offset_;
	return static_cast<std::uint16_t>(
		(static_cast<std::uint16_t>(c0 - static_cast<std::uint8_t>('0')) * 100U) +
		(static_cast<std::uint16_t>(c1 - static_cast<std::uint8_t>('0')) * 10U) +
		static_cast<std::uint16_t>(c2 - static_cast<std::uint8_t>('0'))
	);
}

std::expected<std::uint16_t, CutsceneScriptError>
CutsceneScriptVm::read_number_first(std::uint16_t default_value) noexcept
{
	number_default_ = default_value;
	return read_number_first();
}

std::expected<std::uint16_t, CutsceneScriptError>
CutsceneScriptVm::read_number_second() noexcept
{
	auto next = peek_byte();
	if(!next) {
		return std::unexpected(next.error());
	}
	if(*next != static_cast<std::uint8_t>(',')) {
		return number_default_;
	}
	++byte_offset_;
	return read_number_first();
}

std::expected<CutsceneFilename, CutsceneScriptError>
CutsceneScriptVm::read_filename() noexcept
{
	CutsceneFilename result{};
	for(std::size_t index = 0U; index < cutscene_filename_capacity - 1U;
		++index) {
		auto next = read_byte();
		if(!next) {
			return std::unexpected(next.error());
		}
		if(is_separator(*next)) {
			return result;
		}
		result.bytes[index] = static_cast<char>(*next);
		++result.length;
	}
	return result;
}

CutsceneScriptVm::IterationState CutsceneScriptVm::iteration_state() const noexcept
{
	return {
		byte_offset_,
		cursor_,
		text_interval_,
		text_color_,
		text_weight_,
		number_default_,
		speedup_cycle_,
		fast_forward_,
		finished_,
	};
}

void CutsceneScriptVm::restore_iteration_state(IterationState state) noexcept
{
	byte_offset_ = state.byte_offset;
	cursor_ = state.cursor;
	text_interval_ = state.text_interval;
	text_color_ = state.text_color;
	text_weight_ = state.text_weight;
	number_default_ = state.number_default;
	speedup_cycle_ = state.speedup_cycle;
	fast_forward_ = state.fast_forward;
	finished_ = state.finished;
}

std::size_t CutsceneScriptVm::byte_offset() const noexcept
{
	return byte_offset_;
}

CutsceneCursor CutsceneScriptVm::cursor() const noexcept
{
	return cursor_;
}

std::uint16_t CutsceneScriptVm::text_interval() const noexcept
{
	return text_interval_;
}

std::uint16_t CutsceneScriptVm::text_color() const noexcept
{
	return text_color_;
}

GraphicsTextWeight CutsceneScriptVm::text_weight() const noexcept
{
	return text_weight_;
}

std::uint16_t CutsceneScriptVm::number_default() const noexcept
{
	return number_default_;
}

std::uint8_t CutsceneScriptVm::speedup_cycle() const noexcept
{
	return speedup_cycle_;
}

bool CutsceneScriptVm::fast_forward() const noexcept
{
	return fast_forward_;
}

bool CutsceneScriptVm::started() const noexcept
{
	return started_;
}

bool CutsceneScriptVm::finished() const noexcept
{
	return finished_;
}

} // namespace th03::present
