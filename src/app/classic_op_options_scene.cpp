#include "classic_op_options_scene_internal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "th03/assets/decode.hpp"

namespace th03::app {
namespace {

constexpr std::int32_t box_left = 160;
constexpr std::int32_t box_top = 256;
constexpr std::int32_t box_height = 128;
constexpr std::int32_t box_main_width = 136;
constexpr std::int32_t box_submenu_width = 240;
constexpr std::int32_t box_main_center_x = 228;
constexpr std::int32_t box_submenu_center_x = 280;
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
	const auto y = (box_top / present::gaiji_height) + 1 + line;
	return put_gaiji_run(tram, left, y, glyphs, attribute);
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
	present::Raster raster,
	present::Tram tram,
	bool main_input_allowed
) noexcept :
	resident_(std::move(resident)),
	config_(config),
	raster_(std::move(raster)),
	tram_(std::move(tram)),
	main_input_allowed_(main_input_allowed)
{}

const GameInit &ClassicOpOptionsCompletion::resident() const noexcept
{
	return resident_;
}

ClassicConfigOptions ClassicOpOptionsCompletion::config() const noexcept
{
	return config_;
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

ClassicOpOptionsScene::ClassicOpOptionsScene(
	std::unique_ptr<Prepared> prepared,
	GameInit resident,
	ClassicConfigOptions config,
	bool fm_available,
	present::Raster raster,
	present::Tram tram
) noexcept :
	prepared_(std::move(prepared)),
	resident_(std::move(resident)),
	config_(config),
	raster_(std::move(raster)),
	tram_(std::move(tram)),
	phase_(ClassicOpOptionsPhase::grow_window),
	box_right_left_(box_left + box_main_width - opwin_step_width),
	fm_available_(fm_available)
{}

std::expected<std::unique_ptr<ClassicOpOptionsScene>, ClassicOpError>
ClassicOpOptionsScene::create_from_op(
	const ClassicOpCompletion &completion,
	ClassicConfigOptions config,
	bool fm_available,
	std::span<const LoadedMainResource> resources
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
		completion.tram(), std::move(*background), std::move(*window)
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
	assets::BfntSheet window_sheet
)
{
	if(!classic_config_valid(config) ||
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
	return std::unique_ptr<ClassicOpOptionsScene>(new ClassicOpOptionsScene(
		std::move(prepared), std::move(resident), config, fm_available,
		std::move(raster), std::move(tram)
	));
}

bool ClassicOpOptionsScene::restore_menu_background(
	std::int32_t left,
	std::uint16_t width
) noexcept
{
	const auto &image = prepared_->menu_background.image;
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

bool ClassicOpOptionsScene::draw_option_choice(
	ClassicOpOptionsChoice choice,
	present::TramAttribute attribute
) noexcept
{
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
		const auto y = (box_top / present::gaiji_height) + 3;
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
	case ClassicOpOptionsChoice::quit:
		return put_centered(
			tram_, box_submenu_center_x, 5, 0, label_quit, attribute
		);
	}
	return false;
}

bool ClassicOpOptionsScene::draw_all_option_choices() noexcept
{
	for(std::uint8_t raw = 0U; raw < 4U; ++raw) {
		const auto choice = static_cast<ClassicOpOptionsChoice>(raw);
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
	auto raw = ring_step(
		static_cast<std::uint8_t>(selected_), direction, 3U
	);
	selected_ = static_cast<ClassicOpOptionsChoice>(raw);
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
	case ClassicOpOptionsChoice::bgm:
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
	case ClassicOpOptionsChoice::key_mode: {
		const auto next = ring_step(
			static_cast<std::uint8_t>(config_.key_mode), direction, 2U
		);
		config_.key_mode = static_cast<ClassicKeyMode>(next);
		resident_.key_mode = next;
		config_changed = true;
		break;
	}
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

void ClassicOpOptionsScene::finish() noexcept
{
	completion_ = ClassicOpOptionsCompletion(
		std::move(resident_), config_, std::move(raster_), std::move(tram_),
		menu_input_allowed_
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
	bool config_changed
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
		}
		if(!grow_step()) {
			fail();
			return std::unexpected(options_error(
				ClassicOpErrorCode::draw_failed
			));
		}
		++phase_frame_;
		if(phase_frame_ == grow_frames) {
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
		bool selection_changed = false;
		bool config_changed = false;
		bool horizontal_processed = false;
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
			if(input.left || input.right) {
				if(!update_horizontal(input, config_changed)) {
					fail();
					return std::unexpected(options_error(
						ClassicOpErrorCode::audio_command_pending
					));
				}
				horizontal_processed = true;
			}
			return_requested = input.cancel ||
				((input.ok || input.shot) &&
				 selected_ == ClassicOpOptionsChoice::quit);
		}
		if(input.any()) {
			menu_input_allowed_ = false;
		}
		++resident_.resident_rand;
		if(return_requested) {
			phase_ = ClassicOpOptionsPhase::shrink_window;
			phase_frame_ = 0U;
			box_right_left_ =
				box_left + box_submenu_width - opwin_step_width;
			transition_input_latched_ = false;
		}
		return tick(
			ClassicOpBoundary::vsync, false,
			selection_changed || horizontal_processed, true,
			selection_changed, config_changed
		);
	}
	case ClassicOpOptionsPhase::shrink_window: {
		bool sampled = false;
		bool tram_changed = false;
		if(!transition_input_latched_) {
			transition_input_ = input;
			transition_input_latched_ = true;
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
		if(phase_frame_ == shrink_frames) {
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
		finish();
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

ClassicOpOptionsChoice ClassicOpOptionsScene::selected_choice() const noexcept
{
	return selected_;
}

ClassicConfigOptions ClassicOpOptionsScene::config() const noexcept
{
	return config_;
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
