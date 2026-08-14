#include "th03/app/startup_sequence.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <new>
#include <span>
#include <utility>
#include <variant>
#include <vector>

#include "th03/assets/decode.hpp"
#include "th03/core/numeric.hpp"
#include "th03/exact/trig.hpp"
#include "th03/present/raster.hpp"
#include "th03/present/superimpose.hpp"
#include "th03/present/vectors.hpp"

namespace th03::app {
namespace {

using present::ComposedPixel;
using present::ComposedPixelLayer;
using present::Index4;
using present::Page;

constexpr std::uintmax_t logo_profile_bytes = 8'272U;
constexpr std::uint64_t logo_profile_fnv1a64 = 0x6040576598F4509EULL;
constexpr std::uint16_t rendered_frame_count = 231U;
constexpr std::uint8_t waits_per_rendered_frame = 2U;
constexpr std::size_t circle_count = 4U;
constexpr std::size_t star_count = 50U;
constexpr present::GrcgClipRect clip{96, 100, 543, 299};
constexpr std::array<std::uint8_t, circle_count> circle_colors{4U, 3U, 2U, 1U};

// Public-domain master.lib SinTable7 used by super_wave_put().
constexpr std::array<std::int8_t, 256U> sine7{{
	0,3,6,9,12,15,18,21,24,28,31,34,37,40,43,46,
	48,51,54,57,60,63,65,68,71,73,76,78,81,83,85,88,
	90,92,94,96,98,100,102,104,106,108,109,111,112,114,115,117,
	118,119,120,121,122,123,124,124,125,126,126,127,127,127,127,127,
	127,127,127,127,127,127,126,126,125,124,124,123,122,121,120,119,
	118,117,115,114,112,111,109,108,106,104,102,100,98,96,94,92,
	90,88,85,83,81,78,76,73,71,68,65,63,60,57,54,51,
	48,46,43,40,37,34,31,28,24,21,18,15,12,9,6,3,
	0,-3,-6,-9,-12,-15,-18,-21,-24,-28,-31,-34,-37,-40,-43,-46,
	-48,-51,-54,-57,-60,-63,-65,-68,-71,-73,-76,-78,-81,-83,-85,-88,
	-90,-92,-94,-96,-98,-100,-102,-104,-106,-108,-109,-111,-112,-114,-115,-117,
	-118,-119,-120,-121,-122,-123,-124,-124,-125,-126,-126,-127,-127,-127,-127,-127,
	-127,-127,-127,-127,-127,-127,-126,-126,-125,-124,-124,-123,-122,-121,-120,-119,
	-118,-117,-115,-114,-112,-111,-109,-108,-106,-104,-102,-100,-98,-96,-94,-92,
	-90,-88,-85,-83,-81,-78,-76,-73,-71,-68,-65,-63,-60,-57,-54,-51,
	-48,-46,-43,-40,-37,-34,-31,-28,-24,-21,-18,-15,-12,-9,-6,-3,
}};

struct MovingPoint final {
	std::int16_t x = 0;
	std::int16_t y = 0;
};

[[nodiscard]] constexpr Page other_page(Page page) noexcept
{
	return page == Page::zero ? Page::one : Page::zero;
}

[[nodiscard]] std::uint64_t fnv1a64(std::span<const std::byte> bytes) noexcept
{
	std::uint64_t hash = 14695981039346656037ULL;
	for(const auto value : bytes) {
		hash ^= std::to_integer<std::uint8_t>(value);
		hash *= 1099511628211ULL;
	}
	return hash;
}

[[nodiscard]] std::expected<
	std::vector<std::byte>,
	StartupSequenceError
> load_logo_bytes(const std::filesystem::path &path)
{
	if(path.empty()) {
		return std::unexpected(StartupSequenceError{
			.code = StartupSequenceErrorCode::invalid_path,
		});
	}
	std::error_code system{};
	const auto file_status = std::filesystem::status(path, system);
	if(system) {
		return std::unexpected(StartupSequenceError{
			.code = system == std::errc::no_such_file_or_directory ?
				StartupSequenceErrorCode::file_missing :
				StartupSequenceErrorCode::file_status_failed,
		});
	}
	if(!std::filesystem::exists(file_status)) {
		return std::unexpected(StartupSequenceError{
			.code = StartupSequenceErrorCode::file_missing,
		});
	}
	if(!std::filesystem::is_regular_file(file_status)) {
		return std::unexpected(StartupSequenceError{
			.code = StartupSequenceErrorCode::file_not_regular,
		});
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system) {
		return std::unexpected(StartupSequenceError{
			.code = StartupSequenceErrorCode::file_size_failed,
		});
	}
	if(size != logo_profile_bytes) {
		return std::unexpected(StartupSequenceError{
			.code = StartupSequenceErrorCode::file_size_mismatch,
			.observed_size = size,
		});
	}
	try {
		std::vector<std::byte> bytes(static_cast<std::size_t>(size));
		std::ifstream stream(path, std::ios::binary);
		if(!stream.is_open()) {
			return std::unexpected(StartupSequenceError{
				.code = StartupSequenceErrorCode::file_open_failed,
			});
		}
		stream.read(
			reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
		if(!stream || stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
			return std::unexpected(StartupSequenceError{
				.code = StartupSequenceErrorCode::file_read_failed,
			});
		}
		if(fnv1a64(bytes) != logo_profile_fnv1a64) {
			return std::unexpected(StartupSequenceError{
				.code = StartupSequenceErrorCode::digest_mismatch,
			});
		}
		return bytes;
	} catch(const std::bad_alloc &) {
		return std::unexpected(StartupSequenceError{
			.code = StartupSequenceErrorCode::allocation_failed,
		});
	}
}

[[nodiscard]] bool valid_logo_profile(const assets::BfntSheet &logo) noexcept
{
	return logo.consumer == assets::BfntConsumerPolicy::superimpose &&
		logo.header.color_flags == 0x83U && logo.header.version == 0x16U &&
		logo.header.pixel_width == 64U && logo.header.pixel_height == 16U &&
		logo.header.first_code == 0U && logo.header.last_code == 15U &&
		logo.transparent_color == 0U && logo.palette.has_value() &&
		logo.glyphs.size() == 16U;
}

[[nodiscard]] Index4 glyph_pixel(
	const assets::BfntSheet &logo,
	std::uint16_t code,
	std::uint16_t x,
	std::uint16_t y
) noexcept
{
	if(code < logo.header.first_code || code > logo.header.last_code ||
		x >= logo.header.pixel_width || y >= logo.header.pixel_height) {
		return logo.transparent_color;
	}
	const auto &glyph = logo.glyphs[static_cast<std::size_t>(
		code - logo.header.first_code
	)];
	if(!std::holds_alternative<assets::PlanarBitmap4>(glyph.pixels)) {
		return logo.transparent_color;
	}
	const auto &bitmap = std::get<assets::PlanarBitmap4>(glyph.pixels);
	const auto stride = static_cast<std::size_t>(logo.header.pixel_width / 8U);
	const auto offset = (static_cast<std::size_t>(y) * stride) + (x / 8U);
	const auto mask = static_cast<std::uint8_t>(0x80U >> (x & 7U));
	Index4 result = 0U;
	for(std::size_t plane = 0U; plane < bitmap.color_planes.size(); ++plane) {
		if(offset >= bitmap.color_planes[plane].size()) {
			return logo.transparent_color;
		}
		if((bitmap.color_planes[plane][offset] & mask) != 0U) {
			result = static_cast<Index4>(result | (1U << plane));
		}
	}
	return result;
}

[[nodiscard]] bool circlefill(
	present::Raster &raster,
	MovingPoint center,
	std::int16_t radius,
	Index4 color
) noexcept
{
	if(radius == 0) {
		return raster.plot({center.x, center.y}, color);
	}
	std::int16_t wx = radius;
	std::int16_t wy = 0;
	std::int16_t accumulator = radius;
	while(wx >= wy) {
		if(!present::grcg_hline(
			raster, center.x - wx, center.x + wx, center.y - wy, clip, color
		) || !present::grcg_hline(
			raster, center.x - wx, center.x + wx, center.y + wy, clip, color
		)) {
			return false;
		}
		accumulator = static_cast<std::int16_t>(
			accumulator - ((wy * 2) + 1)
		);
		if(accumulator < 0) {
			if(!present::grcg_hline(
				raster, center.x - wy, center.x + wy, center.y - wx, clip, color
			) || !present::grcg_hline(
				raster, center.x - wy, center.x + wy, center.y + wx, clip, color
			)) {
				return false;
			}
			--wx;
			accumulator = static_cast<std::int16_t>(accumulator + (wx * 2));
		}
		++wy;
	}
	return true;
}

} // namespace

struct StartupSequence::Impl final {
	assets::BfntSheet logo{};
	present::PreparedSuperimposeSheet prepared_logo;
	present::Raster raster{};
	std::vector<ComposedPixel> pixels = std::vector<ComposedPixel>(
		present::graphics_page_pixels,
		ComposedPixel{ComposedPixelLayer::graphics, 0U}
	);
	std::array<MovingPoint, circle_count> circle_positions{{
		{128, 320}, {256, 240}, {384, 160}, {512, 80},
	}};
	std::array<MovingPoint, circle_count> circle_speeds{{
		{-8, 8}, {-8, 8}, {-8, 8}, {-8, 8},
	}};
	std::array<MovingPoint, star_count> star_positions{};
	std::array<std::uint8_t, star_count> star_speeds{};
	std::uint32_t random_seed = 1U;
	std::uint16_t frame = 0U;
	std::int16_t tone = 0;
	std::uint16_t logo_pattern = 0U;
	std::int16_t wave_length = 23;
	std::uint8_t wave_phase = 0U;
	std::int8_t wave_amplitude = 0;
	std::uint8_t star_angle = 0x40U;
	std::uint8_t wait_count = 0U;
	Page back_page = Page::zero;
	Page front_page = Page::one;
	StartupSlot slot = StartupSlot::zun_soft;
	StartupSequenceStatus status = StartupSequenceStatus::running;
	bool draw_failed = false;

