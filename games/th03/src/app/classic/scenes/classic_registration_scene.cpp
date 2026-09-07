#include "classic/scenes/classic_registration_scene_internal.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/assets/decode.hpp"
#include "th03/present/cels.hpp"
#include "th03/present/pi.hpp"

namespace th03::app {
namespace {

constexpr std::int32_t table_left = 24;
constexpr std::int32_t table_top = 104;
constexpr std::int32_t table_row_spacing = 20;
constexpr std::int32_t glyph_width = 32;
constexpr std::int32_t glyph_height = 32;
constexpr std::int32_t place_number_padded_width = 48;
constexpr std::int32_t name_spacing = 24;
constexpr std::int32_t score_spacing = 16;
constexpr std::int32_t cell_padding = 16;
constexpr std::int32_t playchar_width = 96;
constexpr std::int32_t alphabet_left = 64;
constexpr std::int32_t alphabet_top = 320;
constexpr std::int32_t alphabet_row_spacing = 24;
constexpr std::uint8_t alphabet_columns = 16U;
constexpr std::uint8_t alphabet_glyphs = 48U;
constexpr std::uint8_t fade_boundary_count = 35U;
constexpr std::uint8_t new_classic_view_fade_boundary_count = 19U;

enum class LocalizedRegistrationText : std::size_t {
	title,
	header_name,
	header_point,
	header_character,
	header_stage,
	rank_easy,
	rank_normal,
	rank_hard,
	rank_lunatic,
	key_space,
	key_backspace,
	key_end,
	stage_all,
};

constexpr std::array localized_registration_ids{
	NewClassicGameTextId::registration_title,
	NewClassicGameTextId::registration_header_name,
	NewClassicGameTextId::registration_header_point,
	NewClassicGameTextId::registration_header_character,
	NewClassicGameTextId::registration_header_stage,
	NewClassicGameTextId::registration_rank_easy,
	NewClassicGameTextId::registration_rank_normal,
	NewClassicGameTextId::registration_rank_hard,
	NewClassicGameTextId::registration_rank_lunatic,
	NewClassicGameTextId::registration_key_space,
	NewClassicGameTextId::registration_key_backspace,
	NewClassicGameTextId::registration_key_end,
	NewClassicGameTextId::registration_stage_all,
};

constexpr present::Rect registration_title_rect{48, 24, 544U, 56U};
constexpr std::array registration_header_rects{
	present::Rect{112, 80, 96U, 24U},
	present::Rect{328, 80, 96U, 24U},
	present::Rect{432, 80, 120U, 24U},
	present::Rect{552, 80, 80U, 24U},
};
constexpr present::Rect registration_rank_rect{320, 312, 320U, 88U};

[[nodiscard]] constexpr std::size_t localized_index(
	LocalizedRegistrationText value
) noexcept {
	return static_cast<std::size_t>(value);
}

[[nodiscard]] std::span<const std::uint8_t> trim_padding(
	std::span<const std::uint8_t> text
) noexcept {
	while(!text.empty() && text.back() == static_cast<std::uint8_t>(' ')) {
		text = text.first(text.size() - 1U);
	}
	return text;
}

constexpr std::array<std::string_view, new_classic_score_character_count + 1U>
new_classic_page_names{{
	"REIMU", "MIMA", "MARISA", "ELLEN", "KOTOHIME", "KANA",
	"RIKAKO", "CHIYURI", "YUMEMI", "ALL CHARACTERS",
}};

[[nodiscard]] constexpr present::Page new_classic_raster_page(
	std::uint8_t page
) noexcept
{
	return (page & 1U) != 0U ? present::Page::one : present::Page::zero;
}

using Glyph = ClassicRegistrationGlyph;

constexpr std::array<std::array<Glyph, classic_score_name_length>, 9U>
default_names{{
	{{Glyph::r, Glyph::e, Glyph::i, Glyph::m, Glyph::u,
		Glyph::space, Glyph::space, Glyph::space}},
	{{Glyph::m, Glyph::i, Glyph::m, Glyph::a,
		Glyph::space, Glyph::space, Glyph::space, Glyph::space}},
	{{Glyph::m, Glyph::a, Glyph::r, Glyph::i, Glyph::s, Glyph::a,
		Glyph::space, Glyph::space}},
	{{Glyph::e, Glyph::l, Glyph::l, Glyph::e, Glyph::n,
		Glyph::space, Glyph::space, Glyph::space}},
	{{Glyph::k, Glyph::o, Glyph::t, Glyph::o,
		Glyph::h, Glyph::i, Glyph::m, Glyph::e}},
	{{Glyph::k, Glyph::a, Glyph::n, Glyph::a,
		Glyph::space, Glyph::space, Glyph::space, Glyph::space}},
	{{Glyph::r, Glyph::i, Glyph::k, Glyph::a, Glyph::k, Glyph::o,
		Glyph::space, Glyph::space}},
	{{Glyph::c, Glyph::h, Glyph::i, Glyph::y,
		Glyph::u, Glyph::r, Glyph::i, Glyph::space}},
	{{Glyph::y, Glyph::u, Glyph::m, Glyph::e, Glyph::m, Glyph::i,
		Glyph::space, Glyph::space}},
}};

[[nodiscard]] ClassicRegistrationError registration_error(
	ClassicRegistrationErrorCode code,
	std::optional<MainResourceKey> resource = std::nullopt
) noexcept
{
	return {.code = code, .resource = resource};
}

[[nodiscard]] ClassicRegistrationError decode_error(
	MainResourceKey resource,
	assets::AssetError error
)
{
	auto result = registration_error(
		ClassicRegistrationErrorCode::asset_decode_failed, resource
	);
	result.asset_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicRegistrationError text_error(
	present::GraphicsTextError error
) noexcept
{
	auto result = registration_error(
		ClassicRegistrationErrorCode::font_prepare_failed
	);
	result.text_error.emplace(error);
	return result;
}

[[nodiscard]] ClassicRegistrationError store_error(
	ClassicRegistrationErrorCode code,
	ClassicScoreStoreError error
)
{
	auto result = registration_error(code);
	result.store_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicRegistrationError score_error(
	ClassicScoreError error
) noexcept
{
	auto result = registration_error(
		ClassicRegistrationErrorCode::score_mutation_failed
	);
	result.score_error.emplace(error);
	return result;
}

[[nodiscard]] ClassicRegistrationError new_store_error(
	ClassicRegistrationErrorCode code,
	NewClassicScoreStoreError error
)
{
	auto result = registration_error(code);
	result.new_score_store_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicRegistrationError new_score_error(
	NewClassicScoreError error
) noexcept
{
	auto result = registration_error(
		ClassicRegistrationErrorCode::score_mutation_failed
	);
	result.new_score_error.emplace(error);
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

[[nodiscard]] std::expected<void, ClassicRegistrationError>
validate_resource_set(
	std::span<const LoadedMainResource> resources,
	std::span<const MainResourceKey> expected,
	bool authenticate_resource_bytes
)
{
	if(resources.size() != expected.size()) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::resource_set_mismatch
		));
	}
	for(std::size_t index = 0U; index < expected.size(); ++index) {
		const auto key = expected[index];
		const auto &resource = resources[index];
		if(resource.spec.key != key) {
			return std::unexpected(registration_error(
				ClassicRegistrationErrorCode::resource_set_mismatch, key
			));
		}
		if(!resource.bytes) {
			return std::unexpected(registration_error(
				ClassicRegistrationErrorCode::resource_bytes_missing, key
			));
		}
		const auto canonical = main_resource_spec(key);
		if(!canonical || !same_resource_spec(resource.spec, *canonical)) {
			return std::unexpected(registration_error(
				ClassicRegistrationErrorCode::resource_set_mismatch, key
			));
		}
		if(!resource.authenticated_override &&
			(resource.bytes->size() != canonical->expected_bytes ||
			(authenticate_resource_bytes && main_resource_fnv1a64(
				*resource.bytes
			) != canonical->expected_fnv1a64))) {
			return std::unexpected(registration_error(
				ClassicRegistrationErrorCode::resource_integrity_failed, key
			));
		}
	}
	return {};
}

[[nodiscard]] const LoadedMainResource *find_resource(
	std::span<const LoadedMainResource> resources,
	MainResourceKey key
) noexcept
{
	for(const auto &resource : resources) {
		if(resource.spec.key == key) {
			return &resource;
		}
	}
	return nullptr;
}

template <typename Value, typename Decoder>
[[nodiscard]] std::expected<Value, ClassicRegistrationError> decode_asset(
	std::span<const LoadedMainResource> resources,
	MainResourceKey key,
	Decoder decoder
)
{
	const auto *resource = find_resource(resources, key);
	if(!resource || !resource->bytes) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::resource_set_mismatch, key
		));
	}
	auto decoded = decoder(std::span<const std::byte>{*resource->bytes});
	if(!decoded) {
		return std::unexpected(decode_error(key, std::move(decoded.error())));
	}
	return std::move(*decoded);
}

