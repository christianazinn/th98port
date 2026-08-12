#include "th03/app/classic_runner.hpp"

#include "classic_runner_internal.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <ctime>
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

constexpr std::uint16_t input_up = 0x0001U;
constexpr std::uint16_t input_down = 0x0002U;
constexpr std::uint16_t input_left = 0x0004U;
constexpr std::uint16_t input_right = 0x0008U;
constexpr std::uint16_t input_bomb = 0x0010U;
constexpr std::uint16_t input_shot = 0x0020U;
constexpr std::uint16_t input_cancel = 0x1000U;
constexpr std::uint16_t input_ok = 0x2000U;

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

[[nodiscard]] std::uint16_t current_dos_date() noexcept
{
	const auto now = std::time(nullptr);
	std::tm local{};
#if defined(_WIN32)
	if(localtime_s(&local, &now) != 0) {
		return 0U;
	}
#else
	if(!localtime_r(&now, &local)) {
		return 0U;
	}
#endif
	const auto year = std::clamp(local.tm_year + 1900, 1980, 2107) - 1980;
	return static_cast<std::uint16_t>(
		(static_cast<std::uint16_t>(year) << 9U) |
		(static_cast<std::uint16_t>(local.tm_mon + 1) << 5U) |
		static_cast<std::uint16_t>(local.tm_mday)
	);
}

} // namespace

struct ClassicRunner::Impl final {
	std::shared_ptr<void> owner{};
	ClassicRunnerOperations operations{};
	ClassicRunnerStatus status = ClassicRunnerStatus::running;
	ExperienceProfile experience = ExperienceProfile::classic;
	std::unique_ptr<NewClassicReplayRecorder> replay_recorder{};
	std::optional<ClassicRunnerError> failure{};
	std::uint64_t expected_tick_index = 0U;
	std::optional<TickTimestampNs> last_deadline{};

	Impl(
		std::shared_ptr<void> owner_,
		ClassicRunnerOperations operations_,
		ExperienceProfile experience_,
		std::unique_ptr<NewClassicReplayRecorder> replay_recorder_ = nullptr
	) noexcept :
		owner(std::move(owner_)),
		operations(operations_),
		experience(experience_),
		replay_recorder(std::move(replay_recorder_))
	{}
};

std::expected<std::unique_ptr<ClassicRunner>, ClassicRunnerError>
ClassicRunner::load(ClassicRunnerRequest request)
{
	const auto &paths = request.content;
	if(paths.data_root.empty() || paths.main_executable.empty() ||
		paths.mainl_executable.empty() || paths.op_executable.empty() ||
		paths.font_rom.empty() || paths.config.empty() || paths.score.empty()) {
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

	auto resident = initial_resident(request.seed);
	auto replay_recorder = request.experience == ExperienceProfile::new_classic
		? std::make_unique<NewClassicReplayRecorder>(current_dos_date()) : nullptr;
	auto session = ClassicOpSession::create(
		std::move(resident),
		std::make_unique<MainResourceLoader>(paths.data_root),
		std::move(*compiled),
		*font,
		*mainl,
		*op,
		std::move(request.content.config),
		std::move(request.content.score),
		request.fm_available,
		request.kotohime_compatibility,
		replay_recorder.get()
	);
	if(!session) {
		return std::unexpected(session_error(session.error()));
	}
	auto owner = std::shared_ptr<ClassicOpSession>(std::move(*session));
	const ClassicRunnerOperations operations{
		.context = owner.get(),
		.consume = &consume_session,
		.take_audio_command = &take_session_audio,
		.compose_shown_surface = &compose_session,
	};
	auto impl = std::make_unique<Impl>(
		std::static_pointer_cast<void>(std::move(owner)), operations,
		request.experience, std::move(replay_recorder)
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

	ClassicRunnerConsumeResult consumed{
		.tick_index = sample.tick_index,
		.deadline_ns = sample.deadline_ns,
		.surface_changed = advanced->surface_changed,
		.input_sampled = advanced->input_sampled,
		.process_advanced = advanced->process_advanced,
		.round_started = advanced->round_started,
		.status = impl_->status,
	};
	switch(advanced->status) {
	case ClassicOpSessionStatus::running:
		break;
	case ClassicOpSessionStatus::complete:
		impl_->status = ClassicRunnerStatus::session_complete;
		break;
	case ClassicOpSessionStatus::quit_requested:
		impl_->status = ClassicRunnerStatus::quit_requested;
		break;
	case ClassicOpSessionStatus::external_route:
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
	return *surface;
}

ClassicRunnerStatus ClassicRunner::status() const noexcept
{
	return impl_->status;
}

std::optional<formats::ReplayDocument> ClassicRunner::take_completed_replay()
{
	return impl_->replay_recorder
		? impl_->replay_recorder->take_completed() : std::nullopt;
}

std::expected<std::unique_ptr<ClassicRunner>, ClassicRunnerError>
ClassicRunnerTestAccess::create(
	std::shared_ptr<void> owner,
	ClassicRunnerOperations operations,
	ExperienceProfile experience
)
{
	if(!owner || !valid_operations(operations)) {
		return std::unexpected(runner_error(
			ClassicRunnerErrorCode::invalid_request
		));
	}
	auto impl = std::make_unique<ClassicRunner::Impl>(
		std::move(owner), operations, experience
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
