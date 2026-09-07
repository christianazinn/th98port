#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "th03/assets/decode.hpp"
#include "th03/present/cels.hpp"
#include "th03/present/pi.hpp"
#include "th03/present/raster.hpp"
#include "th03/present/superimpose.hpp"

namespace {

namespace fs = std::filesystem;
using th03::assets::BfntConsumerPolicy;
using th03::assets::Palette16Rgb4;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;

constexpr std::size_t maximum_input_bytes = 16U * 1024U * 1024U;

enum class ResourceType {
	pi,
	cdg,
	bfnt_superimpose,
	bfnt_gaiji,
};

struct Preview final {
	Raster raster{};
	std::uint16_t width = 0U;
	std::uint16_t height = 0U;
	std::size_t items = 0U;
};

class PartialOutput final {
public:
	explicit PartialOutput(fs::path path) : path_(std::move(path)) {}
	~PartialOutput()
	{
		if(active_) {
			std::error_code ignored;
			fs::remove(path_, ignored);
		}
	}
	void release() noexcept { active_ = false; }

private:
	fs::path path_{};
	bool active_ = true;
};

[[nodiscard]] int fail(std::string_view stage, std::size_t code = 0U)
{
	std::cerr << "status=error\nstage=" << stage << "\ncode=" << code << '\n';
	return 1;
}

void usage()
{
	std::cout
		<< "usage: th03-resource-preview --input <file> --type <type>\n"
		<< "       --output <new.bmp> [--palette <16-color.RGB>]\n"
		<< "       [--background <0..15>] [--discard-alpha]\n\n"
		<< "Types: pi, cdg, bfnt-superimpose, bfnt-gaiji. PI and BFNT use\n"
		<< "their inline palette when present; --palette supplies or overrides it.\n"
		<< "CDG requires --palette. --discard-alpha is valid only for CDG.\n";
}

[[nodiscard]] std::optional<ResourceType> parse_type(std::string_view value)
{
	if(value == "pi") { return ResourceType::pi; }
	if(value == "cdg") { return ResourceType::cdg; }
	if(value == "bfnt-superimpose") { return ResourceType::bfnt_superimpose; }
	if(value == "bfnt-gaiji") { return ResourceType::bfnt_gaiji; }
	return std::nullopt;
}

[[nodiscard]] std::optional<std::uint8_t> parse_index(std::string_view value)
{
	if(value.empty() || value.size() > 2U) {
		return std::nullopt;
	}
	std::uint8_t result = 0U;
	for(const char digit : value) {
		if(digit < '0' || digit > '9') {
			return std::nullopt;
		}
		result = static_cast<std::uint8_t>(
			(result * 10U) + static_cast<std::uint8_t>(digit - '0')
		);
	}
	return (result <= 15U) ? std::optional{result} : std::nullopt;
}

[[nodiscard]] std::expected<std::vector<std::byte>, std::size_t> read_file(
	const fs::path &path
)
{
	std::error_code system;
	const auto status = fs::status(path, system);
	if(system || !fs::is_regular_file(status)) {
		return std::unexpected(1U);
	}
	const auto size = fs::file_size(path, system);
	if(system || size == 0U || size > maximum_input_bytes ||
		size > static_cast<std::uintmax_t>(SIZE_MAX)) {
		return std::unexpected(2U);
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(3U);
	}
	std::vector<std::byte> result(static_cast<std::size_t>(size));
	input.read(reinterpret_cast<char *>(result.data()),
		static_cast<std::streamsize>(result.size()));
	if(!input || input.gcount() != static_cast<std::streamsize>(result.size())) {
		return std::unexpected(4U);
	}
	return result;
}

[[nodiscard]] bool install_palette(
	Raster &raster,
	const Palette16Rgb4 &palette
) noexcept
{
	for(std::size_t index = 0U; index < palette.colors.size(); ++index) {
		if(!raster.set_pending_palette_entry(
			static_cast<std::uint8_t>(index), palette.colors[index]
		)) {
			return false;
		}
	}
	raster.apply_pending_palette();
	return true;
}

[[nodiscard]] std::optional<std::pair<std::uint16_t, std::uint16_t>> atlas_size(
	std::uint16_t item_width,
	std::uint16_t item_height,
	std::size_t item_count
) noexcept
{
	if(item_width == 0U || item_height == 0U || item_count == 0U ||
		item_width > th03::present::graphics_width ||
		item_height > th03::present::graphics_height) {
		return std::nullopt;
	}
	const auto columns = std::min<std::size_t>(
		item_count, th03::present::graphics_width / item_width
	);
	const auto rows = (item_count + columns - 1U) / columns;
	const auto width = columns * item_width;
	const auto height = rows * item_height;
	if(width > th03::present::graphics_width ||
		height > th03::present::graphics_height) {
		return std::nullopt;
	}
	return std::pair{
		static_cast<std::uint16_t>(width),
		static_cast<std::uint16_t>(height),
	};
}

[[nodiscard]] bool initialize(
	Preview &preview,
	const Palette16Rgb4 &palette,
	std::uint8_t background
) noexcept
{
	return preview.raster.set_access_page(Page::zero) &&
		preview.raster.set_shown_page(Page::zero) &&
		install_palette(preview.raster, palette) &&
		preview.raster.clear({
			0, 0,
			th03::present::graphics_width,
			th03::present::graphics_height,
		}, background);
}

[[nodiscard]] std::expected<Preview, std::size_t> render_pi(
	std::span<const std::byte> bytes,
	const std::optional<Palette16Rgb4> &palette_override,
	std::uint8_t background
)
{
	auto image = th03::assets::decode_pi(bytes);
	if(!image) {
		return std::unexpected(100U + static_cast<std::size_t>(image.error().code));
	}
	const auto &palette = palette_override ? palette_override : image->palette;
	if(!palette || image->image.width == 0U || image->image.height == 0U ||
		image->image.width > th03::present::graphics_width ||
		image->image.height > th03::present::graphics_height) {
		return std::unexpected(110U);
	}
	Preview result{
		.width = image->image.width,
		.height = image->image.height,
		.items = 1U,
	};
	if(!initialize(result, *palette, background) ||
		!th03::present::pi_put_8(result.raster, 0, 0, *image)) {
		return std::unexpected(111U);
	}
	return result;
}

[[nodiscard]] std::expected<Preview, std::size_t> render_cdg(
	std::span<const std::byte> bytes,
	const std::optional<Palette16Rgb4> &palette,
	std::uint8_t background,
	bool discard_alpha
)
{
	if(!palette) {
		return std::unexpected(200U);
	}
	const auto alpha_policy = discard_alpha ?
		th03::assets::CdgAlphaPolicy::discard :
		th03::assets::CdgAlphaPolicy::preserve;
	auto file = th03::assets::decode_cdg(bytes, alpha_policy);
	if(!file) {
		return std::unexpected(210U + static_cast<std::size_t>(file.error().code));
	}
	if(file->cels.empty()) {
		return std::unexpected(220U);
	}
	const auto item_width = file->cels.front().pixel_width;
	const auto item_height = file->cels.front().pixel_height;
	const auto size = atlas_size(item_width, item_height, file->cels.size());
	if(!size || !std::ranges::all_of(file->cels, [=](const auto &cel) {
		return cel.pixel_width == item_width && cel.pixel_height == item_height;
	})) {
		return std::unexpected(221U);
	}
	Preview result{
		.width = size->first,
		.height = size->second,
		.items = file->cels.size(),
	};
	if(!initialize(result, *palette, background)) {
		return std::unexpected(222U);
	}
	const auto columns = static_cast<std::size_t>(result.width / item_width);
	for(std::size_t index = 0U; index < file->cels.size(); ++index) {
		const auto x = static_cast<std::int32_t>((index % columns) * item_width);
		const auto y = static_cast<std::int32_t>((index / columns) * item_height);
		const auto rendered = discard_alpha ?
			th03::present::cdg_put_noalpha_8(result.raster, x, y, file->cels[index]) :
			th03::present::cdg_put_8(result.raster, x, y, file->cels[index]);
		if(!rendered) {
			return std::unexpected(223U + index);
		}
	}
	return result;
}

[[nodiscard]] std::expected<Preview, std::size_t> render_bfnt(
	std::span<const std::byte> bytes,
	BfntConsumerPolicy consumer,
	const std::optional<Palette16Rgb4> &palette_override,
	std::uint8_t background
)
{
	auto sheet = th03::assets::decode_bfnt(bytes, consumer);
	if(!sheet) {
		return std::unexpected(300U + static_cast<std::size_t>(sheet.error().code));
	}
	const auto &palette = palette_override ? palette_override : sheet->palette;
	if(!palette) {
		return std::unexpected(310U);
	}
	const auto size = atlas_size(
		sheet->header.pixel_width,
		sheet->header.pixel_height,
		sheet->glyphs.size()
	);
	if(!size) {
		return std::unexpected(311U);
	}
	Preview result{
		.width = size->first,
		.height = size->second,
		.items = sheet->glyphs.size(),
	};
	if(!initialize(result, *palette, background)) {
		return std::unexpected(312U);
	}
	const auto columns = static_cast<std::size_t>(
		result.width / sheet->header.pixel_width
	);
	if(consumer == BfntConsumerPolicy::superimpose) {
		auto prepared = th03::present::prepare_superimpose_sheet(*sheet);
		if(!prepared) {
			return std::unexpected(313U);
		}
		for(std::size_t index = 0U; index < sheet->glyphs.size(); ++index) {
			const Point position{
				static_cast<std::int32_t>(
					(index % columns) * sheet->header.pixel_width
				),
				static_cast<std::int32_t>(
					(index / columns) * sheet->header.pixel_height
				),
			};
			if(!th03::present::superimpose_put(
				result.raster, *prepared, position, sheet->glyphs[index].code
			)) {
				return std::unexpected(314U + index);
			}
		}
		return result;
	}
	for(std::size_t index = 0U; index < sheet->glyphs.size(); ++index) {
		const auto *monochrome = std::get_if<th03::assets::MonochromeBitmap1>(
			&sheet->glyphs[index].pixels
		);
		if(monochrome == nullptr) {
			return std::unexpected(320U + index);
		}
		const auto stride = static_cast<std::size_t>(sheet->header.pixel_width / 8U);
		const auto origin_x = (index % columns) * sheet->header.pixel_width;
		const auto origin_y = (index / columns) * sheet->header.pixel_height;
		for(std::size_t y = 0U; y < sheet->header.pixel_height; ++y) {
			for(std::size_t x = 0U; x < sheet->header.pixel_width; ++x) {
				const auto byte = monochrome->bits[(y * stride) + (x / 8U)];
				const auto mask = static_cast<std::uint8_t>(0x80U >> (x & 7U));
				if((byte & mask) != 0U && !result.raster.plot({
					static_cast<std::int32_t>(origin_x + x),
					static_cast<std::int32_t>(origin_y + y),
				}, 15U)) {
					return std::unexpected(321U + index);
				}
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

[[nodiscard]] bool write_bmp(const fs::path &path, const Preview &preview)
{
	constexpr std::size_t header_bytes = 14U + 40U + (16U * 4U);
	const auto packed_row_bytes = (static_cast<std::size_t>(preview.width) + 1U) / 2U;
	const auto row_bytes = (packed_row_bytes + 3U) & ~std::size_t{3U};
	const auto image_bytes = row_bytes * preview.height;
	if(image_bytes > std::numeric_limits<std::uint32_t>::max() - header_bytes) {
		return false;
	}
	std::array<std::uint8_t, header_bytes> header{};
	header[0U] = 'B';
	header[1U] = 'M';
	u32(header, 2U, static_cast<std::uint32_t>(header_bytes + image_bytes));
	u32(header, 10U, static_cast<std::uint32_t>(header_bytes));
	u32(header, 14U, 40U);
	u32(header, 18U, preview.width);
	u32(header, 22U, preview.height);
	u16(header, 26U, 1U);
	u16(header, 28U, 4U);
	u32(header, 34U, static_cast<std::uint32_t>(image_bytes));
	u32(header, 46U, 16U);
	const auto palette = preview.raster.applied_palette();
	for(std::size_t index = 0U; index < palette.colors.size(); ++index) {
		const auto base = 54U + (index * 4U);
		header[base + 0U] = static_cast<std::uint8_t>(palette.colors[index].b * 0x11U);
		header[base + 1U] = static_cast<std::uint8_t>(palette.colors[index].g * 0x11U);
		header[base + 2U] = static_cast<std::uint8_t>(palette.colors[index].r * 0x11U);
	}
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if(!output) {
		return false;
	}
	output.write(reinterpret_cast<const char *>(header.data()),
		static_cast<std::streamsize>(header.size()));
	const auto pixels = preview.raster.shown_graphics();
	std::vector<std::uint8_t> row(row_bytes, 0U);
	for(std::size_t y = preview.height; y-- > 0U;) {
		std::ranges::fill(row, std::uint8_t{0U});
		for(std::size_t x = 0U; x < preview.width; x += 2U) {
			const auto left = pixels[y * th03::present::graphics_width + x];
			const auto right = (x + 1U < preview.width) ?
				pixels[y * th03::present::graphics_width + x + 1U] : 0U;
			row[x / 2U] = static_cast<std::uint8_t>((left << 4U) | right);
		}
		output.write(reinterpret_cast<const char *>(row.data()),
			static_cast<std::streamsize>(row.size()));
	}
	return static_cast<bool>(output);
}

} // namespace

int main(int argc, char **argv)
{
	if(argc == 2 && std::string_view(argv[1]) == "--help") {
		usage();
		return 0;
	}
	fs::path input_path{};
	fs::path output_path{};
	fs::path palette_path{};
	std::optional<ResourceType> type{};
	std::uint8_t background = 0U;
	bool discard_alpha = false;
	for(int index = 1; index < argc; ++index) {
		const auto key = std::string_view(argv[index]);
		if(key == "--discard-alpha") {
			if(discard_alpha) {
				return 2;
			}
			discard_alpha = true;
			continue;
		}
		if(index + 1 >= argc) {
			usage();
			return 2;
		}
		const auto value = std::string_view(argv[++index]);
		if(value.empty()) {
			return 2;
		}
		if(key == "--input" && input_path.empty()) {
			input_path = fs::path(value);
		} else if(key == "--output" && output_path.empty()) {
			output_path = fs::path(value);
		} else if(key == "--palette" && palette_path.empty()) {
			palette_path = fs::path(value);
		} else if(key == "--type" && !type) {
			type = parse_type(value);
			if(!type) { return 2; }
		} else if(key == "--background") {
			const auto parsed = parse_index(value);
			if(!parsed) { return 2; }
			background = *parsed;
		} else {
			usage();
			return 2;
		}
	}
	if(input_path.empty() || output_path.empty() || !type ||
		(discard_alpha && *type != ResourceType::cdg)) {
		usage();
		return 2;
	}
	try {
		std::error_code system;
		const auto absolute_output = fs::absolute(output_path, system);
		if(system || fs::exists(absolute_output, system) || system) {
			return fail("output-exists", static_cast<std::size_t>(system.value()));
		}
		auto bytes = read_file(input_path);
		if(!bytes) {
			return fail("input-read", bytes.error());
		}
		std::optional<Palette16Rgb4> palette{};
		if(!palette_path.empty()) {
			auto palette_bytes = read_file(palette_path);
			if(!palette_bytes) {
				return fail("palette-read", palette_bytes.error());
			}
			auto decoded = th03::assets::decode_rgb48(*palette_bytes);
			if(!decoded) {
				return fail("palette-decode", static_cast<std::size_t>(
					decoded.error().code
				));
			}
			palette = *decoded;
		}
		std::expected<Preview, std::size_t> preview = std::unexpected(0U);
		switch(*type) {
		case ResourceType::pi:
			preview = render_pi(*bytes, palette, background);
			break;
		case ResourceType::cdg:
			preview = render_cdg(*bytes, palette, background, discard_alpha);
			break;
		case ResourceType::bfnt_superimpose:
			preview = render_bfnt(
				*bytes, BfntConsumerPolicy::superimpose, palette, background
			);
			break;
		case ResourceType::bfnt_gaiji:
			preview = render_bfnt(
				*bytes, BfntConsumerPolicy::gaiji, palette, background
			);
			break;
		}
		if(!preview) {
			return fail("resource-render", preview.error());
		}
		fs::create_directories(absolute_output.parent_path(), system);
		if(system || !fs::is_directory(absolute_output.parent_path())) {
			return fail("output-parent", static_cast<std::size_t>(system.value()));
		}
		const auto partial_path = fs::path(absolute_output.string() + ".partial-T3RSP001");
		if(fs::exists(partial_path, system) || system) {
			return fail("partial-output-exists", static_cast<std::size_t>(system.value()));
		}
		PartialOutput partial(partial_path);
		if(!write_bmp(partial_path, *preview)) {
			return fail("output-write");
		}
		fs::rename(partial_path, absolute_output, system);
		if(system) {
			return fail("output-publish", static_cast<std::size_t>(system.value()));
		}
		partial.release();
		std::cout << "status=complete\nwidth=" << preview->width
			<< "\nheight=" << preview->height << "\nitems=" << preview->items
			<< "\noutput=" << absolute_output.string() << '\n';
		return 0;
	} catch(const std::bad_alloc &) {
		return fail("allocation");
	} catch(const fs::filesystem_error &error) {
		return fail("filesystem", static_cast<std::size_t>(error.code().value()));
	}
}
