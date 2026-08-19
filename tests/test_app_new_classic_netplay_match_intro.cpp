#include "th03/app/new_classic_netplay_match_intro.hpp"

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
	};
	expect(valid_main_direct_vs_config(config),
		"resident projection fixture is a valid direct match");
	const auto init = main_direct_vs_game_init(config);
	expect(init.game_mode == th03::game_mode::vs_1p_2p &&
		init.random_seed == config.seed &&
		std::bit_cast<std::uint32_t>(init.resident_rand) == config.seed &&
		init.rank == config.rank && init.key_mode == config.key_mode &&
		init.remaining_credits == 3U &&
		init.paletted_character == std::array<std::int8_t, 2U>{6, 13},
		"MAINL intro and direct MAIN share the exact resident projection");
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
	auto intro = NewClassicNetplayMatchIntro::create(std::move(request));
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
}

} // namespace

int main(int argc, char **argv)
{
	test_direct_resident_projection();
	test_invalid_request_fails_closed();
	test_authentic_intro(argc, argv);
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
