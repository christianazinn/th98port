#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <span>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "th03/app/authentic_text.hpp"
#include "th03/app/resources.hpp"
#include "th03/assets/decode.hpp"

namespace {

using th03::app::AuthenticTextErrorCode;
using th03::app::Pc98FontRomProvider;
using th03::app::PreparedGameftGaiji;
using th03::assets::BfntConsumerPolicy;
using th03::assets::BfntGlyph;
using th03::assets::BfntSheet;
using th03::assets::MonochromeBitmap1;
using th03::assets::PlanarBitmap4;
using th03::present::GaijiGlyph;
using th03::present::Tram;

void expect(bool condition, std::string_view message)
{
	if(!condition) {
		std::fprintf(
			stderr,
			"FAIL: %.*s\n",
			static_cast<int>(message.size()),
			message.data()
		);
		std::exit(1);
	}
}

[[nodiscard]] BfntSheet gameft_sheet()
{
	BfntSheet sheet{};
	sheet.consumer = BfntConsumerPolicy::gaiji;
	sheet.header.color_flags = 0U;
	sheet.header.version = 0xA5U;
	sheet.header.pixel_width = th03::present::gaiji_width;
	sheet.header.pixel_height = th03::present::gaiji_height;
	sheet.header.first_code = 0U;
	sheet.header.last_code = 255U;
	sheet.extended_header.bytes = {std::byte{0x12U}, std::byte{0x34U}};
	sheet.glyphs.reserve(th03::present::gaiji_count);
	for(std::size_t index = 0U; index < th03::present::gaiji_count; ++index) {
		MonochromeBitmap1 bitmap{};
		bitmap.bits.resize(th03::present::gaiji_glyph_bytes);
		for(std::size_t byte = 0U; byte < bitmap.bits.size(); ++byte) {
			bitmap.bits[byte] = static_cast<std::uint8_t>(
				(index + (byte * 17U)) & 0xFFU
			);
		}
		sheet.glyphs.push_back(BfntGlyph{
			static_cast<std::uint16_t>(index),
			std::move(bitmap),
		});
	}
	return sheet;
}

[[nodiscard]] std::size_t jis_rom_offset(std::uint16_t canonical)
{
	const auto first = static_cast<std::uint8_t>(canonical >> 8U);
	const auto second = static_cast<std::uint8_t>(canonical & 0x00FFU);
	return 0x1800U +
		(static_cast<std::size_t>(first - 0x21U) * 96U * 32U) +
		(static_cast<std::size_t>(second - 0x20U) * 32U);
}

void test_exact_font_rom_layout()
{
	std::vector<std::byte> short_rom(Pc98FontRomProvider::font_rom_size - 1U);
	auto invalid = Pc98FontRomProvider::from_exact_font_rom(short_rom);
	expect(
		!invalid && invalid.error() == th03::app::AuthenticTextError{
			AuthenticTextErrorCode::font_rom_size,
			short_rom.size(),
		},
		"FONT.ROM one byte short is rejected with its exact size"
	);
	std::vector<std::byte> long_rom(Pc98FontRomProvider::font_rom_size + 1U);
	invalid = Pc98FontRomProvider::from_exact_font_rom(long_rom);
	expect(
		!invalid && invalid.error().code == AuthenticTextErrorCode::font_rom_size &&
			invalid.error().index_or_size == long_rom.size(),
		"FONT.ROM one byte long is rejected"
	);

	std::vector<std::byte> rom(Pc98FontRomProvider::font_rom_size);
	auto blank = Pc98FontRomProvider::from_exact_font_rom(rom);
	expect(
		!blank && blank.error() == th03::app::AuthenticTextError{
			AuthenticTextErrorCode::font_rom_content,
			0x800U,
		},
		"exact-size FONT.ROM with a blank ANK domain is rejected"
	);
	rom[0x800U] = std::byte{0x80U};
	rom[0x800U + 15U] = std::byte{0x01U};
	rom[0x800U + (255U * 16U)] = std::byte{0xA5U};
	rom[0x800U + (255U * 16U) + 15U] = std::byte{0x5AU};
	blank = Pc98FontRomProvider::from_exact_font_rom(rom);
	expect(
		!blank && blank.error() == th03::app::AuthenticTextError{
			AuthenticTextErrorCode::font_rom_content,
			0x1800U,
		},
		"exact-size FONT.ROM with a blank JIS domain is rejected"
	);
	for(const auto [canonical, left, right] : {
		std::array<std::uint16_t, 3U>{0x2120U, 0x8010U, 0x0120U},
		std::array<std::uint16_t, 3U>{0x7C7FU, 0xA530U, 0x5A40U},
	}) {
		const auto glyph_offset = jis_rom_offset(canonical);
		rom[glyph_offset] = static_cast<std::byte>(left >> 8U);
		rom[glyph_offset + 15U] = static_cast<std::byte>(left & 0x00FFU);
		rom[glyph_offset + 16U] = static_cast<std::byte>(right >> 8U);
		rom[glyph_offset + 31U] = static_cast<std::byte>(right & 0x00FFU);
	}

	const auto created = Pc98FontRomProvider::from_exact_font_rom(rom);
	expect(created.has_value(), "exact-size FONT.ROM creates a provider view");
	const auto ank_zero = created->glyph(0U);
	const auto ank_last = created->glyph(255U);
	expect(
		ank_zero && ank_zero->rows[0U] == 0x80U &&
			ank_zero->rows[15U] == 0x01U && ank_last &&
			ank_last->rows[0U] == 0xA5U && ank_last->rows[15U] == 0x5AU,
		"FONT.ROM ANK lookup locks code and row boundaries at offset 0x800"
	);

	const auto jis_first = created->jis_glyph(0x2120U);
	const auto jis_last = created->jis_glyph(0x7C7FU);
	expect(
		jis_first && jis_first->row_halves[0U] == 0x80U &&
			jis_first->row_halves[1U] == 0x01U &&
			jis_first->row_halves[30U] == 0x10U &&
			jis_first->row_halves[31U] == 0x20U,
		"first JIS glyph converts split FONT.ROM halves to interleaved rows"
	);
	expect(
		jis_last && jis_last->row_halves[0U] == 0xA5U &&
			jis_last->row_halves[1U] == 0x5AU &&
			jis_last->row_halves[30U] == 0x30U &&
			jis_last->row_halves[31U] == 0x40U,
		"last 96x92 JIS glyph stays inside the exact FONT.ROM boundary"
	);
	expect(
		!created->jis_glyph(0x2020U) && !created->jis_glyph(0x7D20U) &&
			!created->jis_glyph(0x211FU) && !created->jis_glyph(0x2180U),
		"JIS lookup rejects every edge outside the 92x96 FONT.ROM domain"
	);
}

void expect_gameft_error(
	const BfntSheet &sheet,
	AuthenticTextErrorCode code,
	std::size_t index,
	std::string_view message
)
{
	const auto result = PreparedGameftGaiji::prepare(sheet);
	expect(
		!result && result.error() == th03::app::AuthenticTextError{code, index},
		message
	);
}

void test_gameft_validation()
{
	{
		auto sheet = gameft_sheet();
		sheet.consumer = BfntConsumerPolicy::superimpose;
		expect_gameft_error(
			sheet,
			AuthenticTextErrorCode::gameft_header,
			0U,
			"GAMEFT consumer policy is validated"
		);
	}
	for(const auto mutate : {
		0U,
		1U,
		2U,
		3U,
		4U,
	}) {
		auto sheet = gameft_sheet();
		switch(mutate) {
		case 0U: sheet.header.color_flags = 1U; break;
		case 1U: sheet.header.pixel_width = 15U; break;
		case 2U: sheet.header.pixel_height = 17U; break;
		case 3U: sheet.header.first_code = 1U; break;
		case 4U: sheet.header.last_code = 254U; break;
		default: break;
		}
		expect_gameft_error(
			sheet,
			AuthenticTextErrorCode::gameft_header,
			0U,
			"every native GAMEFT header domain is validated"
		);
	}
	{
		auto sheet = gameft_sheet();
		sheet.glyphs.pop_back();
		expect_gameft_error(
			sheet,
			AuthenticTextErrorCode::gameft_glyph_count,
			255U,
			"GAMEFT requires all 256 gaiji glyphs"
		);
	}
	{
		auto sheet = gameft_sheet();
		sheet.glyphs[255U].code = 254U;
		expect_gameft_error(
			sheet,
			AuthenticTextErrorCode::gameft_glyph_code,
			255U,
			"late GAMEFT glyph code mismatch is rejected"
		);
	}
	{
		auto sheet = gameft_sheet();
		sheet.glyphs[255U].pixels = PlanarBitmap4{};
		expect_gameft_error(
			sheet,
			AuthenticTextErrorCode::gameft_glyph_format,
			255U,
			"late color GAMEFT glyph is rejected"
		);
	}
	{
		auto sheet = gameft_sheet();
		auto &bitmap = std::get<MonochromeBitmap1>(sheet.glyphs[255U].pixels);
		bitmap.row_order = th03::assets::PlanarRowOrder::bottom_to_top;
		expect_gameft_error(
			sheet,
			AuthenticTextErrorCode::gameft_glyph_format,
			255U,
			"late GAMEFT row order mismatch is rejected"
		);
	}
	{
		auto sheet = gameft_sheet();
		auto &bitmap = std::get<MonochromeBitmap1>(sheet.glyphs[255U].pixels);
		bitmap.bits.pop_back();
		expect_gameft_error(
			sheet,
			AuthenticTextErrorCode::gameft_glyph_size,
			255U,
			"late GAMEFT glyph size mismatch is rejected"
		);
	}
}

void test_gameft_transaction_and_owned_copy()
{
	auto sheet = gameft_sheet();
	const auto expected_zero = std::get<MonochromeBitmap1>(
		sheet.glyphs[0U].pixels
	).bits;
	const auto expected_last = std::get<MonochromeBitmap1>(
		sheet.glyphs[255U].pixels
	).bits;
	auto prepared_result = PreparedGameftGaiji::prepare(sheet);
	expect(prepared_result.has_value(), "valid GAMEFT prepares fixed gaiji table");
	auto prepared = std::move(*prepared_result);

	for(auto &glyph : sheet.glyphs) {
		std::get<MonochromeBitmap1>(glyph.pixels).bits.assign(
			th03::present::gaiji_glyph_bytes,
			0xEEU
		);
	}
	sheet.glyphs.clear();

	Tram tram;
	std::array<std::uint8_t, th03::present::gaiji_glyph_bytes> seed{};
	seed.fill(0xCCU);
	for(std::size_t index = 0U; index < th03::present::gaiji_count; ++index) {
		expect(
			tram.load_gaiji(static_cast<std::uint8_t>(index), seed),
			"seed full gaiji bank before transactional install"
		);
	}
	prepared.install(tram);
	expect(
		std::ranges::equal(tram.gaiji(0U).row_halves, expected_zero) &&
			std::ranges::equal(tram.gaiji(255U).row_halves, expected_last),
		"prepared GAMEFT owns its copy and installs first and last glyphs"
	);
	for(std::size_t index = 0U; index < th03::present::gaiji_count; ++index) {
		const auto expected_first = static_cast<std::uint8_t>(index & 0xFFU);
		expect(
			tram.gaiji(static_cast<std::uint8_t>(index)).row_halves[0U] ==
				expected_first,
			"prepared GAMEFT transaction replaces every gaiji slot"
		);
	}
}

[[nodiscard]] std::vector<std::byte> read_binary(const char *path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if(!file) {
		return {};
	}
	const auto end = file.tellg();
	if(end <= 0) {
		return {};
	}
	std::vector<char> characters(static_cast<std::size_t>(end));
	file.seekg(0, std::ios::beg);
	file.read(characters.data(), static_cast<std::streamsize>(characters.size()));
	if(!file) {
		return {};
	}
	std::vector<std::byte> bytes(characters.size());
	for(std::size_t index = 0U; index < characters.size(); ++index) {
		bytes[index] = static_cast<std::byte>(
			static_cast<unsigned char>(characters[index])
		);
	}
	return bytes;
}

void test_optional_local_gameft(const char *path)
{
	const auto bytes = read_binary(path);
	const auto spec = th03::app::main_resource_spec({
		th03::app::MainResourceRole::game_font,
	});
	expect(spec.has_value(), "GAMEFT manifest role resolves");
	expect(
		bytes.size() == spec->expected_bytes,
		"local GAMEFT has the exact manifest size"
	);
	expect(
		th03::app::main_resource_fnv1a64(bytes) == spec->expected_fnv1a64,
		"local GAMEFT has the accepted manifest fingerprint"
	);
	const auto decoded = th03::assets::decode_bfnt(
		bytes,
		BfntConsumerPolicy::gaiji
	);
	expect(decoded.has_value(), "local GAMEFT decodes as a gaiji BFNT");
	const auto prepared = PreparedGameftGaiji::prepare(*decoded);
	expect(prepared.has_value(), "local GAMEFT prepares all 256 gaiji");
	Tram tram;
	prepared->install(tram);
	expect(
		std::ranges::equal(
			tram.gaiji(0U).row_halves,
			std::get<MonochromeBitmap1>(decoded->glyphs[0U].pixels).bits
		) && std::ranges::equal(
			tram.gaiji(255U).row_halves,
			std::get<MonochromeBitmap1>(decoded->glyphs[255U].pixels).bits
		),
		"local GAMEFT installs both bank endpoints"
	);
}

} // namespace

int main(int argc, char **argv)
{
	test_exact_font_rom_layout();
	test_gameft_validation();
	test_gameft_transaction_and_owned_copy();
	if(argc == 2) {
		test_optional_local_gameft(argv[1]);
	} else if(argc != 1) {
		expect(false, "expected zero arguments or one local GAMEFT.BFT path");
	}
	std::fputs("th03-app-authentic-text tests passed\n", stdout);
	return 0;
}
