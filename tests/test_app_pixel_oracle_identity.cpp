#include <array>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

#include "th03/app/classic_cli.hpp"
#include "th03/app/pixel_oracle_identity.hpp"

namespace {

int failures = 0;

void expect(bool condition, const char *message)
{
	if(!condition) {
		std::cerr << "FAIL: " << message << '\n';
		++failures;
	}
}

void test_tracker_and_schema()
{
	using namespace th03::app;
	PixelOracleIdentityTracker tracker{};
	const auto bootstrap = tracker.bootstrap({
		.process = PixelOracleProcess::op,
		.scene = PixelOracleScene::title,
	});
	const auto title_tick = tracker.consume({
		.process = PixelOracleProcess::op,
		.scene = PixelOracleScene::title,
	}, false);
	const auto options = tracker.consume({
		.process = PixelOracleProcess::op,
		.scene = PixelOracleScene::options,
	}, true);
	const auto title_return = tracker.consume({
		.process = PixelOracleProcess::op,
		.scene = PixelOracleScene::title,
	}, true);
	const auto main = tracker.consume({
		.process = PixelOracleProcess::main,
		.scene = PixelOracleScene::gameplay,
		.logical_sample = 12U,
		.replay_global_frame = 19U,
		.round_frame = 7U,
	}, true);
	const auto main_same = tracker.consume({
		.process = PixelOracleProcess::main,
		.scene = PixelOracleScene::gameplay,
		.logical_sample = 12U,
		.replay_global_frame = 19U,
		.round_frame = 7U,
	}, false);
	const auto mainl = tracker.consume({
		.process = PixelOracleProcess::mainl,
		.scene = PixelOracleScene::win,
		.logical_sample = 13U,
		.replay_global_frame = 20U,
	}, true);
	const auto resumed_op = tracker.consume({
		.process = PixelOracleProcess::op,
		.scene = PixelOracleScene::title,
	}, true);

	expect(bootstrap.event == 0U && bootstrap.process_instance == 0U &&
		bootstrap.boundary == PixelOracleBoundary::bootstrap &&
		bootstrap.commit == 0U, "bootstrap starts route and OP invocation zero");
	expect(title_tick.event == 1U && title_tick.process_instance == 0U &&
		title_tick.commit == 1U && !title_tick.source_changed,
		"unchanged source boundary remains ordered and increments duplicate commit");
	expect(options.scene_visit == 1U && title_return.scene_visit == 2U,
		"every OP scene entry increments one invocation-wide scene ordinal");
	expect(main.process_instance == 1U && main.commit == 0U &&
		main.logical_sample == 12U && main.replay_global_frame == 19U &&
		main.round_frame == 7U,
		"MAIN receives a new invocation and independent logical identities");
	expect(main_same.process_instance == 1U && main_same.commit == 1U,
		"same MAIN identity has a deterministic commit ordinal");
	expect(mainl.process_instance == 2U && resumed_op.process_instance == 3U &&
		resumed_op.scene_visit == 0U,
		"MAINL and resumed OP are fresh executable invocations");
	PixelOracleIdentityTracker direct_replay_tracker(1U);
	const auto direct_mainl = direct_replay_tracker.bootstrap({
		.process = PixelOracleProcess::mainl,
		.scene = PixelOracleScene::stage_splash,
		.logical_sample = 0U,
		.replay_global_frame = 0U,
	});
	expect(direct_mainl.process_instance == 1U,
		"direct replay capture reserves the source OP selection invocation");
	PixelOracleIdentityTracker direct_main_tracker(2U);
	const auto direct_main = direct_main_tracker.bootstrap({
		.process = PixelOracleProcess::main,
		.scene = PixelOracleScene::gameplay,
	});
	expect(direct_main.process_instance == 2U,
		"direct replay whose first visible owner is MAIN reserves OP and MAINL");
	expect(pixel_oracle_direct_replay_first_process_instance(
		PixelOracleProcess::mainl
	) == 1U && pixel_oracle_direct_replay_first_process_instance(
		PixelOracleProcess::main
	) == 2U, "direct replay derives invocation offset from its actual first owner");

	constexpr std::string_view fixture =
		"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
	expect(valid_pixel_oracle_fixture_sha256(fixture) &&
		!valid_pixel_oracle_fixture_sha256("0123"),
		"fixture identity requires one exact SHA-256 hex digest");
	expect(pixel_oracle_identity_capture_hidden("capture.t3ali") &&
		!pixel_oracle_identity_capture_hidden(""),
		"identity capture config hides SDL from window creation onward");
	std::ostringstream serialized;
	expect(write_t3ali1_header(serialized) && write_t3ali1_event(
		serialized, fixture, main, 9U
	), "T3ALI1 header and publication-linked row serialize");
	const std::string expected =
		"stream\tschema\tevent\tfixture_sha256\tprocess_instance\tprocess\t"
		"scene\tscene_visit\tlogical_sample\treplay_global_frame\t"
		"round_frame\tcommit\tboundary\tsource_changed\tpublication\n"
		"T3ALI\t1\t4\t" + std::string(fixture) +
		"\t1\tmain\tgameplay\t0\t12\t19\t7\t0\tsource_tick\t1\t9\n";
	expect(serialized.str() == expected,
		"T3ALI1 freezes route, logical, boundary, and publication columns");
}

template<std::size_t Size>
auto parse(const std::array<std::string_view, Size> &arguments)
{
	return th03::app::parse_classic_cli(arguments);
}

void test_capture_cli()
{
	constexpr std::array valid{
		std::string_view("--new-classic"), std::string_view("--data"),
		std::string_view("data"), std::string_view("--main"),
		std::string_view("MAIN.EXE"), std::string_view("--mainl"),
		std::string_view("MAINL.EXE"), std::string_view("--op"),
		std::string_view("OP.EXE"), std::string_view("--font"),
		std::string_view("FONT.ROM"), std::string_view("--smoke-ticks"),
		std::string_view("200"), std::string_view("--capture-replay"),
		std::string_view("route.rpy"), std::string_view("--capture-checkpoint"),
		std::string_view("0"),
	};
	const auto accepted = parse(valid);
	expect(accepted && accepted->capture_replay == "route.rpy" &&
		accepted->capture_checkpoint == 0U && accepted->smoke_ticks == 200U,
		"bounded New Classic replay capture CLI accepts V13 and checkpoint");

	constexpr std::array no_bound{
		std::string_view("--new-classic"), std::string_view("--data"),
		std::string_view("data"), std::string_view("--main"),
		std::string_view("MAIN.EXE"), std::string_view("--mainl"),
		std::string_view("MAINL.EXE"), std::string_view("--op"),
		std::string_view("OP.EXE"), std::string_view("--font"),
		std::string_view("FONT.ROM"), std::string_view("--capture-replay"),
		std::string_view("route.rpy"),
	};
	const auto unbounded = parse(no_bound);
	expect(!unbounded && unbounded.error().code ==
		th03::app::ClassicCliErrorCode::invalid_capture_route,
		"developer capture requires an explicit smoke-tick bound");

	constexpr std::array classic_capture{
		std::string_view("--classic"), std::string_view("--data"),
		std::string_view("data"), std::string_view("--main"),
		std::string_view("MAIN.EXE"), std::string_view("--mainl"),
		std::string_view("MAINL.EXE"), std::string_view("--op"),
		std::string_view("OP.EXE"), std::string_view("--font"),
		std::string_view("FONT.ROM"), std::string_view("--smoke-ticks"),
		std::string_view("2"), std::string_view("--capture-replay"),
		std::string_view("route.rpy"),
	};
	const auto wrong_mode = parse(classic_capture);
	expect(!wrong_mode && wrong_mode.error().code ==
		th03::app::ClassicCliErrorCode::invalid_capture_route,
		"replay capture cannot silently change Classic mode authority");
	auto later_checkpoint = valid;
	later_checkpoint.back() = "3";
	const auto unsupported_checkpoint = parse(later_checkpoint);
	expect(!unsupported_checkpoint && unsupported_checkpoint.error().code ==
		th03::app::ClassicCliErrorCode::invalid_capture_route,
		"capture rejects later checkpoints until skipped invocation count is proven");
}

} // namespace

int main()
{
	test_tracker_and_schema();
	test_capture_cli();
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
