# Copyright (c) 2020-2026 tevador <tevador@gmail.com>
# See LICENSE for licensing information

cmake_minimum_required(VERSION 3.10)

set(HASHWX_VERSION 1)
set(HASHWX_VERSION_MINOR 0)
set(HASHWX_VERSION_PATCH 0)
set(HASHWX_VERSION_STR "${HASHWX_VERSION}.${HASHWX_VERSION_MINOR}.${HASHWX_VERSION_PATCH}")

project(hashwx)

set(hashwx_sources
src/compiler.c
src/compiler_a64.c
src/compiler_wasm.c
src/compiler_x86.c
src/context.c
src/hashwx.c
src/program.c
src/program_exec.c
src/siphash_rng.c
src/virtual_memory.c)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
  message(STATUS "Setting default build type: ${CMAKE_BUILD_TYPE}")
endif()

if (NOT DEFINED EMSCRIPTEN)
  # hashwx.so for dynamic linking
  add_library(hashwx SHARED ${hashwx_sources})
    set_property(TARGET hashwx PROPERTY POSITION_INDEPENDENT_CODE ON)
    set_property(TARGET hashwx PROPERTY PUBLIC_HEADER include/hashwx.h)
  include_directories(hashwx
    include/)
  target_compile_definitions(hashwx PRIVATE HASHWX_SHARED)
  set_target_properties(hashwx PROPERTIES VERSION ${HASHWX_VERSION_STR}
                                          SOVERSION ${HASHWX_VERSION}
                                          C_STANDARD 11)
  # hashwx.a for static linking
  add_library(hashwx_static STATIC ${hashwx_sources})
    set_property(TARGET hashwx_static PROPERTY POSITION_INDEPENDENT_CODE ON)
    target_compile_definitions(hashwx_static PRIVATE HASHWX_STATIC)
    set_target_properties(hashwx_static PROPERTIES OUTPUT_NAME hashwx
                                                   C_STANDARD 11)
  # for make install
  include(GNUInstallDirs)
  install(TARGETS hashwx hashwx_static
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
else()
  # hashwx.wasm to be called from javascript
  add_executable(hashwx ${hashwx_sources})
  include_directories(hashwx
    include/)
  set_target_properties(hashwx PROPERTIES COMPILE_FLAGS ""
                                          LINK_FLAGS "-s MALLOC=emmalloc -s ABORTING_MALLOC=0 -s EXPORTED_FUNCTIONS=['_hashwx_alloc','_hashwx_make','_hashwx_exec','_hashwx_free','_hashwx_seed','_hashwx_registers','_hashwx_memory','_hashwx_module','_hashwx_module_size','_hashwx_exec_begin','_hashwx_exec_final'] --no-entry"
                                          C_STANDARD 11
                                          SUFFIX ".wasm")
endif()

add_executable(hashwx-tests
  src/tests.c)
include_directories(hashwx-tests
  include/)

if (NOT DEFINED EMSCRIPTEN)
  target_compile_definitions(hashwx-tests PRIVATE HASHWX_STATIC)
  set_property(TARGET hashwx-tests PROPERTY C_STANDARD 11)
  target_link_libraries(hashwx-tests
    PRIVATE hashwx_static)
else()
  set_target_properties(hashwx-tests PROPERTIES
    C_STANDARD 11
    LINK_FLAGS "--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/js/hashwx.js --js-library ${CMAKE_CURRENT_SOURCE_DIR}/js/hashwx-em.js")
endif()

if(NOT Threads_FOUND AND UNIX AND NOT APPLE)
  find_package(Threads)
endif()

add_executable(hashwx-bench
  src/bench.c
  src/platform.c
  src/siphash_rng.c)
include_directories(hashwx-bench
  include/)

if (NOT DEFINED EMSCRIPTEN)
  target_compile_definitions(hashwx-bench PRIVATE HASHWX_STATIC)
  set_property(TARGET hashwx-bench PROPERTY C_STANDARD 11)
  target_link_libraries(hashwx-bench
    PRIVATE hashwx_static
    PRIVATE ${CMAKE_THREAD_LIBS_INIT})
else()
  set_target_properties(hashwx-bench PROPERTIES
    C_STANDARD 11
    LINK_FLAGS "--pre-js ${CMAKE_CURRENT_SOURCE_DIR}/js/hashwx.js --js-library ${CMAKE_CURRENT_SOURCE_DIR}/js/hashwx-em.js")
endif()

if (NOT DEFINED EMSCRIPTEN)
  find_library(TESTU01_LIB NAMES libtestu01.a)
  find_library(PROBDIST_LIB NAMES libprobdist.a)
  find_library(MYLIB_LIB NAMES libmylib.a)

  if(TESTU01_LIB)
    add_executable(hashwx-crush
      src/crush.c)
    include_directories(hashwx-crush
      include/)
    target_link_libraries(hashwx-crush
      PRIVATE hashwx_static
      PRIVATE ${TESTU01_LIB}
      PRIVATE ${PROBDIST_LIB}
      PRIVATE ${MYLIB_LIB}
      PRIVATE m)
  endif()
endif()
