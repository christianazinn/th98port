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
	std::size_t selection_starts = 0U;
	std::size_t selection_finishes = 0U;
	std::size_t selection_returns = 0U;
	std::size_t config_commits = 0U;
	std::size_t story_starts = 0U;
	std::size_t story_finishes = 0U;
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
	for(const auto route : {ClassicOpRoute::high_scores, ClassicOpRoute::demo}) {
		auto backend = std::make_shared<FakeBackend>();
		backend->title_result.route = route;
		auto session = make_session(backend);
		if(!session) { continue; }
		const auto tick = session->consume({});
		expect(
			tick && tick->status == ClassicOpSessionStatus::external_route &&
				tick->external_route == route && backend->config_commits == 1U,
			"external title route commits config and publishes exact capability"
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
	th03::GameInit resident{};
	resident.resident_rand = 1;
	resident.random_seed = 1U;
	auto created = ClassicOpSession::create(
		resident,
		std::make_unique<th03::app::MainResourceLoader>(content_root),
		std::move(*compiled),
		*font_rom,
		*mainl_executable,
		*op_executable,
		temporary.path() / "YUME.CFG",
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
	while(session.take_audio_command()) {
	}
	const auto tick = session.consume({});
	if(!tick || tick->scene != ClassicOpSessionScene::title_menu ||
		tick->status != ClassicOpSessionStatus::running ||
		!session.compose_shown_surface(pixels)) {
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
		auto advanced = direct.consume({
			.selection = {.single_player = 0x1000U},
		});
		if(!advanced) {
			std::cerr << "local direct VS selection advance failed code="
				<< static_cast<unsigned>(advanced.error().code) << '\n';
			return false;
		}
		if(advanced->scene == ClassicOpSessionScene::title_menu) {
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
			"usage requires content root, MAIN.EXE, unpacked MAINL.EXE, "
			"unpacked OP.EXE, and FONT.ROM");
	}
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return failures == 0U ? 0 : 1;
}
