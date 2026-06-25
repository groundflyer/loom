cmake_minimum_required(VERSION 3.16)

# Using clang++
set(CMAKE_CXX_COMPILER "clang++" CACHE STRING "C++ compiler" FORCE)
set(CMAKE_C_COMPILER "clang" CACHE STRING "C compiler" FORCE)

project(Loom CXX)

# Compiler with C++20 support is required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Default Build Type is Release
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif()
message(STATUS "Build Type: ${CMAKE_BUILD_TYPE}")

# Options
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(BUILD_FM_BENCHMARKS "Build benchmarks with fast-math" OFF)
option(BUILD_DEBUGGERS "Build debuggers" OFF)
option(BUILD_DEMO "Build SmallPT demo" OFF)
option(BUILD_PROF "Build profiling executables" OFF)
option(BUILD_DOC "Build documentation" OFF)
option(ENABLE_IPO "Enable interprocedural optimization" OFF)
option(SIMD_NARROW_AVX_INT "Use 128 bit integer vectors on AVX instead of 256" OFF)
option(USE_LIBCXX "Link to libc++ when compiling with Clang" ON)
option(USE_CUDA "Enable CUDA support" OFF)
option(USE_SYSTEM_RANGEV3 "Use system range-v3 library" ON)
option(USE_SYSTEM_FMT "Use system libfmt" ON)
option(USE_SYSTEM_GLFW "Use system glfw3" ON)
option(USE_SYSTEM_FAST_FLOAT "Use system fast_float" ON)
set(TARGET_CPU "native" CACHE STRING "Target CPU")
set(CUDA_BLOCK_SIZE 256 CACHE STRING "Number of threads in a block")

if(SIMD_NARROW_AVX_INT)
  add_definitions(-DNARROW_AVX_INT)
endif()

# Check for required compiler features
include(CxxFeatures)

# Fetch content
include(ExternalContent)

add_compile_options(-Wall -Wpedantic -Wextra -fno-rtti -ferror-limit=1)
# Optimize for the host machine
add_compile_options($<$<CONFIG:Release>:-march=${TARGET_CPU}>)

# RelWithDebInfo
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
add_compile_options($<$<CONFIG:RelWithDebInfo>:-march=${TARGET_CPU}>)
add_compile_options($<$<CONFIG:RelWithDebInfo>:-O3>)
add_compile_options($<$<CONFIG:RelWithDebInfo>:-g>)

# Link-time optimization
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  include(CheckIPOSupported)
  check_ipo_supported(RESULT HAS_IPO OUTPUT IPO_ERROR LANGUAGES CXX)
  if(HAS_IPO)
    if(ENABLE_IPO)
      message(STATUS "Interprocedural optimization enabled")
      set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
  else()
    message(STATUS "Interprocedural optimization is not supported")
  endif()
endif()

# Threads
set(CMAKE_THREAD_PREFER_PTHREAD 1)
set(THREADS_PREFER_PTHREAD_FLAG 1)
find_package(Threads REQUIRED)

cmake_host_system_information(RESULT NUM_CORES_P QUERY NUMBER_OF_PHYSICAL_CORES)
cmake_host_system_information(RESULT NUM_CORES_L QUERY NUMBER_OF_LOGICAL_CORES)
message(STATUS "Host physical cores count: " ${NUM_CORES_P})
message(STATUS "Host logical cores count: " ${NUM_CORES_L})

# OpenGL
set(OpenGL_GL_PREFERENCE LEGACY)
find_package(OpenGL REQUIRED)

# GLFW3
if(USE_SYSTEM_GLFW)
  find_package(glfw3 3.3 REQUIRED)
  if(glfw3_FOUND)
    message(STATUS "Found glfw3: " ${glfw3_VERSION})
  endif()
else()
  message(STATUS "Fetching glfw3...")
  FetchContent_MakeAvailable(glfw)
endif()
set(GL_LIBS OpenGL::GL glfw)
set(GL_DEFS "-DGLFW_INCLUDE_GLCOREARB -DGLFW_INCLUDE_GLEXT -DGL_GLEXT_PROTOTYPES")
  
