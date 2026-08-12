#include "../src/app/classic_op_session_internal.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

using th03::app::ClassicAudioCommand;
using th03::app::ClassicAudioCommandType;
using th03::app::ClassicGameplayMusic;
using th03::app::ClassicMusicCue;
using th03::app::ClassicOpRoute;
using th03::app::ClassicOpSelectionMode;
using th03::app::ClassicOpSelectionOutcome;
using th03::app::ClassicOpSession;
using th03::app::ClassicOpSessionBackendOperations;
using th03::app::ClassicOpSessionBackendTick;
using th03::app::ClassicOpSessionError;
using th03::app::ClassicOpSessionErrorCode;
using th03::app::ClassicOpSessionInput;
using th03::app::ClassicOpSessionScene;
using th03::app::ClassicOpSessionSelectionResult;
using th03::app::ClassicOpSessionStatus;
using th03::app::ClassicOpSessionTestAccess;
using th03::app::ClassicOpSessionTitleResult;
using th03::app::ClassicOpSessionVsTerminal;

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

class TemporaryDirectory final {
public:
	TemporaryDirectory()
	{
		const auto suffix = std::chrono::steady_clock::now()
			.time_since_epoch().count();
		path_ = std::filesystem::temp_directory_path() /
			("th03-classic-op-session-" + std::to_string(suffix));
		std::error_code error;
		std::filesystem::create_directories(path_, error);
		valid_ = !error;
	}
	~TemporaryDirectory()
	{
		if(valid_) {
			std::error_code ignored;
			std::filesystem::remove_all(path_, ignored);
		}
	}
	[[nodiscard]] bool valid() const noexcept
	{
		return valid_;
	}
	[[nodiscard]] const std::filesystem::path &path() const noexcept
	{
		return path_;
	}

private:
	std::filesystem::path path_{};
	bool valid_ = false;
};

