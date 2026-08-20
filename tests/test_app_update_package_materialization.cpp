#include "th03/app/update_package_materialization.hpp"
#include "th03/app/update_generation.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

namespace {

using namespace th03::app;
namespace fs = std::filesystem;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *label)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << label << '\n';
	}
}

[[nodiscard]] std::vector<std::byte> read_bytes(const fs::path &path)
{
	std::ifstream input(path, std::ios::binary);
	const std::vector<char> characters{
		std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()
	};
	std::vector<std::byte> result(characters.size());
	for(std::size_t index = 0U; index < characters.size(); ++index) {
		result[index] = static_cast<std::byte>(characters[index]);
	}
	return result;
}

class TemporaryDirectory final {
public:
	TemporaryDirectory()
	{
		path_ = fs::temp_directory_path() / ("th03-update-generation-" +
			std::to_string(
				std::chrono::steady_clock::now().time_since_epoch().count()
			));
	}
	~TemporaryDirectory()
	{
		std::error_code ignored;
		fs::remove_all(path_, ignored);
	}
	[[nodiscard]] const fs::path &path() const noexcept { return path_; }

private:
	fs::path path_{};
};

[[nodiscard]] bool no_temporary(const fs::path &root)
{
	std::error_code system;
	if(!fs::exists(root, system)) {
		return !system;
	}
	for(const auto &entry : fs::directory_iterator(root)) {
		if(entry.path().filename().string().starts_with(".PODD-GEN-TMP-")) {
			return false;
		}
	}
	return true;
}

} // namespace

