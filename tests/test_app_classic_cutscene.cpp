#include "../src/app/classic_cutscene_scene_internal.hpp"

#include "th03/app/new_classic_language.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using namespace th03::app;
using th03::present::AnkGlyph;
using th03::present::AnkGlyphProvider;
using th03::present::JisGlyph;
using th03::present::Page;
using th03::present::Point;
using th03::present::Raster;
using th03::present::Rect;

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

class TestFont final : public AnkGlyphProvider {
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
		return JisGlyph{};
	}
};

[[nodiscard]] std::vector<std::uint8_t> script_bytes(
	std::string_view script
)
{
	std::vector<std::uint8_t> result;
	result.reserve(script.size());
	for(const auto value : script) {
		result.push_back(static_cast<std::uint8_t>(
			static_cast<unsigned char>(value)
		));
	}
	return result;
}

[[nodiscard]] std::unique_ptr<ClassicCutsceneScene> make_scene(
	std::string_view script,
	MainlCutsceneKind kind = MainlCutsceneKind::interstage_chiyuri,
	Raster raster = {}
)
{
	static const TestFont font{};
	const auto bytes = script_bytes(script);
	auto created = ClassicCutsceneSceneTestAccess::create_with_synthetic_script(
		bytes, font, std::move(raster), kind
	);
	expect(created.has_value(), "synthetic scene constructs");
	if(!created) {
		std::terminate();
	}
	return std::move(*created);
}

[[nodiscard]] ClassicCutsceneTick advance(
	ClassicCutsceneScene &scene,
	ClassicCutsceneInput input = {}
)
{
	auto result = scene.advance(input);
	expect(result.has_value(), "scene advance succeeds");
	if(!result) {
		std::terminate();
	}
	return *result;
}

void test_resource_boundary_and_completion()
{
	const TestFont font{};
	const MainlCutsceneResourceRequest request{
		.kind = MainlCutsceneKind::interstage_chiyuri,
		.character = MainCharacter::reimu,
	};
	auto missing = ClassicCutsceneScene::create(
		request, {}, font, Raster{}
	);
	expect(
		!missing && missing.error().code ==
			ClassicCutsceneErrorCode::resource_set_mismatch,
		"production construction requires the complete authenticated closure"
	);

	auto scene = make_scene("\\w2\\$");
	auto tick = advance(*scene);
	expect(
		tick.boundary == ClassicCutsceneBoundary::vsync &&
			tick.phase == ClassicCutscenePhase::running,
		"frame wait consumes its first source VSync"
	);
	tick = advance(*scene);
	expect(
		tick.boundary == ClassicCutsceneBoundary::vsync,
		"frame wait consumes exactly its second source VSync"
	);
	tick = advance(*scene);
	expect(
		tick.ready && tick.phase == ClassicCutscenePhase::ready,
		"script finish publishes a ready transaction"
	);
	auto completion = scene->take_completion();
	expect(completion.has_value(), "completion transfers exactly once");
	expect(
		scene->phase() == ClassicCutscenePhase::consumed &&
			!scene->take_completion(),
		"completion is a one-shot capability"
	);
}

