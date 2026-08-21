cmake_minimum_required(VERSION 3.25)

project(th03_port VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
find_package(Threads REQUIRED)

option(TH03_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(TH03_ENABLE_SDL "Build the optional SDL 3 platform adapter" OFF)
option(TH03_ENABLE_RTC "Build the optional WebRTC netplay transport" OFF)
option(
	TH03_PACKAGE_BETA
	"Install redistributable beta binaries and dependency licenses"
	OFF
)
set(TH03_UPDATE_MANIFEST_URL "" CACHE STRING
	"Compiled absolute HTTPS update-manifest URL (empty disables updates)")
set(TH03_UPDATE_PLATFORM "" CACHE STRING
	"Compiled update platform when updates are enabled")
set_property(CACHE TH03_UPDATE_PLATFORM PROPERTY STRINGS
	"" windows-standalone windows-x64 linux-x64 macos-x64 macos-arm64)
set(TH03_UPDATE_CHANNEL "" CACHE STRING
	"Compiled update channel when updates are enabled")
set_property(CACHE TH03_UPDATE_CHANNEL PROPERTY STRINGS "" stable beta)
set(TH03_UPDATE_CURRENT_SEQUENCE "" CACHE STRING
	"Monotonic sequence of the compiled release when updates are enabled")
set(TH03_UPDATE_PUBLIC_KEY_FILE "" CACHE FILEPATH
	"DER P-256 SubjectPublicKeyInfo compiled as the update trust root")

set(_th03_update_input_count 0)
foreach(_input IN ITEMS
	TH03_UPDATE_MANIFEST_URL TH03_UPDATE_PLATFORM TH03_UPDATE_CHANNEL
	TH03_UPDATE_CURRENT_SEQUENCE TH03_UPDATE_PUBLIC_KEY_FILE
)
	if(NOT "${${_input}}" STREQUAL "")
		math(EXPR _th03_update_input_count "${_th03_update_input_count} + 1")
	endif()
endforeach()

set(TH03_UPDATE_RELEASE_CONFIGURED 0)
set(TH03_UPDATE_PLATFORM_VALUE 1)
set(TH03_UPDATE_CHANNEL_VALUE 2)
set(TH03_UPDATE_KEY_ID_BYTES "")
set(TH03_UPDATE_SPKI_BYTES "")
set(TH03_UPDATE_SPKI_SIZE 0)
if(NOT _th03_update_input_count EQUAL 0)
	if(NOT _th03_update_input_count EQUAL 5)
		message(FATAL_ERROR
			"Update release configuration is all-or-nothing: URL, platform, "
			"channel, current sequence, and public key file are required")
	endif()
	if(NOT TH03_UPDATE_MANIFEST_URL MATCHES
		"^https://[A-Za-z0-9][A-Za-z0-9.-]*(:[0-9]+)?(/[A-Za-z0-9._~:/?%&=+,-]*)?$")
		message(FATAL_ERROR
			"TH03_UPDATE_MANIFEST_URL must be a credential-free absolute HTTPS URL")
	endif()
	if(NOT TH03_UPDATE_CURRENT_SEQUENCE MATCHES "^[1-9][0-9]*$")
		message(FATAL_ERROR
			"TH03_UPDATE_CURRENT_SEQUENCE must be a positive decimal integer")
	endif()
	string(LENGTH "${TH03_UPDATE_CURRENT_SEQUENCE}" _th03_sequence_length)
	if(_th03_sequence_length GREATER 20 OR
		(_th03_sequence_length EQUAL 20 AND
		 "${TH03_UPDATE_CURRENT_SEQUENCE}" STRGREATER "18446744073709551615"))
		message(FATAL_ERROR "TH03_UPDATE_CURRENT_SEQUENCE exceeds uint64")
	endif()

	if(TH03_UPDATE_PLATFORM STREQUAL "windows-standalone")
		set(TH03_UPDATE_PLATFORM_VALUE 1)
	elseif(TH03_UPDATE_PLATFORM STREQUAL "windows-x64")
		set(TH03_UPDATE_PLATFORM_VALUE 2)
	elseif(TH03_UPDATE_PLATFORM STREQUAL "linux-x64")
		set(TH03_UPDATE_PLATFORM_VALUE 3)
	elseif(TH03_UPDATE_PLATFORM STREQUAL "macos-x64")
		set(TH03_UPDATE_PLATFORM_VALUE 4)
	elseif(TH03_UPDATE_PLATFORM STREQUAL "macos-arm64")
		set(TH03_UPDATE_PLATFORM_VALUE 5)
	else()
		message(FATAL_ERROR "Unsupported TH03_UPDATE_PLATFORM")
	endif()
	if(TH03_UPDATE_CHANNEL STREQUAL "stable")
		set(TH03_UPDATE_CHANNEL_VALUE 1)
	elseif(TH03_UPDATE_CHANNEL STREQUAL "beta")
		set(TH03_UPDATE_CHANNEL_VALUE 2)
	else()
		message(FATAL_ERROR "Unsupported TH03_UPDATE_CHANNEL")
	endif()

	get_filename_component(_th03_update_public_key
		"${TH03_UPDATE_PUBLIC_KEY_FILE}" ABSOLUTE
		BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
	if(NOT EXISTS "${_th03_update_public_key}" OR
		IS_DIRECTORY "${_th03_update_public_key}")
		message(FATAL_ERROR "Update public-key file does not exist")
	endif()
	file(SIZE "${_th03_update_public_key}" TH03_UPDATE_SPKI_SIZE)
	if(TH03_UPDATE_SPKI_SIZE LESS 1 OR TH03_UPDATE_SPKI_SIZE GREATER 4096)
		message(FATAL_ERROR "Update public-key DER size is outside 1..4096 bytes")
	endif()
	file(READ "${_th03_update_public_key}" _th03_update_spki_hex HEX)
	file(SHA256 "${_th03_update_public_key}" _th03_update_key_sha256)
	string(SUBSTRING "${_th03_update_key_sha256}" 0 32 _th03_update_key_id_hex)
	string(REGEX MATCHALL ".." _th03_update_spki_pairs
		"${_th03_update_spki_hex}")
	foreach(_pair IN LISTS _th03_update_spki_pairs)
		string(APPEND TH03_UPDATE_SPKI_BYTES "std::byte{0x${_pair}U},")
	endforeach()
	string(REGEX MATCHALL ".." _th03_update_key_id_pairs
		"${_th03_update_key_id_hex}")
	foreach(_pair IN LISTS _th03_update_key_id_pairs)
		string(APPEND TH03_UPDATE_KEY_ID_BYTES "0x${_pair}U,")
	endforeach()
	set(TH03_UPDATE_RELEASE_CONFIGURED 1)
endif()

set(_th03_generated_include_root "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${_th03_generated_include_root}/th03/app")
configure_file(
	cmake/update_release_config_generated.hpp.in
	"${_th03_generated_include_root}/th03/app/update_release_config_generated.hpp"
	@ONLY
)
if(TH03_PACKAGE_BETA AND MSVC)
	# The manual tester package must not require a separately installed VC
	# redistributable. Keep ordinary developer builds on CMake's default.
	set(
		CMAKE_MSVC_RUNTIME_LIBRARY
		"MultiThreaded$<$<CONFIG:Debug>:Debug>"
		CACHE STRING "" FORCE
	)
endif()

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

set(
	TH03_RTC_PROVIDER "source" CACHE STRING
	"libdatachannel provider for the optional RTC adapter (source or package)"
)
set_property(CACHE TH03_RTC_PROVIDER PROPERTY STRINGS source package)
set(
	TH03_LIBDATACHANNEL_SOURCE_DIR
	"${CMAKE_CURRENT_SOURCE_DIR}/out/deps/libdatachannel-0.24.3" CACHE PATH
	"Authenticated libdatachannel 0.24.3 source directory"
)
set(
	TH03_MBEDTLS_SOURCE_DIR
	"${CMAKE_CURRENT_SOURCE_DIR}/out/deps/mbedtls-3.6.7" CACHE PATH
	"Authenticated Mbed TLS 3.6.7 source directory"
)
set(
	TH03_RTC_LICENSE_ROOT "" CACHE PATH
	"License tree for a package-provided RTC dependency set"
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

if(TH03_ENABLE_RTC)
	if(NOT TH03_ENABLE_SDL)
		message(FATAL_ERROR "TH03_ENABLE_RTC requires TH03_ENABLE_SDL")
	endif()

	if(TH03_RTC_PROVIDER STREQUAL "source")
		get_filename_component(
			_th03_libdatachannel_source
			"${TH03_LIBDATACHANNEL_SOURCE_DIR}" ABSOLUTE
			BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
		)
		get_filename_component(
			_th03_mbedtls_source "${TH03_MBEDTLS_SOURCE_DIR}" ABSOLUTE
			BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
		)
		set(_th03_libdatachannel_manifest
			"${_th03_libdatachannel_source}/.th03-source-manifest"
		)
		set(_th03_mbedtls_marker
			"${_th03_mbedtls_source}/.th03-source-sha256"
		)
		foreach(_required_file IN ITEMS
			"${_th03_libdatachannel_source}/CMakeLists.txt"
			"${_th03_libdatachannel_source}/LICENSE"
			"${_th03_libdatachannel_source}/include/rtc/rtc.hpp"
			"${_th03_libdatachannel_manifest}"
			"${_th03_mbedtls_source}/CMakeLists.txt"
			"${_th03_mbedtls_source}/LICENSE"
			"${_th03_mbedtls_source}/include/mbedtls/build_info.h"
			"${_th03_mbedtls_marker}"
		)
			if(NOT EXISTS "${_required_file}" OR IS_DIRECTORY "${_required_file}")
				message(FATAL_ERROR
					"Authenticated RTC source is incomplete: ${_required_file}"
				)
			endif()
		endforeach()

		set(_th03_libdatachannel_expected_manifest
"libdatachannel c6696d157b5612df2a741d9a03b192b47ab6cefb
json 55f93686c01528224f448c19128836e7df245f72
libjuice 22145ec3473c73d811417ac9a6aee0de530aaa3f
libsrtp 24b3bf8f19b6f5ab4cd2bcceb4f4064efca86fd5
plog 94899e0b926ac1b0f4750bfbd495167b4a6ae9ef
usrsctp fec583d54493f879d2ae44a743423bf8a04371ab")
		file(READ
			"${_th03_libdatachannel_manifest}"
			_th03_libdatachannel_manifest_value
		)
		string(STRIP
			"${_th03_libdatachannel_manifest_value}"
			_th03_libdatachannel_manifest_value
		)
		if(NOT _th03_libdatachannel_manifest_value STREQUAL
			_th03_libdatachannel_expected_manifest
		)
			message(FATAL_ERROR
				"libdatachannel source manifest does not match the pinned tree"
			)
		endif()

		set(_th03_mbedtls_sha256
			"a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6"
		)
		file(READ "${_th03_mbedtls_marker}" _th03_mbedtls_marker_value)
		string(STRIP
			"${_th03_mbedtls_marker_value}" _th03_mbedtls_marker_value
		)
		if(NOT _th03_mbedtls_marker_value STREQUAL _th03_mbedtls_sha256)
			message(FATAL_ERROR
				"Mbed TLS source marker does not match the authenticated archive"
			)
		endif()
		file(READ
			"${_th03_mbedtls_source}/include/mbedtls/build_info.h"
			_th03_mbedtls_version_text
		)
		foreach(_version_part IN ITEMS
			"MBEDTLS_VERSION_MAJOR[ \\t]+3"
			"MBEDTLS_VERSION_MINOR[ \\t]+6"
			"MBEDTLS_VERSION_PATCH[ \\t]+7"
		)
			if(NOT _th03_mbedtls_version_text MATCHES
				"#[ \\t]*define[ \\t]+${_version_part}([^0-9]|$)"
			)
				message(FATAL_ERROR
					"Mbed TLS source is not exactly version 3.6.7"
				)
			endif()
		endforeach()

		set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
		set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
		set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
		set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
		set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
		set(MBEDTLS_USER_CONFIG_FILE
			"${CMAKE_CURRENT_SOURCE_DIR}/cmake/rtc_mbedtls_user_config.h"
			CACHE FILEPATH "" FORCE
		)
		add_subdirectory(
			"${_th03_mbedtls_source}"
			"${CMAKE_BINARY_DIR}/_deps/mbedtls-build"
			EXCLUDE_FROM_ALL
		)
		if(NOT TARGET MbedTLS::mbedtls)
			message(FATAL_ERROR
				"Authenticated Mbed TLS source did not define MbedTLS::mbedtls"
			)
		endif()
		# libdatachannel's source build accepts the package-style aggregate
		# target name, while Mbed TLS's add_subdirectory() path exposes its
		# concrete lowercase targets. Keep the compatibility owner local.
		add_library(MbedTLS::MbedTLS INTERFACE IMPORTED GLOBAL)
		set_property(
			TARGET MbedTLS::MbedTLS PROPERTY
			INTERFACE_LINK_LIBRARIES MbedTLS::mbedtls
		)

		set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
		set(BUILD_SHARED_DEPS_LIBS OFF CACHE BOOL "" FORCE)
		set(USE_MBEDTLS ON CACHE BOOL "" FORCE)
		set(USE_GNUTLS OFF CACHE BOOL "" FORCE)
		set(USE_NICE OFF CACHE BOOL "" FORCE)
		set(PREFER_SYSTEM_LIB OFF CACHE BOOL "" FORCE)
		set(NO_WEBSOCKET OFF CACHE BOOL "" FORCE)
		set(NO_MEDIA ON CACHE BOOL "" FORCE)
		set(NO_EXAMPLES ON CACHE BOOL "" FORCE)
		set(NO_TESTS ON CACHE BOOL "" FORCE)
		set(WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
		set(RTC_UPDATE_VERSION_HEADER OFF CACHE BOOL "" FORCE)
		add_subdirectory(
			"${_th03_libdatachannel_source}"
			"${CMAKE_BINARY_DIR}/_deps/libdatachannel-build"
			EXCLUDE_FROM_ALL
		)
		set(_th03_rtc_license_files
			"libdatachannel|${_th03_libdatachannel_source}/LICENSE|LICENSE"
			"MbedTLS|${_th03_mbedtls_source}/LICENSE|LICENSE"
			"libjuice|${_th03_libdatachannel_source}/deps/libjuice/LICENSE|LICENSE"
			"usrsctp|${_th03_libdatachannel_source}/deps/usrsctp/LICENSE.md|LICENSE.md"
			"nlohmann-json|${_th03_libdatachannel_source}/deps/json/LICENSE.MIT|LICENSE.MIT"
			"plog|${_th03_libdatachannel_source}/deps/plog/LICENSE|LICENSE"
		)
	elseif(TH03_RTC_PROVIDER STREQUAL "package")
		find_package(LibDataChannel 0.24.3 EXACT CONFIG REQUIRED)
		if(TH03_RTC_LICENSE_ROOT STREQUAL "")
			message(FATAL_ERROR
				"The RTC package provider requires TH03_RTC_LICENSE_ROOT"
			)
		endif()
		get_filename_component(
			_th03_rtc_license_root "${TH03_RTC_LICENSE_ROOT}" ABSOLUTE
			BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}"
		)
		set(_th03_rtc_license_files
			"libdatachannel|${_th03_rtc_license_root}/libdatachannel/LICENSE|LICENSE"
			"MbedTLS|${_th03_rtc_license_root}/MbedTLS/LICENSE|LICENSE"
			"libjuice|${_th03_rtc_license_root}/libjuice/LICENSE|LICENSE"
			"usrsctp|${_th03_rtc_license_root}/usrsctp/LICENSE.md|LICENSE.md"
			"nlohmann-json|${_th03_rtc_license_root}/nlohmann-json/LICENSE.MIT|LICENSE.MIT"
			"plog|${_th03_rtc_license_root}/plog/LICENSE|LICENSE"
		)
	else()
		message(FATAL_ERROR
			"TH03_RTC_PROVIDER must be either 'source' or 'package'"
		)
	endif()
	foreach(_license_record IN LISTS _th03_rtc_license_files)
		string(REPLACE "|" ";" _license_fields "${_license_record}")
		list(GET _license_fields 1 _license_source)
		if(NOT EXISTS "${_license_source}" OR IS_DIRECTORY "${_license_source}")
			message(FATAL_ERROR
				"RTC dependency license is missing: ${_license_source}"
			)
		endif()
	endforeach()

	function(th03_stage_rtc_licenses target)
		if(NOT TARGET ${target})
			message(FATAL_ERROR
				"th03_stage_rtc_licenses received an unknown target: ${target}"
			)
		endif()
		foreach(_license_record IN LISTS _th03_rtc_license_files)
			string(REPLACE "|" ";" _license_fields "${_license_record}")
			list(GET _license_fields 0 _license_name)
			list(GET _license_fields 1 _license_source)
			list(GET _license_fields 2 _license_filename)
			add_custom_command(
				TARGET ${target} POST_BUILD
				COMMAND "${CMAKE_COMMAND}" -E make_directory
					"$<TARGET_FILE_DIR:${target}>/licenses/${_license_name}"
				COMMAND "${CMAKE_COMMAND}" -E copy_if_different
					"${_license_source}"
					"$<TARGET_FILE_DIR:${target}>/licenses/${_license_name}/${_license_filename}"
				VERBATIM
			)
		endforeach()
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
	src/audio/output_gain.cpp
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
	src/app/new_classic_device_store.cpp
	src/app/new_classic_joy_config_store.cpp
	src/app/new_classic_key_config_store.cpp
	src/app/new_classic_online.cpp
	src/app/new_classic_online_store.cpp
	src/app/new_classic_settings.cpp
	src/app/new_classic_settings_store.cpp
	src/app/new_classic_score.cpp
	src/app/new_classic_score_store.cpp
	src/app/update_manifest.cpp
	src/app/update_package.cpp
)
target_include_directories(th03_app_persistence PUBLIC include)
target_compile_features(th03_app_persistence PUBLIC cxx_std_23)
set_target_properties(th03_app_persistence PROPERTIES CXX_EXTENSIONS OFF)
th03_set_warnings(th03_app_persistence)

if(TH03_ENABLE_RTC)
	if(NOT TARGET MbedTLS::mbedcrypto)
		message(FATAL_ERROR
			"The RTC crypto provider did not define MbedTLS::mbedcrypto"
		)
	endif()
	add_library(th03_app_update_authentication STATIC
		src/app/update_manifest_authentication.cpp
		src/app/update_acquisition.cpp
		src/app/update_package_materialization.cpp
		src/app/update_activation.cpp
		src/app/update_activation_store.cpp
		src/app/update_bootstrap.cpp
		src/app/update_generation.cpp
		src/app/update_generation_cleanup.cpp
		src/app/update_install.cpp
		src/app/update_task.cpp
		src/app/update_release_config.cpp
		src/app/update_process_host.cpp
	)
	target_include_directories(th03_app_update_authentication
		PUBLIC include
		PRIVATE "${_th03_generated_include_root}"
	)
	target_link_libraries(
		th03_app_update_authentication
		PUBLIC th03_app_persistence
		PRIVATE MbedTLS::mbedcrypto Threads::Threads
	)
	target_compile_features(th03_app_update_authentication PUBLIC cxx_std_23)
	set_target_properties(
		th03_app_update_authentication PROPERTIES CXX_EXTENSIONS OFF
	)
	th03_set_warnings(th03_app_update_authentication)
endif()

add_executable(
	th03-app-new-classic-key-config-store-tests
	tests/test_app_new_classic_key_config_store.cpp
)
target_link_libraries(
	th03-app-new-classic-key-config-store-tests
	PRIVATE th03_app_persistence
)
target_compile_features(
	th03-app-new-classic-key-config-store-tests PRIVATE cxx_std_23
)
set_target_properties(
	th03-app-new-classic-key-config-store-tests PROPERTIES CXX_EXTENSIONS OFF
)
th03_set_warnings(th03-app-new-classic-key-config-store-tests)
add_test(
	NAME th03-app-new-classic-key-config-store-tests
	COMMAND th03-app-new-classic-key-config-store-tests
)

add_executable(
	th03-app-new-classic-joy-config-store-tests
	tests/test_app_new_classic_joy_config_store.cpp
)
target_link_libraries(
	th03-app-new-classic-joy-config-store-tests
	PRIVATE th03_app_persistence
)
target_compile_features(
	th03-app-new-classic-joy-config-store-tests PRIVATE cxx_std_23
)
set_target_properties(
	th03-app-new-classic-joy-config-store-tests PROPERTIES CXX_EXTENSIONS OFF
)
th03_set_warnings(th03-app-new-classic-joy-config-store-tests)
add_test(
	NAME th03-app-new-classic-joy-config-store-tests
	COMMAND th03-app-new-classic-joy-config-store-tests
)

add_executable(
	th03-app-new-classic-device-store-tests
	tests/test_app_new_classic_device_store.cpp
)
target_link_libraries(
	th03-app-new-classic-device-store-tests
	PRIVATE th03_app_persistence
)
target_compile_features(
	th03-app-new-classic-device-store-tests PRIVATE cxx_std_23
)
set_target_properties(
	th03-app-new-classic-device-store-tests PROPERTIES CXX_EXTENSIONS OFF
)
th03_set_warnings(th03-app-new-classic-device-store-tests)
add_test(
	NAME th03-app-new-classic-device-store-tests
	COMMAND th03-app-new-classic-device-store-tests
)

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
	src/present/main_rollback.cpp
	src/present/main_static.cpp
	src/present/main_warning_gauge.cpp
	src/present/mono.cpp
	src/present/pi.cpp
	src/present/planar.cpp
	src/present/raster.cpp
	src/present/rollback_snapshot.cpp
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
	src/core/rollback_snapshot.cpp
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
	src/app/display_presentability.cpp
	src/app/host_surface.cpp
	src/app/new_classic_pause.cpp
	src/app/startup_sequence.cpp
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
		src/platform/sdl_update_process.cpp
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
	src/app/new_classic_game_text.cpp
)
target_include_directories(th03_app_content PUBLIC include)
target_link_libraries(th03_app_content PUBLIC th03_present)
target_compile_features(th03_app_content PUBLIC cxx_std_23)
set_target_properties(th03_app_content PROPERTIES CXX_EXTENSIONS OFF)
th03_set_warnings(th03_app_content)

add_library(th03_app_story_route STATIC
	src/app/new_classic_story_route.cpp
)
target_include_directories(th03_app_story_route PUBLIC include)
target_compile_features(th03_app_story_route PUBLIC cxx_std_23)
set_target_properties(th03_app_story_route PROPERTIES CXX_EXTENSIONS OFF)
th03_set_warnings(th03_app_story_route)

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
	src/formats/v13_write.cpp
)
target_include_directories(th03_formats PUBLIC include)
target_link_libraries(th03_formats PUBLIC th03_core)
target_compile_features(th03_formats PUBLIC cxx_std_23)
set_target_properties(th03_formats PROPERTIES CXX_EXTENSIONS OFF)
th03_set_warnings(th03_formats)

# Backend-neutral New Classic rollback/network protocol and deterministic test
# scheduling. This target owns no game process, socket, platform, clock, or UI.
add_library(th03_app_netplay STATIC
	src/app/new_classic_netplay_datagram.cpp
	src/app/new_classic_netplay_input_delay.cpp
	src/app/new_classic_netplay_lobby.cpp
	src/app/new_classic_netplay_match_contract.cpp
	src/app/new_classic_netplay_pause_barrier.cpp
	src/app/new_classic_netplay_protocol.cpp
	src/app/new_classic_netplay_signaling.cpp
	src/app/new_classic_netplay_simulator.cpp
	src/app/new_classic_netplay_timeline.cpp
)
target_include_directories(th03_app_netplay PUBLIC include)
target_link_libraries(th03_app_netplay PUBLIC th03_core)
target_compile_features(th03_app_netplay PUBLIC cxx_std_23)
set_target_properties(th03_app_netplay PROPERTIES CXX_EXTENSIONS OFF)
th03_set_warnings(th03_app_netplay)

add_library(th03_platform_udp STATIC
	src/platform/udp_socket.cpp
)
target_include_directories(th03_platform_udp PUBLIC include)
target_compile_features(th03_platform_udp PUBLIC cxx_std_23)
set_target_properties(th03_platform_udp PROPERTIES CXX_EXTENSIONS OFF)
if(WIN32)
	target_link_libraries(th03_platform_udp PUBLIC ws2_32)
endif()
th03_set_warnings(th03_platform_udp)

if(TH03_ENABLE_RTC)
	add_library(th03_platform_rtc STATIC
		src/platform/rtc_room.cpp
	)
	target_include_directories(th03_platform_rtc PUBLIC include)
	target_link_libraries(
		th03_platform_rtc
		PUBLIC th03_app_netplay LibDataChannel::LibDataChannelStatic
	)
	target_compile_features(th03_platform_rtc PUBLIC cxx_std_23)
	set_target_properties(th03_platform_rtc PROPERTIES CXX_EXTENSIONS OFF)
	th03_set_warnings(th03_platform_rtc)
endif()

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
	src/app/main_rollback_coordinator.cpp
	src/app/main_process_static_content.cpp
	src/app/new_classic_assets.cpp
	src/app/new_classic_device_scene.cpp
	src/app/new_classic_frontend.cpp
	src/app/new_classic_joy_config_scene.cpp
	src/app/new_classic_key_config_scene.cpp
	src/app/new_classic_language.cpp
	src/app/new_classic_messages.cpp
	src/app/new_classic_netplay_match_intro.cpp
	src/app/new_classic_netplay_main_session.cpp
	src/app/new_classic_netplay_post_match.cpp
	src/app/new_classic_netplay_report.cpp
	src/app/new_classic_netplay_room.cpp
	src/app/new_classic_netplay_replay_capture.cpp
	src/app/new_classic_online_client.cpp
	src/app/new_classic_online_task.cpp
	src/app/new_classic_playback.cpp
	src/app/new_classic_practice.cpp
	src/app/new_classic_replay.cpp
	src/app/op_compiled_text.cpp
	src/app/pixel_oracle_identity.cpp
	src/app/replay_slot_store.cpp
)
target_include_directories(th03_app_main PUBLIC include)
target_link_libraries(
	th03_app_main
	PUBLIC th03_app_resources th03_app_content th03_app_persistence
		th03_app_runtime th03_app_story_route th03_audio_pmd th03_formats
		th03_app_netplay Threads::Threads
)
target_compile_features(th03_app_main PUBLIC cxx_std_23)
set_target_properties(th03_app_main PROPERTIES CXX_EXTENSIONS OFF)
th03_set_warnings(th03_app_main)

if(WIN32)
	if(TH03_ENABLE_RTC)
		add_library(th03_platform_windows_update STATIC
			src/platform/windows_update.cpp
		)
		target_include_directories(th03_platform_windows_update PUBLIC include)
		target_link_libraries(
			th03_platform_windows_update
			PUBLIC th03_app_update_authentication winhttp
		)
		target_compile_features(th03_platform_windows_update PUBLIC cxx_std_23)
		set_target_properties(
			th03_platform_windows_update PROPERTIES CXX_EXTENSIONS OFF
		)
		th03_set_warnings(th03_platform_windows_update)
	endif()

	add_library(th03_platform_windows_online STATIC
		src/platform/windows_online.cpp
	)
	target_include_directories(th03_platform_windows_online PUBLIC include)
	target_link_libraries(
		th03_platform_windows_online PUBLIC th03_app_main winhttp crypt32
	)
	target_compile_features(th03_platform_windows_online PUBLIC cxx_std_23)
	set_target_properties(
		th03_platform_windows_online PROPERTIES CXX_EXTENSIONS OFF
	)
	th03_set_warnings(th03_platform_windows_online)
endif()

if(NOT WIN32)
	find_package(CURL REQUIRED)
	add_library(th03_platform_curl_online STATIC
		src/platform/curl_online.cpp
	)
	target_include_directories(th03_platform_curl_online PUBLIC include)
	target_link_libraries(
		th03_platform_curl_online PUBLIC th03_app_main CURL::libcurl
	)
	target_compile_features(th03_platform_curl_online PUBLIC cxx_std_23)
	set_target_properties(
		th03_platform_curl_online PROPERTIES CXX_EXTENSIONS OFF
	)
	th03_set_warnings(th03_platform_curl_online)

	if(TH03_ENABLE_RTC)
		add_library(th03_platform_curl_update STATIC
			src/platform/curl_update.cpp
		)
		target_include_directories(th03_platform_curl_update PUBLIC include)
		target_link_libraries(
			th03_platform_curl_update
			PUBLIC th03_app_update_authentication CURL::libcurl
		)
		target_compile_features(th03_platform_curl_update PUBLIC cxx_std_23)
		set_target_properties(
			th03_platform_curl_update PROPERTIES CXX_EXTENSIONS OFF
		)
		th03_set_warnings(th03_platform_curl_update)
	endif()
endif()

add_executable(th03-headless apps/th03-headless/main.cpp)
target_link_libraries(th03-headless PRIVATE th03_formats)
th03_set_warnings(th03-headless)

add_executable(th03-replay-check apps/th03-replay-check/main.cpp)
target_link_libraries(th03-replay-check PRIVATE th03_formats)
th03_set_warnings(th03-replay-check)

add_executable(th03-cutscene-text apps/th03-cutscene-text/main.cpp)
target_link_libraries(
	th03-cutscene-text PRIVATE th03_app_resources th03_present
)
th03_set_warnings(th03-cutscene-text)

add_library(th03rv_service_protocol STATIC
	apps/th03rv/service_protocol.cpp
)
target_include_directories(th03rv_service_protocol PUBLIC apps/th03rv)
target_link_libraries(
	th03rv_service_protocol PUBLIC th03_app_story_route th03_formats th03_replay
)
target_compile_features(th03rv_service_protocol PUBLIC cxx_std_23)
set_target_properties(
	th03rv_service_protocol PROPERTIES CXX_EXTENSIONS OFF
)
th03_set_warnings(th03rv_service_protocol)

add_executable(th03rv apps/th03rv/main.cpp)
target_link_libraries(th03rv PRIVATE th03rv_service_protocol)
th03_set_warnings(th03rv)

add_library(th03_replay_video_progress STATIC
	apps/th03-replay-video/progress_protocol.cpp
)
target_include_directories(
	th03_replay_video_progress PUBLIC apps/th03-replay-video
)
target_compile_features(th03_replay_video_progress PUBLIC cxx_std_23)
set_target_properties(
	th03_replay_video_progress PROPERTIES CXX_EXTENSIONS OFF
)
th03_set_warnings(th03_replay_video_progress)

add_executable(th03-replay-video apps/th03-replay-video/main.cpp)
target_link_libraries(
	th03-replay-video PRIVATE th03_app_main th03_replay_video_progress
)
if(WIN32)
	target_link_libraries(th03-replay-video PRIVATE shell32)
endif()
th03_set_warnings(th03-replay-video)
th03_stage_audio_licenses(th03-replay-video)

if(TH03_ENABLE_SDL)
	add_executable(th03-app
		apps/th03-app/default_bindings.cpp
		apps/th03-app/main.cpp
	)
	target_link_libraries(
		th03-app PRIVATE th03_app_main th03_platform_sdl th03_platform_udp
	)
	if(TH03_ENABLE_RTC)
		target_link_libraries(
			th03-app PRIVATE
			th03_platform_rtc th03_app_update_authentication
		)
		target_compile_definitions(
			th03-app PRIVATE
			TH03_ENABLE_RTC_TRANSPORT=1 TH03_ENABLE_UPDATE_HOST=1
		)
	endif()
	if(WIN32)
		target_link_libraries(th03-app PRIVATE th03_platform_windows_online)
		if(TH03_ENABLE_RTC)
			target_link_libraries(th03-app PRIVATE th03_platform_windows_update)
		endif()
	endif()
	if(TH03_ENABLE_RTC)
		add_executable(podd-bootstrap apps/podd-bootstrap/main.cpp)
		target_link_libraries(
			podd-bootstrap PRIVATE
			th03_app_update_authentication th03_platform_sdl
		)
		th03_set_warnings(podd-bootstrap)
		th03_stage_sdl3_runtime(podd-bootstrap)
	endif()
	if(NOT WIN32)
		target_link_libraries(th03-app PRIVATE th03_platform_curl_online)
		target_compile_definitions(
			th03-app PRIVATE TH03_ENABLE_CURL_ONLINE=1
		)
		if(TH03_ENABLE_RTC)
			target_link_libraries(th03-app PRIVATE th03_platform_curl_update)
		endif()
	endif()
	target_include_directories(th03-app PRIVATE apps/th03-app)
	th03_set_warnings(th03-app)
	th03_stage_sdl3_runtime(th03-app)
	th03_stage_audio_licenses(th03-app)
	if(TH03_ENABLE_RTC)
		th03_stage_rtc_licenses(th03-app)
	endif()
endif()

if(TH03_PACKAGE_BETA)
	if(NOT TH03_ENABLE_SDL OR NOT TH03_ENABLE_RTC)
		message(FATAL_ERROR
			"TH03_PACKAGE_BETA requires the SDL and RTC product adapters"
		)
	endif()

	if(APPLE)
		set_target_properties(
			th03-app podd-bootstrap PROPERTIES INSTALL_RPATH "@loader_path"
		)
	elseif(UNIX)
		set_target_properties(
			th03-app podd-bootstrap PROPERTIES INSTALL_RPATH "\$ORIGIN"
		)
	endif()

	install(
		TARGETS
			podd-bootstrap
			th03-app
			th03-replay-video
			th03rv
			th03-replay-check
			th03-headless
		RUNTIME DESTINATION .
	)
	install(
		FILES "$<TARGET_FILE:SDL3::SDL3-shared>"
		DESTINATION .
	)
	if(UNIX AND NOT APPLE)
		install(
			FILES "$<TARGET_SONAME_FILE:SDL3::SDL3-shared>"
			DESTINATION .
		)
	endif()
	install(
		FILES "${CMAKE_CURRENT_SOURCE_DIR}/docs/BETA_PACKAGE.md"
		DESTINATION .
		RENAME README.md
	)
	install(
		FILES "${_th03_sdl3_license_file}"
		DESTINATION licenses/SDL3
		RENAME LICENSE.txt
	)
	foreach(_dependency IN ITEMS 98fmplayer ymfm)
		install(
			FILES
				"${CMAKE_CURRENT_SOURCE_DIR}/third_party/${_dependency}/LICENSE"
			DESTINATION "licenses/${_dependency}"
		)
	endforeach()
	foreach(_license_record IN LISTS _th03_rtc_license_files)
		string(REPLACE "|" ";" _license_fields "${_license_record}")
		list(GET _license_fields 0 _license_name)
		list(GET _license_fields 1 _license_source)
		list(GET _license_fields 2 _license_filename)
		install(
			FILES "${_license_source}"
			DESTINATION "licenses/${_license_name}"
			RENAME "${_license_filename}"
		)
	endforeach()
endif()

if(BUILD_TESTING)
	find_package(Python3 COMPONENTS Interpreter)
	add_test(NAME th03-replay-video-help COMMAND th03-replay-video --help)
	add_executable(th03-replay-video-progress-tests
		tests/test_replay_video_progress_protocol.cpp
	)
	target_link_libraries(
		th03-replay-video-progress-tests PRIVATE th03_replay_video_progress
	)
	th03_set_warnings(th03-replay-video-progress-tests)
	add_test(
		NAME th03-replay-video-progress-tests
		COMMAND th03-replay-video-progress-tests
	)

	add_executable(th03rv-service-protocol-tests
		tests/test_th03rv_service_protocol.cpp
	)
	target_link_libraries(
		th03rv-service-protocol-tests PRIVATE th03rv_service_protocol
	)
	th03_set_warnings(th03rv-service-protocol-tests)
	add_test(
		NAME th03rv-service-protocol-tests
		COMMAND th03rv-service-protocol-tests
	)

	add_executable(th03-audio-output-gain-tests
		tests/test_audio_output_gain.cpp
	)
	target_link_libraries(th03-audio-output-gain-tests PRIVATE th03_audio_pmd)
	th03_set_warnings(th03-audio-output-gain-tests)
	add_test(
		NAME th03-audio-output-gain-tests
		COMMAND th03-audio-output-gain-tests
	)
	if(Python3_Interpreter_FOUND)
		add_test(
			NAME th03-replay-video-gui-tests
			COMMAND ${Python3_EXECUTABLE} -m unittest discover
				-s ${CMAKE_CURRENT_SOURCE_DIR}/tools/th03-replay-video-gui/tests
				-v
		)
		set_tests_properties(
			th03-replay-video-gui-tests PROPERTIES
			ENVIRONMENT
				"PYTHONPATH=${CMAKE_CURRENT_SOURCE_DIR}/tools/th03-replay-video-gui/src"
		)
		add_test(
			NAME th03-pixel-frame-comparator-tests
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/tests/test_compare_pixel_frames.py
				${CMAKE_CURRENT_SOURCE_DIR}/tools/compare_pixel_frames.py
		)
		add_test(
			NAME th03-new-classic-language-compiler-tests
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/tools/localization/test_compile_language_pack.py
		)
		add_test(
			NAME th03-new-classic-game-text-compiler-tests
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/tools/localization/test_compile_game_text_pack.py
		)
		add_test(
			NAME th03-cutscene-text-author-tests
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/tools/localization/test_make_cutscene_text_author.py
		)
	endif()
	add_test(NAME th03-cutscene-text-help COMMAND th03-cutscene-text --help)
	find_program(TH03_PWSH_EXECUTABLE NAMES pwsh)
	if(TH03_PWSH_EXECUTABLE)
		add_test(
			NAME th03-pixel-oracle-comparator-tests
			COMMAND ${TH03_PWSH_EXECUTABLE} -NoProfile -File
				${CMAKE_CURRENT_SOURCE_DIR}/tests/test_compare_pixel_oracle.ps1
				-Comparator
				${CMAKE_CURRENT_SOURCE_DIR}/tools/compare_pixel_oracle.ps1
		)
	endif()

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

	add_executable(th03-app-new-classic-score-tests
		tests/test_app_new_classic_score.cpp
	)
	target_link_libraries(
		th03-app-new-classic-score-tests PRIVATE th03_app_persistence
	)
	th03_set_warnings(th03-app-new-classic-score-tests)
	add_test(
		NAME th03-app-new-classic-score-tests
		COMMAND th03-app-new-classic-score-tests
	)

	add_executable(th03-app-new-classic-score-store-tests
		tests/test_app_new_classic_score_store.cpp
	)
	target_link_libraries(
		th03-app-new-classic-score-store-tests PRIVATE th03_app_persistence
	)
	th03_set_warnings(th03-app-new-classic-score-store-tests)
	add_test(
		NAME th03-app-new-classic-score-store-tests
		COMMAND th03-app-new-classic-score-store-tests
	)

	add_executable(th03-app-new-classic-online-tests
		tests/test_app_new_classic_online.cpp
	)
	target_link_libraries(
		th03-app-new-classic-online-tests PRIVATE th03_app_persistence
	)
	target_compile_features(
		th03-app-new-classic-online-tests PRIVATE cxx_std_23
	)
	th03_set_warnings(th03-app-new-classic-online-tests)
	add_test(
		NAME th03-app-new-classic-online-tests
		COMMAND th03-app-new-classic-online-tests
			${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/online_service_projection_t3onl2.hex
	)

	add_executable(th03-app-new-classic-online-client-tests
		tests/test_app_new_classic_online_client.cpp
	)
	add_executable(th03-app-new-classic-online-task-tests
		tests/test_app_new_classic_online_task.cpp
	)
	target_link_libraries(
		th03-app-new-classic-online-task-tests PRIVATE th03_app_main
	)
	target_compile_features(
		th03-app-new-classic-online-task-tests PRIVATE cxx_std_23
	)
	th03_set_warnings(th03-app-new-classic-online-task-tests)
	add_test(
		NAME th03-app-new-classic-online-task-tests
		COMMAND th03-app-new-classic-online-task-tests
	)

	if(WIN32 AND TH03_ENABLE_RTC)
		add_executable(th03-platform-windows-update-tests
			tests/test_platform_windows_update.cpp
		)
		target_link_libraries(
			th03-platform-windows-update-tests PRIVATE th03_platform_windows_update
		)
		target_compile_features(
			th03-platform-windows-update-tests PRIVATE cxx_std_23
		)
		th03_set_warnings(th03-platform-windows-update-tests)
		add_test(
			NAME th03-platform-windows-update-tests
			COMMAND th03-platform-windows-update-tests
		)
	endif()

	if(NOT WIN32)
		add_executable(th03-platform-curl-online-tests
			tests/test_platform_curl_online.cpp
		)
		target_link_libraries(
			th03-platform-curl-online-tests PRIVATE th03_platform_curl_online
		)
		target_compile_features(
			th03-platform-curl-online-tests PRIVATE cxx_std_23
		)
		th03_set_warnings(th03-platform-curl-online-tests)
		add_test(
			NAME th03-platform-curl-online-tests
			COMMAND th03-platform-curl-online-tests
		)

		if(TH03_ENABLE_RTC)
			add_executable(th03-platform-curl-update-tests
				tests/test_platform_curl_update.cpp
			)
			target_link_libraries(
				th03-platform-curl-update-tests PRIVATE th03_platform_curl_update
			)
			target_compile_features(
				th03-platform-curl-update-tests PRIVATE cxx_std_23
			)
			th03_set_warnings(th03-platform-curl-update-tests)
			add_test(
				NAME th03-platform-curl-update-tests
				COMMAND th03-platform-curl-update-tests
			)
		endif()
	endif()

	if(WIN32)
		add_executable(th03-platform-windows-online-tests
			tests/test_platform_windows_online.cpp
		)
		target_link_libraries(
			th03-platform-windows-online-tests PRIVATE th03_platform_windows_online
		)
		target_compile_features(
			th03-platform-windows-online-tests PRIVATE cxx_std_23
		)
		th03_set_warnings(th03-platform-windows-online-tests)
		add_test(
			NAME th03-platform-windows-online-tests
			COMMAND th03-platform-windows-online-tests
		)
	endif()
	target_link_libraries(
		th03-app-new-classic-online-client-tests PRIVATE th03_app_main
	)
	target_compile_features(
		th03-app-new-classic-online-client-tests PRIVATE cxx_std_23
	)
	th03_set_warnings(th03-app-new-classic-online-client-tests)
	add_test(
		NAME th03-app-new-classic-online-client-tests
		COMMAND th03-app-new-classic-online-client-tests
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

	add_executable(th03-app-new-classic-settings-tests
		tests/test_app_new_classic_settings.cpp
	)
	target_link_libraries(
		th03-app-new-classic-settings-tests PRIVATE th03_app_persistence
	)
	th03_set_warnings(th03-app-new-classic-settings-tests)
	add_test(
		NAME th03-app-new-classic-settings-tests
		COMMAND th03-app-new-classic-settings-tests
	)

	add_executable(th03-app-update-manifest-tests
		tests/test_app_update_manifest.cpp
	)
	add_executable(th03-app-update-package-tests
		tests/test_app_update_package.cpp
	)
	target_link_libraries(
		th03-app-update-manifest-tests PRIVATE th03_app_persistence
	)
	target_link_libraries(
		th03-app-update-package-tests PRIVATE th03_app_persistence
	)
	th03_set_warnings(th03-app-update-manifest-tests)
	th03_set_warnings(th03-app-update-package-tests)
	add_test(
		NAME th03-app-update-manifest-tests
		COMMAND th03-app-update-manifest-tests
	)
	add_test(
		NAME th03-app-update-package-tests
		COMMAND th03-app-update-package-tests
	)
	add_test(
		NAME th03-update-release-configuration-tests
		COMMAND "${CMAKE_COMMAND}"
			-DTH03_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
			-DTH03_BINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/update-partial-config-test
			-P ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_update_release_configuration.cmake
	)
	if(TH03_ENABLE_RTC)
		add_executable(th03-app-update-manifest-authentication-tests
			tests/test_app_update_manifest_authentication.cpp
		)
		add_executable(th03-app-update-acquisition-tests
			tests/test_app_update_acquisition.cpp
		)
		add_executable(th03-app-update-package-materialization-tests
			tests/test_app_update_package_materialization.cpp
		)
		add_executable(th03-app-update-activation-tests
			tests/test_app_update_activation.cpp
		)
		add_executable(th03-app-update-bootstrap-tests
			tests/test_app_update_bootstrap.cpp
		)
		add_executable(th03-app-update-process-host-tests
			tests/test_app_update_process_host.cpp
		)
		add_executable(th03-app-update-generation-cleanup-tests
			tests/test_app_update_generation_cleanup.cpp
		)
		add_executable(th03-app-update-install-tests
			tests/test_app_update_install.cpp
		)
		add_executable(th03-app-update-release-config-tests
			tests/test_app_update_release_config.cpp
		)
		add_executable(th03-app-update-task-tests
			tests/test_app_update_task.cpp
		)
		add_executable(th03-app-update-e2e-tests
			tests/test_app_update_e2e.cpp
		)
		target_link_libraries(
			th03-app-update-manifest-authentication-tests
			PRIVATE th03_app_update_authentication
		)
		target_link_libraries(
			th03-app-update-acquisition-tests
			PRIVATE th03_app_update_authentication
		)
		target_link_libraries(
			th03-app-update-package-materialization-tests
			PRIVATE th03_app_update_authentication
		)
		target_link_libraries(
			th03-app-update-activation-tests
			PRIVATE th03_app_update_authentication
		)
		target_link_libraries(
			th03-app-update-bootstrap-tests
			PRIVATE th03_app_update_authentication
		)
		target_link_libraries(
			th03-app-update-process-host-tests
			PRIVATE th03_app_update_authentication
		)
		target_link_libraries(
			th03-app-update-generation-cleanup-tests
			PRIVATE th03_app_update_authentication
		)
		target_link_libraries(
			th03-app-update-install-tests
			PRIVATE th03_app_update_authentication
		)
		target_link_libraries(
			th03-app-update-release-config-tests
			PRIVATE th03_app_update_authentication
		)
		target_link_libraries(
			th03-app-update-task-tests
			PRIVATE th03_app_update_authentication
		)
		target_link_libraries(
			th03-app-update-e2e-tests
			PRIVATE th03_app_update_authentication
		)
		th03_set_warnings(th03-app-update-manifest-authentication-tests)
		th03_set_warnings(th03-app-update-acquisition-tests)
		th03_set_warnings(th03-app-update-package-materialization-tests)
		th03_set_warnings(th03-app-update-activation-tests)
		th03_set_warnings(th03-app-update-bootstrap-tests)
		th03_set_warnings(th03-app-update-process-host-tests)
		th03_set_warnings(th03-app-update-generation-cleanup-tests)
		th03_set_warnings(th03-app-update-install-tests)
		th03_set_warnings(th03-app-update-release-config-tests)
		th03_set_warnings(th03-app-update-task-tests)
		th03_set_warnings(th03-app-update-e2e-tests)
		add_test(
			NAME th03-app-update-activation-tests
			COMMAND th03-app-update-activation-tests
		)
		add_test(
			NAME th03-app-update-bootstrap-tests
			COMMAND th03-app-update-bootstrap-tests
		)
		add_test(
			NAME th03-app-update-process-host-tests
			COMMAND th03-app-update-process-host-tests
		)
		add_test(
			NAME th03-app-update-generation-cleanup-tests
			COMMAND th03-app-update-generation-cleanup-tests
		)
		add_test(
			NAME th03-app-update-release-config-tests
			COMMAND th03-app-update-release-config-tests
		)
	endif()
	find_program(TH03_PWSH_EXECUTABLE pwsh)
	if(TH03_PWSH_EXECUTABLE)
		set(_th03_update_authentication_test_arguments)
		if(TH03_ENABLE_RTC)
			list(APPEND _th03_update_authentication_test_arguments
				-Verifier
				$<TARGET_FILE:th03-app-update-manifest-authentication-tests>
				-AcquisitionVerifier
				$<TARGET_FILE:th03-app-update-acquisition-tests>
				-PackageAuthor
				${CMAKE_CURRENT_SOURCE_DIR}/tools/release/write_update_package.ps1
				-PackageVerifier
				$<TARGET_FILE:th03-app-update-package-materialization-tests>
				-InstallVerifier
				$<TARGET_FILE:th03-app-update-install-tests>
				-TaskVerifier
				$<TARGET_FILE:th03-app-update-task-tests>
				-EndToEndVerifier
				$<TARGET_FILE:th03-app-update-e2e-tests>
			)
		endif()
		add_test(
			NAME th03-update-manifest-authoring-tests
			COMMAND
				${TH03_PWSH_EXECUTABLE} -NoProfile
				-File ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_write_update_manifest.ps1
				-Signer ${CMAKE_CURRENT_SOURCE_DIR}/tools/release/write_update_manifest.ps1
				-Artifact $<TARGET_FILE:th03-app-update-manifest-tests>
				${_th03_update_authentication_test_arguments}
		)
		unset(_th03_update_authentication_test_arguments)
	endif()

	add_executable(th03-app-host-surface-tests
		tests/test_app_host_surface.cpp
	)
	target_link_libraries(th03-app-host-surface-tests PRIVATE th03_app_runtime)
	th03_set_warnings(th03-app-host-surface-tests)
	add_test(
		NAME th03-app-host-surface-tests
		COMMAND th03-app-host-surface-tests
	)

	add_executable(th03-app-startup-sequence-tests
		tests/test_app_startup_sequence.cpp
	)
	target_link_libraries(
		th03-app-startup-sequence-tests PRIVATE th03_app_runtime
	)
	th03_set_warnings(th03-app-startup-sequence-tests)
	add_test(
		NAME th03-app-startup-sequence-tests
		COMMAND th03-app-startup-sequence-tests
	)

	add_executable(th03-app-new-classic-pause-tests
		tests/test_app_new_classic_pause.cpp
	)
	target_link_libraries(
		th03-app-new-classic-pause-tests PRIVATE th03_app_runtime
	)
	th03_set_warnings(th03-app-new-classic-pause-tests)
	add_test(
		NAME th03-app-new-classic-pause-tests
		COMMAND th03-app-new-classic-pause-tests
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

	add_executable(th03-app-display-presentability-tests
		tests/test_app_display_presentability.cpp
	)
	target_link_libraries(
		th03-app-display-presentability-tests PRIVATE th03_app_runtime
	)
	th03_set_warnings(th03-app-display-presentability-tests)
	add_test(
		NAME th03-app-display-presentability-tests
		COMMAND th03-app-display-presentability-tests
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
		if(TH03_ENABLE_RTC)
			add_executable(th03-update-health-child
				tests/update_health_child.cpp
			)
			target_link_libraries(
				th03-update-health-child PRIVATE
				th03_app_update_authentication
			)
			th03_set_warnings(th03-update-health-child)
			add_executable(th03-platform-sdl-update-process-tests
				tests/test_platform_sdl_update_process.cpp
			)
			target_link_libraries(
				th03-platform-sdl-update-process-tests PRIVATE
				th03_app_update_authentication th03_platform_sdl
			)
			th03_set_warnings(th03-platform-sdl-update-process-tests)
			th03_stage_sdl3_runtime(th03-platform-sdl-update-process-tests)
			add_test(
				NAME th03-platform-sdl-update-process-tests
				COMMAND th03-platform-sdl-update-process-tests
					$<TARGET_FILE:th03-update-health-child>
			)
		endif()

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

	add_executable(th03-app-new-classic-assets-tests
		tests/test_app_new_classic_assets.cpp
	)

	add_executable(th03-app-new-classic-netplay-protocol-tests
		tests/test_app_new_classic_netplay_protocol.cpp
	)
	target_link_libraries(
		th03-app-new-classic-netplay-protocol-tests PRIVATE th03_app_netplay
	)
	th03_set_warnings(th03-app-new-classic-netplay-protocol-tests)
	add_test(
		NAME th03-app-new-classic-netplay-protocol-tests
		COMMAND th03-app-new-classic-netplay-protocol-tests
	)
	add_executable(th03-app-new-classic-netplay-signaling-tests
		tests/test_app_new_classic_netplay_signaling.cpp
	)
	target_link_libraries(
		th03-app-new-classic-netplay-signaling-tests PRIVATE th03_app_netplay
	)
	th03_set_warnings(th03-app-new-classic-netplay-signaling-tests)
	add_test(
		NAME th03-app-new-classic-netplay-signaling-tests
		COMMAND th03-app-new-classic-netplay-signaling-tests
	)

	add_executable(th03-app-new-classic-netplay-simulator-tests
		tests/test_app_new_classic_netplay_simulator.cpp
	)
	target_link_libraries(
		th03-app-new-classic-netplay-simulator-tests PRIVATE th03_app_netplay
	)
	th03_set_warnings(th03-app-new-classic-netplay-simulator-tests)
	add_test(
		NAME th03-app-new-classic-netplay-simulator-tests
		COMMAND th03-app-new-classic-netplay-simulator-tests
	)

	add_executable(th03-app-new-classic-netplay-input-delay-tests
		tests/test_app_new_classic_netplay_input_delay.cpp
	)
	target_link_libraries(
		th03-app-new-classic-netplay-input-delay-tests PRIVATE th03_app_netplay
	)
	th03_set_warnings(th03-app-new-classic-netplay-input-delay-tests)
	add_test(
		NAME th03-app-new-classic-netplay-input-delay-tests
		COMMAND th03-app-new-classic-netplay-input-delay-tests
	)

	add_executable(th03-netplay-process-peer
		tests/netplay_process_peer.cpp
	)
	target_link_libraries(
		th03-netplay-process-peer PRIVATE th03_app_netplay
	)
	th03_set_warnings(th03-netplay-process-peer)
	if(Python3_Interpreter_FOUND)
		add_test(
			NAME th03-netplay-two-process-tests
			COMMAND ${Python3_EXECUTABLE}
				${CMAKE_CURRENT_SOURCE_DIR}/tests/test_netplay_two_process.py
				$<TARGET_FILE:th03-netplay-process-peer>
		)
		set_tests_properties(
			th03-netplay-two-process-tests PROPERTIES TIMEOUT 30
		)
	endif()

	add_executable(th03-platform-udp-socket-tests
		tests/test_platform_udp_socket.cpp
	)
	target_link_libraries(
		th03-platform-udp-socket-tests PRIVATE th03_platform_udp
	)
	th03_set_warnings(th03-platform-udp-socket-tests)
	add_test(
		NAME th03-platform-udp-socket-tests
		COMMAND th03-platform-udp-socket-tests
	)
	if(TH03_ENABLE_RTC)
		add_executable(th03-platform-rtc-room-tests
			tests/test_platform_rtc_room.cpp
		)
		target_link_libraries(
			th03-platform-rtc-room-tests PRIVATE th03_platform_rtc
		)
		th03_set_warnings(th03-platform-rtc-room-tests)
		add_test(
			NAME th03-platform-rtc-room-tests
			COMMAND th03-platform-rtc-room-tests
		)
	endif()

	add_executable(th03-app-new-classic-key-config-scene-tests
		tests/test_app_new_classic_key_config_scene.cpp
	)
	target_link_libraries(
		th03-app-new-classic-key-config-scene-tests PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-new-classic-key-config-scene-tests)
	add_test(
		NAME th03-app-new-classic-key-config-scene-tests
		COMMAND th03-app-new-classic-key-config-scene-tests
	)

	add_executable(th03-app-new-classic-joy-config-scene-tests
		tests/test_app_new_classic_joy_config_scene.cpp
	)
	target_link_libraries(
		th03-app-new-classic-joy-config-scene-tests PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-new-classic-joy-config-scene-tests)
	add_test(
		NAME th03-app-new-classic-joy-config-scene-tests
		COMMAND th03-app-new-classic-joy-config-scene-tests
	)

	add_executable(th03-app-new-classic-device-scene-tests
		tests/test_app_new_classic_device_scene.cpp
	)
	target_link_libraries(
		th03-app-new-classic-device-scene-tests PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-new-classic-device-scene-tests)
	add_test(
		NAME th03-app-new-classic-device-scene-tests
		COMMAND th03-app-new-classic-device-scene-tests
	)

	add_executable(th03-app-new-classic-netplay-report-tests
		tests/test_app_new_classic_netplay_report.cpp
	)
	target_link_libraries(
		th03-app-new-classic-netplay-report-tests PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-new-classic-netplay-report-tests)
	add_test(
		NAME th03-app-new-classic-netplay-report-tests
		COMMAND th03-app-new-classic-netplay-report-tests
	)

	add_executable(th03-app-new-classic-netplay-room-tests
		tests/test_app_new_classic_netplay_room.cpp
	)
	target_link_libraries(
		th03-app-new-classic-netplay-room-tests PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-new-classic-netplay-room-tests)
	add_test(
		NAME th03-app-new-classic-netplay-room-tests
		COMMAND th03-app-new-classic-netplay-room-tests
	)

	add_executable(th03-app-new-classic-netplay-match-intro-tests
		tests/test_app_new_classic_netplay_match_intro.cpp
	)
	target_link_libraries(
		th03-app-new-classic-netplay-match-intro-tests PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-new-classic-netplay-match-intro-tests)
	add_test(
		NAME th03-app-new-classic-netplay-match-intro-tests
		COMMAND th03-app-new-classic-netplay-match-intro-tests
	)
	target_link_libraries(
		th03-app-new-classic-assets-tests PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-new-classic-assets-tests)
	add_test(
		NAME th03-app-new-classic-assets-tests
		COMMAND th03-app-new-classic-assets-tests
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
	if(MSVC)
		# The supplied-original lifecycle gate keeps several full 640x400
		# source surfaces live at once. Give the test harness enough stack to
		# exercise that route under MSVC Debug as well as GCC.
		target_link_options(
			th03-app-classic-runner-tests PRIVATE "/STACK:8388608"
		)
	endif()
	th03_set_warnings(th03-app-classic-runner-tests)
	add_test(
		NAME th03-app-classic-runner-tests
		COMMAND th03-app-classic-runner-tests
	)

	add_executable(th03-app-pixel-oracle-identity-tests
		tests/test_app_pixel_oracle_identity.cpp
	)
	target_link_libraries(
		th03-app-pixel-oracle-identity-tests PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-pixel-oracle-identity-tests)
	add_test(
		NAME th03-app-pixel-oracle-identity-tests
		COMMAND th03-app-pixel-oracle-identity-tests
	)

	add_executable(th03-app-new-classic-language-tests
		tests/test_app_new_classic_language.cpp
	)
	target_link_libraries(
		th03-app-new-classic-language-tests PRIVATE th03_app_main
	)
	target_include_directories(
		th03-app-new-classic-language-tests PRIVATE src/app
	)
	th03_set_warnings(th03-app-new-classic-language-tests)
	add_test(
		NAME th03-app-new-classic-language-tests
		COMMAND th03-app-new-classic-language-tests
	)

	add_executable(th03-app-replay-slot-store-tests
		tests/test_app_replay_slot_store.cpp
	)
	target_link_libraries(
		th03-app-replay-slot-store-tests PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-replay-slot-store-tests)
	add_test(
		NAME th03-app-replay-slot-store-tests
		COMMAND th03-app-replay-slot-store-tests
	)

	add_executable(th03-app-new-classic-replay-tests
		tests/test_app_new_classic_replay.cpp
	)
	add_executable(th03-app-new-classic-practice-tests
		tests/test_app_new_classic_practice.cpp
	)
	target_link_libraries(
		th03-app-new-classic-practice-tests PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-new-classic-practice-tests)
	add_test(
		NAME th03-app-new-classic-practice-tests
		COMMAND th03-app-new-classic-practice-tests
	)

	add_executable(th03-app-new-classic-frontend-tests
		tests/test_app_new_classic_frontend.cpp
	)

	add_executable(th03-app-new-classic-playback-tests
		tests/test_app_new_classic_playback.cpp
	)
	target_link_libraries(
		th03-app-new-classic-playback-tests PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-new-classic-playback-tests)
	add_test(
		NAME th03-app-new-classic-playback-tests
		COMMAND th03-app-new-classic-playback-tests
	)
	target_link_libraries(
		th03-app-new-classic-frontend-tests PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-new-classic-frontend-tests)
	add_test(
		NAME th03-app-new-classic-frontend-tests
		COMMAND th03-app-new-classic-frontend-tests
	)
	target_link_libraries(
		th03-app-new-classic-replay-tests PRIVATE th03_app_main
	)
	th03_set_warnings(th03-app-new-classic-replay-tests)
	add_test(
		NAME th03-app-new-classic-replay-tests
		COMMAND th03-app-new-classic-replay-tests
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
