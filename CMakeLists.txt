cmake_minimum_required(VERSION 3.16)

set(desc "Proof-of-concept virtual machine for SPMD execution")

# Using clang++
set(CMAKE_CXX_COMPILER "clang++" CACHE STRING "C++ compiler" FORCE)
set(CMAKE_C_COMPILER "clang" CACHE STRING "C compiler" FORCE)

project(Loom
  VERSION 1.0.0
  DESCRIPTION ${desc}
  LANGUAGES CXX
)

# Options
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(BUILD_FM_BENCHMARKS "Additionaly build fast-math versions of benchmarks" OFF)
option(BUILD_ALL_BENCHMARKS "Enable optional benchmarks. Requires at least 32Gb RAM" OFF)
option(BUILD_HEAVY_BENCHMARKS "Build heavy benchmarks. Requires at least 32Gb RAM" OFF)
option(BUILD_DEBUGGERS "Build debuggers" OFF)
option(BUILD_DEMO "Build SmallPT demo" ON)
option(BUILD_PROF "Build profiling executables" OFF)
option(BUILD_DOCS "Build documentation" OFF)
option(ENABLE_IPO "Enable interprocedural optimization" OFF)
option(USE_CUDA "Enable CUDA support" OFF)
option(USE_SYSTEM_RANGEV3 "Use system range-v3 library" ON)
option(USE_SYSTEM_FMT "Use system libfmt" ON)
option(USE_SYSTEM_GLFW "Use system glfw3" ON)
option(USE_SYSTEM_FAST_FLOAT "Use system fast_float" ON)
option(USE_SYSTEM_HWLOC "Use system hwloc" ON)
option(BINDIST "Build binary distribution" OFF)
set(TARGET_CPU "native" CACHE STRING "Target CPU")
set(TARGET_CPU_FEATURES "" CACHE STRING "Additional CPU features")
set(CUDA_BLOCK_SIZE 256 CACHE STRING "Number of threads in a block")
set(LIBCXX_DIR "" CACHE STRING "Path to libc++ libraries for binary distribution")
set(GLFW_SRC_DIR "" CACHE STRING "Path to glfw source for binary distribution")
set(HWLOC_SRC_DIR "" CACHE STRING "Pathe to hwloc source for binary distribution")
set(REPO_HASH "" CACHE STRING "Git repository revision")

