#include "th03/app/new_classic_playback.hpp"

#include "classic_mainl_stage_internal.hpp"
#include "classic_input_bits_internal.hpp"
#include "classic_mainl_win_internal.hpp"
#include "classic_story_session_internal.hpp"
#include "main_process_static_content_internal.hpp"
#include "new_classic_language_internal.hpp"
#include "new_classic_playback_internal.hpp"
#include "main_process_driver_internal.hpp"
#include "th03/app/new_classic_pause.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace th03::app {
namespace {

[[nodiscard]] NewClassicPlaybackError playback_error(
	NewClassicPlaybackErrorCode code
) noexcept
{
	return {.code = code};
}

[[nodiscard]] std::expected<std::vector<std::byte>, NewClassicPlaybackError>
read_file(const std::filesystem::path &path)
{
	std::error_code system;
	const auto status = std::filesystem::status(path, system);
	if(system || !std::filesystem::is_regular_file(status)) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::file_missing
		));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system || size > static_cast<std::uintmax_t>(
		std::numeric_limits<std::size_t>::max()
	) || size > static_cast<std::uintmax_t>(
		std::numeric_limits<std::streamsize>::max()
	)) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::file_read_failed
		));
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::file_read_failed
		));
	}
	std::vector<std::byte> bytes(static_cast<std::size_t>(size));
	if(!bytes.empty()) {
		input.read(
			reinterpret_cast<char *>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
	}
	if(!input || input.peek() != std::ifstream::traits_type::eof()) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::file_read_failed
		));
	}
	return bytes;
}

std::expected<StepResult, MainProcessError> process_step(
	void *context,
	CoreInput input,
	InputContext input_context
) noexcept
{
	return static_cast<MainProcess *>(context)->step(input, input_context);
}

bool process_deferred_pending(void *context) noexcept
{
	return static_cast<MainProcess *>(context)->deferred_frame_pending();
}

std::expected<StepResult, MainProcessError> process_finish_deferred(
	void *context,
	DeferredFrameAction action
) noexcept
{
	return static_cast<MainProcess *>(context)->finish_deferred_frame(action);
}

std::expected<void, MainProcessError> process_repeat(void *context) noexcept
{
	return static_cast<MainProcess *>(context)->start_next_round();
}

std::expected<void, MainProcessError> process_finish(void *context) noexcept
{
	return static_cast<MainProcess *>(context)->finish_main_process();
}

std::expected<SessionTransition, MainProcessError> process_transition(
	void *context,
	std::optional<ContinueChoice> choice
) noexcept
{
	return static_cast<MainProcess *>(context)->session_transition(choice);
}

constexpr std::array<std::array<std::uint8_t, 7U>, 8U> overlay_glyphs{{
	{{0x1EU,0x11U,0x11U,0x1EU,0x14U,0x12U,0x11U}},
	{{0x1FU,0x10U,0x10U,0x1EU,0x10U,0x10U,0x1FU}},
	{{0x1EU,0x11U,0x11U,0x1EU,0x10U,0x10U,0x10U}},
	{{0x10U,0x10U,0x10U,0x10U,0x10U,0x10U,0x1FU}},
	{{0x0EU,0x11U,0x11U,0x1FU,0x11U,0x11U,0x11U}},
	{{0x11U,0x11U,0x0AU,0x04U,0x04U,0x04U,0x04U}},
	{{0x1FU,0x10U,0x10U,0x1EU,0x10U,0x10U,0x10U}},
	{{0x1FU,0x10U,0x10U,0x1EU,0x10U,0x10U,0x10U}},
}};

void draw_overlay_word(
	std::span<present::ComposedPixel, present::graphics_page_pixels> output,
	std::span<const std::size_t> glyphs,
	std::size_t x,
	std::size_t y
) noexcept
{
	for(const auto glyph_index : glyphs) {
		const auto &glyph = overlay_glyphs[glyph_index];
		for(std::size_t row = 0U; row < glyph.size(); ++row) {
			for(std::size_t column = 0U; column < 5U; ++column) {
				if((glyph[row] & (0x10U >> column)) != 0U) {
					const auto offset = (y + row) * present::graphics_width +
						x + column;
					output[offset] = {
						present::ComposedPixelLayer::text, 5U
					};
				}
			}
		}
		x += 6U;
	}
}

