#include "th03/app/classic_runner.hpp"

#include "classic_runner_internal.hpp"
#include "classic_input_bits_internal.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "th03/app/authentic_text.hpp"
#include "th03/app/new_classic_replay.hpp"

namespace th03::app {
namespace {

[[nodiscard]] ClassicRunnerError runner_error(
	ClassicRunnerErrorCode code,
	ClassicContentFile file = ClassicContentFile::none
) noexcept
{
	return {.code = code, .file = file};
}

[[nodiscard]] ClassicRunnerError session_error(
	const ClassicOpSessionError &error
)
{
	auto result = runner_error(ClassicRunnerErrorCode::session_failed);
	result.detail_code = static_cast<std::uint8_t>(error.code);
	result.resource_error = error.resource_error;
	result.process_error = error.main_error;
	result.surface_error = error.surface_error;
	return result;
}

[[nodiscard]] bool valid_operations(
	const ClassicRunnerOperations &operations
) noexcept
{
	return operations.context != nullptr && operations.consume != nullptr &&
		operations.take_audio_command != nullptr &&
		operations.compose_shown_surface != nullptr;
}

[[nodiscard]] std::expected<ClassicOpSessionTick, ClassicOpSessionError>
consume_session(void *context, const ClassicOpSessionInput &input)
{
	return static_cast<ClassicOpSession *>(context)->consume(input);
}

[[nodiscard]] std::optional<ClassicAudioCommand> take_session_audio(
	void *context
)
{
	return static_cast<ClassicOpSession *>(context)->take_audio_command();
}

[[nodiscard]] std::optional<NewClassicKeyConfig> take_session_key_config(
	void *context
)
{
	return static_cast<ClassicOpSession *>(context)->take_key_config_update();
}

[[nodiscard]] std::optional<NewClassicJoyConfig> take_session_joy_config(
	void *context
)
{
	return static_cast<ClassicOpSession *>(context)->take_joy_config_update();
}

[[nodiscard]] std::optional<NewClassicDeviceAssignment>
take_session_device_assignment(void *context)
{
	return static_cast<ClassicOpSession *>(context)->
		take_device_assignment_update();
}

[[nodiscard]] std::optional<bool> take_session_reduced_flash(void *context)
{
	return static_cast<ClassicOpSession *>(context)->take_reduced_flash_update();
}

[[nodiscard]] std::optional<std::uint8_t>
take_session_master_volume(void *context)
{
	return static_cast<ClassicOpSession *>(context)->take_master_volume_update();
}

[[nodiscard]] std::optional<bool> take_session_mute_when_unfocused(
	void *context
)
{
	return static_cast<ClassicOpSession *>(context)->
		take_mute_when_unfocused_update();
}

[[nodiscard]] std::optional<NewClassicWindowPresentation>
take_session_window_presentation(void *context)
{
	return static_cast<ClassicOpSession *>(context)->
		take_window_presentation_update();
}

[[nodiscard]] PixelOracleSourceIdentity pixel_oracle_session_identity(
	void *context
) noexcept
{
	return static_cast<ClassicOpSession *>(context)->
		pixel_oracle_source_identity();
}

[[nodiscard]] std::expected<present::SourceSurface, ClassicOpSessionError>
compose_session(
	void *context,
	std::span<present::ComposedPixel, present::graphics_page_pixels> output
)
{
	return static_cast<ClassicOpSession *>(context)->compose_shown_surface(output);
}

[[nodiscard]] std::expected<std::vector<std::byte>, ClassicRunnerError>
read_external_file(
	const std::filesystem::path &path,
	ClassicContentFile file,
	std::optional<std::uintmax_t> exact_size = std::nullopt
)
{
	std::error_code error;
	const auto status = std::filesystem::status(path, error);
	if(error || !std::filesystem::exists(status)) {
		return std::unexpected(runner_error(
			ClassicRunnerErrorCode::file_missing, file
		));
	}
	if(!std::filesystem::is_regular_file(status)) {
		return std::unexpected(runner_error(
			ClassicRunnerErrorCode::file_not_regular, file
		));
	}
	const auto size = std::filesystem::file_size(path, error);
	if(error || size > static_cast<std::uintmax_t>(
		std::numeric_limits<std::size_t>::max()
	) || size > static_cast<std::uintmax_t>(
		std::numeric_limits<std::streamsize>::max()
	)) {
		return std::unexpected(runner_error(
			ClassicRunnerErrorCode::file_size_failed, file
		));
	}
	if(exact_size && size != *exact_size) {
		return std::unexpected(runner_error(
			ClassicRunnerErrorCode::file_size_mismatch, file
		));
	}

	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(runner_error(
			ClassicRunnerErrorCode::file_open_failed, file
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
		return std::unexpected(runner_error(
			ClassicRunnerErrorCode::file_read_failed, file
		));
	}
	return bytes;
}

[[nodiscard]] constexpr bool pressed(
	std::uint16_t input,
	std::uint16_t bit
) noexcept
{
	return (input & bit) != 0U;
}

[[nodiscard]] std::uint16_t interface_input_word(
	const LogicalTickInput &sample
) noexcept
{
	if(!sample.physical_domains) {
		return sample.input.system;
	}
	const auto &physical = *sample.physical_domains;
	auto result = static_cast<std::uint16_t>(
		physical.keyboard.system | physical.keyboard.p1
	);
	if(physical.first_joystick_present) {
		result = static_cast<std::uint16_t>(
			result | physical.first_joystick
		);
	}
	return result;
}

[[nodiscard]] ClassicOpInput project_op_input(
	const CoreInput &input
) noexcept
{
	const auto source = input.system;
	return {
		.raw_any = source != 0U,
		.up = pressed(source, input_up),
		.down = pressed(source, input_down),
		.left = pressed(source, input_left),
		.right = pressed(source, input_right),
		.ok = pressed(source, input_ok),
		.shot = pressed(source, input_shot),
		.cancel = pressed(source, input_cancel),
	};
}

[[nodiscard]] ClassicRegistrationInput project_registration_input(
	const CoreInput &input
) noexcept
{
	const auto source = input.system;
	return {
		.raw_any = source != 0U,
		.up = pressed(source, input_up),
		.down = pressed(source, input_down),
		.left = pressed(source, input_left),
		.right = pressed(source, input_right),
		.ok = pressed(source, input_ok),
		.shot = pressed(source, input_shot),
		.bomb = pressed(source, input_bomb),
		.cancel = pressed(source, input_cancel),
	};
}

[[nodiscard]] NewClassicUiInput project_new_classic_ui_input(
	const CoreInput &input,
	bool app_quit_requested
) noexcept
{
	return {
		.up = pressed(input.system, input_up),
		.down = pressed(input.system, input_down),
		.left = pressed(input.system, input_left),
		.right = pressed(input.system, input_right),
		.confirm = pressed(input.system, input_ok) ||
			pressed(input.system, input_shot),
		.bomb = pressed(input.system, input_bomb),
		.cancel = pressed(input.system, input_cancel),
		.app_quit_requested = app_quit_requested,
	};
}

[[nodiscard]] ClassicOpSessionInput project_session_input(
	const LogicalTickInput &sample,
	ClassicAudioStatus audio
) noexcept
{
	const auto &input = sample.input;
	const auto interface_word = interface_input_word(sample);
	auto interface_input = input;
	interface_input.system = interface_word;
	const auto op = project_op_input(interface_input);
	const auto registration = project_registration_input(interface_input);
	const bool cancel = pressed(interface_word, input_cancel);
	const bool ok = pressed(interface_word, input_ok);
	const bool shot = pressed(interface_word, input_shot);
	const bool any = interface_word != 0U;
	const auto selection_player_one = sample.physical_domains ?
		sample.physical_domains->keyboard.p1 : input.p1;
	const auto selection_player_two = sample.physical_domains ?
		sample.physical_domains->keyboard.p2 : input.p2;

	ClassicStorySessionInput story{};
	story.main = {
		.aggregate = input,
		.physical_domains = sample.physical_domains,
		.restart_requested = sample.restart_requested,
	};
	story.cutscene.script = {
		.cancel_held = cancel,
		.any_key_held = any,
	};
	story.cutscene.shot_or_ok_held = shot || ok;
	story.cutscene.audio = audio;
	story.staff = {
		.audio = audio,
		.any_input = any,
		.fast_forward_held = sample.fast_forward_requested,
		.vsync_count_above_one = false,
	};
	story.registration = registration;
	story.continue_menu = {
		.left = pressed(interface_word, input_left),
		.right = pressed(interface_word, input_right),
		.ok = ok,
		.shot = shot,
		.cancel = cancel,
	};
	story.audio = audio;
	story.dismiss_pressed = any;
	story.app_quit_requested = sample.app_quit_requested;

	return {
		.op = op,
		.music_room = {.controls = op},
		.high_scores = registration,
		.selection = {
			.single_player = interface_word,
			.player_one = selection_player_one,
			.player_two = selection_player_two,
			.physical_domains = sample.physical_domains,
		},
		.story = story,
		.main = {
			.aggregate = input,
			.physical_domains = sample.physical_domains,
			.restart_requested = sample.restart_requested,
		},
		.audio = audio,
		.replay_ui = project_new_classic_ui_input(
			interface_input, sample.app_quit_requested
		),
		.replay_fast_forward = sample.fast_forward_requested,
		.scancode_pressed = sample.scancode_pressed,
		.any_scancode_held = sample.any_scancode_held,
		.dismiss_pressed = any,
		.app_quit_requested = sample.app_quit_requested,
	};
}

[[nodiscard]] GameInit initial_resident(std::uint32_t seed) noexcept
{
	GameInit resident{};
	resident.resident_rand = std::bit_cast<std::int32_t>(seed);
	resident.random_seed = seed;
	return resident;
}

} // namespace

struct ClassicRunner::Impl final {
	std::shared_ptr<void> owner{};
	ClassicRunnerOperations operations{};
	ClassicRunnerStatus status = ClassicRunnerStatus::running;
	ExperienceProfile experience = ExperienceProfile::classic;
	bool reduced_flash = false;
	std::unique_ptr<NewClassicReplayRecorder> replay_recorder{};
	std::optional<ClassicRunnerError> failure{};
	std::optional<NewClassicKeyConfig> latest_key_config{};
	std::optional<NewClassicJoyConfig> latest_joy_config{};
	std::optional<NewClassicDeviceAssignment> latest_device_assignment{};
	std::optional<std::uint8_t> latest_master_volume{};
	std::optional<bool> latest_mute_when_unfocused{};
	std::optional<NewClassicWindowPresentation> latest_window_presentation{};
	PixelOracleIdentityTracker pixel_oracle_tracker{};
	bool initial_replay = false;
	std::uint64_t expected_tick_index = 0U;
	std::optional<TickTimestampNs> last_deadline{};

