#ifndef TH03_APP_NEW_CLASSIC_MESSAGES_HPP
#define TH03_APP_NEW_CLASSIC_MESSAGES_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#include "th03/app/classic_config.hpp"

namespace th03::app {

class NewClassicMenuFont;

enum class NewClassicMessageId : std::uint16_t {
#define TH03_NEW_CLASSIC_MESSAGE( \
	name, identifier, width, placeholders, width_values \
) name,
#include "th03/app/new_classic_message_ids.inc"
#undef TH03_NEW_CLASSIC_MESSAGE
	count,
};

inline constexpr std::size_t new_classic_message_count =
	static_cast<std::size_t>(NewClassicMessageId::count);

enum class NewClassicLanguageAssetProfile : std::uint8_t {
	japanese,
	english,
	spanish_test,
};

[[nodiscard]] constexpr NewClassicLanguageAssetProfile
new_classic_language_profile(ClassicLanguage language) noexcept
{
	switch(language) {
	case ClassicLanguage::japanese:
		return NewClassicLanguageAssetProfile::japanese;
	case ClassicLanguage::english:
		return NewClassicLanguageAssetProfile::english;
	case ClassicLanguage::spanish_test:
		return NewClassicLanguageAssetProfile::spanish_test;
	}
	return NewClassicLanguageAssetProfile::japanese;
}

enum class NewClassicMessageEncoding : std::uint8_t {
	menu_font_ascii,
	cp932_mixed,
	utf8,
};

struct NewClassicMessageSchemaEntry final {
	std::string_view identifier{};
	std::uint16_t maximum_width = 0U;
	std::string_view placeholders{};
	std::string_view width_values{};
};

struct NewClassicMessageArgument final {
	std::string_view name{};
	std::string_view value{};
};

enum class NewClassicMessagePackErrorCode : std::uint8_t {
	invalid_path,
	source_status,
	source_not_regular,
	source_size,
	source_open,
	source_read,
	header_shape,
	schema_mismatch,
	body_digest_mismatch,
	locale_shape,
	asset_profile,
	directory_shape,
	message_shape,
	fallback_missing,
	placeholder_mismatch,
	glyph_coverage,
	width_exceeded,
	allocation_failed,
};

struct NewClassicMessagePackError final {
	NewClassicMessagePackErrorCode code =
		NewClassicMessagePackErrorCode::invalid_path;
	std::size_t offset = 0U;
	std::error_code system{};
	NewClassicMessageId message = NewClassicMessageId::count;
};

class NewClassicMessageCatalog final {
public:
	[[nodiscard]] std::string_view locale() const noexcept;
	[[nodiscard]] std::string_view fallback_locale() const noexcept;
	[[nodiscard]] NewClassicLanguageAssetProfile asset_profile() const noexcept;
	[[nodiscard]] NewClassicMessageEncoding encoding() const noexcept;
	[[nodiscard]] std::string_view text(NewClassicMessageId id) const noexcept;
	[[nodiscard]] std::string format(
		NewClassicMessageId id,
		std::span<const NewClassicMessageArgument> arguments
	) const;
	[[nodiscard]] std::string format_percent(
		NewClassicMessageId id,
		std::uint8_t percent
	) const;

private:
	friend struct NewClassicMessageCatalogAccess;
	std::string locale_{};
	std::string fallback_locale_{};
	NewClassicLanguageAssetProfile asset_profile_ =
		NewClassicLanguageAssetProfile::japanese;
	NewClassicMessageEncoding encoding_ =
		NewClassicMessageEncoding::menu_font_ascii;
	std::array<std::string, new_classic_message_count> messages_{};
};

[[nodiscard]] const NewClassicMessageSchemaEntry &
new_classic_message_schema(NewClassicMessageId id) noexcept;

[[nodiscard]] const NewClassicMessageCatalog &
new_classic_builtin_messages(NewClassicLanguageAssetProfile profile) noexcept;

// T3LNG001 packs may omit entries only when [fallback] names the supplied,
// already-validated catalog exactly. Every resolved message is then checked
// against the authenticated MNUFONT proportional width before acceptance.
[[nodiscard]] std::expected<
	NewClassicMessageCatalog,
	NewClassicMessagePackError
> load_new_classic_message_pack(
	const std::filesystem::path &path,
	const NewClassicMenuFont &font,
	const NewClassicMessageCatalog *fallback = nullptr
);

namespace detail {

[[nodiscard]] const NewClassicMessageCatalog &
new_classic_width_test_messages() noexcept;

} // namespace detail
} // namespace th03::app

#endif
