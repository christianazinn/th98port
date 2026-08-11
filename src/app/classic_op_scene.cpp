#include "classic_op_scene_internal.hpp"

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
#include "th03/present/pi.hpp"

namespace th03::app {
namespace {

constexpr std::int32_t box_left = 160;
constexpr std::int32_t box_top = 256;
constexpr std::int32_t box_main_center_x = 228;
constexpr std::int32_t box_height = 128;
constexpr std::int32_t box_main_width = 136;
constexpr std::int32_t opwin_width = 16;
constexpr std::int32_t opwin_step_width = 8;
constexpr std::int32_t initial_box_width =
	opwin_width + opwin_step_width;
constexpr std::uint16_t long_shift_frames = 72U;
constexpr std::uint16_t selection_pause_frames = 16U;
constexpr std::uint16_t flash_frames = 16U;
constexpr std::uint16_t fast_fade_frames = 25U;
constexpr std::uint16_t demo_wait_frames = 520U;

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

constexpr std::array<std::array<std::int8_t, 2U>, 4U> demo_pairings{{
	{{3, 1}},
	{{5, 13}},
	{{7, 11}},
	{{9, 5}},
}};
constexpr std::array<std::int32_t, 4U> demo_random{{
	600, 1000, 3200, 500,
}};

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
		if(resource.bytes->size() != canonical->expected_bytes ||
			(authenticate_resource_bytes && main_resource_fnv1a64(
				*resource.bytes
			) != canonical->expected_fnv1a64)) {
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

[[nodiscard]] std::size_t choice_index(ClassicOpMenuChoice choice) noexcept
{
	return static_cast<std::size_t>(choice);
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
	case ClassicOpMenuChoice::quit: return ClassicOpRoute::quit;
	}
	return ClassicOpRoute::quit;
}

} // namespace

ClassicOpCompletion::ClassicOpCompletion(
	ClassicOpRoute route,
	GameInit resident,
	present::Raster raster,
	present::Tram tram
) noexcept :
	route_(route),
	resident_(std::move(resident)),
	raster_(std::move(raster)),
	tram_(std::move(tram))
{}

ClassicOpRoute ClassicOpCompletion::route() const noexcept
{
	return route_;
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
	ClassicOpTitleMode title_mode
) noexcept :
	prepared_(std::move(prepared)),
	resident_(std::move(resident)),
	page_snapshot_(present::graphics_page_pixels, 0U)
{
	tram_.fill({});
	prepared_->gaiji->install(tram_);
	if(title_mode == ClassicOpTitleMode::long_animation) {
		static_cast<void>(push_audio_command({
			.type = ClassicAudioCommandType::stop_song,
		}));
		static_cast<void>(push_audio_command({
			.type = ClassicAudioCommandType::load_song,
			.cue = ClassicMusicCue::menu_main,
		}));
	}
	resident_.fast_op_animation =
		(title_mode == ClassicOpTitleMode::long_animation) ? 1U : 0U;
	const auto initialized =
		(title_mode == ClassicOpTitleMode::long_animation) ?
			initialize_long_title() : initialize_fast_title();
	if(!initialized) {
		fail();
	}
}

std::expected<std::unique_ptr<ClassicOpTitleMenuScene>, ClassicOpError>
ClassicOpTitleMenuScene::create(
	GameInit resident,
	ClassicOpTitleMode title_mode,
	std::span<const LoadedMainResource> resources
)
{
	return create_impl(
		std::move(resident), title_mode, resources, true
	);
}

std::expected<std::unique_ptr<ClassicOpTitleMenuScene>, ClassicOpError>
ClassicOpTitleMenuScene::create_impl(
	GameInit resident,
	ClassicOpTitleMode title_mode,
	std::span<const LoadedMainResource> resources,
	bool authenticate_resource_bytes
)
{
	const bool mode_matches_resident =
		(title_mode == ClassicOpTitleMode::long_animation) ?
			(resident.fast_op_animation == 0U) :
			(resident.fast_op_animation != 0U);
	if(!mode_matches_resident) {
		return std::unexpected(op_error(ClassicOpErrorCode::invalid_state));
	}
	auto validated = validate_resource_set(
		resources, authenticate_resource_bytes
	);
	if(!validated) {
		return std::unexpected(std::move(validated.error()));
	}

	auto long_background = decode_asset<assets::PiImage>(
		resources,
		{.role = MainResourceRole::op_title_long_background},
		assets::decode_pi
	);
	if(!long_background) {
		return std::unexpected(std::move(long_background.error()));
	}
	auto menu_background = decode_asset<assets::PiImage>(
		resources,
		{.role = MainResourceRole::op_menu_background},
		assets::decode_pi
	);
	if(!menu_background) {
		return std::unexpected(std::move(menu_background.error()));
	}
	auto window_sheet = decode_asset<assets::BfntSheet>(
		resources,
		{.role = MainResourceRole::op_window_sheet},
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
		resources,
		{.role = MainResourceRole::op_gaiji_font},
		[](std::span<const std::byte> bytes) {
			return assets::decode_bfnt(
				bytes, assets::BfntConsumerPolicy::gaiji
			);
		}
	);
	if(!gaiji) {
		return std::unexpected(std::move(gaiji.error()));
	}
	return create_predecoded(
		std::move(resident), title_mode, std::move(*long_background),
		std::move(*menu_background), std::move(*window_sheet),
		std::move(*gaiji)
	);
}

std::expected<std::unique_ptr<ClassicOpTitleMenuScene>, ClassicOpError>
ClassicOpTitleMenuScene::create_predecoded(
	GameInit resident,
	ClassicOpTitleMode title_mode,
	assets::PiImage long_background,
	assets::PiImage menu_background,
	assets::BfntSheet window_sheet,
	assets::BfntSheet gaiji
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
			std::move(prepared), std::move(resident), title_mode
		)
	);
	if(result->phase_ == ClassicOpPhase::failed) {
		return std::unexpected(op_error(ClassicOpErrorCode::draw_failed));
	}
	return result;
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
	if(!raster_.clear(
		{0, 0, present::graphics_width, present::graphics_height}, 0U
	)) {
		return false;
	}
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

