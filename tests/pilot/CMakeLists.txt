# Pilot differential test: the translated pilot functions vs PES's own bytes, bit for bit.
#
# One test runs tools/dev/translate/driver.py --check: every VA that results.json (committed) records as diffed is
# translated from the user's image with its callee closure, built and diffed against the unicorn harness again, and the
# test fails if any of them no longer matches. Undiffed VAs are tracked in the ledger, not here: a known failure is not a
# red gate. Generated code and corpora go into the build tree only, never the repo. Without the unpacked image
# (PES_EXE) or the harness Python (GF_DIFF_PYTHON) the test skips (77), and the gate counts a skipped diff test as red
# when both are set.
add_test(NAME diff_pilot_recorded
  COMMAND bash -c "[ -n \"$PES_EXE\" ] && [ -f \"$PES_EXE\" ] && [ -n \"$GF_DIFF_PYTHON\" ] || exit 77; \
SEL=(); [ -n \"$GF_RECORDED_SELECT\" ] && SEL=(--select \"$GF_RECORDED_SELECT\"); \
\"$GF_DIFF_PYTHON\" '${CMAKE_SOURCE_DIR}/tools/dev/translate/driver.py' --check --image \"$PES_EXE\" \
--out-dir '${CMAKE_BINARY_DIR}/pilot-check' --record '${CMAKE_SOURCE_DIR}/tools/dev/translate/results.json' \"\${SEL[@]}\"")
set_tests_properties(diff_pilot_recorded PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 2400)

# diff_cutscene_recorded: the same check for the cutscene set (specs/cutscene; results_cutscene.json records its diffed VAs).
add_test(NAME diff_cutscene_recorded
  COMMAND bash -c "[ -n \"$PES_EXE\" ] && [ -f \"$PES_EXE\" ] && [ -n \"$GF_DIFF_PYTHON\" ] || exit 77; \
SEL=(); [ -n \"$GF_RECORDED_SELECT\" ] && SEL=(--select \"$GF_RECORDED_SELECT\"); \
\"$GF_DIFF_PYTHON\" '${CMAKE_SOURCE_DIR}/tools/dev/translate/driver.py' --check --image \"$PES_EXE\" \
--spec-dir '${CMAKE_SOURCE_DIR}/tools/dev/diffharness/specs/cutscene' \
--out-dir '${CMAKE_BINARY_DIR}/cutscene-check' --record '${CMAKE_SOURCE_DIR}/tools/dev/translate/results_cutscene.json' \"\${SEL[@]}\"")
# 4800 s: the full 685-function check took 2855 s at load ~30 (batch AO); 2400 timed out twice there.
set_tests_properties(diff_cutscene_recorded PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 4800)

# diff_presentation_recorded: the same check for the presentation set (specs/presentation; results_presentation.json
# records its diffed VAs, each with the subsystem and role of its row).
add_test(NAME diff_presentation_recorded
  COMMAND bash -c "[ -n \"$PES_EXE\" ] && [ -f \"$PES_EXE\" ] && [ -n \"$GF_DIFF_PYTHON\" ] || exit 77; \
SEL=(); [ -n \"$GF_RECORDED_SELECT\" ] && SEL=(--select \"$GF_RECORDED_SELECT\"); \
\"$GF_DIFF_PYTHON\" '${CMAKE_SOURCE_DIR}/tools/dev/translate/driver.py' --check --image \"$PES_EXE\" \
--spec-dir '${CMAKE_SOURCE_DIR}/tools/dev/diffharness/specs/presentation' \
--out-dir '${CMAKE_BINARY_DIR}/presentation-check' --record '${CMAKE_SOURCE_DIR}/tools/dev/translate/results_presentation.json' \"\${SEL[@]}\"")
# 4800 s: 1881 s alone at load ~30 (batch AO); under the full fast tier at load ~30 (batch AP) 2400 timed out.
set_tests_properties(diff_presentation_recorded PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 4800)

# diff_core_recorded: the same check for the core set (specs/core; results_core.json records its diffed VAs, each with the
# subsystem and role of its row). specs/core also holds the callees another set records: they are the callee universe,
# and their records stay in their own sets (driver --exclude-recorded).
add_test(NAME diff_core_recorded
  COMMAND bash -c "[ -n \"$PES_EXE\" ] && [ -f \"$PES_EXE\" ] && [ -n \"$GF_DIFF_PYTHON\" ] || exit 77; \
SEL=(); [ -n \"$GF_RECORDED_SELECT\" ] && SEL=(--select \"$GF_RECORDED_SELECT\"); \
\"$GF_DIFF_PYTHON\" '${CMAKE_SOURCE_DIR}/tools/dev/translate/driver.py' --check --image \"$PES_EXE\" \
--spec-dir '${CMAKE_SOURCE_DIR}/tools/dev/diffharness/specs/core' \
--out-dir '${CMAKE_BINARY_DIR}/core-check' --record '${CMAKE_SOURCE_DIR}/tools/dev/translate/results_core.json' \"\${SEL[@]}\"")
# The check replays all 2,253 recorded core functions: about 2.5 h on a loaded machine (EndClear2, 28-10-26), past the
# old 2 h cap when the gate shares the machine with agents.
set_tests_properties(diff_core_recorded PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 14400)

