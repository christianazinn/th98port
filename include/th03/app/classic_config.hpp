#ifndef TH03_APP_CLASSIC_CONFIG_HPP
#define TH03_APP_CLASSIC_CONFIG_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace th03::app {

inline constexpr std::size_t classic_config_file_bytes = 8U;

enum class ClassicBgmMode : std::uint8_t {
	off,
	fm,
	midi,
};

enum class ClassicKeyMode : std::uint8_t {
	key_key,
	joy_key,
	key_joy,
};

enum class ClassicRank : std::uint8_t {
	easy,
	normal,
	hard,
	lunatic,
};

enum class ClassicLanguage : std::uint8_t {
	japanese,
	english,
	spanish_test,
};

[[nodiscard]] constexpr bool classic_language_uses_english_assets(
	ClassicLanguage language
) noexcept
{
	return language != ClassicLanguage::japanese;
}

struct ClassicConfigOptions final {
	ClassicBgmMode bgm_mode = ClassicBgmMode::fm;
	ClassicKeyMode key_mode = ClassicKeyMode::key_key;
	ClassicRank rank = ClassicRank::normal;
	bool sfx_enabled = true;
	bool legacy_autofire = false;
	ClassicLanguage language = ClassicLanguage::japanese;
	bool reduced_flash = false;

	friend constexpr bool operator==(
		const ClassicConfigOptions &,
		const ClassicConfigOptions &
	) = default;
};

// Classic-compatible metadata. Native code never interprets a DOS resident
// segment and writes both fields as zero through classic_config_document().
struct ClassicConfigMetadata final {
	std::uint16_t resident_segment = 0U;
	std::uint8_t debug = 0U;

	friend constexpr bool operator==(
		const ClassicConfigMetadata &,
		const ClassicConfigMetadata &
	) = default;
};

struct ClassicConfigDocument final {
	ClassicConfigOptions options{};
	ClassicConfigMetadata metadata{};

	friend constexpr bool operator==(
		const ClassicConfigDocument &,
		const ClassicConfigDocument &
	) = default;
};

enum class ClassicConfigErrorCode : std::uint8_t {
	input_size,
	invalid_bgm_mode,
	invalid_key_mode,
	invalid_rank,
	invalid_autofire,
	invalid_language,
};

struct ClassicConfigError final {
	ClassicConfigErrorCode code = ClassicConfigErrorCode::input_size;
	std::size_t offset = classic_config_file_bytes;
};

[[nodiscard]] constexpr ClassicConfigOptions classic_config_defaults() noexcept
{
	return {};
}

// Produces the deterministic native representation. The original's DOS
// resident segment and debug flag are not runtime settings and are
// deliberately zeroed here; Replay Patch options own bytes 3 and 4.
[[nodiscard]] constexpr ClassicConfigDocument classic_config_document(
	ClassicConfigOptions options
) noexcept
{
	return {.options = options};
}

[[nodiscard]] constexpr bool classic_config_valid(
	ClassicConfigOptions options
) noexcept
{
	return static_cast<std::uint8_t>(options.bgm_mode) <=
		static_cast<std::uint8_t>(ClassicBgmMode::midi) &&
		static_cast<std::uint8_t>(options.key_mode) <=
		static_cast<std::uint8_t>(ClassicKeyMode::key_joy) &&
		static_cast<std::uint8_t>(options.rank) <=
		static_cast<std::uint8_t>(ClassicRank::lunatic) &&
		static_cast<std::uint8_t>(options.language) <=
		static_cast<std::uint8_t>(ClassicLanguage::spanish_test);
}

[[nodiscard]] std::expected<ClassicConfigDocument, ClassicConfigError>
decode_classic_config_file(std::span<const std::byte> bytes) noexcept;

[[nodiscard]] std::expected<
	std::array<std::byte, classic_config_file_bytes>,
	ClassicConfigError
> encode_classic_config_file(const ClassicConfigDocument &document) noexcept;

} // namespace th03::app

#endif
