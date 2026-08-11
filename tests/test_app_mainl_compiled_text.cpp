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

#include "mainl_compiled_text_internal.hpp"

namespace {

using th03::app::MainExecutableSha256;
using th03::app::MainlCompiledTextErrorCode;
using th03::app::MainlCompiledTextItem;
using th03::app::MainlExecutableProfile;
using th03::app::MainlStageTextImport;

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

struct TextSpec final {
	MainlCompiledTextItem item;
	std::uint8_t character;
	std::size_t load_offset;
	std::size_t payload_bytes;
};

constexpr std::array<TextSpec, 18U> specs = {{
	{MainlCompiledTextItem::title, 0U, 0xE5C8U, 28U},
	{MainlCompiledTextItem::name, 0U, 0xE5E5U, 13U},
	{MainlCompiledTextItem::title, 1U, 0xE5F3U, 28U},
	{MainlCompiledTextItem::name, 1U, 0xE610U, 10U},
	{MainlCompiledTextItem::title, 2U, 0xE61BU, 28U},
	{MainlCompiledTextItem::name, 2U, 0xE638U, 15U},
	{MainlCompiledTextItem::title, 3U, 0xE648U, 28U},
	{MainlCompiledTextItem::name, 3U, 0xE665U, 10U},
	{MainlCompiledTextItem::title, 4U, 0xE670U, 28U},
	{MainlCompiledTextItem::name, 4U, 0xE68DU, 10U},
	{MainlCompiledTextItem::title, 5U, 0xE698U, 28U},
	{MainlCompiledTextItem::name, 5U, 0xE6B5U, 16U},
	{MainlCompiledTextItem::title, 6U, 0xE6C6U, 28U},
	{MainlCompiledTextItem::name, 6U, 0xE6E3U, 14U},
	{MainlCompiledTextItem::title, 7U, 0xE6F2U, 28U},
	{MainlCompiledTextItem::name, 7U, 0xE70FU, 15U},
	{MainlCompiledTextItem::title, 8U, 0xE71FU, 28U},
	{MainlCompiledTextItem::name, 8U, 0xE73CU, 13U},
}};

constexpr std::array<TextSpec, 10U> registration_specs = {{
	{MainlCompiledTextItem::registration_playchar, 0U, 0xED72U, 12U},
	{MainlCompiledTextItem::registration_playchar, 1U, 0xED7FU, 12U},
	{MainlCompiledTextItem::registration_playchar, 2U, 0xED8CU, 12U},
	{MainlCompiledTextItem::registration_playchar, 3U, 0xED99U, 12U},
	{MainlCompiledTextItem::registration_playchar, 4U, 0xEDA6U, 12U},
	{MainlCompiledTextItem::registration_playchar, 5U, 0xEDB3U, 12U},
	{MainlCompiledTextItem::registration_playchar, 6U, 0xEDC0U, 12U},
	{MainlCompiledTextItem::registration_playchar, 7U, 0xEDCDU, 12U},
	{MainlCompiledTextItem::registration_playchar, 8U, 0xEDDAU, 12U},
	{MainlCompiledTextItem::registration_playchar, 9U, 0xEDE7U, 12U},
}};

constexpr std::array<TextSpec, 15U> staff_specs = {{
	{MainlCompiledTextItem::staff_rank, 0U, 0xEFC2U, 11U},
	{MainlCompiledTextItem::staff_rank, 1U, 0xEFCEU, 13U},
	{MainlCompiledTextItem::staff_rank, 2U, 0xEFDCU, 11U},
	{MainlCompiledTextItem::staff_rank, 3U, 0xEFE8U, 14U},
	{MainlCompiledTextItem::staff_number, 0U, 0xEFF7U, 2U},
	{MainlCompiledTextItem::staff_number, 1U, 0xEFFAU, 2U},
	{MainlCompiledTextItem::staff_number, 2U, 0xEFFDU, 2U},
	{MainlCompiledTextItem::staff_number, 3U, 0xF000U, 2U},
	{MainlCompiledTextItem::staff_number, 4U, 0xF003U, 2U},
	{MainlCompiledTextItem::staff_number, 5U, 0xF006U, 2U},
	{MainlCompiledTextItem::staff_number, 6U, 0xF009U, 2U},
	{MainlCompiledTextItem::staff_number, 7U, 0xF00CU, 2U},
	{MainlCompiledTextItem::staff_number, 8U, 0xF00FU, 2U},
	{MainlCompiledTextItem::staff_number, 9U, 0xF012U, 2U},
	{MainlCompiledTextItem::staff_point, 0U, 0xF015U, 2U},
}};

constexpr std::size_t flake_masks_load_offset = 0xEE52U;

struct Fixture final {
	std::vector<std::byte> executable{};
	std::array<std::vector<std::uint8_t>, 18U> expected{};
	std::array<std::vector<std::uint8_t>, 10U> expected_registration{};
	std::array<std::vector<std::uint8_t>, 15U> expected_staff{};
	std::array<std::uint8_t, 64U> expected_flake_masks{};
};

Fixture fixture()
{
	Fixture result{};
	result.executable.assign(67372U, std::byte{0xCCU});
	write_u16(result.executable, 0U, 0x5A4DU);
	write_u16(result.executable, 2U, 300U);
	write_u16(result.executable, 4U, 132U);
	write_u16(result.executable, 6U, 0U);
	write_u16(result.executable, 8U, 256U);
	write_u16(result.executable, 24U, 28U);

	for(std::size_t index = 0U; index < specs.size(); ++index) {
		const auto &spec = specs[index];
		auto &expected = result.expected[index];
		expected.reserve(spec.payload_bytes);
		while(expected.size() + 1U < spec.payload_bytes) {
			expected.push_back(static_cast<std::uint8_t>(
				0x81U + (spec.character % 0x1FU)
			));
			expected.push_back(static_cast<std::uint8_t>(
				0x40U + ((index + expected.size()) % 0x3EU)
			));
		}
		if(expected.size() < spec.payload_bytes) {
			expected.push_back(static_cast<std::uint8_t>('A' + spec.character));
		}
		const auto file_offset = 4096U + spec.load_offset;
		for(std::size_t byte = 0U; byte < expected.size(); ++byte) {
			result.executable[file_offset + byte] =
				static_cast<std::byte>(expected[byte]);
		}
		result.executable[file_offset + expected.size()] = std::byte{0U};
	}
	for(std::size_t index = 0U; index < staff_specs.size(); ++index) {
		const auto &spec = staff_specs[index];
		auto &expected = result.expected_staff[index];
		expected.reserve(spec.payload_bytes);
		while(expected.size() + 1U < spec.payload_bytes) {
			expected.push_back(static_cast<std::uint8_t>(
				0x81U + (index % 0x1FU)
			));
			expected.push_back(static_cast<std::uint8_t>(
				0x40U + ((index + expected.size()) % 0x3EU)
			));
		}
		if(expected.size() < spec.payload_bytes) {
			expected.push_back(static_cast<std::uint8_t>('a' + index));
		}
		const auto file_offset = 4096U + spec.load_offset;
		for(std::size_t byte = 0U; byte < expected.size(); ++byte) {
			result.executable[file_offset + byte] =
				static_cast<std::byte>(expected[byte]);
		}
		result.executable[file_offset + expected.size()] = std::byte{0U};
	}
	for(std::size_t index = 0U; index < registration_specs.size(); ++index) {
		const auto &spec = registration_specs[index];
		auto &expected = result.expected_registration[index];
		for(std::size_t pair = 0U; pair < 6U; ++pair) {
			expected.push_back(static_cast<std::uint8_t>(0x81U + index));
			expected.push_back(static_cast<std::uint8_t>(0x40U + pair));
		}
		const auto file_offset = 4096U + spec.load_offset;
		for(std::size_t byte = 0U; byte < expected.size(); ++byte) {
			result.executable[file_offset + byte] =
				static_cast<std::byte>(expected[byte]);
		}
		result.executable[file_offset + expected.size()] = std::byte{0U};
	}
	for(std::size_t index = 0U; index < result.expected_flake_masks.size();
		++index) {
		result.expected_flake_masks[index] = static_cast<std::uint8_t>(
			(index * 37U) & 0xFFU
		);
		result.executable[4096U + flake_masks_load_offset + index] =
			static_cast<std::byte>(result.expected_flake_masks[index]);
	}
	return result;
}

std::expected<MainlStageTextImport, th03::app::MainlCompiledTextError>
import_fixture(std::vector<std::byte> &bytes)
{
	const auto digest = th03::app::main_executable_sha256(bytes);
	return th03::app::detail::import_mainl_stage_text_for_test(bytes, digest);
}

void test_success_and_ownership()
{
	auto source = fixture();
	auto imported = import_fixture(source.executable);
	expect(imported.has_value(), "synthetic authenticated profile imports");
	if(!imported) {
		return;
	}
	expect(
		imported->profile == MainlExecutableProfile::unpacked_original_japanese,
		"profile identity is retained"
	);
	std::fill(source.executable.begin(), source.executable.end(), std::byte{0U});
	for(std::size_t index = 0U; index < specs.size(); ++index) {
		const auto &spec = specs[index];
		const auto &character = imported->assets.characters[spec.character];
		const auto &actual = spec.item == MainlCompiledTextItem::title ?
			character.title : character.name;
		expect(actual == source.expected[index], "exact owning text payload");
	}
	for(std::size_t index = 0U; index < staff_specs.size(); ++index) {
		const auto &spec = staff_specs[index];
		const std::vector<std::uint8_t> *actual = &imported->staff.point;
		if(spec.item == MainlCompiledTextItem::staff_rank) {
			actual = &imported->staff.ranks[spec.character];
		} else if(spec.item == MainlCompiledTextItem::staff_number) {
			actual = &imported->staff.numbers[spec.character];
		}
		expect(
			*actual == source.expected_staff[index],
			"exact owning verdict payload"
		);
	}
	for(std::size_t index = 0U; index < registration_specs.size(); ++index) {
		expect(
			imported->assets.registration_playchars[index] ==
				source.expected_registration[index],
			"exact owning registration playchar payload"
		);
	}
	expect(
		imported->staff.flake_masks == source.expected_flake_masks,
		"exact owning flake mask payload"
	);
}

void test_container_and_profile_rejection()
{
	{
		std::array<std::byte, 27U> short_input{};
		auto imported = th03::app::import_mainl_stage_text(short_input);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::input_too_short,
			"short input is named"
		);
	}
	{
		auto source = fixture();
		source.executable[0U] = std::byte{'N'};
		auto imported = import_fixture(source.executable);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::invalid_mz_signature,
			"MZ signature is checked before profile"
		);
	}
	{
		auto source = fixture();
		const auto &spec = registration_specs.back();
		const auto terminator = 4096U + spec.load_offset + spec.payload_bytes;
		source.executable[terminator] = std::byte{'X'};
		auto imported = import_fixture(source.executable);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::malformed_source_text &&
				imported.error().item ==
					MainlCompiledTextItem::registration_playchar &&
				imported.error().character_index == 9U &&
				imported.error().offset == terminator,
			"late registration label terminator is precisely named"
		);
	}
	{
		auto source = fixture();
		write_u16(source.executable, 8U, 0U);
		auto imported = import_fixture(source.executable);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::invalid_mz_layout,
			"MZ header size is validated"
		);
	}
	{
		auto source = fixture();
		const auto accepted = th03::app::main_executable_sha256(source.executable);
		source.executable[100U] ^= std::byte{1U};
		auto imported = th03::app::detail::import_mainl_stage_text_for_test(
			source.executable, accepted
		);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::unsupported_profile,
			"whole-file mutation is rejected"
		);
	}
}

