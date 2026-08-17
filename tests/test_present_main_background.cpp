#include "th03/present/main_background.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "th03/exact/trig.hpp"

namespace {

using th03::MainBackgroundFrame;
using th03::MainBackgroundInitialize;
using th03::MainBackgroundKind;
using th03::MainBackgroundPhase;
using th03::MainBackgroundRandomRead;
using th03::present::Index4;
using th03::present::MainBackgroundAtlasLayout;
using th03::present::MainBackgroundPresenter;
using th03::present::MainBackgroundRecordState;
using th03::present::MainBackgroundStatus;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;
using th03::present::RasterMode;
using th03::present::Rect;
using th03::present::Sprite16AtlasRect;
using th03::present::Sprite16Staging;
using th03::present::authentic_main_background_atlas_layout;
using th03::present::graphics_height;
using th03::present::graphics_width;
using th03::present::main_background_record_count;
using th03::present::main_background_type_a_record_count;
using th03::present::sprite16_source_top;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::fprintf(
			stderr,
			"FAIL: %.*s\n",
			static_cast<int>(message.size()),
			message.data()
		);
	}
}

void expect_pixel(
	const Raster &raster,
	Point point,
	Index4 expected,
	std::string_view message
)
{
	expect(raster.pixel(Page::zero, point) == expected, message);
}

[[nodiscard]] constexpr std::int32_t shift_right_floor(
	std::int32_t value,
	unsigned int count
) noexcept
{
	const auto divisor = static_cast<std::int32_t>(1U << count);
	return (value >= 0) ?
		(value / divisor) : -(((-value) + divisor - 1) / divisor);
}

[[nodiscard]] MainBackgroundAtlasLayout synthetic_layout() noexcept
{
	return {
		.type_a_variants = {{
			{0U, 0U, 16U, 3U},
			{16U, 0U, 16U, 3U},
		}},
		.type_b_static = {32U, 0U, 16U, 8U},
		.type_b_animation = {{
			{48U, 0U, 16U, 8U},
			{64U, 0U, 16U, 8U},
			{80U, 0U, 16U, 8U},
			{96U, 0U, 16U, 8U},
		}},
	};
}

void fill_source(Raster &raster, Sprite16AtlasRect rect, Index4 color)
{
	for(std::uint16_t y = 0U; y < rect.raw_height; ++y) {
		for(std::uint16_t x = 0U; x < rect.width; ++x) {
			expect(
				raster.plot(
					{
						static_cast<std::int32_t>(rect.left + x),
						static_cast<std::int32_t>(
							sprite16_source_top + rect.raw_top + y
						),
					},
					color
				),
				"seed synthetic background source"
			);
		}
	}
}

void commit_sources(
	Raster &raster,
	Sprite16Staging &staging,
	const MainBackgroundAtlasLayout &layout
)
{
	fill_source(raster, layout.type_a_variants[0U], 1U);
	fill_source(raster, layout.type_a_variants[1U], 2U);
	fill_source(raster, layout.type_b_static, 3U);
	for(std::size_t cel = 0U; cel < layout.type_b_animation.size(); ++cel) {
		fill_source(
			raster,
			layout.type_b_animation[cel],
			static_cast<Index4>(4U + cel)
		);
	}
	expect(staging.commit(raster), "commit synthetic background atlas");
	expect(
		raster.set_raster_mode(RasterMode::zoomed_200_upper),
		"select MAIN's zoomed upper mode"
	);
}

void clear_visible(Raster &raster)
{
	expect(
		raster.clear({0, 0, graphics_width, 200U}, 0U),
		"clear MAIN's visible raw rows"
	);
}

struct Placement {
	std::uint16_t x_q12_4 = 0U;
	std::uint16_t y_q12_4 = 0U;
	std::uint8_t variant = 0U;
};

struct Motion {
	std::int16_t x_q12_4 = 0;
	std::int16_t y_q12_4 = 0;
	std::int16_t velocity_x_q12_4 = 0;
	std::int16_t velocity_y_q12_4 = 0;
};

