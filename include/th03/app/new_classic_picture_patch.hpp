#ifndef TH03_APP_NEW_CLASSIC_PICTURE_PATCH_HPP
#define TH03_APP_NEW_CLASSIC_PICTURE_PATCH_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "th03/app/main_compiled_content.hpp"
#include "th03/assets/types.hpp"

namespace th03::app {

inline constexpr std::uint16_t new_classic_picture_patch_width = 640U;
inline constexpr std::uint16_t new_classic_picture_patch_height = 400U;

enum class NewClassicPicturePatchErrorCode : std::uint8_t {
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
	member_shape,
	run_shape,
	image_shape,
	source_digest_mismatch,
	allocation_failed,
};

struct NewClassicPicturePatchError final {
	NewClassicPicturePatchErrorCode code =
		NewClassicPicturePatchErrorCode::invalid_path;
	std::size_t offset = 0U;
	std::size_t member_index = 0U;
	std::error_code system{};
};

// SHA-256 over the canonical decoded source representation: little-endian
// width/height, 16 RGB4 palette triples, then one row-major palette index per
// pixel. PI comments and compressor details cannot affect an image patch.
[[nodiscard]] std::expected<AppSha256, NewClassicPicturePatchError>
new_classic_picture_source_sha256(const assets::PiImage &image);

class NewClassicPicturePatchCatalog final {
public:
	[[nodiscard]] std::string_view locale() const noexcept;
	[[nodiscard]] std::size_t picture_count() const noexcept;

	// Returns false when this complete locale package has no replacement for
	// [member]. A matching entry verifies the decoded source digest before
	// changing any pixel; failure leaves [image] untouched.
	[[nodiscard]] std::expected<bool, NewClassicPicturePatchError> apply(
		std::string_view member,
		assets::PiImage &image
	) const;

private:
	friend std::expected<
		NewClassicPicturePatchCatalog,
		NewClassicPicturePatchError
	> load_new_classic_picture_patch(const std::filesystem::path &path);

	struct Run final {
		std::uint32_t pixel_offset = 0U;
		std::vector<std::uint8_t> indices{};
	};
	struct Picture final {
		std::string member{};
		AppSha256 source_sha256{};
		std::vector<Run> runs{};
	};

	std::string locale_{};
	std::vector<Picture> pictures_{};
};

[[nodiscard]] std::expected<
	NewClassicPicturePatchCatalog,
	NewClassicPicturePatchError
> load_new_classic_picture_patch(const std::filesystem::path &path);

} // namespace th03::app

#endif
