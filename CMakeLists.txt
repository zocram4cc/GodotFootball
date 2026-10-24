cmake_minimum_required(VERSION 3.24)
project(GodotFootball LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(GODOTCPP_API_VERSION 4.7 CACHE STRING "")
set(GODOTCPP_TARGET template_debug CACHE STRING "")
add_subdirectory(extern/godot-cpp)

include(FetchContent)
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

find_package(ZLIB REQUIRED)

add_library(pesfmt STATIC
  src/pes/strcode.cpp src/pes/cpk.cpp src/pes/fpk.cpp src/pes/vfs.cpp src/pes/ask.cpp src/pes/skl.cpp
  src/pes/ftex.cpp src/pes/fmdl.cpp src/pes/pack_rules.cpp src/pes/mtar.cpp src/pes/gani.cpp src/pes/clips.cpp
  src/pes/formation.cpp
  src/pes/anim_names.cpp src/pes/uniform.cpp src/pes/editsave.cpp src/pes/playerdb.cpp src/pes/condition.cpp src/pes/exe.cpp src/pes/handseq.cpp src/pes/marking.cpp src/pes/locomotion.cpp src/pes/animetable.cpp src/pes/mbinfo.cpp src/pes/camera.cpp src/pes/fox2.cpp src/pes/stadium.cpp src/pes/lighting.cpp src/pes/movestep.cpp src/pes/movestep_common.cpp src/pes/movestep_bundle.cpp src/pes/movestep_run.cpp src/pes/movestep_group_a.cpp src/pes/movestep_step.cpp src/pes/movestep_turn.cpp src/pes/effects.cpp src/pes/derivation.cpp)
target_include_directories(pesfmt PUBLIC src)
target_link_libraries(pesfmt PUBLIC ZLIB::ZLIB)

# The dispatcher port's cross-group seams and the shared helpers the full dispatcher links. The
# per-group test targets deliberately keep their probe stubs, so this library is not folded into
# pesfmt; the dispatcher test and the sim adapter link it.
add_library(pesmovestepcore STATIC src/pes/movestep_dispatch.cpp src/pes/movestep_dispatch_seams.cpp
             src/pes/movestep_style.cpp src/pes/movestep_ramps.cpp)
target_link_libraries(pesmovestepcore PUBLIC pesfmt)
add_library(pesrig STATIC src/rig/rig.cpp src/rig/clip_pose.cpp)
target_link_libraries(pesrig PUBLIC pesfmt)

# The pilot cutover library: translated PES functions, generated at build
# time from the user's image into the build tree (never committed), linked
# only on the PES path. Gated at configure time on PES_EXE: without the
# image there is no pespilot target and PES_PILOT_AVAILABLE stays unset, so
# GF call sites take the hand-port fallback and GF mode is untouched.
set(PILOT_CUTOVER_VAS 140A71990)
if(DEFINED ENV{PES_EXE} AND NOT "$ENV{PES_EXE}" STREQUAL "")
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/pilot-lib/pes_pilot.h
           ${CMAKE_BINARY_DIR}/pilot-lib/pes_fn_140A71990.cpp
    COMMAND ${CMAKE_COMMAND} -E env
      "PILOT_IMAGE=$ENV{PES_EXE}"
      python3 ${CMAKE_SOURCE_DIR}/tools/dev/translate/gen_pilot_lib.py
      --image "$ENV{PES_EXE}"
      --out-dir ${CMAKE_BINARY_DIR}/pilot-lib
      --vas ${PILOT_CUTOVER_VAS}
      --spec-dir ${CMAKE_SOURCE_DIR}/tools/dev/diffharness/specs/pilot
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/dev/translate/xlate.py
    COMMENT "Generating pilot cutover TUs")
  add_custom_target(pilot_lib_gen DEPENDS
    ${CMAKE_BINARY_DIR}/pilot-lib/pes_pilot.h)
  add_library(pespilot STATIC
    ${CMAKE_BINARY_DIR}/pilot-lib/pes_fn_140A71990.cpp)
  add_dependencies(pespilot pilot_lib_gen)
  target_include_directories(pespilot PUBLIC
    ${CMAKE_SOURCE_DIR}/tools/dev/translate ${CMAKE_BINARY_DIR}/pilot-lib)
  target_compile_definitions(pespilot PUBLIC PES_PILOT_AVAILABLE)
endif()

add_executable(pesfmt_dump apps/pesfmt_dump.cpp)
target_link_libraries(pesfmt_dump PRIVATE pesrig)
set_target_properties(pesfmt_dump PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps)

add_library(godotfootball SHARED
  src/gdext/register_types.cpp src/gdext/pes_library.cpp src/gdext/pes_texture.cpp src/gdext/pes_character.cpp
  src/gdext/pes_animation.cpp src/gdext/pes_marking.cpp src/gdext/gf_match.cpp src/gdext/pes_motion.cpp src/gdext/pes_broadcast_camera.cpp src/gdext/pes_mesh.cpp src/gdext/pes_model.cpp src/gdext/pes_lighting.cpp src/gdext/pes_effects.cpp)
# pesfmt's movestep_run.cpp calls LocomotionStyle from pesmovestepcore (see gfsim_run below). The
# shared library links without complaint when that symbol is missing, and Godot then refuses to
# load the extension, so name pesmovestepcore after pesfmt here too.
target_link_libraries(godotfootball PRIVATE gfsim pesrig godot-cpp pesfmt pesmovestepcore)
set_target_properties(godotfootball PROPERTIES
  PREFIX lib
  OUTPUT_NAME godotfootball
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/godot/bin)

add_subdirectory(src/sim)

add_executable(gfsim_run apps/gfsim_run.cpp)
target_link_libraries(gfsim_run PRIVATE gfsim)
# pesfmt's movestep_run.cpp calls LocomotionStyle, defined in pesmovestepcore (a static cycle):
# name pesmovestepcore again after pesfmt so the linker pulls it in.
target_link_libraries(gfsim_run PRIVATE pesfmt pesmovestepcore)
set_target_properties(gfsim_run PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/apps)

enable_testing()
add_subdirectory(tests)