	Impl(
		std::shared_ptr<void> owner_,
		ClassicRunnerOperations operations_,
		ExperienceProfile experience_,
		bool reduced_flash_ = false,
		std::unique_ptr<NewClassicReplayRecorder> replay_recorder_ = nullptr,
		bool initial_replay_ = false
	) noexcept :
		owner(std::move(owner_)),
		operations(operations_),
		experience(experience_),
		reduced_flash(reduced_flash_),
		replay_recorder(std::move(replay_recorder_)),
		initial_replay(initial_replay_)
	{}
};

std::expected<std::unique_ptr<ClassicRunner>, ClassicRunnerError>
ClassicRunner::load(ClassicRunnerRequest request)
{
	const auto &paths = request.content;
	if(paths.data_root.empty() || paths.main_executable.empty() ||
		paths.mainl_executable.empty() || paths.op_executable.empty() ||
		paths.font_rom.empty() || paths.config.empty() || paths.score.empty() ||
		(request.autofire_mask > 3U) ||
		(request.experience == ExperienceProfile::classic &&
		 (request.autofire_mask != 0U || request.reduced_flash)) ||
		(request.initial_route != ClassicRunnerInitialRoute::title &&
		 request.experience != ExperienceProfile::new_classic) ||
		(request.initial_route == ClassicRunnerInitialRoute::new_classic_replay) !=
			request.initial_replay.has_value() ||
		!valid_new_classic_device_assignment(
			request.new_classic_device_defaults
		) ||
		(request.new_classic_device_assignment &&
		 !valid_new_classic_device_assignment(
			*request.new_classic_device_assignment
		 ))) {
		return std::unexpected(runner_error(
			ClassicRunnerErrorCode::invalid_request
		));
	}

	auto main = read_external_file(
		paths.main_executable, ClassicContentFile::main_executable
	);
	if(!main) {
		return std::unexpected(std::move(main.error()));
	}
	auto compiled = import_main_compiled_content(*main);
	if(!compiled) {
		auto error = runner_error(
			ClassicRunnerErrorCode::compiled_content_failed,
			ClassicContentFile::main_executable
		);
		error.compiled_content_error = std::move(compiled.error());
		return std::unexpected(std::move(error));
	}
	auto mainl = read_external_file(
		paths.mainl_executable, ClassicContentFile::mainl_executable
	);
	if(!mainl) {
		return std::unexpected(std::move(mainl.error()));
	}
	auto op = read_external_file(
		paths.op_executable, ClassicContentFile::op_executable
	);
	if(!op) {
		return std::unexpected(std::move(op.error()));
	}
	auto font = read_external_file(
		paths.font_rom,
		ClassicContentFile::font_rom,
		Pc98FontRomProvider::font_rom_size
	);
	if(!font) {
		return std::unexpected(std::move(font.error()));
	}
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets{};
	if(request.experience == ExperienceProfile::new_classic) {
		if(request.content.replay_root.empty()) {
			request.content.replay_root = request.content.data_root / "REPLAY";
		}
		if(request.content.replay_patch_assets.empty()) {
			request.content.replay_patch_assets =
				request.content.data_root / "AZINN.DAT";
		}
		auto loaded_assets = load_new_classic_replay_patch_assets(
			request.content.replay_patch_assets
		);
		if(!loaded_assets) {
			auto result = runner_error(
				ClassicRunnerErrorCode::replay_patch_assets_failed,
				ClassicContentFile::replay_patch_assets
			);
			result.replay_patch_assets_error =
				std::move(loaded_assets.error());
			return std::unexpected(std::move(result));
		}
		patch_assets = std::make_shared<const NewClassicReplayPatchAssets>(
			std::move(*loaded_assets)
		);
	}

	auto resident = initial_resident(request.seed);
	resident.autofire_mask = request.autofire_mask;
	if(request.content.new_classic_score.empty()) {
		request.content.new_classic_score =
			request.content.score.parent_path() / "YUME.NEW.NEM";
	}
	if(request.content.new_classic_settings.empty()) {
		request.content.new_classic_settings =
			request.content.config.parent_path() / "PODD.CFG";
	}
	auto replay_recorder = request.experience == ExperienceProfile::new_classic
		? std::make_unique<NewClassicReplayRecorder>(
			request.content.replay_root
		) : nullptr;
	auto session = ClassicOpSession::create(
		std::move(resident),
		std::make_unique<MainResourceLoader>(paths.data_root),
		std::move(*compiled),
		*font,
		*mainl,
		*op,
		request.content.config,
		request.content.score,
		request.fm_available,
		request.kotohime_compatibility,
		replay_recorder.get(),
		request.initial_route == ClassicRunnerInitialRoute::new_classic_practice,
		request.content.new_classic_score,
		request.content.language_overlay,
		std::move(patch_assets), &request
	);
	if(!session) {
		return std::unexpected(session_error(session.error()));
	}
	auto owner = std::shared_ptr<ClassicOpSession>(std::move(*session));
	const ClassicRunnerOperations operations{
		.context = owner.get(),
		.consume = &consume_session,
		.take_audio_command = &take_session_audio,
		.take_key_config_update = &take_session_key_config,
		.take_joy_config_update = &take_session_joy_config,
		.take_device_assignment_update = &take_session_device_assignment,
		.take_reduced_flash_update = &take_session_reduced_flash,
		.take_master_volume_update = &take_session_master_volume,
		.take_mute_when_unfocused_update = &take_session_mute_when_unfocused,
		.take_window_presentation_update = &take_session_window_presentation,
		.pixel_oracle_source_identity = &pixel_oracle_session_identity,
		.compose_shown_surface = &compose_session,
	};
	auto impl = std::make_unique<Impl>(
		std::static_pointer_cast<void>(std::move(owner)), operations,
		request.experience, request.reduced_flash, std::move(replay_recorder),
		request.initial_route == ClassicRunnerInitialRoute::new_classic_replay
	);
	return std::unique_ptr<ClassicRunner>(
		new ClassicRunner(std::move(impl))
	);
}

ClassicRunner::ClassicRunner(std::unique_ptr<Impl> impl) noexcept :
	impl_(std::move(impl))
{}

ClassicRunner::~ClassicRunner() = default;

std::expected<ClassicRunnerConsumeResult, ClassicRunnerError>
ClassicRunner::consume(
	const LogicalTickInput &sample,
	ClassicAudioStatus audio
)
{
	if(impl_->failure) {
		return std::unexpected(*impl_->failure);
	}
	if(impl_->status != ClassicRunnerStatus::running) {
		return std::unexpected(runner_error(
			ClassicRunnerErrorCode::runner_not_running
		));
	}
	if(sample.tick_index != impl_->expected_tick_index ||
		(impl_->last_deadline && sample.deadline_ns < *impl_->last_deadline)) {
		return std::unexpected(runner_error(
			ClassicRunnerErrorCode::invalid_sample_order
		));
	}

	auto advanced = impl_->operations.consume(
		impl_->operations.context,
		project_session_input(sample, audio)
	);
	if(!advanced) {
		impl_->failure = session_error(advanced.error());
		impl_->status = ClassicRunnerStatus::failed;
		return std::unexpected(*impl_->failure);
	}
	impl_->expected_tick_index++;
	impl_->last_deadline = sample.deadline_ns;
	if(impl_->operations.take_key_config_update) {
		if(auto update = impl_->operations.take_key_config_update(
			impl_->operations.context
		)) {
			impl_->latest_key_config = std::move(*update);
		}
	}
	if(impl_->operations.take_joy_config_update) {
		if(auto update = impl_->operations.take_joy_config_update(
			impl_->operations.context
		)) {
			impl_->latest_joy_config = std::move(*update);
		}
	}
	if(impl_->operations.take_device_assignment_update) {
		if(auto update = impl_->operations.take_device_assignment_update(
			impl_->operations.context
		)) {
			impl_->latest_device_assignment = std::move(*update);
		}
	}
	if(impl_->operations.take_reduced_flash_update) {
		if(auto update = impl_->operations.take_reduced_flash_update(
			impl_->operations.context
		)) {
			impl_->reduced_flash = *update;
		}
	}
	if(impl_->operations.take_master_volume_update) {
		if(auto update = impl_->operations.take_master_volume_update(
			impl_->operations.context
		)) {
			impl_->latest_master_volume = *update;
		}
	}
	if(impl_->operations.take_mute_when_unfocused_update) {
		if(auto update = impl_->operations.take_mute_when_unfocused_update(
			impl_->operations.context
		)) {
			impl_->latest_mute_when_unfocused = *update;
		}
	}
	if(impl_->operations.take_window_presentation_update) {
		if(auto update = impl_->operations.take_window_presentation_update(
			impl_->operations.context
		)) {
			impl_->latest_window_presentation = *update;
		}
	}

	ClassicRunnerConsumeResult consumed{
		.tick_index = sample.tick_index,
		.deadline_ns = sample.deadline_ns,
		.surface_changed = advanced->surface_changed,
		.input_sampled = advanced->input_sampled,
		.process_advanced = advanced->process_advanced,
		.round_started = advanced->round_started,
		.clock_rebase_requested = advanced->clock_rebase_requested,
		.pixel_oracle_identity = impl_->pixel_oracle_tracker.consume(
			advanced->pixel_oracle_source_identity,
			advanced->surface_changed
		),
		.status = impl_->status,
	};
	switch(advanced->status) {
	case ClassicOpSessionStatus::running:
		break;
	case ClassicOpSessionStatus::complete:
		impl_->status = ClassicRunnerStatus::session_complete;
		break;
	case ClassicOpSessionStatus::switch_to_classic:
		impl_->status = ClassicRunnerStatus::switch_to_classic;
		break;
	case ClassicOpSessionStatus::quit_requested:
		impl_->status = ClassicRunnerStatus::quit_requested;
		break;
	case ClassicOpSessionStatus::failed:
		impl_->failure = runner_error(
			ClassicRunnerErrorCode::unexpected_session_status
		);
		impl_->failure->detail_code = static_cast<std::uint8_t>(
			advanced->status
		);
		impl_->status = ClassicRunnerStatus::failed;
		return std::unexpected(*impl_->failure);
	}
	consumed.status = impl_->status;
	return consumed;
}

std::optional<ClassicAudioCommand> ClassicRunner::take_audio_command()
{
	return impl_->operations.take_audio_command(impl_->operations.context);
}

std::expected<present::SourceSurface, ClassicRunnerError>
ClassicRunner::compose_shown_surface(
	std::span<present::ComposedPixel, present::graphics_page_pixels> output
) const
{
	auto surface = impl_->operations.compose_shown_surface(
		impl_->operations.context, output
	);
	if(!surface) {
		auto error = session_error(surface.error());
		error.code = ClassicRunnerErrorCode::surface_failed;
		return std::unexpected(std::move(error));
	}
	if(impl_->experience == ExperienceProfile::new_classic &&
		impl_->reduced_flash && surface->palette_tone == 150) {
		surface->graphics_palette = surface->reduced_flash_palette;
		surface->reduced_flash_applied = true;
	}
	return *surface;
}

ClassicRunnerStatus ClassicRunner::status() const noexcept
{
	return impl_->status;
}

PixelOracleIdentity ClassicRunner::pixel_oracle_bootstrap_identity()
{
	auto source = impl_->operations.pixel_oracle_source_identity ?
		impl_->operations.pixel_oracle_source_identity(impl_->operations.context) :
		PixelOracleSourceIdentity{};
	if(impl_->expected_tick_index == 0U && impl_->initial_replay) {
		impl_->pixel_oracle_tracker = PixelOracleIdentityTracker(
			pixel_oracle_direct_replay_first_process_instance(source.process)
		);
	}
	return impl_->pixel_oracle_tracker.bootstrap(source);
}

std::optional<formats::ReplayDocument> ClassicRunner::take_completed_replay()
{
	return impl_->replay_recorder
		? impl_->replay_recorder->take_completed() : std::nullopt;
}

void ClassicRunner::classify_pending_replay_frame(
	bool deadline_missed
) noexcept
{
	if(impl_->replay_recorder) {
		impl_->replay_recorder->classify_pending_frame(deadline_missed);
	}
}

std::optional<NewClassicKeyConfig>
ClassicRunner::take_new_classic_key_config_update() noexcept
{
	return impl_ ? std::exchange(impl_->latest_key_config, std::nullopt) :
		std::nullopt;
}

std::optional<NewClassicJoyConfig>
ClassicRunner::take_new_classic_joy_config_update() noexcept
{
	return impl_ ? std::exchange(impl_->latest_joy_config, std::nullopt) :
		std::nullopt;
}

std::optional<NewClassicDeviceAssignment>
ClassicRunner::take_new_classic_device_assignment_update() noexcept
{
	return impl_ ? std::exchange(
		impl_->latest_device_assignment, std::nullopt
	) : std::nullopt;
}

std::optional<std::uint8_t>
ClassicRunner::take_new_classic_master_volume_update() noexcept
{
	return impl_ ? std::exchange(impl_->latest_master_volume, std::nullopt) :
		std::nullopt;
}

std::optional<bool>
ClassicRunner::take_new_classic_mute_when_unfocused_update() noexcept
{
	return impl_ ? std::exchange(
		impl_->latest_mute_when_unfocused, std::nullopt
	) : std::nullopt;
}

std::optional<NewClassicWindowPresentation>
ClassicRunner::take_new_classic_window_presentation_update() noexcept
{
	return impl_ ? std::exchange(
		impl_->latest_window_presentation, std::nullopt
	) : std::nullopt;
}

std::shared_ptr<const NewClassicReplaySaveArt>
ClassicRunner::new_classic_replay_save_art() const noexcept
{
	if(!impl_ || impl_->operations.consume != &consume_session) {
		return {};
	}
	return static_cast<const ClassicOpSession *>(
		impl_->operations.context
	)->replay_save_art();
}

std::expected<std::unique_ptr<ClassicRunner>, ClassicRunnerError>
ClassicRunnerTestAccess::create(
	std::shared_ptr<void> owner,
	ClassicRunnerOperations operations,
	ExperienceProfile experience,
	bool reduced_flash
)
{
	if(!owner || !valid_operations(operations)) {
		return std::unexpected(runner_error(
			ClassicRunnerErrorCode::invalid_request
		));
	}
	auto impl = std::make_unique<ClassicRunner::Impl>(
		std::move(owner), operations, experience, reduced_flash
	);
	return std::unique_ptr<ClassicRunner>(
		new ClassicRunner(std::move(impl))
	);
}

ExperienceProfile ClassicRunnerTestAccess::experience(
	const ClassicRunner &runner
) noexcept
{
	return runner.impl_->experience;
}

GameInit ClassicRunnerTestAccess::initial_resident(std::uint32_t seed) noexcept
{
	return th03::app::initial_resident(seed);
}

ClassicOpSessionInput ClassicRunnerTestAccess::project_input(
	const LogicalTickInput &sample,
	ClassicAudioStatus audio
) noexcept
{
	return project_session_input(sample, audio);
}

std::optional<ClassicOpSessionScene> ClassicRunnerTestAccess::scene(
	const ClassicRunner &runner
) noexcept
{
	if(runner.impl_->operations.consume != &consume_session) {
		return std::nullopt;
	}
	return static_cast<const ClassicOpSession *>(
		runner.impl_->operations.context
	)->scene();
}

std::optional<ClassicStorySessionScene>
ClassicRunnerTestAccess::story_scene(
	const ClassicRunner &runner
) noexcept
{
	if(runner.impl_->operations.consume != &consume_session) {
		return std::nullopt;
	}
	return ClassicOpSessionTestAccess::story_scene(
		*static_cast<const ClassicOpSession *>(
			runner.impl_->operations.context
		)
	);
}

std::optional<std::uint32_t>
ClassicRunnerTestAccess::story_source_sample_count(
	const ClassicRunner &runner
) noexcept
{
	if(runner.impl_->operations.consume != &consume_session) {
		return std::nullopt;
	}
	return ClassicOpSessionTestAccess::story_source_sample_count(
		*static_cast<const ClassicOpSession *>(
			runner.impl_->operations.context
		)
	);
}

std::optional<ClassicStorySessionErrorCode>
ClassicRunnerTestAccess::story_failure_code(
	const ClassicRunner &runner
) noexcept
{
	if(runner.impl_->operations.consume != &consume_session) {
		return std::nullopt;
	}
	return ClassicOpSessionTestAccess::story_failure_code(
		*static_cast<const ClassicOpSession *>(
			runner.impl_->operations.context
		)
	);
}

std::optional<std::uint8_t>
ClassicRunnerTestAccess::story_failure_detail_code(
	const ClassicRunner &runner
) noexcept
{
	if(runner.impl_->operations.consume != &consume_session) {
		return std::nullopt;
	}
	return ClassicOpSessionTestAccess::story_failure_detail_code(
		*static_cast<const ClassicOpSession *>(
			runner.impl_->operations.context
		)
	);
}

std::expected<void, ClassicRunnerError>
ClassicRunnerTestAccess::force_story_main_outcome(
	ClassicRunner &runner,
	ClassicStoryRouteTestOutcome outcome
)
{
	if(runner.impl_->operations.consume != &consume_session) {
		return std::unexpected(runner_error(
			ClassicRunnerErrorCode::invalid_request
		));
	}
	auto forced = ClassicOpSessionTestAccess::force_story_main_outcome(
		*static_cast<ClassicOpSession *>(runner.impl_->operations.context),
		outcome
	);
	if(!forced) {
		return std::unexpected(session_error(forced.error()));
	}
	return {};
}

std::expected<void, ClassicRunnerError>
ClassicRunnerTestAccess::force_vs_main_outcome(
	ClassicRunner &runner,
	ClassicOpVsRouteTestOutcome outcome
)
{
	if(runner.impl_->operations.consume != &consume_session) {
		return std::unexpected(runner_error(
			ClassicRunnerErrorCode::invalid_request
		));
	}
	auto forced = ClassicOpSessionTestAccess::force_vs_main_outcome(
		*static_cast<ClassicOpSession *>(runner.impl_->operations.context),
		outcome
	);
	if(!forced) {
		return std::unexpected(session_error(forced.error()));
	}
	return {};
}

std::optional<GameInit> ClassicRunnerTestAccess::vs_selection_resident(
	const ClassicRunner &runner
) noexcept
{
	if(runner.impl_->operations.consume != &consume_session) {
		return std::nullopt;
	}
	return ClassicOpSessionTestAccess::vs_selection_resident(
		*static_cast<const ClassicOpSession *>(
			runner.impl_->operations.context
		)
	);
}

std::optional<GameInit> ClassicRunnerTestAccess::vs_terminal_resident(
	const ClassicRunner &runner
) noexcept
{
	if(runner.impl_->operations.consume != &consume_session) {
		return std::nullopt;
	}
	return ClassicOpSessionTestAccess::vs_terminal_resident(
		*static_cast<const ClassicOpSession *>(
			runner.impl_->operations.context
		)
	);
}

std::expected<std::vector<std::byte>, ClassicRunnerError>
ClassicRunnerTestAccess::read_file(
	const std::filesystem::path &path,
	ClassicContentFile file,
	std::optional<std::uintmax_t> exact_size
)
{
	return read_external_file(path, file, exact_size);
}

} // namespace th03::app
