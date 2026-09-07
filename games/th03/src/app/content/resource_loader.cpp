#include "th03/app/content/resources.hpp"

#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

namespace th03::app {
namespace {

[[nodiscard]] ResourceLoadError error(
	ResourceLoadErrorCode code,
	MainResourceKey key
) noexcept
{
	return {.code = code, .key = key};
}

[[nodiscard]] std::filesystem::path archive_directory(
	MainResourceArchive archive
)
{
	return (archive == MainResourceArchive::yume1) ?
		std::filesystem::path{"YUME1.DAT"} :
		std::filesystem::path{"YUME2.DAT"};
}

[[nodiscard]] bool path_is_within_root(
	const std::filesystem::path &root,
	const std::filesystem::path &candidate
) noexcept
{
	const auto relative = candidate.lexically_relative(root);
	if(relative.empty()) {
		return false;
	}
	for(const auto &component : relative) {
		if(component == "..") {
			return false;
		}
	}
	return true;
}

[[nodiscard]] constexpr bool ascii_equal_ignoring_case(
	std::string_view left,
	std::string_view right
) noexcept
{
	if(left.size() != right.size()) {
		return false;
	}
	for(std::size_t index = 0U; index < left.size(); ++index) {
		auto left_character = static_cast<unsigned char>(left[index]);
		auto right_character = static_cast<unsigned char>(right[index]);
		if(left_character >= static_cast<unsigned char>('A') &&
			left_character <= static_cast<unsigned char>('Z')) {
			left_character = static_cast<unsigned char>(
				left_character + (static_cast<unsigned char>('a') -
					static_cast<unsigned char>('A'))
			);
		}
		if(right_character >= static_cast<unsigned char>('A') &&
			right_character <= static_cast<unsigned char>('Z')) {
			right_character = static_cast<unsigned char>(
				right_character + (static_cast<unsigned char>('a') -
					static_cast<unsigned char>('A'))
			);
		}
		if(left_character != right_character) {
			return false;
		}
	}
	return true;
}

struct ExactChild {
	std::filesystem::path path{};
	ResourceLoadErrorCode error = ResourceLoadErrorCode::missing;
};

[[nodiscard]] ExactChild find_exact_child(
	const std::filesystem::path &directory,
	std::string_view expected_name
)
{
	std::error_code filesystem_error;
	std::filesystem::directory_iterator entry(directory, filesystem_error);
	if(filesystem_error) {
		return {.error = ResourceLoadErrorCode::read_failed};
	}
	bool case_mismatch = false;
	for(; entry != std::filesystem::directory_iterator{};
		entry.increment(filesystem_error)) {
		if(filesystem_error) {
			return {.error = ResourceLoadErrorCode::read_failed};
		}
		const auto actual_name = entry->path().filename().string();
		if(actual_name == expected_name) {
			return {.path = entry->path()};
		}
		case_mismatch = case_mismatch || ascii_equal_ignoring_case(
			actual_name, expected_name
		);
	}
	return {.error = case_mismatch ? ResourceLoadErrorCode::case_mismatch :
		ResourceLoadErrorCode::missing};
}

} // namespace

MainResourceLoader::MainResourceLoader(
	std::filesystem::path content_root,
	MainResourceIntegrity integrity
) :
	content_root_(std::move(content_root)),
	integrity_(integrity)
{}

bool MainResourceLoader::install_authenticated_overrides(
	std::map<std::string, ResourceBytes> overrides
)
{
	if(!cache_.empty() || !overrides_.empty()) {
		return false;
	}
	for(const auto &[name, bytes] : overrides) {
		if(!is_safe_extracted_entry_name(name) || !bytes || bytes->empty()) {
			return false;
		}
	}
	overrides_ = std::move(overrides);
	return true;
}

bool MainResourceLoader::switch_authenticated_overrides(
	std::map<std::string, ResourceBytes> overrides
)
{
	for(const auto &[name, bytes] : overrides) {
		if(!is_safe_extracted_entry_name(name) || !bytes || bytes->empty()) {
			return false;
		}
	}
	overrides_ = std::move(overrides);
	cache_.clear();
	return true;
}

std::expected<ResourceBytes, ResourceLoadError> MainResourceLoader::load(
	MainResourceKey key
)
{
	const auto spec = main_resource_spec(key);
	if(!spec) {
		return std::unexpected(spec.error());
	}
	const auto cached = cache_.find(key);
	if(cached != cache_.end()) {
		return cached->second;
	}
	const auto replacement = overrides_.find(spec->filename);
	if(replacement != overrides_.end()) {
		cache_.emplace(key, replacement->second);
		return replacement->second;
	}

	const auto archive = archive_directory(spec->archive);
	if(!is_safe_extracted_entry_name(archive.string()) ||
		!is_safe_extracted_entry_name(spec->filename)) {
		return std::unexpected(error(ResourceLoadErrorCode::path_escape, key));
	}

	std::error_code filesystem_error;
	const auto canonical_root = std::filesystem::canonical(
		content_root_, filesystem_error
	);
	if(filesystem_error ||
		!std::filesystem::is_directory(canonical_root, filesystem_error) ||
		filesystem_error) {
		return std::unexpected(error(ResourceLoadErrorCode::invalid_content_root, key));
	}

	const auto selected_archive = find_exact_child(canonical_root, archive.string());
	if(selected_archive.path.empty()) {
		return std::unexpected(error(selected_archive.error, key));
	}
	const auto canonical_archive = std::filesystem::canonical(
		selected_archive.path, filesystem_error
	);
	if(filesystem_error) {
		return std::unexpected(error(ResourceLoadErrorCode::read_failed, key));
	}
	if(!path_is_within_root(canonical_root, canonical_archive)) {
		return std::unexpected(error(ResourceLoadErrorCode::path_escape, key));
	}
	if(!std::filesystem::is_directory(canonical_archive, filesystem_error) ||
		filesystem_error) {
		return std::unexpected(error(ResourceLoadErrorCode::not_directory, key));
	}
	const auto selected_file = find_exact_child(canonical_archive, spec->filename);
	if(selected_file.path.empty()) {
		return std::unexpected(error(selected_file.error, key));
	}
	const auto canonical_selected = std::filesystem::canonical(
		selected_file.path, filesystem_error
	);
	if(filesystem_error) {
		return std::unexpected(error(ResourceLoadErrorCode::read_failed, key));
	}
	if(!path_is_within_root(canonical_root, canonical_selected)) {
		return std::unexpected(error(ResourceLoadErrorCode::path_escape, key));
	}
	if(!std::filesystem::is_regular_file(canonical_selected, filesystem_error) ||
		filesystem_error) {
		return std::unexpected(error(ResourceLoadErrorCode::not_regular_file, key));
	}
	const auto actual_size = std::filesystem::file_size(
		canonical_selected, filesystem_error
	);
	if(filesystem_error) {
		return std::unexpected(error(ResourceLoadErrorCode::read_failed, key));
	}
	if(actual_size != spec->expected_bytes) {
		return std::unexpected(error(ResourceLoadErrorCode::size_mismatch, key));
	}
	if(spec->expected_bytes >
		static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) {
		return std::unexpected(error(ResourceLoadErrorCode::read_failed, key));
	}

	std::ifstream input(canonical_selected, std::ios::binary);
	if(!input) {
		return std::unexpected(error(ResourceLoadErrorCode::read_failed, key));
	}
	std::vector<std::byte> bytes(spec->expected_bytes);
	input.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	);
	if(!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
		return std::unexpected(error(ResourceLoadErrorCode::read_failed, key));
	}
	if(integrity_ == MainResourceIntegrity::size_and_fnv1a64 &&
		main_resource_fnv1a64(bytes) != spec->expected_fnv1a64) {
		return std::unexpected(error(ResourceLoadErrorCode::checksum_mismatch, key));
	}
	const auto resource = std::make_shared<const std::vector<std::byte>>(
		std::move(bytes)
	);
	cache_.emplace(key, resource);
	return resource;
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_op_title_menu()
{
	return load_batch(op_title_menu_resource_keys());
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_op_character_selection()
{
	return load_batch(op_character_selection_resource_keys());
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_op_music_room()
{
	return load_batch(op_music_room_resource_keys());
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_direct_vs(
	MainPlayerSelection player_one,
	MainPlayerSelection player_two
)
{
	const auto keys = direct_vs_resource_keys(player_one, player_two);
	if(!keys) {
		return std::unexpected(keys.error());
	}
	return load_batch(*keys);
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_main_static()
{
	return load_batch(main_static_resource_keys());
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_main_private(
	MainPlayerSelection player_one,
	MainPlayerSelection player_two
)
{
	const auto keys = main_private_resource_keys(player_one, player_two);
	if(!keys) {
		return std::unexpected(keys.error());
	}
	return load_batch(*keys);
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_mainl_stage(MainlStageResourceRequest request)
{
	const auto keys = mainl_stage_resource_keys(request);
	if(!keys) {
		return std::unexpected(keys.error());
	}
	return load_batch(*keys);
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_mainl_win(MainPlayerSelection winner)
{
	const auto keys = mainl_win_resource_keys(winner);
	if(!keys) {
		return std::unexpected(keys.error());
	}
	return load_batch(*keys);
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_mainl_continue(std::uint8_t remaining_credits)
{
	const auto keys = mainl_continue_resource_keys(remaining_credits);
	if(!keys) {
		return std::unexpected(keys.error());
	}
	return load_batch(*keys);
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_mainl_cutscene(MainlCutsceneResourceRequest request)
{
	const auto keys = mainl_cutscene_resource_keys(request);
	if(!keys) {
		return std::unexpected(keys.error());
	}
	return load_batch(*keys);
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_mainl_staff()
{
	return load_batch(mainl_staff_resource_keys());
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_mainl_registration(
	MainlRegistrationResourceRequest request
)
{
	const auto keys = mainl_registration_resource_keys(request);
	if(!keys) {
		return std::unexpected(keys.error());
	}
	return load_batch(*keys);
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_mainl_gameover()
{
	return load_batch(mainl_gameover_resource_keys());
}

std::expected<std::vector<LoadedMainResource>, ResourceLoadError>
MainResourceLoader::load_batch(std::span<const MainResourceKey> keys)
{
	MainResourceLoader candidate(*this);
	std::vector<LoadedMainResource> result;
	result.reserve(keys.size());
	for(const auto key : keys) {
		const auto spec = main_resource_spec(key);
		if(!spec) {
			return std::unexpected(spec.error());
		}
		const auto resource = candidate.load(key);
		if(!resource) {
			return std::unexpected(resource.error());
		}
		result.push_back({
			.spec = *spec,
			.bytes = *resource,
			.authenticated_override = overrides_.contains(spec->filename),
		});
	}
	cache_ = std::move(candidate.cache_);
	return result;
}

std::size_t MainResourceLoader::cached_resource_count() const noexcept
{
	return cache_.size();
}

} // namespace th03::app
