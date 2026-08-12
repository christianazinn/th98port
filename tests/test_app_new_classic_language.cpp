#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "new_classic_language_internal.hpp"

namespace {

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

void put_u16(std::span<std::byte> bytes, std::size_t offset, std::uint16_t value)
{
	bytes[offset] = static_cast<std::byte>(value & 0xFFU);
	bytes[offset + 1U] = static_cast<std::byte>(value >> 8U);
}

void put_u32(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value)
{
	for(std::size_t index = 0U; index < 4U; ++index) {
		bytes[offset + index] = static_cast<std::byte>(value >> (index * 8U));
	}
}

std::vector<std::byte> archive(bool invalid_type = false)
{
	constexpr std::array names{std::string_view{"A.TXT"}, std::string_view{"B.PI"}};
	constexpr std::array<std::array<std::uint8_t, 3U>, 2U> payloads{{
		{{1U, 2U, 3U}}, {{4U, 5U, 6U}},
	}};
	constexpr std::size_t header = 16U;
	constexpr std::size_t entry = 32U;
	constexpr std::size_t directory_size = (names.size() + 1U) * entry;
	std::vector<std::byte> result(header + directory_size + 6U);
	put_u16(result, 0U, static_cast<std::uint16_t>(directory_size));
	put_u16(result, 2U, 2U);
	put_u16(result, 4U, static_cast<std::uint16_t>(names.size()));
	put_u16(result, 6U, 0xF5U);
	std::vector<std::byte> directory(directory_size);
	std::size_t payload_offset = header + directory_size;
	for(std::size_t member = 0U; member < names.size(); ++member) {
		const auto base = member * entry;
		put_u16(directory, base, invalid_type && member == 0U ? 0x1234U : 0xF388U);
		for(std::size_t index = 0U; index < names[member].size(); ++index) {
			directory[base + 3U + index] = static_cast<std::byte>(names[member][index]);
		}
		put_u16(directory, base + 16U, static_cast<std::uint16_t>(
			payloads[member].size()
		));
		put_u16(directory, base + 18U, static_cast<std::uint16_t>(
			payloads[member].size()
		));
		put_u32(directory, base + 20U, static_cast<std::uint32_t>(payload_offset));
		for(const auto value : payloads[member]) {
			result[payload_offset++] = static_cast<std::byte>(value);
		}
	}
	auto key = std::uint8_t{0xF5U};
	for(std::size_t index = 0U; index < directory.size(); ++index) {
		const auto plain = std::to_integer<std::uint8_t>(directory[index]);
		result[header + index] = static_cast<std::byte>(plain ^ key);
		key = static_cast<std::uint8_t>(key - plain);
	}
	return result;
}

auto parse(std::span<const std::byte> bytes)
{
	return th03::app::detail::parse_new_classic_language_overlay_for_test(
		bytes, th03::app::main_executable_sha256(bytes)
	);
}

void test_bounded_archive()
{
	auto valid_bytes = archive();
	auto valid = parse(valid_bytes);
	expect(valid && valid->member_count() == 2U && valid->contains("A.TXT") &&
		valid->contains("B.PI"),
		"bounded archive decrypts exact unique uncompressed members");

	auto tampered = valid_bytes;
	tampered.back() ^= std::byte{1U};
	const auto digest = th03::app::main_executable_sha256(valid_bytes);
	auto rejected_digest =
		th03::app::detail::parse_new_classic_language_overlay_for_test(
			tampered, digest
		);
	expect(!rejected_digest && rejected_digest.error().code ==
		th03::app::NewClassicLanguageOverlayErrorCode::digest_mismatch,
		"whole-file digest is checked before directory bytes");

	auto bad_header = valid_bytes;
	put_u16(bad_header, 2U, 3U);
	auto rejected_header = parse(bad_header);
	expect(!rejected_header && rejected_header.error().code ==
		th03::app::NewClassicLanguageOverlayErrorCode::header_shape,
		"archive rejects the wrong packfile format");

	auto bad_directory = valid_bytes;
	put_u16(bad_directory, 0U, 32U);
	auto rejected_directory = parse(bad_directory);
	expect(!rejected_directory && rejected_directory.error().code ==
		th03::app::NewClassicLanguageOverlayErrorCode::directory_shape,
		"archive rejects inconsistent directory bounds");

	auto rejected_member = parse(archive(true));
	expect(!rejected_member && rejected_member.error().code ==
		th03::app::NewClassicLanguageOverlayErrorCode::member_shape,
		"archive accepts only bounded uncompressed member entries");

	auto trailing = valid_bytes;
	trailing.push_back(std::byte{0U});
	auto rejected_trailing = parse(trailing);
	expect(!rejected_trailing && rejected_trailing.error().code ==
		th03::app::NewClassicLanguageOverlayErrorCode::member_shape,
		"archive rejects unowned trailing payload bytes");
}

std::vector<std::byte> read_file(const std::filesystem::path &path)
{
	const auto size = std::filesystem::file_size(path);
	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	std::ifstream input(path, std::ios::binary);
	input.read(reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size()));
	return bytes;
}

