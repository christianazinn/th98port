file(MAKE_DIRECTORY "${TH03_TEST_DIR}")
set(short_rom "${TH03_TEST_DIR}/short.rom")
set(exact_rom "${TH03_TEST_DIR}/exact.rom")
file(WRITE "${short_rom}" "1234567")
string(REPEAT "0" 8192 exact_bytes)
file(WRITE "${exact_rom}" "${exact_bytes}")

function(run_classic_with_rom rom result_out output_out error_out)
	execute_process(
		COMMAND "${TH03_APP}"
			--classic
			--data missing-data-root
			--main missing-main.exe
			--mainl missing-mainl.exe
			--op missing-op.exe
			--font missing-font.rom
			--rhythm-rom "${rom}"
			--tick-rate 1/1
		RESULT_VARIABLE result
		OUTPUT_VARIABLE output
		ERROR_VARIABLE error
	)
	set(${result_out} "${result}" PARENT_SCOPE)
	set(${output_out} "${output}" PARENT_SCOPE)
	set(${error_out} "${error}" PARENT_SCOPE)
endfunction()

run_classic_with_rom("${short_rom}" short_result short_output short_error)
if(short_result EQUAL 0)
	message(FATAL_ERROR "short rhythm ROM unexpectedly succeeded")
endif()
if(NOT short_error MATCHES "stage=rhythm-rom")
	message(FATAL_ERROR "short rhythm ROM diagnostic omitted the load stage")
endif()
if(NOT short_error MATCHES "code=file-size-mismatch")
	message(FATAL_ERROR "short rhythm ROM diagnostic omitted the size error")
endif()
if(NOT short_error MATCHES "expected-size=8192")
	message(FATAL_ERROR "short rhythm ROM diagnostic omitted the required size")
endif()
if(NOT short_error MATCHES "observed-size=7")
	message(FATAL_ERROR "short rhythm ROM diagnostic omitted the observed size")
endif()
if(short_error MATCHES "short\.rom" OR short_output MATCHES "short\.rom")
	message(FATAL_ERROR "short rhythm ROM diagnostic leaked the supplied path")
endif()

run_classic_with_rom("${exact_rom}" exact_result exact_output exact_error)
if(exact_result EQUAL 0)
	message(FATAL_ERROR "missing Classic content unexpectedly succeeded")
endif()
if(NOT exact_error MATCHES "stage=th03-load")
	message(FATAL_ERROR "exact-size rhythm ROM did not reach Classic content load")
endif()
if(exact_error MATCHES "stage=rhythm-rom")
	message(FATAL_ERROR "exact-size rhythm ROM was rejected")
endif()
if(exact_error MATCHES "exact\.rom" OR exact_output MATCHES "exact\.rom")
	message(FATAL_ERROR "exact rhythm ROM path leaked to output")
endif()
