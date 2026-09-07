#ifndef TH03_ERROR_HPP
#define TH03_ERROR_HPP

#include <cstddef>
#include <string>

namespace th03 {

enum class ErrorCode {
	io,
	truncated,
	trailing_data,
	bad_magic,
	unsupported_version,
	unsupported_layout,
	invalid_value,
	checksum_mismatch,
	count_mismatch,
	malformed_stream,
	simulation_not_implemented,
};

struct Error {
	ErrorCode code;
	std::string message;
	std::size_t offset = 0;
};

} // namespace th03

#endif