[[nodiscard]] constexpr std::int16_t word_from_bits(
	std::uint16_t bits
) noexcept
{
	return (bits <= 0x7FFFU) ?
		static_cast<std::int16_t>(bits) :
		static_cast<std::int16_t>(static_cast<std::int32_t>(bits) - 0x10000);
}

void move_source_exact(Motion &motion) noexcept
{
	auto x = static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(motion.x_q12_4) +
		static_cast<std::uint16_t>(motion.velocity_x_q12_4)
	);
	if(x < 0x0080U) {
		x = static_cast<std::uint16_t>(x + 0x2700U);
	} else if(x > 0x2780U) {
		x = static_cast<std::uint16_t>(x - 0x2700U);
	}
	motion.x_q12_4 = word_from_bits(x);

	auto y = static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(motion.y_q12_4) +
		static_cast<std::uint16_t>(motion.velocity_y_q12_4)
	);
	if(y < 0x00E0U) {
		y = static_cast<std::uint16_t>(y + 0x1720U);
	} else if(y > 0x1800U) {
		y = static_cast<std::uint16_t>(y - 0x1720U);
	}
	motion.y_q12_4 = word_from_bits(y);
}

void set_motion_vector(Motion &motion, std::uint8_t magnitude) noexcept
{
	const auto vector = th03::exact::vector2(0x10U, magnitude);
	motion.velocity_x_q12_4 = vector.x;
	motion.velocity_y_q12_4 = vector.y;
}

void advance_reference_steady(
	Motion &motion,
	std::uint16_t frame,
	std::uint8_t &magnitude,
	bool &vector_latch
) noexcept
{
	if(frame < 1024U) {
		vector_latch = false;
	} else if(frame < 1280U) {
		if((frame & 7U) == 0U) {
			magnitude = static_cast<std::uint8_t>(magnitude + 1U);
			vector_latch = true;
		}
	} else if(frame < 2048U) {
		vector_latch = false;
	} else if(frame < 2304U) {
		if((frame & 3U) == 0U) {
			magnitude = static_cast<std::uint8_t>(magnitude - 1U);
			vector_latch = true;
		}
	} else if(frame >= 3072U && frame < 4064U) {
		if((frame & 127U) < 64U) {
			magnitude = static_cast<std::uint8_t>(magnitude + 1U);
		} else {
			magnitude = static_cast<std::uint8_t>(magnitude - 1U);
		}
		vector_latch = true;
	} else {
		vector_latch = false;
	}

	move_source_exact(motion);
	if(vector_latch) {
		set_motion_vector(motion, magnitude);
	}
}

void expect_motion_dot(
	const Raster &raster,
	const Motion &motion,
	std::string_view message
)
{
	expect_pixel(
		raster,
		{
			shift_right_floor(motion.x_q12_4, 4U),
			shift_right_floor(motion.y_q12_4, 5U),
		},
		10U,
		message
	);
}

template <typename PlacementFor>
void initialize(
	MainBackgroundPresenter &presenter,
	MainBackgroundKind kind,
	PlacementFor placement_for
)
{
	presenter.main_background_initialize({.kind = kind});
	const auto count = (kind == MainBackgroundKind::type_a) ?
		main_background_type_a_record_count : main_background_record_count;
	for(std::size_t slot = 0U; slot < count; ++slot) {
		const auto placement = placement_for(slot);
		presenter.main_background_random_read({placement.x_q12_4});
		presenter.main_background_random_read({placement.y_q12_4});
		if(kind == MainBackgroundKind::type_a) {
			presenter.main_background_random_read({placement.variant});
			presenter.main_background_random_read({3999U});
		}
	}
}

void initial_frame(
	MainBackgroundPresenter &presenter,
	MainBackgroundKind kind,
	std::uint16_t frame,
	std::uint8_t round_id = 0U,
	std::uint16_t random_value = 0U
)
{
	presenter.main_background_frame({
		.round_id = round_id,
		.round_or_result_frame = frame,
		.kind = kind,
		.phase = MainBackgroundPhase::initial,
	});
	const auto reads = (kind == MainBackgroundKind::type_a) ? 48U : 40U;
	for(std::size_t read = 0U; read < reads; ++read) {
		presenter.main_background_random_read({random_value});
	}
}

