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

#include "th03/app/classic_config.hpp"
#include "th03/assets/decode.hpp"
#include "th03/exact/trig.hpp"
#include "th03/present/cels.hpp"

namespace th03::app {
namespace {

constexpr std::uint16_t input_up = 0x0001U;
constexpr std::uint16_t input_down = 0x0002U;
constexpr std::uint16_t input_bomb = 0x0010U;
constexpr std::uint16_t input_shot = 0x0020U;
constexpr std::uint16_t input_cancel = 0x1000U;
constexpr std::uint16_t valid_input_bits = 0x7F3FU;
constexpr std::uint16_t initial_delay_regular = 30U;
constexpr std::uint16_t initial_delay_two_player = 46U;
constexpr std::uint16_t ordinary_frame_vsyncs = 3U;
constexpr std::uint8_t confirmation_vsyncs = 18U;
constexpr present::Rect full_screen{
	0, 0, present::graphics_width, present::graphics_height,
};

using ResourceResult = std::expected<
	const LoadedMainResource *,
	ClassicOpSelectionError
>;

[[nodiscard]] ClassicOpSelectionError selection_error(
	ClassicOpSelectionErrorCode code,
	std::optional<MainResourceKey> resource = std::nullopt
) noexcept
{
	return {.code = code, .resource = resource};
}

[[nodiscard]] ClassicOpSelectionError decode_error(
	MainResourceKey key,
	assets::AssetError error
)
{
	auto result = selection_error(
		ClassicOpSelectionErrorCode::asset_decode_failed, key
	);
	result.asset_error.emplace(std::move(error));
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

[[nodiscard]] std::expected<void, ClassicOpSelectionError>
validate_resource_set(
	std::span<const LoadedMainResource> resources,
	bool authenticate_resource_bytes
)
{
	const auto expected = op_character_selection_resource_keys();
	if(resources.size() != expected.size()) {
		return std::unexpected(selection_error(
			ClassicOpSelectionErrorCode::resource_set_mismatch
		));
	}
	for(std::size_t index = 0U; index < expected.size(); ++index) {
		const auto key = expected[index];
		const auto &resource = resources[index];
		if(resource.spec.key != key) {
			return std::unexpected(selection_error(
				ClassicOpSelectionErrorCode::resource_set_mismatch, key
			));
		}
		if(!resource.bytes) {
			return std::unexpected(selection_error(
				ClassicOpSelectionErrorCode::resource_bytes_missing, key
			));
		}
		const auto canonical = main_resource_spec(key);
		if(!canonical || !same_resource_spec(resource.spec, *canonical)) {
			return std::unexpected(selection_error(
				ClassicOpSelectionErrorCode::resource_set_mismatch, key
			));
		}
		if(!resource.authenticated_override &&
			(resource.bytes->size() != canonical->expected_bytes ||
			(authenticate_resource_bytes && main_resource_fnv1a64(
				*resource.bytes
			) != canonical->expected_fnv1a64))) {
			return std::unexpected(selection_error(
				ClassicOpSelectionErrorCode::resource_integrity_failed, key
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
	return std::unexpected(selection_error(
		ClassicOpSelectionErrorCode::resource_set_mismatch, key
	));
}

template <typename Value, typename Decoder>
[[nodiscard]] std::expected<Value, ClassicOpSelectionError> decode_asset(
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

[[nodiscard]] bool valid_cel(
	const assets::PlanarCel4 &cel,
	std::uint16_t width,
	std::uint16_t height,
	bool alpha
) noexcept
{
	const auto bytes_per_row = static_cast<std::size_t>((width + 7U) / 8U);
	const auto plane_size = bytes_per_row * height;
	if(cel.pixel_width != width || cel.pixel_height != height ||
		cel.row_order != assets::PlanarRowOrder::bottom_to_top ||
		cel.bit_order != assets::PlanarBitOrder::most_significant_bit_left ||
		cel.bitplane_size != plane_size ||
		cel.offset_at_bottom_left != static_cast<std::int16_t>(
			static_cast<std::size_t>(height - 1U) * 80U
		) || cel.vram_dword_width != (width / 32U) ||
		(alpha != cel.alpha.has_value()) ||
		(alpha && cel.alpha->size() != plane_size)) {
		return false;
	}
	for(const auto &plane : cel.color_planes) {
		if(plane.size() != plane_size) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] bool install_palette(
	present::Raster &raster,
	const assets::Palette16Rgb4 &palette
) noexcept
{
	for(std::size_t index = 0U; index < palette.colors.size(); ++index) {
		if(!raster.set_pending_palette_entry(
			static_cast<std::uint8_t>(index), palette.colors[index]
		)) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] constexpr std::uint8_t character_index(
	MainCharacter character
) noexcept
{
	return static_cast<std::uint8_t>(
		static_cast<std::uint8_t>(character) - 1U
	);
}

[[nodiscard]] constexpr std::uint8_t palette_index(
	MainPaletteVariant palette
) noexcept
{
	return (palette == MainPaletteVariant::first) ? 0U : 1U;
}

[[nodiscard]] constexpr bool is_cpu_selection_mode(
	ClassicOpSelectionMode mode
) noexcept
{
	return mode == ClassicOpSelectionMode::vs_1p_cpu ||
		mode == ClassicOpSelectionMode::vs_cpu_cpu;
}

[[nodiscard]] constexpr bool is_vs_mode(
	ClassicOpSelectionMode mode
) noexcept
{
	return mode != ClassicOpSelectionMode::story;
}

[[nodiscard]] constexpr std::uint8_t game_mode_for(
	ClassicOpSelectionMode mode
) noexcept
{
	switch(mode) {
	case ClassicOpSelectionMode::story: return game_mode::story;
	case ClassicOpSelectionMode::vs_1p_cpu: return game_mode::vs_1p_cpu;
	case ClassicOpSelectionMode::vs_1p_2p: return game_mode::vs_1p_2p;
	case ClassicOpSelectionMode::vs_cpu_cpu: return game_mode::vs_cpu_cpu;
	}
	return game_mode::none;
}

[[nodiscard]] constexpr std::uint16_t player_input(
	ClassicOpSelectionMode mode,
	std::size_t player,
	const ClassicOpSelectionInput &input
) noexcept
{
	if(mode == ClassicOpSelectionMode::vs_1p_2p) {
		return (player == 0U) ? input.player_one : input.player_two;
	}
	return input.single_player;
}

[[nodiscard]] ClassicOpSelectionInput normalize_selection_input(
	ClassicOpSelectionMode mode,
	std::uint8_t key_mode,
	ClassicOpSelectionInput input
) noexcept
{
	if(!input.physical_domains) {
		return input;
	}
	const auto &physical = *input.physical_domains;
	auto interface_word = static_cast<std::uint16_t>(
		physical.keyboard.system | physical.keyboard.p1
	);
	if(physical.first_joystick_present) {
		interface_word = static_cast<std::uint16_t>(
			interface_word | physical.first_joystick
		);
	}
	input.single_player = interface_word;
	input.player_one = physical.keyboard.p1;
	input.player_two = physical.keyboard.p2;
	if(mode != ClassicOpSelectionMode::vs_1p_2p ||
		!physical.first_joystick_present) {
		return input;
	}
	switch(static_cast<ClassicKeyMode>(key_mode)) {
	case ClassicKeyMode::joy_key:
		input.player_one = physical.first_joystick;
		input.player_two = physical.keyboard.system;
		break;
	case ClassicKeyMode::key_joy:
		input.player_one = physical.keyboard.system;
		input.player_two = physical.first_joystick;
		break;
	case ClassicKeyMode::key_key:
		break;
	}
	return input;
}

[[nodiscard]] constexpr std::uint8_t ring_selection_up(
	std::uint8_t value,
	std::uint8_t count
) noexcept
{
	return (value == 0U) ? static_cast<std::uint8_t>(count - 1U) :
		static_cast<std::uint8_t>(value - 1U);
}

[[nodiscard]] constexpr std::uint8_t ring_selection_down(
	std::uint8_t value,
	std::uint8_t count
) noexcept
{
	const auto incremented = static_cast<std::uint8_t>(value + 1U);
	return (incremented >= count) ? 0U : incremented;
}

[[nodiscard]] constexpr std::uint8_t paletted_value(
	std::uint8_t base,
	MainPaletteVariant palette
) noexcept
{
	return static_cast<std::uint8_t>(
		(base * 2U) + ((palette == MainPaletteVariant::first) ? 1U : 2U)
	);
}

[[nodiscard]] std::uint8_t scaled_angle(
	std::uint8_t base,
	std::int16_t offset,
	std::uint16_t frequency
) noexcept
{
	const auto angle = static_cast<std::uint8_t>(
		static_cast<std::int16_t>(base) + offset
	);
	const auto product = static_cast<std::uint16_t>(
		static_cast<std::uint16_t>(angle) * frequency
	);
	auto signed_product = std::bit_cast<std::int16_t>(product);
	if(signed_product < 0) {
		signed_product = static_cast<std::int16_t>(signed_product + 255);
	}
	return static_cast<std::uint8_t>(signed_product / 256);
}

[[nodiscard]] std::uint16_t irand(std::uint32_t &seed) noexcept
{
	seed = (seed * 0x015A4E35U) + 1U;
	return static_cast<std::uint16_t>((seed >> 16U) & 0x7FFFU);
}

} // namespace

GameInit prepare_new_classic_story_restart(GameInit resident) noexcept
{
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
	resident.demo_number = 0U;
	resident.winner = 0;
	resident.story_stage = 0U;
	resident.is_cpu = {0U, 1U};
	resident.game_mode = game_mode::story;
	resident.story_lives = 2U;
	resident.show_score_menu = 0U;
	resident.paletted_character[1U] = -1;

	const auto player_one = static_cast<std::uint8_t>(
		resident.paletted_character[0U]
	);
	const auto player_base = static_cast<std::uint8_t>((player_one - 1U) / 2U);
	const auto fixed = character_index(fixed_stage_six[player_base]);
	std::array<bool, 7U> used{};
	used[fixed] = true;
	auto seed = std::bit_cast<std::uint32_t>(resident.resident_rand);
	for(std::size_t stage = 0U; stage < 6U; ++stage) {
		std::uint8_t candidate = 0U;
		do {
			candidate = static_cast<std::uint8_t>(irand(seed) % 7U);
		} while(used[candidate]);
		used[candidate] = true;
		resident.story_opponents[stage] = static_cast<std::int8_t>(
			paletted_value(candidate, MainPaletteVariant::first)
		);
	}
	resident.story_opponents[6U] = static_cast<std::int8_t>(
		paletted_value(fixed, MainPaletteVariant::first)
	);
	resident.story_opponents[7U] = 15;
	resident.story_opponents[8U] = 17;
	for(auto &opponent : resident.story_opponents) {
		if(opponent == resident.paletted_character[0U]) {
			++opponent;
		}
	}
	resident.paletted_character[1U] = resident.story_opponents[0U];
	resident.remaining_credits = 3U;
	resident.fast_op_animation = 0U;
	resident.skill = static_cast<std::uint8_t>(70U + (resident.rank * 25U));
	resident.last_score = {};
	resident.global_frame = 0U;
	resident.runtime_snapshot.reset();
	resident.compact_runtime_snapshot.reset();
	resident.round_start_snapshot.reset();
	resident.practice_rules.reset();
	resident.random_seed = std::bit_cast<std::uint32_t>(resident.resident_rand);
	return resident;
}

GameInit prepare_new_classic_vs_restart(GameInit resident) noexcept
{
	switch(resident.game_mode) {
	case game_mode::vs_1p_cpu: resident.is_cpu = {0U, 1U}; break;
	case game_mode::vs_1p_2p: resident.is_cpu = {0U, 0U}; break;
	case game_mode::vs_cpu_cpu: resident.is_cpu = {1U, 1U}; break;
	default: return {};
	}
	resident.demo_number = 0U;
	resident.winner = 0;
	resident.story_stage = 0U;
	resident.story_lives = 0U;
	resident.show_score_menu = 0U;
	resident.last_score = {};
	resident.global_frame = 0U;
	resident.runtime_snapshot.reset();
	resident.compact_runtime_snapshot.reset();
	resident.round_start_snapshot.reset();
	resident.random_seed = std::bit_cast<std::uint32_t>(resident.resident_rand);
	return resident;
}

ClassicOpSelectionCompletion::ClassicOpSelectionCompletion(
	ClassicOpSelectionOutcome outcome,
	ClassicOpSelectionMode mode,
	GameInit resident,
	ClassicOpSelectionCarry carry,
	present::Raster raster,
	present::Tram tram
) noexcept :
	outcome_(outcome),
	mode_(mode),
	resident_(std::move(resident)),
	carry_(carry),
	raster_(std::move(raster)),
	tram_(std::move(tram))
{}

ClassicOpSelectionOutcome ClassicOpSelectionCompletion::outcome() const noexcept
{
	return outcome_;
}

ClassicOpSelectionMode ClassicOpSelectionCompletion::mode() const noexcept
{
	return mode_;
}

const GameInit &ClassicOpSelectionCompletion::resident() const noexcept
{
	return resident_;
}

const ClassicOpSelectionCarry &ClassicOpSelectionCompletion::carry() const noexcept
{
	return carry_;
}

const present::Raster &ClassicOpSelectionCompletion::raster() const noexcept
{
	return raster_;
}

const present::Tram &ClassicOpSelectionCompletion::tram() const noexcept
{
	return tram_;
}

ClassicOpCharacterSelection::ClassicOpCharacterSelection(
	std::unique_ptr<Prepared> prepared,
	GameInit resident,
	ClassicOpSelectionMode mode,
	std::uint8_t available_character_count,
	ClassicOpSelectionCarry carry,
	present::Raster raster,
	present::Tram tram,
	bool retain_for_new_classic_practice
) noexcept :
	prepared_(std::move(prepared)),
	resident_(std::move(resident)),
	mode_(mode),
	available_character_count_(available_character_count),
	carry_(carry),
	raster_(std::move(raster)),
	tram_(std::move(tram)),
	retain_for_new_classic_practice_(retain_for_new_classic_practice)
{
	resident_.demo_number = 0U;
	resident_.winner = 0;
	resident_.story_stage = 0U;
	resident_.game_mode = game_mode_for(mode_);
	resident_.show_score_menu = 0U;
	if(mode_ == ClassicOpSelectionMode::story) {
		resident_.is_cpu = {0U, 1U};
		resident_.story_lives = 2U;
		resident_.paletted_character[1U] = -1;
		selection_ = {0U, 127U};
		palette_ = {MainPaletteVariant::first, MainPaletteVariant::first};
		confirmed_[1U] = true;
	} else {
		resident_.is_cpu = {
			static_cast<std::uint8_t>(
				mode_ == ClassicOpSelectionMode::vs_cpu_cpu
			),
			static_cast<std::uint8_t>(
				mode_ != ClassicOpSelectionMode::vs_1p_2p
			),
		};
		for(std::size_t player = 0U; player < selection_.size(); ++player) {
			const auto raw = static_cast<std::uint8_t>(
				resident_.paletted_character[player]
			);
			selection_[player] = static_cast<std::uint8_t>((raw - 1U) / 2U);
			palette_[player] = ((raw & 1U) != 0U) ?
				MainPaletteVariant::first : MainPaletteVariant::second;
		}
	}
	static_cast<void>(push_audio_command({
		.type = ClassicAudioCommandType::stop_song,
	}));
	static_cast<void>(push_audio_command({
		.type = ClassicAudioCommandType::load_song,
		.cue = ClassicMusicCue::character_select,
	}));
	static_cast<void>(push_audio_command({
		.type = ClassicAudioCommandType::play_song,
		.cue = ClassicMusicCue::character_select,
	}));
	if(!raster_.set_raster_mode(present::RasterMode::lines_400) ||
		!raster_.set_access_page(present::Page::one) ||
		!raster_.clear(full_screen, 0U)) {
		fail();
	}
}

std::expected<std::unique_ptr<ClassicOpCharacterSelection>, ClassicOpSelectionError>
ClassicOpCharacterSelection::create_from_op(
	const ClassicOpCompletion &completion,
	ClassicOpSelectionMode mode,
	std::uint8_t available_character_count,
	ClassicOpSelectionCarry carry,
	std::span<const LoadedMainResource> resources,
	bool retain_for_new_classic_practice
)
{
	const auto expected_route = (mode == ClassicOpSelectionMode::story) ?
		ClassicOpRoute::story : ClassicOpRoute::versus;
	if(completion.route() != expected_route ||
		completion.selection_mode() != mode) {
		return std::unexpected(selection_error(
			ClassicOpSelectionErrorCode::invalid_config
		));
	}
	auto resident = completion.resident();
	if(expected_route == ClassicOpRoute::versus &&
		completion.resets_vs_selection()) {
		resident.paletted_character = {1, 1};
	}
	return create_impl(
		std::move(resident), mode, available_character_count, carry,
		completion.raster(), completion.tram(), resources, true,
		retain_for_new_classic_practice
	);
}

std::expected<std::unique_ptr<ClassicOpCharacterSelection>, ClassicOpSelectionError>
ClassicOpCharacterSelection::create_impl(
	GameInit resident,
	ClassicOpSelectionMode mode,
	std::uint8_t available_character_count,
	ClassicOpSelectionCarry carry,
	present::Raster raster,
	present::Tram tram,
	std::span<const LoadedMainResource> resources,
	bool authenticate_resource_bytes,
	bool retain_for_new_classic_practice
)
{
	if((available_character_count != 7U && available_character_count != 9U) ||
		resident.rank > 3U ||
		static_cast<std::uint8_t>(mode) >
			static_cast<std::uint8_t>(ClassicOpSelectionMode::vs_cpu_cpu)) {
		return std::unexpected(selection_error(
			ClassicOpSelectionErrorCode::invalid_config
		));
	}
	if(is_vs_mode(mode)) {
		for(const auto raw : resident.paletted_character) {
			if(raw < 1 || raw > 18) {
				return std::unexpected(selection_error(
					ClassicOpSelectionErrorCode::invalid_config
				));
			}
		}
	}
	if(const auto valid = validate_resource_set(
		resources, authenticate_resource_bytes
	); !valid) {
		return std::unexpected(valid.error());
	}

	auto prepared = std::make_unique<Prepared>();
	for(std::uint8_t character = 0U; character < 9U; ++character) {
		const MainResourceKey key{
			.role = MainResourceRole::character_selection_portrait,
			.character = static_cast<MainCharacter>(character + 1U),
		};
		auto file = decode_asset<assets::CdgFile>(
			resources, key, [](std::span<const std::byte> bytes) {
				return assets::decode_cdg(bytes);
			}
		);
		if(!file) {
			return std::unexpected(file.error());
		}
		if(file->cels.size() != 2U ||
			!valid_cel(file->cels[0U], 192U, 192U, true) ||
			!valid_cel(file->cels[1U], 192U, 192U, true)) {
			return std::unexpected(selection_error(
				ClassicOpSelectionErrorCode::invalid_asset_geometry, key
			));
		}
		prepared->portraits[character][0U] = std::move(file->cels[0U]);
		prepared->portraits[character][1U] = std::move(file->cels[1U]);
	}

	const auto decode_cdg_role = [&](
		MainResourceRole role,
		assets::CdgAlphaPolicy alpha_policy
	) -> std::expected<assets::CdgFile, ClassicOpSelectionError> {
		return decode_asset<assets::CdgFile>(
			resources, {.role = role}, [alpha_policy](
				std::span<const std::byte> bytes
			) { return assets::decode_cdg(bytes, alpha_policy); }
		);
	};
	auto story = decode_cdg_role(
		MainResourceRole::selection_story_portrait,
		assets::CdgAlphaPolicy::preserve
	);
	auto stats = decode_cdg_role(
		MainResourceRole::selection_stats_sheet,
		assets::CdgAlphaPolicy::discard
	);
	auto extra_background = decode_cdg_role(
		MainResourceRole::selection_extra_background,
		assets::CdgAlphaPolicy::discard
	);
	auto extra_icons = decode_cdg_role(
		MainResourceRole::selection_extra_icons,
		assets::CdgAlphaPolicy::discard
	);
	if(!story) { return std::unexpected(story.error()); }
	if(!stats) { return std::unexpected(stats.error()); }
	if(!extra_background) {
		return std::unexpected(extra_background.error());
	}
	if(!extra_icons) { return std::unexpected(extra_icons.error()); }
	if(story->cels.size() != 1U ||
		!valid_cel(story->cels[0U], 192U, 192U, true) ||
		stats->cels.size() != 1U ||
		!valid_cel(stats->cels[0U], 128U, 64U, false) ||
		extra_background->cels.size() != 1U ||
		!valid_cel(extra_background->cels[0U], 64U, 64U, false) ||
		extra_icons->cels.size() != 9U) {
		return std::unexpected(selection_error(
			ClassicOpSelectionErrorCode::invalid_asset_geometry
		));
	}
	prepared->story_portrait = std::move(story->cels[0U]);
	prepared->stats = std::move(stats->cels[0U]);
	prepared->extra_background = std::move(extra_background->cels[0U]);
	for(std::size_t index = 0U; index < extra_icons->cels.size(); ++index) {
		if(!valid_cel(extra_icons->cels[index], 32U, 44U, false)) {
			return std::unexpected(selection_error(
				ClassicOpSelectionErrorCode::invalid_asset_geometry,
				MainResourceKey{.role = MainResourceRole::selection_extra_icons}
			));
		}
		prepared->extra_icons[index] = std::move(extra_icons->cels[index]);
	}

	auto names = decode_asset<assets::BfntSheet>(
		resources,
		{.role = MainResourceRole::selection_name_sheet},
		[](std::span<const std::byte> bytes) {
			return assets::decode_bfnt(
				bytes, assets::BfntConsumerPolicy::superimpose
			);
		}
	);
	if(!names) { return std::unexpected(names.error()); }
	prepared->names = present::prepare_superimpose_sheet(*names);
	if(!prepared->names || prepared->names->pixel_width() != 64U ||
		prepared->names->pixel_height() != 16U ||
		prepared->names->first_code() != 0U ||
		prepared->names->last_code() != 17U) {
		return std::unexpected(selection_error(
			ClassicOpSelectionErrorCode::invalid_asset_geometry,
			MainResourceKey{.role = MainResourceRole::selection_name_sheet}
		));
	}
	auto palette = decode_asset<assets::Palette16Rgb4>(
		resources,
		{.role = MainResourceRole::selection_palette},
		assets::decode_rgb48
	);
	if(!palette) { return std::unexpected(palette.error()); }
	prepared->palette = *palette;

	return create_prepared(
		std::move(prepared), std::move(resident), mode,
		available_character_count, carry, std::move(raster), std::move(tram),
		retain_for_new_classic_practice
	);
}

std::expected<std::unique_ptr<ClassicOpCharacterSelection>, ClassicOpSelectionError>
ClassicOpCharacterSelection::create_prepared(
	std::unique_ptr<Prepared> prepared,
	GameInit resident,
	ClassicOpSelectionMode mode,
	std::uint8_t available_character_count,
	ClassicOpSelectionCarry carry,
	present::Raster raster,
	present::Tram tram,
	bool retain_for_new_classic_practice
)
{
	auto result = std::unique_ptr<ClassicOpCharacterSelection>(
		new ClassicOpCharacterSelection(
			std::move(prepared), std::move(resident), mode,
			available_character_count, carry, std::move(raster), std::move(tram),
			retain_for_new_classic_practice
		)
	);
	if(result->phase_ == ClassicOpSelectionPhase::failed) {
		return std::unexpected(selection_error(
			ClassicOpSelectionErrorCode::draw_failed
		));
	}
	return result;
}

bool ClassicOpCharacterSelection::initialize_after_blank() noexcept
{
	raster_.set_palette_tone(0);
	if(!raster_.set_shown_page(present::Page::one) ||
		!raster_.set_access_page(present::Page::zero)) {
		return false;
	}
	tram_.fill({});
	if(!install_palette(raster_, prepared_->palette)) {
		return false;
	}
	raster_.set_palette_tone(100);
	raster_.apply_pending_palette();
	return true;
}

bool ClassicOpCharacterSelection::draw_curves() noexcept
{
	const auto cycle = carry_.curve_cycle;
	const auto triangle = (cycle < 128U) ?
		static_cast<std::uint16_t>(cycle) :
		static_cast<std::uint16_t>(256U - cycle);
	const auto other = static_cast<std::uint16_t>(256U + triangle);
	const auto x_frequency = static_cast<std::uint16_t>(256U + (triangle * 2U));
	const auto y_frequency = static_cast<std::uint16_t>(512U + (triangle * 2U));
	const auto put_pair = [&](std::int16_t offset, present::Index4 color) {
		const auto cycle_signed = static_cast<std::int16_t>(cycle);
		const auto outer_x_offset = static_cast<std::int16_t>(
			static_cast<std::uint8_t>(cycle_signed - (offset * 2))
		);
		const auto outer_y_offset = static_cast<std::int16_t>(
			static_cast<std::uint8_t>((cycle_signed * 2) - (offset * 4))
		);
		const auto inner_x_offset = static_cast<std::int16_t>(
			static_cast<std::uint8_t>((-cycle_signed) + (offset * 2))
		);
		const auto inner_y_offset = static_cast<std::int16_t>(
			static_cast<std::uint8_t>((cycle_signed / 2) - offset)
		);
		for(std::uint16_t angle_base = 0U; angle_base < 256U; ++angle_base) {
			const auto angle = static_cast<std::uint8_t>(angle_base);
			const auto outer_y_angle = scaled_angle(
				angle, outer_y_offset, y_frequency
			);
			const auto outer_y = static_cast<std::int32_t>(200) +
				exact::vector2(outer_y_angle, 220).y;
			if(static_cast<std::uint32_t>(outer_y) < present::graphics_height) {
				const auto outer_x_angle = scaled_angle(
					angle, outer_x_offset, other
				);
				const auto outer_x = static_cast<std::int32_t>(320) +
					exact::vector2(outer_x_angle, 220).x;
				static_cast<void>(raster_.plot({outer_x, outer_y}, color));
			}
			const auto inner_y_angle = scaled_angle(
				angle, inner_y_offset, other
			);
			const auto inner_y = static_cast<std::int32_t>(200) +
				exact::vector2(inner_y_angle, 120).y;
			if(static_cast<std::uint32_t>(inner_y) < present::graphics_height) {
				const auto inner_x_angle = scaled_angle(
					angle, inner_x_offset, x_frequency
				);
				const auto inner_x = static_cast<std::int32_t>(320) +
					exact::vector2(inner_x_angle, 120).x;
				static_cast<void>(raster_.plot({inner_x, inner_y}, color));
			}
		}
	};
	put_pair(0, 6U);
	const auto bright_end = static_cast<std::uint8_t>((trail_count_ + 1U) / 2U);
	for(std::uint8_t offset = 1U; offset <= trail_count_; ++offset) {
		put_pair(
			static_cast<std::int16_t>(offset),
			(offset <= bright_end) ? 5U : 1U
		);
	}
	carry_.curve_cycle = static_cast<std::uint8_t>(carry_.curve_cycle + 2U);
	return true;
}

bool ClassicOpCharacterSelection::draw_cursor(std::size_t player) noexcept
{
	if(mode_ == ClassicOpSelectionMode::story && player == 1U) {
		return true;
	}
	if(is_cpu_selection_mode(mode_) && player == 1U && !confirmed_[0U]) {
		return true;
	}
	const std::uint8_t first = (player == 0U) ? 0x12U : 0x16U;
	const auto color = static_cast<present::Index4>(
		confirmed_[player] ? 15U : ((player == 0U) ? 8U : 10U)
	);
	const auto left = static_cast<std::int32_t>((player == 0U) ? 240 : 368);
	const auto top = static_cast<std::int32_t>(128 + (selection_[player] * 20U));
	for(std::uint8_t row = 0U; row < 2U; ++row) {
		for(std::uint8_t column = 0U; column < 2U; ++column) {
			const auto &glyph = tram_.gaiji(static_cast<std::uint8_t>(
				first + (row * 2U) + column
			));
			for(std::uint8_t y = 0U; y < present::gaiji_height; ++y) {
				for(std::uint8_t half = 0U; half < 2U; ++half) {
					const auto bits = glyph.row_halves[
						(static_cast<std::size_t>(y) * 2U) + half
					];
					for(std::uint8_t bit = 0U; bit < 8U; ++bit) {
						if((bits & static_cast<std::uint8_t>(0x80U >> bit)) != 0U &&
							!raster_.plot({
								left + (column * 16) + (half * 8) + bit,
								top + (row * 16) + y,
							}, color)) {
							return false;
						}
					}
				}
			}
		}
	}
	return true;
}

bool ClassicOpCharacterSelection::draw_portrait(std::size_t player) noexcept
{
	const auto left = static_cast<std::int32_t>((player == 0U) ? 32 : 416);
	if(mode_ == ClassicOpSelectionMode::story && player == 1U) {
		return present::cdg_put_8(
			raster_, left, 96, prepared_->story_portrait
		);
	}
	const auto &portrait = prepared_->portraits[selection_[player]][
		confirmed_[player] ? palette_index(palette_[player]) : 0U
	];
	const bool portrait_ok = (player == 1U) ?
		present::cdg_put_hflip_8(raster_, left, 96, portrait) :
		present::cdg_put_8(raster_, left, 96, portrait);
	return portrait_ok;
}

bool ClassicOpCharacterSelection::draw_stats_base(std::size_t player) noexcept
{
	if(mode_ == ClassicOpSelectionMode::story && player == 1U) {
		return true;
	}
	const auto left = static_cast<std::int32_t>((player == 0U) ? 32 : 416);
	return present::cdg_put_noalpha_8(raster_, left, 304, prepared_->stats);
}

bool ClassicOpCharacterSelection::draw_stat_row(
	std::size_t player,
	std::uint8_t row
) noexcept
{
	if((mode_ == ClassicOpSelectionMode::story && player == 1U) || row >= 3U) {
		return row < 3U;
	}
	constexpr std::array<std::array<std::uint8_t, 3U>, 9U> stats{
		std::array<std::uint8_t, 3U>{3U, 1U, 5U},
		std::array<std::uint8_t, 3U>{4U, 4U, 2U},
		std::array<std::uint8_t, 3U>{3U, 4U, 3U},
		std::array<std::uint8_t, 3U>{2U, 3U, 1U},
		std::array<std::uint8_t, 3U>{4U, 1U, 4U},
		std::array<std::uint8_t, 3U>{2U, 2U, 5U},
		std::array<std::uint8_t, 3U>{4U, 5U, 1U},
		std::array<std::uint8_t, 3U>{5U, 2U, 4U},
		std::array<std::uint8_t, 3U>{5U, 5U, 3U},
	};
	std::int32_t star_left = static_cast<std::int32_t>(
		((player == 0U) ? 32 : 416) + 108
	);
	for(std::uint8_t star = 5U;
		stats[selection_[player]][row] < star; --star) {
		if(!raster_.clear({
			star_left, 315 + (row * 16), 9U, 16U,
		}, 14U)) {
			return false;
		}
		star_left -= 11;
	}
	return true;
}

bool ClassicOpCharacterSelection::draw_extra(std::size_t player) noexcept
{
	if(mode_ == ClassicOpSelectionMode::story && player == 1U) {
		return true;
	}
	const auto extra_left = static_cast<std::int32_t>((player == 0U) ? 160 : 544);
	return present::cdg_put_noalpha_8(
		raster_, extra_left, 304, prepared_->extra_background
	) && present::cdg_put_noalpha_8(
		raster_, extra_left + 16, 316,
		prepared_->extra_icons[selection_[player]]
	);
}

bool ClassicOpCharacterSelection::draw_player(std::size_t player) noexcept
{
	if(!draw_portrait(player) || !draw_stats_base(player)) {
		return false;
	}
	for(std::uint8_t row = 0U; row < 3U; ++row) {
		if(!draw_stat_row(player, row)) {
			return false;
		}
	}
	return draw_extra(player);
}

bool ClassicOpCharacterSelection::draw_frame() noexcept
{
	if(!raster_.clear(full_screen, 0U) || !draw_curves() ||
		!draw_portrait(0U) || !draw_portrait(1U) ||
		!draw_stats_base(0U) || !draw_stats_base(1U)) {
		return false;
	}
	for(std::uint8_t row = 0U; row < 3U; ++row) {
		if(!draw_stat_row(0U, row) || !draw_stat_row(1U, row)) {
			return false;
		}
	}
	for(std::uint8_t character = 0U;
		character < available_character_count_; ++character) {
		if(!present::superimpose_put(
			raster_, *prepared_->names,
			{256, static_cast<std::int32_t>(136 + (character * 20U))},
			static_cast<std::uint16_t>(character * 2U)
		) || !present::superimpose_put(
			raster_, *prepared_->names,
			{320, static_cast<std::int32_t>(136 + (character * 20U))},
			static_cast<std::uint16_t>((character * 2U) + 1U)
		)) {
			return false;
		}
	}
	return draw_extra(0U) && draw_extra(1U) &&
		draw_cursor(0U) && draw_cursor(1U);
}

bool ClassicOpCharacterSelection::begin_confirmation(
	std::size_t player,
	MainPaletteVariant palette,
	ResumePoint resume
) noexcept
{
	// The Anniversary callback is installed before the source mutates the
	// resident selection and begins the overlapping CDG load.
	raster_.set_palette_tone(200);
	palette_[player] = palette;
	auto raw = paletted_value(selection_[player], palette_[player]);
	resident_.paletted_character[player] = static_cast<std::int8_t>(raw);
	resident_changed_this_advance_ = true;
	const auto other = 1U - player;
	if(confirmed_[other] && resident_.paletted_character[other] ==
		resident_.paletted_character[player]) {
		palette_[player] = (palette_[player] == MainPaletteVariant::first) ?
			MainPaletteVariant::second : MainPaletteVariant::first;
		raw = paletted_value(selection_[player], palette_[player]);
		resident_.paletted_character[player] = static_cast<std::int8_t>(raw);
	}
	confirming_player_ = static_cast<std::uint8_t>(player);
	confirmation_frame_ = 0U;
	resume_point_ = resume;
	phase_ = ClassicOpSelectionPhase::confirm_white_in;
	return true;
}

bool ClassicOpCharacterSelection::process_player(
	std::size_t player,
	std::uint16_t input,
	ResumePoint after_shot,
	ResumePoint after_bomb
) noexcept
{
	if(confirmed_[player]) {
		resume_point_ = after_bomb;
		return resume_sample();
	}
	if(carry_.input_locked[player]) {
		if(input == 0U) {
			carry_.input_locked[player] = false;
		}
		resume_point_ = after_bomb;
		return resume_sample();
	}
	bool moved = false;
	if((input & input_up) != 0U) {
		selection_[player] = ring_selection_up(
			selection_[player], available_character_count_
		);
		moved = true;
	}
	if((input & input_down) != 0U) {
		selection_[player] = ring_selection_down(
			selection_[player], available_character_count_
		);
		moved = true;
	}
	if(moved) {
		carry_.input_locked[player] = true;
	}
	if((input & input_shot) != 0U) {
		return begin_confirmation(
			player, MainPaletteVariant::first, after_shot
		);
	}
	if((input & input_bomb) != 0U) {
		return begin_confirmation(
			player, MainPaletteVariant::second, after_bomb
		);
	}
	resume_point_ = after_bomb;
	return resume_sample();
}

bool ClassicOpCharacterSelection::process_sample() noexcept
{
	if(mode_ == ClassicOpSelectionMode::vs_1p_2p) {
		return process_player(
			0U, sampled_input_.player_one,
			ResumePoint::player_one_bomb,
			ResumePoint::player_two_start
		);
	}
	const auto player = static_cast<std::size_t>(
		(mode_ == ClassicOpSelectionMode::story) ? 0U : active_player_
	);
	return process_player(
		player, sampled_input_.single_player,
		(player == 0U) ? ResumePoint::player_one_bomb :
			ResumePoint::player_two_bomb,
		ResumePoint::after_players
	);
}

bool ClassicOpCharacterSelection::resume_sample() noexcept
{
	switch(resume_point_) {
	case ResumePoint::player_one_bomb:
		if((player_input(mode_, 0U, sampled_input_) & input_bomb) != 0U) {
			return begin_confirmation(
				0U, MainPaletteVariant::second,
				(mode_ == ClassicOpSelectionMode::vs_1p_2p) ?
					ResumePoint::player_two_start : ResumePoint::after_players
			);
		}
		resume_point_ = (mode_ == ClassicOpSelectionMode::vs_1p_2p) ?
			ResumePoint::player_two_start : ResumePoint::after_players;
		return resume_sample();
	case ResumePoint::player_two_start:
		return process_player(
			1U, sampled_input_.player_two,
			ResumePoint::player_two_bomb,
			ResumePoint::after_players
		);
	case ResumePoint::player_two_bomb:
		if((player_input(mode_, 1U, sampled_input_) & input_bomb) != 0U) {
			return begin_confirmation(
				1U, MainPaletteVariant::second, ResumePoint::after_players
			);
		}
		resume_point_ = ResumePoint::after_players;
		return resume_sample();
	case ResumePoint::after_players:
		return finish_sample();
	}
	return false;
}

bool ClassicOpCharacterSelection::confirmation_vsync() noexcept
{
	raster_.apply_pending_palette();
	++confirmation_frame_;
	++frame_elapsed_;
	if(confirmation_frame_ < confirmation_vsyncs) {
		const auto next = std::max<std::int16_t>(
			100, static_cast<std::int16_t>(200 - (confirmation_frame_ * 6U))
		);
		raster_.set_palette_tone(next);
		return true;
	}
	raster_.set_palette_tone(100);
	confirmed_[confirming_player_] = true;
	carry_.input_locked[confirming_player_] = true;
	if(!resume_sample()) {
		return false;
	}
	// The callback clears itself inside the final VBlank. The source resumes
	// the blocked sample while vblank_in() remains true, skips its safety wait,
	// and publishes on this same eighteenth boundary.
	if(phase_ == ClassicOpSelectionPhase::frame_wait) {
		frame_wait_remaining_ = 0U;
		return publish_frame();
	}
	return true;
}

bool ClassicOpCharacterSelection::finish_sample() noexcept
{
	if(practice_resume_release_lock_) {
		if(sampled_input_.single_player == 0U) {
			practice_resume_release_lock_ = false;
		}
		frame_wait_remaining_ = 1U;
		phase_ = ClassicOpSelectionPhase::frame_wait;
		return true;
	}
	if((sampled_input_.single_player & input_cancel) != 0U) {
		finish_canceled();
		return true;
	}
	if(is_cpu_selection_mode(mode_) && active_player_ == 0U) {
		if(confirmed_[0U] && cpu_quirk_frames_ > 12U) {
			active_player_ = 1U;
			phase_ = ClassicOpSelectionPhase::draw_and_input;
			return true;
		}
		++cpu_quirk_frames_;
	}
	if(retain_for_new_classic_practice_ && confirmed_[0U] && confirmed_[1U]) {
		practice_completion_pending_ = true;
	}
	if(fade_counter_ > 32U) {
		return finish_selected();
	}
	const auto target = (delay_elapsed_ != 0U) ? delay_elapsed_ :
		ordinary_frame_vsyncs;
	frame_wait_remaining_ = (frame_elapsed_ >= target) ? 1U :
		static_cast<std::uint16_t>(target - frame_elapsed_);
	phase_ = ClassicOpSelectionPhase::frame_wait;
	return true;
}

bool ClassicOpCharacterSelection::publish_frame() noexcept
{
	if(frame_elapsed_ > 4U && trail_count_ > 1U) {
		--trail_count_;
	}
	const auto old_shown = raster_.shown_page();
	const auto next_shown = (old_shown == present::Page::zero) ?
		present::Page::one : present::Page::zero;
	if(!raster_.set_access_page(old_shown) ||
		!raster_.set_shown_page(next_shown)) {
		return false;
	}
	page_flipped_this_advance_ = true;
	if(confirmed_[0U] && confirmed_[1U]) {
		++fade_counter_;
		if(fade_counter_ >= 16U) {
			raster_.set_palette_tone(static_cast<std::int16_t>(
				200 - (fade_counter_ * 6U)
			));
			raster_.apply_pending_palette();
		}
	}
	resident_.resident_rand = std::bit_cast<std::int32_t>(
		std::bit_cast<std::uint32_t>(resident_.resident_rand) + 1U
	);
	resident_changed_this_advance_ = true;
	frame_elapsed_ = 0U;
	delay_elapsed_ = 0U;
	if(practice_completion_pending_) {
		return finish_selected();
	}
	phase_ = ClassicOpSelectionPhase::draw_and_input;
	return true;
}

bool ClassicOpCharacterSelection::finish_selected() noexcept
{
	resident_changed_this_advance_ = true;
	if(mode_ == ClassicOpSelectionMode::story) {
		resident_ = prepare_new_classic_story_restart(std::move(resident_));
	} else {
		resident_.last_score = {};
		resident_.global_frame = 0U;
		resident_.runtime_snapshot.reset();
		resident_.compact_runtime_snapshot.reset();
		resident_.round_start_snapshot.reset();
		resident_.practice_rules.reset();
		resident_.random_seed = std::bit_cast<std::uint32_t>(
			resident_.resident_rand
		);
	}
	if(!retain_for_new_classic_practice_) {
		if(!push_audio_command({
			.type = ClassicAudioCommandType::stop_song,
			.cue = ClassicMusicCue::character_select,
		})) {
			return false;
		}
	} else if(!raster_.clear(full_screen, 0U)) {
		// The source's final retained selector flip clears the now-hidden page
		// before Practice Setup renders its first curves-only background.
		return false;
	}
	completion_ = ClassicOpSelectionCompletion(
		ClassicOpSelectionOutcome::selected, mode_, resident_, carry_,
		raster_, tram_
	);
	phase_ = ClassicOpSelectionPhase::ready;
	return true;
}

void ClassicOpCharacterSelection::finish_canceled() noexcept
{
	static_cast<void>(raster_.set_access_page(present::Page::zero));
	static_cast<void>(raster_.clear(full_screen, 0U));
	static_cast<void>(raster_.set_shown_page(present::Page::zero));
	tram_.fill({});
	if(is_vs_mode(mode_)) {
		resident_.game_mode = game_mode::none;
		resident_changed_this_advance_ = true;
	}
	if(!push_audio_command({
		.type = ClassicAudioCommandType::stop_song,
		.cue = ClassicMusicCue::character_select,
	})) {
		fail();
		return;
	}
	completion_ = ClassicOpSelectionCompletion(
		ClassicOpSelectionOutcome::canceled, mode_, resident_, carry_,
		raster_, tram_
	);
	phase_ = ClassicOpSelectionPhase::ready;
}

void ClassicOpCharacterSelection::fail() noexcept
{
	phase_ = ClassicOpSelectionPhase::failed;
	completion_.reset();
}

bool ClassicOpCharacterSelection::push_audio_command(
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

ClassicOpSelectionTick ClassicOpCharacterSelection::tick(
	ClassicOpSelectionBoundary boundary,
	bool surface_changed,
	bool input_sampled,
	bool page_flipped,
	bool resident_changed
) const noexcept
{
	return {
		.phase = phase_,
		.boundary = boundary,
		.surface_changed = surface_changed,
		.input_sampled = input_sampled,
		.page_flipped = page_flipped,
		.resident_changed = resident_changed,
		.confirming_player = (phase_ ==
			ClassicOpSelectionPhase::confirm_white_in) ?
			std::optional<std::uint8_t>{confirming_player_} : std::nullopt,
		.ready = phase_ == ClassicOpSelectionPhase::ready,
	};
}

std::expected<ClassicOpSelectionTick, ClassicOpSelectionError>
ClassicOpCharacterSelection::advance(ClassicOpSelectionInput input)
{
	if(audio_count_ != 0U) {
		return std::unexpected(selection_error(
			ClassicOpSelectionErrorCode::audio_command_pending
		));
	}
	input = normalize_selection_input(
		mode_, resident_.key_mode, std::move(input)
	);
	if((input.single_player & ~valid_input_bits) != 0U ||
		(input.player_one & ~valid_input_bits) != 0U ||
		(input.player_two & ~valid_input_bits) != 0U) {
		return std::unexpected(selection_error(
			ClassicOpSelectionErrorCode::invalid_input
		));
	}
	resident_changed_this_advance_ = false;
	page_flipped_this_advance_ = false;
	switch(phase_) {
	case ClassicOpSelectionPhase::initial_blank:
		if(!initialize_after_blank()) {
			fail();
			return std::unexpected(selection_error(
				ClassicOpSelectionErrorCode::draw_failed
			));
		}
		delay_elapsed_ = 1U;
		phase_ = ClassicOpSelectionPhase::initial_delay;
		return tick(ClassicOpSelectionBoundary::vsync, true);
	case ClassicOpSelectionPhase::initial_delay: {
		++delay_elapsed_;
		const auto target = (mode_ == ClassicOpSelectionMode::vs_1p_2p) ?
			initial_delay_two_player : initial_delay_regular;
		if(delay_elapsed_ >= target) {
			frame_elapsed_ = delay_elapsed_;
			phase_ = ClassicOpSelectionPhase::draw_and_input;
		}
		return tick(ClassicOpSelectionBoundary::vsync);
	}
	case ClassicOpSelectionPhase::draw_and_input:
		if(!draw_frame()) {
			fail();
			return std::unexpected(selection_error(
				ClassicOpSelectionErrorCode::draw_failed
			));
		}
		sampled_input_ = input;
		resume_point_ = ResumePoint::after_players;
		if(!process_sample()) {
			fail();
			return std::unexpected(selection_error(
				ClassicOpSelectionErrorCode::draw_failed
			));
		}
		return tick(
			ClassicOpSelectionBoundary::input_poll, true, true, false,
			resident_changed_this_advance_
		);
	case ClassicOpSelectionPhase::confirm_white_in:
		if(!confirmation_vsync()) {
			fail();
			return std::unexpected(selection_error(
				ClassicOpSelectionErrorCode::draw_failed
			));
		}
		return tick(
			ClassicOpSelectionBoundary::vsync, true, false,
			page_flipped_this_advance_,
			resident_changed_this_advance_
		);
	case ClassicOpSelectionPhase::frame_wait:
		++frame_elapsed_;
		if(frame_wait_remaining_ > 0U) {
			--frame_wait_remaining_;
		}
		if(frame_wait_remaining_ == 0U) {
			if(!publish_frame()) {
				fail();
				return std::unexpected(selection_error(
					ClassicOpSelectionErrorCode::draw_failed
				));
			}
			return tick(
				ClassicOpSelectionBoundary::vsync, true, false, true,
				resident_changed_this_advance_
			);
		}
		return tick(ClassicOpSelectionBoundary::vsync);
	case ClassicOpSelectionPhase::ready:
	case ClassicOpSelectionPhase::consumed:
	case ClassicOpSelectionPhase::failed:
		return std::unexpected(selection_error(
			ClassicOpSelectionErrorCode::invalid_state
		));
	}
	return std::unexpected(selection_error(
		ClassicOpSelectionErrorCode::invalid_state
	));
}

std::optional<ClassicAudioCommand>
ClassicOpCharacterSelection::take_audio_command() noexcept
{
	if(audio_count_ == 0U) {
		return std::nullopt;
	}
	const auto result = audio_commands_[audio_head_];
	audio_head_ = (audio_head_ + 1U) % audio_commands_.size();
	--audio_count_;
	return result;
}

std::optional<ClassicOpSelectionCompletion>
ClassicOpCharacterSelection::take_completion() noexcept
{
	if(phase_ != ClassicOpSelectionPhase::ready || !completion_) {
		return std::nullopt;
	}
	auto result = std::move(completion_);
	completion_.reset();
	phase_ = ClassicOpSelectionPhase::consumed;
	return result;
}

ClassicOpPracticeBackgroundStatus ClassicOpCharacterSelection::
advance_new_classic_practice_background() noexcept
{
	if(!retain_for_new_classic_practice_ ||
		mode_ != ClassicOpSelectionMode::vs_1p_cpu ||
		phase_ != ClassicOpSelectionPhase::consumed) {
		return ClassicOpPracticeBackgroundStatus::failed;
	}
	// Replay Patch's Practice bridge retains the selector's curve state and
	// page ownership, but deliberately omits portraits, stats, names, and
	// cursors while the setup text is visible.
	if(!practice_frame_drawn_) {
		if(!draw_curves()) {
			return ClassicOpPracticeBackgroundStatus::failed;
		}
		practice_frame_drawn_ = true;
	}
	++practice_frame_vsyncs_;
	if(practice_frame_vsyncs_ < ordinary_frame_vsyncs) {
		return ClassicOpPracticeBackgroundStatus::waiting;
	}
	const auto old_shown = raster_.shown_page();
	const auto next_shown = (old_shown == present::Page::zero) ?
		present::Page::one : present::Page::zero;
	if(!raster_.set_access_page(old_shown) ||
		!raster_.set_shown_page(next_shown) ||
		!raster_.clear(full_screen, 0U)) {
		return ClassicOpPracticeBackgroundStatus::failed;
	}
	practice_frame_drawn_ = false;
	practice_frame_vsyncs_ = 0U;
	fade_counter_ = static_cast<std::uint8_t>(fade_counter_ + 1U);
	resident_.resident_rand = std::bit_cast<std::int32_t>(
		std::bit_cast<std::uint32_t>(resident_.resident_rand) + 1U
	);
	// OP stores this value in resident.rand. Keep the explicit native seed
	// handoff synchronized until Practice finally starts MAINL.
	resident_.random_seed = std::bit_cast<std::uint32_t>(
		resident_.resident_rand
	);
	return ClassicOpPracticeBackgroundStatus::published;
}

bool ClassicOpCharacterSelection::resume_from_new_classic_practice() noexcept
{
	if(!retain_for_new_classic_practice_ ||
		mode_ != ClassicOpSelectionMode::vs_1p_cpu ||
		phase_ != ClassicOpSelectionPhase::consumed) {
		return false;
	}
	for(std::size_t player = 0U; player < selection_.size(); ++player) {
		const auto raw = static_cast<std::uint8_t>(
			resident_.paletted_character[player]
		);
		selection_[player] = static_cast<std::uint8_t>((raw - 1U) / 2U);
		palette_[player] = ((raw & 1U) != 0U) ?
			MainPaletteVariant::first : MainPaletteVariant::second;
	}
	confirmed_ = {};
	active_player_ = 0U;
	cpu_quirk_frames_ = 0U;
	trail_count_ = 8U;
	fade_counter_ = 0U;
	delay_elapsed_ = 0U;
	frame_elapsed_ = 0U;
	frame_wait_remaining_ = 0U;
	sampled_input_ = {};
	resume_point_ = ResumePoint::after_players;
	confirming_player_ = 0U;
	confirmation_frame_ = 0U;
	practice_completion_pending_ = false;
	practice_resume_release_lock_ = true;
	practice_frame_drawn_ = false;
	practice_frame_vsyncs_ = 0U;
	raster_.set_palette_tone(100);
	raster_.apply_pending_palette();
	phase_ = ClassicOpSelectionPhase::draw_and_input;
	return true;
}

ClassicOpSelectionPhase ClassicOpCharacterSelection::phase() const noexcept
{
	return phase_;
}

const GameInit &ClassicOpCharacterSelection::resident() const noexcept
{
	return resident_;
}

const ClassicOpSelectionCarry &ClassicOpCharacterSelection::carry() const noexcept
{
	return carry_;
}

const present::Raster &ClassicOpCharacterSelection::raster() const noexcept
{
	return raster_;
}

const present::Tram &ClassicOpCharacterSelection::tram() const noexcept
{
	return tram_;
}

std::array<std::uint8_t, 2U>
ClassicOpCharacterSelection::local_selection() const noexcept
{
	return selection_;
}

std::array<bool, 2U> ClassicOpCharacterSelection::confirmed() const noexcept
{
	return confirmed_;
}

ClassicOpSelectionInput ClassicOpSelectionTestAccess::normalize_input(
	ClassicOpSelectionMode mode,
	std::uint8_t key_mode,
	ClassicOpSelectionInput input
) noexcept
{
	return normalize_selection_input(mode, key_mode, std::move(input));
}

std::expected<std::unique_ptr<ClassicOpCharacterSelection>, ClassicOpSelectionError>
ClassicOpSelectionTestAccess::create_with_predecoded_assets(
	GameInit resident,
	ClassicOpSelectionMode mode,
	std::uint8_t available_character_count,
	ClassicOpSelectionCarry carry,
	present::Raster raster,
	present::Tram tram,
	std::array<assets::CdgFile, 9U> portraits,
	assets::CdgFile story_portrait,
	assets::CdgFile stats,
	assets::CdgFile extra_background,
	assets::CdgFile extra_icons,
	assets::BfntSheet names,
	assets::Palette16Rgb4 palette,
	bool retain_for_new_classic_practice
)
{
	auto prepared = std::make_unique<ClassicOpCharacterSelection::Prepared>();
	for(std::size_t character = 0U; character < portraits.size(); ++character) {
		if(portraits[character].cels.size() != 2U ||
			!valid_cel(portraits[character].cels[0U], 192U, 192U, true) ||
			!valid_cel(portraits[character].cels[1U], 192U, 192U, true)) {
			return std::unexpected(selection_error(
				ClassicOpSelectionErrorCode::invalid_asset_geometry
			));
		}
		prepared->portraits[character][0U] =
			std::move(portraits[character].cels[0U]);
		prepared->portraits[character][1U] =
			std::move(portraits[character].cels[1U]);
	}
	if(story_portrait.cels.size() != 1U ||
		stats.cels.size() != 1U || extra_background.cels.size() != 1U ||
		extra_icons.cels.size() != 9U ||
		!valid_cel(story_portrait.cels[0U], 192U, 192U, true) ||
		!valid_cel(stats.cels[0U], 128U, 64U, false) ||
		!valid_cel(extra_background.cels[0U], 64U, 64U, false)) {
		return std::unexpected(selection_error(
			ClassicOpSelectionErrorCode::invalid_asset_geometry
		));
	}
	prepared->story_portrait = std::move(story_portrait.cels[0U]);
	prepared->stats = std::move(stats.cels[0U]);
	prepared->extra_background = std::move(extra_background.cels[0U]);
	for(std::size_t index = 0U; index < extra_icons.cels.size(); ++index) {
		if(!valid_cel(extra_icons.cels[index], 32U, 44U, false)) {
			return std::unexpected(selection_error(
				ClassicOpSelectionErrorCode::invalid_asset_geometry
			));
		}
		prepared->extra_icons[index] = std::move(extra_icons.cels[index]);
	}
	prepared->names = present::prepare_superimpose_sheet(names);
	if(!prepared->names || prepared->names->pixel_width() != 64U ||
		prepared->names->pixel_height() != 16U ||
		prepared->names->first_code() != 0U ||
		prepared->names->last_code() != 17U) {
		return std::unexpected(selection_error(
			ClassicOpSelectionErrorCode::invalid_asset_geometry
		));
	}
	prepared->palette = palette;
	return ClassicOpCharacterSelection::create_prepared(
		std::move(prepared), std::move(resident), mode,
		available_character_count, carry, std::move(raster), std::move(tram),
		retain_for_new_classic_practice
	);
}

} // namespace th03::app
