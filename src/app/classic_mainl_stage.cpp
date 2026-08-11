#include "classic_mainl_stage_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "th03/app/authentic_text.hpp"
#include "th03/assets/decode.hpp"
#include "th03/present/cels.hpp"
#include "th03/present/pi.hpp"
#include "th03/present/sprite_staging.hpp"

namespace th03::app {
namespace {

using ResourceResult = std::expected<
	const LoadedMainResource *,
	ClassicMainlStageError
>;

[[nodiscard]] ClassicMainlStageError stage_error(
	ClassicMainlStageErrorCode code,
	std::optional<MainResourceKey> resource = std::nullopt
) noexcept
{
	return {.code = code, .resource = resource};
}

[[nodiscard]] ClassicMainlStageError decode_error(
	MainResourceKey resource,
	assets::AssetError error
)
{
	auto result = stage_error(
		ClassicMainlStageErrorCode::asset_decode_failed,
		resource
	);
	result.asset_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] ClassicMainlStageError text_error(
	present::GraphicsTextError error
) noexcept
{
	auto result = stage_error(
		ClassicMainlStageErrorCode::font_prepare_failed
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

[[nodiscard]] std::expected<void, ClassicMainlStageError>
validate_resource_set(
	std::span<const LoadedMainResource> resources,
	std::span<const MainResourceKey> expected,
	bool authenticate_resource_bytes
)
{
	if(resources.size() != expected.size()) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::resource_set_mismatch
		));
	}
	for(std::size_t index = 0U; index < expected.size(); ++index) {
		const auto key = expected[index];
		const auto &resource = resources[index];
		if(resource.spec.key != key) {
			return std::unexpected(stage_error(
				ClassicMainlStageErrorCode::resource_set_mismatch,
				key
			));
		}
		if(!resource.bytes) {
			return std::unexpected(stage_error(
				ClassicMainlStageErrorCode::resource_bytes_missing,
				key
			));
		}
		const auto canonical = main_resource_spec(key);
		if(!canonical || !same_resource_spec(resource.spec, *canonical)) {
			return std::unexpected(stage_error(
				ClassicMainlStageErrorCode::resource_set_mismatch,
				key
			));
		}
		if(resource.bytes->size() != canonical->expected_bytes ||
			(authenticate_resource_bytes && main_resource_fnv1a64(
				*resource.bytes
			) != canonical->expected_fnv1a64)) {
			return std::unexpected(stage_error(
				ClassicMainlStageErrorCode::resource_integrity_failed,
				key
			));
		}
	}
	return {};
}

[[nodiscard]] ResourceResult find_resource(
	std::span<const LoadedMainResource> resources,
	MainResourceKey key
)
{
	for(const auto &resource : resources) {
		if(resource.spec.key == key) {
			return &resource;
		}
	}
	return std::unexpected(stage_error(
		ClassicMainlStageErrorCode::resource_set_mismatch,
		key
	));
}

template <typename Value, typename Decoder>
[[nodiscard]] std::expected<Value, ClassicMainlStageError> decode_asset(
	std::span<const LoadedMainResource> resources,
	MainResourceKey key,
	Decoder decoder
)
{
	const auto resource = find_resource(resources, key);
	if(!resource) {
		return std::unexpected(resource.error());
	}
	auto decoded = decoder(std::span<const std::byte>{*(*resource)->bytes});
	if(!decoded) {
		return std::unexpected(decode_error(key, std::move(decoded.error())));
	}
	return std::move(*decoded);
}

[[nodiscard]] MainResourceKey player_key(
	MainResourceRole role,
	MainPlayerSelection selection
) noexcept
{
	return {
		.role = role,
		.character = selection.character,
		.palette = selection.palette,
	};
}

[[nodiscard]] std::size_t character_index(
	MainCharacter character
) noexcept
{
	return static_cast<std::size_t>(
		static_cast<std::uint8_t>(character) - 1U
	);
}

[[nodiscard]] std::size_t portrait_cel(
	MainPaletteVariant palette
) noexcept
{
	return (palette == MainPaletteVariant::first) ? 0U : 1U;
}

[[nodiscard]] bool valid_pi(
	const assets::PiImage &image,
	std::uint16_t width,
	std::uint16_t height,
	bool require_palette = true
) noexcept
{
	if(image.image.width != width || image.image.height != height ||
		image.image.indices.size() != static_cast<std::size_t>(width) * height ||
		(require_palette && !image.palette)) {
		return false;
	}
	for(const auto color : image.image.indices) {
		if(color > 0x0FU) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool valid_cel(
	const assets::PlanarCel4 &cel,
	std::uint16_t width,
	std::uint16_t height
) noexcept
{
	return cel.pixel_width == width && cel.pixel_height == height &&
		cel.alpha.has_value();
}

[[nodiscard]] std::uint16_t extra_height(
	MainPlayerSelection selection
) noexcept
{
	return (selection.character == MainCharacter::reimu ||
		selection.character == MainCharacter::kana ||
		selection.character == MainCharacter::rikako) ? 144U : 128U;
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

[[nodiscard]] ClassicMainlStageTick tick_result(
	ClassicMainlStagePhase phase,
	std::uint8_t wait_count,
	bool input_accepted = false
) noexcept
{
	return {
		.phase = phase,
		.wait_count = wait_count,
		.input_accepted = input_accepted,
		.handoff_ready = phase == ClassicMainlStagePhase::ready,
	};
}

} // namespace

ClassicMainlStageTransition::ClassicMainlStageTransition(
	std::unique_ptr<Prepared> prepared
) noexcept :
	prepared_(std::move(prepared))
{
	if(prepared_) {
		handoff_game_mode_ = prepared_->config.resident.game_mode;
		handoff_story_stage_ = prepared_->config.resident.story_stage;
		handoff_paletted_character_ =
			prepared_->config.resident.paletted_character;
	}
}

ClassicMainlStageTransition::~ClassicMainlStageTransition() = default;

std::unique_ptr<ClassicMainlStageTransition>
ClassicMainlStageTestAccess::create_ready_with_handoff(
	const MainStoryConfig &config,
	present::Raster raster
)
{
	auto transition = std::unique_ptr<ClassicMainlStageTransition>(
		new ClassicMainlStageTransition(nullptr)
	);
	transition->handoff_game_mode_ = config.resident.game_mode;
	transition->handoff_story_stage_ = config.resident.story_stage;
	transition->handoff_paletted_character_ =
		config.resident.paletted_character;
	transition->raster_ = std::move(raster);
	transition->handoff_ = MainStageHandoff(transition->raster_);
	transition->phase_ = ClassicMainlStagePhase::ready;
	return transition;
}

std::expected<
	std::unique_ptr<ClassicMainlStageTransition>,
	ClassicMainlStageError
> ClassicMainlStageTransition::create(
	const MainStoryConfig &config,
	std::span<const LoadedMainResource> resources,
	MainlStageTextAssets text,
	const present::AnkGlyphProvider &font
)
{
	return create_impl(config, resources, std::move(text), font, true);
}

std::expected<
	std::unique_ptr<ClassicMainlStageTransition>,
	ClassicMainlStageError
> ClassicMainlStageTransition::create_impl(
	const MainStoryConfig &config,
	std::span<const LoadedMainResource> resources,
	MainlStageTextAssets text,
	const present::AnkGlyphProvider &font,
	bool authenticate_resource_bytes
)
{
	if(!valid_main_story_config(config)) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::invalid_config
		));
	}
	const auto player_one = main_player_selection_from_paletted(
		config.resident.paletted_character[0U]
	);
	const auto player_two = main_player_selection_from_paletted(
		config.resident.paletted_character[1U]
	);
	if(!player_one || !player_two) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::invalid_config
		));
	}
	const MainlStageResourceRequest request{
		.player_one = *player_one,
		.player_two = *player_two,
		.game_mode = config.resident.game_mode,
		.story_stage = config.resident.story_stage,
	};
	const auto expected = mainl_stage_resource_keys(request);
	if(!expected) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::invalid_config
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
	prepared->config = config;

	const MainResourceKey gaiji_key{
		.role = MainResourceRole::mainl_gaiji_font,
	};
	auto gaiji_sheet = decode_asset<assets::BfntSheet>(
		resources,
		gaiji_key,
		[](std::span<const std::byte> bytes) {
			return assets::decode_bfnt(
				bytes,
				assets::BfntConsumerPolicy::gaiji
			);
		}
	);
	if(!gaiji_sheet) {
		return std::unexpected(gaiji_sheet.error());
	}
	const auto gaiji = PreparedGameftGaiji::prepare(*gaiji_sheet);
	if(!gaiji) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::invalid_asset_geometry,
			gaiji_key
		));
	}
	prepared->mainl_gaiji.emplace(*gaiji);

	const std::array selections{*player_one, *player_two};
	for(std::size_t player = 0U; player < selections.size(); ++player) {
		const MainResourceKey portrait_key{
			.role = MainResourceRole::character_stage_portrait,
			.character = selections[player].character,
		};
		auto portrait = decode_asset<assets::PlanarCel4>(
			resources,
			portrait_key,
			[&](std::span<const std::byte> bytes) {
				return assets::decode_cdg_cel(
					bytes,
					portrait_cel(selections[player].palette)
				);
			}
		);
		if(!portrait) {
			return std::unexpected(portrait.error());
		}
		prepared->portraits[player] = std::move(*portrait);

		auto shot = decode_asset<assets::PiImage>(
			resources,
			player_key(
				MainResourceRole::character_stage_shot,
				selections[player]
			),
			assets::decode_pi
		);
		if(!shot) {
			return std::unexpected(shot.error());
		}
		prepared->player_shots[player] = std::move(*shot);

		auto extra = decode_asset<assets::PiImage>(
			resources,
			player_key(
				MainResourceRole::character_stage_extra_shot,
				selections[player]
			),
			assets::decode_pi
		);
		if(!extra) {
			return std::unexpected(extra.error());
		}
		prepared->player_extra_shots[player] = std::move(*extra);
	}

	const auto background_entry = std::find_if(
		expected->begin(), expected->end(), [](MainResourceKey key) {
			return key.role == MainResourceRole::stage_splash_background;
		}
	);
	if(background_entry == expected->end()) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::resource_set_mismatch
		));
	}
	const auto background_key = *background_entry;
	if(background_key.stage_background == MainlStageBackground::numbered) {
		const MainResourceKey number_key{
			.role = MainResourceRole::stage_number_sheet,
		};
		auto number = decode_asset<assets::PlanarCel4>(
			resources,
			number_key,
			[&](std::span<const std::byte> bytes) {
				return assets::decode_cdg_cel(
					bytes,
					static_cast<std::size_t>(
						config.resident.story_stage + 1U
					)
				);
			}
		);
		if(!number) {
			return std::unexpected(number.error());
		}
		prepared->stage_number.emplace(std::move(*number));
	}

	auto splash_base = decode_asset<assets::PiImage>(
		resources,
		{.role = MainResourceRole::stage_splash_base},
		assets::decode_pi
	);
	if(!splash_base) {
		return std::unexpected(splash_base.error());
	}
	prepared->splash_base = std::move(*splash_base);
	auto splash_background = decode_asset<assets::PiImage>(
		resources,
		background_key,
		assets::decode_pi
	);
	if(!splash_background) {
		return std::unexpected(splash_background.error());
	}
	prepared->splash_background = std::move(*splash_background);

	auto center = decode_asset<assets::PiImage>(
		resources,
		{.role = MainResourceRole::stage_enemy_center},
		assets::decode_pi
	);
	if(!center) {
		return std::unexpected(center.error());
	}
	prepared->enemy_center = std::move(*center);
	const MainResourceKey opponent_key{
		.role = MainResourceRole::character_stage_enemy_group,
		.character = player_two->character,
	};
	auto opponent = decode_asset<assets::PiImage>(
		resources,
		opponent_key,
		assets::decode_pi
	);
	if(!opponent) {
		return std::unexpected(opponent.error());
	}
	prepared->opponent_enemies = std::move(*opponent);

	if(!valid_cel(prepared->portraits[0U], 192U, 192U) ||
		!valid_cel(prepared->portraits[1U], 192U, 192U) ||
		(prepared->stage_number && !valid_cel(
			*prepared->stage_number, 32U, 34U
		)) ||
		!valid_pi(prepared->splash_base, 640U, 400U) ||
		!valid_pi(prepared->splash_background, 640U, 96U) ||
		!valid_pi(prepared->player_shots[0U], 320U, 16U) ||
		!valid_pi(prepared->player_shots[1U], 320U, 16U) ||
		!valid_pi(
			prepared->player_extra_shots[0U],
			320U,
			extra_height(*player_one)
		) || !valid_pi(
			prepared->player_extra_shots[1U],
			320U,
			extra_height(*player_two)
		) || !valid_pi(prepared->enemy_center, 640U, 48U) ||
		!valid_pi(prepared->opponent_enemies, 640U, 192U)) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::invalid_asset_geometry
		));
	}

	for(std::size_t character = 0U; character < text.characters.size(); ++character) {
		auto title = present::prepare_graphics_text_8x16(
			text.characters[character].title,
			font
		);
		if(!title) {
			return std::unexpected(text_error(title.error()));
		}
		auto name = present::prepare_graphics_text_8x16(
			text.characters[character].name,
			font
		);
		if(!name) {
			return std::unexpected(text_error(name.error()));
		}
	}
	const std::array selected_text{
		&text.characters[character_index(player_one->character)].title,
		&text.characters[character_index(player_one->character)].name,
		&text.characters[character_index(player_two->character)].title,
		&text.characters[character_index(player_two->character)].name,
	};
	for(std::size_t index = 0U; index < selected_text.size(); ++index) {
		auto selected = present::prepare_graphics_text_8x16(
			*selected_text[index],
			font
		);
		if(!selected) {
			return std::unexpected(text_error(selected.error()));
		}
		prepared->text[index].emplace(std::move(*selected));
	}

	auto transition = std::unique_ptr<ClassicMainlStageTransition>(
		new ClassicMainlStageTransition(std::move(prepared))
	);
	transition->prepared_->mainl_gaiji->install(transition->tram_);
	if(!transition->raster_.set_raster_mode(
		present::RasterMode::lines_400
	) || !transition->raster_.set_shown_page(present::Page::zero) ||
		!transition->raster_.set_access_page(present::Page::one) ||
		!present::pi_put_8(
			transition->raster_, 0, 0, transition->prepared_->splash_base
		) || !present::pi_put_8(
			transition->raster_,
			0,
			0,
			transition->prepared_->splash_background
		) || !install_palette(
			transition->raster_,
			*transition->prepared_->splash_background.palette
		) || !transition->raster_.copy_page(
			present::Page::one,
			{0, 0, present::graphics_width, present::graphics_height},
			present::Page::zero,
			{0, 0}
		) || !transition->raster_.set_access_page(present::Page::zero) ||
		!present::cdg_put_8(
			transition->raster_, 96, 96, transition->prepared_->portraits[0U]
		) || !present::cdg_put_hflip_8(
			transition->raster_, 352, 96, transition->prepared_->portraits[1U]
		) || (transition->prepared_->stage_number && !present::cdg_put_8(
			transition->raster_,
			384,
			46,
			*transition->prepared_->stage_number
		)) || !present::graphics_text_put_8x16(
			transition->raster_,
			{80, 292},
			*transition->prepared_->text[0U],
			{15U, present::GraphicsTextWeight::bold, 0U}
		) || !present::graphics_text_put_8x16(
			transition->raster_,
			{128, 308},
			*transition->prepared_->text[1U],
			{15U, present::GraphicsTextWeight::bold, 0U}
		) || !present::graphics_text_put_8x16(
			transition->raster_,
			{336, 292},
			*transition->prepared_->text[2U],
			{15U, present::GraphicsTextWeight::bold, 0U}
		) || !present::graphics_text_put_8x16(
			transition->raster_,
			{384, 308},
			*transition->prepared_->text[3U],
			{15U, present::GraphicsTextWeight::bold, 0U}
		)) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::draw_failed
		));
	}
	transition->raster_.set_palette_tone(0);
	transition->raster_.apply_pending_palette();
	return transition;
}