void test_authentic_layout()
{
	const auto layout = authentic_main_background_atlas_layout();
	expect(
		layout.type_a_variants[0U].left == 496U &&
		layout.type_a_variants[0U].raw_top == 105U &&
		layout.type_a_variants[0U].width == 16U &&
		layout.type_a_variants[0U].raw_height == 3U,
		"type-A first offset maps to its authentic 16x3 atlas rectangle"
	);
	expect(
		layout.type_a_variants[1U].left == 496U &&
		layout.type_a_variants[1U].raw_top == 113U,
		"type-A variant bit selects the second authentic rectangle"
	);
	expect(
		layout.type_b_static.left == 496U &&
		layout.type_b_static.raw_top == 112U &&
		layout.type_b_static.width == 16U &&
		layout.type_b_static.raw_height == 8U,
		"type-B static offset maps to its authentic 16x8 rectangle"
	);
	for(std::size_t cel = 0U; cel < layout.type_b_animation.size(); ++cel) {
		expect(
			layout.type_b_animation[cel].left == 496U + (cel * 16U) &&
			layout.type_b_animation[cel].raw_top == 104U &&
			layout.type_b_animation[cel].width == 16U &&
			layout.type_b_animation[cel].raw_height == 8U,
			"type-B animation preserves four consecutive authentic cels"
		);
	}
}

void test_type_a_grouping_immediate_order_and_wrap()
{
	Raster raster;
	Sprite16Staging staging;
	const auto layout = synthetic_layout();
	commit_sources(raster, staging, layout);
	MainBackgroundPresenter presenter(raster, staging, layout);
	initialize(
		presenter,
		MainBackgroundKind::type_a,
		[](std::size_t slot) noexcept {
			if(slot == 0U) {
				return Placement{50U * 16U, 40U * 16U, 0U};
			}
			if(slot == 24U) {
				return Placement{200U * 16U, 100U * 16U, 1U};
			}
			return Placement{400U * 16U, 300U * 16U, 0U};
		}
	);
	expect(presenter.ready(), "type-A grouped initialization completes");
	presenter.main_background_frame({
		.round_id = 7U,
		.round_or_result_frame = 0U,
		.kind = MainBackgroundKind::type_a,
		.phase = MainBackgroundPhase::initial,
	});
	expect_pixel(raster, {50, 20}, 0U, "frame fact alone does not run a future read");
	presenter.main_background_random_read({0U});
	expect_pixel(
		raster, {50, 20}, 10U,
		"first type-A read moves and immediately draws the first two-pixel dot"
	);
	expect_pixel(raster, {51, 20}, 10U, "D031 writes the adjacent dot pixel");
	for(std::size_t slot = 1U; slot < 24U; ++slot) {
		presenter.main_background_random_read({0U});
	}
	expect_pixel(
		raster, {200, 50}, 0U,
		"all 24 dot reads precede the first type-A sprite read"
	);
	presenter.main_background_random_read({1U});
	expect_pixel(
		raster, {200, 50}, 2U,
		"25th read selects the initialized variant and draws slot 24 immediately"
	);
	for(std::size_t slot = 25U; slot < 48U; ++slot) {
		presenter.main_background_random_read({0U});
	}
	expect(presenter.ready(), "complete type-A initial callback returns to idle");

	Raster wrap_raster;
	Sprite16Staging wrap_staging;
	commit_sources(wrap_raster, wrap_staging, layout);
	MainBackgroundPresenter wrap(wrap_raster, wrap_staging, layout);
	initialize(
		wrap,
		MainBackgroundKind::type_a,
		[](std::size_t) noexcept { return Placement{}; }
	);
	wrap.main_background_frame({
		.round_id = 0U,
		.round_or_result_frame = 0U,
		.kind = MainBackgroundKind::type_a,
		.phase = MainBackgroundPhase::initial,
	});
	wrap.main_background_random_read({0U});
	expect_pixel(
		wrap_raster, {624, 185}, 10U,
		"D0FA unsigned minimum checks add the exact X/Y wrap spans once"
	);
}

