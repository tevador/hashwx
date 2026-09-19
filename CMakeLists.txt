# Copyright (c) 2020-2026 tevador <tevador@gmail.com>
# See LICENSE for licensing information

cmake_minimum_required(VERSION 3.15)

set(HASHWX_VERSION 1)
set(HASHWX_VERSION_MINOR 0)
set(HASHWX_VERSION_PATCH 0)
set(HASHWX_VERSION_STR "${HASHWX_VERSION}.${HASHWX_VERSION_MINOR}.${HASHWX_VERSION_PATCH}")

project(hashwx C)

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

include(CheckIncludeFile)
set(CMAKE_C_STANDARD 11)

check_include_file(threads.h HAVE_THREADS_H)

if (MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -pedantic)
endif()

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
    PUBLIC include/)
  target_compile_definitions(hashwx PRIVATE HASHWX_SHARED)
  set_target_properties(hashwx PROPERTIES VERSION ${HASHWX_VERSION_STR}
                                          SOVERSION ${HASHWX_VERSION})
  # hashwx.a for static linking
  add_library(hashwx_static STATIC ${hashwx_sources})
    set_property(TARGET hashwx_static PROPERTY POSITION_INDEPENDENT_CODE ON)
    target_compile_definitions(hashwx_static PUBLIC HASHWX_STATIC)
    set_target_properties(hashwx_static PROPERTIES OUTPUT_NAME hashwx)
  # for make install
  include(GNUInstallDirs)
  install(TARGETS hashwx hashwx_static
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
else()
  # hashwx.wasm to be called from javascript
  # only WebAssembly 1.0 (MVP) features are allowed
  add_executable(hashwx ${hashwx_sources})
  include_directories(hashwx
    include/)
  set_target_properties(hashwx PROPERTIES COMPILE_FLAGS "-mno-bulk-memory -mno-sign-ext"
                                          LINK_FLAGS "-mno-bulk-memory -mno-sign-ext -s MALLOC=emmalloc -s ABORTING_MALLOC=0 -s EXPORTED_FUNCTIONS=['_hashwx_alloc','_hashwx_make','_hashwx_exec','_hashwx_free','_hashwx_seed','_hashwx_registers','_hashwx_memory','_hashwx_module','_hashwx_module_size','_hashwx_exec_begin','_hashwx_exec_final'] --no-entry"
                                          SUFFIX ".wasm")
  # try to find wasm-opt
  get_filename_component(HASHWX_EMSCRIPTEN_DIR "${CMAKE_C_COMPILER}" DIRECTORY)
  find_program(HASHWX_EM_CONFIG NAMES em-config em-config.bat em-config.py
    PATHS "${HASHWX_EMSCRIPTEN_DIR}" NO_DEFAULT_PATH)
  if (NOT HASHWX_EM_CONFIG)
    message(FATAL_ERROR "Could not find em-config next to ${CMAKE_C_COMPILER}")
  endif()
  execute_process(
    COMMAND "${HASHWX_EM_CONFIG}" BINARYEN_ROOT
    OUTPUT_VARIABLE HASHWX_BINARYEN_ROOT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE HASHWX_EM_CONFIG_RESULT)
  if (NOT HASHWX_EM_CONFIG_RESULT EQUAL 0)
    message(FATAL_ERROR "'${HASHWX_EM_CONFIG} BINARYEN_ROOT' failed")
  endif()
  find_program(HASHWX_WASM_OPT NAMES wasm-opt
    PATHS "${HASHWX_BINARYEN_ROOT}/bin" NO_DEFAULT_PATH)
  if (NOT HASHWX_WASM_OPT)
    message(FATAL_ERROR "Could not find wasm-opt in ${HASHWX_BINARYEN_ROOT}/bin")
  endif()
  # lower any bulk-memory/sign-ext instructions
  add_custom_command(TARGET hashwx POST_BUILD
    COMMAND "${HASHWX_WASM_OPT}" "$<TARGET_FILE:hashwx>"
            --enable-bulk-memory --enable-bulk-memory-opt --enable-sign-ext
            --llvm-memory-copy-fill-lowering --signext-lowering
            -o "$<TARGET_FILE:hashwx>"
    COMMENT "Lowering hashwx.wasm to WebAssembly 1.0 (MVP)"
    VERBATIM)
endif()

add_executable(hashwx-tests
  src/tests.c)
include_directories(hashwx-tests
  include/)

if (NOT DEFINED EMSCRIPTEN)
  target_compile_definitions(hashwx-tests PRIVATE HASHWX_STATIC)
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
  if (HAVE_THREADS_H)
    target_compile_definitions(hashwx-bench PRIVATE HASHWX_THREADS)
  endif()
  target_link_libraries(hashwx-bench
    PRIVATE hashwx_static
    PRIVATE ${CMAKE_THREAD_LIBS_INIT})
else()
  if (HAVE_THREADS_H)
    target_compile_definitions(hashwx-bench PRIVATE HASHWX_THREADS)
    set_target_properties(hashwx-bench PROPERTIES COMPILE_FLAGS "-pthread")
  endif()
  set_target_properties(hashwx-bench PROPERTIES
    LINK_FLAGS "-pthread -s USE_PTHREADS=1 --pre-js ${CMAKE_CURRENT_SOURCE_DIR}/js/hashwx.js --js-library ${CMAKE_CURRENT_SOURCE_DIR}/js/hashwx-em.js")
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