bool ClassicOpTitleMenuScene::draw_menu_background() noexcept
{
	const auto &image = prepared_->menu_background;
	if(!image.palette || !apply_palette(*image.palette)) {
		return false;
	}
	raster_.apply_pending_palette();
	return raster_.set_access_page(present::Page::zero) &&
		present::pi_put_8(raster_, 0, 0, image) &&
		raster_.set_shown_page(present::Page::zero);
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
		raster_, *prepared_->window_sheet, {left, box_top}, glyph
	);
}

bool ClassicOpTitleMenuScene::draw_choice(
	ClassicOpMenuChoice choice,
	present::TramAttribute attribute
) noexcept
{
	const auto index = choice_index(choice);
	if(index >= menu_labels.size()) {
		return false;
	}
	const auto &label = menu_labels[index];
	const auto shifted_width = static_cast<std::int32_t>(
		static_cast<std::int16_t>(label.length) + label.center_shift
	) * present::gaiji_width;
	const auto tram_left =
		(box_main_center_x - (shifted_width / 2)) / 8;
	const auto tram_y = (box_top / present::gaiji_height) + 1 + label.line;
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
	for(std::uint8_t raw = 0U; raw < menu_labels.size(); ++raw) {
		const auto choice = static_cast<ClassicOpMenuChoice>(raw);
		if(!draw_choice(
			choice,
			(choice == selected_) ? present::tram_white : present::tram_black
		)) {
			return false;
		}
	}
	menu_input_allowed_ = true;
	return true;
}

bool ClassicOpTitleMenuScene::update_selection(int direction) noexcept
{
	const auto old = selected_;
	auto next = static_cast<int>(choice_index(selected_)) + direction;
	if(next < 0) {
		next = static_cast<int>(menu_labels.size() - 1U);
	} else if(next >= static_cast<int>(menu_labels.size())) {
		next = 0;
	}
	selected_ = static_cast<ClassicOpMenuChoice>(next);
	return draw_choice(old, present::tram_black) &&
		draw_choice(selected_, present::tram_white);
}

bool ClassicOpTitleMenuScene::begin_menu_swap() noexcept
{
	raster_.set_palette_tone(200);
	if(!draw_menu_background() || !push_audio_command({
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
		box_right_left_, box_top,
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

void ClassicOpTitleMenuScene::select_route(ClassicOpRoute route) noexcept
{
	completion_ = ClassicOpCompletion(
		route, std::move(resident_), std::move(raster_), std::move(tram_)
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
			phase_frame_ = 0U;
			phase_ = ClassicOpPhase::wait_title_input;
		}
		return tick(ClassicOpBoundary::vsync, true);
	case ClassicOpPhase::fast_black:
		if(!push_audio_command({
			.type = ClassicAudioCommandType::stop_song,
		}) || !push_audio_command({
			.type = ClassicAudioCommandType::load_song,
			.cue = ClassicMusicCue::menu_main,
		})) {
			fail();
			return std::unexpected(op_error(
				ClassicOpErrorCode::audio_command_pending
			));
		}
		phase_ = ClassicOpPhase::fast_initial_pause;
		return tick(ClassicOpBoundary::vsync, true);
	case ClassicOpPhase::fast_initial_pause:
		raster_.set_palette_tone(0);
		if(!draw_menu_background() || !push_audio_command({
			.type = ClassicAudioCommandType::play_song,
			.cue = ClassicMusicCue::menu_main,
		})) {
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
		if(title_wait_frame_ > demo_wait_frames) {
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
			} else if(input.ok || input.shot) {
				selected_route = route_for_choice(selected_);
			}
		}
		if(input.any()) {
			menu_input_allowed_ = false;
		}
		++resident_.resident_rand;
		if(selected_route) {
			select_route(*selected_route);
		}
		return tick(
			ClassicOpBoundary::vsync, false, tram_changed, true,
			selection_changed
		);
	}
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
	if(phase_ != ClassicOpPhase::ready || !completion_) {
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
