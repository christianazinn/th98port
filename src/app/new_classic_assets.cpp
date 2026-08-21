#include "th03/app/new_classic_assets.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "th03/app/main_compiled_content.hpp"
#include "th03/assets/decode.hpp"
#include "th03/present/pi.hpp"

namespace th03::app {

struct NewClassicAssetsAccess final {
	[[nodiscard]] static auto &advances(NewClassicMenuFont &font) noexcept
	{
		return font.advances_;
	}
	[[nodiscard]] static auto &bitmaps(NewClassicMenuFont &font) noexcept
	{
		return font.bitmaps_;
	}
};

namespace {

constexpr std::size_t archive_file_bytes = 1'183'932U;
constexpr MainExecutableSha256 archive_sha256{
	0xDEU, 0x06U, 0xDCU, 0x9BU, 0x02U, 0x74U, 0xC7U, 0xCEU,
	0x6AU, 0x6EU, 0xC8U, 0x3DU, 0x66U, 0x95U, 0xCBU, 0x6DU,
	0x05U, 0x09U, 0xF4U, 0xC0U, 0xFCU, 0xD9U, 0x06U, 0x40U,
	0xA9U, 0x51U, 0xF1U, 0xC9U, 0x47U, 0xEBU, 0x5CU, 0x44U,
};
constexpr std::size_t archive_header_bytes = 16U;
constexpr std::size_t directory_entry_bytes = 32U;
constexpr std::array<std::string_view, 21U> required_members{
	"SLB1.PI", "SLB1B.PI", "MNUFONT.PF", "PAUSE.GFT", "BG1.PI",
	"BG2A.BMP", "BG2B.BMP", "M001A.BMP", "M001B.BMP",
	"M002A.BMP", "M002B.BMP", "M003A.BMP", "M003B.BMP",
	"M004A.BMP", "M004B.BMP",
	"M005A.BMP", "M005B.BMP", "M006A.BMP", "M006B.BMP",
	"M007A.BMP", "M007B.BMP",
};
constexpr std::size_t menu_font_file_bytes = 3'168U;
constexpr std::size_t menu_font_header_bytes = 32U;
constexpr std::size_t menu_font_advances_offset = 32U;
constexpr std::size_t menu_font_bitmap_offset = 128U;
constexpr std::size_t pause_gaiji_file_bytes = 1'312U;
constexpr std::size_t pause_gaiji_header_bytes = 32U;

using ArchiveMembers = std::map<std::string, std::vector<std::byte>>;

[[nodiscard]] NewClassicAssetsError error(
	NewClassicAssetsErrorCode code,
	std::size_t offset = 0U,
	std::error_code system = {}
) noexcept
{
	return {.code = code, .system = system, .offset = offset};
}

[[nodiscard]] std::uint16_t u16(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	return static_cast<std::uint16_t>(
		std::to_integer<std::uint8_t>(bytes[offset]) |
		(static_cast<std::uint16_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 1U])
		) << 8U)
	);
}

[[nodiscard]] std::uint32_t u32(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept
{
	return std::to_integer<std::uint8_t>(bytes[offset]) |
		(static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 1U])
		) << 8U) |
		(static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 2U])
		) << 16U) |
		(static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 3U])
		) << 24U);
}

[[nodiscard]] bool zero_range(
	std::span<const std::byte> bytes,
	std::size_t first,
	std::size_t last
) noexcept
{
	return std::ranges::all_of(
		bytes.subspan(first, last - first),
		[](std::byte value) { return value == std::byte{0U}; }
	);
}

