#include "experience_session.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iostream>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <SDL3/SDL_timer.h>

#include "../default_bindings.hpp"
#include "audio_output.hpp"
#include "diagnostics.hpp"
#include "product_online.hpp"
#include "replay_save.hpp"
#include "rhythm_assets.hpp"
#include "runtime_environment.hpp"
#include "update_host.hpp"

#include "th03/app/classic/classic_audio_session.hpp"
#include "th03/app/classic/persistence/classic_config_store.hpp"
#include "th03/app/classic/classic_runner.hpp"
#include "th03/app/content/main_compiled_content.hpp"
#include "th03/app/new_classic/new_classic_frontend.hpp"
#include "th03/app/new_classic/content/new_classic_language.hpp"
#include "th03/app/new_classic/online/new_classic_online_client.hpp"
#include "th03/app/new_classic/online/new_classic_online_store.hpp"
#include "th03/app/new_classic/online/new_classic_online_task.hpp"
#include "th03/app/new_classic/persistence/new_classic_settings_store.hpp"
#include "th03/app/new_classic/replay/new_classic_playback.hpp"
#include "th03/app/new_classic/replay/replay_slot_store.hpp"
#include "th03/app/runtime/tick_input.hpp"
#include "th03/formats/v14.hpp"
#include "th03/platform/sdl_audio_device.hpp"

#if defined(TH03_ENABLE_UPDATE_HOST)
#include "th03/app/update/update_bootstrap.hpp"
#include "th03/app/update/update_task.hpp"
#if defined(_WIN32)
#include "th03/platform/windows_update.hpp"
#else
#include "th03/platform/curl_update.hpp"
#endif
#endif

