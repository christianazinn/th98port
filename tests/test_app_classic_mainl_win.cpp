#include "../src/app/classic_mainl_win_internal.hpp"
#include "../src/app/new_classic_netplay_post_match_internal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "th03/app/authentic_text.hpp"
#include "th03/app/main_process.hpp"
#include "th03/present/source_surface.hpp"

namespace {

using th03::app::ClassicAudioCommand;
using th03::app::ClassicAudioCommandType;
using th03::app::ClassicAudioStatus;
using th03::app::ClassicMainlWinBoundary;
using th03::app::ClassicMainlWinContext;
using th03::app::ClassicMainlWinErrorCode;
using th03::app::ClassicMainlWinPhase;
using th03::app::ClassicMainlWinScene;
using th03::app::ClassicMainlWinTestAccess;
using th03::app::ClassicMusicCue;
using th03::app::LoadedMainResource;
using th03::app::MainCharacter;
using th03::app::MainPaletteVariant;
using th03::app::MainResourceKey;
using th03::app::MainResourceRole;
using th03::app::NewClassicNetplayPostMatchErrorCode;
using th03::app::NewClassicNetplayPostMatchRequest;
using th03::app::NewClassicNetplayPostMatchStatus;
using th03::app::NewClassicNetplayPostMatchTestAccess;
using th03::app::ResourceBytes;
using th03::present::AnkGlyph;
using th03::present::AnkGlyphProvider;
using th03::present::JisGlyph;
using th03::present::Page;
using th03::present::Point;

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, std::string_view message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

void append_u8(std::vector<std::byte> &bytes, std::uint8_t value)
{
	bytes.push_back(static_cast<std::byte>(value));
}

void append_le16(std::vector<std::byte> &bytes, std::uint16_t value)
{
	append_u8(bytes, static_cast<std::uint8_t>(value));
	append_u8(bytes, static_cast<std::uint8_t>(value >> 8U));
}

std::vector<std::byte> make_rgb(std::uint8_t seed)
{
	std::vector<std::byte> result;
	result.reserve(48U);
	for(std::uint8_t color = 0U; color < 16U; ++color) {
		append_u8(result, static_cast<std::uint8_t>((color + seed) & 0x0FU));
		append_u8(result, static_cast<std::uint8_t>((15U - color) & 0x0FU));
		append_u8(result, static_cast<std::uint8_t>((color ^ seed) & 0x0FU));
	}
	return result;
}

std::vector<std::byte> make_cdg(
	std::uint16_t width,
	std::uint16_t height,
	std::uint8_t cel_count,
	std::uint8_t first_color
)
{
	const auto stride = static_cast<std::size_t>(width) / 8U;
	const auto plane_size = stride * height;
	std::vector<std::byte> bytes;
	append_le16(bytes, static_cast<std::uint16_t>(plane_size));
	append_le16(bytes, width);
	append_le16(bytes, height);
	append_le16(bytes, static_cast<std::uint16_t>(80U * (height - 1U)));
	append_le16(bytes, static_cast<std::uint16_t>(width / 32U));
	append_u8(bytes, cel_count);
	append_u8(bytes, 0U);
	append_le16(bytes, 0U);
	append_le16(bytes, 0U);
	for(std::uint8_t cel = 0U; cel < cel_count; ++cel) {
		bytes.insert(bytes.end(), plane_size, std::byte{0xFFU});
		const auto color = static_cast<std::uint8_t>(
			(first_color + cel) & 0x0FU
		);
		for(std::size_t plane = 0U; plane < 4U; ++plane) {
			const auto value = ((color & (1U << plane)) != 0U) ?
				std::byte{0xFFU} : std::byte{0U};
			bytes.insert(bytes.end(), plane_size, value);
		}
	}
	return bytes;
}

std::vector<std::byte> make_messages()
{
	std::vector<std::byte> result(1980U);
	for(std::size_t block = 0U; block < 11U; ++block) {
		for(std::size_t line = 0U; line < 3U; ++line) {
			const auto value = static_cast<std::uint8_t>(
				'A' + ((block * 3U + line) % 26U)
			);
			const auto first = (block * 180U) + (line * 60U);
			std::fill_n(result.begin() + static_cast<std::ptrdiff_t>(first),
				60U, static_cast<std::byte>(value));
		}
	}
	return result;
}

class SyntheticFont final : public AnkGlyphProvider {
public:
	[[nodiscard]] std::optional<AnkGlyph> glyph(
		std::uint8_t
	) const override
	{
		return AnkGlyph{};
	}

