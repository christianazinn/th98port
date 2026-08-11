#include "mainl_compiled_text_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>
#include <vector>

namespace th03::app {
namespace {

constexpr std::size_t profile_file_bytes = 67372U;
constexpr std::size_t mz_minimum_header_bytes = 28U;
constexpr std::size_t mz_page_bytes = 512U;

constexpr MainExecutableSha256 profile_sha256 = {
	0x92U, 0x60U, 0x0CU, 0x88U, 0x58U, 0xBBU, 0x40U, 0x75U,
	0x16U, 0xCAU, 0xC6U, 0xA9U, 0xD7U, 0x75U, 0x38U, 0xEBU,
	0xDFU, 0x0CU, 0x33U, 0x52U, 0x2CU, 0x4FU, 0x6BU, 0xD9U,
	0x19U, 0x4DU, 0x56U, 0x9EU, 0xB9U, 0x27U, 0x13U, 0x4AU,
};

struct TextSpec final {
	MainlCompiledTextItem item;
	std::uint8_t character_index;
	std::size_t load_offset;
	std::size_t payload_bytes;
};

constexpr std::array<TextSpec, 18U> text_specs = {{
	{MainlCompiledTextItem::title, 0U, 0xE5C8U, 28U},
	{MainlCompiledTextItem::name, 0U, 0xE5E5U, 13U},
	{MainlCompiledTextItem::title, 1U, 0xE5F3U, 28U},
	{MainlCompiledTextItem::name, 1U, 0xE610U, 10U},
	{MainlCompiledTextItem::title, 2U, 0xE61BU, 28U},
	{MainlCompiledTextItem::name, 2U, 0xE638U, 15U},
	{MainlCompiledTextItem::title, 3U, 0xE648U, 28U},
	{MainlCompiledTextItem::name, 3U, 0xE665U, 10U},
	{MainlCompiledTextItem::title, 4U, 0xE670U, 28U},
	{MainlCompiledTextItem::name, 4U, 0xE68DU, 10U},
	{MainlCompiledTextItem::title, 5U, 0xE698U, 28U},
	{MainlCompiledTextItem::name, 5U, 0xE6B5U, 16U},
	{MainlCompiledTextItem::title, 6U, 0xE6C6U, 28U},
	{MainlCompiledTextItem::name, 6U, 0xE6E3U, 14U},
	{MainlCompiledTextItem::title, 7U, 0xE6F2U, 28U},
	{MainlCompiledTextItem::name, 7U, 0xE70FU, 15U},
	{MainlCompiledTextItem::title, 8U, 0xE71FU, 28U},
	{MainlCompiledTextItem::name, 8U, 0xE73CU, 13U},
}};

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

[[nodiscard]] MainlCompiledTextError error(
	MainlCompiledTextErrorCode code,
	MainlCompiledTextItem item = MainlCompiledTextItem::none,
	std::uint8_t character_index = 0U,
	std::size_t offset = 0U
) noexcept
{
	return {code, item, character_index, offset};
}

[[nodiscard]] std::expected<MzLayout, MainlCompiledTextError>
parse_mz(std::span<const std::byte> bytes) noexcept
{
	if(bytes.size() < mz_minimum_header_bytes) {
		return std::unexpected(error(
			MainlCompiledTextErrorCode::input_too_short
		));
	}
	if(read_u16(bytes, 0U) != 0x5A4DU) {
		return std::unexpected(error(
			MainlCompiledTextErrorCode::invalid_mz_signature,
			MainlCompiledTextItem::none,
			0U,
			0U
		));
	}
	const auto last_page_bytes = static_cast<std::size_t>(
		read_u16(bytes, 2U)
	);
	const auto page_count = static_cast<std::size_t>(read_u16(bytes, 4U));
	const auto relocation_count = static_cast<std::size_t>(
		read_u16(bytes, 6U)
	);
	const auto header_paragraphs = static_cast<std::size_t>(
		read_u16(bytes, 8U)
	);
	const auto relocation_offset = static_cast<std::size_t>(
		read_u16(bytes, 24U)
	);
	if(page_count == 0U || last_page_bytes >= mz_page_bytes) {
		return std::unexpected(error(
			MainlCompiledTextErrorCode::invalid_mz_layout
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
			MainlCompiledTextErrorCode::invalid_mz_layout
		));
	}
	return MzLayout{header_bytes};
}

[[nodiscard]] bool shift_jis_valid(
	std::span<const std::byte> text,
	std::size_t &invalid_offset
) noexcept
{
	std::size_t offset = 0U;
	while(offset < text.size()) {
		const auto first = std::to_integer<std::uint8_t>(text[offset]);
		if((first >= 0x20U && first <= 0x7EU) ||
			(first >= 0xA1U && first <= 0xDFU)) {
			++offset;
			continue;
		}
		const bool lead = (first >= 0x81U && first <= 0x9FU) ||
			(first >= 0xE0U && first <= 0xFCU);
		if(!lead || offset + 1U >= text.size()) {
			invalid_offset = offset;
			return false;
		}
		const auto second = std::to_integer<std::uint8_t>(text[offset + 1U]);
		if(!((second >= 0x40U && second <= 0x7EU) ||
			(second >= 0x80U && second <= 0xFCU))) {
			invalid_offset = offset + 1U;
			return false;
		}
		offset += 2U;
	}
	return true;
}

[[nodiscard]] std::expected<MainlStageTextImport, MainlCompiledTextError>
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
			MainlCompiledTextErrorCode::unsupported_profile
		));
	}

	MainlStageTextImport result{};
	for(const auto &spec : text_specs) {
		const auto file_offset = mz->header_bytes + spec.load_offset;
		if(file_offset > bytes.size() ||
			spec.payload_bytes >= (bytes.size() - file_offset)) {
			return std::unexpected(error(
				MainlCompiledTextErrorCode::compiled_range_out_of_bounds,
				spec.item,
				spec.character_index,
				file_offset
			));
		}
		const auto payload = bytes.subspan(file_offset, spec.payload_bytes);
		if(bytes[file_offset + spec.payload_bytes] != std::byte{0U}) {
			return std::unexpected(error(
				MainlCompiledTextErrorCode::malformed_source_text,
				spec.item,
				spec.character_index,
				file_offset + spec.payload_bytes
			));
		}
		std::size_t invalid_offset = 0U;
		if(!shift_jis_valid(payload, invalid_offset)) {
			return std::unexpected(error(
				MainlCompiledTextErrorCode::malformed_source_text,
				spec.item,
				spec.character_index,
				file_offset + invalid_offset
			));
		}
		auto &character = result.assets.characters[spec.character_index];
		auto &destination = (spec.item == MainlCompiledTextItem::title) ?
			character.title : character.name;
		destination.reserve(payload.size());
		for(const auto value : payload) {
			destination.push_back(std::to_integer<std::uint8_t>(value));
		}
	}
	return result;
}

} // namespace

std::expected<MainlStageTextImport, MainlCompiledTextError>
import_mainl_stage_text(std::span<const std::byte> mainl_executable)
{
	return import_profile(mainl_executable, profile_sha256);
}

namespace detail {

std::expected<MainlStageTextImport, MainlCompiledTextError>
import_mainl_stage_text_for_test(
	std::span<const std::byte> mainl_executable,
	const MainExecutableSha256 &accepted_digest
)
{
	return import_profile(mainl_executable, accepted_digest);
}

} // namespace detail
} // namespace th03::app
