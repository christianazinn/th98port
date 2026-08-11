#include "th03/app/main_process.hpp"

#include "main_process_internal.hpp"

#include "main_process_static_content_internal.hpp"
#include "main_stage_handoff_internal.hpp"

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
#include "th03/assets/decode.hpp"
#include "th03/audio/th03_pmd_service.hpp"
#include "th03/exact/trig.hpp"
#include "th03/formats/enedat.hpp"
#include "th03/present/main_background.hpp"
#include "th03/present/main_bombs.hpp"
#include "th03/present/main_boss.hpp"
#include "th03/present/main_effects.hpp"
#include "th03/present/main_entities.hpp"
#include "th03/present/main_extra_charge.hpp"
#include "th03/present/main_hud_result.hpp"
#include "th03/present/main_players_bullets.hpp"
#include "th03/present/main_static.hpp"
#include "th03/present/main_warning_gauge.hpp"
#include "th03/present/sprite_staging.hpp"
#include "th03/present/superimpose.hpp"

namespace th03::app {
namespace {

using ResourceResult = std::expected<
	const LoadedMainResource *,
	MainProcessError
>;

enum class MainFeedbackAction : std::uint8_t {
	request,
	reset_then_request,
	force,
};

struct MainFeedbackPolicy final {
	std::uint8_t effect_id = 0U;
	MainFeedbackAction action = MainFeedbackAction::request;
};

[[nodiscard]] constexpr MainFeedbackPolicy main_feedback_policy(
	MainFeedbackCueSite site
) noexcept
{
	switch(site) {
	case MainFeedbackCueSite::shot_fired:
		return {1U};
	case MainFeedbackCueSite::player_knockback_started:
		return {2U};
	case MainFeedbackCueSite::enemy_explosion_hitting:
	case MainFeedbackCueSite::low_burst:
		return {3U};
	case MainFeedbackCueSite::heavy_burst:
		return {5U};
	case MainFeedbackCueSite::charge_arc_started:
		return {6U};
	case MainFeedbackCueSite::yumemi_charge_detonated:
	case MainFeedbackCueSite::hyper_activated:
	case MainFeedbackCueSite::yumemi_extra_activated:
		return {7U};
	case MainFeedbackCueSite::story_extend_gained:
		return {8U};
	case MainFeedbackCueSite::gauge_spell_ready:
		return {9U};
	case MainFeedbackCueSite::rapid_burst:
		return {10U};
	case MainFeedbackCueSite::mima_chargeshot_started:
		return {13U};
	case MainFeedbackCueSite::player_defeat_explode:
		return {14U};
	case MainFeedbackCueSite::chiyuri_chargeshot_activated:
	case MainFeedbackCueSite::yumemi_charge_active_pulse:
		return {15U};
	case MainFeedbackCueSite::boss_terminal_first_frame:
	case MainFeedbackCueSite::bomb_flash:
		return {16U};
	case MainFeedbackCueSite::ellen_bomb_started:
		return {17U};
	case MainFeedbackCueSite::bomb_started:
	case MainFeedbackCueSite::boss_launched:
		return {18U};
	case MainFeedbackCueSite::extra_attack_beam_activated:
		return {20U};
	case MainFeedbackCueSite::yumemi_boss_materialization:
		return {5U, MainFeedbackAction::reset_then_request};
	case MainFeedbackCueSite::warning_interstitial_started:
		return {19U, MainFeedbackAction::force};
	}
	return {};
}

[[nodiscard]] MainProcessError process_error(
	MainProcessErrorCode code,
	std::optional<MainResourceKey> resource = std::nullopt
)
{
	return MainProcessError{.code = code, .resource = resource};
}

[[nodiscard]] MainProcessError asset_error(
	MainResourceKey resource,
	assets::AssetError error
)
{
	auto result = process_error(
		MainProcessErrorCode::asset_decode_failed,
		resource
	);
	result.asset_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] MainProcessError core_error(
	MainProcessErrorCode code,
	Error error
)
{
	auto result = process_error(code);
	result.core_error.emplace(std::move(error));
	return result;
}

[[nodiscard]] MainProcessError presentation_error(
	present::MainRoundStatus status
)
{
	auto result = process_error(MainProcessErrorCode::presentation_failed);
	result.presentation_status = status;
	return result;
}

[[nodiscard]] bool valid_selection(MainPlayerSelection selection) noexcept
{
	return selection.character >= MainCharacter::reimu &&
		selection.character <= MainCharacter::yumemi &&
		selection.palette >= MainPaletteVariant::first &&
		selection.palette <= MainPaletteVariant::second;
}

[[nodiscard]] bool valid_packed_decimal_score(
	const std::array<std::uint8_t, 8U> &score
) noexcept
{
	for(const auto digit : score) {
		if(digit > 9U) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool selects_kotohime(MainPlayerSelection selection) noexcept
{
	return selection.character == MainCharacter::kotohime;
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

[[nodiscard]] std::span<const std::byte> resource_bytes(
	const LoadedMainResource &resource
) noexcept
{
	return *resource.bytes;
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

[[nodiscard]] std::expected<void, MainProcessError> validate_resource_set(
	std::span<const LoadedMainResource> resources,
	const std::vector<MainResourceKey> &expected,
	bool authenticate_resource_bytes
)
{
	if(resources.size() != expected.size()) {
		return std::unexpected(process_error(
			MainProcessErrorCode::resource_set_mismatch
		));
	}
	for(const auto key : expected) {
		std::size_t matches = 0U;
		for(const auto &resource : resources) {
			if(resource.spec.key == key) {
				++matches;
			}
		}
		if(matches != 1U) {
			return std::unexpected(process_error(
				MainProcessErrorCode::resource_set_mismatch,
				key
			));
		}
	}
	for(const auto &resource : resources) {
		if(!resource.bytes) {
			return std::unexpected(process_error(
				MainProcessErrorCode::resource_bytes_missing,
				resource.spec.key
			));
		}
		const auto canonical = main_resource_spec(resource.spec.key);
		if(!canonical || !same_resource_spec(resource.spec, *canonical)) {
			return std::unexpected(process_error(
				MainProcessErrorCode::resource_set_mismatch,
				resource.spec.key
			));
		}
		if(resource.bytes->size() != canonical->expected_bytes ||
			(authenticate_resource_bytes &&
			 main_resource_fnv1a64(*resource.bytes) !=
				 canonical->expected_fnv1a64)) {
			return std::unexpected(process_error(
				MainProcessErrorCode::resource_integrity_failed,
				resource.spec.key
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
	const LoadedMainResource *found = nullptr;
	for(const auto &resource : resources) {
		if(resource.spec.key == key) {
			if(found != nullptr) {
				return std::unexpected(process_error(
					MainProcessErrorCode::resource_set_mismatch,
					key
				));
			}
			found = &resource;
		}
	}
	if(found == nullptr) {
		return std::unexpected(process_error(
			MainProcessErrorCode::resource_set_mismatch,
			key
		));
	}
	return found;
}

template <typename Value, typename Decoder>
[[nodiscard]] std::expected<Value, MainProcessError> decode_asset(
	std::span<const LoadedMainResource> resources,
	MainResourceKey key,
	Decoder decode
)
{
	const auto resource = find_resource(resources, key);
	if(!resource) {
		return std::unexpected(resource.error());
	}
	auto decoded = decode(resource_bytes(**resource));
	if(!decoded) {
		return std::unexpected(asset_error(key, std::move(decoded.error())));
	}
	return std::move(*decoded);
}

[[nodiscard]] std::optional<present::MainHudState> hud_state_from_round(
	const RoundStartObservation &round
) noexcept
{
	present::MainHudState hud{};
	switch(round.start.game_mode) {
	case game_mode::story:
		hud.game_mode = present::MainGameMode::story;
		break;
	case game_mode::demo:
		hud.game_mode = present::MainGameMode::demo;
		break;
	case game_mode::vs_1p_cpu:
	case game_mode::vs_1p_2p:
	case game_mode::vs_cpu_cpu:
		hud.game_mode = present::MainGameMode::versus;
		break;
	default:
		return std::nullopt;
	}
	hud.story_lives = round.start.story_lives;
	hud.boss_level = round.round.state.boss_attack_level;
	for(std::size_t player = 0U; player < hud.players.size(); ++player) {
		hud.players[player] = {
			.halfhearts = round.runtime.players[player].halfhearts,
			.bombs = round.runtime.players[player].bombs,
			.rounds_won = round.round.state.rounds_won[player],
			.gauge_level = round.round.state.gauge_attack_level[player],
		};
	}
	return hud;
}

[[nodiscard]] GameInit direct_game_init(
	const MainDirectVsConfig &config
) noexcept
{
	GameInit init{};
	init.resident_rand = std::bit_cast<std::int32_t>(config.seed);
	init.random_seed = config.seed;
	init.game_mode = game_mode::vs_1p_2p;
	init.rank = config.rank;
	init.key_mode = config.key_mode;
	init.remaining_credits = 3U;
	init.paletted_character = {
		*main_paletted_character(config.player_one),
		*main_paletted_character(config.player_two),
	};
	return init;
}

} // namespace

struct MainProcess::Impl {
	InputProtocol input_protocol = InputProtocol::live;
	std::uint8_t continues_used_digit = 0U;
	std::shared_ptr<const MainProcessStaticContent> static_content{};
	std::array<assets::BfntSheet, 2U> character_sheets{};
	std::array<assets::MrsImage, 2U> extra_portraits{};
	std::array<assets::MrsImage, 2U> bomb_portraits{};
	std::array<ClassicAudioCommand, 8U> audio_commands{};
	std::size_t audio_head = 0U;
	std::size_t audio_count = 0U;
	bool classic_audio_lifecycle = false;
	bool audio_feedback_failed = false;
	audio::Th03SoundEffectScheduler feedback_scheduler{};

	[[nodiscard]] bool push_audio(ClassicAudioCommand command) noexcept
	{
		if(audio_count >= audio_commands.size()) {
			return false;
		}
		const auto tail = (audio_head + audio_count) % audio_commands.size();
		audio_commands[tail] = command;
		++audio_count;
		return true;
	}

	void dispatch_effect(std::uint8_t effect_id) noexcept
	{
		if(!push_audio({
			.type = ClassicAudioCommandType::dispatch_effect,
			.parameter = effect_id,
		})) {
			audio_feedback_failed = true;
		}
	}

	void request_feedback(MainFeedbackCueRequested request) noexcept
	{
		if(!classic_audio_lifecycle) {
			return;
		}
		const auto policy = main_feedback_policy(request.site);
		if(policy.effect_id == 0U) {
			audio_feedback_failed = true;
			return;
		}
		if(policy.action == MainFeedbackAction::reset_then_request) {
			feedback_scheduler.reset();
		}
		if(policy.action == MainFeedbackAction::force) {
			auto forced = feedback_scheduler.force(policy.effect_id);
			if(!forced) {
				audio_feedback_failed = true;
			} else if(*forced) {
				dispatch_effect((*forced)->effect_id);
			}
			return;
		}
		if(!feedback_scheduler.request(policy.effect_id)) {
			audio_feedback_failed = true;
		}
	}

	void feedback_dispatch_boundary() noexcept
	{
		if(!classic_audio_lifecycle) {
			return;
		}
		if(const auto effect = feedback_scheduler.update()) {
			dispatch_effect(effect->effect_id);
		}
	}

	static void request_feedback_callback(
		void *context,
		MainFeedbackCueRequested request
	) noexcept
	{
		static_cast<Impl *>(context)->request_feedback(request);
	}

	static void feedback_dispatch_callback(void *context) noexcept
	{
		static_cast<Impl *>(context)->feedback_dispatch_boundary();
	}

	present::Raster raster{};
	present::Tram tram{};
	present::MainPaletteState palette{};
	present::MainStaticPresenter static_presenter{};
	std::optional<present::MainBackgroundPresenter> background{};
	std::optional<present::MainBossPresenter> bosses{};
	std::optional<present::MainEntitiesPresenter> entities{};
	std::optional<present::MainEffectsPresenter> effects{};
	std::optional<present::MainExtraChargePresenter> extra_charge{};
	std::optional<present::MainPlayersBulletsPresenter> players_bullets{};
	std::optional<present::MainBombsPresenter> bombs{};
	std::optional<present::MainWarningGaugePresenter> warning_gauge{};
	std::optional<present::MainHudResultPresenter> hud_result{};
	std::optional<present::MainRoundPresenter> round{};
	std::optional<Game> game{};
};

const assets::MrsImage *MainProcessTestAccess::portrait(
	const MainProcess &process,
	bool bomb,
	std::size_t player
) noexcept
{
	if(player >= process.impl_->extra_portraits.size()) {
		return nullptr;
	}
	return bomb ? &process.impl_->bomb_portraits[player] :
		&process.impl_->extra_portraits[player];
}

present::MainBombsStatus MainProcessTestAccess::bombs_status(
	const MainProcess &process
) noexcept
{
	return process.impl_->bombs->status();
}

std::uint8_t MainProcessTestAccess::continues_used_digit(
	const MainProcess &process
) noexcept
{
	return process.impl_->continues_used_digit;
}

MainFeedbackPolicyFact MainProcessTestAccess::feedback_policy(
	MainFeedbackCueSite site
) noexcept
{
	const auto policy = main_feedback_policy(site);
	MainFeedbackScheduling scheduling = MainFeedbackScheduling::request;
	switch(policy.action) {
	case MainFeedbackAction::request:
		break;
	case MainFeedbackAction::reset_then_request:
		scheduling = MainFeedbackScheduling::reset_then_request;
		break;
	case MainFeedbackAction::force:
		scheduling = MainFeedbackScheduling::force;
		break;
	}
	return {.effect_id = policy.effect_id, .scheduling = scheduling};
}

std::expected<std::unique_ptr<MainProcess>, MainProcessError>
MainProcess::create(
	const MainDirectVsConfig &config,
	std::span<const LoadedMainResource> resources,
	MainCompiledContent compiled_content,
	std::span<const std::byte> font_rom
)
{
	return create_impl(
		config,
		resources,
		std::move(compiled_content),
		font_rom,
		true
	);
}

bool valid_main_direct_vs_config(const MainDirectVsConfig &config) noexcept
{
	return valid_selection(config.player_one) &&
		valid_selection(config.player_two) && config.rank <= 3U &&
		config.key_mode <= 2U &&
		(config.game_options.input_protocol == InputProtocol::live ||
		 config.game_options.input_protocol == InputProtocol::recorded_replay) &&
		(!(selects_kotohime(config.player_one) ||
		   selects_kotohime(config.player_two)) ||
		 config.kotohime_compatibility.red_provider != nullptr);
}

std::optional<MainPlayerSelection> main_player_selection_from_paletted(
	std::int8_t paletted
) noexcept
{
	if(paletted < 1 || paletted > 18) {
		return std::nullopt;
	}
	const auto zero_based = static_cast<std::uint8_t>(paletted - 1);
	return MainPlayerSelection{
		.character = static_cast<MainCharacter>(
			(zero_based / 2U) + 1U
		),
		.palette = static_cast<MainPaletteVariant>(
			(zero_based % 2U) + 1U
		),
	};
}

std::optional<std::int8_t> main_paletted_character(
	MainPlayerSelection selection
) noexcept
{
	if(!valid_selection(selection)) {
		return std::nullopt;
	}
	const auto character = static_cast<std::uint8_t>(selection.character);
	const auto palette = static_cast<std::uint8_t>(selection.palette);
	return static_cast<std::int8_t>(
		((character - 1U) * 2U) + palette
	);
}

bool valid_main_story_config(const MainStoryConfig &config) noexcept
{
	const auto &resident = config.resident;
	if(resident.game_mode != game_mode::story || resident.rank > 3U ||
		resident.key_mode > 2U || resident.story_stage > 8U ||
		resident.remaining_credits > 3U || resident.demo_number != 0U ||
		resident.is_cpu != std::array<std::uint8_t, 2U>{0U, 1U} ||
		(resident.winner != 0 && resident.winner != 1) ||
		resident.show_score_menu > 1U || resident.fast_op_animation > 1U ||
		(resident.autofire_mask & 0xFCU) != 0U ||
		resident.runtime_snapshot || resident.compact_runtime_snapshot ||
		resident.round_start_snapshot || resident.practice_rules ||
		config.game_options.input_protocol != InputProtocol::live) {
		return false;
	}
	const auto player_one = main_player_selection_from_paletted(
		resident.paletted_character[0U]
	);
	const auto player_two = main_player_selection_from_paletted(
		resident.paletted_character[1U]
	);
	if(!player_one || !player_two ||
		resident.paletted_character[1U] !=
			resident.story_opponents[resident.story_stage]) {
		return false;
	}
	std::array<bool, 7U> early_characters{};
	for(std::size_t stage = 0U; stage < resident.story_opponents.size(); ++stage) {
		const auto selection = main_player_selection_from_paletted(
			resident.story_opponents[stage]
		);
		if(!selection || resident.story_opponents[stage] ==
			resident.paletted_character[0U]) {
			return false;
		}
		const auto character = static_cast<std::uint8_t>(
			selection->character
		);
		const auto first_palette = static_cast<std::int8_t>(
			((character - 1U) * 2U) + 1U
		);
		const auto expected_palette =
			(first_palette == resident.paletted_character[0U]) ?
				MainPaletteVariant::second : MainPaletteVariant::first;
		if(selection->palette != expected_palette) {
			return false;
		}
		if(stage < early_characters.size()) {
			if(character < static_cast<std::uint8_t>(MainCharacter::reimu) ||
				character > static_cast<std::uint8_t>(MainCharacter::rikako)) {
				return false;
			}
			const auto index = static_cast<std::size_t>(character - 1U);
			if(early_characters[index]) {
				return false;
			}
			early_characters[index] = true;
		} else {
			const auto expected = (stage == 7U) ?
				MainCharacter::chiyuri : MainCharacter::yumemi;
			if(selection->character != expected) {
				return false;
			}
		}
	}
	constexpr std::array<MainCharacter, 9U> fixed_stage_six{
		MainCharacter::mima,
		MainCharacter::reimu,
		MainCharacter::reimu,
		MainCharacter::marisa,
		MainCharacter::reimu,
		MainCharacter::ellen,
		MainCharacter::kana,
		MainCharacter::kotohime,
		MainCharacter::rikako,
	};
	const auto player_one_index = static_cast<std::size_t>(
		static_cast<std::uint8_t>(player_one->character) - 1U
	);
	const auto stage_six = main_player_selection_from_paletted(
		resident.story_opponents[6U]
	);
	if(!stage_six || stage_six->character != fixed_stage_six[player_one_index]) {
		return false;
	}
	for(const auto &score : resident.last_score) {
		if(!valid_packed_decimal_score(score)) {
			return false;
		}
	}
	for(const auto digit : resident.last_score[1U]) {
		if(digit != 0U) {
			return false;
		}
	}
	if((player_one->character == MainCharacter::kotohime ||
		player_two->character == MainCharacter::kotohime) &&
		config.kotohime_compatibility.red_provider == nullptr) {
		return false;
	}
	return true;
}

std::expected<std::unique_ptr<MainProcess>, MainProcessError>
MainProcess::create_impl(
	const MainDirectVsConfig &config,
	std::span<const LoadedMainResource> resources,
	MainCompiledContent compiled_content,
	std::span<const std::byte> font_rom,
	bool authenticate_resource_bytes
)
{
	if(!valid_main_direct_vs_config(config)) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_config
		));
	}
	const auto expected_keys = direct_vs_resource_keys(
		config.player_one,
		config.player_two
	);
	if(!expected_keys) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_config
		));
	}
	if(const auto valid = validate_resource_set(
		resources,
		*expected_keys,
		authenticate_resource_bytes
	);
		!valid) {
		return std::unexpected(valid.error());
	}
	const auto static_content = MainProcessStaticContentAccess::create(
		resources,
		std::move(compiled_content),
		font_rom,
		authenticate_resource_bytes
	);
	if(!static_content) {
		return std::unexpected(static_content.error());
	}

	const std::array selections{config.player_one, config.player_two};
	std::array<assets::PiImage, 2U> player_shots{};
	std::array<assets::PiImage, 2U> player_extra_shots{};
	for(std::size_t player = 0U; player < selections.size(); ++player) {
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
		player_shots[player] = std::move(*shot);
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
		player_extra_shots[player] = std::move(*extra);
	}
	auto center = decode_asset<assets::PiImage>(
		resources,
		{.role = MainResourceRole::stage_enemy_center},
		assets::decode_pi
	);
	if(!center) {
		return std::unexpected(center.error());
	}
	const MainResourceKey opponent_key{
		.role = MainResourceRole::character_stage_enemy_group,
		.character = config.player_two.character,
	};
	auto opponent = decode_asset<assets::PiImage>(
		resources,
		opponent_key,
		assets::decode_pi
	);
	if(!opponent) {
		return std::unexpected(opponent.error());
	}
	if(!opponent->palette) {
		return std::unexpected(process_error(
			MainProcessErrorCode::pi_palette_missing,
			opponent_key
		));
	}
	present::Raster handoff_raster{};
	if(!present::main_prepare_sprite16_pi_source_sheet(
		handoff_raster,
		{
			.player_shots = {{
				&player_shots[0U],
				&player_shots[1U],
			}},
			.player_extra_shots = {{
				&player_extra_shots[0U],
				&player_extra_shots[1U],
			}},
			.enemy_center = &*center,
			.opponent_enemies = &*opponent,
		}
	)) {
		return std::unexpected(process_error(
			MainProcessErrorCode::pi_staging_failed
		));
	}
	for(std::size_t index = 0U;
		index < opponent->palette->colors.size(); ++index) {
		if(!handoff_raster.set_pending_palette_entry(
			static_cast<std::uint8_t>(index),
			opponent->palette->colors[index]
		)) {
			return std::unexpected(process_error(
				MainProcessErrorCode::pi_palette_missing,
				opponent_key
			));
		}
	}
	handoff_raster.set_palette_tone(100);
	handoff_raster.apply_pending_palette();
	const MainStageHandoff handoff(std::move(handoff_raster));
	return create_common(
		direct_game_init(config),
		config.game_options,
		config.kotohime_compatibility,
		selections,
		handoff,
		resources,
		*static_content,
		0U,
		std::nullopt
	);
}

std::expected<std::unique_ptr<MainProcess>, MainProcessError>
MainProcessStoryAccess::create_story(
	const MainStoryConfig &config,
	ClassicMainlStageTransition &transition,
	std::span<const LoadedMainResource> resources,
	std::shared_ptr<const MainProcessStaticContent> static_content,
	bool authenticate_resource_bytes
)
{
	if(!valid_main_story_config(config) || !static_content ||
		transition.phase() != ClassicMainlStagePhase::ready ||
		transition.handoff() == nullptr ||
		!transition.matches_process_config(config)) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_config
		));
	}
	const auto player_one = main_player_selection_from_paletted(
		config.resident.paletted_character[0U]
	);
	const auto player_two = main_player_selection_from_paletted(
		config.resident.paletted_character[1U]
	);
	if(!player_one || !player_two) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_config
		));
	}
	const auto expected = main_private_resource_keys(*player_one, *player_two);
	if(!expected) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_config
		));
	}
	if(const auto valid = validate_resource_set(
		resources,
		*expected,
		authenticate_resource_bytes
	); !valid) {
		return std::unexpected(valid.error());
	}
	const auto continues_used_digit = static_cast<std::uint8_t>(
		3U - config.resident.remaining_credits
	);
	const auto gameplay_song = classic_main_gameplay_song(config.resident);
	if(!gameplay_song) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_config
		));
	}
	auto process = MainProcess::create_common(
		config.resident,
		config.game_options,
		config.kotohime_compatibility,
		{*player_one, *player_two},
		*transition.handoff(),
		resources,
		std::move(static_content),
		continues_used_digit,
		gameplay_song
	);
	if(process) {
		transition.consume_handoff();
	}
	return process;
}

