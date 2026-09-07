#ifndef TH03_APP_OP_COMPILED_TEXT_INTERNAL_HPP
#define TH03_APP_OP_COMPILED_TEXT_INTERNAL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "th03/app/content/main_compiled_content.hpp"

namespace th03::app {

enum class OpExecutableProfile : std::uint8_t {
	unpacked_original_japanese,
	packed_original_japanese,
};

enum class OpCompiledTextItem : std::uint8_t {
	none,
	track,
	quit,
};

enum class OpCompiledTextErrorCode : std::uint8_t {
	input_too_short,
	invalid_mz_signature,
	invalid_mz_layout,
	unsupported_profile,
	packed_decode_failed,
	compiled_range_out_of_bounds,
	malformed_source_text,
};

struct OpCompiledTextError final {
	OpCompiledTextErrorCode code = OpCompiledTextErrorCode::input_too_short;
	OpCompiledTextItem item = OpCompiledTextItem::none;
	std::uint8_t choice_index = 0U;
	// Absolute byte offset in the supplied executable. For packed_decode_failed,
	// this is the decoder's byte-silent packed/IDATA coordinate. Diagnostics
	// must never print the original-content byte stored at this location.
	std::size_t offset = 0U;
};

struct OpMusicRoomTextImport final {
	static constexpr std::size_t choice_count = 21U;
	static constexpr std::size_t empty_choice_index = 19U;
	static constexpr std::size_t quit_choice_index = 20U;

	OpExecutableProfile profile =
		OpExecutableProfile::unpacked_original_japanese;
	std::array<std::vector<std::uint8_t>, choice_count> choices{};
};

// Authenticates a complete supported packed or unpacked OP.EXE profile before
// reading its private compiled-data ranges. Packed input is boundedly
// reconstructed and authenticated against the unpacked profile first.
[[nodiscard]] std::expected<
	OpMusicRoomTextImport,
	OpCompiledTextError
> import_op_music_room_text(std::span<const std::byte> op_executable);

namespace detail {

// Hermetic test seam that substitutes only the accepted whole-file digest.
// Production never supplies an alternate digest or layout descriptor.
[[nodiscard]] std::expected<
	OpMusicRoomTextImport,
	OpCompiledTextError
> import_op_music_room_text_for_test(
	std::span<const std::byte> op_executable,
	const MainExecutableSha256 &accepted_digest
);

} // namespace detail
} // namespace th03::app

#endif
