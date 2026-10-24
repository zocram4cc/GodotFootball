# The GameplayFootball match engine as a headless, deterministic library.
find_package(SQLite3 REQUIRED)
find_package(Boost REQUIRED CONFIG)

file(GLOB_RECURSE GFSIM_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

add_library(gfsim STATIC ${GFSIM_SOURCES})
target_include_directories(gfsim PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_link_libraries(gfsim PUBLIC pesrig pesmovestepcore SQLite3::SQLite3 Boost::headers)
if(TARGET pespilot)
  target_link_libraries(gfsim PUBLIC pespilot)
  target_include_directories(gfsim PUBLIC
    ${CMAKE_SOURCE_DIR}/tools/dev/translate)
endif()

# Release-only link-time optimization for the headless sim: same IEEE FP
# semantics (no fast-math), only cross-TU inlining and codegen. gfsim_run is
# batch CPU-bound, so the whole-program view pays off; the Godot plugin links
# the same archive and inherits it.
include(CheckIPOSupported)
check_ipo_supported(RESULT gfsim_ipo_supported OUTPUT gfsim_ipo_error)
if(gfsim_ipo_supported)
  set_property(TARGET gfsim PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endif()
