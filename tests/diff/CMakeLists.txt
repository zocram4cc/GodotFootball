# Differential tests: PES's own functions (emulated at test time from the user's
# executable) vs GF's ports on the same inputs, bit for bit. Corpora are
# generated into the build directory by tools/dev/diffharness/generate.sh and
# never committed. Each function wires two ctest entries: diff_gen_<fn> builds
# the corpus (skips with 77 when there is no executable or emulator, like the
# install-gated tests), and the C++ binary compares (skips when the corpus is
# absent). The binary requires the generator's fixture so the two run in order.

function(add_diff_test name specfile test_src link_libs)
  add_executable(diff_${name}_test ${test_src})
  target_link_libraries(diff_${name}_test PRIVATE ${link_libs} GTest::gtest_main)
  target_include_directories(diff_${name}_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_definitions(diff_${name}_test PRIVATE
    GF_DIFF_CORPUS_DIR="${CMAKE_BINARY_DIR}/diff")
  gtest_discover_tests(diff_${name}_test PROPERTIES FIXTURES_REQUIRED diff_gen_${name})
  add_test(NAME diff_gen_${name}
    COMMAND bash ${CMAKE_SOURCE_DIR}/tools/dev/diffharness/generate.sh ${specfile}
            ${CMAKE_BINARY_DIR}/diff/${specfile}.jsonl)
  set_tests_properties(diff_gen_${name} PROPERTIES
    SKIP_RETURN_CODE 77
    FIXTURES_SETUP diff_gen_${name}
    TIMEOUT 600)
endfunction()

include(GoogleTest)
add_diff_test(band band diff_band_test.cpp pesfmt)
add_diff_test(cond_arrow cond_arrow diff_cond_arrow_test.cpp pesfmt)
add_diff_test(cone cone diff_cone_test.cpp "gfsim;pesfmt")
add_diff_test(strategy strategy_minute0 diff_strategy_test.cpp "gfsim;pesfmt")
