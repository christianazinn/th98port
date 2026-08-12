#ifndef TH03_APP_DIET_EXECUTABLE_INTERNAL_HPP
#define TH03_APP_DIET_EXECUTABLE_INTERNAL_HPP

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "th03/app/main_compiled_content.hpp"

namespace th03::app::detail {

struct DietExecutableProfile final {
	std::size_t packed_file_bytes = 0U;
	MainExecutableSha256 packed_sha256{};
	std::size_t unpacked_file_bytes = 0U;
	MainExecutableSha256 unpacked_sha256{};
};

enum class DietExecutableErrorCode : std::uint8_t {
	unsupported_profile,
	invalid_packed_mz,
	invalid_diet_header,
	crc_mismatch,
	truncated_stream,
	invalid_stream,
	output_limit_exceeded,
	invalid_reconstructed_mz,
	invalid_relocation_table,
	reconstructed_profile_mismatch,
};

struct DietExecutableError final {
	DietExecutableErrorCode code =
		DietExecutableErrorCode::unsupported_profile;
	// Byte-silent coordinate in the packed input or intermediate IDATA,
	// depending on the named stage. Callers must never print the source byte.
	std::size_t offset = 0U;
};

// Accepts only the exact caller-supplied packed profile before consulting any
// DIET-private range. The bounded DIET 1.45f reconstruction is then required to
// match the exact unpacked size and digest.
[[nodiscard]] std::expected<std::vector<std::byte>, DietExecutableError>
decode_diet_145f_executable(
	std::span<const std::byte> packed,
	const DietExecutableProfile &profile
);

struct DietStreamResult final {
	std::vector<std::byte> idata{};
	std::size_t consumed_bytes = 0U;
	std::size_t segment_refreshes = 0U;
};

// Hermetic test seam for the generic stream grammar. Production reaches the
// same routine only after whole-file profile authentication and header bounds.
[[nodiscard]] std::expected<DietStreamResult, DietExecutableError>
decode_diet_stream_for_test(
	std::span<const std::byte> compressed,
	std::size_t output_limit,
	bool segment_refresh_allowed
);

} // namespace th03::app::detail

#endif