std::expected<std::unique_ptr<MainProcess>, MainProcessError>
MainProcessClassicVsAccess::create_vs(
	ClassicMainlStageTransition &transition,
	std::span<const LoadedMainResource> resources,
	std::shared_ptr<const MainProcessStaticContent> static_content,
	bool authenticate_resource_bytes
)
{
	if(!static_content ||
		transition.phase() != ClassicMainlStagePhase::ready ||
		transition.handoff() == nullptr || !transition.is_classic_vs_entry() ||
		!transition.prepared_) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_config
		));
	}
	const auto &entry = transition.prepared_->entry;
	const auto player_one = main_player_selection_from_paletted(
		entry.resident.paletted_character[0U]
	);
	const auto player_two = main_player_selection_from_paletted(
		entry.resident.paletted_character[1U]
	);
	if(!player_one || !player_two) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_config
		));
	}
	const auto expected = main_private_resource_keys(*player_one, *player_two);
	if(!expected) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_config
		));
	}
	if(const auto valid = validate_resource_set(
		resources,
		*expected,
		authenticate_resource_bytes
	); !valid) {
		return std::unexpected(valid.error());
	}
	const auto gameplay_song = classic_main_gameplay_song(entry.resident);
	if(!gameplay_song) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_config
		));
	}
	auto process = MainProcess::create_common(
		entry.resident,
		entry.game_options,
		entry.kotohime_compatibility,
		{*player_one, *player_two},
		*transition.handoff(),
		resources,
		std::move(static_content),
		std::uint8_t{0U},
		gameplay_song
	);
	if(process) {
		transition.consume_handoff();
	}
	return process;
}

