execute_process(
	COMMAND "${TH03_APP}"
		--vs
		--data missing-data-root
		--main missing-main.exe
		--font missing-font.rom
		--tick-rate 1/1
	RESULT_VARIABLE result
	OUTPUT_VARIABLE output
	ERROR_VARIABLE error
)

if(result EQUAL 0)
	message(FATAL_ERROR "missing MAIN input unexpectedly succeeded")
endif()
if(NOT error MATCHES "stage=content")
	message(FATAL_ERROR "missing MAIN diagnostic omitted the content stage")
endif()
if(NOT error MATCHES "code=file-missing")
	message(FATAL_ERROR "missing MAIN diagnostic omitted the named error")
endif()
if(NOT error MATCHES "file=main-executable")
	message(FATAL_ERROR "missing MAIN diagnostic omitted the semantic file role")
endif()
if(error MATCHES "missing-main\\.exe" OR output MATCHES "missing-main\\.exe")
	message(FATAL_ERROR "missing MAIN diagnostic leaked the supplied path token")
endif()
