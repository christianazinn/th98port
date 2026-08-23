#include "classic_mainl_stage_internal.hpp"

#include "classic_op_scene_internal.hpp"
#include "classic_op_selection_internal.hpp"

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
#include <vector>

#include "th03/app/authentic_text.hpp"
#include "th03/app/new_classic_unicode.hpp"
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

[[nodiscard]] std::expected<
	present::PreparedGraphicsText8x16,
	present::GraphicsTextError
> prepare_utf8_graphics_text(
	std::span<const std::uint8_t> bytes,
	const present::AnkGlyphProvider &font,
	const NewClassicUnicodeFont *unicode_font
)
{
	if(unicode_font == nullptr) {
		return std::unexpected(present::GraphicsTextError{
			.code = present::GraphicsTextErrorCode::missing_jis_glyph,
		});
	}
	const std::string_view text{
		reinterpret_cast<const char *>(bytes.data()), bytes.size()
	};
	std::vector<present::GraphicsTextGlyph8x16> glyphs;
	glyphs.reserve(bytes.size());
	for(std::size_t offset = 0U; offset < text.size();) {
		const auto scalar = next_new_classic_utf8_scalar(text, offset);
		if(!scalar || scalar->value < 0x20U || scalar->value == 0x7FU ||
			(scalar->value >= 0x80U && scalar->value <= 0x9FU)) {
			return std::unexpected(present::GraphicsTextError{
				.code = present::GraphicsTextErrorCode::malformed_shift_jis,
				.byte_offset = offset,
			});
		}
		if(scalar->value <= 0x7EU) {
			const auto canonical = static_cast<std::uint16_t>(
				scalar->value == 0x20U ? 0x2B21U : 0x2900U + scalar->value
			);
			const auto source = font.jis_glyph(canonical);
			if(!source) {
				return std::unexpected(present::GraphicsTextError{
					.code = present::GraphicsTextErrorCode::missing_jis_glyph,
					.byte_offset = offset,
					.canonical_jis = canonical,
				});
			}
			present::GraphicsTextGlyph8x16 glyph{.width = present::ank_width};
			for(std::size_t row = 0U; row < glyph.rows.size(); ++row) {
				glyph.rows[row] = static_cast<std::uint16_t>(
					source->row_halves[row * present::jis_bytes_per_row]
				) << 8U;
			}
			glyphs.push_back(glyph);
		} else {
			auto source = unicode_font->glyph(scalar->value);
			if(!source) {
				source = unicode_font->glyph(0xFFFDU);
			}
			if(!source) {
				return std::unexpected(present::GraphicsTextError{
					.code = present::GraphicsTextErrorCode::missing_jis_glyph,
					.byte_offset = offset,
				});
			}
			if(new_classic_unicode_combining(scalar->value) && !glyphs.empty()) {
				for(std::size_t row = 0U; row < source->rows.size(); ++row) {
					glyphs.back().rows[row] = static_cast<std::uint16_t>(
						glyphs.back().rows[row] | source->rows[row]
					);
				}
			} else {
				glyphs.push_back({source->rows, source->width});
			}
		}
		offset += scalar->bytes;
	}
	return present::prepare_graphics_glyphs_8x16(glyphs);
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
		if(!resource.authenticated_override &&
			(resource.bytes->size() != canonical->expected_bytes ||
			(authenticate_resource_bytes && main_resource_fnv1a64(
				*resource.bytes
			) != canonical->expected_fnv1a64))) {
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

[[nodiscard]] constexpr std::uint8_t vs_game_mode(
	ClassicOpSelectionMode mode
) noexcept
{
	switch(mode) {
	case ClassicOpSelectionMode::vs_1p_cpu: return game_mode::vs_1p_cpu;
	case ClassicOpSelectionMode::vs_1p_2p: return game_mode::vs_1p_2p;
	case ClassicOpSelectionMode::vs_cpu_cpu: return game_mode::vs_cpu_cpu;
	case ClassicOpSelectionMode::story: break;
	}
	return game_mode::none;
}

[[nodiscard]] constexpr std::array<std::uint8_t, 2U> vs_cpu_flags(
	ClassicOpSelectionMode mode
) noexcept
{
	return {
		static_cast<std::uint8_t>(
			mode == ClassicOpSelectionMode::vs_cpu_cpu
		),
		static_cast<std::uint8_t>(
			mode != ClassicOpSelectionMode::vs_1p_2p
		),
	};
}

[[nodiscard]] bool valid_zero_score(
	const std::array<std::uint8_t, 8U> &score
) noexcept
{
	return std::all_of(score.begin(), score.end(), [](std::uint8_t digit) {
		return digit == 0U;
	});
}

[[nodiscard]] bool valid_demo_completion(
	const ClassicOpCompletion &completion
) noexcept
{
	constexpr std::array<std::array<std::int8_t, 2U>, 4U> pairings{{
		{{3, 1}},
		{{5, 13}},
		{{7, 11}},
		{{9, 5}},
	}};
	constexpr std::array<std::int32_t, 4U> random{{
		600, 1000, 3200, 500,
	}};
	if(completion.route() != ClassicOpRoute::demo ||
		completion.selection_mode()) {
		return false;
	}
	const auto &resident = completion.resident();
	if(resident.game_mode != game_mode::demo || resident.demo_number == 0U ||
		resident.demo_number > pairings.size() ||
		resident.is_cpu != std::array<std::uint8_t, 2U>{1U, 1U} ||
		resident.story_stage != 0U || resident.winner != 0 ||
		resident.show_score_menu != 0U || resident.rank > 3U ||
		resident.key_mode > 2U || resident.remaining_credits > 3U ||
		resident.fast_op_animation > 1U || resident.global_frame != 0U ||
		(resident.autofire_mask & 0xFCU) != 0U ||
		resident.runtime_snapshot || resident.compact_runtime_snapshot ||
		resident.round_start_snapshot || resident.practice_rules ||
		!valid_zero_score(resident.last_score[0U]) ||
		!valid_zero_score(resident.last_score[1U])) {
		return false;
	}
	const auto index = static_cast<std::size_t>(resident.demo_number - 1U);
	const auto player_one = main_player_selection_from_paletted(
		resident.paletted_character[0U]
	);
	const auto player_two = main_player_selection_from_paletted(
		resident.paletted_character[1U]
	);
	return resident.paletted_character == pairings[index] &&
		resident.resident_rand == random[index] && player_one && player_two;
}

[[nodiscard]] bool valid_vs_completion(
	const ClassicOpSelectionCompletion &completion
) noexcept
{
	if(completion.outcome() != ClassicOpSelectionOutcome::selected ||
		completion.mode() == ClassicOpSelectionMode::story) {
		return false;
	}
	const auto &resident = completion.resident();
	const auto player_one = main_player_selection_from_paletted(
		resident.paletted_character[0U]
	);
	const auto player_two = main_player_selection_from_paletted(
		resident.paletted_character[1U]
	);
	if(!player_one || !player_two ||
		resident.paletted_character[0U] == resident.paletted_character[1U] ||
		resident.game_mode != vs_game_mode(completion.mode()) ||
		resident.is_cpu != vs_cpu_flags(completion.mode()) ||
		resident.rank > 3U || resident.key_mode > 2U ||
		resident.story_stage != 0U || resident.demo_number != 0U ||
		resident.winner != 0 || resident.show_score_menu != 0U ||
		resident.fast_op_animation > 1U || resident.remaining_credits > 3U ||
		resident.global_frame != 0U ||
		resident.random_seed != std::bit_cast<std::uint32_t>(
			resident.resident_rand
		) || (resident.autofire_mask & 0xFCU) != 0U ||
		resident.runtime_snapshot || resident.compact_runtime_snapshot ||
		resident.round_start_snapshot || resident.practice_rules ||
		!valid_zero_score(resident.last_score[0U]) ||
		!valid_zero_score(resident.last_score[1U])) {
		return false;
	}
	return true;
}

} // namespace

std::optional<ClassicAudioCommand> classic_main_gameplay_song(
	const GameInit &resident
) noexcept
{
	const auto player_two = main_player_selection_from_paletted(
		resident.paletted_character[1U]
	);
	if(!player_two) {
		return std::nullopt;
	}
	std::uint8_t parameter = 0U;
	if(resident.game_mode == game_mode::story && resident.story_stage == 6U) {
		parameter = static_cast<std::uint8_t>(
			ClassicGameplayMusic::decision
		);
	} else {
		parameter = static_cast<std::uint8_t>(
			static_cast<std::uint8_t>(player_two->character) - 1U
		);
		if(parameter >= static_cast<std::uint8_t>(MainCharacter::chiyuri) - 1U) {
			++parameter;
		}
	}
	return ClassicAudioCommand{
		.type = ClassicAudioCommandType::load_song,
		.cue = ClassicMusicCue::gameplay,
		.parameter = parameter,
	};
}

ClassicMainlStageTransition::ClassicMainlStageTransition(
	std::unique_ptr<Prepared> prepared
) noexcept :
	prepared_(std::move(prepared))
{
	if(prepared_) {
		handoff_game_mode_ = prepared_->entry.resident.game_mode;
		handoff_story_stage_ = prepared_->entry.resident.story_stage;
		handoff_paletted_character_ =
			prepared_->entry.resident.paletted_character;
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

std::unique_ptr<ClassicMainlStageTransition>
ClassicMainlStageTestAccess::create_ready_vs_with_handoff(
	GameInit resident,
	present::KotohimeBombFadeCompatibility kotohime_compatibility,
	present::Raster raster
)
{
	auto prepared = std::make_unique<ClassicMainlStageTransition::Prepared>();
	prepared->entry = {
		.resident = std::move(resident),
		.game_options = {},
		.kotohime_compatibility = kotohime_compatibility,
		.policy = ClassicMainlStageTransition::EntryPolicy::classic_vs,
	};
	auto transition = std::unique_ptr<ClassicMainlStageTransition>(
		new ClassicMainlStageTransition(std::move(prepared))
	);
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
	return create_impl(
		{
			.resident = config.resident,
			.game_options = config.game_options,
			.kotohime_compatibility = config.kotohime_compatibility,
			.policy = EntryPolicy::story,
		},
		resources,
		std::move(text),
		font,
		true
	);
}

std::expected<
	std::unique_ptr<ClassicMainlStageTransition>,
	ClassicMainlStageError
> ClassicMainlStageTransition::create_from_selection(
	const ClassicOpSelectionCompletion &completion,
	present::KotohimeBombFadeCompatibility kotohime_compatibility,
	std::span<const LoadedMainResource> resources,
	MainlStageTextAssets text,
	const present::AnkGlyphProvider &font,
	GameOptions game_options
)
{
	return create_from_selection_impl(
		completion,
		kotohime_compatibility,
		resources,
		std::move(text),
		font,
		game_options,
		true
	);
}

std::expected<
	std::unique_ptr<ClassicMainlStageTransition>,
	ClassicMainlStageError
> ClassicMainlStageTransition::create_from_restart(
	GameInit resident,
	present::KotohimeBombFadeCompatibility kotohime_compatibility,
	std::span<const LoadedMainResource> resources,
	MainlStageTextAssets text,
	const present::AnkGlyphProvider &font,
	GameOptions game_options,
	std::optional<formats::ReplayPracticeConfig> practice,
	std::optional<std::array<MainlStageCharacterText, 2U>> player_text_override,
	const NewClassicUnicodeFont *unicode_font
)
{
	if(resident.game_mode < game_mode::vs_1p_cpu ||
		resident.game_mode > game_mode::vs_cpu_cpu) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::invalid_config
		));
	}
	return create_impl(
		{
			.resident = std::move(resident),
			.game_options = game_options,
			.kotohime_compatibility = kotohime_compatibility,
			.policy = EntryPolicy::classic_vs,
			.practice = std::move(practice),
			.player_text_override = std::move(player_text_override),
			.unicode_font = unicode_font,
		},
		resources,
		std::move(text),
		font,
		true
	);
}

