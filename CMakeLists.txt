cmake_minimum_required(VERSION 3.25)

project(th03_port VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)

option(TH03_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(TH03_ENABLE_SDL "Build the optional SDL 3 platform adapter" OFF)

set(
	TH03_SDL3_PROVIDER "source" CACHE STRING
	"SDL 3 provider for the optional platform adapter (source or package)"
)
set_property(CACHE TH03_SDL3_PROVIDER PROPERTY STRINGS source package)
set(
	TH03_SDL3_SOURCE_DIR
	"${CMAKE_CURRENT_SOURCE_DIR}/out/deps/SDL3-3.4.10" CACHE PATH
	"Authenticated SDL 3.4.10 source directory"
)
set(
	TH03_SDL3_LICENSE_FILE "" CACHE FILEPATH
	"SDL LICENSE.txt supplied by the package provider"
)

if(TH03_ENABLE_SDL)
	set(TH03_SDL3_VERSION "3.4.10")
	set(
		TH03_SDL3_SOURCE_SHA256
		"12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785"
	)

	if(TH03_SDL3_PROVIDER STREQUAL "source")
		get_filename_component(
			_th03_sdl3_source "${TH03_SDL3_SOURCE_DIR}" ABSOLUTE
			BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
		)
		set(_th03_sdl3_marker "${_th03_sdl3_source}/.th03-source-sha256")
		set(_th03_sdl3_version_header
			"${_th03_sdl3_source}/include/SDL3/SDL_version.h"
		)
		set(_th03_sdl3_license_file "${_th03_sdl3_source}/LICENSE.txt")

		foreach(_required_file IN ITEMS
			"${_th03_sdl3_source}/CMakeLists.txt"
			"${_th03_sdl3_marker}"
			"${_th03_sdl3_version_header}"
			"${_th03_sdl3_license_file}"
		)
			if(NOT EXISTS "${_required_file}" OR IS_DIRECTORY "${_required_file}")
				message(FATAL_ERROR
					"Authenticated SDL ${TH03_SDL3_VERSION} source is incomplete: "
					"${_required_file}"
				)
			endif()
		endforeach()

		file(READ "${_th03_sdl3_marker}" _th03_sdl3_marker_value)
		string(STRIP "${_th03_sdl3_marker_value}" _th03_sdl3_marker_value)
		if(NOT _th03_sdl3_marker_value STREQUAL TH03_SDL3_SOURCE_SHA256)
			message(FATAL_ERROR
				"SDL source marker does not match the authenticated "
				"SDL ${TH03_SDL3_VERSION} release archive"
			)
		endif()

		file(READ "${_th03_sdl3_version_header}" _th03_sdl3_version_text)
		foreach(_version_part IN ITEMS
			"SDL_MAJOR_VERSION[ \t]+3"
			"SDL_MINOR_VERSION[ \t]+4"
			"SDL_MICRO_VERSION[ \t]+10"
		)
			if(NOT _th03_sdl3_version_text MATCHES
				"#[ \t]*define[ \t]+${_version_part}([^0-9]|$)"
			)
				message(FATAL_ERROR
					"SDL source version header is not exactly "
					"${TH03_SDL3_VERSION}"
				)
			endif()
		endforeach()

		file(SIZE "${_th03_sdl3_license_file}" _th03_sdl3_license_size)
		if(_th03_sdl3_license_size EQUAL 0)
			message(FATAL_ERROR "SDL LICENSE.txt is empty")
		endif()

		enable_language(C)
		set(SDL_SHARED ON CACHE BOOL "" FORCE)
		set(SDL_STATIC OFF CACHE BOOL "" FORCE)
		set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
		set(SDL_TESTS OFF CACHE BOOL "" FORCE)
		set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
		set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
		add_subdirectory(
			"${_th03_sdl3_source}"
			"${CMAKE_BINARY_DIR}/_deps/sdl3-build"
			EXCLUDE_FROM_ALL
		)
	elseif(TH03_SDL3_PROVIDER STREQUAL "package")
		find_package(
			SDL3 ${TH03_SDL3_VERSION} EXACT CONFIG REQUIRED
			COMPONENTS SDL3-shared
		)
		if(TH03_SDL3_LICENSE_FILE STREQUAL "")
			message(FATAL_ERROR
				"The SDL package provider requires TH03_SDL3_LICENSE_FILE"
			)
		endif()
		get_filename_component(
			_th03_sdl3_license_file "${TH03_SDL3_LICENSE_FILE}" ABSOLUTE
			BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
		)
		if(NOT EXISTS "${_th03_sdl3_license_file}" OR
			IS_DIRECTORY "${_th03_sdl3_license_file}"
		)
			message(FATAL_ERROR
				"TH03_SDL3_LICENSE_FILE must name the package's SDL license file"
			)
		endif()
		file(SIZE "${_th03_sdl3_license_file}" _th03_sdl3_license_size)
		if(_th03_sdl3_license_size EQUAL 0)
			message(FATAL_ERROR "The package's SDL license file is empty")
		endif()
	else()
		message(FATAL_ERROR
			"TH03_SDL3_PROVIDER must be either 'source' or 'package'"
		)
	endif()

	if(NOT TARGET SDL3::SDL3-shared)
		message(FATAL_ERROR
			"The selected SDL provider did not define SDL3::SDL3-shared"
		)
	endif()

	function(th03_stage_sdl3_runtime target)
		if(NOT TARGET ${target})
			message(FATAL_ERROR
				"th03_stage_sdl3_runtime received an unknown target: ${target}"
			)
		endif()
		set(_license_destination
			"$<TARGET_FILE_DIR:${target}>/licenses/SDL3"
		)
		add_custom_command(
			TARGET ${target} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E make_directory
				"${_license_destination}"
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${_th03_sdl3_license_file}"
				"${_license_destination}/LICENSE.txt"
			VERBATIM
		)
		if(WIN32)
			add_custom_command(
				TARGET ${target} POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E copy_if_different
					"$<TARGET_FILE:SDL3::SDL3-shared>"
					"$<TARGET_FILE_DIR:${target}>"
				VERBATIM
			)
		endif()
	endfunction()
endif()

function(th03_set_warnings target)
	if(MSVC)
		target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:preprocessor)
		if(TH03_WARNINGS_AS_ERRORS)
			target_compile_options(${target} PRIVATE /WX)
		endif()
	else()
		target_compile_options(${target} PRIVATE
			-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion
		)
		if(TH03_WARNINGS_AS_ERRORS)
			target_compile_options(${target} PRIVATE -Werror)
		endif()
	endif()
