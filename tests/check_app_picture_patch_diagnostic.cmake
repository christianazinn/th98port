if(NOT DEFINED TH03_APP)
	message(FATAL_ERROR "TH03_APP is required")
endif()

execute_process(
	COMMAND "${TH03_APP}"
		--shell
		--picture-patch-pack one
		--picture-patch-pack two
	RESULT_VARIABLE result
	OUTPUT_VARIABLE output
	ERROR_VARIABLE error
)

if(NOT result EQUAL 1)
	message(FATAL_ERROR "duplicate picture-patch option returned ${result}")
endif()

set(diagnostic "${output}${error}")
foreach(expected IN ITEMS
	"stage=arguments"
	"code=duplicate-option"
	"option=picture-patch-pack"
	"argument-index=3"
)
	string(FIND "${diagnostic}" "${expected}" found)
	if(found EQUAL -1)
		message(FATAL_ERROR
			"missing '${expected}' in duplicate picture-patch diagnostic:\n${diagnostic}"
		)
	endif()
endforeach()
