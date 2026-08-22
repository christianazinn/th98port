#include "new_classic_language_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <new>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace th03::app {

struct NewClassicLanguageOverlayAccess final {
	[[nodiscard]] static std::map<std::string, ResourceBytes> &members(
		NewClassicLanguageOverlay &overlay
	) noexcept
	{
		return overlay.members_;
	}
};

namespace {

constexpr std::size_t overlay_file_bytes = 295'731U;
constexpr MainExecutableSha256 overlay_sha256{
	0xEDU, 0x64U, 0x17U, 0x70U, 0x14U, 0x73U, 0x74U, 0xBBU,
	0x73U, 0x2BU, 0x3EU, 0x54U, 0x17U, 0x46U, 0xE7U, 0x1EU,
	0xB6U, 0x32U, 0x77U, 0xCBU, 0xCEU, 0xA3U, 0xD6U, 0x03U,
	0xFBU, 0x7FU, 0x96U, 0xECU, 0x05U, 0x68U, 0x09U, 0xE0U,
};
constexpr std::size_t header_bytes = 16U;
constexpr std::size_t directory_entry_bytes = 32U;
constexpr std::array<std::string_view, 52U> required_members{
	"@00DM0.TXT", "@00DM1.TXT", "@00ED.TXT", "@00TX.TXT",
	"@01DM0.TXT", "@01DM1.TXT", "@01ED.TXT", "@01TX.TXT",
	"@02DM0.TXT", "@02DM1.TXT", "@02ED.TXT", "@02TX.TXT",
	"@03DM0.TXT", "@03DM1.TXT", "@03ED.TXT", "@03TX.TXT",
	"@04DM0.TXT", "@04DM1.TXT", "@04ED.TXT", "@04TX.TXT",
	"@05DM0.TXT", "@05DM1.TXT", "@05ED.TXT", "@05TX.TXT",
	"@06DM0.TXT", "@06DM1.TXT", "@06ED.TXT", "@06TX.TXT",
	"@07DM0.TXT", "@07DM1.TXT", "@07ED.TXT", "@07TX.TXT",
	"@08DM0.TXT", "@08DM1.TXT", "@08ED.TXT", "@08TX.TXT",
	"@99ED.TXT", "CHNAME.BFT", "DM3B.PI", "DM3D.PI", "DM3F.PI",
	"DM3N.PI", "MUSIC.TXT", "STF9.CDG", "STF10.CDG", "STNX0.PI",
	"STNX1.PI", "STNX2.PI", "STNX3.PI", "STNX4.PI", "STNX5.PI",
	"TL02.PI",
};

[[nodiscard]] NewClassicLanguageOverlayError language_error(
	NewClassicLanguageOverlayErrorCode code,
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
	return std::ranges::all_of(bytes.subspan(first, last - first),
		[](std::byte value) { return value == std::byte{0U}; });
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
			(value >= '0' && value <= '9') || value == '_' || value == '@')) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] std::expected<
	NewClassicLanguageOverlay,
	NewClassicLanguageOverlayError
