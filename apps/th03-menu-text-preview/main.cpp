#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/app/new_classic_assets.hpp"
#include "th03/app/new_classic_messages.hpp"
#include "th03/present/raster.hpp"

namespace {

namespace fs = std::filesystem;
using th03::app::NewClassicMenuFont;
using th03::app::NewClassicMessageArgument;
using th03::app::NewClassicMessageCatalog;
using th03::app::NewClassicMessageId;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;

constexpr std::size_t messages_per_page = 10U;

struct PreviewEntry final {
	std::string_view identifier{};
	std::string page{};
	std::uint32_t x = 0U;
	std::uint32_t y = 0U;
	std::uint32_t width = 0U;
	std::uint16_t maximum_width = 0U;
};

struct PreviewPage final {
	std::string name{};
	Raster raster{};
};

struct Preview final {
	std::vector<PreviewPage> pages{};
	std::vector<PreviewEntry> entries{};
};

[[nodiscard]] int fail(std::string_view stage, std::size_t code = 0U)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode=" << code << '\n';
	return 1;
}

void usage()
{
	std::cout
		<< "usage: th03-menu-text-preview --font <FONT.ROM> --output <dir>\n"
		<< "       [--language-pack <locale.t3lang>]\n"
		<< "       (--menu-font <MNUFONT.PF> | --replay-patch-assets <AZINN.DAT>\n"
		<< "        [--menu-backgrounds <PODDUI-dir>])\n\n"
		<< "Renders all T3LNG001 rows through the production mixed menu font.\n";
}

[[nodiscard]] std::expected<std::vector<std::byte>, int> read_file(
	const fs::path &path,
	std::size_t exact_size
)
{
	std::error_code system;
	const auto status = fs::status(path, system);
	if(system || !fs::is_regular_file(status)) {
		return std::unexpected(1);
	}
	const auto size = fs::file_size(path, system);
	if(system || size != exact_size) {
		return std::unexpected(2);
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(3);
	}
	std::vector<std::byte> result(exact_size);
	input.read(reinterpret_cast<char *>(result.data()),
		static_cast<std::streamsize>(result.size()));
	if(!input || input.gcount() != static_cast<std::streamsize>(result.size())) {
		return std::unexpected(4);
	}
	return result;
}

[[nodiscard]] std::string numbered_name(std::size_t number)
{
	std::string result{"menu-"};
	result.push_back(static_cast<char>('0' + ((number / 10U) % 10U)));
	result.push_back(static_cast<char>('0' + (number % 10U)));
	return result;
}

[[nodiscard]] std::vector<std::string_view> split(
	std::string_view text,
	char delimiter
)
{
	std::vector<std::string_view> result{};
	if(text.empty()) {
		return result;
	}
	std::size_t first = 0U;
	while(first <= text.size()) {
		const auto next = text.find(delimiter, first);
		result.push_back(text.substr(
			first, next == std::string_view::npos ? next : next - first
		));
		if(next == std::string_view::npos) {
			break;
		}
		first = next + 1U;
	}
	return result;
}

[[nodiscard]] std::string formatted_message(
	const NewClassicMessageCatalog &catalog,
	NewClassicMessageId id
)
{
	const auto &schema = th03::app::new_classic_message_schema(id);
	const auto names = split(schema.placeholders, ',');
	const auto values = split(schema.width_values, '|');
	if(names.size() != values.size()) {
		return {};
	}
	std::vector<NewClassicMessageArgument> arguments{};
	arguments.reserve(names.size());
	for(std::size_t index = 0U; index < names.size(); ++index) {
		arguments.push_back({names[index], values[index]});
	}
	return catalog.format(id, arguments);
}

[[nodiscard]] bool initialize_page(PreviewPage &page) noexcept
{
	if(!page.raster.set_access_page(Page::zero) ||
		!page.raster.set_shown_page(Page::zero) ||
		!page.raster.clear({0, 0, 640U, 400U}, 0U) ||
		!page.raster.set_pending_palette_entry(0U, {0U, 0U, 0U}) ||
		!page.raster.set_pending_palette_entry(7U, {7U, 7U, 7U}) ||
		!page.raster.set_pending_palette_entry(9U, {12U, 10U, 15U}) ||
		!page.raster.set_pending_palette_entry(15U, {15U, 15U, 15U})) {
		return false;
	}
	page.raster.apply_pending_palette();
	return true;
}

