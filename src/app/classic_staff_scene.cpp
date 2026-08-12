#include "classic_staff_scene_internal.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>

#include "th03/assets/decode.hpp"
#include "th03/present/cels.hpp"

namespace th03::app {
namespace {

constexpr std::uint16_t source_phase_frame_threshold = 0x100U;
constexpr std::uint16_t source_music_frame_floor = 0xC0U;
constexpr std::uint8_t source_fade_steps = 4U;

[[nodiscard]] ClassicStaffError staff_error(
	ClassicStaffErrorCode code,
	std::optional<MainResourceKey> resource = std::nullopt
) noexcept
{
	return {.code = code, .resource = resource};
}

[[nodiscard]] ClassicStaffError decode_error(
	MainResourceKey resource,
	assets::AssetError error
)
{
	auto result = staff_error(
		ClassicStaffErrorCode::asset_decode_failed, resource
	);
	result.asset_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicStaffError text_error(
	present::GraphicsTextError error
) noexcept
{
	auto result = staff_error(ClassicStaffErrorCode::font_prepare_failed);
	result.text_error.emplace(error);
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

[[nodiscard]] std::expected<void, ClassicStaffError> validate_resource_set(
	std::span<const LoadedMainResource> resources,
	std::span<const MainResourceKey> expected,
	bool authenticate_resource_bytes
)
{
	if(resources.size() != expected.size()) {
		return std::unexpected(staff_error(
			ClassicStaffErrorCode::resource_set_mismatch
		));
	}
	for(std::size_t index = 0U; index < expected.size(); ++index) {
		const auto key = expected[index];
		const auto &resource = resources[index];
		if(resource.spec.key != key) {
			return std::unexpected(staff_error(
				ClassicStaffErrorCode::resource_set_mismatch, key
			));
		}
		if(!resource.bytes) {
			return std::unexpected(staff_error(
				ClassicStaffErrorCode::resource_bytes_missing, key
			));
		}
		const auto canonical = main_resource_spec(key);
		if(!canonical || !same_resource_spec(resource.spec, *canonical)) {
			return std::unexpected(staff_error(
				ClassicStaffErrorCode::resource_set_mismatch, key
			));
		}
		if(!resource.authenticated_override &&
			(resource.bytes->size() != canonical->expected_bytes ||
			(authenticate_resource_bytes && main_resource_fnv1a64(
				*resource.bytes
			) != canonical->expected_fnv1a64))) {
			return std::unexpected(staff_error(
				ClassicStaffErrorCode::resource_integrity_failed, key
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
[[nodiscard]] std::expected<Value, ClassicStaffError> decode_asset(
	std::span<const LoadedMainResource> resources,
	MainResourceKey key,
	Decoder decoder
)
{
	const auto *resource = find_resource(resources, key);
	if(!resource || !resource->bytes) {
		return std::unexpected(staff_error(
			ClassicStaffErrorCode::resource_set_mismatch, key
		));
	}
	auto decoded = decoder(std::span<const std::byte>{*resource->bytes});
	if(!decoded) {
		return std::unexpected(decode_error(key, std::move(decoded.error())));
	}
	return std::move(*decoded);
}

[[nodiscard]] bool valid_cel(
	const assets::PlanarCel4 &cel,
	std::uint16_t width,
	std::uint16_t height,
	bool require_alpha
) noexcept
{
	const auto plane_size = (static_cast<std::size_t>(width) / 8U) * height;
	if(width == 0U || height == 0U || (width % 32U) != 0U ||
		cel.pixel_width != width || cel.pixel_height != height ||
		cel.bitplane_size != plane_size ||
		cel.offset_at_bottom_left != static_cast<std::int16_t>(
			80U * (height - 1U)
		) || cel.vram_dword_width != (width / 32U) ||
		cel.row_order != assets::PlanarRowOrder::bottom_to_top ||
		cel.bit_order != assets::PlanarBitOrder::most_significant_bit_left ||
		cel.alpha.has_value() != require_alpha) {
		return false;
	}
	for(const auto &plane : cel.color_planes) {
		if(plane.size() != plane_size) {
			return false;
		}
	}
	return !require_alpha || cel.alpha->size() == plane_size;
}

[[nodiscard]] constexpr std::size_t page_index(present::Page page) noexcept
{
	return page == present::Page::zero ? 0U : 1U;
}

[[nodiscard]] constexpr present::Page other_page(present::Page page) noexcept
{
	return page == present::Page::zero ? present::Page::one : present::Page::zero;
}

} // namespace

enum class ClassicStaffScene::DriverPhase : std::uint8_t {
	queue_fade,
	blackout_align,
	blackout_wait,
	wait_volume,
	queue_stop,
	queue_load,
	initialize,
	initial_wait,
	initial_flakes,
	sequence_in,
	sequence_out,
	sequence_blank,
	cleanup,
	dissolve,
	verdict_install,
	verdict_loop,
};

enum class ClassicStaffScene::SequenceKind : std::uint8_t {
	slide,
	setup_y,
	overlay,
};

struct ClassicStaffScene::Sequence final {
	SequenceKind kind = SequenceKind::slide;
	std::uint8_t slot = 0U;
	std::uint16_t measure_in = 0U;
	std::uint16_t measure_out = 0U;
	std::uint8_t speed = 1U;
	std::uint16_t draw_frames = 0xA1U;
	std::int16_t y_stop = 200;
	std::int16_t y_start_offset = 0;
	std::uint8_t auxiliary_slot = 0U;
};

constexpr std::array<ClassicStaffScene::Sequence, 9U> staff_sequences{{
	{ClassicStaffScene::SequenceKind::slide, 0U, 8U, 10U, 2U, 0x41U, 200, 0, 0U},
	{ClassicStaffScene::SequenceKind::slide, 1U, 16U, 20U, 1U, 0xA1U, 200, 0, 0U},
	{ClassicStaffScene::SequenceKind::setup_y, 2U, 22U, 24U, 1U, 0xA1U, 168, 32, 0U},
	{ClassicStaffScene::SequenceKind::overlay, 3U, 32U, 34U, 1U, 0xA1U, 216, -16, 7U},
	{ClassicStaffScene::SequenceKind::slide, 4U, 36U, 38U, 1U, 0xA1U, 200, 0, 0U},
	{ClassicStaffScene::SequenceKind::slide, 11U, 42U, 44U, 1U, 0xA1U, 200, 0, 0U},
	{ClassicStaffScene::SequenceKind::slide, 5U, 48U, 50U, 1U, 0xA1U, 200, 0, 0U},
	{ClassicStaffScene::SequenceKind::slide, 6U, 54U, 56U, 1U, 0xA1U, 200, 0, 0U},
	{ClassicStaffScene::SequenceKind::slide, 10U, 60U, 62U, 1U, 0xA1U, 200, 0, 0U},
}};

ClassicStaffCompletion::ClassicStaffCompletion(
	present::Raster raster
) noexcept :
	raster_(std::move(raster))
{}

const present::Raster &ClassicStaffCompletion::raster() const noexcept
{
	return raster_;
}

ClassicStaffScene::ClassicStaffScene(
	std::unique_ptr<Prepared> &&prepared,
	GameInit resident,
	present::StaffFlakeTrigTables trig,
	present::Raster raster,
	bool fast_forward_unlocked
) noexcept :
	prepared_(std::move(prepared)),
	resident_(std::move(resident)),
	trig_(trig),
	raster_(std::move(raster)),
	driver_phase_(DriverPhase::queue_fade),
	fast_forward_unlocked_(fast_forward_unlocked)
{}

ClassicStaffScene::~ClassicStaffScene() = default;

std::expected<std::unique_ptr<ClassicStaffScene>, ClassicStaffError>
ClassicStaffScene::create(
	const GameInit &resident,
	std::span<const LoadedMainResource> resources,
	const MainlStageTextAssets &stage_text,
	const MainlStaffCompiledAssets &compiled,
	const present::AnkGlyphProvider &font,
	present::StaffFlakeTrigTables trig,
	present::Raster initial_raster,
	bool fast_forward_unlocked
)
{
	return create_impl(
		resident, resources, stage_text, compiled, font, trig,
		std::move(initial_raster), true, fast_forward_unlocked
	);
}

std::expected<std::unique_ptr<ClassicStaffScene>, ClassicStaffError>
ClassicStaffScene::create_impl(
	const GameInit &resident,
	std::span<const LoadedMainResource> resources,
	const MainlStageTextAssets &stage_text,
	const MainlStaffCompiledAssets &compiled,
	const present::AnkGlyphProvider &font,
	present::StaffFlakeTrigTables trig,
	present::Raster initial_raster,
	bool authenticate_resource_bytes,
	bool fast_forward_unlocked
)
{
	const auto expected = mainl_staff_resource_keys();
	if(const auto valid = validate_resource_set(
		resources, expected, authenticate_resource_bytes
	); !valid) {
		return std::unexpected(valid.error());
	}
	auto palette = decode_asset<assets::Palette16Rgb4>(
		resources,
		{.role = MainResourceRole::staff_background_palette},
		assets::decode_rgb48
	);
	if(!palette) {
		return std::unexpected(palette.error());
	}

	std::array<assets::PlanarCel4, 12U> cels{};
	for(std::size_t index = 0U; index < cels.size(); ++index) {
		const auto selector = static_cast<MainStaffCdg>(index + 1U);
		const MainResourceKey key{
			.role = MainResourceRole::staff_cdg,
			.staff_cdg = selector,
		};
		const bool preserve_alpha = index == 2U || index == 3U;
		auto decoded = decode_asset<assets::PlanarCel4>(
			resources,
			key,
			[preserve_alpha](std::span<const std::byte> bytes) {
				return assets::decode_cdg_cel(
					bytes,
					0U,
					preserve_alpha ? assets::CdgAlphaPolicy::preserve :
						assets::CdgAlphaPolicy::discard
				);
			}
		);
		if(!decoded) {
			return std::unexpected(decoded.error());
		}
		cels[index] = std::move(*decoded);
	}
	return create_predecoded(
		resident, std::move(cels), *palette, stage_text, compiled, font, trig,
		std::move(initial_raster), fast_forward_unlocked
	);
}

std::expected<std::unique_ptr<ClassicStaffScene>, ClassicStaffError>
ClassicStaffScene::create_predecoded(
	const GameInit &resident,
	std::array<assets::PlanarCel4, 12U> cels,
	assets::Palette16Rgb4 palette,
	const MainlStageTextAssets &stage_text,
	const MainlStaffCompiledAssets &compiled,
	const present::AnkGlyphProvider &font,
	present::StaffFlakeTrigTables trig,
	present::Raster initial_raster,
	bool fast_forward_unlocked
)
{
	constexpr std::array<std::array<std::uint16_t, 2U>, 12U> geometries{{
		{{256U, 48U}}, {{384U, 26U}}, {{416U, 32U}}, {{320U, 40U}},
		{{256U, 32U}}, {{512U, 32U}}, {{448U, 32U}}, {{256U, 230U}},
		{{192U, 36U}}, {{128U, 138U}}, {{288U, 24U}}, {{224U, 36U}},
	}};
	const auto player = main_player_selection_from_paletted(
		resident.paletted_character[0U]
	);
	if(!player || resident.game_mode != game_mode::story ||
		resident.remaining_credits > 3U || resident.rank >= 4U ||
		stage_text.characters.size() != 9U ||
		trig.sine_q8.size() != 256U || trig.cosine_q8.size() != 256U ||
		initial_raster.raster_mode() != present::RasterMode::lines_400) {
		return std::unexpected(staff_error(
			ClassicStaffErrorCode::invalid_context
		));
	}
	for(const auto digit : resident.last_score[0U]) {
		if(digit >= 10U) {
			return std::unexpected(staff_error(
				ClassicStaffErrorCode::invalid_context
			));
		}
	}
	for(std::size_t index = 0U; index < cels.size(); ++index) {
		if(!valid_cel(
			cels[index], geometries[index][0U], geometries[index][1U],
			index == 2U || index == 3U
		)) {
			return std::unexpected(staff_error(
				ClassicStaffErrorCode::invalid_asset_geometry,
				MainResourceKey{
					.role = MainResourceRole::staff_cdg,
					.staff_cdg = static_cast<MainStaffCdg>(index + 1U),
				}
			));
		}
	}

	auto prepared = std::make_unique<Prepared>();
	prepared->palette = palette;
	prepared->cels = std::move(cels);
	prepared->flake_masks = compiled.flake_masks;
	prepared->score = resident.last_score[0U];
	prepared->continues_used = static_cast<std::uint8_t>(
		3U - resident.remaining_credits
	);
	prepared->skill = resident.skill;
	const auto source_score_7 = prepared->score[6U];
	if(source_score_7 == 3U) {
		prepared->skill = static_cast<std::uint8_t>(
			prepared->skill + (prepared->score[5U] / 2U) + 2U
		);
		prepared->skill = static_cast<std::uint8_t>(
			prepared->skill + (prepared->score[5U] / 2U) + 7U
		);
	} else if(source_score_7 == 4U) {
		prepared->skill = static_cast<std::uint8_t>(
			prepared->skill + (prepared->score[5U] / 2U) + 7U
		);
	}
	if(source_score_7 >= 5U) {
		prepared->skill = static_cast<std::uint8_t>(prepared->skill + 15U);
	}
	if(prepared->score[7U] != 0U) {
		prepared->skill = 100U;
	}
	if(prepared->skill > 100U) {
		prepared->skill = 100U;
	}

	const auto character_index = static_cast<std::size_t>(
		static_cast<std::uint8_t>(player->character) -
		static_cast<std::uint8_t>(MainCharacter::reimu)
	);
	auto playchar = present::prepare_graphics_text_8x16(
		stage_text.characters[character_index].name, font
	);
	if(!playchar) {
		return std::unexpected(text_error(playchar.error()));
	}
	prepared->playchar.emplace(std::move(*playchar));
	auto rank = present::prepare_graphics_text_8x16(
		compiled.ranks[resident.rank], font
	);
	if(!rank) {
		return std::unexpected(text_error(rank.error()));
	}
	prepared->rank.emplace(std::move(*rank));
	for(std::size_t index = 0U; index < prepared->numbers.size(); ++index) {
		auto number = present::prepare_graphics_text_8x16(
			compiled.numbers[index], font
		);
		if(!number) {
			return std::unexpected(text_error(number.error()));
		}
		prepared->numbers[index].emplace(std::move(*number));
	}
	auto point = present::prepare_graphics_text_8x16(compiled.point, font);
	if(!point) {
		return std::unexpected(text_error(point.error()));
	}
	prepared->point.emplace(std::move(*point));

	return std::unique_ptr<ClassicStaffScene>(new ClassicStaffScene(
		std::move(prepared), resident, trig, std::move(initial_raster),
		fast_forward_unlocked
	));
}

bool ClassicStaffScene::push_audio_command(
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

bool ClassicStaffScene::initialize_visuals() noexcept
{
	raster_.set_palette_tone(0);
	raster_.apply_pending_palette();
	for(std::size_t index = 0U; index < prepared_->palette.colors.size(); ++index) {
		if(!raster_.set_pending_palette_entry(
			static_cast<std::uint8_t>(index), prepared_->palette.colors[index]
		)) {
			return false;
		}
	}
	raster_.apply_pending_palette();
	for(const auto page : {present::Page::one, present::Page::zero}) {
		if(!raster_.set_access_page(page) || !raster_.clear(
			{0, 0, present::graphics_width, present::graphics_height}, 8U
		) || !raster_.clear({8, 8, 624U, 384U}, 0U)) {
			return false;
		}
	}
	if(!raster_.set_shown_page(present::Page::one)) {
		return false;
	}
	flakes_.emplace(std::bit_cast<std::uint32_t>(resident_.resident_rand));
	raster_.set_palette_tone(100);
	raster_.apply_pending_palette();
	return push_audio_command({
		.type = ClassicAudioCommandType::play_song,
		.cue = ClassicMusicCue::staff,
	});
}

bool ClassicStaffScene::initialize_sequence() noexcept
{
	if(!flakes_ || sequence_index_ >= staff_sequences.size()) {
		return false;
	}
	if(sequence_index_ == 1U) {
		// Source clears the adaptive reset latch immediately after STF1's
		// complete in/out cycle, even if no overrun was ever observed.
		flakes_->reset_pending = false;
	}
	const auto &sequence = staff_sequences[sequence_index_];
	const auto back = flakes_->back_page;
	if(sequence.speed == 2U) {
		center_y_[page_index(back)] = 264;
		center_y_[page_index(other_page(back))] = 263;
	} else {
		center_y_.fill(static_cast<std::int16_t>(
			280 - sequence.y_start_offset
		));
	}
	present::staff_flakes_restart_phase(*flakes_);
	driver_phase_ = DriverPhase::sequence_in;
	return true;
}

bool ClassicStaffScene::tick_flakes(bool overrun) noexcept
{
	return flakes_ && present::staff_flakes_tick(
		raster_, *flakes_, prepared_->masks(), trig_, overrun
	);
}

bool ClassicStaffScene::phase_done(
	ClassicAudioStatus audio,
	std::uint16_t measure,
	bool fast_forward_held
) const noexcept
{
	if(!flakes_) {
		return false;
	}
	const auto frame = static_cast<std::uint16_t>(flakes_->frame);
	// Replay Patch substitutes the fixed 0x100 frame threshold while physical
	// Z is held. ED.M continues at normal speed, so measure is deliberately
	// ignored for accelerated visual phases.
	if(fast_forward_unlocked_ && fast_forward_held) {
		return frame > source_phase_frame_threshold;
	}
	if(!audio.bgm_active) {
		return frame > source_phase_frame_threshold;
	}
	return audio.measure >= measure && frame > source_music_frame_floor;
}

bool ClassicStaffScene::slide_up() noexcept
{
	const auto &sequence = staff_sequences[sequence_index_];
	present::staffroll_blue_plane_clear(raster_);
	if(flakes_->frame > static_cast<std::int16_t>(sequence.draw_frames)) {
		return true;
	}
	const auto page = page_index(flakes_->back_page);
	auto &center = center_y_[page];
	if(!present::cdg_unput_for_upwards_motion_e_8(
		raster_, 320, center, prepared_->cels[sequence.slot]
	)) {
		return false;
	}
	if(center > sequence.y_stop) {
		center = static_cast<std::int16_t>(center - sequence.speed);
		if(center < sequence.y_stop) {
			center = sequence.y_stop;
		}
	}
	const auto divisor = static_cast<std::int16_t>(sequence.draw_frames / 8U);
	auto strength = static_cast<std::int16_t>(
		7 - (flakes_->frame / divisor)
	);
	if(strength < 0) {
		strength = 0;
	}
	const bool use_alpha = flakes_->cdg_put_alpha &&
		sequence.auxiliary_slot != 0U;
	if(use_alpha && !present::cdg_put_dissolve_e_8(
		raster_, 504, 200, prepared_->cels[sequence.auxiliary_slot],
		static_cast<std::uint8_t>(strength), present::CdgAlphaMode::no_alpha
	)) {
		return false;
	}
	return present::cdg_put_dissolve_e_8(
		raster_, 320, center, prepared_->cels[sequence.slot],
		static_cast<std::uint8_t>(strength),
		use_alpha ? present::CdgAlphaMode::alpha :
			present::CdgAlphaMode::no_alpha
	);
}

bool ClassicStaffScene::slide_out() noexcept
{
	const auto &sequence = staff_sequences[sequence_index_];
	present::staffroll_blue_plane_clear(raster_);
	if(flakes_->frame > 0xA1) {
		return true;
	}
	if(flakes_->frame >= 0xA0) {
		return clear_inner();
	}
	const auto page = page_index(flakes_->back_page);
	auto &center = center_y_[page];
	if(!present::cdg_unput_for_upwards_motion_e_8(
		raster_, 320, center, prepared_->cels[sequence.slot]
	)) {
		return false;
	}
	--center;
	auto strength = static_cast<std::int16_t>(flakes_->frame / 20);
	if(strength > 7) {
		strength = 7;
	}
	const bool use_alpha = flakes_->cdg_put_alpha &&
		sequence.auxiliary_slot != 0U;
	if(use_alpha && !present::cdg_put_dissolve_e_8(
		raster_, 504, 200, prepared_->cels[sequence.auxiliary_slot],
		static_cast<std::uint8_t>(strength), present::CdgAlphaMode::no_alpha
	)) {
		return false;
	}
	return present::cdg_put_dissolve_e_8(
		raster_, 320, center, prepared_->cels[sequence.slot],
		static_cast<std::uint8_t>(strength),
		use_alpha ? present::CdgAlphaMode::alpha :
			present::CdgAlphaMode::no_alpha
	);
}

bool ClassicStaffScene::overlay_up() noexcept
{
	if(!slide_up()) {
		return false;
	}
	return present::cdg_put_dissolve_e_8(
		raster_, 320, overlay_y_, prepared_->cels[2U], 0U,
		present::CdgAlphaMode::alpha
	);
}

bool ClassicStaffScene::overlay_out() noexcept
{
	if(flakes_->frame <= 0xA1 &&
		!present::cdg_unput_for_upwards_motion_e_8(
			raster_, 320, overlay_y_, prepared_->cels[2U]
		)) {
		return false;
	}
	if(!slide_out()) {
		return false;
	}
	if(flakes_->frame > 0xA1) {
		return true;
	}
	auto strength = static_cast<std::int16_t>((flakes_->frame - 1) / 20);
	if(strength > 7) {
		strength = 7;
	}
	if(flakes_->back_page == present::Page::zero) {
		--overlay_y_;
	}
	if(flakes_->frame < 0xA0) {
		return present::cdg_put_dissolve_e_8(
			raster_, 320, overlay_y_, prepared_->cels[2U],
			static_cast<std::uint8_t>(strength),
			present::CdgAlphaMode::alpha
		);
	}
	return clear_inner();
}

bool ClassicStaffScene::dissolve() noexcept
{
	present::staffroll_blue_plane_clear(raster_);
	if(flakes_->frame > 0xA0) {
		return true;
	}
	auto strength = static_cast<std::int16_t>(7 - (flakes_->frame / 20));
	if(strength < 0) {
		strength = 0;
	}
	return present::cdg_put_dissolve_e_8(
		raster_, 320, 128, prepared_->cels[8U],
		static_cast<std::uint8_t>(strength), present::CdgAlphaMode::no_alpha
	) && present::cdg_put_dissolve_e_8(
		raster_, 192, 240, prepared_->cels[9U],
		static_cast<std::uint8_t>(strength), present::CdgAlphaMode::no_alpha
	);
}

bool ClassicStaffScene::draw_verdict() noexcept
{
	const present::GraphicsTextStyle style{
		.color = 15U,
		.weight = present::GraphicsTextWeight::bold,
	};
	auto draw_page = [this, style]() -> bool {
		if(!present::graphics_text_put_8x16(
			raster_, {352, 174}, *prepared_->playchar, style
		) || !present::graphics_text_put_8x16(
			raster_, {360, 199}, *prepared_->rank, style
		)) {
			return false;
		}
		std::int32_t left = 408;
		bool digit_seen = false;
		for(std::int32_t digits_left = 8; digits_left > 0; --digits_left) {
			const auto digit = prepared_->score[
				static_cast<std::size_t>(digits_left - 1)
			];
			if(!digit_seen && digit != 0U) {
				left -= digits_left * 8;
				digit_seen = true;
			}
			if(digit_seen && (digit >= prepared_->numbers.size() ||
				!present::graphics_text_put_8x16(
					raster_, {left, 224}, *prepared_->numbers[digit], style
				))) {
				return false;
			}
			if(digit_seen) {
				left += 16;
			}
		}
		const auto continues = prepared_->continues_used;
		if(!present::graphics_text_put_8x16(
			raster_, {left, 224}, *prepared_->numbers[continues], style
		) || !present::graphics_text_put_8x16(
			raster_, {408, 248}, *prepared_->numbers[continues], style
		)) {
			return false;
		}

		const auto skill = prepared_->skill;
		left = 408;
		digit_seen = false;
		auto digit = static_cast<std::uint8_t>(skill / 100U);
		if(digit != 0U) {
			left -= 16;
			digit_seen = true;
			if(!present::graphics_text_put_8x16(
				raster_, {left, 291}, *prepared_->numbers[digit], style
			)) {
				return false;
			}
			left += 16;
		}
		digit = static_cast<std::uint8_t>((skill % 100U) / 10U);
		if(digit != 0U && !digit_seen) {
			digit_seen = true;
			left -= 8;
		}
		if(digit_seen) {
			if(!present::graphics_text_put_8x16(
				raster_, {left, 291}, *prepared_->numbers[digit], style
			)) {
				return false;
			}
			left += 16;
		}
		digit = static_cast<std::uint8_t>(skill % 10U);
		return present::graphics_text_put_8x16(
			raster_, {left, 291}, *prepared_->numbers[digit], style
		) && present::graphics_text_put_8x16(
			raster_, {left + 16, 291}, *prepared_->point, style
		);
	};

	const auto back = flakes_->back_page;
	return raster_.set_access_page(other_page(back)) && draw_page() &&
		raster_.set_access_page(back) && draw_page();
}

bool ClassicStaffScene::clear_inner() noexcept
{
	return raster_.clear({8, 8, 624U, 384U}, 0U);
}

ClassicStaffTick ClassicStaffScene::tick(
	ClassicStaffBoundary boundary,
	bool surface_changed,
	bool input_sampled
) const noexcept
{
	return {
		.phase = phase_,
		.boundary = boundary,
		.surface_changed = surface_changed,
		.input_sampled = input_sampled,
		.ready = phase_ == ClassicStaffPhase::ready,
	};
}

std::expected<ClassicStaffTick, ClassicStaffError>
ClassicStaffScene::advance(const ClassicStaffInput &input)
{
	const auto accelerated = input.fast_forward_held && fast_forward_step();
	const std::size_t step_count = accelerated ? 4U : 1U;
	std::optional<ClassicStaffTick> result{};
	for(std::size_t step = 0U; step < step_count; ++step) {
		auto advanced = advance_one(input);
		if(!advanced) {
			return std::unexpected(std::move(advanced.error()));
		}
		if(result) {
			advanced->surface_changed = advanced->surface_changed ||
				result->surface_changed;
			advanced->input_sampled = advanced->input_sampled ||
				result->input_sampled;
		}
		result = *advanced;
		if(advanced->ready ||
			advanced->boundary != ClassicStaffBoundary::vsync ||
			audio_count_ != 0U || !fast_forward_step()) {
			break;
		}
	}
	return *result;
}

bool ClassicStaffScene::fast_forward_step() const noexcept
{
	if(!fast_forward_unlocked_) {
		return false;
	}
	switch(driver_phase_) {
	case DriverPhase::initial_flakes:
	case DriverPhase::sequence_in:
	case DriverPhase::sequence_out:
	case DriverPhase::sequence_blank:
	case DriverPhase::cleanup:
	case DriverPhase::dissolve:
	case DriverPhase::verdict_loop:
		return true;
	default:
		return false;
	}
}

std::expected<ClassicStaffTick, ClassicStaffError>
ClassicStaffScene::advance_one(const ClassicStaffInput &input)
{
	if(phase_ == ClassicStaffPhase::ready ||
		phase_ == ClassicStaffPhase::consumed ||
		phase_ == ClassicStaffPhase::failed) {
		return std::unexpected(staff_error(
			ClassicStaffErrorCode::invalid_state
		));
	}
	if(audio_count_ != 0U) {
		return std::unexpected(staff_error(
			ClassicStaffErrorCode::audio_command_pending
		));
	}

	for(std::size_t immediate = 0U; immediate < 16U; ++immediate) {
		switch(driver_phase_) {
		case DriverPhase::queue_fade:
			if(!push_audio_command({
				.type = ClassicAudioCommandType::fade_song,
				.parameter = 16U,
			})) {
				break;
			}
			driver_phase_ = DriverPhase::blackout_align;
			return tick(ClassicStaffBoundary::immediate);

		case DriverPhase::blackout_align:
			raster_.set_palette_tone(100);
			fade_tone_ = 100;
			fade_wait_remaining_ = 0U;
			fade_tone_applied_ = false;
			driver_phase_ = DriverPhase::blackout_wait;
			return tick(ClassicStaffBoundary::vsync);

		case DriverPhase::blackout_wait:
			if(!fade_tone_applied_) {
				raster_.set_palette_tone(fade_tone_);
				raster_.apply_pending_palette();
				fade_tone_applied_ = true;
				fade_wait_remaining_ = source_fade_steps;
			}
			if(fade_wait_remaining_ != 0U) {
				--fade_wait_remaining_;
				return tick(ClassicStaffBoundary::vsync, true);
			}
			fade_tone_ = static_cast<std::int16_t>(fade_tone_ - 6);
			if(fade_tone_ > 0) {
				fade_tone_applied_ = false;
				continue;
			}
			raster_.set_palette_tone(0);
			raster_.apply_pending_palette();
			phase_ = ClassicStaffPhase::wait_volume;
			driver_phase_ = DriverPhase::wait_volume;
			continue;

		case DriverPhase::wait_volume:
			// The donor's snd_delay_until_volume() does not test snd_active,
			// leaving Music Off with no driver state that can satisfy this wait.
			if(input.audio.bgm_active && input.audio.volume != 255U) {
				return tick(ClassicStaffBoundary::audio_volume);
			}
			driver_phase_ = DriverPhase::queue_stop;
			continue;

		case DriverPhase::queue_stop:
			if(!push_audio_command({
				.type = ClassicAudioCommandType::stop_song,
			})) {
				break;
			}
			driver_phase_ = DriverPhase::queue_load;
			return tick(ClassicStaffBoundary::immediate);

		case DriverPhase::queue_load:
			if(!push_audio_command({
				.type = ClassicAudioCommandType::load_song,
				.cue = ClassicMusicCue::staff,
			})) {
				break;
			}
			driver_phase_ = DriverPhase::initialize;
			return tick(ClassicStaffBoundary::immediate);

		case DriverPhase::initialize:
			if(!initialize_visuals()) {
				break;
			}
			phase_ = ClassicStaffPhase::presentation;
			driver_phase_ = DriverPhase::initial_wait;
			return tick(ClassicStaffBoundary::immediate, true);

		case DriverPhase::initial_wait:
			driver_phase_ = DriverPhase::initial_flakes;
			return tick(ClassicStaffBoundary::vsync);

		case DriverPhase::initial_flakes:
			present::staffroll_blue_plane_clear(raster_);
			if(!tick_flakes(input.vsync_count_above_one)) {
				break;
			}
			present::staff_flakes_advance_frame(*flakes_);
			if(phase_done(input.audio, 4U, input.fast_forward_held) &&
				!initialize_sequence()) {
				break;
			}
			return tick(ClassicStaffBoundary::vsync, true);

		case DriverPhase::sequence_in: {
			const auto &sequence = staff_sequences[sequence_index_];
			const bool drawn = sequence.kind == SequenceKind::overlay ?
				overlay_up() : slide_up();
			if(!drawn || !tick_flakes(input.vsync_count_above_one)) {
				break;
			}
			present::staff_flakes_advance_frame(*flakes_);
			if(phase_done(
				input.audio, sequence.measure_in, input.fast_forward_held
			)) {
				present::staff_flakes_restart_phase(*flakes_);
				driver_phase_ = sequence.kind == SequenceKind::setup_y ?
					DriverPhase::sequence_blank : DriverPhase::sequence_out;
			}
			return tick(ClassicStaffBoundary::vsync, true);
		}

		case DriverPhase::sequence_blank: {
			const auto &sequence = staff_sequences[sequence_index_];
			present::staffroll_blue_plane_clear(raster_);
			if(!tick_flakes(input.vsync_count_above_one)) {
				break;
			}
			present::staff_flakes_advance_frame(*flakes_);
			if(phase_done(
				input.audio, sequence.measure_out, input.fast_forward_held
			)) {
				overlay_y_ = center_y_[page_index(flakes_->back_page)];
				++sequence_index_;
				if(!initialize_sequence()) {
					break;
				}
			}
			return tick(ClassicStaffBoundary::vsync, true);
		}

		case DriverPhase::sequence_out: {
			const auto &sequence = staff_sequences[sequence_index_];
			const bool drawn = sequence.kind == SequenceKind::overlay ?
				overlay_out() : slide_out();
			if(!drawn || !tick_flakes(input.vsync_count_above_one)) {
				break;
			}
			present::staff_flakes_advance_frame(*flakes_);
			if(phase_done(
				input.audio, sequence.measure_out, input.fast_forward_held
			)) {
				cleanup_ticks_ = 0U;
				driver_phase_ = DriverPhase::cleanup;
			}
			return tick(ClassicStaffBoundary::vsync, true);
		}

		case DriverPhase::cleanup:
			if(!clear_inner() || !tick_flakes(input.vsync_count_above_one)) {
				break;
			}
			++cleanup_ticks_;
			if(cleanup_ticks_ == 2U) {
				++sequence_index_;
				if(sequence_index_ < staff_sequences.size()) {
					if(!initialize_sequence()) {
						break;
					}
				} else {
					present::staff_flakes_restart_phase(*flakes_);
					driver_phase_ = DriverPhase::dissolve;
				}
			}
			return tick(ClassicStaffBoundary::vsync, true);

		case DriverPhase::dissolve:
			if(!dissolve() || !tick_flakes(input.vsync_count_above_one)) {
				break;
			}
			present::staff_flakes_advance_frame(*flakes_);
			if(phase_done(input.audio, 66U, input.fast_forward_held)) {
				driver_phase_ = DriverPhase::verdict_install;
			}
			return tick(ClassicStaffBoundary::vsync, true);

		case DriverPhase::verdict_install:
			if(!draw_verdict()) {
				break;
			}
			present::staff_flakes_restart_phase(*flakes_);
			verdict_tone_ = 0;
			phase_ = ClassicStaffPhase::verdict;
			driver_phase_ = DriverPhase::verdict_loop;
			return tick(ClassicStaffBoundary::immediate, true);

		case DriverPhase::verdict_loop:
			present::staffroll_blue_plane_clear(raster_);
			if(!tick_flakes(input.vsync_count_above_one)) {
				break;
			}
			present::staff_flakes_advance_frame(*flakes_);
			if(verdict_tone_ != 0) {
				raster_.set_palette_tone(verdict_tone_);
				raster_.apply_pending_palette();
				if((flakes_->frame & 1) != 0) {
					--verdict_tone_;
					if(verdict_tone_ == 0) {
						completion_.emplace(
							ClassicStaffCompletion(std::move(raster_))
						);
						phase_ = ClassicStaffPhase::ready;
					}
				}
			} else if(input.any_input &&
				flakes_->frame > static_cast<std::int16_t>(
					source_phase_frame_threshold
				)) {
				if(!push_audio_command({
					.type = ClassicAudioCommandType::fade_song,
					.cue = ClassicMusicCue::staff,
					.parameter = 8U,
				})) {
					break;
				}
				verdict_tone_ = 100;
				present::staff_flakes_restart_phase(*flakes_);
			}
			return tick(ClassicStaffBoundary::vsync, true, true);
		}
		break;
	}

	fail();
	return std::unexpected(staff_error(ClassicStaffErrorCode::draw_failed));
}

std::optional<ClassicAudioCommand>
ClassicStaffScene::take_audio_command() noexcept
{
	if(audio_count_ == 0U) {
		return std::nullopt;
	}
	const auto command = audio_commands_[audio_head_];
	audio_head_ = (audio_head_ + 1U) % audio_commands_.size();
	--audio_count_;
	return command;
}

std::optional<ClassicStaffCompletion>
ClassicStaffScene::take_completion() noexcept
{
	if(phase_ != ClassicStaffPhase::ready || !completion_) {
		return std::nullopt;
	}
	phase_ = ClassicStaffPhase::consumed;
	auto result = std::move(completion_);
	completion_.reset();
	return result;
}

ClassicStaffPhase ClassicStaffScene::phase() const noexcept
{
	return phase_;
}

const present::Raster &ClassicStaffScene::raster() const noexcept
{
	return completion_ ? completion_->raster() : raster_;
}

bool ClassicStaffSceneTestAccess::forced_overlay_alpha_probe(
	ClassicStaffScene &scene
) noexcept
{
	if(!scene.flakes_ || !scene.prepared_->cels[2U].alpha) {
		return false;
	}
	std::fill(
		scene.prepared_->cels[2U].alpha->begin(),
		scene.prepared_->cels[2U].alpha->end(),
		std::uint8_t{0U}
	);
	scene.sequence_index_ = 3U;
	scene.flakes_->back_page = present::Page::zero;
	scene.flakes_->frame = 0;
	scene.flakes_->cdg_put_alpha = false;
	scene.center_y_.fill(200);
	scene.overlay_y_ = 100;
	if(!scene.raster_.set_access_page(present::Page::zero) ||
		!scene.raster_.plot({120, 84}, 8U) || !scene.overlay_up()) {
		return false;
	}
	const auto result = scene.raster_.pixel(present::Page::zero, {120, 84});
	return result && *result == 11U;
}

std::int16_t ClassicStaffSceneTestAccess::frame(
	const ClassicStaffScene &scene
) noexcept
{
	return scene.flakes_ ? scene.flakes_->frame : 0;
}

void ClassicStaffScene::fail() noexcept
{
	phase_ = ClassicStaffPhase::failed;
}

} // namespace th03::app