[[nodiscard]] ClassicStorySessionInput playback_story_input(
	std::uint16_t source,
	ClassicAudioStatus audio
) noexcept
{
	const auto pressed = [source](std::uint16_t bit) noexcept {
		return (source & bit) != 0U;
	};
	const bool any = source != 0U;
	ClassicStorySessionInput input{};
	input.main.aggregate.system = source;
	input.cutscene.script = {
		.cancel_held = pressed(input_cancel),
		.any_key_held = any,
	};
	input.cutscene.shot_or_ok_held =
		pressed(input_shot) || pressed(input_ok);
	input.cutscene.audio = audio;
	input.staff = {
		.audio = audio,
		.any_input = any,
		.fast_forward_held = false,
		.vsync_count_above_one = false,
	};
	input.registration = {
		.raw_any = any,
		.up = pressed(input_up),
		.down = pressed(input_down),
		.left = pressed(input_left),
		.right = pressed(input_right),
		.ok = pressed(input_ok),
		.shot = pressed(input_shot),
		.bomb = pressed(input_bomb),
		.cancel = pressed(input_cancel),
	};
	input.continue_menu = {
		.left = pressed(input_left),
		.right = pressed(input_right),
		.ok = pressed(input_ok),
		.shot = pressed(input_shot),
		.cancel = pressed(input_cancel),
	};
	input.audio = audio;
	input.dismiss_pressed = any;
	return input;
}

} // namespace

struct NewClassicReplayPlayback::Impl final {
	formats::ReplayDocument document{};
	std::shared_ptr<MainResourceLoader> loader{};
	MainCompiledContent compiled_content;
	std::vector<std::byte> font{};
	std::shared_ptr<const MainProcessStaticContent> static_content{};
	MainlStageTextAssets stage_text{};
	MainlStaffCompiledAssets staff_compiled{};
	std::filesystem::path score_path{};
	std::filesystem::path legacy_score_path{};
	present::KotohimeBombFadeCompatibility kotohime_compatibility{};
	std::unique_ptr<MainProcess> process{};
	std::unique_ptr<MainProcessDriver> driver{};
	std::unique_ptr<ClassicStorySession> story{};
	std::unique_ptr<ClassicMainlWinScene> vs_win{};
	present::Tram blank_tram{};
	GameInit process_start{};
	std::size_t active_checkpoint = 0U;
	std::size_t record_index = 0U;
	std::uint32_t sample_index = 0U;
	bool in_mainl = false;
	bool skip_mainl = false;
	bool fast_forward = false;
	bool reduced_flash = false;
	NewClassicPlaybackStatus status = NewClassicPlaybackStatus::running;
	std::optional<ClassicAudioCommand> load_song{};
	std::array<ClassicAudioCommand, 2U> ui_audio{};
	std::size_t ui_audio_index = 0U;
	std::size_t ui_audio_count = 0U;
	std::optional<NewClassicPauseMenu> pause_menu{};

	Impl(
		formats::ReplayDocument document_,
		std::shared_ptr<MainResourceLoader> loader_,
		MainCompiledContent compiled_content_,
		std::vector<std::byte> font_,
		std::shared_ptr<const MainProcessStaticContent> static_content_,
		MainlStageTextAssets stage_text_,
		MainlStaffCompiledAssets staff_compiled_,
		std::filesystem::path score_path_,
		std::filesystem::path legacy_score_path_,
		present::KotohimeBombFadeCompatibility compatibility,
		bool reduced_flash_
	) :
		document(std::move(document_)),
		loader(std::move(loader_)),
		compiled_content(std::move(compiled_content_)),
		font(std::move(font_)),
		static_content(std::move(static_content_)),
		stage_text(std::move(stage_text_)),
		staff_compiled(std::move(staff_compiled_)),
		score_path(std::move(score_path_)),
		legacy_score_path(std::move(legacy_score_path_)),
		kotohime_compatibility(compatibility),
		reduced_flash(reduced_flash_)
	{
		blank_tram.fill({
			' ', static_cast<present::TramAttribute>(present::tram_black)
		});
	}