[[nodiscard]] bool valid_context(
	const GameInit &resident,
	bool view_only
) noexcept
{
	if(resident.rank >= 4U || resident.remaining_credits > 3U ||
		resident.runtime_snapshot || resident.compact_runtime_snapshot ||
		resident.round_start_snapshot || resident.practice_rules) {
		return false;
	}
	if(view_only) {
		return resident.game_mode == game_mode::none &&
			resident.story_stage == 0xFFU && resident.show_score_menu == 1U;
	}
	if(resident.game_mode != game_mode::story ||
		resident.show_score_menu != 0U ||
		resident.paletted_character[0U] < 1 ||
		resident.paletted_character[0U] > 18 ||
		!((resident.story_stage >= 1U && resident.story_stage <= 9U) ||
			resident.story_stage == 99U)) {
		return false;
	}
	return std::all_of(
		resident.last_score[0U].begin(), resident.last_score[0U].end(),
		[](std::uint8_t digit) { return digit < 10U; }
	);
}

[[nodiscard]] bool valid_background(const assets::PiImage &image) noexcept
{
	return image.image.width == present::graphics_width &&
		image.image.height == present::graphics_height && image.palette &&
		image.image.indices.size() == present::graphics_page_pixels &&
		std::all_of(
			image.image.indices.begin(), image.image.indices.end(),
			[](std::uint8_t color) { return color <= 0x0FU; }
		);
}

[[nodiscard]] bool valid_rank(const assets::PlanarCel4 &cel) noexcept
{
	constexpr std::uint16_t width = 320U;
	constexpr std::uint16_t height = 88U;
	constexpr std::size_t plane_size = 3520U;
	if(cel.pixel_width != width || cel.pixel_height != height ||
		cel.bitplane_size != plane_size || cel.offset_at_bottom_left != 6960 ||
		cel.vram_dword_width != 10U ||
		cel.row_order != assets::PlanarRowOrder::bottom_to_top ||
		cel.bit_order != assets::PlanarBitOrder::most_significant_bit_left ||
		!cel.alpha || cel.alpha->size() != plane_size) {
		return false;
	}
	return std::all_of(
		cel.color_planes.begin(), cel.color_planes.end(),
		[](const auto &plane) { return plane.size() == plane_size; }
	);
}

[[nodiscard]] bool valid_choice_cel(const assets::PlanarCel4 &cel) noexcept
{
	constexpr std::uint16_t width = 96U;
	constexpr std::uint16_t height = 41U;
	constexpr std::size_t plane_size = 492U;
	if(cel.pixel_width != width || cel.pixel_height != height ||
		cel.bitplane_size != plane_size || cel.offset_at_bottom_left != 3200 ||
		cel.vram_dword_width != 3U ||
		cel.row_order != assets::PlanarRowOrder::bottom_to_top ||
		cel.bit_order != assets::PlanarBitOrder::most_significant_bit_left ||
		cel.alpha) {
		return false;
	}
	return std::all_of(
		cel.color_planes.begin(), cel.color_planes.end(),
		[](const auto &plane) { return plane.size() == plane_size; }
	);
}

[[nodiscard]] bool valid_sheet(
	const present::PreparedSuperimposeSheet &sheet
) noexcept
{
	return sheet.first_code() == 0U && sheet.last_code() == 48U &&
		sheet.pixel_width() == 32U && sheet.pixel_height() == 32U;
}

[[nodiscard]] bool install_palette(
	present::Raster &raster,
	const assets::Palette16Rgb4 &palette
) noexcept
{
	for(std::size_t index = 0U; index < palette.colors.size(); ++index) {
		if(!raster.set_pending_palette_entry(
			static_cast<std::uint8_t>(index), palette.colors[index]
		)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] constexpr MainRegistrationRank registration_rank(
	std::uint8_t rank
) noexcept
{
	return static_cast<MainRegistrationRank>(rank + 1U);
}

[[nodiscard]] constexpr std::uint8_t glyph_value(Glyph glyph) noexcept
{
	return static_cast<std::uint8_t>(glyph);
}

[[nodiscard]] constexpr bool doublewide(Glyph glyph) noexcept
{
	return (glyph_value(glyph) % alphabet_columns) == glyph_value(Glyph::space);
}

[[nodiscard]] constexpr std::uint8_t base_character(
	std::int8_t paletted
) noexcept
{
	return static_cast<std::uint8_t>(
		(static_cast<std::uint8_t>(paletted) - 1U) / 2U
	);
}

class SourceRandom final {
public:
	explicit SourceRandom(std::uint32_t seed) noexcept : seed_(seed) {}

	[[nodiscard]] std::uint16_t next() noexcept
	{
		seed_ = (seed_ * 0x015A4E35U) + 1U;
		return static_cast<std::uint16_t>((seed_ >> 16U) & 0x7FFFU);
	}

	[[nodiscard]] ClassicScoreCodecFields next_codec() noexcept
	{
		const auto key1 = static_cast<std::uint8_t>(next());
		const auto key2 = static_cast<std::uint8_t>(next());
		const auto unknown = static_cast<std::uint8_t>(next());
		return {.unknown = unknown, .key1 = key1, .key2 = key2};
	}

private:
	std::uint32_t seed_ = 0U;
};

[[nodiscard]] constexpr present::Point alphabet_position(Glyph glyph) noexcept
{
	const auto raw = glyph_value(glyph);
	return {
		alphabet_left + static_cast<std::int32_t>(raw % alphabet_columns) *
			glyph_width,
		alphabet_top + static_cast<std::int32_t>(raw / alphabet_columns) *
			alphabet_row_spacing,
	};
}

[[nodiscard]] constexpr std::int32_t row_top(std::size_t place) noexcept
{
	return table_top + static_cast<std::int32_t>(place) * table_row_spacing;
}

[[nodiscard]] constexpr std::int32_t name_left(
	std::uint8_t cursor_backwards
) noexcept
{
	return table_left + place_number_padded_width +
		static_cast<std::int32_t>(
			(classic_score_name_length - 1U) - cursor_backwards
		) * name_spacing;
}

} // namespace

ClassicRegistrationCompletion::ClassicRegistrationCompletion(
	GameInit resident,
	present::Raster raster,
	MainlRegistrationNextScreen next_screen,
	std::array<std::optional<assets::PlanarCel4>, 4U> choice_cels,
	std::optional<std::size_t> entered_place,
	ClassicScoreStoreDisposition load_disposition
) noexcept :
	resident_(std::move(resident)),
	raster_(std::move(raster)),
	next_screen_(next_screen),
	choice_cels_(std::move(choice_cels)),
	entered_place_(entered_place),
	load_disposition_(load_disposition)
{}

const present::Raster &ClassicRegistrationCompletion::raster() const noexcept
{
	return raster_;
}

std::optional<std::size_t>
ClassicRegistrationCompletion::entered_place() const noexcept
{
	return entered_place_;
}

ClassicScoreStoreDisposition
ClassicRegistrationCompletion::load_disposition() const noexcept
{
	return load_disposition_;
}

MainlRegistrationNextScreen
ClassicRegistrationCompletion::next_screen() const noexcept
{
	return next_screen_;
}

const GameInit &ClassicRegistrationCompletion::resident() const noexcept
{
	return resident_;
}

ClassicRegistrationScene::ClassicRegistrationScene(
	std::unique_ptr<Prepared> prepared,
	GameInit resident,
	std::filesystem::path score_path,
	ClassicScoreDocument document,
	ClassicScoreStoreDisposition load_disposition,
	ClassicScoreCodecFields commit_codec,
	std::optional<NewClassicScoreDocument> new_classic_document,
	std::uint8_t new_classic_character,
	std::uint8_t new_classic_page,
	const present::AnkGlyphProvider *font,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	std::optional<std::size_t> entered_place,
	present::Raster raster,
	bool view_only,
	bool persist_changes,
	bool retain_high_score_music
) noexcept :
	prepared_(std::move(prepared)),
	resident_(std::move(resident)),
	score_path_(std::move(score_path)),
	document_(std::move(document)),
	load_disposition_(load_disposition),
	commit_codec_(commit_codec),
	new_classic_document_(std::move(new_classic_document)),
	new_classic_character_(new_classic_character),
	new_classic_page_(new_classic_page),
	font_(font),
	patch_assets_(std::move(patch_assets)),
	entered_place_(entered_place),
	raster_(std::move(raster)),
	view_only_(view_only),
	persist_changes_(persist_changes),
	retain_high_score_music_(retain_high_score_music)
{}

ClassicRegistrationScene::~ClassicRegistrationScene() = default;

std::expected<std::unique_ptr<ClassicRegistrationScene>, ClassicRegistrationError>
ClassicRegistrationScene::create(
	const GameInit &resident,
	const std::filesystem::path &score_path,
	std::span<const LoadedMainResource> resources,
	const MainlStageTextAssets &stage_text,
	const present::AnkGlyphProvider &font,
	present::Raster initial_raster
)
{
	return create_impl(
		resident, score_path, resources, stage_text, font,
		std::move(initial_raster), true, false, false, {}, std::nullopt, true, {},
		nullptr
	);
}

std::expected<std::unique_ptr<ClassicRegistrationScene>, ClassicRegistrationError>
ClassicRegistrationScene::create_new_classic(
	const GameInit &resident,
	const std::filesystem::path &score_path,
	const std::filesystem::path &legacy_score_path,
	std::span<const LoadedMainResource> resources,
	const MainlStageTextAssets &stage_text,
	const present::AnkGlyphProvider &font,
	present::Raster initial_raster,
	NewClassicScoreRun run,
	const NewClassicGameTextCatalog *game_text
)
{
	return create_impl(
		resident, score_path, resources, stage_text, font,
		std::move(initial_raster), true, false, true, legacy_score_path, run, true,
		{}, game_text
	);
}

std::expected<std::unique_ptr<ClassicRegistrationScene>, ClassicRegistrationError>
ClassicRegistrationScene::create_new_classic_playback(
	const GameInit &resident,
	const std::filesystem::path &score_path,
	const std::filesystem::path &legacy_score_path,
	std::span<const LoadedMainResource> resources,
	const MainlStageTextAssets &stage_text,
	const present::AnkGlyphProvider &font,
	present::Raster initial_raster,
	const NewClassicGameTextCatalog *game_text
)
{
	return create_impl(
		resident, score_path, resources, stage_text, font,
		std::move(initial_raster), true, false, true, legacy_score_path,
		NewClassicScoreRun{}, false, {}, game_text
	);
}

std::expected<std::unique_ptr<ClassicRegistrationScene>, ClassicRegistrationError>
ClassicRegistrationScene::create_viewer(
	const ClassicOpCompletion &completion,
	const std::filesystem::path &score_path,
	std::span<const LoadedMainResource> resources,
	const MainlStageTextAssets &stage_text,
	const present::AnkGlyphProvider &font
)
{
	if(completion.route() != ClassicOpRoute::high_scores ||
		completion.selection_mode()) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::invalid_context
		));
	}
	return create_impl(
		completion.resident(), score_path, resources, stage_text, font,
		present::Raster{}, true, true, false, {}, std::nullopt, true, {}, nullptr
	);
}