void test_text_validation()
{
	{
		auto source = fixture();
		const auto &spec = specs[17U];
		const auto terminator = 4096U + spec.load_offset + spec.payload_bytes;
		source.executable[terminator] = std::byte{'X'};
		auto imported = import_fixture(source.executable);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::malformed_source_text &&
				imported.error().item == MainlCompiledTextItem::name &&
				imported.error().character_index == 8U &&
				imported.error().offset == terminator,
			"late missing terminator is precisely named"
		);
	}
	{
		auto source = fixture();
		const auto &spec = specs[0U];
		const auto malformed = 4096U + spec.load_offset + 1U;
		source.executable[malformed] = std::byte{0x7FU};
		auto imported = import_fixture(source.executable);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::malformed_source_text &&
				imported.error().item == MainlCompiledTextItem::title &&
				imported.error().character_index == 0U &&
				imported.error().offset == malformed,
			"invalid Shift-JIS trail is precisely named"
		);
	}
	{
		auto source = fixture();
		const auto &spec = specs[4U];
		const auto malformed = 4096U + spec.load_offset + spec.payload_bytes - 1U;
		source.executable[malformed - 1U] = std::byte{'A'};
		source.executable[malformed] = std::byte{0x81U};
		auto imported = import_fixture(source.executable);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::malformed_source_text &&
				imported.error().offset == malformed,
			"dangling Shift-JIS lead is rejected"
		);
	}
	{
		auto source = fixture();
		const auto &spec = staff_specs.back();
		const auto terminator = 4096U + spec.load_offset + spec.payload_bytes;
		source.executable[terminator] = std::byte{'X'};
		auto imported = import_fixture(source.executable);
		expect(
			!imported && imported.error().code ==
				MainlCompiledTextErrorCode::malformed_source_text &&
				imported.error().item == MainlCompiledTextItem::staff_point &&
				imported.error().offset == terminator,
			"late verdict terminator is precisely named"
		);
	}
}