> parse_overlay(
	std::span<const std::byte> bytes,
	const MainExecutableSha256 &accepted_digest
)
{
	if(main_executable_sha256(bytes) != accepted_digest) {
		return std::unexpected(language_error(
			NewClassicLanguageOverlayErrorCode::digest_mismatch
		));
	}
	if(bytes.size() < header_bytes || u16(bytes, 2U) != 2U ||
		u16(bytes, 4U) == 0U || u16(bytes, 6U) > 0xFFU ||
		!zero_range(bytes, 8U, header_bytes)) {
		return std::unexpected(language_error(
			NewClassicLanguageOverlayErrorCode::header_shape
		));
	}
	const auto member_count = static_cast<std::size_t>(u16(bytes, 4U));
	if(member_count > ((std::numeric_limits<std::uint16_t>::max() /
		directory_entry_bytes) - 1U)) {
		return std::unexpected(language_error(
			NewClassicLanguageOverlayErrorCode::directory_shape, 4U
		));
	}
	const auto directory_bytes = (member_count + 1U) * directory_entry_bytes;
	if(u16(bytes, 0U) != directory_bytes ||
		directory_bytes > bytes.size() - header_bytes) {
		return std::unexpected(language_error(
			NewClassicLanguageOverlayErrorCode::directory_shape
		));
	}
	std::vector<std::byte> directory(directory_bytes);
	auto key = static_cast<std::uint8_t>(u16(bytes, 6U));
	for(std::size_t index = 0U; index < directory.size(); ++index) {
		const auto decoded = static_cast<std::uint8_t>(
			std::to_integer<std::uint8_t>(bytes[header_bytes + index]) ^ key
		);
		directory[index] = static_cast<std::byte>(decoded);
		key = static_cast<std::uint8_t>(key - decoded);
	}
	if(!zero_range(directory, member_count * directory_entry_bytes,
		directory.size())) {
		return std::unexpected(language_error(
			NewClassicLanguageOverlayErrorCode::directory_shape,
			header_bytes + (member_count * directory_entry_bytes)
		));
	}

	NewClassicLanguageOverlay result{};
	auto &members = NewClassicLanguageOverlayAccess::members(result);
	std::size_t expected_offset = header_bytes + directory_bytes;
	for(std::size_t member = 0U; member < member_count; ++member) {
		const auto base = member * directory_entry_bytes;
		if(u16(directory, base) != 0xF388U ||
			directory[base + 2U] != std::byte{0U} ||
			!zero_range(directory, base + 24U, base + directory_entry_bytes)) {
			return std::unexpected(language_error(
				NewClassicLanguageOverlayErrorCode::member_shape,
				header_bytes + base
			));
		}
		std::size_t name_bytes = 0U;
		while(name_bytes < 13U &&
			directory[base + 3U + name_bytes] != std::byte{0U}) {
			++name_bytes;
		}
		if(name_bytes == 0U || name_bytes >= 13U ||
			!zero_range(directory, base + 3U + name_bytes,
				base + 16U)) {
			return std::unexpected(language_error(
				NewClassicLanguageOverlayErrorCode::member_shape,
				header_bytes + base + 3U
			));
		}
		std::string name{};
		name.reserve(name_bytes);
		for(std::size_t index = 0U; index < name_bytes; ++index) {
			name.push_back(static_cast<char>(std::to_integer<std::uint8_t>(
				directory[base + 3U + index]
			)));
		}
		const auto stored = static_cast<std::size_t>(u16(directory, base + 16U));
		const auto original = static_cast<std::size_t>(u16(directory, base + 18U));
		const auto offset = static_cast<std::size_t>(u32(directory, base + 20U));
		if(!valid_member_name(name) || stored == 0U || stored != original ||
			offset != expected_offset || stored > bytes.size() - offset ||
			members.contains(name)) {
			return std::unexpected(language_error(
				NewClassicLanguageOverlayErrorCode::member_shape,
				header_bytes + base
			));
		}
		auto payload = std::make_shared<const std::vector<std::byte>>(
			bytes.begin() + static_cast<std::ptrdiff_t>(offset),
			bytes.begin() + static_cast<std::ptrdiff_t>(offset + stored)
		);
		members.emplace(std::move(name), std::move(payload));
		expected_offset += stored;
	}
	if(expected_offset != bytes.size()) {
		return std::unexpected(language_error(
			NewClassicLanguageOverlayErrorCode::member_shape, expected_offset
		));
	}
	return result;
}

void assign_text(
	std::vector<std::uint8_t> &destination,
	std::span<const std::uint8_t> text
)
{
	destination.assign(text.begin(), text.end());
}

} // namespace

std::size_t NewClassicLanguageOverlay::member_count() const noexcept
{
	return members_.size();
}

bool NewClassicLanguageOverlay::contains(std::string_view name) const noexcept
{
	return members_.contains(std::string{name});
}

std::map<std::string, ResourceBytes>
NewClassicLanguageOverlay::release_members() &&
{
	return std::move(members_);
}

