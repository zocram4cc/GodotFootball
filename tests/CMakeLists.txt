add_test(NAME repo_pii_scan
  COMMAND bash ${CMAKE_SOURCE_DIR}/tools/check_pii.sh
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

include(GoogleTest)
add_executable(pes_strcode_test pes_strcode_test.cpp)
target_link_libraries(pes_strcode_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_strcode_test)

add_executable(pes_rig_test pes_rig_test.cpp)
target_link_libraries(pes_rig_test PRIVATE pesrig GTest::gtest_main)
gtest_discover_tests(pes_rig_test)

add_executable(pes_clock_test pes_clock_test.cpp)
target_link_libraries(pes_clock_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_clock_test)

add_executable(pes_formats_test pes_formats_test.cpp)
target_link_libraries(pes_formats_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_formats_test)

add_executable(pes_animetable_test pes_animetable_test.cpp)
target_link_libraries(pes_animetable_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_animetable_test)

add_executable(pes_fdc_test pes_fdc_test.cpp)
target_link_libraries(pes_fdc_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_fdc_test)

add_executable(pes_mbinfo_test pes_mbinfo_test.cpp)
target_link_libraries(pes_mbinfo_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_mbinfo_test)

add_executable(pes_dodge_test pes_dodge_test.cpp)
target_link_libraries(pes_dodge_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_dodge_test)

add_executable(pes_tackle_test pes_tackle_test.cpp)
target_link_libraries(pes_tackle_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_tackle_test)

add_executable(pes_anim_test pes_anim_test.cpp)
target_link_libraries(pes_anim_test PRIVATE pesrig GTest::gtest_main)
gtest_discover_tests(pes_anim_test)

add_executable(pes_clip_pose_test pes_clip_pose_test.cpp)
target_link_libraries(pes_clip_pose_test PRIVATE pesrig GTest::gtest_main)
gtest_discover_tests(pes_clip_pose_test)

add_executable(pes_angle_oracle_test pes_angle_oracle_test.cpp)
target_link_libraries(pes_angle_oracle_test PRIVATE gfsim pesfmt pestranslated GTest::gtest_main)
gtest_discover_tests(pes_angle_oracle_test)

add_executable(pes_trap_oracle_test pes_trap_oracle_test.cpp)
target_link_libraries(pes_trap_oracle_test PRIVATE gfsim pesfmt pestranslated GTest::gtest_main)
gtest_discover_tests(pes_trap_oracle_test)

add_executable(pes_danger_oracle_test pes_danger_oracle_test.cpp)
target_link_libraries(pes_danger_oracle_test PRIVATE gfsim pesfmt pestranslated GTest::gtest_main)
gtest_discover_tests(pes_danger_oracle_test)

add_executable(pes_walk_oracle_test pes_walk_oracle_test.cpp)
target_link_libraries(pes_walk_oracle_test PRIVATE gfsim pesfmt pestranslated GTest::gtest_main)
gtest_discover_tests(pes_walk_oracle_test)

add_executable(pes_dribble_oracle_test pes_dribble_oracle_test.cpp)
target_link_libraries(pes_dribble_oracle_test PRIVATE gfsim pesfmt pestranslated GTest::gtest_main)
gtest_discover_tests(pes_dribble_oracle_test)

# Godot-side checks run the real extension headless; 77 = skipped (no PES21_ROOT).
add_test(NAME godot_character_test
  COMMAND ${CMAKE_SOURCE_DIR}/tools/godot.sh --headless --path ${CMAKE_SOURCE_DIR}/godot -s res://tests/test_character.gd)
set_tests_properties(godot_character_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
add_test(NAME godot_player_test
  COMMAND ${CMAKE_SOURCE_DIR}/tools/godot.sh --headless --path ${CMAKE_SOURCE_DIR}/godot -s res://tests/test_player.gd)
set_tests_properties(godot_player_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
add_test(NAME godot_textures_test
  COMMAND ${CMAKE_SOURCE_DIR}/tools/godot.sh --headless --path ${CMAKE_SOURCE_DIR}/godot -s res://tests/test_textures.gd)
set_tests_properties(godot_textures_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
add_test(NAME godot_match_test
  COMMAND ${CMAKE_SOURCE_DIR}/tools/godot.sh --headless --path ${CMAKE_SOURCE_DIR}/godot -s res://tests/test_match.gd)
set_tests_properties(godot_match_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 300)
add_test(NAME godot_match_scene_test
  COMMAND ${CMAKE_SOURCE_DIR}/tools/godot.sh --headless --path ${CMAKE_SOURCE_DIR}/godot res://tests/test_match_scene.tscn)
set_tests_properties(godot_match_scene_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 300)
add_test(NAME godot_motion_test
  COMMAND ${CMAKE_SOURCE_DIR}/tools/godot.sh --headless --path ${CMAKE_SOURCE_DIR}/godot -s res://tests/test_motion.gd)
set_tests_properties(godot_motion_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
add_test(NAME godot_pes_loop_kickoff_test
  COMMAND ${CMAKE_SOURCE_DIR}/tools/godot.sh --headless --path ${CMAKE_SOURCE_DIR}/godot res://tests/test_pes_loop_kickoff.tscn)
set_tests_properties(godot_pes_loop_kickoff_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 900)
add_test(NAME godot_camera_test
  COMMAND ${CMAKE_SOURCE_DIR}/tools/godot.sh --headless --path ${CMAKE_SOURCE_DIR}/godot -s res://tests/test_camera.gd)
set_tests_properties(godot_camera_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
add_test(NAME godot_cutscene_camera_test
  COMMAND ${CMAKE_SOURCE_DIR}/tools/godot.sh --headless --path ${CMAKE_SOURCE_DIR}/godot -s res://tests/test_cutscene_camera.gd)
set_tests_properties(godot_cutscene_camera_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
add_test(NAME godot_model_test
  COMMAND ${CMAKE_SOURCE_DIR}/tools/godot.sh --headless --path ${CMAKE_SOURCE_DIR}/godot -s res://tests/test_model.gd)
set_tests_properties(godot_model_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
add_test(NAME godot_stadium_test
  COMMAND ${CMAKE_SOURCE_DIR}/tools/godot.sh --headless --path ${CMAKE_SOURCE_DIR}/godot res://tests/test_stadium.tscn)
set_tests_properties(godot_stadium_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 600)
add_test(NAME godot_animation_test
  COMMAND ${CMAKE_SOURCE_DIR}/tools/godot.sh --headless --path ${CMAKE_SOURCE_DIR}/godot res://tests/test_animation.tscn)
# A script error in _ready stops before quit(): the timeout turns a hang into a failure.
set_tests_properties(godot_animation_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)
add_subdirectory(sim)
add_subdirectory(diff)
add_subdirectory(pilot)

add_executable(pes_uniform_test pes_uniform_test.cpp)
target_link_libraries(pes_uniform_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_uniform_test)

add_executable(pes_editsave_test pes_editsave_test.cpp)
target_link_libraries(pes_editsave_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_editsave_test)

add_executable(pes_playerdb_test pes_playerdb_test.cpp)
target_link_libraries(pes_playerdb_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_playerdb_test)

add_executable(pes_formation_test pes_formation_test.cpp)
target_link_libraries(pes_formation_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_formation_test)

add_executable(pes_exe_test pes_exe_test.cpp)
target_link_libraries(pes_exe_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_exe_test)

add_executable(pes_derivation_test pes_derivation_test.cpp)
target_link_libraries(pes_derivation_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_derivation_test)

add_executable(pes_handseq_test pes_handseq_test.cpp)
target_link_libraries(pes_handseq_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_handseq_test)

add_executable(pes_marking_test pes_marking_test.cpp)
target_link_libraries(pes_marking_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_marking_test)

add_executable(pes_locomotion_test pes_locomotion_test.cpp)
target_link_libraries(pes_locomotion_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_locomotion_test)

add_executable(pes_movestep_test pes_movestep_test.cpp)
target_link_libraries(pes_movestep_test PRIVATE pesfmt pesmovestepcore GTest::gtest_main)
gtest_discover_tests(pes_movestep_test)

add_executable(pes_movestep_run_test pes_movestep_run_test.cpp movestep_probe_stubs.cpp)
target_link_libraries(pes_movestep_run_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_movestep_run_test)
add_executable(pes_movestep_turn_test pes_movestep_turn_test.cpp)
target_link_libraries(pes_movestep_turn_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_movestep_turn_test)

add_executable(pes_playstate_test pes_playstate_test.cpp)
target_link_libraries(pes_playstate_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_playstate_test)

add_executable(pes_movestep_step_test pes_movestep_step_test.cpp)
target_link_libraries(pes_movestep_step_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_movestep_step_test)

add_executable(pes_camera_test pes_camera_test.cpp)
target_link_libraries(pes_camera_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_camera_test)

add_executable(pes_fixdemo_table_test pes_fixdemo_table_test.cpp)
target_link_libraries(pes_fixdemo_table_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_fixdemo_table_test)

add_executable(pes_fox2_test pes_fox2_test.cpp)
target_link_libraries(pes_fox2_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_fox2_test)

add_executable(pes_stadium_test pes_stadium_test.cpp)
target_link_libraries(pes_stadium_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_stadium_test)
add_executable(pes_lighting_test pes_lighting_test.cpp)
target_link_libraries(pes_lighting_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_lighting_test)

add_executable(pes_fmdl_uv_test pes_fmdl_uv_test.cpp)
target_link_libraries(pes_fmdl_uv_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_fmdl_uv_test)

add_executable(pes_effects_test pes_effects_test.cpp)
target_link_libraries(pes_effects_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_effects_test)

# Godot-side checks run the real extension headless; 77 = skipped (no PES21_ROOT).
add_test(NAME godot_effects_test
  COMMAND ${CMAKE_SOURCE_DIR}/tools/godot.sh --headless --path ${CMAKE_SOURCE_DIR}/godot -s res://tests/test_effects.gd)
set_tests_properties(godot_effects_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)

# The compose pass needs a GPU: gamescope, as tools/capture_viewer.sh does. gamescope exits 0 whatever
# the child does, so the verdict is the script's own summary line.
add_test(NAME godot_post_compose_test
  COMMAND gamescope --backend headless -W 1280 -H 720 -- ${CMAKE_SOURCE_DIR}/tools/godot.sh --path ${CMAKE_SOURCE_DIR}/godot -s res://tests/test_post_compose.gd)
set_tests_properties(godot_post_compose_test PROPERTIES PASS_REGULAR_EXPRESSION "post compose: 0 failure" FAIL_REGULAR_EXPRESSION "FAIL:;SCRIPT ERROR" TIMEOUT 180)

# The meter passes need a GPU for the same reason: the verdict is the script's summary line.
add_test(NAME godot_post_meter_test
  COMMAND gamescope --backend headless -W 1280 -H 720 -- ${CMAKE_SOURCE_DIR}/tools/godot.sh --path ${CMAKE_SOURCE_DIR}/godot -s res://tests/test_post_meter.gd)
set_tests_properties(godot_post_meter_test PROPERTIES PASS_REGULAR_EXPRESSION "post meter: 0 failure" FAIL_REGULAR_EXPRESSION "FAIL:;SCRIPT ERROR" TIMEOUT 180)

add_executable(pes_events_test pes_events_test.cpp)
target_link_libraries(pes_events_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_events_test)

add_executable(pes_possession_test pes_possession_test.cpp)
target_link_libraries(pes_possession_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_possession_test)

add_executable(pes_outside_test pes_outside_test.cpp)
target_link_libraries(pes_outside_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_outside_test)

add_executable(pes_condition_loader_test pes_condition_test.cpp)
target_link_libraries(pes_condition_loader_test PRIVATE pesfmt GTest::gtest_main)
gtest_discover_tests(pes_condition_loader_test)
add_test(NAME parity_metrics_test
  COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/parity_metrics_test.py)

# Recorded-call emulator entry (frame_trace.run_recorded): synthetic snapshots, emulated with unicorn in the
# harness Python. Without GF_DIFF_PYTHON the test skips (77).
add_test(NAME translate_recorded_call_test
  COMMAND bash -c "[ -n \"$GF_DIFF_PYTHON\" ] || exit 77; \
\"$GF_DIFF_PYTHON\" '${CMAKE_SOURCE_DIR}/tools/dev/translate/test_recorded_call.py'")
set_tests_properties(translate_recorded_call_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)

# Closure walk (xlate.capstone_decode bound, pdata.is_boundary, snapdiff's nothing-compiled exit): synthetic image,
# harness Python (capstone). Without GF_DIFF_PYTHON the test skips (77).
add_test(NAME translate_closure_walk_test
  COMMAND bash -c "[ -n \"$GF_DIFF_PYTHON\" ] || exit 77; \
\"$GF_DIFF_PYTHON\" '${CMAKE_SOURCE_DIR}/tools/dev/translate/test_closure_walk.py'")
set_tests_properties(translate_closure_walk_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)

# Function extent (.pdata range + chained chunks: pdata.Pdata.extent/decode, xlate, closure, classify): a synthetic image,
# plus the real PES image when PES_EXE is set. Without GF_DIFF_PYTHON the test skips (77).
add_test(NAME translate_extent_test
  COMMAND bash -c "[ -n \"$GF_DIFF_PYTHON\" ] || exit 77; \
\"$GF_DIFF_PYTHON\" '${CMAKE_SOURCE_DIR}/tools/dev/translate/test_extent.py'")
set_tests_properties(translate_extent_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 300)

# SETcc to memory stores the condition byte (found by lockstep at 0x14084CE60, trap kind-0 closure): compiles a translated
# synthetic function and runs it. Needs g++. Without GF_DIFF_PYTHON the test skips (77).
add_test(NAME translate_setcc_store_test
  COMMAND bash -c "[ -n \"$GF_DIFF_PYTHON\" ] || exit 77; \
\"$GF_DIFF_PYTHON\" '${CMAKE_SOURCE_DIR}/tools/dev/translate/test_setcc_store.py'")
set_tests_properties(translate_setcc_store_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 120)

# Fast-gate selector: a mutated closure callee selects its recorded caller, an unrelated change selects nothing.
# Plain python3 (no harness Python), so it always runs.
add_test(NAME translate_select_recorded_test
  COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/dev/translate/test_select_recorded.py)
set_tests_properties(translate_select_recorded_test PROPERTIES TIMEOUT 120)