endfunction()

# Imported permissive dependencies keep their upstream diagnostics rather than
# entering the port-owned warning-as-error contract. All adapter/shim code below
# still uses th03_set_warnings().
function(th03_silence_vendor_warnings target)
	if(MSVC)
		target_compile_options(${target} PRIVATE /W0)
	else()
		target_compile_options(${target} PRIVATE -w)
	endif()
endfunction()

add_library(th03_vendor_98fmplayer STATIC
	third_party/98fmplayer/fmdriver/fmdriver_common.c
	third_party/98fmplayer/fmdriver/fmdriver_pmd.c
)
target_include_directories(
	th03_vendor_98fmplayer PRIVATE third_party/98fmplayer
)
target_compile_features(th03_vendor_98fmplayer PRIVATE c_std_11)
set_target_properties(
	th03_vendor_98fmplayer PROPERTIES C_EXTENSIONS OFF
)
th03_silence_vendor_warnings(th03_vendor_98fmplayer)

add_library(th03_vendor_ymfm STATIC
	third_party/ymfm/src/ymfm_adpcm.cpp
	third_party/ymfm/src/ymfm_opn.cpp
	third_party/ymfm/src/ymfm_pcm.cpp
	third_party/ymfm/src/ymfm_ssg.cpp
)
target_include_directories(
	th03_vendor_ymfm SYSTEM PUBLIC third_party/ymfm/src
)
target_compile_features(th03_vendor_ymfm PRIVATE cxx_std_14)
set_target_properties(th03_vendor_ymfm PROPERTIES CXX_EXTENSIONS OFF)
th03_silence_vendor_warnings(th03_vendor_ymfm)

add_library(th03_audio_pmd STATIC
	src/audio/fixed_resampler.cpp
	src/audio/pmd_opna.cpp
	src/audio/pmd_shim.c
	src/audio/th03_pmd_service.cpp
)
target_include_directories(
	th03_audio_pmd
	PUBLIC include
	PRIVATE src/audio
)
target_include_directories(
	th03_audio_pmd SYSTEM PRIVATE third_party/98fmplayer
)
target_link_libraries(
	th03_audio_pmd PRIVATE th03_vendor_98fmplayer th03_vendor_ymfm
)
target_compile_features(th03_audio_pmd PUBLIC cxx_std_23 PRIVATE c_std_11)
set_target_properties(
	th03_audio_pmd PROPERTIES CXX_EXTENSIONS OFF C_EXTENSIONS OFF
)
th03_set_warnings(th03_audio_pmd)

function(th03_stage_audio_licenses target)
	if(NOT TARGET ${target})
		message(FATAL_ERROR
			"th03_stage_audio_licenses received an unknown target: ${target}"
		)
	endif()
	foreach(_dependency IN ITEMS 98fmplayer ymfm)
		add_custom_command(
			TARGET ${target} POST_BUILD
			COMMAND "${CMAKE_COMMAND}" -E make_directory
				"$<TARGET_FILE_DIR:${target}>/licenses/${_dependency}"
			COMMAND "${CMAKE_COMMAND}" -E copy_if_different
				"${CMAKE_CURRENT_SOURCE_DIR}/third_party/${_dependency}/LICENSE"
				"$<TARGET_FILE_DIR:${target}>/licenses/${_dependency}/LICENSE"
			VERBATIM
		)
	endforeach()
endfunction()

add_library(th03_assets STATIC
	src/assets/bfnt.cpp
	src/assets/cdg.cpp
	src/assets/mrs.cpp
	src/assets/pi.cpp
	src/assets/rgb.cpp
)
target_include_directories(th03_assets PUBLIC include)
target_compile_features(th03_assets PUBLIC cxx_std_23)
set_target_properties(th03_assets PROPERTIES CXX_EXTENSIONS OFF)
th03_set_warnings(th03_assets)

# Application-owned external-data import boundary. This library performs
# scene-scoped filesystem validation/caching and has no gameplay or renderer
# dependency; decoded byte spans flow inward to th03_assets at the app layer.
add_library(th03_app_resources STATIC
	src/app/resources.cpp
)
target_include_directories(th03_app_resources PUBLIC include)
target_compile_features(th03_app_resources PUBLIC cxx_std_23)
set_target_properties(th03_app_resources PROPERTIES CXX_EXTENSIONS OFF)
th03_set_warnings(th03_app_resources)

# Exact Classic persistent-value codecs, source mutation rules, and checked
# filesystem transactions. Registration and OP presentation remain in the app
# scene layer rather than this persistence boundary.
add_library(th03_app_persistence STATIC
	src/app/classic_config.cpp
	src/app/classic_config_store.cpp
	src/app/classic_score.cpp
	src/app/classic_score_store.cpp
)
target_include_directories(th03_app_persistence PUBLIC include)
target_compile_features(th03_app_persistence PUBLIC cxx_std_23)
set_target_properties(th03_app_persistence PROPERTIES CXX_EXTENSIONS OFF)
th03_set_warnings(th03_app_persistence)

