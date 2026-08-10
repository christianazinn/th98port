#ifndef TH03_APP_DIRECT_VS_LAUNCH_INTERNAL_HPP
#define TH03_APP_DIRECT_VS_LAUNCH_INTERNAL_HPP

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <vector>

#include "th03/app/direct_vs_launch.hpp"

namespace th03::app {

struct DirectVsLaunchTestAccess {
	[[nodiscard]] static std::expected<
		std::vector<std::byte>,
		DirectVsLaunchError
	> read_file(
		const std::filesystem::path &path,
		DirectVsFile file,
		std::optional<std::uintmax_t> exact_size = std::nullopt
	);
};

} // namespace th03::app

#endif
