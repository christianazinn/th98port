#include "th03/app/update_acquisition.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <utility>
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
	if(!input) {
		return {};
	}
	std::vector<char> chars{
		std::istreambuf_iterator<char>{input},
		std::istreambuf_iterator<char>{},
	};
	std::vector<std::byte> result(chars.size());
	for(std::size_t index = 0U; index < chars.size(); ++index) {
		result[index] = static_cast<std::byte>(
			static_cast<unsigned char>(chars[index])
		);
	}
	return result;
}

class TemporaryDirectory final {
public:
	TemporaryDirectory()
	{
		path_ = fs::temp_directory_path() /
			("th03-update-acquire-" + std::to_string(
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

struct FakeTransport final {
	std::vector<std::byte> manifest{};
	std::vector<std::byte> artifact{};
	std::vector<UpdateHttpRequest> requests{};
	std::uint16_t manifest_status = 200U;
	std::uint16_t artifact_status = 200U;
	std::optional<UpdateTransportError> manifest_error{};
	std::optional<UpdateTransportError> artifact_error{};

	[[nodiscard]] UpdateTransport transport() noexcept
	{
		return {this, &perform};
	}

	static std::expected<UpdateHttpResponse, UpdateTransportError> perform(
		void *context,
		const UpdateHttpRequest &request
	)
	{
		auto &self = *static_cast<FakeTransport *>(context);
		self.requests.push_back(request);
		if(self.requests.size() == 1U) {
			if(self.manifest_error) {
				return std::unexpected(*self.manifest_error);
			}
			return UpdateHttpResponse{self.manifest_status, self.manifest};
		}
		if(self.artifact_error) {
			return std::unexpected(*self.artifact_error);
		}
		return UpdateHttpResponse{self.artifact_status, self.artifact};
	}
};

[[nodiscard]] bool no_directory(const fs::path &path)
{
	std::error_code error;
	return !fs::exists(path, error) && !error;
}

} // namespace

int main(int argc, char **argv)
{
	if(argc != 4) {
		std::cerr << "usage: update-acquisition-tests MANIFEST SPKI ARTIFACT\n";
		return 2;
	}
	const auto manifest = read_bytes(argv[1]);
	const auto public_key = read_bytes(argv[2]);
	const auto artifact = read_bytes(argv[3]);
	const auto parsed = parse_update_manifest(manifest);
	expect(parsed.has_value() && !public_key.empty() && !artifact.empty(),
		"cross-provider acquisition fixtures read");
	if(!parsed) {
		return 1;
	}
	const UpdatePinnedKey pin{parsed->signing_key_id, public_key};
	const std::span<const UpdatePinnedKey> pins(&pin, 1U);
	TemporaryDirectory root;
	constexpr auto manifest_url = "https://updates.example.test/beta.t3upd";

	FakeTransport success{manifest, artifact};
	const auto staged = acquire_and_stage_update(
		manifest_url, pins, UpdatePlatform::windows_x64, UpdateChannel::beta,
		5000U, root.path() / "success", success.transport()
	);
	expect(staged && read_bytes(*staged) == artifact,
		"authenticated acquisition stages exact artifact bytes");
	expect(success.requests.size() == 2U &&
		success.requests[0U].url == manifest_url &&
		success.requests[0U].maximum_response_bytes == update_manifest_max_bytes &&
		success.requests[1U].url == parsed->artifact_url &&
		success.requests[1U].maximum_response_bytes == parsed->artifact_size,
		"manifest is bounded and selected before exact signed artifact request");

	auto expect_before_artifact = [&](FakeTransport fake,
		UpdatePlatform platform, UpdateChannel channel,
		std::uint64_t sequence, UpdateAcquisitionErrorCode code,
		const char *directory, const char *label) {
		const auto path = root.path() / directory;
		const auto result = acquire_and_stage_update(
			manifest_url, pins, platform, channel, sequence, path,
			fake.transport()
		);
		expect(!result && result.error().code == code &&
			fake.requests.size() == 1U && no_directory(path), label);
	};
	FakeTransport bad_signature{manifest, artifact};
	bad_signature.manifest.back() ^= std::byte{0x01U};
	expect_before_artifact(
		std::move(bad_signature), UpdatePlatform::windows_x64,
		UpdateChannel::beta, 5000U,
		UpdateAcquisitionErrorCode::manifest_authentication,
		"signature", "unauthenticated manifest cannot request an artifact"
	);
	expect_before_artifact(
		FakeTransport{manifest, artifact}, UpdatePlatform::linux_x64,
		UpdateChannel::beta, 5000U,
		UpdateAcquisitionErrorCode::wrong_platform,
		"platform", "wrong platform cannot request an artifact"
	);
	expect_before_artifact(
		FakeTransport{manifest, artifact}, UpdatePlatform::windows_x64,
		UpdateChannel::stable, 5000U,
		UpdateAcquisitionErrorCode::wrong_channel,
		"channel", "wrong channel cannot request an artifact"
	);
	expect_before_artifact(
		FakeTransport{manifest, artifact}, UpdatePlatform::windows_x64,
		UpdateChannel::beta, parsed->sequence,
		UpdateAcquisitionErrorCode::current_or_older,
		"current", "current release cannot request an artifact"
	);

	FakeTransport manifest_failure{manifest, artifact};
	manifest_failure.manifest_error = UpdateTransportError{
		UpdateTransportErrorCode::timeout
	};
	const auto manifest_failed = acquire_and_stage_update(
		manifest_url, pins, UpdatePlatform::windows_x64, UpdateChannel::beta,
		5000U, root.path() / "manifest-failure", manifest_failure.transport()
	);
	expect(!manifest_failed && manifest_failed.error().code ==
		UpdateAcquisitionErrorCode::manifest_transport &&
		manifest_failed.error().transport &&
		manifest_failed.error().transport->code ==
			UpdateTransportErrorCode::timeout,
		"manifest transport failure remains typed");
	FakeTransport manifest_status{manifest, artifact};
	manifest_status.manifest_status = 304U;
	const auto status_failed = acquire_and_stage_update(
		manifest_url, pins, UpdatePlatform::windows_x64, UpdateChannel::beta,
		5000U, root.path() / "manifest-status", manifest_status.transport()
	);
	expect(!status_failed && status_failed.error().code ==
		UpdateAcquisitionErrorCode::manifest_status &&
		status_failed.error().http_status == 304U,
		"manifest HTTP status remains typed");

	FakeTransport artifact_failure{manifest, artifact};
	artifact_failure.artifact_error = UpdateTransportError{
		UpdateTransportErrorCode::response_limit
	};
	const auto artifact_failed = acquire_and_stage_update(
		manifest_url, pins, UpdatePlatform::windows_x64, UpdateChannel::beta,
		5000U, root.path() / "artifact-failure", artifact_failure.transport()
	);
	expect(!artifact_failed && artifact_failed.error().code ==
		UpdateAcquisitionErrorCode::artifact_transport &&
		artifact_failed.error().transport &&
		artifact_failed.error().transport->code ==
			UpdateTransportErrorCode::response_limit &&
		no_directory(root.path() / "artifact-failure"),
		"artifact response limit cannot create staging state");
	FakeTransport artifact_status{manifest, artifact};
	artifact_status.artifact_status = 404U;
	const auto artifact_not_found = acquire_and_stage_update(
		manifest_url, pins, UpdatePlatform::windows_x64, UpdateChannel::beta,
		5000U, root.path() / "artifact-status", artifact_status.transport()
	);
	expect(!artifact_not_found && artifact_not_found.error().code ==
		UpdateAcquisitionErrorCode::artifact_status &&
		artifact_not_found.error().http_status == 404U &&
		no_directory(root.path() / "artifact-status"),
		"artifact HTTP status cannot create staging state");
	FakeTransport short_artifact{manifest, artifact};
	short_artifact.artifact.pop_back();
	const auto short_result = acquire_and_stage_update(
		manifest_url, pins, UpdatePlatform::windows_x64, UpdateChannel::beta,
		5000U, root.path() / "short", short_artifact.transport()
	);
	expect(!short_result && short_result.error().code ==
		UpdateAcquisitionErrorCode::artifact_size &&
		no_directory(root.path() / "short"),
		"short artifact cannot enter staging");
	FakeTransport corrupt_artifact{manifest, artifact};
	corrupt_artifact.artifact[0U] ^= std::byte{0x01U};
	const auto corrupt_result = acquire_and_stage_update(
		manifest_url, pins, UpdatePlatform::windows_x64, UpdateChannel::beta,
		5000U, root.path() / "corrupt", corrupt_artifact.transport()
	);
	expect(!corrupt_result && corrupt_result.error().code ==
		UpdateAcquisitionErrorCode::staging && corrupt_result.error().staging &&
		corrupt_result.error().staging->code ==
			UpdateStagingErrorCode::artifact_digest &&
		no_directory(root.path() / "corrupt"),
		"wrong artifact digest fails before filesystem mutation");
	const auto invalid_transport = acquire_and_stage_update(
		manifest_url, pins, UpdatePlatform::windows_x64, UpdateChannel::beta,
		5000U, root.path() / "invalid", UpdateTransport{}
	);
	expect(!invalid_transport && invalid_transport.error().code ==
		UpdateAcquisitionErrorCode::invalid_transport,
		"missing transport fails closed");

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