std::expected<
	std::unique_ptr<ClassicMainlStageTransition>,
	ClassicMainlStageError
> ClassicMainlStageTransition::create_from_demo(
	const ClassicOpCompletion &completion,
	present::KotohimeBombFadeCompatibility kotohime_compatibility,
	std::span<const LoadedMainResource> resources,
	MainlStageTextAssets text,
	const present::AnkGlyphProvider &font,
	GameOptions game_options
)
{
	return create_from_demo_impl(
		completion,
		kotohime_compatibility,
		resources,
		std::move(text),
		font,
		game_options,
		true
	);
}

std::expected<
	std::unique_ptr<ClassicMainlStageTransition>,
	ClassicMainlStageError
> ClassicMainlStageTransition::create_from_selection_impl(
	const ClassicOpSelectionCompletion &completion,
	present::KotohimeBombFadeCompatibility kotohime_compatibility,
	std::span<const LoadedMainResource> resources,
	MainlStageTextAssets text,
	const present::AnkGlyphProvider &font,
	GameOptions game_options,
	bool authenticate_resource_bytes
)
{
	if(!valid_vs_completion(completion)) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::invalid_config
		));
	}
	return create_impl(
		{
			.resident = completion.resident(),
			.game_options = game_options,
			.kotohime_compatibility = kotohime_compatibility,
			.policy = EntryPolicy::classic_vs,
		},
		resources,
		std::move(text),
		font,
		authenticate_resource_bytes
	);
}