[[nodiscard]] bool valid_member_name(std::string_view name) noexcept
{
	const auto dot = name.find('.');
	if(dot == std::string_view::npos || dot == 0U || dot > 8U ||
		name.find('.', dot + 1U) != std::string_view::npos ||
		name.size() - dot - 1U < 1U || name.size() - dot - 1U > 3U) {
		return false;
	}
	for(std::size_t index = 0U; index < name.size(); ++index) {
		const auto value = static_cast<unsigned char>(name[index]);
		if(index == dot) {
			continue;
		}
		if(!((value >= 'A' && value <= 'Z') ||
			(value >= '0' && value <= '9') || value == '_')) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] std::expected<ArchiveMembers, NewClassicAssetsError>
parse_archive(std::span<const std::byte> bytes)
{
	if(bytes.size() != archive_file_bytes ||
		main_executable_sha256(bytes) != archive_sha256) {
		return std::unexpected(error(
			NewClassicAssetsErrorCode::digest_mismatch
		));
	}
	if(u16(bytes, 2U) != 2U || u16(bytes, 4U) != required_members.size() ||
		u16(bytes, 6U) > 0xFFU ||
		!zero_range(bytes, 8U, archive_header_bytes)) {
		return std::unexpected(error(
			NewClassicAssetsErrorCode::archive_header
		));
	}
	const auto member_count = static_cast<std::size_t>(u16(bytes, 4U));
	const auto directory_bytes = (member_count + 1U) * directory_entry_bytes;
	if(u16(bytes, 0U) != directory_bytes ||
		directory_bytes > bytes.size() - archive_header_bytes) {
		return std::unexpected(error(
			NewClassicAssetsErrorCode::archive_directory
		));
	}
	std::vector<std::byte> directory(directory_bytes);
	auto key = static_cast<std::uint8_t>(u16(bytes, 6U));
	for(std::size_t index = 0U; index < directory.size(); ++index) {
		const auto decoded = static_cast<std::uint8_t>(
			std::to_integer<std::uint8_t>(
				bytes[archive_header_bytes + index]
			) ^ key
		);
		directory[index] = static_cast<std::byte>(decoded);
		key = static_cast<std::uint8_t>(key - decoded);
	}
	if(!zero_range(
		directory, member_count * directory_entry_bytes, directory.size()
	)) {
		return std::unexpected(error(
			NewClassicAssetsErrorCode::archive_directory,
			archive_header_bytes + (member_count * directory_entry_bytes)
		));
	}

	ArchiveMembers members{};
	std::size_t expected_offset = archive_header_bytes + directory_bytes;
	for(std::size_t member = 0U; member < member_count; ++member) {
		const auto base = member * directory_entry_bytes;
		if(u16(directory, base) != 0xF388U ||
			directory[base + 2U] != std::byte{0U} ||
			!zero_range(directory, base + 24U, base + directory_entry_bytes)) {
			return std::unexpected(error(
				NewClassicAssetsErrorCode::archive_member,
				archive_header_bytes + base
			));
		}
		std::size_t name_bytes = 0U;
		while(name_bytes < 13U &&
			directory[base + 3U + name_bytes] != std::byte{0U}) {
			++name_bytes;
		}
		if(name_bytes == 0U || name_bytes >= 13U ||
			!zero_range(directory, base + 3U + name_bytes, base + 16U)) {
			return std::unexpected(error(
				NewClassicAssetsErrorCode::archive_member,
				archive_header_bytes + base + 3U
			));
		}
		std::string name{};
		name.reserve(name_bytes);
		for(std::size_t index = 0U; index < name_bytes; ++index) {
			name.push_back(static_cast<char>(std::to_integer<std::uint8_t>(
				directory[base + 3U + index]
			)));
		}
		const auto stored = static_cast<std::size_t>(
			u16(directory, base + 16U)
		);
		const auto original = static_cast<std::size_t>(
			u16(directory, base + 18U)
		);
		const auto offset = static_cast<std::size_t>(
			u32(directory, base + 20U)
		);
		if(!valid_member_name(name) || stored == 0U || stored != original ||
			offset != expected_offset || stored > bytes.size() - offset ||
			members.contains(name)) {
			return std::unexpected(error(
				NewClassicAssetsErrorCode::archive_member,
				archive_header_bytes + base
			));
		}
		members.emplace(
			std::move(name),
			std::vector<std::byte>(
				bytes.begin() + static_cast<std::ptrdiff_t>(offset),
				bytes.begin() + static_cast<std::ptrdiff_t>(offset + stored)
			)
		);
		expected_offset += stored;
	}
	if(expected_offset != bytes.size() || members.size() != required_members.size() ||
		!std::ranges::all_of(required_members, [&](std::string_view name) {
			return members.contains(std::string{name});
		})) {
		return std::unexpected(error(
			NewClassicAssetsErrorCode::archive_member_set, expected_offset
		));
	}
	return members;
}

[[nodiscard]] std::expected<assets::PiImage, NewClassicAssetsError> decode_pi(
	const ArchiveMembers &members,
	std::string_view name
)
{
	const auto found = members.find(std::string{name});
	if(found == members.end()) {
		return std::unexpected(error(
			NewClassicAssetsErrorCode::archive_member_set
		));
	}
	auto decoded = assets::decode_pi(found->second);
	if(!decoded) {
		auto result = error(NewClassicAssetsErrorCode::pi_decode);
		result.asset_error.emplace(std::move(decoded.error()));
		return std::unexpected(std::move(result));
	}
	if(!decoded->palette ||
		decoded->image.width != present::graphics_width ||
		decoded->image.height != present::graphics_height ||
		decoded->image.indices.size() != present::graphics_page_pixels) {
		return std::unexpected(error(NewClassicAssetsErrorCode::pi_geometry));
	}
	return std::move(*decoded);
}

} // namespace

