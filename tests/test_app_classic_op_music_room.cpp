#include "../src/app/classic_op_music_room_internal.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "th03/app/authentic_text.hpp"
#include "th03/app/resources.hpp"

namespace {

using th03::GameInit;
using th03::app::ClassicAudioCommandType;
using th03::app::ClassicMusicCue;
using th03::app::ClassicOpMusicRoomBoundary;
using th03::app::ClassicOpMusicRoomCarry;
using th03::app::ClassicOpMusicRoomErrorCode;
using th03::app::ClassicOpMusicRoomPhase;
using th03::app::ClassicOpMusicRoomScene;
using th03::app::ClassicOpMusicRoomSceneTestAccess;
using th03::app::ClassicOpMusicPolygon;
using th03::app::OpExecutableProfile;
using th03::app::OpMusicRoomTextImport;
using th03::assets::PiImage;
using th03::present::AnkGlyph;
using th03::present::AnkGlyphProvider;
using th03::present::JisGlyph;
using th03::present::Page;

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

class SyntheticFont final : public AnkGlyphProvider {
public:
	[[nodiscard]] std::optional<AnkGlyph> glyph(
		std::uint8_t
	) const override {
		AnkGlyph result{};
		result.rows.fill(0xFFU);
		return result;
	}

	[[nodiscard]] std::optional<JisGlyph> jis_glyph(
		std::uint16_t canonical_jis
	) const override {
		if(missing_jis && *missing_jis == canonical_jis) {
			return std::nullopt;
		}
		JisGlyph result{};
		result.row_halves.fill(0xFFU);
		return result;
	}

