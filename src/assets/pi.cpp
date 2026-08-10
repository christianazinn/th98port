#include "th03/assets/decode.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace th03::assets {
namespace {

constexpr std::uint8_t pi_comment_end = 0x1AU;
constexpr std::uint8_t pi_mode_palette_suppressed = 0x80U;
constexpr std::size_t pi_palette_size = 48U;
constexpr std::uint8_t pi_plane_count = 4U;
constexpr std::uint8_t no_previous_position = 0xFFU;

AssetError error(
	AssetErrorCode code,
	std::string message,
	std::size_t offset
)
{
	return AssetError{code, std::move(message), offset};
}

std::uint8_t u8(std::span<const std::byte> bytes, std::size_t offset)
{
	return std::to_integer<std::uint8_t>(bytes[offset]);
}

std::uint16_t be16(std::span<const std::byte> bytes, std::size_t offset)
{
	return static_cast<std::uint16_t>(
		(static_cast<std::uint16_t>(u8(bytes, offset)) << 8U) |
		static_cast<std::uint16_t>(u8(bytes, offset + 1U))
	);
}

class BitReader {
public:
	BitReader(std::span<const std::byte> bytes, std::size_t base_offset)
		: bytes_(bytes), base_offset_(base_offset)
	{
	}

	std::expected<std::uint8_t, AssetError> bit()
	{
		if((bit_offset_ / 8U) >= bytes_.size()) {
			return std::unexpected(error(
				AssetErrorCode::truncated,
				"PI compressed stream ends in the middle of a code",
				current_offset()
			));
		}
		const auto byte = u8(bytes_, bit_offset_ / 8U);
		const auto shift = static_cast<unsigned>(7U - (bit_offset_ % 8U));
		++bit_offset_;
		return static_cast<std::uint8_t>((byte >> shift) & 1U);
	}

	std::expected<std::uint32_t, AssetError> bits(unsigned count)
	{
		std::uint32_t value = 0U;
		for(unsigned index = 0U; index < count; ++index) {
			const auto next = bit();
			if(!next) {
				return std::unexpected(next.error());
			}
			value = static_cast<std::uint32_t>((value << 1U) | *next);
		}
		return value;
	}

	[[nodiscard]] std::size_t current_offset() const
	{
		return base_offset_ + (bit_offset_ / 8U);
	}

private:
	std::span<const std::byte> bytes_;
	std::size_t base_offset_;
	std::size_t bit_offset_ = 0U;
};

class ColorDecoder {
public:
	ColorDecoder()
	{
		for(std::size_t previous = 0U; previous < tables_.size(); ++previous) {
			for(std::size_t index = 0U; index < tables_[previous].size(); ++index) {
				tables_[previous][index] = static_cast<std::uint8_t>(
					(previous + index + 1U) & 0x0FU
				);
			}
		}
	}

