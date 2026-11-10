# Checkout-independent compiler output (issue #405). CMake passes every source
# by its absolute path, and assert() and other __FILE__ users then put the
# checkout directory into the PEF: the same commit built in two directories
# gave two pef_sha256 values. -ffile-prefix-map (GCC 8 and later) rewrites the
# source tree to "." in __FILE__ and in debug information. A build tree outside
# the source tree is rewritten to "build". GCC tries the last map first, so the
# source tree map comes last and wins when one tree lies inside the other.
#
# CMakeLists.txt includes this file; tests/run_reproducible_build_tests.sh
# includes it from two directories and compares the objects it compiles.

if(NOT CMAKE_BINARY_DIR STREQUAL CMAKE_SOURCE_DIR)
    add_compile_options("-ffile-prefix-map=${CMAKE_BINARY_DIR}=build")
endif()
add_compile_options("-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.")
