# Pilot differential tests: the translated pilot functions vs PES's own
# bytes, bit for bit. Wired like tests/diff/ (see ../tests/diff/CMakeLists.txt):
# each VA gets diff_gen_pilot_<va> (corpus into the build tree, skip 77
# without PES_EXE/unicorn) and diff_pilot_<va> (the translated TU + generated
# diff main, compiled at build time from the committed translator and specs;
# skips when the corpus is absent). The translator reads constants from the
# user's image at build time (never committed); without PES_EXE the translate
# step emits a skip stub so the build stays green.
#
# The VA list is results.json (committed): every translated VA gets a test;
# functions whose spec cannot run still get a corpus test that skips via the
# harness probe. Keeping all 252 wired (not just the diffed ones) means the
# suite measures the pilot every gate run.

function(add_pilot_diff_test va)
  set(gen_dir "${CMAKE_BINARY_DIR}/pilot")
  file(MAKE_DIRECTORY "${gen_dir}")
  # Translate at build time (needs PES_EXE for image bytes; without it,
  # write a skip stub so the build stays green and the test skips).
  add_custom_command(
    OUTPUT "${gen_dir}/pes_fn_${va}.cpp" "${gen_dir}/diff_${va}.cpp"
    COMMAND ${CMAKE_COMMAND} -E env
      "PILOT_VA=${va}" "PILOT_GEN=${gen_dir}"
      "PILOT_SPEC=${CMAKE_SOURCE_DIR}/tools/dev/diffharness/specs/pilot/${va}.json"
      "PILOT_IMAGE=$ENV{PES_EXE}"
      python3 "${CMAKE_SOURCE_DIR}/tools/dev/translate/gen_pilot_test.py"
    DEPENDS "${CMAKE_SOURCE_DIR}/tools/dev/diffharness/specs/pilot/${va}.json"
    COMMENT "Translating pilot ${va}")
  add_executable(diff_pilot_${va} "${gen_dir}/diff_${va}.cpp"
    "${gen_dir}/pes_fn_${va}.cpp")
  target_include_directories(diff_pilot_${va} PRIVATE
    "${CMAKE_SOURCE_DIR}/tools/dev/translate" "${gen_dir}")
  target_compile_definitions(diff_pilot_${va} PRIVATE
    GF_DIFF_CORPUS_DIR="${CMAKE_BINARY_DIR}/pilot")
  add_test(NAME diff_pilot_${va}
    COMMAND bash -c "test -f '${CMAKE_BINARY_DIR}/pilot/corp_${va}.jsonl' || exit 77; '${CMAKE_BINARY_DIR}/tests/pilot/diff_pilot_${va}' '${CMAKE_BINARY_DIR}/pilot/corp_${va}.jsonl'")
  set_tests_properties(diff_pilot_${va} PROPERTIES
    SKIP_RETURN_CODE 77
    FIXTURES_REQUIRED diff_gen_pilot_${va}
    TIMEOUT 600)
  add_test(NAME diff_gen_pilot_${va}
    COMMAND bash ${CMAKE_SOURCE_DIR}/tools/dev/diffharness/generate_pilot.sh ${va}
            ${CMAKE_BINARY_DIR}/pilot/corp_${va}.jsonl)
  set_tests_properties(diff_gen_pilot_${va} PROPERTIES
    SKIP_RETURN_CODE 77
    FIXTURES_SETUP diff_gen_pilot_${va}
    TIMEOUT 600)
endfunction()

# The translated set: every VA with translated=true in results.json.
file(READ "${CMAKE_SOURCE_DIR}/tools/dev/translate/results.json" _pilot_json)
string(JSON _pilot_len LENGTH "${_pilot_json}")
# Parse the VA list with a build-time glob over the committed specs instead:
# results.json is the progress record; the spec dir is the test list.
file(GLOB _pilot_specs RELATIVE "${CMAKE_SOURCE_DIR}/tools/dev/diffharness/specs/pilot"
  "${CMAKE_SOURCE_DIR}/tools/dev/diffharness/specs/pilot/*.json")
foreach(_spec ${_pilot_specs})
  string(REPLACE ".json" "" _va "${_spec}")
  add_pilot_diff_test(${_va})
endforeach()
