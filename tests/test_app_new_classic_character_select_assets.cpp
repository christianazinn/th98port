#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

#include "th03/app/new_classic_character_select_assets.hpp"

namespace {

int checks = 0;
int failures = 0;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

std::vector<std::byte> read(const std::filesystem::path &path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	const auto size = input.tellg();
	std::vector<std::byte> result(
		size > 0 ? static_cast<std::size_t>(size) : 0U
	);
	input.seekg(0);
	input.read(
		reinterpret_cast<char *>(result.data()),
		static_cast<std::streamsize>(result.size())
	);
	return result;
}

void write(const std::filesystem::path &path, std::span<const std::byte> bytes)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output.write(
		reinterpret_cast<const char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
}

void test_tracked_package()
{
	const auto source = std::filesystem::path(TH03_TEST_SOURCE_DIR) /
		"assets" / "new-classic" / "menu-backgrounds" /
		"T3CSP001-ES-TEST.BIN";
	auto loaded = th03::app::load_new_classic_character_select_assets(source);
	expect(loaded && loaded->locale() == "es-TEST",
		"runtime accepts the tracked test-locale package");
	if(!loaded) { return; }
	expect(
		loaded->names().header.pixel_width == 64U &&
		loaded->names().header.pixel_height == 16U &&
		loaded->names().glyphs.size() == 18U,
		"package exposes the exact source two-cel name geometry"
	);
	expect(
		loaded->stat_labels().pixel_width == 64U &&
		loaded->stat_labels().pixel_height == 48U &&
		loaded->stat_labels().alpha.has_value(),
		"package exposes one alpha-protected three-row stat overlay"
	);
	expect(
		!loaded->matches_source({}, {}, {}),
		"generated assets reject a source profile with the wrong identity"
	);

	const auto temporary = std::filesystem::temp_directory_path() /
		"th03-character-select-assets-test.bin";
	auto bytes = read(source);
	bytes.back() ^= std::byte{1U};
	write(temporary, bytes);
	auto rejected = th03::app::load_new_classic_character_select_assets(
		temporary
	);
	expect(
		!rejected && rejected.error().code ==
			th03::app::NewClassicCharacterSelectAssetsErrorCode::body_digest_mismatch,
		"runtime authenticates the complete native-asset body"
	);
	bytes = read(source);
	bytes[184U] = std::byte{1U};
	write(temporary, bytes);
	rejected = th03::app::load_new_classic_character_select_assets(temporary);
	expect(
		!rejected && rejected.error().code ==
			th03::app::NewClassicCharacterSelectAssetsErrorCode::header_shape,
		"reserved package bytes must remain zero"
	);
	std::error_code ignored{};
	std::filesystem::remove(temporary, ignored);
}

void test_authenticated_source_match(
	const std::filesystem::path &names,
	const std::filesystem::path &stats,
	const std::filesystem::path &palette
)
{
	const auto package = std::filesystem::path(TH03_TEST_SOURCE_DIR) /
		"assets" / "new-classic" / "menu-backgrounds" /
		"T3CSP001-ES-TEST.BIN";
	auto loaded = th03::app::load_new_classic_character_select_assets(package);
	const auto name_bytes = read(names);
	const auto stat_bytes = read(stats);
	const auto palette_bytes = read(palette);
	expect(
		loaded && loaded->matches_source(
			name_bytes, stat_bytes, palette_bytes
		),
		"package binds the authenticated English Character Select source profile"
	);
}

} // namespace

int main(int argc, char **argv)
{
	test_tracked_package();
	if(argc == 4) {
		test_authenticated_source_match(argv[1], argv[2], argv[3]);
	} else {
		expect(argc == 1, "optional source gate accepts three asset paths");
	}
	std::cout << "new classic Character Select asset checks: " << checks << '\n';
	return failures == 0 ? 0 : 1;
}
