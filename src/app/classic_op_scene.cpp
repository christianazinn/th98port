#include "classic_op_scene_internal.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "classic_op_music_room_internal.hpp"
#include "classic_op_options_scene_internal.hpp"
#include "classic_op_selection_internal.hpp"
#include "th03/assets/decode.hpp"
#include "th03/present/pi.hpp"

namespace th03::app {
namespace {

constexpr std::int32_t box_left = 160;
constexpr std::int32_t classic_box_top = 256;
constexpr std::int32_t replay_patch_box_top = 264;
constexpr std::int32_t box_main_center_x = 228;
constexpr std::int32_t box_height = 128;
constexpr std::int32_t box_main_width = 136;
constexpr std::int32_t box_submenu_width = 240;
constexpr std::int32_t box_submenu_center_x =
	box_left + (box_submenu_width / 2);
constexpr std::int32_t opwin_width = 16;
constexpr std::int32_t opwin_step_width = 8;
constexpr std::int32_t initial_box_width =
	opwin_width + opwin_step_width;
constexpr std::uint16_t long_shift_frames = 72U;
constexpr std::uint16_t selection_pause_frames = 16U;
constexpr std::uint16_t flash_frames = 16U;
constexpr std::uint16_t fast_fade_frames = 25U;
constexpr std::uint16_t demo_wait_frames = 520U;
constexpr std::uint16_t submenu_grow_frames =
	static_cast<std::uint16_t>(
		(box_submenu_width - box_main_width) / opwin_step_width
	);
constexpr std::uint16_t submenu_shrink_frames =
	static_cast<std::uint16_t>(submenu_grow_frames + 1U);

struct MenuLabel final {
	std::uint8_t first = 0U;
	std::uint8_t length = 0U;
	std::int8_t center_shift = 0;
	std::uint8_t line = 0U;
};

constexpr std::array<MenuLabel, 6U> menu_labels{{
	{0x30U, 3U, 0, 0U},
	{0x33U, 6U, -1, 1U},
	{0x41U, 7U, -1, 2U},
	{0x82U, 5U, -1, 3U},
	{0x3DU, 4U, -1, 4U},
	{0x48U, 3U, -1, 5U},
}};

constexpr std::array<ClassicOpMenuChoice, 6U> classic_menu_choices{
	ClassicOpMenuChoice::story,
	ClassicOpMenuChoice::versus,
	ClassicOpMenuChoice::music_room,
	ClassicOpMenuChoice::high_scores,
	ClassicOpMenuChoice::options,
	ClassicOpMenuChoice::quit,
};

constexpr std::array<ClassicOpMenuChoice, 7U> replay_patch_menu_choices{
	ClassicOpMenuChoice::story,
	ClassicOpMenuChoice::versus,
	ClassicOpMenuChoice::music_room,
	ClassicOpMenuChoice::high_scores,
	ClassicOpMenuChoice::replay,
	ClassicOpMenuChoice::options,
	ClassicOpMenuChoice::quit,
};

constexpr std::array<ClassicOpVsChoice, 4U> classic_vs_choices{
	ClassicOpVsChoice::one_player_cpu,
	ClassicOpVsChoice::one_player_two_player,
	ClassicOpVsChoice::cpu_cpu,
	ClassicOpVsChoice::quit,
};

constexpr std::array<ClassicOpVsChoice, 4U> new_classic_vs_choices{
	ClassicOpVsChoice::one_player_cpu,
	ClassicOpVsChoice::one_player_two_player,
	ClassicOpVsChoice::cpu_cpu,
	ClassicOpVsChoice::online_match,
};

constexpr std::array<NewClassicMessageId, 7U> replay_patch_menu_labels{
	NewClassicMessageId::title_start,
	NewClassicMessageId::title_vs_start,
	NewClassicMessageId::title_music_room,
	NewClassicMessageId::title_hiscore,
	NewClassicMessageId::title_replay,
	NewClassicMessageId::title_option,
	NewClassicMessageId::title_quit,
};

constexpr std::array<std::array<std::int8_t, 2U>, 4U> demo_pairings{{
	{{3, 1}},
	{{5, 13}},
	{{7, 11}},
	{{9, 5}},
}};
constexpr std::array<std::int32_t, 4U> demo_random{{
	600, 1000, 3200, 500,
}};

[[nodiscard]] bool arrange_rules(const GameInit &resident) noexcept
{
	return !stock_gameplay_rules(resident.gameplay_ruleset);
}

[[nodiscard]] bool choice_available(
	const GameInit &resident,
	ClassicOpMenuChoice choice
) noexcept
{
	return !arrange_rules(resident) || choice != ClassicOpMenuChoice::replay;
}

[[nodiscard]] bool vs_choice_available(
	const GameInit &resident,
	ClassicOpVsChoice choice
) noexcept
{
	return !arrange_rules(resident) ||
		choice != ClassicOpVsChoice::online_match;
}

[[nodiscard]] ClassicOpError op_error(
	ClassicOpErrorCode code,
	std::optional<MainResourceKey> resource = std::nullopt
) noexcept
{
	return {.code = code, .resource = resource};
}

[[nodiscard]] ClassicOpError decode_error(
	MainResourceKey resource,
	assets::AssetError error
)
{
	auto result = op_error(ClassicOpErrorCode::asset_decode_failed, resource);
	result.asset_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] bool same_resource_spec(
	const MainResourceSpec &actual,
	const MainResourceSpec &canonical
) noexcept
{
	return actual == canonical;
}

[[nodiscard]] std::expected<void, ClassicOpError> validate_resource_set(
	std::span<const LoadedMainResource> resources,
	bool authenticate_resource_bytes
)
{
	const auto expected = op_title_menu_resource_keys();
	if(resources.size() != expected.size()) {
		return std::unexpected(op_error(
			ClassicOpErrorCode::resource_set_mismatch
		));
	}
	for(std::size_t index = 0U; index < expected.size(); ++index) {
		const auto key = expected[index];
		const auto &resource = resources[index];
		if(resource.spec.key != key) {
			return std::unexpected(op_error(
				ClassicOpErrorCode::resource_set_mismatch, key
			));
		}
		if(!resource.bytes) {
			return std::unexpected(op_error(
				ClassicOpErrorCode::resource_bytes_missing, key
			));
		}
		const auto canonical = main_resource_spec(key);
		if(!canonical || !same_resource_spec(resource.spec, *canonical)) {
			return std::unexpected(op_error(
				ClassicOpErrorCode::resource_set_mismatch, key
			));
		}
		if(!resource.authenticated_override &&
			(resource.bytes->size() != canonical->expected_bytes ||
			(authenticate_resource_bytes && main_resource_fnv1a64(
				*resource.bytes
			) != canonical->expected_fnv1a64))) {
			return std::unexpected(op_error(
				ClassicOpErrorCode::resource_integrity_failed, key
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
[[nodiscard]] std::expected<Value, ClassicOpError> decode_asset(
	std::span<const LoadedMainResource> resources,
	MainResourceKey key,
	Decoder decoder
)
{
	const auto *resource = find_resource(resources, key);
	if(!resource || !resource->bytes) {
		return std::unexpected(op_error(
			ClassicOpErrorCode::resource_set_mismatch, key
		));
	}
	auto decoded = decoder(std::span<const std::byte>{*resource->bytes});
	if(!decoded) {
		return std::unexpected(decode_error(key, std::move(decoded.error())));
	}
	return std::move(*decoded);
}

[[nodiscard]] bool valid_fullscreen_pi(const assets::PiImage &image) noexcept
{
	return image.palette.has_value() &&
		image.image.width == present::graphics_width &&
		image.image.height == present::graphics_height &&
		image.image.indices.size() == present::graphics_page_pixels;
}

[[nodiscard]] ClassicOpRoute route_for_choice(
	ClassicOpMenuChoice choice
) noexcept
{
	switch(choice) {
	case ClassicOpMenuChoice::story: return ClassicOpRoute::story;
	case ClassicOpMenuChoice::versus: return ClassicOpRoute::versus;
	case ClassicOpMenuChoice::music_room: return ClassicOpRoute::music_room;
	case ClassicOpMenuChoice::high_scores: return ClassicOpRoute::high_scores;
	case ClassicOpMenuChoice::options: return ClassicOpRoute::options;
	case ClassicOpMenuChoice::replay: return ClassicOpRoute::replay;
	case ClassicOpMenuChoice::quit: return ClassicOpRoute::quit;
	}
	return ClassicOpRoute::quit;
}

} // namespace

ClassicOpCompletion::ClassicOpCompletion(
	ClassicOpRoute route,
	std::optional<ClassicOpSelectionMode> selection_mode,
	bool reset_vs_selection,
	GameInit resident,
	present::Raster raster,
	present::Tram tram
) noexcept :
	route_(route),
	selection_mode_(selection_mode),
	reset_vs_selection_(reset_vs_selection),
	resident_(std::move(resident)),
	raster_(std::move(raster)),
	tram_(std::move(tram))
{}

ClassicOpRoute ClassicOpCompletion::route() const noexcept
{
	return route_;
}

bool ClassicOpCompletion::resets_vs_selection() const noexcept
{
	return reset_vs_selection_;
}

std::optional<ClassicOpSelectionMode>
ClassicOpCompletion::selection_mode() const noexcept
{
	return selection_mode_;
}

const GameInit &ClassicOpCompletion::resident() const noexcept
{
	return resident_;
}

const present::Raster &ClassicOpCompletion::raster() const noexcept
{
	return raster_;
}

const present::Tram &ClassicOpCompletion::tram() const noexcept
{
	return tram_;
}

ClassicOpTitleMenuScene::ClassicOpTitleMenuScene(
	std::unique_ptr<Prepared> prepared,
	GameInit resident,
	EntryPolicy policy,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	bool restart_bgm
) noexcept :
	prepared_(std::move(prepared)),
	patch_assets_(std::move(patch_assets)),
	resident_(std::move(resident)),
	page_snapshot_(present::graphics_page_pixels, 0U),
	restart_bgm_(restart_bgm)
{
	tram_.fill({});
	prepared_->gaiji->install(tram_);
	increment_rand_after_box_ = policy == EntryPolicy::fullscreen_return;
	if(policy == EntryPolicy::startup_long) {
		static_cast<void>(push_audio_command({
			.type = ClassicAudioCommandType::stop_song,
		}));
		static_cast<void>(push_audio_command({
			.type = ClassicAudioCommandType::load_song,
			.cue = ClassicMusicCue::menu_main,
		}));
	}
	if(policy == EntryPolicy::startup_long) {
		resident_.fast_op_animation = 1U;
	} else if(policy == EntryPolicy::startup_fast) {
		resident_.fast_op_animation = 0U;
	}
	const auto initialized = (policy == EntryPolicy::startup_long) ?
		initialize_long_title() : initialize_fast_title();
	if(!initialized) {
		fail();
	}
}

std::expected<std::unique_ptr<ClassicOpTitleMenuScene>, ClassicOpError>
ClassicOpTitleMenuScene::return_from_other_screen(
	GameInit resident,
	std::span<const LoadedMainResource> resources,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	bool restart_bgm
)
{
	auto prepared = prepare_resources(resources, true);
	if(!prepared) {
		return std::unexpected(std::move(prepared.error()));
	}
	auto result = std::unique_ptr<ClassicOpTitleMenuScene>(
		new ClassicOpTitleMenuScene(
			std::move(*prepared), std::move(resident),
			EntryPolicy::fullscreen_return, std::move(patch_assets), restart_bgm
		)
	);
	if(result->phase_ == ClassicOpPhase::failed) {
		return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
	}
	return result;
}

ClassicOpTitleMenuScene::ClassicOpTitleMenuScene(
	std::unique_ptr<Prepared> prepared,
	GameInit resident,
	present::Raster raster,
	present::Tram tram,
	bool input_allowed,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets
) noexcept :
	prepared_(std::move(prepared)),
	patch_assets_(std::move(patch_assets)),
	resident_(std::move(resident)),
	raster_(std::move(raster)),
	tram_(std::move(tram)),
	page_snapshot_(present::graphics_page_pixels, 0U),
	phase_(ClassicOpPhase::main_menu),
	selected_(ClassicOpMenuChoice::options),
	phase_frame_(1U),
	menu_input_allowed_(input_allowed)
{
	if(!patch_assets_) {
		return;
	}
	const auto visible = raster_.graphics(present::Page::zero);
	const auto bare = raster_.graphics(present::Page::one);
	if(visible.size() != page_snapshot_.size() ||
		bare.size() != page_snapshot_.size()) {
		fail();
		return;
	}
	std::copy(visible.begin(), visible.end(), page_snapshot_.begin());
	const auto top = ((replay_patch_box_top / present::gaiji_height) + 1) *
		present::gaiji_height;
	for(std::int32_t y = top;
		y < top + (7 * present::gaiji_height); ++y) {
		for(std::int32_t x = box_left + 2;
			x < box_left + box_main_width - 2; ++x) {
			const auto offset = (static_cast<std::size_t>(y) *
				present::graphics_width) + static_cast<std::size_t>(x);
			page_snapshot_[offset] =
				(((x + (y - replay_patch_box_top)) & 1) == 0) ?
					3U : bare[offset];
		}
	}
	menu_base_snapshot_valid_ = true;
}

std::expected<std::unique_ptr<ClassicOpTitleMenuScene>, ClassicOpError>
ClassicOpTitleMenuScene::create(
	GameInit resident,
	ClassicOpTitleMode title_mode,
	std::span<const LoadedMainResource> resources,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets
)
{
	return create_impl(
		std::move(resident), title_mode, resources, true,
		std::move(patch_assets)
	);
}

std::expected<std::unique_ptr<ClassicOpTitleMenuScene>, ClassicOpError>
ClassicOpTitleMenuScene::create_impl(
	GameInit resident,
	ClassicOpTitleMode title_mode,
	std::span<const LoadedMainResource> resources,
	bool authenticate_resource_bytes,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets
)
{
	const bool mode_matches_resident =
		(title_mode == ClassicOpTitleMode::long_animation) ?
			(resident.fast_op_animation == 0U) :
			(resident.fast_op_animation != 0U);
	if(!mode_matches_resident) {
		return std::unexpected(op_error(ClassicOpErrorCode::invalid_state));
	}
	auto prepared = prepare_resources(resources, authenticate_resource_bytes);
	if(!prepared) {
		return std::unexpected(std::move(prepared.error()));
	}
	const auto policy = (title_mode == ClassicOpTitleMode::long_animation) ?
		EntryPolicy::startup_long : EntryPolicy::startup_fast;
	auto result = std::unique_ptr<ClassicOpTitleMenuScene>(
		new ClassicOpTitleMenuScene(
			std::move(*prepared), std::move(resident), policy,
			std::move(patch_assets)
		)
	);
	if(result->phase_ == ClassicOpPhase::failed) {
		return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
	}
	return result;
}

std::expected<std::unique_ptr<ClassicOpTitleMenuScene>, ClassicOpError>
ClassicOpTitleMenuScene::resume_from_options(
	const ClassicOpOptionsCompletion &completion,
	std::span<const LoadedMainResource> resources,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	std::optional<GameInit> resident_override
)
{
	if(completion.raster().raster_mode() != present::RasterMode::lines_400 ||
		completion.raster().shown_page() != present::Page::zero ||
		completion.raster().access_page() != present::Page::zero) {
		return std::unexpected(op_error(ClassicOpErrorCode::invalid_state));
	}
	auto prepared = prepare_resources(resources, true);
	if(!prepared) {
		return std::unexpected(std::move(prepared.error()));
	}
	auto result = std::unique_ptr<ClassicOpTitleMenuScene>(
		new ClassicOpTitleMenuScene(
			std::move(*prepared), resident_override.value_or(completion.resident()),
			completion.raster(),
			completion.tram(), completion.main_input_allowed(),
			std::move(patch_assets)
		)
	);
	if(result->phase_ == ClassicOpPhase::failed) {
		return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
	}
	return result;
}

std::expected<std::unique_ptr<ClassicOpTitleMenuScene>, ClassicOpError>
ClassicOpTitleMenuScene::return_from_music_room(
	const ClassicOpMusicRoomCompletion &completion,
	std::span<const LoadedMainResource> resources,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets
)
{
	auto prepared = prepare_resources(resources, true);
	if(!prepared) {
		return std::unexpected(std::move(prepared.error()));
	}
	auto result = std::unique_ptr<ClassicOpTitleMenuScene>(
		new ClassicOpTitleMenuScene(
			std::move(*prepared), completion.resident(),
			EntryPolicy::fullscreen_return, std::move(patch_assets),
			completion.carry().track_playing != 0U
		)
	);
	if(result->phase_ == ClassicOpPhase::failed) {
		return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
	}
	return result;
}

std::expected<std::unique_ptr<ClassicOpTitleMenuScene>, ClassicOpError>
ClassicOpTitleMenuScene::return_from_canceled_selection(
	const ClassicOpSelectionCompletion &completion,
	std::span<const LoadedMainResource> resources,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets
)
{
	if(completion.outcome() != ClassicOpSelectionOutcome::canceled) {
		return std::unexpected(op_error(ClassicOpErrorCode::invalid_state));
	}
	auto prepared = prepare_resources(resources, true);
	if(!prepared) {
		return std::unexpected(std::move(prepared.error()));
	}
	auto result = std::unique_ptr<ClassicOpTitleMenuScene>(
		new ClassicOpTitleMenuScene(
			std::move(*prepared), completion.resident(),
			EntryPolicy::fullscreen_return, std::move(patch_assets)
		)
	);
	if(result->phase_ == ClassicOpPhase::failed) {
		return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
	}
	return result;
}

std::expected<
	std::unique_ptr<ClassicOpTitleMenuScene::Prepared>,
	ClassicOpError
> ClassicOpTitleMenuScene::prepare_resources(
	std::span<const LoadedMainResource> resources,
	bool authenticate_resource_bytes
)
{
	auto validated = validate_resource_set(
		resources, authenticate_resource_bytes
	);
	if(!validated) {
		return std::unexpected(std::move(validated.error()));
	}
	auto long_background = decode_asset<assets::PiImage>(
		resources, {.role = MainResourceRole::op_title_long_background},
		assets::decode_pi
	);
	if(!long_background) {
		return std::unexpected(std::move(long_background.error()));
	}
	auto menu_background = decode_asset<assets::PiImage>(
		resources, {.role = MainResourceRole::op_menu_background},
		assets::decode_pi
	);
	if(!menu_background) {
		return std::unexpected(std::move(menu_background.error()));
	}
	auto window_sheet = decode_asset<assets::BfntSheet>(
		resources, {.role = MainResourceRole::op_window_sheet},
		[](std::span<const std::byte> bytes) {
			return assets::decode_bfnt(
				bytes, assets::BfntConsumerPolicy::superimpose
			);
		}
	);
	if(!window_sheet) {
		return std::unexpected(std::move(window_sheet.error()));
	}
	auto gaiji = decode_asset<assets::BfntSheet>(
		resources, {.role = MainResourceRole::op_gaiji_font},
		[](std::span<const std::byte> bytes) {
			return assets::decode_bfnt(
				bytes, assets::BfntConsumerPolicy::gaiji
			);
		}
	);
	if(!gaiji) {
		return std::unexpected(std::move(gaiji.error()));
	}
	if(!valid_fullscreen_pi(*long_background) ||
		!valid_fullscreen_pi(*menu_background) ||
		window_sheet->header.pixel_width != 16U ||
		window_sheet->header.pixel_height != 128U ||
		window_sheet->header.first_code != 0U ||
		window_sheet->header.last_code != 2U) {
		return std::unexpected(op_error(
			ClassicOpErrorCode::invalid_asset_geometry
		));
	}
	auto prepared_window = present::prepare_superimpose_sheet(*window_sheet);
	if(!prepared_window) {
		return std::unexpected(op_error(
			ClassicOpErrorCode::invalid_asset_geometry
		));
	}
	auto prepared_gaiji = PreparedGameftGaiji::prepare(*gaiji);
	if(!prepared_gaiji) {
		auto error = op_error(ClassicOpErrorCode::font_prepare_failed);
		error.text_error.emplace(prepared_gaiji.error());
		return std::unexpected(std::move(error));
	}
	auto prepared = std::make_unique<Prepared>();
	prepared->long_background = std::move(*long_background);
	prepared->menu_background = std::move(*menu_background);
	prepared->window_sheet.emplace(std::move(*prepared_window));
	prepared->gaiji.emplace(std::move(*prepared_gaiji));
	return prepared;
}

std::expected<std::unique_ptr<ClassicOpTitleMenuScene>, ClassicOpError>
ClassicOpTitleMenuScene::create_predecoded(
	GameInit resident,
	ClassicOpTitleMode title_mode,
	assets::PiImage long_background,
	assets::PiImage menu_background,
	assets::BfntSheet window_sheet,
	assets::BfntSheet gaiji,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets
)
{
	const bool mode_matches_resident =
		(title_mode == ClassicOpTitleMode::long_animation) ?
			(resident.fast_op_animation == 0U) :
			(resident.fast_op_animation != 0U);
	if(!mode_matches_resident) {
		return std::unexpected(op_error(ClassicOpErrorCode::invalid_state));
	}
	if(!valid_fullscreen_pi(long_background) ||
		!valid_fullscreen_pi(menu_background) ||
		window_sheet.header.pixel_width != 16U ||
		window_sheet.header.pixel_height != 128U ||
		window_sheet.header.first_code != 0U ||
		window_sheet.header.last_code != 2U) {
		return std::unexpected(op_error(
			ClassicOpErrorCode::invalid_asset_geometry
		));
	}
	auto prepared_window = present::prepare_superimpose_sheet(window_sheet);
	if(!prepared_window) {
		return std::unexpected(op_error(
			ClassicOpErrorCode::invalid_asset_geometry
		));
	}
	auto prepared_gaiji = PreparedGameftGaiji::prepare(gaiji);
	if(!prepared_gaiji) {
		auto error = op_error(ClassicOpErrorCode::font_prepare_failed);
		error.text_error.emplace(prepared_gaiji.error());
		return std::unexpected(std::move(error));
	}

	auto prepared = std::make_unique<Prepared>();
	prepared->long_background = std::move(long_background);
	prepared->menu_background = std::move(menu_background);
	prepared->window_sheet.emplace(std::move(*prepared_window));
	prepared->gaiji.emplace(std::move(*prepared_gaiji));
	auto result = std::unique_ptr<ClassicOpTitleMenuScene>(
		new ClassicOpTitleMenuScene(
			std::move(prepared), std::move(resident),
			(title_mode == ClassicOpTitleMode::long_animation) ?
				EntryPolicy::startup_long : EntryPolicy::startup_fast,
			std::move(patch_assets)
		)
	);
	if(result->phase_ == ClassicOpPhase::failed) {
		return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
	}
	return result;
}

std::expected<ClassicOpCompletion, ClassicOpError>
ClassicOpTitleMenuScene::direct_vs_reentry(
	GameInit resident,
	ClassicOpSelectionMode mode,
	std::span<const LoadedMainResource> resources
)
{
	if(mode == ClassicOpSelectionMode::story) {
		return std::unexpected(op_error(ClassicOpErrorCode::invalid_state));
	}
	auto prepared = prepare_resources(resources, true);
	if(!prepared) {
		return std::unexpected(std::move(prepared.error()));
	}
	present::Raster raster{};
	present::Tram tram{};
	tram.fill({});
	prepared.value()->gaiji->install(tram);
	return ClassicOpCompletion{
		ClassicOpRoute::versus,
		mode,
		false,
		std::move(resident),
		std::move(raster),
		std::move(tram),
	};
}

bool ClassicOpTitleMenuScene::apply_palette(
	const assets::Palette16Rgb4 &palette
) noexcept
{
	for(std::size_t index = 0U; index < palette.colors.size(); ++index) {
		if(!raster_.set_pending_palette_entry(
			static_cast<std::uint8_t>(index), palette.colors[index]
		)) {
			return false;
		}
	}
	return true;
}

bool ClassicOpTitleMenuScene::shift_access_page_left(
	std::uint16_t pixels
) noexcept
{
	if(pixels == 0U || pixels >= present::graphics_width ||
		page_snapshot_.size() != present::graphics_page_pixels) {
		return false;
	}
	const auto page = raster_.access_page();
	const auto source = raster_.graphics(page);
	if(source.size() != page_snapshot_.size()) {
		return false;
	}
	std::copy(source.begin(), source.end(), page_snapshot_.begin());
	// egc_shift_left_all() programs a [640 - pixels]-bit transfer. It does
	// not clear the vacated edge; the rightmost pixels retain their previous
	// VRAM contents while the copied prefix shifts left.
	const present::IndexedSource shifted{
		std::span<const present::Index4>(page_snapshot_).subspan(pixels),
		static_cast<std::uint16_t>(present::graphics_width - pixels),
		present::graphics_height,
		present::graphics_width,
	};
	return raster_.blit(shifted, {0, 0});
}

bool ClassicOpTitleMenuScene::initialize_long_title() noexcept
{
	const auto &image = prepared_->long_background;
	if(!image.palette ||
		!raster_.set_access_page(present::Page::one) ||
		!present::pi_put_8(raster_, 0, 0, image) ||
		!raster_.set_access_page(present::Page::zero) ||
		!apply_palette(*image.palette) ||
		!present::pi_put_8(raster_, 0, 0, image) ||
		!shift_access_page_left(2U) ||
		!raster_.set_pending_palette_entry_rgb8(15U, {0U, 0U, 0U}) ||
		!raster_.set_pending_palette_entry_rgb8(11U, {0U, 0U, 0U}) ||
		!raster_.set_shown_page(present::Page::one)) {
		return false;
	}
	raster_.set_palette_tone(0);
	raster_.apply_pending_palette();
	title_page_ = present::Page::zero;
	brightness_ = 0;
	phase_frame_ = 0U;
	phase_ = ClassicOpPhase::long_shift;
	return true;
}

bool ClassicOpTitleMenuScene::initialize_fast_title() noexcept
{
	raster_.set_palette_tone(0);
	raster_.apply_pending_palette();
	phase_frame_ = 0U;
	phase_ = ClassicOpPhase::fast_black;
	return true;
}

bool ClassicOpTitleMenuScene::draw_menu_background(bool populate_hidden) noexcept
{
	const auto &image = prepared_->menu_background;
	if(!image.palette || !apply_palette(*image.palette)) {
		return false;
	}
	raster_.apply_pending_palette();
	// OP keeps page 1 as the bare title backing for every proportional-font
	// unput while page 0 owns the animated window. Leaving page 1 at reset or
	// at TL01 makes later VS/Option text restoration punch black holes through
	// OPWIN's transparent checker.
	return (!populate_hidden || (
			raster_.set_access_page(present::Page::one) &&
			present::pi_put_8(raster_, 0, 0, image)
		)) &&
		raster_.set_access_page(present::Page::zero) &&
		present::pi_put_8(raster_, 0, 0, image) &&
		raster_.set_shown_page(present::Page::zero);
}

bool ClassicOpTitleMenuScene::draw_hidden_menu_background() noexcept
{
	return raster_.set_access_page(present::Page::one) &&
		present::pi_put_8(raster_, 0, 0, prepared_->menu_background) &&
		raster_.set_access_page(present::Page::zero);
}

bool ClassicOpTitleMenuScene::restore_menu_background(
	std::int32_t left,
	std::int32_t top,
	std::uint16_t width,
	std::uint16_t height
) noexcept
{
	const auto &image = prepared_->menu_background.image;
	if(left < 0 || top < 0 ||
		static_cast<std::uint32_t>(left) + width > image.width ||
		static_cast<std::uint32_t>(top) + height > image.height) {
		return false;
	}
	const auto offset =
		(static_cast<std::size_t>(top) * image.width) +
		static_cast<std::size_t>(left);
	const present::IndexedSource source{
		std::span<const present::Index4>(image.indices).subspan(offset),
		width,
		height,
		image.width,
	};
	return raster_.blit(source, {left, top});
}

bool ClassicOpTitleMenuScene::draw_window_glyph(
	std::int32_t left,
	std::uint16_t glyph
) noexcept
{
	return prepared_->window_sheet && present::superimpose_put(
		raster_, *prepared_->window_sheet, {left, box_top_value()}, glyph
	);
}

std::int32_t ClassicOpTitleMenuScene::box_top_value() const noexcept
{
	return patch_assets_ ? replay_patch_box_top : classic_box_top;
}

bool ClassicOpTitleMenuScene::draw_choice(
	ClassicOpMenuChoice choice,
	present::TramAttribute attribute
) noexcept
{
	if(patch_assets_) {
		const auto found = std::find(
			replay_patch_menu_choices.begin(), replay_patch_menu_choices.end(),
			choice
		);
		if(found == replay_patch_menu_choices.end() ||
			!menu_base_snapshot_valid_ ||
			page_snapshot_.size() != present::graphics_page_pixels) {
			return false;
		}
		const auto row = static_cast<std::size_t>(
			found - replay_patch_menu_choices.begin()
		);
		const auto top = ((replay_patch_box_top / present::gaiji_height) + 1 +
			static_cast<std::int32_t>(row)) * present::gaiji_height;
		const auto offset = (static_cast<std::size_t>(top) *
			present::graphics_width) + static_cast<std::size_t>(box_left);
		const present::IndexedSource source{
			std::span<const present::Index4>(page_snapshot_).subspan(offset),
			static_cast<std::uint16_t>(box_main_width),
			present::gaiji_height,
			present::graphics_width,
		};
		const auto color = static_cast<present::Index4>(
			!choice_available(resident_, choice) ? 8U :
			(attribute == present::tram_black ? 0U : 15U)
		);
		const auto clip = present::Rect{
			box_left + 2, top,
			static_cast<std::uint16_t>(box_main_width - 4),
			present::gaiji_height,
		};
		const auto shadow = static_cast<present::Index4>(
			color == 0U ? 15U : 0U
		);
		return raster_.set_access_page(present::Page::zero) &&
			raster_.blit(source, {box_left, top}) &&
			patch_assets_->menu_font.put_centered_clipped(
				raster_, box_main_center_x + 1, top + 1,
				patch_assets_->messages().text(replay_patch_menu_labels[row]),
				shadow, clip
			) &&
			patch_assets_->menu_font.put_centered(
				raster_, box_main_center_x, top,
				patch_assets_->messages().text(replay_patch_menu_labels[row]),
				color
			);
	}
	const auto found = std::find(
		classic_menu_choices.begin(), classic_menu_choices.end(), choice
	);
	if(found == classic_menu_choices.end()) {
		return false;
	}
	const auto index = static_cast<std::size_t>(
		found - classic_menu_choices.begin()
	);
	const auto &label = menu_labels[index];
	const auto shifted_width = static_cast<std::int32_t>(
		static_cast<std::int16_t>(label.length) + label.center_shift
	) * present::gaiji_width;
	const auto tram_left =
		(box_main_center_x - (shifted_width / 2)) / 8;
	const auto tram_y =
		(box_top_value() / present::gaiji_height) + 1 + label.line;
	for(std::uint8_t glyph = 0U; glyph < label.length; ++glyph) {
		if(!tram_.put_gaiji(
			{tram_left + (static_cast<std::int32_t>(glyph) * 2), tram_y},
			static_cast<std::uint8_t>(label.first + glyph),
			attribute
		)) {
			return false;
		}
	}
	return true;
}

bool ClassicOpTitleMenuScene::draw_main_choices() noexcept
{
	const auto page = raster_.graphics(present::Page::zero);
	if(patch_assets_ && !menu_base_snapshot_valid_) {
		if(page.size() != page_snapshot_.size()) {
			return false;
		}
		std::copy(page.begin(), page.end(), page_snapshot_.begin());
		menu_base_snapshot_valid_ = true;
	}
	const auto choices = patch_assets_ ?
		std::span<const ClassicOpMenuChoice>(replay_patch_menu_choices) :
		std::span<const ClassicOpMenuChoice>(classic_menu_choices);
	for(const auto choice : choices) {
		if(!draw_choice(
			choice,
			(choice == selected_) ? present::tram_white : present::tram_black
		)) {
			return false;
		}
	}
	if(patch_assets_ && !draw_title_credit()) {
		return false;
	}
	menu_input_allowed_ = true;
	return true;
}

bool ClassicOpTitleMenuScene::restore_patch_choice_rows(
	std::int32_t box_right
) noexcept
{
	if(!patch_assets_ || !menu_base_snapshot_valid_ ||
		box_right <= box_left + 4 || box_right > present::graphics_width) {
		return false;
	}
	const auto top = ((replay_patch_box_top / present::gaiji_height) + 1) *
		present::gaiji_height;
	const auto left = box_left + 2;
	const auto right = box_right - 2;
	const auto bare = raster_.graphics(present::Page::one);
	if(bare.size() != present::graphics_page_pixels ||
		!raster_.set_access_page(present::Page::zero)) {
		return false;
	}
	for(std::int32_t y = top;
		y < top + (7 * present::gaiji_height); ++y) {
		for(std::int32_t x = left; x < right; ++x) {
			const auto offset = (static_cast<std::size_t>(y) *
				present::graphics_width) + static_cast<std::size_t>(x);
			if(!raster_.plot({x, y}, bare[offset])) {
				return false;
			}
			if(((x + (y - replay_patch_box_top)) & 1) == 0 &&
				!raster_.plot({x, y}, 3U)) {
				return false;
			}
		}
	}
	return true;
}

bool ClassicOpTitleMenuScene::restore_patch_menu_rows(
	std::int32_t box_right
) noexcept
{
	if(!restore_patch_choice_rows(box_right)) {
		return false;
	}
	const auto bare = raster_.graphics(present::Page::one);
	return raster_.blit({
		.pixels = bare,
		.width = present::graphics_width,
		.height = 32U,
		.stride = present::graphics_width,
	}, {0, 0});
}

bool ClassicOpTitleMenuScene::draw_title_credit() noexcept
{
	if(!patch_assets_ || !raster_.set_access_page(present::Page::zero)) {
		return !patch_assets_;
	}
	return patch_assets_->menu_font.put_right(
		raster_, 638, 0,
		patch_assets_->messages().text(NewClassicMessageId::credit_project), 0U
	) && patch_assets_->menu_font.put_right(
		raster_, 638, 16,
		patch_assets_->messages().text(NewClassicMessageId::credit_patch), 0U
	) && (!arrange_rules(resident_) || patch_assets_->menu_font.put_right(
		raster_, 638, 32, "DOPAMINE ARRANGE", 12U
	));
}

bool ClassicOpTitleMenuScene::update_selection(int direction) noexcept
{
	const auto old = selected_;
	const auto choices = patch_assets_ ?
		std::span<const ClassicOpMenuChoice>(replay_patch_menu_choices) :
		std::span<const ClassicOpMenuChoice>(classic_menu_choices);
	const auto found = std::find(choices.begin(), choices.end(), selected_);
	if(found == choices.end()) {
		return false;
	}
	auto next = static_cast<int>(found - choices.begin()) + direction;
	if(next < 0) {
		next = static_cast<int>(choices.size() - 1U);
	} else if(next >= static_cast<int>(choices.size())) {
		next = 0;
	}
	while(!choice_available(resident_, choices[static_cast<std::size_t>(next)])) {
		next += direction;
		if(next < 0) {
			next = static_cast<int>(choices.size() - 1U);
		} else if(next >= static_cast<int>(choices.size())) {
			next = 0;
		}
	}
	selected_ = choices[static_cast<std::size_t>(next)];
	return draw_choice(old, present::tram_black) &&
		draw_choice(selected_, present::tram_white);
}

bool ClassicOpTitleMenuScene::draw_vs_choice(
	ClassicOpVsChoice choice,
	present::TramAttribute attribute
) noexcept
{
	constexpr std::array<std::array<std::uint8_t, 8U>, 3U> labels{{
		{{0x88U, 0x89U, 0x8AU, 0x8BU, 0x8CU, 0x8DU, 0x8EU, 0x8FU}},
		{{0x88U, 0x89U, 0x8AU, 0x8BU, 0x96U, 0x97U, 0x98U, 0x99U}},
		{{0x92U, 0x93U, 0x94U, 0x95U, 0x8CU, 0x8DU, 0x8EU, 0x8FU}},
	}};
	const auto choices = patch_assets_ ?
		std::span<const ClassicOpVsChoice>(new_classic_vs_choices) :
		std::span<const ClassicOpVsChoice>(classic_vs_choices);
	const auto found = std::find(choices.begin(), choices.end(), choice);
	if(found == choices.end()) {
		return false;
	}
	const auto row = static_cast<std::size_t>(found - choices.begin());
	const auto raw = static_cast<std::size_t>(choice);
	const auto tram_y = (box_top_value() / present::gaiji_height) + 1 +
		static_cast<std::int32_t>(row + 1U);
	if(choice == ClassicOpVsChoice::online_match) {
		const auto top = tram_y * present::gaiji_height;
		const auto bare = raster_.graphics(present::Page::one);
		if(!patch_assets_ || bare.size() != present::graphics_page_pixels ||
			!raster_.set_access_page(present::Page::zero)) {
			return false;
		}
		for(std::int32_t y = top; y < top + present::gaiji_height; ++y) {
			for(std::int32_t x = box_left + 2;
				x < box_left + box_submenu_width - 2; ++x) {
				const auto offset = (static_cast<std::size_t>(y) *
					present::graphics_width) + static_cast<std::size_t>(x);
				if(!raster_.plot({x, y}, bare[offset]) ||
					(((x + (y - replay_patch_box_top)) & 1) == 0 &&
					!raster_.plot({x, y}, 3U))) {
					return false;
				}
			}
		}
		const auto color = static_cast<present::Index4>(
			!vs_choice_available(resident_, choice) ? 8U :
			(attribute == present::tram_black ? 0U : 15U)
		);
		const auto center = box_submenu_center_x + 4;
		const auto text = patch_assets_->messages().text(
			NewClassicMessageId::netplay_online_match
		);
		const auto shadow = static_cast<present::Index4>(
			color == 0U ? 15U : 0U
		);
		if(!patch_assets_->menu_font.put_centered_clipped(
			raster_, center + 1, top + 1, text, shadow,
			{
				box_left + 2, top,
				static_cast<std::uint16_t>(box_submenu_width - 4),
				present::gaiji_height,
			}
		)) {
			return false;
		}
		return patch_assets_->menu_font.put_centered(
			raster_, center, top, text, color
		);
	}
	if(choice == ClassicOpVsChoice::quit) {
		if(patch_assets_) {
			return false;
		}
		constexpr std::uint8_t first_glyph = 0x48U;
		constexpr std::uint8_t glyph_count = 3U;
		const auto tram_left = (box_submenu_center_x -
			((glyph_count * present::gaiji_width) / 2)) / 8;
		for(std::uint8_t glyph = 0U; glyph < glyph_count; ++glyph) {
			if(!tram_.put_gaiji(
				{tram_left + (static_cast<std::int32_t>(glyph) * 2), tram_y},
				static_cast<std::uint8_t>(first_glyph + glyph), attribute
			)) {
				return false;
			}
		}
		return true;
	}
	if(raw >= labels.size()) {
		return false;
	}
	const std::int32_t tram_left = patch_assets_ ? 28 : 27;
	for(std::size_t glyph = 0U; glyph < labels[raw].size(); ++glyph) {
		if(!tram_.put_gaiji(
			{tram_left + (static_cast<std::int32_t>(glyph) * 2), tram_y},
			labels[raw][glyph], attribute
		)) {
			return false;
		}
	}
	return true;
}

bool ClassicOpTitleMenuScene::draw_vs_choices() noexcept
{
	const auto choices = patch_assets_ ?
		std::span<const ClassicOpVsChoice>(new_classic_vs_choices) :
		std::span<const ClassicOpVsChoice>(classic_vs_choices);
	for(const auto choice : choices) {
		if(!draw_vs_choice(
			choice,
			(choice == selected_vs_) ?
				present::tram_white : present::tram_black
		)) {
			return false;
		}
	}
	menu_input_allowed_ = true;
	return true;
}

bool ClassicOpTitleMenuScene::update_vs_selection(int direction) noexcept
{
	const auto old = selected_vs_;
	const auto choices = patch_assets_ ?
		std::span<const ClassicOpVsChoice>(new_classic_vs_choices) :
		std::span<const ClassicOpVsChoice>(classic_vs_choices);
	const auto found = std::find(choices.begin(), choices.end(), selected_vs_);
	if(found == choices.end()) {
		return false;
	}
	auto next = static_cast<int>(found - choices.begin()) + direction;
	if(next < 0) {
		next = static_cast<int>(choices.size() - 1U);
	} else if(next >= static_cast<int>(choices.size())) {
		next = 0;
	}
	while(!vs_choice_available(
		resident_, choices[static_cast<std::size_t>(next)]
	)) {
		next += direction;
		if(next < 0) {
			next = static_cast<int>(choices.size() - 1U);
		} else if(next >= static_cast<int>(choices.size())) {
			next = 0;
		}
	}
	selected_vs_ = choices[static_cast<std::size_t>(next)];
	return draw_vs_choice(old, present::tram_black) &&
		draw_vs_choice(selected_vs_, present::tram_white);
}

bool ClassicOpTitleMenuScene::grow_box_step() noexcept
{
	if(!restore_menu_background(
		box_right_left_, box_top_value(),
		static_cast<std::uint16_t>(opwin_step_width * 2),
		static_cast<std::uint16_t>(box_height)
	) || !draw_window_glyph(box_right_left_, 2U)) {
		return false;
	}
	box_right_left_ += opwin_step_width;
	return true;
}

bool ClassicOpTitleMenuScene::shrink_box_step() noexcept
{
	if(!restore_menu_background(
		box_right_left_, box_top_value(),
		static_cast<std::uint16_t>(opwin_step_width * 2),
		static_cast<std::uint16_t>(box_height)
	) || !draw_window_glyph(box_right_left_ - opwin_step_width, 2U)) {
		return false;
	}
	box_right_left_ -= opwin_step_width;
	return true;
}

bool ClassicOpTitleMenuScene::begin_menu_swap() noexcept
{
	raster_.set_palette_tone(200);
	if(!draw_menu_background(false) || !push_audio_command({
		.type = ClassicAudioCommandType::play_song,
		.cue = ClassicMusicCue::menu_main,
	})) {
		return false;
	}
	return true;
}

bool ClassicOpTitleMenuScene::begin_box_animation() noexcept
{
	if(!raster_.set_access_page(present::Page::zero) ||
		!draw_window_glyph(box_left, 0U)) {
		return false;
	}
	box_right_left_ = box_left + initial_box_width - opwin_step_width;
	phase_frame_ = 0U;
	return true;
}

bool ClassicOpTitleMenuScene::box_step() noexcept
{
	if(!restore_menu_background(
		box_right_left_, box_top_value(),
		static_cast<std::uint16_t>(opwin_step_width * 2),
		static_cast<std::uint16_t>(box_height)
	) || !draw_window_glyph(box_right_left_, 2U)) {
		return false;
	}
	box_right_left_ += opwin_step_width;
	return true;
}

bool ClassicOpTitleMenuScene::push_audio_command(
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

void ClassicOpTitleMenuScene::select_route(
	ClassicOpRoute route,
	std::optional<ClassicOpSelectionMode> selection_mode
) noexcept
{
	if(route == ClassicOpRoute::high_scores) {
		resident_.story_stage = 0xFFU;
		resident_.show_score_menu = 1U;
		resident_.game_mode = game_mode::none;
		resident_.last_score = {};
		static_cast<void>(push_audio_command({
			.type = ClassicAudioCommandType::stop_song,
			.cue = ClassicMusicCue::menu_main,
		}));
	}
	completion_ = ClassicOpCompletion(
		route, selection_mode, route == ClassicOpRoute::versus,
		resident_, raster_, tram_
	);
	phase_ = ClassicOpPhase::ready;
}

void ClassicOpTitleMenuScene::select_demo_route() noexcept
{
	resident_.is_cpu = {1U, 1U};
	resident_.demo_number = static_cast<std::uint8_t>(
		(resident_.demo_number % demo_pairings.size()) + 1U
	);
	resident_.winner = 0;
	resident_.story_stage = 0U;
	resident_.game_mode = game_mode::demo;
	resident_.show_score_menu = 0U;
	const auto index = static_cast<std::size_t>(resident_.demo_number - 1U);
	resident_.paletted_character = demo_pairings[index];
	resident_.resident_rand = demo_random[index];
	resident_.last_score = {};
	resident_.global_frame = 0U;
	resident_.random_seed = std::bit_cast<std::uint32_t>(
		resident_.resident_rand
	);
	resident_.runtime_snapshot.reset();
	resident_.compact_runtime_snapshot.reset();
	resident_.round_start_snapshot.reset();
	resident_.practice_rules.reset();
	demo_route_pending_ = true;
	phase_frame_ = 0U;
	phase_ = ClassicOpPhase::demo_black_out;
}

void ClassicOpTitleMenuScene::fail() noexcept
{
	phase_ = ClassicOpPhase::failed;
	completion_.reset();
}

ClassicOpTick ClassicOpTitleMenuScene::tick(
	ClassicOpBoundary boundary,
	bool surface_changed,
	bool tram_changed,
	bool input_sampled,
	bool selection_changed
) const noexcept
{
	return {
		.phase = phase_,
		.boundary = boundary,
		.surface_changed = surface_changed,
		.tram_changed = tram_changed,
		.input_sampled = input_sampled,
		.selection_changed = selection_changed,
		.ready = phase_ == ClassicOpPhase::ready,
	};
}

std::expected<ClassicOpTick, ClassicOpError>
ClassicOpTitleMenuScene::advance(ClassicOpInput input)
{
	if(audio_count_ != 0U) {
		return std::unexpected(op_error(
			ClassicOpErrorCode::audio_command_pending
		));
	}
	switch(phase_) {
	case ClassicOpPhase::long_shift: {
		const auto value = static_cast<std::uint8_t>(brightness_);
		if(!raster_.set_pending_palette_entry_rgb8(15U, {value, value, value})) {
			fail();
			return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
		}
		if(brightness_ <= 0x80 && !raster_.set_pending_palette_entry_rgb8(
			11U, {value, value, value}
		)) {
			fail();
			return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
		}
		if(brightness_ <= 100) {
			raster_.set_palette_tone(brightness_);
		}
		raster_.apply_pending_palette();
		brightness_ += 2;
		if(!raster_.set_shown_page(title_page_)) {
			fail();
			return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
		}
		title_page_ = (title_page_ == present::Page::zero) ?
			present::Page::one : present::Page::zero;
		if(!raster_.set_access_page(title_page_) ||
			!shift_access_page_left(4U)) {
			fail();
			return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
		}
		++phase_frame_;
		if(phase_frame_ == long_shift_frames) {
			phase_frame_ = 0U;
			phase_ = ClassicOpPhase::long_background_fade;
		}
		return tick(ClassicOpBoundary::vsync, true);
	}
	case ClassicOpPhase::long_background_fade: {
		const auto value = static_cast<std::uint8_t>(brightness_);
		if(!raster_.set_pending_palette_entry_rgb8(15U, {value, value, value})) {
			fail();
			return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
		}
		raster_.apply_pending_palette();
		brightness_ += 2;
		if(brightness_ >= 256) {
			phase_frame_ = 0U;
			phase_ = ClassicOpPhase::long_selection_pause;
		}
		return tick(ClassicOpBoundary::vsync, true);
	}
	case ClassicOpPhase::long_selection_pause:
		++phase_frame_;
		if(phase_frame_ == selection_pause_frames) {
			phase_frame_ = 0U;
			phase_ = ClassicOpPhase::long_first_flash;
		}
		return tick(ClassicOpBoundary::vsync);
	case ClassicOpPhase::long_first_flash:
		raster_.set_palette_tone((phase_frame_ & 1U) == 0U ? 200 : 100);
		raster_.apply_pending_palette();
		++phase_frame_;
		if(phase_frame_ == flash_frames) {
			phase_frame_ = 0U;
			phase_ = ClassicOpPhase::long_menu_swap_bright;
		}
		return tick(ClassicOpBoundary::vsync, true);
	case ClassicOpPhase::long_menu_swap_bright:
		if(!begin_menu_swap()) {
			fail();
			return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
		}
		phase_ = ClassicOpPhase::long_menu_swap_normal;
		return tick(ClassicOpBoundary::vsync, true);
	case ClassicOpPhase::long_menu_swap_normal:
		raster_.set_palette_tone(100);
		raster_.apply_pending_palette();
		phase_frame_ = 0U;
		phase_ = ClassicOpPhase::long_second_flash;
		return tick(ClassicOpBoundary::vsync, true);
	case ClassicOpPhase::long_second_flash:
		raster_.set_palette_tone((phase_frame_ & 1U) == 0U ? 200 : 100);
		raster_.apply_pending_palette();
		++phase_frame_;
		if(phase_frame_ == flash_frames) {
			if(!draw_hidden_menu_background()) {
				fail();
				return std::unexpected(op_error(
					ClassicOpErrorCode::draw_failed
				));
			}
			phase_frame_ = 0U;
			phase_ = ClassicOpPhase::wait_title_input;
		}
		return tick(ClassicOpBoundary::vsync, true);
	case ClassicOpPhase::fast_black:
		if(restart_bgm_ && (!push_audio_command({
			.type = ClassicAudioCommandType::stop_song,
		}) || !push_audio_command({
			.type = ClassicAudioCommandType::load_song,
			.cue = ClassicMusicCue::menu_main,
		}))) {
			fail();
			return std::unexpected(op_error(
				ClassicOpErrorCode::audio_command_pending
			));
		}
		phase_ = ClassicOpPhase::fast_initial_pause;
		return tick(ClassicOpBoundary::vsync, true);
	case ClassicOpPhase::fast_initial_pause:
		raster_.set_palette_tone(0);
		if(!draw_menu_background(true) || (restart_bgm_ && !push_audio_command({
			.type = ClassicAudioCommandType::play_song,
			.cue = ClassicMusicCue::menu_main,
		}))) {
			fail();
			return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
		}
		phase_frame_ = 0U;
		phase_ = ClassicOpPhase::fast_fade;
		return tick(ClassicOpBoundary::vsync, true);
	case ClassicOpPhase::fast_fade:
		++phase_frame_;
		raster_.set_palette_tone(static_cast<std::int16_t>(phase_frame_ * 4U));
		raster_.apply_pending_palette();
		if(phase_frame_ == fast_fade_frames) {
			phase_frame_ = 0U;
			phase_ = ClassicOpPhase::wait_title_input;
		}
		return tick(ClassicOpBoundary::vsync, true);
	case ClassicOpPhase::wait_title_input:
		++resident_.resident_rand;
		++title_wait_frame_;
		if(!arrange_rules(resident_) && title_wait_frame_ > demo_wait_frames) {
			select_demo_route();
			return tick(
				ClassicOpBoundary::input_poll, false, false, true
			);
		}
		if(input.any()) {
			phase_ = ClassicOpPhase::box_to_main;
			phase_frame_ = 0U;
		}
		return tick(ClassicOpBoundary::vsync, false, false, true);
	case ClassicOpPhase::box_to_main:
		if(phase_frame_ == 0U && !begin_box_animation()) {
			fail();
			return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
		}
		if(!box_step()) {
			fail();
			return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
		}
		++phase_frame_;
		if(box_right_left_ ==
			(box_left + box_main_width - opwin_step_width)) {
			if(increment_rand_after_box_) {
				++resident_.resident_rand;
				increment_rand_after_box_ = false;
			}
			phase_frame_ = 0U;
			phase_ = ClassicOpPhase::main_menu;
		}
		return tick(ClassicOpBoundary::vsync, true);
	case ClassicOpPhase::main_menu: {
		bool tram_changed = false;
		bool selection_changed = false;
		std::optional<ClassicOpRoute> selected_route{};
		if(phase_frame_ == 0U) {
			if(!draw_main_choices()) {
				fail();
				return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
			}
			phase_frame_ = 1U;
			tram_changed = true;
		}
		if(!input.any()) {
			menu_input_allowed_ = true;
		}
		if(menu_input_allowed_) {
			if(input.up) {
				if(!update_selection(-1)) {
					fail();
					return std::unexpected(op_error(
						ClassicOpErrorCode::draw_failed
					));
				}
				tram_changed = true;
				selection_changed = true;
			}
			if(input.down) {
				if(!update_selection(1)) {
					fail();
					return std::unexpected(op_error(
						ClassicOpErrorCode::draw_failed
					));
				}
				tram_changed = true;
				selection_changed = true;
			}
			if(input.cancel || ((input.ok || input.shot) &&
				selected_ == ClassicOpMenuChoice::quit)) {
				selected_route = ClassicOpRoute::quit;
			} else if((input.ok || input.shot) &&
				choice_available(resident_, selected_)) {
				selected_route = route_for_choice(selected_);
			}
		}
		if(input.any()) {
			menu_input_allowed_ = false;
		}
		if(!selected_route || *selected_route == ClassicOpRoute::options ||
			*selected_route == ClassicOpRoute::quit) {
			++resident_.resident_rand;
		}
		if(selected_route == ClassicOpRoute::versus) {
			resident_.paletted_character = {1, 1};
			if(patch_assets_ && !restore_patch_menu_rows(
				box_left + box_main_width
			)) {
				fail();
				return std::unexpected(op_error(
					ClassicOpErrorCode::draw_failed
				));
			}
			tram_.fill({});
			selected_vs_ = ClassicOpVsChoice::one_player_cpu;
			box_right_left_ =
				box_left + box_main_width - opwin_step_width;
			phase_frame_ = 0U;
			phase_ = ClassicOpPhase::vs_grow_window;
			// The Replay Patch removes the proportional title text and enters
			// vs_menu() synchronously.  Its first observable boundary is the
			// first expanded-window frame, not this cleaned intermediate page.
			tram_changed = false;
		} else if(selected_route == ClassicOpRoute::story) {
			select_route(
				ClassicOpRoute::story, ClassicOpSelectionMode::story
			);
		} else if(selected_route) {
			select_route(*selected_route);
		}
		return tick(
			ClassicOpBoundary::vsync, false, tram_changed, true,
			selection_changed
		);
	}
	case ClassicOpPhase::vs_grow_window:
		if(!grow_box_step()) {
			fail();
			return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
		}
		++phase_frame_;
		if(phase_frame_ == submenu_grow_frames) {
			phase_frame_ = 0U;
			if(!draw_vs_choices()) {
				fail();
				return std::unexpected(op_error(
					ClassicOpErrorCode::draw_failed
				));
			}
			phase_ = ClassicOpPhase::vs_menu;
			return tick(ClassicOpBoundary::vsync, true, true);
		}
		return tick(ClassicOpBoundary::vsync, true);
	case ClassicOpPhase::vs_menu: {
		bool tram_changed = false;
		bool selection_changed = false;
		if(!input.any()) {
			menu_input_allowed_ = true;
		}
		if(menu_input_allowed_) {
			if(input.up || input.down) {
				if(!update_vs_selection(input.up ? -1 : 1)) {
					fail();
					return std::unexpected(op_error(
						ClassicOpErrorCode::draw_failed
					));
				}
				tram_changed = true;
				selection_changed = true;
			}
			if(input.cancel || (!patch_assets_ &&
				(input.ok || input.shot) &&
				selected_vs_ == ClassicOpVsChoice::quit)) {
				// Replay Patch vs_menu() unputs the complete submenu-width
				// proportional-font interior before contracting the window.
				if(patch_assets_ && !restore_patch_choice_rows(
					box_left + box_submenu_width
				)) {
					fail();
					return std::unexpected(op_error(
						ClassicOpErrorCode::draw_failed
					));
				}
				++resident_.resident_rand;
				tram_.fill({});
				box_right_left_ =
					box_left + box_submenu_width - opwin_step_width;
				phase_frame_ = 0U;
				phase_ = ClassicOpPhase::vs_shrink_window;
				// vs_menu() contracts synchronously as well.  Keep the cleared
				// submenu private until the first contracted-window boundary.
				tram_changed = false;
			} else if((input.ok || input.shot) &&
				vs_choice_available(resident_, selected_vs_)) {
				if(selected_vs_ == ClassicOpVsChoice::online_match) {
					select_route(ClassicOpRoute::netplay);
				} else {
					const auto mode = static_cast<ClassicOpSelectionMode>(
						static_cast<std::uint8_t>(selected_vs_) + 1U
					);
					select_route(ClassicOpRoute::versus, mode);
				}
			}
		}
		if(input.any()) {
			menu_input_allowed_ = false;
		}
		return tick(
			ClassicOpBoundary::vsync, false, tram_changed, true,
			selection_changed
		);
	}
	case ClassicOpPhase::vs_shrink_window:
		if(!shrink_box_step()) {
			fail();
			return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
		}
		++phase_frame_;
		if(phase_frame_ == submenu_shrink_frames) {
			phase_frame_ = 0U;
			phase_ = ClassicOpPhase::vs_return_main;
		}
		return tick(ClassicOpBoundary::vsync, true);
	case ClassicOpPhase::vs_return_main:
		selected_ = ClassicOpMenuChoice::versus;
		if(!draw_main_choices()) {
			fail();
			return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
		}
		menu_input_allowed_ = false;
		phase_frame_ = 1U;
		phase_ = ClassicOpPhase::main_menu;
		++resident_.resident_rand;
		return tick(ClassicOpBoundary::vsync, false, true);
	case ClassicOpPhase::demo_black_out:
		if(!demo_route_pending_) {
			fail();
			return std::unexpected(op_error(ClassicOpErrorCode::invalid_state));
		}
		if(phase_frame_ == 0U) {
			phase_frame_ = 1U;
			return tick(ClassicOpBoundary::vsync);
		}
		raster_.set_palette_tone(static_cast<std::int16_t>(
			100 - ((phase_frame_ - 1U) * 6U)
		));
		raster_.apply_pending_palette();
		++phase_frame_;
		if(phase_frame_ == 18U) {
			raster_.set_palette_tone(0);
			raster_.apply_pending_palette();
			demo_route_pending_ = false;
			if(!push_audio_command({
				.type = ClassicAudioCommandType::stop_song,
			})) {
				fail();
				return std::unexpected(op_error(
					ClassicOpErrorCode::audio_command_pending
				));
			}
			select_route(ClassicOpRoute::demo);
		}
		return tick(ClassicOpBoundary::vsync, true);
	case ClassicOpPhase::ready:
	case ClassicOpPhase::consumed:
	case ClassicOpPhase::failed:
		return std::unexpected(op_error(ClassicOpErrorCode::invalid_state));
	}
	return std::unexpected(op_error(ClassicOpErrorCode::invalid_state));
}

std::optional<ClassicAudioCommand>
ClassicOpTitleMenuScene::take_audio_command() noexcept
{
	if(audio_count_ == 0U) {
		return std::nullopt;
	}
	const auto command = audio_commands_[audio_head_];
	audio_head_ = (audio_head_ + 1U) % audio_commands_.size();
	--audio_count_;
	return command;
}

std::optional<ClassicOpCompletion>
ClassicOpTitleMenuScene::take_completion() noexcept
{
	if(phase_ != ClassicOpPhase::ready || !completion_ || audio_count_ != 0U) {
		return std::nullopt;
	}
	auto result = std::move(completion_);
	completion_.reset();
	phase_ = ClassicOpPhase::consumed;
	return result;
}

ClassicOpPhase ClassicOpTitleMenuScene::phase() const noexcept
{
	return phase_;
}

ClassicOpMenuChoice ClassicOpTitleMenuScene::selected_choice() const noexcept
{
	return selected_;
}

ClassicOpVsChoice ClassicOpTitleMenuScene::selected_vs_choice() const noexcept
{
	return selected_vs_;
}

const GameInit &ClassicOpTitleMenuScene::resident() const noexcept
{
	return resident_;
}

const present::Raster &ClassicOpTitleMenuScene::raster() const noexcept
{
	return raster_;
}

const present::Tram &ClassicOpTitleMenuScene::tram() const noexcept
{
	return tram_;
}

} // namespace th03::app