std::expected<std::unique_ptr<MainProcess>, MainProcessError>
MainProcessDemoAccess::create_demo(
	ClassicMainlStageTransition &transition,
	std::span<const LoadedMainResource> resources,
	std::shared_ptr<const MainProcessStaticContent> static_content,
	bool authenticate_resource_bytes
)
{
	if(!static_content ||
		transition.phase() != ClassicMainlStagePhase::ready ||
		transition.handoff() == nullptr || !transition.is_demo_entry() ||
		!transition.prepared_) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_config
		));
	}
	const auto &entry = transition.prepared_->entry;
	const auto player_one = main_player_selection_from_paletted(
		entry.resident.paletted_character[0U]
	);
	const auto player_two = main_player_selection_from_paletted(
		entry.resident.paletted_character[1U]
	);
	if(!player_one || !player_two) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_config
		));
	}
	const auto expected = main_private_resource_keys(*player_one, *player_two);
	if(!expected) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_config
		));
	}
	if(const auto valid = validate_resource_set(
		resources,
		*expected,
		authenticate_resource_bytes
	); !valid) {
		return std::unexpected(valid.error());
	}
	const auto gameplay_song = classic_main_gameplay_song(entry.resident);
	if(!gameplay_song) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_config
		));
	}
	auto process = MainProcess::create_common(
		entry.resident,
		entry.game_options,
		entry.kotohime_compatibility,
		{*player_one, *player_two},
		*transition.handoff(),
		resources,
		std::move(static_content),
		std::uint8_t{0U},
		gameplay_song
	);
	if(process) {
		transition.consume_handoff();
	}
	return process;
}

