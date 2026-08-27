#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_filesystem.h>

#include "default_bindings.hpp"
#include "support/diagnostics.hpp"
#include "support/experience_session.hpp"
#include "support/input_catalog.hpp"
#include "support/netplay_session.hpp"
#include "support/online_frontend.hpp"
#include "support/product_online.hpp"
#include "support/runtime_environment.hpp"
#include "support/update_host.hpp"

#include "th03/app/classic/classic_cli.hpp"
#include "th03/app/classic/classic_runner.hpp"
#include "th03/app/direct_vs/direct_vs_cli.hpp"
#include "th03/app/direct_vs/direct_vs_launch.hpp"
#include "th03/app/main/main_direct_vs_runner.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_room.hpp"
#include "th03/app/new_classic/new_classic_frontend.hpp"
#include "th03/app/new_classic/content/new_classic_language.hpp"
#include "th03/app/new_classic/online/new_classic_online_client.hpp"
#include "th03/app/new_classic/persistence/new_classic_settings_store.hpp"
#include "th03/app/runtime/startup_sequence.hpp"
#include "th03/app/runtime/tick_input.hpp"
#include "th03/platform/sdl_input.hpp"
#include "th03/platform/sdl_platform.hpp"
#include "th03/platform/rtc_room.hpp"
namespace {

using th03::app::launcher_support::fail;
using th03::app::launcher_support::fail_content;
using th03::app::launcher_support::fail_netplay;
using th03::app::launcher_support::fail_output_gain;
using th03::app::launcher_support::fail_tick;
using th03::app::launcher_support::environment_value;
using th03::app::launcher_support::ExperienceRunDisposition;
using th03::app::launcher_support::ExperienceRunResult;
using th03::app::launcher_support::fresh_netplay_seed;
using th03::app::launcher_support::name;
using th03::app::launcher_support::netplay_ice_environment;
using th03::app::launcher_support::NetplayFrontendDisposition;
using th03::app::launcher_support::NetplayFrontendHandler;
using th03::app::launcher_support::NetplayFrontendResult;
using th03::app::launcher_support::online_token_environment;
using th03::app::launcher_support::online_user_environment;
using th03::app::launcher_support::platform_config;
using th03::app::launcher_support::platform_window_presentation;
using th03::app::launcher_support::print_usage;
using th03::app::launcher_support::product_online_origin;
using th03::app::launcher_support::run_netplay_room_modal;
using th03::app::launcher_support::run_retained_menu_network_job;
#if defined(_WIN32) || defined(TH03_ENABLE_CURL_ONLINE)
using th03::app::launcher_support::run_online_profile_modal;
#endif
using th03::app::launcher_support::sdl_controller_catalog;
using th03::app::launcher_support::sdl_scancode_name;
#if defined(_WIN32) || defined(TH03_ENABLE_CURL_ONLINE)
using th03::app::launcher_support::create_product_online_transport;
using th03::app::launcher_support::load_or_enroll_product_online_credentials;
using th03::app::launcher_support::ProductOnlineError;
using th03::app::launcher_support::ProductOnlineTransport;
using th03::app::launcher_support::remove_product_online_credentials;
using th03::app::launcher_support::store_product_online_credentials;
#endif
using th03::app::launcher_support::refresh_host_presentability;
using th03::app::launcher_support::RetainedMenuAudio;
using th03::app::launcher_support::RtcNetplayRequest;
using th03::app::launcher_support::run_direct_netplay_on_platform;
using th03::app::launcher_support::run_classic_session;
#if defined(TH03_ENABLE_UPDATE_HOST)
using th03::app::launcher_support::acknowledge_update_health;
using th03::app::launcher_support::initialize_update_host;
#endif

constexpr std::uint64_t maximum_idle_delay_ns = 1'000'000U;

struct StartupRunResult final {
	int exit_code = 0;
	bool application_quit = false;
};

} // namespace