	std::optional<std::uint16_t> missing_jis{};
};

[[nodiscard]] PiImage background()
{
	PiImage result{};
	result.palette.emplace();
	for(std::uint8_t index = 0U; index < 16U; ++index) {
		result.palette->colors[index] = {index, index, index};
	}
	result.image.width = th03::present::graphics_width;
	result.image.height = th03::present::graphics_height;
	result.image.indices.assign(th03::present::graphics_page_pixels, 4U);
	return result;
}

[[nodiscard]] OpMusicRoomTextImport labels()
{
	OpMusicRoomTextImport result{};
	result.profile = OpExecutableProfile::unpacked_original_japanese;
	for(std::size_t choice = 0U; choice < result.choices.size(); ++choice) {
		if(choice == OpMusicRoomTextImport::empty_choice_index) {
			continue;
		}
		result.choices[choice].assign(
			32U,
			static_cast<std::uint8_t>('A' + (choice % 26U))
		);
	}
	return result;
}

[[nodiscard]] std::vector<std::byte> comments()
{
	std::vector<std::byte> result(
		th03::app::classic_music_room_comment_file_bytes
	);
	for(std::size_t track = 0U;
		track < th03::app::classic_music_room_track_count;
		++track) {
		for(std::size_t line = 0U;
			line < th03::app::classic_music_room_comment_lines;
			++line) {
			const auto offset = ((track *
				th03::app::classic_music_room_comment_lines) + line) *
				th03::app::classic_music_room_comment_line_bytes;
			std::fill_n(
				result.begin() + static_cast<std::ptrdiff_t>(offset),
				th03::app::classic_music_room_comment_text_bytes,
				static_cast<std::byte>('A' + track)
			);
			result[offset + 40U] = std::byte{0x0DU};
			result[offset + 41U] = std::byte{0x0AU};
		}
	}
	return result;
}

[[nodiscard]] std::unique_ptr<ClassicOpMusicRoomScene> make_scene(
	ClassicOpMusicRoomCarry carry = {},
	GameInit resident = {}
)
{
	const SyntheticFont font{};
	const auto text = labels();
	const auto comment_file = comments();
	auto created = ClassicOpMusicRoomSceneTestAccess::
		create_with_predecoded_assets(
			std::move(resident), std::move(carry), background(), text,
			comment_file, font
		);
	expect(created.has_value(), "synthetic Music Room scene creates");
	return created ? std::move(*created) : nullptr;
}

void release_to_controls(ClassicOpMusicRoomScene &scene)
{
	const auto released = scene.advance();
	expect(
		released &&
			released->boundary == ClassicOpMusicRoomBoundary::input_poll &&
			released->input_sampled &&
			scene.phase() == ClassicOpMusicRoomPhase::controls_poll,
		"empty held-input poll immediately reaches the controls poll"
	);
}

void test_asset_validation_and_initial_presentation()
{
	auto scene = make_scene();
	if(!scene) {
		return;
	}
	expect(
		scene->phase() == ClassicOpMusicRoomPhase::held_release_poll &&
			scene->selection() == 0U &&
			!scene->carry().polygons_initialized,
		"entry retains the playing track and waits before first polygon frame"
	);
	expect(
		scene->raster().shown_page() == Page::zero &&
			scene->raster().access_page() == Page::zero &&
			scene->raster().palette_tone() == 100,
		"entry ends on source page zero with the OP3 palette fully shown"
	);
	for(const auto page : {Page::zero, Page::one}) {
		expect(
			scene->raster().pixel(page, {16, 40}) == 15U &&
				scene->raster().pixel(page, {16, 56}) == 3U,
			"initial selected and unselected labels are drawn on both pages"
		);
		expect(
			scene->raster().pixel(page, {304, 64}) == 15U &&
				scene->raster().pixel(page, {304, 80}) == 13U,
			"initial title and comment colors are drawn on both pages"
		);
	}

	const SyntheticFont font{};
	const auto valid_labels = labels();
	const auto valid_comments = comments();
	PiImage bad_background{};
	auto bad = ClassicOpMusicRoomSceneTestAccess::create_with_predecoded_assets(
		{}, {}, std::move(bad_background), valid_labels, valid_comments, font
	);
	expect(
		!bad && bad.error().code ==
			ClassicOpMusicRoomErrorCode::invalid_background,
		"missing full-screen PI palette is rejected"
	);

	ClassicOpMusicRoomCarry bad_carry{};
	bad_carry.track_playing = 19U;
	bad = ClassicOpMusicRoomSceneTestAccess::create_with_predecoded_assets(
		{}, bad_carry, background(), valid_labels, valid_comments, font
	);
	expect(
		!bad && bad.error().code == ClassicOpMusicRoomErrorCode::invalid_context,
		"non-track process carry is rejected"
	);

	auto short_label = valid_labels;
	short_label.choices[4U].pop_back();
	bad = ClassicOpMusicRoomSceneTestAccess::create_with_predecoded_assets(
		{}, {}, background(), short_label, valid_comments, font
	);
	expect(
		!bad && bad.error().code ==
			ClassicOpMusicRoomErrorCode::invalid_compiled_text,
		"compiled label widths are revalidated at the scene boundary"
	);

	auto bad_comments = valid_comments;
	bad_comments[41U] = std::byte{0U};
	bad = ClassicOpMusicRoomSceneTestAccess::create_with_predecoded_assets(
		{}, {}, background(), valid_labels, bad_comments, font
	);
	expect(
		!bad && bad.error().code ==
			ClassicOpMusicRoomErrorCode::malformed_comment_file,
		"MUSIC.TXT fixed CRLF records are validated"
	);

	auto jis_comments = valid_comments;
	constexpr std::size_t bad_track = 3U;
	constexpr std::size_t bad_line = 7U;
	const auto bad_offset = ((bad_track *
		th03::app::classic_music_room_comment_lines) + bad_line) *
		th03::app::classic_music_room_comment_line_bytes;
	jis_comments[bad_offset] = std::byte{0x82U};
	jis_comments[bad_offset + 1U] = std::byte{0xA0U};
	SyntheticFont missing_font{};
	missing_font.missing_jis = static_cast<std::uint16_t>(0x2422U);
	bad = ClassicOpMusicRoomSceneTestAccess::create_with_predecoded_assets(
		{}, {}, background(), valid_labels, jis_comments, missing_font
	);
	expect(
		!bad && bad.error().code ==
			ClassicOpMusicRoomErrorCode::font_prepare_failed &&
			bad.error().track == bad_track && bad.error().line == bad_line,
		"font preparation failure reports only its track and line coordinate"
	);
}

void test_input_flip_and_polygon_cadence()
{
	auto scene = make_scene();
	if(!scene) {
		return;
	}
	release_to_controls(*scene);
	const auto first_frame = scene->advance();
	expect(
		first_frame &&
			first_frame->boundary == ClassicOpMusicRoomBoundary::vsync &&
			first_frame->page_flipped &&
			scene->phase() == ClassicOpMusicRoomPhase::controls_poll,
		"empty controls sample renders and jumps directly back to controls"
	);
	expect(
		scene->raster().shown_page() == Page::one &&
			scene->raster().access_page() == Page::zero,
		"first source update draws access page zero but publishes page one"
	);
	expect(
		scene->carry().polygons_initialized &&
			scene->carry().random_seed == 0x59C5FB21U,
		"fresh polygon setup consumes exactly 96 master.lib IRAND calls"
	);
	expect(
		scene->carry().polygons[0U] == ClassicOpMusicPolygon{
			344, 194, -2, 64, 135U, 255U,
		} && scene->carry().polygons[15U] == ClassicOpMusicPolygon{
			291, 3107, 3, 48, 170U, 255U,
		},
		"first and last polygons preserve source call order and word wrapping"
	);
	expect(
		!std::equal(
			scene->raster().graphics(Page::zero).begin(),
			scene->raster().graphics(Page::zero).end(),
			scene->raster().graphics(Page::one).begin()
		),
		"first polygon update changes only the actual access page"
	);

	const auto both = scene->advance({.controls = {.up = true, .down = true}});
	expect(
		both && both->selection_changed && both->surface_changed &&
			scene->selection() == 0U &&
			scene->phase() == ClassicOpMusicRoomPhase::held_release_poll,
		"simultaneous Up then Down performs both source branches"
	);
	const auto held = scene->advance({.controls = {.down = true}});
	expect(
		held && held->boundary == ClassicOpMusicRoomBoundary::vsync &&
			!held->selection_changed && scene->selection() == 0U,
		"held directional input animates without repeating the selection"
	);
	release_to_controls(*scene);

	expect(
		scene->advance({.controls = {.down = true}}).has_value() &&
			scene->selection() == 1U,
		"Down advances to the next track"
	);
	for(const auto page : {Page::zero, Page::one}) {
		expect(
			scene->raster().pixel(page, {16, 40}) == 3U &&
				scene->raster().pixel(page, {16, 56}) == 15U,
			"selection labels are redrawn on both pages before the next VSync"
		);
	}
	release_to_controls(*scene);
	expect(
		scene->advance({.controls = {.up = true}}).has_value() &&
			scene->selection() == 0U,
		"Up returns to track zero"
	);
	release_to_controls(*scene);
	expect(
		scene->advance({.controls = {.up = true}}).has_value() &&
			scene->selection() == 20U,
		"Up wraps from track zero to Quit"
	);
	release_to_controls(*scene);
	expect(
		scene->advance({.controls = {.up = true}}).has_value() &&
			scene->selection() == 18U,
		"Up skips source-semantic empty row 19"
	);
	release_to_controls(*scene);
	expect(
		scene->advance({.controls = {.down = true}}).has_value() &&
			scene->selection() == 20U,
		"Down skips source-semantic empty row 19"
	);
	release_to_controls(*scene);
	expect(
		scene->advance({.controls = {.down = true}}).has_value() &&
			scene->selection() == 0U,
		"Down wraps from Quit to track zero"
	);
}

void test_track_audio_exit_and_completion()
{
	GameInit resident{};
	resident.resident_rand = 73;
	auto scene = make_scene({}, resident);
	if(!scene) {
		return;
	}
	release_to_controls(*scene);
	expect(
		scene->advance({.controls = {.down = true}}).has_value() &&
			scene->selection() == 1U,
		"audio test selects track one"
	);
	release_to_controls(*scene);
	const auto selected = scene->advance({
		.controls = {.shot = true, .cancel = true},
	});
	expect(
		selected && selected->phase ==
			ClassicOpMusicRoomPhase::selected_track_frame &&
			scene->carry().track_playing == 1U,
		"Shot loads the track before simultaneous Cancel exits"
	);
	const auto blocked = scene->advance();
	expect(
		!blocked && blocked.error().code ==
			ClassicOpMusicRoomErrorCode::audio_command_pending,
		"scene cannot cross the selected-track VSync before audio is drained"
	);

	const auto stop = scene->take_audio_command();
	const auto load = scene->take_audio_command();
	const auto play = scene->take_audio_command();
	expect(
		stop && stop->type == ClassicAudioCommandType::stop_song &&
			load && load->type == ClassicAudioCommandType::load_song &&
			load->cue == ClassicMusicCue::music_room_track &&
			load->parameter == 1U &&
			play && play->type == ClassicAudioCommandType::play_song &&
			play->cue == ClassicMusicCue::music_room_track &&
			play->parameter == 1U && !scene->take_audio_command(),
		"track selection emits exact Stop, Load, Play ordering"
	);
	const auto second_comment = scene->advance();
	expect(
		second_comment && second_comment->page_flipped &&
			scene->phase() == ClassicOpMusicRoomPhase::exit_release_poll,
		"second page comment is drawn around one intervening polygon VSync"
	);
	const auto held_cancel = scene->advance({.controls = {.cancel = true}});
	expect(
		held_cancel && held_cancel->boundary ==
			ClassicOpMusicRoomBoundary::vsync && held_cancel->page_flipped,
		"exit release loop renders once per held sample"
	);
	const auto finished = scene->advance();
	expect(
		finished && finished->ready &&
			finished->boundary == ClassicOpMusicRoomBoundary::input_poll &&
			!finished->page_flipped,
		"empty exit sample clears and hands off page zero without another VSync"
	);
	expect(
		scene->raster().graphics(Page::zero).size() ==
			th03::present::graphics_page_pixels &&
			scene->raster().shown_page() == Page::zero &&
			scene->tram().cells().size() == 80U * 25U,
		"ready scene accessors retain the completion surface until handoff"
	);

	auto completion = scene->take_completion();
	expect(
		completion && completion->resident().resident_rand == 73 &&
			completion->carry().track_playing == 1U &&
			completion->carry().polygons_initialized,
		"completion returns resident and process-local Music Room carry"
	);
	if(completion) {
		expect(
			completion->raster().shown_page() == Page::zero &&
				completion->raster().access_page() == Page::zero &&
				std::all_of(
					completion->raster().graphics(Page::zero).begin(),
					completion->raster().graphics(Page::zero).end(),
					[](std::uint8_t pixel) { return pixel == 0U; }
				) && completion->raster().palette_tone() == 100,
			"exit completion clears shown page zero to color zero at full tone"
		);
	}
	expect(
		!scene->take_completion() &&
			scene->phase() == ClassicOpMusicRoomPhase::consumed,
		"completion is move-only and single-use"
	);
	const auto consumed = scene->advance();
	expect(
		!consumed && consumed.error().code ==
			ClassicOpMusicRoomErrorCode::invalid_state,
		"consumed scene rejects further advancement"
	);
}

void test_persistent_carry_and_bottom_reset()
{
	auto scene = make_scene();
	if(!scene) {
		return;
	}
	const auto first_frame = scene->advance({.controls = {.cancel = true}});
	expect(first_frame.has_value(), "held key initializes persistent polygons");
	const auto carried_after_frame = scene->carry();
	release_to_controls(*scene);
	expect(
		scene->advance({.controls = {.cancel = true}}).has_value(),
		"control-loop Cancel enters exit release"
	);
	expect(scene->advance().has_value(), "empty exit sample completes scene");
	auto completion = scene->take_completion();
	expect(completion.has_value(), "persistent-carry completion is available");
	if(!completion) {
		return;
	}
	auto resumed = make_scene(completion->carry());
	if(!resumed) {
		return;
	}
	expect(
		resumed->carry() == carried_after_frame,
		"re-entry does not reinitialize process-local polygon state"
	);
	release_to_controls(*resumed);
	const auto seed_before = resumed->carry().random_seed;
	expect(resumed->advance().has_value(), "resumed polygon frame advances");
	expect(
		resumed->carry().random_seed == seed_before,
		"ordinary resumed motion consumes no random values"
	);

	ClassicOpMusicRoomCarry reset_carry{};
	reset_carry.polygons_initialized = true;
	reset_carry.random_seed = 1U;
	for(auto &polygon : reset_carry.polygons) {
		polygon.center_x = 100;
		polygon.center_y_q12_4 = 0;
		polygon.velocity_x = 1;
	}
	reset_carry.polygons[0U] = {100, 7999, 1, 32, 0U, 1U};
	auto reset = make_scene(reset_carry);
	if(!reset) {
		return;
	}
	release_to_controls(*reset);
	expect(reset->advance().has_value(), "bottom-reset frame advances");
	expect(
		reset->carry().random_seed == 0xAD889112U &&
			reset->carry().polygons[0U] == ClassicOpMusicPolygon{
				346, -1600, 6, 64, 66U, 4U,
			},
		"bottom reset consumes five ordered IRAND calls and publishes next state"
	);
}

std::optional<std::vector<std::byte>> read_file(
	const std::filesystem::path &path
)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if(!file) {
		return std::nullopt;
	}
	const auto end = file.tellg();
	if(end < 0) {
		return std::nullopt;
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(end));
	file.seekg(0, std::ios::beg);
	if(!bytes.empty() && !file.read(
		reinterpret_cast<char *>(bytes.data()),
		static_cast<std::streamsize>(bytes.size())
	)) {
		return std::nullopt;
	}
	return bytes;
}

