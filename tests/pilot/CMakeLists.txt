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
--out-dir '${CMAKE_BINARY_DIR}/pilot-check' --record '${CMAKE_SOURCE_DIR}/tools/dev/translate/results.json'")
set_tests_properties(diff_pilot_recorded PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 2400)

# diff_cutscene_recorded: the same check for the cutscene set (specs/cutscene; results_cutscene.json records its diffed VAs).
add_test(NAME diff_cutscene_recorded
  COMMAND bash -c "[ -n \"$PES_EXE\" ] && [ -f \"$PES_EXE\" ] && [ -n \"$GF_DIFF_PYTHON\" ] || exit 77; \
\"$GF_DIFF_PYTHON\" '${CMAKE_SOURCE_DIR}/tools/dev/translate/driver.py' --check --image \"$PES_EXE\" \
--spec-dir '${CMAKE_SOURCE_DIR}/tools/dev/diffharness/specs/cutscene' \
--out-dir '${CMAKE_BINARY_DIR}/cutscene-check' --record '${CMAKE_SOURCE_DIR}/tools/dev/translate/results_cutscene.json'")
set_tests_properties(diff_cutscene_recorded PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 2400)

# Image check: the user's dump must be the image the pilot specs were authored
# against. verify_image.py hashes the stable sections (image_hashes.json) and
# fails naming any section that differs. Skips (77) without PES_EXE, and the
# gate counts a skipped diff test as red when PES_EXE is set.
add_test(NAME diff_image_verify
  COMMAND bash -c "[ -n \"$PES_EXE\" ] && [ -f \"$PES_EXE\" ] || exit 77; \
python3 '${CMAKE_SOURCE_DIR}/tools/pes21_import/verify_image.py' --image \"$PES_EXE\"")
set_tests_properties(diff_image_verify PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)
