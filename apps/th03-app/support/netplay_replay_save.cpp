#include "netplay_replay_save.hpp"

#include <cstddef>
#include <fstream>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "diagnostics.hpp"

#include "th03/app/content/resources.hpp"

namespace th03::app::launcher_support {
namespace {

[[nodiscard]] std::expected<std::vector<std::byte>, int> read_file(
	const std::filesystem::path &path,
	const char *code
)
{
	std::error_code system{};
	const auto status = std::filesystem::status(path, system);
	if(system || !std::filesystem::is_regular_file(status)) {
		return std::unexpected(fail_netplay(
			"netplay-replay-save-art", code
		));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system || size > static_cast<std::uintmax_t>(
		std::numeric_limits<std::size_t>::max()
	) || size > static_cast<std::uintmax_t>(
		std::numeric_limits<std::streamsize>::max()
	)) {
		return std::unexpected(fail_netplay(
			"netplay-replay-save-art", code
		));
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(fail_netplay(
			"netplay-replay-save-art", code
		));
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	if(!bytes.empty()) {
		input.read(
			reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
	}
	if(!input || input.peek() != std::ifstream::traits_type::eof()) {
		return std::unexpected(fail_netplay(
			"netplay-replay-save-art", code
		));
	}
	return bytes;
}

} // namespace

std::expected<DirectNetplayReplaySaveResources, int>
prepare_direct_netplay_replay_save_resources(
	const DirectVsContentPaths &content
)
{
	auto patch = load_new_classic_replay_patch_assets(
		content.data_root / "AZINN.DAT", content.new_classic_menu_assets,
		content.font_rom
	);
	if(!patch) {
		return std::unexpected(fail_netplay(
			"netplay-replay-save-art", "patch-assets"
		));
	}
	auto patch_assets = std::make_shared<const NewClassicReplayPatchAssets>(
		std::move(*patch)
	);

	MainResourceLoader loader(content.data_root);
	auto normal = loader.load({
		.role = MainResourceRole::registration_normal_glyphs,
	});
	auto highlight = loader.load({
		.role = MainResourceRole::registration_highlight_glyphs,
	});
	if(!normal || !highlight) {
		return std::unexpected(fail_netplay(
			"netplay-replay-save-art", "registration-glyphs"
		));
	}
	auto font = read_file(content.font_rom, "font-rom");
	if(!font) {
		return std::unexpected(font.error());
	}
	auto save_art = NewClassicReplaySaveArt::prepare(
		std::span<const std::byte>{**normal},
		std::span<const std::byte>{**highlight},
		*font
	);
	if(!save_art) {
		return std::unexpected(fail_netplay(
			"netplay-replay-save-art", "prepare"
		));
	}
	return DirectNetplayReplaySaveResources{
		.replay_root = content.data_root / "REPLAY",
		.patch_assets = std::move(patch_assets),
		.save_art = std::move(*save_art),
	};
}

} // namespace th03::app::launcher_support
