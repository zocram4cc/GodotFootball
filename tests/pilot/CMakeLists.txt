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
\"$GF_DIFF_PYTHON\" '${CMAKE_SOURCE_DIR}/tools/dev/translate/driver.py' --check --image \"$PES_EXE\" \
--out-dir '${CMAKE_BINARY_DIR}/pilot-check'")
set_tests_properties(diff_pilot_recorded PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 2400)

# Image check: the user's dump must be the image the pilot specs were authored
# against. verify_image.py hashes the stable sections (image_hashes.json) and
# fails naming any section that differs. Skips (77) without PES_EXE, and the
# gate counts a skipped diff test as red when PES_EXE is set.
add_test(NAME diff_image_verify
  COMMAND bash -c "[ -n \"$PES_EXE\" ] && [ -f \"$PES_EXE\" ] || exit 77; \
python3 '${CMAKE_SOURCE_DIR}/tools/pes21_import/verify_image.py' --image \"$PES_EXE\"")
set_tests_properties(diff_image_verify PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)

# Team sheet readers: each translated reader agrees with its hand port on the same random sheet bytes (the
# window maps S onto the bytes in place). Skipped (the TU gets no PES_PILOT_AVAILABLE) when no image is configured.
add_executable(pes_team_sheet_test pes_team_sheet_test.cpp)
target_link_libraries(pes_team_sheet_test PRIVATE gfsim pesfmt GTest::gtest_main)
if(TARGET pespilot)
  target_link_libraries(pes_team_sheet_test PRIVATE pespilot)
endif()
gtest_discover_tests(pes_team_sheet_test)