std::expected<std::unique_ptr<ClassicRegistrationScene>, ClassicRegistrationError>
ClassicRegistrationScene::create_new_classic_viewer(
	const ClassicOpCompletion &completion,
	const std::filesystem::path &score_path,
	const std::filesystem::path &legacy_score_path,
	std::span<const LoadedMainResource> resources,
	const MainlStageTextAssets &stage_text,
	const present::AnkGlyphProvider &font,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	const NewClassicGameTextCatalog *game_text,
	bool retain_high_score_music
)
{
	if(completion.route() != ClassicOpRoute::high_scores ||
		completion.selection_mode()) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::invalid_context
		));
	}
	return create_impl(
		completion.resident(), score_path, resources, stage_text, font,
		present::Raster{}, true, true, true, legacy_score_path, std::nullopt, false,
		std::move(patch_assets), game_text, retain_high_score_music
	);
}

std::expected<std::unique_ptr<ClassicRegistrationScene>, ClassicRegistrationError>
ClassicRegistrationScene::create_impl(
	const GameInit &resident,
	const std::filesystem::path &score_path,
	std::span<const LoadedMainResource> resources,
	const MainlStageTextAssets &stage_text,
	const present::AnkGlyphProvider &font,
	present::Raster initial_raster,
	bool authenticate_resource_bytes,
	bool view_only,
	bool new_classic,
	std::filesystem::path legacy_score_path,
	std::optional<NewClassicScoreRun> run,
	bool persist_changes,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	const NewClassicGameTextCatalog *game_text,
	bool retain_high_score_music
)
{
	if(!valid_context(resident, view_only)) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::invalid_context
		));
	}
	const auto next_screen = (resident.remaining_credits != 0U &&
		resident.story_stage != 99U) ?
		MainlRegistrationNextScreen::continue_menu :
		MainlRegistrationNextScreen::game_over;
	const auto keys = mainl_registration_resource_keys({
		.rank = registration_rank(resident.rank),
		.next_screen = next_screen,
	});
	if(!keys) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::invalid_context
		));
	}
	if(const auto valid = validate_resource_set(
		resources, *keys, authenticate_resource_bytes
	); !valid) {
		return std::unexpected(valid.error());
	}

	const MainResourceKey background_key{
		.role = MainResourceRole::registration_background,
	};
	auto background = decode_asset<assets::PiImage>(
		resources, background_key, assets::decode_pi
	);
	if(!background) {
		return std::unexpected(background.error());
	}
	const MainResourceKey rank_key{
		.role = MainResourceRole::registration_rank,
		.registration_rank = registration_rank(resident.rank),
	};
	auto rank = decode_asset<assets::PlanarCel4>(
		resources,
		rank_key,
		[](std::span<const std::byte> bytes) {
			return assets::decode_cdg_cel(
				bytes, 0U, assets::CdgAlphaPolicy::preserve
			);
		}
	);
	if(!rank) {
		return std::unexpected(rank.error());
	}
	auto decode_sheet = [&](MainResourceRole role) ->
		std::expected<assets::BfntSheet, ClassicRegistrationError> {
		const MainResourceKey key{.role = role};
		return decode_asset<assets::BfntSheet>(
			resources,
			key,
			[](std::span<const std::byte> bytes) {
				return assets::decode_bfnt(
					bytes, assets::BfntConsumerPolicy::superimpose
				);
			}
		);
	};
	auto normal = decode_sheet(MainResourceRole::registration_normal_glyphs);
	if(!normal) {
		return std::unexpected(normal.error());
	}
	auto highlight = decode_sheet(
		MainResourceRole::registration_highlight_glyphs
	);
	if(!highlight) {
		return std::unexpected(highlight.error());
	}
	const MainResourceKey next_background_key{
		.role = next_screen == MainlRegistrationNextScreen::continue_menu ?
			MainResourceRole::continue_background :
			MainResourceRole::gameover_background,
	};
	auto next_background = decode_asset<assets::PiImage>(
		resources, next_background_key, assets::decode_pi
	);
	if(!next_background) {
		return std::unexpected(next_background.error());
	}
	std::array<std::optional<assets::PlanarCel4>, 4U> choice_cels{};
	if(next_screen == MainlRegistrationNextScreen::continue_menu) {
		const MainResourceKey choice_key{
			.role = MainResourceRole::continue_choice_cels,
		};
		for(std::size_t cel = 0U; cel < choice_cels.size(); ++cel) {
			auto decoded = decode_asset<assets::PlanarCel4>(
				resources,
				choice_key,
				[cel](std::span<const std::byte> bytes) {
					return assets::decode_cdg_cel(
						bytes, cel, assets::CdgAlphaPolicy::discard
					);
				}
			);
			if(!decoded) {
				return std::unexpected(decoded.error());
			}
			choice_cels[cel].emplace(std::move(*decoded));
		}
	}
	return create_predecoded(
		resident, score_path, std::move(*background), std::move(*rank),
		std::move(*normal), std::move(*highlight),
		std::move(*next_background), std::move(choice_cels), stage_text, font,
		std::move(initial_raster), view_only, new_classic,
		std::move(legacy_score_path), run, persist_changes,
		std::move(patch_assets), game_text, retain_high_score_music
	);
}

