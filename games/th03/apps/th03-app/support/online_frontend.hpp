#pragma once

#include <algorithm>
#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

#include <SDL3/SDL_timer.h>

#include "diagnostics.hpp"
#include "netplay_session.hpp"
#include "product_online.hpp"
#include "runtime_environment.hpp"

#include "th03/app/new_classic/content/new_classic_assets.hpp"
#include "th03/app/new_classic/content/new_classic_language.hpp"
#include "th03/app/new_classic/netplay/new_classic_netplay_room.hpp"
#include "th03/app/new_classic/online/new_classic_online_profile_scene.hpp"
#include "th03/app/runtime/tick_input.hpp"
#include "th03/platform/sdl_input.hpp"
#include "th03/platform/sdl_platform.hpp"

namespace th03::app::launcher_support {

template <typename Work>
[[nodiscard]] std::expected<std::invoke_result_t<Work>, int>
run_retained_menu_network_job(
	RationalTickRate tick_rate,
	platform::SdlPlatform &platform,
	platform::SdlInputMapper &mapper,
	const RetainedMenuAudio &retained_audio,
	bool &application_quit,
	Work work
)
{
	auto scheduler = TickInputScheduler::create(
		tick_rate, platform::sdl_ticks_now_ns(),
		TickInputCatchUpPolicy::discard_missed_deadlines
	);
	if(!scheduler) {
		return std::unexpected(fail_tick(scheduler.error()));
	}
	if(const auto reset = mapper.reset_after_modal(
		*scheduler, platform::sdl_ticks_now_ns()
	); !reset) {
		return std::unexpected(fail("online-profile-input", reset.error()));
	}

	std::future<std::invoke_result_t<Work>> future;
	try {
		future = std::async(std::launch::async, std::move(work));
	} catch(...) {
		return std::unexpected(fail_netplay(
			"online-profile", "worker-start"
		));
	}
	bool retry_blocked_event = false;
	while(future.wait_for(std::chrono::nanoseconds::zero()) !=
		std::future_status::ready) {
		const auto cutoff = platform::sdl_ticks_now_ns();
		if(!retry_blocked_event) {
			auto pumped = mapper.pump_events(*scheduler, false);
			if(!pumped) {
				return std::unexpected(fail(
					"online-profile-input", pumped.error()
				));
			}
			retry_blocked_event = pumped->queue_blocked;
		}
		for(;;) {
			auto due = scheduler->pop_due(cutoff);
			if(!due) {
				return std::unexpected(fail_tick(due.error()));
			}
			if(!*due) {
				break;
			}
			if(const auto advanced = retained_audio.advance_tick(); !advanced) {
				return std::unexpected(advanced.error());
			}
			application_quit = application_quit ||
				new_classic_menu_ui(**due, true).app_quit_requested;
			if(retry_blocked_event) {
				auto retried = mapper.pump_events(*scheduler, false);
				if(!retried) {
					return std::unexpected(fail(
						"online-profile-input", retried.error()
					));
				}
				retry_blocked_event = retried->queue_blocked;
			}
		}
		if(auto refreshed = refresh_host_presentability(
			platform, "online-profile-presentability"
		); !refreshed) {
			return std::unexpected(refreshed.error());
		}
		const auto deadline = scheduler->next_deadline_ns();
		const auto now = platform::sdl_ticks_now_ns();
		if(deadline && *deadline > now) {
			constexpr std::uint64_t retained_network_maximum_idle_delay_ns =
				1'000'000U;
			SDL_DelayPrecise(std::min(
				*deadline - now, retained_network_maximum_idle_delay_ns
			));
		}
	}
	try {
		return future.get();
	} catch(...) {
		return std::unexpected(fail_netplay(
			"online-profile", "worker-failed"
		));
	}
}

struct NetplayRoomRunResult final {
	int exit_code = 0;
	bool application_quit = false;
	bool profile_requested = false;
	std::optional<NewClassicNetplayRoomLaunch> launch{};
	std::optional<NewClassicNetplayPreferences> preferences{};
};

[[nodiscard]] NetplayRoomRunResult run_netplay_room_modal(
	RationalTickRate tick_rate,
	platform::SdlPlatform &platform,
	platform::SdlInputMapper &mapper,
	NewClassicNetplayRoom &room,
	const RetainedMenuAudio *retained_audio = nullptr
);

#if defined(_WIN32) || defined(TH03_ENABLE_CURL_ONLINE)

struct OnlineProfileRunResult final {
	int exit_code = 0;
	bool application_quit = false;
	std::optional<NewClassicOnlineProfile> profile{};
};

[[nodiscard]] OnlineProfileRunResult run_online_profile_modal(
	RationalTickRate tick_rate,
	platform::SdlPlatform &platform,
	platform::SdlInputMapper &mapper,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	NewClassicLanguageAssetProfile language,
	const std::filesystem::path &credential_path,
	ProductOnlineTransport &online_transport,
	NewClassicOnlineCredentials &credentials,
	NewClassicOnlineProfile profile,
	const RetainedMenuAudio *retained_audio = nullptr
);

#endif

} // namespace th03::app::launcher_support