std::expected<std::unique_ptr<MainProcess>, MainProcessError>
MainProcess::create_common(
	const GameInit &init,
	GameOptions game_options,
	present::KotohimeBombFadeCompatibility kotohime_compatibility,
	std::array<MainPlayerSelection, 2U> selections,
	const MainStageHandoff &handoff,
	std::span<const LoadedMainResource> resources,
	std::shared_ptr<const MainProcessStaticContent> static_content,
	std::uint8_t continues_used_digit,
	std::optional<ClassicAudioCommand> gameplay_song
)
{
	if(!static_content || continues_used_digit > 3U ||
		(gameplay_song && (gameplay_song->type !=
			ClassicAudioCommandType::load_song || gameplay_song->cue !=
			ClassicMusicCue::gameplay))) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_config
		));
	}
	auto impl = std::make_unique<Impl>();
	impl->input_protocol = game_options.input_protocol;
	impl->continues_used_digit = continues_used_digit;
	impl->static_content = std::move(static_content);
	impl->classic_audio_lifecycle = gameplay_song.has_value();
	const auto superimpose_decoder = [](std::span<const std::byte> bytes) {
		return assets::decode_bfnt(
			bytes,
			assets::BfntConsumerPolicy::superimpose
		);
	};
	for(std::size_t player = 0U; player < selections.size(); ++player) {
		auto character = decode_asset<assets::BfntSheet>(
			resources,
			player_key(MainResourceRole::character_sheet, selections[player]),
			superimpose_decoder
		);
		if(!character) {
			return std::unexpected(character.error());
		}
		impl->character_sheets[player] = std::move(*character);
		auto extra = decode_asset<assets::MrsImage>(
			resources,
			player_key(
				MainResourceRole::character_extra_portrait,
				selections[player]
			),
			assets::decode_mrs
		);
		if(!extra) {
			return std::unexpected(extra.error());
		}
		impl->extra_portraits[player] = std::move(*extra);
		auto bomb = decode_asset<assets::MrsImage>(
			resources,
			player_key(
				MainResourceRole::character_bomb_portrait,
				selections[player]
			),
			assets::decode_mrs
		);
		if(!bomb) {
			return std::unexpected(bomb.error());
		}
		impl->bomb_portraits[player] = std::move(*bomb);
	}
	assets::horizontal_flip(impl->extra_portraits[1U]);
	assets::horizontal_flip(impl->bomb_portraits[1U]);
	impl->raster = handoff.raster_;
	impl->static_content->gameft_gaiji().install(impl->tram);

	const auto trig = impl->static_content->trig();
	const auto &staging = impl->static_presenter.sprite_staging();
	impl->background.emplace(
		impl->raster,
		staging,
		present::authentic_main_background_atlas_layout()
	);
	if(!impl->background->ready()) {
		return std::unexpected(process_error(
			MainProcessErrorCode::presenter_setup_failed
		));
	}
	auto game = Game::create(
		init,
		impl->static_content->game_content(),
		game_options,
		&*impl->background
	);
	if(!game) {
		return std::unexpected(core_error(
			MainProcessErrorCode::game_create_failed,
			std::move(game.error())
		));
	}
	impl->game.emplace(std::move(*game));
	if(!impl->background->initialization_complete()) {
		return std::unexpected(process_error(
			MainProcessErrorCode::background_initialization_failed
		));
	}
	const auto initial_hud = hud_state_from_round(
		impl->game->round_start_observation()
	);
	if(!initial_hud) {
		return std::unexpected(process_error(
			MainProcessErrorCode::game_create_failed
		));
	}
	if(!impl->static_presenter.round_startup(
		impl->raster,
		impl->tram,
		*initial_hud,
		{
			.player_one = &impl->character_sheets[0U],
			.player_two = &impl->character_sheets[1U],
		},
		impl->palette
	)) {
		return std::unexpected(process_error(
			MainProcessErrorCode::static_startup_failed
		));
	}
	impl->bosses.emplace(impl->raster, staging, trig, impl->palette);
	impl->entities.emplace(
		impl->raster,
		staging,
		present::authentic_main_entity_atlas_layout()
	);
	impl->effects.emplace(
		impl->raster,
		staging,
		present::authentic_main_effects_atlas_layout(),
		trig
	);
	impl->extra_charge.emplace(impl->raster, staging, trig);
	impl->players_bullets.emplace(
		impl->raster,
		staging,
		present::authentic_main_players_bullets_atlas_layout(),
		present::MainPlayerOverlayAssets{
			.character_sheets = {{
				&impl->character_sheets[0U],
				&impl->character_sheets[1U],
			}},
			.loaded_characters = {{
				static_cast<present::MainPalettedCharacter>(
					*main_paletted_character(selections[0U])
				),
				static_cast<present::MainPalettedCharacter>(
					*main_paletted_character(selections[1U])
				),
			}},
			.gauge_sheet = &impl->static_content->zikicw_sheet(),
		},
		impl->static_content->compiled().pellet_masks(),
		trig
	);
	impl->bombs.emplace(
		impl->raster,
		staging,
		present::authentic_main_bomb_atlas_layout(),
		present::MainBombMrsAssets{{
			&impl->bomb_portraits[0U],
			&impl->bomb_portraits[1U],
		}},
		trig,
		impl->palette,
		kotohime_compatibility
	);
	impl->warning_gauge.emplace(
		impl->raster,
		impl->tram,
		staging,
		impl->palette,
		present::MainWarningMrsAssets{{
			&impl->extra_portraits[0U],
			&impl->extra_portraits[1U],
		}}
	);
	impl->hud_result.emplace(
		impl->raster,
		impl->tram,
		impl->static_content->compiled().score_font_masks(),
		trig,
		present::MainHudResultSheets{
			.lose = &impl->static_content->lose(),
			.round = &impl->static_content->round(),
			.zikicw = &impl->static_content->zikicw(),
		},
		impl->static_content->compiled().hud_result_tram_assets(),
		impl->continues_used_digit
	);
	if(!impl->background->ready() || !impl->bosses->ready() ||
		!impl->entities->ready() || !impl->effects->ready() ||
		!impl->extra_charge->ready() || !impl->players_bullets->ready() ||
		!impl->bombs->ready() || !impl->warning_gauge->ready() ||
		!impl->hud_result->ready()) {
		return std::unexpected(process_error(
			MainProcessErrorCode::presenter_setup_failed
		));
	}
	impl->round.emplace(
		impl->raster,
		impl->tram,
		impl->static_presenter,
		impl->palette,
		*impl->background,
		*impl->bosses,
		*impl->entities,
		*impl->effects,
		*impl->extra_charge,
		*impl->players_bullets,
		*impl->bombs,
		*impl->warning_gauge,
		*impl->hud_result,
		present::MainFeedbackSink{
			.context = impl.get(),
			.request = &Impl::request_feedback_callback,
			.dispatch = &Impl::feedback_dispatch_callback,
		}
	);
	if(!impl->round->ready()) {
		return std::unexpected(presentation_error(impl->round->status()));
	}
	if(gameplay_song) {
		auto play = *gameplay_song;
		play.type = ClassicAudioCommandType::play_song;
		if(!impl->push_audio({
			.type = ClassicAudioCommandType::reset_effects,
		}) || !impl->push_audio(play)) {
			return std::unexpected(process_error(
				MainProcessErrorCode::audio_command_pending
			));
		}
	}
	return std::unique_ptr<MainProcess>(
		new MainProcess(std::move(impl))
	);
}

