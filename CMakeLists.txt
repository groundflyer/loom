cmake_minimum_required(VERSION 3.16)

# Using clang++
set(CMAKE_CXX_COMPILER "clang++" CACHE STRING "C++ compiler" FORCE)
set(CMAKE_C_COMPILER "clang" CACHE STRING "C compiler" FORCE)

project(Lucid CXX)

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
option(USE_DOUBLE_PRECISION "Use double precision floating-point format" OFF)
option(BUILD_TESTS "Build tests" OFF)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(BUILD_DEBUGGERS "Build debuggers" OFF)
option(BUILD_DOC "Build documentation" OFF)
option(ENABLE_IPO "Enable interprocedural optimization" OFF)
option(SIMD_NARROW_AVX_INT "Use 128 bit integer vectors on AVX instead of 256" OFF)
option(USE_LIBCXX "Link to libc++ when compiling with Clang" ON)
option(USE_CUDA "Enable CUDA support" OFF)
option(USE_SYSTEM_RANGEV3 "Use system range-v3 library" ON)
option(USE_SYSTEM_FMT "Use system libfmt" ON)
option(USE_SYSTEM_GLFW "Use system glfw3" ON)
option(USE_SYSTEM_FAST_FLOAT "Use system fast_float" ON)
set(CUDA_BLOCK_SIZE 256 CACHE STRING "Number of threads in a block")
option(THREAD_POOL_AFFINITY "Enable affinity for threads" ON)
set(THREAD_POOL_SIZE PHYSICAL CACHE STRING "Number of threads in CPU thread pool. Set the value to PHYSICAL to use number of physical cores of the host machine, LOGICAL to use number of logical cores of the host machine, DYNAMIC for dynamic size or a positive integer number to set custom size.")

# Set floating point precision
set(_precision "single")
if(USE_DOUBLE_PRECISION)
  set(_precision "double")
  add_definitions(-D_DOUBLE_PRECISION)
endif()
message(STATUS "Floating-point format: ${_precision} precision")

if(SIMD_NARROW_AVX_INT)
  add_definitions(-DNARROW_AVX_INT)
endif()

# Check for required compiler features
include(CxxFeatures)

# Clang-related checks
include(Clang)

# Fetch content
include(ExternalContent)

add_compile_options(-Wall -Wpedantic -Wextra -fno-rtti)
# Optimize for the host machine
add_compile_options($<$<CONFIG:Release>:-march=native>)
add_compile_options($<$<CONFIG:Release>:-ffast-math>)

# RelWithDebInfo
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "")
add_compile_options($<$<CONFIG:RelWithDebInfo>:-march=native>)
add_compile_options($<$<CONFIG:RelWithDebInfo>:-ffast-math>)
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

if(THREAD_POOL_SIZE STREQUAL "PHYSICAL")
  cmake_host_system_information(RESULT NUM_THREADS QUERY NUMBER_OF_PHYSICAL_CORES)
  message(STATUS "Static thread pool size: " ${NUM_THREADS} " physical cores")
elseif(THREAD_POOL_SIZE STREQUAL "LOGICAL")
  cmake_host_system_information(RESULT NUM_THREADS QUERY NUMBER_OF_LOGICAL_CORES)
  message(STATUS "Static thread pool size: " ${NUM_THREADS} " logical cores")
elseif(THREAD_POOL_SIZE STREQUAL "DYNAMIC")
  set(NUM_THREADS -1)
  message(STATUS "Dynamic thread pool size")
else()
  set(NUM_THREADS ${THREAD_POOL_SIZE})
  message(STATUS "Static thread pool size: " ${NUM_THREADS})
endif()

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
else()
  message(STATUS "Fetching fast_float...")
  FetchContent_MakeAvailable(fast_float)
endif()

# External includes
include_directories(ext/include)

