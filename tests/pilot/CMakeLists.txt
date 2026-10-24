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