namespace {



int run_direct_netplay(th03::app::DirectVsCliRequest parsed)
{
	auto platform = th03::platform::SdlPlatform::create(platform_config());
	if(!platform) {
		return fail("platform-create", platform.error());
	}
	auto mapper = th03::platform::SdlInputMapper::create(
		th03::app::detail::new_classic_netplay_story_bindings(
			parsed.netplay_local_autofire
		),
		static_cast<SDL_WindowID>((*platform)->window_id()),
		th03::platform::native_sdl_gamepad_backend()
	);
	if(!mapper) {
		return fail("input-create", mapper.error());
	}
#if defined(TH03_ENABLE_UPDATE_HOST)
	if(auto acknowledged = acknowledge_update_health(); !acknowledged) {
		return acknowledged.error();
	}
#endif
	return run_direct_netplay_on_platform(
		std::move(parsed), **platform, *mapper
	);
}

int run_direct_vs(th03::app::DirectVsCliRequest parsed)
{
	if(parsed.netplay_mode != th03::app::DirectVsNetplayMode::none) {
		return run_direct_netplay(std::move(parsed));
	}
	auto process = th03::app::load_direct_vs_process(std::move(parsed.launch));
	if(!process) {
		return fail_content(process.error());
	}
	auto runner = th03::app::MainDirectVsRunner::create(std::move(*process));
	if(!runner) {
		return fail("runner-create", runner.error());
	}

	auto platform = th03::platform::SdlPlatform::create(platform_config());
	if(!platform) {
		return fail("platform-create", platform.error());
	}
	auto mapper = th03::platform::SdlInputMapper::create(
		th03::app::detail::classic_direct_vs_bindings(),
		static_cast<SDL_WindowID>((*platform)->window_id()),
		th03::platform::native_sdl_gamepad_backend()
	);
	if(!mapper) {
		return fail("input-create", mapper.error());
	}
#if defined(TH03_ENABLE_UPDATE_HOST)
	if(auto acknowledged = acknowledge_update_health(); !acknowledged) {
		return acknowledged.error();
	}
#endif
	auto scheduler = th03::app::TickInputScheduler::create(
		parsed.tick_rate,
		th03::platform::sdl_ticks_now_ns(),
		parsed.smoke_ticks ?
			th03::app::TickInputCatchUpPolicy::preserve_all :
			th03::app::TickInputCatchUpPolicy::discard_missed_deadlines
	);
	if(!scheduler) {
		return fail_tick(scheduler.error());
	}

	std::vector<th03::present::ComposedPixel> pixels(
		th03::present::graphics_page_pixels
	);
	const auto pixel_span = std::span<
		th03::present::ComposedPixel,
		th03::present::graphics_page_pixels
	>(pixels.data(), pixels.size());
	auto surface = (*runner)->compose_shown_surface(pixel_span);
	if(!surface) {
		return fail("compose", surface.error());
	}
	auto presented = (*platform)->present(*surface);
	if(!presented) {
		return fail("present", presented.error());
	}

	bool retry_blocked_event = false;
	bool presentation_pending = false;
	std::uint64_t process_ticks = 0U;
	while((*runner)->status() ==
		th03::app::MainDirectVsRunnerStatus::running) {
		const auto cutoff = th03::platform::sdl_ticks_now_ns();
		if(!retry_blocked_event) {
			auto result = mapper->pump_events(*scheduler, true);
			if(!result) {
				return fail("input-pump", result.error());
			}
			retry_blocked_event = result->queue_blocked;
		}

		bool smoke_complete = false;
		for(;;) {
			auto due = scheduler->pop_due(cutoff);
			if(!due) {
				return fail_tick(due.error());
			}
			if(!*due) {
				break;
			}
			auto consumed = (*runner)->consume(**due);
			if(!consumed) {
				return fail("runner", consumed.error());
			}
			presentation_pending =
				presentation_pending || consumed->surface_changed;
			if(consumed->process_advanced) {
				process_ticks++;
			}
			// A retained SDL event must be retried as soon as this logical
			// boundary frees scheduler history, before any frame is presented.
			if(retry_blocked_event) {
				auto retried = mapper->pump_events(*scheduler, true);
				if(!retried) {
					return fail("input-pump", retried.error());
				}
				retry_blocked_event = retried->queue_blocked;
			}
			if(parsed.smoke_ticks && process_ticks >= *parsed.smoke_ticks) {
				smoke_complete = true;
				break;
			}
			if(consumed->status !=
				th03::app::MainDirectVsRunnerStatus::running) {
				break;
			}
		}

		if((*runner)->status() ==
			th03::app::MainDirectVsRunnerStatus::quit_requested) {
			return 0;
		}
		const bool session_complete = (*runner)->status() ==
			th03::app::MainDirectVsRunnerStatus::session_complete;
		bool presented_this_iteration = false;
		if(presentation_pending &&
			(!retry_blocked_event || session_complete || smoke_complete)) {
			surface = (*runner)->compose_shown_surface(pixel_span);
			if(!surface) {
				return fail("compose", surface.error());
			}
			presented = (*platform)->present(*surface);
			if(!presented) {
				return fail("present", presented.error());
			}
			presentation_pending = false;
			presented_this_iteration = true;
		}
		if(!presentation_pending) {
			if(auto refreshed = refresh_host_presentability(
				**platform, "presentability-refresh"
			); !refreshed) {
				return refreshed.error();
			}
		}
		if(session_complete) {
			return 0;
		}
		if(smoke_complete) {
			return 0;
		}

		if(presented_this_iteration) {
			continue;
		}
		const auto deadline = scheduler->next_deadline_ns();
		const auto now = th03::platform::sdl_ticks_now_ns();
		if(deadline && *deadline > now) {
			SDL_DelayPrecise(std::min(*deadline - now, maximum_idle_delay_ns));
		}
	}
	return 0;
}

StartupRunResult run_startup_sequence(
	th03::app::RationalTickRate tick_rate,
	th03::platform::SdlPlatform &platform,
	th03::platform::SdlInputMapper &mapper,
	th03::app::StartupSequence sequence
)
{
	auto presented = platform.present(sequence.surface());
	if(!presented) {
		return {.exit_code = fail("startup-present", presented.error())};
	}
	auto scheduler = th03::app::TickInputScheduler::create(
		tick_rate,
		th03::platform::sdl_ticks_now_ns(),
		th03::app::TickInputCatchUpPolicy::discard_missed_deadlines
	);
	if(!scheduler) {
		return {.exit_code = fail_tick(scheduler.error())};
	}

	bool retry_blocked_event = false;
	while(sequence.status() == th03::app::StartupSequenceStatus::running) {
		const auto cutoff = th03::platform::sdl_ticks_now_ns();
		if(!retry_blocked_event) {
			auto result = mapper.pump_events(*scheduler, false);
			if(!result) {
				return {.exit_code = fail("startup-input-pump", result.error())};
			}
			retry_blocked_event = result->queue_blocked;
		}

		bool presentation_pending = false;
		for(;;) {
			auto due = scheduler->pop_due(cutoff);
			if(!due) {
				return {.exit_code = fail_tick(due.error())};
			}
			if(!*due) {
				break;
			}
			const auto &sample = **due;
			const auto consumed = sequence.consume({
				.any_input = sample.input.system != 0U ||
					sample.input.p1 != 0U || sample.input.p2 != 0U ||
					mapper.any_keyboard_scancode_held(),
				.app_quit_requested = sample.app_quit_requested,
			});
			presentation_pending =
				presentation_pending || consumed.surface_changed;
			if(retry_blocked_event) {
				auto retried = mapper.pump_events(*scheduler, false);
				if(!retried) {
					return {
						.exit_code = fail("startup-input-pump", retried.error())
					};
				}
				retry_blocked_event = retried->queue_blocked;
			}
			if(consumed.status != th03::app::StartupSequenceStatus::running) {
				break;
			}
		}

		if(presentation_pending) {
			presented = platform.present(sequence.surface());
			if(!presented) {
				return {.exit_code = fail("startup-present", presented.error())};
			}
		}
		if(auto refreshed = refresh_host_presentability(
			platform, "startup-presentability-refresh"
		); !refreshed) {
			return {.exit_code = refreshed.error()};
		}
		if(sequence.status() ==
			th03::app::StartupSequenceStatus::quit_requested) {
			return {.application_quit = true};
		}
		if(sequence.status() == th03::app::StartupSequenceStatus::failed) {
			return {.exit_code = fail(
				"startup-render",
				th03::app::StartupSequenceError{
					.code = th03::app::StartupSequenceErrorCode::draw_failed,
				}
			)};
		}
		if(sequence.status() == th03::app::StartupSequenceStatus::complete) {
			return {};
		}
		const auto deadline = scheduler->next_deadline_ns();
		const auto now = th03::platform::sdl_ticks_now_ns();
		if(deadline && *deadline > now) {
			SDL_DelayPrecise(std::min(*deadline - now, maximum_idle_delay_ns));
		}
	}
	return {};
}

std::expected<
	std::pair<
		std::unique_ptr<th03::platform::SdlPlatform>,
		th03::platform::SdlInputMapper
	>,
	int
> create_product_platform(
	bool force_hidden = false,
	std::optional<th03::app::NewClassicWindowPresentation>
		initial_presentation = std::nullopt
)
{
	auto config = platform_config(force_hidden);
	if(initial_presentation) {
		config.initial_window_presentation = platform_window_presentation(
			*initial_presentation
		);
	}
	auto platform = th03::platform::SdlPlatform::create(
		config
	);
	if(!platform) {
		return std::unexpected(fail("platform-create", platform.error()));
	}
	auto mapper = th03::platform::SdlInputMapper::create(
		th03::app::detail::classic_direct_vs_bindings(),
		static_cast<SDL_WindowID>((*platform)->window_id()),
		th03::platform::native_sdl_gamepad_backend()
	);
	if(!mapper) {
		return std::unexpected(fail("input-create", mapper.error()));
	}
#if defined(TH03_ENABLE_UPDATE_HOST)
	if(auto acknowledged = acknowledge_update_health(); !acknowledged) {
		return std::unexpected(acknowledged.error());
	}
#endif
	return std::pair{std::move(*platform), std::move(*mapper)};
}

[[nodiscard]] th03::app::DirectVsCliRequest product_netplay_request(
	const th03::app::ClassicCliRequest &product,
	const th03::app::NewClassicSettings &settings,
	const th03::app::NewClassicNetplayRoomLaunch &launch
);

[[nodiscard]] std::expected<std::optional<RtcNetplayRequest>, int>
product_rtc_netplay_request(
	const th03::app::NewClassicNetplayRoomLaunch &launch,
	const th03::app::ClassicContentPaths &content
);

ExperienceRunResult run_new_classic_experience(
	th03::app::ClassicCliRequest parsed,
	th03::platform::SdlPlatform &platform,
	th03::platform::SdlInputMapper &mapper
)
{
	constexpr auto maximum_key = static_cast<std::uint16_t>(
		SDL_SCANCODE_COUNT
	);
	const auto key_defaults = th03::app::detail::new_classic_default_key_config();
	auto loaded_settings = th03::app::load_new_classic_settings_store(
		parsed.launch.content.new_classic_settings,
		parsed.launch.content.config
	);
	if(!loaded_settings) {
		return {.exit_code = fail("settings-load", loaded_settings.error())};
	}
	auto settings = loaded_settings->settings;
	if(!parsed.capture_replay && parsed.launch.initial_route ==
		th03::app::ClassicRunnerInitialRoute::title &&
		th03::stock_gameplay_rules(parsed.launch.gameplay_ruleset)) {
		parsed.launch.gameplay_ruleset = settings.gameplay_ruleset;
	}
	auto config_options = settings.options;
	auto patch_asset_path = parsed.launch.content.replay_patch_assets;
	if(patch_asset_path.empty()) {
		patch_asset_path = parsed.launch.content.data_root / "AZINN.DAT";
	}
	auto loaded_patch_assets =
		th03::app::load_new_classic_replay_patch_assets(
			patch_asset_path, parsed.launch.content.new_classic_menu_assets,
			parsed.launch.content.font_rom
		);
	if(!loaded_patch_assets) {
		return {.exit_code = fail("replay-patch-assets", loaded_patch_assets.error())};
	}
	if(!parsed.launch.content.language_pack.empty()) {
		const auto &fallback = th03::app::new_classic_builtin_messages(
			th03::app::NewClassicLanguageAssetProfile::english
		);
		auto loaded_messages = th03::app::load_new_classic_message_pack(
			parsed.launch.content.language_pack,
			loaded_patch_assets->menu_font, &fallback
		);
		if(!loaded_messages) {
			return {.exit_code = fail("language-pack", loaded_messages.error())};
		}
		loaded_patch_assets->custom_messages = std::make_shared<
			const th03::app::NewClassicMessageCatalog
		>(std::move(*loaded_messages));
	}
	if(!parsed.launch.content.game_text_pack.empty()) {
		const bool english =
			config_options.language == th03::app::ClassicLanguage::english;
		if(english && parsed.launch.content.language_overlay.empty()) {
			return {.exit_code = fail_output_gain(
				"game-text-pack", "english-overlay-required"
			)};
		}
		const auto &fallback = english ?
			th03::app::new_classic_builtin_english_game_text() :
			th03::app::new_classic_builtin_japanese_game_text();
		auto loaded_game_text = th03::app::load_new_classic_game_text_pack(
			parsed.launch.content.game_text_pack, &fallback
		);
		if(!loaded_game_text) {
			return {.exit_code = fail("game-text-pack", loaded_game_text.error())};
		}
		if((loaded_game_text->locale() == "ja-JP") == english) {
			return {.exit_code = fail_output_gain(
				"game-text-pack", "locale-mismatch"
			)};
		}
		if(loaded_patch_assets->custom_messages &&
			loaded_patch_assets->custom_messages->locale() !=
				loaded_game_text->locale()) {
			return {.exit_code = fail_output_gain(
				"game-text-pack", "locale-mismatch"
			)};
		}
		loaded_patch_assets->custom_game_text = std::make_shared<
			const th03::app::NewClassicGameTextCatalog
		>(std::move(*loaded_game_text));
	}
	if(!parsed.launch.content.cutscene_text_pack.empty()) {
		auto loaded_cutscene_text =
			th03::app::load_new_classic_cutscene_text_pack(
				parsed.launch.content.cutscene_text_pack
			);
		if(!loaded_cutscene_text) {
			std::cerr << "status=error\nstage=cutscene-text-pack\ncode="
				<< name(loaded_cutscene_text.error().code)
				<< "\noffset=" << loaded_cutscene_text.error().offset
				<< "\nscript=" << loaded_cutscene_text.error().script_index
				<< "\nrun=" << loaded_cutscene_text.error().run_index << '\n';
			return {.exit_code = 1};
		}
		const bool japanese_catalog =
			loaded_cutscene_text->source_profile_fnv1a64() ==
				th03::app::new_classic_cutscene_original_profile_fnv1a64;
		const bool english_catalog =
			loaded_cutscene_text->source_profile_fnv1a64() ==
				th03::app::new_classic_cutscene_english_profile_fnv1a64;
		if(!japanese_catalog && !english_catalog) {
			return {.exit_code = fail_output_gain(
				"cutscene-text-pack", "source-profile-mismatch"
			)};
		}
		if(!japanese_catalog && parsed.launch.content.language_overlay.empty()) {
			return {.exit_code = fail_output_gain(
				"cutscene-text-pack", "english-overlay-required"
			)};
		}
		const auto expected_locale = japanese_catalog ?
			std::string_view{"ja-JP"} :
			(loaded_patch_assets->custom_game_text ?
				loaded_patch_assets->custom_game_text->locale() :
				th03::app::new_classic_builtin_english_game_text().locale());
		if(loaded_cutscene_text->locale() != expected_locale ||
			(loaded_patch_assets->custom_messages &&
				loaded_patch_assets->custom_messages->locale() !=
					loaded_cutscene_text->locale())) {
			return {.exit_code = fail_output_gain(
				"cutscene-text-pack", "locale-mismatch"
			)};
		}
		loaded_patch_assets->custom_cutscene_text = std::make_shared<
			const th03::app::NewClassicCutsceneTextCatalog
		>(std::move(*loaded_cutscene_text));
	}
	auto patch_assets = std::make_shared<
		const th03::app::NewClassicReplayPatchAssets
	>(std::move(*loaded_patch_assets));
	bool english_available = false;
	if(!parsed.launch.content.language_overlay.empty()) {
		auto overlay = th03::app::load_new_classic_language_overlay(
			parsed.launch.content.language_overlay
		);
		if(!overlay) {
			return {.exit_code = fail("language-overlay", overlay.error())};
		}
		english_available = true;
	}
	if(!english_available &&
		config_options.language == th03::app::ClassicLanguage::english) {
		config_options.language = th03::app::ClassicLanguage::japanese;
		settings.options = config_options;
		if(auto stored = th03::app::commit_new_classic_settings_store(
			parsed.launch.content.new_classic_settings, settings
		); !stored) {
			return {.exit_code = fail("settings-language-fallback", stored.error())};
		}
	}
	const auto key_path = parsed.launch.content.config.parent_path() /
		"TH3KEY.CFG";
	const auto joy_defaults = th03::app::detail::new_classic_default_joy_config();
	const auto joy_path = parsed.launch.content.config.parent_path() /
		"TH3JOY.CFG";
	const th03::app::NewClassicDeviceAssignment device_defaults{};
	const auto device_path = parsed.launch.content.config.parent_path() /
		"TH3DEV.CFG";
	auto loaded_keys = th03::app::load_new_classic_key_config(
		key_path, key_defaults, maximum_key, config_options.legacy_autofire
	);
	if(!loaded_keys) {
		return {.exit_code = fail("key-config-load", loaded_keys.error())};
	}
	auto key_config = loaded_keys->config;
	if(loaded_keys->recovered_defaults) {
		std::cerr << "status=warning\nstage=key-config-load\n"
			"code=recovered-defaults\n";
	}
	auto loaded_joy = th03::app::load_new_classic_joy_config(
		joy_path, joy_defaults
	);
	if(!loaded_joy) {
		return {.exit_code = fail("joy-config-load", loaded_joy.error())};
	}
	auto joy_config = loaded_joy->config;
	if(loaded_joy->recovered_defaults) {
		std::cerr << "status=warning\nstage=joy-config-load\n"
			"code=recovered-defaults\n";
	}
	auto loaded_device = th03::app::load_new_classic_device_assignment(
		device_path, device_defaults
	);
	if(!loaded_device) {
		return {
			.exit_code = fail("device-assignment-load", loaded_device.error())
		};
	}
	auto device_assignment = loaded_device->assignment;
	if(loaded_device->recovered_defaults) {
		std::cerr << "status=warning\nstage=device-assignment-load\n"
			"code=recovered-defaults\n";
	}
	if(auto replaced = mapper.replace_bindings(
		th03::app::detail::new_classic_bindings(
			key_config, joy_config, device_assignment
		)
	); !replaced) {
		return {.exit_code = fail("key-config-apply", replaced.error())};
	}
	parsed.mode = th03::app::ClassicLaunchMode::new_classic;
	parsed.launch.autofire_mask = key_config.autofire;
	parsed.launch.reduced_flash = config_options.reduced_flash;
	parsed.launch.new_classic_key_config = key_config;
	parsed.launch.new_classic_key_defaults = key_defaults;
	parsed.launch.new_classic_joy_config = joy_config;
	parsed.launch.new_classic_joy_defaults = joy_defaults;
	parsed.launch.new_classic_device_assignment = device_assignment;
	parsed.launch.new_classic_device_defaults = device_defaults;
	parsed.launch.new_classic_controller_catalog = &sdl_controller_catalog;
	parsed.launch.new_classic_controller_context = &mapper;
	parsed.launch.new_classic_maximum_key = maximum_key;
	parsed.launch.new_classic_capture_cancel_key = static_cast<std::uint16_t>(
		SDL_SCANCODE_ESCAPE
	);
	parsed.launch.new_classic_key_name = &sdl_scancode_name;
	parsed.launch.initial_route = th03::app::ClassicRunnerInitialRoute::title;
	// Replay, Practice, KeyConfig, Options, and replay playback are children of
	// the patched OP session. Its title is the normal product root; no Replay
	// Patch screen is substituted by a host route.
	NetplayFrontendHandler netplay_frontend = [&](const RetainedMenuAudio &audio) {
		auto latest_settings = th03::app::load_new_classic_settings_store(
			parsed.launch.content.new_classic_settings,
			parsed.launch.content.config
		);
		if(!latest_settings) {
			return NetplayFrontendResult{
				.exit_code = fail("settings-load", latest_settings.error())
			};
		}
		auto latest_keys = th03::app::load_new_classic_key_config(
			key_path, key_defaults, maximum_key,
			latest_settings->settings.options.legacy_autofire
		);
		if(!latest_keys) {
			return NetplayFrontendResult{
				.exit_code = fail("key-config-load", latest_keys.error())
			};
		}
		auto latest_joy = th03::app::load_new_classic_joy_config(
			joy_path, joy_defaults
		);
		if(!latest_joy) {
			return NetplayFrontendResult{
				.exit_code = fail("joy-config-load", latest_joy.error())
			};
		}
		auto latest_device = th03::app::load_new_classic_device_assignment(
			device_path, device_defaults
		);
		if(!latest_device) {
			return NetplayFrontendResult{
				.exit_code = fail(
					"device-assignment-load", latest_device.error()
				)
			};
		}
		key_config = latest_keys->config;
		joy_config = latest_joy->config;
		device_assignment = latest_device->assignment;
		parsed.launch.autofire_mask = key_config.autofire;
		parsed.launch.new_classic_key_config = key_config;
		parsed.launch.new_classic_joy_config = joy_config;
		parsed.launch.new_classic_device_assignment = device_assignment;
		if(auto replaced = mapper.replace_bindings(
			th03::app::detail::new_classic_bindings(
				key_config, joy_config, device_assignment
			)
		); !replaced) {
			return NetplayFrontendResult{
				.exit_code = fail("key-config-apply", replaced.error())
			};
		}
		const auto language = patch_assets->custom_messages ?
			patch_assets->custom_messages->asset_profile() :
			(latest_settings->settings.options.language ==
				th03::app::ClassicLanguage::english ?
				th03::app::NewClassicLanguageAssetProfile::english :
				th03::app::NewClassicLanguageAssetProfile::japanese);
		th03::app::NewClassicNetplayRoom room(
			patch_assets, language,
			static_cast<std::uint8_t>(latest_settings->settings.options.rank),
			latest_settings->settings.netplay
		);
#if defined(_WIN32) || defined(TH03_ENABLE_CURL_ONLINE)
		{
			auto settings_path = parsed.launch.content.new_classic_settings;
			if(settings_path.empty()) {
				settings_path = parsed.launch.content.config.parent_path() /
					"PODD.CFG";
			}
			std::error_code identity_error{};
			const bool saved_identity = std::filesystem::is_regular_file(
				settings_path.parent_path() / "PODD.AUTH", identity_error
			);
			const auto environment_user = environment_value(
				online_user_environment
			);
			const auto environment_token = environment_value(
				online_token_environment
			);
			const bool environment_identity = environment_user &&
				environment_token &&
				th03::app::validate_new_classic_online_credentials({
					.user_id = *environment_user,
					.token = *environment_token,
				});
			if(!saved_identity && !environment_identity &&
				!room.request_profile()) {
				return NetplayFrontendResult{
					.exit_code = fail_netplay(
						"online-profile", "onboarding-request-failed"
					)
				};
			}
		}
#endif
		for(;;) {
			auto room_run = run_netplay_room_modal(
				parsed.tick_rate, platform, mapper, room, &audio
			);
			if(room_run.exit_code != 0) {
				return NetplayFrontendResult{.exit_code = room_run.exit_code};
			}
			if(room_run.preferences &&
				*room_run.preferences != latest_settings->settings.netplay) {
				latest_settings->settings.netplay = *room_run.preferences;
				if(auto committed = th03::app::commit_new_classic_settings_store(
					parsed.launch.content.new_classic_settings,
					latest_settings->settings
				); !committed) {
					return NetplayFrontendResult{
						.exit_code = fail("settings-save", committed.error())
					};
				}
			}
			if(room_run.application_quit) {
				return NetplayFrontendResult{
					.disposition = NetplayFrontendDisposition::application_quit
				};
			}
			const bool account_requested = room_run.profile_requested ||
				(room_run.launch && room_run.launch->transport ==
					th03::app::NewClassicNetplayRoomTransport::online_service);
			if(account_requested) {
#if defined(_WIN32) || defined(TH03_ENABLE_CURL_ONLINE)
				const auto origin = product_online_origin();
				auto settings_path = parsed.launch.content.new_classic_settings;
				if(settings_path.empty()) {
					settings_path = parsed.launch.content.config.parent_path() /
						"PODD.CFG";
				}
				const auto credential_path = settings_path.parent_path() /
					"PODD.AUTH";
				auto online_transport = origin ?
					create_product_online_transport(*origin) :
					std::expected<ProductOnlineTransport, ProductOnlineError>{
						std::unexpected(ProductOnlineError{})
					};
				std::expected<
					th03::app::NewClassicOnlineCredentials,
					ProductOnlineError
				> credentials = std::unexpected(ProductOnlineError{});
				bool account_application_quit = false;
				const auto environment_user = environment_value(
					online_user_environment
				);
				const auto environment_token = environment_value(
					online_token_environment
				);
				bool environment_credentials = false;
				if(environment_user && environment_token &&
					th03::app::validate_new_classic_online_credentials({
						.user_id = *environment_user,
						.token = *environment_token,
					})) {
					environment_credentials = true;
					credentials = th03::app::NewClassicOnlineCredentials{
						*environment_user, *environment_token
					};
				} else if(online_transport) {
					auto acquired = run_retained_menu_network_job(
						parsed.tick_rate, platform, mapper, audio,
						account_application_quit,
						[credential_path,
						 transport = online_transport->transport()]() {
							return load_or_enroll_product_online_credentials(
								credential_path, transport
							);
						}
					);
					if(!acquired) {
						return NetplayFrontendResult{.exit_code = acquired.error()};
					}
					if(account_application_quit) {
						return NetplayFrontendResult{
							.disposition =
								NetplayFrontendDisposition::application_quit
						};
					}
					credentials = std::move(*acquired);
				}
				std::expected<
					th03::app::NewClassicOnlineProfile,
					th03::app::NewClassicOnlineClientError
				> profile = std::unexpected(
					th03::app::NewClassicOnlineClientError{}
				);
				if(credentials && online_transport) {
					auto fetched = run_retained_menu_network_job(
						parsed.tick_rate, platform, mapper, audio,
						account_application_quit,
						[credentials = *credentials,
						 transport = online_transport->transport()]() {
							return th03::app::fetch_new_classic_online_profile(
								credentials, transport
							);
						}
					);
					if(!fetched) {
						return NetplayFrontendResult{.exit_code = fetched.error()};
					}
					if(account_application_quit) {
						return NetplayFrontendResult{
							.disposition =
								NetplayFrontendDisposition::application_quit
						};
					}
					profile = std::move(*fetched);
				}
				if(!profile && credentials && online_transport &&
					!environment_credentials &&
					th03::app::new_classic_online_credentials_rejected(
						profile.error()
					)) {
					if(auto removed = remove_product_online_credentials(
						credential_path
					); !removed) {
						return NetplayFrontendResult{
							.exit_code = removed.error().code
						};
					}
					auto reacquired = run_retained_menu_network_job(
						parsed.tick_rate, platform, mapper, audio,
						account_application_quit,
						[credential_path,
						 transport = online_transport->transport()]() {
							return load_or_enroll_product_online_credentials(
								credential_path, transport
							);
						}
					);
					if(!reacquired) {
						return NetplayFrontendResult{
							.exit_code = reacquired.error()
						};
					}
					if(account_application_quit) {
						return NetplayFrontendResult{
							.disposition =
								NetplayFrontendDisposition::application_quit
						};
					}
					credentials = std::move(*reacquired);
					auto refetched = run_retained_menu_network_job(
						parsed.tick_rate, platform, mapper, audio,
						account_application_quit,
						[credentials = *credentials,
						 transport = online_transport->transport()]() {
							return th03::app::fetch_new_classic_online_profile(
								credentials, transport
							);
						}
					);
					if(!refetched) {
						return NetplayFrontendResult{
							.exit_code = refetched.error()
						};
					}
					if(account_application_quit) {
						return NetplayFrontendResult{
							.disposition =
								NetplayFrontendDisposition::application_quit
						};
					}
					profile = std::move(*refetched);
				}
				if(!profile) {
					if(room_run.profile_requested) {
						if(!room.show_profile_failure()) {
							return NetplayFrontendResult{
								.exit_code = fail_netplay(
									"online-profile", "failure-screen-failed"
								)
							};
						}
					} else if(!room.show_launch_failure()) {
						return NetplayFrontendResult{
							.exit_code = fail_netplay(
								"online-profile", "failure-screen-failed"
							)
						};
					}
					continue;
				}
				const auto cache_profile_nametag = [&credentials, &credential_path,
					environment_credentials](
					const th03::app::NewClassicOnlineProfile &current
				) -> std::expected<void, int> {
					if(environment_credentials || !credentials || !current.nametag ||
						credentials->cached_nametag == current.nametag) {
						return {};
					}
					auto updated = *credentials;
					updated.cached_nametag = current.nametag;
					if(auto stored = store_product_online_credentials(
						credential_path, updated
					); !stored) {
						return std::unexpected(fail_netplay(
							"online-profile-cache", "store-failed"
						));
					}
					*credentials = std::move(updated);
					return {};
				};
				if(auto cached = cache_profile_nametag(*profile); !cached) {
					return NetplayFrontendResult{.exit_code = cached.error()};
				}
				if(room_run.profile_requested || !profile->nametag) {
					auto edited = run_online_profile_modal(
						parsed.tick_rate, platform, mapper, patch_assets, language,
						credential_path, *online_transport, *credentials,
						std::move(*profile), &audio
					);
					if(edited.exit_code != 0) {
						return NetplayFrontendResult{.exit_code = edited.exit_code};
					}
					if(edited.application_quit) {
						return NetplayFrontendResult{
							.disposition =
								NetplayFrontendDisposition::application_quit
						};
					}
					if(edited.profile) {
						if(auto cached = cache_profile_nametag(*edited.profile);
							!cached) {
							return NetplayFrontendResult{
								.exit_code = cached.error()
							};
						}
					}
					if(room_run.profile_requested) {
						if(!room.resume_after_profile()) {
							return NetplayFrontendResult{
								.exit_code = fail_netplay(
									"online-profile", "resume-failed"
								)
							};
						}
						continue;
					}
					if(!edited.profile || !edited.profile->nametag) {
						if(!room.resume_after_launch()) {
							return NetplayFrontendResult{
								.exit_code = fail_netplay(
									"online-profile", "cancel-resume-failed"
								)
							};
						}
						continue;
					}
				}
#else
				if(room_run.profile_requested) {
					if(!room.resume_after_profile()) {
						return NetplayFrontendResult{.exit_code = fail_netplay(
							"online-profile", "platform-unavailable"
						)};
					}
					continue;
				}
#endif
			}
			if(!room_run.launch) {
				return NetplayFrontendResult{
					.disposition = NetplayFrontendDisposition::resume_title
				};
			}
			auto rtc = product_rtc_netplay_request(
				*room_run.launch, parsed.launch.content
			);
			if(!rtc) {
				if(!room.show_launch_failure()) {
					return NetplayFrontendResult{
						.exit_code = fail_netplay(
							"netplay-room", "failure-screen-failed"
						)
					};
				}
				continue;
			}
			bool gameplay_started = false;
			bool quit = false;
			const int match = run_direct_netplay_on_platform(
				product_netplay_request(
					parsed, latest_settings->settings, *room_run.launch
				),
				platform, mapper, std::move(*rtc), &audio,
				&gameplay_started, &quit,
				&key_config, &joy_config, &device_assignment
			);
			if(quit) {
				return NetplayFrontendResult{
					.disposition = NetplayFrontendDisposition::application_quit
				};
			}
			if(!gameplay_started) {
				if(match != 0) {
					if(!room.show_launch_failure()) {
						return NetplayFrontendResult{
							.exit_code = fail_netplay(
								"netplay-room", "failure-screen-failed"
							)
						};
					}
					continue;
				}
				if(!room.resume_after_launch()) {
					return NetplayFrontendResult{
						.exit_code = fail_netplay(
							"netplay-room", "resume-failed"
						)
					};
				}
				continue;
			}
			if(match != 0) {
				if(const auto resumed = audio.resume(); !resumed) {
					return NetplayFrontendResult{
						.exit_code = resumed.error()
					};
				}
				if(auto restored = mapper.replace_bindings(
					th03::app::detail::new_classic_bindings(
						key_config, joy_config, device_assignment
					)
				); !restored) {
					return NetplayFrontendResult{
						.exit_code = fail(
							"netplay-room-input-restore", restored.error()
						)
					};
				}
				if(!room.show_launch_failure()) {
					return NetplayFrontendResult{
						.exit_code = fail_netplay(
							"netplay-room", "failure-screen-failed"
						)
					};
				}
				continue;
			}
			if(auto restored = mapper.replace_bindings(
				th03::app::detail::new_classic_bindings(
					key_config, joy_config, device_assignment
				)
			); !restored) {
				return NetplayFrontendResult{
					.exit_code = fail(
						"netplay-room-input-restore", restored.error()
					)
				};
			}
			return NetplayFrontendResult{
				.disposition = NetplayFrontendDisposition::restart_new_classic
			};
		}
	};
	for(;;) {
		auto result = run_classic_session(
			parsed, platform, mapper, patch_assets, &netplay_frontend
		);
		if(result.exit_code == 0 && result.disposition ==
			ExperienceRunDisposition::restart_new_classic) {
			continue;
		}
		return result;
	}
}

[[nodiscard]] th03::app::DirectVsCliRequest product_netplay_request(
	const th03::app::ClassicCliRequest &product,
	const th03::app::NewClassicSettings &settings,
	const th03::app::NewClassicNetplayRoomLaunch &launch
)
{
	th03::app::DirectVsCliRequest result{};
	result.launch.content = {
		.data_root = product.launch.content.data_root,
		.main_executable = product.launch.content.main_executable,
		.mainl_executable = product.launch.content.mainl_executable,
		.font_rom = product.launch.content.font_rom,
		.language_overlay = product.launch.content.language_overlay,
		.game_text_pack = product.launch.content.game_text_pack,
		.new_classic_menu_assets =
			product.launch.content.new_classic_menu_assets,
	};
	result.launch.process.rank = launch.rank;
	result.launch.process.seed = fresh_netplay_seed();
	result.launch.process.game_options.input_protocol = th03::InputProtocol::live;
	result.launch.process.game_options.three_digit_combos = true;
	result.tick_rate = product.tick_rate;
	result.replay_root = product.replay_root;
	result.netplay_input_delay_frames = launch.input_delay_frames;
	result.netplay_local_autofire = (product.launch.autofire_mask & 1U) != 0U;
	result.master_volume_percent = settings.master_volume_percent;
	result.music_volume_percent = settings.music_volume_percent;
	result.effect_volume_percent = settings.effect_volume_percent;
	result.mute_when_unfocused = settings.mute_when_unfocused;
	result.presentation_language = settings.options.language ==
		th03::app::ClassicLanguage::english ?
			th03::app::NewClassicLanguageAssetProfile::english :
			th03::app::NewClassicLanguageAssetProfile::japanese;
	if(launch.mode == th03::app::NewClassicNetplayRoomLaunchMode::host) {
		result.netplay_mode = th03::app::DirectVsNetplayMode::host;
		result.netplay_host_port = launch.host_port;
		result.launch.process.player_one = {
			.character = static_cast<th03::app::MainCharacter>(launch.character),
			.palette = static_cast<th03::app::MainPaletteVariant>(launch.palette),
		};
	} else if(launch.mode ==
		th03::app::NewClassicNetplayRoomLaunchMode::join) {
		result.netplay_mode = th03::app::DirectVsNetplayMode::join;
		result.netplay_join_endpoint = launch.join_endpoint;
		result.launch.process.player_two = {
			.character = static_cast<th03::app::MainCharacter>(launch.character),
			.palette = static_cast<th03::app::MainPaletteVariant>(launch.palette),
		};
	} else {
		// Matchmaking assigns the network role after signaling. Both local lock
		// projections therefore carry the one selection made on this keyboard.
		result.netplay_mode = th03::app::DirectVsNetplayMode::host;
		result.launch.process.player_one = {
			.character = static_cast<th03::app::MainCharacter>(launch.character),
			.palette = static_cast<th03::app::MainPaletteVariant>(launch.palette),
		};
		result.launch.process.player_two = result.launch.process.player_one;
	}
	return result;
}

[[nodiscard]] std::expected<std::optional<RtcNetplayRequest>, int>
product_rtc_netplay_request(
	const th03::app::NewClassicNetplayRoomLaunch &launch,
	const th03::app::ClassicContentPaths &content
)
{
#if !defined(_WIN32) && !defined(TH03_ENABLE_RTC_TRANSPORT)
	(void)content;
#endif
	if(launch.transport !=
		th03::app::NewClassicNetplayRoomTransport::online_service) {
		return std::optional<RtcNetplayRequest>{};
	}
	const auto origin = product_online_origin();
	const auto user = environment_value(online_user_environment);
	const auto token = environment_value(online_token_environment);
	if(!origin || origin->empty()) {
		return std::unexpected(fail_netplay(
			"netplay-room-account", "online-origin-missing"
		));
	}
	std::optional<th03::app::NewClassicOnlineCredentials> credentials{};
	if(user || token) {
		if(!user || !token || !th03::app::validate_new_classic_online_credentials({
			.user_id = *user,
			.token = *token,
		})) {
			return std::unexpected(fail_netplay(
				"netplay-room-account", "online-environment-invalid"
			));
		}
		credentials = th03::app::NewClassicOnlineCredentials{*user, *token};
	} else {
#if defined(_WIN32)
		auto settings_path = content.new_classic_settings;
		if(settings_path.empty()) {
			settings_path = content.config.parent_path() / "PODD.CFG";
		}
		const auto credential_root = settings_path.parent_path();
		if(credential_root.empty()) {
			return std::unexpected(fail_netplay(
				"netplay-room-account", "credential-path-missing"
			));
		}
		auto transport =
			th03::platform::create_windows_online_transport(*origin);
		if(!transport) {
			return std::unexpected(fail_netplay(
				"netplay-room-account", "online-origin-invalid"
			));
		}
		auto acquired =
			th03::platform::load_or_enroll_windows_online_credentials(
				credential_root / "PODD.AUTH", transport->transport()
			);
		if(!acquired) {
			return std::unexpected(fail_netplay(
				"netplay-room-account",
				acquired.error().code == th03::platform::
					WindowsOnlineIdentityErrorCode::load ?
						"credential-load-failed" :
					(acquired.error().code == th03::platform::
						WindowsOnlineIdentityErrorCode::enroll ?
							"credential-enroll-failed" :
							"credential-store-failed")
			));
		}
		credentials = std::move(*acquired);
#else
#if defined(TH03_ENABLE_RTC_TRANSPORT)
		auto settings_path = content.new_classic_settings;
		if(settings_path.empty()) {
			settings_path = content.config.parent_path() / "PODD.CFG";
		}
		const auto credential_root = settings_path.parent_path();
		if(credential_root.empty()) {
			return std::unexpected(fail_netplay(
				"netplay-room-account", "credential-path-missing"
			));
		}
		auto acquired =
			th03::platform::load_or_enroll_rtc_online_credentials(
				credential_root / "PODD.AUTH", *origin
			);
		if(!acquired) {
			std::cerr << "netplay-identity-code=" <<
				static_cast<unsigned>(acquired.error().code) << '\n';
			return std::unexpected(fail_netplay(
				"netplay-room-account", "credential-enroll-failed"
			));
		}
		credentials = th03::app::NewClassicOnlineCredentials{
			.user_id = std::move(acquired->user_id),
			.token = std::move(acquired->token),
		};
#else
		return std::unexpected(fail_netplay(
			"netplay-room-account", "rtc-not-built"
		));
#endif
#endif
	}
	RtcNetplayRequest result{
		.mode = launch.mode ==
			th03::app::NewClassicNetplayRoomLaunchMode::quick_match ?
				th03::platform::RtcRoomMode::matchmaking :
			(launch.mode == th03::app::NewClassicNetplayRoomLaunchMode::join ?
				th03::platform::RtcRoomMode::join :
				th03::platform::RtcRoomMode::create),
		.origin = *origin,
		.user_id = credentials->user_id,
		.token = credentials->token,
		.room_code = launch.mode ==
			th03::app::NewClassicNetplayRoomLaunchMode::join ?
				launch.join_endpoint : std::string{},
		.rank = launch.rank,
		.relay_only = launch.relay_only,
	};
	if(const auto ice = environment_value(netplay_ice_environment); ice) {
		std::size_t start = 0U;
		while(start <= ice->size()) {
			const auto separator = ice->find(';', start);
			const auto value = ice->substr(
				start, separator == std::string::npos ? std::string::npos :
					separator - start
			);
			if(value.empty()) {
				return std::unexpected(fail_netplay(
					"netplay-room-account", "invalid-ice-servers"
				));
			}
			result.ice_servers.push_back({.url = value});
			if(separator == std::string::npos) {
				break;
			}
			start = separator + 1U;
		}
	}
	return std::optional<RtcNetplayRequest>{std::move(result)};
}

int run_classic(th03::app::ClassicCliRequest parsed)
{
	std::optional<th03::app::NewClassicWindowPresentation>
		initial_presentation{};
	if(parsed.mode == th03::app::ClassicLaunchMode::new_classic) {
		auto settings = th03::app::load_new_classic_settings_store(
			parsed.launch.content.new_classic_settings,
			parsed.launch.content.config
		);
		if(!settings) {
			return fail("settings-load", settings.error());
		}
		initial_presentation = settings->settings.window;
	}
	auto product = create_product_platform(
		parsed.capture_replay.has_value(), initial_presentation
	);
	if(!product) {
		return product.error();
	}
	auto result = parsed.mode == th03::app::ClassicLaunchMode::new_classic ?
		run_new_classic_experience(
			parsed, *product->first, product->second
		) : run_classic_session(
			parsed, *product->first, product->second
		);
	if(result.exit_code == 0 && result.disposition ==
		ExperienceRunDisposition::switch_to_classic) {
		parsed.mode = th03::app::ClassicLaunchMode::classic;
		result = run_classic_session(
			std::move(parsed), *product->first, product->second
		);
	}
	if(result.exit_code == 0 && result.disposition ==
		ExperienceRunDisposition::netplay_requested) {
		std::cerr << "status=error\nstage=netplay-room\n"
			"code=retained-frontend-missing\n";
		return 1;
	}
	return result.exit_code;
}

int run_product_shell(th03::app::ClassicCliRequest parsed)
{
	auto settings = th03::app::load_new_classic_settings_store(
		parsed.launch.content.new_classic_settings,
		parsed.launch.content.config
	);
	if(!settings) {
		return fail("settings-load", settings.error());
	}
	auto product = create_product_platform(
		false, settings->settings.window
	);
	if(!product) {
		return product.error();
	}
	// Automated packaging smoke exercises the normal direct New Classic root
	// without waiting for a physical title selection.
	if(parsed.smoke_ticks) {
		parsed.mode = th03::app::ClassicLaunchMode::new_classic;
		return run_new_classic_experience(
			std::move(parsed), *product->first, product->second
		).exit_code;
	}
	auto startup = th03::app::StartupSequence::load(
		parsed.launch.content.data_root / "TOUHOU.DAT"
	);
	if(!startup) {
		return fail("startup-logo-load", startup.error());
	}
	const auto startup_result = run_startup_sequence(
		parsed.tick_rate,
		*product->first,
		product->second,
		std::move(*startup)
	);
	if(startup_result.exit_code != 0 || startup_result.application_quit) {
		return startup_result.exit_code;
	}

	for(;;) {
		parsed.mode = th03::app::ClassicLaunchMode::new_classic;
		auto experience = run_new_classic_experience(
			parsed, *product->first, product->second
		);
		if(experience.exit_code != 0) {
			return experience.exit_code;
		}
		if(experience.disposition ==
			ExperienceRunDisposition::netplay_requested) {
			std::cerr << "status=error\nstage=netplay-room\n"
				"code=retained-frontend-missing\n";
			return 1;
		}
		const auto new_classic_transition =
			th03::app::product_lifecycle_transition(
				th03::app::ExperienceProfile::new_classic,
				experience.disposition ==
					ExperienceRunDisposition::switch_to_classic ?
					th03::app::ProductLifecycleSignal::switch_to_classic :
					(experience.disposition ==
						ExperienceRunDisposition::application_quit ?
						th03::app::ProductLifecycleSignal::application_quit :
						th03::app::ProductLifecycleSignal::return_to_menu)
			);
		if(new_classic_transition.exit_application) {
			return 0;
		}

		if(auto restored = product->second.replace_bindings(
			th03::app::detail::classic_direct_vs_bindings()
		); !restored) {
			return fail("classic-input-policy", restored.error());
		}
		parsed.mode = th03::app::ClassicLaunchMode::classic;
		experience = run_classic_session(
			parsed, *product->first, product->second
		);
		if(experience.exit_code != 0 || experience.disposition !=
			ExperienceRunDisposition::return_to_menu) {
			return experience.exit_code;
		}
		const auto classic_transition =
			th03::app::product_lifecycle_transition(
				th03::app::ExperienceProfile::classic,
				th03::app::ProductLifecycleSignal::return_to_menu
			);
		if(classic_transition.exit_application || classic_transition.next !=
			th03::app::ExperienceProfile::new_classic) {
			std::cerr << "status=error\nstage=product-lifecycle\n"
				"code=invalid-classic-return\n";
			return 1;
		}
		// Classic's exact Quit has completed. Begin a fresh New Classic OP
		// owner on the next iteration without replaying startup.
	}
}

template <typename ContentPaths>
void bind_packaged_menu_assets(ContentPaths &content)
{
	if(!content.new_classic_menu_assets.empty()) {
		return;
	}
	const auto *base_path = SDL_GetBasePath();
	if(base_path && *base_path != '\0') {
		content.new_classic_menu_assets =
			std::filesystem::path(base_path) / "PODDUI";
	}
}

} // namespace

