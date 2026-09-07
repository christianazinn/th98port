if(NOT DEFINED TH03_SOURCE_DIR OR NOT DEFINED TH03_BINARY_DIR)
	message(FATAL_ERROR "Source and binary test directories are required")
endif()

file(REMOVE_RECURSE "${TH03_BINARY_DIR}")
execute_process(
	COMMAND "${CMAKE_COMMAND}"
		-S "${TH03_SOURCE_DIR}"
		-B "${TH03_BINARY_DIR}"
		-DTH03_UPDATE_MANIFEST_URL=https://updates.example.test/podd/beta.t3upd
	RESULT_VARIABLE result
	OUTPUT_VARIABLE output
	ERROR_VARIABLE error
)
file(REMOVE_RECURSE "${TH03_BINARY_DIR}")
if(result EQUAL 0)
	message(FATAL_ERROR "Partial update release configuration was accepted")
endif()
string(CONCAT transcript "${output}" "${error}")
if(NOT transcript MATCHES "all-or-nothing")
	message(FATAL_ERROR
		"Partial update configuration failed for the wrong reason:\n${transcript}")
endif()
