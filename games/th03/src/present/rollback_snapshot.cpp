#include "th03/present/rollback_snapshot.hpp"

#include "th03/present/raster.hpp"
#include "th03/present/tram.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ranges>
#include <span>

namespace th03::present {
namespace {

constexpr std::array<std::byte, 8U> snapshot_magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'R'}, std::byte{'B'},
	std::byte{'V'}, std::byte{'0'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::uint16_t snapshot_version = 1U;
constexpr std::uint16_t snapshot_header_size = 32U;
constexpr std::uint32_t snapshot_visual_flag = 2U;
constexpr std::uint32_t fnv_offset_basis = 2166136261U;
constexpr std::uint32_t fnv_prime = 16777619U;

class ByteWriter final {
public:
	explicit ByteWriter(std::span<std::byte> bytes, std::size_t offset) noexcept
		: bytes_(bytes), offset_(offset) {}

	[[nodiscard]] bool byte(std::uint8_t value) noexcept {
		if(offset_ >= bytes_.size()) {
			return false;
		}
		bytes_[offset_++] = static_cast<std::byte>(value);
		return true;
	}

	[[nodiscard]] bool u16(std::uint16_t value) noexcept {
		return byte(static_cast<std::uint8_t>(value)) &&
			byte(static_cast<std::uint8_t>(value >> 8U));
	}

	[[nodiscard]] bool raw(const void *source, std::size_t size) noexcept {
		if(offset_ > bytes_.size() || size > bytes_.size() - offset_) {
			return false;
		}
		std::memcpy(bytes_.data() + offset_, source, size);
		offset_ += size;
		return true;
	}

	[[nodiscard]] std::size_t offset() const noexcept { return offset_; }

private:
	std::span<std::byte> bytes_{};
	std::size_t offset_ = 0U;
};

[[nodiscard]] std::uint16_t read_u16(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept {
	return static_cast<std::uint16_t>(
		std::to_integer<std::uint8_t>(bytes[offset]) |
		(static_cast<std::uint16_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 1U])
		) << 8U)
	);
}

[[nodiscard]] std::uint32_t read_u32(
	std::span<const std::byte> bytes,
	std::size_t offset
) noexcept {
	return static_cast<std::uint32_t>(
		std::to_integer<std::uint8_t>(bytes[offset]) |
		(static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 1U])
		) << 8U) |
		(static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 2U])
		) << 16U) |
		(static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[offset + 3U])
		) << 24U)
	);
}

void write_u16(
	std::span<std::byte> bytes,
	std::size_t offset,
	std::uint16_t value
) noexcept {
	bytes[offset] = static_cast<std::byte>(value & 0xFFU);
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

void write_u32(
	std::span<std::byte> bytes,
	std::size_t offset,
	std::uint32_t value
) noexcept {
	bytes[offset] = static_cast<std::byte>(value & 0xFFU);
	bytes[offset + 1U] = static_cast<std::byte>((value >> 8U) & 0xFFU);
	bytes[offset + 2U] = static_cast<std::byte>((value >> 16U) & 0xFFU);
	bytes[offset + 3U] = static_cast<std::byte>(value >> 24U);
}

[[nodiscard]] std::uint32_t fnv1a32(
	std::span<const std::byte> bytes
) noexcept {
	auto hash = fnv_offset_basis;
	for(const auto value : bytes) {
		hash ^= std::to_integer<std::uint8_t>(value);
		hash *= fnv_prime;
	}
	return hash;
}

[[nodiscard]] RollbackVisualError error(
	RollbackVisualErrorCode code,
	const char *message,
	std::size_t offset = 0U
) noexcept {
	return RollbackVisualError{code, message, offset};
}

[[nodiscard]] bool valid_page(std::uint8_t value) noexcept {
	return value <= static_cast<std::uint8_t>(Page::one);
}

[[nodiscard]] bool valid_mode(std::uint8_t value) noexcept {
	return value <= static_cast<std::uint8_t>(RasterMode::zoomed_200_lower);
}

} // namespace