[[nodiscard]] std::expected<Preview, std::size_t> render_preview(
	const NewClassicMessageCatalog &catalog,
	const NewClassicMenuFont &font
)
{
	Preview result{};
	const auto page_count =
		(th03::app::new_classic_message_count + messages_per_page - 1U) /
		messages_per_page;
	result.pages.reserve(page_count);
	result.entries.reserve(th03::app::new_classic_message_count);
	for(std::size_t page = 0U; page < page_count; ++page) {
		result.pages.push_back({.name = numbered_name(page)});
		if(!initialize_page(result.pages.back())) {
			return std::unexpected(page);
		}
		const auto title = std::string{"MENU TEXT "} +
			numbered_name(page).substr(5U) + "/" +
			std::to_string(page_count) + " - " + std::string{catalog.locale()};
		if(!font.put(result.pages.back().raster, 8, 4, title, 9U)) {
			return std::unexpected(page);
		}
	}
	for(std::size_t index = 0U;
		index < th03::app::new_classic_message_count;
		++index) {
		const auto id = static_cast<NewClassicMessageId>(index);
		const auto &schema = th03::app::new_classic_message_schema(id);
		const auto text = formatted_message(catalog, id);
		if(text.empty()) {
			return std::unexpected(index);
		}
		const auto page = index / messages_per_page;
		const auto row = index % messages_per_page;
		const auto label_y = static_cast<std::int32_t>(28U + (row * 36U));
		const auto text_y = label_y + 16;
		auto &raster = result.pages[page].raster;
		if(!font.put(raster, 8, label_y, schema.identifier, 7U) ||
			!font.put(raster, 16, text_y, text, 15U)) {
			return std::unexpected(index);
		}
		const auto marker_x = 16 + static_cast<std::int32_t>(schema.maximum_width);
		if(marker_x < static_cast<std::int32_t>(th03::present::graphics_width)) {
			for(std::int32_t y = text_y; y < text_y + 16; ++y) {
				if(!raster.plot({marker_x, y}, 9U)) {
					return std::unexpected(index);
				}
			}
		}
		result.entries.push_back({
			.identifier = schema.identifier,
			.page = result.pages[page].name,
			.x = 16U,
			.y = static_cast<std::uint32_t>(text_y),
			.width = static_cast<std::uint32_t>(font.width(text)),
			.maximum_width = schema.maximum_width,
		});
	}
	return result;
}

[[nodiscard]] bool write_ppm(const fs::path &path, const Raster &raster)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if(!output) {
		return false;
	}
	output << "P6\n640 400\n255\n";
	const auto palette = raster.applied_palette();
	for(const auto index : raster.shown_graphics()) {
		const auto color = palette.colors[index];
		const std::array<char, 3U> pixel{
			static_cast<char>(color.r * 0x11U),
			static_cast<char>(color.g * 0x11U),
			static_cast<char>(color.b * 0x11U),
		};
		output.write(pixel.data(), static_cast<std::streamsize>(pixel.size()));
	}
	return static_cast<bool>(output);
}

[[nodiscard]] bool write_report(
	const fs::path &path,
	const Preview &preview,
	std::string_view locale
)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if(!output) {
		return false;
	}
	output << "{\n  \"format\": \"T3MTP001\",\n  \"locale\": \""
		<< locale << "\",\n  \"width\": 640,\n  \"height\": 400,\n"
		<< "  \"messages\": [\n";
	for(std::size_t index = 0U; index < preview.entries.size(); ++index) {
		const auto &entry = preview.entries[index];
		output << "    {\"id\": \"" << entry.identifier << "\", \"page\": \""
			<< entry.page << "\", \"x\": " << entry.x << ", \"y\": "
			<< entry.y << ", \"width\": " << entry.width
			<< ", \"maximum_width\": " << entry.maximum_width << '}';
		if(index + 1U != preview.entries.size()) {
			output << ',';
		}
		output << '\n';
	}
	output << "  ]\n}\n";
	return static_cast<bool>(output);
}

} // namespace

