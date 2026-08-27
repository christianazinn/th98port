#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "th03/app/content/authentic_text.hpp"
#include "th03/app/new_classic/content/new_classic_cutscene_text.hpp"
#include "th03/present/graphics_text.hpp"
#include "th03/present/raster.hpp"

namespace {

namespace fs = std::filesystem;
using th03::app::NewClassicCutsceneTextCatalog;
using th03::present::GraphicsTextStyle;
using th03::present::GraphicsTextWeight;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;

constexpr std::size_t runs_per_page = 24U;
constexpr std::array<std::string_view, 28U> script_identifiers{
	"dm0.reimu", "dm0.mima", "dm0.marisa", "dm0.ellen",
	"dm0.kotohime", "dm0.kana", "dm0.rikako", "dm0.chiyuri",
	"dm0.yumemi", "dm1.reimu", "dm1.mima", "dm1.marisa",
	"dm1.ellen", "dm1.kotohime", "dm1.kana", "dm1.rikako",
	"dm1.chiyuri", "dm1.yumemi", "ending.reimu", "ending.mima",
	"ending.marisa", "ending.ellen", "ending.kotohime", "ending.kana",
	"ending.rikako", "ending.chiyuri", "ending.yumemi", "extra-ending",
};

struct PreviewEntry final {
	std::string identifier{};
	std::string page{};
	std::uint16_t script = 0U;
	std::uint16_t run = 0U;
	std::uint32_t x = 0U;
	std::uint32_t y = 0U;
	std::uint32_t width = 0U;
	std::uint16_t glyphs = 0U;
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
		<< "usage: th03-cutscene-text-preview --font <FONT.ROM>\n"
		<< "       --cutscene-text-pack <locale.t3ctp> --output <dir>\n\n"
		<< "Renders every validated T3CTP001 run through production graphics text.\n";
}

[[nodiscard]] std::expected<std::vector<std::byte>, int> read_font(
	const fs::path &path
)
{
	std::error_code system;
	const auto status = fs::status(path, system);
	if(system || !fs::is_regular_file(status)) {
		return std::unexpected(1);
	}
	const auto size = fs::file_size(path, system);
	if(system || size != th03::app::Pc98FontRomProvider::font_rom_size) {
		return std::unexpected(2);
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(3);
	}
	std::vector<std::byte> result(static_cast<std::size_t>(size));
	input.read(reinterpret_cast<char *>(result.data()),
		static_cast<std::streamsize>(result.size()));
	if(!input || input.gcount() != static_cast<std::streamsize>(result.size())) {
		return std::unexpected(4);
	}
	return result;
}

[[nodiscard]] std::string two_digits(std::size_t value)
{
	std::string result{};
	result.push_back(static_cast<char>('0' + ((value / 10U) % 10U)));
	result.push_back(static_cast<char>('0' + (value % 10U)));
	return result;
}

[[nodiscard]] std::string page_name(std::size_t script, std::size_t page)
{
	return "script-" + two_digits(script) + '-' + two_digits(page);
}

[[nodiscard]] std::string run_identifier(
	std::size_t script,
	std::size_t run
)
{
	std::string result{"cutscene."};
	result.append(script_identifiers[script]);
	result.append(".run.");
	result.push_back(static_cast<char>('0' + ((run / 100U) % 10U)));
	result.push_back(static_cast<char>('0' + ((run / 10U) % 10U)));
	result.push_back(static_cast<char>('0' + (run % 10U)));
	return result;
}

[[nodiscard]] bool initialize_page(PreviewPage &page) noexcept
{
	if(!page.raster.set_access_page(Page::zero) ||
		!page.raster.set_shown_page(Page::zero) ||
		!page.raster.clear({0, 0, 640U, 400U}, 0U) ||
		!page.raster.set_pending_palette_entry(0U, {0U, 0U, 0U}) ||
		!page.raster.set_pending_palette_entry(11U, {12U, 10U, 15U}) ||
		!page.raster.set_pending_palette_entry(15U, {15U, 15U, 15U})) {
		return false;
	}
	page.raster.apply_pending_palette();
	return true;
}

[[nodiscard]] bool put(
	Raster &raster,
	const th03::present::AnkGlyphProvider &font,
	std::span<const std::uint8_t> bytes,
	Point position,
	std::uint8_t color,
	std::uint32_t *width = nullptr
)
{
	auto prepared = th03::present::prepare_graphics_text_8x16(bytes, font);
	if(!prepared) {
		return false;
	}
	if(width) {
		*width = prepared->pixel_width();
	}
	return th03::present::graphics_text_put_8x16(
		raster, position, *prepared,
		GraphicsTextStyle{color, GraphicsTextWeight::normal, 0U}
	);
}

[[nodiscard]] bool put_ascii(
	Raster &raster,
	const th03::present::AnkGlyphProvider &font,
	std::string_view text,
	Point position,
	std::uint8_t color
)
{
	return put(raster, font, std::span<const std::uint8_t>{
		reinterpret_cast<const std::uint8_t *>(text.data()), text.size()
	}, position, color);
}

[[nodiscard]] std::expected<Preview, std::size_t> render_preview(
	const NewClassicCutsceneTextCatalog &catalog,
	const th03::present::AnkGlyphProvider &font
)
{
	std::size_t total_pages = 0U;
	std::size_t total_runs = 0U;
	for(std::size_t script = 0U; script < script_identifiers.size(); ++script) {
		const auto runs = catalog.run_count(script);
		if(runs == 0U) {
			return std::unexpected(script);
		}
		total_runs += runs;
		total_pages += (runs + runs_per_page - 1U) / runs_per_page;
	}
	Preview result{};
	result.pages.reserve(total_pages);
	result.entries.reserve(total_runs);
	for(std::size_t script = 0U; script < script_identifiers.size(); ++script) {
		const auto runs = catalog.run_count(script);
		const auto pages = (runs + runs_per_page - 1U) / runs_per_page;
		for(std::size_t page = 0U; page < pages; ++page) {
			result.pages.push_back({.name = page_name(script, page)});
			auto &output = result.pages.back();
			if(!initialize_page(output)) {
				return std::unexpected(script);
			}
			const auto title = std::string{script_identifiers[script]} + ' ' +
				std::to_string(page + 1U) + '/' + std::to_string(pages) + " - " +
				std::string{catalog.locale()};
			if(!put_ascii(output.raster, font, title, {8, 0}, 11U)) {
				return std::unexpected(script);
			}
			const auto first_run = page * runs_per_page;
			const auto page_runs = std::min(runs_per_page, runs - first_run);
			for(std::size_t row = 0U; row < page_runs; ++row) {
				const auto run = first_run + row;
				const auto bytes = catalog.replacement_run_bytes(script, run);
				if(bytes.empty() || (bytes.size() & 1U) != 0U ||
					bytes.size() / 2U > 39U) {
					return std::unexpected(script);
				}
				std::uint32_t width = 0U;
				const auto y = static_cast<std::int32_t>(16U + row * 16U);
				if(!put(output.raster, font, bytes, {8, y}, 15U, &width)) {
					return std::unexpected(script);
				}
				result.entries.push_back({
					.identifier = run_identifier(script, run),
					.page = output.name,
					.script = static_cast<std::uint16_t>(script),
					.run = static_cast<std::uint16_t>(run),
					.x = 8U,
					.y = static_cast<std::uint32_t>(y),
					.width = width,
					.glyphs = static_cast<std::uint16_t>(bytes.size() / 2U),
				});
			}
		}
	}
	return result;
}

void u16(std::span<std::uint8_t> bytes, std::size_t offset, std::uint16_t value)
{
	bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
	bytes[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
}

void u32(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value)
{
	for(std::size_t index = 0U; index < 4U; ++index) {
		bytes[offset + index] = static_cast<std::uint8_t>(value >> (index * 8U));
	}
}

[[nodiscard]] bool write_bmp(const fs::path &path, const Raster &raster)
{
	constexpr std::size_t header_bytes = 14U + 40U + (16U * 4U);
	constexpr std::size_t row_bytes = 640U / 2U;
	constexpr std::size_t image_bytes = row_bytes * 400U;
	std::array<std::uint8_t, header_bytes> header{};
	header[0U] = 'B';
	header[1U] = 'M';
	u32(header, 2U, static_cast<std::uint32_t>(header_bytes + image_bytes));
	u32(header, 10U, static_cast<std::uint32_t>(header_bytes));
	u32(header, 14U, 40U);
	u32(header, 18U, 640U);
	u32(header, 22U, 400U);
	u16(header, 26U, 1U);
	u16(header, 28U, 4U);
	u32(header, 34U, static_cast<std::uint32_t>(image_bytes));
	u32(header, 46U, 16U);
	const auto palette = raster.applied_palette();
	for(std::size_t index = 0U; index < palette.colors.size(); ++index) {
		const auto base = 54U + (index * 4U);
		header[base + 0U] = static_cast<std::uint8_t>(
			palette.colors[index].b * 0x11U
		);
		header[base + 1U] = static_cast<std::uint8_t>(
			palette.colors[index].g * 0x11U
		);
		header[base + 2U] = static_cast<std::uint8_t>(
			palette.colors[index].r * 0x11U
		);
	}
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if(!output) {
		return false;
	}
	output.write(reinterpret_cast<const char *>(header.data()),
		static_cast<std::streamsize>(header.size()));
	const auto pixels = raster.shown_graphics();
	std::array<std::uint8_t, row_bytes> row{};
	for(std::size_t y = 400U; y-- > 0U;) {
		for(std::size_t x = 0U; x < 640U; x += 2U) {
			row[x / 2U] = static_cast<std::uint8_t>(
				(pixels[y * 640U + x] << 4U) | pixels[y * 640U + x + 1U]
			);
		}
		output.write(reinterpret_cast<const char *>(row.data()),
			static_cast<std::streamsize>(row.size()));
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
	output << "{\n  \"format\": \"T3STP001\",\n  \"locale\": \""
		<< locale << "\",\n  \"width\": 640,\n  \"height\": 400,\n"
		<< "  \"messages\": [\n";
	for(std::size_t index = 0U; index < preview.entries.size(); ++index) {
		const auto &entry = preview.entries[index];
		output << "    {\"id\": \"" << entry.identifier << "\", \"page\": \""
			<< entry.page << "\", \"script\": " << entry.script
			<< ", \"run\": " << entry.run << ", \"x\": " << entry.x
			<< ", \"y\": " << entry.y << ", \"width\": " << entry.width
			<< ", \"glyphs\": " << entry.glyphs << '}';
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
	fs::path pack_path{};
	fs::path output_path{};
	if(argc != 7) {
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
		} else if(key == "--cutscene-text-pack" && pack_path.empty()) {
			pack_path = value;
		} else if(key == "--output" && output_path.empty()) {
			output_path = value;
		} else {
			usage();
			return 2;
		}
	}
	if(font_path.empty() || pack_path.empty() || output_path.empty()) {
		usage();
		return 2;
	}
	try {
		auto font_bytes = read_font(font_path);
		if(!font_bytes) {
			return fail("font-read", static_cast<std::size_t>(font_bytes.error()));
		}
		auto font = th03::app::Pc98FontRomProvider::from_exact_font_rom(*font_bytes);
		if(!font) {
			return fail("font-validate", static_cast<std::size_t>(font.error().code));
		}
		auto catalog = th03::app::load_new_classic_cutscene_text_pack(pack_path);
		if(!catalog) {
			return fail("cutscene-text-pack",
				static_cast<std::size_t>(catalog.error().code));
		}
		auto preview = render_preview(*catalog, *font);
		if(!preview) {
			return fail("render", preview.error());
		}
		std::error_code system;
		fs::create_directories(output_path, system);
		if(system || !fs::is_directory(output_path)) {
			return fail("output-directory", static_cast<std::size_t>(system.value()));
		}
		for(const auto &page : preview->pages) {
			if(!write_bmp(output_path / (page.name + ".bmp"), page.raster)) {
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
