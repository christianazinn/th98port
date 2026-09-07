cmake_minimum_required(VERSION 3.25)

project(touhou_pc98_native VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)

option(PC98_BUILD_TH01 "Build the TH01 native foundations" ON)
option(PC98_BUILD_TH02 "Build the TH02 native foundations" ON)
option(PC98_BUILD_TH03 "Build the TH03 port and replay tools" ON)

add_subdirectory(shared)

# Each game can also be configured directly. Keep target names and namespaces
# game-specific; common libraries are introduced when their consumers exist.
if(PC98_BUILD_TH01)
	add_subdirectory(games/th01)
endif()
if(PC98_BUILD_TH02)
	add_subdirectory(games/th02)
endif()
if(PC98_BUILD_TH03)
	add_subdirectory(games/th03)
endif()
if(NOT PC98_BUILD_TH01 AND NOT PC98_BUILD_TH02 AND NOT PC98_BUILD_TH03)
	message(FATAL_ERROR "Select at least one implemented native game")
endif()