# compilation
if(BUILD_TESTS OR BUILD_BENCHMARKS OR BUILD_DEBUGGERS OR BUILD_DEMO OR BUILD_PROF)
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
  message(STATUS "Target architecture: ${TARGET_CPU}")

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

  if(TARGET_CPU_FEATURES)
    foreach(feature IN LISTS TARGET_CPU_FEATURES)
      add_compile_options($<$<CONFIG:Release>:-m${feature}>)
      add_compile_options($<$<CONFIG:RelWithDebInfo>:-m${feature}>)
    endforeach()
  endif()

  include_directories(src)

  if(BINDIST)
    set(USE_SYSTEM_GLFW OFF)
    set(USE_SYSTEM_HWLOC OFF)
    add_link_options(-nostdlib++ -L${LIBCXX_DIR})
    link_libraries(-l:libc++.a -l:libc++abi.a)
  endif()

  # Link-time optimization
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT HAS_IPO OUTPUT IPO_ERROR LANGUAGES CXX C)
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

  if(BUILD_DEMO)
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
      set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
      set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
      set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
      set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
      set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
      FetchContent_MakeAvailable(glfw)
    endif()
    set(GL_LIBS OpenGL::GL glfw)
    set(GL_DEFS "-DGLFW_INCLUDE_GLCOREARB -DGLFW_INCLUDE_GLEXT -DGL_GLEXT_PROTOTYPES")
  endif()

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
    if(FmtLibcxx AND NOT BINDIST)
      set(FMT_TARGET fmt::fmt)
    else()
      set(FMT_TARGET fmt::fmt-header-only)
    endif()
  else()
    message(STATUS "Fetching fmt...")
    FetchContent_MakeAvailable(libfmt)
    set(FMT_TARGET fmt::fmt-header-only)
  endif()

  # CUDA
  if(USE_CUDA)
    if(NOT (CMAKE_CXX_COMPILER_ID STREQUAL "Clang"))
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
    set(RANGES_BUILD_CALENDAR_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(RANGES_DEBUG_INFO OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(range-v3)
    include_directories(SYSTEM ${Range-v3_SOURCE_DIR}/include)
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
  if(USE_SYSTEM_HWLOC)
    find_package(PkgConfig REQUIRED QUIET)
    pkg_check_modules(HWLOC REQUIRED IMPORTED_TARGET hwloc)
    set(HWLOC_TARGET PkgConfig::HWLOC)
  else()
    include(ExternalProject)
    cmake_policy(SET CMP0169 OLD)

    message(STATUS "Fetching hwloc...")

    if(NOT HWLOC_SRC_DIR)
      FetchContent_Populate(hwloc)
    else()
      set(hwloc_SOURCE_DIR ${HWLOC_SRC_DIR})
    endif()

    set(HWLOC_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/hwloc-prefix/include")
    set(HWLOC_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/hwloc-prefix/lib/libhwloc.a")

    ExternalProject_Add(hwloc
      SOURCE_DIR ${hwloc_SOURCE_DIR}
      CONFIGURE_COMMAND
      <SOURCE_DIR>/configure
      --prefix=<INSTALL_DIR>
      --enable-static=yes
      --enable-shared=no
      --disable-readme
      --disable-cairo
      --disable-opencl
      --disable-cuda
      --disable-nvml
      --disable-rsmi
      --disable-levelzero
      --disable-gl
      --disable-libudev
      BUILD_COMMAND ${CMAKE_MAKE_PROGRAM}
      INSTALL_COMMAND ${CMAKE_MAKE_PROGRAM} install
      BUILD_BYPRODUCTS ${HWLOC_INCLUDE} ${HWLOC_LIBRARY}
    )

    file(MAKE_DIRECTORY ${HWLOC_INCLUDE})
    add_library(loom::HWLOC STATIC IMPORTED GLOBAL)
    set_target_properties(loom::HWLOC PROPERTIES
      IMPORTED_LOCATION ${HWLOC_LIBRARY}
      INTERFACE_INCLUDE_DIRECTORIES ${HWLOC_INCLUDE}
    )
    add_dependencies(loom::HWLOC hwloc)
    set(HWLOC_TARGET loom::HWLOC)
  endif()

  # External includes
  include_directories(ext/include)
  # Disable static_math warnings
  add_compile_options(-Wno-deprecated-literal-operator -Wno-nan-infinity-disabled -Wno-unused-function)

  add_custom_target(light_targets)
  add_custom_target(heavy_targets)

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
      vm_fp_mix.cpp
    )

    set(TESTS_SRC_DIR src/tests)

    add_custom_target(tests)
    add_dependencies(light_targets tests)

    foreach(src ${test_srcs})
      string(REPLACE ".cpp" "" name ${src})
      string(PREPEND name "test_")

      add_executable(${name} ${TESTS_SRC_DIR}/${src})
      add_test(NAME ${name} COMMAND ${name})
      target_link_libraries(${name} PUBLIC FastFloat::fast_float ${FMT_TARGET})
      add_dependencies(tests ${name})
    endforeach()

    target_link_libraries(test_vm_mp PUBLIC Threads::Threads ${HWLOC_TARGET})
    target_link_libraries(test_threadpool PUBLIC Threads::Threads ${HWLOC_TARGET})

    if(USE_CUDA)
      add_executable(test_vm_cuda ${TESTS_SRC_DIR}/vm.cu)
      target_compile_definitions(test_vm_cuda PUBLIC USE_CUDA FMT_USE_FLOAT128=0)
      target_link_libraries(test_vm_cuda PRIVATE CUDA::cudart_static fmt::fmt)
      add_dependencies(tests test_vm_cuda)
      add_test(NAME test_vm_cuda COMMAND test_vm_cuda)
    endif()

    set(noexcept_tests test_vm test_vm_root_finding test_vm_simd_compact test_vm_smallpt)

    foreach(target ${noexcept_tests})
      target_compile_options(${target} PUBLIC -fno-exceptions)
    endforeach()

    # target_compile_options(test_vm PUBLIC -fverbose-asm)
    target_compile_options(test_vm PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
    target_link_options(test_vm PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
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
    set(dbg_srcs root_finding.cpp fibonacci.cpp factorial.cpp smallpt.cpp)

    foreach(src ${dbg_srcs})
      string(REPLACE ".cpp" "" name ${src})
      string(PREPEND name "dbg_")

      add_executable(${name} ${DBGS_SRC_DIR}/${src})
      target_link_libraries(${name} PUBLIC FastFloat::fast_float ${FMT_TARGET})
      add_dependencies(dbgs ${name})
      install(TARGETS ${name})
    endforeach()

    target_compile_options(dbg_smallpt PRIVATE -ftemplate-depth=2048 -fbracket-depth=1600 -ftemplate-backtrace-limit=1)
  endif()

  if(BUILD_BENCHMARKS)
    message(STATUS "Building benchmarks")

    if(NOT REPO_HASH)
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
          string(APPEND REPO_HASH "-dirty")
        endif()
      else()
        set(REPO_HASH "unknown")
      endif()
    endif()

    set(BM_SRC_DIR src/benchmarks)

    set(bm_srcs threefry.cpp root_finding_bisection.cpp factorial.cpp fibonacci.cpp fp_mix.cpp)

    if(BUILD_ALL_BENCHMARKS)
      message(STATUS "Building optional benchmarks")
    endif()

    if(BUILD_FM_BENCHMARKS)
      message(STATUS "Building fast-math benchmarks")
    endif()

    macro(set_bm_target_flags target opt)
      target_compile_definitions(${target} PRIVATE -DREPO_HASH="${REPO_HASH}")
      target_compile_options(${target} PRIVATE -ftemplate-depth=2048 -fbracket-depth=512 -ftemplate-backtrace-limit=1 -fno-exceptions ${opt})
      target_link_libraries(${target} PRIVATE ${FMT_TARGET} FastFloat::fast_float)
      if(BUILD_ALL_BENCHMARKS)
        target_compile_definitions(${target} PRIVATE -DBENCH_ALL)
      endif()
    endmacro()

    foreach(src ${bm_srcs})
      string(REPLACE ".cpp" "" name ${src})
      string(PREPEND name "bm_")

      add_executable(${name} ${BM_SRC_DIR}/${src})
      set_bm_target_flags(${name} "")
      install(TARGETS ${name})

      if(BUILD_FM_BENCHMARKS)
        set(fname ${name}_fm)
        add_executable(${fname} ${BM_SRC_DIR}/${src})
        set_bm_target_flags(${fname} -ffast-math)
        install(TARGETS ${fname})
      endif()
    endforeach()

    add_custom_target(simd_bms DEPENDS bm_threefry)
    add_custom_target(light_bms DEPENDS bm_root_finding_bisection bm_factorial bm_fibonacci simd_bms)
    add_custom_target(heavy_bms)
    add_dependencies(light_targets light_bms)
    add_dependencies(heavy_targets heavy_bms)

    # target_compile_options(bm_fp_mix PRIVATE -save-temps)

    if(BUILD_HEAVY_BENCHMARKS)
      set(b_values 1 5 20)
      set(t_values 0.001 0.0005 0.0001)
      set(d_values 2 5 20)
      set(pt_kernels pt_const pt2_const)

      set(pt_src ${BM_SRC_DIR}/pt.cpp)
      set(primary_src ${BM_SRC_DIR}/primary.cpp)

      foreach(bval ${b_values})
        set(primary_name "bm_primary_b${bval}")
        add_executable(${primary_name} ${primary_src})
        target_compile_definitions(${primary_name} PRIVATE -DBLUR_VALUE="${bval}")
        set_bm_target_flags(${primary_name} "")
        add_dependencies(heavy_bms ${primary_name})
        install(TARGETS ${primary_name})

        if(BUILD_FM_BENCHMARKS)
          set(fname ${primary_name}_fm)
          add_executable(${fname} ${primary_src})
          set_bm_target_flags(${fname} -ffast-math)
          add_dependencies(heavy_bms ${fname})
          install(TARGETS ${fname})
        endif()

        foreach(ptk ${pt_kernels})
          foreach(tval ${t_values})
            foreach(dval ${d_values})
              set(pt_name "bm_${ptk}_b${bval}_t${tval}_d${dval}")

              add_executable(${pt_name} ${pt_src})
              target_compile_definitions(${pt_name} PRIVATE -DPT_KERNEL=${ptk} -DTHRESHOLD_VALUE="${tval}" -DBLUR_VALUE="${bval}" -DDEPTH_VALUE="${dval}")
              set_bm_target_flags(${pt_name} "")
              add_dependencies(heavy_bms ${pt_name})
              install(TARGETS ${pt_name})

              if(BUILD_FM_BENCHMARKS)
                set(fname ${pt_name}_fm)
                add_executable(${fname} ${pt_src})
                target_compile_definitions(${fname} PRIVATE -DPT_KERNEL=${ptk} -DTHRESHOLD_VALUE="${tval}" -DBLUR_VALUE="${bval}" -DDEPTH_VALUE="${dval}")
                set_bm_target_flags(${fname} -ffast-math)
                add_dependencies(heavy_bms ${fname})
                install(TARGETS ${fname})
              endif()
            endforeach()
          endforeach()
        endforeach()
      endforeach()
    endif()

    add_custom_target(benchmarks DEPENDS light_bms heavy_bms)
    install(PROGRAMS src/scripts/benchmark TYPE BIN)
  endif()

  if(BUILD_DEMO)
    message(STATUS "Building demo")

    add_library(viewport src/smallpt/common/viewport.cpp)
    target_include_directories(viewport PUBLIC ${OPENGL_INCLUDE_DIR})
    target_compile_definitions(viewport PUBLIC ${GL_DEFS})
    target_link_libraries(viewport PUBLIC ${GL_LIBS})

    add_library(pt_ref src/smallpt/ref/ref.cpp)
    target_compile_options(pt_ref PRIVATE -ffast-math -fno-exceptions)
    target_include_directories(pt_ref PUBLIC ${OPENGL_INCLUDE_DIR})
    target_link_libraries(pt_ref PUBLIC FastFloat::fast_float ${HWLOC_TARGET} ${GL_LIBS} ${FMT_TARGET})

    add_library(pt_vm src/smallpt/vm/vm.cpp)
    target_compile_options(pt_vm PRIVATE -fno-exceptions -ftemplate-depth=2048 -fbracket-depth=512 -ftemplate-backtrace-limit=1)
    target_compile_definitions(pt_vm PRIVATE -DNUM_CORES_L=${NUM_CORES_L} -DNUM_CORES_P=${NUM_CORES_P})
    target_link_libraries(pt_vm PUBLIC FastFloat::fast_float ${HWLOC_TARGET} ${GL_LIBS} ${FMT_TARGET})

    add_executable(loom_pt_demo src/smallpt/vm/pt.cpp src/smallpt/vm/events.cpp)
    target_link_libraries(loom_pt_demo PUBLIC viewport pt_vm pt_ref Threads::Threads ${HWLOC_TARGET} ${FMT_TARGET})
    target_compile_options(loom_pt_demo PUBLIC -ftemplate-depth=2048 -fbracket-depth=512 -ftemplate-backtrace-limit=1)
    install(TARGETS loom_pt_demo)
    add_dependencies(heavy_targets loom_pt_demo)
  endif()

  if(BUILD_PROF)
    message(STATUS "Building profiling executables")

    set(PROF_SRC_DIR src/prof)

    add_custom_target(prof)

    add_executable(prof_pt_compact_tbd ${PROF_SRC_DIR}/pt_compact.cpp)
    add_executable(prof_pt_compact_cld ${PROF_SRC_DIR}/pt_compact.cpp)

    target_compile_options(prof_pt_compact_tbd PUBLIC -ftemplate-depth=2048 -fbracket-depth=512 -ftemplate-backtrace-limit=1 -fno-exceptions -save-temps)
    target_link_libraries(prof_pt_compact_tbd PRIVATE ${FMT_TARGET})
    target_compile_options(prof_pt_compact_cld PUBLIC -ftemplate-depth=2048 -fbracket-depth=512 -ftemplate-backtrace-limit=1 -fno-exceptions -save-temps)
    target_link_libraries(prof_pt_compact_cld PRIVATE ${FMT_TARGET})
    target_compile_definitions(prof_pt_compact_cld PRIVATE -DUSE_CLD)

    add_dependencies(prof prof_pt_compact_tbd prof_pt_compact_cld)
    add_dependencies(heavy_targets prof)
  endif()
endif()

# docs
if(BUILD_DOCS)
  find_package(Doxygen REQUIRED dot)
  if(DOXYGEN_FOUND)
    message(STATUS "Building documentation")

    set(_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set(DOC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/doc)
    set(BIB_DIR ${DOC_DIR}/bib)

    set(DOXYGEN_PROJECT_BRIEF ${desc})
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
    set(DOXYGEN_MATHJAX_RELPATH "mathjax")
    set(DOXYGEN_CITE_BIB_FILES
      ${BIB_DIR}/Duff2017Basis.bib
      ${BIB_DIR}/other.bib
      ${BIB_DIR}/LD04ERQIT_citation.bib
      ${BIB_DIR}/acm_2063384.2063405.bib
      ${BIB_DIR}/josa-65-5-531.bib)
    doxygen_add_docs(docs ${_SRC_DIR} ${DOC_DIR} ALL
      COMMENT "Generating documentation with Doxygen"
      WORKING_DIRECTORY ${_SRC_DIR})
    file(COPY ${DOC_DIR}/tex-chtml.js DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/html/mathjax)
  else()
    message(SEND_ERROR "Doxygen not found. Can't generate documentation.")
  endif()
endif()