	[[nodiscard]] std::optional<JisGlyph> jis_glyph(
		std::uint16_t
	) const override
	{
		JisGlyph glyph{};
		glyph.row_halves[0U] = 0x80U;
		return glyph;
	}
};

std::vector<std::byte> bytes_for(const th03::app::MainResourceSpec &spec)
{
	switch(spec.key.role) {
	case MainResourceRole::win_logo_palette_pre:
		return make_rgb(1U);
	case MainResourceRole::win_logo_fade_cels:
		return make_cdg(256U, 72U, 5U, 1U);
	case MainResourceRole::win_logo_final:
		return make_cdg(256U, 72U, 1U, 12U);
	case MainResourceRole::character_win_portrait:
		return make_cdg(192U, 192U, 2U, 6U);
	case MainResourceRole::character_win_message:
		return make_messages();
	case MainResourceRole::win_logo_palette_post:
		return make_rgb(7U);
	default:
		break;
	}
	return {};
}

std::vector<LoadedMainResource> resources_for(std::int8_t winner_paletted)
{
	const auto winner = *th03::app::main_player_selection_from_paletted(
		winner_paletted
	);
	const auto keys = *th03::app::mainl_win_resource_keys(winner);
	std::vector<LoadedMainResource> result;
	result.reserve(keys.size());
	for(const auto key : keys) {
		const auto spec = *th03::app::main_resource_spec(key);
		auto bytes = std::make_shared<const std::vector<std::byte>>(
			bytes_for(spec)
		);
		expect(bytes->size() == spec.expected_bytes,
			"synthetic win resource has canonical byte count");
		result.push_back({.spec = spec, .bytes = std::move(bytes)});
	}
	return result;
}

std::expected<std::unique_ptr<ClassicMainlWinScene>, th03::app::ClassicMainlWinError>
create_scene(ClassicMainlWinContext context)
{
	auto resources = resources_for(context.winner_paletted_character);
	SyntheticFont font;
	return ClassicMainlWinTestAccess::create_with_synthetic_resources(
		context,
		resources,
		font
	);
}

void expect_pixel(
	const th03::present::Raster &raster,
	Page page,
	Point point,
	std::uint8_t value,
	std::string_view message
)
{
	expect(raster.pixel(page, point) == value, message);
}

void drain_initial_audio(ClassicMainlWinScene &scene)
{
	expect(scene.take_audio_command() == ClassicAudioCommand{
		.type = ClassicAudioCommandType::load_song,
		.cue = ClassicMusicCue::winner,
	}, "win scene first requests WIN-song load");
	expect(scene.take_audio_command() == ClassicAudioCommand{
		.type = ClassicAudioCommandType::play_song,
		.cue = ClassicMusicCue::winner,
	}, "win scene requests play after load");
	expect(!scene.take_audio_command(), "initial audio queue drains exactly");
}

void advance_black_in(ClassicMainlWinScene &scene, ClassicAudioStatus audio)
{
	for(std::uint16_t boundary = 1U; boundary <= 35U; ++boundary) {
		const auto result = scene.advance(audio, false);
		expect(result && result->boundary == ClassicMainlWinBoundary::vsync,
			"black-in consumes one exact VSync boundary");
		const auto expected_tone = (boundary == 35U) ? 100 :
			static_cast<std::int16_t>(((boundary - 1U) / 2U) * 6U);
		expect(scene.raster().palette_tone() == expected_tone,
			"black-in speed-two tone schedule is exact");
	}
	expect(scene.phase() == ClassicMainlWinPhase::wait_measure_6,
		"black-in reaches the first measure wait on boundary 35");
}

void advance_logo_cels(ClassicMainlWinScene &scene, ClassicAudioStatus audio)
{
	for(std::uint8_t frame = 1U; frame <= 24U; ++frame) {
		const auto result = scene.advance(audio, false);
		expect(result && result->boundary == ClassicMainlWinBoundary::vsync,
			"each logo delay is a VSync boundary");
		if((frame % 6U) == 0U && frame < 24U) {
			expect_pixel(
				scene.raster(), Page::zero, {352, 300},
				static_cast<std::uint8_t>(2U + (frame / 6U)),
				"six-frame logo boundary selects the next cel"
			);
		}
	}
	expect(scene.phase() == ClassicMainlWinPhase::wait_measure_10,
		"four six-frame logo holds reach measure ten");
}

void test_active_audio_preload_and_completion()
{
	const ClassicMainlWinContext context{
		.winner_paletted_character = 2,
		.loser_paletted_character = 3,
		.message_block = 9U,
		.preload_stage_splash = true,
	};
	auto scene = create_scene(context);
	expect(scene.has_value(), "active-audio win scene creates");
	if(!scene) {
		return;
	}
	expect((*scene)->raster().palette_tone() == 0,
		"created winner scene publishes the first black boundary");
	expect_pixel((*scene)->raster(), Page::zero, {352, 300}, 1U,
		"created winner scene draws LOGO fade cel zero");
	const auto blocked = (*scene)->advance({.bgm_active = true}, false);
	expect(!blocked && blocked.error().code ==
		ClassicMainlWinErrorCode::audio_command_pending,
		"visual timeline cannot overtake pending load/play commands");
	drain_initial_audio(**scene);

	advance_black_in(**scene, {.bgm_active = true, .measure = 0U});
	const auto waiting_six = (*scene)->advance(
		{.bgm_active = false, .measure = 5U}, false
	);
	expect(waiting_six && waiting_six->boundary ==
		ClassicMainlWinBoundary::audio_measure &&
		(*scene)->phase() == ClassicMainlWinPhase::wait_measure_6,
		"active measure wait stays a busy poll even if later status says inactive");
	const auto at_six = (*scene)->advance(
		{.bgm_active = false, .measure = 6U}, false
	);
	expect(at_six && at_six->boundary ==
		ClassicMainlWinBoundary::immediate &&
		(*scene)->phase() == ClassicMainlWinPhase::logo_fade,
		"measure six immediately draws logo cel one without a frame");
	expect_pixel((*scene)->raster(), Page::zero, {352, 300}, 2U,
		"measure six selects fade cel one");

	advance_logo_cels(**scene, {.bgm_active = true, .measure = 6U});
	const auto before_ten = (*scene)->advance(
		{.bgm_active = true, .measure = 9U}, false
	);
	expect(before_ten && before_ten->boundary ==
		ClassicMainlWinBoundary::audio_measure,
		"measure ten wait does not fabricate a VSync");
	const auto at_ten = (*scene)->advance(
		{.bgm_active = true, .measure = 10U}, false
	);
	expect(at_ten && at_ten->boundary ==
		ClassicMainlWinBoundary::immediate &&
		(*scene)->phase() == ClassicMainlWinPhase::wait_measure_11 &&
		(*scene)->raster().palette_tone() == 200,
		"measure ten performs the white reveal immediately");
	expect_pixel((*scene)->raster(), Page::zero, {224, 64}, 7U,
		"second-palette winner selects portrait cel one");
	expect_pixel((*scene)->raster(), Page::zero, {352, 300}, 12U,
		"reveal replaces the fade sheet with LOGO5");

	const auto before_eleven = (*scene)->advance(
		{.bgm_active = true, .measure = 10U}, false
	);
	expect(before_eleven && before_eleven->boundary ==
		ClassicMainlWinBoundary::audio_measure,
		"measure eleven remains an audio busy poll");
	const auto at_eleven = (*scene)->advance(
		{.bgm_active = true, .measure = 11U}, false
	);
	expect(at_eleven && at_eleven->boundary ==
		ClassicMainlWinBoundary::immediate &&
		(*scene)->phase() == ClassicMainlWinPhase::white_in,
		"measure eleven enters white-in without a VSync");

	for(std::uint8_t boundary = 1U; boundary <= 18U; ++boundary) {
		const auto result = (*scene)->advance({}, false);
		expect(result && result->boundary == ClassicMainlWinBoundary::vsync,
			"white-in consumes its exact VSync boundary");
		const auto expected_tone = (boundary == 18U) ? 100 :
			static_cast<std::int16_t>(
				200U - (((boundary - 1U) == 0U ? 0U : boundary - 1U) * 6U)
			);
		expect((*scene)->raster().palette_tone() == expected_tone,
			"white-in alignment and tone schedule are exact");
	}
	expect((*scene)->phase() == ClassicMainlWinPhase::post_reveal_delay,
		"white-in reaches the post-reveal frame delay");
	for(std::uint8_t frame = 1U; frame <= 8U; ++frame) {
		const auto result = (*scene)->advance({}, true);
		expect(result && result->boundary == ClassicMainlWinBoundary::vsync &&
			!result->input_sampled,
			"held input is not sampled during the eight-frame delay");
	}
	expect((*scene)->phase() == ClassicMainlWinPhase::stage_preload,
		"ordinary Story win requests preload after drawing text");
	expect_pixel((*scene)->raster(), Page::zero, {80, 272}, 15U,
		"selected message block draws white bold text");
	expect_pixel((*scene)->raster(), Page::zero, {80, 288}, 15U,
		"selected message block draws its second line at +16 pixels");
	expect_pixel((*scene)->raster(), Page::zero, {80, 304}, 15U,
		"selected message block draws its third line at +32 pixels");
	std::vector<th03::present::ComposedPixel> composed_pixels(
		th03::present::graphics_page_pixels
	);
	SyntheticFont composed_font;
	const auto composed = th03::present::compose_source_surface(
		std::span<
			th03::present::ComposedPixel,
			th03::present::graphics_page_pixels
		>(composed_pixels.data(), composed_pixels.size()),
		(*scene)->raster(),
		th03::present::Tram{},
		composed_font
	);
	expect(composed && std::ranges::all_of(
		std::array{Point{80, 272}, Point{80, 288}, Point{80, 304}},
		[&composed_pixels](Point point) {
			const auto &pixel = composed_pixels[
				(static_cast<std::size_t>(point.y) *
					th03::present::graphics_width) +
				static_cast<std::size_t>(point.x)
			];
			return pixel.layer ==
				th03::present::ComposedPixelLayer::graphics &&
				pixel.value == 15U;
		}
	), "shown-surface composition publishes all three winner quote lines");
	const auto still_preload = (*scene)->advance({}, true);
	expect(still_preload && still_preload->boundary ==
		ClassicMainlWinBoundary::stage_preload &&
		still_preload->stage_preload_requested &&
		!still_preload->input_sampled,
		"preload blocks even already-held dismissal input");
	expect(!(*scene)->acknowledge_stage_preload(),
		"preload acknowledgement rejects the wrong access page");
	auto &preload_raster = (*scene)->stage_preload_raster();
	expect(preload_raster.set_access_page(Page::one) &&
		preload_raster.plot({17, 19}, 13U),
		"synthetic stage preload mutates the exact hidden page");
	expect((*scene)->acknowledge_stage_preload().has_value() &&
		(*scene)->phase() == ClassicMainlWinPhase::waiting_for_input,
		"exact page-role preload acknowledgement opens input polling");

	const auto dismissed = (*scene)->advance({}, true);
	expect(dismissed && dismissed->boundary ==
		ClassicMainlWinBoundary::immediate && dismissed->input_sampled &&
		dismissed->dismissed &&
		(*scene)->phase() == ClassicMainlWinPhase::black_out,
		"held input dismisses immediately without a release lock");
	for(std::uint8_t boundary = 1U; boundary <= 18U; ++boundary) {
		const auto result = (*scene)->advance({}, false);
		expect(result && result->boundary == ClassicMainlWinBoundary::vsync,
			"black-out consumes its exact VSync boundary");
		const auto expected_tone = (boundary == 18U) ? 0 :
			static_cast<std::int16_t>(
				100U - (((boundary - 1U) == 0U ? 0U : boundary - 1U) * 6U)
			);
		expect((*scene)->raster().palette_tone() == expected_tone,
			"black-out alignment and tone schedule are exact");
	}
	expect((*scene)->phase() == ClassicMainlWinPhase::ready &&
		!(*scene)->take_completion(),
		"completion remains gated by the final song stop");
	expect((*scene)->take_audio_command() == ClassicAudioCommand{
		.type = ClassicAudioCommandType::stop_song,
		.cue = ClassicMusicCue::winner,
	}, "ready boundary requests exact WIN-song stop");
	auto completion = (*scene)->take_completion();
	expect(completion.has_value() && completion->stage_splash_preloaded() &&
		completion->raster().palette_tone() == 0 &&
		completion->raster().access_page() == Page::one,
		"drained scene publishes the exact preloaded black completion");
	expect_pixel(completion->raster(), Page::one, {17, 19}, 13U,
		"completion retains the hidden stage preload");
	expect((*scene)->phase() == ClassicMainlWinPhase::consumed,
		"completion capability is consumable exactly once");
}

void test_silent_fallback_and_atomicity()
{
	const ClassicMainlWinContext context{
		.winner_paletted_character = 1,
		.loser_paletted_character = 18,
		.message_block = 8U,
		.preload_stage_splash = false,
	};
	auto scene = create_scene(context);
	expect(scene.has_value(), "silent-fallback win scene creates");
	if(!scene) {
		return;
	}
	drain_initial_audio(**scene);
	advance_black_in(**scene, {.bgm_active = false, .measure = 99U});
	for(std::uint8_t frame = 1U; frame <= 16U; ++frame) {
		const auto result = (*scene)->advance(
			{.bgm_active = true, .measure = 99U}, false
		);
		expect(result && result->boundary == ClassicMainlWinBoundary::vsync,
			"inactive first wait latches its sixteen-frame fallback");
	}
	expect((*scene)->phase() == ClassicMainlWinPhase::logo_fade,
		"sixteen fallback frames draw logo cel one");
	advance_logo_cels(**scene, {.bgm_active = false});
	for(std::uint8_t frame = 1U; frame <= 64U; ++frame) {
		const auto result = (*scene)->advance({}, false);
		expect(result && result->boundary == ClassicMainlWinBoundary::vsync,
			"inactive second wait consumes exactly 64 frames");
	}
	expect((*scene)->phase() == ClassicMainlWinPhase::wait_measure_11,
		"64-frame fallback performs the reveal");
	for(std::uint8_t frame = 1U; frame <= 4U; ++frame) {
		const auto result = (*scene)->advance({}, false);
		expect(result && result->boundary == ClassicMainlWinBoundary::vsync,
			"inactive third wait consumes exactly four frames");
	}
	expect((*scene)->phase() == ClassicMainlWinPhase::white_in,
		"four-frame fallback reaches white-in");

	for(std::uint8_t frame = 0U; frame < 18U + 8U; ++frame) {
		expect((*scene)->advance({}, true).has_value(),
			"silent fixture reaches its first input poll");
	}
	expect((*scene)->phase() == ClassicMainlWinPhase::waiting_for_input,
		"non-preload win enters input directly after text");
	const auto wait = (*scene)->advance({}, false);
	expect(wait && wait->boundary == ClassicMainlWinBoundary::vsync &&
		wait->input_sampled && !wait->dismissed,
		"empty input consumes exactly one source frame");

	auto resources = resources_for(context.winner_paletted_character);
	SyntheticFont font;
	auto reordered = resources;
	std::swap(reordered[0U], reordered[1U]);
	auto rejected = ClassicMainlWinTestAccess::create_with_synthetic_resources(
		context, reordered, font
	);
	expect(!rejected && rejected.error().code ==
		ClassicMainlWinErrorCode::resource_set_mismatch,
		"win transaction rejects reordered resources atomically");
	auto missing = resources;
	missing.back().bytes.reset();
	rejected = ClassicMainlWinTestAccess::create_with_synthetic_resources(
		context, missing, font
	);
	expect(!rejected && rejected.error().code ==
		ClassicMainlWinErrorCode::resource_bytes_missing,
		"win transaction rejects a late missing resource");
	auto malformed = resources;
	auto malformed_bytes = std::make_shared<std::vector<std::byte>>(
		*malformed[4U].bytes
	);
	malformed_bytes->back() = std::byte{0U};
	malformed[4U].bytes = malformed_bytes;
	rejected = ClassicMainlWinTestAccess::create_with_synthetic_resources(
		context, malformed, font
	);
	expect(!rejected && rejected.error().code ==
		ClassicMainlWinErrorCode::message_prepare_failed,
		"late malformed unselected message block is not a false green");
	auto public_rejected = ClassicMainlWinScene::create(
		context, resources, font
	);
	expect(!public_rejected && public_rejected.error().code ==
		ClassicMainlWinErrorCode::resource_integrity_failed,
		"production scene reauthenticates every supplied byte closure");
	auto invalid_context = context;
	invalid_context.message_block = 11U;
	rejected = ClassicMainlWinTestAccess::create_with_synthetic_resources(
		invalid_context, resources, font
	);
	expect(!rejected && rejected.error().code ==
		ClassicMainlWinErrorCode::invalid_context,
		"message block eleven is rejected before decode");
}

void test_all_winners_and_message_blocks()
{
	for(std::int8_t winner = 1; winner <= 18; ++winner) {
		for(std::uint8_t block = 0U; block <= 10U; ++block) {
			const ClassicMainlWinContext context{
				.winner_paletted_character = winner,
				.loser_paletted_character = static_cast<std::int8_t>(
					(winner == 18) ? 1 : 18
				),
				.message_block = block,
				.preload_stage_splash = block < 9U,
			};
			auto scene = create_scene(context);
			expect(scene.has_value(),
				"every paletted winner and fixed message block constructs");
		}
	}
}

NewClassicNetplayPostMatchRequest post_match_request(std::int8_t winner)
{
	NewClassicNetplayPostMatchRequest request{};
	request.process_start.game_mode = th03::game_mode::vs_1p_2p;
	request.process_start.paletted_character = {1, 18};
	request.terminal.game_mode = th03::game_mode::vs_1p_2p;
	request.terminal.route = 2U;
	request.terminal.winner = winner;
	request.transition.action = th03::SessionAction::exit_to_operator;
	request.transition.resident = request.process_start;
	return request;
}

void test_netplay_post_match_owner()
{
	SyntheticFont font;
	auto request = post_match_request(1);
	auto resources = resources_for(18);
	auto owner = NewClassicNetplayPostMatchTestAccess::create(
		request, resources, font
	);
	expect(owner.has_value(),
		"confirmed P2 match constructs the exact winner-specific MAINL owner");
	if(!owner) {
		return;
	}
	expect((*owner)->take_audio_command() == ClassicAudioCommand{
		.type = ClassicAudioCommandType::load_song,
		.cue = ClassicMusicCue::winner,
	}, "netplay post-match loads the ordinary winner song");
	expect((*owner)->take_audio_command() == ClassicAudioCommand{
		.type = ClassicAudioCommandType::play_song,
		.cue = ClassicMusicCue::winner,
	}, "netplay post-match plays the ordinary winner song");
	expect(!(*owner)->take_audio_command(),
		"netplay post-match startup audio drains exactly");

	std::vector<th03::present::ComposedPixel> pixels(
		th03::present::graphics_page_pixels
	);
	auto composed = (*owner)->compose_shown_surface(std::span<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	>(pixels.data(), pixels.size()));
	expect(composed.has_value(),
		"netplay post-match publishes the source winner surface");

	for(std::size_t frame = 0U; frame < 512U; ++frame) {
		auto advanced = (*owner)->advance({}, false);
		expect(advanced.has_value(),
			"silent netplay post-match reaches its dismissal poll");
		if(!advanced) {
			return;
		}
	}
	expect((*owner)->status() == NewClassicNetplayPostMatchStatus::running,
		"winner scene waits indefinitely for a fresh dismissal");
	bool completed = false;
	bool stopped_song = false;
	for(std::size_t frame = 0U; frame < 64U; ++frame) {
		auto advanced = (*owner)->advance({}, true);
		expect(advanced.has_value(),
			"winner dismissal owns the complete final fade");
		if(!advanced) {
			return;
		}
		while(const auto command = (*owner)->take_audio_command()) {
			stopped_song = stopped_song || *command == ClassicAudioCommand{
				.type = ClassicAudioCommandType::stop_song,
				.cue = ClassicMusicCue::winner,
			};
		}
		if(advanced->status == NewClassicNetplayPostMatchStatus::complete) {
			completed = true;
			break;
		}
	}
	expect(completed,
		"confirmed netplay post-match completes after the source black-out");
	expect(stopped_song,
		"netplay post-match stops the winner song at its source boundary");
	const auto after_complete = (*owner)->advance({}, false);
	expect(!after_complete && after_complete.error().code ==
		NewClassicNetplayPostMatchErrorCode::not_running,
		"completed netplay post-match cannot advance twice");

	auto quit_owner = NewClassicNetplayPostMatchTestAccess::create(
		post_match_request(0), resources_for(1), font
	);
	expect(quit_owner.has_value(), "P1 winner owner constructs independently");
	if(quit_owner) {
		const auto quit = (*quit_owner)->advance({}, false, true);
		expect(quit && quit->status ==
			NewClassicNetplayPostMatchStatus::quit_requested,
			"application quit bypasses the local winner timeline");
	}

	auto invalid = post_match_request(0);
	invalid.terminal.route = 0U;
	auto rejected = NewClassicNetplayPostMatchTestAccess::create(
		invalid, resources_for(1), font
	);
	expect(!rejected && rejected.error().code ==
		NewClassicNetplayPostMatchErrorCode::invalid_request,
		"nonterminal netplay transition cannot enter MAINL win");
}

void local_original_gate(const std::filesystem::path &content_root)
{
	SyntheticFont font;
	th03::app::MainResourceLoader loader(content_root);
	for(std::int8_t winner = 1; winner <= 18; ++winner) {
		const auto selection = *th03::app::main_player_selection_from_paletted(
			winner
		);
		const auto resources = loader.load_mainl_win(selection);
		expect(resources.has_value(),
			"local original winner closure authenticates");
		if(!resources) {
			continue;
		}
		for(std::uint8_t block = 0U; block <= 10U; ++block) {
			const ClassicMainlWinContext context{
				.winner_paletted_character = winner,
				.loser_paletted_character = static_cast<std::int8_t>(
					(winner == 18) ? 1 : 18
				),
				.message_block = block,
			};
			auto scene = ClassicMainlWinScene::create(
				context, *resources, font
			);
			expect(scene.has_value(),
				"local original winner/message pair prepares");
		}
	}
}

} // namespace

int main(int argc, char **argv)
{
	test_active_audio_preload_and_completion();
	test_silent_fallback_and_atomicity();
	test_all_winners_and_message_blocks();
	test_netplay_post_match_owner();
	if(argc == 2) {
		local_original_gate(argv[1]);
	} else if(argc != 1) {
		std::cerr << "usage: test_app_classic_mainl_win [content-root]\n";
		return 2;
	}

	if(failures != 0U) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << checks << " checks passed\n";
	return 0;
}