void test_type_b_read_boundary_and_animation()
{
	Raster raster;
	Sprite16Staging staging;
	const auto layout = synthetic_layout();
	commit_sources(raster, staging, layout);
	MainBackgroundPresenter presenter(raster, staging, layout);
	initialize(
		presenter,
		MainBackgroundKind::type_b,
		[](std::size_t) noexcept {
			return Placement{120U * 16U, 100U * 16U, 0U};
		}
	);
	presenter.main_background_frame({
		.round_id = 0U,
		.round_or_result_frame = 0U,
		.kind = MainBackgroundKind::type_b,
		.phase = MainBackgroundPhase::initial,
	});
	for(std::size_t read = 0U; read < 39U; ++read) {
		presenter.main_background_random_read({0U});
	}
	expect_pixel(
		raster, {120, 50}, 3U,
		"before read 40 only type-B static sprite slots have drawn"
	);
	presenter.main_background_random_read({0U});
	expect_pixel(
		raster, {120, 50}, 4U,
		"read 40 completes slot 39 then draws ten no-read animated slots"
	);

	for(std::uint16_t frame = 1U; frame < 146U; ++frame) {
		initial_frame(presenter, MainBackgroundKind::type_b, frame);
	}
	expect(presenter.ready(), "type-B accepts exactly 146 initial callbacks");
	clear_visible(raster);
	presenter.main_background_frame({
		.round_id = 0U,
		.round_or_result_frame = 8U,
		.kind = MainBackgroundKind::type_b,
		.phase = MainBackgroundPhase::steady,
	});
	expect(presenter.ready(), "type-B enters steady callback after frame 145");
	expect_pixel(
		raster, {120, 123}, 5U,
		"steady type-B animation selects ((frame & 31) >> 3) cel one"
	);
}

void test_type_a_initial_clip_then_steady_noclip()
{
	Raster raster;
	Sprite16Staging staging;
	const auto layout = synthetic_layout();
	commit_sources(raster, staging, layout);
	MainBackgroundPresenter presenter(raster, staging, layout);
	initialize(
		presenter,
		MainBackgroundKind::type_a,
		[](std::size_t slot) noexcept {
			return (slot == main_background_type_a_record_count / 2U)
				? Placement{0x2780U, 100U * 16U, 1U}
				: Placement{400U * 16U, 100U * 16U, 0U};
		}
	);
	clear_visible(raster);
	initial_frame(presenter, MainBackgroundKind::type_a, 0U);
	expect_pixel(
		raster,
		{632, 50},
		0U,
		"D135 clipped put drops the complete right-edge Sprite16 column"
	);
	for(std::uint16_t frame = 1U; frame < 130U; ++frame) {
		initial_frame(presenter, MainBackgroundKind::type_a, frame);
	}

	clear_visible(raster);
	presenter.main_background_frame({
		.round_id = 0U,
		.round_or_result_frame = 130U,
		.kind = MainBackgroundKind::type_a,
		.phase = MainBackgroundPhase::steady,
	});
	expect_pixel(
		raster,
		{632, 115},
		2U,
		"D1E7 noclip preserves the visible half of a right-edge Sprite16"
	);
	expect_pixel(
		raster,
		{0, 116},
		2U,
		"D1E7 noclip preserves the source 16-bit VRAM row wrap"
	);
	expect(presenter.ready(), "clip-selection probe remains valid");
}