std::expected<std::unique_ptr<ClassicRegistrationScene>, ClassicRegistrationError>
ClassicRegistrationScene::create_predecoded(
	const GameInit &resident,
	const std::filesystem::path &score_path,
	assets::PiImage background,
	assets::PlanarCel4 rank,
	assets::BfntSheet normal,
	assets::BfntSheet highlight,
	assets::PiImage next_background,
	std::array<std::optional<assets::PlanarCel4>, 4U> choice_cels,
	const MainlStageTextAssets &stage_text,
	const present::AnkGlyphProvider &font,
	present::Raster initial_raster,
	bool view_only,
	bool new_classic,
	std::filesystem::path legacy_score_path,
	std::optional<NewClassicScoreRun> run,
	bool persist_changes,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	const NewClassicGameTextCatalog *game_text,
	bool retain_high_score_music
)
{
	if(!valid_context(resident, view_only) ||
		(new_classic && view_only && !patch_assets) ||
		initial_raster.raster_mode() != present::RasterMode::lines_400) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::invalid_context
		));
	}
	const auto next_screen = (resident.remaining_credits != 0U &&
		resident.story_stage != 99U) ?
		MainlRegistrationNextScreen::continue_menu :
		MainlRegistrationNextScreen::game_over;
	if(!valid_background(background) || !valid_rank(rank) ||
		!valid_background(next_background) ||
		(next_screen == MainlRegistrationNextScreen::continue_menu &&
			!std::all_of(
				choice_cels.begin(), choice_cels.end(),
				[](const auto &cel) { return cel && valid_choice_cel(*cel); }
			))) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::invalid_asset_geometry
		));
	}

	auto prepared = std::make_unique<Prepared>();
	prepared->background = std::move(background);
	prepared->rank = std::move(rank);
	prepared->next_background = std::move(next_background);
	prepared->choice_cels = std::move(choice_cels);
	prepared->next_screen = next_screen;
	if(game_text) {
		const auto &english = new_classic_builtin_english_game_text();
		for(std::size_t index = 0U;
			index < localized_registration_ids.size();
			++index) {
			const auto id = localized_registration_ids[index];
			const auto translated = game_text->bytes(id);
			if(std::ranges::equal(translated, english.bytes(id))) {
				continue;
			}
			const auto visible = trim_padding(translated);
			if(visible.empty()) {
				return std::unexpected(registration_error(
					ClassicRegistrationErrorCode::font_prepare_failed
				));
			}
			auto text = present::prepare_graphics_text_8x16(visible, font);
			if(!text) {
				return std::unexpected(text_error(text.error()));
			}
			prepared->localized_text[index].emplace(std::move(*text));
		}
	}
	auto normal_prepared = present::prepare_superimpose_sheet(normal);
	auto highlight_prepared = present::prepare_superimpose_sheet(highlight);
	if(!normal_prepared || !highlight_prepared ||
		!valid_sheet(*normal_prepared) || !valid_sheet(*highlight_prepared) ||
		!present::superimpose_remap_color_preserve_opacity(
			*normal_prepared, 10U, 15U
		) || !present::superimpose_remap_color_preserve_opacity(
			*highlight_prepared, 15U, 10U
		)) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::invalid_asset_geometry
		));
	}
	prepared->normal.emplace(std::move(*normal_prepared));
	prepared->highlight.emplace(std::move(*highlight_prepared));

	for(std::size_t index = 0U;
		index < stage_text.registration_playchars.size();
		++index) {
		if(stage_text.registration_playchars[index].size() != 12U) {
			return std::unexpected(registration_error(
				ClassicRegistrationErrorCode::font_prepare_failed
			));
		}
		auto name = present::prepare_graphics_text_8x16(
			stage_text.registration_playchars[index], font
		);
		if(!name) {
			return std::unexpected(text_error(name.error()));
		}
		prepared->playchars[index].emplace(std::move(*name));
	}

	SourceRandom first_random(std::bit_cast<std::uint32_t>(
		resident.resident_rand
	));
	const auto first_commit_codec = first_random.next_codec();
	SourceRandom recreate_random(std::bit_cast<std::uint32_t>(
		resident.resident_rand
	));
	std::array<ClassicScoreCodecFields, classic_score_rank_count> stock_codec{};
	for(auto &fields : stock_codec) {
		fields = recreate_random.next_codec();
	}
	const auto recreated_commit_codec = recreate_random.next_codec();

	ClassicScoreDocument document{};
	ClassicScoreStoreDisposition load_disposition =
		ClassicScoreStoreDisposition::loaded;
	std::optional<NewClassicScoreDocument> new_document{};
	const auto character = base_character(resident.paletted_character[0U]);
	const auto initial_page = view_only ? new_classic_score_all_characters :
		character;
	std::optional<std::size_t> entered_place{};
	if(new_classic) {
		auto loaded = persist_changes ?
			load_new_classic_score_store(score_path, legacy_score_path) :
			load_new_classic_score_store_read_only(score_path, legacy_score_path);
		if(!loaded) {
			return std::unexpected(new_store_error(
				ClassicRegistrationErrorCode::store_load_failed,
				std::move(loaded.error())
			));
		}
		new_document.emplace(std::move(loaded->document));
		if(!view_only) {
			if(!run) {
				return std::unexpected(registration_error(
					ClassicRegistrationErrorCode::invalid_context
				));
			}
			if(persist_changes) {
				if(auto applied = new_classic_score_apply_run(
					*new_document, resident.rank, character, *run
				); !applied) {
					return std::unexpected(new_score_error(applied.error()));
				}
			}
			const ClassicScoreCandidate candidate{
				.digits = resident.last_score[0U],
				.continues_used = static_cast<std::uint8_t>(
					3U - resident.remaining_credits
				),
				.stage = resident.story_stage,
				.playchar = character,
			};
			auto inserted = new_classic_score_insert(
				*new_document, resident.rank, character, candidate
			);
			if(!inserted) {
				return std::unexpected(new_score_error(inserted.error()));
			}
			entered_place = *inserted;
			if(resident.story_stage == 99U && resident.remaining_credits == 3U) {
				new_document->extra_unlocked = true;
			}
		}
		auto view = new_classic_score_view(
			*new_document, resident.rank, initial_page
		);
		if(!view) {
			return std::unexpected(new_score_error(view.error()));
		}
		document = classic_score_stock_file(stock_codec);
		document.boards[resident.rank] = std::move(view->board);
		load_disposition =
			loaded->disposition == NewClassicScoreStoreDisposition::loaded ?
			ClassicScoreStoreDisposition::loaded :
			ClassicScoreStoreDisposition::created_stock;
	} else {
		auto loaded = load_classic_score_store(score_path, stock_codec);
		if(!loaded) {
			return std::unexpected(store_error(
				ClassicRegistrationErrorCode::store_load_failed,
				std::move(loaded.error())
			));
		}
		document = std::move(loaded->document);
		load_disposition = loaded->disposition;
	}
	if(!new_classic && !view_only) {
		auto &board = document.boards[resident.rank];
		const ClassicScoreCandidate candidate{
			.digits = resident.last_score[0U],
			.continues_used = static_cast<std::uint8_t>(
				3U - resident.remaining_credits
			),
			.stage = resident.story_stage,
			.playchar = base_character(resident.paletted_character[0U]),
		};
		auto inserted = classic_score_insert(board, candidate);
		if(!inserted) {
			return std::unexpected(score_error(inserted.error()));
		}
		entered_place = *inserted;
		if(resident.story_stage == 99U && resident.remaining_credits == 3U) {
			board.cleared = ClassicScoreCleared::cleared;
		}
	}
	const auto commit_codec =
		(load_disposition == ClassicScoreStoreDisposition::loaded) ?
		first_commit_codec : recreated_commit_codec;
	auto scene = std::unique_ptr<ClassicRegistrationScene>(
		new ClassicRegistrationScene(
			std::move(prepared), resident, score_path, std::move(document),
			load_disposition, commit_codec, std::move(new_document),
			character, initial_page, &font, std::move(patch_assets), entered_place,
			std::move(initial_raster), view_only, persist_changes,
			retain_high_score_music
		)
	);
	if(new_classic) {
		if(auto page = scene->load_new_classic_page(initial_page, false); !page) {
			return std::unexpected(std::move(page.error()));
		}
	}
	if(!scene->initialize_visuals()) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::draw_failed
		));
	}
	if(new_classic && view_only) {
		scene->phase_ = ClassicRegistrationPhase::initial_view_vsync;
	}
	if(!retain_high_score_music && (!scene->push_audio_command({
		.type = ClassicAudioCommandType::load_song,
		.cue = ClassicMusicCue::high_score,
	}) || !scene->push_audio_command({
		.type = ClassicAudioCommandType::play_song,
		.cue = ClassicMusicCue::high_score,
	}))) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::invalid_state
		));
	}
	return scene;
}

