#ifndef TH03_APP_NEW_CLASSIC_CUTSCENE_TEXT_HPP
#define TH03_APP_NEW_CLASSIC_CUTSCENE_TEXT_HPP

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

inline constexpr std::size_t new_classic_cutscene_script_count = 28U;
inline constexpr std::uint64_t new_classic_cutscene_original_profile_fnv1a64 =
	0xFE26F73526487310ULL;
inline constexpr std::uint64_t
new_classic_cutscene_english_profile_fnv1a64 = 0x5C33CB0520F00380ULL;

enum class NewClassicCutsceneTextPackErrorCode : std::uint8_t {
	invalid_path,
	source_status,
	source_not_regular,
	source_size,
	source_open,
	source_read,
	header_shape,
	body_digest_mismatch,
	locale_shape,
	directory_shape,
	message_shape,
	malformed_shift_jis,
	source_profile_mismatch,
	script_mismatch,
	run_mismatch,
	allocation_failed,
};

struct NewClassicCutsceneTextPackError final {
	NewClassicCutsceneTextPackErrorCode code =
		NewClassicCutsceneTextPackErrorCode::invalid_path;
	std::size_t offset = 0U;
	std::size_t script_index = new_classic_cutscene_script_count;
	std::size_t run_index = 0U;
	std::error_code system{};
};

// Complete T3CTP001 replacement catalog. Script indices are the canonical
// MainCutsceneScript values minus one. Materialization first re-extracts the
// production VM run contract, then copies only exact two-byte, 16-pixel CP932
// VM cells into a detached semantic sequence. A cell can hold one fullwidth
// glyph or two halfwidth glyphs. The authenticated script remains untouched;
// source commands and byte offsets never come from the pack.
class NewClassicCutsceneTextCatalog final {
public:
	[[nodiscard]] std::string_view locale() const noexcept;
	[[nodiscard]] std::uint64_t source_profile_fnv1a64() const noexcept;
	[[nodiscard]] std::expected<
		std::vector<std::array<std::uint8_t, 2U>>,
		NewClassicCutsceneTextPackError
	> replacement_glyphs(
		std::size_t script_index,
		std::span<const std::uint8_t> authenticated_script
	) const;

private:
	friend struct NewClassicCutsceneTextCatalogAccess;
	friend std::expected<
		NewClassicCutsceneTextCatalog,
		NewClassicCutsceneTextPackError
	> load_new_classic_cutscene_text_pack(const std::filesystem::path &path);
	struct Run final {
		std::size_t source_offset = 0U;
		std::uint16_t glyphs = 0U;
		std::uint64_t source_fnv1a64 = 0U;
		std::vector<std::uint8_t> replacement{};
	};
	struct Script final {
		std::size_t source_bytes = 0U;
		std::uint64_t source_fnv1a64 = 0U;
		std::vector<Run> runs{};
	};
	std::string locale_{};
	std::uint64_t source_profile_fnv1a64_ = 0U;
	std::vector<Script> scripts_{};
};

[[nodiscard]] std::expected<
	NewClassicCutsceneTextCatalog,
	NewClassicCutsceneTextPackError
> load_new_classic_cutscene_text_pack(const std::filesystem::path &path);

} // namespace th03::app

#endif