const NewClassicMessageCatalog &NewClassicReplayPatchAssets::messages(
	NewClassicLanguageAssetProfile profile
) const noexcept
{
	return custom_messages ? *custom_messages :
		new_classic_builtin_messages(profile);
}

NewClassicPatchSurface::NewClassicPatchSurface() :
	pixels_(present::graphics_page_pixels)
{}

bool NewClassicPatchSurface::reset(const assets::PiImage &background) noexcept
{
	if(!background.palette ||
		background.image.width != present::graphics_width ||
		background.image.height != present::graphics_height ||
		background.image.indices.size() != present::graphics_page_pixels) {
		return false;
	}
	for(std::size_t index = 0U; index < background.palette->colors.size();
		++index) {
		if(!raster_.set_pending_palette_entry(
			static_cast<std::uint8_t>(index),
			background.palette->colors[index]
		)) {
			return false;
		}
	}
	raster_.apply_pending_palette();
	raster_.set_palette_tone(100);
	for(const auto page : {present::Page::zero, present::Page::one}) {
		if(!raster_.set_access_page(page) ||
			!present::pi_put_8(raster_, 0, 0, background)) {
			return false;
		}
	}
	if(!raster_.set_access_page(present::Page::zero) ||
		!raster_.set_shown_page(present::Page::zero)) {
		return false;
	}
	initialized_ = true;
	return publish();
}

bool NewClassicPatchSurface::begin_hidden_frame(
	const assets::PiImage &background
) noexcept
{
	if(!initialized_) {
		if(!reset(background)) {
			return false;
		}
	}
	if(!background.palette ||
		background.image.width != present::graphics_width ||
		background.image.height != present::graphics_height ||
		background.image.indices.size() != present::graphics_page_pixels) {
		return false;
	}
	const auto hidden = raster_.shown_page() == present::Page::zero ?
		present::Page::one : present::Page::zero;
	return raster_.set_access_page(hidden) &&
		present::pi_put_8(raster_, 0, 0, background);
}

bool NewClassicPatchSurface::begin_shown_frame(
	const assets::PiImage &background
) noexcept
{
	if(!initialized_ && !reset(background)) {
		return false;
	}
	const auto shown = raster_.shown_page();
	return raster_.set_access_page(shown) &&
		present::pi_put_8(raster_, 0, 0, background);
}

std::optional<present::Page>
NewClassicPatchSurface::copy_shown_to_hidden() noexcept
{
	if(!initialized_) {
		return std::nullopt;
	}
	const auto shown = raster_.shown_page();
	const auto hidden = shown == present::Page::zero ?
		present::Page::one : present::Page::zero;
	if(!raster_.copy_page(
		shown, {0, 0, present::graphics_width, present::graphics_height},
		hidden, {0, 0}
	) || !raster_.set_access_page(shown)) {
		return std::nullopt;
	}
	return hidden;
}

bool NewClassicPatchSurface::terminal_black_clear() noexcept
{
	if(!initialized_) {
		return false;
	}
	raster_.set_palette_tone(0);
	raster_.apply_pending_palette();
	for(const auto page : {present::Page::zero, present::Page::one}) {
		if(!raster_.set_access_page(page) ||
			!raster_.clear({0, 0, present::graphics_width,
				present::graphics_height}, 0U)) {
			return false;
		}
	}
	return raster_.set_shown_page(present::Page::zero) &&
		raster_.set_access_page(present::Page::zero) && publish(present::Page::zero);
}

present::Raster &NewClassicPatchSurface::raster() noexcept
{
	return raster_;
}

const present::Raster &NewClassicPatchSurface::raster() const noexcept
{
	return raster_;
}

bool NewClassicPatchSurface::publish(present::Page page) noexcept
{
	if(!raster_.set_shown_page(page)) {
		return false;
	}
	const auto graphics = raster_.graphics(page);
	if(graphics.size() != pixels_.size()) {
		return false;
	}
	for(std::size_t index = 0U; index < graphics.size(); ++index) {
		pixels_[index] = {
			present::ComposedPixelLayer::graphics, graphics[index]
		};
	}
	return true;
}