	[[nodiscard]] std::expected<void, NewClassicPlaybackError>
	start_checkpoint(std::size_t checkpoint_index)
	{
		if(checkpoint_index >= document.checkpoints.size()) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::invalid_request
			));
		}
		const auto &checkpoint = document.checkpoints[checkpoint_index];
		const auto player_one = main_player_selection_from_paletted(
			checkpoint.init.paletted_character[0U]
		);
		const auto player_two = main_player_selection_from_paletted(
			checkpoint.init.paletted_character[1U]
		);
		if(!player_one || !player_two) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::invalid_request
			));
		}
		auto resources = loader->load_direct_vs(*player_one, *player_two);
		if(!resources) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::resource_failed
			);
			error.resource = std::move(resources.error());
			return std::unexpected(std::move(error));
		}
		auto created = MainProcess::create_replay(
			{
				.checkpoint = checkpoint.init,
				.game_options = {
					.input_protocol = InputProtocol::recorded_replay,
					.three_digit_combos = true,
				},
				.initialize_fresh = !document.practice,
				.kotohime_compatibility = kotohime_compatibility,
			},
			*resources,
			compiled_content,
			font
		);
		if(!created) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::process_failed
			);
			error.process = std::move(created.error());
			return std::unexpected(std::move(error));
		}
		process = std::move(*created);
		auto created_driver = MainProcessDriver::create({
			.context = process.get(),
			.step = &process_step,
			.deferred_frame_pending = &process_deferred_pending,
			.finish_deferred_frame = &process_finish_deferred,
			.start_next_round = &process_repeat,
			.finish_main_process = &process_finish,
			.session_transition = &process_transition,
		});
		if(!created_driver) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::driver_failed
			));
		}
		driver = std::move(*created_driver);
		active_checkpoint = checkpoint_index;
		process_start = checkpoint.init;
		// MainProcess::create_replay() applies the same fresh-start projection
		// before constructing the process.  Keep the retained handoff resident
		// identical to that effective process start so the shared Story MAINL
		// owner receives a source-valid resident without replay-only snapshots.
		if(!document.practice) {
			process_start.compact_runtime_snapshot.reset();
			process_start.round_start_snapshot.reset();
			process_start.practice_rules.reset();
		}
		sample_index = checkpoint.sample_index;
		in_mainl = false;
		skip_mainl = false;
		story.reset();
		vs_win.reset();
		load_song = classic_main_gameplay_song(checkpoint.init);
		if(!load_song) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::invalid_request
			));
		}
		return {};
	}

	[[nodiscard]] bool final_story_clear() const noexcept
	{
		return process && driver && driver->transition() &&
			process_start.game_mode == game_mode::story &&
			process->canonical_state().route == 2U &&
			process->canonical_state().winner == 0 &&
			process->canonical_state().story_stage == 8U &&
			driver->transition()->action == SessionAction::ending;
	}

	[[nodiscard]] std::expected<void, NewClassicPlaybackError>
	start_mainl_owner()
	{
		if(!process || !driver ||
			driver->status() != MainProcessDriverStatus::terminal ||
			!driver->transition()) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		const auto terminal = process->canonical_state();
		if(process_start.game_mode == game_mode::story) {
			if(final_story_clear()) {
				skip_mainl = true;
				return {};
			}
			auto created = ClassicStorySession::create_replay_interstitial(
				{
					.resident = process_start,
					.game_options = {
						.input_protocol = InputProtocol::recorded_replay,
						.three_digit_combos = true,
					},
					.kotohime_compatibility = kotohime_compatibility,
				},
				loader, static_content, stage_text, staff_compiled, score_path,
				legacy_score_path, std::move(process), std::move(driver)
			);
			if(!created) {
				auto error = playback_error(
					NewClassicPlaybackErrorCode::interstitial_failed
				);
				error.interstitial_detail = static_cast<std::uint8_t>(
					created.error().code
				);
				return std::unexpected(std::move(error));
			}
			story = std::move(*created);
			return {};
		}
		if(process_start.game_mode < game_mode::vs_1p_cpu ||
			process_start.game_mode > game_mode::vs_cpu_cpu ||
			terminal.route != 2U || terminal.winner < 0 || terminal.winner > 1) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		const auto winner = static_cast<std::size_t>(terminal.winner);
		const auto loser = 1U - winner;
		const auto winner_selection = main_player_selection_from_paletted(
			process_start.paletted_character[winner]
		);
		const auto loser_selection = main_player_selection_from_paletted(
			process_start.paletted_character[loser]
		);
		if(!winner_selection || !loser_selection) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		auto resources = loader->load_mainl_win(*winner_selection);
		if(!resources) {
			auto error = playback_error(NewClassicPlaybackErrorCode::resource_failed);
			error.resource = std::move(resources.error());
			return std::unexpected(std::move(error));
		}
		auto created = ClassicMainlWinScene::create(
			{
				.winner_paletted_character =
					process_start.paletted_character[winner],
				.loser_paletted_character =
					process_start.paletted_character[loser],
				.message_block = static_cast<std::uint8_t>(
					(static_cast<std::uint8_t>(
						process_start.paletted_character[loser]
					) - 1U) / 2U
				),
				.preload_stage_splash = false,
			},
			*resources, static_content->font()
		);
		if(!created) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::interstitial_failed
			);
			error.interstitial_detail = static_cast<std::uint8_t>(
				created.error().code
			);
			return std::unexpected(std::move(error));
		}
		vs_win = std::move(*created);
		return {};
	}

	[[nodiscard]] bool process_start_checkpoint(std::size_t checkpoint_index)
		const noexcept
	{
		if(checkpoint_index >= document.checkpoints.size()) {
			return false;
		}
		const auto sample = document.checkpoints[checkpoint_index].sample_index;
		if(sample == 0U) {
			return true;
		}
		return std::any_of(
			document.session.records.begin(), document.session.records.end(),
			[sample](const replay::SessionRecord &record) {
				const auto *boundary = std::get_if<replay::ProcessBoundary>(&record);
				return boundary && boundary->sample_index == sample &&
					boundary->completed_process == replay::ProcessId::mainl;
			}
		);
	}

	[[nodiscard]] std::optional<std::size_t> next_process_checkpoint() const
		noexcept
	{
		for(std::size_t index = active_checkpoint + 1U;
			index < document.checkpoints.size(); ++index) {
			if(document.checkpoints[index].sample_index == sample_index) {
				return index;
			}
			if(document.checkpoints[index].sample_index > sample_index) {
				break;
			}
		}
		return std::nullopt;
	}
};

