#include "th03/app/update/update_generation.hpp"
#include "th03/app/update/update_generation_cleanup.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

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

class TemporaryDirectory final {
public:
	TemporaryDirectory()
	{
		path_ = fs::temp_directory_path() / (
			"th03-update-cleanup-" + std::to_string(
				std::chrono::steady_clock::now().time_since_epoch().count()
			)
		);
		fs::create_directories(path_);
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

[[nodiscard]] UpdateGenerationIdentity identity(std::uint64_t sequence)
{
	UpdateGenerationIdentity result{.sequence = sequence};
	for(std::size_t index = 0U; index < result.artifact_sha256.size(); ++index) {
		result.artifact_sha256[index] = static_cast<std::uint8_t>(
			sequence + index + 1U
		);
	}
	return result;
}

[[nodiscard]] fs::path write_generation(
	const fs::path &root,
	const UpdateGenerationIdentity &directory_identity,
	const UpdateGenerationIdentity &marker_identity
)
{
	const auto path = root /
		update_generation_directory_name(directory_identity);
	fs::create_directories(path);
	const auto encoded = encode_update_generation_marker(marker_identity);
	if(!encoded) {
		return {};
	}
	std::ofstream output(
		path / fs::path(update_generation_marker_filename),
		std::ios::binary
	);
	output.write(
		reinterpret_cast<const char *>(encoded->data()),
		static_cast<std::streamsize>(encoded->size())
	);
	output.close();
#if !defined(_WIN32)
	fs::permissions(
		path,
		fs::perms::owner_read | fs::perms::owner_write |
			fs::perms::owner_exec,
		fs::perm_options::replace
	);
	fs::permissions(
		path / fs::path(update_generation_marker_filename),
		fs::perms::owner_read | fs::perms::owner_write,
		fs::perm_options::replace
	);
#endif
	return path;
}

} // namespace

int main()
{
	const auto valid_identity = identity(9U);
	const auto encoded = encode_update_generation_marker(valid_identity);
	expect(encoded.has_value(), "valid generation marker encodes");
	if(encoded) {
		expect(decode_update_generation_marker(*encoded) == valid_identity,
			"generation marker round-trips exact full identity");
		auto changed = *encoded;
		changed[12U] = std::byte{1U};
		const auto reserved = decode_update_generation_marker(changed);
		expect(!reserved && reserved.error().code ==
			UpdateGenerationMarkerErrorCode::reserved,
			"nonzero marker reservation is rejected");
	}
	expect(!encode_update_generation_marker({}),
		"invalid generation identity cannot become an ownership marker");

	TemporaryDirectory temporary;
	const auto root = temporary.path() / "generations";
	fs::create_directories(root);
	std::array<fs::path, 9U> generation{};
	for(std::uint64_t sequence = 1U; sequence <= 8U; ++sequence) {
		generation[sequence] = write_generation(
			root, identity(sequence), identity(sequence)
		);
	}
	const auto mismatched = write_generation(root, identity(9U), identity(10U));
	std::ofstream(root / "notes.txt") << "not updater-owned";
	fs::create_directories(root / "PODD-GEN-malformed");

	UpdateActivationState state{
		.revision = 4U,
		.highest_sequence = 8U,
		.failed_trials = 1U,
		.phase = UpdateActivationPhase::stable,
		.active = identity(5U),
		.previous = identity(4U),
		.last_failed = identity(8U),
	};
	expect(valid_update_activation_state(state),
		"cleanup fixture activation state is valid");
	const auto cleanup = cleanup_update_generations(state, root, 2U);
	expect(cleanup && cleanup->retained_referenced == 3U &&
		cleanup->retained_history == 2U && cleanup->removed == 3U,
		"cleanup retains references and two newest unreferenced generations");
	expect(fs::exists(generation[4U]) && fs::exists(generation[5U]) &&
		fs::exists(generation[8U]),
		"active previous and last-failed generations are preserved");
	expect(fs::exists(generation[6U]) && fs::exists(generation[7U]) &&
		!fs::exists(generation[1U]) && !fs::exists(generation[2U]) &&
		!fs::exists(generation[3U]),
		"only newest unreferenced history survives");
	expect(fs::exists(mismatched) && fs::exists(root / "notes.txt") &&
		fs::exists(root / "PODD-GEN-malformed"),
		"mismatched malformed and unrelated entries are never deletion targets");

	auto invalid_state = state;
	invalid_state.highest_sequence = 7U;
	const auto invalid = cleanup_update_generations(invalid_state, root, 0U);
	expect(!invalid && invalid.error().code ==
		UpdateGenerationCleanupErrorCode::invalid_state &&
		fs::exists(generation[6U]),
		"invalid activation state fails before deleting generations");

	const auto absent = cleanup_update_generations(
		UpdateActivationState{}, temporary.path() / "absent", 0U
	);
	expect(absent && absent->removed == 0U,
		"absent generation root is an inert success");
	std::ofstream(temporary.path() / "root-file") << "x";
	const auto wrong_root = cleanup_update_generations(
		UpdateActivationState{}, temporary.path() / "root-file", 0U
	);
	expect(!wrong_root && wrong_root.error().code ==
		UpdateGenerationCleanupErrorCode::invalid_root,
		"non-directory generation root is rejected");

	const auto tenth = identity(10U);
	const auto tenth_path = write_generation(root, tenth, tenth);
	const auto pending_state = nominate_update_generation(state, tenth);
	expect(pending_state.has_value(),
		"newer candidate produces a valid pending cleanup fixture");
	if(pending_state) {
		const auto pending_cleanup = cleanup_update_generations(
			*pending_state, root, 0U
		);
		expect(pending_cleanup && fs::exists(tenth_path) &&
			fs::exists(generation[4U]) && fs::exists(generation[5U]),
			"pending active and previous identities all survive zero-history cleanup");
	}

	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
