#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <vector>

#include "classic_story_session_internal.hpp"

namespace {

using th03::CanonicalState;
using th03::ContinueChoice;
using th03::GameInit;
using th03::SessionAction;
using th03::SessionTransition;
using th03::app::ClassicMainlContinueInput;
using th03::app::MainlCutsceneKind;
using th03::app::MainlCutsceneResourceRequest;
using th03::app::ClassicStoryBackendTick;
using th03::app::ClassicStorySession;
using th03::app::ClassicStorySessionBackendOperations;
using th03::app::ClassicStorySessionError;
using th03::app::ClassicStorySessionErrorCode;
using th03::app::ClassicStorySessionInput;
using th03::app::ClassicStorySessionScene;
using th03::app::ClassicStorySessionStatus;
using th03::app::ClassicStorySessionTestAccess;
using th03::app::ClassicStoryTerminal;
using th03::app::MainStoryConfig;

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

bool kotohime_fade(
	void *,
	const th03::BombFrameStep &,
	std::uint8_t,
	std::uint8_t &red_out
) noexcept
{
	red_out = 0U;
	return true;
}

MainStoryConfig story_config(std::uint8_t credits = 3U) noexcept
{
	MainStoryConfig config{};
	auto &resident = config.resident;
	resident.global_frame = 1000U;
	resident.resident_rand = 12345;
	resident.random_seed = 12345U;
	resident.game_mode = th03::game_mode::story;
	resident.rank = 1U;
	resident.key_mode = 0U;
	resident.story_stage = 0U;
	resident.story_lives = 2U;
	resident.remaining_credits = credits;
	resident.skill = 3U;
	resident.is_cpu = {0U, 1U};
	resident.paletted_character = {2, 5};
	resident.story_opponents = {5, 1, 7, 9, 11, 13, 3, 15, 17};
	config.kotohime_compatibility.component_provider = &kotohime_fade;
	return config;
}

CanonicalState terminal_state(const GameInit &start, std::int8_t winner)
{
	CanonicalState terminal{};
	terminal.route = 2U;
	terminal.game_mode = th03::game_mode::story;
	terminal.story_stage = start.story_stage;
	terminal.winner = winner;
	terminal.global_frame = start.global_frame + 1U;
	terminal.round_frame = 77U;
	terminal.resident_rand = 54321;
	terminal.p1_score = {8U, 7U, 6U, 5U, 4U, 3U, 2U, 1U};
	return terminal;
}

SessionTransition win_transition(
	const GameInit &start,
	const CanonicalState &terminal
)
{
	SessionTransition transition{};
	transition.resident = start;
	transition.resident.global_frame = terminal.global_frame;
	transition.resident.resident_rand = terminal.resident_rand;
	transition.resident.random_seed = terminal.round_frame;
	transition.resident.winner = terminal.winner;
	transition.resident.story_stage = static_cast<std::uint8_t>(
		start.story_stage + 1U
	);
	transition.resident.last_score = {};
	transition.resident.last_score[0U] = terminal.p1_score;
	if(start.story_stage == 8U) {
		transition.resident.story_stage = 99U;
		transition.high_score_registration = transition.resident;
		transition.action = SessionAction::ending;
	} else {
		transition.resident.paletted_character[1U] =
			start.story_opponents[transition.resident.story_stage];
		transition.action = SessionAction::start_gameplay;
	}
	return transition;
}

SessionTransition loss_transition(
	const GameInit &start,
	const CanonicalState &terminal
)
{
	SessionTransition transition{};
	transition.resident = start;
	transition.resident.global_frame = terminal.global_frame;
	transition.resident.resident_rand = terminal.resident_rand;
	transition.resident.random_seed = terminal.round_frame;
	transition.resident.winner = 1;
	transition.resident.story_stage = static_cast<std::uint8_t>(
		start.story_stage + 1U
	);
	transition.resident.last_score = {};
	transition.resident.last_score[0U] = terminal.p1_score;
	transition.high_score_registration = transition.resident;
	transition.resident.last_score = {};
	transition.action = (start.remaining_credits == 0U) ?
		SessionAction::exit_to_operator : SessionAction::request_continue;
	return transition;
}

struct FakeBackend final {
	GameInit process_start{};
	bool lose_next_main = false;
	bool route_zero = false;
	bool fail_stage_advance = false;
	bool stage_surface_changed = true;
	ContinueChoice continue_choice = ContinueChoice::accept;
	bool win_preload = false;
	bool win_preload_requested = false;
	bool win_preloaded = false;
	std::uint8_t last_continue_credits = 0U;
	std::size_t stage_count = 0U;
	std::size_t stage_advance_count = 0U;
	std::size_t main_count = 0U;
	std::size_t win_count = 0U;
	std::size_t cutscene_count = 0U;
	std::size_t staff_count = 0U;
	std::size_t registration_count = 0U;
	std::size_t preload_count = 0U;
	std::size_t continue_count = 0U;
	std::size_t resolve_count = 0U;
	std::size_t game_over_count = 0U;
	std::size_t main_advance_count = 0U;
	bool cutscene_active = false;
	std::vector<MainlCutsceneKind> cutscene_history{};
	std::array<th03::present::ComposedPixel,
		th03::present::graphics_page_pixels> pixels{};

