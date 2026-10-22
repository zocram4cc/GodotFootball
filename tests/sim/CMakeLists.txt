add_executable(sim_database_test sim_database_test.cpp)
target_link_libraries(sim_database_test PRIVATE gfsim GTest::gtest_main)
gtest_discover_tests(sim_database_test)

add_executable(sim_fixture_test sim_fixture_test.cpp)
target_link_libraries(sim_fixture_test PRIVATE gfsim GTest::gtest_main)
target_compile_definitions(sim_fixture_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(sim_fixture_test)

add_executable(sim_match_test sim_match_test.cpp)
target_link_libraries(sim_match_test PRIVATE gfsim pesfmt pesmovestepcore GTest::gtest_main)
target_compile_definitions(sim_match_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
# Whole matches: far beyond gtest_discover_tests' default 5 s discovery budget per test.
gtest_discover_tests(sim_match_test PROPERTIES TIMEOUT 1800)

add_executable(pes_clock_match_test pes_clock_match_test.cpp)
target_link_libraries(pes_clock_match_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_clock_match_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_clock_match_test PROPERTIES TIMEOUT 1800)

add_executable(pes_period_end_test pes_period_end_test.cpp)
target_link_libraries(pes_period_end_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_period_end_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_period_end_test PROPERTIES TIMEOUT 1800)

add_executable(pes_keeper_test pes_keeper_test.cpp)
target_link_libraries(pes_keeper_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_keeper_test)

add_test(NAME gfsim_run_test
  COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/gfsim_run_test.sh $<TARGET_FILE:gfsim_run> ${CMAKE_SOURCE_DIR})
set_tests_properties(gfsim_run_test PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 1800)

add_executable(pes_ball_physics_test pes_ball_physics_test.cpp)
target_link_libraries(pes_ball_physics_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_ball_physics_test PROPERTIES TIMEOUT 600)

add_executable(pes_fieldinfo_test pes_fieldinfo_test.cpp)
target_link_libraries(pes_fieldinfo_test PRIVATE gfsim GTest::gtest_main)
gtest_discover_tests(pes_fieldinfo_test)

add_executable(pes_ball_track_test pes_ball_track_test.cpp)
target_link_libraries(pes_ball_track_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_ball_track_test)

add_executable(pes_mover_test pes_mover_test.cpp)
target_link_libraries(pes_mover_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_mover_test)

add_executable(pes_tick_a1_test pes_tick_a1_test.cpp)
target_link_libraries(pes_tick_a1_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_tick_a1_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
# Three of the tests build a whole match to reach the live ball and the AI.
gtest_discover_tests(pes_tick_a1_test PROPERTIES TIMEOUT 1800)

add_executable(pes_event_wiring_test pes_event_wiring_test.cpp)
target_link_libraries(pes_event_wiring_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_event_wiring_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_event_wiring_test PROPERTIES TIMEOUT 1800)

add_executable(pes_tick_order_test pes_tick_order_test.cpp)
target_link_libraries(pes_tick_order_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_tick_order_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_tick_order_test PROPERTIES TIMEOUT 1800)

add_executable(pes_movestep_wire_test pes_movestep_wire_test.cpp)
target_link_libraries(pes_movestep_wire_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_movestep_wire_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_movestep_wire_test PROPERTIES TIMEOUT 1800)

add_executable(pes_judge_test pes_judge_test.cpp)
target_link_libraries(pes_judge_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_judge_test)

add_executable(pes_offside_test pes_offside_test.cpp)
target_link_libraries(pes_offside_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_offside_test)

add_executable(pes_stoppage_timing_test pes_stoppage_timing_test.cpp)
target_link_libraries(pes_stoppage_timing_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_stoppage_timing_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_stoppage_timing_test PROPERTIES TIMEOUT 1800)

add_executable(pes_restart_sequencer_test pes_restart_sequencer_test.cpp)
target_link_libraries(pes_restart_sequencer_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_restart_sequencer_test)

add_executable(pes_restart_delay_test pes_restart_delay_test.cpp)
target_link_libraries(pes_restart_delay_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_restart_delay_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_restart_delay_test PROPERTIES TIMEOUT 1800)

add_executable(pes_throwin_refusal_test pes_throwin_refusal_test.cpp)
target_link_libraries(pes_throwin_refusal_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_throwin_refusal_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_throwin_refusal_test PROPERTIES TIMEOUT 1800)

add_executable(pes_random_test pes_random_test.cpp)
target_link_libraries(pes_random_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_random_test)

add_executable(pes_ball_collision_test pes_ball_collision_test.cpp)
target_link_libraries(pes_ball_collision_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_ball_collision_test)

add_executable(pes_carrier_test pes_carrier_test.cpp)
target_link_libraries(pes_carrier_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_carrier_test)

add_executable(pes_scorerows_test pes_scorerows_test.cpp)
target_link_libraries(pes_scorerows_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_scorerows_test)

add_executable(pes_image_test pes_image_test.cpp)
target_link_libraries(pes_image_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_image_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_image_test)

add_executable(pes_defensive_shape_test pes_defensive_shape_test.cpp)
target_link_libraries(pes_defensive_shape_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_defensive_shape_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_defensive_shape_test PROPERTIES TIMEOUT 1800)
add_executable(pes_defender_decisions_test pes_defender_decisions_test.cpp)
target_link_libraries(pes_defender_decisions_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_defender_decisions_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_defender_decisions_test PROPERTIES TIMEOUT 1800)
add_executable(pes_press_test pes_press_test.cpp)
target_link_libraries(pes_press_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_press_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_press_test PROPERTIES TIMEOUT 1800)
add_executable(pes_press_approach_test pes_press_approach_test.cpp)
target_link_libraries(pes_press_approach_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_press_approach_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_press_approach_test PROPERTIES TIMEOUT 1800)

add_executable(pes_receive_test pes_receive_test.cpp)
target_link_libraries(pes_receive_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_receive_test)

add_executable(pes_challenge_counters_test pes_challenge_counters_test.cpp)
target_link_libraries(pes_challenge_counters_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_challenge_counters_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_challenge_counters_test PROPERTIES TIMEOUT 1800)

add_executable(pes_stamina_test pes_stamina_test.cpp)
target_link_libraries(pes_stamina_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_stamina_test)

add_executable(sim_stamina_test sim_stamina_test.cpp)
target_link_libraries(sim_stamina_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(sim_stamina_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(sim_stamina_test PROPERTIES TIMEOUT 1800)

add_executable(pes_restart_taking_test pes_restart_taking_test.cpp)
target_link_libraries(pes_restart_taking_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_restart_taking_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_restart_taking_test PROPERTIES TIMEOUT 900)

add_executable(pes_setpiece_taker_test pes_setpiece_taker_test.cpp)
target_link_libraries(pes_setpiece_taker_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_setpiece_taker_test)

add_executable(pes_setpiece_stand_test pes_setpiece_stand_test.cpp)
target_link_libraries(pes_setpiece_stand_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_setpiece_stand_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_setpiece_stand_test PROPERTIES TIMEOUT 900)

add_executable(pes_goal_kick_test pes_goal_kick_test.cpp)
target_link_libraries(pes_goal_kick_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_goal_kick_test PROPERTIES TIMEOUT 900)

add_executable(pes_style_gate_test pes_style_gate_test.cpp)
target_link_libraries(pes_style_gate_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_style_gate_test)

add_executable(pes_setpiece_team_test pes_setpiece_team_test.cpp)
target_link_libraries(pes_setpiece_team_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_setpiece_team_test)

add_executable(pes_strategy_test pes_strategy_test.cpp)
target_link_libraries(pes_strategy_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_strategy_test)

add_executable(pes_subs_test pes_subs_test.cpp)
target_link_libraries(pes_subs_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_subs_test)

add_executable(pes_kick_error_test pes_kick_error_test.cpp)
target_link_libraries(pes_kick_error_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_kick_error_test)

add_executable(pes_penalty_scatter_test pes_penalty_scatter_test.cpp)
target_link_libraries(pes_penalty_scatter_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_penalty_scatter_test)

add_executable(pes_team_concepts_test pes_team_concepts_test.cpp)
target_link_libraries(pes_team_concepts_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_team_concepts_test)

add_executable(pes_contact_test pes_contact_test.cpp)
target_link_libraries(pes_contact_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_contact_test)

add_executable(pes_clips_test pes_clips_test.cpp)
target_link_libraries(pes_clips_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_clips_test PROPERTIES TIMEOUT 1800)
add_executable(pes_offball_test pes_offball_test.cpp)
target_link_libraries(pes_offball_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_offball_test)
add_executable(pes_roles_test pes_roles_test.cpp)
target_link_libraries(pes_roles_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_roles_test)
add_executable(pes_forward_test pes_forward_test.cpp)
target_link_libraries(pes_forward_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_forward_test)

add_executable(pes_team_tick_test pes_team_tick_test.cpp)
target_link_libraries(pes_team_tick_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_team_tick_test)

add_executable(sim_heading_test sim_heading_test.cpp)
target_link_libraries(sim_heading_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(sim_heading_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(sim_heading_test PROPERTIES TIMEOUT 1800)

add_executable(sim_missing_player_test sim_missing_player_test.cpp)
target_link_libraries(sim_missing_player_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(sim_missing_player_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(sim_missing_player_test PROPERTIES TIMEOUT 1800)

add_executable(pes_nearest_opponent_test pes_nearest_opponent_test.cpp)
target_link_libraries(pes_nearest_opponent_test PRIVATE gfsim pesfmt GTest::gtest_main)
target_compile_definitions(pes_nearest_opponent_test PRIVATE GF_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
gtest_discover_tests(pes_nearest_opponent_test PROPERTIES TIMEOUT 1800)

add_executable(pes_lane_test pes_lane_test.cpp)
target_link_libraries(pes_lane_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_lane_test PROPERTIES TIMEOUT 1800)

add_executable(penalty_shootout_aim_test penalty_shootout_aim_test.cpp)
target_link_libraries(penalty_shootout_aim_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(penalty_shootout_aim_test)

add_executable(pes_designation_test pes_designation_test.cpp)
target_link_libraries(pes_designation_test PRIVATE gfsim pesfmt GTest::gtest_main)
gtest_discover_tests(pes_designation_test)