bool ClassicMainlStageTransition::stage_hidden_sheet() noexcept
{
	return present::main_prepare_sprite16_pi_source_sheet(
		raster_,
		{
			.player_shots = {{
				&prepared_->player_shots[0U],
				&prepared_->player_shots[1U],
			}},
			.player_extra_shots = {{
				&prepared_->player_extra_shots[0U],
				&prepared_->player_extra_shots[1U],
			}},
			.enemy_center = &prepared_->enemy_center,
			.opponent_enemies = &prepared_->opponent_enemies,
		}
	);
}

bool ClassicMainlStageTransition::finalize_handoff() noexcept
{
	tram_.fill({
		' ',
		static_cast<present::TramAttribute>(
			present::tram_black | present::tram_reverse
		),
	});
	if(!install_palette(raster_, *prepared_->opponent_enemies.palette) ||
		!raster_.set_access_page(present::Page::zero) ||
		!raster_.set_shown_page(present::Page::zero)) {
		return false;
	}
	raster_.set_palette_tone(100);
	raster_.apply_pending_palette();
	handoff_ = MainStageHandoff(raster_);
	return true;
}

std::expected<ClassicMainlStageTick, ClassicMainlStageError>
ClassicMainlStageTransition::advance(bool dismiss_pressed) noexcept
{
	if(phase_ == ClassicMainlStagePhase::failed ||
		phase_ == ClassicMainlStagePhase::ready ||
		phase_ == ClassicMainlStagePhase::consumed) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::invalid_state
		));
	}

	switch(phase_) {
	case ClassicMainlStagePhase::black_in:
		if(fade_index_ < 17U) {
			++fade_index_;
			raster_.set_palette_tone(static_cast<std::int16_t>(
				(fade_index_ - 1U) * 6U
			));
			raster_.apply_pending_palette();
			return tick_result(phase_, wait_count_);
		}
		raster_.set_palette_tone(100);
		raster_.apply_pending_palette();
		if(!stage_hidden_sheet()) {
			fail();
			return std::unexpected(stage_error(
				ClassicMainlStageErrorCode::draw_failed
			));
		}
		phase_ = ClassicMainlStagePhase::waiting;
		fade_index_ = 0U;
		wait_count_ = 0U;
		return tick_result(phase_, wait_count_);

	case ClassicMainlStagePhase::waiting: {
		if(wait_count_ < 97U) {
			++wait_count_;
		}
		const bool accepted = wait_count_ > 32U && dismiss_pressed;
		if(accepted || wait_count_ > 96U) {
			phase_ = ClassicMainlStagePhase::white_out;
			fade_index_ = 0U;
		}
		return tick_result(phase_, wait_count_, accepted);
	}

	case ClassicMainlStagePhase::white_out:
		if(fade_index_ < 17U) {
			++fade_index_;
			raster_.set_palette_tone(static_cast<std::int16_t>(
				100U + ((fade_index_ - 1U) * 6U)
			));
			raster_.apply_pending_palette();
			return tick_result(phase_, wait_count_);
		}
		if(!raster_.set_access_page(present::Page::zero) || !raster_.clear(
			{0, 0, present::graphics_width, present::graphics_height},
			0U
		)) {
			fail();
			return std::unexpected(stage_error(
				ClassicMainlStageErrorCode::draw_failed
			));
		}
		raster_.set_palette_tone(200);
		raster_.apply_pending_palette();
		phase_ = ClassicMainlStagePhase::white_in;
		fade_index_ = 0U;
		return tick_result(phase_, wait_count_);

	case ClassicMainlStagePhase::white_in:
		if(fade_index_ < 17U) {
			++fade_index_;
			raster_.set_palette_tone(static_cast<std::int16_t>(
				200U - ((fade_index_ - 1U) * 6U)
			));
			raster_.apply_pending_palette();
			return tick_result(phase_, wait_count_);
		}
		if(!finalize_handoff()) {
			fail();
			return std::unexpected(stage_error(
				ClassicMainlStageErrorCode::draw_failed
			));
		}
		phase_ = ClassicMainlStagePhase::ready;
		return tick_result(phase_, wait_count_);

	case ClassicMainlStagePhase::ready:
	case ClassicMainlStagePhase::consumed:
	case ClassicMainlStagePhase::failed:
		break;
	}
	return std::unexpected(stage_error(
		ClassicMainlStageErrorCode::invalid_state
	));
}

ClassicMainlStagePhase ClassicMainlStageTransition::phase() const noexcept
{
	return phase_;
}

std::uint8_t ClassicMainlStageTransition::wait_count() const noexcept
{
	return wait_count_;
}

const present::Raster &ClassicMainlStageTransition::raster() const noexcept
{
	return raster_;
}

const present::Tram &ClassicMainlStageTransition::tram() const noexcept
{
	return tram_;
}

const MainStageHandoff *ClassicMainlStageTransition::handoff() const noexcept
{
	return handoff_ ? &*handoff_ : nullptr;
}

bool ClassicMainlStageTransition::matches_process_config(
	const MainStoryConfig &config
) const noexcept
{
	return handoff_game_mode_ == config.resident.game_mode &&
		handoff_story_stage_ == config.resident.story_stage &&
		handoff_paletted_character_ == config.resident.paletted_character;
}

void ClassicMainlStageTransition::consume_handoff() noexcept
{
	handoff_.reset();
	phase_ = ClassicMainlStagePhase::consumed;
}

void ClassicMainlStageTransition::fail() noexcept
{
	phase_ = ClassicMainlStagePhase::failed;
}

} // namespace th03::app