std::expected<std::unique_ptr<NewClassicReplayPlayback>, NewClassicPlaybackError>
NewClassicReplayPlayback::load(NewClassicPlaybackRequest request)
{
	if(request.content.data_root.empty() ||
		request.content.main_executable.empty() ||
		request.content.mainl_executable.empty() ||
		request.content.font_rom.empty() ||
		request.content.score.empty() ||
		!formats::replay_can_be_verified(request.document) ||
		request.checkpoint_index >= request.document.checkpoints.size()) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::invalid_request
		));
	}
	auto main = read_file(request.content.main_executable);
	if(!main) {
		return std::unexpected(std::move(main.error()));
	}
	auto compiled = import_main_compiled_content(*main);
	if(!compiled) {
		auto error = playback_error(
			NewClassicPlaybackErrorCode::compiled_content_failed
		);
		error.compiled_content = std::move(compiled.error());
		return std::unexpected(std::move(error));
	}
	auto font = read_file(request.content.font_rom);
	if(!font) {
		return std::unexpected(std::move(font.error()));
	}
	auto mainl = read_file(request.content.mainl_executable);
	if(!mainl) {
		return std::unexpected(std::move(mainl.error()));
	}
	auto mainl_text = import_mainl_stage_text(*mainl);
	if(!mainl_text) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::mainl_content_failed
		));
	}
	auto loader = std::make_shared<MainResourceLoader>(
		request.content.data_root
	);
	if(request.language == ClassicLanguage::english) {
		auto overlay = load_new_classic_language_overlay(
			request.content.language_overlay
		);
		if(!overlay) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::language_overlay_failed
			);
			error.language_overlay.emplace(std::move(overlay.error()));
			return std::unexpected(std::move(error));
		}
		if(!loader->install_authenticated_overrides(
			std::move(*overlay).release_members()
		)) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::language_overlay_failed
			));
		}
		OpMusicRoomTextImport unused_music{};
		apply_new_classic_english_text(
			*compiled, mainl_text->assets, unused_music
		);
	}
	auto static_resources = loader->load_main_static();
	if(!static_resources) {
		auto error = playback_error(NewClassicPlaybackErrorCode::resource_failed);
		error.resource = std::move(static_resources.error());
		return std::unexpected(std::move(error));
	}
	auto static_content = MainProcessStaticContentAccess::create(
		*static_resources, *compiled, *font
	);
	if(!static_content) {
		auto error = playback_error(
			NewClassicPlaybackErrorCode::static_content_failed
		);
		error.process = std::move(static_content.error());
		return std::unexpected(std::move(error));
	}
	const auto legacy_score_path = request.content.score;
	const auto score_path = legacy_score_path.parent_path() / "YUME.NEW.NEM";
	auto impl = std::make_unique<Impl>(
		std::move(request.document), std::move(loader),
		std::move(*compiled), std::move(*font), std::move(*static_content),
		std::move(mainl_text->assets), std::move(mainl_text->staff),
		score_path, legacy_score_path, request.kotohime_compatibility,
		request.reduced_flash
	);
	const auto target_sample =
		impl->document.checkpoints[request.checkpoint_index].sample_index;
	std::size_t start_checkpoint = request.checkpoint_index;
	while(start_checkpoint != 0U &&
		!impl->process_start_checkpoint(start_checkpoint)) {
		--start_checkpoint;
	}
	if(!impl->process_start_checkpoint(start_checkpoint)) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::replay_contract_failed
		));
	}
	const auto sample_index =
		impl->document.checkpoints[start_checkpoint].sample_index;
	const auto found = std::find_if(
		impl->document.session.records.begin(),
		impl->document.session.records.end(),
		[sample_index](const replay::SessionRecord &record) {
			const auto *sample = std::get_if<replay::InputSample>(&record);
			return sample && sample->frame_index == sample_index;
		}
	);
	if(found == impl->document.session.records.end()) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::replay_contract_failed
		));
	}
	impl->record_index = static_cast<std::size_t>(std::distance(
		impl->document.session.records.begin(), found
	));
	if(auto started = impl->start_checkpoint(start_checkpoint); !started) {
		return std::unexpected(std::move(started.error()));
	}
	auto result = std::unique_ptr<NewClassicReplayPlayback>(
		new NewClassicReplayPlayback(std::move(impl))
	);
	while(result->sample_index() < target_sample &&
		result->status() == NewClassicPlaybackStatus::running) {
		auto advanced = result->advance({}, false, false);
		if(!advanced) {
			return std::unexpected(std::move(advanced.error()));
		}
		while(result->take_audio_command()) {
		}
	}
	if(result->sample_index() != target_sample ||
		result->status() != NewClassicPlaybackStatus::running) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::replay_contract_failed
		));
	}
	return result;
}