add_library(th03_present STATIC
	src/present/cels.cpp
	src/present/cutscene_raster.cpp
	src/present/cutscene_script.cpp
	src/present/graphics_text.cpp
	src/present/main_background.cpp
	src/present/main_boss.cpp
	src/present/main_bullets.cpp
	src/present/main_entities.cpp
	src/present/main_effects.cpp
	src/present/main_extra.cpp
	src/present/main_charge.cpp
	src/present/main_hud.cpp
	src/present/main_result.cpp
	src/present/main_bomb_palette.cpp
	src/present/main_bombs.cpp
	src/present/main_bombs_characters.cpp
	src/present/main_palette.cpp
	src/present/main_players.cpp
	src/present/main_round.cpp
	src/present/main_static.cpp
	src/present/main_warning_gauge.cpp
	src/present/mono.cpp
	src/present/pi.cpp
	src/present/planar.cpp
	src/present/raster.cpp
	src/present/sprite16.cpp
	src/present/sprite_staging.cpp
	src/present/staff_flakes.cpp
	src/present/source_surface.cpp
	src/present/superimpose.cpp
	src/present/text.cpp
	src/present/triangles.cpp
	src/present/tram.cpp
	src/present/vectors.cpp
)
target_include_directories(th03_present PUBLIC include)
target_link_libraries(th03_present PUBLIC th03_assets)
target_compile_features(th03_present PUBLIC cxx_std_23)
set_target_properties(th03_present PROPERTIES CXX_EXTENSIONS OFF)
th03_set_warnings(th03_present)

add_library(th03_core STATIC
	src/core/game.cpp
	src/core/frame.cpp
	src/core/frame/boss_common.inl
	src/core/frame/bullets_core.inl
	src/core/frame/bullets_transfer.inl
	src/core/frame/bullets_world.inl
	src/core/frame/characters/bombs.inl
	src/core/frame/characters/boss_dispatch.inl
	src/core/frame/characters/bosses_kotohime_kana.inl
	src/core/frame/characters/bosses_marisa_ellen.inl
	src/core/frame/characters/bosses_reimu_mima.inl
	src/core/frame/characters/bosses_rikako_chiyuri.inl
	src/core/frame/characters/bosses_yumemi.inl
	src/core/frame/characters/charge_add.inl
	src/core/frame/characters/charge_collision.inl
	src/core/frame/characters/charge_update.inl
	src/core/frame/characters/extra_dispatch.inl
	src/core/frame/characters/extra_spawn.inl
	src/core/frame/characters/extra_update.inl
	src/core/frame/characters/gauge.inl
	src/core/frame/characters/kana_extra_bullet.inl
	src/core/frame/enemies_motion.inl
	src/core/frame/extra_common.inl
	src/core/frame/player_actions.inl
	src/core/frame/player_collision.inl
	src/core/frame/player_cpu.inl
	src/core/frame/player_movement.inl
	src/core/frame/round.inl
	src/core/frame/shots.inl
	src/core/frame/world_collision.inl
	src/core/observer.cpp
	src/core/random.cpp
	src/core/round_start.cpp
	src/core/state_hash.cpp
	src/core/trig.cpp
)
target_include_directories(th03_core PUBLIC include)
target_compile_features(th03_core PUBLIC cxx_std_23)
set_target_properties(th03_core PROPERTIES CXX_EXTENSIONS OFF)
th03_set_warnings(th03_core)

# Presentation consumes only the public, read-only gameplay observation API.
# The deterministic core has no reciprocal presentation dependency.
target_link_libraries(th03_present PUBLIC th03_core)

# Platform-neutral application policy. SDL and other host APIs translate their
# events, clocks, and texture storage at this boundary; none enter core or
# presentation state.
add_library(th03_app_runtime STATIC
	src/app/experience_menu.cpp
	src/app/host_surface.cpp
	src/app/tick_input.cpp
)
target_include_directories(th03_app_runtime PUBLIC include)
target_link_libraries(th03_app_runtime PUBLIC th03_present)
target_compile_features(th03_app_runtime PUBLIC cxx_std_23)
set_target_properties(th03_app_runtime PROPERTIES CXX_EXTENSIONS OFF)
th03_set_warnings(th03_app_runtime)

if(TH03_ENABLE_SDL)
	add_library(th03_platform_sdl STATIC
		src/platform/sdl_audio_device.cpp
		src/platform/sdl_input.cpp
		src/platform/sdl_platform.cpp
	)
	target_include_directories(th03_platform_sdl PUBLIC include)
	target_link_libraries(
		th03_platform_sdl
		PUBLIC th03_app_runtime th03_audio_pmd SDL3::SDL3-shared
	)
	target_compile_features(th03_platform_sdl PUBLIC cxx_std_23)
	set_target_properties(th03_platform_sdl PROPERTIES CXX_EXTENSIONS OFF)
	th03_set_warnings(th03_platform_sdl)
endif()

# Authenticated, process-lifetime content that is compiled into the original
# MAIN executable or supplied by the PC-98 text/font boundary. Original bytes
# remain caller-owned external inputs and are never embedded by this target.
add_library(th03_app_content STATIC
	src/app/authentic_text.cpp
	src/app/main_compiled_content.cpp
)
target_include_directories(th03_app_content PUBLIC include)
target_link_libraries(th03_app_content PUBLIC th03_present)
target_compile_features(th03_app_content PUBLIC cxx_std_23)
set_target_properties(th03_app_content PROPERTIES CXX_EXTENSIONS OFF)
th03_set_warnings(th03_app_content)

add_library(th03_replay STATIC
	src/replay/run.cpp
)
target_include_directories(th03_replay PUBLIC include)
target_link_libraries(th03_replay PUBLIC th03_core)
target_compile_features(th03_replay PUBLIC cxx_std_23)
set_target_properties(th03_replay PROPERTIES CXX_EXTENSIONS OFF)
th03_set_warnings(th03_replay)

