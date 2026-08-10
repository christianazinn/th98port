#include "th03/present/pi.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace th03::present {
namespace {

constexpr std::uint16_t pixels_per_byte = 8U;

constexpr std::array<std::array<std::uint16_t, 4U>, 4U> cutscene_masks{{
	{{0x0000U, 0x1111U, 0x0000U, 0x4444U}},
	{{0x8888U, 0x1111U, 0x2222U, 0x4444U}},
	{{0xAAAAU, 0x5555U, 0xAAAAU, 0x5555U}},
	{{0xEEEEU, 0x7777U, 0xBBBBU, 0xDDDDU}},
}};

[[nodiscard]] bool valid_image(const assets::PiImage &image) noexcept {
	const auto width = static_cast<std::size_t>(image.image.width);
	const auto height = static_cast<std::size_t>(image.image.height);
	if(width == 0U || height == 0U ||
		width > (std::numeric_limits<std::size_t>::max() / height) ||
		image.image.indices.size() != (width * height)) {
		return false;
	}
	for(const auto color : image.image.indices) {
		if(color > 0x0FU) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool valid_cutscene_image(
	const assets::PiImage &image
) noexcept {
	return valid_image(image) && image.image.width == graphics_width &&
		image.image.height == graphics_height;
}

[[nodiscard]] std::optional<std::int32_t> align_left_8(
	std::int64_t left
) noexcept {
	auto remainder = left % pixels_per_byte;
	if(remainder < 0) {
		remainder += pixels_per_byte;
	}
	const auto aligned = left - remainder;
	if(aligned < std::numeric_limits<std::int32_t>::min() ||
		aligned > std::numeric_limits<std::int32_t>::max()) {
		return std::nullopt;
	}
	return static_cast<std::int32_t>(aligned);
}

[[nodiscard]] std::optional<std::size_t> quarter_index(
	CutscenePiQuarter quarter
) noexcept {
	switch(quarter) {
	case CutscenePiQuarter::top_left:
		return 0U;
	case CutscenePiQuarter::top_right:
		return 1U;
	case CutscenePiQuarter::bottom_left:
		return 2U;
	case CutscenePiQuarter::bottom_right:
		return 3U;
	}
	return std::nullopt;
}

[[nodiscard]] std::optional<std::size_t> mask_step_index(
	CutscenePiMaskStep step
) noexcept {
	switch(step) {
	case CutscenePiMaskStep::first:
		return 0U;
	case CutscenePiMaskStep::second:
		return 1U;
	case CutscenePiMaskStep::third:
		return 2U;
	case CutscenePiMaskStep::fourth:
		return 3U;
	}
	return std::nullopt;
}

[[nodiscard]] std::size_t cutscene_source_offset(
	std::size_t quarter,
	std::size_t x,
	std::size_t y
) noexcept {
	const auto source_x = ((quarter & 1U) != 0U) ?
		static_cast<std::size_t>(cutscene_pi_width) : 0U;
	const auto source_y = ((quarter & 2U) != 0U) ?
		static_cast<std::size_t>(cutscene_pi_height) : 0U;
	return ((source_y + y) * graphics_width) + source_x + x;
}

[[nodiscard]] bool mask_bit(std::uint16_t pattern, std::size_t x) noexcept {
	const auto shift = static_cast<unsigned int>(15U - (x & 15U));
	return ((pattern >> shift) & 1U) != 0U;
}

} // namespace

bool pi_put_8(
	Raster &raster,
	std::int32_t left,
	std::int32_t top,
	const assets::PiImage &image
) {
	if(!valid_image(image) || top < 0 || top >= graphics_height) {
		return false;
	}
	const auto aligned_left = align_left_8(left);
	if(!aligned_left) {
		return false;
	}
	const auto put_width = static_cast<std::uint16_t>(
		image.image.width - (image.image.width % pixels_per_byte)
	);
	if(put_width == 0U) {
		return true;
	}

	std::int64_t destination_y = top;
	for(std::size_t row = 0U; row < image.image.height; ++row) {
		const auto source_offset = row * image.image.width;
		const IndexedSource source{
			std::span<const Index4>(image.image.indices).subspan(source_offset),
			put_width,
			1U,
			image.image.width,
		};
		if(!raster.blit(
			source,
			{*aligned_left, static_cast<std::int32_t>(destination_y)}
		)) {
			return false;
		}
		++destination_y;
		if(destination_y >= graphics_height) {
			destination_y -= graphics_height;
		}
	}
	return true;
}

bool pi_cutscene_quarter_put_8(
	Raster &raster,
	const assets::PiImage &image,
	CutscenePiQuarter quarter
) {
	const auto selected_quarter = quarter_index(quarter);
	if(!selected_quarter || !valid_cutscene_image(image)) {
		return false;
	}
	const auto source_offset = cutscene_source_offset(
		*selected_quarter, 0U, 0U
	);
	const IndexedSource source{
		std::span<const Index4>(image.image.indices).subspan(source_offset),
		cutscene_pi_width,
		cutscene_pi_height,
		graphics_width,
	};
	return raster.blit(source, {cutscene_pi_left, cutscene_pi_top});
}

bool pi_cutscene_quarter_put_masked(
	Raster &raster,
	const assets::PiImage &image,
	CutscenePiQuarter quarter,
	CutscenePiMaskStep step
) {
	const auto selected_quarter = quarter_index(quarter);
	const auto selected_step = mask_step_index(step);
	if(!selected_quarter || !selected_step || !valid_cutscene_image(image)) {
		return false;
	}

	for(std::size_t y = 0U; y < cutscene_pi_height; ++y) {
		const auto pattern = cutscene_masks[*selected_step][y & 3U];
		for(std::size_t x = 0U; x < cutscene_pi_width; ++x) {
			if(!mask_bit(pattern, x)) {
				continue;
			}
			const auto color = image.image.indices[
				cutscene_source_offset(*selected_quarter, x, y)
			];
			const bool plotted = raster.plot(
				{
					cutscene_pi_left + static_cast<std::int32_t>(x),
					cutscene_pi_top + static_cast<std::int32_t>(y),
				},
				color
			);
			if(!plotted) {
				return false;
			}
		}
	}
	return true;
}

} // namespace th03::present