MainProcess::MainProcess(std::unique_ptr<Impl> impl) noexcept :
	impl_(std::move(impl))
{}

MainProcess::~MainProcess() = default;

void MainProcess::fail_presentation_for_test() noexcept
{
	// A footer is impossible at the freshly armed source-prefix phase. This
	// deliberately latches the coordinator's existing invalid-order status
	// without drawing or exposing an implementation child to the test.
	impl_->round->main_frame_footer_reached({});
}

std::optional<MainProcessError>
MainProcess::presentation_precheck() const noexcept
{
	if(!impl_->round->ready()) {
		return presentation_error(impl_->round->status());
	}
	return std::nullopt;
}

std::expected<StepResult, MainProcessError> MainProcess::step(
	CoreInput input,
	InputContext context
) noexcept
{
	if(const auto error = presentation_precheck()) {
		return std::unexpected(*error);
	}
	if(context != InputContext::gameplay &&
		context != InputContext::interstitial) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_runtime_argument
		));
	}
	const auto result = impl_->game->step(input, context, &*impl_->round);
	if(impl_->audio_feedback_failed) {
		return std::unexpected(process_error(
			MainProcessErrorCode::audio_command_pending
		));
	}
	if(!impl_->round->ready()) {
		return std::unexpected(presentation_error(impl_->round->status()));
	}
	return result;
}