	Impl(
		assets::BfntSheet source_logo,
		present::PreparedSuperimposeSheet source_prepared
	) :
		logo(std::move(source_logo)),
		prepared_logo(std::move(source_prepared))
	{
		(void)raster.set_access_page(Page::one);
		(void)raster.clear(
			{0, 0, present::graphics_width, present::graphics_height}, 0U
		);
		(void)raster.set_access_page(Page::zero);
		(void)raster.clear(
			{0, 0, present::graphics_width, present::graphics_height}, 0U
		);
		(void)raster.set_shown_page(Page::zero);
		for(std::size_t index = 0U; index < logo.palette->colors.size(); ++index) {
			(void)raster.set_pending_palette_entry(
				static_cast<std::uint8_t>(index), logo.palette->colors[index]
			);
		}
		raster.set_palette_tone(0);
		raster.apply_pending_palette();
		for(std::size_t index = 0U; index < star_count; ++index) {
			star_positions[index].x = static_cast<std::int16_t>(irand() % 640U);
			star_positions[index].y = static_cast<std::int16_t>(irand() % 400U);
			star_speeds[index] = static_cast<std::uint8_t>((irand() % 32U) + 6U);
		}
		compose();
	}

	[[nodiscard]] std::uint16_t irand() noexcept
	{
		random_seed = (random_seed * 0x015A4E35U) + 1U;
		return static_cast<std::uint16_t>((random_seed >> 16U) & 0x7FFFU);
	}