std::vector<std::byte> read_file(const std::filesystem::path &path)
{
	std::ifstream input(path, std::ios::binary);
	const std::vector<char> raw{
		std::istreambuf_iterator<char>(input),
		std::istreambuf_iterator<char>()
	};
	std::vector<std::byte> result{};
	result.reserve(raw.size());
	for(const auto value : raw) {
		result.push_back(static_cast<std::byte>(
			static_cast<unsigned char>(value)
		));
	}
	return result;
}

void optional_local_gate(const char *path)
{
	auto bytes = read_file(path);
	auto imported = th03::app::import_mainl_stage_text(bytes);
	expect(imported.has_value(), "local authenticated MAINL profile imports");
	if(!imported) {
		return;
	}
	for(std::size_t index = 0U; index < imported->assets.characters.size(); ++index) {
		expect(
			imported->assets.characters[index].title.size() == 28U,
			"local title length matches source table"
		);
		expect(
			!imported->assets.characters[index].name.empty(),
			"local name is nonempty"
		);
	}
	for(const auto &rank : imported->staff.ranks) {
		expect(!rank.empty(), "local verdict rank is nonempty");
	}
	for(const auto &playchar : imported->assets.registration_playchars) {
		expect(
			playchar.size() == 12U,
			"local registration playchar is exactly six fullwidth cells"
		);
	}
	for(const auto &number : imported->staff.numbers) {
		expect(number.size() == 2U, "local verdict number is one fullwidth glyph");
	}
	expect(
		imported->staff.point.size() == 2U,
		"local verdict point is one fullwidth glyph"
	);
	expect(
		std::any_of(
			imported->staff.flake_masks.begin(),
			imported->staff.flake_masks.end(),
			[](std::uint8_t value) { return value != 0U; }
		),
		"local flake mask table is populated"
	);
}

} // namespace

int main(int argc, char **argv)
{
	test_success_and_ownership();
	test_container_and_profile_rejection();
	test_text_validation();
	if(argc == 2) {
		optional_local_gate(argv[1]);
	} else if(argc != 1) {
		std::cerr << "usage: test_app_mainl_compiled_text [MAINL.EXE]\n";
		return 2;
	}
	if(failures != 0U) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << checks << " MAINL compiled-text checks passed\n";
	return 0;
}
