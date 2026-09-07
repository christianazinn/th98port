#include "online_frontend.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_timer.h>

#include "th03/app/new_classic/online/new_classic_online_client.hpp"

namespace th03::app::launcher_support {
namespace {

constexpr std::uint64_t maximum_idle_delay_ns = 1'000'000U;

// Text fields own the keyboard while SDL text input is active. In particular,
// printable Z/X/numpad keys must never be reinterpreted through fixed System,
// Story, or VS gameplay bindings. Only explicit editor/navigation scancodes
// cross this boundary; SDL_TEXT_INPUT remains the sole source of characters.
[[nodiscard]] th03::app::NewClassicUiInput project_new_classic_text_entry_ui(
	const th03::app::LogicalTickInput &sample,
	std::optional<SDL_Scancode> scancode
) noexcept
{
	using Command = th03::app::NewClassicTextEditorCommand;
	auto command = Command::none;
	if(scancode) {
		switch(*scancode) {
		case SDL_SCANCODE_UP: command = Command::up; break;
		case SDL_SCANCODE_DOWN: command = Command::down; break;
		case SDL_SCANCODE_LEFT: command = Command::left; break;
		case SDL_SCANCODE_RIGHT: command = Command::right; break;
		case SDL_SCANCODE_RETURN:
		case SDL_SCANCODE_KP_ENTER: command = Command::confirm; break;
		case SDL_SCANCODE_ESCAPE: command = Command::cancel; break;
		default: break;
		}
	}
	return th03::app::new_classic_text_editor_ui(sample, command);
}


class NetplayTextInputOwner final {
public:
	explicit NetplayTextInputOwner(SDL_WindowID window_id) noexcept :
		window_(SDL_GetWindowFromID(window_id))
	{
	}

	~NetplayTextInputOwner()
	{
		if(active_ && window_ != nullptr) {
			static_cast<void>(SDL_StopTextInput(window_));
		}
	}

	[[nodiscard]] bool valid() const noexcept { return window_ != nullptr; }