std::expected<StepResult, MainProcessError>
MainProcess::finish_deferred_frame(DeferredFrameAction action) noexcept
{
	if(const auto error = presentation_precheck()) {
		return std::unexpected(*error);
	}
	if(action != DeferredFrameAction::resume &&
		action != DeferredFrameAction::exit_session) {
		return std::unexpected(process_error(
			MainProcessErrorCode::invalid_runtime_argument
		));
	}
	auto result = impl_->game->finish_deferred_frame(action, &*impl_->round);
	if(!result) {
		return std::unexpected(core_error(
			MainProcessErrorCode::core_operation_failed,
			std::move(result.error())
		));
	}
	if(impl_->audio_feedback_failed) {
		return std::unexpected(process_error(
			MainProcessErrorCode::audio_command_pending
		));
	}
	if(!impl_->round->ready()) {
		return std::unexpected(presentation_error(impl_->round->status()));
	}
	return *result;
}

std::expected<void, MainProcessError> MainProcess::start_next_round() noexcept
{
	if(const auto error = presentation_precheck()) {
		return std::unexpected(*error);
	}
	if(impl_->classic_audio_lifecycle &&
		impl_->audio_count >= impl_->audio_commands.size()) {
		return std::unexpected(process_error(
			MainProcessErrorCode::audio_command_pending
		));
	}
	const auto started = impl_->game->start_next_round();
	if(!started) {
		return std::unexpected(core_error(
			MainProcessErrorCode::core_operation_failed,
			started.error()
		));
	}
	const auto hud = hud_state_from_round(impl_->game->round_start_observation());
	if(!hud) {
		return std::unexpected(process_error(
			MainProcessErrorCode::repeat_startup_failed
		));
	}
	if(!impl_->round->repeat_round_startup(*hud)) {
		if(!impl_->round->ready()) {
			return std::unexpected(presentation_error(impl_->round->status()));
		}
		return std::unexpected(process_error(
			MainProcessErrorCode::repeat_startup_failed
		));
	}
	impl_->feedback_scheduler.reset();
	if(impl_->classic_audio_lifecycle && !impl_->push_audio({
		.type = ClassicAudioCommandType::reset_effects,
	})) {
		return std::unexpected(process_error(
			MainProcessErrorCode::audio_command_pending
		));
	}
	return {};
}