NewClassicReplayPlayback::NewClassicReplayPlayback(
	std::unique_ptr<Impl> impl
) noexcept : impl_(std::move(impl)) {}

NewClassicReplayPlayback::~NewClassicReplayPlayback() = default;

std::expected<NewClassicPlaybackTick, NewClassicPlaybackError>
NewClassicReplayPlayback::advance(
	ClassicAudioStatus audio,
	bool abort_requested,
	bool application_quit_requested,
	std::uint16_t live_system_input
)
{
	if(impl_->status != NewClassicPlaybackStatus::running) {
		return NewClassicPlaybackTick{.status = impl_->status};
	}
	if(application_quit_requested) {
		impl_->status = NewClassicPlaybackStatus::application_quit;
		return NewClassicPlaybackTick{.status = impl_->status};
	}
	if(abort_requested) {
		impl_->status = NewClassicPlaybackStatus::aborted;
		return NewClassicPlaybackTick{.status = impl_->status};
	}
	if(impl_->record_index >= impl_->document.session.records.size()) {
		impl_->status = NewClassicPlaybackStatus::complete;
		return NewClassicPlaybackTick{.status = impl_->status};
	}
	if(impl_->pause_menu) {
		// Replay Patch 0.4.6-rc8 removed the VSync wait from replay-owned
		// Pause samples. Consume the complete recorded menu transaction in
		// order at this one host boundary; physical Esc/application quit were
		// already honored above, and PMD remains paused until the final action.
		NewClassicPlaybackTick result{
			.surface_changed = false,
			.process_advanced = false,
			.status = impl_->status,
		};
		for(;;) {
			if(impl_->record_index >= impl_->document.session.records.size()) {
				return std::unexpected(playback_error(
					NewClassicPlaybackErrorCode::replay_contract_failed
				));
			}
			const auto *sample = std::get_if<replay::InputSample>(
				&impl_->document.session.records[impl_->record_index]
			);
			if(!sample || sample->frame_index != impl_->sample_index ||
				impl_->in_mainl ||
				sample->phase != replay::InputPhase::interstitial ||
				impl_->driver->input_context() != InputContext::pause) {
				return std::unexpected(playback_error(
					NewClassicPlaybackErrorCode::replay_contract_failed
				));
			}
			const auto pause_status = impl_->pause_menu->consume(
				project_new_classic_pause_input(sample->input)
			);
			if(impl_->pause_menu->take_rejected_save_effect()) {
				impl_->ui_audio = {{
					{.type = ClassicAudioCommandType::play_effect, .parameter = 21U},
					{},
				}};
				impl_->ui_audio_index = 0U;
				impl_->ui_audio_count = 1U;
			}
			result.surface_changed = true;
			result.process_advanced = true;
			if(pause_status == NewClassicPauseStatus::running) {
				++impl_->record_index;
				++impl_->sample_index;
				continue;
			}
			const bool resume = pause_status == NewClassicPauseStatus::resume;
			if(!resume && pause_status != NewClassicPauseStatus::save_exit) {
				return std::unexpected(playback_error(
					NewClassicPlaybackErrorCode::replay_contract_failed
				));
			}
			auto advanced = impl_->driver->finish_deferred_frame(
				resume ? DeferredFrameAction::resume :
					DeferredFrameAction::exit_session
			);
			impl_->pause_menu.reset();
			if(resume) {
				impl_->ui_audio = {{
					{.type = ClassicAudioCommandType::resume_song},
					{.type = ClassicAudioCommandType::play_effect, .parameter = 21U},
				}};
				impl_->ui_audio_index = 0U;
				impl_->ui_audio_count = impl_->ui_audio.size();
			}
			if(!advanced) {
				auto error = playback_error(
					NewClassicPlaybackErrorCode::driver_failed
				);
				if(advanced.error().process_error) {
					error.process = std::move(*advanced.error().process_error);
				}
				return std::unexpected(std::move(error));
			}
			if(advanced->process_advanced) {
				++impl_->record_index;
				++impl_->sample_index;
			}
			result.surface_changed = result.surface_changed ||
				advanced->surface_changed;
			result.process_advanced = result.process_advanced ||
				advanced->process_advanced;
			return result;
		}
	}

	const auto &record = impl_->document.session.records[impl_->record_index];
	if(const auto *sample = std::get_if<replay::InputSample>(&record)) {
		if(sample->frame_index != impl_->sample_index) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		if(impl_->in_mainl) {
			if(sample->phase != replay::InputPhase::interstitial) {
				return std::unexpected(playback_error(
					NewClassicPlaybackErrorCode::replay_contract_failed
				));
			}
			if(impl_->skip_mainl) {
				++impl_->record_index;
				++impl_->sample_index;
				return NewClassicPlaybackTick{.status = impl_->status};
			}
			if(impl_->story) {
				if(impl_->story->scene() ==
					ClassicStorySessionScene::high_score_registration) {
					// Replay Patch 0.4.10 leaves regist_menu() on the live
					// input_mode_interface() path.  It therefore contributes no
					// replay samples; any samples left before MAINL_END belong to
					// the preceding replay-aware owner and are only transition
					// padding from the native owner's lower rendering cost.
					++impl_->record_index;
					++impl_->sample_index;
					return NewClassicPlaybackTick{.status = impl_->status};
				}
				if(impl_->story->status() ==
					ClassicStorySessionStatus::complete) {
					// Replay Patch's MAINL transition finisher drains any
					// remaining VSync-sampled input up to MAINL_END.  Native
					// raster work is not tied to PC-98 rendering cost, so a
					// typed scene can legitimately finish before that marker.
					++impl_->record_index;
					++impl_->sample_index;
					return NewClassicPlaybackTick{.status = impl_->status};
				}
				const auto source = sample->input.system;
				auto input = playback_story_input(source, audio);
				input.main.aggregate = sample->input;
				auto advanced = impl_->story->consume(input);
				if(!advanced) {
					auto error = playback_error(
						NewClassicPlaybackErrorCode::interstitial_failed
					);
					error.interstitial_detail =
						ClassicStorySessionTestAccess::failure_detail_code(
							*impl_->story
						).value_or(static_cast<std::uint8_t>(advanced.error().code));
					return std::unexpected(std::move(error));
				}
				if(advanced->input_sampled) {
					++impl_->record_index;
					++impl_->sample_index;
				}
				return NewClassicPlaybackTick{
					.surface_changed = advanced->surface_changed,
					.process_advanced = advanced->process_advanced,
					.status = impl_->status,
				};
			}
			if(impl_->vs_win) {
				if(impl_->vs_win->phase() == ClassicMainlWinPhase::ready) {
					++impl_->record_index;
					++impl_->sample_index;
					return NewClassicPlaybackTick{.status = impl_->status};
				}
				auto advanced = impl_->vs_win->advance(
					audio, sample->input.system != 0U
				);
				if(!advanced) {
					auto error = playback_error(
						NewClassicPlaybackErrorCode::interstitial_failed
					);
					error.interstitial_detail = static_cast<std::uint8_t>(
						advanced.error().code
					);
					return std::unexpected(std::move(error));
				}
				if(advanced->input_sampled) {
					++impl_->record_index;
					++impl_->sample_index;
				}
				return NewClassicPlaybackTick{
					.surface_changed = advanced->surface_changed,
					.status = impl_->status,
				};
			}
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		const auto context = impl_->driver->input_context();
		const auto expected_phase = context == InputContext::gameplay ?
			replay::InputPhase::gameplay : replay::InputPhase::interstitial;
		if(sample->phase != expected_phase) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		std::expected<MainProcessDriverTick, MainProcessDriverError> advanced =
			std::unexpected(MainProcessDriverError{});
		advanced = impl_->driver->consume(sample->input);
		if(advanced && impl_->driver->deferred_frame_pending()) {
			impl_->pause_menu.emplace(true);
			impl_->ui_audio = {{
				{.type = ClassicAudioCommandType::pause_song},
				{.type = ClassicAudioCommandType::play_effect, .parameter = 21U},
			}};
			impl_->ui_audio_index = 0U;
			impl_->ui_audio_count = impl_->ui_audio.size();
		}
		if(!advanced) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::driver_failed
			);
			if(advanced.error().process_error) {
				error.process = std::move(*advanced.error().process_error);
			}
			return std::unexpected(std::move(error));
		}
		if(advanced->process_advanced) {
			++impl_->record_index;
			++impl_->sample_index;
		}
		return NewClassicPlaybackTick{
			.surface_changed = advanced->surface_changed,
			.process_advanced = advanced->process_advanced,
			.status = impl_->status,
		};
	}

	const auto &boundary = std::get<replay::ProcessBoundary>(record);
	if(boundary.sample_index != impl_->sample_index) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::replay_contract_failed
		));
	}
	const bool registration_terminal_ready = impl_->in_mainl && impl_->story &&
		(impl_->story->scene() == ClassicStorySessionScene::continue_menu ||
		 impl_->story->scene() == ClassicStorySessionScene::game_over);
	if(impl_->in_mainl && impl_->story && !registration_terminal_ready &&
		impl_->story->status() ==
		ClassicStorySessionStatus::running && !(
			impl_->story->scene() == ClassicStorySessionScene::stage_splash &&
			impl_->next_process_checkpoint().has_value()
	)) {
		const std::uint16_t source = impl_->story->scene() ==
			ClassicStorySessionScene::high_score_registration ?
			live_system_input : static_cast<std::uint16_t>(0U);
		auto advanced = impl_->story->consume(playback_story_input(source, audio));
		if(!advanced) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::interstitial_failed
			);
			error.interstitial_detail =
				ClassicStorySessionTestAccess::failure_detail_code(
					*impl_->story
				).value_or(static_cast<std::uint8_t>(advanced.error().code));
			return std::unexpected(std::move(error));
		}
		if(advanced->input_sampled && impl_->story->scene() !=
			ClassicStorySessionScene::high_score_registration) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		return NewClassicPlaybackTick{
			.surface_changed = advanced->surface_changed,
			.process_advanced = advanced->process_advanced,
			.status = impl_->status,
		};
	}
	if(impl_->in_mainl && impl_->vs_win &&
		impl_->vs_win->phase() != ClassicMainlWinPhase::ready) {
		auto advanced = impl_->vs_win->advance(audio, false);
		if(!advanced) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::interstitial_failed
			);
			error.interstitial_detail = static_cast<std::uint8_t>(
				advanced.error().code
			);
			return std::unexpected(std::move(error));
		}
		if(advanced->input_sampled) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		return NewClassicPlaybackTick{
			.surface_changed = advanced->surface_changed,
			.status = impl_->status,
		};
	}
	if(!impl_->in_mainl) {
		if(boundary.completed_process != replay::ProcessId::main ||
			impl_->driver->status() != MainProcessDriverStatus::terminal) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::replay_contract_failed
			));
		}
		impl_->in_mainl = true;
		++impl_->record_index;
		if(impl_->record_index >= impl_->document.session.records.size()) {
			impl_->status = NewClassicPlaybackStatus::complete;
		} else if(auto started = impl_->start_mainl_owner(); !started) {
			return std::unexpected(std::move(started.error()));
		}
		return NewClassicPlaybackTick{
			.surface_changed = impl_->status == NewClassicPlaybackStatus::running,
			.status = impl_->status,
		};
	}
	if(boundary.completed_process != replay::ProcessId::mainl) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::replay_contract_failed
		));
	}
	const auto next = impl_->next_process_checkpoint();
	const bool selected_stage_splash_skip = impl_->story &&
		impl_->story->scene() == ClassicStorySessionScene::stage_splash &&
		next.has_value();
	if((impl_->story && impl_->story->status() !=
			ClassicStorySessionStatus::complete &&
			!selected_stage_splash_skip && !registration_terminal_ready) ||
		(impl_->vs_win && impl_->vs_win->phase() !=
			ClassicMainlWinPhase::ready) ||
		(!impl_->story && !impl_->vs_win && !impl_->skip_mainl)) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::replay_contract_failed
		));
	}
	++impl_->record_index;
	impl_->story.reset();
	impl_->vs_win.reset();
	impl_->process.reset();
	impl_->driver.reset();
	if(!next) {
		impl_->status = NewClassicPlaybackStatus::complete;
		return NewClassicPlaybackTick{.status = impl_->status};
	}
	if(auto started = impl_->start_checkpoint(*next); !started) {
		return std::unexpected(std::move(started.error()));
	}
	return NewClassicPlaybackTick{
		.surface_changed = true,
		.status = impl_->status,
	};
}