	std::expected<std::uint8_t, AssetError> read(
		BitReader &bits,
		std::uint8_t previous
	)
	{
		std::uint8_t base = 0U;
		unsigned suffix_width = 1U;
		const auto first = bits.bit();
		if(!first) {
			return std::unexpected(first.error());
		}
		if(*first == 0U) {
			base = 2U;
			const auto second = bits.bit();
			if(!second) {
				return std::unexpected(second.error());
			}
			if(*second != 0U) {
				base = 4U;
				suffix_width = 2U;
				const auto third = bits.bit();
				if(!third) {
					return std::unexpected(third.error());
				}
				if(*third != 0U) {
					base = 8U;
					suffix_width = 3U;
				}
			}
		}

		const auto suffix = bits.bits(suffix_width);
		if(!suffix) {
			return std::unexpected(suffix.error());
		}
		const auto table_index = static_cast<std::uint8_t>(
			(base + static_cast<std::uint8_t>(*suffix)) ^ 0x0FU
		);
		auto &table = tables_[previous & 0x0FU];
		const auto color = table[table_index];
		std::move(
			table.begin() + static_cast<std::ptrdiff_t>(table_index + 1U),
			table.end(),
			table.begin() + static_cast<std::ptrdiff_t>(table_index)
		);
		table.back() = color;
		return color;
	}

private:
	std::array<std::array<std::uint8_t, 16U>, 16U> tables_{};
};

std::expected<std::uint8_t, AssetError> read_position(BitReader &bits)
{
	const auto prefix = bits.bits(2U);
	if(!prefix) {
		return std::unexpected(prefix.error());
	}
	if(*prefix != 3U) {
		return static_cast<std::uint8_t>(*prefix);
	}
	const auto suffix = bits.bit();
	if(!suffix) {
		return std::unexpected(suffix.error());
	}
	return static_cast<std::uint8_t>(3U + *suffix);
}

std::expected<std::size_t, AssetError> read_run_length(BitReader &bits)
{
	unsigned prefix_ones = 0U;
	while(true) {
		const auto next = bits.bit();
		if(!next) {
			return std::unexpected(next.error());
		}
		if(*next == 0U) {
			break;
		}
		++prefix_ones;
		if(prefix_ones >= std::numeric_limits<std::size_t>::digits) {
			return std::unexpected(error(
				AssetErrorCode::malformed_stream,
				"PI run-length prefix is too large",
				bits.current_offset()
			));
		}
	}

	std::size_t suffix = 0U;
	for(unsigned index = 0U; index < prefix_ones; ++index) {
		const auto next = bits.bit();
		if(!next) {
			return std::unexpected(next.error());
		}
		suffix = (suffix << 1U) | static_cast<std::size_t>(*next);
	}
	return (std::size_t{1U} << prefix_ones) | suffix;
}

std::expected<void, AssetError> append_reference_run(
	std::vector<std::uint8_t> &packed,
	std::uint8_t position,
	std::size_t length,
	std::size_t width,
	std::size_t target_size,
	std::size_t error_offset
)
{
	if(length > (target_size - packed.size())) {
		return std::unexpected(error(
			AssetErrorCode::output_overflow,
			"PI run extends past the declared image dimensions",
			error_offset
		));
	}

	if(position == 0U) {
		const auto previous = packed.back();
		const auto high = static_cast<std::uint8_t>(previous >> 4U);
		const auto low = static_cast<std::uint8_t>(previous & 0x0FU);
		const auto distance = static_cast<std::size_t>(
			(high == low) ? 1U : 2U
		);
		if(packed.size() < distance) {
			return std::unexpected(error(
				AssetErrorCode::malformed_stream,
				"PI left-reference precedes the predictor rows",
				error_offset
			));
		}
		for(std::size_t index = 0U; index < length; ++index) {
			packed.push_back(packed[packed.size() - distance]);
		}
		return {};
	}

	std::size_t pixel_distance = width;
	if(position == 2U) {
		if(width > (std::numeric_limits<std::size_t>::max() / 2U)) {
			return std::unexpected(error(
				AssetErrorCode::malformed_stream,
				"PI two-row reference distance overflows",
				error_offset
			));
		}
		pixel_distance = width * 2U;
	} else if(position == 3U) {
		pixel_distance = width - 1U;
	} else if(position == 4U) {
		pixel_distance = width + 1U;
	}

	for(std::size_t index = 0U; index < length; ++index) {
		const auto current_pixel = packed.size() * 2U;
		if(
			pixel_distance > current_pixel ||
			(current_pixel - pixel_distance + 1U) >= current_pixel
		) {
			return std::unexpected(error(
				AssetErrorCode::malformed_stream,
				"PI position code references unavailable predictor pixels",
				error_offset
			));
		}
		const auto source_pixel = current_pixel - pixel_distance;
		const auto source_byte = packed[source_pixel / 2U];
		const auto next_source_byte = packed[(source_pixel + 1U) / 2U];
		const auto first = static_cast<std::uint8_t>(
			((source_pixel & 1U) == 0U) ?
			(source_byte >> 4U) : (source_byte & 0x0FU)
		);
		const auto second = static_cast<std::uint8_t>(
			(((source_pixel + 1U) & 1U) == 0U) ?
			(next_source_byte >> 4U) : (next_source_byte & 0x0FU)
		);
		packed.push_back(static_cast<std::uint8_t>((first << 4U) | second));
	}
	return {};
}

Palette16Rgb4 decode_pi_palette(std::span<const std::byte> bytes)
{
	Palette16Rgb4 palette{};
	for(std::size_t color = 0U; color < palette.colors.size(); ++color) {
		const auto offset = color * 3U;
		palette.colors[color] = Rgb4{
			static_cast<std::uint8_t>(u8(bytes, offset) >> 4U),
			static_cast<std::uint8_t>(u8(bytes, offset + 1U) >> 4U),
			static_cast<std::uint8_t>(u8(bytes, offset + 2U) >> 4U),
		};
	}
	return palette;
}

} // namespace