std::expected<void, MainProcessError>
MainProcess::finish_main_process() noexcept
{
	if(const auto error = presentation_precheck()) {
		return std::unexpected(*error);
	}
	if(impl_->classic_audio_lifecycle &&
		impl_->audio_count >= impl_->audio_commands.size()) {
		return std::unexpected(process_error(
			MainProcessErrorCode::audio_command_pending
		));
	}
	const auto finished = impl_->game->finish_main_process();
	if(!finished) {
		return std::unexpected(core_error(
			MainProcessErrorCode::core_operation_failed,
			finished.error()
		));
	}
	if(impl_->classic_audio_lifecycle && !impl_->push_audio({
		.type = ClassicAudioCommandType::stop_song,
	})) {
		return std::unexpected(process_error(
			MainProcessErrorCode::audio_command_pending
		));
	}
	return {};
}

std::expected<SessionTransition, MainProcessError>
MainProcess::session_transition(
	std::optional<ContinueChoice> continue_choice
) const
{
	if(const auto error = presentation_precheck()) {
		return std::unexpected(*error);
	}
	auto transition = impl_->game->session_transition(continue_choice);
	if(!transition) {
		return std::unexpected(core_error(
			MainProcessErrorCode::core_operation_failed,
			std::move(transition.error())
		));
	}
	return std::move(*transition);
}

CanonicalState MainProcess::canonical_state() const
{
	return impl_->game->canonical_state();
}

RoundStartObservation MainProcess::round_start_observation() const
{
	return impl_->game->round_start_observation();
}

InputProtocol MainProcess::input_protocol() const noexcept
{
	return impl_->input_protocol;
}

std::expected<present::SourceSurface, present::SourceSurfaceError>
MainProcess::compose_shown_surface(
	std::span<present::ComposedPixel, present::graphics_page_pixels> output
) const
{
	return impl_->round->compose_shown_surface(output, impl_->static_content->font());
}

const present::Raster &MainProcess::raster() const noexcept
{
	return impl_->raster;
}

const present::Tram &MainProcess::tram() const noexcept
{
	return impl_->tram;
}

present::MainRoundStatus MainProcess::presentation_status() const noexcept
{
	return impl_->round->status();
}

std::optional<ClassicAudioCommand> MainProcess::take_audio_command() noexcept
{
	if(impl_->audio_count == 0U) {
		return std::nullopt;
	}
	const auto command = impl_->audio_commands[impl_->audio_head];
	impl_->audio_head = (impl_->audio_head + 1U) % impl_->audio_commands.size();
	--impl_->audio_count;
	return command;
}

} // namespace th03::app