int main(int argc, char **argv)
{
	if(argc != 6) {
		std::cerr << "usage: update-package-materialization-tests "
			"MANIFEST SPKI PACKAGE BAD-MANIFEST BAD-PACKAGE\n";
		return 2;
	}
	const auto manifest = read_bytes(argv[1]);
	const auto public_key = read_bytes(argv[2]);
	const auto package_bytes = read_bytes(argv[3]);
	const auto bad_manifest = read_bytes(argv[4]);
	const auto bad_package = read_bytes(argv[5]);
	const auto parsed_manifest = parse_update_manifest(manifest);
	const auto parsed_package = parse_update_package(package_bytes);
	expect(parsed_manifest && parsed_package && !public_key.empty(),
		"independently authored signed package fixtures parse");
	if(!parsed_manifest || !parsed_package) {
		return 1;
	}
	const UpdatePinnedKey pin{
		parsed_manifest->signing_key_id, public_key
	};
	const std::span<const UpdatePinnedKey> pins(&pin, 1U);
	auto materialize = [&](const fs::path &root) {
		return materialize_authenticated_update_package(
			manifest, pins, UpdatePlatform::windows_x64, UpdateChannel::beta,
			5000U, package_bytes, root
		);
	};

	TemporaryDirectory temporary;
	const auto root = temporary.path() / "success";
	const auto generation = materialize(root);
	expect(generation && generation->parent_path() == root &&
		generation->filename().string().starts_with("PODD-GEN-5001-"),
		"authenticated exact package publishes one inert local generation");
	if(generation) {
		bool exact = true;
		for(const auto &entry : parsed_package->entries) {
			const auto expected = std::span<const std::byte>(package_bytes).subspan(
				static_cast<std::size_t>(entry.payload_offset),
				static_cast<std::size_t>(entry.size)
			);
			exact = exact && read_bytes(*generation / fs::path(entry.path)) ==
				std::vector<std::byte>(expected.begin(), expected.end());
#if !defined(_WIN32)
			std::error_code system;
			const auto permissions = fs::status(
				*generation / fs::path(entry.path), system
			).permissions();
			exact = exact && !system &&
				((permissions & fs::perms::owner_exec) != fs::perms::none) ==
					entry.executable;
#endif
		}
		const auto marker_bytes = read_bytes(
			*generation / fs::path(update_generation_marker_filename)
		);
		const UpdateGenerationIdentity expected_identity{
			.sequence = parsed_manifest->sequence,
			.artifact_sha256 = parsed_manifest->artifact_sha256,
		};
		exact = exact &&
			decode_update_generation_marker(marker_bytes) == expected_identity;
		expect(exact, "every member and executable bit matches the package");
		expect(marker_bytes.size() == update_generation_marker_bytes,
			"published generation carries its exact atomic ownership marker");
	}
	expect(no_temporary(root), "successful publication leaves no temporary");
	const auto repeated = materialize(root);
	expect(repeated && generation && *repeated == *generation,
		"identical generation materialization is idempotent");

	TemporaryDirectory authentication_failure;
	auto changed_manifest = manifest;
	changed_manifest.back() ^= std::byte{1U};
	const auto unauthenticated = materialize_authenticated_update_package(
		changed_manifest, pins, UpdatePlatform::windows_x64,
		UpdateChannel::beta, 5000U, package_bytes,
		authentication_failure.path() / "never"
	);
	expect(!unauthenticated && unauthenticated.error().code ==
		UpdateMaterializationErrorCode::authentication &&
		!fs::exists(authentication_failure.path() / "never"),
		"unauthenticated input mutates no filesystem state");
	const auto bad_parsed_manifest = parse_update_manifest(bad_manifest);
	expect(bad_parsed_manifest.has_value(), "bad-member signed manifest parses");
	if(bad_parsed_manifest) {
		const UpdatePinnedKey bad_pin{
			bad_parsed_manifest->signing_key_id, public_key
		};
		TemporaryDirectory member_failure;
		const auto invalid_member = materialize_authenticated_update_package(
			bad_manifest, std::span(&bad_pin, 1U), UpdatePlatform::windows_x64,
			UpdateChannel::beta, 5000U, bad_package,
			member_failure.path() / "never"
		);
		expect(!invalid_member && invalid_member.error().code ==
			UpdateMaterializationErrorCode::member_digest &&
			!fs::exists(member_failure.path() / "never"),
			"signed package with a false member digest fails before mutation");
	}

	TemporaryDirectory selection_failure;
	const auto wrong_platform = materialize_authenticated_update_package(
		manifest, pins, UpdatePlatform::linux_x64, UpdateChannel::beta,
		5000U, package_bytes, selection_failure.path() / "platform"
	);
	const auto current = materialize_authenticated_update_package(
		manifest, pins, UpdatePlatform::windows_x64, UpdateChannel::beta,
		5001U, package_bytes, selection_failure.path() / "current"
	);
	expect(!wrong_platform && wrong_platform.error().code ==
		UpdateMaterializationErrorCode::wrong_platform &&
		!current && current.error().code ==
			UpdateMaterializationErrorCode::current_or_older &&
		!fs::exists(selection_failure.path() / "platform") &&
		!fs::exists(selection_failure.path() / "current"),
		"manifest selection failures occur before filesystem mutation");
	auto changed_package = package_bytes;
	changed_package.back() ^= std::byte{1U};
	TemporaryDirectory digest_failure;
	const auto wrong_digest = materialize_authenticated_update_package(
		manifest, pins, UpdatePlatform::windows_x64, UpdateChannel::beta,
		5000U, changed_package, digest_failure.path() / "never"
	);
	expect(!wrong_digest && wrong_digest.error().code ==
		UpdateMaterializationErrorCode::artifact_digest &&
		!fs::exists(digest_failure.path() / "never"),
		"wrong complete artifact digest mutates no filesystem state");

	if(generation) {
		const auto first = *generation /
			fs::path(parsed_package->entries.front().path);
		{
			std::ofstream output(first, std::ios::binary | std::ios::trunc);
			output << "collision";
		}
		const auto conflict = materialize(root);
		expect(!conflict && conflict.error().code ==
			UpdateMaterializationErrorCode::target_conflict &&
			read_bytes(first).size() == 9U,
			"different existing generation is preserved and rejected");
	}

	TemporaryDirectory extra_failure;
	const auto extra_generation = materialize(extra_failure.path() / "root");
	if(extra_generation) {
		std::ofstream(*extra_generation / "unexpected.txt") << "unexpected";
	}
	const auto extra_conflict = materialize(extra_failure.path() / "root");
	expect(extra_generation && !extra_conflict &&
		extra_conflict.error().code ==
			UpdateMaterializationErrorCode::target_conflict,
		"extra generation members fail exact idempotent validation");

	TemporaryDirectory bad_root;
	const auto root_file = bad_root.path() / "not-a-directory";
	fs::create_directories(bad_root.path());
	std::ofstream(root_file) << "x";
	const auto invalid_root = materialize(root_file);
	expect(!invalid_root && invalid_root.error().code ==
		UpdateMaterializationErrorCode::invalid_directory,
		"regular-file generation root is rejected");
	expect(no_temporary(temporary.path()) && no_temporary(extra_failure.path()),
		"failure paths leave no transaction temporaries");

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