void test_steady_order_latch_schedule_and_repeat_reset()
{
	Raster raster;
	Sprite16Staging staging;
	const auto layout = synthetic_layout();
	commit_sources(raster, staging, layout);
	MainBackgroundPresenter presenter(raster, staging, layout);
	initialize(
		presenter,
		MainBackgroundKind::type_a,
		[](std::size_t slot) noexcept {
			return (slot < 24U)
				? Placement{100U * 16U, 100U * 16U, 0U}
				: Placement{400U * 16U, 100U * 16U, 0U};
		}
	);
	for(std::uint16_t frame = 0U; frame < 130U; ++frame) {
		initial_frame(presenter, MainBackgroundKind::type_a, frame);
	}
	Motion motion{
		.x_q12_4 = 100 * 16,
		.y_q12_4 = 230 * 16,
		.velocity_x_q12_4 = 0,
		.velocity_y_q12_4 = 16,
	};
	auto magnitude = std::uint8_t{0x40U};
	bool vector_latch = false;
	for(std::uint16_t frame = 130U; frame <= 1025U; ++frame) {
		if(frame == 1024U || frame == 1025U) {
			clear_visible(raster);
		}
		presenter.main_background_frame({
			.round_id = 0U,
			.round_or_result_frame = frame,
			.kind = MainBackgroundKind::type_a,
			.phase = MainBackgroundPhase::steady,
		});
		advance_reference_steady(
			motion, frame, magnitude, vector_latch
		);
		if(frame == 1024U) {
			expect_motion_dot(
				raster,
				motion,
				"1024 moves with length 64 before latching length 65"
			);
		} else if(frame == 1025U) {
			expect_motion_dot(
				raster,
				motion,
				"1025 consumes the length-65 vector latched at 1024"
			);
		}
	}
	expect(presenter.ready(), "type-A enters steady callback after frame 129");

	const auto vector_64 = th03::exact::vector2(0x10U, 64);
	magnitude = 0x40U;
	vector_latch = false;
	motion.velocity_x_q12_4 = vector_64.x;
	motion.velocity_y_q12_4 = vector_64.y;
	clear_visible(raster);
	presenter.main_background_frame({
		.round_id = 1U,
		.round_or_result_frame = 0U,
		.kind = MainBackgroundKind::type_a,
		.phase = MainBackgroundPhase::steady,
	});
	advance_reference_steady(motion, 0U, magnitude, vector_latch);
	expect(presenter.ready(), "next round needs no background reinitialization");
	expect_motion_dot(
		raster,
		motion,
		"round_id change preserves position and resets velocity to length 64"
	);
	for(std::uint16_t frame = 1U; frame < 15U; ++frame) {
		presenter.main_background_frame({
			.round_id = 1U,
			.round_or_result_frame = frame,
			.kind = MainBackgroundKind::type_a,
			.phase = MainBackgroundPhase::steady,
		});
		advance_reference_steady(motion, frame, magnitude, vector_latch);
	}
	clear_visible(raster);
	presenter.main_background_frame({
		.round_id = 1U,
		.round_or_result_frame = 15U,
		.kind = MainBackgroundKind::type_a,
		.phase = MainBackgroundPhase::steady,
	});
	advance_reference_steady(motion, 15U, magnitude, vector_latch);
	expect_motion_dot(
		raster,
		motion,
		"repeat velocity reset remains distinguishable from prior length 65"
	);
	presenter.main_background_random_read({0U});
	expect(
		presenter.status() == MainBackgroundStatus::invalid_observation,
		"steady repeat has no synthetic random-read stream"
	);
}