std::expected<
	std::unique_ptr<ClassicMainlStageTransition>,
	ClassicMainlStageError
> ClassicMainlStageTransition::create_from_demo_impl(
	const ClassicOpCompletion &completion,
	present::KotohimeBombFadeCompatibility kotohime_compatibility,
	std::span<const LoadedMainResource> resources,
	MainlStageTextAssets text,
	const present::AnkGlyphProvider &font,
	GameOptions game_options,
	bool authenticate_resource_bytes
)
{
	if(!valid_demo_completion(completion)) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::invalid_config
		));
	}
	auto resident = completion.resident();
	// MAIN independently seeds its process RNG from the resident word after
	// MAINL. GameInit keeps that process-local seed explicit.
	resident.random_seed = std::bit_cast<std::uint32_t>(resident.resident_rand);
	return create_impl(
		{
			.resident = std::move(resident),
			.game_options = game_options,
			.kotohime_compatibility = kotohime_compatibility,
			.policy = EntryPolicy::demo,
		},
		resources,
		std::move(text),
		font,
		authenticate_resource_bytes
	);
}

std::expected<
	std::unique_ptr<ClassicMainlStageTransition>,
	ClassicMainlStageError
> ClassicMainlStageTransition::create_impl(
	EntryConfig entry,
	std::span<const LoadedMainResource> resources,
	MainlStageTextAssets text,
	const present::AnkGlyphProvider &font,
	bool authenticate_resource_bytes
)
{
	if((entry.policy == EntryPolicy::story && !valid_main_story_config({
		.resident = entry.resident,
		.game_options = entry.game_options,
		.kotohime_compatibility = entry.kotohime_compatibility,
	})) || (entry.policy == EntryPolicy::classic_vs &&
		(entry.resident.game_mode != game_mode::vs_1p_cpu &&
		 entry.resident.game_mode != game_mode::vs_1p_2p &&
		 entry.resident.game_mode != game_mode::vs_cpu_cpu)) ||
		(entry.policy == EntryPolicy::demo &&
		 (entry.resident.game_mode != game_mode::demo ||
		  entry.resident.demo_number == 0U ||
		  entry.game_options.input_protocol != InputProtocol::live))) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::invalid_config
		));
	}
	const auto player_one = main_player_selection_from_paletted(
		entry.resident.paletted_character[0U]
	);
	const auto player_two = main_player_selection_from_paletted(
		entry.resident.paletted_character[1U]
	);
	if(!player_one || !player_two) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::invalid_config
		));
	}
	const MainlStageResourceRequest request{
		.player_one = *player_one,
		.player_two = *player_two,
		.game_mode = entry.resident.game_mode,
		.story_stage = entry.resident.story_stage,
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
	prepared->entry = std::move(entry);

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
						prepared->entry.resident.story_stage + 1U
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
		const auto &text_entry = text.characters[character];
		auto title = text_entry.utf8 ? prepare_utf8_graphics_text(
			text_entry.title, font, prepared->entry.unicode_font
		) : present::prepare_graphics_text_8x16(text_entry.title, font);
		if(!title) {
			return std::unexpected(text_error(title.error()));
		}
		auto name = text_entry.utf8 ? prepare_utf8_graphics_text(
			text_entry.name, font, prepared->entry.unicode_font
		) : present::prepare_graphics_text_8x16(text_entry.name, font);
		if(!name) {
			return std::unexpected(text_error(name.error()));
		}
		if(!text_entry.title_second.empty()) {
			auto second = text_entry.utf8 ? prepare_utf8_graphics_text(
				text_entry.title_second, font, prepared->entry.unicode_font
			) : present::prepare_graphics_text_8x16(
				text_entry.title_second, font
			);
			if(!second) {
				return std::unexpected(text_error(second.error()));
			}
		}
	}
	prepared->english_text = prepared->entry.player_text_override.has_value() ||
		std::ranges::all_of(
			text.characters,
			[](const MainlStageCharacterText &character) {
				return !character.title_second.empty();
			}
		);
	if(!prepared->entry.player_text_override && !prepared->english_text &&
		std::ranges::any_of(
		text.characters,
		[](const MainlStageCharacterText &character) {
			return !character.title_second.empty();
		}
	)) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::invalid_config
		));
	}
	const auto &first = prepared->entry.player_text_override ?
		(*prepared->entry.player_text_override)[0U] :
		text.characters[character_index(player_one->character)];
	const auto &second = prepared->entry.player_text_override ?
		(*prepared->entry.player_text_override)[1U] :
		text.characters[character_index(player_two->character)];
	if(prepared->entry.player_text_override &&
		(first.title.empty() || first.title_second.empty() || first.name.empty() ||
		 second.title.empty() || second.title_second.empty() ||
		 second.name.empty())) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::invalid_config
		));
	}
	const std::array<const std::vector<std::uint8_t> *, 6U> selected_text =
		prepared->english_text ? std::array{
			&first.title, &first.title_second, &first.name,
			&second.title, &second.title_second, &second.name,
		} : std::array{
			&first.title, &first.name, &first.name,
			&second.title, &second.name, &second.name,
		};
	const auto prepared_count = prepared->english_text ? 6U : 5U;
	for(std::size_t index = 0U; index < prepared_count; ++index) {
		if(!prepared->english_text && index == 2U) {
			continue;
		}
		const auto utf8 = prepared->entry.player_text_override &&
			(index < 3U ? first.utf8 : second.utf8);
		auto selected = utf8 ? prepare_utf8_graphics_text(
			*selected_text[index], font, prepared->entry.unicode_font
		) : present::prepare_graphics_text_8x16(*selected_text[index], font);
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
		) || !transition->begin_splash_show()) {
		return std::unexpected(stage_error(
			ClassicMainlStageErrorCode::draw_failed
		));
	}
	return transition;
}

