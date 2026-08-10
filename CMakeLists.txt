cmake_minimum_required(VERSION 3.25)

project(th03_port VERSION 0.1.0 LANGUAGES CXX)

include(CTest)

option(TH03_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

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

add_library(th03_present STATIC
	src/present/cels.cpp
	src/present/main_bullets.cpp
	src/present/main_entities.cpp
	src/present/main_players.cpp
	src/present/main_static.cpp
	src/present/mono.cpp
	src/present/pi.cpp
	src/present/planar.cpp
	src/present/raster.cpp
	src/present/sprite16.cpp
	src/present/sprite_staging.cpp
	src/present/staff_flakes.cpp
	src/present/superimpose.cpp
	src/present/text.cpp
	src/present/tram.cpp
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

add_executable(th03-headless apps/th03-headless/main.cpp)
target_link_libraries(th03-headless PRIVATE th03_formats)
th03_set_warnings(th03-headless)

add_executable(th03-replay-check apps/th03-replay-check/main.cpp)
target_link_libraries(th03-replay-check PRIVATE th03_formats)
th03_set_warnings(th03-replay-check)

add_executable(th03rv apps/th03rv/main.cpp)
target_link_libraries(th03rv PRIVATE th03_formats th03_replay)
th03_set_warnings(th03rv)

if(BUILD_TESTING)
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

	add_executable(th03-present-superimpose-tests
		tests/test_present_superimpose.cpp
	)
	target_link_libraries(th03-present-superimpose-tests PRIVATE th03_present)
	th03_set_warnings(th03-present-superimpose-tests)
	add_test(
		NAME th03-present-superimpose-tests
		COMMAND th03-present-superimpose-tests
	)
endif()
