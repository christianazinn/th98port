#include "th03/present/tram.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace th03::present {
namespace {

constexpr std::uint64_t fnv_offset_basis = 14695981039346656037ULL;
constexpr std::uint64_t fnv_prime = 1099511628211ULL;
constexpr std::uint8_t tram_hash_schema = 1U;

[[nodiscard]] bool point_offset(TramPoint point, std::size_t &offset) noexcept {
	if((point.x < 0) || (point.y < 0) || (point.x >= tram_width) ||
		(point.y >= tram_height)) {
		return false;
	}
	offset = (static_cast<std::size_t>(point.y) * tram_width) +
		static_cast<std::size_t>(point.x);
	return true;
}

[[nodiscard]] bool valid_rect(TramRect rect) noexcept {
	if((rect.width == 0U) || (rect.height == 0U)) {
		return true;
	}
	if((rect.x < 0) || (rect.y < 0)) {
		return false;
	}
	return (static_cast<std::int64_t>(rect.x) + rect.width <= tram_width) &&
		(static_cast<std::int64_t>(rect.y) + rect.height <= tram_height);
}

void hash_byte(std::uint64_t &hash, std::uint8_t value) noexcept {
	hash ^= value;
	hash *= fnv_prime;
}

void hash_u16_le(std::uint64_t &hash, std::uint16_t value) noexcept {
	hash_byte(hash, static_cast<std::uint8_t>(value & 0x00FFU));
	hash_byte(hash, static_cast<std::uint8_t>(value >> 8U));
}

} // namespace

std::optional<GaijiCellReference> decode_gaiji_cell(TramCode code) noexcept {
	const auto row = static_cast<std::uint8_t>(code & 0x00FFU);
	if((row != 0x56U) && (row != 0x57U)) {
		return std::nullopt;
	}
	const auto column = static_cast<std::uint8_t>(code >> 8U);
	const auto bank = static_cast<std::uint8_t>((row - 0x56U) << 7U);
	return GaijiCellReference{
		static_cast<std::uint8_t>(bank | (column & 0x7FU)),
		((column & 0x80U) != 0U) ? GaijiHalf::right : GaijiHalf::left,
	};
}

std::optional<GaijiCellReference> decode_paired_gaiji_cell(
	std::span<const TramCell> cells,
	std::size_t offset
) noexcept {
	if(offset >= cells.size()) {
		return std::nullopt;
	}
	const auto decoded = decode_gaiji_cell(cells[offset].code);
	if(!decoded.has_value()) {
		return std::nullopt;
	}
	const auto cell_x = offset % tram_width;
	switch(decoded->half) {
	case GaijiHalf::left:
		if((cell_x + 1U < tram_width) && (offset + 1U < cells.size()) &&
			(cells[offset + 1U].code == gaiji_cell_code(
				decoded->glyph_id,
				GaijiHalf::right
			))) {
			return decoded;
		}
		break;
	case GaijiHalf::right:
		if((cell_x != 0U) && (offset != 0U) &&
			(cells[offset - 1U].code == gaiji_cell_code(
				decoded->glyph_id,
				GaijiHalf::left
			))) {
			return decoded;
		}
		break;
	}
	return std::nullopt;
}

std::optional<GaijiCellReference> decode_visible_gaiji_cell(
	std::span<const TramCell> cells,
	std::size_t offset
) noexcept {
	if(offset >= cells.size()) {
		return std::nullopt;
	}
	const auto decoded = decode_gaiji_cell(cells[offset].code);
	if(!decoded.has_value()) {
		return std::nullopt;
	}
	if((cells[offset].code & 0xFF00U) != 0U) {
		return decoded;
	}
	return decode_paired_gaiji_cell(cells, offset);
}

void Tram::fill(TramCell cell_value) noexcept {
	cells_.fill(cell_value);
}