bool ClassicMainlStageTransition::begin_splash_show() noexcept
{
	const bool english = prepared_ && prepared_->english_text;
	if(!prepared_ || !raster_.copy_page(
		present::Page::one,
		{0, 0, present::graphics_width, present::graphics_height},
		present::Page::zero,
		{0, 0}
	) || !raster_.set_access_page(present::Page::zero) ||
		!present::cdg_put_8(
			raster_, 96, english ? 80 : 96, prepared_->portraits[0U]
		) || !present::cdg_put_hflip_8(
			raster_, 352, english ? 80 : 96, prepared_->portraits[1U]
		) || (prepared_->stage_number && !present::cdg_put_8(
			raster_, 384, english ? 23 : 46, *prepared_->stage_number
		))) {
		return false;
	}
	const present::GraphicsTextStyle style{
		15U, present::GraphicsTextWeight::bold, 0U
	};
	const bool text_drawn = english ?
		(present::graphics_text_put_8x16(
			raster_, {80, 276}, *prepared_->text[0U], style
		) && present::graphics_text_put_8x16(
			raster_, {80, 292}, *prepared_->text[1U], style
		) && present::graphics_text_put_8x16(
			raster_, {80, 308}, *prepared_->text[2U], style
		) && present::graphics_text_put_8x16(
			raster_, {336, 276}, *prepared_->text[3U], style
		) && present::graphics_text_put_8x16(
			raster_, {336, 292}, *prepared_->text[4U], style
		) && present::graphics_text_put_8x16(
			raster_, {336, 308}, *prepared_->text[5U], style
		)) :
		(present::graphics_text_put_8x16(
			raster_, {80, 292}, *prepared_->text[0U], style
		) && present::graphics_text_put_8x16(
			raster_, {128, 308}, *prepared_->text[1U], style
		) && present::graphics_text_put_8x16(
			raster_, {336, 292}, *prepared_->text[3U], style
		) && present::graphics_text_put_8x16(
			raster_, {384, 308}, *prepared_->text[4U], style
		));
	if(!text_drawn) {
		return false;
	}
	raster_.set_palette_tone(0);
	raster_.apply_pending_palette();
	return true;
}

