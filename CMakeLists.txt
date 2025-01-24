cmake_minimum_required(VERSION 3.16)

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
option(BUILD_DOC "Build documentation" OFF)
option(ENABLE_IPO "Enable interprocedural optimization" OFF)
option(SIMD_NARROW_AVX_INT "Use 128 bit integer vectors on AVX instead of 256" ON)
option(USE_LIBCXX "Link to libc++ when compiling with Clang" ON)
option(USE_CUDA "Enable CUDA support" OFF)
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
  cmake_host_system_information(RESULT STREAMER_NUM_THREADS QUERY NUMBER_OF_PHYSICAL_CORES)
  message(STATUS "Static thread pool size: " ${STREAMER_NUM_THREADS} " physical cores")
elseif(THREAD_POOL_SIZE STREQUAL "LOGICAL")
  cmake_host_system_information(RESULT STREAMER_NUM_THREADS QUERY NUMBER_OF_LOGICAL_CORES)
  message(STATUS "Static thread pool size: " ${STREAMER_NUM_THREADS} " logical cores")
elseif(THREAD_POOL_SIZE STREQUAL "DYNAMIC")
  set(STREAMER_NUM_THREADS -1)
  message(STATUS "Dynamic thread pool size")
else()
  set(STREAMER_NUM_THREADS ${THREAD_POOL_SIZE})
  message(STATUS "Static thread pool size: " ${STREAMER_NUM_THREADS})
endif()

# OpenGL
set(OpenGL_GL_PREFERENCE LEGACY)
find_package(OpenGL REQUIRED)

# GLFW3
find_package(glfw3 3.3 REQUIRED)
if(glfw3_FOUND)
  message(STATUS "Found glfw3: " ${glfw3_VERSION})
  set(GL_LIBS OpenGL::GL glfw)
  set(GL_DEFS "-DGLFW_INCLUDE_GLCOREARB -DGLFW_INCLUDE_GLEXT -DGL_GLEXT_PROTOTYPES")
endif()
  
# fmt
find_package(fmt 9 REQUIRED)
if(fmt_FOUND)
  message(STATUS "Found fmt: " ${fmt_VERSION})
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
find_package(range-v3 REQUIRED)

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
    lang.cpp
    linal.cpp
    string.cpp
    tmp.cpp
    transforms.cpp
    tuple.cpp
    union.cpp
    viewport.cpp
    test.cpp
    threadpool.cpp
  )

  # header-only fmt
  set(fmt_ho test_string test_linal)

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

  target_compile_definitions(test_lang PUBLIC NUM_THREADS=${STREAMER_NUM_THREADS})

  if(USE_CUDA)
    add_executable(test_lang_cuda ${TESTS_SRC_DIR}/lang.cu)
    target_compile_definitions(test_lang_cuda PUBLIC USE_CUDA FMT_USE_FLOAT128=0)
    target_link_libraries(test_lang_cuda PRIVATE CUDA::cudart_static fmt::fmt)
    target_include_directories(test_lang_cuda PUBLIC src)
    add_dependencies(tests test_lang_cuda)
    add_test(NAME test_lang_cuda COMMAND test_lang_cuda)
  endif()

  target_include_directories(test_viewport PUBLIC ${OPENGL_INCLUDE_DIR})
  target_link_libraries(test_viewport PUBLIC ${GL_LIBS})
  target_compile_definitions(test_viewport PUBLIC ${GL_DEFS})
  target_compile_options(test_argparse PUBLIC -fno-exceptions)
  target_link_options(test_argparse PUBLIC -fno-exceptions)
  target_link_libraries(test_threadpool PUBLIC Threads::Threads)

  # target_compile_options(test_lang PUBLIC -save-temps)
  # target_compile_options(test_lang PUBLIC -fverbose-asm)
  target_compile_options(test_lang PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_compile_options(test_lang PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=alignment>)
  target_link_options(test_lang PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_link_options(test_lang PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=alignment>)
  target_compile_options(test_argparse PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_link_options(test_argparse PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_compile_options(test_string PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
  target_link_options(test_string PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)

endif()