	[[nodiscard]] bool set_active(bool active) noexcept
	{
		if(window_ == nullptr) {
			return false;
		}
		if(active == active_) {
			return true;
		}
		const bool changed = active ? SDL_StartTextInput(window_) :
			SDL_StopTextInput(window_);
		if(changed) {
			active_ = active;
		}
		return changed;
	}

private:
	SDL_Window *window_ = nullptr;
	bool active_ = false;
};

} // namespace

[[nodiscard]] NetplayRoomRunResult run_netplay_room_modal(
	th03::app::RationalTickRate tick_rate,
	th03::platform::SdlPlatform &platform,
	th03::platform::SdlInputMapper &mapper,
	th03::app::NewClassicNetplayRoom &room,
	const RetainedMenuAudio *retained_audio
)
{
	if(!room.valid()) {
		return {.exit_code = fail_netplay(
			"netplay-room", "surface-create-failed"
		)};
	}
	auto scheduler = th03::app::TickInputScheduler::create(
		tick_rate, th03::platform::sdl_ticks_now_ns(),
		th03::app::TickInputCatchUpPolicy::discard_missed_deadlines
	);
	if(!scheduler) {
		return {.exit_code = fail_tick(scheduler.error())};
	}
	if(const auto reset = mapper.reset_after_modal(
		*scheduler, th03::platform::sdl_ticks_now_ns()
	); !reset) {
		return {.exit_code = fail("netplay-room-input", reset.error())};
	}
	if(const auto presented = platform.present(room.surface()); !presented) {
		return {.exit_code = fail("netplay-room-present", presented.error())};
	}
	NetplayTextInputOwner text_input(
		static_cast<SDL_WindowID>(platform.window_id())
	);
	if(!text_input.valid()) {
		return {.exit_code = fail_netplay(
			"netplay-room-input", "window-missing"
		)};
	}

	bool retry_blocked_event = false;
	while(room.status() == th03::app::NewClassicNetplayRoomStatus::running) {
		const auto cutoff = th03::platform::sdl_ticks_now_ns();
		if(!retry_blocked_event) {
			auto pumped = mapper.pump_events(*scheduler, false);
			if(!pumped) {
				return {.exit_code = fail(
					"netplay-room-input", pumped.error()
				)};
			}
			retry_blocked_event = pumped->queue_blocked;
		}
		bool surface_changed = false;
		for(;;) {
			auto due = scheduler->pop_due(cutoff);
			if(!due) {
				return {.exit_code = fail_tick(due.error())};
			}
			if(!*due) {
				break;
			}
			if(retained_audio) {
				const auto advanced = retained_audio->advance_tick();
				if(!advanced) {
					return {.exit_code = advanced.error()};
				}
			}
			while(const auto character = mapper.take_keyboard_text_character()) {
				const auto typed = room.consume({.text_character = *character});
				surface_changed = surface_changed || typed.surface_changed;
				if(typed.status !=
					th03::app::NewClassicNetplayRoomStatus::running) {
					break;
				}
			}
			const auto scancode = mapper.take_keyboard_scancode_press();
			const bool text_entry = room.text_entry_active();
			const auto consumed = room.consume({
				.ui = text_entry ?
					project_new_classic_text_entry_ui(**due, scancode) :
					th03::app::new_classic_menu_ui(**due, true),
				.erase = text_entry && scancode &&
					*scancode == SDL_SCANCODE_BACKSPACE,
				.delete_forward = text_entry && scancode &&
					*scancode == SDL_SCANCODE_DELETE,
				.home = text_entry && scancode &&
					*scancode == SDL_SCANCODE_HOME,
				.end = text_entry && scancode &&
					*scancode == SDL_SCANCODE_END,
			});
			if(!text_input.set_active(room.text_entry_active())) {
				return {.exit_code = fail_netplay(
					"netplay-room-input", "text-input-state"
				)};
			}
			if(!room.text_entry_active()) {
				while(mapper.take_keyboard_text_character()) {
				}
			}
			surface_changed = surface_changed || consumed.surface_changed;
			if(retry_blocked_event) {
				auto retried = mapper.pump_events(*scheduler, false);
				if(!retried) {
					return {.exit_code = fail(
						"netplay-room-input", retried.error()
					)};
				}
				retry_blocked_event = retried->queue_blocked;
			}
			if(consumed.status !=
				th03::app::NewClassicNetplayRoomStatus::running) {
				break;
			}
		}
		if(surface_changed) {
			if(const auto presented = platform.present(room.surface()); !presented) {
				return {.exit_code = fail(
					"netplay-room-present", presented.error()
				)};
			}
		} else if(auto refreshed = refresh_host_presentability(
			platform, "netplay-room-presentability"
		); !refreshed) {
			return {.exit_code = refreshed.error()};
		}
		const auto deadline = scheduler->next_deadline_ns();
		const auto now = th03::platform::sdl_ticks_now_ns();
		if(deadline && *deadline > now) {
			SDL_DelayPrecise(std::min(*deadline - now, maximum_idle_delay_ns));
		}
	}
	if(room.status() ==
		th03::app::NewClassicNetplayRoomStatus::application_quit) {
		return {
			.application_quit = true,
			.preferences = room.preferences(),
		};
	}
	if(room.status() ==
		th03::app::NewClassicNetplayRoomStatus::launch_requested) {
		return {
			.launch = room.take_launch(),
			.preferences = room.preferences(),
		};
	}
	if(room.status() ==
		th03::app::NewClassicNetplayRoomStatus::profile_requested) {
		return {
			.profile_requested = true,
			.preferences = room.preferences(),
		};
	}
	return {.preferences = room.preferences()};
}

#if defined(_WIN32) || defined(TH03_ENABLE_CURL_ONLINE)
[[nodiscard]] OnlineProfileRunResult run_online_profile_modal(
	th03::app::RationalTickRate tick_rate,
	th03::platform::SdlPlatform &platform,
	th03::platform::SdlInputMapper &mapper,
	std::shared_ptr<const th03::app::NewClassicReplayPatchAssets> patch_assets,
	th03::app::NewClassicLanguageAssetProfile language,
	const std::filesystem::path &credential_path,
	ProductOnlineTransport &online_transport,
	th03::app::NewClassicOnlineCredentials &credentials,
	th03::app::NewClassicOnlineProfile profile,
	const RetainedMenuAudio *retained_audio
)
{
	th03::app::NewClassicOnlineProfileScene scene(
		std::move(patch_assets), language, profile.nametag
	);
	if(!scene.valid()) {
		return {.exit_code = fail_netplay(
			"online-profile", "surface-create-failed"
		)};
	}
	auto scheduler = th03::app::TickInputScheduler::create(
		tick_rate, th03::platform::sdl_ticks_now_ns(),
		th03::app::TickInputCatchUpPolicy::discard_missed_deadlines
	);
	if(!scheduler) {
		return {.exit_code = fail_tick(scheduler.error())};
	}
	if(const auto reset = mapper.reset_after_modal(
		*scheduler, th03::platform::sdl_ticks_now_ns()
	); !reset) {
		return {.exit_code = fail("online-profile-input", reset.error())};
	}
	if(const auto presented = platform.present(scene.surface()); !presented) {
		return {.exit_code = fail("online-profile-present", presented.error())};
	}
	NetplayTextInputOwner text_input(
		static_cast<SDL_WindowID>(platform.window_id())
	);
	if(!text_input.valid()) {
		return {.exit_code = fail_netplay(
			"online-profile-input", "window-missing"
		)};
	}

	bool retry_blocked_event = false;
	for(;;) {
		const auto cutoff = th03::platform::sdl_ticks_now_ns();
		if(!retry_blocked_event) {
			auto pumped = mapper.pump_events(*scheduler, false);
			if(!pumped) {
				return {.exit_code = fail(
					"online-profile-input", pumped.error()
				)};
			}
			retry_blocked_event = pumped->queue_blocked;
		}
		bool surface_changed = false;
		for(;;) {
			auto due = scheduler->pop_due(cutoff);
			if(!due) {
				return {.exit_code = fail_tick(due.error())};
			}
			if(!*due) {
				break;
			}
			if(retained_audio) {
				const auto advanced = retained_audio->advance_tick();
				if(!advanced) {
					return {.exit_code = advanced.error()};
				}
			}
			if(auto text = mapper.take_keyboard_text_utf8(); !text.empty()) {
				surface_changed = scene.consume({.text_utf8 = std::move(text)}) ||
					surface_changed;
			}
			const auto scancode = mapper.take_keyboard_scancode_press();
			const bool text_entry = scene.text_entry_active();
			surface_changed = scene.consume({
				.ui = text_entry ?
					project_new_classic_text_entry_ui(**due, scancode) :
					th03::app::new_classic_menu_ui(**due, true),
				.erase = text_entry && scancode &&
					*scancode == SDL_SCANCODE_BACKSPACE,
				.delete_forward = text_entry && scancode &&
					*scancode == SDL_SCANCODE_DELETE,
				.home = text_entry && scancode &&
					*scancode == SDL_SCANCODE_HOME,
				.end = text_entry && scancode &&
					*scancode == SDL_SCANCODE_END,
			}) || surface_changed;
			if(!text_input.set_active(scene.text_entry_active())) {
				return {.exit_code = fail_netplay(
					"online-profile-input", "text-input-state"
				)};
			}
			if(!scene.text_entry_active()) {
				static_cast<void>(mapper.take_keyboard_text_utf8());
			}
			if(auto action = scene.take_action()) {
				if(surface_changed) {
					if(const auto presented = platform.present(scene.surface());
						!presented) {
						return {.exit_code = fail(
							"online-profile-present", presented.error()
						)};
					}
					surface_changed = false;
				}
				using Action = th03::app::NewClassicOnlineProfileActionType;
				if(action->type == Action::back) {
					return {.profile = std::move(profile)};
				}
				if(action->type == Action::application_quit) {
					return {.application_quit = true};
				}
				bool request_quit = false;
				const auto run_request = [&]<typename Work>(Work work) ->
					std::expected<std::invoke_result_t<Work>, int> {
					if(retained_audio) {
						return run_retained_menu_network_job(
							tick_rate, platform, mapper, *retained_audio,
							request_quit, std::move(work)
						);
					}
					try {
						return std::invoke(std::move(work));
					} catch(...) {
						return std::unexpected(fail_netplay(
							"online-profile", "request-failed"
						));
					}
				};
				bool reported = false;
				if(action->type == Action::update_nametag) {
					auto request = run_request([
						text = action->text, credentials,
						transport = online_transport.transport()
					]() {
						return th03::app::update_new_classic_online_profile(
							text, credentials, transport
						);
					});
					if(!request) {
						return {.exit_code = request.error()};
					}
					if(request_quit) {
						return {.application_quit = true};
					}
					if(*request) {
						profile = **request;
						reported = scene.report_profile(std::move(**request));
					}
				} else if(action->type == Action::create_link_code) {
					auto request = run_request([
						credentials, transport = online_transport.transport()
					]() {
						return th03::app::create_new_classic_online_link_code(
							credentials, transport
						);
					});
					if(!request) {
						return {.exit_code = request.error()};
					}
					if(request_quit) {
						return {.application_quit = true};
					}
					if(*request) {
						reported = scene.report_link_code(
							std::move((*request)->code)
						);
					}
				} else {
					auto request = run_request([
						code = action->text,
						transport = online_transport.transport()
					]() {
						return th03::app::redeem_new_classic_online_link_code(
							code, transport
						);
					});
					if(!request) {
						return {.exit_code = request.error()};
					}
					if(request_quit) {
						return {.application_quit = true};
					}
					if(*request && store_product_online_credentials(
						credential_path, **request
					)) {
						credentials = std::move(**request);
						auto loaded_request = run_request([
							credentials,
							transport = online_transport.transport()
						]() {
							return th03::app::fetch_new_classic_online_profile(
								credentials, transport
							);
						});
						if(!loaded_request) {
							return {.exit_code = loaded_request.error()};
						}
						if(request_quit) {
							return {.application_quit = true};
						}
						if(*loaded_request) {
							profile = **loaded_request;
							reported = scene.report_linked(
								std::move(**loaded_request)
							);
						}
					}
				}
				if(!reported && !scene.report_failure()) {
					return {.exit_code = fail_netplay(
						"online-profile", "request-report-failed"
					)};
				}
				surface_changed = true;
				if(const auto reset = mapper.reset_after_modal(
					*scheduler, th03::platform::sdl_ticks_now_ns()
				); !reset) {
					return {.exit_code = fail(
						"online-profile-input", reset.error()
					)};
				}
				retry_blocked_event = false;
			}
			if(retry_blocked_event) {
				auto retried = mapper.pump_events(*scheduler, false);
				if(!retried) {
					return {.exit_code = fail(
						"online-profile-input", retried.error()
					)};
				}
				retry_blocked_event = retried->queue_blocked;
			}
		}
		if(surface_changed) {
			if(const auto presented = platform.present(scene.surface()); !presented) {
				return {.exit_code = fail(
					"online-profile-present", presented.error()
				)};
			}
		} else if(auto refreshed = refresh_host_presentability(
			platform, "online-profile-presentability"
		); !refreshed) {
			return {.exit_code = refreshed.error()};
		}
		const auto deadline = scheduler->next_deadline_ns();
		const auto now = th03::platform::sdl_ticks_now_ns();
		if(deadline && *deadline > now) {
			SDL_DelayPrecise(std::min(*deadline - now, maximum_idle_delay_ns));
		}
	}
}
#endif

} // namespace th03::app::launcher_support