void test_empty_batches_and_input_waits()
{
	// Leading separators force empty VM batches. They must never expose the
	// batch storage's default-initialized operation.
	auto scene = make_scene("\r\n \\s2\\$");
	auto tick = ClassicCutsceneTick{};
	for(std::size_t separator = 0U; separator < 3U; ++separator) {
		tick = advance(*scene);
		expect(
			tick.boundary == ClassicCutsceneBoundary::immediate &&
				tick.input_sampled,
			"each empty source iteration owns one input poll"
		);
	}
	tick = advance(*scene, {
		.script = {.any_key_held = true},
	});
	expect(
		tick.boundary == ClassicCutsceneBoundary::immediate &&
			tick.input_sampled,
		"input-change separates its nested poll from the VM poll"
	);
	tick = advance(*scene, {.script = {.any_key_held = true}});
	expect(
		tick.boundary == ClassicCutsceneBoundary::vsync &&
			tick.input_sampled,
		"input-change first waits for release"
	);
	tick = advance(*scene);
	expect(
		tick.boundary == ClassicCutsceneBoundary::immediate &&
			tick.input_sampled,
		"release and press-loop polls have no invented VSync between them"
	);
	tick = advance(*scene, {
		.shot_or_ok_held = true,
	});
	expect(
		tick.boundary == ClassicCutsceneBoundary::immediate &&
			tick.input_sampled,
		"a new press ends input-change immediately"
	);
	tick = advance(*scene);
	expect(tick.ready, "input-change completion preserves the next VM poll");

	scene = make_scene("\\wk2\\$");
	tick = advance(*scene);
	expect(
		tick.boundary == ClassicCutsceneBoundary::immediate &&
			tick.input_sampled,
		"finite OK wait separates the nested source poll"
	);
	tick = advance(*scene);
	expect(
		tick.boundary == ClassicCutsceneBoundary::vsync &&
			tick.input_sampled,
		"finite OK wait consumes its first VSync"
	);
	tick = advance(*scene, {.shot_or_ok_held = true});
	expect(
		tick.boundary == ClassicCutsceneBoundary::immediate,
		"OK or shot ends the finite OK wait without a VSync"
	);
	tick = advance(*scene);
	expect(tick.ready, "OK-wait completion preserves the next VM poll");

	scene = make_scene("\\wk0\\$");
	tick = advance(*scene);
	expect(
		tick.boundary == ClassicCutsceneBoundary::immediate,
		"zero-frame OK wait first preserves the VM poll"
	);
	tick = advance(*scene);
	expect(
		tick.boundary == ClassicCutsceneBoundary::immediate &&
			tick.input_sampled,
		"zero-frame OK wait performs its nested poll without a VSync"
	);
	tick = advance(*scene);
	expect(tick.ready, "zero-frame OK wait preserves the next VM poll");
}

void test_measure_latch_and_audio_queue()
{
	auto scene = make_scene(
		"\\m,DEMO1.M\r\\wm3,2\\fm999\\e257\\m$\\m*\\$"
	);
	auto tick = advance(*scene);
	expect(
		tick.boundary == ClassicCutsceneBoundary::immediate,
		"song load-and-play yields before later script work"
	);
	auto blocked = scene->advance({});
	expect(
		!blocked && blocked.error().code ==
			ClassicCutsceneErrorCode::audio_command_pending &&
			scene->phase() == ClassicCutscenePhase::running,
		"advancing cannot silently discard queued audio"
	);
	constexpr std::array expected_types{
		ClassicAudioCommandType::stop_song,
		ClassicAudioCommandType::load_song,
		ClassicAudioCommandType::play_song,
	};
	for(std::size_t index = 0U; index < expected_types.size(); ++index) {
		const auto command = scene->take_audio_command();
		expect(
			command && command->type == expected_types[index] &&
				command->cue == (index == 0U ? ClassicMusicCue::none :
					ClassicMusicCue::cutscene_one),
			"load-and-play stops the prior song before assigning the new cue"
		);
	}
	expect(!scene->take_audio_command(), "audio queue drains exactly");

	tick = advance(*scene, {
		.audio = {.bgm_active = true, .measure = 2U},
	});
	expect(
		tick.boundary == ClassicCutsceneBoundary::audio_measure,
		"active music selects the measure path"
	);
	tick = advance(*scene, {
		.audio = {.bgm_active = false, .measure = 2U},
	});
	expect(
		tick.boundary == ClassicCutsceneBoundary::audio_measure,
		"measure-vs-frame policy remains latched after wait entry"
	);
	tick = advance(*scene, {.audio = {.measure = 3U}});
	expect(
		tick.boundary == ClassicCutsceneBoundary::immediate,
		"reaching the measure continues to the following audio operation"
	);
	auto command = scene->take_audio_command();
	expect(
		command && command->type == ClassicAudioCommandType::fade_song &&
			command->parameter == 999U,
		"script audio fade stays 16-bit at the semantic boundary"
	);
	tick = advance(*scene);
	command = scene->take_audio_command();
	expect(
		command && command->type == ClassicAudioCommandType::play_effect &&
			command->parameter == 257U,
		"script effect ID stays 16-bit at the semantic boundary"
	);
	tick = advance(*scene);
	command = scene->take_audio_command();
	expect(
		command && command->type == ClassicAudioCommandType::stop_song,
		"song stop is typed"
	);
	tick = advance(*scene);
	command = scene->take_audio_command();
	expect(
		command && command->type == ClassicAudioCommandType::play_song &&
			command->cue == ClassicMusicCue::cutscene_one,
		"song restart reuses the loaded cue"
	);
	tick = advance(*scene);
	expect(tick.ready, "audio-bearing script completes after drains");
}

