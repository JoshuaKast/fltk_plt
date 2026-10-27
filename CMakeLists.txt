cmake_minimum_required(VERSION 3.15)
project(fltk_plot VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(FL_PLOT_BUILD_EXAMPLES "Build the example programs" ON)
option(FL_PLOT_BUILD_TESTS "Build the tests" ON)
option(FL_PLOT_FETCH_FLTK "Download and build FLTK if it is not found" OFF)

# FLTK 1.4: point CMAKE_PREFIX_PATH (or FLTK_DIR) at an installed FLTK,
# or use -DFL_PLOT_FETCH_FLTK=ON.
find_package(FLTK 1.4 CONFIG QUIET)
if(NOT FLTK_FOUND)
  if(FL_PLOT_FETCH_FLTK)
    include(FetchContent)
    set(FLTK_BUILD_TEST OFF CACHE BOOL "" FORCE)
    set(FLTK_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(FLTK_BUILD_FLUID OFF CACHE BOOL "" FORCE)
    set(FLTK_BUILD_FLTK_OPTIONS OFF CACHE BOOL "" FORCE)
    set(FLTK_BUILD_GL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(fltk
      GIT_REPOSITORY https://github.com/fltk/fltk.git
      GIT_TAG release-1.4.4
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(fltk)
  else()
    message(FATAL_ERROR "FLTK 1.4 not found. Set CMAKE_PREFIX_PATH or use -DFL_PLOT_FETCH_FLTK=ON")
  endif()
endif()

add_library(fltk_plot
  src/Fl_Plot.cxx
  src/Fl_Plot_Items.cxx
  src/Fl_Plot_Text.cxx
  src/Fl_Plot_Algo.cxx
  src/Fl_Plot_Window.cxx)
target_include_directories(fltk_plot PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
# fltk::images provides fl_write_png()
target_link_libraries(fltk_plot PUBLIC fltk::fltk fltk::images)
if(MSVC)
  target_compile_options(fltk_plot PRIVATE /utf-8)   # UTF-8 symbol literals
else()
  target_compile_options(fltk_plot PRIVATE -Wall -Wextra)
endif()

if(FL_PLOT_BUILD_EXAMPLES)
  foreach(ex demo bigdata custom_bindings plot_window)
    add_executable(${ex} examples/${ex}.cxx)
    target_link_libraries(${ex} PRIVATE fltk_plot)
  endforeach()
endif()

if(FL_PLOT_BUILD_TESTS)
  enable_testing()
  # algorithms only, no display needed
  add_executable(test_algo tests/test_algo.cxx src/Fl_Plot_Algo.cxx)
  target_include_directories(test_algo PRIVATE include)
  add_test(NAME algo COMMAND test_algo)
  # renders offscreen; needs a display (use xvfb-run on headless Linux)
  add_executable(test_render tests/test_render.cxx)
  target_link_libraries(test_render PRIVATE fltk_plot)
  find_program(XVFB_RUN xvfb-run)
  if(XVFB_RUN)
    add_test(NAME render COMMAND ${XVFB_RUN} -a -s "-screen 0 1400x1000x24"
             $<TARGET_FILE:test_render> ${CMAKE_CURRENT_BINARY_DIR})
  endif()
endif()

install(TARGETS fltk_plot ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
install(FILES include/Fl_Plot.H include/Fl_Plot_Algo.H include/Fl_Plot_Text.H
              include/Fl_Plot_Window.H
        DESTINATION include)