std::optional<ClassicAudioCommand>
NewClassicReplayPlayback::take_audio_command()
{
	if(impl_->load_song) {
		auto result = impl_->load_song;
		impl_->load_song.reset();
		return result;
	}
	if(impl_->ui_audio_count != 0U) {
		const auto result = impl_->ui_audio[impl_->ui_audio_index++];
		--impl_->ui_audio_count;
		if(impl_->ui_audio_count == 0U) {
			impl_->ui_audio_index = 0U;
		}
		return result;
	}
	if(impl_->story) {
		if(auto command = impl_->story->take_audio_command()) {
			return command;
		}
	}
	if(impl_->process) {
		if(auto command = impl_->process->take_audio_command()) {
			return command;
		}
	}
	if(impl_->vs_win) {
		if(auto command = impl_->vs_win->take_audio_command()) {
			return command;
		}
	}
	return std::nullopt;
}

std::expected<present::SourceSurface, NewClassicPlaybackError>
NewClassicReplayPlayback::compose_shown_surface(
	std::span<present::ComposedPixel, present::graphics_page_pixels> output
) const
{
	std::expected<present::SourceSurface, NewClassicPlaybackError> surface =
		std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::surface_failed
		));
	if(impl_->story) {
		auto composed = impl_->story->compose_shown_surface(output);
		if(!composed) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::interstitial_failed
			);
			error.interstitial_detail = static_cast<std::uint8_t>(
				composed.error().code
			);
			return std::unexpected(std::move(error));
		}
		surface = *composed;
	} else if(impl_->vs_win) {
		auto composed = present::compose_source_surface(
			output, impl_->vs_win->raster(), impl_->blank_tram,
			impl_->static_content->font()
		);
		if(!composed) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::surface_failed
			);
			error.surface = std::move(composed.error());
			return std::unexpected(std::move(error));
		}
		surface = *composed;
	} else if(impl_->process) {
		auto composed = impl_->process->compose_shown_surface(output);
		if(!composed) {
			auto error = playback_error(
				NewClassicPlaybackErrorCode::surface_failed
			);
			error.surface = std::move(composed.error());
			return std::unexpected(std::move(error));
		}
		surface = *composed;
	}
	if(!surface) {
		return std::unexpected(std::move(surface.error()));
	}
	if(impl_->pause_menu) {
		return impl_->pause_menu->compose_over(*surface);
	}
	constexpr std::array<std::size_t, 6U> replay_word{0U, 1U, 2U, 3U, 4U, 5U};
	draw_overlay_word(output, replay_word, 302U, 5U);
	if(impl_->fast_forward && fast_forward_step_count() > 1U) {
		constexpr std::array<std::size_t, 2U> ff_word{6U, 7U};
		draw_overlay_word(output, ff_word, 342U, 5U);
	}
	auto result = *surface;
	result.pixels = output;
	result.host_overlay = true;
	if(impl_->reduced_flash && result.palette_tone == 150) {
		result.graphics_palette = result.reduced_flash_palette;
		result.reduced_flash_applied = true;
	}
	return result;
}