add_library(th03_formats STATIC
	src/formats/enedat.cpp
	src/formats/replay.cpp
	src/formats/rpy_checkpoint.cpp
	src/formats/rpy_input.cpp
	src/formats/t3case.cpp
	src/formats/t3split.cpp
	src/formats/v13.cpp
)
target_include_directories(th03_formats PUBLIC include)
target_link_libraries(th03_formats PUBLIC th03_core)
target_compile_features(th03_formats PUBLIC cxx_std_23)
set_target_properties(th03_formats PROPERTIES CXX_EXTENSIONS OFF)
th03_set_warnings(th03_formats)

# Stable MAIN-process/application policy. It owns both the guarded direct-VS
# process and the complete source-private Classic OP/MAINL/MAIN lifecycle;
# platform code sees only the narrow public runners and composed surface.
add_library(th03_app_main STATIC
	src/app/classic_audio_session.cpp
	src/app/classic_cli.cpp
	src/app/classic_cutscene_scene.cpp
	src/app/classic_mainl_continue.cpp
	src/app/classic_op_music_room.cpp
	src/app/classic_op_options_scene.cpp
	src/app/classic_op_scene.cpp
	src/app/classic_op_session.cpp
	src/app/classic_op_selection.cpp
	src/app/classic_registration_scene.cpp
	src/app/classic_runner.cpp
	src/app/classic_mainl_stage.cpp
	src/app/classic_mainl_story.cpp
	src/app/classic_mainl_win.cpp
	src/app/classic_staff_scene.cpp
	src/app/classic_story_session.cpp
	src/app/direct_vs_cli.cpp
	src/app/direct_vs_launch.cpp
	src/app/diet_executable.cpp
	src/app/main_direct_vs_runner.cpp
	src/app/mainl_compiled_text.cpp
	src/app/main_process_driver.cpp
	src/app/main_process.cpp
	src/app/main_process_static_content.cpp
	src/app/op_compiled_text.cpp
)
target_include_directories(th03_app_main PUBLIC include)
target_link_libraries(
	th03_app_main
	PUBLIC th03_app_resources th03_app_content th03_app_persistence
		th03_app_runtime th03_audio_pmd th03_formats
)
target_compile_features(th03_app_main PUBLIC cxx_std_23)
set_target_properties(th03_app_main PROPERTIES CXX_EXTENSIONS OFF)
th03_set_warnings(th03_app_main)

add_executable(th03-headless apps/th03-headless/main.cpp)
target_link_libraries(th03-headless PRIVATE th03_formats)
th03_set_warnings(th03-headless)

add_executable(th03-replay-check apps/th03-replay-check/main.cpp)
target_link_libraries(th03-replay-check PRIVATE th03_formats)
th03_set_warnings(th03-replay-check)

add_executable(th03rv apps/th03rv/main.cpp)
target_link_libraries(th03rv PRIVATE th03_formats th03_replay)
th03_set_warnings(th03rv)

if(TH03_ENABLE_SDL)
	add_executable(th03-app
		apps/th03-app/default_bindings.cpp
		apps/th03-app/main.cpp
	)
	target_link_libraries(th03-app PRIVATE th03_app_main th03_platform_sdl)
	target_include_directories(th03-app PRIVATE apps/th03-app)
	th03_set_warnings(th03-app)
	th03_stage_sdl3_runtime(th03-app)
	th03_stage_audio_licenses(th03-app)
endif()