[[nodiscard]] std::optional<std::vector<std::byte>> read_file(
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

[[nodiscard]] ClassicOpSessionError error(
	ClassicOpSessionErrorCode code =
		ClassicOpSessionErrorCode::backend_contract_failed
) noexcept
{
	return {.code = code};
}

struct FakeBackend final {
	ClassicOpSessionTitleResult title_result{};
	ClassicOpSessionSelectionResult selection_result{};
	ClassicOpSessionVsTerminal vs_terminal{};
	bool queue_title_audio = false;
	bool audio_pending = false;
	bool fail_title_advance = false;
	bool fail_start_options = false;
	std::size_t title_advances = 0U;
	std::size_t options_starts = 0U;
	std::size_t options_finishes = 0U;
	std::size_t music_starts = 0U;
	std::size_t music_finishes = 0U;
	std::size_t high_score_starts = 0U;
	std::size_t high_score_finishes = 0U;
	std::size_t selection_starts = 0U;
	std::size_t selection_finishes = 0U;
	std::size_t selection_returns = 0U;
	std::size_t config_commits = 0U;
	std::size_t story_starts = 0U;
	std::size_t story_finishes = 0U;
	std::size_t demo_stage_starts = 0U;
	std::size_t demo_main_starts = 0U;
	std::size_t demo_main_finishes = 0U;
	std::size_t stage_starts = 0U;
	std::size_t main_starts = 0U;
	std::size_t win_starts = 0U;
	std::size_t win_finishes = 0U;
	std::size_t direct_selection_starts = 0U;
	ClassicOpSelectionMode started_mode = ClassicOpSelectionMode::story;

	[[nodiscard]] ClassicOpSessionBackendTick ready_tick() const noexcept
	{
		return {
			.surface_changed = true,
			.input_sampled = true,
			.ready = true,
		};
	}
};

template <typename Result, typename Function>
Result fake_call(void *context, Function function)
{
	return function(*static_cast<FakeBackend *>(context));
}

[[nodiscard]] ClassicOpSessionBackendOperations operations(
	FakeBackend &fake
) noexcept
{
	return {
		.context = &fake,
		.advance_title = [](void *context, auto) {
			return fake_call<std::expected<ClassicOpSessionBackendTick,
				ClassicOpSessionError>>(context, [](FakeBackend &backend) {
				++backend.title_advances;
				if(backend.fail_title_advance) {
					return std::expected<ClassicOpSessionBackendTick,
						ClassicOpSessionError>{std::unexpected(error(
							ClassicOpSessionErrorCode::title_failed
						))};
				}
				backend.audio_pending = backend.queue_title_audio;
				return std::expected<ClassicOpSessionBackendTick,
					ClassicOpSessionError>{backend.ready_tick()};
			});
		},
		.finish_title = [](void *context) {
			return std::expected<ClassicOpSessionTitleResult,
				ClassicOpSessionError>{
				static_cast<FakeBackend *>(context)->title_result
			};
		},
		.start_options = [](void *context) {
			auto &backend = *static_cast<FakeBackend *>(context);
			++backend.options_starts;
			if(backend.fail_start_options) {
				backend.fail_start_options = false;
				return std::expected<void, ClassicOpSessionError>{
					std::unexpected(error())
				};
			}
			return std::expected<void, ClassicOpSessionError>{};
		},
		.advance_options = [](void *context, auto) {
			return std::expected<ClassicOpSessionBackendTick,
				ClassicOpSessionError>{
				static_cast<FakeBackend *>(context)->ready_tick()
			};
		},
		.finish_options = [](void *context) {
			++static_cast<FakeBackend *>(context)->options_finishes;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.start_music_room = [](void *context) {
			++static_cast<FakeBackend *>(context)->music_starts;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.advance_music_room = [](void *context, const auto &) {
			return std::expected<ClassicOpSessionBackendTick,
				ClassicOpSessionError>{
				static_cast<FakeBackend *>(context)->ready_tick()
			};
		},
		.finish_music_room = [](void *context) {
			++static_cast<FakeBackend *>(context)->music_finishes;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.start_high_scores = [](void *context) {
			++static_cast<FakeBackend *>(context)->high_score_starts;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.advance_high_scores = [](void *context, auto, auto) {
			return std::expected<ClassicOpSessionBackendTick,
				ClassicOpSessionError>{
				static_cast<FakeBackend *>(context)->ready_tick()
			};
		},
		.finish_high_scores = [](void *context) {
			++static_cast<FakeBackend *>(context)->high_score_finishes;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.start_selection = [](void *context, ClassicOpSelectionMode mode) {
			auto &backend = *static_cast<FakeBackend *>(context);
			++backend.selection_starts;
			backend.started_mode = mode;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.advance_selection = [](void *context, auto) {
			return std::expected<ClassicOpSessionBackendTick,
				ClassicOpSessionError>{
				static_cast<FakeBackend *>(context)->ready_tick()
			};
		},
		.finish_selection = [](void *context) {
			auto &backend = *static_cast<FakeBackend *>(context);
			++backend.selection_finishes;
			return std::expected<ClassicOpSessionSelectionResult,
				ClassicOpSessionError>{backend.selection_result};
		},
		.return_from_canceled_selection = [](void *context) {
			++static_cast<FakeBackend *>(context)->selection_returns;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.commit_config = [](void *context) {
			++static_cast<FakeBackend *>(context)->config_commits;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.start_story = [](void *context) {
			++static_cast<FakeBackend *>(context)->story_starts;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.advance_story = [](void *context, const auto &) {
			return std::expected<ClassicOpSessionBackendTick,
				ClassicOpSessionError>{
				static_cast<FakeBackend *>(context)->ready_tick()
			};
		},
		.finish_story = [](void *context) {
			++static_cast<FakeBackend *>(context)->story_finishes;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.start_demo_stage = [](void *context) {
			++static_cast<FakeBackend *>(context)->demo_stage_starts;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.advance_demo_stage = [](void *context, bool) {
			return std::expected<ClassicOpSessionBackendTick,
				ClassicOpSessionError>{
				static_cast<FakeBackend *>(context)->ready_tick()
			};
		},
		.start_demo_main = [](void *context) {
			++static_cast<FakeBackend *>(context)->demo_main_starts;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.advance_demo_main = [](void *context, auto) {
			auto tick = static_cast<FakeBackend *>(context)->ready_tick();
			tick.process_advanced = true;
			return std::expected<ClassicOpSessionBackendTick,
				ClassicOpSessionError>{tick};
		},
		.finish_demo_main = [](void *context) {
			++static_cast<FakeBackend *>(context)->demo_main_finishes;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.start_vs_stage = [](void *context) {
			++static_cast<FakeBackend *>(context)->stage_starts;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.advance_vs_stage = [](void *context, bool) {
			return std::expected<ClassicOpSessionBackendTick,
				ClassicOpSessionError>{
				static_cast<FakeBackend *>(context)->ready_tick()
			};
		},
		.start_vs_main = [](void *context) {
			++static_cast<FakeBackend *>(context)->main_starts;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.advance_vs_main = [](void *context, auto) {
			auto tick = static_cast<FakeBackend *>(context)->ready_tick();
			tick.process_advanced = true;
			tick.round_started = true;
			return std::expected<ClassicOpSessionBackendTick,
				ClassicOpSessionError>{tick};
		},
		.finish_vs_main = [](void *context) {
			return std::expected<ClassicOpSessionVsTerminal,
				ClassicOpSessionError>{
				static_cast<FakeBackend *>(context)->vs_terminal
			};
		},
		.start_vs_win = [](void *context, const auto &) {
			++static_cast<FakeBackend *>(context)->win_starts;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.advance_vs_win = [](void *context, auto, bool) {
			return std::expected<ClassicOpSessionBackendTick,
				ClassicOpSessionError>{
				static_cast<FakeBackend *>(context)->ready_tick()
			};
		},
		.finish_vs_win = [](void *context) {
			++static_cast<FakeBackend *>(context)->win_finishes;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.start_direct_vs_selection = [](void *context) {
			++static_cast<FakeBackend *>(context)->direct_selection_starts;
			return std::expected<void, ClassicOpSessionError>{};
		},
		.audio_pending = [](void *context) {
			return static_cast<FakeBackend *>(context)->audio_pending;
		},
		.take_audio_command = [](void *context) {
			auto &backend = *static_cast<FakeBackend *>(context);
			if(!backend.audio_pending) {
				return std::optional<ClassicAudioCommand>{};
			}
			backend.audio_pending = false;
			return std::optional<ClassicAudioCommand>{ClassicAudioCommand{
				.type = ClassicAudioCommandType::stop_song,
			}};
		},
		.compose_shown_surface = [](void *, auto output) {
			std::fill(output.begin(), output.end(), th03::present::ComposedPixel{});
			th03::present::SourceSurface surface{.pixels = output};
			return std::expected<th03::present::SourceSurface,
				ClassicOpSessionError>{surface};
		},
	};
}

[[nodiscard]] std::unique_ptr<ClassicOpSession> make_session(
	const std::shared_ptr<FakeBackend> &backend
)
{
	auto created = ClassicOpSessionTestAccess::create(
		std::static_pointer_cast<void>(backend), operations(*backend)
	);
	expect(created.has_value(), "fake Classic OP session creates");
	return created ? std::move(*created) : nullptr;
}

void test_vs_cpu_input_modes()
{
	const th03::CoreInput physical{
		.p1 = 0x0104U,
		.p2 = 0x0208U,
		.system = 0x6032U,
		.charge_mask = 0x03U,
	};
	th03::GameInit resident{};
	resident.game_mode = th03::game_mode::vs_1p_cpu;
	resident.is_cpu = {0U, 1U};
	const auto one_player = ClassicOpSessionTestAccess::vs_cpu_main_input(
		resident, {.aggregate = physical}, th03::InputContext::gameplay
	);
	expect(one_player.p1 == 0x6136U && one_player.p2 == 0U &&
		one_player.system == physical.system &&
		one_player.charge_mask == 0x01U,
		"VS 1P-vs-CPU merges input_sp into P1 and clears the CPU side");

	resident.game_mode = th03::game_mode::vs_cpu_cpu;
	resident.is_cpu = {1U, 1U};
	const auto cpu_exit = ClassicOpSessionTestAccess::vs_cpu_main_input(
		resident, {.aggregate = physical}, th03::InputContext::gameplay
	);
	expect(cpu_exit.p1 == 0U && cpu_exit.p2 == 0U &&
		cpu_exit.system == 0x1000U && cpu_exit.charge_mask == 0U,
		"VS CPU-vs-CPU folds OK into Cancel and clears both player words");
	auto passive = physical;
	passive.system = 0x4005U;
	const auto cpu_passive = ClassicOpSessionTestAccess::vs_cpu_main_input(
		resident, {.aggregate = passive}, th03::InputContext::gameplay
	);
	expect(cpu_passive.system == passive.system && cpu_passive.p1 == 0U &&
		cpu_passive.p2 == 0U && cpu_passive.charge_mask == 0U,
		"VS CPU-vs-CPU retains non-exit input_sp bits while clearing players");

	resident.game_mode = th03::game_mode::vs_1p_2p;
	resident.is_cpu = {0U, 0U};
	for(const auto context : {
		th03::InputContext::gameplay,
		th03::InputContext::interstitial,
		th03::InputContext::pause,
	}) {
		const auto raw = ClassicOpSessionTestAccess::vs_cpu_main_input(
			resident, {.aggregate = physical}, context
		);
		expect(raw.p1 == physical.p1 && raw.p2 == physical.p2 &&
			raw.system == physical.system &&
			raw.charge_mask == physical.charge_mask,
			"two-human and blocking samples remain outside CPU mode projection");
	}
	resident.game_mode = th03::game_mode::vs_1p_cpu;
	for(const auto context : {
		th03::InputContext::interstitial,
		th03::InputContext::pause,
	}) {
		const auto raw = ClassicOpSessionTestAccess::vs_cpu_main_input(
			resident, {.aggregate = physical}, context
		);
		expect(raw.p1 == physical.p1 && raw.p2 == physical.p2 &&
			raw.system == physical.system &&
			raw.charge_mask == physical.charge_mask,
			"VS warning and pause samples retain raw source words");
	}

	th03::app::ClassicMainInputSample live{
		.aggregate = physical,
		.physical_domains = th03::app::PhysicalInputDomains{
			.keyboard = {
				.p1 = 0x0004U,
				.p2 = 0x0008U,
				.system = 0x1032U,
				.charge_mask = 0x03U,
			},
			.first_joystick = 0x0009U,
			.first_joystick_present = true,
		},
	};
	resident.game_mode = th03::game_mode::vs_1p_cpu;
	const auto live_one_player = ClassicOpSessionTestAccess::vs_cpu_main_input(
		resident, live, th03::InputContext::gameplay
	);
	expect(live_one_player.p1 == 0x103FU && live_one_player.p2 == 0U &&
		live_one_player.system == 0x1032U &&
		live_one_player.charge_mask == 0x01U,
		"live 1P-vs-CPU merges keyboard interface and joystick zero into P1");

	resident.game_mode = th03::game_mode::vs_1p_2p;
	resident.key_mode = 0U;
	const auto key_key = ClassicOpSessionTestAccess::vs_cpu_main_input(
		resident, live, th03::InputContext::gameplay
	);
	expect(key_key.p1 == 0x0004U && key_key.p2 == 0x0008U &&
		key_key.system == 0x1032U && key_key.charge_mask == 0x03U,
		"live Key-vs-Key uses the two keyboard multiplayer domains");
	resident.key_mode = 1U;
	const auto joy_key = ClassicOpSessionTestAccess::vs_cpu_main_input(
		resident, live, th03::InputContext::gameplay
	);
	expect(joy_key.p1 == 0x0009U && joy_key.p2 == 0x1032U &&
		joy_key.charge_mask == 0x02U,
		"live Joy-vs-Key routes joystick zero and keyboard input_sp exactly");
	resident.key_mode = 2U;
	const auto key_joy = ClassicOpSessionTestAccess::vs_cpu_main_input(
		resident, live, th03::InputContext::gameplay
	);
	expect(key_joy.p1 == 0x1032U && key_joy.p2 == 0x0009U &&
		key_joy.charge_mask == 0x01U,
		"live Key-vs-Joy routes keyboard input_sp and joystick zero exactly");
	live.physical_domains->first_joystick_present = false;
	const auto missing_joystick = ClassicOpSessionTestAccess::vs_cpu_main_input(
		resident, live, th03::InputContext::gameplay
	);
	expect(missing_joystick.p1 == 0x0004U &&
		missing_joystick.p2 == 0x0008U && missing_joystick.charge_mask == 0x03U,
		"missing joystick preserves the keyboard-sensed fallback words");
	const auto live_pause = ClassicOpSessionTestAccess::vs_cpu_main_input(
		resident, live, th03::InputContext::pause
	);
	expect(live_pause.system == 0x1032U && live_pause.p1 == physical.p1 &&
		live_pause.p2 == physical.p2,
		"live pause observes raw keyboard input_sp outside gameplay projection");
	th03::app::ClassicMainInputSample attract{
		.aggregate = physical,
		.physical_domains = th03::app::PhysicalInputDomains{
			.keyboard = {.p2 = 0x0008U, .charge_mask = 0x03U},
			.first_joystick = 0x0009U,
			.first_joystick_present = false,
		},
	};
	expect(!ClassicOpSessionTestAccess::attract_input_present(attract),
		"live attract mode ignores keyboard P2 and charge-only input");
	attract.physical_domains->first_joystick_present = true;
	expect(ClassicOpSessionTestAccess::attract_input_present(attract),
		"live attract mode merges joystick zero into its interface word");
	expect(ClassicOpSessionTestAccess::attract_input_present({
		.aggregate = {.p2 = 0x0008U},
	}), "scripted attract samples retain their aggregate exit contract");
}

void test_retained_children_and_audio_barrier()
{
	auto backend = std::make_shared<FakeBackend>();
	backend->title_result.route = ClassicOpRoute::options;
	backend->queue_title_audio = true;
	auto session = make_session(backend);
	if(!session) { return; }
	auto tick = session->consume({});
	expect(
		tick && tick->scene == ClassicOpSessionScene::title_menu &&
			backend->title_advances == 1U && backend->options_starts == 0U,
		"ready title waits for its semantic audio queue before adoption"
	);
	expect(
		session->take_audio_command().has_value(),
		"session exposes the queued title audio command"
	);
	tick = session->consume({});
	expect(
		tick && tick->scene == ClassicOpSessionScene::options &&
			backend->title_advances == 1U && backend->options_starts == 1U,
		"drained audio resumes the cached boundary without a second advance"
	);
	backend->queue_title_audio = false;
	tick = session->consume({});
	expect(
		tick && tick->scene == ClassicOpSessionScene::title_menu &&
			backend->options_finishes == 1U,
		"Options returns to the retained title owner"
	);

	backend->title_result.route = ClassicOpRoute::music_room;
	tick = session->consume({});
	expect(tick && tick->scene == ClassicOpSessionScene::music_room,
		"Music Room route starts");
	tick = session->consume({});
	expect(
		tick && tick->scene == ClassicOpSessionScene::title_menu &&
			backend->music_starts == 1U && backend->music_finishes == 1U,
		"Music Room returns through the title owner"
	);
}

void test_story_and_selection_cancel()
{
	auto backend = std::make_shared<FakeBackend>();
	backend->title_result = {
		.route = ClassicOpRoute::story,
		.selection_mode = ClassicOpSelectionMode::story,
	};
	backend->selection_result = {
		.outcome = ClassicOpSelectionOutcome::canceled,
		.mode = ClassicOpSelectionMode::story,
	};
	auto session = make_session(backend);
	if(!session) { return; }
	auto tick = session->consume({});
	expect(
		tick && tick->scene == ClassicOpSessionScene::character_selection &&
			backend->started_mode == ClassicOpSelectionMode::story,
		"Story title route enters the typed selector"
	);
	tick = session->consume({});
	expect(
		tick && tick->scene == ClassicOpSessionScene::title_menu &&
			backend->selection_returns == 1U && backend->config_commits == 0U,
		"canceled selector returns without a config write"
	);

	backend->selection_result.outcome = ClassicOpSelectionOutcome::selected;
	static_cast<void>(session->consume({}));
	tick = session->consume({});
	expect(
		tick && tick->scene == ClassicOpSessionScene::story &&
			backend->story_starts == 1U && backend->config_commits == 1U,
		"selected Story commits config before starting gameplay"
	);
	tick = session->consume({});
	expect(
		tick && tick->scene == ClassicOpSessionScene::title_menu &&
			backend->story_finishes == 1U,
		"complete Story campaign returns to a fresh title process"
	);
}

void test_vs_routes()
{
	for(const auto route : {
		std::uint8_t{0U}, std::uint8_t{2U}
	}) {
		auto backend = std::make_shared<FakeBackend>();
		backend->title_result = {
			.route = ClassicOpRoute::versus,
			.selection_mode = ClassicOpSelectionMode::vs_1p_2p,
		};
		backend->selection_result = {
			.outcome = ClassicOpSelectionOutcome::selected,
			.mode = ClassicOpSelectionMode::vs_1p_2p,
		};
		backend->vs_terminal.terminal.route =
			static_cast<std::uint8_t>(route);
		auto session = make_session(backend);
		if(!session) { continue; }
		static_cast<void>(session->consume({}));
		auto tick = session->consume({});
		expect(
			tick && tick->scene == ClassicOpSessionScene::vs_stage_splash &&
				backend->config_commits == 1U,
			"selected VS commits config and starts MAINL stage splash"
		);
		tick = session->consume({});
		expect(
			tick && tick->scene == ClassicOpSessionScene::vs_main &&
				backend->main_starts == 1U,
			"VS stage completion starts MAIN"
		);
		tick = session->consume({});
		expect(
			tick && tick->process_advanced && tick->round_started,
			"VS MAIN tick preserves process and round-start facts"
		);
		if(route == 2U) {
			expect(
				tick->scene == ClassicOpSessionScene::vs_win &&
					backend->win_starts == 1U,
				"route 2 enters the winner screen"
			);
			tick = session->consume({});
			expect(
				tick && tick->scene ==
					ClassicOpSessionScene::character_selection &&
					backend->win_finishes == 1U,
				"winner screen returns to direct VS selection"
			);
		} else {
			expect(
				tick->scene == ClassicOpSessionScene::character_selection,
				"route 0 skips the winner screen and returns directly"
			);
		}
		expect(
			backend->direct_selection_starts == 1U,
			"each terminal VS route starts exactly one direct selector"
		);
	}
}

void test_terminal_routes_and_failures()
{
	auto scores_backend = std::make_shared<FakeBackend>();
	scores_backend->title_result.route = ClassicOpRoute::high_scores;
	auto scores = make_session(scores_backend);
	if(scores) {
		auto tick = scores->consume({});
		expect(
			tick && tick->status == ClassicOpSessionStatus::running &&
				tick->scene == ClassicOpSessionScene::high_scores &&
				scores_backend->config_commits == 1U &&
				scores_backend->high_score_starts == 1U,
			"HiScore commits config and enters the owned MAINL viewer"
		);
		tick = scores->consume({});
		expect(
			tick && tick->scene == ClassicOpSessionScene::title_menu &&
				scores_backend->high_score_finishes == 1U,
			"completed HiScore viewer returns to a fresh OP title"
		);
	}

	auto demo_backend = std::make_shared<FakeBackend>();
	demo_backend->title_result.route = ClassicOpRoute::demo;
	auto demo = make_session(demo_backend);
	if(demo) {
		auto tick = demo->consume({});
		expect(
			tick && tick->status == ClassicOpSessionStatus::running &&
				tick->scene == ClassicOpSessionScene::demo_stage_splash &&
				demo_backend->config_commits == 1U &&
				demo_backend->demo_stage_starts == 1U,
			"automatic Demo commits config and enters the owned MAINL splash"
		);
		tick = demo->consume({});
		expect(
			tick && tick->scene == ClassicOpSessionScene::demo_main &&
				demo_backend->demo_main_starts == 1U,
			"completed Demo splash enters the owned CPU-vs-CPU MAIN process"
		);
		tick = demo->consume({});
		expect(
			tick && tick->scene == ClassicOpSessionScene::title_menu &&
				demo_backend->demo_main_finishes == 1U,
			"terminal Demo route returns to a fresh OP title"
		);
	}

	auto quit_backend = std::make_shared<FakeBackend>();
	quit_backend->title_result.route = ClassicOpRoute::quit;
	auto quit = make_session(quit_backend);
	if(quit) {
		const auto tick = quit->consume({});
		expect(
			tick && tick->status == ClassicOpSessionStatus::complete &&
				quit_backend->config_commits == 1U,
			"title Quit commits config and completes the session"
		);
	}

	auto host_backend = std::make_shared<FakeBackend>();
	auto host = make_session(host_backend);
	if(host) {
		const auto tick = host->consume({.app_quit_requested = true});
		expect(
			tick && tick->status == ClassicOpSessionStatus::quit_requested &&
				host_backend->title_advances == 0U,
			"host quit preempts every child without mutation"
		);
	}

	auto failing_backend = std::make_shared<FakeBackend>();
	failing_backend->title_result.route = ClassicOpRoute::options;
	failing_backend->fail_start_options = true;
	auto failing = make_session(failing_backend);
	if(failing) {
		auto tick = failing->consume({});
		expect(!tick, "fallible boundary construction reports its error");
		expect(
			failing->status() == ClassicOpSessionStatus::running,
			"fallible boundary construction remains running"
		);
		expect(
			failing->scene() == ClassicOpSessionScene::title_menu,
			"fallible boundary retains the completed title capability"
		);
		tick = failing->consume({});
		expect(tick.has_value(), "fallible boundary retry succeeds");
		expect(tick && tick->scene == ClassicOpSessionScene::options,
			"successful retry enters options");
		expect(failing_backend->title_advances == 1U,
			"boundary retry does not advance title twice");
		expect(failing_backend->options_starts == 2U,
			"boundary retry repeats only fallible child creation");
	}

	auto mutated_backend = std::make_shared<FakeBackend>();
	mutated_backend->fail_title_advance = true;
	auto mutated = make_session(mutated_backend);
	if(mutated) {
		auto tick = mutated->consume({});
		expect(
			!tick && mutated->status() == ClassicOpSessionStatus::failed,
			"failure after child mutation is sticky"
		);
		expect(!mutated->consume({}),
			"sticky child failure rejects every later consume");
	}

	auto invalid_backend = std::make_shared<FakeBackend>();
	auto invalid_operations = operations(*invalid_backend);
	invalid_operations.advance_title = nullptr;
	auto invalid = ClassicOpSessionTestAccess::create(
		std::static_pointer_cast<void>(invalid_backend), invalid_operations
	);
	expect(
		!invalid && invalid.error().code ==
			ClassicOpSessionErrorCode::invalid_backend,
		"session rejects an incomplete backend table"
	);
}

void test_surface_forwarding()
{
	expect(ClassicOpSessionTestAccess::vs_surface_source(
		false, false, true
	) == th03::app::ClassicOpVsSurfaceSource::main_process,
		"VS MAIN owns composition before a win scene exists");
	expect(ClassicOpSessionTestAccess::vs_surface_source(
		true, false, true
	) == th03::app::ClassicOpVsSurfaceSource::win &&
		ClassicOpSessionTestAccess::vs_surface_source(
			true, true, true
		) == th03::app::ClassicOpVsSurfaceSource::win_completion,
		"completed VS win surface precedes both its moved-from scene and retained MAIN");
	expect(ClassicOpSessionTestAccess::vs_surface_source(
		false, false, false
	) == th03::app::ClassicOpVsSurfaceSource::none,
		"VS surface selection reports no owner outside retained VS scenes");

	auto backend = std::make_shared<FakeBackend>();
	auto session = make_session(backend);
	if(!session) { return; }
	std::array<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	> pixels{};
	auto surface = session->compose_shown_surface(pixels);
	expect(
		surface && surface->pixels.data() == pixels.data() &&
			surface->pixels.size() == pixels.size(),
		"session forwards composition into caller-owned fixed storage"
	);
}

[[nodiscard]] bool run_local_original_gate(
	const std::filesystem::path &content_root,
	const std::filesystem::path &main_executable_path,
	const std::filesystem::path &mainl_executable_path,
	const std::filesystem::path &op_executable_path,
	const std::filesystem::path &font_rom_path
)
{
	const auto main_executable = read_file(main_executable_path);
	const auto mainl_executable = read_file(mainl_executable_path);
	const auto op_executable = read_file(op_executable_path);
	const auto font_rom = read_file(font_rom_path);
	if(!main_executable || !mainl_executable || !op_executable || !font_rom) {
		return false;
	}
	auto compiled = th03::app::import_main_compiled_content(*main_executable);
	if(!compiled) {
		return false;
	}
	TemporaryDirectory temporary;
	if(!temporary.valid()) {
		return false;
	}
	const auto config_path = temporary.path() / "YUME.CFG";
	if(!th03::app::commit_classic_config_store(config_path, {
		.bgm_mode = th03::app::ClassicBgmMode::midi,
	})) {
		return false;
	}
	th03::GameInit resident{};
	resident.resident_rand = 1;
	resident.random_seed = 1U;
	resident.remaining_credits = 2U;
	auto rejected_compiled =
		th03::app::import_main_compiled_content(*main_executable);
	if(!rejected_compiled) {
		return false;
	}
	auto malformed_op = *op_executable;
	malformed_op[0U] ^= std::byte{0x01U};
	auto rejected_op = ClassicOpSession::create(
		resident,
		std::make_unique<th03::app::MainResourceLoader>(content_root),
		std::move(*rejected_compiled),
		*font_rom,
		*mainl_executable,
		malformed_op,
		config_path,
		temporary.path() / "YUME.NEM"
	);
	if(rejected_op || rejected_op.error().code !=
		ClassicOpSessionErrorCode::op_text_failed ||
		!rejected_op.error().op_text_error) {
		std::cerr << "local malformed OP did not fail during session creation\n";
		return false;
	}
	auto created = ClassicOpSession::create(
		resident,
		std::make_unique<th03::app::MainResourceLoader>(content_root),
		std::move(*compiled),
		*font_rom,
		*mainl_executable,
		*op_executable,
		config_path,
		temporary.path() / "YUME.NEM"
	);
	if(!created) {
		std::cerr << "local Classic OP session create failed code="
			<< static_cast<unsigned>(created.error().code) << '\n';
		return false;
	}
	auto &session = **created;
	std::array<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	> pixels{};
	if(session.scene() != ClassicOpSessionScene::title_menu ||
		!session.compose_shown_surface(pixels)) {
		return false;
	}
	const auto normalized_bgm = session.take_audio_command();
	if(!normalized_bgm || normalized_bgm->type !=
		ClassicAudioCommandType::set_bgm_active ||
		normalized_bgm->parameter != 0U) {
		std::cerr << "local MIDI config was not normalized to inactive BGM\n";
		return false;
	}
	while(session.take_audio_command()) {
	}
	const auto tick = session.consume({});
	if(!tick || tick->scene != ClassicOpSessionScene::title_menu ||
		tick->status != ClassicOpSessionStatus::running ||
		!session.compose_shown_surface(pixels)) {
		return false;
	}
	bool title_input = tick->input_sampled;
	for(std::size_t guard = 0U; !title_input && guard < 512U; ++guard) {
		auto advanced = session.consume({});
		if(!advanced) {
			std::cerr << "local HiScore route failed while reaching title input\n";
			return false;
		}
		title_input = advanced->input_sampled;
		while(session.take_audio_command()) {
		}
	}
	if(!title_input) {
		std::cerr << "local HiScore route did not reach title input\n";
		return false;
	}
	auto advanced = session.consume({.op = {.ok = true}});
	if(!advanced) {
		std::cerr << "local HiScore route could not open the main menu\n";
		return false;
	}
	bool main_menu_input = false;
	for(std::size_t guard = 0U; !main_menu_input && guard < 32U; ++guard) {
		advanced = session.consume({});
		if(!advanced) {
			std::cerr << "local HiScore route failed during main-menu growth\n";
			return false;
		}
		main_menu_input = advanced->input_sampled;
	}
	if(!main_menu_input) {
		std::cerr << "local HiScore route did not reach main-menu input\n";
		return false;
	}
	for(std::size_t choice = 0U; choice < 3U; ++choice) {
		advanced = session.consume({.op = {.down = true}});
		if(!advanced) {
			std::cerr << "local HiScore route failed while selecting HiScore\n";
			return false;
		}
		advanced = session.consume({});
		if(!advanced) {
			std::cerr << "local HiScore route failed while releasing Down\n";
			return false;
		}
	}
	while(session.take_audio_command()) {
	}
	advanced = session.consume({.op = {.ok = true}});
	if(!advanced) {
		std::cerr << "local HiScore route could not confirm its menu choice\n";
		return false;
	}
	bool viewer_started = advanced->scene == ClassicOpSessionScene::high_scores;
	for(std::size_t guard = 0U; !viewer_started && guard < 64U; ++guard) {
		while(session.take_audio_command()) {
		}
		advanced = session.consume({});
		if(!advanced) {
			std::cerr << "local HiScore route failed during title black-out code="
				<< static_cast<unsigned>(advanced.error().code);
			if(advanced.error().resource_error) {
				std::cerr << " resource=" << static_cast<unsigned>(
					advanced.error().resource_error->code
				) << " key=" << static_cast<unsigned>(
					advanced.error().resource_error->key.role
				);
			}
			if(advanced.error().registration_error) {
				std::cerr << " registration=" << static_cast<unsigned>(
					advanced.error().registration_error->code
				);
			}
			std::cerr << '\n';
			return false;
		}
		viewer_started = advanced->scene == ClassicOpSessionScene::high_scores;
		while(session.take_audio_command()) {
		}
	}
	if(!viewer_started) {
		std::cerr << "local HiScore route did not enter the owned viewer\n";
		return false;
	}
	while(session.take_audio_command()) {
	}
	bool viewer_input = false;
	for(std::size_t guard = 0U; !viewer_input && guard < 64U; ++guard) {
		advanced = session.consume({});
		if(!advanced) {
			std::cerr << "local HiScore viewer failed before input\n";
			return false;
		}
		viewer_input = advanced->input_sampled;
		while(session.take_audio_command()) {
		}
	}
	if(!viewer_input) {
		std::cerr << "local HiScore viewer did not reach input\n";
		return false;
	}
	advanced = session.consume({.high_scores = {.ok = true}});
	if(!advanced) {
		std::cerr << "local HiScore viewer could not dismiss\n";
		return false;
	}
	bool viewer_returned = false;
	for(std::size_t guard = 0U; !viewer_returned && guard < 64U; ++guard) {
		advanced = session.consume({});
		if(!advanced) {
			std::cerr << "local HiScore viewer failed while returning to OP\n";
			return false;
		}
		viewer_returned = advanced->scene == ClassicOpSessionScene::title_menu;
		while(session.take_audio_command()) {
		}
	}
	if(!viewer_returned || !session.compose_shown_surface(pixels)) {
		std::cerr << "local HiScore viewer did not publish the fresh title\n";
		return false;
	}

	// The fresh title must own the automatic attract route rather than expose
	// it to the launcher. Let its exact idle counter expire, run the ordinary
	// non-Story MAINL splash, and use one physical gameplay bit to request the
	// source Demo exit.
	bool demo_stage_seen = false;
	bool demo_main_seen = false;
	bool demo_returned = false;
	std::vector<ClassicAudioCommand> demo_audio{};
	const auto drain_demo_audio = [&]() {
		while(auto command = session.take_audio_command()) {
			demo_audio.push_back(*command);
		}
	};
	for(std::size_t guard = 0U; guard < 900U; ++guard) {
		drain_demo_audio();
		ClassicOpSessionInput input{};
		if(session.scene() == ClassicOpSessionScene::demo_main) {
			input.main.aggregate.p1 = 1U;
		}
		auto demo_advanced = session.consume(input);
		if(!demo_advanced) {
			std::cerr << "local automatic Demo route failed code="
				<< static_cast<unsigned>(demo_advanced.error().code) << '\n';
			return false;
		}
		demo_stage_seen = demo_stage_seen || demo_advanced->scene ==
			ClassicOpSessionScene::demo_stage_splash;
		demo_main_seen = demo_main_seen || demo_advanced->scene ==
			ClassicOpSessionScene::demo_main;
		if(demo_main_seen && demo_advanced->scene ==
			ClassicOpSessionScene::title_menu) {
			demo_returned = true;
			break;
		}
	}
	drain_demo_audio();
	const std::array expected_demo_audio{
		ClassicAudioCommand{
			.type = ClassicAudioCommandType::stop_song,
		},
		ClassicAudioCommand{
			.type = ClassicAudioCommandType::load_song,
			.cue = ClassicMusicCue::gameplay,
			.parameter = static_cast<std::uint16_t>(
				ClassicGameplayMusic::stage_00
			),
		},
		ClassicAudioCommand{
			.type = ClassicAudioCommandType::load_effects,
		},
		ClassicAudioCommand{
			.type = ClassicAudioCommandType::reset_effects,
		},
		ClassicAudioCommand{
			.type = ClassicAudioCommandType::play_song,
			.cue = ClassicMusicCue::gameplay,
			.parameter = static_cast<std::uint16_t>(
				ClassicGameplayMusic::stage_00
			),
		},
		ClassicAudioCommand{
			.type = ClassicAudioCommandType::stop_song,
		},
	};
	const auto lifecycle = std::search(
		demo_audio.begin(), demo_audio.end(),
		expected_demo_audio.begin(), expected_demo_audio.end()
	);
	if(!demo_stage_seen || !demo_main_seen || !demo_returned ||
		lifecycle == demo_audio.end() || !session.compose_shown_surface(pixels)) {
		std::cerr << "local automatic Demo did not complete owned OP-MAINL-MAIN-OP"
			<< " stage=" << demo_stage_seen << " main=" << demo_main_seen
			<< " returned=" << demo_returned
			<< " audio=" << (lifecycle != demo_audio.end()) << '\n';
		return false;
	}

	auto direct_compiled =
		th03::app::import_main_compiled_content(*main_executable);
	if(!direct_compiled) {
		return false;
	}
	th03::GameInit direct_resident{};
	direct_resident.resident_rand = 1;
	direct_resident.random_seed = 1U;
	direct_resident.game_mode = th03::game_mode::vs_1p_2p;
	direct_resident.paletted_character = {1, 1};
	auto direct_created = ClassicOpSession::create(
		direct_resident,
		std::make_unique<th03::app::MainResourceLoader>(content_root),
		std::move(*direct_compiled),
		*font_rom,
		*mainl_executable,
		*op_executable,
		temporary.path() / "YUME.CFG",
		temporary.path() / "YUME.NEM"
	);
	if(!direct_created) {
		std::cerr << "local direct VS session create failed code="
			<< static_cast<unsigned>(direct_created.error().code);
		if(direct_created.error().selection_error) {
			std::cerr << " selection=" << static_cast<unsigned>(
				direct_created.error().selection_error->code
			);
		}
		std::cerr << '\n';
		return false;
	}
	if((*direct_created)->scene() !=
		ClassicOpSessionScene::character_selection) {
		std::cerr << "local direct VS session did not start in selection\n";
		return false;
	}
	auto &direct = **direct_created;
	while(direct.take_audio_command()) {
	}
	bool returned_to_title = false;
	for(std::size_t guard = 0U; guard < 256U; ++guard) {
		auto direct_advanced = direct.consume({
			.selection = {.single_player = 0x1000U},
		});
		if(!direct_advanced) {
			std::cerr << "local direct VS selection advance failed code="
				<< static_cast<unsigned>(direct_advanced.error().code) << '\n';
			return false;
		}
		if(direct_advanced->scene == ClassicOpSessionScene::title_menu) {
			returned_to_title = true;
			break;
		}
		while(direct.take_audio_command()) {
		}
	}
	const auto stop = direct.take_audio_command();
	const auto load = direct.take_audio_command();
	const bool direct_return_ok = returned_to_title && stop && load &&
		stop->type == ClassicAudioCommandType::stop_song &&
		load->type == ClassicAudioCommandType::load_song &&
		load->cue == th03::app::ClassicMusicCue::menu_main &&
		!direct.take_audio_command() &&
		tick->status == ClassicOpSessionStatus::running &&
		direct.compose_shown_surface(pixels).has_value();
	if(!direct_return_ok) {
		std::cerr << "local direct VS cancel did not enter ordinary title startup"
			<< " returned=" << returned_to_title
			<< " stop=" << static_cast<bool>(stop)
			<< " load=" << static_cast<bool>(load) << '\n';
	}
	return direct_return_ok;
}

} // namespace

int main(int argc, char **argv)
{
	test_vs_cpu_input_modes();
	test_retained_children_and_audio_barrier();
	test_story_and_selection_cancel();
	test_vs_routes();
	test_terminal_routes_and_failures();
	test_surface_forwarding();
	if(argc == 6) {
		expect(
			run_local_original_gate(argv[1], argv[2], argv[3], argv[4], argv[5]),
			"local original assets construct and advance the owning title session"
		);
	} else if(argc != 1) {
		expect(false,
			"usage requires content root, MAIN.EXE, supported MAINL.EXE, "
			"supported OP.EXE, and FONT.ROM");
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