namespace th03::app::launcher_support {
namespace {

constexpr std::uint64_t maximum_idle_delay_ns = 1'000'000U;

} // namespace

ExperienceRunResult run_classic_session(
	th03::app::ClassicCliRequest parsed,
	th03::platform::SdlPlatform &platform,
	th03::platform::SdlInputMapper &mapper,
	std::shared_ptr<const th03::app::NewClassicReplayPatchAssets>
		patch_assets,
	NetplayFrontendHandler *netplay_frontend
)
{
	const auto data_root = parsed.launch.content.data_root;
	const auto config_path = parsed.launch.content.config;
	const auto new_classic_settings_path =
		parsed.launch.content.new_classic_settings;
	const bool new_classic =
		parsed.mode == th03::app::ClassicLaunchMode::new_classic;
	const bool stock_rules = th03::stock_gameplay_rules(
		parsed.launch.gameplay_ruleset
	);
	if(parsed.rhythm_rom && parsed.rhythm_pcm) {
		std::cerr << "status=error\nstage=audio-profile\n"
			"code=conflicting-rhythm-profiles\n";
		return {.exit_code = 1};
	}
	std::vector<std::byte> rhythm_rom{};
	if(parsed.rhythm_rom) {
		auto loaded = th03::app_executable::load_rhythm_rom(
			*parsed.rhythm_rom
		);
		if(!loaded) {
			return {.exit_code = th03::app_executable::report_rhythm_load_failure(
				"rhythm-rom", loaded.error()
			)};
		}
		rhythm_rom = std::move(*loaded);
	}
	th03::app_executable::LoadedRhythmPcm rhythm_pcm{};
	if(parsed.rhythm_pcm) {
		auto loaded = th03::app_executable::load_rhythm_pcm(
			*parsed.rhythm_pcm
		);
		if(!loaded) {
			return {.exit_code = th03::app_executable::report_rhythm_load_failure(
				"rhythm-pcm", loaded.error()
			)};
		}
		rhythm_pcm = std::move(*loaded);
	}
	parsed.launch.fm_available = true;
	parsed.launch.experience =
		new_classic ?
			th03::app::ExperienceProfile::new_classic :
			th03::app::ExperienceProfile::classic;
	parsed.launch.content.replay_root = parsed.replay_root;
#if defined(_WIN32) || defined(TH03_ENABLE_CURL_ONLINE)
	{
		const auto credential_root = new_classic_settings_path.empty() ?
			config_path.parent_path() : new_classic_settings_path.parent_path();
		const auto credential_path = credential_root / "PODD.AUTH";
		std::error_code credential_error{};
		const bool saved_identity = std::filesystem::is_regular_file(
			credential_path, credential_error
		);
		const bool environment_identity =
			environment_value(online_user_environment).has_value() ||
			environment_value(online_token_environment).has_value();
		parsed.launch.new_classic_online_identity_reset_available =
			new_classic && saved_identity && !credential_error &&
			!environment_identity;
		std::optional<std::string> replay_user = environment_value(
			online_user_environment
		);
		std::optional<std::string> replay_nametag{};
		if(!replay_user && saved_identity && !credential_error) {
			if(const auto loaded = load_product_online_credentials(credential_path);
				loaded && *loaded) {
				replay_user = (**loaded).user_id;
				replay_nametag = (**loaded).cached_nametag;
			}
		}
		if(replay_user) {
			if(const auto uuid = th03::app::parse_new_classic_replay_account_uuid(
				*replay_user
			)) {
				parsed.launch.new_classic_replay_identity.player_uuid[0U] = *uuid;
				if(replay_nametag) {
					parsed.launch.new_classic_replay_identity.
						player_nametag_snapshot[0U] = std::move(*replay_nametag);
				}
			}
		}
	}
#endif
	if(parsed.capture_replay) {
		auto replay = th03::app::load_replay_file(*parsed.capture_replay);
		if(!replay) {
			return {.exit_code = fail("capture-replay-load", replay.error())};
		}
		if(!replay->document) {
			std::cerr << "status=error\nstage=capture-replay-load\n"
				"code=file-missing\n";
			return {.exit_code = 1};
		}
		parsed.launch.initial_route =
			th03::app::ClassicRunnerInitialRoute::new_classic_replay;
		parsed.launch.initial_replay = th03::app::ClassicRunnerInitialReplay{
			.document = std::move(*replay->document),
			.checkpoint_index = parsed.capture_checkpoint,
		};
	}
	auto active_key_config = parsed.launch.new_classic_key_config;
	auto active_joy_config = parsed.launch.new_classic_joy_config;
	auto active_device_assignment =
		parsed.launch.new_classic_device_assignment;
	const auto online_root =
		new_classic_settings_path.parent_path() / "ONLINE";
	const auto online_origin = product_online_origin();
#if defined(_WIN32) || defined(TH03_ENABLE_CURL_ONLINE)
	parsed.launch.new_classic_online_upload_available = new_classic &&
		stock_rules &&
		online_origin &&
		!parsed.capture_replay &&
		!platform.oracle_enabled();
	std::optional<ProductOnlineTransport> online_transport_owner{};
	std::optional<th03::app::NewClassicOnlineTransport> online_transport{};
	struct OnlineUploadCredentialContext final {
		std::filesystem::path path{};
		std::optional<th03::app::NewClassicOnlineCredentials> configured{};
		bool configured_invalid = false;
		th03::app::NewClassicOnlineTransport transport{};
	};
	std::optional<OnlineUploadCredentialContext>
		online_upload_credential_context{};
	std::optional<th03::app::NewClassicOnlineCredentialSource>
		online_credential_source{};
	std::optional<th03::app::NewClassicOnlineCredentialSource>
		online_existing_credential_source{};
#else
	parsed.launch.new_classic_online_upload_available = false;
#endif
#if defined(TH03_ENABLE_UPDATE_HOST)
	const auto &update_release =
		th03::app::compiled_update_release_configuration();
#if defined(_WIN32)
	const auto update_transport = th03::platform::windows_update_transport();
#else
	const auto update_transport = th03::platform::curl_update_transport();
#endif
	const bool update_available = th03::app::new_classic_update_available(
		new_classic, static_cast<bool>(parsed.capture_replay),
		platform.oracle_enabled(), update_release,
		current_update_runtime_context(),
		update_transport
	);
	parsed.launch.new_classic_update_available = update_available;
	std::optional<th03::app::UpdateInstallTask> update_install_task{};
#endif
	// The platform transport must outlive the worker that refers to it.
	std::optional<th03::app::NewClassicOnlineBoardRefreshTask>
		online_refresh_task{};
	std::optional<th03::app::NewClassicOnlineMyRecordsRefreshTask>
		online_my_records_refresh_task{};
	bool online_my_records_refresh_requested = false;
	std::optional<th03::app::NewClassicOnlineReplayDownloadTask>
		online_replay_download_task{};
	std::optional<th03::app::NewClassicOnlineReplayId>
		online_replay_download_id{};
	std::optional<th03::app::NewClassicOnlineReplayUploadTask>
		online_replay_upload_task{};
	std::optional<std::uint8_t> online_replay_upload_slot{};
	const auto overlay_online_downloads = [&](auto &document) {
		for(auto &record : document.replay_records) {
			record.availability =
				th03::app::NewClassicOnlineReplayAvailability::metadata_only;
			auto downloaded =
				th03::app::probe_new_classic_online_replay_download(
					record, parsed.replay_root
				);
			if(downloaded && *downloaded) {
				record.availability = th03::app::
					NewClassicOnlineReplayAvailability::downloaded;
			} else if(!downloaded) {
				std::cerr << "status=warning\n"
					"stage=online-replay-probe\nrank="
					<< static_cast<unsigned>(record.category.rank)
					<< "\ncharacter="
					<< static_cast<unsigned>(record.category.character)
					<< "\ncode="
					<< static_cast<unsigned>(downloaded.error().code) << '\n';
			}
		}
	};
	if(new_classic) {
		for(std::uint8_t rank = 0U; rank < 4U; ++rank) {
			for(std::uint8_t character = 0U;
				character <= th03::app::new_classic_online_overall_character;
				++character) {
				const th03::app::NewClassicOnlineCategory category{
					.mode = th03::app::NewClassicOnlineMode::story,
					.rank = rank,
					.character = character,
				};
				const auto path =
					th03::app::new_classic_online_board_cache_path(
						online_root, category
					);
				auto loaded = th03::app::load_new_classic_online_store(path);
				if(!loaded) {
					std::cerr << "status=warning\nstage=online-cache-load\n"
						"rank=" << static_cast<unsigned>(rank) << "\ncharacter="
						<< static_cast<unsigned>(character) << "\ncode="
						<< static_cast<unsigned>(loaded.error().code) << '\n';
					continue;
				}
				if(loaded->document.board_category) {
					overlay_online_downloads(loaded->document);
					parsed.launch.new_classic_online_boards.push_back(
						std::move(loaded->document)
					);
				}
			}
		}
	}
	auto runner = th03::app::ClassicRunner::load(std::move(parsed.launch));
	if(!runner) {
		return {.exit_code = fail("th03-load", runner.error())};
	}
	auto identity_output_result = pixel_oracle_identity_output(
		platform.oracle_enabled()
	);
	if(!identity_output_result) {
		std::cerr << "status=error\nstage=pixel-oracle-identity\ncode="
			<< identity_output_result.error() << '\n';
		return {.exit_code = 1};
	}
	auto identity_output = std::move(*identity_output_result);
	std::optional<th03::app::PixelOracleCaptureSelector>
		pixel_oracle_selector{};
	if(const auto selector = environment_value(
		pixel_oracle_selector_environment
	)) {
		if(!parsed.capture_replay || !identity_output.enabled()) {
			std::cerr << "status=error\nstage=pixel-oracle-selector\n"
				"code=selector-requires-replay-identity-capture\n";
			return {.exit_code = 1};
		}
		auto parsed_selector =
			th03::app::parse_pixel_oracle_capture_selector(*selector);
		if(!parsed_selector) {
			std::cerr << "status=error\nstage=pixel-oracle-selector\ncode="
				<< parsed_selector.error() << '\n';
			return {.exit_code = 1};
		}
		pixel_oracle_selector = *parsed_selector;
	}
	// Authenticate the selected game corpus before creating or recovering a
	// persistence file. A misspelled/incomplete content root must remain a
	// read-only startup failure rather than leaving a stray YUME.CFG behind.
	th03::app::ClassicConfigOptions persisted_options{};
	std::uint8_t master_volume_percent = 100U;
	std::uint8_t music_volume_percent = 100U;
	std::uint8_t effect_volume_percent = 100U;
	bool mute_when_unfocused = false;
	[[maybe_unused]] bool automatic_updates = true;
	th03::app::NewClassicWindowPresentation window_presentation{};
	if(new_classic) {
		auto persisted = th03::app::load_new_classic_settings_store(
			new_classic_settings_path, config_path
		);
		if(!persisted) {
			return {.exit_code = fail("settings-load", persisted.error())};
		}
		persisted_options = persisted->settings.options;
		master_volume_percent = persisted->settings.master_volume_percent;
		music_volume_percent = persisted->settings.music_volume_percent;
		effect_volume_percent = persisted->settings.effect_volume_percent;
		mute_when_unfocused = persisted->settings.mute_when_unfocused;
		automatic_updates = persisted->settings.automatic_updates;
		window_presentation = persisted->settings.window;
		if(auto applied = platform.apply_window_presentation(
			platform_window_presentation(window_presentation)
		); !applied) {
			return {.exit_code = fail("window-presentation", applied.error())};
		}
	} else {
		auto persisted = th03::app::load_classic_config_store(config_path);
		if(!persisted) {
			return {.exit_code = fail("config-load", persisted.error())};
		}
		persisted_options = persisted->options;
	}
#if defined(TH03_ENABLE_UPDATE_HOST)
	static bool automatic_update_attempted = false;
	if(th03::app::claim_new_classic_automatic_update_attempt(
		update_available, automatic_updates, automatic_update_attempted
	)) {
		if(auto begun = (*runner)->begin_new_classic_update_check(); !begun) {
			return {.exit_code = fail("update-check-begin", begun.error())};
		}
		if(!update_release || !current_update_runtime_context()) {
			if(auto reported = (*runner)->report_new_classic_update_check(
				th03::app::NewClassicUpdateCheckOutcome::failed
			); !reported) {
				return {.exit_code = fail(
					"update-check-report", reported.error()
				)};
			}
		} else {
			auto started = th03::app::start_update_install_task(
				*update_release, *current_update_runtime_context(), update_transport
			);
			if(started) {
				update_install_task.emplace(std::move(*started));
			} else {
				std::cerr << "status=warning\nstage=update-check-start\ncode="
					<< static_cast<unsigned>(started.error().code) << '\n';
				if(auto reported = (*runner)->report_new_classic_update_check(
					th03::app::NewClassicUpdateCheckOutcome::failed
				); !reported) {
					return {.exit_code = fail(
						"update-check-report", reported.error()
					)};
				}
			}
		}
	}
#endif
	if(new_classic && stock_rules) {
		if(online_origin) {
			if(platform.oracle_enabled() || parsed.capture_replay) {
				std::cerr << "status=warning\nstage=online-refresh-start\n"
					"code=disabled-during-capture\n";
#if defined(_WIN32) || defined(TH03_ENABLE_CURL_ONLINE)
			} else if(auto created =
				create_product_online_transport(*online_origin)) {
				std::error_code directory_error{};
				std::filesystem::create_directories(
					online_root, directory_error
				);
				if(directory_error) {
					std::cerr << "status=warning\nstage=online-refresh-start\n"
						"code=cache-directory\nsystem="
						<< directory_error.value() << '\n';
				} else {
					online_transport_owner.emplace(std::move(*created));
					online_transport.emplace(
						online_transport_owner->transport()
					);
					auto credential_path = new_classic_settings_path.empty() ?
						config_path.parent_path() / "PODD.AUTH" :
						new_classic_settings_path.parent_path() / "PODD.AUTH";
					std::optional<th03::app::NewClassicOnlineCredentials>
						configured_credentials{};
					bool configured_credentials_invalid = false;
					const auto configured_user = environment_value(
						online_user_environment
					);
					const auto configured_token = environment_value(
						online_token_environment
					);
					if(configured_user || configured_token) {
						const th03::app::NewClassicOnlineCredentials candidate{
							configured_user.value_or(std::string{}),
							configured_token.value_or(std::string{}),
						};
						if(th03::app::validate_new_classic_online_credentials(
							candidate
						)) {
							configured_credentials = candidate;
						} else {
							configured_credentials_invalid = true;
							std::cerr << "status=warning\n"
								"stage=online-upload-identity\n"
								"code=environment-invalid\n";
						}
					}
					online_upload_credential_context.emplace(
						OnlineUploadCredentialContext{
							.path = std::move(credential_path),
							.configured = std::move(configured_credentials),
							.configured_invalid = configured_credentials_invalid,
							.transport = *online_transport,
						}
					);
					online_credential_source =
						th03::app::NewClassicOnlineCredentialSource{
							.context = &*online_upload_credential_context,
							.acquire = +[](void *opaque) -> std::expected<
								th03::app::NewClassicOnlineCredentials,
								th03::app::NewClassicOnlineCredentialAcquireError
							> {
								auto &context = *static_cast<
									OnlineUploadCredentialContext *>(opaque);
								if(context.configured_invalid ||
									context.transport.perform == nullptr) {
									return std::unexpected(th03::app::
										NewClassicOnlineCredentialAcquireError{1U});
								}
								if(context.configured) {
									return *context.configured;
								}
								auto acquired =
									load_or_enroll_product_online_credentials(
										context.path, context.transport
									);
								if(!acquired) {
									return std::unexpected(th03::app::
									NewClassicOnlineCredentialAcquireError{
										static_cast<std::uint8_t>(
											acquired.error().code + 2U
										)
										});
								}
								return std::move(*acquired);
							},
						};
					online_existing_credential_source =
						th03::app::NewClassicOnlineCredentialSource{
							.context = &*online_upload_credential_context,
							.acquire = +[](void *opaque) -> std::expected<
								th03::app::NewClassicOnlineCredentials,
								th03::app::NewClassicOnlineCredentialAcquireError
							> {
								auto &context = *static_cast<
									OnlineUploadCredentialContext *>(opaque);
								if(context.configured_invalid) {
									return std::unexpected(th03::app::
										NewClassicOnlineCredentialAcquireError{1U});
								}
								if(context.configured) {
									return *context.configured;
								}
								auto loaded =
									load_product_online_credentials(context.path);
								if(!loaded) {
									return std::unexpected(th03::app::
									NewClassicOnlineCredentialAcquireError{
										static_cast<std::uint8_t>(
											loaded.error().code + 2U
										)
										});
								}
								if(!*loaded) {
									return std::unexpected(th03::app::
										NewClassicOnlineCredentialAcquireError{2U});
								}
								return std::move(**loaded);
							},
						};
					std::error_code credential_status_error{};
					const bool stored_identity = std::filesystem::is_regular_file(
						online_upload_credential_context->path,
						credential_status_error
					);
					online_my_records_refresh_requested =
						!configured_credentials_invalid &&
						(static_cast<bool>(
							online_upload_credential_context->configured
						) || (stored_identity && !credential_status_error));
					std::vector<th03::app::NewClassicOnlineCategory> categories{};
					categories.reserve(40U);
					for(std::uint8_t rank = 0U; rank < 4U; ++rank) {
						for(std::uint8_t character = 0U;
							character <= th03::app::
								new_classic_online_overall_character;
							++character) {
							categories.push_back({
								.mode = th03::app::NewClassicOnlineMode::story,
								.rank = rank,
								.character = character,
							});
						}
					}
					auto started =
						th03::app::start_new_classic_online_board_refresh(
							std::move(categories), online_root,
							*online_transport
						);
					if(started) {
						online_refresh_task.emplace(std::move(*started));
					} else {
						std::cerr << "status=warning\n"
							"stage=online-refresh-start\ncode="
							<< static_cast<unsigned>(started.error().code) << '\n';
					}
				}
			} else {
				std::cerr << "status=warning\nstage=online-refresh-start\ncode="
					<< static_cast<unsigned>(created.error().code) << '\n';
#else
			} else {
				std::cerr << "status=warning\nstage=online-refresh-start\n"
					"code=platform-unavailable\n";
#endif
			}
		}
	}
	auto created_session = th03::app::ClassicAudioSession::create({
		.data_root = data_root,
		.tick_rate = parsed.tick_rate,
		.bgm_active = persisted_options.bgm_mode !=
			th03::app::ClassicBgmMode::off &&
			(!new_classic || music_volume_percent != 0U),
		.midi_bgm = persisted_options.bgm_mode ==
			th03::app::ClassicBgmMode::midi,
		.effects_active = !new_classic || persisted_options.sfx_enabled,
		.effects_follow_bgm = !new_classic,
		.music_volume_percent = new_classic ? music_volume_percent :
			std::uint8_t{100},
		.effect_volume_percent = new_classic ? effect_volume_percent :
			std::uint8_t{100},
		.rhythm_rom = std::span<const std::byte>(rhythm_rom),
		.rhythm_pcm = rhythm_pcm.view(),
	});
	if(!created_session) {
		return {
			.exit_code = fail("audio-session-create", created_session.error())
		};
	}
	auto audio_session = std::move(*created_session);
	const auto drain_audio = [&]() -> std::expected<
		void,
		th03::app::ClassicAudioSessionError
	> {
		while(const auto command = (*runner)->take_audio_command()) {
			if(auto applied = audio_session->apply(*command); !applied) {
				return std::unexpected(applied.error());
			}
		}
		return {};
	};
	if(auto drained = drain_audio(); !drained) {
		return {.exit_code = fail("audio-command", drained.error())};
	}
	if(new_classic && !parsed.capture_replay) {
		if(auto discarded = th03::app::discard_replay_fallback_on_startup(
			parsed.replay_root
		); !discarded) {
			return {
				.exit_code = fail("replay-fallback-remove", discarded.error())
			};
		}
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
		return {.exit_code = fail("compose", surface.error())};
	}
	const auto bootstrap_identity = (*runner)->pixel_oracle_bootstrap_identity();
	const bool bootstrap_selected = !pixel_oracle_selector ||
		pixel_oracle_selector->matches(bootstrap_identity);
	if(bootstrap_selected) {
		auto presented = platform.present(*surface);
		if(!presented) {
			return {.exit_code = fail("present", presented.error())};
		}
		if(identity_output.enabled() &&
			(!presented->oracle_publication || !th03::app::write_t3ali1_event(
				identity_output.stream, identity_output.fixture_sha256,
				bootstrap_identity, presented->oracle_publication
			))) {
			std::cerr << "status=error\nstage=pixel-oracle-identity\n"
				"code=identity-oracle-write-failed\n";
			return {.exit_code = 1};
		}
		if(pixel_oracle_selector) {
			return {.disposition = ExperienceRunDisposition::smoke_complete};
		}
	} else if(identity_output.enabled() &&
		!th03::app::write_t3ali1_event(
			identity_output.stream, identity_output.fixture_sha256,
			bootstrap_identity, std::nullopt
		)) {
		std::cerr << "status=error\nstage=pixel-oracle-identity\n"
			"code=identity-oracle-write-failed\n";
		return {.exit_code = 1};
	}

	std::optional<th03::platform::SdlAudioDevice> audio_device{};
	if(!pixel_oracle_selector) {
		auto created_device = th03::platform::SdlAudioDevice::create({
			.start_paused = true,
		});
		if(created_device) {
			audio_device.emplace(std::move(*created_device));
		} else {
			warn_audio_device("audio-device-create", created_device.error());
		}
	}
	bool audio_started = false;
	bool audio_focus_muted = false;
	std::vector<th03::audio::PmdCanonicalStereoSample> startup_silence{};
	std::vector<th03::audio::PmdCanonicalStereoSample> gain_frames{};
	auto scheduler = th03::app::TickInputScheduler::create(
		parsed.tick_rate,
		th03::platform::sdl_ticks_now_ns(),
		th03::app::classic_interactive_catch_up_policy
	);
	if(!scheduler) {
		return {.exit_code = fail_tick(scheduler.error())};
	}
	bool retry_blocked_event = false;
	bool presentation_pending = false;
	std::optional<std::uint64_t> pending_publication_tick{};
	std::uint32_t pending_publication_ticks = 0U;
	std::optional<th03::app::PixelOracleIdentity>
		pending_publication_identity{};
	std::uint64_t logical_ticks = 0U;
	std::optional<th03::formats::ReplayDocument> completed_replay{};
	std::optional<th03::app::NewClassicNetplayMatchReadyAudioGate>
		netplay_match_ready_audio{};
	auto sync_audio_focus_mute = [&]() noexcept {
		const bool desired = new_classic && mute_when_unfocused &&
			!mapper.main_window_focused();
		if(desired && !audio_focus_muted && audio_device) {
			if(auto cleared = audio_device->clear(); !cleared) {
				warn_audio_device("audio-device-focus-clear", cleared.error());
				audio_device.reset();
			}
		}
		audio_focus_muted = desired;
	};
	const auto restore_retained_menu_song = [&]() -> std::expected<void, int> {
		if(!netplay_match_ready_audio) {
			return {};
		}
		if(audio_device) {
			if(const auto cleared = audio_device->clear(); !cleared) {
				warn_audio_device(
					"netplay-match-audio-cancel-clear", cleared.error()
				);
			}
		}
		for(const auto command : th03::app::
			new_classic_netplay_menu_audio_restore_commands) {
			if(auto applied = audio_session->apply(command); !applied) {
				return std::unexpected(fail("audio-command", applied.error()));
			}
		}
		netplay_match_ready_audio.reset();
		return {};
	};
	const auto advance_retained_audio_tick = [&]() -> std::expected<void, int> {
		if(netplay_match_ready_audio &&
			netplay_match_ready_audio->complete) {
			return {};
		}
		sync_audio_focus_mute();
		auto audio_tick = audio_session->advance_tick();
		if(!audio_tick) {
			return std::unexpected(fail("audio-tick", audio_tick.error()));
		}
		if(audio_device && !audio_focus_muted && !audio_started &&
			!audio_tick->canonical_frames.empty() &&
			audio_tick->canonical_frames.size() <=
				audio_device->maximum_queued_frames() / 3U) {
			startup_silence.assign(
				audio_tick->canonical_frames.size() * 2U, {}
			);
			const auto prefilled = audio_device->submit(startup_silence);
			if(!prefilled) {
				warn_audio_device("audio-device-prefill", prefilled.error());
				audio_device.reset();
			}
		}
		if(audio_device && !audio_focus_muted) {
			auto output = output_gain_frames(
				audio_tick->canonical_frames,
				new_classic ? master_volume_percent : std::uint8_t{100U},
				gain_frames
			);
			if(!output) {
				return std::unexpected(fail_output_gain(
					"audio-output-gain", output.error()
				));
			}
			const auto submitted = audio_device->submit(*output);
			if(!submitted) {
				warn_audio_device("audio-device-submit", submitted.error());
				audio_device.reset();
			} else if(!audio_started && !audio_tick->canonical_frames.empty()) {
				const auto resumed = audio_device->resume();
				if(!resumed) {
					warn_audio_device("audio-device-resume", resumed.error());
					audio_device.reset();
				} else {
					audio_started = true;
				}
			}
		}
		if(netplay_match_ready_audio) {
			netplay_match_ready_audio->advance(audio_tick->status);
		}
		return {};
	};
	const RetainedMenuAudio retained_menu_audio{
		.advance_tick = advance_retained_audio_tick,
		.begin_match_ready = [&]() -> std::expected<void, int> {
			if(netplay_match_ready_audio) {
				return std::unexpected(fail_netplay(
					"netplay-match-audio", "already-started"
				));
			}
			netplay_match_ready_audio.emplace();
			for(const auto command : th03::app::
				new_classic_netplay_match_ready_audio_commands) {
				if(auto applied = audio_session->apply(command); !applied) {
					static_cast<void>(restore_retained_menu_song());
					return std::unexpected(fail(
						"netplay-match-audio", applied.error()
					));
				}
			}
			return {};
		},
		.match_ready_complete = [&]() -> std::expected<bool, int> {
			if(!netplay_match_ready_audio) {
				return std::unexpected(fail_netplay(
					"netplay-match-audio", "not-started"
				));
			}
			if(!netplay_match_ready_audio->complete || !audio_device ||
				audio_focus_muted) {
				return netplay_match_ready_audio->complete;
			}
			auto queued = audio_device->queued_frames();
			if(!queued) {
				warn_audio_device(
					"netplay-match-audio-queue", queued.error()
				);
				return std::unexpected(fail_netplay(
					"netplay-match-audio", "queue-query-failed"
				));
			}
			return *queued == 0U;
		},
		.cancel_match_ready = restore_retained_menu_song,
		.suspend = [&]() noexcept {
			if(!audio_device) {
				return;
			}
			if(const auto cleared = audio_device->clear(); !cleared) {
				warn_audio_device("audio-device-clear", cleared.error());
			}
			if(const auto paused = audio_device->pause(); !paused) {
				warn_audio_device("audio-device-pause", paused.error());
			}
			audio_device.reset();
			audio_started = false;
		},
		.resume = [&]() -> std::expected<void, int> {
			if(const auto restored = restore_retained_menu_song(); !restored) {
				return restored;
			}
			if(pixel_oracle_selector || audio_device) {
				return {};
			}
			auto created_device = th03::platform::SdlAudioDevice::create({
				.start_paused = true,
			});
			if(created_device) {
				audio_device.emplace(std::move(*created_device));
			} else {
				warn_audio_device(
					"audio-device-retained-resume", created_device.error()
				);
			}
			audio_started = false;
			return {};
		},
	};
	while((*runner)->status() == th03::app::ClassicRunnerStatus::running) {
#if defined(TH03_ENABLE_UPDATE_HOST)
		if(update_install_task && update_install_task->finished()) {
			auto outcome =
				th03::app::NewClassicUpdateCheckOutcome::failed;
			if(auto completed = update_install_task->take_completed()) {
				if(*completed) {
					switch((*completed)->status) {
					case th03::app::UpdateInstallStatus::nominated:
					case th03::app::UpdateInstallStatus::already_nominated:
						outcome = th03::app::NewClassicUpdateCheckOutcome::
							restart_to_apply;
						break;
					case th03::app::UpdateInstallStatus::already_active:
						outcome = th03::app::NewClassicUpdateCheckOutcome::
							up_to_date;
						break;
					}
				} else {
					const auto &error = completed->error();
					if(error.code ==
						th03::app::UpdateInstallErrorCode::acquisition &&
						error.acquisition && error.acquisition->code ==
						th03::app::UpdateAcquisitionErrorCode::current_or_older) {
						outcome = th03::app::NewClassicUpdateCheckOutcome::
							up_to_date;
					} else {
						std::cerr << "status=warning\nstage=update-check\ncode="
							<< static_cast<unsigned>(error.code) << '\n';
					}
				}
			} else {
				std::cerr << "status=warning\nstage=update-check\ncode="
					<< (update_install_task->worker_failed() ?
						"worker-failed" : "missing-result") << '\n';
			}
			if(auto reported =
				(*runner)->report_new_classic_update_check(outcome); !reported) {
				std::cerr << "status=warning\nstage=update-check-report\ncode="
					<< static_cast<unsigned>(reported.error().code) << '\n';
			}
			update_install_task.reset();
		}
#endif
#if defined(_WIN32) || defined(TH03_ENABLE_CURL_ONLINE)
		if(online_my_records_refresh_requested &&
			!online_my_records_refresh_task && online_transport &&
			online_existing_credential_source) {
			std::vector<th03::app::NewClassicOnlineCategory> categories{};
			categories.reserve(40U);
			for(std::uint8_t rank = 0U; rank < 4U; ++rank) {
				for(std::uint8_t character = 0U; character < 9U; ++character) {
					categories.push_back({
						.mode = th03::app::NewClassicOnlineMode::story,
						.rank = rank,
						.character = character,
					});
				}
			}
			for(std::uint8_t rank = 0U; rank < 4U; ++rank) {
				categories.push_back({
					.mode = th03::app::NewClassicOnlineMode::vs_1p_2p,
					.rank = rank,
					.character = th03::app::new_classic_online_overall_character,
				});
			}
			auto started =
				th03::app::start_new_classic_online_my_records_refresh(
					std::move(categories), *online_existing_credential_source,
					*online_transport
				);
			online_my_records_refresh_requested = false;
			if(started) {
				online_my_records_refresh_task.emplace(std::move(*started));
			} else {
				std::cerr << "status=warning\n"
					"stage=online-my-records-start\ncode="
					<< static_cast<unsigned>(started.error().code) << '\n';
			}
		}
#endif
		if(online_my_records_refresh_task) {
			while(auto refreshed =
				online_my_records_refresh_task->take_completed()) {
				if(refreshed->error) {
					std::cerr << "status=warning\nstage=online-my-records\nrank="
						<< static_cast<unsigned>(refreshed->category.rank)
						<< "\ncharacter="
						<< static_cast<unsigned>(refreshed->category.character)
						<< "\ncode="
						<< static_cast<unsigned>(refreshed->error->code) << '\n';
					continue;
				}
				if(!refreshed->document) {
					std::cerr << "status=warning\nstage=online-my-records\n"
						"code=missing-result\n";
					continue;
				}
				overlay_online_downloads(*refreshed->document);
				if(auto merged =
					(*runner)->merge_new_classic_online_my_records(
						std::move(*refreshed->document)
					); !merged) {
					std::cerr << "status=warning\n"
						"stage=online-my-records-merge\ncode="
						<< static_cast<unsigned>(merged.error().code) << '\n';
				}
			}
			if(online_my_records_refresh_task->finished()) {
				if(auto credential_error =
					online_my_records_refresh_task->take_credential_error()) {
					std::cerr << "status=warning\n"
						"stage=online-my-records-identity\ncode="
						<< static_cast<unsigned>(credential_error->detail_code)
						<< '\n';
				}
				if(online_my_records_refresh_task->worker_failed()) {
					std::cerr << "status=warning\nstage=online-my-records\n"
						"code=worker-failed\n";
				}
				online_my_records_refresh_task.reset();
			}
		}
		if(online_refresh_task) {
			while(auto refreshed = online_refresh_task->take_completed()) {
				if(refreshed->error) {
					std::cerr << "status=warning\nstage=online-refresh\nrank="
						<< static_cast<unsigned>(refreshed->category.rank)
						<< "\ncharacter="
						<< static_cast<unsigned>(refreshed->category.character)
						<< "\ncode="
						<< static_cast<unsigned>(refreshed->error->code) << '\n';
					continue;
				}
				if(!refreshed->document) {
					std::cerr << "status=warning\nstage=online-refresh\n"
						"code=missing-result\n";
					continue;
				}
				overlay_online_downloads(*refreshed->document);
				if(auto merged = (*runner)->merge_new_classic_online_board(
					std::move(*refreshed->document)
				); !merged) {
					std::cerr << "status=warning\nstage=online-refresh-merge\n"
						"code=" << static_cast<unsigned>(merged.error().code)
						<< '\n';
				}
			}
			if(online_refresh_task->finished()) {
				if(online_refresh_task->worker_failed()) {
					std::cerr << "status=warning\nstage=online-refresh\n"
						"code=worker-failed\n";
				}
				online_refresh_task.reset();
			}
		}
		if(online_replay_download_task) {
			bool completion_reported = false;
			if(auto downloaded =
				online_replay_download_task->take_completed()) {
				const auto outcome = downloaded->path && !downloaded->error ?
					th03::app::NewClassicOnlineReplayDownloadOutcome::downloaded :
					th03::app::NewClassicOnlineReplayDownloadOutcome::failed;
				if(auto reported =
					(*runner)->report_new_classic_online_replay_download(
						downloaded->replay_id, outcome
					); !reported) {
					return {.exit_code = fail(
						"online-replay-download-report", reported.error()
					)};
				}
				if(downloaded->error) {
					std::cerr << "status=warning\n"
						"stage=online-replay-download\ncode="
						<< static_cast<unsigned>(downloaded->error->code)
						<< '\n';
				}
				completion_reported = true;
				online_replay_download_id.reset();
			}
			if(online_replay_download_task->finished()) {
				if(!completion_reported && online_replay_download_id) {
					if(auto reported =
						(*runner)->report_new_classic_online_replay_download(
							*online_replay_download_id,
							th03::app::NewClassicOnlineReplayDownloadOutcome::failed
						); !reported) {
						return {.exit_code = fail(
							"online-replay-download-report", reported.error()
						)};
					}
					std::cerr << "status=warning\n"
						"stage=online-replay-download\ncode=worker-failed\n";
					online_replay_download_id.reset();
				}
				online_replay_download_task.reset();
			}
		}
		if(online_replay_upload_task) {
			bool completion_reported = false;
			if(auto uploaded = online_replay_upload_task->take_completed()) {
				auto outcome =
					th03::app::NewClassicReplayUploadOutcome::failed;
				if(uploaded->upload) {
					if(uploaded->upload->status ==
						th03::app::NewClassicOnlineUploadStatus::accepted) {
						outcome = th03::app::NewClassicReplayUploadOutcome::accepted;
					} else if(uploaded->upload->reason == "duplicate") {
						outcome = th03::app::NewClassicReplayUploadOutcome::duplicate;
					} else if(uploaded->upload->reason == "slowdown") {
						outcome = th03::app::NewClassicReplayUploadOutcome::slowdown;
					} else if(uploaded->upload->reason == "split_mismatch") {
						outcome = th03::app::
							NewClassicReplayUploadOutcome::verification_failed;
					} else {
						outcome = th03::app::NewClassicReplayUploadOutcome::ineligible;
					}
				} else if(uploaded->error && uploaded->error->code ==
					th03::app::NewClassicOnlineClientErrorCode::invalid_document) {
					outcome = th03::app::NewClassicReplayUploadOutcome::ineligible;
				}
				if(auto reported =
					(*runner)->report_new_classic_online_replay_upload(
						uploaded->slot, outcome
					); !reported) {
					return {.exit_code = fail(
						"online-replay-upload-report", reported.error()
					)};
				}
				if(outcome ==
					th03::app::NewClassicReplayUploadOutcome::accepted) {
					online_my_records_refresh_requested = true;
				}
				if(uploaded->error) {
					std::cerr << "status=warning\n"
						"stage=online-replay-upload\ncode="
						<< static_cast<unsigned>(uploaded->error->code)
						<< "\nhttp=" << uploaded->error->http_status;
					if(uploaded->error->transport) {
						std::cerr << "\ntransport=" << static_cast<unsigned>(
							uploaded->error->transport->code
						);
					}
					std::cerr << '\n';
				}
				if(uploaded->credential_error) {
					std::cerr << "status=warning\n"
						"stage=online-replay-upload-identity\ncode="
						<< static_cast<unsigned>(
							uploaded->credential_error->detail_code
						) << '\n';
				}
				completion_reported = true;
				online_replay_upload_slot.reset();
			}
			if(online_replay_upload_task->finished()) {
				if(!completion_reported && online_replay_upload_slot) {
					if(auto reported =
						(*runner)->report_new_classic_online_replay_upload(
							*online_replay_upload_slot,
							th03::app::NewClassicReplayUploadOutcome::failed
						); !reported) {
						return {.exit_code = fail(
							"online-replay-upload-report", reported.error()
						)};
					}
					std::cerr << "status=warning\n"
						"stage=online-replay-upload\ncode=worker-failed\n";
					online_replay_upload_slot.reset();
				}
				online_replay_upload_task.reset();
			}
		}
		// Sparse replay evidence advances one exact source boundary per host loop
		// without waiting for wall time. Replay samples still own all deterministic
		// input; no audio is submitted and discarded surfaces are never composed.
		const auto cutoff = pixel_oracle_selector ?
			scheduler->next_deadline_ns().value_or(
				th03::platform::sdl_ticks_now_ns()
			) : th03::platform::sdl_ticks_now_ns();
		if(!retry_blocked_event) {
			auto result = mapper.pump_events(*scheduler, false);
			if(!result) {
				return {.exit_code = fail("input-pump", result.error())};
			}
			retry_blocked_event = result->queue_blocked;
		}
		sync_audio_focus_mute();

		bool smoke_complete = false;
		for(;;) {
			auto due = scheduler->pop_due(cutoff);
			if(!due) {
				return {.exit_code = fail_tick(due.error())};
			}
			if(!*due) {
				break;
			}
			(*due)->restart_requested =
				parsed.mode == th03::app::ClassicLaunchMode::new_classic &&
				mapper.keyboard_scancode_held(SDL_SCANCODE_R);
			(*due)->fast_forward_requested =
				parsed.mode == th03::app::ClassicLaunchMode::new_classic &&
				mapper.keyboard_scancode_held(SDL_SCANCODE_Z);
			if(new_classic) {
				const auto scancode = mapper.take_keyboard_scancode_press();
				(*due)->scancode_pressed = scancode ?
					std::optional<std::uint16_t>{static_cast<std::uint16_t>(
						*scancode
					)} : std::nullopt;
				(*due)->any_scancode_held =
					mapper.any_keyboard_scancode_held();
			}
			auto consumed = (*runner)->consume(**due, audio_session->status());
			if(!consumed) {
				std::cerr << "logical-tick=" << logical_ticks << '\n';
				return {
					.exit_code = fail("th03-runner", consumed.error())
				};
			}
			if(auto folder =
				(*runner)->take_new_classic_open_folder_request()) {
				std::error_code folder_error{};
				std::filesystem::create_directories(*folder, folder_error);
				if(folder_error) {
					std::cerr << "status=warning\n"
						"stage=open-folder-create\ncode="
						<< folder_error.value() << '\n';
				} else if(auto opened = platform.open_folder(*folder); !opened) {
					std::cerr << "status=warning\n"
						"stage=open-folder\ncode="
						<< static_cast<unsigned>(opened.error().code) << '\n';
				}
			}
#if defined(TH03_ENABLE_UPDATE_HOST)
			if((*runner)->take_new_classic_update_check_request()) {
				if(!update_available || update_install_task || !update_release ||
					!current_update_runtime_context()) {
					if(auto reported =
						(*runner)->report_new_classic_update_check(
							th03::app::NewClassicUpdateCheckOutcome::failed
						); !reported) {
						return {.exit_code = fail(
							"update-check-report", reported.error()
						)};
					}
				} else {
					auto started = th03::app::start_update_install_task(
						*update_release, *current_update_runtime_context(),
						update_transport
					);
					if(started) {
						update_install_task.emplace(std::move(*started));
					} else {
						std::cerr << "status=warning\nstage=update-check-start\n"
							"code=" << static_cast<unsigned>(
								started.error().code
							) << '\n';
						if(auto reported =
							(*runner)->report_new_classic_update_check(
								th03::app::NewClassicUpdateCheckOutcome::failed
							); !reported) {
							return {.exit_code = fail(
								"update-check-report", reported.error()
							)};
						}
					}
				}
			}
#endif
			if((*runner)->take_new_classic_online_identity_reset_request()) {
#if defined(_WIN32) || defined(TH03_ENABLE_CURL_ONLINE)
				online_my_records_refresh_task.reset();
				online_my_records_refresh_requested = false;
				if(auto cleared =
					(*runner)->clear_new_classic_online_my_records(); !cleared) {
					return {.exit_code = fail(
						"online-my-records-clear", cleared.error()
					)};
				}
				const auto identity_root = new_classic_settings_path.empty() ?
					config_path.parent_path() :
					new_classic_settings_path.parent_path();
				if(auto removed = remove_product_online_credentials(
					identity_root / "PODD.AUTH"
				); !removed) {
					return {.exit_code = fail(
						"online-identity-reset", removed.error()
					)};
				}
#else
				return {.exit_code = fail_netplay(
					"online-identity-reset", "unsupported-platform"
				)};
#endif
			}
			if(auto request =
				(*runner)->take_new_classic_online_replay_download_request()) {
				auto report_immediate = [&](auto outcome) {
					return (*runner)->report_new_classic_online_replay_download(
						request->replay_id, outcome
					);
				};
#if defined(_WIN32) || defined(TH03_ENABLE_CURL_ONLINE)
				if(!online_transport) {
					if(auto reported = report_immediate(
						th03::app::NewClassicOnlineReplayDownloadOutcome::unavailable
					); !reported) {
						return {.exit_code = fail(
							"online-replay-download-report", reported.error()
						)};
					}
				} else if(online_replay_download_task ||
					online_replay_download_id) {
					if(auto reported = report_immediate(
						th03::app::NewClassicOnlineReplayDownloadOutcome::failed
					); !reported) {
						return {.exit_code = fail(
							"online-replay-download-report", reported.error()
						)};
					}
				} else {
					auto started =
						th03::app::start_new_classic_online_replay_download(
							*request, parsed.replay_root,
							*online_transport
						);
					if(!started) {
						if(auto reported = report_immediate(
							th03::app::NewClassicOnlineReplayDownloadOutcome::failed
						); !reported) {
							return {.exit_code = fail(
								"online-replay-download-report", reported.error()
							)};
						}
						std::cerr << "status=warning\n"
							"stage=online-replay-download-start\ncode="
							<< static_cast<unsigned>(started.error().code)
							<< '\n';
					} else {
						online_replay_download_id = request->replay_id;
						online_replay_download_task.emplace(std::move(*started));
					}
				}
#else
				if(auto reported = report_immediate(
					th03::app::NewClassicOnlineReplayDownloadOutcome::unavailable
				); !reported) {
					return {.exit_code = fail(
						"online-replay-download-report", reported.error()
					)};
				}
#endif
			}
			if(auto request =
				(*runner)->take_new_classic_online_replay_upload_request()) {
				auto report_immediate = [&](auto outcome) {
					return (*runner)->report_new_classic_online_replay_upload(
						request->slot, outcome
					);
				};
#if defined(_WIN32) || defined(TH03_ENABLE_CURL_ONLINE)
				if(!online_transport || !online_credential_source) {
					if(auto reported = report_immediate(
						th03::app::NewClassicReplayUploadOutcome::unavailable
					); !reported) {
						return {.exit_code = fail(
							"online-replay-upload-report", reported.error()
						)};
					}
				} else if(online_replay_upload_task ||
					online_replay_upload_slot) {
					if(auto reported = report_immediate(
						th03::app::NewClassicReplayUploadOutcome::failed
					); !reported) {
						return {.exit_code = fail(
							"online-replay-upload-report", reported.error()
						)};
					}
				} else {
					auto started =
						th03::app::start_new_classic_online_replay_upload(
							std::move(request->document), request->slot,
							*online_credential_source,
							*online_transport
						);
					if(!started) {
						if(auto reported = report_immediate(
							th03::app::NewClassicReplayUploadOutcome::failed
						); !reported) {
							return {.exit_code = fail(
								"online-replay-upload-report", reported.error()
							)};
						}
						std::cerr << "status=warning\n"
							"stage=online-replay-upload-start\ncode="
							<< static_cast<unsigned>(started.error().code) << '\n';
					} else {
						online_replay_upload_slot = request->slot;
						online_replay_upload_task.emplace(std::move(*started));
					}
				}
#else
				if(auto reported = report_immediate(
					th03::app::NewClassicReplayUploadOutcome::unavailable
				); !reported) {
					return {.exit_code = fail(
						"online-replay-upload-report", reported.error()
					)};
				}
#endif
			}
			if(auto request =
				(*runner)->take_new_classic_online_replay_watch_request()) {
				auto loaded = th03::app::load_new_classic_online_replay(
					*request, parsed.replay_root
				);
				std::optional<th03::formats::ReplayDocument> document{};
				if(loaded) {
					document.emplace(std::move(*loaded));
				} else {
					std::cerr << "status=warning\n"
						"stage=online-replay-watch-load\ncode="
						<< static_cast<unsigned>(loaded.error().code) << '\n';
				}
				if(auto reported =
					(*runner)->report_new_classic_online_replay_watch(
						request->replay_id, std::move(document)
					); !reported) {
					return {.exit_code = fail(
						"online-replay-watch-report", reported.error()
					)};
				}
			}
			if(auto update = (*runner)->take_new_classic_key_config_update()) {
				active_key_config = *update;
				if(!active_joy_config || !active_device_assignment) {
					std::cerr << "status=error\nstage=key-config-apply\n"
						"code=input-config-missing\n";
					return {.exit_code = 1};
				}
				if(auto replaced = mapper.replace_bindings(
					th03::app::detail::new_classic_bindings(
						*active_key_config, *active_joy_config,
						*active_device_assignment
					)
				); !replaced) {
					return {.exit_code = fail("key-config-apply", replaced.error())};
				}
			}
			if(auto update = (*runner)->take_new_classic_joy_config_update()) {
				active_joy_config = *update;
				if(!active_key_config || !active_device_assignment) {
					std::cerr << "status=error\nstage=joy-config-apply\n"
						"code=input-config-missing\n";
					return {.exit_code = 1};
				}
				if(auto replaced = mapper.replace_bindings(
					th03::app::detail::new_classic_bindings(
						*active_key_config, *active_joy_config,
						*active_device_assignment
					)
				); !replaced) {
					return {.exit_code = fail("joy-config-apply", replaced.error())};
				}
			}
			if(auto update =
				(*runner)->take_new_classic_device_assignment_update()) {
				active_device_assignment = *update;
				if(!active_key_config || !active_joy_config) {
					std::cerr << "status=error\n"
						"stage=device-assignment-apply\n"
						"code=input-config-missing\n";
					return {.exit_code = 1};
				}
				if(auto replaced = mapper.replace_bindings(
					th03::app::detail::new_classic_bindings(
						*active_key_config, *active_joy_config,
						*active_device_assignment
					)
				); !replaced) {
					return {.exit_code = fail(
						"device-assignment-apply", replaced.error()
					)};
				}
			}
			if(consumed->clock_rebase_requested) {
				const auto resumed_at = th03::platform::sdl_ticks_now_ns();
				if(auto rebased = scheduler->rebase_after_modal(resumed_at);
					!rebased) {
					return {.exit_code = fail_tick(rebased.error())};
				}
				if(auto reset = mapper.reset_after_modal(*scheduler, resumed_at);
					!reset) {
					return {.exit_code = fail("input-rebase", reset.error())};
				}
				retry_blocked_event = false;
			}
			if(auto update =
				(*runner)->take_new_classic_master_volume_update()) {
				master_volume_percent = *update;
			}
			if(auto update =
				(*runner)->take_new_classic_mute_when_unfocused_update()) {
				mute_when_unfocused = *update;
				sync_audio_focus_mute();
			}
			if(auto update =
				(*runner)->take_new_classic_window_presentation_update()) {
				if(auto applied = platform.apply_window_presentation(
					platform_window_presentation(*update)
				); !applied) {
					return {.exit_code = fail(
						"window-presentation", applied.error()
					)};
				}
				window_presentation = *update;
			}
			const bool clock_rebased_this_tick =
				consumed->clock_rebase_requested;
			const auto next_source_deadline = scheduler->next_deadline_ns();
			(*runner)->classify_pending_replay_frame(
				next_source_deadline &&
				th03::platform::sdl_ticks_now_ns() >= *next_source_deadline
			);
			if(auto drained = drain_audio(); !drained) {
				std::cerr << "logical-tick=" << logical_ticks << '\n';
				return {.exit_code = fail("audio-command", drained.error())};
			}
			if(const auto advanced = advance_retained_audio_tick(); !advanced) {
				std::cerr << "logical-tick=" << logical_ticks << '\n';
				return {.exit_code = advanced.error()};
			}
			if(consumed->surface_changed) {
				if(pixel_oracle_selector &&
					!pixel_oracle_selector->matches(
						consumed->pixel_oracle_identity
					)) {
					if(!th03::app::write_t3ali1_event(
						identity_output.stream, identity_output.fixture_sha256,
						consumed->pixel_oracle_identity, std::nullopt
					)) {
						std::cerr << "status=error\n"
							"stage=pixel-oracle-identity\n"
							"code=identity-oracle-write-failed\n";
						return {.exit_code = 1};
					}
				} else {
					presentation_pending = true;
					if(identity_output.enabled() && pending_publication_identity) {
						std::cerr << "status=error\n"
							"stage=pixel-oracle-identity\n"
							"code=identity-oracle-coalesced-publication\n";
						return {.exit_code = 1};
					}
					pending_publication_tick = logical_ticks;
					++pending_publication_ticks;
					pending_publication_identity =
						consumed->pixel_oracle_identity;
				}
			} else if(identity_output.enabled() &&
				!th03::app::write_t3ali1_event(
					identity_output.stream, identity_output.fixture_sha256,
					consumed->pixel_oracle_identity, std::nullopt
				)) {
				std::cerr << "status=error\nstage=pixel-oracle-identity\n"
					"code=identity-oracle-write-failed\n";
				return {.exit_code = 1};
			}
			if(auto replay = (*runner)->take_completed_replay()) {
				completed_replay = std::move(*replay);
			}
			logical_ticks++;
			if(retry_blocked_event) {
				auto retried = mapper.pump_events(*scheduler, false);
				if(!retried) {
					return {.exit_code = fail("input-pump", retried.error())};
				}
				retry_blocked_event = retried->queue_blocked;
			}
			if(parsed.smoke_ticks && logical_ticks >= *parsed.smoke_ticks) {
				smoke_complete = true;
				break;
			}
			if(completed_replay || consumed->status !=
				th03::app::ClassicRunnerStatus::running) {
				break;
			}
			if(clock_rebased_this_tick) {
				// cutoff was sampled before synchronous replay reconstruction
				// established the new scheduler baseline. Publish the selected
				// checkpoint now, then begin a fresh host iteration before asking
				// the rebased scheduler for another logical tick.
				break;
			}
			if(platform.oracle_enabled() && presentation_pending) {
				// Oracle captures are logical-tick aligned: never let wall-clock
				// catch-up coalesce two source publications into one row.
				break;
			}
		}

		const bool selected_publication_pending =
			pixel_oracle_selector && presentation_pending;
		if(pixel_oracle_selector && !selected_publication_pending &&
			(completed_replay || (*runner)->status() !=
				th03::app::ClassicRunnerStatus::running)) {
			std::cerr << "status=error\nstage=pixel-oracle-selector\n"
				"code=selector-not-reached\n";
			return {.exit_code = 1};
		}
		if(!selected_publication_pending && (*runner)->status() ==
			th03::app::ClassicRunnerStatus::quit_requested) {
			return {.disposition = ExperienceRunDisposition::application_quit};
		}
		if(!selected_publication_pending && (*runner)->status() ==
			th03::app::ClassicRunnerStatus::switch_to_classic) {
			return {.disposition = ExperienceRunDisposition::switch_to_classic};
		}
		if(!selected_publication_pending && (*runner)->status() ==
			th03::app::ClassicRunnerStatus::netplay_requested) {
			if(!netplay_frontend) {
				return {
					.disposition = ExperienceRunDisposition::netplay_requested
				};
			}
			const auto handled = (*netplay_frontend)(retained_menu_audio);
			if(handled.exit_code != 0 || handled.disposition ==
				NetplayFrontendDisposition::failed) {
				return {.exit_code = handled.exit_code == 0 ? 1 : handled.exit_code};
			}
			if(handled.disposition ==
				NetplayFrontendDisposition::application_quit) {
				return {
					.disposition = ExperienceRunDisposition::application_quit
				};
			}
			if(handled.disposition ==
				NetplayFrontendDisposition::restart_new_classic) {
				return {
					.disposition = ExperienceRunDisposition::restart_new_classic
				};
			}
			if(auto resumed = (*runner)->resume_from_netplay(); !resumed) {
				return {.exit_code = fail("netplay-resume", resumed.error())};
			}
			if(auto drained = drain_audio(); !drained) {
				return {.exit_code = fail("audio-command", drained.error())};
			}
			const auto resumed_at = th03::platform::sdl_ticks_now_ns();
			if(auto rebased = scheduler->rebase_after_modal(resumed_at); !rebased) {
				return {.exit_code = fail_tick(rebased.error())};
			}
			if(auto reset = mapper.reset_after_modal(*scheduler, resumed_at);
				!reset) {
				return {.exit_code = fail("input-rebase", reset.error())};
			}
			retry_blocked_event = false;
			presentation_pending = false;
			pending_publication_tick.reset();
			pending_publication_ticks = 0U;
			pending_publication_identity.reset();
			surface = (*runner)->compose_shown_surface(pixel_span);
			if(!surface) {
				return {.exit_code = fail("compose", surface.error())};
			}
			if(const auto presented = platform.present(*surface); !presented) {
				return {.exit_code = fail("present", presented.error())};
			}
			continue;
		}
		const bool session_complete = (*runner)->status() ==
			th03::app::ClassicRunnerStatus::session_complete;
		// Replay Patch finalizes and offers the recording before constructing or
		// showing Continue. The typed owner may already have prepared that surface,
		// so retain its pending publication across the host-owned save transaction.
		if(!selected_publication_pending && completed_replay && new_classic) {
			auto saved = run_replay_save(
				std::move(*completed_replay), parsed.replay_root,
				parsed.tick_rate, platform, mapper, patch_assets,
				(*runner)->new_classic_replay_save_art(),
				{
					.session = audio_session.get(),
					.device = &audio_device,
					.started = &audio_started,
					.master_volume_percent = &master_volume_percent,
					.mute_when_unfocused = &mute_when_unfocused,
				}
			);
			completed_replay.reset();
			if(saved.exit_code != 0) {
				return {.exit_code = saved.exit_code};
			}
			if(saved.application_quit) {
				return {
					.disposition = ExperienceRunDisposition::application_quit
				};
			}
			const auto resumed_at = th03::platform::sdl_ticks_now_ns();
			if(auto rebased = scheduler->rebase_after_modal(
				resumed_at
			); !rebased) {
				return {.exit_code = fail_tick(rebased.error())};
			}
			if(auto reset = mapper.reset_after_modal(*scheduler, resumed_at);
				!reset) {
				return {.exit_code = fail("input-rebase", reset.error())};
			}
			retry_blocked_event = false;
			if(session_complete) {
				return {.disposition = ExperienceRunDisposition::return_to_menu};
			}
			continue;
		}
		bool presented_this_iteration = false;
		if(presentation_pending &&
			(!retry_blocked_event || session_complete || smoke_complete ||
			 completed_replay)) {
			surface = (*runner)->compose_shown_surface(pixel_span);
			if(!surface) {
				return {.exit_code = fail("compose", surface.error())};
			}
			auto presented = platform.present(*surface, {
				.logical_tick = pending_publication_tick,
				.coalesced_ticks = pending_publication_ticks,
			});
			if(!presented) {
				return {.exit_code = fail("present", presented.error())};
			}
			if(identity_output.enabled() &&
				(!pending_publication_identity ||
				 !presented->oracle_publication ||
				 !th03::app::write_t3ali1_event(
					identity_output.stream, identity_output.fixture_sha256,
					*pending_publication_identity,
					presented->oracle_publication
				 ))) {
				std::cerr << "status=error\nstage=pixel-oracle-identity\n"
					"code=identity-oracle-write-failed\n";
				return {.exit_code = 1};
			}
			presentation_pending = false;
			pending_publication_tick.reset();
			pending_publication_ticks = 0U;
			pending_publication_identity.reset();
			presented_this_iteration = true;
			if(pixel_oracle_selector) {
				return {.disposition = ExperienceRunDisposition::smoke_complete};
			}
		}
		if(!presentation_pending) {
			if(auto refreshed = refresh_host_presentability(
				platform, "presentability-refresh"
			); !refreshed) {
				return {.exit_code = refreshed.error()};
			}
		}
		if(session_complete) {
			return {.disposition = ExperienceRunDisposition::return_to_menu};
		}
		if(completed_replay) {
			return {
				.disposition = ExperienceRunDisposition::replay_completed,
				.replay = std::move(completed_replay),
			};
		}
		if(smoke_complete) {
			if(pixel_oracle_selector) {
				std::cerr << "status=error\nstage=pixel-oracle-selector\n"
					"code=selector-not-reached\n";
				return {.exit_code = 1};
			}
			return {.disposition = ExperienceRunDisposition::smoke_complete};
		}
		if(presented_this_iteration) {
			continue;
		}
		const auto deadline = scheduler->next_deadline_ns();
		const auto now = th03::platform::sdl_ticks_now_ns();
		if(!pixel_oracle_selector && deadline && *deadline > now) {
			SDL_DelayPrecise(std::min(*deadline - now, maximum_idle_delay_ns));
		}
	}
	return {.disposition = ExperienceRunDisposition::return_to_menu};
}

} // namespace th03::app::launcher_support