std::expected<NewClassicLanguageOverlay, NewClassicLanguageOverlayError>
load_new_classic_language_overlay(const std::filesystem::path &path)
{
	if(path.empty()) {
		return std::unexpected(language_error(
			NewClassicLanguageOverlayErrorCode::invalid_path
		));
	}
	std::error_code system;
	const auto status = std::filesystem::status(path, system);
	if(system) {
		return std::unexpected(language_error(
			NewClassicLanguageOverlayErrorCode::source_status, 0U, system
		));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(language_error(
			NewClassicLanguageOverlayErrorCode::source_not_regular
		));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system) {
		return std::unexpected(language_error(
			NewClassicLanguageOverlayErrorCode::source_size, 0U, system
		));
	}
	if(size != overlay_file_bytes) {
		return std::unexpected(language_error(
			NewClassicLanguageOverlayErrorCode::source_size
		));
	}
	try {
		std::ifstream input(path, std::ios::binary);
		if(!input) {
			return std::unexpected(language_error(
				NewClassicLanguageOverlayErrorCode::source_open
			));
		}
		std::vector<std::byte> bytes(overlay_file_bytes);
		input.read(reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size()));
		if(!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
			return std::unexpected(language_error(
				NewClassicLanguageOverlayErrorCode::source_read
			));
		}
		auto parsed = parse_overlay(bytes, overlay_sha256);
		if(!parsed) {
			return std::unexpected(parsed.error());
		}
		if(parsed->member_count() != required_members.size() ||
			!std::ranges::all_of(required_members, [&](std::string_view name) {
				return parsed->contains(name);
			})) {
			return std::unexpected(language_error(
				NewClassicLanguageOverlayErrorCode::member_set
			));
		}
		return parsed;
	} catch(const std::bad_alloc &) {
		return std::unexpected(language_error(
			NewClassicLanguageOverlayErrorCode::allocation_failed
		));
	}
}

void apply_new_classic_english_text(
	MainCompiledContent &compiled,
	MainlStageTextAssets &stage,
	OpMusicRoomTextImport &music,
	const NewClassicGameTextCatalog &text
)
{
	for(std::size_t index = 0U; index < 19U; ++index) {
		assign_text(music.choices[index], text.bytes(
			static_cast<NewClassicGameTextId>(
				static_cast<std::size_t>(NewClassicGameTextId::music_track_01) + index
			)
		));
	}
	apply_new_classic_english_stage_text(stage, text);
	compiled.apply_new_classic_english_results(text);
}

void apply_new_classic_english_stage_text(
	MainlStageTextAssets &stage,
	const NewClassicGameTextCatalog &text
)
{
	for(std::size_t index = 0U; index < stage.characters.size(); ++index) {
		const auto id = [index](NewClassicGameTextId first) {
			return static_cast<NewClassicGameTextId>(
				static_cast<std::size_t>(first) + index
			);
		};
		assign_text(stage.characters[index].title,
			text.bytes(id(NewClassicGameTextId::stage_title_first_00)));
		if(text.locale() == "ja-JP") {
			stage.characters[index].title_second.clear();
		} else {
			assign_text(stage.characters[index].title_second,
				text.bytes(id(NewClassicGameTextId::stage_title_second_00)));
		}
		assign_text(stage.characters[index].name,
			text.bytes(id(NewClassicGameTextId::stage_name_00)));
	}
	for(std::size_t index = 0U; index < stage.registration_playchars.size(); ++index) {
		assign_text(stage.registration_playchars[index], text.bytes(
			static_cast<NewClassicGameTextId>(
				static_cast<std::size_t>(NewClassicGameTextId::registration_00) + index
			)
		));
	}
}

namespace detail {

std::expected<NewClassicLanguageOverlay, NewClassicLanguageOverlayError>
parse_new_classic_language_overlay_for_test(
	std::span<const std::byte> bytes,
	const MainExecutableSha256 &accepted_digest
)
{
	try {
		return parse_overlay(bytes, accepted_digest);
	} catch(const std::bad_alloc &) {
		return std::unexpected(language_error(
			NewClassicLanguageOverlayErrorCode::allocation_failed
		));
	}
}

} // namespace detail
} // namespace th03::app