bool ClassicRegistrationScene::initialize_visuals() noexcept
{
	if(!prepared_ || !prepared_->background.palette || !prepared_->normal ||
		!prepared_->highlight ||
		!raster_.set_raster_mode(present::RasterMode::lines_400) ||
		!raster_.set_shown_page(present::Page::zero) ||
		!raster_.set_access_page(present::Page::zero) ||
		!install_palette(raster_, *prepared_->background.palette)) {
		return false;
	}
	raster_.set_palette_tone(0);
	raster_.apply_pending_palette();
	const auto copy_visible_to_backup = [this]() noexcept {
		return raster_.copy_page(
			present::Page::zero,
			{0, 0, present::graphics_width, present::graphics_height},
			present::Page::one,
			{0, 0}
		);
	};
	if(!present::pi_put_8(raster_, 0, 0, prepared_->background) ||
		!draw_registration_art() ||
		(view_only_ && !copy_visible_to_backup()) || !draw_rows(false) ||
		(!view_only_ && !copy_visible_to_backup()) ||
		(!(view_only_ && new_classic_document_) &&
		 !raster_.set_access_page(present::Page::zero))) {
		return false;
	}
	return !entered_place_ || draw_alphabet();
}

bool ClassicRegistrationScene::draw_registration_art() noexcept
{
	if(!prepared_) {
		return false;
	}
	const auto draw_panel = [this](
		present::Rect rect,
		const present::PreparedGraphicsText8x16 &text,
		std::uint8_t scale
	) noexcept {
		const auto width = static_cast<std::uint64_t>(text.pixel_width()) * scale;
		const auto height = static_cast<std::uint64_t>(present::jis_height) * scale;
		if(width == 0U || width > rect.width - 4U || height > rect.height - 4U ||
			!raster_.clear(rect, 1U) ||
			!raster_.clear({rect.x, rect.y, rect.width, 1U}, 4U) ||
			!raster_.clear({
				rect.x, rect.y + static_cast<std::int32_t>(rect.height - 1U),
				rect.width, 1U
			}, 4U) ||
			!raster_.clear({rect.x, rect.y, 1U, rect.height}, 4U) ||
			!raster_.clear({
				rect.x + static_cast<std::int32_t>(rect.width - 1U), rect.y,
				1U, rect.height
			}, 4U)) {
			return false;
		}
		const present::Point position{
			rect.x + static_cast<std::int32_t>((rect.width - width) / 2U),
			rect.y + static_cast<std::int32_t>((rect.height - height) / 2U),
		};
		return present::graphics_text_put_scaled_8x16(
			raster_, position, text,
			{present::Index4{15U}, present::GraphicsTextWeight::black, 0U},
			scale
		);
	};
	const auto &title = prepared_->localized_text[
		localized_index(LocalizedRegistrationText::title)
	];
	if(title && !draw_panel(registration_title_rect, *title, 2U)) {
		return false;
	}
	for(std::size_t header = 0U; header < registration_header_rects.size();
		++header) {
		const auto &text = prepared_->localized_text[
			localized_index(LocalizedRegistrationText::header_name) + header
		];
		if(text && !draw_panel(registration_header_rects[header], *text, 1U)) {
			return false;
		}
	}
	const auto rank_index =
		localized_index(LocalizedRegistrationText::rank_easy) + resident_.rank;
	const auto &rank = prepared_->localized_text[rank_index];
	if(!rank) {
		return present::cdg_put_8(raster_, 320, 312, prepared_->rank);
	}
	const auto width = static_cast<std::uint64_t>(rank->pixel_width()) * 2U;
	constexpr auto height = static_cast<std::uint32_t>(present::jis_height) * 2U;
	if(width == 0U || width > registration_rank_rect.width ||
		height > registration_rank_rect.height) {
		return false;
	}
	return present::graphics_text_put_scaled_8x16(
		raster_,
		{
			registration_rank_rect.x + static_cast<std::int32_t>(
				(registration_rank_rect.width - width) / 2U
			),
			registration_rank_rect.y + static_cast<std::int32_t>(
				(registration_rank_rect.height - height) / 2U
			),
		},
		*rank,
		{present::Index4{15U}, present::GraphicsTextWeight::black, 0U},
		2U
	);
}

bool ClassicRegistrationScene::draw_localized_command(
	present::Point position,
	std::size_t text_index,
	bool highlight
) noexcept {
	if(!prepared_ || text_index >= prepared_->localized_text.size() ||
		!prepared_->localized_text[text_index]) {
		return false;
	}
	const auto &text = *prepared_->localized_text[text_index];
	const auto width = static_cast<std::uint64_t>(text.pixel_width()) * 2U;
	constexpr auto command_width = glyph_width * 2;
	if(width == 0U || width > static_cast<std::uint32_t>(command_width)) {
		return false;
	}
	return present::graphics_text_put_scaled_8x16(
		raster_,
		{
			position.x + static_cast<std::int32_t>((command_width - width) / 2U),
			position.y,
		},
		text,
		{
			static_cast<present::Index4>(highlight ? 10U : 15U),
			present::GraphicsTextWeight::black,
			0U,
		},
		2U
	);
}

bool ClassicRegistrationScene::preload_next_screen() noexcept
{
	if(!prepared_ || !prepared_->next_background.palette ||
		!raster_.set_access_page(present::Page::zero) ||
		!raster_.set_shown_page(present::Page::zero) ||
		!install_palette(raster_, *prepared_->next_background.palette) ||
		!present::pi_put_8(raster_, 0, 0, prepared_->next_background)) {
		return false;
	}
	raster_.set_palette_tone(0);
	raster_.apply_pending_palette();
	return true;
}

bool ClassicRegistrationScene::draw_glyph(
	present::Point position,
	ClassicRegistrationGlyph glyph,
	bool highlight
) noexcept
{
	if(glyph == Glyph::all && prepared_ && prepared_->localized_text[
		localized_index(LocalizedRegistrationText::stage_all)
	]) {
		return draw_localized_command(
			position, localized_index(LocalizedRegistrationText::stage_all),
			highlight
		);
	}
	if(!classic_registration_glyph_valid(glyph) || !prepared_->normal ||
		!prepared_->highlight) {
		return false;
	}
	const auto &sheet = highlight ? *prepared_->highlight : *prepared_->normal;
	const auto code = static_cast<std::uint16_t>(glyph_value(glyph));
	return present::superimpose_put(raster_, sheet, position, code) &&
		(!doublewide(glyph) || present::superimpose_put(
			raster_, sheet, {position.x + glyph_width, position.y},
			static_cast<std::uint16_t>(code + 1U)
		));
}

bool ClassicRegistrationScene::draw_alphabet_glyph(
	ClassicRegistrationGlyph glyph,
	bool highlight
) noexcept
{
	std::optional<std::size_t> localized{};
	if(glyph == Glyph::space) {
		localized = localized_index(LocalizedRegistrationText::key_space);
	} else if(glyph == Glyph::backspace) {
		localized = localized_index(LocalizedRegistrationText::key_backspace);
	} else if(glyph == Glyph::end) {
		localized = localized_index(LocalizedRegistrationText::key_end);
	}
	if(localized && prepared_ && prepared_->localized_text[*localized]) {
		return draw_localized_command(
			alphabet_position(glyph), *localized, highlight
		);
	}
	return glyph_value(glyph) < alphabet_glyphs &&
		draw_glyph(alphabet_position(glyph), glyph, highlight);
}

bool ClassicRegistrationScene::draw_alphabet() noexcept
{
	for(std::uint8_t raw = 0U; raw < alphabet_glyphs; ++raw) {
		if(!draw_alphabet_glyph(static_cast<Glyph>(raw), false)) {
			return false;
		}
	}
	return true;
}

