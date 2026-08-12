#include "th03/app/new_classic_playback.hpp"

#include "classic_mainl_stage_internal.hpp"
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

} // namespace

struct NewClassicReplayPlayback::Impl final {
	formats::ReplayDocument document{};
	MainResourceLoader loader;
	MainCompiledContent compiled_content;
	std::vector<std::byte> font{};
	present::KotohimeBombFadeCompatibility kotohime_compatibility{};
	std::unique_ptr<MainProcess> process{};
	std::unique_ptr<MainProcessDriver> driver{};
	std::size_t active_checkpoint = 0U;
	std::size_t record_index = 0U;
	std::uint32_t sample_index = 0U;
	bool in_mainl = false;
	bool fast_forward = false;
	bool reduced_flash = false;
	NewClassicPlaybackStatus status = NewClassicPlaybackStatus::running;
	std::optional<ClassicAudioCommand> load_song{};
	std::optional<ClassicAudioCommand> ui_audio{};
	std::optional<NewClassicPauseMenu> pause_menu{};

	Impl(
		formats::ReplayDocument document_,
		std::filesystem::path data_root,
		MainCompiledContent compiled_content_,
		std::vector<std::byte> font_,
		present::KotohimeBombFadeCompatibility compatibility,
		bool reduced_flash_
	) :
		document(std::move(document_)),
		loader(std::move(data_root)),
		compiled_content(std::move(compiled_content_)),
		font(std::move(font_)),
		kotohime_compatibility(compatibility),
		reduced_flash(reduced_flash_)
	{}

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
		auto resources = loader.load_direct_vs(*player_one, *player_two);
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
				.initialize_fresh = true,
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
		sample_index = checkpoint.sample_index;
		in_mainl = false;
		load_song = classic_main_gameplay_song(checkpoint.init);
		if(!load_song) {
			return std::unexpected(playback_error(
				NewClassicPlaybackErrorCode::invalid_request
			));
		}
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
		request.content.font_rom.empty() ||
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
	auto impl = std::make_unique<Impl>(
		std::move(request.document), request.content.data_root,
		std::move(*compiled), std::move(*font), request.kotohime_compatibility,
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
		auto advanced = result->advance(false, false);
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
	bool abort_requested,
	bool application_quit_requested
)
{
	if(impl_->status != NewClassicPlaybackStatus::running) {
		return NewClassicPlaybackTick{.status = impl_->status};
	}
	if(application_quit_requested) {
		impl_->status = NewClassicPlaybackStatus::application_quit;
		return NewClassicPlaybackTick{.status = impl_->status};
	}
	if(abort_requested && !impl_->in_mainl) {
		impl_->status = NewClassicPlaybackStatus::aborted;
		return NewClassicPlaybackTick{.status = impl_->status};
	}
	if(impl_->record_index >= impl_->document.session.records.size()) {
		impl_->status = NewClassicPlaybackStatus::complete;
		return NewClassicPlaybackTick{.status = impl_->status};
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
			++impl_->record_index;
			++impl_->sample_index;
			return NewClassicPlaybackTick{.status = impl_->status};
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
		if(impl_->pause_menu) {
			const auto pause_status = impl_->pause_menu->consume(
				project_new_classic_pause_input(sample->input)
			);
			if(pause_status == NewClassicPauseStatus::running) {
				++impl_->record_index;
				++impl_->sample_index;
				return NewClassicPlaybackTick{
					.surface_changed = true,
					.process_advanced = true,
					.status = impl_->status,
				};
			}
			const bool resume = pause_status == NewClassicPauseStatus::resume;
			if(!resume && pause_status != NewClassicPauseStatus::save_exit) {
				return std::unexpected(playback_error(
					NewClassicPlaybackErrorCode::replay_contract_failed
				));
			}
			advanced = impl_->driver->finish_deferred_frame(
				resume ? DeferredFrameAction::resume :
					DeferredFrameAction::exit_session
			);
			impl_->pause_menu.reset();
			if(resume) {
				impl_->ui_audio = ClassicAudioCommand{
					.type = ClassicAudioCommandType::resume_song,
				};
			}
		} else {
			advanced = impl_->driver->consume(sample->input);
			if(advanced && impl_->driver->deferred_frame_pending()) {
				impl_->pause_menu.emplace(true);
				impl_->ui_audio = ClassicAudioCommand{
					.type = ClassicAudioCommandType::pause_song,
				};
			}
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
		}
		return NewClassicPlaybackTick{.status = impl_->status};
	}
	if(boundary.completed_process != replay::ProcessId::mainl) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::replay_contract_failed
		));
	}
	++impl_->record_index;
	const auto next = impl_->next_process_checkpoint();
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
	if(impl_->process) {
		if(auto command = impl_->process->take_audio_command()) {
			return command;
		}
	}
	if(impl_->ui_audio) {
		auto result = impl_->ui_audio;
		impl_->ui_audio.reset();
		return result;
	}
	return std::nullopt;
}

std::expected<present::SourceSurface, NewClassicPlaybackError>
NewClassicReplayPlayback::compose_shown_surface(
	std::span<present::ComposedPixel, present::graphics_page_pixels> output
) const
{
	if(!impl_->process) {
		return std::unexpected(playback_error(
			NewClassicPlaybackErrorCode::surface_failed
		));
	}
	auto surface = impl_->process->compose_shown_surface(output);
	if(!surface) {
		auto error = playback_error(
			NewClassicPlaybackErrorCode::surface_failed
		);
		error.surface = std::move(surface.error());
		return std::unexpected(std::move(error));
	}
	if(impl_->pause_menu) {
		return impl_->pause_menu->compose_over(*surface);
	}
	constexpr std::array<std::size_t, 6U> replay_word{0U, 1U, 2U, 3U, 4U, 5U};
	draw_overlay_word(output, replay_word, 302U, 5U);
	if(impl_->fast_forward) {
		constexpr std::array<std::size_t, 2U> ff_word{6U, 7U};
		draw_overlay_word(output, ff_word, 342U, 5U);
	}
	auto result = *surface;
	result.pixels = output;
	if(impl_->reduced_flash && result.palette_tone == 150) {
		result.graphics_palette = result.reduced_flash_palette;
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

NewClassicPlaybackStatus NewClassicReplayPlayback::status() const noexcept
{
	return impl_->status;
}

std::uint32_t NewClassicReplayPlayback::sample_index() const noexcept
{
	return impl_->sample_index;
}

} // namespace th03::app