std::expected<void, RollbackVisualError> capture_rollback_visual(
	const Raster &raster,
	const Tram &tram,
	RollbackVisualSnapshot &snapshot
) noexcept {
	snapshot.used_size = 0U;
	for(const auto &page : raster.pages_) {
		if(page.size() != graphics_page_pixels) {
			return std::unexpected(error(
				RollbackVisualErrorCode::count_mismatch,
				"Raster page size is not canonical"
			));
		}
	}

	auto bytes = std::span<std::byte>(snapshot.bytes);
	ByteWriter writer(bytes, snapshot_header_size);
	for(const auto &page : raster.pages_) {
		if(!writer.raw(page.data(), page.size())) {
			return std::unexpected(error(
				RollbackVisualErrorCode::count_mismatch,
				"T3RBV001 exceeds its fixed capacity"
			));
		}
	}

	if(!writer.byte(static_cast<std::uint8_t>(raster.access_page_)) ||
		!writer.byte(static_cast<std::uint8_t>(raster.shown_page_)) ||
		!writer.byte(static_cast<std::uint8_t>(raster.raster_mode_))) {
		return std::unexpected(error(
			RollbackVisualErrorCode::count_mismatch,
			"T3RBV001 exceeds its fixed capacity"
		));
	}
	for(const auto color : raster.pending_palette_.colors) {
		if(!writer.byte(color.r) || !writer.byte(color.g) || !writer.byte(color.b)) {
			return std::unexpected(error(
				RollbackVisualErrorCode::count_mismatch,
				"T3RBV001 exceeds its fixed capacity"
			));
		}
	}
	for(const auto color : raster.applied_palette_.colors) {
		if(color.r > 0x0FU || color.g > 0x0FU || color.b > 0x0FU) {
			return std::unexpected(error(
				RollbackVisualErrorCode::invalid_value,
				"applied palette contains a non-RGB4 component",
				writer.offset()
			));
		}
		if(!writer.byte(color.r) || !writer.byte(color.g) || !writer.byte(color.b)) {
			return std::unexpected(error(
				RollbackVisualErrorCode::count_mismatch,
				"T3RBV001 exceeds its fixed capacity"
			));
		}
	}
	if(!writer.u16(static_cast<std::uint16_t>(raster.palette_tone_)) ||
		!writer.u16(raster.scroll_y_) || !writer.byte(tram.visible_ ? 1U : 0U)) {
		return std::unexpected(error(
			RollbackVisualErrorCode::count_mismatch,
			"T3RBV001 exceeds its fixed capacity"
		));
	}
	for(const auto cell : tram.cells_) {
		if(!writer.u16(cell.code) || !writer.u16(cell.attribute)) {
			return std::unexpected(error(
				RollbackVisualErrorCode::count_mismatch,
				"T3RBV001 exceeds its fixed capacity"
			));
		}
	}
	for(const auto &glyph : tram.gaiji_) {
		if(!writer.raw(glyph.row_halves.data(), glyph.row_halves.size())) {
			return std::unexpected(error(
				RollbackVisualErrorCode::count_mismatch,
				"T3RBV001 exceeds its fixed capacity"
			));
		}
	}

	snapshot.used_size = static_cast<std::uint32_t>(writer.offset());
	std::ranges::copy(snapshot_magic, bytes.begin());
	write_u16(bytes, 8U, snapshot_version);
	write_u16(bytes, 10U, snapshot_header_size);
	write_u32(bytes, 12U, snapshot.used_size);
	write_u32(bytes, 16U, snapshot.used_size - snapshot_header_size);
	write_u32(bytes, 20U, fnv1a32(
		std::span<const std::byte>(snapshot.bytes).subspan(
			snapshot_header_size,
			snapshot.used_size - snapshot_header_size
		)
	));
	write_u32(bytes, 24U, snapshot_visual_flag);
	write_u32(bytes, 28U, 0U);
	std::fill(bytes.begin() + snapshot.used_size, bytes.end(), std::byte{0});
	return {};
}