if(BUILD_TESTING)
	add_executable(th03-audio-pmd-tests
		tests/test_audio_pmd_opna.cpp
	)
	target_link_libraries(th03-audio-pmd-tests PRIVATE th03_audio_pmd)
	th03_set_warnings(th03-audio-pmd-tests)
	th03_stage_audio_licenses(th03-audio-pmd-tests)
	add_test(NAME th03-audio-pmd-tests COMMAND th03-audio-pmd-tests)

	add_executable(th03-audio-pmd-service-tests
		tests/test_audio_th03_pmd_service.cpp
	)
	target_link_libraries(
		th03-audio-pmd-service-tests PRIVATE th03_audio_pmd
	)
	th03_set_warnings(th03-audio-pmd-service-tests)
	th03_stage_audio_licenses(th03-audio-pmd-service-tests)
	add_test(
		NAME th03-audio-pmd-service-tests
		COMMAND th03-audio-pmd-service-tests
	)

	add_executable(th03-audio-fixed-resampler-tests
		tests/test_audio_fixed_resampler.cpp
	)
	target_link_libraries(
		th03-audio-fixed-resampler-tests PRIVATE th03_audio_pmd
	)
	th03_set_warnings(th03-audio-fixed-resampler-tests)
	th03_stage_audio_licenses(th03-audio-fixed-resampler-tests)
	add_test(
		NAME th03-audio-fixed-resampler-tests
		COMMAND th03-audio-fixed-resampler-tests
	)

	add_executable(th03-tests
		tests/test_main.cpp
	)
	target_link_libraries(th03-tests PRIVATE th03_formats th03_replay)
	target_include_directories(th03-tests PRIVATE src/core)
	th03_set_warnings(th03-tests)
	add_test(NAME th03-tests COMMAND th03-tests)

	add_executable(th03-observer-tests
		tests/test_observer.cpp
	)
	target_link_libraries(th03-observer-tests PRIVATE th03_core)
	target_include_directories(th03-observer-tests PRIVATE src/core)
	th03_set_warnings(th03-observer-tests)
	add_test(NAME th03-observer-tests COMMAND th03-observer-tests)

	add_executable(th03-exact-trig-tests
		tests/test_exact_trig.cpp
	)
	target_link_libraries(th03-exact-trig-tests PRIVATE th03_core)
	th03_set_warnings(th03-exact-trig-tests)
	add_test(NAME th03-exact-trig-tests COMMAND th03-exact-trig-tests)

	add_executable(th03-v13-tests
		tests/test_v13.cpp
	)
	target_link_libraries(th03-v13-tests PRIVATE th03_formats)
	th03_set_warnings(th03-v13-tests)
	add_test(NAME th03-v13-tests COMMAND th03-v13-tests)

	add_executable(th03-assets-tests
		tests/test_assets.cpp
	)
	target_link_libraries(th03-assets-tests PRIVATE th03_assets)
	th03_set_warnings(th03-assets-tests)
	add_test(NAME th03-assets-tests COMMAND th03-assets-tests)

	add_executable(th03-app-resources-tests
		tests/test_app_resources.cpp
	)
	target_link_libraries(th03-app-resources-tests PRIVATE th03_app_resources)
	th03_set_warnings(th03-app-resources-tests)
	add_test(NAME th03-app-resources-tests COMMAND th03-app-resources-tests)

	add_executable(th03-app-classic-score-tests
		tests/test_app_classic_score.cpp
	)
	target_link_libraries(
		th03-app-classic-score-tests PRIVATE th03_app_persistence
	)
	th03_set_warnings(th03-app-classic-score-tests)
	add_test(
		NAME th03-app-classic-score-tests
		COMMAND th03-app-classic-score-tests
	)

	add_executable(th03-app-classic-score-store-tests
		tests/test_app_classic_score_store.cpp
	)
	target_link_libraries(
		th03-app-classic-score-store-tests PRIVATE th03_app_persistence
	)
	th03_set_warnings(th03-app-classic-score-store-tests)
	add_test(
		NAME th03-app-classic-score-store-tests
		COMMAND th03-app-classic-score-store-tests
	)

	add_executable(th03-app-classic-config-tests
		tests/test_app_classic_config.cpp
	)
	target_link_libraries(
		th03-app-classic-config-tests PRIVATE th03_app_persistence
	)
	th03_set_warnings(th03-app-classic-config-tests)
	add_test(
		NAME th03-app-classic-config-tests
		COMMAND th03-app-classic-config-tests
	)

	add_executable(th03-app-host-surface-tests
		tests/test_app_host_surface.cpp
	)
	target_link_libraries(th03-app-host-surface-tests PRIVATE th03_app_runtime)
	th03_set_warnings(th03-app-host-surface-tests)
	add_test(
		NAME th03-app-host-surface-tests
		COMMAND th03-app-host-surface-tests
	)

	add_executable(th03-app-experience-menu-tests
		tests/test_app_experience_menu.cpp
	)
	target_link_libraries(
		th03-app-experience-menu-tests PRIVATE th03_app_runtime
	)
	th03_set_warnings(th03-app-experience-menu-tests)
	add_test(
		NAME th03-app-experience-menu-tests
		COMMAND th03-app-experience-menu-tests
	)

	add_executable(th03-app-tick-input-tests
		tests/test_app_tick_input.cpp
	)
	target_link_libraries(th03-app-tick-input-tests PRIVATE th03_app_runtime)
	th03_set_warnings(th03-app-tick-input-tests)
	add_test(
		NAME th03-app-tick-input-tests
		COMMAND th03-app-tick-input-tests
	)

	add_executable(th03-app-classic-mainl-win-tests
		tests/test_app_classic_mainl_win.cpp
	)
	target_link_libraries(
		th03-app-classic-mainl-win-tests PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-classic-mainl-win-tests)
	add_test(
		NAME th03-app-classic-mainl-win-tests
		COMMAND th03-app-classic-mainl-win-tests
	)

	add_executable(th03-app-classic-mainl-continue-tests
		tests/test_app_classic_mainl_continue.cpp
	)
	target_link_libraries(
		th03-app-classic-mainl-continue-tests PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-classic-mainl-continue-tests)
	add_test(
		NAME th03-app-classic-mainl-continue-tests
		COMMAND th03-app-classic-mainl-continue-tests
	)

	add_executable(th03-app-classic-cutscene-tests
		tests/test_app_classic_cutscene.cpp
	)
	target_link_libraries(
		th03-app-classic-cutscene-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-classic-cutscene-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-classic-cutscene-tests)
	add_test(
		NAME th03-app-classic-cutscene-tests
		COMMAND th03-app-classic-cutscene-tests
	)

	add_executable(th03-app-classic-staff-scene-tests
		tests/test_app_classic_staff_scene.cpp
	)
	target_link_libraries(
		th03-app-classic-staff-scene-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-classic-staff-scene-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-classic-staff-scene-tests)
	add_test(
		NAME th03-app-classic-staff-scene-tests
		COMMAND th03-app-classic-staff-scene-tests
	)

	add_executable(th03-app-classic-registration-scene-tests
		tests/test_app_classic_registration_scene.cpp
	)
	target_link_libraries(
		th03-app-classic-registration-scene-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-classic-registration-scene-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-classic-registration-scene-tests)
	add_test(
		NAME th03-app-classic-registration-scene-tests
		COMMAND th03-app-classic-registration-scene-tests
	)

	add_executable(th03-app-classic-op-scene-tests
		tests/test_app_classic_op_scene.cpp
	)
	target_link_libraries(
		th03-app-classic-op-scene-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-classic-op-scene-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-classic-op-scene-tests)
	add_test(
		NAME th03-app-classic-op-scene-tests
		COMMAND th03-app-classic-op-scene-tests
	)

	add_executable(th03-app-classic-op-music-room-tests
		tests/test_app_classic_op_music_room.cpp
	)
	target_link_libraries(
		th03-app-classic-op-music-room-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-classic-op-music-room-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-classic-op-music-room-tests)
	add_test(
		NAME th03-app-classic-op-music-room-tests
		COMMAND th03-app-classic-op-music-room-tests
	)

	add_executable(th03-app-classic-op-options-scene-tests
		tests/test_app_classic_op_options_scene.cpp
	)
	target_link_libraries(
		th03-app-classic-op-options-scene-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-classic-op-options-scene-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-classic-op-options-scene-tests)
	add_test(
		NAME th03-app-classic-op-options-scene-tests
		COMMAND th03-app-classic-op-options-scene-tests
	)

	add_executable(th03-app-classic-op-selection-tests
		tests/test_app_classic_op_selection.cpp
	)
	target_link_libraries(
		th03-app-classic-op-selection-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-classic-op-selection-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-classic-op-selection-tests)
	add_test(
		NAME th03-app-classic-op-selection-tests
		COMMAND th03-app-classic-op-selection-tests
	)

	add_executable(th03-app-classic-op-session-tests
		tests/test_app_classic_op_session.cpp
	)
	target_link_libraries(
		th03-app-classic-op-session-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-classic-op-session-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-classic-op-session-tests)
	add_test(
		NAME th03-app-classic-op-session-tests
		COMMAND th03-app-classic-op-session-tests
	)

	add_executable(th03-app-op-compiled-text-tests
		tests/test_app_op_compiled_text.cpp
	)
	target_link_libraries(
		th03-app-op-compiled-text-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-op-compiled-text-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-op-compiled-text-tests)
	add_test(
		NAME th03-app-op-compiled-text-tests
		COMMAND th03-app-op-compiled-text-tests
	)

	if(TH03_ENABLE_SDL)
		add_test(NAME th03-app-help COMMAND th03-app --help)
		add_test(NAME th03-app-classic-help COMMAND th03-app --classic --help)
		add_test(NAME th03-app-new-classic-help COMMAND th03-app --new-classic --help)
		add_test(NAME th03-app-shell-help COMMAND th03-app --shell --help)
		add_test(
			NAME th03-app-missing-main
			COMMAND ${CMAKE_COMMAND}
				-DTH03_APP=$<TARGET_FILE:th03-app>
				-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_app_missing_main.cmake
		)
		add_test(
			NAME th03-app-classic-missing-main
			COMMAND ${CMAKE_COMMAND}
				-DTH03_APP=$<TARGET_FILE:th03-app>
				-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_app_classic_missing_main.cmake
		)
		add_test(
			NAME th03-app-classic-rhythm-rom
			COMMAND ${CMAKE_COMMAND}
				-DTH03_APP=$<TARGET_FILE:th03-app>
				-DTH03_TEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/classic-rhythm-rom-test
				-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_app_classic_rhythm_rom.cmake
		)
		add_executable(th03-app-sdl-defaults-tests
			apps/th03-app/default_bindings.cpp
			tests/test_app_sdl_defaults.cpp
		)
		target_link_libraries(
			th03-app-sdl-defaults-tests PRIVATE th03_platform_sdl
		)
		target_include_directories(
			th03-app-sdl-defaults-tests PRIVATE apps/th03-app
		)
		th03_set_warnings(th03-app-sdl-defaults-tests)
		th03_stage_sdl3_runtime(th03-app-sdl-defaults-tests)
		add_test(
			NAME th03-app-sdl-defaults-tests
			COMMAND th03-app-sdl-defaults-tests
		)
		add_executable(th03-platform-sdl-input-tests
			tests/test_platform_sdl_input.cpp
		)
		target_link_libraries(
			th03-platform-sdl-input-tests PRIVATE th03_platform_sdl
		)
		th03_set_warnings(th03-platform-sdl-input-tests)
		th03_stage_sdl3_runtime(th03-platform-sdl-input-tests)
		add_test(
			NAME th03-platform-sdl-input-tests
			COMMAND th03-platform-sdl-input-tests
		)

		add_executable(th03-platform-sdl-platform-tests
			tests/test_platform_sdl_platform.cpp
		)
		target_link_libraries(
			th03-platform-sdl-platform-tests PRIVATE th03_platform_sdl
		)
		target_include_directories(
			th03-platform-sdl-platform-tests PRIVATE src/platform
		)
		th03_set_warnings(th03-platform-sdl-platform-tests)
		th03_stage_sdl3_runtime(th03-platform-sdl-platform-tests)
		add_test(
			NAME th03-platform-sdl-platform-tests
			COMMAND th03-platform-sdl-platform-tests
		)

		add_executable(th03-platform-sdl-audio-tests
			tests/test_platform_sdl_audio.cpp
		)
		target_link_libraries(
			th03-platform-sdl-audio-tests PRIVATE th03_platform_sdl
		)
		target_include_directories(
			th03-platform-sdl-audio-tests PRIVATE src/platform
		)
		th03_set_warnings(th03-platform-sdl-audio-tests)
		th03_stage_sdl3_runtime(th03-platform-sdl-audio-tests)
		th03_stage_audio_licenses(th03-platform-sdl-audio-tests)
		add_test(
			NAME th03-platform-sdl-audio-tests
			COMMAND th03-platform-sdl-audio-tests
		)

		set_tests_properties(
			th03-platform-sdl-input-tests
			th03-platform-sdl-platform-tests
			PROPERTIES ENVIRONMENT
				"SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software"
		)
		set_tests_properties(
			th03-platform-sdl-audio-tests
			PROPERTIES ENVIRONMENT
				"SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software;SDL_AUDIO_DRIVER=dummy"
		)
	endif()

	add_executable(th03-app-authentic-text-tests
		tests/test_app_authentic_text.cpp
	)
	target_link_libraries(
		th03-app-authentic-text-tests
		PRIVATE th03_app_content th03_app_resources
	)
	th03_set_warnings(th03-app-authentic-text-tests)
	add_test(
		NAME th03-app-authentic-text-tests
		COMMAND th03-app-authentic-text-tests
	)

	add_executable(th03-app-main-compiled-content-tests
		tests/test_app_main_compiled_content.cpp
	)
	target_link_libraries(
		th03-app-main-compiled-content-tests
		PRIVATE th03_app_content
	)
	target_include_directories(
		th03-app-main-compiled-content-tests
		PRIVATE src/app
	)
	th03_set_warnings(th03-app-main-compiled-content-tests)
	add_test(
		NAME th03-app-main-compiled-content-tests
		COMMAND th03-app-main-compiled-content-tests
	)

	add_executable(th03-app-main-process-tests
		tests/test_app_main_process.cpp
	)
	target_link_libraries(
		th03-app-main-process-tests
		PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-main-process-tests)
	add_test(
		NAME th03-app-main-process-tests
		COMMAND th03-app-main-process-tests
	)

	add_executable(th03-app-main-direct-vs-runner-tests
		tests/test_app_main_direct_vs_runner.cpp
	)
	target_link_libraries(
		th03-app-main-direct-vs-runner-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-main-direct-vs-runner-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-main-direct-vs-runner-tests)
	add_test(
		NAME th03-app-main-direct-vs-runner-tests
		COMMAND th03-app-main-direct-vs-runner-tests
	)

	add_executable(th03-app-classic-runner-tests
		tests/test_app_classic_runner.cpp
	)
	target_link_libraries(
		th03-app-classic-runner-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-classic-runner-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-classic-runner-tests)
	add_test(
		NAME th03-app-classic-runner-tests
		COMMAND th03-app-classic-runner-tests
	)

	add_executable(th03-app-classic-audio-session-tests
		tests/test_app_classic_audio_session.cpp
	)
	target_link_libraries(
		th03-app-classic-audio-session-tests PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-classic-audio-session-tests)
	th03_stage_audio_licenses(th03-app-classic-audio-session-tests)
	add_test(
		NAME th03-app-classic-audio-session-tests
		COMMAND th03-app-classic-audio-session-tests
	)

	add_executable(th03-app-mainl-compiled-text-tests
		tests/test_app_mainl_compiled_text.cpp
	)
	target_link_libraries(
		th03-app-mainl-compiled-text-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-mainl-compiled-text-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-mainl-compiled-text-tests)
	add_test(
		NAME th03-app-mainl-compiled-text-tests
		COMMAND th03-app-mainl-compiled-text-tests
	)

	add_executable(th03-app-classic-mainl-story-tests
		tests/test_app_classic_mainl_story.cpp
	)
	target_link_libraries(
		th03-app-classic-mainl-story-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-classic-mainl-story-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-classic-mainl-story-tests)
	add_test(
		NAME th03-app-classic-mainl-story-tests
		COMMAND th03-app-classic-mainl-story-tests
	)

	add_executable(th03-app-classic-story-session-tests
		tests/test_app_classic_story_session.cpp
	)
	target_link_libraries(
		th03-app-classic-story-session-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-classic-story-session-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-classic-story-session-tests)
	add_test(
		NAME th03-app-classic-story-session-tests
		COMMAND th03-app-classic-story-session-tests
	)

	add_executable(th03-app-classic-mainl-stage-tests
		tests/test_app_classic_mainl_stage.cpp
	)
	target_link_libraries(
		th03-app-classic-mainl-stage-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-classic-mainl-stage-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-classic-mainl-stage-tests)
	add_test(
		NAME th03-app-classic-mainl-stage-tests
		COMMAND th03-app-classic-mainl-stage-tests
	)

	add_executable(th03-app-direct-vs-launch-tests
		tests/test_app_direct_vs_launch.cpp
	)
	target_link_libraries(
		th03-app-direct-vs-launch-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-direct-vs-launch-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-direct-vs-launch-tests)
	add_test(
		NAME th03-app-direct-vs-launch-tests
		COMMAND th03-app-direct-vs-launch-tests
	)

	add_executable(th03-present-tests
		tests/test_present.cpp
	)
	target_link_libraries(th03-present-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-tests)
	add_test(NAME th03-present-tests COMMAND th03-present-tests)

	add_executable(th03-present-main-entities-tests
		tests/test_present_main_entities.cpp
	)
	target_link_libraries(th03-present-main-entities-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-main-entities-tests)
	add_test(
		NAME th03-present-main-entities-tests
		COMMAND th03-present-main-entities-tests
	)

	add_executable(th03-present-main-players-bullets-tests
		tests/test_present_main_players_bullets.cpp
	)
	target_link_libraries(
		th03-present-main-players-bullets-tests
		PRIVATE th03_present
	)
	th03_set_warnings(th03-present-main-players-bullets-tests)
	add_test(
		NAME th03-present-main-players-bullets-tests
		COMMAND th03-present-main-players-bullets-tests
	)

	add_executable(th03-present-cels-tests
		tests/test_present_cels.cpp
	)
	target_link_libraries(th03-present-cels-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-cels-tests)
	add_test(NAME th03-present-cels-tests COMMAND th03-present-cels-tests)

	add_executable(th03-present-text-tests
		tests/test_present_text.cpp
	)
	target_link_libraries(th03-present-text-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-text-tests)
	add_test(NAME th03-present-text-tests COMMAND th03-present-text-tests)

	add_executable(th03-present-graphics-text-tests
		tests/test_present_graphics_text.cpp
	)
	target_link_libraries(th03-present-graphics-text-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-graphics-text-tests)
	add_test(
		NAME th03-present-graphics-text-tests
		COMMAND th03-present-graphics-text-tests
	)

	add_executable(th03-present-pi-tests
		tests/test_present_pi.cpp
	)
	target_link_libraries(th03-present-pi-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-pi-tests)
	add_test(NAME th03-present-pi-tests COMMAND th03-present-pi-tests)

	add_executable(th03-present-mono-tests
		tests/test_present_mono.cpp
	)
	target_link_libraries(th03-present-mono-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-mono-tests)
	add_test(NAME th03-present-mono-tests COMMAND th03-present-mono-tests)

	add_executable(th03-present-sprite16-tests
		tests/test_present_sprite16.cpp
	)
	target_link_libraries(th03-present-sprite16-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-sprite16-tests)
	add_test(
		NAME th03-present-sprite16-tests
		COMMAND th03-present-sprite16-tests
	)

	add_executable(th03-present-sprite-staging-tests
		tests/test_present_sprite_staging.cpp
	)
	target_link_libraries(th03-present-sprite-staging-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-sprite-staging-tests)
	add_test(
		NAME th03-present-sprite-staging-tests
		COMMAND th03-present-sprite-staging-tests
	)

	add_executable(th03-present-staff-flakes-tests
		tests/test_present_staff_flakes.cpp
	)
	target_link_libraries(th03-present-staff-flakes-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-staff-flakes-tests)
	add_test(
		NAME th03-present-staff-flakes-tests
		COMMAND th03-present-staff-flakes-tests
	)

	add_executable(th03-present-cutscene-script-tests
		tests/test_present_cutscene_script.cpp
	)
	target_link_libraries(th03-present-cutscene-script-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-cutscene-script-tests)
	add_test(
		NAME th03-present-cutscene-script-tests
		COMMAND th03-present-cutscene-script-tests
	)

	add_executable(th03-present-cutscene-raster-tests
		tests/test_present_cutscene_raster.cpp
	)
	target_link_libraries(th03-present-cutscene-raster-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-cutscene-raster-tests)
	add_test(
		NAME th03-present-cutscene-raster-tests
		COMMAND th03-present-cutscene-raster-tests
	)

	add_executable(th03-present-superimpose-tests
		tests/test_present_superimpose.cpp
	)
	target_link_libraries(th03-present-superimpose-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-superimpose-tests)
	add_test(
		NAME th03-present-superimpose-tests
		COMMAND th03-present-superimpose-tests
	)

	add_executable(th03-present-vectors-tests
		tests/test_present_vectors.cpp
	)
	target_link_libraries(th03-present-vectors-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-vectors-tests)
	add_test(
		NAME th03-present-vectors-tests
		COMMAND th03-present-vectors-tests
	)

	add_executable(th03-present-triangles-tests
		tests/test_present_triangles.cpp
	)
	target_link_libraries(th03-present-triangles-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-triangles-tests)
	add_test(
		NAME th03-present-triangles-tests
		COMMAND th03-present-triangles-tests
	)

	add_executable(th03-present-main-effects-tests
		tests/test_present_main_effects.cpp
	)
	target_link_libraries(th03-present-main-effects-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-main-effects-tests)
	add_test(
		NAME th03-present-main-effects-tests
		COMMAND th03-present-main-effects-tests
	)

	add_executable(th03-present-main-extra-charge-tests
		tests/test_present_main_extra_charge.cpp
	)
	target_link_libraries(
		th03-present-main-extra-charge-tests
		PRIVATE th03_present
	)
	target_include_directories(
		th03-present-main-extra-charge-tests
		PRIVATE src/core
	)
	th03_set_warnings(th03-present-main-extra-charge-tests)
	add_test(
		NAME th03-present-main-extra-charge-tests
		COMMAND th03-present-main-extra-charge-tests
	)

	add_executable(th03-present-main-palette-tests
		tests/test_present_main_palette.cpp
	)
	target_link_libraries(th03-present-main-palette-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-main-palette-tests)
	add_test(
		NAME th03-present-main-palette-tests
		COMMAND th03-present-main-palette-tests
	)

	add_executable(th03-present-main-bomb-palette-tests
		tests/test_present_main_bomb_palette.cpp
	)
	target_link_libraries(
		th03-present-main-bomb-palette-tests
		PRIVATE th03_present
	)
	th03_set_warnings(th03-present-main-bomb-palette-tests)
	add_test(
		NAME th03-present-main-bomb-palette-tests
		COMMAND th03-present-main-bomb-palette-tests
	)

	add_executable(th03-present-main-bombs-tests
		tests/test_present_main_bombs.cpp
	)
	target_link_libraries(th03-present-main-bombs-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-main-bombs-tests)
	add_test(
		NAME th03-present-main-bombs-tests
		COMMAND th03-present-main-bombs-tests
	)

	add_executable(th03-present-main-boss-tests
		tests/test_present_main_boss.cpp
	)
	target_link_libraries(th03-present-main-boss-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-main-boss-tests)
	add_test(
		NAME th03-present-main-boss-tests
		COMMAND th03-present-main-boss-tests
	)

	add_executable(th03-present-main-background-tests
		tests/test_present_main_background.cpp
	)
	target_link_libraries(
		th03-present-main-background-tests
		PRIVATE th03_present
	)
	th03_set_warnings(th03-present-main-background-tests)
	add_test(
		NAME th03-present-main-background-tests
		COMMAND th03-present-main-background-tests
	)

	add_executable(th03-present-main-warning-gauge-tests
		tests/test_present_main_warning_gauge.cpp
	)
	target_link_libraries(
		th03-present-main-warning-gauge-tests
		PRIVATE th03_present
	)
	th03_set_warnings(th03-present-main-warning-gauge-tests)
	add_test(
		NAME th03-present-main-warning-gauge-tests
		COMMAND th03-present-main-warning-gauge-tests
	)

	add_executable(th03-present-main-hud-result-tests
		tests/test_present_main_hud_result.cpp
	)
	target_link_libraries(
		th03-present-main-hud-result-tests
		PRIVATE th03_present
	)
	th03_set_warnings(th03-present-main-hud-result-tests)
	add_test(
		NAME th03-present-main-hud-result-tests
		COMMAND th03-present-main-hud-result-tests
	)

	add_executable(th03-present-main-round-tests
		tests/test_present_main_round.cpp
	)
	target_link_libraries(th03-present-main-round-tests PRIVATE th03_present)
	target_include_directories(th03-present-main-round-tests PRIVATE src/core)
	th03_set_warnings(th03-present-main-round-tests)
	add_test(
		NAME th03-present-main-round-tests
		COMMAND th03-present-main-round-tests
	)

	add_executable(th03-present-source-surface-tests
		tests/test_present_source_surface.cpp
	)
	target_link_libraries(
		th03-present-source-surface-tests
		PRIVATE th03_present
	)
	th03_set_warnings(th03-present-source-surface-tests)
	add_test(
		NAME th03-present-source-surface-tests
		COMMAND th03-present-source-surface-tests
	)
endif()