void test_complete_steady_schedule()
{
	Raster raster;
	Sprite16Staging staging;
	const auto layout = synthetic_layout();
	commit_sources(raster, staging, layout);
	MainBackgroundPresenter presenter(raster, staging, layout);
	initialize(
		presenter,
		MainBackgroundKind::type_a,
		[](std::size_t slot) noexcept {
			return (slot < 24U)
				? Placement{100U * 16U, 100U * 16U, 0U}
				: Placement{400U * 16U, 100U * 16U, 0U};
		}
	);
	for(std::uint16_t frame = 0U; frame < 130U; ++frame) {
		initial_frame(presenter, MainBackgroundKind::type_a, frame);
	}

	Motion motion{
		.x_q12_4 = 100 * 16,
		.y_q12_4 = 230 * 16,
		.velocity_x_q12_4 = 0,
		.velocity_y_q12_4 = 16,
	};
	auto magnitude = std::uint8_t{0x40U};
	const auto present_steady = [&presenter](std::uint16_t frame) noexcept {
		presenter.main_background_frame({
			.round_id = 0U,
			.round_or_result_frame = frame,
			.kind = MainBackgroundKind::type_a,
			.phase = MainBackgroundPhase::steady,
		});
	};
	bool vector_latch = false;
	for(std::uint16_t frame = 130U; frame <= 4096U; ++frame) {
		const bool sample =
			frame == 1023U || frame == 1024U || frame == 1025U ||
			frame == 1272U || frame == 1279U || frame == 1280U ||
			frame == 2047U || frame == 2048U || frame == 2049U ||
			frame == 2303U || frame == 2304U || frame == 3071U ||
			frame == 3072U || frame == 3135U || frame == 3136U ||
			frame == 3137U || frame == 4063U || frame == 4064U ||
			frame == 4096U;
		if(sample) {
			clear_visible(raster);
		}
		present_steady(frame);
		advance_reference_steady(
			motion, frame, magnitude, vector_latch
		);
		if(sample) {
			expect_motion_dot(
				raster,
				motion,
				"steady schedule sample matches the contiguous source model"
			);
		}
		if(frame == 1279U) {
			expect(
				magnitude == 96U,
				"1024..1279 performs all 32 reachable increments"
			);
		} else if(frame == 2303U) {
			expect(
				magnitude == 32U,
				"2048..2303 performs all 64 reachable decrements"
			);
		} else if(frame == 4063U) {
			expect(
				magnitude == 64U,
				"3072..4063 ends at the reachable length 64"
			);
		}
	}
	expect(
		magnitude == 64U,
		"4064..4096 preserves the vector materialized at frame 4063"
	);
	expect(presenter.ready(), "complete steady schedule remains valid");
}

void test_accelerator_restore_preserves_target_round_motion()
{
	Raster raster;
	Sprite16Staging staging;
	const auto layout = synthetic_layout();
	commit_sources(raster, staging, layout);
	MainBackgroundPresenter presenter(raster, staging, layout);
	initialize(
		presenter,
		MainBackgroundKind::type_a,
		[](std::size_t) noexcept {
			return Placement{400U * 16U, 300U * 16U, 0U};
		}
	);
	for(std::uint16_t frame = 0U; frame < 130U; ++frame) {
		initial_frame(presenter, MainBackgroundKind::type_a, frame);
	}

	std::array<MainBackgroundRecordState, main_background_record_count> records{};
	for(auto &record : records) {
		record.x_q12_4 = 400 * 16;
		record.y_q12_4 = 300 * 16;
	}
	records[0U] = {
		.x_q12_4 = 100 * 16,
		.y_q12_4 = 200 * 16,
		.velocity_x_q12_4 = 0,
		.velocity_y_q12_4 = 16,
		.angle = 0x40U,
	};
	expect(
		presenter.accelerator_restore(records, 0x55U, 7U),
		"accelerator restores a complete typed background pool"
	);
	clear_visible(raster);
	presenter.main_background_frame({
		.round_id = 7U,
		.round_or_result_frame = 0U,
		.kind = MainBackgroundKind::type_a,
		.phase = MainBackgroundPhase::steady,
	});
	expect_pixel(
		raster, {100, 100}, 10U,
		"target round identity prevents a duplicate repeat velocity reset"
	);
	expect_pixel(
		raster, {100, 102}, 0U,
		"accelerator velocity is not replaced by the angle-derived reset vector"
	);

	auto invalid = records;
	invalid[0U].x_q12_4 = 200 * 16;
	invalid.back().sprite_variant = 2U;
	expect(
		!presenter.accelerator_restore(invalid, 0x40U, 7U),
		"accelerator rejects an invalid sprite variant"
	);
	clear_visible(raster);
	presenter.main_background_frame({
		.round_id = 7U,
		.round_or_result_frame = 1U,
		.kind = MainBackgroundKind::type_a,
		.phase = MainBackgroundPhase::steady,
	});
	expect_pixel(
		raster, {100, 101}, 10U,
		"invalid accelerator restore leaves the prior pool mutation-atomic"
	);
}

