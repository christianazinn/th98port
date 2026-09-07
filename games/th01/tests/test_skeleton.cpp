// The th01-port CMake skeleton's own self-test.
//
// WHAT THIS COVERS, AND WHAT IT DOES NOT. It covers the two terms of the
// skeleton's compile contract that are observable from inside a translation
// unit: that the C++23 library facility the copied code is built on is
// actually available, and whether GNU extensions are on. It does NOT cover
// whether the warning set and `-Werror`/`/WX` reached the target -- no portable
// construct inside a TU can see its own command line. That term is checked from
// the generator's output instead (`CMakeFiles/<target>.dir/flags.make`), and
// `th01-port/docs/COPY_DIFF.md` records the comparison. Saying which half is
// checked where is the point: a test that claimed all three would be the more
// comfortable lie.
//
// WHY A FEATURE-TEST MACRO RATHER THAN `__cplusplus`. Measured on this host,
// MSVC 19.41 (VS 2022, toolset 14.41) compiling at `stdcpplatest` -- which is
// what CMake emits for `cxx_std_23` and for `CMAKE_CXX_STANDARD 23` -- reports:
//
//     _MSVC_LANG  = 202004      (C++20 plus the working draft; NOT 202302)
//     __cplusplus = 199711      (`/Zc:__cplusplus` is not in the skeleton)
//
// So neither version macro can witness "C++23" on MSVC, and an assertion on
// either would be a demand this toolset cannot satisfy no matter how correct
// the build files are. `__cpp_lib_expected` can: it is 202211 on BOTH compilers
// here, and `std::expected` is the facility the copied platform library
// actually uses -- 32 occurrences across the six copied translation units and
// headers. Asserting the capability the code depends on, rather than a version
// number that stands in for it, is the difference between a test that measures
// the build and a test that measures the vendor's macro policy.
//
// WHY THE EXTENSIONS TERM IS WORTH A TEST. `th03-port/CMakeLists.txt` records,
// at the `set(CMAKE_CXX_EXTENSIONS OFF)` copied into this tree, that the
// property is per-target and does NOT travel through a library's
// `PUBLIC cxx_std_23`: 80 test executables and 4 apps compiled `-std=gnu++23`
// while every library compiled `-std=c++23`, and the note calls that "an
// inconsistency in the direction that fails silently: a test may use a GNU
// extension, pass, and be uncompilable in the library it is testing". The
// directory-scoped `set()` is the fix. Nothing asserted that the fix reached a
// target. This does, and the control is recorded in COPY_DIFF.md: compiled by
// hand at `-std=gnu++23`, this file fails on exactly that assertion.

#include <iostream>
#include <version>

namespace {

unsigned checks = 0U;
unsigned failures = 0U;

void expect(bool condition, const char *message)
{
	++checks;
	if(!condition) {
		++failures;
		std::cerr << "FAIL: " << message << '\n';
	}
}

// P0323R12 `std::expected`, C++23. The whole error model of the copied
// platform library is `std::expected<T, E>`; if this is absent the library
// cannot compile at all, so it is the honest thing to gate on.
void test_cxx23_library_is_available()
{
	expect(
#if defined(__cpp_lib_expected)
		__cpp_lib_expected >= 202202L,
#else
		false,
#endif
		"the C++23 library mode reached this target "
		"(__cpp_lib_expected, the facility the copied library is built on)"
	);
}

// `-std=c++23` defines `__STRICT_ANSI__`; `-std=gnu++23` does not. That macro
// is a direct witness for `CXX_EXTENSIONS OFF` on GCC and Clang.
//
// MSVC has no such pair. th03-port measured this at the same `set()`:
// extensions-off and extensions-on MSVC targets "already emit identical
// `stdcpplatest` and identical additional options -- so this is a GCC/Clang
// normalization". An assertion on MSVC would be an assertion over a construct
// that does not exist in the target, which is green forever and measures
// nothing. It is skipped, and the skip is printed so a run that checked nothing
// is distinguishable from a run that checked something.
void test_gnu_extensions_are_off()
{
#if defined(_MSC_VER) && !defined(__clang__)
	std::cout
		<< "SKIP: CXX_EXTENSIONS has no observable witness on MSVC; "
		   "th03-port measured extensions-on and extensions-off MSVC "
		   "targets as emitting identical options\n";
#else
	expect(
#	if defined(__STRICT_ANSI__)
		true,
#	else
		false,
#	endif
		"CXX_EXTENSIONS OFF reached this target "
		"(-std=c++NN, not -std=gnu++NN; __STRICT_ANSI__ is defined)"
	);
#endif
}

// Not an assertion: a record. These are the numbers COPY_DIFF.md quotes, and
// printing them means a future toolchain that moves them is visible in the
// ctest log rather than silently satisfying a threshold nobody re-derived.
void report_standard_macros()
{
	std::cout << "standard: __cplusplus=" << __cplusplus;
#if defined(_MSVC_LANG)
	std::cout << " _MSVC_LANG=" << _MSVC_LANG;
#endif
#if defined(__cpp_lib_expected)
	std::cout << " __cpp_lib_expected=" << __cpp_lib_expected;
#endif
	std::cout << '\n';
}

} // namespace

int main()
{
	report_standard_macros();
	test_cxx23_library_is_available();
	test_gnu_extensions_are_off();
	std::cout << "checks=" << checks << " failures=" << failures << '\n';
	return (failures == 0U) ? 0 : 1;
}
