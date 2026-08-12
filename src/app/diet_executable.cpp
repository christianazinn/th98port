#include "diet_executable_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <utility>
#include <vector>

namespace th03::app::detail {
namespace {

constexpr std::size_t mz_minimum_header_bytes = 28U;
constexpr std::size_t mz_page_bytes = 512U;
constexpr std::size_t diet_host_header_bytes = 32U;
constexpr std::size_t diet_fingerprint_offset = 0x4DU;
constexpr std::size_t diet_amz_pointer_offset = 0x3AU;
constexpr std::size_t diet_dlz_offset = 0x6CU;
constexpr std::size_t diet_compressed_offset = diet_dlz_offset + 11U;
constexpr std::array<std::byte, 16U> diet_145f_fingerprint = {
	std::byte{0x8EU}, std::byte{0xDBU}, std::byte{0x8EU}, std::byte{0xC0U},
	std::byte{0x33U}, std::byte{0xF6U}, std::byte{0x33U}, std::byte{0xFFU},
	std::byte{0xB9U}, std::byte{0x08U}, std::byte{0x00U}, std::byte{0xF3U},
	std::byte{0xA5U}, std::byte{0x4BU}, std::byte{0x48U}, std::byte{0x4AU},
};

[[nodiscard]] DietExecutableError error(
	DietExecutableErrorCode code,
	std::size_t offset = 0U
) noexcept
{
	return {code, offset};
}

[[nodiscard]] std::uint16_t read_u16(
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

void append_u16(std::vector<std::byte> &bytes, std::uint16_t value)
{
	bytes.push_back(static_cast<std::byte>(value & 0xFFU));
	bytes.push_back(static_cast<std::byte>(value >> 8U));
}

[[nodiscard]] std::size_t declared_mz_bytes(
	std::uint16_t last_page_bytes,
	std::uint16_t page_count
) noexcept
{
	if(page_count == 0U || last_page_bytes >= mz_page_bytes) {
		return 0U;
	}
	return ((static_cast<std::size_t>(page_count) - 1U) * mz_page_bytes) +
		((last_page_bytes == 0U) ? mz_page_bytes : last_page_bytes);
}

[[nodiscard]] std::uint16_t crc16_arc(
	std::span<const std::byte> bytes
) noexcept
{
	std::uint16_t crc = 0U;
	for(const auto value : bytes) {
		crc ^= std::to_integer<std::uint8_t>(value);
		for(std::uint8_t bit = 0U; bit < 8U; ++bit) {
			crc = ((crc & 1U) != 0U) ?
				static_cast<std::uint16_t>((crc >> 1U) ^ 0xA001U) :
				static_cast<std::uint16_t>(crc >> 1U);
		}
	}
	return crc;
}

class DietStreamDecoder final {
public:
	DietStreamDecoder(
		std::span<const std::byte> input,
		std::size_t output_limit,
		bool segment_refresh_allowed
	) noexcept :
		input_(input),
		output_limit_(output_limit),
		segment_refresh_allowed_(segment_refresh_allowed)
	{
	}

	[[nodiscard]] std::expected<DietStreamResult, DietExecutableError> run()
	{
		while(true) {
			auto first = bit();
			if(!first) {
				return std::unexpected(first.error());
			}
			if(*first != 0U) {
				auto literal = byte();
				if(!literal) {
					return std::unexpected(literal.error());
				}
				if(!append(*literal)) {
					return std::unexpected(error(
						DietExecutableErrorCode::output_limit_exceeded,
						output_.size()
					));
				}
				continue;
			}

			auto second = bit();
			if(!second) {
				return std::unexpected(second.error());
			}
			auto operand = byte();
			if(!operand) {
				return std::unexpected(operand.error());
			}
			if(*second == 0U) {
				auto selector = bit();
				if(!selector) {
					return std::unexpected(selector.error());
				}
				if(*selector != 0U) {
					auto a = bit();
					auto b = bit();
					auto c = bit();
					if(!a || !b || !c) {
						return std::unexpected((!a) ? a.error() :
							((!b) ? b.error() : c.error()));
					}
					const auto distance = 2304U -
						((static_cast<std::size_t>(*a) * 1024U) +
						 (static_cast<std::size_t>(*b) * 512U) +
						 (static_cast<std::size_t>(*c) * 256U) +
						 *operand);
					auto copied = copy(distance, 2U);
					if(!copied) {
						return std::unexpected(copied.error());
					}
					continue;
				}
				if(*operand != 0xFFU) {
					auto copied = copy(256U - *operand, 2U);
					if(!copied) {
						return std::unexpected(copied.error());
					}
					continue;
				}
				auto special = bit();
				if(!special) {
					return std::unexpected(special.error());
				}
				if(*special == 0U) {
					if(cursor_ != input_.size()) {
						return std::unexpected(error(
							DietExecutableErrorCode::invalid_stream,
							cursor_
						));
					}
					return DietStreamResult{
						std::move(output_), cursor_, segment_refreshes_,
					};
				}
				if(!segment_refresh_allowed_) {
					return std::unexpected(error(
						DietExecutableErrorCode::invalid_stream,
						cursor_
					));
				}
				++segment_refreshes_;
				continue;
			}

			auto distance = variable_distance(*operand);
			if(!distance) {
				return std::unexpected(distance.error());
			}
			auto length = match_length();
			if(!length) {
				return std::unexpected(length.error());
			}
			auto copied = copy(*distance, *length);
			if(!copied) {
				return std::unexpected(copied.error());
			}
		}
	}

private:
	[[nodiscard]] std::expected<std::uint8_t, DietExecutableError> byte()
	{
		if(cursor_ >= input_.size()) {
			return std::unexpected(error(
				DietExecutableErrorCode::truncated_stream,
				cursor_
			));
		}
		return std::to_integer<std::uint8_t>(input_[cursor_++]);
	}

	[[nodiscard]] std::expected<std::uint8_t, DietExecutableError> bit()
	{
		if(bits_remaining_ == 0U) {
			auto filled = refill();
			if(!filled) {
				return std::unexpected(filled.error());
			}
		}
		const auto result = static_cast<std::uint8_t>(bit_buffer_ & 1U);
		bit_buffer_ >>= 1U;
		--bits_remaining_;
		// DIET refills immediately when the sixteenth bit is consumed, before
		// the grammar reads its next byte-stream operand.
		if(bits_remaining_ == 0U) {
			auto filled = refill();
			if(!filled) {
				return std::unexpected(filled.error());
			}
		}
		return result;
	}

	[[nodiscard]] std::expected<void, DietExecutableError> refill()
	{
		if(cursor_ > input_.size() || (input_.size() - cursor_) < 2U) {
			return std::unexpected(error(
				DietExecutableErrorCode::truncated_stream,
				cursor_
			));
		}
		bit_buffer_ = read_u16(input_, cursor_);
		cursor_ += 2U;
		bits_remaining_ = 16U;
		return {};
	}

	[[nodiscard]] bool append(std::uint8_t value)
	{
		if(output_.size() >= output_limit_) {
			return false;
		}
		output_.push_back(static_cast<std::byte>(value));
		return true;
	}

	[[nodiscard]] std::expected<void, DietExecutableError> copy(
		std::size_t distance,
		std::size_t length
	)
	{
		if(distance == 0U || distance > output_.size() || length > distance) {
			return std::unexpected(error(
				DietExecutableErrorCode::invalid_stream,
				cursor_
			));
		}
		if(length > (output_limit_ - output_.size())) {
			return std::unexpected(error(
				DietExecutableErrorCode::output_limit_exceeded,
				output_.size()
			));
		}
		const auto source = output_.size() - distance;
		for(std::size_t index = 0U; index < length; ++index) {
			output_.push_back(output_[source + index]);
		}
		return {};
	}

	[[nodiscard]] std::expected<std::size_t, DietExecutableError>
	variable_distance(std::uint8_t operand)
	{
		auto a = bit();
		auto branch = bit();
		if(!a || !branch) {
			return std::unexpected((!a) ? a.error() : branch.error());
		}
		if(*branch != 0U) {
			return 512U - ((static_cast<std::size_t>(*a) * 256U) + operand);
		}
		auto next = bit();
		if(!next) {
			return std::unexpected(next.error());
		}
		if(*next != 0U) {
			return 1024U - ((static_cast<std::size_t>(*a) * 256U) + operand);
		}
		auto b = bit();
		auto branch_2048 = bit();
		if(!b || !branch_2048) {
			return std::unexpected((!b) ? b.error() : branch_2048.error());
		}
		if(*branch_2048 != 0U) {
			return 2048U - ((static_cast<std::size_t>(*a) * 512U) +
				(static_cast<std::size_t>(*b) * 256U) + operand);
		}
		auto c = bit();
		auto branch_4096 = bit();
		if(!c || !branch_4096) {
			return std::unexpected((!c) ? c.error() : branch_4096.error());
		}
		if(*branch_4096 != 0U) {
			return 4096U - ((static_cast<std::size_t>(*a) * 1024U) +
				(static_cast<std::size_t>(*b) * 512U) +
				(static_cast<std::size_t>(*c) * 256U) + operand);
		}
		auto d = bit();
		if(!d) {
			return std::unexpected(d.error());
		}
		return 8192U - ((static_cast<std::size_t>(*a) * 2048U) +
			(static_cast<std::size_t>(*b) * 1024U) +
			(static_cast<std::size_t>(*c) * 512U) +
			(static_cast<std::size_t>(*d) * 256U) + operand);
	}

	[[nodiscard]] std::expected<std::size_t, DietExecutableError> match_length()
	{
		for(std::size_t bit_count = 1U; bit_count <= 4U; ++bit_count) {
			auto current = bit();
			if(!current) {
				return std::unexpected(current.error());
			}
			if(*current != 0U) {
				return 2U + bit_count;
			}
		}
		auto first = bit();
		auto second = bit();
		if(!first || !second) {
			return std::unexpected((!first) ? first.error() : second.error());
		}
		if(*first != 0U) {
			return 7U + *second;
		}
		if(*second == 0U) {
			auto a = bit();
			auto b = bit();
			auto c = bit();
			if(!a || !b || !c) {
				return std::unexpected((!a) ? a.error() :
					((!b) ? b.error() : c.error()));
			}
			return 9U + (static_cast<std::size_t>(*a) * 4U) +
				(static_cast<std::size_t>(*b) * 2U) + *c;
		}
		auto extended = byte();
		if(!extended) {
			return std::unexpected(extended.error());
		}
		return 17U + *extended;
	}

	std::span<const std::byte> input_{};
	std::size_t output_limit_ = 0U;
	bool segment_refresh_allowed_ = false;
	std::size_t cursor_ = 0U;
	std::uint16_t bit_buffer_ = 0U;
	std::uint8_t bits_remaining_ = 0U;
	std::size_t segment_refreshes_ = 0U;
	std::vector<std::byte> output_{};
};

[[nodiscard]] std::expected<std::vector<std::byte>, DietExecutableError>
reconstruct_executable(
	std::span<const std::byte> packed,
	std::span<const std::byte> idata,
	std::size_t original_size
)
{
	if(diet_amz_pointer_offset + 2U > packed.size()) {
		return std::unexpected(error(
			DietExecutableErrorCode::invalid_reconstructed_mz,
			diet_amz_pointer_offset
		));
	}
	const auto approximate_mz =
		static_cast<std::size_t>(read_u16(packed, diet_amz_pointer_offset)) * 16U;
	const auto mz_offset = approximate_mz + (original_size & 0x0FU);
	if(mz_offset > idata.size() || (idata.size() - mz_offset) < 2U) {
		return std::unexpected(error(
			DietExecutableErrorCode::invalid_reconstructed_mz,
			mz_offset
		));
	}
	const auto signature = read_u16(idata, mz_offset);
	if(signature != 0x5A4DU && signature != 0x4D5AU) {
		return std::unexpected(error(
			DietExecutableErrorCode::invalid_reconstructed_mz,
			mz_offset
		));
	}

	std::array<std::byte, mz_minimum_header_bytes> header{};
	const auto available_header = std::min(
		header.size(), idata.size() - mz_offset
	);
	std::copy_n(idata.begin() + static_cast<std::ptrdiff_t>(mz_offset),
		available_header, header.begin());
	header[3U] &= std::byte{0x01U};
	const auto declared_size = declared_mz_bytes(
		read_u16(header, 2U), read_u16(header, 4U)
	);
	const auto relocation_count = static_cast<std::size_t>(read_u16(header, 6U));
	const auto header_bytes = static_cast<std::size_t>(read_u16(header, 8U)) * 16U;
	const auto relocation_offset = static_cast<std::size_t>(read_u16(header, 24U));
	if(declared_size != original_size || header_bytes < header.size() ||
		header_bytes > original_size || mz_offset != (original_size - header_bytes) ||
		relocation_offset < header.size() || relocation_offset > header_bytes ||
		relocation_count > ((header_bytes - relocation_offset) / 4U)) {
		return std::unexpected(error(
			DietExecutableErrorCode::invalid_reconstructed_mz,
			mz_offset
		));
	}

	const auto cdata_one_size = (relocation_count == 0U) ?
		(header_bytes - header.size()) : (relocation_offset - header.size());
	const auto relocation_bytes = relocation_count * 4U;
	const auto cdata_two_size = (relocation_count == 0U) ? 0U :
		(header_bytes - relocation_offset - relocation_bytes);
	const auto encoded_relocation_offset = mz_offset + relocation_offset;
	if(mz_offset + header.size() > idata.size() ||
		cdata_one_size > (idata.size() - mz_offset - header.size()) ||
		encoded_relocation_offset > idata.size()) {
		return std::unexpected(error(
			DietExecutableErrorCode::invalid_reconstructed_mz,
			mz_offset
		));
	}

	std::vector<std::byte> decoded_relocations{};
	decoded_relocations.reserve(relocation_bytes);
	std::size_t encoded_cursor = encoded_relocation_offset;
	std::uint16_t segment = 0U;
	std::uint16_t offset = 0U;
	for(std::size_t index = 0U; index < relocation_count; ++index) {
		if(encoded_cursor > idata.size() ||
			(idata.size() - encoded_cursor) < 2U) {
			return std::unexpected(error(
				DietExecutableErrorCode::invalid_relocation_table,
				encoded_cursor
			));
		}
		const auto encoded = read_u16(idata, encoded_cursor);
		encoded_cursor += 2U;
		if((encoded & 0x8000U) != 0U) {
			const auto delta = (encoded >= 0xC000U) ? encoded :
				static_cast<std::uint16_t>(encoded - 0x8000U);
			offset = static_cast<std::uint16_t>(offset + delta);
		} else {
			segment = encoded;
			if(encoded_cursor > idata.size() ||
				(idata.size() - encoded_cursor) < 2U) {
				return std::unexpected(error(
					DietExecutableErrorCode::invalid_relocation_table,
					encoded_cursor
				));
			}
			offset = read_u16(idata, encoded_cursor);
			encoded_cursor += 2U;
		}
		append_u16(decoded_relocations, offset);
		append_u16(decoded_relocations, segment);
	}
	if(cdata_two_size > original_size || encoded_cursor > idata.size()) {
		return std::unexpected(error(
			DietExecutableErrorCode::invalid_relocation_table,
			encoded_cursor
		));
	}

	std::vector<std::byte> result{};
	result.reserve(original_size);
	result.insert(result.end(), header.begin(), header.end());
	result.insert(
		result.end(),
		idata.begin() + static_cast<std::ptrdiff_t>(mz_offset + header.size()),
		idata.begin() + static_cast<std::ptrdiff_t>(
			mz_offset + header.size() + cdata_one_size
		)
	);
	result.insert(
		result.end(), decoded_relocations.begin(), decoded_relocations.end()
	);
	for(std::size_t index = 0U; index < cdata_two_size; ++index) {
		const auto source = encoded_cursor + index;
		result.push_back((source < idata.size()) ? idata[source] : std::byte{0U});
	}
	result.insert(
		result.end(), idata.begin(),
		idata.begin() + static_cast<std::ptrdiff_t>(mz_offset)
	);
	if(result.size() != original_size) {
		return std::unexpected(error(
			DietExecutableErrorCode::invalid_reconstructed_mz,
			result.size()
		));
	}
	return result;
}

} // namespace

std::expected<DietStreamResult, DietExecutableError>
decode_diet_stream_for_test(
	std::span<const std::byte> compressed,
	std::size_t output_limit,
	bool segment_refresh_allowed
)
{
	return DietStreamDecoder(
		compressed, output_limit, segment_refresh_allowed
	).run();
}

std::expected<std::vector<std::byte>, DietExecutableError>
decode_diet_145f_executable(
	std::span<const std::byte> packed,
	const DietExecutableProfile &profile
)
{
	if(packed.size() != profile.packed_file_bytes ||
		main_executable_sha256(packed) != profile.packed_sha256) {
		return std::unexpected(error(
			DietExecutableErrorCode::unsupported_profile
		));
	}
	if(packed.size() < diet_compressed_offset ||
		read_u16(packed, 0U) != 0x5A4DU ||
		declared_mz_bytes(read_u16(packed, 2U), read_u16(packed, 4U)) !=
			packed.size() ||
		read_u16(packed, 6U) != 0U ||
		read_u16(packed, 8U) * 16U != diet_host_header_bytes ||
		read_u16(packed, 20U) != 0U || read_u16(packed, 22U) != 0U) {
		return std::unexpected(error(
			DietExecutableErrorCode::invalid_packed_mz
		));
	}
	if(diet_fingerprint_offset + diet_145f_fingerprint.size() > packed.size() ||
		!std::equal(
			diet_145f_fingerprint.begin(), diet_145f_fingerprint.end(),
			packed.begin() + static_cast<std::ptrdiff_t>(diet_fingerprint_offset)
		) || packed[diet_dlz_offset] != std::byte{'d'} ||
		packed[diet_dlz_offset + 1U] != std::byte{'l'} ||
		packed[diet_dlz_offset + 2U] != std::byte{'z'}) {
		return std::unexpected(error(
			DietExecutableErrorCode::invalid_diet_header,
			diet_dlz_offset
		));
	}
	const auto size_and_flags = std::to_integer<std::uint8_t>(
		packed[diet_dlz_offset + 3U]
	);
	const auto flags = static_cast<std::uint8_t>(size_and_flags & 0xF0U);
	const auto compressed_size =
		(static_cast<std::size_t>(size_and_flags & 0x0FU) << 16U) |
		read_u16(packed, diet_dlz_offset + 4U);
	const auto original_size_and_flags = std::to_integer<std::uint8_t>(
		packed[diet_dlz_offset + 8U]
	);
	const auto original_size =
		(static_cast<std::size_t>(original_size_and_flags & 0xFCU) << 14U) |
		read_u16(packed, diet_dlz_offset + 9U);
	if((flags != 0x20U && flags != 0x30U) ||
		(original_size_and_flags & 0x03U) != 0U ||
		compressed_size == 0U || original_size != profile.unpacked_file_bytes ||
		diet_compressed_offset > packed.size() ||
		compressed_size > (packed.size() - diet_compressed_offset)) {
		return std::unexpected(error(
			DietExecutableErrorCode::invalid_diet_header,
			diet_dlz_offset + 3U
		));
	}
	const auto compressed = packed.subspan(
		diet_compressed_offset, compressed_size
	);
	if(crc16_arc(compressed) != read_u16(packed, diet_dlz_offset + 6U)) {
		return std::unexpected(error(
			DietExecutableErrorCode::crc_mismatch,
			diet_dlz_offset + 6U
		));
	}
	DietStreamDecoder decoder(
		compressed, original_size, (flags & 0x10U) != 0U
	);
	auto stream = decoder.run();
	if(!stream) {
		auto stream_error = stream.error();
		stream_error.offset += diet_compressed_offset;
		return std::unexpected(stream_error);
	}
	auto reconstructed = reconstruct_executable(
		packed, stream->idata, original_size
	);
	if(!reconstructed) {
		return std::unexpected(reconstructed.error());
	}
	if(reconstructed->size() != profile.unpacked_file_bytes ||
		main_executable_sha256(*reconstructed) != profile.unpacked_sha256) {
		return std::unexpected(error(
			DietExecutableErrorCode::reconstructed_profile_mismatch
		));
	}
	return reconstructed;
}

} // namespace th03::app::detail