void local_original_gate(
	const std::filesystem::path &content_root,
	const std::filesystem::path &op_executable_path,
	const std::filesystem::path &font_rom_path
)
{
	const auto op_executable = read_file(op_executable_path);
	const auto font_rom = read_file(font_rom_path);
	expect(
		op_executable.has_value() && font_rom.has_value(),
		"local Music Room gate inputs read"
	);
	if(!op_executable || !font_rom) {
		return;
	}
	const auto font = th03::app::Pc98FontRomProvider::from_exact_font_rom(
		*font_rom
	);
	expect(font.has_value(), "local exact FONT.ROM creates text provider");
	if(!font) {
		return;
	}

	th03::app::MainResourceLoader loader(content_root);
	const auto title_resources = loader.load_op_title_menu();
	const auto music_resources = loader.load_op_music_room();
	expect(
		title_resources.has_value() && music_resources.has_value() &&
			music_resources->size() == 21U,
		"local title and complete Music Room closures authenticate"
	);
	if(!title_resources || !music_resources) {
		return;
	}

	GameInit resident{};
	resident.fast_op_animation = 1U;
	auto title = th03::app::ClassicOpTitleMenuScene::create(
		resident, th03::app::ClassicOpTitleMode::fast_fade, *title_resources
	);
	expect(title.has_value(), "local fast title creates for Music Room entry");
	if(!title) {
		return;
	}
	expect((*title)->advance().has_value(), "local title black frame advances");
	const auto title_stop = (*title)->take_audio_command();
	const auto title_load = (*title)->take_audio_command();
	expect(
		title_stop && title_stop->type == ClassicAudioCommandType::stop_song &&
			title_load && title_load->type == ClassicAudioCommandType::load_song,
		"local title startup audio drains"
	);
	expect((*title)->advance().has_value(), "local title initial frame advances");
	expect(
		(*title)->take_audio_command().has_value(),
		"local title play command drains"
	);
	for(std::uint16_t frame = 0U; frame < 25U; ++frame) {
		expect((*title)->advance().has_value(), "local title fade advances");
	}
	expect(
		(*title)->advance({.ok = true}).has_value(),
		"local title accepts the pre-menu input"
	);
	for(std::uint16_t frame = 0U; frame < 14U; ++frame) {
		expect((*title)->advance().has_value(), "local title box step advances");
	}
	expect((*title)->advance().has_value(), "local title main menu draws");
	for(std::uint8_t row = 0U; row < 2U; ++row) {
		expect(
			(*title)->advance({.down = true}).has_value(),
			"local title selection advances toward Music Room"
		);
		expect(
			(*title)->advance().has_value(),
			"local title selection release advances"
		);
	}
	expect(
		(*title)->advance({.ok = true}).has_value(),
		"local title confirms Music Room"
	);
	auto title_completion = (*title)->take_completion();
	expect(
		title_completion && title_completion->route() ==
			th03::app::ClassicOpRoute::music_room,
		"local title publishes typed Music Room route"
	);
	if(!title_completion) {
		return;
	}

	auto reordered_resources = *music_resources;
	std::swap(reordered_resources[0U], reordered_resources[1U]);
	auto rejected = ClassicOpMusicRoomScene::create_from_op(
		*title_completion, {}, reordered_resources, *op_executable, *font
	);
	expect(
		!rejected && rejected.error().code ==
			ClassicOpMusicRoomErrorCode::resource_set_mismatch &&
			title_completion->route() == th03::app::ClassicOpRoute::music_room,
		"failed resource adoption leaves title completion retryable"
	);
	const std::array<std::byte, 2U> malformed_op{
		std::byte{0x4DU}, std::byte{0x5AU},
	};
	rejected = ClassicOpMusicRoomScene::create_from_op(
		*title_completion, {}, *music_resources, malformed_op, *font
	);
	expect(
		!rejected && rejected.error().code ==
			ClassicOpMusicRoomErrorCode::compiled_text_import_failed &&
			title_completion->route() == th03::app::ClassicOpRoute::music_room,
		"failed compiled-label adoption leaves title completion retryable"
	);
	auto music = ClassicOpMusicRoomScene::create_from_op(
		*title_completion, {}, *music_resources, *op_executable, *font
	);
	expect(
		music.has_value() &&
			(*music)->phase() == ClassicOpMusicRoomPhase::held_release_poll,
		"authenticated title completion adopts into Music Room"
	);
	if(!music) {
		return;
	}
	release_to_controls(**music);
	expect(
		(*music)->advance({.controls = {.down = true}}).has_value() &&
			(*music)->selection() == 1U,
		"local Music Room selects the second authenticated track"
	);
	release_to_controls(**music);
	expect(
		(*music)->advance({.controls = {.shot = true}}).has_value(),
		"local Music Room confirms the authenticated track"
	);
	const auto stop = (*music)->take_audio_command();
	const auto load = (*music)->take_audio_command();
	const auto play = (*music)->take_audio_command();
	expect(
		stop && stop->type == ClassicAudioCommandType::stop_song &&
			load && load->type == ClassicAudioCommandType::load_song &&
			load->cue == ClassicMusicCue::music_room_track &&
			load->parameter == 1U &&
			play && play->type == ClassicAudioCommandType::play_song &&
			play->cue == ClassicMusicCue::music_room_track &&
			play->parameter == 1U,
		"local track selection publishes stable indexed audio commands"
	);
	expect(
		(*music)->advance().has_value(),
		"local selected-track presentation frame advances"
	);
	release_to_controls(**music);
	expect(
		(*music)->advance({.controls = {.cancel = true}}).has_value(),
		"local Music Room accepts Cancel"
	);
	expect((*music)->advance().has_value(),
		"local Music Room reaches its typed completion");
	auto music_completion = (*music)->take_completion();
	expect(music_completion.has_value(),
		"local Music Room returns through its typed completion");
	if(!music_completion) {
		return;
	}
	auto returned = th03::app::ClassicOpTitleMenuScene::return_from_music_room(
		*music_completion, *title_resources
	);
	expect(
		returned && (*returned)->phase() == th03::app::ClassicOpPhase::fast_black &&
			(*returned)->resident().fast_op_animation ==
				music_completion->resident().fast_op_animation &&
			!(*returned)->take_audio_command(),
		"Music Room return starts the ordinary fast title without toggling state"
	);
}

} // namespace

int main(int argc, char **argv)
{
	test_asset_validation_and_initial_presentation();
	test_input_flip_and_polygon_cadence();
	test_track_audio_exit_and_completion();
	test_persistent_carry_and_bottom_reset();
	if(argc == 4) {
		local_original_gate(argv[1], argv[2], argv[3]);
	} else {
		expect(
			argc == 1,
			"optional local gate accepts content root, OP.EXE, and FONT.ROM"
		);
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
