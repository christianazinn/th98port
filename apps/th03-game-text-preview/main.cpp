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

#include "th03/app/authentic_text.hpp"
#include "th03/app/new_classic_game_text.hpp"
#include "th03/present/graphics_text.hpp"
#include "th03/present/raster.hpp"

namespace {

namespace fs = std::filesystem;
using th03::app::NewClassicGameTextCatalog;
using th03::app::NewClassicGameTextId;
using th03::present::GraphicsTextStyle;
using th03::present::GraphicsTextWeight;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;

struct PreviewEntry final {
	std::string_view identifier{};
	std::string_view page{};
	std::uint32_t x = 0U;
	std::uint32_t y = 0U;
	std::uint32_t width = 0U;
	std::uint32_t height = 16U;
	std::uint16_t exact_bytes = 0U;
};

struct PreviewPage final {
	std::string name{};
	Raster raster{};
};

struct Preview final {
	std::vector<PreviewPage> pages{};
	std::vector<PreviewEntry> entries{};
};

[[nodiscard]] std::string numbered_name(
	std::string_view prefix,
	std::size_t number,
	std::string_view suffix = {}
)
{
	std::string result{prefix};
	result.push_back(static_cast<char>('0' + ((number / 10U) % 10U)));
	result.push_back(static_cast<char>('0' + (number % 10U)));
	result.append(suffix);
	return result;
}

[[nodiscard]] int fail(std::string_view stage, std::size_t code = 0U)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode=" << code << '\n';
	return 1;
}

void usage()
{
	std::cout
		<< "usage: th03-game-text-preview --font <FONT.ROM> --output <dir>\n"
		<< "       [--game-text-pack <locale.t3gtl>]\n\n"
		<< "Renders deterministic 640x400 PPM contact sheets and preview.json\n"
		<< "through the production PC-98 graphics-text renderer.\n";
}

