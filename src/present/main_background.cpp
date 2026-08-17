#include "th03/present/main_background.hpp"

#include <cstddef>
#include <cstdint>

#include "th03/exact/trig.hpp"

namespace th03::present {
namespace {

constexpr std::uint16_t type_a_width = 16U;
constexpr std::uint16_t type_a_raw_height = 3U;
constexpr std::uint16_t type_b_width = 16U;
constexpr std::uint16_t type_b_raw_height = 8U;
constexpr std::uint16_t initial_type_a_frame_count = 130U;
constexpr std::uint16_t initial_type_b_frame_count = 146U;
constexpr std::uint16_t frame_cycle_mask = 0x0FFFU;
constexpr std::uint16_t x_min_q12_4 = 0x0080U;
constexpr std::uint16_t x_max_q12_4 = 0x2780U;
constexpr std::uint16_t x_wrap_q12_4 = 0x2700U;
constexpr std::uint16_t y_min_q12_4 = 0x00E0U;
constexpr std::uint16_t y_max_q12_4 = 0x1800U;
constexpr std::uint16_t y_wrap_q12_4 = 0x1720U;
constexpr Sprite16HorizontalClip full_clip{
	0,
	static_cast<std::int32_t>(graphics_width - 1U),
};

[[nodiscard]] constexpr bool valid_kind(MainBackgroundKind kind) noexcept
{
	return kind == MainBackgroundKind::type_a ||
		kind == MainBackgroundKind::type_b;
}

[[nodiscard]] constexpr bool valid_phase(MainBackgroundPhase phase) noexcept
{
	return phase == MainBackgroundPhase::initial ||
		phase == MainBackgroundPhase::steady;
}

[[nodiscard]] constexpr bool valid_rect(
	Sprite16AtlasRect rect,
	std::uint16_t width,
	std::uint16_t raw_height
) noexcept
{
	return rect.width == width && rect.raw_height == raw_height &&
		(rect.left % 16U) == 0U &&
		(static_cast<std::uint32_t>(rect.left) + rect.width <=
			sprite16_source_width) &&
		(static_cast<std::uint32_t>(rect.raw_top) + rect.raw_height <=
			sprite16_source_height);
}

[[nodiscard]] constexpr bool valid_layout(
	const MainBackgroundAtlasLayout &layout
) noexcept
{
	for(const auto rect : layout.type_a_variants) {
		if(!valid_rect(rect, type_a_width, type_a_raw_height)) {
			return false;
		}
	}
	if(!valid_rect(layout.type_b_static, type_b_width, type_b_raw_height)) {
		return false;
	}
	for(const auto rect : layout.type_b_animation) {
		if(!valid_rect(rect, type_b_width, type_b_raw_height)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] constexpr std::int16_t word_from_bits(
	std::uint16_t bits
) noexcept
{
	return (bits <= 0x7FFFU) ?
		static_cast<std::int16_t>(bits) :
		static_cast<std::int16_t>(static_cast<std::int32_t>(bits) - 0x10000);
}

[[nodiscard]] constexpr std::int16_t wrap_add_word(
	std::int16_t left,
	std::int16_t right
) noexcept
{
	return word_from_bits(static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(left) + static_cast<std::uint16_t>(right)
	));
}

[[nodiscard]] constexpr std::int32_t signed_shift_right(
	std::int16_t value,
	unsigned int count
) noexcept
{
	const auto divisor = static_cast<std::int32_t>(1U << count);
	const auto wide = static_cast<std::int32_t>(value);
	if(wide >= 0) {
		return wide / divisor;
	}
	return -(((-wide) + divisor - 1) / divisor);
}

[[nodiscard]] constexpr std::size_t active_count(
	MainBackgroundKind kind
) noexcept
{
	return (kind == MainBackgroundKind::type_a) ?
		main_background_type_a_record_count : main_background_record_count;
}

[[nodiscard]] constexpr std::uint16_t initial_frame_count(
	MainBackgroundKind kind
) noexcept
{
	return (kind == MainBackgroundKind::type_a) ?
		initial_type_a_frame_count : initial_type_b_frame_count;
}

[[nodiscard]] constexpr std::uint16_t unsigned_remainder(
	std::uint16_t value,
	std::uint16_t divisor
) noexcept
{
	// IRAND returns 0..0x7FFF, so D092/D2E8's signed CWD/IDIV has this same
	// nonnegative remainder for every fact the core can emit.
	return static_cast<std::uint16_t>(value % divisor);
}

} // namespace

MainBackgroundAtlasLayout authentic_main_background_atlas_layout() noexcept
{
	// Anniversary 3e2381d4 th03/main/player/d3f9.cpp D092/D2E8 stores
	// offsets 210Eh/238Eh, 233Eh, and 20BEh (+2 bytes per animated cel).
	// Decoding each offset as raw_row * 80 + byte_x yields these atlas-local
	// Sprite16 rectangles.
	return {
		.type_a_variants = {{
			{496U, 105U, type_a_width, type_a_raw_height},
			{496U, 113U, type_a_width, type_a_raw_height},
		}},
		.type_b_static = {496U, 112U, type_b_width, type_b_raw_height},
		.type_b_animation = {{
			{496U, 104U, type_b_width, type_b_raw_height},
			{512U, 104U, type_b_width, type_b_raw_height},
			{528U, 104U, type_b_width, type_b_raw_height},
			{544U, 104U, type_b_width, type_b_raw_height},
		}},
	};
}

MainBackgroundPresenter::MainBackgroundPresenter(
	Raster &raster,
	const Sprite16Staging &staging,
	MainBackgroundAtlasLayout layout
) noexcept :
	raster_(&raster),
	staging_(&staging),
	layout_(layout)
{
	if(!valid_layout(layout_)) {
		status_ = MainBackgroundStatus::invalid_layout;
	}
}

void MainBackgroundPresenter::fail(MainBackgroundStatus status) noexcept
{
	if(ready()) {
		status_ = status;
	}
}

void MainBackgroundPresenter::main_background_initialize(
	const MainBackgroundInitialize &initialize
) noexcept
{
	if(!ready()) {
		return;
	}
	if(
		initialized_ || read_stream_ != ReadStream::none ||
		!valid_kind(initialize.kind)
	) {
		fail(MainBackgroundStatus::invalid_observation);
		return;
	}

	kind_ = initialize.kind;
	records_ = {};
	read_stream_ = ReadStream::initialize;
	read_slot_ = 0U;
	read_field_ = 0U;
	initial_frames_completed_ = 0U;
	magnitude_ = 0x40U;
	vector_latch_ = false;
	have_round_id_ = false;
}

void MainBackgroundPresenter::consume_initialize_read(
	std::uint16_t raw_value
) noexcept
{
	// Anniversary 3e2381d4 d3f9.cpp D092 consumes X/Y/variant/unknown_C for
	// each of 48 records; D2E8 consumes X/Y for each of 50 records.
	auto &record = records_[read_slot_];
	if(kind_ == MainBackgroundKind::type_a) {
		switch(read_field_) {
		case 0U:
			record.x_q12_4 = static_cast<std::int16_t>(
				unsigned_remainder(raw_value, graphics_width * 16U)
			);
			break;
		case 1U:
			record.y_q12_4 = static_cast<std::int16_t>(
				unsigned_remainder(raw_value, graphics_height * 16U)
			);
			record.velocity_x_q12_4 = 0;
			record.velocity_y_q12_4 = 0x10;
			record.angle = 0x20U;
			break;
		case 2U:
			record.sprite_variant = static_cast<std::uint8_t>(raw_value & 1U);
			break;
		case 3U:
			// D092 stores raw_value % 4000 in unknown_C. No source reader
			// exists, but consuming this fourth grouped fact is observable.
			break;
		default:
			fail(MainBackgroundStatus::invalid_observation);
			return;
		}
		read_field_ = static_cast<std::uint8_t>(read_field_ + 1U);
		if(read_field_ < 4U) {
			return;
		}
		read_field_ = 0U;
		++read_slot_;
		if(read_slot_ < main_background_type_a_record_count) {
			return;
		}
	} else {
		if(read_field_ == 0U) {
			record.x_q12_4 = static_cast<std::int16_t>(
				unsigned_remainder(raw_value, graphics_width * 16U)
			);
			read_field_ = 1U;
			return;
		}
		if(read_field_ != 1U) {
			fail(MainBackgroundStatus::invalid_observation);
			return;
		}
		record.y_q12_4 = static_cast<std::int16_t>(
			unsigned_remainder(raw_value, graphics_height * 16U)
		);
		record.velocity_x_q12_4 = 0;
		record.velocity_y_q12_4 = 0x10;
		record.angle = 0x10U;
		record.sprite_variant = static_cast<std::uint8_t>(
			(read_slot_ < main_background_type_b_static_count) ? 0U : 1U
		);
		read_field_ = 0U;
		++read_slot_;
		if(read_slot_ < main_background_record_count) {
			return;
		}
	}

	read_stream_ = ReadStream::none;
	read_slot_ = 0U;
	initialized_ = true;
}

void MainBackgroundPresenter::main_background_frame(
	const MainBackgroundFrame &frame
) noexcept
{
	if(!ready()) {
		return;
	}
	if(
		!initialized_ || read_stream_ != ReadStream::none ||
		!valid_kind(frame.kind) || !valid_phase(frame.phase) ||
		frame.kind != kind_
	) {
		fail(MainBackgroundStatus::invalid_observation);
		return;
	}
	const bool first_round = !have_round_id_;
	bool repeat_round = false;
	if(!first_round && frame.round_id != round_id_) {
		if(frame.round_id != static_cast<std::uint8_t>(round_id_ + 1U)) {
			fail(MainBackgroundStatus::invalid_observation);
			return;
		}
		repeat_round = true;
	}

	const auto expected_phase =
		(initial_frames_completed_ < initial_frame_count(kind_))
			? MainBackgroundPhase::initial
			: MainBackgroundPhase::steady;
	// An unusually short round can repeat while D135/D340 is still installed.
	// Its round_frame test starts over at zero, so the complete initial interval
	// begins again without rerunning D092/D2E8.
	if(frame.phase != expected_phase) {
		fail(MainBackgroundStatus::invalid_observation);
		return;
	}
	if(!staging_->committed()) {
		fail(MainBackgroundStatus::staging_not_committed);
		return;
	}
	if(first_round) {
		round_id_ = frame.round_id;
		have_round_id_ = true;
	}
	if(repeat_round) {
		if(expected_phase == MainBackgroundPhase::initial) {
			initial_frames_completed_ = 0U;
		}
		round_id_ = frame.round_id;
		repeat_velocity_reset();
	}

	if(frame.phase == MainBackgroundPhase::initial) {
		// D135/D340 overwrite the global magnitude before consuming their
		// ordered per-record IRAND stream.
		magnitude_ = 0x40U;
		read_stream_ = ReadStream::initial_frame;
		read_slot_ = 0U;
		read_field_ = 0U;
		return;
	}

	steady_frame(static_cast<std::uint16_t>(
		frame.round_or_result_frame & frame_cycle_mask
	));
}

void MainBackgroundPresenter::consume_initial_frame_read(
	std::uint16_t raw_value
) noexcept
{
	// Anniversary 3e2381d4 d3f9.cpp D135/D340 consumes one read, mutates one
	// record, and draws it before advancing to the next record.
	if(read_slot_ >= active_count(kind_)) {
		fail(MainBackgroundStatus::invalid_observation);
		return;
	}
	auto &record = records_[read_slot_];
	record.velocity_y_q12_4 = wrap_add_word(
		record.velocity_y_q12_4,
		static_cast<std::int16_t>(raw_value & 1U)
	);
	record.angle = static_cast<std::uint8_t>(record.velocity_y_q12_4);
	move(record);

	const bool drawn = (read_slot_ < main_background_dot_count) ?
		draw_dot(record) :
		draw_sprite(record, read_slot_, 0U, kind_ == MainBackgroundKind::type_a);
	if(!drawn) {
		fail(MainBackgroundStatus::draw_failed);
		return;
	}
	++read_slot_;

	if(kind_ == MainBackgroundKind::type_a) {
		if(read_slot_ == main_background_type_a_record_count) {
			finish_initial_frame();
		}
		return;
	}
	if(read_slot_ < main_background_type_b_static_count) {
		return;
	}

	// D340's final ten animated records add AX=0 and consume no IRAND.
	for(; read_slot_ < main_background_record_count; ++read_slot_) {
		auto &animated = records_[read_slot_];
		animated.angle = static_cast<std::uint8_t>(
			animated.velocity_y_q12_4
		);
		move(animated);
		if(!draw_sprite(animated, read_slot_, 0U, false)) {
			fail(MainBackgroundStatus::draw_failed);
			return;
		}
	}
	finish_initial_frame();
}

void MainBackgroundPresenter::finish_initial_frame() noexcept
{
	read_stream_ = ReadStream::none;
	read_slot_ = 0U;
	initial_frames_completed_ = static_cast<std::uint16_t>(
		initial_frames_completed_ + 1U
	);
}

void MainBackgroundPresenter::main_background_random_read(
	const MainBackgroundRandomRead &read
) noexcept
{
	if(!ready()) {
		return;
	}
	if(read.raw_value > 0x7FFFU || read_stream_ == ReadStream::none) {
		fail(MainBackgroundStatus::invalid_observation);
		return;
	}
	if(
		read_stream_ == ReadStream::initial_frame &&
		!staging_->committed()
	) {
		fail(MainBackgroundStatus::staging_not_committed);
		return;
	}
	if(read_stream_ == ReadStream::initialize) {
		consume_initialize_read(read.raw_value);
		return;
	}
	consume_initial_frame_read(read.raw_value);
}

void MainBackgroundPresenter::repeat_velocity_reset() noexcept
{
	// Anniversary 3e2381d4 d3f9.cpp D05D preserves every position and angle,
	// restores the shared length to 64, and revectors the complete live pool.
	magnitude_ = 0x40U;
	for(std::size_t slot = 0U; slot < active_count(kind_); ++slot) {
		const auto vector = exact::vector2(records_[slot].angle, 0x40);
		records_[slot].velocity_x_q12_4 = vector.x;
		records_[slot].velocity_y_q12_4 = vector.y;
	}
}

void MainBackgroundPresenter::move(Record &record) noexcept
{
	// Anniversary 3e2381d4 d3f9.cpp D0FA compares the wrapped words as
	// unsigned values and applies at most one correction per axis.
	auto x = static_cast<std::uint16_t>(wrap_add_word(
		record.x_q12_4, record.velocity_x_q12_4
	));
	if(x < x_min_q12_4) {
		x = static_cast<std::uint16_t>(x + x_wrap_q12_4);
	} else if(x > x_max_q12_4) {
		x = static_cast<std::uint16_t>(x - x_wrap_q12_4);
	}
	record.x_q12_4 = word_from_bits(x);

	auto y = static_cast<std::uint16_t>(wrap_add_word(
		record.y_q12_4, record.velocity_y_q12_4
	));
	if(y < y_min_q12_4) {
		y = static_cast<std::uint16_t>(y + y_wrap_q12_4);
	} else if(y > y_max_q12_4) {
		y = static_cast<std::uint16_t>(y - y_wrap_q12_4);
	}
	record.y_q12_4 = word_from_bits(y);
}

bool MainBackgroundPresenter::draw_dot(const Record &record) noexcept
{
	if(!rendering_enabled_) {
		return true;
	}
	const auto x = signed_shift_right(record.x_q12_4, 4U);
	const auto screen_y = signed_shift_right(record.y_q12_4, 4U);
	const auto raw_y = signed_shift_right(
		static_cast<std::int16_t>(screen_y), 1U
	);
	const auto color = static_cast<Index4>(
		(kind_ == MainBackgroundKind::type_a) ? 10U : 13U
	);
	return raster_->plot({x, raw_y}, color) &&
		raster_->plot({x + 1, raw_y}, color);
}

bool MainBackgroundPresenter::draw_sprite(
	const Record &record,
	std::size_t slot,
	std::uint16_t frame_mod_4096,
	bool clipped_type_a
) noexcept
{
	Sprite16AtlasRect source{};
	if(kind_ == MainBackgroundKind::type_a) {
		if(record.sprite_variant >= layout_.type_a_variants.size()) {
			return false;
		}
		source = layout_.type_a_variants[record.sprite_variant];
	} else if(slot < main_background_type_b_static_count) {
		source = layout_.type_b_static;
	} else {
		const auto cel = static_cast<std::size_t>(
			(frame_mod_4096 & 0x001FU) >> 3U
		);
		source = layout_.type_b_animation[cel];
	}
	if(!rendering_enabled_) {
		return true;
	}

	const Point position{
		signed_shift_right(record.x_q12_4, 4U),
		signed_shift_right(record.y_q12_4, 4U),
	};
	if(clipped_type_a) {
		return sprite16_put(
			*raster_, *staging_, source, position, full_clip
		);
	}
	return sprite16_put_noclip(*raster_, *staging_, source, position);
}

bool MainBackgroundPresenter::draw_all(
	std::uint16_t frame_mod_4096
) noexcept
{
	for(std::size_t slot = 0U; slot < main_background_dot_count; ++slot) {
		if(!draw_dot(records_[slot])) {
			return false;
		}
	}
	for(
		std::size_t slot = main_background_dot_count;
		slot < active_count(kind_);
		++slot
	) {
		if(!draw_sprite(records_[slot], slot, frame_mod_4096, false)) {
			return false;
		}
	}
	return true;
}

void MainBackgroundPresenter::steady_frame(
	std::uint16_t frame_mod_4096
) noexcept
{
	// Anniversary 3e2381d4 th03/main/player/d3f9.cpp D1E7/D3F9:
	// the two non-triggering acceleration ranges jump past assignment to the
	// automatic flag. `vector_latch_` deliberately retains the preceding call.
	// Compatibility status: this is a deterministic stand-in for a source-
	// uninitialized stack dependency, not an emulator-verified value. In the
	// closed stock state, retained true only repeats an identical vector2 write
	// and retained false only skips it, so the two outcomes produce the same
	// subsequent pixels until a branch explicitly changes the magnitude.
	if(frame_mod_4096 < 1024U) {
		vector_latch_ = false;
	} else if(frame_mod_4096 < 1280U) {
		if((frame_mod_4096 & 7U) == 0U) {
			magnitude_ = static_cast<std::uint8_t>(magnitude_ + 1U);
			vector_latch_ = true;
		}
	} else if(frame_mod_4096 < 2048U) {
		vector_latch_ = false;
	} else if(frame_mod_4096 < 2304U) {
		if((frame_mod_4096 & 3U) == 0U) {
			magnitude_ = static_cast<std::uint8_t>(magnitude_ - 1U);
			vector_latch_ = true;
		}
	} else if(frame_mod_4096 >= 3072U && frame_mod_4096 < 4064U) {
		if((frame_mod_4096 & 127U) < 64U) {
			magnitude_ = static_cast<std::uint8_t>(magnitude_ + 1U);
		} else {
			magnitude_ = static_cast<std::uint8_t>(magnitude_ - 1U);
		}
		vector_latch_ = true;
	} else {
		vector_latch_ = false;
	}

	// Source order is significant: D0FA moves with the old velocity before
	// vector2 writes the velocity consumed by the next callback.
	for(std::size_t slot = 0U; slot < active_count(kind_); ++slot) {
		auto &record = records_[slot];
		move(record);
		if(vector_latch_) {
			const auto vector = exact::vector2(record.angle, magnitude_);
			record.velocity_x_q12_4 = vector.x;
			record.velocity_y_q12_4 = vector.y;
		}
	}
	if(!draw_all(frame_mod_4096)) {
		fail(MainBackgroundStatus::draw_failed);
	}
}

MainBackgroundStatus MainBackgroundPresenter::status() const noexcept
{
	return status_;
}

bool MainBackgroundPresenter::ready() const noexcept
{
	return status_ == MainBackgroundStatus::ready;
}

bool MainBackgroundPresenter::initialization_complete() const noexcept
{
	return ready() && initialized_ && read_stream_ == ReadStream::none;
}

void MainBackgroundPresenter::set_rendering_enabled(bool enabled) noexcept
{
	rendering_enabled_ = enabled;
}

bool MainBackgroundPresenter::accelerator_restore(
	const std::array<MainBackgroundRecordState, main_background_record_count>
		&records,
	std::uint8_t magnitude,
	std::uint8_t round_id
) noexcept
{
	if(!initialization_complete()) {
		return false;
	}
	for(const auto &source : records) {
		if(source.sprite_variant >= layout_.type_a_variants.size()) {
			return false;
		}
	}
	for(std::size_t slot = 0U; slot < records_.size(); ++slot) {
		const auto &source = records[slot];
		records_[slot] = {
			.x_q12_4 = source.x_q12_4,
			.y_q12_4 = source.y_q12_4,
			.velocity_x_q12_4 = source.velocity_x_q12_4,
			.velocity_y_q12_4 = source.velocity_y_q12_4,
			.angle = source.angle,
			.sprite_variant = source.sprite_variant,
		};
	}
	magnitude_ = magnitude;
	round_id_ = round_id;
	have_round_id_ = true;
	return true;
}

} // namespace th03::present
