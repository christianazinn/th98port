#include "th03/app/new_classic/content/new_classic_picture_patch.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <new>
#include <utility>

namespace th03::app {
namespace {

constexpr std::array<std::byte, 8U> magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'P'}, std::byte{'T'},
	std::byte{'P'}, std::byte{'0'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::size_t header_size = 96U;
constexpr std::size_t picture_entry_size = 80U;
constexpr std::size_t run_entry_size = 12U;
constexpr std::size_t maximum_file_size = 16U * 1024U * 1024U;
constexpr std::size_t decoded_pixel_count =
	static_cast<std::size_t>(new_classic_picture_patch_width) *
	new_classic_picture_patch_height;

[[nodiscard]] NewClassicPicturePatchError error(
	NewClassicPicturePatchErrorCode code,
	std::size_t offset = 0U,
	std::size_t member = 0U,
	std::error_code system = {}
) noexcept
{
	return {code, offset, member, system};
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
	std::uint32_t result = 0U;
	for(std::size_t index = 0U; index < 4U; ++index) {
		result |= static_cast<std::uint32_t>(
			std::to_integer<std::uint8_t>(bytes[offset + index])
		) << (index * 8U);
	}
	return result;
}

[[nodiscard]] bool canonical_member_character(char value) noexcept
{
	return (value >= 'A' && value <= 'Z') ||
		(value >= '0' && value <= '9') || value == '_' || value == '.';
}

[[nodiscard]] std::string normalized_member(std::string_view member)
{
	std::string result(member);
	for(auto &value : result) {
		if(value >= 'a' && value <= 'z') {
			value = static_cast<char>(value - ('a' - 'A'));
		}
	}
	return result;
}

[[nodiscard]] bool valid_locale(std::string_view locale) noexcept
{
	if(locale.empty() || locale.size() > 15U) {
		return false;
	}
	return std::ranges::all_of(locale, [](char value) {
		return (value >= 'A' && value <= 'Z') ||
			(value >= 'a' && value <= 'z') ||
			(value >= '0' && value <= '9') || value == '-';
	});
}

[[nodiscard]] bool range_within(
	std::size_t offset,
	std::size_t length,
	std::size_t size
) noexcept
{
	return offset <= size && length <= size - offset;
}

} // namespace

std::expected<AppSha256, NewClassicPicturePatchError>
new_classic_picture_source_sha256(const assets::PiImage &image)
{
	if(!image.palette || image.image.width != new_classic_picture_patch_width ||
		image.image.height != new_classic_picture_patch_height ||
		image.image.indices.size() != decoded_pixel_count ||
		!std::ranges::all_of(image.image.indices, [](std::uint8_t value) {
			return value < 16U;
		})) {
		return std::unexpected(error(
			NewClassicPicturePatchErrorCode::image_shape
		));
	}
	try {
		std::vector<std::byte> canonical{};
		canonical.reserve(4U + 48U + decoded_pixel_count);
		canonical.push_back(std::byte{
			static_cast<std::uint8_t>(image.image.width & 0xFFU)
		});
		canonical.push_back(std::byte{
			static_cast<std::uint8_t>(image.image.width >> 8U)
		});
		canonical.push_back(std::byte{
			static_cast<std::uint8_t>(image.image.height & 0xFFU)
		});
		canonical.push_back(std::byte{
			static_cast<std::uint8_t>(image.image.height >> 8U)
		});
		for(const auto &color : image.palette->colors) {
			canonical.push_back(std::byte{color.r});
			canonical.push_back(std::byte{color.g});
			canonical.push_back(std::byte{color.b});
		}
		for(const auto index : image.image.indices) {
			canonical.push_back(std::byte{index});
		}
		return app_sha256(canonical);
	} catch(const std::bad_alloc &) {
		return std::unexpected(error(
			NewClassicPicturePatchErrorCode::allocation_failed
		));
	}
}

std::string_view NewClassicPicturePatchCatalog::locale() const noexcept
{
	return locale_;
}

std::size_t NewClassicPicturePatchCatalog::picture_count() const noexcept
{
	return pictures_.size();
}

std::expected<bool, NewClassicPicturePatchError>
NewClassicPicturePatchCatalog::apply(
	std::string_view member,
	assets::PiImage &image
) const
{
	std::string normalized{};
	try {
		normalized = normalized_member(member);
	} catch(const std::bad_alloc &) {
		return std::unexpected(error(
			NewClassicPicturePatchErrorCode::allocation_failed
		));
	}
	const auto found = std::ranges::find_if(pictures_, [&](const Picture &value) {
		return value.member == normalized;
	});
	if(found == pictures_.end()) {
		return false;
	}
	const auto digest = new_classic_picture_source_sha256(image);
	if(!digest) {
		return std::unexpected(digest.error());
	}
	if(*digest != found->source_sha256) {
		return std::unexpected(error(
			NewClassicPicturePatchErrorCode::source_digest_mismatch,
			0U,
			static_cast<std::size_t>(found - pictures_.begin())
		));
	}
	for(const auto &run : found->runs) {
		std::ranges::copy(
			run.indices,
			image.image.indices.begin() + run.pixel_offset
		);
	}
	return true;
}

std::expected<NewClassicPicturePatchCatalog, NewClassicPicturePatchError>
load_new_classic_picture_patch(const std::filesystem::path &path)
{
	if(path.empty()) {
		return std::unexpected(error(
			NewClassicPicturePatchErrorCode::invalid_path
		));
	}
	std::error_code system{};
	const auto status = std::filesystem::status(path, system);
	if(system) {
		return std::unexpected(error(
			NewClassicPicturePatchErrorCode::source_status, 0U, 0U, system
		));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(error(
			NewClassicPicturePatchErrorCode::source_not_regular
		));
	}
	const auto file_size = std::filesystem::file_size(path, system);
	if(system) {
		return std::unexpected(error(
			NewClassicPicturePatchErrorCode::source_size, 0U, 0U, system
		));
	}
	if(file_size < header_size || file_size > maximum_file_size ||
		file_size > std::numeric_limits<std::size_t>::max()) {
		return std::unexpected(error(
			NewClassicPicturePatchErrorCode::source_size
		));
	}
	try {
		std::vector<std::byte> storage(static_cast<std::size_t>(file_size));
		std::ifstream source(path, std::ios::binary);
		if(!source) {
			return std::unexpected(error(
				NewClassicPicturePatchErrorCode::source_open
			));
		}
		source.read(
			reinterpret_cast<char *>(storage.data()),
			static_cast<std::streamsize>(storage.size())
		);
		if(!source || source.peek() != std::ifstream::traits_type::eof()) {
			return std::unexpected(error(
				NewClassicPicturePatchErrorCode::source_read
			));
		}
		const std::span<const std::byte> bytes(storage);
		if(!std::ranges::equal(magic, bytes.first(magic.size())) ||
			u32(bytes, 8U) != header_size || u32(bytes, 28U) != bytes.size()) {
			return std::unexpected(error(
				NewClassicPicturePatchErrorCode::header_shape
			));
		}
		const auto picture_count = static_cast<std::size_t>(u32(bytes, 12U));
		const auto run_count = static_cast<std::size_t>(u32(bytes, 16U));
		const auto picture_offset = static_cast<std::size_t>(u32(bytes, 20U));
		const auto run_offset = static_cast<std::size_t>(u32(bytes, 24U));
		const auto payload_offset = static_cast<std::size_t>(u32(bytes, 32U));
		if(picture_offset != header_size || picture_count > 24U ||
			run_count > decoded_pixel_count ||
			!range_within(picture_offset, picture_count * picture_entry_size, bytes.size()) ||
			run_offset != picture_offset + picture_count * picture_entry_size ||
			!range_within(run_offset, run_count * run_entry_size, bytes.size()) ||
			payload_offset != run_offset + run_count * run_entry_size) {
			return std::unexpected(error(
				NewClassicPicturePatchErrorCode::directory_shape
			));
		}
		AppSha256 expected_body{};
		for(std::size_t index = 0U; index < expected_body.size(); ++index) {
			expected_body[index] = std::to_integer<std::uint8_t>(bytes[36U + index]);
		}
		if(app_sha256(bytes.subspan(header_size)) != expected_body) {
			return std::unexpected(error(
				NewClassicPicturePatchErrorCode::body_digest_mismatch, 36U
			));
		}
		const auto locale_bytes = bytes.subspan(68U, 16U);
		const auto locale_end = std::ranges::find(locale_bytes, std::byte{0U});
		const auto locale_length = static_cast<std::size_t>(
			locale_end - locale_bytes.begin()
		);
		std::string locale{};
		locale.reserve(locale_length);
		for(std::size_t index = 0U; index < locale_length; ++index) {
			locale.push_back(static_cast<char>(
				std::to_integer<std::uint8_t>(bytes[68U + index])
			));
		}
		if(!valid_locale(locale) || !std::ranges::all_of(
			bytes.subspan(68U + locale_length, 28U - locale_length),
			[](std::byte value) { return value == std::byte{0U}; }
		)) {
			return std::unexpected(error(
				NewClassicPicturePatchErrorCode::locale_shape, 68U
			));
		}

		NewClassicPicturePatchCatalog result{};
		result.locale_ = std::move(locale);
		result.pictures_.reserve(picture_count);
		std::size_t expected_first_run = 0U;
		std::size_t expected_payload = 0U;
		std::string previous_member{};
		for(std::size_t picture_index = 0U;
			picture_index < picture_count;
			++picture_index) {
			const auto entry = picture_offset + picture_index * picture_entry_size;
			const auto name_bytes = bytes.subspan(entry, 16U);
			const auto name_end = std::ranges::find(name_bytes, std::byte{0U});
			const auto name_length = static_cast<std::size_t>(name_end - name_bytes.begin());
			std::string name{};
			name.reserve(name_length);
			for(std::size_t index = 0U; index < name_length; ++index) {
				name.push_back(static_cast<char>(
					std::to_integer<std::uint8_t>(name_bytes[index])
				));
			}
			const auto first_run = static_cast<std::size_t>(u32(bytes, entry + 48U));
			const auto member_runs = static_cast<std::size_t>(u32(bytes, entry + 52U));
			const auto left = u16(bytes, entry + 56U);
			const auto top = u16(bytes, entry + 58U);
			const auto right = u16(bytes, entry + 60U);
			const auto bottom = u16(bytes, entry + 62U);
			const auto declared_pixels = static_cast<std::size_t>(u32(bytes, entry + 64U));
			if(name.empty() || name.size() > 15U ||
				!std::ranges::all_of(name, canonical_member_character) ||
				name.size() < 4U || name.substr(name.size() - 3U) != ".PI" ||
				(!previous_member.empty() && name <= previous_member) ||
				first_run != expected_first_run || member_runs == 0U ||
				member_runs > run_count - first_run || left >= right || top >= bottom ||
				right > new_classic_picture_patch_width ||
				bottom > new_classic_picture_patch_height ||
				declared_pixels == 0U ||
				!std::ranges::all_of(
					name_bytes.subspan(name_length),
					[](std::byte value) { return value == std::byte{0U}; }
				) || !std::ranges::all_of(
					bytes.subspan(entry + 68U, 12U),
					[](std::byte value) { return value == std::byte{0U}; }
				)) {
				return std::unexpected(error(
					NewClassicPicturePatchErrorCode::member_shape,
					entry,
					picture_index
				));
			}
			NewClassicPicturePatchCatalog::Picture picture{};
			picture.member = name;
			for(std::size_t index = 0U; index < picture.source_sha256.size(); ++index) {
				picture.source_sha256[index] =
					std::to_integer<std::uint8_t>(bytes[entry + 16U + index]);
			}
			picture.runs.reserve(member_runs);
			std::size_t previous_end = 0U;
			std::size_t actual_pixels = 0U;
			std::uint16_t actual_left = new_classic_picture_patch_width;
			std::uint16_t actual_top = new_classic_picture_patch_height;
			std::uint16_t actual_right = 0U;
			std::uint16_t actual_bottom = 0U;
			for(std::size_t local = 0U; local < member_runs; ++local) {
				const auto run_index = first_run + local;
				const auto run = run_offset + run_index * run_entry_size;
				const auto pixel = static_cast<std::size_t>(u32(bytes, run));
				const auto length = static_cast<std::size_t>(u16(bytes, run + 4U));
				const auto payload = static_cast<std::size_t>(u32(bytes, run + 8U));
				const auto packed = (length + 1U) / 2U;
				if(length == 0U || pixel >= decoded_pixel_count ||
					length > decoded_pixel_count - pixel ||
					pixel / new_classic_picture_patch_width !=
						(pixel + length - 1U) / new_classic_picture_patch_width ||
					(local != 0U && pixel < previous_end) ||
					payload != expected_payload ||
					!range_within(payload_offset + payload, packed, bytes.size()) ||
					u16(bytes, run + 6U) != 0U) {
					return std::unexpected(error(
						NewClassicPicturePatchErrorCode::run_shape,
						run,
						picture_index
					));
				}
				NewClassicPicturePatchCatalog::Run decoded{};
				decoded.pixel_offset = static_cast<std::uint32_t>(pixel);
				decoded.indices.reserve(length);
				for(std::size_t index = 0U; index < length; ++index) {
					const auto value = std::to_integer<std::uint8_t>(
						bytes[payload_offset + payload + index / 2U]
					);
					decoded.indices.push_back(static_cast<std::uint8_t>(
						(index % 2U == 0U) ? value >> 4U : value & 0x0FU
					));
				}
				if(length % 2U != 0U &&
					(std::to_integer<std::uint8_t>(
						bytes[payload_offset + payload + packed - 1U]
					) & 0x0FU) != 0U) {
					return std::unexpected(error(
						NewClassicPicturePatchErrorCode::run_shape,
						payload_offset + payload + packed - 1U,
						picture_index
					));
				}
				const auto run_left = static_cast<std::uint16_t>(
					pixel % new_classic_picture_patch_width
				);
				const auto run_top = static_cast<std::uint16_t>(
					pixel / new_classic_picture_patch_width
				);
				actual_left = std::min(actual_left, run_left);
				actual_top = std::min(actual_top, run_top);
				actual_right = std::max(actual_right, static_cast<std::uint16_t>(
					run_left + length
				));
				actual_bottom = std::max(actual_bottom, static_cast<std::uint16_t>(
					run_top + 1U
				));
				actual_pixels += length;
				previous_end = pixel + length;
				expected_payload += packed;
				picture.runs.push_back(std::move(decoded));
			}
			if(actual_pixels != declared_pixels || actual_left != left ||
				actual_top != top || actual_right != right || actual_bottom != bottom) {
				return std::unexpected(error(
					NewClassicPicturePatchErrorCode::member_shape,
					entry + 56U,
					picture_index
				));
			}
			expected_first_run += member_runs;
			previous_member = name;
			result.pictures_.push_back(std::move(picture));
		}
		if(expected_first_run != run_count ||
			payload_offset + expected_payload != bytes.size()) {
			return std::unexpected(error(
				NewClassicPicturePatchErrorCode::directory_shape, run_offset
			));
		}
		return result;
	} catch(const std::bad_alloc &) {
		return std::unexpected(error(
			NewClassicPicturePatchErrorCode::allocation_failed
		));
	}
}

} // namespace th03::app
