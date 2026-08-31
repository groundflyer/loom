include(FetchContent)

cmake_policy(SET CMP0135 OLD)

FetchContent_Declare(
  range-v3
  URL       https://github.com/ericniebler/range-v3/archive/refs/tags/0.12.0.tar.gz
  URL_HASH  MD5=373a795e450fd1c8251df624be784710
)

FetchContent_Declare(
  libfmt
  URL       https://github.com/fmtlib/fmt/archive/refs/tags/12.1.0.tar.gz
  URL_HASH  MD5=92eb6f492e4838e5f024ce5207beafc7
)

FetchContent_Declare(
  fast_float
  URL       https://github.com/fastfloat/fast_float/archive/refs/tags/v7.0.0.tar.gz
  URL_HASH  MD5=b5accc3951d38b22179cf29d9459079a
)

if(GLFW_SRC_DIR)
  FetchContent_Declare(
    glfw
    SOURCE_DIR ${GLFW_SRC_DIR}
  )
else()
  FetchContent_Declare(
    glfw
    URL       https://github.com/glfw/glfw/archive/refs/tags/3.3.8.tar.gz
    URL_HASH  MD5=55d99dc968f4cec01a412562a7cf851c
  )
endif()

if(HWLOC_SRC_DIR)
  FetchContent_Declare(
    hwloc
    SOURCE_DIR ${HWLOC_SRC_DIR}
  )
else()
  FetchContent_Declare(
    hwloc
    URL       https://download.open-mpi.org/release/hwloc/v2.13/hwloc-2.13.0.tar.bz2
    URL       https://github.com/open-mpi/hwloc/releases/download/hwloc-2.13.0/hwloc-2.13.0.tar.bz2
    URL_HASH  MD5=5cb444604b6a80d39d837297c6573577
  )
endif()