std::expected<PiImage, AssetError> decode_pi(
	std::span<const std::byte> bytes
)
{
	if(bytes.size() < 2U) {
		return std::unexpected(error(
			AssetErrorCode::truncated,
			"PI data is shorter than its magic",
			bytes.size()
		));
	}
	if(u8(bytes, 0U) != static_cast<std::uint8_t>('P') ||
		u8(bytes, 1U) != static_cast<std::uint8_t>('i')) {
		return std::unexpected(error(
			AssetErrorCode::bad_magic,
			"PI magic is not 'Pi'",
			0U
		));
	}

	PiImage result{};
	std::size_t cursor = 2U;
	const auto comment_begin = cursor;
	while(cursor < bytes.size() && u8(bytes, cursor) != pi_comment_end) {
		++cursor;
	}
	if(cursor == bytes.size()) {
		return std::unexpected(error(
			AssetErrorCode::truncated,
			"PI comment has no 0x1A terminator",
			cursor
		));
	}
	result.comment.assign(
		bytes.begin() + static_cast<std::ptrdiff_t>(comment_begin),
		bytes.begin() + static_cast<std::ptrdiff_t>(cursor)
	);
	++cursor;
	while(cursor < bytes.size() && u8(bytes, cursor) != 0U) {
		++cursor;
	}
	if(cursor == bytes.size()) {
		return std::unexpected(error(
			AssetErrorCode::truncated,
			"PI dummy header has no zero terminator",
			cursor
		));
	}
	++cursor;

	// Mode through the big-endian machine-extension length.
	constexpr std::size_t fixed_header_prefix_size = 10U;
	if((bytes.size() - cursor) < fixed_header_prefix_size) {
		return std::unexpected(error(
			AssetErrorCode::truncated,
			"PI fixed header is truncated",
			cursor
		));
	}
	result.mode = u8(bytes, cursor++);
	result.aspect_n = u8(bytes, cursor++);
	result.aspect_m = u8(bytes, cursor++);
	if(result.aspect_n != 0U || result.aspect_m != 0U) {
		return std::unexpected(error(
			AssetErrorCode::unsupported_layout,
			"PI aspect ratio is not the source-supported 0:0 value",
			cursor - 2U
		));
	}
	result.plane_count = u8(bytes, cursor++);
	if(result.plane_count != pi_plane_count) {
		return std::unexpected(error(
			AssetErrorCode::unsupported_layout,
			"PI image is not four-plane indexed color",
			cursor - 1U
		));
	}
	for(auto &machine_byte : result.machine) {
		machine_byte = u8(bytes, cursor++);
	}
	const auto extension_size = static_cast<std::size_t>(be16(bytes, cursor));
	cursor += 2U;
	if((bytes.size() - cursor) < (extension_size + 4U)) {
		return std::unexpected(error(
			AssetErrorCode::truncated,
			"PI machine extension or dimensions are truncated",
			cursor
		));
	}
	result.machine_extension.assign(
		bytes.begin() + static_cast<std::ptrdiff_t>(cursor),
		bytes.begin() + static_cast<std::ptrdiff_t>(cursor + extension_size)
	);
	cursor += extension_size;
	const auto width = static_cast<std::size_t>(be16(bytes, cursor));
	cursor += 2U;
	const auto height = static_cast<std::size_t>(be16(bytes, cursor));
	cursor += 2U;
	if(width == 0U || height == 0U || (width & 1U) != 0U) {
		return std::unexpected(error(
			AssetErrorCode::unsupported_layout,
			"PI dimensions must be nonzero with an even pixel width",
			cursor - 4U
		));
	}
	if(width > (std::numeric_limits<std::size_t>::max() / height)) {
		return std::unexpected(error(
			AssetErrorCode::invalid_value,
			"PI pixel count overflows the host size type",
			cursor - 4U
		));
	}
	const auto pixel_count = width * height;
	const auto image_byte_count = pixel_count / 2U;
	const auto predictor_byte_count = width;
	if(predictor_byte_count > (
		std::numeric_limits<std::size_t>::max() - image_byte_count
	)) {
		return std::unexpected(error(
			AssetErrorCode::invalid_value,
			"PI decoded buffer size overflows the host size type",
			cursor - 4U
		));
	}

	if((result.mode & pi_mode_palette_suppressed) == 0U) {
		if((bytes.size() - cursor) < pi_palette_size) {
			return std::unexpected(error(
				AssetErrorCode::truncated,
				"PI inline palette is truncated",
				cursor
			));
		}
		result.palette = decode_pi_palette(bytes.subspan(cursor, pi_palette_size));
		cursor += pi_palette_size;
	}
	if(cursor == bytes.size()) {
		return std::unexpected(error(
			AssetErrorCode::truncated,
			"PI image has no compressed bitstream",
			cursor
		));
	}

	BitReader bits(bytes.subspan(cursor), cursor);
	ColorDecoder colors{};
	const auto first = colors.read(bits, 0U);
	if(!first) {
		return std::unexpected(first.error());
	}
	const auto second = colors.read(bits, *first);
	if(!second) {
		return std::unexpected(second.error());
	}
	const auto predictor = static_cast<std::uint8_t>((*first << 4U) | *second);
	const auto decoded_byte_count = predictor_byte_count + image_byte_count;
	std::vector<std::uint8_t> packed;
	packed.reserve(decoded_byte_count);
	packed.insert(packed.end(), predictor_byte_count, predictor);

	std::uint8_t previous_position = no_previous_position;
	while(packed.size() < decoded_byte_count) {
		const auto position = read_position(bits);
		if(!position) {
			return std::unexpected(position.error());
		}
		if(*position == previous_position) {
			while(true) {
				if(packed.size() >= decoded_byte_count) {
					return std::unexpected(error(
						AssetErrorCode::output_overflow,
						"PI literal sequence extends past the declared dimensions",
						bits.current_offset()
					));
				}
				const auto previous_color = static_cast<std::uint8_t>(
					packed.back() & 0x0FU
				);
				const auto high = colors.read(bits, previous_color);
				if(!high) {
					return std::unexpected(high.error());
				}
				const auto low = colors.read(bits, *high);
				if(!low) {
					return std::unexpected(low.error());
				}
				packed.push_back(static_cast<std::uint8_t>((*high << 4U) | *low));
				const auto again = bits.bit();
				if(!again) {
					return std::unexpected(again.error());
				}
				if(*again == 0U) {
					break;
				}
			}
			previous_position = no_previous_position;
			continue;
		}

		const auto length = read_run_length(bits);
		if(!length) {
			return std::unexpected(length.error());
		}
		const auto append = append_reference_run(
			packed,
			*position,
			*length,
			width,
			decoded_byte_count,
			bits.current_offset()
		);
		if(!append) {
			return std::unexpected(append.error());
		}
		previous_position = *position;
	}

	result.image.width = static_cast<std::uint16_t>(width);
	result.image.height = static_cast<std::uint16_t>(height);
	result.image.indices.reserve(pixel_count);
	for(std::size_t index = predictor_byte_count; index < packed.size(); ++index) {
		result.image.indices.push_back(static_cast<std::uint8_t>(packed[index] >> 4U));
		result.image.indices.push_back(static_cast<std::uint8_t>(packed[index] & 0x0FU));
	}
	return result;
}

} // namespace th03::assets
