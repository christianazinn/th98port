#include "th03/app/new_classic/content/new_classic_game_text.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/app/content/main_compiled_content.hpp"

namespace th03::app {

struct NewClassicGameTextCatalogAccess final {
	static void assign(
		NewClassicGameTextCatalog &catalog,
		std::string locale,
		std::string fallback,
		std::array<
			std::vector<std::uint8_t>, new_classic_game_text_count
		> messages
	)
	{
		catalog.locale_ = std::move(locale);
		catalog.fallback_locale_ = std::move(fallback);
		catalog.messages_ = std::move(messages);
	}
};

namespace {

constexpr std::size_t header_bytes = 96U;
constexpr std::size_t directory_entry_bytes = 8U;
constexpr std::size_t maximum_pack_bytes = 1U << 20U;
constexpr std::array<std::byte, 8U> magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'G'}, std::byte{'T'},
	std::byte{'L'}, std::byte{'0'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::size_t game_text_count = new_classic_game_text_count;

constexpr std::array<NewClassicGameTextSchemaEntry, game_text_count> schema{{
#define TH03_NEW_CLASSIC_GAME_TEXT(name, identifier, exact_bytes) \
	{identifier, exact_bytes},
#include "th03/app/new_classic/content/new_classic_game_text_ids.inc"
#undef TH03_NEW_CLASSIC_GAME_TEXT
}};

#include "new_classic_game_text_generated.inc"

[[nodiscard]] NewClassicGameTextPackError error(
	NewClassicGameTextPackErrorCode code,
	std::size_t offset = 0U,
	std::error_code system = {},
	NewClassicGameTextId message = NewClassicGameTextId::count
) noexcept
{
	return {.code = code, .offset = offset, .system = system, .message = message};
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

[[nodiscard]] bool valid_locale(std::string_view locale, bool empty) noexcept
{
	if(locale.empty()) {
		return empty;
	}
	if(locale.size() > 31U || locale.front() == '-' || locale.back() == '-') {
		return false;
	}
	std::size_t component = 0U;
	bool first_component = true;
	for(const auto character : locale) {
		if(character == '-') {
			if(component < (first_component ? 2U : 1U) || component > 8U) {
				return false;
			}
			component = 0U;
			first_component = false;
			continue;
		}
		const auto value = static_cast<unsigned char>(character);
		if(!((value >= 'A' && value <= 'Z') ||
			(value >= 'a' && value <= 'z') ||
			(value >= '0' && value <= '9'))) {
			return false;
		}
		++component;
	}
	return component >= (first_component ? 2U : 1U) && component <= 8U;
}

[[nodiscard]] bool valid_shift_jis(
	std::span<const std::uint8_t> text,
	std::size_t &invalid_offset
) noexcept
{
	std::size_t offset = 0U;
	while(offset < text.size()) {
		const auto first = text[offset];
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
		const auto second = text[offset + 1U];
		if(!((second >= 0x40U && second <= 0x7EU) ||
			(second >= 0x80U && second <= 0xFCU))) {
			invalid_offset = offset + 1U;
			return false;
		}
		offset += 2U;
	}
	return true;
}

[[nodiscard]] std::expected<void, NewClassicGameTextPackError>
validate_message(
	NewClassicGameTextId id,
	std::span<const std::uint8_t> text
) noexcept
{
	const auto index = static_cast<std::size_t>(id);
	if(index >= schema.size() || text.size() != schema[index].exact_bytes) {
		return std::unexpected(error(
			NewClassicGameTextPackErrorCode::length_mismatch, 0U, {}, id
		));
	}
	std::size_t invalid_offset = 0U;
	if(!valid_shift_jis(text, invalid_offset)) {
		return std::unexpected(error(
			NewClassicGameTextPackErrorCode::malformed_shift_jis,
			invalid_offset, {}, id
		));
	}
	return {};
}

[[nodiscard]] NewClassicGameTextCatalog builtin(
	std::string locale,
	const std::array<std::string_view, game_text_count> &source
)
{
	std::array<std::vector<std::uint8_t>, game_text_count> messages{};
	for(std::size_t index = 0U; index < messages.size(); ++index) {
		const auto value = source[index];
		messages[index].assign(value.begin(), value.end());
	}
	NewClassicGameTextCatalog result{};
	NewClassicGameTextCatalogAccess::assign(
		result, std::move(locale), "", std::move(messages)
	);
	return result;
}

} // namespace

std::string_view NewClassicGameTextCatalog::locale() const noexcept
{
	return locale_;
}

std::string_view NewClassicGameTextCatalog::fallback_locale() const noexcept
{
	return fallback_locale_;
}

std::span<const std::uint8_t> NewClassicGameTextCatalog::bytes(
	NewClassicGameTextId id
) const noexcept
{
	const auto index = static_cast<std::size_t>(id);
	return index < messages_.size() ?
		std::span<const std::uint8_t>{messages_[index]} :
		std::span<const std::uint8_t>{};
}

const NewClassicGameTextSchemaEntry &new_classic_game_text_schema(
	NewClassicGameTextId id
) noexcept
{
	const auto index = static_cast<std::size_t>(id);
	return schema[index < schema.size() ? index : 0U];
}

const NewClassicGameTextCatalog &new_classic_builtin_english_game_text() noexcept
{
	static const auto catalog = builtin("en-US", builtin_english_game_text);
	return catalog;
}

const NewClassicGameTextCatalog &new_classic_builtin_japanese_game_text() noexcept
{
	static const auto catalog = builtin("ja-JP", builtin_japanese_game_text);
	return catalog;
}

std::expected<NewClassicGameTextCatalog, NewClassicGameTextPackError>
load_new_classic_game_text_pack(
	const std::filesystem::path &path,
	const NewClassicGameTextCatalog *fallback
)
{
	if(path.empty()) {
		return std::unexpected(error(
			NewClassicGameTextPackErrorCode::invalid_path
		));
	}
	std::error_code system;
	const auto status = std::filesystem::status(path, system);
	if(system) {
		return std::unexpected(error(
			NewClassicGameTextPackErrorCode::source_status, 0U, system
		));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(error(
			NewClassicGameTextPackErrorCode::source_not_regular
		));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system) {
		return std::unexpected(error(
			NewClassicGameTextPackErrorCode::source_size, 0U, system
		));
	}
	if(size < header_bytes || size > maximum_pack_bytes) {
		return std::unexpected(error(
			NewClassicGameTextPackErrorCode::source_size
		));
	}
	try {
		std::ifstream input(path, std::ios::binary);
		if(!input) {
			return std::unexpected(error(
				NewClassicGameTextPackErrorCode::source_open
			));
		}
		std::vector<std::byte> bytes(static_cast<std::size_t>(size));
		input.read(reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size()));
		if(!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
			return std::unexpected(error(
				NewClassicGameTextPackErrorCode::source_read
			));
		}
		if(!std::ranges::equal(magic, std::span<const std::byte>{bytes}.first(8U)) ||
			u16(bytes, 8U) != 1U || u16(bytes, 10U) != header_bytes ||
			u16(bytes, 12U) != game_text_count || !zero_range(bytes, 17U, 20U) ||
			!zero_range(bytes, 28U, 32U)) {
			return std::unexpected(error(
				NewClassicGameTextPackErrorCode::header_shape
			));
		}
		if(std::to_integer<std::uint8_t>(bytes[14U]) != 1U) {
			return std::unexpected(error(
				NewClassicGameTextPackErrorCode::encoding, 14U
			));
		}
		if(!std::ranges::equal(
			game_text_schema_sha256,
			std::span<const std::byte>{bytes}.subspan(32U, 32U),
			{}, [](std::uint8_t value) { return static_cast<std::byte>(value); },
			[](std::byte value) { return value; }
		)) {
			return std::unexpected(error(
				NewClassicGameTextPackErrorCode::schema_mismatch, 32U
			));
		}
		const auto locale_bytes = std::to_integer<std::uint8_t>(bytes[15U]);
		const auto fallback_bytes = std::to_integer<std::uint8_t>(bytes[16U]);
		const auto directory_bytes = static_cast<std::size_t>(u32(bytes, 20U));
		const auto payload_bytes = static_cast<std::size_t>(u32(bytes, 24U));
		if(locale_bytes == 0U || locale_bytes > 31U || fallback_bytes > 31U ||
			directory_bytes != game_text_count * directory_entry_bytes ||
			header_bytes + locale_bytes + fallback_bytes + directory_bytes +
				payload_bytes != bytes.size()) {
			return std::unexpected(error(
				NewClassicGameTextPackErrorCode::directory_shape
			));
		}
		const auto body = std::span<const std::byte>{bytes}.subspan(header_bytes);
		const auto body_digest = app_sha256(body);
		if(!std::ranges::equal(
			body_digest, std::span<const std::byte>{bytes}.subspan(64U, 32U),
			{}, [](std::uint8_t value) { return static_cast<std::byte>(value); },
			[](std::byte value) { return value; }
		)) {
			return std::unexpected(error(
				NewClassicGameTextPackErrorCode::body_digest_mismatch, 64U
			));
		}
		const auto locale_offset = header_bytes;
		const auto fallback_offset = locale_offset + locale_bytes;
		const auto directory_offset = fallback_offset + fallback_bytes;
		const auto payload_offset = directory_offset + directory_bytes;
		const std::string locale{
			reinterpret_cast<const char *>(bytes.data() + locale_offset), locale_bytes
		};
		const std::string fallback_locale{
			reinterpret_cast<const char *>(bytes.data() + fallback_offset),
			fallback_bytes
		};
		if(!valid_locale(locale, false) ||
			!valid_locale(fallback_locale, true) || locale == fallback_locale) {
			return std::unexpected(error(
				NewClassicGameTextPackErrorCode::locale_shape, locale_offset
			));
		}
		if(!fallback_locale.empty() &&
			(!fallback || fallback->locale() != fallback_locale)) {
			return std::unexpected(error(
				NewClassicGameTextPackErrorCode::fallback_missing, fallback_offset
			));
		}
		std::array<std::vector<std::uint8_t>, game_text_count> messages{};
		std::size_t expected_payload_offset = 0U;
		for(std::size_t index = 0U; index < messages.size(); ++index) {
			const auto entry = directory_offset + index * directory_entry_bytes;
			const auto offset = static_cast<std::size_t>(u32(bytes, entry));
			const auto length = static_cast<std::size_t>(u16(bytes, entry + 4U));
			const auto id = static_cast<NewClassicGameTextId>(index);
			if(u16(bytes, entry + 6U) != 0U) {
				return std::unexpected(error(
					NewClassicGameTextPackErrorCode::directory_shape, entry, {}, id
				));
			}
			if(length == 0U) {
				if(offset != 0U || !fallback) {
					return std::unexpected(error(
						NewClassicGameTextPackErrorCode::fallback_missing, entry, {}, id
					));
				}
				const auto source = fallback->bytes(id);
				messages[index].assign(source.begin(), source.end());
			} else {
				if(offset != expected_payload_offset || length > payload_bytes - offset) {
					return std::unexpected(error(
						NewClassicGameTextPackErrorCode::message_shape, entry, {}, id
					));
				}
				const auto source = std::span<const std::byte>{bytes}.subspan(
					payload_offset + offset, length
				);
				messages[index].reserve(length);
				std::ranges::transform(source, std::back_inserter(messages[index]),
					[](std::byte value) { return std::to_integer<std::uint8_t>(value); });
				expected_payload_offset += length;
			}
			if(auto valid = validate_message(id, messages[index]); !valid) {
				auto result = valid.error();
				result.offset += entry;
				return std::unexpected(std::move(result));
			}
		}
		if(expected_payload_offset != payload_bytes) {
			return std::unexpected(error(
				NewClassicGameTextPackErrorCode::message_shape, payload_offset
			));
		}
		NewClassicGameTextCatalog result{};
		NewClassicGameTextCatalogAccess::assign(
			result, locale, fallback_locale, std::move(messages)
		);
		return result;
	} catch(const std::bad_alloc &) {
		return std::unexpected(error(
			NewClassicGameTextPackErrorCode::allocation_failed
		));
	}
}

} // namespace th03::app