bool ClassicRegistrationScene::draw_row(std::size_t place) noexcept
{
	if(place >= classic_score_place_count) {
		return false;
	}
	const auto &row = document_.boards[resident_.rank].rows[place];
	const bool highlight = entered_place_ && *entered_place_ == place;
	std::int32_t left = table_left;
	const auto top = row_top(place);
	if(place != 9U) {
		if(!draw_glyph(
			{left, top},
			static_cast<Glyph>(glyph_value(Glyph::digit_1) +
				static_cast<std::uint8_t>(place)),
			highlight
		)) {
			return false;
		}
	} else if(!new_classic_row_total_[place] &&
		(!draw_glyph({left - 8, top}, Glyph::digit_1, highlight) ||
		 !draw_glyph({left + 8, top}, Glyph::digit_0, highlight))) {
		return false;
	}
	left += place_number_padded_width;

	for(std::size_t remaining = classic_score_name_length;
		remaining > 0U;
		--remaining) {
		const auto glyph = row.name_backwards[remaining - 1U];
		if(glyph != Glyph::space &&
			!draw_glyph({left, top}, glyph, highlight)) {
			return false;
		}
		left += name_spacing;
	}
	left += cell_padding;

	bool digit_seen = false;
	for(std::size_t remaining = classic_score_cell_count;
		remaining > 0U;
		--remaining) {
		const auto glyph = row.score[remaining - 1U];
		digit_seen = digit_seen || glyph != Glyph::digit_0;
		if(digit_seen && !draw_glyph({left, top}, glyph, highlight)) {
			return false;
		}
		left += score_spacing;
	}
	left += cell_padding;
	if(new_classic_document_ && new_classic_row_total_[place]) {
		return true;
	}

	if(row.playchar_optional >= prepared_->playchars.size() ||
		!prepared_->playchars[row.playchar_optional] ||
		!present::graphics_text_put_8x16(
			raster_,
			{left, top + ((glyph_height - 16) / 2)},
			*prepared_->playchars[row.playchar_optional],
			{
				.color = static_cast<present::Index4>(highlight ? 15U : 4U),
				.weight = present::GraphicsTextWeight::bold,
			}
		)) {
		return false;
	}
	left += playchar_width + cell_padding;
	return draw_glyph({left, top}, row.stage, highlight);
}

bool ClassicRegistrationScene::draw_rows(bool restore_background) noexcept
{
	if(view_only_ && new_classic_document_) {
		const auto target = new_classic_raster_page(new_classic_page_);
		if(!raster_.set_access_page(target) ||
			!present::pi_put_8(raster_, 0, 0, prepared_->background) ||
			!draw_registration_art()) {
			return false;
		}
		for(std::size_t place = 0U; place < classic_score_place_count; ++place) {
			if(!draw_row(place)) {
				return false;
			}
		}
		if(!draw_new_classic_overlay()) {
			return false;
		}
		return !restore_background ||
			(raster_.set_shown_page(target) && raster_.set_access_page(target));
	}
	if(restore_background && !raster_.copy_page(
		present::Page::one,
		{0, 0, present::graphics_width, present::graphics_height},
		present::Page::zero,
		{0, 0}
	)) {
		return false;
	}
	if(!raster_.set_access_page(present::Page::zero)) {
		return false;
	}
	for(std::size_t place = 0U; place < classic_score_place_count; ++place) {
		if(!draw_row(place)) {
			return false;
		}
	}
	return !view_only_ || !new_classic_document_ ||
		draw_new_classic_overlay();
}

bool ClassicRegistrationScene::draw_new_classic_overlay() noexcept
{
	if(!new_classic_document_ || !view_only_ || !patch_assets_ ||
		new_classic_overlay_[0U].empty() || new_classic_overlay_[1U].empty() ||
		new_classic_overlay_[2U].empty()) {
		return false;
	}
	const auto &menu_font = patch_assets_->menu_font;
	const auto prefix_width = menu_font.width("< ");
	std::int32_t name_field_width = 0;
	for(const auto page_name : new_classic_page_names) {
		name_field_width = std::max(name_field_width, menu_font.width(page_name));
	}
	const auto name_width = menu_font.width(new_classic_overlay_[0U]);
	const auto name_field_left = 24 + prefix_width;
	const auto name_left = name_field_left +
		((name_field_width - name_width) / 2);
	const auto suffix_left = name_field_left + name_field_width +
		menu_font.width(" ");
	if(!menu_font.put(raster_, 24, 320, "<", 15U) ||
		!menu_font.put(raster_, name_left, 320, new_classic_overlay_[0U], 15U) ||
		!menu_font.put(raster_, suffix_left, 320, ">", 15U)) {
		return false;
	}
	std::int32_t left = 24;
	for(const auto value : new_classic_overlay_[1U]) {
		const std::string_view glyph(&value, 1U);
		if(value >= '0' && value <= '9') {
			if(!menu_font.put_n(
				raster_, left + (value == '1' ? 2 : 0), 340,
				glyph, 1U, 15U
			)) {
				return false;
			}
			left += 12;
		} else {
			if(!menu_font.put_n(raster_, left, 340, glyph, 1U, 15U)) {
				return false;
			}
			left += menu_font.width_n(glyph, 1U);
		}
	}
	return menu_font.put(raster_, 24, 360, new_classic_overlay_[2U], 15U);
}

std::expected<void, ClassicRegistrationError>
ClassicRegistrationScene::load_new_classic_page(
	std::uint8_t page,
	bool redraw
)
{
	if(!new_classic_document_ || !font_) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::invalid_state
		));
	}
	auto view = new_classic_score_view(
		*new_classic_document_, resident_.rank, page
	);
	if(!view) {
		return std::unexpected(new_score_error(view.error()));
	}
	const auto page_index = page == new_classic_score_all_characters ?
		new_classic_score_character_count : static_cast<std::size_t>(page);
	const auto seconds = static_cast<std::uint32_t>(
		(view->stats.play_frames / 282U) * 5U +
		((view->stats.play_frames % 282U) * 5U) / 282U
	);
	const auto hours = std::min<std::uint32_t>(seconds / 3600U, 9999U);
	auto fixed = [](std::uint32_t value, std::size_t digits) {
		std::string result(digits, '0');
		while(digits != 0U) {
			result[--digits] = static_cast<char>('0' + (value % 10U));
			value /= 10U;
		}
		return result;
	};
	const std::array<std::string, 3U> lines{{
		std::string(new_classic_page_names[page_index]),
		std::string("PLAY TIME ") + fixed(hours, 4U) + ":" +
			fixed((seconds / 60U) % 60U, 2U) + ":" +
			fixed(seconds % 60U, 2U),
		std::string("1CC ") + std::to_string(view->stats.one_ccs) +
			"   CONTINUES " + std::to_string(view->stats.continues_used),
	}};
	document_.boards[resident_.rank] = std::move(view->board);
	new_classic_row_total_ = view->total;
	new_classic_page_ = page;
	new_classic_overlay_ = lines;
	if(redraw && !draw_rows(true)) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::draw_failed
		));
	}
	return {};
}

bool ClassicRegistrationScene::blank_cursor() noexcept
{
	if(!entered_place_) {
		return false;
	}
	const auto left = name_left(name_cursor_backwards_);
	const auto top = row_top(*entered_place_);
	if(!raster_.copy_page(
		present::Page::one,
		{left, top, static_cast<std::uint16_t>(glyph_width),
			static_cast<std::uint16_t>(glyph_height)},
		present::Page::zero,
		{left, top}
	)) {
		return false;
	}
	document_.boards[resident_.rank].rows[*entered_place_]
		.name_backwards[name_cursor_backwards_] = Glyph::space;
	return true;
}

bool ClassicRegistrationScene::rerender_name_cursor() noexcept
{
	if(!entered_place_ || !draw_alphabet_glyph(selected_, true) ||
		!blank_cursor() || !draw_row(*entered_place_)) {
		return false;
	}
	return doublewide(selected_) || draw_glyph(
		{name_left(name_cursor_backwards_), row_top(*entered_place_)},
		selected_, false
	);
}

bool ClassicRegistrationScene::rerender_completed_name() noexcept
{
	if(!entered_place_) {
		return false;
	}
	// The source decrements its unsigned cursor below zero after the eighth
	// glyph and then tries to blank and preview that impossible position. Keep
	// the observable row redraw at this frame boundary without reproducing the
	// resulting layout-dependent VRAM access.
	return draw_alphabet_glyph(selected_, true) && draw_row(*entered_place_);
}