	ClassicStoryTerminal terminal() const
	{
		const auto winner = static_cast<std::int8_t>(lose_next_main ? 1 : 0);
		auto terminal = terminal_state(process_start, winner);
		if(route_zero) {
			terminal.route = 0U;
			SessionTransition transition{};
			transition.resident = process_start;
			transition.action = SessionAction::exit_to_operator;
			return {
				.process_start = process_start,
				.terminal = terminal,
				.transition = transition,
			};
		}
		return {
			.process_start = process_start,
			.terminal = terminal,
			.transition = lose_next_main ?
				loss_transition(process_start, terminal) :
				win_transition(process_start, terminal),
		};
	}
};

ClassicStorySessionError contract_error()
{
	return {.code = ClassicStorySessionErrorCode::backend_contract_failed};
}

ClassicStorySessionBackendOperations operations(FakeBackend &fake)
{
	return {
		.context = &fake,
		.start_stage = [](void *context, const MainStoryConfig &) {
			++static_cast<FakeBackend *>(context)->stage_count;
			return std::expected<void, ClassicStorySessionError>{};
		},
		.advance_stage = [](void *context, bool) {
			auto &backend = *static_cast<FakeBackend *>(context);
			++backend.stage_advance_count;
			if(backend.fail_stage_advance) {
				return std::expected<ClassicStoryBackendTick,
					ClassicStorySessionError>{
					std::unexpected(contract_error())
				};
			}
			return std::expected<ClassicStoryBackendTick,
				ClassicStorySessionError>{ClassicStoryBackendTick{
				.surface_changed = backend.stage_surface_changed,
				.input_sampled = true,
				.ready = true,
			}};
		},
		.start_main = [](void *context, const MainStoryConfig &config) {
			auto &backend = *static_cast<FakeBackend *>(context);
			backend.process_start = config.resident;
			++backend.main_count;
			return std::expected<void, ClassicStorySessionError>{};
		},
		.advance_main = [](
			void *context,
			const th03::app::ClassicMainInputSample &
		) {
			auto &backend = *static_cast<FakeBackend *>(context);
			++backend.main_advance_count;
			return std::expected<ClassicStoryBackendTick,
				ClassicStorySessionError>{ClassicStoryBackendTick{
				.surface_changed = true,
				.input_sampled = true,
				.ready = true,
				.process_advanced = true,
				.step_result = th03::StepResult{th03::StepStatus::terminal},
			}};
		},
		.main_terminal = [](void *context) {
			return std::expected<ClassicStoryTerminal,
				ClassicStorySessionError>{
				static_cast<FakeBackend *>(context)->terminal()
			};
		},
		.start_win = [](void *context, const th03::app::ClassicMainlWinContext &win) {
			auto &backend = *static_cast<FakeBackend *>(context);
			++backend.win_count;
			backend.win_preload = win.preload_stage_splash;
			backend.win_preload_requested = false;
			backend.win_preloaded = false;
			return std::expected<void, ClassicStorySessionError>{};
		},
		.advance_win = [](void *context, th03::app::ClassicAudioStatus, bool) {
			auto &backend = *static_cast<FakeBackend *>(context);
			if(backend.win_preload && !backend.win_preload_requested) {
				backend.win_preload_requested = true;
				return std::expected<ClassicStoryBackendTick,
					ClassicStorySessionError>{ClassicStoryBackendTick{
					.stage_preload_requested = true,
				}};
			}
			return std::expected<ClassicStoryBackendTick,
				ClassicStorySessionError>{ClassicStoryBackendTick{
				.surface_changed = true, .ready = true,
			}};
		},
		.preload_stage = [](void *context, const MainStoryConfig &) {
			auto &backend = *static_cast<FakeBackend *>(context);
			++backend.preload_count;
			backend.win_preloaded = true;
			return std::expected<void, ClassicStorySessionError>{};
		},
		.finish_win = [](void *context) {
			auto &backend = *static_cast<FakeBackend *>(context);
			if(backend.win_preload != backend.win_preloaded) {
				return std::expected<void, ClassicStorySessionError>{
					std::unexpected(contract_error())
				};
			}
			return std::expected<void, ClassicStorySessionError>{};
		},
		.start_cutscene = [](void *context, MainlCutsceneResourceRequest request) {
			auto &backend = *static_cast<FakeBackend *>(context);
			if(backend.cutscene_active) {
				return std::expected<void, ClassicStorySessionError>{
					std::unexpected(contract_error())
				};
			}
			backend.cutscene_active = true;
			++backend.cutscene_count;
			backend.cutscene_history.push_back(request.kind);
			return std::expected<void, ClassicStorySessionError>{};
		},
		.advance_cutscene = [](void *context, const th03::app::ClassicCutsceneInput &) {
			auto &backend = *static_cast<FakeBackend *>(context);
			if(!backend.cutscene_active) {
				return std::expected<ClassicStoryBackendTick,
					ClassicStorySessionError>{
						std::unexpected(contract_error())
					};
			}
			return std::expected<ClassicStoryBackendTick,
				ClassicStorySessionError>{ClassicStoryBackendTick{
					.surface_changed = true,
					.input_sampled = true,
					.ready = true,
				}};
		},
		.finish_cutscene = [](void *context) {
			auto &backend = *static_cast<FakeBackend *>(context);
			if(!backend.cutscene_active) {
				return std::expected<void, ClassicStorySessionError>{
					std::unexpected(contract_error())
				};
			}
			backend.cutscene_active = false;
			return std::expected<void, ClassicStorySessionError>{};
		},
		.start_staff = [](void *context, const GameInit &) {
			++static_cast<FakeBackend *>(context)->staff_count;
			return std::expected<void, ClassicStorySessionError>{};
		},
		.advance_staff = [](void *, const th03::app::ClassicStaffInput &) {
			return std::expected<ClassicStoryBackendTick,
				ClassicStorySessionError>{ClassicStoryBackendTick{
				.surface_changed = true,
				.input_sampled = true,
				.ready = true,
			}};
		},
		.finish_staff = [](void *) {
			return std::expected<void, ClassicStorySessionError>{};
		},
		.start_registration = [](void *context, const GameInit &) {
			++static_cast<FakeBackend *>(context)->registration_count;
			return std::expected<void, ClassicStorySessionError>{};
		},
		.advance_registration = [](
			void *,
			th03::app::ClassicRegistrationInput,
			th03::app::ClassicAudioStatus
		) {
			return std::expected<ClassicStoryBackendTick,
				ClassicStorySessionError>{ClassicStoryBackendTick{
				.surface_changed = true,
				.input_sampled = true,
				.ready = true,
			}};
		},
		.finish_registration = [](void *) {
			return std::expected<void, ClassicStorySessionError>{};
		},
		.start_continue = [](void *context, std::uint8_t credits) {
			auto &backend = *static_cast<FakeBackend *>(context);
			++backend.continue_count;
			backend.last_continue_credits = credits;
			return std::expected<bool, ClassicStorySessionError>{credits == 0U};
		},
		.advance_continue = [](void *, ClassicMainlContinueInput) {
			return std::expected<ClassicStoryBackendTick,
				ClassicStorySessionError>{ClassicStoryBackendTick{
				.surface_changed = true,
				.input_sampled = true,
				.ready = true,
			}};
		},
		.finish_continue = [](void *context) {
			return std::expected<ContinueChoice, ClassicStorySessionError>{
				static_cast<FakeBackend *>(context)->last_continue_credits == 0U ?
					ContinueChoice::decline :
					static_cast<FakeBackend *>(context)->continue_choice
			};
		},
		.resolve_continue = [](void *context, ContinueChoice choice) {
			auto &backend = *static_cast<FakeBackend *>(context);
			++backend.resolve_count;
			auto transition = backend.terminal().transition;
			transition.resident.story_stage = backend.process_start.story_stage;
			transition.resident.story_lives = 2U;
			if(choice == ContinueChoice::accept) {
				transition.action = SessionAction::start_gameplay;
				--transition.resident.remaining_credits;
			} else {
				transition.action = SessionAction::exit_to_operator;
			}
			return std::expected<SessionTransition,
				ClassicStorySessionError>{transition};
		},
		.start_game_over = [](void *context) {
			++static_cast<FakeBackend *>(context)->game_over_count;
			return std::expected<void, ClassicStorySessionError>{};
		},
		.advance_game_over = [](void *, th03::app::ClassicAudioStatus) {
			return std::expected<ClassicStoryBackendTick,
				ClassicStorySessionError>{ClassicStoryBackendTick{
				.surface_changed = true, .ready = true,
			}};
		},
		.finish_game_over = [](void *) {
			return std::expected<void, ClassicStorySessionError>{};
		},
		.take_audio_command = [](void *) {
			return std::optional<th03::app::ClassicAudioCommand>{};
		},
		.compose_shown_surface = [](void *, auto output) {
			std::fill(output.begin(), output.end(), th03::present::ComposedPixel{});
			return std::expected<th03::present::SourceSurface,
				ClassicStorySessionError>{th03::present::SourceSurface{
				.pixels = output,
			}};
		},
	};
}

std::unique_ptr<ClassicStorySession> create_session(
	const MainStoryConfig &config,
	const std::shared_ptr<FakeBackend> &backend
)
{
	auto created = ClassicStorySessionTestAccess::create(
		config,
		std::static_pointer_cast<void>(backend),
		operations(*backend)
	);
	expect(created.has_value(), "Story session creates with retained backend");
	return created ? std::move(*created) : nullptr;
}

void test_story_input_mode_normalization()
{
	const th03::CoreInput physical{
		.p1 = 0x0104U,
		.p2 = 0x0208U,
		.system = 0x7032U,
		.charge_mask = 0x03U,
	};
	const auto gameplay = ClassicStorySessionTestAccess::story_main_input(
		{.aggregate = physical}, th03::InputContext::gameplay
	);
	expect(gameplay.p1 == 0x7136U && gameplay.p2 == 0U &&
		gameplay.system == physical.system && gameplay.charge_mask == 0x01U,
		"Story MAIN reproduces input_mode_1p_vs_cpu while retaining input_sp");
	for(const auto context : {
		th03::InputContext::interstitial,
		th03::InputContext::pause,
	}) {
		const auto raw = ClassicStorySessionTestAccess::story_main_input(
			{.aggregate = physical}, context
		);
		expect(raw.p1 == physical.p1 && raw.p2 == physical.p2 &&
			raw.system == physical.system &&
			raw.charge_mask == physical.charge_mask,
			"Story warning and pause samples retain raw source words");
	}

	const th03::app::ClassicMainInputSample live{
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
	const auto live_gameplay =
		ClassicStorySessionTestAccess::story_main_input(
			live, th03::InputContext::gameplay
		);
	expect(live_gameplay.p1 == 0x103FU && live_gameplay.p2 == 0U &&
		live_gameplay.system == 0x1032U &&
		live_gameplay.charge_mask == 0x01U,
		"live Story input merges keyboard interface and joystick zero into P1");
	const auto new_classic_gameplay =
		ClassicStorySessionTestAccess::story_main_input(
			live, th03::InputContext::gameplay, true
		);
	expect(new_classic_gameplay.p1 == 0x103BU &&
		new_classic_gameplay.p2 == 0U &&
		new_classic_gameplay.system == 0x1032U,
		"New Classic Story replaces multiplayer movement with Story keys");
	const auto live_pause = ClassicStorySessionTestAccess::story_main_input(
		live, th03::InputContext::pause
	);
	expect(live_pause.system == 0x1032U &&
		live_pause.p1 == physical.p1 && live_pause.p2 == physical.p2,
		"live Story pause observes raw keyboard input_sp without joystick merging");
}

void test_mainl_surface_handoffs()
{
	const th03::app::ClassicCutsceneInput held{
		.script = {.cancel_held = true, .any_key_held = true},
		.shot_or_ok_held = true,
		.audio = {.measure = 3U},
	};
	const th03::app::ClassicCutsceneInput next{
		.script = {},
		.shot_or_ok_held = false,
		.audio = {.measure = 9U},
	};
	const auto reused = ClassicStorySessionTestAccess::replay_cutscene_input(
		next, held, false
	);
	const auto sampled = ClassicStorySessionTestAccess::replay_cutscene_input(
		next, held, true
	);
	expect(reused.script.cancel_held && reused.script.any_key_held &&
		reused.shot_or_ok_held && reused.audio.measure == 9U &&
		!sampled.script.cancel_held && !sampled.script.any_key_held &&
		!sampled.shot_or_ok_held && sampled.audio.measure == 9U,
		"New Classic reuses one recorded cutscene input throughout a VSync while PMD status remains live");

	th03::present::Raster main;
	expect(main.set_access_page(th03::present::Page::zero) &&
		main.plot({17, 23}, 3U) &&
		main.set_access_page(th03::present::Page::one) &&
		main.plot({29, 31}, 12U) &&
		main.set_shown_page(th03::present::Page::one) &&
		main.set_raster_mode(th03::present::RasterMode::zoomed_200_upper),
		"MAIN cutscene seed fixture establishes retained zoomed VRAM");
	main.set_palette_tone(173);
	const auto seed = ClassicStorySessionTestAccess::normalize_main_cutscene_seed(
		main
	);
	expect(seed.raster_mode() == th03::present::RasterMode::lines_400 &&
		seed.access_page() == th03::present::Page::zero &&
		seed.shown_page() == th03::present::Page::one &&
		seed.pixel(th03::present::Page::zero, {17, 23}) == 3U &&
		seed.pixel(th03::present::Page::one, {29, 31}) == 12U &&
		seed.palette_tone() == 173,
		"MAIN-to-MAINL seed restores 400 lines while preserving VRAM and display state");

	auto backend = std::make_shared<FakeBackend>();
	backend->stage_surface_changed = false;
	auto session = create_session(story_config(), backend);
	if(!session) {
		return;
	}
	const auto transitioned = session->consume({});
	expect(transitioned && transitioned->scene ==
		ClassicStorySessionScene::main && transitioned->surface_changed,
		"Story publishes a scene transition even when the completed child is unchanged");
}

void test_complete_nine_stage_route()
{
	auto backend = std::make_shared<FakeBackend>();
	auto session = create_session(story_config(), backend);
	if(!session) {
		return;
	}
	std::size_t guard = 0U;
	while(session->status() != ClassicStorySessionStatus::complete &&
		guard++ < 100U) {
		const auto advanced = session->consume({.dismiss_pressed = true});
		expect(advanced.has_value(), "complete Story route advances");
		if(!advanced) {
			break;
		}
	}
	expect(session->status() == ClassicStorySessionStatus::complete &&
		backend->stage_count == 9U && backend->main_count == 9U &&
		backend->win_count == 7U && backend->preload_count == 6U &&
		backend->cutscene_count == 4U && backend->staff_count == 1U &&
		backend->registration_count == 1U,
		"nine-stage route preserves win/preload/cutscene boundaries");
	expect(session->source_sample_count() ==
		backend->process_start.global_frame + 5U,
		"source input coordinate includes every owned MAINL scene sample");
	const std::array expected_cutscenes{
		MainlCutsceneKind::interstage_chiyuri,
		MainlCutsceneKind::interstage_yumemi,
		MainlCutsceneKind::ending,
		MainlCutsceneKind::extra_ending,
	};
	expect(backend->cutscene_history == std::vector<MainlCutsceneKind>{
		expected_cutscenes.begin(), expected_cutscenes.end()
	}, "all interstage and ending scenes run inside the retained session");
	expect(!session->consume({}) &&
		session->status() == ClassicStorySessionStatus::complete,
		"completed Story rejects further source samples");
}

void advance_to_continue(
	ClassicStorySession &session,
	FakeBackend &backend
)
{
	expect(session.consume({}).has_value() &&
		session.scene() == ClassicStorySessionScene::main,
		"loss fixture advances splash into MAIN");
	expect(backend.process_start.global_frame == 1001U,
		"stage input poll advances the first MAIN source coordinate");
	expect(session.consume({}).has_value() &&
		session.scene() == ClassicStorySessionScene::win,
		"terminal loss enters opponent win");
	expect(session.consume({.dismiss_pressed = true}).has_value() &&
		session.scene() ==
			ClassicStorySessionScene::high_score_registration &&
		session.status() == ClassicStorySessionStatus::running,
		"loss win advances to owned registration");
	expect(session.consume({}).has_value() &&
		backend.registration_count == 1U,
		"owned registration completes before Continue activation");
}

void test_continue_accept_decline_and_zero_credit()
{
	for(const auto choice : {ContinueChoice::accept, ContinueChoice::decline}) {
		auto backend = std::make_shared<FakeBackend>();
		backend->lose_next_main = true;
		backend->continue_choice = choice;
		auto session = create_session(story_config(2U), backend);
		if(!session) {
			continue;
		}
		advance_to_continue(*session, *backend);
		expect(session->scene() == ClassicStorySessionScene::continue_menu,
			"credited loss enters Continue menu");
		expect(session->consume({}).has_value() &&
			backend->resolve_count == 1U,
			"Continue choice resolves retained MAIN exactly once");
		if(choice == ContinueChoice::accept) {
			expect(session->scene() == ClassicStorySessionScene::stage_splash &&
				backend->stage_count == 2U,
				"Continue Yes restarts the same stage through splash");
		} else {
			expect(session->scene() == ClassicStorySessionScene::game_over &&
				backend->game_over_count == 1U,
				"Continue No transfers OVER into Game Over");
			expect(session->consume({}).has_value() &&
				session->status() == ClassicStorySessionStatus::complete,
				"Game Over completes to operator exit");
		}
	}

	auto backend = std::make_shared<FakeBackend>();
	backend->lose_next_main = true;
	auto session = create_session(story_config(0U), backend);
	if(!session) {
		return;
	}
	advance_to_continue(*session, *backend);
	expect(session->scene() == ClassicStorySessionScene::game_over &&
		backend->continue_count == 1U && backend->resolve_count == 0U,
		"zero-credit Continue returns directly to Game Over without second core transition");
}

void test_quit_and_backend_failure()
{
	auto backend = std::make_shared<FakeBackend>();
	auto session = create_session(story_config(), backend);
	if(!session) {
		return;
	}
	const auto before = backend->main_advance_count;
	const auto quit = session->consume({.app_quit_requested = true});
	expect(quit && quit->status == ClassicStorySessionStatus::quit_requested &&
		backend->main_advance_count == before &&
		session->source_sample_count() == 1000U,
		"app quit stops before any active-scene mutation");
	expect(!session->consume({}) &&
		session->status() == ClassicStorySessionStatus::quit_requested,
		"quit session rejects later samples");

	auto invalid = ClassicStorySessionTestAccess::create(
		story_config(), backend, {}
	);
	expect(!invalid && invalid.error().code ==
		ClassicStorySessionErrorCode::invalid_backend,
		"missing backend operation table is rejected atomically");

	auto route_backend = std::make_shared<FakeBackend>();
	route_backend->route_zero = true;
	auto route_session = create_session(story_config(), route_backend);
	if(route_session) {
		expect(route_session->consume({}).has_value() &&
			route_session->scene() == ClassicStorySessionScene::main,
			"route-zero fixture enters MAIN");
		expect(route_session->consume({}).has_value() &&
			route_session->status() == ClassicStorySessionStatus::complete &&
			route_backend->win_count == 0U,
			"route zero bypasses every MAINL scene");
	}

	auto failed_backend = std::make_shared<FakeBackend>();
	failed_backend->fail_stage_advance = true;
	auto failed_session = create_session(story_config(), failed_backend);
	if(failed_session) {
		const auto failed = failed_session->consume({});
		expect(!failed && failed_session->status() ==
			ClassicStorySessionStatus::failed &&
			failed_backend->stage_advance_count == 1U,
			"post-entry backend failure is sticky");
		expect(!failed_session->consume({}) &&
			failed_backend->stage_advance_count == 1U,
			"sticky failure prevents a second backend mutation");
	}
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

bool run_local_original_gate(
	const std::filesystem::path &content_root,
	const std::filesystem::path &main_executable_path,
	const std::filesystem::path &mainl_executable_path
)
{
	const auto main_executable = read_file(main_executable_path);
	const auto mainl_executable = read_file(mainl_executable_path);
	if(!main_executable || !mainl_executable) {
		return false;
	}
	auto compiled = th03::app::import_main_compiled_content(*main_executable);
	if(!compiled) {
		return false;
	}
	std::vector<std::byte> font_rom(
		th03::app::Pc98FontRomProvider::font_rom_size,
		std::byte{0U}
	);
	font_rom[0x800U] = std::byte{0x80U};
	font_rom[0x1800U] = std::byte{0x80U};
	const auto font = th03::app::Pc98FontRomProvider::from_exact_font_rom(
		font_rom
	);
	if(!font) {
		return false;
	}
	th03::app::MainResourceLoader cutscene_loader(content_root);
	for(const auto request : {
		MainlCutsceneResourceRequest{
			.kind = MainlCutsceneKind::interstage_yumemi,
			.character = th03::app::MainCharacter::reimu,
		},
		MainlCutsceneResourceRequest{
			.kind = MainlCutsceneKind::ending,
			.character = th03::app::MainCharacter::reimu,
		},
	}) {
		auto resources = cutscene_loader.load_mainl_cutscene(request);
		if(!resources) {
			return false;
		}
		th03::present::Raster main_seed;
		if(!main_seed.set_raster_mode(
			th03::present::RasterMode::zoomed_200_upper
		)) {
			return false;
		}
		auto normalized =
			ClassicStorySessionTestAccess::normalize_main_cutscene_seed(main_seed);
		auto cutscene = th03::app::ClassicCutsceneScene::create(
			request, *resources, *font, std::move(normalized)
		);
		if(!cutscene) {
			std::cerr << "local MAIN-seeded cutscene create failed kind="
				<< static_cast<unsigned>(request.kind) << " code="
				<< static_cast<unsigned>(cutscene.error().code) << '\n';
			return false;
		}
	}
	auto created = ClassicStorySession::create(
		story_config(),
		std::make_unique<th03::app::MainResourceLoader>(content_root),
		std::move(*compiled),
		font_rom,
		*mainl_executable
	);
	if(!created) {
		std::cerr << "local session create failed code="
			<< static_cast<unsigned>(created.error().code);
		if(created.error().resource_error) {
			std::cerr << " resource=" << static_cast<unsigned>(
				created.error().resource_error->code
			) << " role=" << static_cast<unsigned>(
				created.error().resource_error->key.role
			);
		}
		if(created.error().main_error) {
			std::cerr << " main=" << static_cast<unsigned>(
				created.error().main_error->code
			);
		}
		if(created.error().stage_error) {
			std::cerr << " stage=" << static_cast<unsigned>(
				created.error().stage_error->code
			);
		}
		std::cerr << '\n';
		return false;
	}
	auto &session = **created;
	std::array<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	> pixels{};
	if(const auto composed = session.compose_shown_surface(pixels); !composed) {
		std::cerr << "local stage compose failed code="
			<< static_cast<unsigned>(composed.error().code) << '\n';
		return false;
	}
	for(std::size_t guard = 0U;
		guard < 128U && session.scene() != ClassicStorySessionScene::main;
		++guard) {
		if(const auto consumed = session.consume({.dismiss_pressed = true});
			!consumed) {
			std::cerr << "local stage advance failed code="
				<< static_cast<unsigned>(consumed.error().code) << '\n';
			return false;
		}
	}
	if(session.scene() != ClassicStorySessionScene::main ||
		!session.compose_shown_surface(pixels)) {
		return false;
	}
	const auto stepped = session.consume({});
	return stepped && stepped->scene == ClassicStorySessionScene::main &&
		stepped->process_advanced && session.compose_shown_surface(pixels);
}

} // namespace

int main(int argc, char **argv)
{
	test_story_input_mode_normalization();
	test_mainl_surface_handoffs();
	test_complete_nine_stage_route();
	test_continue_accept_decline_and_zero_credit();
	test_quit_and_backend_failure();
	if(argc == 4) {
		expect(run_local_original_gate(argv[1], argv[2], argv[3]),
			"local original assets construct Stage, retained MAIN, and surface");
	} else if(argc != 1) {
		expect(false,
			"usage requires content root, MAIN.EXE, and supported MAINL.EXE");
	}

	if(failures != 0U) {
		std::cerr << failures << " of " << checks
			<< " Classic Story session checks failed\n";
		return 1;
	}
	std::cout << checks << " Classic Story session checks passed\n";
	return 0;
}