present::SourceSurface NewClassicPatchSurface::surface() const noexcept
{
	return {
		.pixels = std::span<
			const present::ComposedPixel, present::graphics_page_pixels
		>(pixels_.data(), pixels_.size()),
		.graphics_palette = raster_.applied_palette(),
		.shown_page = raster_.shown_page(),
		.raster_mode = raster_.raster_mode(),
		.effective_scroll_y = raster_.scroll_y(),
		.palette_tone = raster_.palette_tone(),
		.reduced_flash_palette = raster_.applied_palette(),
		.host_overlay = false,
	};
}

std::size_t NewClassicMenuFont::glyph_index(std::uint8_t value) noexcept
{
	if(value < first_character || value > last_character) {
		value = fallback_character;
	}
	return static_cast<std::size_t>(value - first_character);
}

std::int32_t NewClassicMenuFont::width(std::string_view text) const noexcept
{
	return width_n(text, text.size());
}

std::int32_t NewClassicMenuFont::width_n(
	std::string_view text,
	std::size_t count
) const noexcept
{
	std::int32_t result = 0;
	const auto size = std::min(text.size(), count);
	for(std::size_t index = 0U; index < size; ++index) {
		result += advances_[glyph_index(static_cast<std::uint8_t>(text[index]))];
	}
	return result;
}

bool NewClassicMenuFont::put(
	present::Raster &raster,
	std::int32_t left,
	std::int32_t top,
	std::string_view text,
	present::Index4 color
) const noexcept
{
	return put_n(raster, left, top, text, text.size(), color);
}

bool NewClassicMenuFont::put_n(
	present::Raster &raster,
	std::int32_t left,
	std::int32_t top,
	std::string_view text,
	std::size_t count,
	present::Index4 color
) const noexcept
{
	return put_n_clipped(
		raster, left, top, text, count, color,
		{0, 0, present::graphics_width, present::graphics_height}
	);
}

bool NewClassicMenuFont::put_n_clipped(
	present::Raster &raster,
	std::int32_t left,
	std::int32_t top,
	std::string_view text,
	std::size_t count,
	present::Index4 color,
	present::Rect clip
) const noexcept
{
	const auto clip_right = static_cast<std::int64_t>(clip.x) + clip.width;
	const auto clip_bottom = static_cast<std::int64_t>(clip.y) + clip.height;
	if(color > 0x0FU || top < 0 ||
		top > static_cast<std::int32_t>(
			present::graphics_height - glyph_height
		) || clip.x < 0 || clip.y < 0 ||
		clip_right > present::graphics_width ||
		clip_bottom > present::graphics_height) {
		return false;
	}
	const auto size = std::min(text.size(), count);
	for(std::size_t character = 0U; character < size; ++character) {
		const auto value = static_cast<std::uint8_t>(text[character]);
		const auto index = glyph_index(value);
		if(value != ' ' && left >= 0 && left < present::graphics_width) {
			const auto &glyph = bitmaps_[index];
			for(std::int32_t row = 0; row < glyph_height; ++row) {
				for(std::int32_t column = 0; column < glyph_width; ++column) {
					const auto byte = glyph[
						(static_cast<std::size_t>(row) * 2U) +
						(static_cast<std::size_t>(column) / 8U)
					];
					if((byte & (0x80U >> (column & 7))) == 0U) {
						continue;
					}
					const auto x = left + column;
					const auto y = top + row;
					if(x >= clip.x && x < clip_right &&
						y >= clip.y && y < clip_bottom &&
						!raster.plot({x, top + row}, color)) {
						return false;
					}
				}
			}
		}
		left += advances_[index];
	}
	return true;
}

bool NewClassicMenuFont::put_centered(
	present::Raster &raster,
	std::int32_t center_x,
	std::int32_t top,
	std::string_view text,
	present::Index4 color
) const noexcept
{
	return put(raster, center_x - (width(text) / 2), top, text, color);
}

bool NewClassicMenuFont::put_centered_clipped(
	present::Raster &raster,
	std::int32_t center_x,
	std::int32_t top,
	std::string_view text,
	present::Index4 color,
	present::Rect clip
) const noexcept
{
	return put_n_clipped(
		raster, center_x - (width(text) / 2), top, text, text.size(), color,
		clip
	);
}

bool NewClassicMenuFont::put_right(
	present::Raster &raster,
	std::int32_t right,
	std::int32_t top,
	std::string_view text,
	present::Index4 color
) const noexcept
{
	return put(raster, right - width(text), top, text, color);
}