std::expected<void, ClassicRegistrationError>
ClassicRegistrationScene::finish_name()
{
	if(!entered_place_) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::invalid_state
		));
	}
	const auto character = base_character(resident_.paletted_character[0U]);
	auto replaced = classic_score_replace_uniform_name(
		document_.boards[resident_.rank], *entered_place_,
		default_names[character]
	);
	if(!replaced) {
		return std::unexpected(score_error(replaced.error()));
	}
	if(new_classic_document_ && !view_only_) {
		std::array<Glyph, classic_score_name_length> forward{};
		const auto &stored = document_.boards[resident_.rank]
			.rows[*entered_place_].name_backwards;
		for(std::size_t c = 0U; c < forward.size(); ++c) {
			forward[c] = stored[(forward.size() - 1U) - c];
		}
		if(auto updated = new_classic_score_set_name(
			*new_classic_document_, resident_.rank, new_classic_character_,
			*entered_place_, forward
		); !updated) {
			return std::unexpected(new_score_error(updated.error()));
		}
	}
	if(!draw_rows(true)) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::draw_failed
		));
	}
	phase_ = ClassicRegistrationPhase::wait_release;
	name_rerender_ = false;
	return {};
}

bool ClassicRegistrationScene::direction_trigger(
	bool pressed,
	std::uint32_t &hold
) noexcept
{
	if(!pressed) {
		hold = 0U;
		return false;
	}
	const bool trigger = hold == 0U || (hold >= 30U && (hold % 4U) == 0U);
	if(hold != static_cast<std::uint32_t>(-1)) {
		++hold;
	}
	return trigger;
}

bool ClassicRegistrationScene::push_audio_command(
	ClassicAudioCommand command
) noexcept
{
	if(audio_count_ >= audio_commands_.size()) {
		return false;
	}
	const auto tail = (audio_head_ + audio_count_) % audio_commands_.size();
	audio_commands_[tail] = command;
	++audio_count_;
	return true;
}

std::optional<ClassicAudioCommand>
ClassicRegistrationScene::take_audio_command() noexcept
{
	if(audio_count_ == 0U) {
		return std::nullopt;
	}
	const auto result = audio_commands_[audio_head_];
	audio_head_ = (audio_head_ + 1U) % audio_commands_.size();
	--audio_count_;
	return result;
}

ClassicRegistrationTick ClassicRegistrationScene::tick(
	ClassicRegistrationBoundary boundary,
	bool surface_changed,
	bool input_sampled,
	bool selection_changed,
	bool name_changed,
	bool account_playtime
) noexcept
{
	if(account_playtime &&
		(boundary == ClassicRegistrationBoundary::vsync ||
		 boundary == ClassicRegistrationBoundary::audio_volume)) {
		account_new_classic_playtime();
	}
	return {
		.phase = phase_,
		.boundary = boundary,
		.surface_changed = surface_changed,
		.input_sampled = input_sampled,
		.selection_changed = selection_changed,
		.name_changed = name_changed,
		.ready = phase_ == ClassicRegistrationPhase::ready,
	};
}

void ClassicRegistrationScene::account_new_classic_playtime() noexcept
{
	if(!new_classic_document_ || view_only_ || !persist_changes_ ||
		new_classic_score_committed_) {
		return;
	}
	auto &frames = new_classic_document_->boards[
		resident_.rank
	][new_classic_character_].stats.play_frames;
	if(frames != 0xFFFFFFFFU) {
		++frames;
	}
}

std::expected<ClassicRegistrationTick, ClassicRegistrationError>
ClassicRegistrationScene::fade_tick(bool black_in)
{
	bool changed = false;
	const bool new_classic_view = view_only_ && new_classic_document_.has_value();
	if(new_classic_view && fade_boundary_ != 0U && fade_boundary_ <= 17U) {
		const auto pair = static_cast<std::uint8_t>(fade_boundary_ - 1U);
		const auto tone = black_in ?
			static_cast<std::int16_t>(pair * 6U) :
			static_cast<std::int16_t>(100 - pair * 6U);
		raster_.set_palette_tone(tone);
		raster_.apply_pending_palette();
		changed = true;
	} else if(!new_classic_view && fade_boundary_ != 0U &&
		(fade_boundary_ % 2U) == 1U) {
		const auto pair = static_cast<std::uint8_t>(
			(fade_boundary_ - 1U) / 2U
		);
		const auto tone = black_in ?
			static_cast<std::int16_t>(pair * 6U) :
			static_cast<std::int16_t>(100 - pair * 6U);
		raster_.set_palette_tone(tone);
		raster_.apply_pending_palette();
		changed = true;
	}
	++fade_boundary_;
	const auto boundary_count = new_classic_view ?
		new_classic_view_fade_boundary_count : fade_boundary_count;
	if(fade_boundary_ < boundary_count) {
		return tick(ClassicRegistrationBoundary::vsync, changed);
	}

	raster_.set_palette_tone(black_in ? 100 : 0);
	raster_.apply_pending_palette();
	changed = true;
	fade_boundary_ = 0U;
	if(black_in) {
		phase_ = entered_place_ ? ClassicRegistrationPhase::name_entry :
			ClassicRegistrationPhase::wait_release;
		return tick(ClassicRegistrationBoundary::vsync, changed);
	}

	account_new_classic_playtime();
	if(new_classic_document_) {
		if(!view_only_ && persist_changes_) {
			if(auto committed = commit_new_classic_score_store(
				score_path_, *new_classic_document_
			); !committed) {
				fail();
				return std::unexpected(new_store_error(
					ClassicRegistrationErrorCode::store_commit_failed,
					std::move(committed.error())
				));
			}
			new_classic_score_committed_ = true;
		}
	} else if(persist_changes_) {
		document_.codec[resident_.rank] = commit_codec_;
		if(auto committed = commit_classic_score_store(score_path_, document_);
			!committed) {
			fail();
			return std::unexpected(store_error(
				ClassicRegistrationErrorCode::store_commit_failed,
				std::move(committed.error())
			));
		}
	}
	if(!preload_next_screen()) {
		fail();
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::draw_failed
		));
	}
	phase_ = (!retain_high_score_music_ &&
		(resident_.remaining_credits == 0U || resident_.story_stage == 99U)) ?
		ClassicRegistrationPhase::wait_volume :
		ClassicRegistrationPhase::ready;
	auto result = tick(
		ClassicRegistrationBoundary::vsync, changed, false, false, false,
		false
	);
	result.new_classic_score_committed = new_classic_score_committed_;
	return result;
}

void ClassicRegistrationScene::fail() noexcept
{
	phase_ = ClassicRegistrationPhase::failed;
}