	void compose() noexcept
	{
		const auto graphics = raster.shown_graphics();
		for(std::size_t index = 0U; index < pixels.size(); ++index) {
			pixels[index] = {
				ComposedPixelLayer::graphics,
				graphics[index],
			};
		}
	}

	[[nodiscard]] bool draw_logo_normal() noexcept
	{
		return present::superimpose_put(
			raster, prepared_logo, {256, 192}, logo_pattern
		) && present::superimpose_put(
			raster, prepared_logo, {320, 192}, logo_pattern + 1U
		);
	}

	[[nodiscard]] bool draw_logo_wave(std::int32_t left, std::uint16_t code) noexcept
	{
		if(wave_length <= 0) {
			return false;
		}
		for(std::uint16_t y = 0U; y < logo.header.pixel_height; ++y) {
			const auto table_index = static_cast<std::uint8_t>(
				((static_cast<std::uint32_t>(y) << 8U) /
					static_cast<std::uint16_t>(wave_length)) + wave_phase
			);
			const auto product = static_cast<std::int32_t>(sine7[table_index]) *
				wave_amplitude;
			const auto offset = exact::arithmetic_shift_right(product, 7U);
			for(std::uint16_t x = 0U; x < logo.header.pixel_width; ++x) {
				const auto color = glyph_pixel(logo, code, x, y);
				if(color != logo.transparent_color && !raster.plot(
					{left + offset + x, 192 + y}, color
				)) {
					return false;
				}
			}
		}
		return true;
	}