# docs
if(BUILD_DOC)
  find_package(Doxygen REQUIRED dot)
  if(DOXYGEN_FOUND)
    message(STATUS "Building documentation")

    set(_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
    set(DOC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/doc)
    set(BIB_DIR ${DOC_DIR}/bib)

    set(DOXYGEN_PROJECT_BRIEF "Experimental path-tracer")
    set(DOXYGEN_CLANG_ASSISTED_PARSING YES)
    set(DOXYGEN_CLANG_DATABASE_PATH ${CMAKE_CURRENT_BINARY_DIR})
    set(DOXYGEN_FILE_PATTERNS *.cpp *.hpp *.md)
    set(DOXYGEN COLLABORATION_GRAPH NO)
    set(DOXYGEN_EXTRACT_LOCAL_CLASSES NO)
    set(DOXYGEN_HTML_DYNAMIC_MENUS YES)
    set(DOXYGEN_HTML_DYNAMIC_SECTIONS YES)
    set(DOXYGEN_EXCLUDE_SYMBOLS "detail")
    set(DOXYGEN_HIDE_UNDOC_CLASSES YES)
    set(DOXYGEN_GENERATE_TREEVIEW YES)
    set(DOXYGEN_INCLUDE_GRAPH NO)
    set(DOXYGEN_INCLUDED_BY_GRAPH NO)
    set(DOXYGEN_VERBATIM_HEADERS NO)
    set(DOXYGEN_DIRECTORY_GRAPH NO)
    set(DOXYGEN_GROUP_GRAPHS NO)
    set(DOXYGEN_DOT_IMAGE_FORMAT svg)
    set(DOXYGEN_INTERACTIVE_SVG YES)
    set(DOXYGEN_TOC_EXPAND YES)
    set(DOXYGEN_USE_MATHJAX YES)
    set(DOXYGEN_CITE_BIB_FILES
      ${BIB_DIR}/Duff2017Basis.bib
      ${BIB_DIR}/other.bib
      ${BIB_DIR}/LD04ERQIT_citation.bib)
    doxygen_add_docs(doc ${_SRC_DIR} ${DOC_DIR} ALL
      COMMENT "Generating documentation with Doxygen"
      WORKING_DIRECTORY ${_SRC_DIR})
  else()
    message(SEND_ERROR "Doxygen not found. Can't generate documentation.")
  endif()
endif()

add_subdirectory(src)

# Tests
if(BUILD_TESTS)
  message(STATUS "Building tests")
  enable_testing()

  include(CTest)

  set(test_srcs
    argparse.cpp
    functional.cpp
    vm.cpp
    vm_root_finding.cpp
    vm_mp.cpp
    vm_embedded_simd.cpp
    linal.cpp
    string.cpp
    tmp.cpp
    transforms.cpp
    primitives.cpp
    tuple.cpp
    union.cpp
    # viewport.cpp
    test.cpp
    threadpool.cpp
    sort.cpp
  )

  # FIXME: my system fmtlib linked against stdlibc++, but we use libc++
  # this is temporary workaround
  # header-only fmt
  set(fmt_ho test_string test_linal test_vm test_vm_root_finding test_vm_mp test_vm_embedded_simd)

  set(TESTS_SRC_DIR src/tests)

  add_custom_target(tests)

  foreach(src ${test_srcs})
    string(REPLACE ".cpp" "" name ${src})
    string(PREPEND name "test_")

    add_executable(${name} ${TESTS_SRC_DIR}/${src})
    add_test(NAME ${name} COMMAND ${name})
    if (name IN_LIST fmt_ho)
      target_link_libraries(${name} PRIVATE fmt::fmt-header-only)
    else()
      target_link_libraries(${name} PRIVATE fmt::fmt)
    endif()
    target_include_directories(${name} PUBLIC src)
    add_dependencies(tests ${name})
  endforeach()

  target_compile_definitions(test_vm_mp PUBLIC NUM_THREADS=${NUM_THREADS})
  target_link_libraries(test_vm_mp PUBLIC Threads::Threads)

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
endif()

if(BUILD_DEBUGGERS)
  add_custom_target(dbgs)
  set(DBGS_SRC_DIR src/vm/dbg)
  set(dbg_srcs root_finding.cpp fibonacci.cpp)

  foreach(src ${dbg_srcs})
    string(REPLACE ".cpp" "" name ${src})
    string(PREPEND name "dbg_")

    add_executable(${name} ${DBGS_SRC_DIR}/${src})
    target_include_directories(${name} PUBLIC src ${DBGS_SRC_DIR})
    target_link_libraries(${name} PUBLIC fmt::fmt FastFloat::fast_float)
    target_compile_definitions(${name} PUBLIC FMT_HEADER_ONLY)

    add_dependencies(dbgs ${name})
  endforeach()
endif()