[[nodiscard]] std::expected<std::vector<std::byte>, int> read_file(
	const fs::path &path,
	std::size_t exact_size = 0U
)
{
	std::error_code system;
	const auto status = fs::status(path, system);
	if(system || !fs::is_regular_file(status)) {
		return std::unexpected(1);
	}
	const auto size = fs::file_size(path, system);
	if(system || size == 0U || (exact_size != 0U && size != exact_size)) {
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

[[nodiscard]] bool initialize_page(PreviewPage &page) noexcept
{
	return page.raster.set_access_page(Page::zero) &&
		page.raster.set_shown_page(Page::zero) &&
		page.raster.clear({0, 0, 640U, 400U}, 0U) &&
		page.raster.set_pending_palette_entry(0U, {0U, 0U, 0U}) &&
		page.raster.set_pending_palette_entry(7U, {7U, 7U, 7U}) &&
		page.raster.set_pending_palette_entry(11U, {0U, 15U, 15U}) &&
		page.raster.set_pending_palette_entry(15U, {15U, 15U, 15U});
}

[[nodiscard]] bool put_text(
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
	return put_text(raster, font, std::span<const std::uint8_t>{
		reinterpret_cast<const std::uint8_t *>(text.data()), text.size()
	}, position, color);
}

[[nodiscard]] bool put_message(
	Preview &preview,
	std::size_t page_index,
	const NewClassicGameTextCatalog &catalog,
	const th03::present::AnkGlyphProvider &font,
	NewClassicGameTextId id,
	Point position
)
{
	const auto &schema = th03::app::new_classic_game_text_schema(id);
	std::uint32_t width = 0U;
	if(!put_text(
		preview.pages[page_index].raster, font, catalog.bytes(id), position, 15U,
		&width
	)) {
		return false;
	}
	preview.entries.push_back({
		.identifier = schema.identifier,
		.page = preview.pages[page_index].name,
		.x = static_cast<std::uint32_t>(position.x),
		.y = static_cast<std::uint32_t>(position.y),
		.width = width,
		.exact_bytes = schema.exact_bytes,
	});
	return true;
}

[[nodiscard]] NewClassicGameTextId add(
	NewClassicGameTextId first,
	std::size_t offset
) noexcept
{
	return static_cast<NewClassicGameTextId>(
		static_cast<std::size_t>(first) + offset
	);
}

[[nodiscard]] std::expected<Preview, std::size_t> render_preview(
	const NewClassicGameTextCatalog &catalog,
	const th03::present::AnkGlyphProvider &font
)
{
	Preview result{};
	result.pages.push_back({.name = "music"});
	result.pages.push_back({.name = "stage"});
	result.pages.push_back({.name = "registration-result"});
	result.pages.push_back({.name = "hud-banners"});
	result.pages.push_back({.name = "continue-gameover"});
	for(std::size_t character = 0U;
		character < th03::app::new_classic_win_quote_characters;
		++character) {
		result.pages.push_back({
			.name = numbered_name("win-quotes-", character, "-a"),
		});
		result.pages.push_back({
			.name = numbered_name("win-quotes-", character, "-b"),
		});
	}
	for(std::size_t track = 0U;
		track < th03::app::new_classic_music_comment_tracks;
		++track) {
		result.pages.push_back({
			.name = numbered_name("music-comments-", track + 1U),
		});
	}
	for(auto &page : result.pages) {
		if(!initialize_page(page)) {
			return std::unexpected(0U);
		}
		page.raster.apply_pending_palette();
	}
	if(!put_ascii(result.pages[0U].raster, font,
		"MUSIC ROOM TITLES", {16, 8}, 11U)) {
		return std::unexpected(0U);
	}
	for(std::size_t index = 0U; index < 19U; ++index) {
		const auto id = add(NewClassicGameTextId::music_track_01, index);
		const auto &schema = th03::app::new_classic_game_text_schema(id);
		const auto y = static_cast<std::int32_t>(40U + index * 18U);
		if(!put_ascii(result.pages[0U].raster, font, schema.identifier,
			{16, y}, 7U) ||
			!put_message(result, 0U, catalog, font, id, {192, y})) {
			return std::unexpected(static_cast<std::size_t>(id));
		}
	}
	if(!put_ascii(result.pages[1U].raster, font,
		"STAGE AND INTERSTITIAL TEXT", {16, 8}, 11U)) {
		return std::unexpected(0U);
	}
	for(std::size_t index = 0U; index < 9U; ++index) {
		const auto y = static_cast<std::int32_t>(40U + index * 40U);
		const std::array label{
			static_cast<char>(static_cast<std::size_t>('0') + index),
			' ', 'F', '/', 'S', '\0'
		};
		const std::string_view label_text{label.data(), label.size() - 1U};
		const auto first = add(NewClassicGameTextId::stage_title_first_00, index);
		const auto second = add(NewClassicGameTextId::stage_title_second_00, index);
		const auto name = add(NewClassicGameTextId::stage_name_00, index);
		if(!put_ascii(result.pages[1U].raster, font, label_text, {16, y}, 7U) ||
			!put_message(result, 1U, catalog, font, first, {80, y}) ||
			!put_message(result, 1U, catalog, font, second, {352, y}) ||
			!put_ascii(result.pages[1U].raster, font, "NAME", {16, y + 18}, 7U) ||
			!put_message(result, 1U, catalog, font, name, {80, y + 18})) {
			return std::unexpected(static_cast<std::size_t>(first));
		}
	}
	if(!put_ascii(result.pages[2U].raster, font,
		"REGISTRATION NAMES", {16, 8}, 11U)) {
		return std::unexpected(0U);
	}
	for(std::size_t index = 0U; index < 10U; ++index) {
		const auto id = add(NewClassicGameTextId::registration_00, index);
		const auto &schema = th03::app::new_classic_game_text_schema(id);
		const auto y = static_cast<std::int32_t>(40U + index * 18U);
		if(!put_ascii(result.pages[2U].raster, font, schema.identifier,
			{16, y}, 7U) ||
			!put_message(result, 2U, catalog, font, id, {280, y})) {
			return std::unexpected(static_cast<std::size_t>(id));
		}
	}
	if(!put_ascii(result.pages[2U].raster, font,
		"RESULT LABELS", {16, 224}, 11U)) {
		return std::unexpected(0U);
	}
	for(std::size_t index = 0U; index < 5U; ++index) {
		const auto id = add(NewClassicGameTextId::result_gauge_attack, index);
		const auto &schema = th03::app::new_classic_game_text_schema(id);
		const auto y = static_cast<std::int32_t>(248U + index * 18U);
		if(!put_ascii(result.pages[2U].raster, font, schema.identifier,
			{16, y}, 7U) ||
			!put_message(result, 2U, catalog, font, id, {280, y})) {
			return std::unexpected(static_cast<std::size_t>(id));
		}
	}
	if(!put_ascii(result.pages[3U].raster, font,
		"GAMEPLAY HUD BANNERS", {16, 8}, 11U)) {
		return std::unexpected(0U);
	}
	for(std::size_t index = 0U; index < 5U; ++index) {
		const auto id = add(NewClassicGameTextId::hud_banner_lose, index);
		const auto &schema = th03::app::new_classic_game_text_schema(id);
		const auto y = static_cast<std::int32_t>(40U + index * 40U);
		if(!put_ascii(result.pages[3U].raster, font, schema.identifier,
			{16, y}, 7U) ||
			!put_message(result, 3U, catalog, font, id, {280, y})) {
			return std::unexpected(static_cast<std::size_t>(id));
		}
	}
	if(!put_ascii(result.pages[4U].raster, font,
		"CONTINUE AND GAME OVER", {16, 8}, 11U)) {
		return std::unexpected(0U);
	}
	for(std::size_t index = 0U; index < 5U; ++index) {
		const auto id = add(NewClassicGameTextId::continue_question, index);
		const auto &schema = th03::app::new_classic_game_text_schema(id);
		const auto y = static_cast<std::int32_t>(40U + index * 56U);
		if(!put_ascii(result.pages[4U].raster, font, schema.identifier,
			{16, y}, 7U) ||
			!put_message(result, 4U, catalog, font, id, {280, y})) {
			return std::unexpected(static_cast<std::size_t>(id));
		}
	}
	constexpr std::size_t win_page_first = 5U;
	for(std::size_t character = 0U;
		character < th03::app::new_classic_win_quote_characters;
		++character) {
		for(std::size_t row = 0U;
			row < th03::app::new_classic_win_quote_blocks *
				th03::app::new_classic_win_quote_lines;
			++row) {
			const auto part = row / 17U;
			const auto page = win_page_first + (character * 2U) + part;
			const auto y = static_cast<std::int32_t>(
				32U + (row % 17U) * 21U
			);
			const auto block = row / th03::app::new_classic_win_quote_lines;
			const auto line = row % th03::app::new_classic_win_quote_lines;
			const auto id = th03::app::new_classic_win_quote_text_id(
				character, block, line
			);
			const auto &schema = th03::app::new_classic_game_text_schema(id);
			if(!put_ascii(result.pages[page].raster, font, schema.identifier,
				{8, y}, 7U) ||
				!put_message(result, page, catalog, font, id, {160, y})) {
				return std::unexpected(static_cast<std::size_t>(id));
			}
		}
	}
	constexpr std::size_t music_comment_page_first = win_page_first +
		(th03::app::new_classic_win_quote_characters * 2U);
	for(std::size_t track = 0U;
		track < th03::app::new_classic_music_comment_tracks;
		++track) {
		const auto page = music_comment_page_first + track;
		for(std::size_t line = 0U;
			line < th03::app::new_classic_music_comment_lines;
			++line) {
			const auto id = th03::app::new_classic_music_comment_text_id(
				track, line
			);
			const auto &schema = th03::app::new_classic_game_text_schema(id);
			const auto y = static_cast<std::int32_t>(28U + line * 18U);
			if(!put_ascii(result.pages[page].raster, font, schema.identifier,
				{8, y}, 7U) ||
				!put_message(result, page, catalog, font, id, {208, y})) {
				return std::unexpected(static_cast<std::size_t>(id));
			}
		}
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
	output << "{\n  \"format\": \"T3GTP001\",\n  \"locale\": \""
		<< locale << "\",\n  \"width\": 640,\n  \"height\": 400,\n"
		<< "  \"messages\": [\n";
	for(std::size_t index = 0U; index < preview.entries.size(); ++index) {
		const auto &entry = preview.entries[index];
		output << "    {\"id\": \"" << entry.identifier << "\", \"page\": \""
			<< entry.page << "\", \"x\": " << entry.x << ", \"y\": "
			<< entry.y << ", \"width\": " << entry.width << ", \"height\": "
			<< entry.height << ", \"exact_bytes\": " << entry.exact_bytes << '}';
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
	if(argc < 5 || argc > 7 || (argc & 1) == 0) {
		usage();
		return 2;
	}
	for(int index = 1; index < argc; index += 2) {
		const auto key = std::string_view(argv[index]);
		if(std::string_view(argv[index + 1]).empty()) {
			return 2;
		}
		if(key == "--font" && font_path.empty()) {
			font_path = fs::path(argv[index + 1]);
		} else if(key == "--output" && output_path.empty()) {
			output_path = fs::path(argv[index + 1]);
		} else if(key == "--game-text-pack" && pack_path.empty()) {
			pack_path = fs::path(argv[index + 1]);
		} else {
			usage();
			return 2;
		}
	}
	if(font_path.empty() || output_path.empty()) {
		usage();
		return 2;
	}
	try {
		auto font_bytes = read_file(
			font_path, th03::app::Pc98FontRomProvider::font_rom_size
		);
		if(!font_bytes) {
			return fail("font-read", static_cast<std::size_t>(font_bytes.error()));
		}
		auto font = th03::app::Pc98FontRomProvider::from_exact_font_rom(*font_bytes);
		if(!font) {
			return fail("font-validate", static_cast<std::size_t>(font.error().code));
		}
		const NewClassicGameTextCatalog *catalog =
			&th03::app::new_classic_builtin_english_game_text();
		std::optional<NewClassicGameTextCatalog> custom{};
		if(!pack_path.empty()) {
			auto loaded = th03::app::load_new_classic_game_text_pack(
				pack_path, catalog
			);
			if(!loaded) {
				return fail("game-text-pack",
					static_cast<std::size_t>(loaded.error().code));
			}
			custom.emplace(std::move(*loaded));
			catalog = &*custom;
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
			if(!write_ppm(
				output_path / (std::string(page.name) + ".ppm"), page.raster
			)) {
				return fail("output-write");
			}
		}
		if(!write_report(output_path / "preview.json", *preview, catalog->locale())) {
			return fail("report-write");
		}
		std::cout << "status=complete\nlocale=" << catalog->locale()
			<< "\nmessages=" << preview->entries.size() << '\n';
		return 0;
	} catch(const std::bad_alloc &) {
		return fail("allocation");
	}
}
