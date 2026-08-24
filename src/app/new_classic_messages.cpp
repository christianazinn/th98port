#include "th03/app/new_classic_messages.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/app/main_compiled_content.hpp"
#include "th03/app/new_classic_assets.hpp"

namespace th03::app {

struct NewClassicMessageCatalogAccess final {
	static void assign(
		NewClassicMessageCatalog &catalog,
		std::string locale,
		std::string fallback,
		NewClassicLanguageAssetProfile profile,
		NewClassicMessageEncoding encoding,
		std::array<std::string, new_classic_message_count> messages
	)
	{
		catalog.locale_ = std::move(locale);
		catalog.fallback_locale_ = std::move(fallback);
		catalog.asset_profile_ = profile;
		catalog.encoding_ = encoding;
		catalog.messages_ = std::move(messages);
	}
};

namespace {

constexpr std::size_t header_bytes = 96U;
constexpr std::size_t directory_entry_bytes = 8U;
constexpr std::size_t maximum_pack_bytes = 1U << 20U;
constexpr std::array<std::byte, 8U> magic{
	std::byte{'T'}, std::byte{'3'}, std::byte{'L'}, std::byte{'N'},
	std::byte{'G'}, std::byte{'0'}, std::byte{'0'}, std::byte{'1'},
};
constexpr std::size_t message_count = new_classic_message_count;

constexpr std::array<NewClassicMessageSchemaEntry, message_count> schema{{
#define TH03_NEW_CLASSIC_MESSAGE( \
	name, identifier, width, placeholders, width_values \
) {identifier, width, placeholders, width_values},
#include "th03/app/new_classic_message_ids.inc"
#undef TH03_NEW_CLASSIC_MESSAGE
}};

#include "new_classic_messages_generated.inc"

[[nodiscard]] NewClassicMessagePackError error(
	NewClassicMessagePackErrorCode code,
	std::size_t offset = 0U,
	std::error_code system = {},
	NewClassicMessageId message = NewClassicMessageId::count
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

[[nodiscard]] bool valid_glyphs(
	std::string_view text,
	NewClassicMessageEncoding encoding,
	const NewClassicMenuFont &font
) noexcept
{
	if(text.empty()) {
		return false;
	}
	if(encoding == NewClassicMessageEncoding::utf8) {
		return font.has_unicode_font() && font.supports_utf8(text);
	}
	for(std::size_t offset = 0U; offset < text.size();) {
		const auto value = static_cast<std::uint8_t>(text[offset]);
		if(value >= NewClassicMenuFont::first_character &&
			value <= NewClassicMenuFont::last_character) {
			++offset;
			continue;
		}
		if(encoding != NewClassicMessageEncoding::cp932_mixed ||
			!font.has_machine_font()) {
			return false;
		}
		if(value >= 0xA1U && value <= 0xDFU) {
			++offset;
			continue;
		}
		const auto lead = (value >= 0x81U && value <= 0x9FU) ||
			(value >= 0xE0U && value <= 0xFCU);
		if(!lead || offset + 1U >= text.size()) {
			return false;
		}
		const auto trail = static_cast<std::uint8_t>(text[offset + 1U]);
		if(!((trail >= 0x40U && trail <= 0x7EU) ||
			(trail >= 0x80U && trail <= 0xFCU))) {
			return false;
		}
		offset += 2U;
	}
	return true;
}

[[nodiscard]] bool cp932_pair_at(
	std::string_view text,
	std::size_t offset,
	NewClassicMessageEncoding encoding
) noexcept
{
	if(encoding != NewClassicMessageEncoding::cp932_mixed ||
		offset + 1U >= text.size()) {
		return false;
	}
	const auto lead = static_cast<std::uint8_t>(text[offset]);
	const auto trail = static_cast<std::uint8_t>(text[offset + 1U]);
	return ((lead >= 0x81U && lead <= 0x9FU) ||
		(lead >= 0xE0U && lead <= 0xFCU)) &&
		((trail >= 0x40U && trail <= 0x7EU) ||
		 (trail >= 0x80U && trail <= 0xFCU));
}

[[nodiscard]] std::string placeholder_set(
	std::string_view text,
	NewClassicMessageEncoding encoding
)
{
	std::vector<std::string> names{};
	for(std::size_t cursor = 0U; cursor < text.size();) {
		if(cp932_pair_at(text, cursor, encoding)) {
			cursor += 2U;
			continue;
		}
		if(text[cursor] == '}') {
			return "!";
		}
		if(text[cursor] != '{') {
			++cursor;
			continue;
		}
		const auto close = text.find('}', cursor + 1U);
		if(close == std::string_view::npos || close == cursor + 1U) {
			return "!";
		}
		const auto name = text.substr(cursor + 1U, close - cursor - 1U);
		if(!std::ranges::all_of(name, [](char character) {
			const auto value = static_cast<unsigned char>(character);
			return (value >= 'a' && value <= 'z') ||
				(value >= '0' && value <= '9') || value == '_';
		}) || std::ranges::find(names, std::string{name}) != names.end()) {
			return "!";
		}
		names.emplace_back(name);
		cursor = close + 1U;
	}
	std::ranges::sort(names);
	std::string result{};
	for(const auto &name : names) {
		if(!result.empty()) {
			result.push_back(',');
		}
		result += name;
	}
	return result;
}

[[nodiscard]] std::vector<std::string_view> split_values(
	std::string_view values,
	char separator
)
{
	std::vector<std::string_view> result{};
	if(values.empty()) {
		return result;
	}
	for(std::size_t first = 0U;;) {
		const auto next = values.find(separator, first);
		result.push_back(values.substr(first, next - first));
		if(next == std::string_view::npos) {
			return result;
		}
		first = next + 1U;
	}
}

[[nodiscard]] std::string format_message(
	std::string_view source,
	std::span<const NewClassicMessageArgument> arguments,
	NewClassicMessageEncoding encoding
)
{
	std::string result{};
	std::vector<bool> used(arguments.size(), false);
	for(std::size_t cursor = 0U; cursor < source.size();) {
		if(cp932_pair_at(source, cursor, encoding)) {
			result.push_back(source[cursor++]);
			result.push_back(source[cursor++]);
			continue;
		}
		if(source[cursor] != '{') {
			result.push_back(source[cursor++]);
			continue;
		}
		const auto close = source.find('}', cursor + 1U);
		if(close == std::string_view::npos) {
			return {};
		}
		const auto name = source.substr(cursor + 1U, close - cursor - 1U);
		const auto found = std::ranges::find(
			arguments, name, &NewClassicMessageArgument::name
		);
		if(found == arguments.end()) {
			return {};
		}
		const auto index = static_cast<std::size_t>(found - arguments.begin());
		if(used[index]) {
			return {};
		}
		used[index] = true;
		result.append(found->value);
		cursor = close + 1U;
	}
	return std::ranges::all_of(used, [](bool value) { return value; }) ?
		result : std::string{};
}

[[nodiscard]] std::expected<void, NewClassicMessagePackError>
validate_message(
	NewClassicMessageId id,
	std::string_view text,
	const NewClassicMenuFont &font,
	NewClassicMessageEncoding encoding
)
{
	const auto index = static_cast<std::size_t>(id);
	if(index >= schema.size() || !valid_glyphs(text, encoding, font)) {
		return std::unexpected(error(
			NewClassicMessagePackErrorCode::glyph_coverage, 0U, {}, id
		));
	}
	if(placeholder_set(text, encoding) != schema[index].placeholders) {
		return std::unexpected(error(
			NewClassicMessagePackErrorCode::placeholder_mismatch, 0U, {}, id
		));
	}
	const auto names = split_values(schema[index].placeholders, ',');
	const auto values = split_values(schema[index].width_values, '|');
	if(names.size() != values.size()) {
		return std::unexpected(error(
			NewClassicMessagePackErrorCode::placeholder_mismatch, 0U, {}, id
		));
	}
	std::vector<NewClassicMessageArgument> arguments{};
	arguments.reserve(names.size());
	for(std::size_t argument = 0U; argument < names.size(); ++argument) {
		arguments.push_back({names[argument], values[argument]});
	}
	const auto formatted = format_message(text, arguments, encoding);
	if(formatted.empty() || !valid_glyphs(formatted, encoding, font) ||
		font.width(formatted) > schema[index].maximum_width) {
		return std::unexpected(error(
			NewClassicMessagePackErrorCode::width_exceeded, 0U, {}, id
		));
	}
	return {};
}

[[nodiscard]] NewClassicMessageCatalog builtin(
	std::string locale,
	std::string fallback,
	NewClassicLanguageAssetProfile profile,
	NewClassicMessageEncoding encoding,
	const std::array<std::string_view, message_count> &values
)
{
	std::array<std::string, message_count> messages{};
	for(std::size_t index = 0U; index < values.size(); ++index) {
		messages[index] = values[index];
	}
	NewClassicMessageCatalog result{};
	NewClassicMessageCatalogAccess::assign(
		result, std::move(locale), std::move(fallback), profile, encoding,
		std::move(messages)
	);
	return result;
}

} // namespace

std::string_view NewClassicMessageCatalog::locale() const noexcept
{
	return locale_;
}

std::string_view NewClassicMessageCatalog::fallback_locale() const noexcept
{
	return fallback_locale_;
}

NewClassicLanguageAssetProfile
NewClassicMessageCatalog::asset_profile() const noexcept
{
	return asset_profile_;
}

NewClassicMessageEncoding NewClassicMessageCatalog::encoding() const noexcept
{
	return encoding_;
}

std::string_view NewClassicMessageCatalog::text(
	NewClassicMessageId id
) const noexcept
{
	const auto index = static_cast<std::size_t>(id);
	return index < messages_.size() ? std::string_view{messages_[index]} :
		std::string_view{};
}

std::string NewClassicMessageCatalog::format_percent(
	NewClassicMessageId id,
	std::uint8_t percent
) const
{
	const auto value = std::to_string(percent);
	const std::array arguments{NewClassicMessageArgument{"percent", value}};
	return format(id, arguments);
}

std::string NewClassicMessageCatalog::format(
	NewClassicMessageId id,
	std::span<const NewClassicMessageArgument> arguments
) const
{
	const auto index = static_cast<std::size_t>(id);
	if(index >= messages_.size() ||
		placeholder_set(messages_[index], encoding_) != schema[index].placeholders) {
		return {};
	}
	return format_message(messages_[index], arguments, encoding_);
}

const NewClassicMessageSchemaEntry &new_classic_message_schema(
	NewClassicMessageId id
) noexcept
{
	const auto index = static_cast<std::size_t>(id);
	return schema[index < schema.size() ? index : 0U];
}

const NewClassicMessageCatalog &new_classic_builtin_messages(
	NewClassicLanguageAssetProfile profile
) noexcept
{
	static const auto japanese = builtin(
		"ja-JP", "en-US", NewClassicLanguageAssetProfile::japanese,
		NewClassicMessageEncoding::cp932_mixed,
		builtin_japanese
	);
	static const auto english = builtin(
		"en-US", "", NewClassicLanguageAssetProfile::english,
		NewClassicMessageEncoding::menu_font_ascii,
		builtin_english
	);
	static const auto spanish_test = builtin(
		"es-TEST", "en-US", NewClassicLanguageAssetProfile::spanish_test,
		NewClassicMessageEncoding::utf8,
		builtin_spanish_test
	);
	switch(profile) {
	case NewClassicLanguageAssetProfile::japanese:
		return japanese;
	case NewClassicLanguageAssetProfile::english:
		return english;
	case NewClassicLanguageAssetProfile::spanish_test:
		return spanish_test;
	}
	return japanese;
}

std::expected<NewClassicMessageCatalog, NewClassicMessagePackError>
load_new_classic_message_pack(
	const std::filesystem::path &path,
	const NewClassicMenuFont &font,
	const NewClassicMessageCatalog *fallback
)
{
	if(path.empty()) {
		return std::unexpected(error(
			NewClassicMessagePackErrorCode::invalid_path
		));
	}
	std::error_code system;
	const auto status = std::filesystem::status(path, system);
	if(system) {
		return std::unexpected(error(
			NewClassicMessagePackErrorCode::source_status, 0U, system
		));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(error(
			NewClassicMessagePackErrorCode::source_not_regular
		));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system) {
		return std::unexpected(error(
			NewClassicMessagePackErrorCode::source_size, 0U, system
		));
	}
	if(size < header_bytes || size > maximum_pack_bytes) {
		return std::unexpected(error(
			NewClassicMessagePackErrorCode::source_size
		));
	}
	try {
		std::ifstream input(path, std::ios::binary);
		if(!input) {
			return std::unexpected(error(
				NewClassicMessagePackErrorCode::source_open
			));
		}
		std::vector<std::byte> bytes(static_cast<std::size_t>(size));
		input.read(reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size()));
		if(!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
			return std::unexpected(error(
				NewClassicMessagePackErrorCode::source_read
			));
		}
		if(!std::ranges::equal(magic, std::span<const std::byte>{bytes}.first(8U)) ||
			u16(bytes, 8U) != 1U || u16(bytes, 10U) != header_bytes ||
			u16(bytes, 12U) != message_count || !zero_range(bytes, 18U, 20U) ||
			!zero_range(bytes, 28U, 32U)) {
			return std::unexpected(error(
				NewClassicMessagePackErrorCode::header_shape
			));
		}
		const auto profile_value = std::to_integer<std::uint8_t>(bytes[14U]);
		const auto encoding_value = std::to_integer<std::uint8_t>(bytes[17U]);
		if(profile_value > 1U) {
			return std::unexpected(error(
				NewClassicMessagePackErrorCode::asset_profile, 14U
			));
		}
		if(encoding_value > 2U || (profile_value == 0U && encoding_value != 1U) ||
			(profile_value == 1U && encoding_value == 1U)) {
			return std::unexpected(error(
				NewClassicMessagePackErrorCode::glyph_coverage, 17U
			));
		}
		const auto encoding = static_cast<NewClassicMessageEncoding>(
			encoding_value
		);
		if(!std::ranges::equal(
			message_schema_sha256,
			std::span<const std::byte>{bytes}.subspan(32U, 32U),
			{}, [](std::uint8_t value) { return static_cast<std::byte>(value); },
			[](std::byte value) { return value; }
		)) {
			return std::unexpected(error(
				NewClassicMessagePackErrorCode::schema_mismatch, 32U
			));
		}
		const auto locale_bytes = std::to_integer<std::uint8_t>(bytes[15U]);
		const auto fallback_bytes = std::to_integer<std::uint8_t>(bytes[16U]);
		const auto directory_bytes = static_cast<std::size_t>(u32(bytes, 20U));
		const auto payload_bytes = static_cast<std::size_t>(u32(bytes, 24U));
		if(locale_bytes == 0U || locale_bytes > 31U || fallback_bytes > 31U ||
			directory_bytes != message_count * directory_entry_bytes ||
			header_bytes + locale_bytes + fallback_bytes + directory_bytes +
				payload_bytes != bytes.size()) {
			return std::unexpected(error(
				NewClassicMessagePackErrorCode::directory_shape
			));
		}
		const auto body = std::span<const std::byte>{bytes}.subspan(header_bytes);
		const auto body_digest = main_executable_sha256(body);
		if(!std::ranges::equal(
			body_digest,
			std::span<const std::byte>{bytes}.subspan(64U, 32U),
			{}, [](std::uint8_t value) { return static_cast<std::byte>(value); },
			[](std::byte value) { return value; }
		)) {
			return std::unexpected(error(
				NewClassicMessagePackErrorCode::body_digest_mismatch, 64U
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
				NewClassicMessagePackErrorCode::locale_shape, locale_offset
			));
		}
		if(!fallback_locale.empty() &&
			(!fallback || fallback->locale() != fallback_locale)) {
			return std::unexpected(error(
				NewClassicMessagePackErrorCode::fallback_missing, fallback_offset
			));
		}
		std::array<std::string, message_count> messages{};
		std::size_t expected_payload_offset = 0U;
		for(std::size_t index = 0U; index < message_count; ++index) {
			const auto entry = directory_offset + index * directory_entry_bytes;
			const auto offset = static_cast<std::size_t>(u32(bytes, entry));
			const auto length = static_cast<std::size_t>(u16(bytes, entry + 4U));
			const auto id = static_cast<NewClassicMessageId>(index);
			if(u16(bytes, entry + 6U) != 0U) {
				return std::unexpected(error(
					NewClassicMessagePackErrorCode::directory_shape, entry, {}, id
				));
			}
			if(length == 0U) {
				if(offset != 0U || !fallback) {
					return std::unexpected(error(
						NewClassicMessagePackErrorCode::fallback_missing, entry, {}, id
					));
				}
				messages[index] = fallback->text(id);
			} else {
				if(offset != expected_payload_offset ||
					length > payload_bytes - offset) {
					return std::unexpected(error(
						NewClassicMessagePackErrorCode::message_shape, entry, {}, id
					));
				}
				messages[index].assign(
					reinterpret_cast<const char *>(bytes.data() + payload_offset + offset),
					length
				);
				expected_payload_offset += length;
			}
			if(auto valid = validate_message(
				id, messages[index], font, encoding
			); !valid) {
				auto result = valid.error();
				result.offset = entry;
				return std::unexpected(std::move(result));
			}
		}
		if(expected_payload_offset != payload_bytes) {
			return std::unexpected(error(
				NewClassicMessagePackErrorCode::message_shape, payload_offset
			));
		}
		NewClassicMessageCatalog result{};
		NewClassicMessageCatalogAccess::assign(
			result, locale, fallback_locale,
			static_cast<NewClassicLanguageAssetProfile>(profile_value),
			encoding,
			std::move(messages)
		);
		return result;
	} catch(const std::bad_alloc &) {
		return std::unexpected(error(
			NewClassicMessagePackErrorCode::allocation_failed
		));
	}
}

namespace detail {

const NewClassicMessageCatalog &new_classic_width_test_messages() noexcept
{
	static const auto result = builtin(
		"qps-ploc", "en-US", NewClassicLanguageAssetProfile::english,
		NewClassicMessageEncoding::menu_font_ascii,
		builtin_width_test
	);
	return result;
}

} // namespace detail
} // namespace th03::app