	[[nodiscard]] bool draw_logo() noexcept
	{
		if(frame < 50U || frame >= 170U) {
			return true;
		}
		if(frame < 90U) {
			if(frame == 55U || frame == 60U || frame == 65U) {
				logo_pattern += 2U;
			}
			return draw_logo_normal();
		}
		if(frame < 110U) {
			const bool result = draw_logo_wave(256, logo_pattern) &&
				draw_logo_wave(320, logo_pattern + 1U);
			--wave_length;
			wave_phase = static_cast<std::uint8_t>(wave_phase + 4U);
			wave_amplitude = static_cast<std::int8_t>(wave_amplitude + 4);
			return result;
		}
		if(frame < 130U) {
			if(frame == 110U) {
				logo_pattern += 2U;
			}
			const bool result = draw_logo_wave(256, logo_pattern) &&
				draw_logo_wave(320, logo_pattern + 1U);
			++wave_length;
			wave_phase = static_cast<std::uint8_t>(wave_phase + 4U);
			wave_amplitude = static_cast<std::int8_t>(wave_amplitude - 4);
			return result;
		}
		if(frame == 155U || frame == 160U || frame == 165U) {
			logo_pattern += 2U;
		}
		return draw_logo_normal();
	}

	[[nodiscard]] bool render_source_frame() noexcept
	{
		if(frame > 180U) {
			if(tone <= 0) {
				return false;
			}
			tone = static_cast<std::int16_t>(tone - 2);
		} else if(tone < 100) {
			tone = static_cast<std::int16_t>(tone + 2);
		}
		raster.set_palette_tone(tone);
		raster.apply_pending_palette();
		if(!raster.clear({96, 100, 448U, 200U}, 0U)) {
			draw_failed = true;
			return false;
		}
		for(std::size_t index = 0U; index < star_count; ++index) {
			const auto point = star_positions[index];
			if(point.x >= clip.left && point.x <= clip.right &&
				point.y >= clip.top && point.y <= clip.bottom &&
				!raster.plot({point.x, point.y}, 5U)) {
				draw_failed = true;
				return false;
			}
			const auto vector = exact::vector2(star_angle, star_speeds[index]);
			auto &position = star_positions[index];
			position.x = static_cast<std::int16_t>(position.x + vector.x);
			position.y = static_cast<std::int16_t>(position.y + vector.y);
			if(position.x < 0) position.x = static_cast<std::int16_t>(position.x + 640);
			else if(position.x >= 640) position.x = static_cast<std::int16_t>(position.x - 640);
			if(position.y < 0) position.y = static_cast<std::int16_t>(position.y + 400);
			else if(position.y >= 400) position.y = static_cast<std::int16_t>(position.y - 400);
		}
		++star_angle;
		for(std::size_t reverse = circle_count; reverse > 0U; --reverse) {
			const auto index = reverse - 1U;
			if(!circlefill(
				raster, circle_positions[index], 96, circle_colors[index]
			)) {
				draw_failed = true;
				return false;
			}
			auto &position = circle_positions[index];
			auto &speed = circle_speeds[index];
			position.x = static_cast<std::int16_t>(position.x + speed.x);
			position.y = static_cast<std::int16_t>(position.y + speed.y);
			if(position.x <= 32 || position.x > 607) speed.x = -speed.x;
			if(position.y <= 32 || position.y > 367) speed.y = -speed.y;
		}
		if(!draw_logo()) {
			draw_failed = true;
			return false;
		}
		return true;
	}

