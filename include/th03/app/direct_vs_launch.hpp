#ifndef TH03_APP_DIRECT_VS_LAUNCH_HPP
#define TH03_APP_DIRECT_VS_LAUNCH_HPP

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>

#include "th03/app/main_process.hpp"

namespace th03::app {

struct DirectVsContentPaths {
	std::filesystem::path data_root{};
	std::filesystem::path main_executable{};
	std::filesystem::path mainl_executable{};
	std::filesystem::path font_rom{};
	std::filesystem::path language_overlay{};
};

struct DirectVsLaunchRequest {
	DirectVsContentPaths content{};
	MainDirectVsConfig process{};
};

enum class DirectVsFile : std::uint8_t {
	none,
	main_executable,
	font_rom,
};

enum class DirectVsLaunchErrorCode : std::uint8_t {
	invalid_request,
	file_missing,
	file_not_regular,
	file_size_failed,
	file_size_mismatch,
	file_open_failed,
	file_read_failed,
	resource_load_failed,
	compiled_content_failed,
	process_failed,
};

// Diagnostics deliberately retain only semantic roles and nested stable
// errors. Original paths, digests, and byte values never enter this value.
struct DirectVsLaunchError {
	DirectVsLaunchErrorCode code = DirectVsLaunchErrorCode::invalid_request;
	DirectVsFile file = DirectVsFile::none;
	std::optional<ResourceLoadError> resource_error{};
	std::optional<MainCompiledContentError> compiled_content_error{};
	std::optional<MainProcessError> process_error{};
};

// Transactional external-input boundary for one direct-VS MAIN process.
// MAIN.EXE is authenticated by its compiled-content profile, FONT.ROM must be
// the exact structural size accepted by Pc98FontRomProvider, and the extracted
// data root is loaded with the canonical size+fingerprint policy. All loader
// buffers die before this function returns; MainProcess owns everything it
// continues to use.
[[nodiscard]] std::expected<std::unique_ptr<MainProcess>, DirectVsLaunchError>
load_direct_vs_process(DirectVsLaunchRequest request);

} // namespace th03::app

#endif