void test_sticky_validation_and_failure_atomicity()
{
	const auto layout = synthetic_layout();
	{
		Raster raster;
		Sprite16Staging staging;
		expect(raster.plot({9, 9}, 9U), "seed malformed-layout sentinel");
		const auto before = raster.display_state_hash();
		auto malformed = layout;
		malformed.type_a_variants[0U].width = 32U;
		MainBackgroundPresenter presenter(raster, staging, malformed);
		expect(
			presenter.status() == MainBackgroundStatus::invalid_layout,
			"constructor rejects a malformed typed rectangle"
		);
		presenter.main_background_initialize({MainBackgroundKind::type_a});
		presenter.main_background_random_read({0U});
		expect(
			raster.display_state_hash() == before,
			"malformed-layout rejection is mutation-atomic and sticky"
		);
	}
	{
		Raster raster;
		Sprite16Staging staging;
		MainBackgroundPresenter presenter(raster, staging, layout);
		initialize(
			presenter,
			MainBackgroundKind::type_a,
			[](std::size_t) noexcept {
				return Placement{100U * 16U, 100U * 16U, 0U};
			}
		);
		const auto before = raster.display_state_hash();
		presenter.main_background_frame({
			.round_id = 0U,
			.round_or_result_frame = 0U,
			.kind = MainBackgroundKind::type_a,
			.phase = MainBackgroundPhase::initial,
		});
		expect(
			presenter.status() == MainBackgroundStatus::staging_not_committed,
			"drawing requires a committed Sprite16 staging surface"
		);
		expect(
			raster.display_state_hash() == before,
			"uncommitted-staging failure precedes particle mutation"
		);
	}
	{
		Raster raster;
		Sprite16Staging staging;
		commit_sources(raster, staging, layout);
		MainBackgroundPresenter presenter(raster, staging, layout);
		presenter.main_background_initialize({MainBackgroundKind::type_a});
		presenter.main_background_random_read({0U});
		const auto before = raster.display_state_hash();
		presenter.main_background_frame({
			.round_id = 0U,
			.round_or_result_frame = 0U,
			.kind = MainBackgroundKind::type_a,
			.phase = MainBackgroundPhase::initial,
		});
		expect(
			presenter.status() == MainBackgroundStatus::invalid_observation,
			"frame callback rejects an incomplete grouped initializer"
		);
		expect(
			raster.display_state_hash() == before,
			"incomplete initialization failure draws nothing"
		);
	}
	{
		Raster raster;
		Sprite16Staging staging;
		commit_sources(raster, staging, layout);
		MainBackgroundPresenter presenter(raster, staging, layout);
		presenter.main_background_initialize({MainBackgroundKind::type_b});
		presenter.main_background_random_read({0x8000U});
		expect(
			presenter.status() == MainBackgroundStatus::invalid_observation,
			"presenter rejects values outside IRAND's 15-bit result range"
		);
	}
	{
		Raster raster;
		Sprite16Staging staging;
		commit_sources(raster, staging, layout);
		MainBackgroundPresenter presenter(raster, staging, layout);
		initialize(
			presenter,
			MainBackgroundKind::type_a,
			[](std::size_t) noexcept {
				return Placement{100U * 16U, 100U * 16U, 0U};
			}
		);
		const auto before = raster.display_state_hash();
		presenter.main_background_frame({
			.round_id = 0U,
			.round_or_result_frame = 0U,
			.kind = MainBackgroundKind::type_a,
			.phase = MainBackgroundPhase::steady,
		});
		expect(
			presenter.status() == MainBackgroundStatus::invalid_observation,
			"steady callback cannot precede all 130 type-A initial callbacks"
		);
		expect(
			raster.display_state_hash() == before,
			"wrong-phase failure is mutation-atomic"
		);
	}
}

} // namespace

int main()
{
	static_assert(sizeof(MainBackgroundPresenter) < 1024U);
	test_authentic_layout();
	test_type_a_grouping_immediate_order_and_wrap();
	test_type_b_read_boundary_and_animation();
	test_type_a_initial_clip_then_steady_noclip();
	test_steady_order_latch_schedule_and_repeat_reset();
	test_complete_steady_schedule();
	test_accelerator_restore_preserves_target_round_motion();
	test_sticky_validation_and_failure_atomicity();

	if(failures != 0U) {
		std::fprintf(
			stderr,
			"%zu of %zu main-background checks failed\n",
			failures,
			checks
		);
		return 1;
	}
	std::printf("All %zu main-background checks passed\n", checks);
	return 0;
}
