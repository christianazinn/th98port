#include "classic_op_music_room_internal.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "th03/assets/decode.hpp"
#include "th03/exact/trig.hpp"
#include "th03/present/pi.hpp"
#include "th03/present/planar.hpp"
#include "th03/present/triangles.hpp"

namespace th03::app {
namespace {

constexpr std::uint8_t choice_empty = 19U;
constexpr std::uint8_t choice_quit = 20U;
constexpr std::size_t plane_stride = present::graphics_width / 8U;
constexpr present::Rect full_screen{
	0, 0, present::graphics_width, present::graphics_height,
};
constexpr present::Rect comment_title_rect{304, 64, 320U, 16U};
constexpr present::Rect comment_body_rect{304, 80, 320U, 304U};
constexpr present::GrcgClipRect full_clip{0, 0, 639, 399};

[[nodiscard]] ClassicOpMusicRoomError music_error(
	ClassicOpMusicRoomErrorCode code,
	std::uint8_t track = 0U,
	std::uint8_t line = 0U
) noexcept
{
	return {.code = code, .track = track, .line = line};
}

[[nodiscard]] ClassicOpMusicRoomError text_error(
	present::GraphicsTextError error,
	std::uint8_t track,
	std::uint8_t line
) noexcept
{
	auto result = music_error(
		ClassicOpMusicRoomErrorCode::font_prepare_failed,
		track,
		line
	);
	result.text_error.emplace(error);
	return result;
}

[[nodiscard]] ClassicOpMusicRoomError resource_error(
	ClassicOpMusicRoomErrorCode code,
	std::optional<MainResourceKey> resource = std::nullopt
) noexcept
{
	auto result = music_error(code);
	result.resource = resource;
	return result;
}

[[nodiscard]] ClassicOpMusicRoomError decode_error(
	MainResourceKey resource,
	assets::AssetError error
)
{
	auto result = resource_error(
		ClassicOpMusicRoomErrorCode::asset_decode_failed, resource
	);
	result.asset_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicOpMusicRoomError compiled_text_error(
	OpCompiledTextError error
) noexcept
{
	auto result = music_error(
		ClassicOpMusicRoomErrorCode::compiled_text_import_failed
	);
	result.compiled_text_error.emplace(error);
	return result;
}

[[nodiscard]] std::expected<void, ClassicOpMusicRoomError>
validate_music_room_resources(
	std::span<const LoadedMainResource> resources
)
{
	const auto expected = op_music_room_resource_keys();
	if(resources.size() != expected.size()) {
		return std::unexpected(resource_error(
			ClassicOpMusicRoomErrorCode::resource_set_mismatch
		));
	}
	for(std::size_t index = 0U; index < expected.size(); ++index) {
		const auto key = expected[index];
		const auto &resource = resources[index];
		if(resource.spec.key != key) {
			return std::unexpected(resource_error(
				ClassicOpMusicRoomErrorCode::resource_set_mismatch, key
			));
		}
		if(!resource.bytes) {
			return std::unexpected(resource_error(
				ClassicOpMusicRoomErrorCode::resource_bytes_missing, key
			));
		}
		const auto canonical = main_resource_spec(key);
		if(!canonical || resource.spec != *canonical) {
			return std::unexpected(resource_error(
				ClassicOpMusicRoomErrorCode::resource_set_mismatch, key
			));
		}
		if(resource.bytes->size() != canonical->expected_bytes ||
			main_resource_fnv1a64(*resource.bytes) !=
				canonical->expected_fnv1a64) {
			return std::unexpected(resource_error(
				ClassicOpMusicRoomErrorCode::resource_integrity_failed, key
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

[[nodiscard]] constexpr present::Page other_page(present::Page page) noexcept
{
	return page == present::Page::zero ? present::Page::one : present::Page::zero;
}

[[nodiscard]] constexpr std::int16_t wrap_word(std::int32_t value) noexcept
{
	return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(
		static_cast<std::uint32_t>(value) & 0xFFFFU
	));
}

[[nodiscard]] constexpr std::int16_t add_word(
	std::int16_t left,
	std::int32_t right
) noexcept
{
	return wrap_word(static_cast<std::int32_t>(left) + right);
}

[[nodiscard]] constexpr std::int16_t arithmetic_shift_right(
	std::int32_t value,
	std::uint8_t bits
) noexcept
{
	const auto divisor = static_cast<std::int32_t>(1U << bits);
	const auto shifted = (value >= 0) ? (value / divisor) :
		-(((-value) + divisor - 1) / divisor);
	return wrap_word(shifted);
}

[[nodiscard]] std::uint16_t irand(std::uint32_t &seed) noexcept
{
	seed = (seed * 0x015A4E35U) + 1U;
	return static_cast<std::uint16_t>((seed >> 16U) & 0x7FFFU);
}

[[nodiscard]] bool valid_background(const assets::PiImage &background) noexcept
{
	if(!background.palette ||
		background.image.width != present::graphics_width ||
		background.image.height != present::graphics_height ||
		background.image.indices.size() != present::graphics_page_pixels) {
		return false;
	}
	for(const auto pixel : background.image.indices) {
		if(pixel > 0x0FU) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool valid_carry(const ClassicOpMusicRoomCarry &carry) noexcept
{
	return carry.track_playing < classic_music_room_track_count;
}

[[nodiscard]] bool valid_labels(const OpMusicRoomTextImport &labels) noexcept
{
	if(labels.profile != OpExecutableProfile::unpacked_original_japanese) {
		return false;
	}
	for(std::size_t index = 0U; index < labels.choices.size(); ++index) {
		const auto expected = (index == choice_empty) ? 0U : 32U;
		if(labels.choices[index].size() != expected) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] std::span<const std::uint8_t> comment_line(
	std::span<const std::byte> comments,
	std::size_t track,
	std::size_t line
) noexcept
{
	const auto offset = ((track * classic_music_room_comment_lines) + line) *
		classic_music_room_comment_line_bytes;
	const auto bytes = comments.subspan(
		offset,
		classic_music_room_comment_text_bytes
	);
	return {
		reinterpret_cast<const std::uint8_t *>(bytes.data()),
		bytes.size(),
	};
}

[[nodiscard]] bool valid_comments(std::span<const std::byte> comments) noexcept
{
	if(comments.size() != classic_music_room_comment_file_bytes) {
		return false;
	}
	for(std::size_t line = 0U;
		line < classic_music_room_track_count *
			classic_music_room_comment_lines;
		++line) {
		const auto offset = line * classic_music_room_comment_line_bytes;
		if(comments[offset + 40U] != std::byte{0x0DU} ||
			comments[offset + 41U] != std::byte{0x0AU}) {
			return false;
		}
	}
	return true;
}

} // namespace

ClassicOpMusicRoomCompletion::ClassicOpMusicRoomCompletion(
	GameInit resident,
	ClassicOpMusicRoomCarry carry,
	present::Raster raster,
	present::Tram tram
) noexcept :
	resident_(std::move(resident)),
	carry_(std::move(carry)),
	raster_(std::move(raster)),
	tram_(std::move(tram))
{}

const GameInit &ClassicOpMusicRoomCompletion::resident() const noexcept
{
	return resident_;
}

const ClassicOpMusicRoomCarry &ClassicOpMusicRoomCompletion::carry() const noexcept
{
	return carry_;
}

const present::Raster &ClassicOpMusicRoomCompletion::raster() const noexcept
{
	return raster_;
}

const present::Tram &ClassicOpMusicRoomCompletion::tram() const noexcept
{
	return tram_;
}

ClassicOpMusicRoomScene::ClassicOpMusicRoomScene(
	std::unique_ptr<Prepared> prepared,
	GameInit resident,
	ClassicOpMusicRoomCarry carry,
	present::Raster raster,
	present::Tram tram
) noexcept :
	prepared_(std::move(prepared)),
	resident_(std::move(resident)),
	carry_(std::move(carry)),
	raster_(std::move(raster)),
	tram_(std::move(tram)),
	selection_(carry_.track_playing)
{}

ClassicOpMusicRoomScene::~ClassicOpMusicRoomScene() = default;

std::expected<
	std::unique_ptr<ClassicOpMusicRoomScene>,
	ClassicOpMusicRoomError
> ClassicOpMusicRoomScene::create_from_op(
	const ClassicOpCompletion &completion,
	ClassicOpMusicRoomCarry carry,
	std::span<const LoadedMainResource> resources,
	std::span<const std::byte> op_executable,
	const present::AnkGlyphProvider &font
)
{
	if(completion.route() != ClassicOpRoute::music_room) {
		return std::unexpected(music_error(
			ClassicOpMusicRoomErrorCode::invalid_route
		));
	}
	if(const auto validated = validate_music_room_resources(resources);
		!validated) {
		return std::unexpected(validated.error());
	}
	const auto *background_resource = find_resource(
		resources, MainResourceRole::op_music_room_background
	);
	const auto *comments_resource = find_resource(
		resources, MainResourceRole::op_music_room_comments
	);
	if(!background_resource || !background_resource->bytes ||
		!comments_resource || !comments_resource->bytes) {
		return std::unexpected(resource_error(
			ClassicOpMusicRoomErrorCode::resource_set_mismatch
		));
	}
	auto background = assets::decode_pi(*background_resource->bytes);
	if(!background) {
		return std::unexpected(decode_error(
			background_resource->spec.key, std::move(background.error())
		));
	}
	auto labels = import_op_music_room_text(op_executable);
	if(!labels) {
		return std::unexpected(compiled_text_error(labels.error()));
	}
	return create_predecoded(
		completion.resident(), std::move(carry), std::move(*background),
		*labels, *comments_resource->bytes, font, completion.raster(),
		completion.tram()
	);
}

std::expected<
	std::unique_ptr<ClassicOpMusicRoomScene>,
	ClassicOpMusicRoomError
> ClassicOpMusicRoomScene::create_predecoded(
	GameInit resident,
	ClassicOpMusicRoomCarry carry,
	assets::PiImage background,
	const OpMusicRoomTextImport &labels,
	std::span<const std::byte> comments,
	const present::AnkGlyphProvider &font,
	present::Raster raster,
	present::Tram tram
)
{
	if(raster.raster_mode() != present::RasterMode::lines_400 ||
		!valid_carry(carry)) {
		return std::unexpected(music_error(
			ClassicOpMusicRoomErrorCode::invalid_context
		));
	}
	if(!valid_background(background)) {
		return std::unexpected(music_error(
			ClassicOpMusicRoomErrorCode::invalid_background
		));
	}
	if(!valid_labels(labels)) {
		return std::unexpected(music_error(
			ClassicOpMusicRoomErrorCode::invalid_compiled_text
		));
	}
	if(!valid_comments(comments)) {
		return std::unexpected(music_error(
			ClassicOpMusicRoomErrorCode::malformed_comment_file
		));
	}

	auto prepared = std::make_unique<Prepared>();
	prepared->background = std::move(background);
	for(std::size_t choice = 0U; choice < prepared->choices.size(); ++choice) {
		auto text = present::prepare_graphics_text_8x16(
			labels.choices[choice],
			font
		);
		if(!text) {
			return std::unexpected(text_error(
				text.error(),
				static_cast<std::uint8_t>(choice),
				0U
			));
		}
		prepared->choices[choice].emplace(std::move(*text));
	}
	for(std::size_t track = 0U; track < prepared->comments.size(); ++track) {
		for(std::size_t line = 0U;
			line < prepared->comments[track].size();
			++line) {
			auto text = present::prepare_graphics_text_8x16(
				comment_line(comments, track, line),
				font
			);
			if(!text) {
				return std::unexpected(text_error(
					text.error(),
					static_cast<std::uint8_t>(track),
					static_cast<std::uint8_t>(line)
				));
			}
			prepared->comments[track][line].emplace(std::move(*text));
		}
	}

	auto scene = std::unique_ptr<ClassicOpMusicRoomScene>(
		new ClassicOpMusicRoomScene(
			std::move(prepared),
			std::move(resident),
			std::move(carry),
			std::move(raster),
			std::move(tram)
		)
	);
	if(!scene->initialize_visuals()) {
		return std::unexpected(music_error(
			ClassicOpMusicRoomErrorCode::draw_failed
		));
	}
	return scene;
}

bool ClassicOpMusicRoomScene::initialize_visuals() noexcept
{
	tram_.fill({});
	raster_.set_palette_tone(0);
	for(std::size_t index = 0U;
		index < prepared_->background.palette->colors.size();
		++index) {
		if(!raster_.set_pending_palette_entry(
			static_cast<std::uint8_t>(index),
			prepared_->background.palette->colors[index]
		)) {
			return false;
		}
	}
	raster_.apply_pending_palette();
	if(!raster_.set_shown_page(present::Page::zero) ||
		!raster_.set_access_page(present::Page::zero) ||
		!raster_.clear(full_screen, 0U) ||
		!raster_.set_access_page(present::Page::one) ||
		!present::pi_put_8(raster_, 0, 0, prepared_->background) ||
		!raster_.set_access_page(present::Page::zero) ||
		!present::pi_put_8(raster_, 0, 0, prepared_->background) ||
		!draw_tracklist() ||
		!raster_.set_access_page(present::Page::one) ||
		!snapshot_blue_plane() || !draw_comment(carry_.track_playing) ||
		!snapshot_comment_blue() ||
		!raster_.set_access_page(present::Page::zero) ||
		!draw_comment(carry_.track_playing) ||
		!snapshot_comment_blue()) {
		return false;
	}
	raster_.set_palette_tone(100);
	raster_.apply_pending_palette();
	phase_ = ClassicOpMusicRoomPhase::held_release_poll;
	return true;
}

bool ClassicOpMusicRoomScene::draw_track(
	std::uint8_t choice,
	present::GraphicsTextStyle style
) noexcept
{
	if(choice >= prepared_->choices.size() || !prepared_->choices[choice]) {
		return false;
	}
	return present::graphics_text_put_8x16(
		raster_,
		{16, 40 + (static_cast<std::int32_t>(choice) * 16)},
		*prepared_->choices[choice],
		style
	);
}

bool ClassicOpMusicRoomScene::draw_tracklist() noexcept
{
	for(std::uint8_t choice = 0U; choice < classic_music_room_choice_count;
		++choice) {
		if(!draw_track_both(
			choice,
			{
				.color = static_cast<present::Index4>(
					(choice == selection_) ? 15U : 3U
				),
				.weight = present::GraphicsTextWeight::bold,
			}
		)) {
			return false;
		}
	}
	return true;
}

bool ClassicOpMusicRoomScene::draw_track_both(
	std::uint8_t choice,
	present::GraphicsTextStyle style
) noexcept
{
	if(!raster_.set_access_page(other_page(music_page_accessed_)) ||
		!draw_track(choice, style) ||
		!raster_.set_access_page(music_page_accessed_) ||
		!draw_track(choice, style)) {
		return false;
	}
	return true;
}

bool ClassicOpMusicRoomScene::restore_comment_background() noexcept
{
	const auto &pixels = prepared_->background.image.indices;
	const auto restore = [&](present::Rect rect) noexcept {
		const auto offset = (static_cast<std::size_t>(rect.y) *
			present::graphics_width) + static_cast<std::size_t>(rect.x);
		return raster_.blit(
			{
				std::span<const present::Index4>(pixels).subspan(offset),
				rect.width,
				rect.height,
				present::graphics_width,
			},
			{rect.x, rect.y}
		);
	};
	return restore(comment_title_rect) && restore(comment_body_rect);
}

bool ClassicOpMusicRoomScene::put_comment_text(std::uint8_t track) noexcept
{
	if(track >= prepared_->comments.size()) {
		return false;
	}
	for(std::size_t line = 0U; line < prepared_->comments[track].size(); ++line) {
		if(!prepared_->comments[track][line]) {
			return false;
		}
		const auto top = (line == 0U) ? 64 :
			(80 + (static_cast<std::int32_t>(line - 1U) * 16));
		if(!present::graphics_text_put_8x16(
			raster_,
			{304, top},
			*prepared_->comments[track][line],
			{
				.color = static_cast<present::Index4>(
					(line == 0U) ? 15U : 13U
				),
				.weight = present::GraphicsTextWeight::heavy,
			}
		)) {
			return false;
		}
	}
	return true;
}

bool ClassicOpMusicRoomScene::draw_comment(std::uint8_t track) noexcept
{
	return restore_blue_plane() && restore_comment_background() &&
		put_comment_text(track);
}

bool ClassicOpMusicRoomScene::snapshot_blue_plane() noexcept
{
	for(std::uint16_t y = 0U; y < present::graphics_height; ++y) {
		for(std::uint16_t byte_x = 0U; byte_x < plane_stride; ++byte_x) {
			const auto source = raster_.planar_byte(
				raster_.access_page(),
				present::PlanarPlane::blue,
				byte_x,
				y
			);
			if(!source) {
				return false;
			}
			no_polygon_blue_[(static_cast<std::size_t>(y) * plane_stride) +
				byte_x] = *source;
		}
	}
	return true;
}

bool ClassicOpMusicRoomScene::snapshot_comment_blue() noexcept
{
	const auto snapshot = [&](present::Rect rect) noexcept {
		const auto first_byte = static_cast<std::uint16_t>(rect.x / 8);
		const auto byte_width = static_cast<std::uint16_t>(rect.width / 8U);
		for(std::uint16_t row = 0U; row < rect.height; ++row) {
			const auto y = static_cast<std::uint16_t>(rect.y + row);
			for(std::uint16_t byte = 0U; byte < byte_width; ++byte) {
				const auto source = raster_.planar_byte(
					raster_.access_page(),
					present::PlanarPlane::blue,
					static_cast<std::uint16_t>(first_byte + byte),
					y
				);
				if(!source) {
					return false;
				}
				no_polygon_blue_[
					(static_cast<std::size_t>(y) * plane_stride) +
					first_byte + byte
				] = *source;
			}
		}
		return true;
	};
	return snapshot(comment_title_rect) && snapshot(comment_body_rect);
}

bool ClassicOpMusicRoomScene::restore_blue_plane() noexcept
{
	present::PlanarSource source{};
	source.planes = {
		no_polygon_blue_, zero_plane_, zero_plane_, zero_plane_,
	};
	source.width = present::graphics_width;
	source.height = present::graphics_height;
	source.stride = plane_stride;
	source.row_order = present::PackedRowOrder::top_to_bottom;
	return raster_.clear_plane(present::PlanarPlane::blue) &&
		raster_.blit_planar(
			source,
			{0, 0},
			present::PlanarWriteMode::or_planes
		);
}

void ClassicOpMusicRoomScene::initialize_polygon(
	ClassicOpMusicPolygon &polygon
) noexcept
{
	if(polygon.velocity_x == 0) {
		polygon.velocity_x = 1;
	}
	polygon.velocity_y_q12_4 = static_cast<std::int16_t>(
		32U + ((irand(carry_.random_seed) & 3U) << 4U)
	);
	polygon.angle = static_cast<std::uint8_t>(irand(carry_.random_seed));
	polygon.angle_speed = static_cast<std::uint8_t>(
		4U - (irand(carry_.random_seed) & 7U)
	);
	if(polygon.angle_speed == 0U) {
		polygon.angle_speed = 4U;
	}
}

bool ClassicOpMusicRoomScene::render_polygons() noexcept
{
	if(!carry_.polygons_initialized) {
		for(auto &polygon : carry_.polygons) {
			polygon.center_x = static_cast<std::int16_t>(
				irand(carry_.random_seed) % present::graphics_width
			);
			polygon.center_y_q12_4 = static_cast<std::int16_t>(
				irand(carry_.random_seed) %
				(present::graphics_height << 4U)
			);
			polygon.velocity_x = static_cast<std::int16_t>(
				4 - static_cast<std::int16_t>(irand(carry_.random_seed) & 7U)
			);
			initialize_polygon(polygon);
		}
		carry_.polygons_initialized = true;
	}

	for(std::size_t index = 0U; index < carry_.polygons.size(); ++index) {
		auto &polygon = carry_.polygons[index];
		const auto center_y = arithmetic_shift_right(
			polygon.center_y_q12_4,
			4U
		);
		const auto radius = static_cast<std::int16_t>(
			64U + ((index & 3U) * 16U)
		);
		const auto point_count = (index / 4U) + 3U;
		std::array<present::GrcgPoint, 6U> points{};
		for(std::size_t point = 0U; point < point_count; ++point) {
			const auto angle = static_cast<std::uint8_t>(
				((point << 8U) / point_count) + polygon.angle
			);
			const auto x_delta = arithmetic_shift_right(
				static_cast<std::int32_t>(radius) * exact::cosine8(angle),
				8U
			);
			const auto y_delta = arithmetic_shift_right(
				static_cast<std::int32_t>(radius) * exact::sine8(angle),
				8U
			);
			points[point] = {
				add_word(polygon.center_x, x_delta),
				add_word(center_y, y_delta),
			};
		}

		polygon.center_x = add_word(
			polygon.center_x,
			polygon.velocity_x
		);
		polygon.center_y_q12_4 = add_word(
			polygon.center_y_q12_4,
			polygon.velocity_y_q12_4
		);
		polygon.angle = static_cast<std::uint8_t>(
			polygon.angle + polygon.angle_speed
		);
		if(polygon.center_x <= 0 || polygon.center_x >= 639) {
			polygon.velocity_x = wrap_word(-polygon.velocity_x);
		}
		if(polygon.center_y_q12_4 >= 8000) {
			polygon.center_x = static_cast<std::int16_t>(
				irand(carry_.random_seed) % present::graphics_width
			);
			polygon.center_y_q12_4 = -1600;
			polygon.velocity_x = static_cast<std::int16_t>(
				8 - static_cast<std::int16_t>(irand(carry_.random_seed) & 15U)
			);
			initialize_polygon(polygon);
		}
		if(!present::grcg_polygon_c_blue(
			raster_,
			std::span<const present::GrcgPoint>(points).first(point_count),
			full_clip
		)) {
			return false;
		}
	}
	return true;
}

bool ClassicOpMusicRoomScene::flip_page() noexcept
{
	if(!raster_.set_shown_page(music_page_accessed_)) {
		return false;
	}
	music_page_accessed_ = other_page(music_page_accessed_);
	return raster_.set_access_page(music_page_accessed_);
}

bool ClassicOpMusicRoomScene::render_and_flip() noexcept
{
	return restore_blue_plane() && render_polygons() && flip_page();
}

bool ClassicOpMusicRoomScene::finish() noexcept
{
	if(!raster_.set_shown_page(present::Page::zero) ||
		!raster_.set_access_page(present::Page::zero) ||
		!raster_.clear(full_screen, 0U)) {
		return false;
	}
	completion_ = ClassicOpMusicRoomCompletion(
		std::move(resident_),
		std::move(carry_),
		std::move(raster_),
		std::move(tram_)
	);
	phase_ = ClassicOpMusicRoomPhase::ready;
	return true;
}

bool ClassicOpMusicRoomScene::push_audio_command(
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

ClassicOpMusicRoomTick ClassicOpMusicRoomScene::tick(
	ClassicOpMusicRoomBoundary boundary,
	bool surface_changed,
	bool input_sampled,
	bool page_flipped,
	bool selection_changed,
	bool carry_changed
) const noexcept
{
	return {
		.phase = phase_,
		.boundary = boundary,
		.surface_changed = surface_changed,
		.input_sampled = input_sampled,
		.page_flipped = page_flipped,
		.selection_changed = selection_changed,
		.carry_changed = carry_changed,
		.ready = phase_ == ClassicOpMusicRoomPhase::ready,
	};
}

void ClassicOpMusicRoomScene::fail() noexcept
{
	phase_ = ClassicOpMusicRoomPhase::failed;
	completion_.reset();
}

std::expected<ClassicOpMusicRoomTick, ClassicOpMusicRoomError>
ClassicOpMusicRoomScene::advance(const ClassicOpMusicRoomInput &input)
{
	if(audio_count_ != 0U) {
		return std::unexpected(music_error(
			ClassicOpMusicRoomErrorCode::audio_command_pending
		));
	}
	switch(phase_) {
	case ClassicOpMusicRoomPhase::held_release_poll:
		if(input.controls.any()) {
			if(!render_and_flip()) {
				fail();
				return std::unexpected(music_error(
					ClassicOpMusicRoomErrorCode::draw_failed
				));
			}
			return tick(
				ClassicOpMusicRoomBoundary::vsync,
				true, true, true, false, true
			);
		}
		phase_ = ClassicOpMusicRoomPhase::controls_poll;
		return tick(ClassicOpMusicRoomBoundary::input_poll, false, true);

	case ClassicOpMusicRoomPhase::controls_poll: {
		bool selection_changed = false;
		if(input.controls.up) {
			if(!draw_track_both(
				selection_,
				{.color = 3U, .weight = present::GraphicsTextWeight::bold}
			)) {
				fail();
				return std::unexpected(music_error(
					ClassicOpMusicRoomErrorCode::draw_failed
				));
			}
			selection_ = (selection_ > 0U) ?
				static_cast<std::uint8_t>(selection_ - 1U) : choice_quit;
			if(selection_ == choice_empty) {
				--selection_;
			}
			if(!draw_track_both(
				selection_,
				{.color = 15U, .weight = present::GraphicsTextWeight::bold}
			)) {
				fail();
				return std::unexpected(music_error(
					ClassicOpMusicRoomErrorCode::draw_failed
				));
			}
			selection_changed = true;
		}
		if(input.controls.down) {
			if(!draw_track_both(
				selection_,
				{.color = 3U, .weight = present::GraphicsTextWeight::bold}
			)) {
				fail();
				return std::unexpected(music_error(
					ClassicOpMusicRoomErrorCode::draw_failed
				));
			}
			selection_ = (selection_ < choice_quit) ?
				static_cast<std::uint8_t>(selection_ + 1U) : 0U;
			if(selection_ == choice_empty) {
				++selection_;
			}
			if(!draw_track_both(
				selection_,
				{.color = 15U, .weight = present::GraphicsTextWeight::bold}
			)) {
				fail();
				return std::unexpected(music_error(
					ClassicOpMusicRoomErrorCode::draw_failed
				));
			}
			selection_changed = true;
		}
		if(input.controls.ok || input.controls.shot) {
			if(selection_ != choice_quit) {
				if(!push_audio_command({
					.type = ClassicAudioCommandType::stop_song,
				}) || !push_audio_command({
					.type = ClassicAudioCommandType::load_song,
					.cue = ClassicMusicCue::music_room_track,
					.parameter = selection_,
				}) || !push_audio_command({
					.type = ClassicAudioCommandType::play_song,
					.cue = ClassicMusicCue::music_room_track,
					.parameter = selection_,
				}) || !draw_comment(selection_) ||
					!snapshot_comment_blue()) {
					fail();
					return std::unexpected(music_error(
						ClassicOpMusicRoomErrorCode::draw_failed
					));
				}
				carry_.track_playing = selection_;
				exit_after_selected_frame_ = input.controls.cancel;
				phase_ = ClassicOpMusicRoomPhase::selected_track_frame;
				return tick(
					ClassicOpMusicRoomBoundary::input_poll,
					true, true, false, selection_changed, true
				);
			}
			phase_ = ClassicOpMusicRoomPhase::exit_release_poll;
			return tick(
				ClassicOpMusicRoomBoundary::input_poll,
				false, true, false, selection_changed
			);
		}
		if(input.controls.cancel) {
			phase_ = ClassicOpMusicRoomPhase::exit_release_poll;
			return tick(
				ClassicOpMusicRoomBoundary::input_poll,
				false, true, false, selection_changed
			);
		}
		if(!input.controls.any()) {
			if(!render_and_flip()) {
				fail();
				return std::unexpected(music_error(
					ClassicOpMusicRoomErrorCode::draw_failed
				));
			}
			return tick(
				ClassicOpMusicRoomBoundary::vsync,
				true, true, true, selection_changed, true
			);
		}
		phase_ = ClassicOpMusicRoomPhase::held_release_poll;
		return tick(
			ClassicOpMusicRoomBoundary::input_poll,
			selection_changed, true, false, selection_changed
		);
	}

	case ClassicOpMusicRoomPhase::selected_track_frame:
		if(!render_and_flip() || !draw_comment(carry_.track_playing) ||
			!snapshot_comment_blue()) {
			fail();
			return std::unexpected(music_error(
				ClassicOpMusicRoomErrorCode::draw_failed
			));
		}
		phase_ = exit_after_selected_frame_ ?
			ClassicOpMusicRoomPhase::exit_release_poll :
			ClassicOpMusicRoomPhase::held_release_poll;
		exit_after_selected_frame_ = false;
		return tick(
			ClassicOpMusicRoomBoundary::vsync,
			true, false, true, false, true
		);

	case ClassicOpMusicRoomPhase::exit_release_poll:
		if(input.controls.any()) {
			if(!render_and_flip()) {
				fail();
				return std::unexpected(music_error(
					ClassicOpMusicRoomErrorCode::draw_failed
				));
			}
			return tick(
				ClassicOpMusicRoomBoundary::vsync,
				true, true, true, false, true
			);
		}
		if(!finish()) {
			fail();
			return std::unexpected(music_error(
				ClassicOpMusicRoomErrorCode::draw_failed
			));
		}
		return tick(
			ClassicOpMusicRoomBoundary::input_poll,
			true, true, false
		);

	case ClassicOpMusicRoomPhase::ready:
	case ClassicOpMusicRoomPhase::consumed:
	case ClassicOpMusicRoomPhase::failed:
		return std::unexpected(music_error(
			ClassicOpMusicRoomErrorCode::invalid_state
		));
	}
	return std::unexpected(music_error(
		ClassicOpMusicRoomErrorCode::invalid_state
	));
}

std::optional<ClassicAudioCommand>
ClassicOpMusicRoomScene::take_audio_command() noexcept
{
	if(audio_count_ == 0U) {
		return std::nullopt;
	}
	auto command = audio_commands_[audio_head_];
	audio_head_ = (audio_head_ + 1U) % audio_commands_.size();
	--audio_count_;
	return command;
}

std::optional<ClassicOpMusicRoomCompletion>
ClassicOpMusicRoomScene::take_completion() noexcept
{
	if(phase_ != ClassicOpMusicRoomPhase::ready || !completion_) {
		return std::nullopt;
	}
	phase_ = ClassicOpMusicRoomPhase::consumed;
	auto completion = std::move(completion_);
	completion_.reset();
	return completion;
}

ClassicOpMusicRoomPhase ClassicOpMusicRoomScene::phase() const noexcept
{
	return phase_;
}

std::uint8_t ClassicOpMusicRoomScene::selection() const noexcept
{
	return selection_;
}

const ClassicOpMusicRoomCarry &ClassicOpMusicRoomScene::carry() const noexcept
{
	return carry_;
}

const present::Raster &ClassicOpMusicRoomScene::raster() const noexcept
{
	return raster_;
}

const present::Tram &ClassicOpMusicRoomScene::tram() const noexcept
{
	return tram_;
}

std::expected<
	std::unique_ptr<ClassicOpMusicRoomScene>,
	ClassicOpMusicRoomError
> ClassicOpMusicRoomSceneTestAccess::create_with_predecoded_assets(
	GameInit resident,
	ClassicOpMusicRoomCarry carry,
	assets::PiImage background,
	const OpMusicRoomTextImport &labels,
	std::span<const std::byte> comments,
	const present::AnkGlyphProvider &font,
	present::Raster raster,
	present::Tram tram
)
{
	return ClassicOpMusicRoomScene::create_predecoded(
		std::move(resident),
		std::move(carry),
		std::move(background),
		labels,
		comments,
		font,
		std::move(raster),
		std::move(tram)
	);
}

} // namespace th03::app
