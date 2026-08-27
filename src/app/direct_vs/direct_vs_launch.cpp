#include "th03/app/direct_vs/direct_vs_launch.hpp"

#include "direct_vs/direct_vs_launch_internal.hpp"

#include <fstream>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "th03/app/content/authentic_text.hpp"

namespace th03::app {
namespace {

struct ExternalFile {
	std::vector<std::byte> bytes{};
};

DirectVsLaunchError launch_error(
	DirectVsLaunchErrorCode code,
	DirectVsFile file = DirectVsFile::none
)
{
	return DirectVsLaunchError{.code = code, .file = file};
}

std::expected<ExternalFile, DirectVsLaunchError> read_external_file(
	const std::filesystem::path &path,
	DirectVsFile file,
	std::optional<std::uintmax_t> exact_size = std::nullopt
)
{
	std::error_code error;
	const auto status = std::filesystem::status(path, error);
	if(error) {
		return std::unexpected(launch_error(
			DirectVsLaunchErrorCode::file_missing, file
		));
	}
	if(!std::filesystem::exists(status)) {
		return std::unexpected(launch_error(
			DirectVsLaunchErrorCode::file_missing, file
		));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(launch_error(
			DirectVsLaunchErrorCode::file_not_regular, file
		));
	}
	const auto size = std::filesystem::file_size(path, error);
	if(error || size > static_cast<std::uintmax_t>(
		std::numeric_limits<std::size_t>::max()
	) || size > static_cast<std::uintmax_t>(
		std::numeric_limits<std::streamsize>::max()
	)) {
		return std::unexpected(launch_error(
			DirectVsLaunchErrorCode::file_size_failed, file
		));
	}
	if(exact_size && size != *exact_size) {
		return std::unexpected(launch_error(
			DirectVsLaunchErrorCode::file_size_mismatch, file
		));
	}

	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(launch_error(
			DirectVsLaunchErrorCode::file_open_failed, file
		));
	}
	ExternalFile loaded{};
	loaded.bytes.resize(static_cast<std::size_t>(size));
	if(!loaded.bytes.empty()) {
		input.read(
			reinterpret_cast<char *>(loaded.bytes.data()),
			static_cast<std::streamsize>(loaded.bytes.size())
		);
	}
	if(!input || input.peek() != std::ifstream::traits_type::eof()) {
		return std::unexpected(launch_error(
			DirectVsLaunchErrorCode::file_read_failed, file
		));
	}
	return loaded;
}

} // namespace

std::expected<std::vector<std::byte>, DirectVsLaunchError>
DirectVsLaunchTestAccess::read_file(
	const std::filesystem::path &path,
	DirectVsFile file,
	std::optional<std::uintmax_t> exact_size
)
{
	auto loaded = read_external_file(path, file, exact_size);
	if(!loaded) {
		return std::unexpected(std::move(loaded.error()));
	}
	return std::move(loaded->bytes);
}

std::expected<std::unique_ptr<MainProcess>, DirectVsLaunchError>
load_direct_vs_process(DirectVsLaunchRequest request)
{
	if(request.content.data_root.empty() ||
		request.content.main_executable.empty() ||
		request.content.font_rom.empty() ||
		!valid_main_direct_vs_config(request.process)) {
		return std::unexpected(launch_error(
			DirectVsLaunchErrorCode::invalid_request
		));
	}

	auto main = read_external_file(
		request.content.main_executable,
		DirectVsFile::main_executable
	);
	if(!main) {
		return std::unexpected(std::move(main.error()));
	}
	auto compiled = import_main_compiled_content(main->bytes);
	if(!compiled) {
		return std::unexpected(DirectVsLaunchError{
			.code = DirectVsLaunchErrorCode::compiled_content_failed,
			.file = DirectVsFile::main_executable,
			.compiled_content_error = std::move(compiled.error()),
		});
	}

	auto font = read_external_file(
		request.content.font_rom,
		DirectVsFile::font_rom,
		Pc98FontRomProvider::font_rom_size
	);
	if(!font) {
		return std::unexpected(std::move(font.error()));
	}

	MainResourceLoader loader(request.content.data_root);
	auto resources = loader.load_direct_vs(
		request.process.player_one,
		request.process.player_two
	);
	if(!resources) {
		return std::unexpected(DirectVsLaunchError{
			.code = DirectVsLaunchErrorCode::resource_load_failed,
			.resource_error = std::move(resources.error()),
		});
	}

	auto process = MainProcess::create(
		request.process,
		*resources,
		std::move(*compiled),
		font->bytes
	);
	if(!process) {
		return std::unexpected(DirectVsLaunchError{
			.code = DirectVsLaunchErrorCode::process_failed,
			.process_error = std::move(process.error()),
		});
	}
	return std::move(*process);
}

} // namespace th03::app
