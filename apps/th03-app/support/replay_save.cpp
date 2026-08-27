#include "replay_save.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <ctime>
#include <fstream>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include <SDL3/SDL_timer.h>

#include "audio_output.hpp"
#include "diagnostics.hpp"

#include "th03/app/content/resources.hpp"

namespace th03::app::launcher_support {
namespace {

constexpr std::uint64_t maximum_idle_delay_ns = 1'000'000U;

struct HostUiStep final {
	bool surface_changed = false;
	bool done = false;
};

[[nodiscard]] std::expected<std::vector<std::byte>, int> read_file(
	const std::filesystem::path &path,
	const char *code
)
{
	std::error_code system{};
	const auto status = std::filesystem::status(path, system);
	if(system || !std::filesystem::is_regular_file(status)) {
		return std::unexpected(fail_netplay(
			"netplay-replay-save-art", code
		));
	}
	const auto size = std::filesystem::file_size(path, system);
	if(system || size > static_cast<std::uintmax_t>(
		std::numeric_limits<std::size_t>::max()
	) || size > static_cast<std::uintmax_t>(
		std::numeric_limits<std::streamsize>::max()
	)) {
		return std::unexpected(fail_netplay(
			"netplay-replay-save-art", code
		));
	}
	std::ifstream input(path, std::ios::binary);
	if(!input) {
		return std::unexpected(fail_netplay(
			"netplay-replay-save-art", code
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
		return std::unexpected(fail_netplay(
			"netplay-replay-save-art", code
		));
	}
	return bytes;
}

[[nodiscard]] std::uint16_t current_dos_date(void *) noexcept
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

template <typename Surface, typename Consume>
std::expected<void, int> run_host_ui(
	RationalTickRate tick_rate,
	platform::SdlPlatform &platform,
	platform::SdlInputMapper &mapper,
	Surface surface,
	Consume consume,
	ReplaySaveAudioContext audio
)
{
	auto presented = platform.present(surface());
	if(!presented) {
		return std::unexpected(fail("new-classic-present", presented.error()));
	}
	auto scheduler = TickInputScheduler::create(
		tick_rate,
		platform::sdl_ticks_now_ns(),
		// Replay Patch menus and a retained SCORE.M still live on the same
		// 56.423 Hz source timeline while the host owns this modal transaction.
		classic_interactive_catch_up_policy
	);
	if(!scheduler) {
		return std::unexpected(fail_tick(scheduler.error()));
	}
	bool retry_blocked_event = false;
	bool audio_focus_muted = false;
	std::vector<audio::PmdCanonicalStereoSample> gain_frames{};
	auto sync_audio_focus_mute = [&]() noexcept {
		const bool desired = audio.mute_when_unfocused &&
			*audio.mute_when_unfocused && !mapper.main_window_focused();
		if(desired && !audio_focus_muted && audio.device && *audio.device) {
			if(auto cleared = (*audio.device)->clear(); !cleared) {
				warn_audio_device(
					"replay-save-audio-focus-clear", cleared.error()
				);
				audio.device->reset();
			}
		}
		audio_focus_muted = desired;
	};
	for(;;) {
		const auto cutoff = platform::sdl_ticks_now_ns();
		if(!retry_blocked_event) {
			auto pumped = mapper.pump_events(*scheduler, false);
			if(!pumped) {
				return std::unexpected(fail(
					"new-classic-input-pump", pumped.error()
				));
			}
			retry_blocked_event = pumped->queue_blocked;
		}
		sync_audio_focus_mute();
		bool presentation_pending = false;
		bool done = false;
		for(;;) {
			auto due = scheduler->pop_due(cutoff);
			if(!due) {
				return std::unexpected(fail_tick(due.error()));
			}
			if(!*due) {
				break;
			}
			auto step = consume(new_classic_menu_ui(**due, true));
			if(!step) {
				return std::unexpected(step.error());
			}
			presentation_pending =
				presentation_pending || step->surface_changed;
			if(audio.session) {
				sync_audio_focus_mute();
				auto rendered = audio.session->advance_tick();
				if(!rendered) {
					return std::unexpected(fail(
						"replay-save-audio-tick", rendered.error()
					));
				}
				if(audio.device && *audio.device && !audio_focus_muted) {
					const std::uint8_t percent = audio.master_volume_percent ?
						*audio.master_volume_percent : std::uint8_t{100U};
					auto output = output_gain_frames(
						rendered->canonical_frames, percent, gain_frames
					);
					if(!output) {
						return std::unexpected(fail_output_gain(
							"replay-save-audio-output-gain", output.error()
						));
					}
					auto submitted = (*audio.device)->submit(*output);
					if(!submitted) {
						warn_audio_device(
							"replay-save-audio-submit", submitted.error()
						);
						audio.device->reset();
					} else if(audio.started && !*audio.started &&
						!rendered->canonical_frames.empty()) {
						auto resumed = (*audio.device)->resume();
						if(!resumed) {
							warn_audio_device(
								"replay-save-audio-resume", resumed.error()
							);
							audio.device->reset();
						} else {
							*audio.started = true;
						}
					}
				}
			}
			done = step->done;
			if(retry_blocked_event) {
				auto retried = mapper.pump_events(*scheduler, false);
				if(!retried) {
					return std::unexpected(fail(
						"new-classic-input-pump", retried.error()
					));
				}
				retry_blocked_event = retried->queue_blocked;
			}
			if(done) {
				break;
			}
		}
		if(presentation_pending) {
			presented = platform.present(surface());
			if(!presented) {
				return std::unexpected(fail(
					"new-classic-present", presented.error()
				));
			}
		}
		if(auto refreshed = refresh_host_presentability(
			platform, "new-classic-presentability-refresh"
		); !refreshed) {
			return std::unexpected(refreshed.error());
		}
		if(done) {
			return {};
		}
		const auto deadline = scheduler->next_deadline_ns();
		const auto now = platform::sdl_ticks_now_ns();
		if(deadline && *deadline > now) {
			SDL_DelayPrecise(std::min(*deadline - now, maximum_idle_delay_ns));
		}
	}
}

} // namespace

std::expected<DirectNetplayReplaySaveResources, int>
prepare_direct_netplay_replay_save_resources(
	const DirectVsContentPaths &content
)
{
	auto patch = load_new_classic_replay_patch_assets(
		content.data_root / "AZINN.DAT", content.new_classic_menu_assets,
		content.font_rom
	);
	if(!patch) {
		return std::unexpected(fail_netplay(
			"netplay-replay-save-art", "patch-assets"
		));
	}
	auto patch_assets = std::make_shared<const NewClassicReplayPatchAssets>(
		std::move(*patch)
	);

	MainResourceLoader loader(content.data_root);
	auto normal = loader.load({
		.role = MainResourceRole::registration_normal_glyphs,
	});
	auto highlight = loader.load({
		.role = MainResourceRole::registration_highlight_glyphs,
	});
	if(!normal || !highlight) {
		return std::unexpected(fail_netplay(
			"netplay-replay-save-art", "registration-glyphs"
		));
	}
	auto font = read_file(content.font_rom, "font-rom");
	if(!font) {
		return std::unexpected(font.error());
	}
	auto save_art = NewClassicReplaySaveArt::prepare(
		std::span<const std::byte>{**normal},
		std::span<const std::byte>{**highlight},
		*font
	);
	if(!save_art) {
		return std::unexpected(fail_netplay(
			"netplay-replay-save-art", "prepare"
		));
	}
	return DirectNetplayReplaySaveResources{
		.replay_root = content.data_root / "REPLAY",
		.patch_assets = std::move(patch_assets),
		.save_art = std::move(*save_art),
	};
}

ReplaySaveRunResult run_replay_save(
	formats::ReplayDocument document,
	const std::filesystem::path &replay_root,
	RationalTickRate tick_rate,
	platform::SdlPlatform &platform,
	platform::SdlInputMapper &mapper,
	std::shared_ptr<const NewClassicReplayPatchAssets> patch_assets,
	std::shared_ptr<const NewClassicReplaySaveArt> save_art,
	ReplaySaveAudioContext audio,
	std::optional<ReplaySlotLocalView> local_view
)
{
	auto catalog = scan_new_classic_replays(replay_root);
	if(!catalog) {
		return {.exit_code = fail("replay-catalog", catalog.error())};
	}
	NewClassicReplaySaveMenu menu(
		std::move(document), std::move(patch_assets), std::move(save_art),
		&current_dos_date, nullptr, local_view
	);
	bool application_quit = false;
	bool replay_published = false;
	auto driven = run_host_ui(
		tick_rate, platform, mapper,
		[&]() { return menu.surface(); },
		[&](NewClassicUiInput input) -> std::expected<HostUiStep, int> {
			auto result = menu.consume(input, *catalog);
			if(result.status == NewClassicReplaySaveStatus::save_requested) {
				auto committed = commit_replay_slot(
					replay_root, menu.selected_slot(), menu.document(), local_view
				);
				if(committed) {
					replay_published = true;
					auto refreshed = scan_new_classic_replays(replay_root);
					if(!refreshed) {
						return std::unexpected(fail(
							"replay-catalog-refresh", refreshed.error()
						));
					}
					catalog = std::move(refreshed);
				}
				menu.report_commit(committed.has_value(), *catalog);
				result.surface_changed = true;
				result.status = NewClassicReplaySaveStatus::running;
			}
			application_quit = result.status ==
				NewClassicReplaySaveStatus::application_quit;
			return HostUiStep{
				.surface_changed = result.surface_changed,
				.done = result.status != NewClassicReplaySaveStatus::running,
			};
		},
		audio
	);
	if(!driven) {
		return {.exit_code = driven.error()};
	}
	return {
		.application_quit = application_quit,
		.replay_published = replay_published,
	};
}

} // namespace th03::app::launcher_support
