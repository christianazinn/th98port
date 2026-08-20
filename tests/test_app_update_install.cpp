#include "th03/app/update_generation.hpp"
#include "th03/app/update_install.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
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
		path_ = fs::temp_directory_path() / (
			"th03-update-install-" + std::to_string(
				std::chrono::steady_clock::now().time_since_epoch().count()
			)
		);
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

struct FakeTransport final {
	std::vector<std::byte> manifest{};
	std::vector<std::byte> artifact{};
	std::vector<UpdateHttpRequest> requests{};

	[[nodiscard]] UpdateTransport seam() noexcept
	{
		return {.context = this, .perform = perform};
	}

	static std::expected<UpdateHttpResponse, UpdateTransportError> perform(
		void *context,
		const UpdateHttpRequest &request
	)
	{
		auto &self = *static_cast<FakeTransport *>(context);
		self.requests.push_back(request);
		return UpdateHttpResponse{
			.status = 200U,
			.body = self.requests.size() == 1U ?
				self.manifest : self.artifact,
		};
	}
};

} // namespace

int main(int argc, char **argv)
{
	if(argc != 4) {
		std::cerr << "usage: update-install-tests MANIFEST SPKI PACKAGE\n";
		return 2;
	}
	const auto manifest = read_bytes(argv[1]);
	const auto public_key = read_bytes(argv[2]);
	const auto package = read_bytes(argv[3]);
	const auto parsed = parse_update_manifest(manifest);
	expect(parsed && !public_key.empty() && !package.empty(),
		"signed install fixtures parse");
	if(!parsed) {
		return 1;
	}
	const UpdatePinnedKey pin{parsed->signing_key_id, public_key};
	const std::span<const UpdatePinnedKey> pins(&pin, 1U);
	constexpr auto manifest_url = "https://updates.example.test/beta.t3upd";
	TemporaryDirectory temporary;
	const auto state_path = temporary.path() / "update" / "PODD.UPDATE";
	const auto generation_root = temporary.path() / "update" / "generations";

	FakeTransport transport{manifest, package};
	const auto installed = install_update_for_next_launch(
		manifest_url, pins, UpdatePlatform::windows_x64, UpdateChannel::beta,
		parsed->sequence - 1U, state_path, generation_root, transport.seam()
	);
	const UpdateGenerationIdentity expected_identity{
		.sequence = parsed->sequence,
		.artifact_sha256 = parsed->artifact_sha256,
	};
	const auto loaded = load_update_activation_state(state_path);
	expect(installed && installed->status == UpdateInstallStatus::nominated &&
		installed->generation == expected_identity && loaded &&
		loaded->state == installed->state &&
		loaded->state.phase == UpdateActivationPhase::pending &&
		loaded->state.pending == expected_identity,
		"authenticated package is durably nominated for the next launch");
	expect(installed && fs::is_directory(installed->generation_path) &&
		fs::file_size(installed->generation_path /
			fs::path(update_generation_marker_filename)) ==
			update_generation_marker_bytes,
		"install publishes the exact marked immutable generation");
	expect(transport.requests.size() == 2U &&
		transport.requests[0U].url == manifest_url &&
		transport.requests[1U].url == parsed->artifact_url &&
		transport.requests[1U].maximum_response_bytes == parsed->artifact_size,
		"install authenticates bounded manifest before exact artifact request");

	FakeTransport repeated{manifest, package};
	const auto current = install_update_for_next_launch(
		manifest_url, pins, UpdatePlatform::windows_x64, UpdateChannel::beta,
		parsed->sequence - 1U, state_path, generation_root, repeated.seam()
	);
	expect(!current && current.error().code ==
		UpdateInstallErrorCode::acquisition && current.error().acquisition &&
		current.error().acquisition->code ==
		UpdateAcquisitionErrorCode::current_or_older &&
		repeated.requests.size() == 1U,
		"already nominated sequence never downloads the artifact again");

	const auto corrupt_state = temporary.path() / "corrupt" / "PODD.UPDATE";
	const auto corrupt_root = temporary.path() / "corrupt" / "generations";
	FakeTransport corrupted{manifest, package};
	corrupted.artifact[0U] ^= std::byte{1U};
	const auto wrong_digest = install_update_for_next_launch(
		manifest_url, pins, UpdatePlatform::windows_x64, UpdateChannel::beta,
		parsed->sequence - 1U, corrupt_state, corrupt_root,
		corrupted.seam()
	);
	const auto corrupt_loaded = load_update_activation_state(corrupt_state);
	expect(!wrong_digest && wrong_digest.error().code ==
		UpdateInstallErrorCode::materialization &&
		wrong_digest.error().materialization &&
		wrong_digest.error().materialization->code ==
		UpdateMaterializationErrorCode::artifact_digest &&
		corrupt_loaded && !corrupt_loaded->file_present &&
		!fs::exists(corrupt_root),
		"wrong artifact digest cannot create a generation or nomination");

	fs::create_directories(temporary.path() / "bad-root-parent");
	const auto root_file = temporary.path() / "bad-root-parent" / "file";
	std::ofstream(root_file) << "x";
	FakeTransport bad_root_transport{manifest, package};
	const auto bad_root = install_update_for_next_launch(
		manifest_url, pins, UpdatePlatform::windows_x64, UpdateChannel::beta,
		parsed->sequence - 1U,
		temporary.path() / "bad-root-state" / "PODD.UPDATE", root_file,
		bad_root_transport.seam()
	);
	expect(!bad_root && bad_root.error().code ==
		UpdateInstallErrorCode::materialization &&
		bad_root.error().materialization &&
		bad_root.error().materialization->code ==
		UpdateMaterializationErrorCode::invalid_directory,
		"invalid generation root fails before activation mutation");

	const auto floor_state = temporary.path() / "floor" / "PODD.UPDATE";
	const auto floor_root = temporary.path() / "floor" / "generations";
	FakeTransport floor_transport{manifest, package};
	const auto installed_floor = install_update_for_next_launch(
		manifest_url, pins, UpdatePlatform::windows_x64, UpdateChannel::beta,
		parsed->sequence, floor_state, floor_root, floor_transport.seam()
	);
	expect(!installed_floor && installed_floor.error().code ==
		UpdateInstallErrorCode::acquisition &&
		installed_floor.error().acquisition &&
		installed_floor.error().acquisition->code ==
		UpdateAcquisitionErrorCode::current_or_older &&
		floor_transport.requests.size() == 1U &&
		!fs::exists(floor_state) && !fs::exists(floor_root),
		"installed release sequence rejects a signed downgrade without state");

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
