execute_process(
	COMMAND "${TH03_APP}"
		--classic
		--data missing-data-root
		--main missing-main.exe
		--mainl missing-mainl.exe
		--op missing-op.exe
		--font missing-font.rom
		--tick-rate 1/1
	RESULT_VARIABLE result
	OUTPUT_VARIABLE output
	ERROR_VARIABLE error
)

if(result EQUAL 0)
	message(FATAL_ERROR "missing Classic MAIN input unexpectedly succeeded")
endif()
if(NOT error MATCHES "stage=th03-load")
	message(FATAL_ERROR "missing Classic MAIN diagnostic omitted the load stage")
endif()
if(NOT error MATCHES "code=file-missing")
	message(FATAL_ERROR "missing Classic MAIN diagnostic omitted the named error")
endif()
if(NOT error MATCHES "file=main-executable")
	message(FATAL_ERROR "missing Classic MAIN diagnostic omitted the semantic file role")
endif()
if(error MATCHES "missing-main\\.exe" OR output MATCHES "missing-main\\.exe")
	message(FATAL_ERROR "missing Classic MAIN diagnostic leaked the supplied path token")
endif()
