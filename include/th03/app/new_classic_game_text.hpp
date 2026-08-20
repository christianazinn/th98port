#ifndef TH03_APP_NEW_CLASSIC_GAME_TEXT_HPP
#define TH03_APP_NEW_CLASSIC_GAME_TEXT_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace th03::app {

enum class NewClassicGameTextId : std::uint16_t {
#define TH03_NEW_CLASSIC_GAME_TEXT(name, identifier, exact_bytes) name,
#include "th03/app/new_classic_game_text_ids.inc"
#undef TH03_NEW_CLASSIC_GAME_TEXT
	count,
};

inline constexpr std::size_t new_classic_game_text_count =
	static_cast<std::size_t>(NewClassicGameTextId::count);

struct NewClassicGameTextSchemaEntry final {
	std::string_view identifier{};
	std::uint16_t exact_bytes = 0U;
};

enum class NewClassicGameTextPackErrorCode : std::uint8_t {
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
	encoding,
	directory_shape,
	message_shape,
	fallback_missing,
	malformed_shift_jis,
	length_mismatch,
	allocation_failed,
};

struct NewClassicGameTextPackError final {
	NewClassicGameTextPackErrorCode code =
		NewClassicGameTextPackErrorCode::invalid_path;
	std::size_t offset = 0U;
	std::error_code system{};
	NewClassicGameTextId message = NewClassicGameTextId::count;
};

class NewClassicGameTextCatalog final {
public:
	[[nodiscard]] std::string_view locale() const noexcept;
	[[nodiscard]] std::string_view fallback_locale() const noexcept;
	[[nodiscard]] std::span<const std::uint8_t> bytes(
		NewClassicGameTextId id
	) const noexcept;

private:
	friend struct NewClassicGameTextCatalogAccess;
	std::string locale_{};
	std::string fallback_locale_{};
	std::array<
		std::vector<std::uint8_t>, new_classic_game_text_count
	> messages_{};
};

[[nodiscard]] const NewClassicGameTextSchemaEntry &
new_classic_game_text_schema(NewClassicGameTextId id) noexcept;

[[nodiscard]] const NewClassicGameTextCatalog &
new_classic_builtin_english_game_text() noexcept;

// T3GTL001 entries contain exact CP932 bytes for source-constrained graphics
// text. An omitted entry is legal only when [fallback] names the supplied,
// already validated catalog exactly.
[[nodiscard]] std::expected<
	NewClassicGameTextCatalog,
	NewClassicGameTextPackError
> load_new_classic_game_text_pack(
	const std::filesystem::path &path,
	const NewClassicGameTextCatalog *fallback = nullptr
);

} // namespace th03::app

#endif