void NewClassicReplayPlayback::set_fast_forward(bool enabled) noexcept
{
	impl_->fast_forward = enabled;
}

bool NewClassicReplayPlayback::fast_forward() const noexcept
{
	return impl_->fast_forward;
}

std::size_t NewClassicReplayPlayback::fast_forward_step_count() const noexcept
{
	if(!impl_->fast_forward) {
		return 1U;
	}
	if(!impl_->in_mainl) {
		return 4U;
	}
	return impl_->story &&
		impl_->story->scene() == ClassicStorySessionScene::staff_verdict ? 4U : 1U;
}

NewClassicPlaybackStatus NewClassicReplayPlayback::status() const noexcept
{
	return impl_->status;
}

NewClassicPlaybackOwner NewClassicReplayPlaybackTestAccess::owner(
	const NewClassicReplayPlayback &playback
) noexcept
{
	const auto &impl = *playback.impl_;
	if(impl.skip_mainl) {
		return NewClassicPlaybackOwner::final_clear_skip;
	}
	if(impl.vs_win) {
		return NewClassicPlaybackOwner::vs_win;
	}
	if(impl.story) {
		switch(impl.story->scene()) {
		case ClassicStorySessionScene::stage_splash:
			return NewClassicPlaybackOwner::story_stage_splash;
		case ClassicStorySessionScene::win:
			return NewClassicPlaybackOwner::story_win;
		case ClassicStorySessionScene::cutscene:
			return NewClassicPlaybackOwner::story_cutscene;
		case ClassicStorySessionScene::staff_verdict:
			return NewClassicPlaybackOwner::story_staff_verdict;
		case ClassicStorySessionScene::high_score_registration:
			return NewClassicPlaybackOwner::story_registration;
		case ClassicStorySessionScene::continue_menu:
			return NewClassicPlaybackOwner::story_continue;
		case ClassicStorySessionScene::game_over:
			return NewClassicPlaybackOwner::story_game_over;
		case ClassicStorySessionScene::main:
			return NewClassicPlaybackOwner::main;
		case ClassicStorySessionScene::complete:
		case ClassicStorySessionScene::failed:
			return NewClassicPlaybackOwner::none;
		}
	}
	return impl.process ? NewClassicPlaybackOwner::main :
		NewClassicPlaybackOwner::none;
}

std::uint32_t NewClassicReplayPlaybackTestAccess::interstitial_sample_count(
	const NewClassicReplayPlayback &playback
) noexcept
{
	return playback.impl_->story ?
		playback.impl_->story->source_sample_count() : playback.impl_->sample_index;
}

std::uint8_t NewClassicReplayPlaybackTestAccess::main_input_context(
	const NewClassicReplayPlayback &playback
) noexcept
{
	return playback.impl_->driver ? static_cast<std::uint8_t>(
		playback.impl_->driver->input_context()
	) : 0xFFU;
}

std::uint32_t NewClassicReplayPlayback::sample_index() const noexcept
{
	return impl_->sample_index;
}

} // namespace th03::app
