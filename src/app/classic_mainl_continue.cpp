#include "classic_mainl_continue_internal.hpp"

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
#include "th03/present/pi.hpp"

namespace th03::app {
namespace {

[[nodiscard]] ClassicMainlContinueError continue_error(
	ClassicMainlContinueErrorCode code,
	std::optional<MainResourceKey> resource = std::nullopt
) noexcept
{
	return {.code = code, .resource = resource};
}

[[nodiscard]] ClassicMainlContinueError decode_error(
	MainResourceKey resource,
	assets::AssetError error
)
{
	auto result = continue_error(
		ClassicMainlContinueErrorCode::asset_decode_failed,
		resource
	);
	result.asset_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicMainlContinueError text_error(
	present::GraphicsTextError error
) noexcept
{
	auto result = continue_error(
		ClassicMainlContinueErrorCode::font_prepare_failed
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

[[nodiscard]] std::expected<void, ClassicMainlContinueError>
validate_resource_set(
	std::span<const LoadedMainResource> resources,
	std::span<const MainResourceKey> expected,
	bool authenticate_resource_bytes
)
{
	if(resources.size() != expected.size()) {
		return std::unexpected(continue_error(
			ClassicMainlContinueErrorCode::resource_set_mismatch
		));
	}
	for(std::size_t index = 0U; index < expected.size(); ++index) {
		const auto key = expected[index];
		const auto &resource = resources[index];
		if(resource.spec.key != key) {
			return std::unexpected(continue_error(
				ClassicMainlContinueErrorCode::resource_set_mismatch,
				key
			));
		}
		if(!resource.bytes) {
			return std::unexpected(continue_error(
				ClassicMainlContinueErrorCode::resource_bytes_missing,
				key
			));
		}
		const auto canonical = main_resource_spec(key);
		if(!canonical || !same_resource_spec(resource.spec, *canonical)) {
			return std::unexpected(continue_error(
				ClassicMainlContinueErrorCode::resource_set_mismatch,
				key
			));
		}
		if(resource.bytes->size() != canonical->expected_bytes ||
			(authenticate_resource_bytes && main_resource_fnv1a64(
				*resource.bytes
			) != canonical->expected_fnv1a64)) {
			return std::unexpected(continue_error(
				ClassicMainlContinueErrorCode::resource_integrity_failed,
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
[[nodiscard]] std::expected<Value, ClassicMainlContinueError> decode_asset(
	std::span<const LoadedMainResource> resources,
	MainResourceKey key,
	Decoder decoder
)
{
	const auto *resource = find_resource(resources, key);
	if(!resource || !resource->bytes) {
		return std::unexpected(continue_error(
			ClassicMainlContinueErrorCode::resource_set_mismatch,
			key
		));
	}
	auto decoded = decoder(std::span<const std::byte>{*resource->bytes});
	if(!decoded) {
		return std::unexpected(decode_error(key, std::move(decoded.error())));
	}
	return std::move(*decoded);
}

[[nodiscard]] bool valid_pi(const assets::PiImage &image) noexcept
{
	if(image.image.width != present::graphics_width ||
		image.image.height != present::graphics_height || !image.palette ||
		image.image.indices.size() != present::graphics_page_pixels) {
		return false;
	}
	for(const auto color : image.image.indices) {
		if(color > 0x0FU) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool valid_choice_cel(
	const assets::PlanarCel4 &cel
) noexcept
{
	constexpr std::uint16_t width = 96U;
	constexpr std::uint16_t height = 41U;
	constexpr std::size_t plane_size = 492U;
	if(cel.pixel_width != width || cel.pixel_height != height ||
		cel.bitplane_size != plane_size ||
		cel.offset_at_bottom_left != 3200 || cel.vram_dword_width != 3U ||
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

[[nodiscard]] bool valid_continue_resident(const GameInit &resident) noexcept
{
	const auto valid_character = [](std::int8_t value) {
		return value >= 1 && value <= 18;
	};
	return resident.game_mode == game_mode::story &&
		resident.story_stage >= 1U && resident.story_stage <= 8U &&
		resident.remaining_credits <= 3U &&
		valid_character(resident.paletted_character[0U]) &&
		valid_character(resident.paletted_character[1U]) &&
		!resident.runtime_snapshot && !resident.compact_runtime_snapshot &&
		!resident.round_start_snapshot && !resident.practice_rules;
}

[[nodiscard]] bool install_palette(
	present::Raster &raster,
	const assets::Palette16Rgb4 &palette
) noexcept
{
	for(std::size_t index = 0U; index < palette.colors.size(); ++index) {
		if(!raster.set_pending_palette_entry(
			static_cast<std::uint8_t>(index),
			palette.colors[index]
		)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] ClassicMainlContinueTick continue_tick(
	ClassicMainlContinuePhase phase,
	ClassicMainlContinueBoundary boundary,
	bool surface_changed = false,
	bool input_sampled = false,
	bool choice_changed = false,
	bool choice_committed = false
) noexcept
{
	return {
		.phase = phase,
		.boundary = boundary,
		.surface_changed = surface_changed,
		.input_sampled = input_sampled,
		.choice_changed = choice_changed,
		.choice_committed = choice_committed,
		.ready = phase == ClassicMainlContinuePhase::ready,
	};
}

[[nodiscard]] ClassicMainlGameOverTick gameover_tick(
	ClassicMainlGameOverPhase phase,
	ClassicMainlGameOverBoundary boundary,
	bool surface_changed = false
) noexcept
{
	return {
		.phase = phase,
		.boundary = boundary,
		.surface_changed = surface_changed,
		.ready = phase == ClassicMainlGameOverPhase::ready,
	};
}

} // namespace

ClassicMainlContinueCompletion::ClassicMainlContinueCompletion(
	ContinueChoice choice,
	GameInit resident,
	present::Raster raster
) noexcept :
	choice_(choice),
	resident_(std::move(resident)),
	raster_(std::move(raster))
{}

ContinueChoice ClassicMainlContinueCompletion::choice() const noexcept
{
	return choice_;
}

const GameInit &ClassicMainlContinueCompletion::resident() const noexcept
{
	return resident_;
}

const present::Raster &ClassicMainlContinueCompletion::raster() const noexcept
{
	return raster_;
}

ClassicMainlContinueScene::ClassicMainlContinueScene(
	std::unique_ptr<Prepared> prepared,
	GameInit resident
) noexcept :
	prepared_(std::move(prepared)),
	resident_(std::move(resident))
{}

ClassicMainlContinueScene::~ClassicMainlContinueScene() = default;

std::expected<std::unique_ptr<ClassicMainlContinueScene>, ClassicMainlContinueError>
ClassicMainlContinueScene::create(
	GameInit resident,
	std::span<const LoadedMainResource> resources,
	const present::AnkGlyphProvider &font
)
{
	return create_impl(std::move(resident), resources, font, true);
}

std::expected<std::unique_ptr<ClassicMainlContinueScene>, ClassicMainlContinueError>
ClassicMainlContinueScene::create_impl(
	GameInit resident,
	std::span<const LoadedMainResource> resources,
	const present::AnkGlyphProvider &font,
	bool authenticate_resource_bytes
)
{
	if(!valid_continue_resident(resident)) {
		return std::unexpected(continue_error(
			ClassicMainlContinueErrorCode::invalid_context
		));
	}
	for(auto &score : resident.last_score) {
		score.fill(0U);
	}
	const auto expected = mainl_continue_resource_keys(
		resident.remaining_credits
	);
	if(const auto valid = validate_resource_set(
		resources,
		*expected,
		authenticate_resource_bytes
	); !valid) {
		return std::unexpected(valid.error());
	}

	auto prepared = std::make_unique<Prepared>();
	const MainResourceKey gameover_key{
		.role = MainResourceRole::gameover_background,
	};
	auto gameover = decode_asset<assets::PiImage>(
		resources,
		gameover_key,
		assets::decode_pi
	);
	if(!gameover) {
		return std::unexpected(gameover.error());
	}
	prepared->gameover_background = std::move(*gameover);
	if(!valid_pi(prepared->gameover_background)) {
		return std::unexpected(continue_error(
			ClassicMainlContinueErrorCode::invalid_asset_geometry,
			gameover_key
		));
	}

	if(resident.remaining_credits != 0U) {
		const MainResourceKey background_key{
			.role = MainResourceRole::continue_background,
		};
		auto background = decode_asset<assets::PiImage>(
			resources,
			background_key,
			assets::decode_pi
		);
		if(!background) {
			return std::unexpected(background.error());
		}
		if(!valid_pi(*background)) {
			return std::unexpected(continue_error(
				ClassicMainlContinueErrorCode::invalid_asset_geometry,
				background_key
			));
		}
		prepared->continue_background.emplace(std::move(*background));

		const MainResourceKey choice_key{
			.role = MainResourceRole::continue_choice_cels,
		};
		for(std::size_t cel = 0U; cel < prepared->choice_cels.size(); ++cel) {
			auto decoded = decode_asset<assets::PlanarCel4>(
				resources,
				choice_key,
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
			if(!valid_choice_cel(*decoded)) {
				return std::unexpected(continue_error(
					ClassicMainlContinueErrorCode::invalid_asset_geometry,
					choice_key
				));
			}
			prepared->choice_cels[cel].emplace(std::move(*decoded));
		}

		for(std::size_t digit = 0U; digit < prepared->credit_digits.size(); ++digit) {
			const std::array source{
				static_cast<std::uint8_t>('0' + digit),
			};
			auto text = present::prepare_graphics_text_8x16(source, font);
			if(!text) {
				return std::unexpected(text_error(text.error()));
			}
			prepared->credit_digits[digit].emplace(std::move(*text));
		}
	}

	auto scene = std::unique_ptr<ClassicMainlContinueScene>(
		new ClassicMainlContinueScene(
			std::move(prepared), std::move(resident)
		)
	);
	if(!scene->raster_.set_raster_mode(present::RasterMode::lines_400) ||
		!scene->raster_.set_shown_page(present::Page::zero) ||
		!scene->raster_.set_access_page(present::Page::zero)) {
		return std::unexpected(continue_error(
			ClassicMainlContinueErrorCode::draw_failed
		));
	}
	if(scene->resident_.remaining_credits == 0U) {
		if(!scene->draw_gameover()) {
			return std::unexpected(continue_error(
				ClassicMainlContinueErrorCode::draw_failed
			));
		}
		scene->phase_ = ClassicMainlContinuePhase::ready;
		scene->choice_ = ContinueChoice::decline;
		scene->completion_ = ClassicMainlContinueCompletion(
			ContinueChoice::decline,
			std::move(scene->resident_),
			std::move(scene->raster_)
		);
		return scene;
	}
	if(!scene->draw_continue()) {
		return std::unexpected(continue_error(
			ClassicMainlContinueErrorCode::draw_failed
		));
	}
	scene->raster_.set_palette_tone(0);
	scene->raster_.apply_pending_palette();
	return scene;
}

bool ClassicMainlContinueScene::draw_continue() noexcept
{
	return prepared_->continue_background &&
		prepared_->continue_background->palette &&
		install_palette(
			raster_, *prepared_->continue_background->palette
		) && present::pi_put_8(
			raster_, 0, 0, *prepared_->continue_background
		) && draw_choices() && draw_credit(resident_.remaining_credits);
}

bool ClassicMainlContinueScene::draw_choices() noexcept
{
	const std::size_t left = (choice_ == ContinueChoice::accept) ? 0U : 2U;
	const std::size_t right = (choice_ == ContinueChoice::accept) ? 3U : 1U;
	return prepared_->choice_cels[left] && prepared_->choice_cels[right] &&
		present::cdg_put_noalpha_8(
			raster_, 192, 272, *prepared_->choice_cels[left]
		) && present::cdg_put_noalpha_8(
			raster_, 352, 272, *prepared_->choice_cels[right]
		);
}

bool ClassicMainlContinueScene::draw_credit(std::uint8_t value) noexcept
{
	return value < prepared_->credit_digits.size() &&
		prepared_->credit_digits[value] &&
		present::graphics_text_put_8x16(
			raster_,
			{576, 371},
			*prepared_->credit_digits[value],
			{15U, present::GraphicsTextWeight::bold, 0U}
		);
}

bool ClassicMainlContinueScene::draw_gameover() noexcept
{
	if(!prepared_->gameover_background.palette ||
		!raster_.set_access_page(present::Page::zero) ||
		!raster_.set_shown_page(present::Page::zero) ||
		!install_palette(raster_, *prepared_->gameover_background.palette) ||
		!present::pi_put_8(
			raster_, 0, 0, prepared_->gameover_background
		)) {
		return false;
	}
	raster_.set_palette_tone(0);
	raster_.apply_pending_palette();
	return true;
}

bool ClassicMainlContinueScene::push_audio_command(
	ClassicAudioCommand command
) noexcept
{
	if(audio_count_ != 0U) {
		return false;
	}
	audio_commands_[0U] = command;
	audio_count_ = 1U;
	return true;
}

std::optional<ClassicAudioCommand>
ClassicMainlContinueScene::take_audio_command() noexcept
{
	if(audio_count_ == 0U) {
		return std::nullopt;
	}
	audio_count_ = 0U;
	return audio_commands_[0U];
}

void ClassicMainlContinueScene::fail() noexcept
{
	phase_ = ClassicMainlContinuePhase::failed;
}

std::expected<ClassicMainlContinueTick, ClassicMainlContinueError>
ClassicMainlContinueScene::advance(ClassicMainlContinueInput input) noexcept
{
	if(audio_count_ != 0U) {
		return std::unexpected(continue_error(
			ClassicMainlContinueErrorCode::audio_command_pending
		));
	}
	if(phase_ == ClassicMainlContinuePhase::ready ||
		phase_ == ClassicMainlContinuePhase::consumed ||
		phase_ == ClassicMainlContinuePhase::failed) {
		return std::unexpected(continue_error(
			ClassicMainlContinueErrorCode::invalid_state
		));
	}

	switch(phase_) {
	case ClassicMainlContinuePhase::black_in:
		++fade_count_;
		if(fade_count_ == 1U) {
			return continue_tick(
				phase_, ClassicMainlContinueBoundary::vsync
			);
		}
		if(fade_count_ < 18U) {
			raster_.set_palette_tone(static_cast<std::int16_t>(
				(fade_count_ - 1U) * 6U
			));
			raster_.apply_pending_palette();
			return continue_tick(
				phase_, ClassicMainlContinueBoundary::vsync, true
			);
		}
		raster_.set_palette_tone(100);
		raster_.apply_pending_palette();
		phase_ = ClassicMainlContinuePhase::waiting_for_input;
		fade_count_ = 0U;
		return continue_tick(
			phase_, ClassicMainlContinueBoundary::vsync, true
		);

	case ClassicMainlContinuePhase::waiting_for_input: {
		bool choice_changed = false;
		if(input.left || input.right) {
			if(!input_locked_) {
				choice_ = (choice_ == ContinueChoice::accept) ?
					ContinueChoice::decline : ContinueChoice::accept;
				input_locked_ = true;
				choice_changed = true;
				if(!draw_choices()) {
					fail();
					return std::unexpected(continue_error(
						ClassicMainlContinueErrorCode::draw_failed
					));
				}
			}
		} else {
			input_locked_ = false;
		}

		const bool confirmed = input.ok || input.shot;
		if(!confirmed && !input.cancel) {
			return continue_tick(
				phase_,
				ClassicMainlContinueBoundary::vsync,
				choice_changed,
				true,
				choice_changed
			);
		}
		if(!confirmed && input.cancel) {
			choice_ = ContinueChoice::decline;
		}
		bool surface_changed = choice_changed;
		if(confirmed && choice_ == ContinueChoice::accept) {
			if(!raster_.clear({576, 371, 17U, 17U}, 0U) ||
				!draw_credit(static_cast<std::uint8_t>(
					resident_.remaining_credits - 1U
				))) {
				fail();
				return std::unexpected(continue_error(
					ClassicMainlContinueErrorCode::draw_failed
				));
			}
			--resident_.remaining_credits;
			surface_changed = true;
		}
		if(!push_audio_command({
			.type = ClassicAudioCommandType::fade_song,
			.cue = ClassicMusicCue::high_score,
			.parameter = 3U,
		})) {
			fail();
			return std::unexpected(continue_error(
				ClassicMainlContinueErrorCode::invalid_state
			));
		}
		phase_ = ClassicMainlContinuePhase::black_out;
		fade_count_ = 0U;
		return continue_tick(
			phase_,
			ClassicMainlContinueBoundary::immediate,
			surface_changed,
			true,
			choice_changed,
			true
		);
	}

	case ClassicMainlContinuePhase::black_out:
		++fade_count_;
		if(fade_count_ == 1U) {
			return continue_tick(
				phase_, ClassicMainlContinueBoundary::vsync
			);
		}
		if(fade_count_ < 18U) {
			raster_.set_palette_tone(static_cast<std::int16_t>(
				100U - ((fade_count_ - 1U) * 6U)
			));
			raster_.apply_pending_palette();
			return continue_tick(
				phase_, ClassicMainlContinueBoundary::vsync, true
			);
		}
		if(!draw_gameover()) {
			fail();
			return std::unexpected(continue_error(
				ClassicMainlContinueErrorCode::draw_failed
			));
		}
		if(!push_audio_command({
			.type = ClassicAudioCommandType::stop_song,
			.cue = ClassicMusicCue::high_score,
		})) {
			fail();
			return std::unexpected(continue_error(
				ClassicMainlContinueErrorCode::invalid_state
			));
		}
		--resident_.story_stage;
		resident_.story_lives = 2U;
		phase_ = ClassicMainlContinuePhase::ready;
		completion_ = ClassicMainlContinueCompletion(
			choice_, std::move(resident_), std::move(raster_)
		);
		return continue_tick(
			phase_, ClassicMainlContinueBoundary::vsync, true
		);

	case ClassicMainlContinuePhase::ready:
	case ClassicMainlContinuePhase::consumed:
	case ClassicMainlContinuePhase::failed:
		break;
	}
	return std::unexpected(continue_error(
		ClassicMainlContinueErrorCode::invalid_state
	));
}

ClassicMainlContinuePhase ClassicMainlContinueScene::phase() const noexcept
{
	return phase_;
}

ContinueChoice ClassicMainlContinueScene::selected_choice() const noexcept
{
	return choice_;
}

const present::Raster &ClassicMainlContinueScene::raster() const noexcept
{
	return completion_ ? completion_->raster() : raster_;
}

std::optional<ClassicMainlContinueCompletion>
ClassicMainlContinueScene::take_completion() noexcept
{
	if(phase_ != ClassicMainlContinuePhase::ready || audio_count_ != 0U ||
		!completion_) {
		return std::nullopt;
	}
	auto result = std::move(completion_);
	completion_.reset();
	phase_ = ClassicMainlContinuePhase::consumed;
	return result;
}

ClassicMainlGameOverCompletion::ClassicMainlGameOverCompletion(
	GameInit resident,
	present::Raster raster
) noexcept :
	resident_(std::move(resident)),
	raster_(std::move(raster))
{}

const GameInit &ClassicMainlGameOverCompletion::resident() const noexcept
{
	return resident_;
}

const present::Raster &ClassicMainlGameOverCompletion::raster() const noexcept
{
	return raster_;
}

ClassicMainlGameOverScene::ClassicMainlGameOverScene(
	GameInit resident,
	present::Raster raster
) noexcept :
	resident_(std::move(resident)),
	raster_(std::move(raster))
{}

std::expected<std::unique_ptr<ClassicMainlGameOverScene>, ClassicMainlContinueError>
ClassicMainlGameOverScene::create(
	ClassicMainlContinueCompletion &&continue_completion
)
{
	if(continue_completion.choice_ != ContinueChoice::decline ||
		continue_completion.raster_.raster_mode() !=
			present::RasterMode::lines_400 ||
		continue_completion.raster_.access_page() != present::Page::zero ||
		continue_completion.raster_.shown_page() != present::Page::zero ||
		continue_completion.raster_.palette_tone() != 0) {
		return std::unexpected(continue_error(
			ClassicMainlContinueErrorCode::invalid_context
		));
	}
	auto scene = std::unique_ptr<ClassicMainlGameOverScene>(
		new ClassicMainlGameOverScene(
			std::move(continue_completion.resident_),
			std::move(continue_completion.raster_)
		)
	);
	if(!scene->push_audio_command({
		.type = ClassicAudioCommandType::stop_song,
	}) || !scene->push_audio_command({
		.type = ClassicAudioCommandType::load_song,
		.cue = ClassicMusicCue::game_over,
	}) || !scene->push_audio_command({
		.type = ClassicAudioCommandType::play_song,
		.cue = ClassicMusicCue::game_over,
	})) {
		return std::unexpected(continue_error(
			ClassicMainlContinueErrorCode::invalid_state
		));
	}
	return scene;
}

bool ClassicMainlGameOverScene::push_audio_command(
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
ClassicMainlGameOverScene::take_audio_command() noexcept
{
	if(audio_count_ == 0U) {
		return std::nullopt;
	}
	const auto command = audio_commands_[audio_head_];
	audio_head_ = (audio_head_ + 1U) % audio_commands_.size();
	--audio_count_;
	return command;
}

void ClassicMainlGameOverScene::begin_measure_wait(
	ClassicAudioStatus audio
) noexcept
{
	phase_ = ClassicMainlGameOverPhase::wait_measure_3;
	wait_uses_measure_ = audio.bgm_active;
	wait_count_ = 0U;
}

bool ClassicMainlGameOverScene::measure_wait_done(
	ClassicAudioStatus audio,
	ClassicMainlGameOverBoundary &boundary
) noexcept
{
	if(wait_uses_measure_) {
		const bool done = audio.measure >= 3U;
		boundary = done ? ClassicMainlGameOverBoundary::immediate :
			ClassicMainlGameOverBoundary::audio_measure;
		return done;
	}
	boundary = ClassicMainlGameOverBoundary::vsync;
	if(wait_count_ < 64U) {
		++wait_count_;
	}
	return wait_count_ >= 64U;
}

void ClassicMainlGameOverScene::fail() noexcept
{
	phase_ = ClassicMainlGameOverPhase::failed;
}

std::expected<ClassicMainlGameOverTick, ClassicMainlContinueError>
ClassicMainlGameOverScene::advance(ClassicAudioStatus audio) noexcept
{
	if(audio_count_ != 0U) {
		return std::unexpected(continue_error(
			ClassicMainlContinueErrorCode::audio_command_pending
		));
	}
	if(phase_ == ClassicMainlGameOverPhase::ready ||
		phase_ == ClassicMainlGameOverPhase::consumed ||
		phase_ == ClassicMainlGameOverPhase::failed) {
		return std::unexpected(continue_error(
			ClassicMainlContinueErrorCode::invalid_state
		));
	}

	switch(phase_) {
	case ClassicMainlGameOverPhase::black_in:
		++fade_count_;
		if(fade_count_ == 1U) {
			return gameover_tick(
				phase_, ClassicMainlGameOverBoundary::vsync
			);
		}
		if(fade_count_ < 18U) {
			raster_.set_palette_tone(static_cast<std::int16_t>(
				(fade_count_ - 1U) * 6U
			));
			raster_.apply_pending_palette();
			return gameover_tick(
				phase_, ClassicMainlGameOverBoundary::vsync, true
			);
		}
		raster_.set_palette_tone(100);
		raster_.apply_pending_palette();
		begin_measure_wait(audio);
		return gameover_tick(
			phase_, ClassicMainlGameOverBoundary::vsync, true
		);

	case ClassicMainlGameOverPhase::wait_measure_3: {
		ClassicMainlGameOverBoundary boundary{};
		if(!measure_wait_done(audio, boundary)) {
			return gameover_tick(phase_, boundary);
		}
		phase_ = ClassicMainlGameOverPhase::black_out;
		fade_count_ = 0U;
		return gameover_tick(phase_, boundary);
	}

	case ClassicMainlGameOverPhase::black_out:
		++fade_count_;
		if(fade_count_ == 1U) {
			return gameover_tick(
				phase_, ClassicMainlGameOverBoundary::vsync
			);
		}
		if(fade_count_ < 18U) {
			raster_.set_palette_tone(static_cast<std::int16_t>(
				100U - ((fade_count_ - 1U) * 6U)
			));
			raster_.apply_pending_palette();
			return gameover_tick(
				phase_, ClassicMainlGameOverBoundary::vsync, true
			);
		}
		raster_.set_palette_tone(0);
		raster_.apply_pending_palette();
		if(!push_audio_command({
			.type = ClassicAudioCommandType::stop_song,
			.cue = ClassicMusicCue::game_over,
		})) {
			fail();
			return std::unexpected(continue_error(
				ClassicMainlContinueErrorCode::invalid_state
			));
		}
		phase_ = ClassicMainlGameOverPhase::ready;
		completion_ = ClassicMainlGameOverCompletion(
			std::move(resident_), std::move(raster_)
		);
		return gameover_tick(
			phase_, ClassicMainlGameOverBoundary::vsync, true
		);

	case ClassicMainlGameOverPhase::ready:
	case ClassicMainlGameOverPhase::consumed:
	case ClassicMainlGameOverPhase::failed:
		break;
	}
	return std::unexpected(continue_error(
		ClassicMainlContinueErrorCode::invalid_state
	));
}

ClassicMainlGameOverPhase ClassicMainlGameOverScene::phase() const noexcept
{
	return phase_;
}

const present::Raster &ClassicMainlGameOverScene::raster() const noexcept
{
	return completion_ ? completion_->raster() : raster_;
}

std::optional<ClassicMainlGameOverCompletion>
ClassicMainlGameOverScene::take_completion() noexcept
{
	if(phase_ != ClassicMainlGameOverPhase::ready || audio_count_ != 0U ||
		!completion_) {
		return std::nullopt;
	}
	auto result = std::move(completion_);
	completion_.reset();
	phase_ = ClassicMainlGameOverPhase::consumed;
	return result;
}

} // namespace th03::app
