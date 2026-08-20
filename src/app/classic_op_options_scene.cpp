#include "classic_op_options_scene_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "th03/assets/decode.hpp"
#include "th03/present/pi.hpp"

namespace th03::app {
namespace {

constexpr std::int32_t box_left = 160;
constexpr std::int32_t classic_box_top = 256;
constexpr std::int32_t replay_patch_box_top = 264;
constexpr std::int32_t box_height = 128;
constexpr std::int32_t box_main_width = 136;
constexpr std::int32_t box_submenu_width = 240;
constexpr std::int32_t box_option_width = 272;
constexpr std::int32_t box_main_center_x = 228;
constexpr std::int32_t box_submenu_center_x = 280;
constexpr std::int32_t box_option_center_x = 296;
constexpr std::int32_t box_option_value_center_x = 356;
constexpr std::int32_t opwin_step_width = 8;
constexpr std::int32_t opwin_width = 16;
constexpr std::uint16_t grow_frames = 13U;
constexpr std::uint16_t shrink_frames = 14U;
constexpr std::uint8_t gaiji_space = 0xCFU;

struct MenuLabel final {
	std::uint8_t first = 0U;
	std::uint8_t length = 0U;
	std::int8_t center_shift = 0;
	std::uint8_t line = 0U;
};

constexpr std::array<MenuLabel, 6U> main_labels{{
	{0x30U, 3U, 0, 0U},
	{0x33U, 6U, -1, 1U},
	{0x41U, 7U, -1, 2U},
	{0x82U, 5U, -1, 3U},
	{0x3DU, 4U, -1, 4U},
	{0x48U, 3U, -1, 5U},
}};

constexpr std::array<std::uint8_t, 3U> label_rank{{0x6EU, 0x6FU, 0x70U}};
constexpr std::array<std::uint8_t, 4U> label_music{{
	0x4BU, 0x4CU, 0x4DU, 0x4EU,
}};
constexpr std::array<std::uint8_t, 6U> label_key_config{{
	0x5CU, 0x5DU, 0x5EU, 0x5FU, 0x60U, 0x61U,
}};
constexpr std::array<std::uint8_t, 3U> label_quit{{0x48U, 0x49U, 0x4AU}};
constexpr std::array<std::uint8_t, 4U> value_spaces{{
	gaiji_space, gaiji_space, gaiji_space, gaiji_space,
}};
constexpr std::array<std::uint8_t, 3U> value_easy{{0x71U, 0x72U, 0x73U}};
constexpr std::array<std::uint8_t, 4U> value_normal{{
	0x74U, 0x75U, 0x76U, 0x77U,
}};
constexpr std::array<std::uint8_t, 3U> value_hard{{0x78U, 0x79U, 0x7AU}};
constexpr std::array<std::uint8_t, 4U> value_lunatic{{
	0x7BU, 0x7CU, 0x7DU, 0x7EU,
}};
constexpr std::array<std::uint8_t, 7U> value_off{{
	gaiji_space, gaiji_space, 0x5AU, 0x5BU,
	gaiji_space, gaiji_space, gaiji_space,
}};
constexpr std::array<std::uint8_t, 7U> value_fm{{
	gaiji_space, 0x4FU, 0x50U, 0x51U, 0x52U,
	gaiji_space, gaiji_space,
}};
constexpr std::array<std::uint8_t, 7U> value_midi{{
	0x53U, 0x54U, 0x55U, 0x56U, 0x57U, 0x58U, 0x59U,
}};
constexpr std::array<std::uint8_t, 6U> value_key_key{{
	0x68U, 0x69U, 0x6CU, 0x6DU, 0x68U, 0x69U,
}};
constexpr std::array<std::uint8_t, 6U> value_joy_key{{
	0x6AU, 0x6BU, 0x6CU, 0x6DU, 0x68U, 0x69U,
}};
constexpr std::array<std::uint8_t, 6U> value_key_joy{{
	0x68U, 0x69U, 0x6CU, 0x6DU, 0x6AU, 0x6BU,
}};

constexpr std::array<ClassicOpOptionsChoice, 7U> category_choices{{
	ClassicOpOptionsChoice::category_gameplay,
	ClassicOpOptionsChoice::category_audio,
	ClassicOpOptionsChoice::category_input,
	ClassicOpOptionsChoice::category_video,
	ClassicOpOptionsChoice::category_language,
	ClassicOpOptionsChoice::category_system,
	ClassicOpOptionsChoice::quit,
}};
constexpr std::array<ClassicOpOptionsChoice, 2U> gameplay_choices{{
	ClassicOpOptionsChoice::rank,
	ClassicOpOptionsChoice::return_page,
}};
constexpr std::array<ClassicOpOptionsChoice, 5U> audio_choices{{
	ClassicOpOptionsChoice::master_volume,
	ClassicOpOptionsChoice::bgm,
	ClassicOpOptionsChoice::sfx,
	ClassicOpOptionsChoice::mute_when_unfocused,
	ClassicOpOptionsChoice::return_page,
}};
constexpr std::array<ClassicOpOptionsChoice, 6U> input_choices{{
	ClassicOpOptionsChoice::key_mode,
	ClassicOpOptionsChoice::key_config,
	ClassicOpOptionsChoice::joy_config,
	ClassicOpOptionsChoice::device_assignment,
	ClassicOpOptionsChoice::input_test,
	ClassicOpOptionsChoice::return_page,
}};

constexpr std::array<ClassicOpOptionsChoice, 6U> video_choices{{
	ClassicOpOptionsChoice::window_mode,
	ClassicOpOptionsChoice::window_scale,
	ClassicOpOptionsChoice::aspect_ratio,
	ClassicOpOptionsChoice::filtering,
	ClassicOpOptionsChoice::photosensitivity,
	ClassicOpOptionsChoice::return_page,
}};
constexpr std::array<ClassicOpOptionsChoice, 2U> language_choices{{
	ClassicOpOptionsChoice::language,
	ClassicOpOptionsChoice::return_page,
}};
constexpr std::array<ClassicOpOptionsChoice, 6U> system_choices{{
	ClassicOpOptionsChoice::switch_classic,
	ClassicOpOptionsChoice::data_storage,
	ClassicOpOptionsChoice::online_privacy,
	ClassicOpOptionsChoice::credits,
	ClassicOpOptionsChoice::diagnostics,
	ClassicOpOptionsChoice::return_page,
}};
constexpr std::array<ClassicOpOptionsChoice, 7U> system_update_choices{{
	ClassicOpOptionsChoice::switch_classic,
	ClassicOpOptionsChoice::data_storage,
	ClassicOpOptionsChoice::online_privacy,
	ClassicOpOptionsChoice::credits,
	ClassicOpOptionsChoice::diagnostics,
	ClassicOpOptionsChoice::updates,
	ClassicOpOptionsChoice::return_page,
}};
constexpr std::array<ClassicOpOptionsChoice, 3U> data_storage_choices{{
	ClassicOpOptionsChoice::open_user_data,
	ClassicOpOptionsChoice::open_replay_folder,
	ClassicOpOptionsChoice::return_page,
}};
constexpr std::array<ClassicOpOptionsChoice, 2U> online_privacy_choices{{
	ClassicOpOptionsChoice::forget_room_identity,
	ClassicOpOptionsChoice::return_page,
}};
constexpr std::array<ClassicOpOptionsChoice, 1U>
	online_privacy_environment_choices{{ClassicOpOptionsChoice::return_page}};
constexpr std::array<ClassicOpOptionsChoice, 1U> credits_choices{{
	ClassicOpOptionsChoice::return_page,
}};
constexpr std::array<ClassicOpOptionsChoice, 2U> diagnostics_choices{{
	ClassicOpOptionsChoice::open_log_folder,
	ClassicOpOptionsChoice::return_page,
}};
constexpr std::array<ClassicOpOptionsChoice, 2U> updates_choices{{
	ClassicOpOptionsChoice::check_for_updates,
	ClassicOpOptionsChoice::return_page,
}};
constexpr std::array<NewClassicMessageId, 6U> credit_messages{{
	NewClassicMessageId::credits_project,
	NewClassicMessageId::credits_original,
	NewClassicMessageId::credits_patch,
	NewClassicMessageId::credits_patch_author,
	NewClassicMessageId::credits_port,
	NewClassicMessageId::credits_research,
}};
constexpr std::array<ClassicOpOptionsChoice, 2U> switch_confirmation_choices{{
	ClassicOpOptionsChoice::confirm_switch_classic,
	ClassicOpOptionsChoice::cancel_switch_classic,
}};
constexpr std::array<ClassicOpOptionsChoice, 2U>
	forget_identity_confirmation_choices{{
		ClassicOpOptionsChoice::confirm_forget_room_identity,
		ClassicOpOptionsChoice::cancel_forget_room_identity,
	}};
constexpr std::array<NewClassicMessageId, 7U> patch_main_labels{{
	NewClassicMessageId::title_start,
	NewClassicMessageId::title_vs_start,
	NewClassicMessageId::title_music_room,
	NewClassicMessageId::title_hiscore,
	NewClassicMessageId::title_replay,
	NewClassicMessageId::title_option,
	NewClassicMessageId::title_quit,
}};
constexpr std::array<ClassicOpMenuChoice, 7U> patch_main_choices{{
	ClassicOpMenuChoice::story,
	ClassicOpMenuChoice::versus,
	ClassicOpMenuChoice::music_room,
	ClassicOpMenuChoice::high_scores,
	ClassicOpMenuChoice::replay,
	ClassicOpMenuChoice::options,
	ClassicOpMenuChoice::quit,
}};

[[nodiscard]] constexpr std::int32_t choice_top(
	std::int32_t box_top,
	std::uint8_t line
) noexcept
{
	return ((box_top / present::gaiji_height) + 1 + line) *
		present::gaiji_height;
}

[[nodiscard]] constexpr NewClassicMessageId patch_choice_message(
	ClassicOpOptionsChoice choice
) noexcept
{
	switch(choice) {
	case ClassicOpOptionsChoice::rank: return NewClassicMessageId::option_rank;
	case ClassicOpOptionsChoice::master_volume: return NewClassicMessageId::option_master_volume;
	case ClassicOpOptionsChoice::window_mode: return NewClassicMessageId::option_window_mode;
	case ClassicOpOptionsChoice::window_scale: return NewClassicMessageId::option_window_scale;
	case ClassicOpOptionsChoice::aspect_ratio: return NewClassicMessageId::option_aspect_ratio;
	case ClassicOpOptionsChoice::filtering: return NewClassicMessageId::option_filtering;
	case ClassicOpOptionsChoice::bgm: return NewClassicMessageId::option_music;
	case ClassicOpOptionsChoice::key_mode: return NewClassicMessageId::option_key_mode;
	case ClassicOpOptionsChoice::quit: return NewClassicMessageId::option_return;
	case ClassicOpOptionsChoice::sfx: return NewClassicMessageId::option_sfx;
	case ClassicOpOptionsChoice::mute_when_unfocused:
		return NewClassicMessageId::option_mute_unfocused;
	case ClassicOpOptionsChoice::language: return NewClassicMessageId::option_language;
	case ClassicOpOptionsChoice::photosensitivity: return NewClassicMessageId::option_flash_fx;
	case ClassicOpOptionsChoice::category_gameplay:
		return NewClassicMessageId::option_category_gameplay;
	case ClassicOpOptionsChoice::category_audio: return NewClassicMessageId::option_category_audio;
	case ClassicOpOptionsChoice::category_input: return NewClassicMessageId::option_category_input;
	case ClassicOpOptionsChoice::category_video: return NewClassicMessageId::option_category_video;
	case ClassicOpOptionsChoice::category_language:
		return NewClassicMessageId::option_category_language;
	case ClassicOpOptionsChoice::category_system: return NewClassicMessageId::option_category_system;
	case ClassicOpOptionsChoice::key_config: return NewClassicMessageId::option_key_config;
	case ClassicOpOptionsChoice::joy_config: return NewClassicMessageId::option_joy_config;
	case ClassicOpOptionsChoice::device_assignment:
		return NewClassicMessageId::option_device_assignment;
	case ClassicOpOptionsChoice::input_test: return NewClassicMessageId::option_input_test;
	case ClassicOpOptionsChoice::return_page: return NewClassicMessageId::option_return;
	case ClassicOpOptionsChoice::switch_classic: return NewClassicMessageId::option_switch_classic;
	case ClassicOpOptionsChoice::data_storage: return NewClassicMessageId::option_data_storage;
	case ClassicOpOptionsChoice::online_privacy: return NewClassicMessageId::option_online_privacy;
	case ClassicOpOptionsChoice::credits: return NewClassicMessageId::option_credits;
	case ClassicOpOptionsChoice::diagnostics: return NewClassicMessageId::option_diagnostics;
	case ClassicOpOptionsChoice::updates: return NewClassicMessageId::option_updates;
	case ClassicOpOptionsChoice::open_user_data: return NewClassicMessageId::option_open_user_data;
	case ClassicOpOptionsChoice::open_replay_folder:
		return NewClassicMessageId::option_open_replay_folder;
	case ClassicOpOptionsChoice::open_log_folder:
		return NewClassicMessageId::option_open_log_folder;
	case ClassicOpOptionsChoice::forget_room_identity:
		return NewClassicMessageId::option_forget_room_identity;
	case ClassicOpOptionsChoice::check_for_updates:
		return NewClassicMessageId::option_check_updates;
	case ClassicOpOptionsChoice::confirm_switch_classic: return NewClassicMessageId::option_yes;
	case ClassicOpOptionsChoice::cancel_switch_classic: return NewClassicMessageId::option_no;
	case ClassicOpOptionsChoice::confirm_forget_room_identity:
		return NewClassicMessageId::option_yes;
	case ClassicOpOptionsChoice::cancel_forget_room_identity:
		return NewClassicMessageId::option_no;
	}
	return NewClassicMessageId::count;
}

[[nodiscard]] ClassicOpError options_error(
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
	auto result = options_error(
		ClassicOpErrorCode::asset_decode_failed, resource
	);
	result.asset_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] std::expected<void, ClassicOpError> validate_title_resources(
	std::span<const LoadedMainResource> resources
)
{
	const auto expected = op_title_menu_resource_keys();
	if(resources.size() != expected.size()) {
		return std::unexpected(options_error(
			ClassicOpErrorCode::resource_set_mismatch
		));
	}
	for(std::size_t index = 0U; index < expected.size(); ++index) {
		const auto key = expected[index];
		const auto &resource = resources[index];
		if(resource.spec.key != key || !resource.bytes) {
			return std::unexpected(options_error(
				resource.bytes ? ClassicOpErrorCode::resource_set_mismatch :
					ClassicOpErrorCode::resource_bytes_missing,
				key
			));
		}
		const auto canonical = main_resource_spec(key);
		if(!canonical || resource.spec != *canonical) {
			return std::unexpected(options_error(
				ClassicOpErrorCode::resource_set_mismatch, key
			));
		}
		if(!resource.authenticated_override &&
			(resource.bytes->size() != canonical->expected_bytes ||
			main_resource_fnv1a64(*resource.bytes) !=
				canonical->expected_fnv1a64)) {
			return std::unexpected(options_error(
				ClassicOpErrorCode::resource_integrity_failed, key
			));
		}
	}
	return {};
}

[[nodiscard]] const LoadedMainResource *find_resource(
	std::span<const LoadedMainResource> resources,
	MainResourceRole role
) noexcept
{
	for(const auto &resource : resources) {
		if(resource.spec.key.role == role) {
			return &resource;
		}
	}
	return nullptr;
}

[[nodiscard]] bool valid_menu_background(
	const assets::PiImage &image
) noexcept
{
	return image.palette.has_value() &&
		image.image.width == present::graphics_width &&
		image.image.height == present::graphics_height &&
		image.image.indices.size() == present::graphics_page_pixels;
}

[[nodiscard]] bool put_gaiji_run(
	present::Tram &tram,
	std::int32_t left,
	std::int32_t y,
	std::span<const std::uint8_t> glyphs,
	present::TramAttribute attribute
) noexcept
{
	for(std::size_t index = 0U; index < glyphs.size(); ++index) {
		if(!tram.put_gaiji(
			{left + static_cast<std::int32_t>(index * 2U), y},
			glyphs[index], attribute
		)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool put_centered(
	present::Tram &tram,
	std::int32_t center_x,
	std::int32_t line,
	std::int32_t center_shift,
	std::span<const std::uint8_t> glyphs,
	present::TramAttribute attribute
) noexcept
{
	const auto shifted_width = (
		static_cast<std::int32_t>(glyphs.size()) + center_shift
	) * present::gaiji_width;
	const auto left = (center_x - (shifted_width / 2)) / 8;
	const auto y = (classic_box_top / present::gaiji_height) + 1 + line;
	return put_gaiji_run(tram, left, y, glyphs, attribute);
}

[[nodiscard]] bool put_patch_option_centered(
	const NewClassicMenuFont &font,
	present::Raster &raster,
	std::int32_t center_x,
	std::int32_t top,
	std::string_view text,
	present::Index4 color
) noexcept
{
	if(color != 0U && color != 15U) {
		return false;
	}
	const auto shadow = static_cast<present::Index4>(
		color == 0U ? 15U : 0U
	);
	return font.put_centered_clipped(
		raster, center_x + 1, top + 1, text, shadow,
		{
			box_left + 2, top,
			static_cast<std::uint16_t>(box_option_width - 4),
			present::gaiji_height,
		}
	) && font.put_centered(raster, center_x, top, text, color);
}

[[nodiscard]] constexpr std::uint8_t ring_step(
	std::uint8_t value,
	int direction,
	std::uint8_t maximum
) noexcept
{
	if(direction < 0) {
		return value == 0U ? maximum : static_cast<std::uint8_t>(value - 1U);
	}
	return value == maximum ? 0U : static_cast<std::uint8_t>(value + 1U);
}

} // namespace

ClassicOpOptionsCompletion::ClassicOpOptionsCompletion(
	GameInit resident,
	ClassicConfigOptions config,
	std::uint8_t master_volume_percent,
	NewClassicWindowPresentation window_presentation,
	std::uint8_t music_volume_percent,
	std::uint8_t effect_volume_percent,
	bool mute_when_unfocused,
	present::Raster raster,
	present::Tram tram,
	bool main_input_allowed,
	ClassicOpOptionsOutcome outcome
) noexcept :
	resident_(std::move(resident)),
	config_(config),
	master_volume_percent_(master_volume_percent),
	music_volume_percent_(music_volume_percent),
	effect_volume_percent_(effect_volume_percent),
	mute_when_unfocused_(mute_when_unfocused),
	window_presentation_(window_presentation),
	raster_(std::move(raster)),
	tram_(std::move(tram)),
	main_input_allowed_(main_input_allowed),
	outcome_(outcome)
{}

const GameInit &ClassicOpOptionsCompletion::resident() const noexcept
{
	return resident_;
}

ClassicConfigOptions ClassicOpOptionsCompletion::config() const noexcept
{
	return config_;
}

std::uint8_t ClassicOpOptionsCompletion::master_volume_percent() const noexcept
{
	return master_volume_percent_;
}

std::uint8_t ClassicOpOptionsCompletion::music_volume_percent() const noexcept
{
	return music_volume_percent_;
}

std::uint8_t ClassicOpOptionsCompletion::effect_volume_percent() const noexcept
{
	return effect_volume_percent_;
}

bool ClassicOpOptionsCompletion::mute_when_unfocused() const noexcept
{
	return mute_when_unfocused_;
}

NewClassicWindowPresentation
ClassicOpOptionsCompletion::window_presentation() const noexcept
{
	return window_presentation_;
}

const present::Raster &ClassicOpOptionsCompletion::raster() const noexcept
{
	return raster_;
}

const present::Tram &ClassicOpOptionsCompletion::tram() const noexcept
{
	return tram_;
}

bool ClassicOpOptionsCompletion::main_input_allowed() const noexcept
{
	return main_input_allowed_;
}

ClassicOpOptionsOutcome ClassicOpOptionsCompletion::outcome() const noexcept
{
	return outcome_;
}

ClassicOpOptionsScene::ClassicOpOptionsScene(
	std::unique_ptr<Prepared> prepared,
	GameInit resident,
	ClassicConfigOptions config,
	bool fm_available,
	present::Raster raster,
	present::Tram tram,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	bool language_available,
	std::uint8_t master_volume_percent,
	NewClassicWindowPresentation window_presentation,
	std::uint8_t music_volume_percent,
	std::uint8_t effect_volume_percent,
	bool mute_when_unfocused,
	bool online_identity_reset_available,
	bool update_available
) noexcept :
	prepared_(std::move(prepared)),
	resident_(std::move(resident)),
	config_(config),
	master_volume_percent_(master_volume_percent),
	music_volume_percent_(music_volume_percent),
	effect_volume_percent_(effect_volume_percent),
	mute_when_unfocused_(mute_when_unfocused),
	online_identity_reset_available_(online_identity_reset_available),
	update_available_(update_available),
	window_presentation_(window_presentation),
	raster_(std::move(raster)),
	tram_(std::move(tram)),
	phase_(ClassicOpOptionsPhase::grow_window),
	box_right_left_(box_left + box_main_width - opwin_step_width),
	fm_available_(fm_available),
	language_available_(language_available),
	patch_assets_(std::move(patch_assets))
{
	if(patch_assets_) {
		selected_ = ClassicOpOptionsChoice::category_gameplay;
	}
}

std::expected<std::unique_ptr<ClassicOpOptionsScene>, ClassicOpError>
ClassicOpOptionsScene::create_from_op(
	const ClassicOpCompletion &completion,
	ClassicConfigOptions config,
	bool fm_available,
	std::span<const LoadedMainResource> resources,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	const present::AnkGlyphProvider *font,
	bool language_available,
	std::uint8_t master_volume_percent,
	NewClassicWindowPresentation window_presentation,
	std::uint8_t music_volume_percent,
	std::uint8_t effect_volume_percent,
	bool mute_when_unfocused,
	bool online_identity_reset_available,
	bool update_available
)
{
	if(completion.route() != ClassicOpRoute::options) {
		return std::unexpected(options_error(
			ClassicOpErrorCode::invalid_state
		));
	}
	if(const auto validated = validate_title_resources(resources); !validated) {
		return std::unexpected(validated.error());
	}
	const auto *background_resource = find_resource(
		resources, MainResourceRole::op_menu_background
	);
	const auto *window_resource = find_resource(
		resources, MainResourceRole::op_window_sheet
	);
	if(!background_resource || !background_resource->bytes ||
		!window_resource || !window_resource->bytes) {
		return std::unexpected(options_error(
			ClassicOpErrorCode::resource_set_mismatch
		));
	}
	auto background = assets::decode_pi(*background_resource->bytes);
	if(!background) {
		return std::unexpected(decode_error(
			background_resource->spec.key, std::move(background.error())
		));
	}
	auto window = assets::decode_bfnt(
		*window_resource->bytes,
		assets::BfntConsumerPolicy::superimpose
	);
	if(!window) {
		return std::unexpected(decode_error(
			window_resource->spec.key, std::move(window.error())
		));
	}
	return create(
		completion.resident(), config, fm_available, completion.raster(),
		completion.tram(), std::move(*background), std::move(*window),
		std::move(patch_assets), font, language_available,
		master_volume_percent, window_presentation, music_volume_percent,
		effect_volume_percent, mute_when_unfocused,
		online_identity_reset_available, update_available
	);
}

std::expected<std::unique_ptr<ClassicOpOptionsScene>, ClassicOpError>
ClassicOpOptionsScene::resume_after_key_config(
	const ClassicOpOptionsCompletion &completion,
	bool fm_available,
	std::span<const LoadedMainResource> resources,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	const present::AnkGlyphProvider *font,
	bool language_available,
	std::uint8_t master_volume_percent,
	NewClassicWindowPresentation window_presentation,
	std::uint8_t music_volume_percent,
	std::uint8_t effect_volume_percent,
	bool mute_when_unfocused,
	bool online_identity_reset_available,
	bool update_available
)
{
	if((completion.outcome() != ClassicOpOptionsOutcome::key_config &&
		completion.outcome() != ClassicOpOptionsOutcome::joy_config &&
		completion.outcome() != ClassicOpOptionsOutcome::device_assignment &&
		completion.outcome() != ClassicOpOptionsOutcome::input_test) ||
		!patch_assets || !font) {
		return std::unexpected(options_error(
			ClassicOpErrorCode::invalid_state
		));
	}
	if(const auto validated = validate_title_resources(resources); !validated) {
		return std::unexpected(validated.error());
	}
	const auto *background_resource = find_resource(
		resources, MainResourceRole::op_menu_background
	);
	const auto *window_resource = find_resource(
		resources, MainResourceRole::op_window_sheet
	);
	if(!background_resource || !background_resource->bytes ||
		!window_resource || !window_resource->bytes) {
		return std::unexpected(options_error(
			ClassicOpErrorCode::resource_set_mismatch
		));
	}
	auto background = assets::decode_pi(*background_resource->bytes);
	if(!background) {
		return std::unexpected(decode_error(
			background_resource->spec.key, std::move(background.error())
		));
	}
	auto window = assets::decode_bfnt(
		*window_resource->bytes,
		assets::BfntConsumerPolicy::superimpose
	);
	if(!window) {
		return std::unexpected(decode_error(
			window_resource->spec.key, std::move(window.error())
		));
	}
	auto created = create(
		completion.resident(), completion.config(), fm_available,
		completion.raster(), completion.tram(), std::move(*background),
		std::move(*window), std::move(patch_assets), font,
		language_available, master_volume_percent, window_presentation,
		music_volume_percent, effect_volume_percent, mute_when_unfocused,
		online_identity_reset_available, update_available
	);
	if(!created) {
		return std::unexpected(std::move(created.error()));
	}
	auto &scene = **created;
	scene.phase_ = ClassicOpOptionsPhase::options_menu;
	scene.page_ = ClassicOpOptionsPage::input;
	switch(completion.outcome()) {
	case ClassicOpOptionsOutcome::key_config:
		scene.selected_ = ClassicOpOptionsChoice::key_config;
		break;
	case ClassicOpOptionsOutcome::joy_config:
		scene.selected_ = ClassicOpOptionsChoice::joy_config;
		break;
	case ClassicOpOptionsOutcome::device_assignment:
		scene.selected_ = ClassicOpOptionsChoice::device_assignment;
		break;
	case ClassicOpOptionsOutcome::input_test:
		scene.selected_ = ClassicOpOptionsChoice::input_test;
		break;
	case ClassicOpOptionsOutcome::return_main:
	case ClassicOpOptionsOutcome::switch_classic:
		return std::unexpected(options_error(
			ClassicOpErrorCode::invalid_state
		));
	}
	scene.menu_input_allowed_ = false;
	scene.box_right_left_ = box_left + box_option_width - opwin_step_width;
	if(!scene.restore_patch_rows(0U, 7U, box_left + box_option_width) ||
		!scene.draw_all_option_choices()) {
		return std::unexpected(options_error(ClassicOpErrorCode::draw_failed));
	}
	return created;
}

std::expected<std::unique_ptr<ClassicOpOptionsScene>, ClassicOpError>
ClassicOpOptionsScene::resume_after_joy_config(
	const ClassicOpOptionsCompletion &completion,
	bool fm_available,
	std::span<const LoadedMainResource> resources,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	const present::AnkGlyphProvider *font,
	bool language_available,
	std::uint8_t master_volume_percent,
	NewClassicWindowPresentation window_presentation,
	std::uint8_t music_volume_percent,
	std::uint8_t effect_volume_percent,
	bool mute_when_unfocused,
	bool online_identity_reset_available,
	bool update_available
)
{
	if(completion.outcome() != ClassicOpOptionsOutcome::joy_config) {
		return std::unexpected(options_error(ClassicOpErrorCode::invalid_state));
	}
	return resume_after_key_config(
		completion, fm_available, resources, std::move(patch_assets), font,
		language_available, master_volume_percent, window_presentation,
		music_volume_percent, effect_volume_percent, mute_when_unfocused,
		online_identity_reset_available, update_available
	);
}

std::expected<std::unique_ptr<ClassicOpOptionsScene>, ClassicOpError>
ClassicOpOptionsScene::resume_after_device_scene(
	const ClassicOpOptionsCompletion &completion,
	bool fm_available,
	std::span<const LoadedMainResource> resources,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	const present::AnkGlyphProvider *font,
	bool language_available,
	std::uint8_t master_volume_percent,
	NewClassicWindowPresentation window_presentation,
	std::uint8_t music_volume_percent,
	std::uint8_t effect_volume_percent,
	bool mute_when_unfocused,
	bool online_identity_reset_available,
	bool update_available
)
{
	if(completion.outcome() != ClassicOpOptionsOutcome::device_assignment &&
		completion.outcome() != ClassicOpOptionsOutcome::input_test) {
		return std::unexpected(options_error(ClassicOpErrorCode::invalid_state));
	}
	return resume_after_key_config(
		completion, fm_available, resources, std::move(patch_assets), font,
		language_available, master_volume_percent, window_presentation,
		music_volume_percent, effect_volume_percent, mute_when_unfocused,
		online_identity_reset_available, update_available
	);
}

std::expected<std::unique_ptr<ClassicOpOptionsScene>, ClassicOpError>
ClassicOpOptionsScene::create(
	GameInit resident,
	ClassicConfigOptions config,
	bool fm_available,
	present::Raster raster,
	present::Tram tram,
	assets::PiImage menu_background,
	assets::BfntSheet window_sheet,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	const present::AnkGlyphProvider *font,
	bool language_available,
	std::uint8_t master_volume_percent,
	NewClassicWindowPresentation window_presentation,
	std::uint8_t music_volume_percent,
	std::uint8_t effect_volume_percent,
	bool mute_when_unfocused,
	bool online_identity_reset_available,
	bool update_available
)
{
	if(!classic_config_valid(config) || (patch_assets && !font) ||
		master_volume_percent > 100U || (master_volume_percent % 5U) != 0U ||
		music_volume_percent > 100U || (music_volume_percent % 5U) != 0U ||
		effect_volume_percent > 100U || (effect_volume_percent % 5U) != 0U ||
		static_cast<std::uint8_t>(window_presentation.mode) >
			static_cast<std::uint8_t>(
				NewClassicWindowMode::borderless_fullscreen
			) ||
		static_cast<std::uint8_t>(window_presentation.scale) >
			static_cast<std::uint8_t>(NewClassicWindowScale::automatic) ||
		static_cast<std::uint8_t>(window_presentation.aspect) >
			static_cast<std::uint8_t>(NewClassicAspectRatio::display_4_3) ||
		static_cast<std::uint8_t>(window_presentation.filtering) >
			static_cast<std::uint8_t>(NewClassicFiltering::linear) ||
		(!patch_assets && master_volume_percent != 100U) ||
		(!patch_assets && (music_volume_percent != 100U ||
			effect_volume_percent != 100U)) ||
		(!patch_assets && window_presentation !=
			NewClassicWindowPresentation{}) ||
		(!patch_assets && update_available) ||
		resident.rank != static_cast<std::uint8_t>(config.rank) ||
		resident.key_mode != static_cast<std::uint8_t>(config.key_mode) ||
		(!fm_available && config.bgm_mode != ClassicBgmMode::off) ||
		raster.raster_mode() != present::RasterMode::lines_400 ||
		raster.access_page() != present::Page::zero ||
		raster.shown_page() != present::Page::zero) {
		return std::unexpected(options_error(
			ClassicOpErrorCode::invalid_state
		));
	}
	if(!valid_menu_background(menu_background) ||
		window_sheet.header.pixel_width != opwin_width ||
		window_sheet.header.pixel_height != box_height ||
		window_sheet.header.first_code != 0U ||
		window_sheet.header.last_code != 2U) {
		return std::unexpected(options_error(
			ClassicOpErrorCode::invalid_asset_geometry
		));
	}
	auto prepared_window = present::prepare_superimpose_sheet(window_sheet);
	if(!prepared_window) {
		return std::unexpected(options_error(
			ClassicOpErrorCode::invalid_asset_geometry
		));
	}
	auto prepared = std::make_unique<Prepared>();
	prepared->menu_background = std::move(menu_background);
	prepared->window_sheet.emplace(std::move(*prepared_window));
	if(patch_assets) {
		constexpr std::array<std::uint8_t, 6U> japanese{{
			0x93U, 0xFAU, 0x96U, 0x7BU, 0x8CU, 0xEAU,
		}};
		auto prepared_japanese = present::prepare_graphics_text_8x16(
			japanese, *font
		);
		if(!prepared_japanese) {
			return std::unexpected(options_error(
				ClassicOpErrorCode::font_prepare_failed
			));
		}
		prepared->japanese.emplace(std::move(*prepared_japanese));
	}
	return std::unique_ptr<ClassicOpOptionsScene>(new ClassicOpOptionsScene(
		std::move(prepared), std::move(resident), config, fm_available,
		std::move(raster), std::move(tram), std::move(patch_assets),
		language_available, master_volume_percent, window_presentation,
		music_volume_percent, effect_volume_percent, mute_when_unfocused,
		online_identity_reset_available, update_available
	));
}

std::expected<void, ClassicOpError>
ClassicOpOptionsScene::report_update_check(
	NewClassicUpdateCheckOutcome outcome
)
{
	if(!update_available_ || !update_check_in_progress_) {
		return std::unexpected(options_error(ClassicOpErrorCode::invalid_state));
	}
	update_check_in_progress_ = false;
	update_check_outcome_ = outcome;
	if(page_ == ClassicOpOptionsPage::updates &&
		!draw_patch_update_status()) {
		return std::unexpected(options_error(ClassicOpErrorCode::draw_failed));
	}
	external_surface_changed_ = page_ == ClassicOpOptionsPage::updates;
	return {};
}

std::expected<void, ClassicOpError>
ClassicOpOptionsScene::refresh_patch_language_background(
	std::span<const LoadedMainResource> resources
)
{
	if(!patch_assets_ || phase_ != ClassicOpOptionsPhase::shrink_window) {
		return std::unexpected(options_error(ClassicOpErrorCode::invalid_state));
	}
	if(const auto validated = validate_title_resources(resources); !validated) {
		return std::unexpected(validated.error());
	}
	const auto *background_resource = find_resource(
		resources, MainResourceRole::op_menu_background
	);
	if(!background_resource || !background_resource->bytes) {
		return std::unexpected(options_error(
			ClassicOpErrorCode::resource_set_mismatch
		));
	}
	auto background = assets::decode_pi(*background_resource->bytes);
	if(!background) {
		return std::unexpected(decode_error(
			background_resource->spec.key, std::move(background.error())
		));
	}
	if(!valid_menu_background(*background) ||
		!raster_.set_access_page(present::Page::one) ||
		!present::pi_put_8(raster_, 0, 0, *background) ||
		!raster_.set_access_page(present::Page::zero)) {
		return std::unexpected(options_error(
			ClassicOpErrorCode::invalid_asset_geometry
		));
	}
	const auto &image = background->image;
	const auto blit = [&](std::int32_t left, std::int32_t top,
		std::uint16_t width, std::uint16_t height) {
		const auto offset = (static_cast<std::size_t>(top) * image.width) +
			static_cast<std::size_t>(left);
		return raster_.blit({
			.pixels = std::span<const present::Index4>(image.indices).subspan(offset),
			.width = width,
			.height = height,
			.stride = image.width,
		}, {left, top});
	};
	if(!blit(0, 0, present::graphics_width, replay_patch_box_top) ||
		!blit(0, replay_patch_box_top + box_height,
			present::graphics_width,
			present::graphics_height - (replay_patch_box_top + box_height)) ||
		!blit(0, replay_patch_box_top, box_left, box_height) ||
		!blit(box_left + box_option_width, replay_patch_box_top,
			present::graphics_width - (box_left + box_option_width), box_height) ||
		!patch_assets_->menu_font.put_right(
			raster_, 638, 0, patch_assets_->messages().text(
				NewClassicMessageId::credit_project
			), 0U
		) || !patch_assets_->menu_font.put_right(
			raster_, 638, 16, patch_assets_->messages().text(
				NewClassicMessageId::credit_patch
			), 0U
		)) {
		return std::unexpected(options_error(ClassicOpErrorCode::draw_failed));
	}
	for(std::size_t index = 0U; index < background->palette->colors.size(); ++index) {
		if(!raster_.set_pending_palette_entry(
			static_cast<std::uint8_t>(index), background->palette->colors[index]
		)) {
			return std::unexpected(options_error(ClassicOpErrorCode::draw_failed));
		}
	}
	raster_.apply_pending_palette();
	prepared_->menu_background = std::move(*background);
	return {};
}

bool ClassicOpOptionsScene::restore_menu_background(
	std::int32_t left,
	std::uint16_t width
) noexcept
{
	const auto &image = prepared_->menu_background.image;
	const auto box_top = patch_assets_ ? replay_patch_box_top : classic_box_top;
	if(left < 0 || static_cast<std::uint32_t>(left) + width > image.width ||
		box_top + box_height > image.height) {
		return false;
	}
	const auto offset =
		(static_cast<std::size_t>(box_top) * image.width) +
		static_cast<std::size_t>(left);
	return raster_.blit({
		.pixels = std::span<const present::Index4>(image.indices).subspan(offset),
		.width = width,
		.height = static_cast<std::uint16_t>(box_height),
		.stride = image.width,
	}, {left, box_top});
}

bool ClassicOpOptionsScene::draw_window_glyph(
	std::int32_t left,
	std::uint16_t glyph
) noexcept
{
	const auto box_top = patch_assets_ ? replay_patch_box_top : classic_box_top;
	return prepared_->window_sheet && present::superimpose_put(
		raster_, *prepared_->window_sheet, {left, box_top}, glyph
	);
}

bool ClassicOpOptionsScene::grow_step() noexcept
{
	if(!restore_menu_background(
		box_right_left_, static_cast<std::uint16_t>(opwin_step_width * 2)
	) || !draw_window_glyph(box_right_left_, 2U)) {
		return false;
	}
	box_right_left_ += opwin_step_width;
	return true;
}

bool ClassicOpOptionsScene::shrink_step() noexcept
{
	if(!restore_menu_background(
		box_right_left_, static_cast<std::uint16_t>(opwin_step_width * 2)
	) || !draw_window_glyph(box_right_left_ - opwin_step_width, 2U)) {
		return false;
	}
	box_right_left_ -= opwin_step_width;
	return true;
}

std::span<const ClassicOpOptionsChoice>
ClassicOpOptionsScene::active_choices() const noexcept
{
	if(!patch_assets_) {
		static constexpr std::array classic_choices{
			ClassicOpOptionsChoice::rank,
			ClassicOpOptionsChoice::bgm,
			ClassicOpOptionsChoice::key_mode,
			ClassicOpOptionsChoice::quit,
		};
		return classic_choices;
	}
	switch(page_) {
	case ClassicOpOptionsPage::categories: return category_choices;
	case ClassicOpOptionsPage::gameplay: return gameplay_choices;
	case ClassicOpOptionsPage::audio: return audio_choices;
	case ClassicOpOptionsPage::input: return input_choices;
	case ClassicOpOptionsPage::video: return video_choices;
	case ClassicOpOptionsPage::language: return language_choices;
	case ClassicOpOptionsPage::system:
		return update_available_ ?
			std::span<const ClassicOpOptionsChoice>(system_update_choices) :
			std::span<const ClassicOpOptionsChoice>(system_choices);
	case ClassicOpOptionsPage::data_storage: return data_storage_choices;
	case ClassicOpOptionsPage::online_privacy:
		return online_identity_reset_available_ ?
			std::span<const ClassicOpOptionsChoice>(online_privacy_choices) :
			std::span<const ClassicOpOptionsChoice>(
				online_privacy_environment_choices
			);
	case ClassicOpOptionsPage::credits: return credits_choices;
	case ClassicOpOptionsPage::diagnostics: return diagnostics_choices;
	case ClassicOpOptionsPage::updates: return updates_choices;
	case ClassicOpOptionsPage::switch_confirmation:
		return switch_confirmation_choices;
	case ClassicOpOptionsPage::forget_identity_confirmation:
		return forget_identity_confirmation_choices;
	}
	return {};
}

std::optional<std::uint8_t> ClassicOpOptionsScene::choice_line(
	ClassicOpOptionsChoice choice
) const noexcept
{
	const auto choices = active_choices();
	const auto found = std::find(choices.begin(), choices.end(), choice);
	if(found == choices.end()) {
		return std::nullopt;
	}
	const auto line = static_cast<std::uint8_t>(found - choices.begin());
	if(page_ == ClassicOpOptionsPage::switch_confirmation ||
		page_ == ClassicOpOptionsPage::forget_identity_confirmation) {
		return static_cast<std::uint8_t>(line + 2U);
	}
	if(page_ == ClassicOpOptionsPage::online_privacy) {
		return static_cast<std::uint8_t>(
			line + (online_identity_reset_available_ ? 2U : 4U)
		);
	}
	return page_ == ClassicOpOptionsPage::credits ?
		static_cast<std::uint8_t>(6U) : line;
}

bool ClassicOpOptionsScene::open_patch_page(
	ClassicOpOptionsPage page
) noexcept
{
	if(!patch_assets_ || page == ClassicOpOptionsPage::categories) {
		return false;
	}
	page_ = page;
	selected_ = active_choices().front();
	menu_input_allowed_ = false;
	if(!restore_patch_rows(0U, 7U, box_left + box_option_width)) {
		return false;
	}
	return (page_ != ClassicOpOptionsPage::credits || draw_patch_credits()) &&
		(page_ != ClassicOpOptionsPage::online_privacy ||
			draw_patch_online_identity_status()) &&
		(page_ != ClassicOpOptionsPage::updates ||
			draw_patch_update_status()) &&
		draw_all_option_choices();
}

bool ClassicOpOptionsScene::return_to_patch_categories() noexcept
{
	if(!patch_assets_ || page_ == ClassicOpOptionsPage::categories) {
		return false;
	}
	switch(page_) {
	case ClassicOpOptionsPage::gameplay:
		selected_ = ClassicOpOptionsChoice::category_gameplay;
		break;
	case ClassicOpOptionsPage::audio:
		selected_ = ClassicOpOptionsChoice::category_audio;
		break;
	case ClassicOpOptionsPage::input:
		selected_ = ClassicOpOptionsChoice::category_input;
		break;
	case ClassicOpOptionsPage::video:
		selected_ = ClassicOpOptionsChoice::category_video;
		break;
	case ClassicOpOptionsPage::language:
		selected_ = ClassicOpOptionsChoice::category_language;
		break;
	case ClassicOpOptionsPage::system:
		selected_ = ClassicOpOptionsChoice::category_system;
		break;
	case ClassicOpOptionsPage::data_storage:
		page_ = ClassicOpOptionsPage::system;
		selected_ = ClassicOpOptionsChoice::data_storage;
		menu_input_allowed_ = false;
		return restore_patch_rows(0U, 7U, box_left + box_option_width) &&
			draw_all_option_choices();
	case ClassicOpOptionsPage::online_privacy:
		page_ = ClassicOpOptionsPage::system;
		selected_ = ClassicOpOptionsChoice::online_privacy;
		menu_input_allowed_ = false;
		return restore_patch_rows(0U, 7U, box_left + box_option_width) &&
			draw_all_option_choices();
	case ClassicOpOptionsPage::credits:
		page_ = ClassicOpOptionsPage::system;
		selected_ = ClassicOpOptionsChoice::credits;
		menu_input_allowed_ = false;
		return restore_patch_rows(0U, 7U, box_left + box_option_width) &&
			draw_all_option_choices();
	case ClassicOpOptionsPage::diagnostics:
		page_ = ClassicOpOptionsPage::system;
		selected_ = ClassicOpOptionsChoice::diagnostics;
		menu_input_allowed_ = false;
		return restore_patch_rows(0U, 7U, box_left + box_option_width) &&
			draw_all_option_choices();
	case ClassicOpOptionsPage::updates:
		page_ = ClassicOpOptionsPage::system;
		selected_ = ClassicOpOptionsChoice::updates;
		menu_input_allowed_ = false;
		return restore_patch_rows(0U, 7U, box_left + box_option_width) &&
			draw_all_option_choices();
	case ClassicOpOptionsPage::switch_confirmation:
		page_ = ClassicOpOptionsPage::system;
		selected_ = ClassicOpOptionsChoice::switch_classic;
		menu_input_allowed_ = false;
		return restore_patch_rows(0U, 7U, box_left + box_option_width) &&
			draw_all_option_choices();
	case ClassicOpOptionsPage::forget_identity_confirmation:
		page_ = ClassicOpOptionsPage::online_privacy;
		selected_ = online_identity_reset_available_ ?
			ClassicOpOptionsChoice::forget_room_identity :
			ClassicOpOptionsChoice::return_page;
		menu_input_allowed_ = false;
		return restore_patch_rows(0U, 7U, box_left + box_option_width) &&
			draw_patch_online_identity_status() && draw_all_option_choices();
	case ClassicOpOptionsPage::categories:
		return false;
	}
	page_ = ClassicOpOptionsPage::categories;
	menu_input_allowed_ = false;
	return restore_patch_rows(0U, 7U, box_left + box_option_width) &&
		draw_all_option_choices();
}

bool ClassicOpOptionsScene::restore_patch_rows(
	std::uint8_t first,
	std::uint8_t count,
	std::int32_t box_right
) noexcept
{
	if(!patch_assets_ || box_right <= (box_left + 4) ||
		static_cast<std::uint16_t>(first) + count > 7U) {
		return false;
	}
	const auto bare = raster_.graphics(present::Page::one);
	if(bare.size() != present::graphics_page_pixels ||
		!raster_.set_access_page(present::Page::zero)) {
		return false;
	}
	const auto top = choice_top(replay_patch_box_top, first);
	const auto height = static_cast<std::int32_t>(count) *
		present::gaiji_height;
	for(std::int32_t y = top; y < top + height; ++y) {
		for(std::int32_t x = box_left + 2; x < box_right - 2; ++x) {
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

bool ClassicOpOptionsScene::draw_patch_choice(
	ClassicOpOptionsChoice choice,
	present::Index4 color
) noexcept
{
	if(!patch_assets_) {
		return false;
	}
	const auto line = choice_line(choice);
	if(!line || !restore_patch_rows(
		*line, 1U, box_left + box_option_width
	)) {
		return false;
	}
	const auto y = choice_top(replay_patch_box_top, *line);
	const auto &font = patch_assets_->menu_font;
	const auto &messages = patch_assets_->messages(
		config_.language == ClassicLanguage::english ?
			NewClassicLanguageAssetProfile::english :
			NewClassicLanguageAssetProfile::japanese
	);
	const auto message = patch_choice_message(choice);
	const auto label = messages.text(message);
	if(message == NewClassicMessageId::count || label.empty()) {
		return false;
	}
	if(choice == ClassicOpOptionsChoice::quit ||
		choice == ClassicOpOptionsChoice::return_page ||
		choice == ClassicOpOptionsChoice::key_config ||
		choice == ClassicOpOptionsChoice::joy_config ||
		choice == ClassicOpOptionsChoice::device_assignment ||
		choice == ClassicOpOptionsChoice::input_test ||
		choice == ClassicOpOptionsChoice::switch_classic ||
		choice == ClassicOpOptionsChoice::data_storage ||
		choice == ClassicOpOptionsChoice::online_privacy ||
		choice == ClassicOpOptionsChoice::credits ||
		choice == ClassicOpOptionsChoice::diagnostics ||
		choice == ClassicOpOptionsChoice::updates ||
		choice == ClassicOpOptionsChoice::open_user_data ||
		choice == ClassicOpOptionsChoice::open_replay_folder ||
		choice == ClassicOpOptionsChoice::open_log_folder ||
		choice == ClassicOpOptionsChoice::forget_room_identity ||
		choice == ClassicOpOptionsChoice::check_for_updates ||
		choice == ClassicOpOptionsChoice::confirm_switch_classic ||
		choice == ClassicOpOptionsChoice::cancel_switch_classic ||
		choice == ClassicOpOptionsChoice::confirm_forget_room_identity ||
		choice == ClassicOpOptionsChoice::cancel_forget_room_identity ||
		choice == ClassicOpOptionsChoice::category_gameplay ||
		choice == ClassicOpOptionsChoice::category_audio ||
		choice == ClassicOpOptionsChoice::category_input ||
		choice == ClassicOpOptionsChoice::category_video ||
		choice == ClassicOpOptionsChoice::category_language ||
		choice == ClassicOpOptionsChoice::category_system) {
		return put_patch_option_centered(
			font, raster_, box_option_center_x, y, label, color
		);
	}
	if(!put_patch_option_centered(
		font, raster_, box_main_center_x, y, label, color
	)) {
		return false;
	}
	std::string_view value{};
	std::string value_storage{};
	const auto percent_value = [&](std::uint8_t percent) {
		value_storage = messages.format_percent(
			NewClassicMessageId::value_percent, percent
		);
		return std::string_view{value_storage};
	};
	switch(choice) {
	case ClassicOpOptionsChoice::rank:
		switch(config_.rank) {
		case ClassicRank::easy:
			value = messages.text(NewClassicMessageId::value_easy); break;
		case ClassicRank::normal:
			value = messages.text(NewClassicMessageId::value_normal); break;
		case ClassicRank::hard:
			value = messages.text(NewClassicMessageId::value_hard); break;
		case ClassicRank::lunatic:
			value = messages.text(NewClassicMessageId::value_lunatic); break;
		}
		break;
	case ClassicOpOptionsChoice::master_volume:
		value = percent_value(master_volume_percent_);
		break;
	case ClassicOpOptionsChoice::window_mode:
		value = window_presentation_.mode == NewClassicWindowMode::windowed ?
			messages.text(NewClassicMessageId::value_windowed) :
			messages.text(NewClassicMessageId::value_borderless);
		break;
	case ClassicOpOptionsChoice::window_scale:
		switch(window_presentation_.scale) {
		case NewClassicWindowScale::x1:
			value = messages.text(NewClassicMessageId::value_scale_1x); break;
		case NewClassicWindowScale::x2:
			value = messages.text(NewClassicMessageId::value_scale_2x); break;
		case NewClassicWindowScale::x3:
			value = messages.text(NewClassicMessageId::value_scale_3x); break;
		case NewClassicWindowScale::automatic:
			value = messages.text(NewClassicMessageId::value_scale_auto); break;
		}
		break;
	case ClassicOpOptionsChoice::aspect_ratio:
		value = window_presentation_.aspect ==
			NewClassicAspectRatio::pixels_8_5 ?
			messages.text(NewClassicMessageId::value_aspect_8_5) :
			messages.text(NewClassicMessageId::value_aspect_4_3);
		break;
	case ClassicOpOptionsChoice::filtering:
		value = window_presentation_.filtering == NewClassicFiltering::nearest ?
			messages.text(NewClassicMessageId::value_nearest) :
			messages.text(NewClassicMessageId::value_linear);
		break;
	case ClassicOpOptionsChoice::bgm:
		value = patch_assets_ ? percent_value(music_volume_percent_) :
			(config_.bgm_mode == ClassicBgmMode::off ?
				messages.text(NewClassicMessageId::value_off) :
				messages.text(NewClassicMessageId::value_fm86));
		break;
	case ClassicOpOptionsChoice::sfx:
		value = patch_assets_ ? percent_value(effect_volume_percent_) :
			(config_.sfx_enabled ? messages.text(NewClassicMessageId::value_on) :
				messages.text(NewClassicMessageId::value_off));
		break;
	case ClassicOpOptionsChoice::language:
		if(config_.language == ClassicLanguage::english) {
			value = messages.text(NewClassicMessageId::value_english);
		} else {
			return prepared_->japanese && present::graphics_text_put_8x16(
				raster_, {box_option_value_center_x - 24, y},
				*prepared_->japanese, {.color = color}
			);
		}
		break;
	case ClassicOpOptionsChoice::key_mode:
		switch(config_.key_mode) {
		case ClassicKeyMode::key_key:
			value = messages.text(NewClassicMessageId::value_key_key); break;
		case ClassicKeyMode::joy_key:
			value = messages.text(NewClassicMessageId::value_joy_key); break;
		case ClassicKeyMode::key_joy:
			value = messages.text(NewClassicMessageId::value_key_joy); break;
		}
		break;
	case ClassicOpOptionsChoice::photosensitivity:
		value = config_.reduced_flash ?
			messages.text(NewClassicMessageId::value_reduced) :
			messages.text(NewClassicMessageId::value_full);
		break;
	case ClassicOpOptionsChoice::mute_when_unfocused:
		value = mute_when_unfocused_ ?
			messages.text(NewClassicMessageId::value_on) :
			messages.text(NewClassicMessageId::value_off);
		break;
	case ClassicOpOptionsChoice::category_gameplay:
	case ClassicOpOptionsChoice::category_audio:
	case ClassicOpOptionsChoice::category_input:
	case ClassicOpOptionsChoice::category_video:
	case ClassicOpOptionsChoice::category_language:
	case ClassicOpOptionsChoice::category_system:
	case ClassicOpOptionsChoice::key_config:
	case ClassicOpOptionsChoice::joy_config:
	case ClassicOpOptionsChoice::device_assignment:
	case ClassicOpOptionsChoice::input_test:
	case ClassicOpOptionsChoice::return_page:
	case ClassicOpOptionsChoice::switch_classic:
	case ClassicOpOptionsChoice::data_storage:
	case ClassicOpOptionsChoice::online_privacy:
	case ClassicOpOptionsChoice::credits:
	case ClassicOpOptionsChoice::diagnostics:
	case ClassicOpOptionsChoice::updates:
	case ClassicOpOptionsChoice::open_user_data:
	case ClassicOpOptionsChoice::open_replay_folder:
	case ClassicOpOptionsChoice::open_log_folder:
	case ClassicOpOptionsChoice::forget_room_identity:
	case ClassicOpOptionsChoice::check_for_updates:
	case ClassicOpOptionsChoice::confirm_switch_classic:
	case ClassicOpOptionsChoice::cancel_switch_classic:
	case ClassicOpOptionsChoice::confirm_forget_room_identity:
	case ClassicOpOptionsChoice::cancel_forget_room_identity:
	case ClassicOpOptionsChoice::quit:
		return false;
	}
	return put_patch_option_centered(
		font, raster_, box_option_value_center_x, y, value, color
	);
}

bool ClassicOpOptionsScene::draw_patch_credits() noexcept
{
	if(!patch_assets_ || page_ != ClassicOpOptionsPage::credits) {
		return false;
	}
	const auto &messages = patch_assets_->messages(
		config_.language == ClassicLanguage::english ?
			NewClassicLanguageAssetProfile::english :
			NewClassicLanguageAssetProfile::japanese
	);
	for(std::size_t index = 0U; index < credit_messages.size(); ++index) {
		const auto text = messages.text(credit_messages[index]);
		if(text.empty() || !put_patch_option_centered(
			patch_assets_->menu_font, raster_, box_option_center_x,
			choice_top(
				replay_patch_box_top, static_cast<std::uint8_t>(index)
			), text, 0U
		)) {
			return false;
		}
	}
	return true;
}

bool ClassicOpOptionsScene::draw_patch_online_identity_status() noexcept
{
	if(!patch_assets_ || page_ != ClassicOpOptionsPage::online_privacy) {
		return false;
	}
	const auto &messages = patch_assets_->messages(
		config_.language == ClassicLanguage::english ?
			NewClassicLanguageAssetProfile::english :
			NewClassicLanguageAssetProfile::japanese
	);
	const auto y = choice_top(replay_patch_box_top, 0U);
	return put_patch_option_centered(
		patch_assets_->menu_font, raster_, box_main_center_x, y,
		messages.text(NewClassicMessageId::option_room_identity), 0U
	) && put_patch_option_centered(
		patch_assets_->menu_font, raster_, box_option_value_center_x, y,
		messages.text(online_identity_reset_available_ ?
			NewClassicMessageId::value_protected :
			NewClassicMessageId::value_environment), 0U
	);
}

bool ClassicOpOptionsScene::draw_patch_update_status() noexcept
{
	if(!patch_assets_ || page_ != ClassicOpOptionsPage::updates ||
		!restore_patch_rows(3U, 1U, box_left + box_option_width)) {
		return false;
	}
	if(!update_check_in_progress_ && !update_check_outcome_) {
		return true;
	}
	NewClassicMessageId message = NewClassicMessageId::option_update_checking;
	if(!update_check_in_progress_) {
		switch(*update_check_outcome_) {
		case NewClassicUpdateCheckOutcome::up_to_date:
			message = NewClassicMessageId::option_update_up_to_date;
			break;
		case NewClassicUpdateCheckOutcome::restart_to_apply:
			message = NewClassicMessageId::option_update_restart;
			break;
		case NewClassicUpdateCheckOutcome::failed:
			message = NewClassicMessageId::option_update_failed;
			break;
		}
	}
	const auto &messages = patch_assets_->messages(
		config_.language == ClassicLanguage::english ?
			NewClassicLanguageAssetProfile::english :
			NewClassicLanguageAssetProfile::japanese
	);
	return put_patch_option_centered(
		patch_assets_->menu_font, raster_, box_option_center_x,
		choice_top(replay_patch_box_top, 3U), messages.text(message), 0U
	);
}

bool ClassicOpOptionsScene::draw_option_choice(
	ClassicOpOptionsChoice choice,
	present::TramAttribute attribute
) noexcept
{
	if(patch_assets_) {
		return draw_patch_choice(
			choice, attribute == present::tram_black ? 0U : 15U
		);
	}
	switch(choice) {
	case ClassicOpOptionsChoice::rank: {
		// ZUN quirk: Labels use the old main-box center rather than the
		// actual center of the submenu's left column.
		if(!put_centered(
			tram_, box_main_center_x, 0, 0, label_rank, attribute
		) || !put_centered(
			tram_, 340, 0, 1, value_spaces, present::tram_white
		)) {
			return false;
		}
		switch(config_.rank) {
		case ClassicRank::easy:
			return put_centered(tram_, 340, 0, 1, value_easy, attribute);
		case ClassicRank::normal:
			return put_centered(tram_, 340, 0, 1, value_normal, attribute);
		case ClassicRank::hard:
			return put_centered(tram_, 340, 0, 1, value_hard, attribute);
		case ClassicRank::lunatic:
			return put_centered(tram_, 340, 0, 1, value_lunatic, attribute);
		}
		return false;
	}
	case ClassicOpOptionsChoice::bgm: {
		if(!put_centered(
			tram_, box_main_center_x, 2, -1, label_music, attribute
		)) {
			return false;
		}
		const auto y = (classic_box_top / present::gaiji_height) + 3;
		switch(config_.bgm_mode) {
		case ClassicBgmMode::off:
			return put_gaiji_run(tram_, 35, y, value_off, attribute);
		case ClassicBgmMode::fm:
			return put_gaiji_run(tram_, 35, y, value_fm, attribute);
		case ClassicBgmMode::midi:
			return put_gaiji_run(tram_, 35, y, value_midi, attribute);
		}
		return false;
	}
	case ClassicOpOptionsChoice::key_mode: {
		if(!put_centered(
			tram_, box_main_center_x, 4, -1, label_key_config, attribute
		)) {
			return false;
		}
		switch(config_.key_mode) {
		case ClassicKeyMode::key_key:
			return put_centered(tram_, 340, 4, -1, value_key_key, attribute);
		case ClassicKeyMode::joy_key:
			return put_centered(tram_, 340, 4, -1, value_joy_key, attribute);
		case ClassicKeyMode::key_joy:
			return put_centered(tram_, 340, 4, -1, value_key_joy, attribute);
		}
		return false;
	}
	case ClassicOpOptionsChoice::sfx:
	case ClassicOpOptionsChoice::mute_when_unfocused:
	case ClassicOpOptionsChoice::master_volume:
	case ClassicOpOptionsChoice::window_mode:
	case ClassicOpOptionsChoice::window_scale:
	case ClassicOpOptionsChoice::aspect_ratio:
	case ClassicOpOptionsChoice::filtering:
	case ClassicOpOptionsChoice::language:
	case ClassicOpOptionsChoice::photosensitivity:
	case ClassicOpOptionsChoice::category_gameplay:
	case ClassicOpOptionsChoice::category_audio:
	case ClassicOpOptionsChoice::category_input:
	case ClassicOpOptionsChoice::category_video:
	case ClassicOpOptionsChoice::category_language:
	case ClassicOpOptionsChoice::category_system:
	case ClassicOpOptionsChoice::key_config:
	case ClassicOpOptionsChoice::joy_config:
	case ClassicOpOptionsChoice::device_assignment:
	case ClassicOpOptionsChoice::input_test:
	case ClassicOpOptionsChoice::return_page:
	case ClassicOpOptionsChoice::switch_classic:
	case ClassicOpOptionsChoice::data_storage:
	case ClassicOpOptionsChoice::online_privacy:
	case ClassicOpOptionsChoice::credits:
	case ClassicOpOptionsChoice::diagnostics:
	case ClassicOpOptionsChoice::updates:
	case ClassicOpOptionsChoice::open_user_data:
	case ClassicOpOptionsChoice::open_replay_folder:
	case ClassicOpOptionsChoice::open_log_folder:
	case ClassicOpOptionsChoice::forget_room_identity:
	case ClassicOpOptionsChoice::check_for_updates:
	case ClassicOpOptionsChoice::confirm_switch_classic:
	case ClassicOpOptionsChoice::cancel_switch_classic:
	case ClassicOpOptionsChoice::confirm_forget_room_identity:
	case ClassicOpOptionsChoice::cancel_forget_room_identity:
		return false;
	case ClassicOpOptionsChoice::quit:
		return put_centered(
			tram_, box_submenu_center_x, 5, 0, label_quit, attribute
		);
	}
	return false;
}

bool ClassicOpOptionsScene::draw_all_option_choices() noexcept
{
	const auto &messages = patch_assets_ ? patch_assets_->messages(
		config_.language == ClassicLanguage::english ?
			NewClassicLanguageAssetProfile::english :
			NewClassicLanguageAssetProfile::japanese
	) : new_classic_builtin_messages(
		NewClassicLanguageAssetProfile::english
	);
	if(patch_assets_ && (page_ ==
		ClassicOpOptionsPage::switch_confirmation || page_ ==
		ClassicOpOptionsPage::forget_identity_confirmation) &&
		!put_patch_option_centered(
			patch_assets_->menu_font, raster_, box_option_center_x,
			choice_top(replay_patch_box_top, 0U),
			messages.text(page_ ==
				ClassicOpOptionsPage::switch_confirmation ?
					NewClassicMessageId::prompt_switch_classic :
					NewClassicMessageId::prompt_forget_room_identity), 15U
		)) {
		return false;
	}
	for(const auto choice : active_choices()) {
		if(!draw_option_choice(
			choice,
			(choice == selected_) ? present::tram_white : present::tram_black
		)) {
			return false;
		}
	}
	return true;
}

bool ClassicOpOptionsScene::draw_main_choices() noexcept
{
	if(patch_assets_) {
		if(!restore_patch_rows(0U, 7U, box_left + box_main_width)) {
			return false;
		}
		for(std::size_t index = 0U; index < patch_main_labels.size(); ++index) {
			if(!patch_assets_->menu_font.put_centered(
				raster_, box_main_center_x,
				choice_top(
					replay_patch_box_top, static_cast<std::uint8_t>(index)
				),
				patch_assets_->messages().text(patch_main_labels[index]),
				patch_main_choices[index] == ClassicOpMenuChoice::options ?
					15U : 0U
			)) {
				return false;
			}
		}
		return true;
	}
	for(std::size_t index = 0U; index < main_labels.size(); ++index) {
		const auto &label = main_labels[index];
		const std::array<std::uint8_t, 7U> storage{{
			label.first,
			static_cast<std::uint8_t>(label.first + 1U),
			static_cast<std::uint8_t>(label.first + 2U),
			static_cast<std::uint8_t>(label.first + 3U),
			static_cast<std::uint8_t>(label.first + 4U),
			static_cast<std::uint8_t>(label.first + 5U),
			static_cast<std::uint8_t>(label.first + 6U),
		}};
		if(!put_centered(
			tram_, box_main_center_x, label.line, label.center_shift,
			std::span<const std::uint8_t>(storage).first(label.length),
			(index == static_cast<std::size_t>(ClassicOpMenuChoice::options)) ?
				present::tram_white : present::tram_black
		)) {
			return false;
		}
	}
	return true;
}

bool ClassicOpOptionsScene::update_selection(int direction) noexcept
{
	if(!draw_option_choice(selected_, present::tram_black)) {
		return false;
	}
	const auto choices = active_choices();
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
	selected_ = choices[static_cast<std::size_t>(next)];
	return draw_option_choice(selected_, present::tram_white);
}

bool ClassicOpOptionsScene::update_horizontal(
	ClassicOpInput input,
	bool &config_changed
) noexcept
{
	const int direction = input.left ? -1 : 1;
	switch(selected_) {
	case ClassicOpOptionsChoice::rank: {
		const auto next = ring_step(
			static_cast<std::uint8_t>(config_.rank), direction, 3U
		);
		config_.rank = static_cast<ClassicRank>(next);
		resident_.rank = next;
		config_changed = true;
		break;
	}
	case ClassicOpOptionsChoice::master_volume: {
		const auto step = static_cast<std::uint8_t>(
			master_volume_percent_ / 5U
		);
		master_volume_percent_ = static_cast<std::uint8_t>(
			ring_step(step, direction, 20U) * 5U
		);
		config_changed = true;
		break;
	}
	case ClassicOpOptionsChoice::window_mode:
		window_presentation_.mode =
			window_presentation_.mode == NewClassicWindowMode::windowed ?
				NewClassicWindowMode::borderless_fullscreen :
				NewClassicWindowMode::windowed;
		config_changed = true;
		break;
	case ClassicOpOptionsChoice::window_scale: {
		const auto next = ring_step(
			static_cast<std::uint8_t>(window_presentation_.scale),
			direction, 3U
		);
		window_presentation_.scale = static_cast<NewClassicWindowScale>(next);
		config_changed = true;
		break;
	}
	case ClassicOpOptionsChoice::aspect_ratio:
		window_presentation_.aspect = window_presentation_.aspect ==
			NewClassicAspectRatio::pixels_8_5 ?
				NewClassicAspectRatio::display_4_3 :
				NewClassicAspectRatio::pixels_8_5;
		config_changed = true;
		break;
	case ClassicOpOptionsChoice::filtering:
		window_presentation_.filtering = window_presentation_.filtering ==
			NewClassicFiltering::nearest ? NewClassicFiltering::linear :
			NewClassicFiltering::nearest;
		config_changed = true;
		break;
	case ClassicOpOptionsChoice::bgm:
		if(patch_assets_) {
			const auto step = static_cast<std::uint8_t>(
				music_volume_percent_ / 5U
			);
			const auto next = static_cast<std::uint8_t>(
				ring_step(step, direction, 20U) * 5U
			);
			const auto previous = music_volume_percent_;
			music_volume_percent_ = next;
			config_.bgm_mode = next == 0U ? ClassicBgmMode::off :
				ClassicBgmMode::fm;
			if(previous != 0U && next == 0U) {
				if(!push_audio_command({
					.type = ClassicAudioCommandType::stop_song,
				}) || !push_audio_command({
					.type = ClassicAudioCommandType::set_bgm_active,
					.parameter = 0U,
				}) || !push_audio_command({
					.type = ClassicAudioCommandType::set_music_volume,
					.parameter = next,
				})) {
					return false;
				}
			} else if(previous == 0U && next != 0U) {
				if(!push_audio_command({
					.type = ClassicAudioCommandType::set_music_volume,
					.parameter = next,
				}) || !push_audio_command({
					.type = ClassicAudioCommandType::set_bgm_active,
					.parameter = 1U,
				}) || !push_audio_command({
					.type = ClassicAudioCommandType::play_song,
					.cue = ClassicMusicCue::menu_main,
				})) {
					return false;
				}
			} else if(!push_audio_command({
				.type = ClassicAudioCommandType::set_music_volume,
				.parameter = next,
			})) {
				return false;
			}
			config_changed = true;
			break;
		}
		if(fm_available_) {
			// ZUN quirk: Both directions toggle Off and FM, while a loaded
			// MIDI setting collapses to Off instead of participating in a ring.
			if(config_.bgm_mode == ClassicBgmMode::off) {
				config_.bgm_mode = ClassicBgmMode::fm;
				if(!push_audio_command({
					.type = ClassicAudioCommandType::stop_song,
				}) || !push_audio_command({
					.type = ClassicAudioCommandType::set_bgm_active,
					.parameter = 1U,
				}) || !push_audio_command({
					.type = ClassicAudioCommandType::play_song,
					.cue = ClassicMusicCue::menu_main,
				})) {
					return false;
				}
			} else {
				config_.bgm_mode = ClassicBgmMode::off;
				if(!push_audio_command({
					.type = ClassicAudioCommandType::stop_song,
				}) || !push_audio_command({
					.type = ClassicAudioCommandType::set_bgm_active,
					.parameter = 0U,
				})) {
					return false;
				}
			}
			config_changed = true;
		}
		break;
	case ClassicOpOptionsChoice::sfx:
		if(patch_assets_) {
			const auto step = static_cast<std::uint8_t>(
				effect_volume_percent_ / 5U
			);
			const auto next = static_cast<std::uint8_t>(
				ring_step(step, direction, 20U) * 5U
			);
			const auto previous = effect_volume_percent_;
			effect_volume_percent_ = next;
			config_.sfx_enabled = next != 0U;
			if(previous != 0U && next == 0U) {
				if(!push_audio_command({
					.type = ClassicAudioCommandType::set_effects_active,
					.parameter = 0U,
				}) || !push_audio_command({
					.type = ClassicAudioCommandType::set_effect_volume,
					.parameter = next,
				})) {
					return false;
				}
			} else if(previous == 0U && next != 0U) {
				if(!push_audio_command({
					.type = ClassicAudioCommandType::set_effect_volume,
					.parameter = next,
				}) || !push_audio_command({
					.type = ClassicAudioCommandType::set_effects_active,
					.parameter = 1U,
				})) {
					return false;
				}
			} else if(!push_audio_command({
				.type = ClassicAudioCommandType::set_effect_volume,
				.parameter = next,
			})) {
				return false;
			}
			config_changed = true;
		}
		break;
	case ClassicOpOptionsChoice::language:
		if(patch_assets_ && language_available_) {
			config_.language = config_.language == ClassicLanguage::japanese ?
				ClassicLanguage::english : ClassicLanguage::japanese;
			config_changed = true;
		}
		break;
	case ClassicOpOptionsChoice::key_mode: {
		const auto next = ring_step(
			static_cast<std::uint8_t>(config_.key_mode), direction, 2U
		);
		config_.key_mode = static_cast<ClassicKeyMode>(next);
		resident_.key_mode = next;
		config_changed = true;
		break;
	}
	case ClassicOpOptionsChoice::photosensitivity:
		if(patch_assets_) {
			config_.reduced_flash = !config_.reduced_flash;
			config_changed = true;
		}
		break;
	case ClassicOpOptionsChoice::mute_when_unfocused:
		if(patch_assets_) {
			mute_when_unfocused_ = !mute_when_unfocused_;
			config_changed = true;
		}
		break;
	case ClassicOpOptionsChoice::category_gameplay:
	case ClassicOpOptionsChoice::category_audio:
	case ClassicOpOptionsChoice::category_input:
	case ClassicOpOptionsChoice::category_video:
	case ClassicOpOptionsChoice::category_language:
	case ClassicOpOptionsChoice::category_system:
	case ClassicOpOptionsChoice::key_config:
	case ClassicOpOptionsChoice::joy_config:
	case ClassicOpOptionsChoice::device_assignment:
	case ClassicOpOptionsChoice::input_test:
	case ClassicOpOptionsChoice::return_page:
	case ClassicOpOptionsChoice::switch_classic:
	case ClassicOpOptionsChoice::data_storage:
	case ClassicOpOptionsChoice::online_privacy:
	case ClassicOpOptionsChoice::credits:
	case ClassicOpOptionsChoice::diagnostics:
	case ClassicOpOptionsChoice::updates:
	case ClassicOpOptionsChoice::open_user_data:
	case ClassicOpOptionsChoice::open_replay_folder:
	case ClassicOpOptionsChoice::open_log_folder:
	case ClassicOpOptionsChoice::forget_room_identity:
	case ClassicOpOptionsChoice::check_for_updates:
	case ClassicOpOptionsChoice::confirm_switch_classic:
	case ClassicOpOptionsChoice::cancel_switch_classic:
	case ClassicOpOptionsChoice::confirm_forget_room_identity:
	case ClassicOpOptionsChoice::cancel_forget_room_identity:
	case ClassicOpOptionsChoice::quit:
		break;
	}
	return draw_option_choice(selected_, present::tram_white);
}

bool ClassicOpOptionsScene::push_audio_command(
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

void ClassicOpOptionsScene::finish(ClassicOpOptionsOutcome outcome) noexcept
{
	completion_ = ClassicOpOptionsCompletion(
		std::move(resident_), config_, master_volume_percent_,
		window_presentation_, music_volume_percent_, effect_volume_percent_,
		mute_when_unfocused_,
		std::move(raster_), std::move(tram_),
		menu_input_allowed_, outcome
	);
	phase_ = ClassicOpOptionsPhase::ready;
}

void ClassicOpOptionsScene::fail() noexcept
{
	phase_ = ClassicOpOptionsPhase::failed;
}

ClassicOpOptionsTick ClassicOpOptionsScene::tick(
	ClassicOpBoundary boundary,
	bool surface_changed,
	bool tram_changed,
	bool input_sampled,
	bool selection_changed,
	bool config_changed,
	bool forget_online_identity,
	bool check_for_updates,
	std::optional<ClassicOpOptionsOpenFolder> open_folder
) const noexcept
{
	return {
		.phase = phase_,
		.boundary = boundary,
		.surface_changed = surface_changed,
		.tram_changed = tram_changed,
		.input_sampled = input_sampled,
		.selection_changed = selection_changed,
		.config_changed = config_changed,
		.ready = phase_ == ClassicOpOptionsPhase::ready,
		.forget_online_identity = forget_online_identity,
		.check_for_updates = check_for_updates,
		.open_folder = open_folder,
	};
}

std::expected<ClassicOpOptionsTick, ClassicOpError>
ClassicOpOptionsScene::advance(ClassicOpInput input)
{
	if(audio_count_ != 0U) {
		return std::unexpected(options_error(
			ClassicOpErrorCode::audio_command_pending
		));
	}
	switch(phase_) {
	case ClassicOpOptionsPhase::grow_window: {
		bool sampled = false;
		bool tram_changed = false;
		if(!transition_input_latched_) {
			transition_input_ = input;
			transition_input_latched_ = true;
			tram_.fill({});
			sampled = true;
			tram_changed = true;
			if(patch_assets_ &&
				!restore_patch_rows(0U, 7U, box_left + box_main_width)) {
				fail();
				return std::unexpected(options_error(
					ClassicOpErrorCode::draw_failed
				));
			}
		}
		if(!grow_step()) {
			fail();
			return std::unexpected(options_error(
				ClassicOpErrorCode::draw_failed
			));
		}
		++phase_frame_;
		const auto frames = static_cast<std::uint16_t>(
			patch_assets_ ? 17U : grow_frames
		);
		if(phase_frame_ == frames) {
			phase_frame_ = 0U;
			phase_ = ClassicOpOptionsPhase::enter_options;
		}
		return tick(
			ClassicOpBoundary::vsync, true, tram_changed, sampled
		);
	}
	case ClassicOpOptionsPhase::enter_options:
		if(!draw_all_option_choices()) {
			fail();
			return std::unexpected(options_error(
				ClassicOpErrorCode::draw_failed
			));
		}
		menu_input_allowed_ = false;
		if(!transition_input_.any()) {
			menu_input_allowed_ = true;
		}
		if(transition_input_.any()) {
			menu_input_allowed_ = false;
		}
		++resident_.resident_rand;
		transition_input_latched_ = false;
		transition_input_ = {};
		phase_ = ClassicOpOptionsPhase::options_menu;
		return tick(ClassicOpBoundary::vsync, false, true);
	case ClassicOpOptionsPhase::options_menu: {
		const bool external_surface_changed = std::exchange(
			external_surface_changed_, false
		);
		bool selection_changed = false;
		bool config_changed = false;
		bool horizontal_processed = false;
		bool page_changed = false;
		bool key_config_requested = false;
		bool joy_config_requested = false;
		bool device_assignment_requested = false;
		bool input_test_requested = false;
		bool switch_classic_requested = false;
		bool forget_identity_requested = false;
		bool update_check_requested = false;
		std::optional<ClassicOpOptionsOpenFolder> open_folder{};
		if(!input.any()) {
			menu_input_allowed_ = true;
		}
		bool return_requested = false;
		if(menu_input_allowed_) {
			if(input.up) {
				if(!update_selection(-1)) {
					fail();
					return std::unexpected(options_error(
						ClassicOpErrorCode::draw_failed
					));
				}
				selection_changed = true;
			}
			if(input.down) {
				if(!update_selection(1)) {
					fail();
					return std::unexpected(options_error(
						ClassicOpErrorCode::draw_failed
					));
				}
				selection_changed = true;
			}
			if(input.right && (patch_assets_ || !input.left)) {
				ClassicOpInput horizontal{};
				horizontal.right = true;
				if(!update_horizontal(horizontal, config_changed)) {
					fail();
					return std::unexpected(options_error(
						ClassicOpErrorCode::audio_command_pending
					));
				}
				horizontal_processed = true;
			}
			if(input.left) {
				ClassicOpInput horizontal{};
				horizontal.left = true;
				if(!update_horizontal(horizontal, config_changed)) {
					fail();
					return std::unexpected(options_error(
						ClassicOpErrorCode::audio_command_pending
					));
				}
				horizontal_processed = true;
			}
			if(patch_assets_ && (input.ok || input.shot)) {
				ClassicOpOptionsPage child{};
				bool opens_child = true;
				switch(selected_) {
				case ClassicOpOptionsChoice::category_gameplay:
					child = ClassicOpOptionsPage::gameplay;
					break;
				case ClassicOpOptionsChoice::category_audio:
					child = ClassicOpOptionsPage::audio;
					break;
				case ClassicOpOptionsChoice::category_input:
					child = ClassicOpOptionsPage::input;
					break;
				case ClassicOpOptionsChoice::category_video:
					child = ClassicOpOptionsPage::video;
					break;
				case ClassicOpOptionsChoice::category_language:
					child = ClassicOpOptionsPage::language;
					break;
				case ClassicOpOptionsChoice::category_system:
					child = ClassicOpOptionsPage::system;
					break;
				case ClassicOpOptionsChoice::data_storage:
					child = ClassicOpOptionsPage::data_storage;
					break;
				case ClassicOpOptionsChoice::online_privacy:
					child = ClassicOpOptionsPage::online_privacy;
					break;
				case ClassicOpOptionsChoice::credits:
					child = ClassicOpOptionsPage::credits;
					break;
				case ClassicOpOptionsChoice::diagnostics:
					child = ClassicOpOptionsPage::diagnostics;
					break;
				case ClassicOpOptionsChoice::updates:
					child = ClassicOpOptionsPage::updates;
					break;
				default:
					opens_child = false;
					break;
				}
				if(opens_child) {
					if(!open_patch_page(child)) {
						fail();
						return std::unexpected(options_error(
							ClassicOpErrorCode::draw_failed
						));
					}
					page_changed = true;
				} else if(selected_ == ClassicOpOptionsChoice::return_page) {
					if(!return_to_patch_categories()) {
						fail();
						return std::unexpected(options_error(
							ClassicOpErrorCode::draw_failed
						));
					}
					page_changed = true;
				} else if(selected_ == ClassicOpOptionsChoice::language ||
					selected_ == ClassicOpOptionsChoice::window_mode ||
					selected_ == ClassicOpOptionsChoice::window_scale ||
					selected_ == ClassicOpOptionsChoice::aspect_ratio ||
					selected_ == ClassicOpOptionsChoice::filtering ||
					selected_ == ClassicOpOptionsChoice::photosensitivity) {
					ClassicOpInput horizontal{};
					horizontal.right = true;
					if(!update_horizontal(horizontal, config_changed)) {
						fail();
						return std::unexpected(options_error(
							ClassicOpErrorCode::draw_failed
						));
					}
					horizontal_processed = true;
				} else if(selected_ == ClassicOpOptionsChoice::key_config) {
					key_config_requested = true;
				} else if(selected_ == ClassicOpOptionsChoice::joy_config) {
					joy_config_requested = true;
				} else if(selected_ ==
					ClassicOpOptionsChoice::device_assignment) {
					device_assignment_requested = true;
				} else if(selected_ == ClassicOpOptionsChoice::input_test) {
					input_test_requested = true;
				} else if(selected_ == ClassicOpOptionsChoice::switch_classic) {
					if(!open_patch_page(
						ClassicOpOptionsPage::switch_confirmation
					)) {
						fail();
						return std::unexpected(options_error(
							ClassicOpErrorCode::draw_failed
						));
					}
					page_changed = true;
				} else if(selected_ ==
					ClassicOpOptionsChoice::forget_room_identity &&
					online_identity_reset_available_) {
					if(!open_patch_page(
						ClassicOpOptionsPage::forget_identity_confirmation
					)) {
						fail();
						return std::unexpected(options_error(
							ClassicOpErrorCode::draw_failed
						));
					}
					page_changed = true;
				} else if(selected_ ==
					ClassicOpOptionsChoice::check_for_updates &&
					update_available_ && !update_check_in_progress_) {
					update_check_in_progress_ = true;
					update_check_outcome_.reset();
					if(!draw_patch_update_status()) {
						fail();
						return std::unexpected(options_error(
							ClassicOpErrorCode::draw_failed
						));
					}
					update_check_requested = true;
				} else if(selected_ == ClassicOpOptionsChoice::open_user_data) {
					open_folder = ClassicOpOptionsOpenFolder::user_data;
				} else if(selected_ ==
					ClassicOpOptionsChoice::open_replay_folder) {
					open_folder = ClassicOpOptionsOpenFolder::replays;
				} else if(selected_ ==
					ClassicOpOptionsChoice::open_log_folder) {
					open_folder = ClassicOpOptionsOpenFolder::user_data;
				} else if(selected_ ==
					ClassicOpOptionsChoice::confirm_switch_classic) {
					switch_classic_requested = true;
				} else if(selected_ ==
					ClassicOpOptionsChoice::cancel_switch_classic) {
					if(!return_to_patch_categories()) {
						fail();
						return std::unexpected(options_error(
							ClassicOpErrorCode::draw_failed
						));
					}
					page_changed = true;
				} else if(selected_ ==
					ClassicOpOptionsChoice::confirm_forget_room_identity) {
					forget_identity_requested = true;
					if(!return_to_patch_categories()) {
						fail();
						return std::unexpected(options_error(
							ClassicOpErrorCode::draw_failed
						));
					}
					page_changed = true;
				} else if(selected_ ==
					ClassicOpOptionsChoice::cancel_forget_room_identity) {
					if(!return_to_patch_categories()) {
						fail();
						return std::unexpected(options_error(
							ClassicOpErrorCode::draw_failed
						));
					}
					page_changed = true;
				}
			}
			if(input.cancel && patch_assets_ &&
				page_ != ClassicOpOptionsPage::categories) {
				if(!return_to_patch_categories()) {
					fail();
					return std::unexpected(options_error(
						ClassicOpErrorCode::draw_failed
					));
				}
				page_changed = true;
			} else {
				return_requested = input.cancel ||
					((input.ok || input.shot) &&
					 selected_ == ClassicOpOptionsChoice::quit);
			}
		}
		if(input.any()) {
			menu_input_allowed_ = false;
		}
		++resident_.resident_rand;
		if(key_config_requested) {
			finish(ClassicOpOptionsOutcome::key_config);
			return tick(
				ClassicOpBoundary::vsync, false, horizontal_processed, true,
				selection_changed, config_changed
			);
		}
		if(joy_config_requested) {
			finish(ClassicOpOptionsOutcome::joy_config);
			return tick(
				ClassicOpBoundary::vsync, false, horizontal_processed, true,
				selection_changed, config_changed
			);
		}
		if(device_assignment_requested || input_test_requested) {
			finish(device_assignment_requested ?
				ClassicOpOptionsOutcome::device_assignment :
				ClassicOpOptionsOutcome::input_test);
			return tick(
				ClassicOpBoundary::vsync, false, horizontal_processed, true,
				selection_changed, config_changed
			);
		}
		if(return_requested || switch_classic_requested) {
			pending_outcome_ = switch_classic_requested ?
				ClassicOpOptionsOutcome::switch_classic :
				ClassicOpOptionsOutcome::return_main;
			phase_ = ClassicOpOptionsPhase::shrink_window;
			phase_frame_ = 0U;
			box_right_left_ = box_left +
				(patch_assets_ ? box_option_width : box_submenu_width) -
				opwin_step_width;
			transition_input_latched_ = false;
		}
		return tick(
			ClassicOpBoundary::vsync, external_surface_changed,
			selection_changed || horizontal_processed || page_changed ||
				update_check_requested, true,
			selection_changed, config_changed, forget_identity_requested,
			update_check_requested, open_folder
		);
	}
	case ClassicOpOptionsPhase::shrink_window: {
		bool sampled = false;
		bool tram_changed = false;
		if(!transition_input_latched_) {
			transition_input_ = input;
			transition_input_latched_ = true;
			if(patch_assets_ && !restore_patch_rows(
				0U, 7U, box_left + box_option_width
			)) {
				fail();
				return std::unexpected(options_error(
					ClassicOpErrorCode::draw_failed
				));
			}
			tram_.fill({});
			sampled = true;
			tram_changed = true;
		}
		if(!shrink_step()) {
			fail();
			return std::unexpected(options_error(
				ClassicOpErrorCode::draw_failed
			));
		}
		++phase_frame_;
		const auto frames = static_cast<std::uint16_t>(
			patch_assets_ ? 18U : shrink_frames
		);
		if(phase_frame_ == frames) {
			phase_frame_ = 0U;
			phase_ = ClassicOpOptionsPhase::return_main;
		}
		return tick(
			ClassicOpBoundary::vsync, true, tram_changed, sampled
		);
	}
	case ClassicOpOptionsPhase::return_main:
		if(!draw_main_choices()) {
			fail();
			return std::unexpected(options_error(
				ClassicOpErrorCode::draw_failed
			));
		}
		menu_input_allowed_ = false;
		if(!transition_input_.any()) {
			menu_input_allowed_ = true;
		}
		if(transition_input_.any()) {
			menu_input_allowed_ = false;
		}
		++resident_.resident_rand;
		finish(pending_outcome_);
		return tick(ClassicOpBoundary::vsync, false, true);
	case ClassicOpOptionsPhase::ready:
	case ClassicOpOptionsPhase::consumed:
	case ClassicOpOptionsPhase::failed:
		return std::unexpected(options_error(
			ClassicOpErrorCode::invalid_state
		));
	}
	return std::unexpected(options_error(ClassicOpErrorCode::invalid_state));
}

std::optional<ClassicAudioCommand>
ClassicOpOptionsScene::take_audio_command() noexcept
{
	if(audio_count_ == 0U) {
		return std::nullopt;
	}
	const auto command = audio_commands_[audio_head_];
	audio_head_ = (audio_head_ + 1U) % audio_commands_.size();
	--audio_count_;
	return command;
}

std::optional<ClassicOpOptionsCompletion>
ClassicOpOptionsScene::take_completion() noexcept
{
	if(phase_ != ClassicOpOptionsPhase::ready || !completion_) {
		return std::nullopt;
	}
	auto result = std::move(completion_);
	completion_.reset();
	phase_ = ClassicOpOptionsPhase::consumed;
	return result;
}

ClassicOpOptionsPhase ClassicOpOptionsScene::phase() const noexcept
{
	return phase_;
}

ClassicOpOptionsPage ClassicOpOptionsScene::page() const noexcept
{
	return page_;
}

ClassicOpOptionsChoice ClassicOpOptionsScene::selected_choice() const noexcept
{
	return selected_;
}

ClassicConfigOptions ClassicOpOptionsScene::config() const noexcept
{
	return config_;
}

std::uint8_t ClassicOpOptionsScene::master_volume_percent() const noexcept
{
	return master_volume_percent_;
}

std::uint8_t ClassicOpOptionsScene::music_volume_percent() const noexcept
{
	return music_volume_percent_;
}

std::uint8_t ClassicOpOptionsScene::effect_volume_percent() const noexcept
{
	return effect_volume_percent_;
}

bool ClassicOpOptionsScene::mute_when_unfocused() const noexcept
{
	return mute_when_unfocused_;
}

NewClassicWindowPresentation
ClassicOpOptionsScene::window_presentation() const noexcept
{
	return window_presentation_;
}

const GameInit &ClassicOpOptionsScene::resident() const noexcept
{
	return resident_;
}

const present::Raster &ClassicOpOptionsScene::raster() const noexcept
{
	return raster_;
}

const present::Tram &ClassicOpOptionsScene::tram() const noexcept
{
	return tram_;
}

bool ClassicOpOptionsScene::input_allowed() const noexcept
{
	return menu_input_allowed_;
}

} // namespace th03::app