void test_timed_presenter_operations()
{
	auto scene = make_scene("\\fi0\\g1\\$");
	auto tick = advance(*scene);
	expect(
		tick.boundary == ClassicCutsceneBoundary::vsync,
		"palette fade begins with its source alignment VSync"
	);
	tick = advance(*scene);
	expect(
		tick.boundary == ClassicCutsceneBoundary::vsync &&
			tick.surface_changed,
		"zero-speed fade completes before the first shake wait"
	);
	tick = advance(*scene);
	expect(
		tick.boundary == ClassicCutsceneBoundary::vsync,
		"shake parameter one includes its second position"
	);
	tick = advance(*scene);
	expect(tick.ready, "inclusive shake finishes and restores scrolling");

	scene = make_scene("\\p,DM1A.PI\r\\p=\\==1,1\\$");
	tick = advance(*scene);
	expect(
		tick.boundary == ClassicCutsceneBoundary::immediate &&
			tick.input_sampled,
		"PI load consumes exactly one VM input poll"
	);
	tick = advance(*scene);
	expect(
		tick.boundary == ClassicCutsceneBoundary::immediate &&
			tick.input_sampled,
		"PI presentation consumes exactly one VM input poll"
	);
	for(std::size_t mask = 0U; mask < 4U; ++mask) {
		tick = advance(*scene);
		expect(
			tick.boundary == ClassicCutsceneBoundary::vsync &&
				tick.surface_changed,
			"animated crossfade consumes one VSync per cumulative mask"
		);
	}
	tick = advance(*scene);
	expect(tick.ready, "crossfade final copy has no following wait");

	scene = make_scene("\\p,UNKNOWN.PI\r\\$");
	auto failed = scene->advance({});
	expect(
		!failed && failed.error().code ==
			ClassicCutsceneErrorCode::unknown_picture &&
			scene->phase() == ClassicCutscenePhase::failed,
		"unknown script picture fails the scene without partial replacement"
	);

	scene = make_scene("\\$", MainlCutsceneKind::ending);
	for(std::size_t index = 0U; index < 96U; ++index) {
		tick = advance(*scene);
		expect(
			tick.boundary == ClassicCutsceneBoundary::vsync &&
				tick.phase == ClassicCutscenePhase::prelude,
			"primary ending consumes its exact 96-VSync black prelude"
		);
	}
	tick = advance(*scene);
	expect(tick.ready, "ending script begins after the 96th VSync");

	Raster seeded{};
	const Rect page{0, 0, th03::present::graphics_width,
		th03::present::graphics_height};
	expect(seeded.set_access_page(Page::zero) && seeded.clear(page, 7U) &&
		seeded.set_access_page(Page::one) && seeded.clear(page, 9U),
		"extra-ending fixture seeds both inherited pages");
	auto interstage_seed = seeded;
	scene = make_scene(
		"\\$", MainlCutsceneKind::interstage_yumemi,
		std::move(interstage_seed)
	);
	tick = advance(*scene);
	expect(tick.ready &&
		scene->raster().shown_page() == Page::zero &&
		scene->raster().access_page() == Page::zero &&
		scene->raster().pixel(Page::zero, Point{0, 0}) == 0U &&
		scene->raster().pixel(Page::zero, Point{500, 360}) == 0U &&
		scene->raster().pixel(Page::one, Point{0, 0}) == 9U,
		"interstage entry clears only page 0 and preserves its black backing page");
	scene = make_scene(
		"\\$", MainlCutsceneKind::extra_ending, std::move(seeded)
	);
	tick = advance(*scene);
	expect(tick.ready &&
		scene->raster().pixel(Page::zero, Point{0, 0}) == 0U &&
		scene->raster().pixel(Page::one, Point{0, 0}) == 0U,
		"extra ending clears both pages before executing its script");
}

