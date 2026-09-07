#include "classic/mainl/classic_mainl_win_internal.hpp"

#include <array>
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

constexpr std::size_t win_message_line_bytes = 60U;
constexpr std::size_t win_message_text_bytes = 58U;
constexpr std::size_t win_message_lines = 3U;
constexpr std::size_t win_message_blocks = 11U;
constexpr std::size_t win_message_file_bytes =
	win_message_line_bytes * win_message_lines * win_message_blocks;
constexpr std::size_t win_message_file_without_final_crlf_bytes =
	win_message_file_bytes - 2U;

[[nodiscard]] ClassicMainlWinError win_error(
	ClassicMainlWinErrorCode code,
	std::optional<MainResourceKey> resource = std::nullopt
) noexcept
{
	return {.code = code, .resource = resource};
}

[[nodiscard]] ClassicMainlWinError decode_error(
	MainResourceKey resource,
	assets::AssetError error
)
{
	auto result = win_error(
		ClassicMainlWinErrorCode::asset_decode_failed,
		resource
	);
	result.asset_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicMainlWinError text_error(
	present::GraphicsTextError error
) noexcept
{
	auto result = win_error(
		ClassicMainlWinErrorCode::message_prepare_failed
	);
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

[[nodiscard]] std::expected<void, ClassicMainlWinError>
validate_resource_set(
	std::span<const LoadedMainResource> resources,
	std::span<const MainResourceKey> expected,
	bool authenticate_resource_bytes
)
{
	if(resources.size() != expected.size()) {
		return std::unexpected(win_error(
			ClassicMainlWinErrorCode::resource_set_mismatch
		));
	}
	for(std::size_t index = 0U; index < expected.size(); ++index) {
		const auto key = expected[index];
		const auto &resource = resources[index];
		if(resource.spec.key != key) {
			return std::unexpected(win_error(
				ClassicMainlWinErrorCode::resource_set_mismatch,
				key
			));
		}
		if(!resource.bytes) {
			return std::unexpected(win_error(
				ClassicMainlWinErrorCode::resource_bytes_missing,
				key
			));
		}
		const auto canonical = main_resource_spec(key);
		if(!canonical || !same_resource_spec(resource.spec, *canonical)) {
			return std::unexpected(win_error(
				ClassicMainlWinErrorCode::resource_set_mismatch,
				key
			));
		}
		if(!resource.authenticated_override &&
			(resource.bytes->size() != canonical->expected_bytes ||
			(authenticate_resource_bytes && main_resource_fnv1a64(
				*resource.bytes
			) != canonical->expected_fnv1a64))) {
			return std::unexpected(win_error(
				ClassicMainlWinErrorCode::resource_integrity_failed,
				key
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
[[nodiscard]] std::expected<Value, ClassicMainlWinError> decode_asset(
	std::span<const LoadedMainResource> resources,
	MainResourceKey key,
	Decoder decoder
)
{
	const auto *resource = find_resource(resources, key);
	if(!resource || !resource->bytes) {
		return std::unexpected(win_error(
			ClassicMainlWinErrorCode::resource_set_mismatch,
			key
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
	std::uint16_t height
) noexcept
{
	const auto byte_width = static_cast<std::size_t>(width) / 8U;
	const auto plane_size = byte_width * height;
	if(width == 0U || height == 0U || (width % 32U) != 0U ||
		cel.pixel_width != width || cel.pixel_height != height ||
		cel.bitplane_size != plane_size ||
		cel.offset_at_bottom_left != static_cast<std::int16_t>(
			80U * (height - 1U)
		) || cel.vram_dword_width != (width / 32U) ||
		cel.row_order != assets::PlanarRowOrder::bottom_to_top ||
		cel.bit_order != assets::PlanarBitOrder::most_significant_bit_left ||
		cel.alpha.has_value()) {
		return false;
	}
	for(const auto &plane : cel.color_planes) {
		if(plane.size() != plane_size) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] ClassicMainlWinTick tick(
	ClassicMainlWinPhase phase,
	ClassicMainlWinBoundary boundary,
	bool surface_changed = false,
	bool input_sampled = false,
	bool dismissed = false
) noexcept
{
	return {
		.phase = phase,
		.boundary = boundary,
		.surface_changed = surface_changed,
		.input_sampled = input_sampled,
		.dismissed = dismissed,
		.stage_preload_requested =
			phase == ClassicMainlWinPhase::stage_preload,
		.ready = phase == ClassicMainlWinPhase::ready,
	};
}

[[nodiscard]] bool valid_message_file(std::span<const std::byte> bytes) noexcept
{
	if(bytes.size() != win_message_file_bytes &&
		bytes.size() != win_message_file_without_final_crlf_bytes) {
		return false;
	}
	constexpr auto rows = win_message_blocks * win_message_lines;
	for(std::size_t row = 0U; row < rows; ++row) {
		const auto offset = row * win_message_line_bytes;
		if(offset + win_message_text_bytes > bytes.size()) {
			return false;
		}
		if(row + 1U == rows &&
			bytes.size() == win_message_file_without_final_crlf_bytes) {
			continue;
		}
		if(offset + win_message_line_bytes > bytes.size() ||
			bytes[offset + win_message_text_bytes] != std::byte{0x0DU} ||
			bytes[offset + win_message_text_bytes + 1U] != std::byte{0x0AU}) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] std::span<const std::uint8_t> message_row(
	std::span<const std::byte> bytes,
	std::size_t block,
	std::size_t line
) noexcept
{
	const auto offset = ((block * win_message_lines) + line) *
		win_message_line_bytes;
	return {
		reinterpret_cast<const std::uint8_t *>(bytes.data() + offset),
		win_message_text_bytes,
	};
}

} // namespace

ClassicMainlWinCompletion::ClassicMainlWinCompletion(
	present::Raster raster,
	bool stage_splash_preloaded
) noexcept :
	raster_(std::move(raster)),
	stage_splash_preloaded_(stage_splash_preloaded)
{}

const present::Raster &ClassicMainlWinCompletion::raster() const noexcept
{
	return raster_;
}

bool ClassicMainlWinCompletion::stage_splash_preloaded() const noexcept
{
	return stage_splash_preloaded_;
}

ClassicMainlWinScene::ClassicMainlWinScene(
	std::unique_ptr<Prepared> prepared
) noexcept :
	prepared_(std::move(prepared))
{}

ClassicMainlWinScene::~ClassicMainlWinScene() = default;

std::expected<std::unique_ptr<ClassicMainlWinScene>, ClassicMainlWinError>
ClassicMainlWinScene::create(
	const ClassicMainlWinContext &context,
	std::span<const LoadedMainResource> resources,
	const present::AnkGlyphProvider &font,
	const NewClassicGameTextCatalog *localized_text
)
{
	return create_impl(context, resources, font, true, localized_text);
}

std::expected<std::unique_ptr<ClassicMainlWinScene>, ClassicMainlWinError>
ClassicMainlWinScene::create_impl(
	const ClassicMainlWinContext &context,
	std::span<const LoadedMainResource> resources,
	const present::AnkGlyphProvider &font,
	bool authenticate_resource_bytes,
	const NewClassicGameTextCatalog *localized_text
)
{
	const auto winner = main_player_selection_from_paletted(
		context.winner_paletted_character
	);
	if(!winner || !main_player_selection_from_paletted(
		context.loser_paletted_character
	) || context.message_block >= win_message_blocks) {
		return std::unexpected(win_error(
			ClassicMainlWinErrorCode::invalid_context
		));
	}
	const auto expected = mainl_win_resource_keys(*winner);
	if(!expected) {
		return std::unexpected(win_error(
			ClassicMainlWinErrorCode::invalid_context
		));
	}
	if(const auto valid = validate_resource_set(
		resources,
		*expected,
		authenticate_resource_bytes
	); !valid) {
		return std::unexpected(valid.error());
	}

	auto prepared = std::make_unique<Prepared>();
	auto palette_pre = decode_asset<assets::Palette16Rgb4>(
		resources,
		{.role = MainResourceRole::win_logo_palette_pre},
		assets::decode_rgb48
	);
	if(!palette_pre) {
		return std::unexpected(palette_pre.error());
	}
	prepared->palette_pre = std::move(*palette_pre);

	const MainResourceKey fade_key{
		.role = MainResourceRole::win_logo_fade_cels,
	};
	for(std::size_t cel = 0U; cel < prepared->logo_fade.size(); ++cel) {
		auto decoded = decode_asset<assets::PlanarCel4>(
			resources,
			fade_key,
			[cel](std::span<const std::byte> bytes) {
				return assets::decode_cdg_cel(
					bytes,
					cel,
					assets::CdgAlphaPolicy::discard
				);
			}
		);
		if(!decoded) {
			return std::unexpected(decoded.error());
		}
		prepared->logo_fade[cel] = std::move(*decoded);
	}
	auto logo_final = decode_asset<assets::PlanarCel4>(
		resources,
		{.role = MainResourceRole::win_logo_final},
		[](std::span<const std::byte> bytes) {
			return assets::decode_cdg_cel(
				bytes,
				0U,
				assets::CdgAlphaPolicy::discard
			);
		}
	);
	if(!logo_final) {
		return std::unexpected(logo_final.error());
	}
	prepared->logo_final = std::move(*logo_final);

	const MainResourceKey portrait_key{
		.role = MainResourceRole::character_win_portrait,
		.character = winner->character,
	};
	const std::size_t portrait_cel =
		(winner->palette == MainPaletteVariant::first) ? 0U : 1U;
	auto portrait = decode_asset<assets::PlanarCel4>(
		resources,
		portrait_key,
		[portrait_cel](std::span<const std::byte> bytes) {
			return assets::decode_cdg_cel(
				bytes,
				portrait_cel,
				assets::CdgAlphaPolicy::discard
			);
		}
	);
	if(!portrait) {
		return std::unexpected(portrait.error());
	}
	prepared->portrait = std::move(*portrait);

	auto palette_post = decode_asset<assets::Palette16Rgb4>(
		resources,
		{.role = MainResourceRole::win_logo_palette_post},
		assets::decode_rgb48
	);
	if(!palette_post) {
		return std::unexpected(palette_post.error());
	}
	prepared->palette_post = std::move(*palette_post);

	for(const auto &cel : prepared->logo_fade) {
		if(!valid_cel(cel, 256U, 72U)) {
			return std::unexpected(win_error(
				ClassicMainlWinErrorCode::invalid_asset_geometry,
				fade_key
			));
		}
	}
	if(!valid_cel(prepared->logo_final, 256U, 72U) ||
		!valid_cel(prepared->portrait, 192U, 192U)) {
		return std::unexpected(win_error(
			ClassicMainlWinErrorCode::invalid_asset_geometry
		));
	}

	const MainResourceKey message_key{
		.role = MainResourceRole::character_win_message,
		.character = winner->character,
	};
	const auto *message = find_resource(resources, message_key);
	if(!message || !message->bytes || !valid_message_file(*message->bytes)) {
		return std::unexpected(win_error(
			ClassicMainlWinErrorCode::message_prepare_failed,
			message_key
		));
	}
	const auto message_bytes = std::span<const std::byte>{*message->bytes};
	for(std::size_t block = 0U; block < win_message_blocks; ++block) {
		for(std::size_t line = 0U; line < win_message_lines; ++line) {
			const auto shift_jis = message_row(message_bytes, block, line);
			auto text = present::prepare_graphics_text_8x16(
				shift_jis,
				font
			);
			if(!text) {
				return std::unexpected(text_error(text.error()));
			}
			if(block == context.message_block) {
				if(localized_text) {
					const auto replacement = localized_text->bytes(
						classic_mainl_win_text_id(
							winner->character, block, line
						)
					);
					auto localized = present::prepare_graphics_text_8x16(
						replacement, font
					);
					if(!localized) {
						return std::unexpected(text_error(localized.error()));
					}
					prepared->message[line].emplace(std::move(*localized));
				} else {
					prepared->message[line].emplace(std::move(*text));
				}
			}
		}
	}

	auto scene = std::unique_ptr<ClassicMainlWinScene>(
		new ClassicMainlWinScene(std::move(prepared))
	);
	scene->preload_stage_splash_ = context.preload_stage_splash;
	if(!scene->raster_.set_raster_mode(present::RasterMode::lines_400) ||
		!scene->raster_.set_access_page(present::Page::one) ||
		!scene->raster_.clear(
			{0, 0, present::graphics_width, present::graphics_height},
			0U
		) || !scene->raster_.set_access_page(present::Page::zero) ||
		!scene->raster_.clear(
			{0, 0, present::graphics_width, present::graphics_height},
			0U
		) || !scene->raster_.set_shown_page(present::Page::zero) ||
		!scene->install_palette(scene->prepared_->palette_pre) ||
		!scene->draw_logo(0U)) {
		return std::unexpected(win_error(
			ClassicMainlWinErrorCode::draw_failed
		));
	}
	scene->raster_.set_palette_tone(0);
	scene->raster_.apply_pending_palette();
	if(!scene->push_audio_command({
		.type = ClassicAudioCommandType::load_song,
		.cue = ClassicMusicCue::winner,
	}) || !scene->push_audio_command({
		.type = ClassicAudioCommandType::play_song,
		.cue = ClassicMusicCue::winner,
	})) {
		return std::unexpected(win_error(
			ClassicMainlWinErrorCode::invalid_state
		));
	}
	return scene;
}

bool ClassicMainlWinScene::install_palette(
	const assets::Palette16Rgb4 &palette
) noexcept
{
	for(std::size_t index = 0U; index < palette.colors.size(); ++index) {
		if(!raster_.set_pending_palette_entry(
			static_cast<std::uint8_t>(index),
			palette.colors[index]
		)) {
			return false;
		}
	}
	return true;
}

bool ClassicMainlWinScene::draw_logo(std::size_t cel) noexcept
{
	return cel < prepared_->logo_fade.size() && present::cdg_put_noalpha_8(
		raster_,
		352,
		300,
		prepared_->logo_fade[cel]
	);
}

bool ClassicMainlWinScene::draw_reveal() noexcept
{
	raster_.set_palette_tone(200);
	raster_.apply_pending_palette();
	return present::cdg_put_noalpha_8(
		raster_, 224, 64, prepared_->portrait
	) && present::cdg_put_noalpha_8(
		raster_, 352, 300, prepared_->logo_final
	) && install_palette(prepared_->palette_post) &&
		(raster_.apply_pending_palette(), true);
}

bool ClassicMainlWinScene::draw_message() noexcept
{
	for(std::size_t line = 0U; line < prepared_->message.size(); ++line) {
		if(!prepared_->message[line] || !present::graphics_text_put_8x16(
			raster_,
			{80, static_cast<std::int32_t>(272U + (line * 16U))},
			*prepared_->message[line],
			{15U, present::GraphicsTextWeight::bold, 0U}
		)) {
			return false;
		}
	}
	return true;
}

void ClassicMainlWinScene::begin_measure_wait(
	ClassicMainlWinPhase phase,
	ClassicAudioStatus audio,
	std::uint16_t measure,
	std::uint16_t fallback_frames
) noexcept
{
	phase_ = phase;
	wait_uses_measure_ = audio.bgm_active;
	wait_measure_ = measure;
	wait_fallback_frames_ = fallback_frames;
	delay_count_ = 0U;
}

bool ClassicMainlWinScene::measure_wait_done(
	ClassicAudioStatus audio,
	ClassicMainlWinBoundary &boundary
) noexcept
{
	if(wait_uses_measure_) {
		const bool done = audio.measure >= wait_measure_;
		boundary = done ? ClassicMainlWinBoundary::immediate :
			ClassicMainlWinBoundary::audio_measure;
		return done;
	}
	boundary = ClassicMainlWinBoundary::vsync;
	if(delay_count_ < wait_fallback_frames_) {
		++delay_count_;
	}
	return delay_count_ >= wait_fallback_frames_;
}

bool ClassicMainlWinScene::push_audio_command(
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
ClassicMainlWinScene::take_audio_command() noexcept
{
	if(audio_count_ == 0U) {
		return std::nullopt;
	}
	const auto command = audio_commands_[audio_head_];
	audio_head_ = (audio_head_ + 1U) % audio_commands_.size();
	--audio_count_;
	return command;
}

void ClassicMainlWinScene::fail() noexcept
{
	phase_ = ClassicMainlWinPhase::failed;
}

std::expected<ClassicMainlWinTick, ClassicMainlWinError>
ClassicMainlWinScene::advance(
	ClassicAudioStatus audio,
	bool dismiss_pressed
) noexcept
{
	if(audio_count_ != 0U) {
		return std::unexpected(win_error(
			ClassicMainlWinErrorCode::audio_command_pending
		));
	}
	if(phase_ == ClassicMainlWinPhase::ready ||
		phase_ == ClassicMainlWinPhase::failed) {
		return std::unexpected(win_error(
			ClassicMainlWinErrorCode::invalid_state
		));
	}

	switch(phase_) {
	case ClassicMainlWinPhase::black_in:
		// palette_black_in(2): one alignment VSync, then two waits at
		// every shown tone 0..96. The final tone 100 is shown immediately
		// after the 35th boundary.
		++delay_count_;
		if(delay_count_ == 1U) {
			return tick(phase_, ClassicMainlWinBoundary::vsync);
		}
		if(delay_count_ < 35U) {
			const auto tone = static_cast<std::int16_t>(
				((delay_count_ - 1U) / 2U) * 6U
			);
			const bool changed = tone != raster_.palette_tone();
			if(changed) {
				raster_.set_palette_tone(tone);
				raster_.apply_pending_palette();
			}
			return tick(
				phase_, ClassicMainlWinBoundary::vsync, changed
			);
		}
		raster_.set_palette_tone(100);
		raster_.apply_pending_palette();
		begin_measure_wait(
			ClassicMainlWinPhase::wait_measure_6,
			audio,
			6U,
			16U
		);
		return tick(
			phase_, ClassicMainlWinBoundary::vsync, true
		);

	case ClassicMainlWinPhase::wait_measure_6: {
		ClassicMainlWinBoundary boundary{};
		if(!measure_wait_done(audio, boundary)) {
			return tick(phase_, boundary);
		}
		logo_cel_ = 1U;
		delay_count_ = 0U;
		if(!draw_logo(logo_cel_)) {
			fail();
			return std::unexpected(win_error(
				ClassicMainlWinErrorCode::draw_failed
			));
		}
		phase_ = ClassicMainlWinPhase::logo_fade;
		return tick(phase_, boundary, true);
	}

	case ClassicMainlWinPhase::logo_fade:
		++delay_count_;
		if(delay_count_ < 6U) {
			return tick(phase_, ClassicMainlWinBoundary::vsync);
		}
		delay_count_ = 0U;
		if(logo_cel_ < 4U) {
			++logo_cel_;
			if(!draw_logo(logo_cel_)) {
				fail();
				return std::unexpected(win_error(
					ClassicMainlWinErrorCode::draw_failed
				));
			}
			return tick(
				phase_, ClassicMainlWinBoundary::vsync, true
			);
		}
		begin_measure_wait(
			ClassicMainlWinPhase::wait_measure_10,
			audio,
			10U,
			64U
		);
		return tick(phase_, ClassicMainlWinBoundary::vsync);

	case ClassicMainlWinPhase::wait_measure_10: {
		ClassicMainlWinBoundary boundary{};
		if(!measure_wait_done(audio, boundary)) {
			return tick(phase_, boundary);
		}
		if(!draw_reveal()) {
			fail();
			return std::unexpected(win_error(
				ClassicMainlWinErrorCode::draw_failed
			));
		}
		begin_measure_wait(
			ClassicMainlWinPhase::wait_measure_11,
			audio,
			11U,
			4U
		);
		return tick(phase_, boundary, true);
	}

	case ClassicMainlWinPhase::wait_measure_11: {
		ClassicMainlWinBoundary boundary{};
		if(!measure_wait_done(audio, boundary)) {
			return tick(phase_, boundary);
		}
		phase_ = ClassicMainlWinPhase::white_in;
		delay_count_ = 0U;
		return tick(phase_, boundary);
	}

	case ClassicMainlWinPhase::white_in:
		// palette_white_in(1): one alignment VSync, then one wait at each
		// tone 200..104. The final 100 show follows boundary 18.
		++delay_count_;
		if(delay_count_ == 1U) {
			return tick(phase_, ClassicMainlWinBoundary::vsync);
		}
		if(delay_count_ < 18U) {
			raster_.set_palette_tone(static_cast<std::int16_t>(
				200U - ((delay_count_ - 1U) * 6U)
			));
			raster_.apply_pending_palette();
			return tick(
				phase_, ClassicMainlWinBoundary::vsync, true
			);
		}
		raster_.set_palette_tone(100);
		raster_.apply_pending_palette();
		phase_ = ClassicMainlWinPhase::post_reveal_delay;
		delay_count_ = 0U;
		return tick(
			phase_, ClassicMainlWinBoundary::vsync, true
		);

	case ClassicMainlWinPhase::post_reveal_delay:
		++delay_count_;
		if(delay_count_ < 8U) {
			return tick(phase_, ClassicMainlWinBoundary::vsync);
		}
		if(!draw_message()) {
			fail();
			return std::unexpected(win_error(
				ClassicMainlWinErrorCode::draw_failed
			));
		}
		phase_ = preload_stage_splash_ ?
			ClassicMainlWinPhase::stage_preload :
			ClassicMainlWinPhase::waiting_for_input;
		return tick(
			phase_,
			ClassicMainlWinBoundary::vsync,
			true
		);

	case ClassicMainlWinPhase::stage_preload:
		return tick(phase_, ClassicMainlWinBoundary::stage_preload);

	case ClassicMainlWinPhase::waiting_for_input:
		if(!dismiss_pressed) {
			return tick(
				phase_, ClassicMainlWinBoundary::vsync, false, true
			);
		}
		phase_ = ClassicMainlWinPhase::black_out;
		delay_count_ = 0U;
		return tick(
			phase_, ClassicMainlWinBoundary::immediate,
			false, true, true
		);

	case ClassicMainlWinPhase::black_out:
		// palette_black_out(1) has the same 18-boundary shape as
		// white-in, descending from 100 through 4 and then showing 0.
		++delay_count_;
		if(delay_count_ == 1U) {
			return tick(phase_, ClassicMainlWinBoundary::vsync);
		}
		if(delay_count_ < 18U) {
			raster_.set_palette_tone(static_cast<std::int16_t>(
				100U - ((delay_count_ - 1U) * 6U)
			));
			raster_.apply_pending_palette();
			return tick(
				phase_, ClassicMainlWinBoundary::vsync, true
			);
		}
		raster_.set_palette_tone(0);
		raster_.apply_pending_palette();
		phase_ = ClassicMainlWinPhase::ready;
		if(!push_audio_command({
			.type = ClassicAudioCommandType::stop_song,
			.cue = ClassicMusicCue::winner,
		})) {
			fail();
			return std::unexpected(win_error(
				ClassicMainlWinErrorCode::invalid_state
			));
		}
		completion_ = ClassicMainlWinCompletion(
			std::move(raster_), stage_splash_preloaded_
		);
		return tick(
			phase_, ClassicMainlWinBoundary::vsync, true
		);

	case ClassicMainlWinPhase::ready:
	case ClassicMainlWinPhase::consumed:
	case ClassicMainlWinPhase::failed:
		break;
	}
	return std::unexpected(win_error(
		ClassicMainlWinErrorCode::invalid_state
	));
}

ClassicMainlWinPhase ClassicMainlWinScene::phase() const noexcept
{
	return phase_;
}

const present::Raster &ClassicMainlWinScene::raster() const noexcept
{
	return completion_ ? completion_->raster() : raster_;
}

present::Raster &ClassicMainlWinScene::stage_preload_raster() noexcept
{
	return raster_;
}

std::expected<void, ClassicMainlWinError>
ClassicMainlWinScene::acknowledge_stage_preload() noexcept
{
	if(phase_ != ClassicMainlWinPhase::stage_preload ||
		raster_.shown_page() != present::Page::zero ||
		raster_.access_page() != present::Page::one) {
		return std::unexpected(win_error(
			ClassicMainlWinErrorCode::invalid_state
		));
	}
	stage_splash_preloaded_ = true;
	phase_ = ClassicMainlWinPhase::waiting_for_input;
	return {};
}

std::optional<ClassicMainlWinCompletion>
ClassicMainlWinScene::take_completion() noexcept
{
	if(phase_ != ClassicMainlWinPhase::ready || audio_count_ != 0U ||
		!completion_) {
		return std::nullopt;
	}
	auto result = std::move(completion_);
	completion_.reset();
	phase_ = ClassicMainlWinPhase::consumed;
	return result;
}

} // namespace th03::app