bool NewClassicMenuFont::put_vertical_flipped(
	present::Raster &raster,
	std::int32_t left,
	std::int32_t top,
	char character,
	present::Index4 color
) const noexcept
{
	if(color > 0x0FU || left < 0 ||
		left > static_cast<std::int32_t>(present::graphics_width - glyph_width) ||
		top < 0 || top > static_cast<std::int32_t>(
			present::graphics_height - glyph_height
		)) {
		return false;
	}
	const auto index = glyph_index(static_cast<std::uint8_t>(character));
	const auto &glyph = bitmaps_[index];
	for(std::int32_t row = 0; row < glyph_height; ++row) {
		const auto source_row = static_cast<std::int32_t>(glyph_height) - row - 1;
		for(std::int32_t column = 0; column < glyph_width; ++column) {
			const auto byte = glyph[
				(static_cast<std::size_t>(source_row) * 2U) +
				(static_cast<std::size_t>(column) / 8U)
			];
			if((byte & (0x80U >> (column & 7))) != 0U &&
				!raster.plot({left + column, top + row}, color)) {
				return false;
			}
		}
	}
	return true;
}

bool NewClassicMenuFont::put_cells(
	present::Raster &raster,
	std::int32_t cell_left,
	std::int32_t top,
	std::string_view text,
	present::Index4 color
) const noexcept
{
	std::size_t cursor = 0U;
	std::size_t cells = 0U;
	while(cursor < text.size()) {
		while(cursor < text.size() && text[cursor] == ' ') {
			++cursor;
			++cells;
		}
		const auto first = cursor;
		while(cursor < text.size() && text[cursor] != ' ') {
			++cursor;
		}
		if(cursor != first && !put(
			raster,
			cell_left + static_cast<std::int32_t>(cells * 8U),
			top,
			text.substr(first, cursor - first),
			color
		)) {
			return false;
		}
		cells += cursor - first;
	}
	return true;
}

bool NewClassicMenuFont::restore_rect(
	present::Raster &raster,
	std::int32_t left,
	std::int32_t top,
	std::uint16_t width_value,
	std::uint16_t height
) const
{
	auto right = left + static_cast<std::int32_t>(width_value);
	left = std::max(left, 0);
	right = std::min(right, static_cast<std::int32_t>(present::graphics_width));
	if(top < 0 || top >= present::graphics_height || right <= left ||
		height == 0U) {
		return false;
	}
	const auto bottom = std::min(
		top + static_cast<std::int32_t>(height),
		static_cast<std::int32_t>(present::graphics_height)
	);
	const auto aligned_left = left & ~0x0F;
	const auto aligned_right = std::min(
		(right + 15) & ~0x0F,
		static_cast<std::int32_t>(present::graphics_width)
	);
	if(!raster.copy_page(
		present::Page::one,
		{
			.x = aligned_left,
			.y = top,
			.width = static_cast<std::uint16_t>(aligned_right - aligned_left),
			.height = static_cast<std::uint16_t>(bottom - top),
		},
		present::Page::zero,
		{aligned_left, top}
	)) {
		return false;
	}
	return raster.set_access_page(present::Page::zero);
}