bool Tram::fill_attributes(
	TramRect rect,
	TramAttribute attribute
) noexcept {
	if(!valid_rect(rect)) {
		return false;
	}
	for(std::uint8_t y = 0U; y < rect.height; ++y) {
		const auto destination_y = static_cast<std::size_t>(rect.y) + y;
		for(std::uint8_t x = 0U; x < rect.width; ++x) {
			const auto destination_x = static_cast<std::size_t>(rect.x) + x;
			cells_[(destination_y * tram_width) + destination_x].attribute =
				attribute;
		}
	}
	return true;
}

bool Tram::put_cell(TramPoint point, TramCell cell_value) noexcept {
	std::size_t offset = 0U;
	if(!point_offset(point, offset)) {
		return false;
	}
	cells_[offset] = cell_value;
	return true;
}

bool Tram::put_gaiji(
	TramPoint left,
	std::uint8_t glyph_id,
	TramAttribute attribute
) noexcept {
	std::size_t left_offset = 0U;
	std::size_t right_offset = 0U;
	if(!point_offset(left, left_offset) ||
		!point_offset({left.x + 1, left.y}, right_offset)) {
		return false;
	}
	cells_[left_offset] = {
		gaiji_cell_code(glyph_id, GaijiHalf::left),
		attribute,
	};
	cells_[right_offset] = {
		gaiji_cell_code(glyph_id, GaijiHalf::right),
		attribute,
	};
	return true;
}

bool Tram::load_gaiji(
	std::uint8_t glyph_id,
	std::span<const std::uint8_t> row_halves
) noexcept {
	if(row_halves.size() != gaiji_glyph_bytes) {
		return false;
	}
	std::copy(row_halves.begin(), row_halves.end(), gaiji_[glyph_id].row_halves.begin());
	return true;
}

void Tram::set_visible(bool visible_value) noexcept {
	visible_ = visible_value;
}

bool Tram::visible() const noexcept {
	return visible_;
}

std::optional<TramCell> Tram::cell(TramPoint point) const noexcept {
	std::size_t offset = 0U;
	if(!point_offset(point, offset)) {
		return std::nullopt;
	}
	return cells_[offset];
}

std::span<const TramCell> Tram::cells() const noexcept {
	return cells_;
}

const GaijiGlyph &Tram::gaiji(std::uint8_t glyph_id) const noexcept {
	return gaiji_[glyph_id];
}

std::optional<std::uint8_t> Tram::gaiji_row_half(
	std::uint8_t glyph_id,
	std::uint8_t row,
	GaijiHalf half
) const noexcept {
	if(row >= gaiji_height) {
		return std::nullopt;
	}
	std::size_t half_offset = 0U;
	switch(half) {
	case GaijiHalf::left:
		half_offset = 0U;
		break;
	case GaijiHalf::right:
		half_offset = 1U;
		break;
	default:
		return std::nullopt;
	}
	return gaiji_[glyph_id].row_halves[
		(static_cast<std::size_t>(row) * gaiji_bytes_per_row) + half_offset
	];
}

std::uint64_t Tram::visible_state_hash() const noexcept {
	std::uint64_t hash = fnv_offset_basis;
	hash_byte(hash, tram_hash_schema);
	hash_byte(hash, visible_ ? 1U : 0U);
	std::array<bool, gaiji_count> referenced{};
	for(std::size_t cell_offset = 0U; cell_offset < cells_.size(); ++cell_offset) {
		const auto &cell_value = cells_[cell_offset];
		hash_u16_le(hash, cell_value.code);
		hash_u16_le(hash, cell_value.attribute);
		const auto gaiji_ref = decode_visible_gaiji_cell(cells_, cell_offset);
		if(gaiji_ref.has_value()) {
			referenced[gaiji_ref->glyph_id] = true;
		}
	}
	for(std::size_t glyph_id = 0U; glyph_id < gaiji_count; ++glyph_id) {
		if(!referenced[glyph_id]) {
			continue;
		}
		hash_byte(hash, static_cast<std::uint8_t>(glyph_id));
		for(const auto bits : gaiji_[glyph_id].row_halves) {
			hash_byte(hash, bits);
		}
	}
	return hash;
}

} // namespace th03::present
