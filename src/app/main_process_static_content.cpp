#include "main_process_static_content_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "th03/assets/decode.hpp"
#include "th03/exact/trig.hpp"
#include "th03/formats/enedat.hpp"
#include "th03/app/new_classic_game_text.hpp"
#include "th03/present/graphics_text.hpp"

namespace th03::app {
namespace {

[[nodiscard]] MainProcessError error(
	MainProcessErrorCode code,
	std::optional<MainResourceKey> resource = std::nullopt
)
{
	return {.code = code, .resource = resource};
}

[[nodiscard]] MainProcessError asset_error(
	MainResourceKey resource,
	assets::AssetError asset
)
{
	auto result = error(MainProcessErrorCode::asset_decode_failed, resource);
	result.asset_error.emplace(std::move(asset));
	return result;
}

[[nodiscard]] bool same_resource_spec(
	const MainResourceSpec &actual,
	const MainResourceSpec &canonical
) noexcept
{
	return actual.key == canonical.key && actual.scene == canonical.scene &&
		actual.archive == canonical.archive &&
		actual.decoder == canonical.decoder &&
		actual.logical_role == canonical.logical_role &&
		actual.filename == canonical.filename &&
		actual.expected_bytes == canonical.expected_bytes &&
		actual.expected_fnv1a64 == canonical.expected_fnv1a64;
}

[[nodiscard]] std::span<const std::uint8_t> trim_ascii_spaces(
	std::span<const std::uint8_t> text
) noexcept
{
	while(!text.empty() && text.front() == static_cast<std::uint8_t>(' ')) {
		text = text.subspan(1U);
	}
	while(!text.empty() && text.back() == static_cast<std::uint8_t>(' ')) {
		text = text.first(text.size() - 1U);
	}
	return text;
}

[[nodiscard]] bool copy_raster_word_to_cels(
	assets::BfntSheet &sheet,
	std::size_t first_cel,
	std::size_t cel_count,
	std::span<const std::uint8_t> text,
	const Pc98FontRomProvider &font,
	present::Index4 foreground,
	bool white_shadow
)
{
	if(sheet.consumer != assets::BfntConsumerPolicy::superimpose ||
		sheet.header.pixel_width == 0U ||
		(sheet.header.pixel_width % 8U) != 0U ||
		sheet.header.pixel_height > present::graphics_height ||
		sheet.transparent_color != 0U || cel_count == 0U ||
		first_cel > sheet.glyphs.size() ||
		cel_count > (sheet.glyphs.size() - first_cel)) {
		return false;
	}
	text = trim_ascii_spaces(text);
	if(text.empty()) {
		return false;
	}
	const auto prepared = present::prepare_graphics_text_8x16(text, font);
	if(!prepared) {
		return false;
	}
	const auto total_width = static_cast<std::uint32_t>(
		sheet.header.pixel_width
	) * cel_count;
	if(total_width > present::graphics_width ||
		prepared->pixel_width() > total_width ||
		sheet.header.pixel_height < present::jis_height) {
		return false;
	}

	present::Raster raster{};
	if(!raster.set_raster_mode(present::RasterMode::lines_400)) {
		return false;
	}
	const auto left = static_cast<std::int32_t>(
		(total_width - prepared->pixel_width()) / 2U
	);
	const auto top = static_cast<std::int32_t>(
		(sheet.header.pixel_height - present::jis_height) / 2U
	);
	if(white_shadow && !present::graphics_text_put_8x16(
		raster, {left + 1, top + 1}, *prepared,
		{15U, present::GraphicsTextWeight::black, 0U}
	)) {
		return false;
	}
	if(!present::graphics_text_put_8x16(
		raster, {left, top}, *prepared,
		{foreground, present::GraphicsTextWeight::black, 0U}
	)) {
		return false;
	}

	const auto stride = static_cast<std::size_t>(
		sheet.header.pixel_width / 8U
	);
	const auto plane_bytes = stride * sheet.header.pixel_height;
	for(std::size_t cel = 0U; cel < cel_count; ++cel) {
		auto *pixels = std::get_if<assets::PlanarBitmap4>(
			&sheet.glyphs[first_cel + cel].pixels
		);
		if(pixels == nullptr ||
			pixels->row_order != assets::PlanarRowOrder::top_to_bottom ||
			pixels->bit_order !=
				assets::PlanarBitOrder::most_significant_bit_left) {
			return false;
		}
		for(auto &plane : pixels->color_planes) {
			plane.assign(plane_bytes, 0U);
		}
		for(std::size_t plane = 0U;
			plane < present::planar_plane_count; ++plane) {
			for(std::size_t y = 0U; y < sheet.header.pixel_height; ++y) {
				for(std::size_t byte = 0U; byte < stride; ++byte) {
					const auto source = raster.planar_byte(
						present::Page::zero,
						static_cast<present::PlanarPlane>(plane),
						static_cast<std::uint16_t>((cel * stride) + byte),
						static_cast<std::uint16_t>(y)
					);
					if(!source) {
						return false;
					}
					pixels->color_planes[plane][(y * stride) + byte] = *source;
				}
			}
		}
	}
	return true;
}

[[nodiscard]] bool same_message(
	const NewClassicGameTextCatalog &left,
	const NewClassicGameTextCatalog &right,
	NewClassicGameTextId id
) noexcept
{
	return std::ranges::equal(left.bytes(id), right.bytes(id));
}

[[nodiscard]] bool localize_hud_sheets(
	assets::BfntSheet &lose,
	assets::BfntSheet &round,
	const Pc98FontRomProvider &font,
	const NewClassicGameTextCatalog &text
)
{
	const auto &english = new_classic_builtin_english_game_text();
	struct Word {
		NewClassicGameTextId id;
		assets::BfntSheet *sheet;
		std::size_t first_cel;
		std::size_t cel_count;
		present::Index4 color;
		bool white_shadow;
	};
	const std::array words{
		Word{NewClassicGameTextId::hud_banner_lose, &lose, 0U, 2U, 10U, true},
		Word{NewClassicGameTextId::hud_banner_win, &lose, 2U, 2U, 8U, true},
		Word{NewClassicGameTextId::hud_banner_stage, &round, 0U, 4U, 15U, false},
		Word{NewClassicGameTextId::hud_banner_round, &round, 4U, 4U, 15U, false},
		Word{NewClassicGameTextId::hud_banner_start, &round, 8U, 4U, 15U, false},
	};
	for(const auto &word : words) {
		// The source artwork is already the built-in English spelling. Retain its
		// cursive/stylized pixels exactly until a locale actually substitutes it.
		if(same_message(text, english, word.id)) {
			continue;
		}
		if(!copy_raster_word_to_cels(
			*word.sheet, word.first_cel, word.cel_count, text.bytes(word.id),
			font, word.color, word.white_shadow
		)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] std::expected<
	const LoadedMainResource *,
	MainProcessError
> find_static_resource(
	std::span<const LoadedMainResource> resources,
	MainResourceKey key,
	bool authenticate_resource_bytes
)
{
	const LoadedMainResource *found = nullptr;
	for(const auto &resource : resources) {
		if(resource.spec.key == key) {
			if(found != nullptr) {
				return std::unexpected(error(
					MainProcessErrorCode::resource_set_mismatch,
					key
				));
			}
			found = &resource;
		}
	}
	if(found == nullptr) {
		return std::unexpected(error(
			MainProcessErrorCode::resource_set_mismatch,
			key
		));
	}
	if(!found->bytes) {
		return std::unexpected(error(
			MainProcessErrorCode::resource_bytes_missing,
			key
		));
	}
	const auto canonical = main_resource_spec(key);
	if(!canonical || !same_resource_spec(found->spec, *canonical)) {
		return std::unexpected(error(
			MainProcessErrorCode::resource_set_mismatch,
			key
		));
	}
	if(found->bytes->size() != canonical->expected_bytes ||
		(authenticate_resource_bytes && main_resource_fnv1a64(
			*found->bytes
		) != canonical->expected_fnv1a64)) {
		return std::unexpected(error(
			MainProcessErrorCode::resource_integrity_failed,
			key
		));
	}
	return found;
}

} // namespace

MainProcessStaticContent::MainProcessStaticContent(
	MainCompiledContent compiled,
	std::span<const std::byte> font_rom
) :
	compiled_(std::move(compiled)),
	font_rom_(font_rom.begin(), font_rom.end())
{}

const MainCompiledContent &MainProcessStaticContent::compiled() const noexcept
{
	return compiled_;
}

const Pc98FontRomProvider &MainProcessStaticContent::font() const noexcept
{
	return *font_provider_;
}

const PreparedGameftGaiji &
MainProcessStaticContent::gameft_gaiji() const noexcept
{
	return *gameft_gaiji_;
}

const GameContent &MainProcessStaticContent::game_content() const noexcept
{
	return game_content_;
}

const assets::BfntSheet &MainProcessStaticContent::zikicw_sheet() const noexcept
{
	return zikicw_sheet_;
}

const present::PreparedSuperimposeSheet &
MainProcessStaticContent::lose() const noexcept
{
	return *prepared_lose_;
}

const present::PreparedSuperimposeSheet &
MainProcessStaticContent::round() const noexcept
{
	return *prepared_round_;
}

const present::PreparedSuperimposeSheet &
MainProcessStaticContent::zikicw() const noexcept
{
	return *prepared_zikicw_;
}

present::StaffFlakeTrigTables MainProcessStaticContent::trig() const noexcept
{
	return {sine_q8_, cosine_q8_};
}

std::expected<
	std::shared_ptr<const MainProcessStaticContent>,
	MainProcessError
> MainProcessStaticContentAccess::create(
	std::span<const LoadedMainResource> resources,
	MainCompiledContent compiled,
	std::span<const std::byte> font_rom,
	bool authenticate_resource_bytes,
	const NewClassicGameTextCatalog *localized_hud_text
)
{
	constexpr std::array static_keys{
		MainResourceKey{.role = MainResourceRole::enemy_scripts},
		MainResourceKey{.role = MainResourceRole::game_font},
		MainResourceKey{.role = MainResourceRole::hud_lose_sheet},
		MainResourceKey{.role = MainResourceRole::hud_round_sheet},
		MainResourceKey{.role = MainResourceRole::hud_result_sheet},
	};
	std::array<const LoadedMainResource *, static_keys.size()> selected{};
	for(std::size_t index = 0U; index < static_keys.size(); ++index) {
		const auto found = find_static_resource(
			resources,
			static_keys[index],
			authenticate_resource_bytes
		);
		if(!found) {
			return std::unexpected(found.error());
		}
		selected[index] = *found;
	}

	auto candidate = std::shared_ptr<MainProcessStaticContent>(
		new MainProcessStaticContent(std::move(compiled), font_rom)
	);
	const auto font = Pc98FontRomProvider::from_exact_font_rom(
		candidate->font_rom_
	);
	if(!font) {
		return std::unexpected(error(MainProcessErrorCode::font_rom_failed));
	}
	candidate->font_provider_.emplace(*font);

	auto game_content = formats::parse_enedat(*selected[0U]->bytes);
	if(!game_content) {
		auto result = error(
			MainProcessErrorCode::enedat_decode_failed,
			static_keys[0U]
		);
		result.core_error.emplace(std::move(game_content.error()));
		return std::unexpected(std::move(result));
	}
	candidate->game_content_ = std::move(*game_content);

	auto gameft = assets::decode_bfnt(
		*selected[1U]->bytes,
		assets::BfntConsumerPolicy::gaiji
	);
	if(!gameft) {
		return std::unexpected(asset_error(
			static_keys[1U],
			std::move(gameft.error())
		));
	}
	const auto gaiji = PreparedGameftGaiji::prepare(*gameft);
	if(!gaiji) {
		return std::unexpected(error(
			MainProcessErrorCode::gameft_prepare_failed,
			static_keys[1U]
		));
	}
	candidate->gameft_gaiji_.emplace(*gaiji);

	std::array<assets::BfntSheet, 3U> sheets{};
	for(std::size_t index = 0U; index < sheets.size(); ++index) {
		auto decoded = assets::decode_bfnt(
			*selected[index + 2U]->bytes,
			assets::BfntConsumerPolicy::superimpose
		);
		if(!decoded) {
			return std::unexpected(asset_error(
				static_keys[index + 2U],
				std::move(decoded.error())
			));
		}
		sheets[index] = std::move(*decoded);
	}
	if(localized_hud_text && !localize_hud_sheets(
		sheets[0U], sheets[1U], *candidate->font_provider_,
		*localized_hud_text
	)) {
		return std::unexpected(error(
			MainProcessErrorCode::presenter_setup_failed
		));
	}
	candidate->prepared_lose_ = present::prepare_superimpose_sheet(sheets[0U]);
	candidate->prepared_round_ = present::prepare_superimpose_sheet(sheets[1U]);
	candidate->zikicw_sheet_ = std::move(sheets[2U]);
	candidate->prepared_zikicw_ = present::prepare_superimpose_sheet(
		candidate->zikicw_sheet_
	);
	if(!candidate->prepared_lose_ || !candidate->prepared_round_ ||
		!candidate->prepared_zikicw_) {
		return std::unexpected(error(
			MainProcessErrorCode::presenter_setup_failed
		));
	}

	for(std::size_t index = 0U; index < candidate->sine_q8_.size(); ++index) {
		const auto angle = static_cast<std::uint8_t>(index);
		candidate->sine_q8_[index] = exact::sine8(angle);
		candidate->cosine_q8_[index] = exact::cosine8(angle);
	}
	return std::shared_ptr<const MainProcessStaticContent>{std::move(candidate)};
}

} // namespace th03::app