namespace detail {

std::expected<NewClassicMenuFont, NewClassicAssetsError>
decode_new_classic_menu_font(std::span<const std::byte> bytes)
{
	if(bytes.size() != menu_font_file_bytes ||
		std::to_integer<std::uint8_t>(bytes[0U]) != 'T' ||
		std::to_integer<std::uint8_t>(bytes[1U]) != '3' ||
		std::to_integer<std::uint8_t>(bytes[2U]) != 'P' ||
		std::to_integer<std::uint8_t>(bytes[3U]) != 'F' ||
		std::to_integer<std::uint8_t>(bytes[4U]) != 'N' ||
		std::to_integer<std::uint8_t>(bytes[5U]) != 'T' ||
		std::to_integer<std::uint8_t>(bytes[6U]) != '1' ||
		bytes[7U] != std::byte{0U} || u16(bytes, 8U) != menu_font_file_bytes ||
		u16(bytes, 10U) != menu_font_header_bytes ||
		u16(bytes, 12U) != menu_font_advances_offset ||
		u16(bytes, 14U) != menu_font_bitmap_offset ||
		u16(bytes, 16U) != NewClassicMenuFont::glyph_count ||
		std::to_integer<std::uint8_t>(bytes[18U]) !=
			NewClassicMenuFont::first_character ||
		std::to_integer<std::uint8_t>(bytes[19U]) !=
			NewClassicMenuFont::last_character ||
		std::to_integer<std::uint8_t>(bytes[20U]) != 16U ||
		std::to_integer<std::uint8_t>(bytes[21U]) != 16U ||
		std::to_integer<std::uint8_t>(bytes[22U]) !=
			NewClassicMenuFont::fallback_character ||
		bytes[23U] != std::byte{0U} || !zero_range(bytes, 26U, 32U) ||
		bytes[127U] != std::byte{0U}) {
		return std::unexpected(error(NewClassicAssetsErrorCode::font_shape));
	}
	std::uint16_t checksum = 0U;
	for(std::size_t index = menu_font_header_bytes; index < bytes.size(); ++index) {
		checksum = static_cast<std::uint16_t>(
			checksum + std::to_integer<std::uint8_t>(bytes[index])
		);
	}
	if(checksum != u16(bytes, 24U)) {
		return std::unexpected(error(NewClassicAssetsErrorCode::font_checksum));
	}
	NewClassicMenuFont result{};
	auto &advances = NewClassicAssetsAccess::advances(result);
	auto &bitmaps = NewClassicAssetsAccess::bitmaps(result);
	for(std::size_t glyph = 0U; glyph < NewClassicMenuFont::glyph_count; ++glyph) {
		const auto advance = std::to_integer<std::uint8_t>(
			bytes[menu_font_advances_offset + glyph]
		);
		if(advance == 0U || advance > NewClassicMenuFont::glyph_width) {
			return std::unexpected(error(
				NewClassicAssetsErrorCode::font_shape,
				menu_font_advances_offset + glyph
			));
		}
		advances[glyph] = advance;
		for(std::size_t index = 0U; index < NewClassicMenuFont::glyph_bytes;
			++index) {
			bitmaps[glyph][index] = std::to_integer<std::uint8_t>(bytes[
				menu_font_bitmap_offset +
				(glyph * NewClassicMenuFont::glyph_bytes) + index
			]);
		}
	}
	return result;
}

std::expected<
	std::array<std::array<std::uint8_t, 32U>, 40U>,
	NewClassicAssetsError
> decode_new_classic_pause_gaiji(std::span<const std::byte> bytes)
{
	if(bytes.size() != pause_gaiji_file_bytes ||
		std::to_integer<std::uint8_t>(bytes[0U]) != 'T' ||
		std::to_integer<std::uint8_t>(bytes[1U]) != '3' ||
		std::to_integer<std::uint8_t>(bytes[2U]) != 'P' ||
		std::to_integer<std::uint8_t>(bytes[3U]) != 'G' ||
		std::to_integer<std::uint8_t>(bytes[4U]) != 'J' ||
		std::to_integer<std::uint8_t>(bytes[5U]) != '1' ||
		bytes[6U] != std::byte{0U} || bytes[7U] != std::byte{0U} ||
		u16(bytes, 8U) != pause_gaiji_file_bytes ||
		u16(bytes, 10U) != pause_gaiji_header_bytes ||
		u16(bytes, 12U) != 40U ||
		std::to_integer<std::uint8_t>(bytes[14U]) != 0x30U ||
		std::to_integer<std::uint8_t>(bytes[15U]) != 32U ||
		std::to_integer<std::uint8_t>(bytes[16U]) != 0xD3U ||
		std::to_integer<std::uint8_t>(bytes[17U]) != 8U ||
		std::to_integer<std::uint8_t>(bytes[18U]) != 16U ||
		std::to_integer<std::uint8_t>(bytes[19U]) != 16U ||
		u16(bytes, 20U) != pause_gaiji_header_bytes ||
		!zero_range(bytes, 24U, 32U)) {
		return std::unexpected(error(
			NewClassicAssetsErrorCode::pause_gaiji_shape
		));
	}
	std::uint16_t checksum = 0U;
	for(std::size_t index = pause_gaiji_header_bytes; index < bytes.size();
		++index) {
		checksum = static_cast<std::uint16_t>(
			checksum + std::to_integer<std::uint8_t>(bytes[index])
		);
	}
	if(checksum != u16(bytes, 22U)) {
		return std::unexpected(error(
			NewClassicAssetsErrorCode::pause_gaiji_checksum
		));
	}
	std::array<std::array<std::uint8_t, 32U>, 40U> result{};
	for(std::size_t tile = 0U; tile < result.size(); ++tile) {
		for(std::size_t index = 0U; index < result[tile].size(); ++index) {
			result[tile][index] = std::to_integer<std::uint8_t>(bytes[
				pause_gaiji_header_bytes + (tile * result[tile].size()) + index
			]);
		}
	}
	return result;
}

std::expected<assets::PiImage, NewClassicAssetsError>
decode_new_classic_menu_background(
	std::span<const std::byte> top,
	std::span<const std::byte> bottom
)
{
	constexpr std::size_t file_header_bytes = 14U;
	constexpr std::size_t dib_header_bytes = 40U;
	constexpr std::size_t palette_bytes = 64U;
	constexpr std::size_t pixel_offset =
		file_header_bytes + dib_header_bytes + palette_bytes;
	constexpr std::uint16_t width = present::graphics_width;
	constexpr std::uint16_t half_height = present::graphics_height / 2U;
	constexpr std::size_t packed_row_bytes = width / 2U;
	constexpr std::size_t packed_pixels = packed_row_bytes * half_height;
	constexpr std::size_t file_bytes = pixel_offset + packed_pixels;
	const auto le16 = [](std::span<const std::byte> bytes,
		std::size_t offset) noexcept {
		return static_cast<std::uint16_t>(
			std::to_integer<std::uint8_t>(bytes[offset]) |
			(static_cast<std::uint16_t>(
				std::to_integer<std::uint8_t>(bytes[offset + 1U])
			) << 8U)
		);
	};
	const auto le32 = [](std::span<const std::byte> bytes,
		std::size_t offset) noexcept {
		return std::to_integer<std::uint8_t>(bytes[offset]) |
			(static_cast<std::uint32_t>(
				std::to_integer<std::uint8_t>(bytes[offset + 1U])
			) << 8U) |
			(static_cast<std::uint32_t>(
				std::to_integer<std::uint8_t>(bytes[offset + 2U])
			) << 16U) |
			(static_cast<std::uint32_t>(
				std::to_integer<std::uint8_t>(bytes[offset + 3U])
			) << 24U);
	};
	const auto valid_half = [&](std::span<const std::byte> bytes) noexcept {
		return bytes.size() == file_bytes &&
			bytes[0U] == std::byte{'B'} && bytes[1U] == std::byte{'M'} &&
			le32(bytes, 2U) == file_bytes && le32(bytes, 10U) == pixel_offset &&
			le32(bytes, 14U) == dib_header_bytes && le32(bytes, 18U) == width &&
			le32(bytes, 22U) == half_height && le16(bytes, 26U) == 1U &&
			le16(bytes, 28U) == 4U && le32(bytes, 30U) == 0U &&
			le32(bytes, 34U) == packed_pixels && le32(bytes, 46U) == 16U &&
			le32(bytes, 50U) == 16U;
	};
	if(!valid_half(top) || !valid_half(bottom) ||
		!std::ranges::equal(
			top.subspan(file_header_bytes + dib_header_bytes, palette_bytes),
			bottom.subspan(file_header_bytes + dib_header_bytes, palette_bytes)
		)) {
		return std::unexpected(error(
			NewClassicAssetsErrorCode::menu_background_shape
		));
	}

	assets::PiImage result{};
	result.plane_count = 4U;
	result.image.width = width;
	result.image.height = present::graphics_height;
	result.image.indices.resize(present::graphics_page_pixels);
	assets::Palette16Rgb4 palette{};
	for(std::size_t index = 0U; index < palette.colors.size(); ++index) {
		const auto at = file_header_bytes + dib_header_bytes + (index * 4U);
		palette.colors[index] = {
			static_cast<std::uint8_t>(
				std::to_integer<std::uint8_t>(top[at + 2U]) >> 4U
			),
			static_cast<std::uint8_t>(
				std::to_integer<std::uint8_t>(top[at + 1U]) >> 4U
			),
			static_cast<std::uint8_t>(
				std::to_integer<std::uint8_t>(top[at]) >> 4U
			),
		};
	}
	result.palette = palette;
	const auto unpack_half = [&](std::span<const std::byte> bytes,
		std::size_t output_top) noexcept {
		for(std::size_t y = 0U; y < half_height; ++y) {
			const auto source = pixel_offset +
				((half_height - y - 1U) * packed_row_bytes);
			const auto destination = (output_top + y) * width;
			for(std::size_t packed = 0U; packed < packed_row_bytes; ++packed) {
				const auto value = std::to_integer<std::uint8_t>(
					bytes[source + packed]
				);
				result.image.indices[destination + (packed * 2U)] =
					static_cast<std::uint8_t>(value >> 4U);
				result.image.indices[destination + (packed * 2U) + 1U] =
					static_cast<std::uint8_t>(value & 0x0FU);
			}
		}
	};
	unpack_half(top, 0U);
	unpack_half(bottom, half_height);

	// These two original colors are represented nearly identically by another
	// background entry. Reserving their indices prevents the yellow/white menu
	// palette overrides from recoloring thousands of background pixels.
	for(auto &index : result.image.indices) {
		if(index == 12U) {
			index = 10U;
		} else if(index == 15U) {
			index = 0U;
		}
	}
	// Retained-menu artwork and text share one PC-98 palette. These slots are
	// fixed across every screen so navigation never changes title, status, or
	// selection colors. New artwork is authored without these indices.
	result.palette->colors[8U] = {12U, 3U, 14U};
	result.palette->colors[9U] = {13U, 8U, 15U};
	result.palette->colors[11U] = {15U, 10U, 15U};
	result.palette->colors[12U] = {15U, 15U, 0U};
	result.palette->colors[13U] = {15U, 12U, 15U};
	result.palette->colors[15U] = {15U, 15U, 15U};
	return result;
}

} // namespace detail

