#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <vector>

#include "op_compiled_text_internal.hpp"

namespace {

using th03::app::MainExecutableSha256;
using th03::app::OpCompiledTextErrorCode;
using th03::app::OpCompiledTextItem;
using th03::app::OpExecutableProfile;
using th03::app::OpMusicRoomTextImport;

constexpr std::size_t profile_file_bytes = 63866U;
constexpr std::size_t header_bytes = 4096U;
constexpr std::size_t track_load_offset = 0xDE84U;
constexpr std::size_t payload_bytes = 32U;
constexpr std::size_t stride_bytes = payload_bytes + 1U;
constexpr std::size_t quit_load_offset = track_load_offset +
	(OpMusicRoomTextImport::quit_choice_index * stride_bytes);

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

void write_u16(
	std::vector<std::byte> &bytes,
	std::size_t offset,
	std::uint16_t value
)
{
	bytes[offset] = static_cast<std::byte>(value & 0xFFU);
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

[[nodiscard]] std::size_t choice_load_offset(std::size_t index)
{
	return (index == OpMusicRoomTextImport::quit_choice_index) ?
		quit_load_offset : (track_load_offset + (index * stride_bytes));
}

struct Fixture final {
	std::vector<std::byte> executable{};
	std::array<
		std::vector<std::uint8_t>,
		OpMusicRoomTextImport::choice_count
	> expected{};
};

Fixture fixture()
{
	Fixture result{};
	result.executable.assign(profile_file_bytes, std::byte{0xCCU});
	write_u16(result.executable, 0U, 0x5A4DU);
	write_u16(result.executable, 2U, 378U);
	write_u16(result.executable, 4U, 125U);
	write_u16(result.executable, 6U, 0U);
	write_u16(result.executable, 8U, 256U);
	write_u16(result.executable, 24U, 28U);

	for(std::size_t index = 0U; index < result.expected.size(); ++index) {
		if(index == OpMusicRoomTextImport::empty_choice_index) {
			continue;
		}
		auto &expected = result.expected[index];
		expected.reserve(payload_bytes);
		for(std::size_t byte = 0U; byte < payload_bytes; ++byte) {
			expected.push_back(static_cast<std::uint8_t>(
				'A' + ((index + byte) % 26U)
			));
		}
		const auto file_offset = header_bytes + choice_load_offset(index);
		for(std::size_t byte = 0U; byte < expected.size(); ++byte) {
			result.executable[file_offset + byte] =
				static_cast<std::byte>(expected[byte]);
		}
		result.executable[file_offset + expected.size()] = std::byte{0U};
	}
	return result;
}

std::expected<OpMusicRoomTextImport, th03::app::OpCompiledTextError>
import_fixture(std::vector<std::byte> &bytes)
{
	const auto digest = th03::app::main_executable_sha256(bytes);
	return th03::app::detail::import_op_music_room_text_for_test(bytes, digest);
}

void test_success_and_ownership()
{
	auto source = fixture();
	auto imported = import_fixture(source.executable);
	expect(imported.has_value(), "synthetic authenticated OP profile imports");
	if(!imported) {
		return;
	}
	expect(
		imported->profile == OpExecutableProfile::unpacked_original_japanese,
		"OP profile identity is retained"
	);
	std::fill(source.executable.begin(), source.executable.end(), std::byte{0U});
	for(std::size_t index = 0U; index < imported->choices.size(); ++index) {
		expect(
			imported->choices[index] == source.expected[index],
			"each imported OP choice owns its exact payload"
		);
	}
	expect(
		imported->choices[OpMusicRoomTextImport::empty_choice_index].empty(),
		"source-semantic separator row imports no original bytes"
	);
}

void test_container_rejections()
{
	std::array<std::byte, 12U> short_input{};
	auto short_result = th03::app::detail::import_op_music_room_text_for_test(
		short_input,
		MainExecutableSha256{}
	);
	expect(
		!short_result &&
		short_result.error().code == OpCompiledTextErrorCode::input_too_short,
		"short OP input is rejected before MZ reads"
	);

	auto invalid_signature = fixture();
	invalid_signature.executable[0U] = std::byte{'N'};
	auto invalid_signature_result = import_fixture(invalid_signature.executable);
	expect(
		!invalid_signature_result &&
		invalid_signature_result.error().code ==
			OpCompiledTextErrorCode::invalid_mz_signature,
		"invalid OP MZ signature is rejected"
	);

	auto invalid_pages = fixture();
	write_u16(invalid_pages.executable, 2U, 512U);
	auto invalid_pages_result = import_fixture(invalid_pages.executable);
	expect(
		!invalid_pages_result &&
		invalid_pages_result.error().code ==
			OpCompiledTextErrorCode::invalid_mz_layout,
		"invalid OP final-page byte count is rejected"
	);

	auto mismatched_size = fixture();
	write_u16(mismatched_size.executable, 4U, 124U);
	auto mismatched_size_result = import_fixture(mismatched_size.executable);
	expect(
		!mismatched_size_result &&
		mismatched_size_result.error().code ==
			OpCompiledTextErrorCode::invalid_mz_layout,
		"OP MZ declared size mismatch is rejected"
	);

	auto invalid_relocations = fixture();
	write_u16(invalid_relocations.executable, 6U, 0xFFFFU);
	auto invalid_relocations_result = import_fixture(
		invalid_relocations.executable
	);
	expect(
		!invalid_relocations_result &&
		invalid_relocations_result.error().code ==
			OpCompiledTextErrorCode::invalid_mz_layout,
		"OP relocation table bounds are validated"
	);

	auto unsupported = fixture();
	const auto accepted = th03::app::main_executable_sha256(
		unsupported.executable
	);
	unsupported.executable.back() ^= std::byte{1U};
	auto unsupported_result =
		th03::app::detail::import_op_music_room_text_for_test(
			unsupported.executable,
			accepted
		);
	expect(
		!unsupported_result &&
		unsupported_result.error().code ==
			OpCompiledTextErrorCode::unsupported_profile,
		"whole-file OP digest authenticates before private ranges"
	);
}

void test_range_and_text_rejections()
{
	auto range = fixture();
	write_u16(range.executable, 8U, 3840U);
	auto range_result = import_fixture(range.executable);
	expect(
		!range_result &&
		range_result.error().code ==
			OpCompiledTextErrorCode::compiled_range_out_of_bounds &&
		range_result.error().item == OpCompiledTextItem::track &&
		range_result.error().choice_index == 0U,
		"authenticated OP compiled range is independently bounded"
	);

	auto track_terminator = fixture();
	constexpr std::size_t track_index = 7U;
	const auto track_terminator_offset = header_bytes +
		choice_load_offset(track_index) + payload_bytes;
	track_terminator.executable[track_terminator_offset] = std::byte{'X'};
	auto track_terminator_result = import_fixture(
		track_terminator.executable
	);
	expect(
		!track_terminator_result &&
		track_terminator_result.error().code ==
			OpCompiledTextErrorCode::malformed_source_text &&
		track_terminator_result.error().item == OpCompiledTextItem::track &&
		track_terminator_result.error().choice_index == track_index &&
		track_terminator_result.error().offset == track_terminator_offset,
		"track-label terminator failure identifies only index and offset"
	);

	auto track_shift_jis = fixture();
	constexpr std::size_t invalid_track_index = 12U;
	const auto invalid_track_offset = header_bytes +
		choice_load_offset(invalid_track_index) + 3U;
	track_shift_jis.executable[invalid_track_offset] = std::byte{0x80U};
	auto track_shift_jis_result = import_fixture(track_shift_jis.executable);
	expect(
		!track_shift_jis_result &&
		track_shift_jis_result.error().code ==
			OpCompiledTextErrorCode::malformed_source_text &&
		track_shift_jis_result.error().item == OpCompiledTextItem::track &&
		track_shift_jis_result.error().choice_index == invalid_track_index &&
		track_shift_jis_result.error().offset == invalid_track_offset,
		"invalid track-label Shift-JIS reports a content-free coordinate"
	);

	auto quit_terminator = fixture();
	const auto quit_terminator_offset = header_bytes + quit_load_offset +
		payload_bytes;
	quit_terminator.executable[quit_terminator_offset] = std::byte{'X'};
	auto quit_terminator_result = import_fixture(quit_terminator.executable);
	expect(
		!quit_terminator_result &&
		quit_terminator_result.error().item == OpCompiledTextItem::quit &&
		quit_terminator_result.error().choice_index ==
			OpMusicRoomTextImport::quit_choice_index &&
		quit_terminator_result.error().offset == quit_terminator_offset,
		"Quit terminator failure reports its semantic role"
	);

	auto quit_shift_jis = fixture();
	const auto quit_invalid_offset = header_bytes + quit_load_offset + 4U;
	quit_shift_jis.executable[quit_invalid_offset] = std::byte{0x80U};
	auto quit_shift_jis_result = import_fixture(quit_shift_jis.executable);
	expect(
		!quit_shift_jis_result &&
		quit_shift_jis_result.error().item == OpCompiledTextItem::quit &&
		quit_shift_jis_result.error().offset == quit_invalid_offset,
		"invalid Quit Shift-JIS reports no source byte"
	);
}

[[nodiscard]] std::expected<std::vector<std::byte>, const char *>
read_all(const std::filesystem::path &path)
{
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected("local unpacked OP input is readable");
	}
	std::vector<char> chars(
		(std::istreambuf_iterator<char>(input)),
		std::istreambuf_iterator<char>()
	);
	std::vector<std::byte> bytes;
	bytes.reserve(chars.size());
	for(const char value : chars) {
		bytes.push_back(static_cast<std::byte>(
			static_cast<unsigned char>(value)
		));
	}
	return bytes;
}

void test_authentic_profile(const std::filesystem::path &path)
{
	auto executable = read_all(path);
	expect(executable.has_value(), executable ? "" : executable.error());
	if(!executable) {
		return;
	}
	auto imported = th03::app::import_op_music_room_text(*executable);
	expect(imported.has_value(), "local unpacked OP profile authenticates");
	if(!imported) {
		return;
	}
	for(std::size_t index = 0U; index < imported->choices.size(); ++index) {
		const auto expected_size =
			(index == OpMusicRoomTextImport::empty_choice_index) ? 0U :
			payload_bytes;
		expect(
			imported->choices[index].size() == expected_size,
			"authentic OP label has exact semantic size"
		);
	}
}

} // namespace

int main(int argc, char **argv)
{
	test_success_and_ownership();
	test_container_rejections();
	test_range_and_text_rejections();
	if(argc == 2) {
		test_authentic_profile(argv[1]);
	} else {
		expect(argc == 1, "optional local gate accepts one unpacked OP path");
	}

	if(failures != 0U) {
		std::cerr << failures << " of " << checks
			<< " OP compiled-text checks failed\n";
		return 1;
	}
	std::cout << checks << " OP compiled-text checks passed\n";
	return 0;
}