bool authentic_text_gate(
	const std::filesystem::path &main_path,
	const std::filesystem::path &mainl_path,
	const std::filesystem::path &op_path,
	const std::filesystem::path &overlay_path
)
{
	auto overlay = th03::app::load_new_classic_language_overlay(overlay_path);
	if(!overlay || overlay->member_count() != 52U) {
		return false;
	}
	const auto main_bytes = read_file(main_path);
	const auto mainl_bytes = read_file(mainl_path);
	const auto op_bytes = read_file(op_path);
	auto compiled = th03::app::import_main_compiled_content(main_bytes);
	auto mainl = th03::app::import_mainl_stage_text(mainl_bytes);
	auto op = th03::app::import_op_music_room_text(op_bytes);
	if(!compiled || !mainl || !op) {
		return false;
	}
	const auto japanese_results = compiled->hud_result_tram_assets();
	const auto japanese_max_combo = std::vector<th03::present::TramCell>{
		japanese_results.result_labels[static_cast<std::size_t>(
			th03::present::MainResultLabel::max_combo
		)].begin(),
		japanese_results.result_labels[static_cast<std::size_t>(
			th03::present::MainResultLabel::max_combo
		)].end(),
	};
	const auto japanese_gauge = std::vector<th03::present::TramCell>{
		japanese_results.result_labels[static_cast<std::size_t>(
			th03::present::MainResultLabel::gauge_attack
		)].begin(),
		japanese_results.result_labels[static_cast<std::size_t>(
			th03::present::MainResultLabel::gauge_attack
		)].end(),
	};
	th03::app::apply_new_classic_english_text(
		*compiled, mainl->assets, *op
	);
	const auto english_results = compiled->hud_result_tram_assets();
	const auto music_first = std::string{
		op->choices[0U].begin(), op->choices[0U].end()
	};
	const auto title_second = std::string{
		mainl->assets.characters[0U].title_second.begin(),
		mainl->assets.characters[0U].title_second.end()
	};
	const auto registration = std::string{
		mainl->assets.registration_playchars[0U].begin(),
		mainl->assets.registration_playchars[0U].end()
	};
	return music_first == "NO.1 Dream Transcending Space..." &&
		title_second == "        and Tradition       " &&
		registration == "  No Entry! " &&
		std::ranges::equal(
			japanese_max_combo,
			english_results.result_labels[static_cast<std::size_t>(
				th03::present::MainResultLabel::max_combo
			)]
		) && !std::ranges::equal(
			japanese_gauge,
			english_results.result_labels[static_cast<std::size_t>(
				th03::present::MainResultLabel::gauge_attack
			)]
		);
}

} // namespace

int main(int argc, char **argv)
{
	test_bounded_archive();
	if(argc == 6 && std::string_view(argv[1]) == "--authentic") {
		expect(authentic_text_gate(argv[2], argv[3], argv[4], argv[5]),
			"authenticated overlay applies every compiled English text family");
	} else if(argc != 1) {
		std::cerr << "usage: th03-app-new-classic-language-tests "
			"[--authentic <MAIN.EXE> <MAINL.EXE> <OP.EXE> <TH3EN.DAT>]\n";
		return 2;
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
