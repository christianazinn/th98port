#include "th03/app/new_classic_netplay_match_intro.hpp"
#include "th03/app/classic_audio_session.hpp"
#include "th03/app/new_classic_netplay_room.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <vector>

namespace {

std::size_t checks = 0U;
std::size_t failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

void test_direct_resident_projection()
{
	using namespace th03::app;
	MainDirectVsConfig config{
		.player_one = {
			.character = MainCharacter::marisa,
			.palette = MainPaletteVariant::second,
		},
		.player_two = {
			.character = MainCharacter::rikako,
			.palette = MainPaletteVariant::first,
		},
		.seed = 0x1234'5678U,
		.rank = 3U,
		.key_mode = 2U,
		.autofire_mask = 3U,
	};
	expect(valid_main_direct_vs_config(config),
		"resident projection fixture is a valid direct match");
	const auto init = main_direct_vs_game_init(config);
	expect(init.game_mode == th03::game_mode::vs_1p_2p &&
		init.random_seed == config.seed &&
		std::bit_cast<std::uint32_t>(init.resident_rand) == config.seed &&
		init.rank == config.rank && init.key_mode == config.key_mode &&
		init.autofire_mask == config.autofire_mask &&
		init.remaining_credits == 3U &&
		init.paletted_character == std::array<std::int8_t, 2U>{6, 13},
		"MAINL intro and direct MAIN share the exact resident projection");
	auto invalid = config;
	invalid.autofire_mask = 4U;
	expect(!valid_main_direct_vs_config(invalid),
		"direct match projection rejects Autofire bits outside P1/P2");
}

void test_invalid_request_fails_closed()
{
	auto created = th03::app::NewClassicNetplayMatchIntro::create({});
	expect(!created && created.error().code == th03::app::
		NewClassicNetplayMatchIntroErrorCode::invalid_request,
		"match intro rejects absent authenticated content");
}

void test_authentic_intro(int argc, char **argv)
{
	if(argc != 5) {
		return;
	}
	using namespace th03::app;
	NewClassicNetplayMatchIntroRequest request{
		.content = {
			.data_root = std::filesystem::path(argv[1]),
			.mainl_executable = std::filesystem::path(argv[2]),
			.font_rom = std::filesystem::path(argv[3]),
			.language_overlay = std::filesystem::path(argv[4]),
			.new_classic_menu_assets = std::filesystem::path(
				TH03_TEST_SOURCE_DIR
			) / "assets" / "new-classic" / "menu-backgrounds",
		},
		.process = {
			.player_one = {
				.character = MainCharacter::reimu,
				.palette = MainPaletteVariant::first,
			},
			.player_two = {
				.character = MainCharacter::mima,
				.palette = MainPaletteVariant::first,
			},
			.seed = 0x8765'4321U,
			.rank = 1U,
			.classic_audio_lifecycle = true,
		},
		.language = NewClassicLanguageAssetProfile::english,
	};
	auto intro = NewClassicNetplayMatchIntro::create(request);
	expect(intro.has_value(),
		"authenticated English content constructs the source VS intro");
	if(!intro) {
		return;
	}
	std::vector<th03::present::ComposedPixel> pixels(
		th03::present::graphics_page_pixels
	);
	const auto pixel_span = std::span<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	>(pixels.data(), pixels.size());
	auto initial = (*intro)->compose_shown_surface(pixel_span);
	expect(initial.has_value() && initial->pixels.size() == pixels.size() &&
		!initial->host_overlay,
		"VS intro starts on a complete source-domain surface");
	std::vector<ClassicAudioCommandType> audio{};
	std::size_t ticks = 0U;
	while((*intro)->status() == NewClassicNetplayMatchIntroStatus::running &&
		ticks < 200U) {
		auto advanced = (*intro)->advance(false);
		expect(advanced.has_value() && advanced->surface_changed,
			"each source VS intro boundary publishes a complete surface");
		if(!advanced) {
			return;
		}
		while(const auto command = (*intro)->take_audio_command()) {
			audio.push_back(command->type);
		}
		++ticks;
	}
	expect((*intro)->status() == NewClassicNetplayMatchIntroStatus::ready &&
		ticks == 151U,
		"undismissed VS intro preserves the exact 18+97+18+18 cadence");
	expect(audio == std::vector<ClassicAudioCommandType>{
		ClassicAudioCommandType::stop_song,
		ClassicAudioCommandType::load_song,
		ClassicAudioCommandType::load_effects,
	}, "VS intro retains the source stop/load/effects command order");
	const auto final = (*intro)->compose_shown_surface(pixel_span);
	expect(final.has_value() && final->palette_tone == 100,
		"VS intro finishes on the ordinary MAIN handoff palette");

	request.process.player_two.character = MainCharacter::reimu;
	auto player_one_nametag = layout_new_classic_nametag(
		"Resident of Fantasy Who Runs Through Time"
	);
	auto player_two_nametag = layout_new_classic_nametag(
		"\xE5\xB9\xBB\xE6\x83\xB3\xE3\x81\xAE\xE4\xBD\x8F\xE4\xBA\xBA"
	);
	expect(player_one_nametag.has_value() && player_two_nametag.has_value(),
		"profile fixture satisfies the exact title geometry");
	if(!player_one_nametag || !player_two_nametag) {
		return;
	}
	request.profiles = NewClassicNetplayRoomPaired{
		.player_one = {
			.user_id = "01234567-89ab-cdef-0123-456789abcdef",
			.nametag = *player_one_nametag,
		},
		.player_two = {
			.user_id = "fedcba98-7654-3210-fedc-ba9876543210",
			.nametag = *player_two_nametag,
		},
	};
	auto profiled = NewClassicNetplayMatchIntro::create(std::move(request));
	expect(profiled.has_value(),
		"same-character players retain independent account-title rows");
	if(!profiled) {
		return;
	}
	std::vector<th03::present::ComposedPixel> profiled_pixels(
		th03::present::graphics_page_pixels
	);
	const auto profiled_span = std::span<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	>(profiled_pixels.data(), profiled_pixels.size());
	auto profiled_surface = (*profiled)->compose_shown_surface(profiled_span);
	expect(profiled_surface.has_value(),
		"profiled VS intro composes a complete source-domain surface");
	if(!profiled_surface) {
		return;
	}
	const auto different_region = [&profiled_pixels](
		std::size_t left_x,
		std::size_t right_x,
		std::size_t y,
		std::size_t width,
		std::size_t height
	) {
		for(std::size_t row = 0U; row < height; ++row) {
			for(std::size_t column = 0U; column < width; ++column) {
				const auto left = (y + row) * 640U + left_x + column;
				const auto right = (y + row) * 640U + right_x + column;
				if(profiled_pixels[left] != profiled_pixels[right]) {
					return true;
				}
			}
		}
		return false;
	};
	const auto same_region = [&profiled_pixels](
		std::size_t left_x,
		std::size_t right_x,
		std::size_t y,
		std::size_t width,
		std::size_t height
	) {
		for(std::size_t row = 0U; row < height; ++row) {
			for(std::size_t column = 0U; column < width; ++column) {
				const auto left = (y + row) * 640U + left_x + column;
				const auto right = (y + row) * 640U + right_x + column;
				if(profiled_pixels[left] != profiled_pixels[right]) {
					return false;
				}
			}
		}
		return true;
	};
	expect(different_region(80U, 336U, 276U, 224U, 32U),
		"P1 and P2 nametags render independently even for one character");
	expect(same_region(80U, 336U, 308U, 224U, 16U),
		"the source character-name line remains identical for one character");
}

void test_authentic_match_ready_jingle(int argc, char **argv)
{
	if(argc != 5) {
		return;
	}
	using namespace th03::app;
	auto created = ClassicAudioSession::create({
		.data_root = std::filesystem::path(argv[1]),
		.tick_rate = {.ticks = 526'315U, .seconds = 9'328U},
	});
	expect(created.has_value(),
		"authenticated content constructs the match-ready audio session");
	if(!created) {
		return;
	}
	expect((*created)->apply({
		.type = ClassicAudioCommandType::load_song,
		.cue = ClassicMusicCue::menu_main,
	}).has_value() && (*created)->apply({
		.type = ClassicAudioCommandType::play_song,
		.cue = ClassicMusicCue::menu_main,
	}).has_value(), "match-ready fixture begins with the retained title song");
	for(const auto command : new_classic_netplay_match_ready_audio_commands) {
		expect((*created)->apply(command).has_value(),
			"match-ready command is accepted by the authentic PMD session");
	}
	NewClassicNetplayMatchReadyAudioGate gate{};
	std::size_t ticks = 0U;
	while(!gate.complete && ticks < 5'000U) {
		auto advanced = (*created)->advance_tick();
		expect(advanced.has_value(),
			"match-ready jingle renders one authentic audio tick");
		if(!advanced) {
			return;
		}
		gate.advance(advanced->status);
		++ticks;
	}
	expect(gate.observed_jingle && gate.complete && ticks == 428U,
		"WIN.M reaches its deterministic natural PMD stop before the match intro");
	std::cout << "match_ready_jingle_ticks=" << ticks << '\n';
}

} // namespace

int main(int argc, char **argv)
{
	test_direct_resident_projection();
	test_invalid_request_fails_closed();
	test_authentic_match_ready_jingle(argc, argv);
	test_authentic_intro(argc, argv);
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