[[nodiscard]] std::vector<MainlCutsceneResourceRequest> stock_requests()
{
	std::vector<MainlCutsceneResourceRequest> result;
	result.reserve(28U);
	constexpr std::array characters{
		MainCharacter::reimu,
		MainCharacter::mima,
		MainCharacter::marisa,
		MainCharacter::ellen,
		MainCharacter::kotohime,
		MainCharacter::kana,
		MainCharacter::rikako,
		MainCharacter::chiyuri,
		MainCharacter::yumemi,
	};
	for(const auto kind : {
		MainlCutsceneKind::interstage_chiyuri,
		MainlCutsceneKind::interstage_yumemi,
		MainlCutsceneKind::ending,
	}) {
		for(const auto character : characters) {
			result.push_back({.kind = kind, .character = character});
		}
	}
	result.push_back({
		.kind = MainlCutsceneKind::extra_ending,
		.character = MainCharacter::reimu,
	});
	return result;
}

void test_local_scripts(
	const std::filesystem::path &content_root,
	const std::filesystem::path &language_overlay = {},
	const std::filesystem::path &cutscene_text_pack = {}
)
{
	MainResourceLoader loader(content_root);
	std::optional<NewClassicCutsceneTextCatalog> catalog{};
	if(!language_overlay.empty()) {
		auto overlay = load_new_classic_language_overlay(language_overlay);
		expect(overlay.has_value(), "local language overlay authenticates");
		if(!overlay || !loader.install_authenticated_overrides(
			std::move(*overlay).release_members()
		)) {
			return;
		}
	}
	if(!cutscene_text_pack.empty()) {
		auto loaded = load_new_classic_cutscene_text_pack(cutscene_text_pack);
		expect(loaded.has_value(), "local cutscene text pack authenticates");
		if(!loaded) {
			return;
		}
		catalog.emplace(std::move(*loaded));
	}
	const TestFont font{};
	for(const auto request : stock_requests()) {
		auto resources = loader.load_mainl_cutscene(request);
		expect(resources.has_value(), "local stock closure authenticates");
		if(!resources) {
			continue;
		}
		auto created = ClassicCutsceneScene::create(
			request, *resources, font, Raster{},
			catalog ? &*catalog : nullptr
		);
		expect(created.has_value(), "local stock scene constructs atomically");
		if(!created) {
			continue;
		}
		auto scene = std::move(*created);
		for(std::size_t step = 0U; step < 1000000U && (
			scene->phase() == ClassicCutscenePhase::prelude ||
			scene->phase() == ClassicCutscenePhase::running
		); ++step) {
			while(scene->take_audio_command()) {
			}
			auto advanced = scene->advance({
				.script = {
					.cancel_held = true,
					.any_key_held = true,
				},
				.shot_or_ok_held = true,
			});
			expect(advanced.has_value(), "local stock scene step succeeds");
			if(!advanced) {
				std::cerr << "cutscene request kind="
					<< static_cast<unsigned int>(request.kind)
					<< " character="
					<< static_cast<unsigned int>(request.character)
					<< " error="
					<< static_cast<unsigned int>(advanced.error().code);
				if(advanced.error().raster_error) {
					std::cerr << " raster=" << static_cast<unsigned int>(
						advanced.error().raster_error->code
					);
					if(advanced.error().raster_error->text_error) {
						std::cerr << " text=" << static_cast<unsigned int>(
							advanced.error().raster_error->text_error->code
						) << " text-offset="
							<< advanced.error().raster_error->text_error->byte_offset;
					}
				}
				std::cerr << '\n';
				break;
			}
		}
		expect(
			scene->phase() == ClassicCutscenePhase::ready,
			"local stock scene reaches its source terminator"
		);
	}
}

} // namespace

int main(int argc, char **argv)
{
	test_resource_boundary_and_completion();
	test_empty_batches_and_input_waits();
	test_measure_latch_and_audio_queue();
	test_timed_presenter_operations();
	if(argc == 2) {
		test_local_scripts(std::filesystem::path(argv[1]));
	} else if(argc == 4) {
		test_local_scripts(
			std::filesystem::path(argv[1]), std::filesystem::path(argv[2]),
			std::filesystem::path(argv[3])
		);
	} else {
		expect(argc == 1,
			"optional local gate accepts a root or root/overlay/text-pack triple");
	}

	if(failures != 0U) {
		std::cerr << failures << " of " << checks << " checks failed\n";
		return 1;
	}
	std::cout << "classic cutscene checks: " << checks << '\n';
	return 0;
}