	void finish_logo() noexcept
	{
		(void)raster.set_access_page(Page::one);
		(void)raster.clear(
			{0, 0, present::graphics_width, present::graphics_height}, 0U
		);
		(void)raster.set_access_page(Page::zero);
		(void)raster.clear(
			{0, 0, present::graphics_width, present::graphics_height}, 0U
		);
		(void)raster.set_shown_page(Page::zero);
		back_page = Page::zero;
		front_page = Page::one;
		compose();
		// The reserved project-credit slot is deliberately empty today. Keeping
		// it as an explicit transition avoids coupling its future cadence or
		// assets to either the ZUN Soft scene or the product menu.
		slot = StartupSlot::project_credits;
		slot = StartupSlot::complete;
		status = StartupSequenceStatus::complete;
	}
};

std::expected<StartupSequence, StartupSequenceError>
StartupSequence::create_from_decoded_logo(assets::BfntSheet logo)
{
	if(!valid_logo_profile(logo)) {
		return std::unexpected(StartupSequenceError{
			.code = StartupSequenceErrorCode::asset_profile_mismatch,
		});
	}
	auto prepared = present::prepare_superimpose_sheet(logo);
	if(!prepared) {
		return std::unexpected(StartupSequenceError{
			.code = StartupSequenceErrorCode::asset_prepare_failed,
		});
	}
	try {
		return StartupSequence(std::make_unique<Impl>(
			std::move(logo), std::move(*prepared)
		));
	} catch(const std::bad_alloc &) {
		return std::unexpected(StartupSequenceError{
			.code = StartupSequenceErrorCode::allocation_failed,
		});
	}
}

std::expected<StartupSequence, StartupSequenceError>
StartupSequence::load(const std::filesystem::path &logo_path)
{
	auto bytes = load_logo_bytes(logo_path);
	if(!bytes) {
		return std::unexpected(bytes.error());
	}
	auto logo = assets::decode_bfnt(
		*bytes, assets::BfntConsumerPolicy::superimpose
	);
	if(!logo) {
		return std::unexpected(StartupSequenceError{
			.code = StartupSequenceErrorCode::asset_decode_failed,
		});
	}
	return create_from_decoded_logo(std::move(*logo));
}

StartupSequence::StartupSequence(std::unique_ptr<Impl> impl) noexcept :
	impl_(std::move(impl))
{
}

StartupSequence::~StartupSequence() = default;
StartupSequence::StartupSequence(StartupSequence &&) noexcept = default;
StartupSequence &StartupSequence::operator=(StartupSequence &&) noexcept = default;

StartupSequenceResult StartupSequence::consume(
	StartupSequenceInput input
) noexcept
{
	if(impl_->status != StartupSequenceStatus::running) {
		return {.status = impl_->status, .slot = impl_->slot};
	}
	if(input.app_quit_requested) {
		impl_->status = StartupSequenceStatus::quit_requested;
		return {.status = impl_->status, .slot = impl_->slot};
	}
	if(impl_->slot != StartupSlot::zun_soft) {
		impl_->slot = StartupSlot::complete;
		impl_->status = StartupSequenceStatus::complete;
		return {.status = impl_->status, .slot = impl_->slot};
	}
	if(impl_->wait_count == 0U) {
		if(!impl_->render_source_frame()) {
			if(impl_->draw_failed) {
				impl_->status = StartupSequenceStatus::failed;
				return {.status = impl_->status, .slot = impl_->slot};
			}
			impl_->finish_logo();
			return {
				.surface_changed = true,
				.status = impl_->status,
				.slot = impl_->slot,
			};
		}
		impl_->wait_count = 1U;
		return {.status = impl_->status, .slot = impl_->slot};
	}
	++impl_->wait_count;
	if(impl_->wait_count < waits_per_rendered_frame) {
		return {.status = impl_->status, .slot = impl_->slot};
	}
	impl_->wait_count = 0U;
	impl_->front_page = impl_->back_page;
	(void)impl_->raster.set_shown_page(impl_->front_page);
	impl_->back_page = other_page(impl_->back_page);
	(void)impl_->raster.set_access_page(impl_->back_page);
	impl_->compose();
	if(input.any_input) {
		impl_->finish_logo();
		return {
			.surface_changed = true,
			.status = impl_->status,
			.slot = impl_->slot,
		};
	}
	++impl_->frame;
	return {
		.surface_changed = true,
		.status = impl_->status,
		.slot = impl_->slot,
	};
}

present::SourceSurface StartupSequence::surface() const noexcept
{
	return {
		.pixels = std::span<
			const ComposedPixel,
			present::graphics_page_pixels
		>(impl_->pixels.data(), impl_->pixels.size()),
		.graphics_palette = impl_->raster.applied_palette(),
		.shown_page = impl_->raster.shown_page(),
		.raster_mode = impl_->raster.raster_mode(),
		.effective_scroll_y = impl_->raster.scroll_y(),
		.palette_tone = impl_->raster.palette_tone(),
		.host_overlay = false,
	};
}

StartupSequenceStatus StartupSequence::status() const noexcept
{
	return impl_->status;
}

StartupSlot StartupSequence::slot() const noexcept
{
	return impl_->slot;
}

std::uint16_t StartupSequence::source_frame() const noexcept
{
	return impl_->frame;
}

} // namespace th03::app
