#include "th03/app/new_classic_character_select_assets.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <new>
#include <optional>
#include <ranges>
#include <variant>
#include <vector>

#include "th03/assets/decode.hpp"

namespace th03::app {
namespace {

constexpr std::array<std::byte, 8U> magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'C'}, std::byte{'S'},
	std::byte{'P'}, std::byte{'0'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::size_t header_size = 192U;
constexpr std::size_t maximum_file_size = 1U * 1024U * 1024U;
constexpr std::uint32_t english_source_profile = 1U;
constexpr std::uint32_t typography_contract_version = 3U;
constexpr std::uint16_t stat_storage_width = 64U;
constexpr std::uint16_t stat_height = 48U;
constexpr std::uint8_t stat_foreground = 7U;
constexpr std::uint8_t stat_background = 14U;

[[nodiscard]] NewClassicCharacterSelectAssetsError error(
	NewClassicCharacterSelectAssetsErrorCode code,
	std::size_t offset = 0U,
	std::error_code system = {}
) noexcept
{
	return {.code = code, .offset = offset, .system = system};
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

[[nodiscard]] bool valid_locale(std::string_view locale) noexcept
{
	return !locale.empty() && locale.size() <= 15U &&
		std::ranges::all_of(locale, [](char value) {
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

[[nodiscard]] bool bit(
	std::span<const std::uint8_t> plane,
	const assets::PlanarCel4 &cel,
	std::size_t x,
	std::size_t y
) noexcept
{
	const auto stride = static_cast<std::size_t>(cel.pixel_width) / 8U;
	const auto stored_y = static_cast<std::size_t>(cel.pixel_height) - 1U - y;
	return (plane[(stored_y * stride) + (x / 8U)] &
		static_cast<std::uint8_t>(0x80U >> (x & 7U))) != 0U;
}

[[nodiscard]] std::uint8_t color(
	const assets::PlanarCel4 &cel,
	std::size_t x,
	std::size_t y
) noexcept
{
	std::uint8_t result = 0U;
	for(std::size_t plane = 0U; plane < cel.color_planes.size(); ++plane) {
		if(bit(cel.color_planes[plane], cel, x, y)) {
			result = static_cast<std::uint8_t>(result | (1U << plane));
		}
	}
	return result;
}

[[nodiscard]] bool valid_stat_asset(const assets::PlanarCel4 &cel) noexcept
{
	if(cel.pixel_width != stat_storage_width || cel.pixel_height != stat_height ||
		!cel.alpha || cel.alpha->size() != cel.bitplane_size) {
		return false;
	}
	std::array<bool, 3U> foreground_seen{};
	for(std::size_t y = 0U; y < stat_height; ++y) {
		for(std::size_t x = 0U; x < stat_storage_width; ++x) {
			const auto alpha = bit(*cel.alpha, cel, x, y);
			const auto value = color(cel, x, y);
			if(x < new_classic_character_select_stat_width) {
				if(!alpha || (value != stat_foreground && value != stat_background)) {
					return false;
				}
				if(value == stat_foreground) {
					foreground_seen[y / 16U] = true;
				}
			} else if(alpha || value != 0U) {
				return false;
			}
		}
	}
	return std::ranges::all_of(foreground_seen, [](bool value) { return value; });
}

} // namespace

std::string_view NewClassicCharacterSelectAssets::locale() const noexcept
{
	return locale_;
}

const assets::PlanarCel4 &
NewClassicCharacterSelectAssets::stat_labels() const noexcept
{
	return stat_labels_;
}

bool NewClassicCharacterSelectAssets::matches_source(
	std::span<const std::byte> names,
	std::span<const std::byte> stats,
	std::span<const std::byte> palette
) const noexcept
{
	try {
		return app_sha256(names) == source_name_sha256_ &&
			app_sha256(stats) == source_stats_sha256_ &&
			app_sha256(palette) == source_palette_sha256_;
	} catch(const std::bad_alloc &) {
		return false;
	}
}

std::expected<
	NewClassicCharacterSelectAssets,
	NewClassicCharacterSelectAssetsError
> load_new_classic_character_select_assets(const std::filesystem::path &path)
{
	if(path.empty()) {
		return std::unexpected(error(
			NewClassicCharacterSelectAssetsErrorCode::invalid_path
		));
	}
	std::error_code system{};
	const auto status = std::filesystem::status(path, system);
	if(system) {
		return std::unexpected(error(
			NewClassicCharacterSelectAssetsErrorCode::source_status, 0U, system
		));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(error(
			NewClassicCharacterSelectAssetsErrorCode::source_not_regular
		));
	}
	const auto file_size = std::filesystem::file_size(path, system);
	if(system) {
		return std::unexpected(error(
			NewClassicCharacterSelectAssetsErrorCode::source_size, 0U, system
		));
	}
	if(file_size < header_size || file_size > maximum_file_size ||
		file_size > std::numeric_limits<std::size_t>::max()) {
		return std::unexpected(error(
			NewClassicCharacterSelectAssetsErrorCode::source_size
		));
	}
	try {
		std::vector<std::byte> storage(static_cast<std::size_t>(file_size));
		std::ifstream input(path, std::ios::binary);
		if(!input) {
			return std::unexpected(error(
				NewClassicCharacterSelectAssetsErrorCode::source_open
			));
		}
		input.read(
			reinterpret_cast<char *>(storage.data()),
			static_cast<std::streamsize>(storage.size())
		);
		if(!input || input.peek() != std::ifstream::traits_type::eof()) {
			return std::unexpected(error(
				NewClassicCharacterSelectAssetsErrorCode::source_read
			));
		}
		const std::span<const std::byte> bytes(storage);
		if(!std::ranges::equal(bytes.first(magic.size()), magic) ||
			u32(bytes, 8U) != header_size || u32(bytes, 12U) != bytes.size() ||
			u32(bytes, 32U) != english_source_profile ||
			u32(bytes, 36U) != typography_contract_version ||
			!std::ranges::all_of(bytes.subspan(184U, 8U), [](std::byte value) {
				return value == std::byte{0U};
			})) {
			return std::unexpected(error(
				NewClassicCharacterSelectAssetsErrorCode::header_shape
			));
		}
		const auto name_offset = static_cast<std::size_t>(u32(bytes, 16U));
		const auto name_size = static_cast<std::size_t>(u32(bytes, 20U));
		const auto stat_offset = static_cast<std::size_t>(u32(bytes, 24U));
		const auto stat_size = static_cast<std::size_t>(u32(bytes, 28U));
		if(name_offset != 0U || name_size != 0U || stat_size == 0U ||
			stat_offset != header_size ||
			!range_within(stat_offset, stat_size, bytes.size()) ||
			stat_offset + stat_size != bytes.size()) {
			return std::unexpected(error(
				NewClassicCharacterSelectAssetsErrorCode::header_shape, 16U
			));
		}
		AppSha256 expected_body{};
		for(std::size_t index = 0U; index < expected_body.size(); ++index) {
			expected_body[index] = std::to_integer<std::uint8_t>(bytes[40U + index]);
		}
		if(app_sha256(bytes.subspan(header_size)) != expected_body) {
			return std::unexpected(error(
				NewClassicCharacterSelectAssetsErrorCode::body_digest_mismatch, 40U
			));
		}
		const auto locale_bytes = bytes.subspan(72U, 16U);
		const auto locale_end = std::ranges::find(locale_bytes, std::byte{0U});
		const auto locale_size = static_cast<std::size_t>(
			locale_end - locale_bytes.begin()
		);
		std::string locale{};
		locale.reserve(locale_size);
		for(std::size_t index = 0U; index < locale_size; ++index) {
			locale.push_back(static_cast<char>(
				std::to_integer<std::uint8_t>(locale_bytes[index])
			));
		}
		if(!valid_locale(locale) || !std::ranges::all_of(
			locale_bytes.subspan(locale_size), [](std::byte value) {
				return value == std::byte{0U};
			})) {
			return std::unexpected(error(
				NewClassicCharacterSelectAssetsErrorCode::locale_shape, 72U
			));
		}
		auto stats = assets::decode_cdg(
			bytes.subspan(stat_offset, stat_size),
			assets::CdgAlphaPolicy::preserve
		);
		if(!stats) {
			auto failure = error(
				NewClassicCharacterSelectAssetsErrorCode::asset_decode_failed,
				stat_offset + stats.error().offset
			);
			failure.asset_error.emplace(std::move(stats.error()));
			return std::unexpected(std::move(failure));
		}
		if(stats->cels.size() != 1U || !valid_stat_asset(stats->cels.front())) {
			return std::unexpected(error(
				NewClassicCharacterSelectAssetsErrorCode::stat_asset_shape,
				stat_offset
			));
		}
		NewClassicCharacterSelectAssets result{};
		result.locale_ = std::move(locale);
		for(std::size_t index = 0U; index < 32U; ++index) {
			result.source_name_sha256_[index] =
				std::to_integer<std::uint8_t>(bytes[88U + index]);
			result.source_stats_sha256_[index] =
				std::to_integer<std::uint8_t>(bytes[120U + index]);
			result.source_palette_sha256_[index] =
				std::to_integer<std::uint8_t>(bytes[152U + index]);
		}
		result.stat_labels_ = std::move(stats->cels.front());
		return result;
	} catch(const std::bad_alloc &) {
		return std::unexpected(error(
			NewClassicCharacterSelectAssetsErrorCode::allocation_failed
		));
	}
}

} // namespace th03::app