# diff_impure_recorded: the same check for the impure set (specs/impure; results_impure.json records its diffed VAs).
# Without it the impure record was never re-checked: the driver's default spec dir (specs/pilot) held none of its VAs.
# The impure record names each VA's corpus (results_impure.json "corpus"). 1404386D0's pointer and handle fields take the
# generator's generic 0..255 domain, so its random corpus dereferences small addresses; its recorded corpus is its capture
# bank, which is PES-derived and never committed. GF_IMPURE_BANKS names the bank directory (<VA>.bank.jsonl); without it
# that VA fails (it is not run without its bank), so the check is red, never skipped.
add_test(NAME diff_impure_recorded
  COMMAND bash -c "[ -n \"$PES_EXE\" ] && [ -f \"$PES_EXE\" ] && [ -n \"$GF_DIFF_PYTHON\" ] || exit 77; \
BANK=(); [ -n \"$GF_IMPURE_BANKS\" ] && BANK=(--seed-bank \"$GF_IMPURE_BANKS\"); \
SEL=(); [ -n \"$GF_RECORDED_SELECT\" ] && SEL=(--select \"$GF_RECORDED_SELECT\"); \
\"$GF_DIFF_PYTHON\" '${CMAKE_SOURCE_DIR}/tools/dev/translate/driver.py' --check --image \"$PES_EXE\" \
--spec-dir '${CMAKE_SOURCE_DIR}/tools/dev/diffharness/specs/impure' \
--out-dir '${CMAKE_BINARY_DIR}/impure-check' --record '${CMAKE_SOURCE_DIR}/tools/dev/translate/results_impure.json' \
\"\${BANK[@]}\" \"\${SEL[@]}\"")
set_tests_properties(diff_impure_recorded PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 3600)

# Image check: the user's dump must be the image the pilot specs were authored
# against. verify_image.py hashes the stable sections (image_hashes.json) and
# fails naming any section that differs. Skips (77) without PES_EXE, and the
# gate counts a skipped diff test as red when PES_EXE is set.
add_test(NAME diff_image_verify
  COMMAND bash -c "[ -n \"$PES_EXE\" ] && [ -f \"$PES_EXE\" ] || exit 77; \
python3 '${CMAKE_SOURCE_DIR}/tools/pes21_import/verify_image.py' --image \"$PES_EXE\"")
set_tests_properties(diff_image_verify PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)

# Team sheet readers: the translated readers on the record's bytes, checked against the record layout and the
# refusals PES makes. No image is needed: the translated code is committed.
add_executable(pes_team_sheet_test pes_team_sheet_test.cpp)
target_link_libraries(pes_team_sheet_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_team_sheet_test)

# World record tables: the translated table accessors read G's entries and a slot's block fields in place.
add_executable(pes_world_test pes_world_test.cpp)
target_link_libraries(pes_world_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_world_test)

# Native heap seam: translated code that allocates (the FixDemo evaluator, vector growth) runs on GF memory through PES's
# own allocator table, whose vtable targets dispatch to PesHeap (stubs.json native_services).
add_executable(pes_native_heap_test pes_native_heap_test.cpp)
target_link_libraries(pes_native_heap_test PRIVATE pestranslated GTest::gtest_main)
gtest_discover_tests(pes_native_heap_test)
# The stub table's native services and the dispatch table gen_dispatch.py emits for them (stdlib only).
add_test(NAME translate_native_services
  COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/dev/translate/test_native_services.py)
# A tail jump to an untranslated target is loud, never a silent return (xlate, driver.runnable_extra, the layer check).
add_test(NAME translate_tail_jump_loud
  COMMAND bash -c "[ -n \"$GF_DIFF_PYTHON\" ] || exit 77; \"$GF_DIFF_PYTHON\" '${CMAKE_SOURCE_DIR}/tools/dev/translate/test_tail_jump_loud.py'")
set_tests_properties(translate_tail_jump_loud PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 300)
# The logf/powf models against C99 Annex F and (when the capture is there) the snapshot's ucrtbase.
add_test(NAME translate_libm_models
  COMMAND bash -c "[ -n \"$GF_DIFF_PYTHON\" ] || exit 77; \"$GF_DIFF_PYTHON\" '${CMAKE_SOURCE_DIR}/tools/dev/translate/test_libm_models.py'")
set_tests_properties(translate_libm_models PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)

# finish_one's coverage record skips an undecodable function (block_coverage's None), never unpacks it (plain python3).
add_test(NAME translate_finish_coverage
  COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/dev/translate/test_finish_coverage.py)
set_tests_properties(translate_finish_coverage PROPERTIES TIMEOUT 120)

# traced_cov's report leaves an undecodable function (block_coverage's None) out, never unpacks it (plain python3).
add_test(NAME translate_traced_cov
  COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/dev/translate/test_traced_cov.py)
set_tests_properties(translate_traced_cov PROPERTIES TIMEOUT 120)

# Drift: regenerating every committed translated function from the image must reproduce the committed files.
# Hand-refined functions are skipped by the check (their bit-exact proof is the harness check above).
add_test(NAME diff_translated_drift
  COMMAND bash -c "[ -n \"$PES_EXE\" ] && [ -f \"$PES_EXE\" ] && [ -n \"$GF_DIFF_PYTHON\" ] || exit 77; \
\"$GF_DIFF_PYTHON\" '${CMAKE_SOURCE_DIR}/tools/dev/translate/gen_translated.py' --check --image \"$PES_EXE\"")
set_tests_properties(diff_translated_drift PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 1800)
