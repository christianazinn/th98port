#ifndef TH03_TESTS_NEW_CLASSIC_MENU_PREVIEW_HPP
#define TH03_TESTS_NEW_CLASSIC_MENU_PREVIEW_HPP

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string_view>

#include "th03/app/new_classic_assets.hpp"

namespace th03::test {

[[nodiscard]] inline const char *environment(const char *name) noexcept
{
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
	const auto *value = std::getenv(name);
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
	return value;
}

inline void write_menu_preview(
	present::SourceSurface surface,
	const std::filesystem::path &path
)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output << "P6\n640 400\n255\n";
	for(const auto pixel : surface.pixels) {
		std::array<std::uint8_t, 3U> rgb{};
		if(pixel.layer == present::ComposedPixelLayer::graphics) {
			const auto color = surface.graphics_palette.colors[pixel.value];
			rgb = {
				static_cast<std::uint8_t>(color.r * 17U),
				static_cast<std::uint8_t>(color.g * 17U),
				static_cast<std::uint8_t>(color.b * 17U),
			};
		} else {
			rgb = {
				((pixel.value & 0x02U) != 0U) ?
					std::uint8_t{255U} : std::uint8_t{0U},
				((pixel.value & 0x04U) != 0U) ?
					std::uint8_t{255U} : std::uint8_t{0U},
				((pixel.value & 0x01U) != 0U) ?
					std::uint8_t{255U} : std::uint8_t{0U},
			};
		}
		output.write(reinterpret_cast<const char *>(rgb.data()), 3);
	}
}

inline void dump_menu_preview(
	present::SourceSurface surface,
	std::string_view filename
)
{
	const auto *directory = environment("TH03_MENU_PREVIEW_DIR");
	if(!directory || *directory == '\0') {
		return;
	}
	const std::filesystem::path root(directory);
	std::filesystem::create_directories(root);
	write_menu_preview(surface, root / filename);
}

[[nodiscard]] inline std::shared_ptr<const app::NewClassicReplayPatchAssets>
authentic_menu_assets()
{
	const auto *path = environment("TH03_AZINN_DAT");
	if(!path || *path == '\0') {
		return {};
	}
	auto loaded = app::load_new_classic_replay_patch_assets(
		std::filesystem::path(path)
	);
	if(!loaded) {
		return {};
	}
	return std::make_shared<const app::NewClassicReplayPatchAssets>(
		std::move(*loaded)
	);
}

} // namespace th03::test

#endif
