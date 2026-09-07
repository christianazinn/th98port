#include "th03/app/update/update_generation_cleanup.hpp"

#include "th03/app/update/update_generation.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <ranges>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#if !defined(_WIN32)
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace th03::app {
namespace {

namespace fs = std::filesystem;

struct Candidate final {
	fs::path path{};
	UpdateGenerationIdentity identity{};
};

[[nodiscard]] UpdateGenerationCleanupError cleanup_error(
	UpdateGenerationCleanupErrorCode code,
	fs::path path = {},
	std::error_code system = {}
)
{
	return {.code = code, .path = std::move(path), .system = system};
}

[[nodiscard]] std::optional<UpdateGenerationIdentity> read_marker(
	const fs::path &directory
)
{
	std::error_code system;
	const auto directory_status = fs::symlink_status(directory, system);
	if(system || !fs::is_directory(directory_status)) {
		return std::nullopt;
	}
#if !defined(_WIN32)
	if((directory_status.permissions() & fs::perms::mask) !=
		(fs::perms::owner_read | fs::perms::owner_write |
			fs::perms::owner_exec)) {
		return std::nullopt;
	}
#endif
	const auto path = directory / fs::path(update_generation_marker_filename);
	const auto status = fs::symlink_status(path, system);
	if(system || !fs::is_regular_file(status) ||
		fs::file_size(path, system) != update_generation_marker_bytes || system) {
		return std::nullopt;
	}
#if !defined(_WIN32)
	if((status.permissions() & fs::perms::mask) !=
		(fs::perms::owner_read | fs::perms::owner_write)) {
		return std::nullopt;
	}
#endif
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::nullopt;
	}
	std::array<std::byte, update_generation_marker_bytes> bytes{};
	input.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	if(!input || input.peek() != std::ifstream::traits_type::eof()) {
		return std::nullopt;
	}
	const auto decoded = decode_update_generation_marker(bytes);
	if(!decoded || directory.filename().string() !=
		update_generation_directory_name(*decoded)) {
		return std::nullopt;
	}
	return *decoded;
}

[[nodiscard]] bool referenced(
	const UpdateActivationState &state,
	const UpdateGenerationIdentity &identity
)
{
	const std::array<const std::optional<UpdateGenerationIdentity> *, 4U> keep{
		&state.active, &state.pending, &state.previous, &state.last_failed,
	};
	return std::ranges::any_of(keep, [&](const auto *candidate) {
		return *candidate && **candidate == identity;
	});
}

[[nodiscard]] bool newer(const Candidate &left, const Candidate &right)
{
	if(left.identity.sequence != right.identity.sequence) {
		return left.identity.sequence > right.identity.sequence;
	}
	return std::ranges::lexicographical_compare(
		right.identity.artifact_sha256, left.identity.artifact_sha256
	);
}

[[nodiscard]] bool sync_directory(const fs::path &path) noexcept
{
#if defined(_WIN32)
	(void)path;
	return true;
#else
	const auto handle = ::open(path.c_str(), O_RDONLY | O_DIRECTORY);
	if(handle < 0) {
		return false;
	}
	const bool result = ::fsync(handle) == 0;
	(void)::close(handle);
	return result;
#endif
}

[[nodiscard]] std::error_code last_system_error() noexcept
{
#if defined(_WIN32)
	return {};
#else
	return {errno, std::generic_category()};
#endif
}

} // namespace

std::expected<UpdateGenerationCleanupResult, UpdateGenerationCleanupError>
cleanup_update_generations(
	const UpdateActivationState &state,
	const fs::path &generation_root,
	std::size_t retain_unreferenced
)
{
	if(!valid_update_activation_state(state)) {
		return std::unexpected(cleanup_error(
			UpdateGenerationCleanupErrorCode::invalid_state
		));
	}
	if(generation_root.empty()) {
		return std::unexpected(cleanup_error(
			UpdateGenerationCleanupErrorCode::invalid_root
		));
	}
	std::error_code system;
	const auto root_status = fs::symlink_status(generation_root, system);
	if(system == std::errc::no_such_file_or_directory) {
		return UpdateGenerationCleanupResult{};
	}
	if(system) {
		return std::unexpected(cleanup_error(
			UpdateGenerationCleanupErrorCode::status, generation_root, system
		));
	}
	if(!fs::is_directory(root_status)) {
		return std::unexpected(cleanup_error(
			UpdateGenerationCleanupErrorCode::invalid_root, generation_root
		));
	}

	UpdateGenerationCleanupResult result{};
	std::vector<Candidate> unreferenced;
	fs::directory_iterator iterator(generation_root, system), end;
	if(system) {
		return std::unexpected(cleanup_error(
			UpdateGenerationCleanupErrorCode::enumerate,
			generation_root, system
		));
	}
	for(; iterator != end; iterator.increment(system)) {
		if(system) {
			return std::unexpected(cleanup_error(
				UpdateGenerationCleanupErrorCode::enumerate,
				generation_root, system
			));
		}
		++result.inspected;
		const auto marker = read_marker(iterator->path());
		if(!marker) {
			++result.ignored;
			continue;
		}
		if(referenced(state, *marker)) {
			++result.retained_referenced;
			continue;
		}
		unreferenced.push_back({iterator->path(), *marker});
	}
	if(system) {
		return std::unexpected(cleanup_error(
			UpdateGenerationCleanupErrorCode::enumerate,
			generation_root, system
		));
	}
	std::ranges::sort(unreferenced, newer);
	result.retained_history = std::min(
		retain_unreferenced, unreferenced.size()
	);
	for(std::size_t index = result.retained_history;
		index < unreferenced.size(); ++index) {
		const auto &candidate = unreferenced[index];
		const auto marker = read_marker(candidate.path);
		if(!marker || *marker != candidate.identity) {
			++result.ignored;
			continue;
		}
		const auto removed = fs::remove_all(candidate.path, system);
		if(system || removed == 0U) {
			return std::unexpected(cleanup_error(
				UpdateGenerationCleanupErrorCode::remove,
				candidate.path, system
			));
		}
		++result.removed;
	}
	if(result.removed != 0U && !sync_directory(generation_root)) {
		return std::unexpected(cleanup_error(
			UpdateGenerationCleanupErrorCode::flush,
			generation_root, last_system_error()
		));
	}
	return result;
}

} // namespace th03::app