std::expected<NewClassicReplayPatchAssets, NewClassicAssetsError>
load_new_classic_replay_patch_assets(const std::filesystem::path &path)
{
	if(path.empty()) {
		return std::unexpected(error(NewClassicAssetsErrorCode::invalid_path));
	}
	std::error_code system;
	const auto status = std::filesystem::status(path, system);
	if(system) {
		return std::unexpected(error(
			NewClassicAssetsErrorCode::source_status, 0U, system
		));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(error(
			NewClassicAssetsErrorCode::source_not_regular
		));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system) {
		return std::unexpected(error(
			NewClassicAssetsErrorCode::source_size, 0U, system
		));
	}
	if(size != archive_file_bytes) {
		return std::unexpected(error(NewClassicAssetsErrorCode::source_size));
	}
	try {
		std::ifstream input(path, std::ios::binary);
		if(!input) {
			return std::unexpected(error(NewClassicAssetsErrorCode::source_open));
		}
		std::vector<std::byte> bytes(archive_file_bytes);
		input.read(
			reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
		if(!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
			return std::unexpected(error(NewClassicAssetsErrorCode::source_read));
		}
		auto members = parse_archive(bytes);
		if(!members) {
			return std::unexpected(std::move(members.error()));
		}
		NewClassicReplayPatchAssets result{};
		auto list = decode_pi(*members, "SLB1.PI");
		auto name = decode_pi(*members, "SLB1B.PI");
		auto key = decode_pi(*members, "BG1.PI");
		if(!list) return std::unexpected(std::move(list.error()));
		if(!name) return std::unexpected(std::move(name.error()));
		if(!key) return std::unexpected(std::move(key.error()));
		auto font = detail::decode_new_classic_menu_font(
			members->at("MNUFONT.PF")
		);
		if(!font) return std::unexpected(std::move(font.error()));
		auto pause = detail::decode_new_classic_pause_gaiji(
			members->at("PAUSE.GFT")
		);
		if(!pause) return std::unexpected(std::move(pause.error()));
		result.replay_list_background = std::move(*list);
		result.replay_name_background = std::move(*name);
		result.key_config_background = std::move(*key);
		constexpr std::array background_members{
			std::pair{"BG2A.BMP", "BG2B.BMP"},
			std::pair{"M001A.BMP", "M001B.BMP"},
			std::pair{"M002A.BMP", "M002B.BMP"},
			std::pair{"M003A.BMP", "M003B.BMP"},
			std::pair{"M004A.BMP", "M004B.BMP"},
			std::pair{"M005A.BMP", "M005B.BMP"},
			std::pair{"M006A.BMP", "M006B.BMP"},
			std::pair{"M007A.BMP", "M007B.BMP"},
		};
		for(std::size_t index = 0U; index < background_members.size(); ++index) {
			auto background = detail::decode_new_classic_menu_background(
				members->at(background_members[index].first),
				members->at(background_members[index].second)
			);
			if(!background) {
				return std::unexpected(std::move(background.error()));
			}
			result.new_menu_backgrounds[index] = std::move(*background);
		}
		result.menu_font = std::move(*font);
		result.pause_gaiji = std::move(*pause);
		return result;
	} catch(const std::bad_alloc &) {
		return std::unexpected(error(
			NewClassicAssetsErrorCode::allocation_failed
		));
	}
}

} // namespace th03::app