std::expected<void, RollbackVisualError> restore_rollback_visual(
	Raster &raster,
	Tram &tram,
	const RollbackVisualSnapshot &snapshot
) noexcept {
	if(snapshot.used_size < snapshot_header_size ||
		snapshot.used_size > snapshot.bytes.size()) {
		return std::unexpected(error(
			RollbackVisualErrorCode::truncated,
			"T3RBV001 size is outside its fixed image"
		));
	}
	const auto bytes = std::span<const std::byte>(snapshot.bytes).first(
		snapshot.used_size
	);
	if(!std::ranges::equal(snapshot_magic, bytes.first(snapshot_magic.size()))) {
		return std::unexpected(error(
			RollbackVisualErrorCode::bad_magic,
			"rollback visual magic is not T3RBV001"
		));
	}
	if(read_u16(bytes, 8U) != snapshot_version) {
		return std::unexpected(error(
			RollbackVisualErrorCode::unsupported_version,
			"rollback visual version is not supported", 8U
		));
	}
	if(read_u16(bytes, 10U) != snapshot_header_size ||
		read_u32(bytes, 24U) != snapshot_visual_flag ||
		read_u32(bytes, 28U) != 0U) {
		return std::unexpected(error(
			RollbackVisualErrorCode::unsupported_layout,
			"rollback visual layout is not supported", 10U
		));
	}
	if(read_u32(bytes, 12U) != snapshot.used_size ||
		read_u32(bytes, 16U) != snapshot.used_size - snapshot_header_size) {
		return std::unexpected(error(
			RollbackVisualErrorCode::count_mismatch,
			"rollback visual size fields disagree", 12U
		));
	}
	const auto payload = bytes.subspan(snapshot_header_size);
	if(read_u32(bytes, 20U) != fnv1a32(payload)) {
		return std::unexpected(error(
			RollbackVisualErrorCode::checksum_mismatch,
			"rollback visual payload checksum does not match", 20U
		));
	}

	constexpr std::size_t pages_bytes = graphics_page_pixels * 2U;
	constexpr std::size_t selectors_offset = snapshot_header_size + pages_bytes;
	constexpr std::size_t pending_offset = selectors_offset + 3U;
	constexpr std::size_t applied_offset = pending_offset + (16U * 3U);
	constexpr std::size_t tone_offset = applied_offset + (16U * 3U);
	constexpr std::size_t scroll_offset = tone_offset + 2U;
	constexpr std::size_t visible_offset = scroll_offset + 2U;
	constexpr std::size_t cells_offset = visible_offset + 1U;
	constexpr std::size_t gaiji_offset = cells_offset + (tram_cell_count * 4U);
	constexpr std::size_t exact_size = gaiji_offset +
		(gaiji_count * gaiji_glyph_bytes);
	if(snapshot.used_size != exact_size) {
		return std::unexpected(error(
			RollbackVisualErrorCode::count_mismatch,
			"rollback visual payload has a noncanonical size", 12U
		));
	}
	for(std::size_t offset = snapshot_header_size;
		offset < selectors_offset; ++offset) {
		if(std::to_integer<std::uint8_t>(bytes[offset]) > 0x0FU) {
			return std::unexpected(error(
				RollbackVisualErrorCode::invalid_value,
				"rollback visual contains a non-Index4 pixel", offset
			));
		}
	}
	const auto access = std::to_integer<std::uint8_t>(bytes[selectors_offset]);
	const auto shown = std::to_integer<std::uint8_t>(bytes[selectors_offset + 1U]);
	const auto mode = std::to_integer<std::uint8_t>(bytes[selectors_offset + 2U]);
	if(!valid_page(access) || !valid_page(shown) || !valid_mode(mode)) {
		return std::unexpected(error(
			RollbackVisualErrorCode::invalid_value,
			"rollback visual contains an invalid page or raster mode",
			selectors_offset
		));
	}
	for(std::size_t offset = applied_offset; offset < tone_offset; ++offset) {
		if(std::to_integer<std::uint8_t>(bytes[offset]) > 0x0FU) {
			return std::unexpected(error(
				RollbackVisualErrorCode::invalid_value,
				"rollback visual contains a non-RGB4 palette component", offset
			));
		}
	}
	const auto visible = std::to_integer<std::uint8_t>(bytes[visible_offset]);
	if(visible > 1U) {
		return std::unexpected(error(
			RollbackVisualErrorCode::invalid_value,
			"rollback visual contains a non-Boolean TRAM visibility",
			visible_offset
		));
	}
	for(const auto &page : raster.pages_) {
		if(page.size() != graphics_page_pixels) {
			return std::unexpected(error(
				RollbackVisualErrorCode::count_mismatch,
				"destination Raster page size is not canonical"
			));
		}
	}

	std::size_t offset = snapshot_header_size;
	for(auto &page : raster.pages_) {
		std::memcpy(page.data(), bytes.data() + offset, page.size());
		offset += page.size();
	}
	raster.access_page_ = static_cast<Page>(access);
	raster.shown_page_ = static_cast<Page>(shown);
	raster.raster_mode_ = static_cast<RasterMode>(mode);
	offset = pending_offset;
	for(auto &color : raster.pending_palette_.colors) {
		color.r = std::to_integer<std::uint8_t>(bytes[offset++]);
		color.g = std::to_integer<std::uint8_t>(bytes[offset++]);
		color.b = std::to_integer<std::uint8_t>(bytes[offset++]);
	}
	for(auto &color : raster.applied_palette_.colors) {
		color.r = std::to_integer<std::uint8_t>(bytes[offset++]);
		color.g = std::to_integer<std::uint8_t>(bytes[offset++]);
		color.b = std::to_integer<std::uint8_t>(bytes[offset++]);
	}
	raster.palette_tone_ = static_cast<std::int16_t>(read_u16(bytes, tone_offset));
	raster.scroll_y_ = read_u16(bytes, scroll_offset);
	tram.visible_ = (visible != 0U);
	offset = cells_offset;
	for(auto &cell : tram.cells_) {
		cell.code = read_u16(bytes, offset);
		cell.attribute = read_u16(bytes, offset + 2U);
		offset += 4U;
	}
	for(auto &glyph : tram.gaiji_) {
		std::memcpy(
			glyph.row_halves.data(), bytes.data() + offset,
			glyph.row_halves.size()
		);
		offset += glyph.row_halves.size();
	}
	return {};
}

} // namespace th03::present