int main(int argc, char **argv)
{
	if(argc == 2 && std::string_view(argv[1]) == "--help") {
		usage();
		return 0;
	}
	fs::path font_path{};
	fs::path output_path{};
	fs::path pack_path{};
	fs::path menu_font_path{};
	fs::path assets_path{};
	fs::path menu_backgrounds{};
	if(argc < 7 || argc > 13 || (argc & 1) == 0) {
		usage();
		return 2;
	}
	for(int index = 1; index < argc; index += 2) {
		const auto key = std::string_view(argv[index]);
		const auto value = fs::path(argv[index + 1]);
		if(value.empty()) {
			return 2;
		}
		if(key == "--font" && font_path.empty()) {
			font_path = value;
		} else if(key == "--output" && output_path.empty()) {
			output_path = value;
		} else if(key == "--language-pack" && pack_path.empty()) {
			pack_path = value;
		} else if(key == "--menu-font" && menu_font_path.empty()) {
			menu_font_path = value;
		} else if(key == "--replay-patch-assets" && assets_path.empty()) {
			assets_path = value;
		} else if(key == "--menu-backgrounds" && menu_backgrounds.empty()) {
			menu_backgrounds = value;
		} else {
			usage();
			return 2;
		}
	}
	if(font_path.empty() || output_path.empty() ||
		(menu_font_path.empty() == assets_path.empty()) ||
		(!menu_backgrounds.empty() && assets_path.empty())) {
		usage();
		return 2;
	}
	try {
		NewClassicMenuFont font{};
		if(!assets_path.empty()) {
			auto assets = th03::app::load_new_classic_replay_patch_assets(
				assets_path, menu_backgrounds, font_path
			);
			if(!assets) {
				return fail("replay-patch-assets",
					static_cast<std::size_t>(assets.error().code));
			}
			font = std::move(assets->menu_font);
		} else {
			auto font_bytes = read_file(menu_font_path, 3'168U);
			if(!font_bytes) {
				return fail("menu-font-read", static_cast<std::size_t>(font_bytes.error()));
			}
			auto decoded = th03::app::detail::decode_new_classic_menu_font(*font_bytes);
			if(!decoded) {
				return fail("menu-font-decode",
					static_cast<std::size_t>(decoded.error().code));
			}
			auto machine_font = read_file(font_path, 288'768U);
			if(!machine_font) {
				return fail("font-read", static_cast<std::size_t>(machine_font.error()));
			}
			if(!decoded->install_machine_font(*machine_font)) {
				return fail("font-validate");
			}
			font = std::move(*decoded);
		}

		const NewClassicMessageCatalog *catalog =
			&th03::app::new_classic_builtin_messages(
				th03::app::NewClassicLanguageAssetProfile::english
			);
		std::optional<NewClassicMessageCatalog> custom{};
		if(!pack_path.empty()) {
			auto loaded = th03::app::load_new_classic_message_pack(
				pack_path, font
			);
			if(!loaded) {
				return fail("language-pack",
					static_cast<std::size_t>(loaded.error().code));
			}
			custom.emplace(std::move(*loaded));
			catalog = &*custom;
		}
		auto preview = render_preview(*catalog, font);
		if(!preview) {
			return fail("render", preview.error());
		}
		std::error_code system;
		fs::create_directories(output_path, system);
		if(system || !fs::is_directory(output_path)) {
			return fail("output-directory", static_cast<std::size_t>(system.value()));
		}
		for(const auto &page : preview->pages) {
			if(!write_ppm(output_path / (page.name + ".ppm"), page.raster)) {
				return fail("output-write");
			}
		}
		if(!write_report(output_path / "preview.json", *preview, catalog->locale())) {
			return fail("report-write");
		}
		std::cout << "status=complete\nlocale=" << catalog->locale()
			<< "\nmessages=" << preview->entries.size()
			<< "\npages=" << preview->pages.size() << '\n';
		return 0;
	} catch(const std::bad_alloc &) {
		return fail("allocation");
	}
}