int main(int argc, char **argv)
{
#if defined(TH03_ENABLE_UPDATE_HOST)
	if(auto initialized = initialize_update_host(); !initialized) {
		return initialized.error();
	}
#endif
	std::vector<std::string_view> arguments;
	arguments.reserve(static_cast<std::size_t>(std::max(argc - 1, 0)));
	for(int index = 1; index < argc; index++) {
		arguments.emplace_back(argv[index]);
	}

	const bool classic = std::ranges::find(arguments, "--classic") !=
		arguments.end() ||
		std::ranges::find(arguments, "--new-classic") != arguments.end() ||
		std::ranges::find(arguments, "--shell") != arguments.end();
	if(classic) {
		auto parsed = th03::app::parse_classic_cli(arguments);
		if(!parsed) {
			return fail("arguments", parsed.error());
		}
		if(parsed->show_help) {
			print_usage();
			return 0;
		}
		bind_packaged_menu_assets(parsed->launch.content);
		if(parsed->mode == th03::app::ClassicLaunchMode::shell) {
			return run_product_shell(std::move(*parsed));
		}
		return run_classic(std::move(*parsed));
	}

	auto parsed = th03::app::parse_direct_vs_cli(arguments);
	if(!parsed) {
		return fail("arguments", parsed.error());
	}
	if(parsed->show_help) {
		print_usage();
		return 0;
	}
	bind_packaged_menu_assets(parsed->launch.content);
	return run_direct_vs(std::move(*parsed));
}