bool ClassicMainlStageTransition::preload_splash_into(
	present::Raster &target
) const noexcept
{
	if(!prepared_ || phase_ != ClassicMainlStagePhase::black_in ||
		fade_index_ != 0U || wait_count_ != 0U ||
		raster_.raster_mode() != present::RasterMode::lines_400 ||
		target.raster_mode() != present::RasterMode::lines_400 ||
		target.shown_page() != present::Page::zero ||
		!target.set_access_page(present::Page::one) || !target.blit(
			{
				.pixels = raster_.graphics(present::Page::one),
				.width = present::graphics_width,
				.height = present::graphics_height,
				.stride = present::graphics_width,
			},
			{0, 0}
		)) {
		return false;
	}
	const auto &palette = raster_.pending_palette();
	for(std::size_t index = 0U; index < palette.colors.size(); ++index) {
		if(!target.set_pending_palette_entry_rgb8(
			static_cast<std::uint8_t>(index),
			palette.colors[index]
		)) {
			return false;
		}
	}
	return true;
}

bool ClassicMainlStageTransition::adopt_preloaded_raster(
	present::Raster raster
) noexcept
{
	if(!prepared_ || phase_ != ClassicMainlStagePhase::black_in ||
		fade_index_ != 0U || wait_count_ != 0U ||
		raster.raster_mode() != present::RasterMode::lines_400 ||
		raster.shown_page() != present::Page::zero ||
		raster.access_page() != present::Page::one) {
		return false;
	}
	raster_ = std::move(raster);
	if(!begin_splash_show()) {
		fail();
		return false;
	}
	return true;
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

bool ClassicMainlStageTransition::push_audio_command(
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
	case ClassicMainlStagePhase::black_in: {
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
		const auto song = classic_main_gameplay_song(prepared_->entry.resident);
		if(!song || !push_audio_command({
			.type = ClassicAudioCommandType::stop_song,
		}) || !push_audio_command(*song) || !push_audio_command({
			.type = ClassicAudioCommandType::load_effects,
		})) {
			fail();
			return std::unexpected(stage_error(
				ClassicMainlStageErrorCode::invalid_state
			));
		}
		phase_ = ClassicMainlStagePhase::waiting;
		fade_index_ = 0U;
		wait_count_ = 0U;
		return tick_result(phase_, wait_count_);
	}

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

std::optional<ClassicAudioCommand>
ClassicMainlStageTransition::take_audio_command() noexcept
{
	if(audio_count_ == 0U) {
		return std::nullopt;
	}
	const auto command = audio_commands_[audio_head_];
	audio_head_ = (audio_head_ + 1U) % audio_commands_.size();
	--audio_count_;
	return command;
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

bool ClassicMainlStageTransition::is_classic_vs_entry() const noexcept
{
	return prepared_ &&
		prepared_->entry.policy == EntryPolicy::classic_vs;
}

bool ClassicMainlStageTransition::is_demo_entry() const noexcept
{
	return prepared_ &&
		prepared_->entry.policy == EntryPolicy::demo;
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