std::expected<ClassicRegistrationTick, ClassicRegistrationError>
ClassicRegistrationScene::advance(
	ClassicRegistrationInput input,
	ClassicAudioStatus audio
)
{
	if(audio_count_ != 0U) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::audio_command_pending
		));
	}
	if(phase_ == ClassicRegistrationPhase::ready ||
		phase_ == ClassicRegistrationPhase::consumed ||
		phase_ == ClassicRegistrationPhase::failed) {
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::invalid_state
		));
	}

	if(phase_ == ClassicRegistrationPhase::initial_view_vsync) {
		const auto target = new_classic_raster_page(new_classic_page_);
		if(!raster_.set_shown_page(target) || !raster_.set_access_page(target)) {
			fail();
			return std::unexpected(registration_error(
				ClassicRegistrationErrorCode::draw_failed
			));
		}
		phase_ = ClassicRegistrationPhase::black_in;
		return tick(ClassicRegistrationBoundary::vsync, true);
	}
	if(phase_ == ClassicRegistrationPhase::black_in) {
		return fade_tick(true);
	}
	if(phase_ == ClassicRegistrationPhase::black_out) {
		return fade_tick(false);
	}
	if(phase_ == ClassicRegistrationPhase::wait_volume) {
		// The donor's snd_delay_until_volume() does not test snd_active,
		// leaving Music Off with no driver state that can satisfy this wait.
		if(audio.bgm_active && audio.volume != 255U) {
			return tick(ClassicRegistrationBoundary::audio_volume);
		}
		if(!push_audio_command({
			.type = ClassicAudioCommandType::stop_song,
			.cue = ClassicMusicCue::high_score,
		})) {
			fail();
			return std::unexpected(registration_error(
				ClassicRegistrationErrorCode::invalid_state
			));
		}
		phase_ = ClassicRegistrationPhase::ready;
		return tick(ClassicRegistrationBoundary::immediate);
	}
	if(phase_ == ClassicRegistrationPhase::finalize_name) {
		if(auto finished = finish_name(); !finished) {
			fail();
			return std::unexpected(finished.error());
		}
	}
	if(phase_ == ClassicRegistrationPhase::wait_release) {
		if(input.any()) {
			return tick(
				ClassicRegistrationBoundary::vsync, false, true
			);
		}
		phase_ = ClassicRegistrationPhase::wait_press;
		return tick(
			ClassicRegistrationBoundary::input_poll, false, true
		);
	}
	if(phase_ == ClassicRegistrationPhase::wait_press) {
		if(!input.any()) {
			return tick(
				ClassicRegistrationBoundary::vsync, false, true
			);
		}
		if(view_only_ && new_classic_document_ && (input.right || input.left)) {
			const auto visible = new_classic_document_->extra_unlocked ?
				new_classic_score_character_count :
				new_classic_score_locked_character_count;
			std::uint8_t page = new_classic_page_;
			if(input.right) {
				page = page == new_classic_score_all_characters ? 0U :
					(static_cast<std::size_t>(page) + 1U >= visible ?
					 new_classic_score_all_characters :
					 static_cast<std::uint8_t>(page + 1U));
			} else {
				page = page == new_classic_score_all_characters ?
					static_cast<std::uint8_t>(visible - 1U) :
					(page == 0U ? new_classic_score_all_characters :
					 static_cast<std::uint8_t>(page - 1U));
			}
			if(auto loaded = load_new_classic_page(page); !loaded) {
				fail();
				return std::unexpected(std::move(loaded.error()));
			}
			phase_ = ClassicRegistrationPhase::wait_release;
			return tick(
				ClassicRegistrationBoundary::vsync, true, true,
				true
			);
		}
		if(!retain_high_score_music_ &&
			(resident_.remaining_credits == 0U || resident_.story_stage == 99U) &&
			!push_audio_command({
				.type = ClassicAudioCommandType::fade_song,
				.cue = ClassicMusicCue::high_score,
				.parameter = 16U,
			})) {
			fail();
			return std::unexpected(registration_error(
				ClassicRegistrationErrorCode::invalid_state
			));
		}
		phase_ = ClassicRegistrationPhase::black_out;
		return tick(
			ClassicRegistrationBoundary::input_poll, false, true
		);
	}

	if(phase_ != ClassicRegistrationPhase::name_entry || !entered_place_) {
		fail();
		return std::unexpected(registration_error(
			ClassicRegistrationErrorCode::invalid_state
		));
	}

	bool selection_changed = false;
	bool name_changed = false;
	auto change_selection = [&](std::uint8_t raw) -> bool {
		if(!draw_alphabet_glyph(selected_, false)) {
			return false;
		}
		selected_ = static_cast<Glyph>(raw);
		selection_changed = true;
		name_rerender_ = true;
		return true;
	};

	if(direction_trigger(input.up, direction_hold_[0U]) &&
		selected_ != Glyph::question) {
		auto raw = static_cast<std::int16_t>(glyph_value(selected_)) -
			alphabet_columns;
		if(raw < 0) {
			raw += alphabet_glyphs;
		}
		if(!change_selection(static_cast<std::uint8_t>(raw))) {
			fail();
			return std::unexpected(registration_error(
				ClassicRegistrationErrorCode::draw_failed
			));
		}
	}
	if(direction_trigger(input.down, direction_hold_[1U]) &&
		selected_ != Glyph::question) {
		auto raw = static_cast<std::uint8_t>(
			glyph_value(selected_) + alphabet_columns
		);
		if(raw >= alphabet_glyphs) {
			raw = static_cast<std::uint8_t>(raw - alphabet_glyphs);
		}
		if(!change_selection(raw)) {
			fail();
			return std::unexpected(registration_error(
				ClassicRegistrationErrorCode::draw_failed
			));
		}
	}
	if(direction_trigger(input.left, direction_hold_[2U])) {
		auto raw = glyph_value(selected_);
		if((raw % alphabet_columns) == 0U) {
			raw = static_cast<std::uint8_t>(raw + glyph_value(Glyph::space));
		} else if(selected_ == Glyph::backspace || selected_ == Glyph::space) {
			raw = static_cast<std::uint8_t>(raw - 2U);
		} else {
			--raw;
		}
		if(!change_selection(raw)) {
			fail();
			return std::unexpected(registration_error(
				ClassicRegistrationErrorCode::draw_failed
			));
		}
	}
	if(direction_trigger(input.right, direction_hold_[3U])) {
		auto raw = glyph_value(selected_);
		if((raw % alphabet_columns) == glyph_value(Glyph::space)) {
			raw = static_cast<std::uint8_t>(raw - glyph_value(Glyph::space));
		} else if(selected_ == Glyph::m || selected_ == Glyph::z) {
			raw = static_cast<std::uint8_t>(raw + 2U);
		} else {
			++raw;
		}
		if(!change_selection(raw)) {
			fail();
			return std::unexpected(registration_error(
				ClassicRegistrationErrorCode::draw_failed
			));
		}
	}

	bool done = false;
	bool cursor_exhausted = false;
	const bool enter = input.ok || input.shot;
	if(enter) {
		if(!enter_locked_) {
			if(selected_ == Glyph::backspace) {
				if(!blank_cursor()) {
					fail();
					return std::unexpected(registration_error(
						ClassicRegistrationErrorCode::draw_failed
					));
				}
				if(name_cursor_backwards_ <
					static_cast<std::uint8_t>(classic_score_name_length - 1U)) {
					++name_cursor_backwards_;
				}
			} else if(selected_ == Glyph::end) {
				done = true;
			} else {
				auto &name = document_.boards[resident_.rank]
					.rows[*entered_place_].name_backwards;
				name[name_cursor_backwards_] = selected_;
				if(name_cursor_backwards_ == 0U) {
					done = true;
					cursor_exhausted = true;
				} else {
					--name_cursor_backwards_;
				}
			}
			name_changed = true;
			name_rerender_ = true;
		}
		enter_locked_ = true;
	} else {
		enter_locked_ = false;
	}

	if(!done && input.bomb) {
		if(!back_locked_) {
			if(!blank_cursor()) {
				fail();
				return std::unexpected(registration_error(
					ClassicRegistrationErrorCode::draw_failed
				));
			}
			if(name_cursor_backwards_ <
				static_cast<std::uint8_t>(classic_score_name_length - 1U)) {
				++name_cursor_backwards_;
			}
			name_changed = true;
			name_rerender_ = true;
		}
		back_locked_ = true;
	} else if(!input.bomb) {
		back_locked_ = false;
	}

	if(done) {
		const bool rendered = cursor_exhausted ?
			rerender_completed_name() : rerender_name_cursor();
		if(!rendered) {
			fail();
			return std::unexpected(registration_error(
				ClassicRegistrationErrorCode::draw_failed
			));
		}
		phase_ = ClassicRegistrationPhase::finalize_name;
		name_rerender_ = false;
		return tick(
			ClassicRegistrationBoundary::vsync, true, true,
			selection_changed, name_changed
		);
	}
	if(name_rerender_) {
		if(!rerender_name_cursor()) {
			fail();
			return std::unexpected(registration_error(
				ClassicRegistrationErrorCode::draw_failed
			));
		}
		name_rerender_ = false;
		return tick(
			ClassicRegistrationBoundary::vsync, true, true,
			selection_changed, name_changed
		);
	}
	return tick(
		ClassicRegistrationBoundary::vsync, false, true,
		selection_changed, name_changed
	);
}

std::optional<ClassicRegistrationCompletion>
ClassicRegistrationScene::take_completion() noexcept
{
	if(phase_ != ClassicRegistrationPhase::ready || audio_count_ != 0U) {
		return std::nullopt;
	}
	ClassicRegistrationCompletion result(
		std::move(resident_), std::move(raster_), prepared_->next_screen,
		std::move(prepared_->choice_cels), entered_place_, load_disposition_
	);
	phase_ = ClassicRegistrationPhase::consumed;
	return result;
}

ClassicRegistrationPhase ClassicRegistrationScene::phase() const noexcept
{
	return phase_;
}

const present::Raster &ClassicRegistrationScene::raster() const noexcept
{
	return raster_;
}

std::optional<std::size_t>
ClassicRegistrationScene::entered_place() const noexcept
{
	return entered_place_;
}

} // namespace th03::app