# fmt
if(USE_SYSTEM_FMT)
  find_package(fmt 9 REQUIRED)
  if(fmt_FOUND)
    message(STATUS "Found fmt: " ${fmt_VERSION})
  endif()
  include(CheckCXXSourceCompiles)

  set(CMAKE_REQUIRED_FLAGS -std=c++20)
  set(CMAKE_REQUIRED_LIBRARIES fmt::fmt)
  check_cxx_source_compiles("
#include <iostream>
#include <fmt/core.h>
int main(){std::cout << fmt::format(\"{}\",0);}" FmtLibcxx)
else()
  message(STATUS "Fetching fmt...")
  FetchContent_MakeAvailable(libfmt)
endif()

# CUDA
if(USE_CUDA)
  if(NOT ((CMAKE_CXX_COMPILER_ID STREQUAL "Clang") AND USE_LIBCXX))
    message(SEND_ERROR "CUDA is supported only with Clang and libc++")
  endif()
  find_package(CUDAToolkit 8 REQUIRED)
  set(CMAKE_CUDA_COMPILER ${CMAKE_CXX_COMPILER})
  enable_language(CUDA)
  add_compile_definitions($<$<AND:$<COMPILE_LANGUAGE:CUDA>,$<CONFIG:RelWithDebInfo>>:DEBUG>)
  add_compile_definitions($<$<COMPILE_LANGUAGE:CUDA>:CUDA_BLOCK_SIZE=${CUDA_BLOCK_SIZE}>)
endif()

# range-v3
if(USE_SYSTEM_RANGEV3)
  find_package(range-v3 REQUIRED)
else()
  message(STATUS "Fetching range-v3...")
  FetchContent_MakeAvailable(range-v3)
  include_directories(${Range-v3_SOURCE_DIR}/include)
endif()

# fast_float
if(USE_SYSTEM_FAST_FLOAT)
  find_package(FastFloat REQUIRED)
  if(FastFloat_FOUND)
    message(STATUS "Found fast_float: " ${FastFloat_VERSION})
  endif()
else()
  message(STATUS "Fetching fast_float...")
  FetchContent_MakeAvailable(fast_float)
endif()

# hwloc
find_package(PkgConfig REQUIRED QUIET)
pkg_check_modules(HWLOC REQUIRED IMPORTED_TARGET hwloc)

# External includes
include_directories(ext/include)
# Disable static_math warnings
add_compile_options(-Wno-deprecated-literal-operator -Wno-nan-infinity-disabled)

# docs
if(BUILD_DOC)
  find_package(Doxygen REQUIRED dot)
  if(DOXYGEN_FOUND)
    message(STATUS "Building documentation")

    set(_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set(DOC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/doc)
    set(BIB_DIR ${DOC_DIR}/bib)

    set(DOXYGEN_PROJECT_BRIEF "Proof-of-concept VM for SPMD execution")
    set(DOXYGEN_CLANG_ASSISTED_PARSING YES)
    set(DOXYGEN_CLANG_DATABASE_PATH ${CMAKE_CURRENT_BINARY_DIR})
    set(DOXYGEN_FILE_PATTERNS *.cpp *.hpp *.md)
    set(DOXYGEN_FORCE_LOCAL_INCLUDES YES)
    # graphs
    set(DOXYGEN_COLLABORATION_GRAPH YES)
    set(DOXYGEN_TEMPLATE_RELATIONS YES)
    set(DOXYGEN_INCLUDE_GRAPH YES)
    set(DOXYGEN_INCLUDED_BY_GRAPH YES)
    set(DOXYGEN_DIRECTORY_GRAPH YES)
    set(DOXYGEN_GROUP_GRAPHS YES)
    set(DOXYGEN_CALL_GRAPH YES)
    set(DOXYGEN_CALLER_GRAPH YES)

    set(DOXYGEN_EXTRACT_STATIC YES)
    set(DOXYGEN_EXTRACT_LOCAL_CLASSES YES)
    set(DOXYGEN_HTML_DYNAMIC_MENUS YES)
    set(DOXYGEN_HTML_DYNAMIC_SECTIONS YES)
    set(DOXYGEN_EXCLUDE_SYMBOLS "detail")
    set(DOXYGEN_HIDE_UNDOC_CLASSES YES)
    set(DOXYGEN_HIDE_UNDOC_NAMESPACES NO)
    set(DOXYGEN_SHOW_NAMESPACES YES)
    set(DOXYGEN_GENERATE_TREEVIEW YES)
    set(DOXYGEN_VERBATIM_HEADERS NO)
    set(DOXYGEN_GRAPHICAL_HIERARCHY YES)
    set(DOXYGEN_DOT_IMAGE_FORMAT svg)
    set(DOXYGEN_INTERACTIVE_SVG YES)
    set(DOXYGEN_TOC_EXPAND YES)
    set(DOXYGEN_USE_MATHJAX YES)
    set(DOXYGEN_MATHJAX_VERSION MathJax_4)
    set(DOXYGEN_MATHJAX_FORMAT chtml)
    set(DOXYGEN_MATHJAX_RELPATH "../mathjax")
    set(DOXYGEN_CITE_BIB_FILES
      ${BIB_DIR}/Duff2017Basis.bib
      ${BIB_DIR}/other.bib
      ${BIB_DIR}/LD04ERQIT_citation.bib
      ${BIB_DIR}/acm_2063384.2063405.bib
      ${BIB_DIR}/josa-65-5-531.bib)
    doxygen_add_docs(doc ${_SRC_DIR} ${DOC_DIR} ALL
      COMMENT "Generating documentation with Doxygen"
      WORKING_DIRECTORY ${_SRC_DIR})
    file(COPY ${DOC_DIR}/tex-chtml.js DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/mathjax)
  else()
    message(SEND_ERROR "Doxygen not found. Can't generate documentation.")
  endif()
endif()

# Tests
if(BUILD_TESTS)
  message(STATUS "Building tests")
  enable_testing()

  include(CTest)

  set(test_srcs
    argparse.cpp
    functional.cpp
    vm.cpp
    vm_simd.cpp
    vm_root_finding.cpp
    vm_mp.cpp
    vm_simd_compact.cpp
    linal.cpp
    string.cpp
    tmp.cpp
    transforms.cpp
    primitives.cpp
    tuple.cpp
    union.cpp
    test.cpp
    threadpool.cpp
    sort.cpp
    vm_smallpt.cpp
    vm_linal.cpp
  )

  # header-only fmt in case if system fmt is not linked to libc++
  set(fmt_ho
    test_string
    test_linal
    test_vm
    test_vm_simd
    test_vm_root_finding
    test_vm_mp
    test_vm_simd_compact
    test_vm_linal
    test_sort)

  set(TESTS_SRC_DIR src/tests)

  add_custom_target(tests)

  foreach(src ${test_srcs})
    string(REPLACE ".cpp" "" name ${src})
    string(PREPEND name "test_")

    add_executable(${name} ${TESTS_SRC_DIR}/${src})
    add_test(NAME ${name} COMMAND ${name})
    if (USE_SYSTEM_FMT AND (NOT FmtLibcxx) AND (name IN_LIST fmt_ho))
      target_link_libraries(${name} PRIVATE fmt::fmt-header-only)
    else()
      target_link_libraries(${name} PRIVATE fmt::fmt)
    endif()
    target_link_libraries(${name} PUBLIC FastFloat::fast_float)
    target_include_directories(${name} PUBLIC src)
    add_dependencies(tests ${name})
  endforeach()

  target_link_libraries(test_vm_mp PUBLIC Threads::Threads PkgConfig::HWLOC)

  if(USE_CUDA)
    add_executable(test_vm_cuda ${TESTS_SRC_DIR}/vm.cu)
    target_compile_definitions(test_vm_cuda PUBLIC USE_CUDA FMT_USE_FLOAT128=0)
    target_link_libraries(test_vm_cuda PRIVATE CUDA::cudart_static fmt::fmt)
    target_include_directories(test_vm_cuda PUBLIC src)
    add_dependencies(tests test_vm_cuda)
    add_test(NAME test_vm_cuda COMMAND test_vm_cuda)
  endif()

  # target_include_directories(test_viewport PUBLIC ${OPENGL_INCLUDE_DIR})
  # target_link_libraries(test_viewport PUBLIC ${GL_LIBS})
  # target_compile_definitions(test_viewport PUBLIC ${GL_DEFS})
  # target_link_libraries(test_threadpool PUBLIC Threads::Threads)

  # target_compile_options(test_vm_simd_compact PUBLIC -save-temps)
  # target_compile_options(test_vm PUBLIC -save-temps)
  # target_compile_options(test_vm PUBLIC -fverbose-asm)
  target_compile_options(test_vm PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_compile_options(test_vm PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=alignment>)
  target_link_options(test_vm PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_link_options(test_vm PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=alignment>)
  target_compile_options(test_argparse PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_link_options(test_argparse PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_compile_options(test_string PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_link_options(test_string PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_compile_options(test_vm_root_finding PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_link_options(test_vm_root_finding PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_compile_options(test_vm_mp PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=thread>)
  target_link_options(test_vm_mp PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=thread>)
  target_compile_options(test_sort PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_link_options(test_sort PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_compile_options(test_vm_smallpt PUBLIC -fbracket-depth=1024)
endif()

if(BUILD_DEBUGGERS)
  message(STATUS "Building debuggers")
  add_custom_target(dbgs)
  set(DBGS_SRC_DIR src/dbg)
  set(dbg_srcs root_finding.cpp fibonacci.cpp factorial.cpp)

  foreach(src ${dbg_srcs})
    string(REPLACE ".cpp" "" name ${src})
    string(PREPEND name "dbg_")

    add_executable(${name} ${DBGS_SRC_DIR}/${src})
    target_include_directories(${name} PUBLIC src ${DBGS_SRC_DIR})
    target_link_libraries(${name} PUBLIC fmt::fmt FastFloat::fast_float)

    add_dependencies(dbgs ${name})
  endforeach()
endif()

if(BUILD_BENCHMARKS)
  message(STATUS "Building benchmarks")
  find_package(Git)
  if(Git_FOUND)
    execute_process(COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      OUTPUT_VARIABLE _REPO_HASH)
    string(REPLACE "\n" "" REPO_HASH ${_REPO_HASH})
    execute_process(COMMAND ${GIT_EXECUTABLE} diff-index --quiet HEAD
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      RESULT_VARIABLE REPO_HAS_CHANGES)
    if(REPO_HAS_CHANGES)
      string(APPEND REPO_HASH "*")
    endif()
  endif()

  set(BM_SRC_DIR src/benchmarks)

  set(bm_srcs pt.cpp primary.cpp primary2.cpp threefry.cpp)

  add_custom_target(benchmarks)

  foreach(src ${bm_srcs})
    string(REPLACE ".cpp" "" name ${src})
    string(PREPEND name "bm_")

    add_executable(${name} ${BM_SRC_DIR}/${src})
    if (BUILD_FM_BENCHMARKS)
      add_executable(${name}_fm ${BM_SRC_DIR}/${src})
    endif()

    if (USE_SYSTEM_FMT AND (NOT FmtLibcxx))
      target_link_libraries(${name} PRIVATE fmt::fmt-header-only)
      if (BUILD_FM_BENCHMARKS)
        target_link_libraries(${name}_fm PRIVATE fmt::fmt-header-only)
      endif()
    else()
      target_link_libraries(${name} PRIVATE fmt::fmt)
      if (BUILD_FM_BENCHMARKS)
        target_link_libraries(${name}_fm PRIVATE fmt::fmt)
      endif()
    endif()

    target_include_directories(${name} PUBLIC src)
    target_compile_definitions(${name} PRIVATE -DREPO_HASH="${REPO_HASH}")
    target_compile_options(${name} PUBLIC -ftemplate-depth=2048 -fbracket-depth=512 -ftemplate-backtrace-limit=1)
    add_dependencies(benchmarks ${name})

    if (BUILD_FM_BENCHMARKS)
      target_include_directories(${name}_fm PUBLIC src)
      target_compile_definitions(${name}_fm PRIVATE -DREPO_HASH="${REPO_HASH}")
      target_compile_options(${name}_fm PRIVATE $<$<CONFIG:Release>:-ffast-math>)
      target_compile_options(${name}_fm PUBLIC -ftemplate-depth=2048 -fbracket-depth=512 -ftemplate-backtrace-limit=1)
      add_dependencies(benchmarks ${name}_fm)
    endif()
  endforeach()

  # target_compile_options(bm_pt PUBLIC -save-temps)
  target_compile_options(bm_primary PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_link_options(bm_primary PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
endif()

if (BUILD_DEMO)
  message(STATUS "Building demo")
  add_executable(pt_ref src/smallpt/ref/pt.cpp)
  target_link_libraries(pt_ref PUBLIC ${GL_LIBS} Threads::Threads PkgConfig::HWLOC)
  if (USE_SYSTEM_FMT AND (NOT FmtLibcxx))
    target_link_libraries(pt_ref PUBLIC fmt::fmt-header-only)
  else()
    target_link_libraries(pt_ref PUBLIC fmt::fmt)
  endif()
  target_compile_definitions(pt_ref PUBLIC ${GL_DEFS})
  target_include_directories(pt_ref PUBLIC src ${OPENGL_INCLUDE_DIR})
  target_compile_options(pt_ref PUBLIC $<$<CONFIG:Release>:-ffast-math>)
  target_compile_options(pt_ref PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_link_options(pt_ref PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  # target_compile_options(pt_ref PUBLIC -save-temps)

  add_executable(pt_vm src/smallpt/vm/pt.cpp)
  target_link_libraries(pt_vm PUBLIC ${GL_LIBS} Threads::Threads PkgConfig::HWLOC)
  if (USE_SYSTEM_FMT AND (NOT FmtLibcxx))
    target_link_libraries(pt_vm PUBLIC fmt::fmt-header-only)
  else()
    target_link_libraries(pt_vm PUBLIC fmt::fmt)
  endif()
  target_compile_definitions(pt_vm PUBLIC ${GL_DEFS} -DNUM_CORES_L=${NUM_CORES_L} -DNUM_CORES_P=${NUM_CORES_P})
  target_include_directories(pt_vm PUBLIC src ${OPENGL_INCLUDE_DIR})
  target_compile_options(pt_vm PUBLIC -ftemplate-depth=2048 -fbracket-depth=512 -ftemplate-backtrace-limit=1)
  # target_compile_options(pt_vm PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=thread>)
  # target_link_options(pt_vm PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=thread>)
  # target_compile_options(pt_vm PUBLIC $<$<CONFIG:Release>:-ffast-math>)
  # target_compile_options(pt_vm PUBLIC -save-temps)
endif()

if(BUILD_PROF)
  message(STATUS "Building profiling executables")

  set(PROF_SRC_DIR src/prof)
  set(prof_srcs pt_compact.cpp pt_mp_compact.cpp)

  add_custom_target(prof)

  foreach(src ${prof_srcs})
    string(REPLACE ".cpp" "" name ${src})
    string(PREPEND name "prof_")

    add_executable(${name} ${PROF_SRC_DIR}/${src})

    if (USE_SYSTEM_FMT AND (NOT FmtLibcxx))
      target_link_libraries(${name} PRIVATE fmt::fmt-header-only)
    else()
      target_link_libraries(${name} PRIVATE fmt::fmt)
    endif()

    target_include_directories(${name} PUBLIC src)
    target_compile_options(${name} PUBLIC -ftemplate-depth=2048 -fbracket-depth=512 -ftemplate-backtrace-limit=1)
    target_compile_options(${name} PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address -fsanitize=undefined>)
    target_link_options(${name} PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address -fsanitize=undefined>)
    add_dependencies(prof ${name})
  endforeach()

  target_link_libraries(prof_pt_mp_compact PUBLIC Threads::Threads PkgConfig::HWLOC)
endif()
