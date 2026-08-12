#include "op_compiled_text_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace th03::app {
namespace {

constexpr std::size_t profile_file_bytes = 63866U;
constexpr std::size_t mz_minimum_header_bytes = 28U;
constexpr std::size_t mz_page_bytes = 512U;
constexpr std::size_t track_load_offset = 0xDE84U;
constexpr std::size_t choice_payload_bytes = 32U;
constexpr std::size_t choice_stride_bytes = choice_payload_bytes + 1U;
constexpr std::size_t quit_load_offset = track_load_offset +
	(OpMusicRoomTextImport::quit_choice_index * choice_stride_bytes);

constexpr MainExecutableSha256 profile_sha256 = {
	0xEFU, 0xD8U, 0x58U, 0xAEU, 0xF6U, 0x9AU, 0x24U, 0x0AU,
	0xF3U, 0xA2U, 0x7CU, 0x74U, 0x7AU, 0x5BU, 0xEAU, 0xE1U,
	0x50U, 0xF5U, 0x5FU, 0x0BU, 0x41U, 0xB8U, 0xAFU, 0xDDU,
	0x1EU, 0xDAU, 0x17U, 0x60U, 0xA7U, 0x59U, 0xECU, 0xC0U,
};

struct MzLayout final {
	std::size_t header_bytes = 0U;
};

[[nodiscard]] std::uint16_t read_u16(
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

[[nodiscard]] OpCompiledTextError error(
	OpCompiledTextErrorCode code,
	OpCompiledTextItem item = OpCompiledTextItem::none,
	std::uint8_t choice_index = 0U,
	std::size_t offset = 0U
) noexcept
{
	return {code, item, choice_index, offset};
}

[[nodiscard]] std::expected<MzLayout, OpCompiledTextError>
parse_mz(std::span<const std::byte> bytes) noexcept
{
	if(bytes.size() < mz_minimum_header_bytes) {
		return std::unexpected(error(
			OpCompiledTextErrorCode::input_too_short
		));
	}
	if(read_u16(bytes, 0U) != 0x5A4DU) {
		return std::unexpected(error(
			OpCompiledTextErrorCode::invalid_mz_signature,
			OpCompiledTextItem::none,
			0U,
			0U
		));
	}
	const auto last_page_bytes = static_cast<std::size_t>(read_u16(bytes, 2U));
	const auto page_count = static_cast<std::size_t>(read_u16(bytes, 4U));
	const auto relocation_count = static_cast<std::size_t>(read_u16(bytes, 6U));
	const auto header_paragraphs = static_cast<std::size_t>(read_u16(bytes, 8U));
	const auto relocation_offset = static_cast<std::size_t>(read_u16(bytes, 24U));
	if(page_count == 0U || last_page_bytes >= mz_page_bytes) {
		return std::unexpected(error(
			OpCompiledTextErrorCode::invalid_mz_layout
		));
	}
	const auto declared_bytes = ((page_count - 1U) * mz_page_bytes) +
		((last_page_bytes == 0U) ? mz_page_bytes : last_page_bytes);
	const auto header_bytes = header_paragraphs * 16U;
	if(declared_bytes != bytes.size() ||
		header_bytes < mz_minimum_header_bytes ||
		header_bytes > declared_bytes ||
		relocation_offset > header_bytes ||
		relocation_count > ((header_bytes - relocation_offset) / 4U)) {
		return std::unexpected(error(
			OpCompiledTextErrorCode::invalid_mz_layout
		));
	}
	return MzLayout{header_bytes};
}

[[nodiscard]] bool shift_jis_valid(
	std::span<const std::byte> source,
	std::size_t &invalid_offset
) noexcept
{
	std::size_t offset = 0U;
	while(offset < source.size()) {
		const auto first = std::to_integer<std::uint8_t>(source[offset]);
		if((first >= 0x20U && first <= 0x7EU) ||
			(first >= 0xA1U && first <= 0xDFU)) {
			++offset;
			continue;
		}
		const bool lead = (first >= 0x81U && first <= 0x9FU) ||
			(first >= 0xE0U && first <= 0xFCU);
		if(!lead || offset + 1U >= source.size()) {
			invalid_offset = offset;
			return false;
		}
		const auto second = std::to_integer<std::uint8_t>(source[offset + 1U]);
		if(!((second >= 0x40U && second <= 0x7EU) ||
			(second >= 0x80U && second <= 0xFCU))) {
			invalid_offset = offset + 1U;
			return false;
		}
		offset += 2U;
	}
	return true;
}

[[nodiscard]] std::expected<OpMusicRoomTextImport, OpCompiledTextError>
import_profile(
	std::span<const std::byte> bytes,
	const MainExecutableSha256 &accepted_digest
)
{
	auto mz = parse_mz(bytes);
	if(!mz) {
		return std::unexpected(mz.error());
	}
	if(bytes.size() != profile_file_bytes ||
		main_executable_sha256(bytes) != accepted_digest) {
		return std::unexpected(error(
			OpCompiledTextErrorCode::unsupported_profile
		));
	}

	OpMusicRoomTextImport result{};
	for(std::size_t index = 0U; index < result.choices.size(); ++index) {
		if(index == OpMusicRoomTextImport::empty_choice_index) {
			continue;
		}
		const bool is_quit = index == OpMusicRoomTextImport::quit_choice_index;
		const auto load_offset = is_quit ? quit_load_offset :
			(track_load_offset + (index * choice_stride_bytes));
		const auto file_offset = mz->header_bytes + load_offset;
		const auto item = is_quit ? OpCompiledTextItem::quit :
			OpCompiledTextItem::track;
		if(file_offset > bytes.size() ||
			choice_payload_bytes >= (bytes.size() - file_offset)) {
			return std::unexpected(error(
				OpCompiledTextErrorCode::compiled_range_out_of_bounds,
				item,
				static_cast<std::uint8_t>(index),
				file_offset
			));
		}
		const auto payload = bytes.subspan(file_offset, choice_payload_bytes);
		if(bytes[file_offset + choice_payload_bytes] != std::byte{0U}) {
			return std::unexpected(error(
				OpCompiledTextErrorCode::malformed_source_text,
				item,
				static_cast<std::uint8_t>(index),
				file_offset + choice_payload_bytes
			));
		}
		std::size_t invalid_offset = 0U;
		if(!shift_jis_valid(payload, invalid_offset)) {
			return std::unexpected(error(
				OpCompiledTextErrorCode::malformed_source_text,
				item,
				static_cast<std::uint8_t>(index),
				file_offset + invalid_offset
			));
		}
		auto &destination = result.choices[index];
		destination.reserve(payload.size());
		for(const auto value : payload) {
			destination.push_back(std::to_integer<std::uint8_t>(value));
		}
	}
	return result;
}

} // namespace

std::expected<OpMusicRoomTextImport, OpCompiledTextError>
import_op_music_room_text(std::span<const std::byte> op_executable)
{
	return import_profile(op_executable, profile_sha256);
}

namespace detail {

std::expected<OpMusicRoomTextImport, OpCompiledTextError>
import_op_music_room_text_for_test(
	std::span<const std::byte> op_executable,
	const MainExecutableSha256 &accepted_digest
)
{
	return import_profile(op_executable, accepted_digest);
}

} // namespace detail
} // namespace th03::app
