include(FetchContent)

cmake_policy(SET CMP0135 OLD)

FetchContent_Declare(
  range-v3
  URL       https://github.com/ericniebler/range-v3/archive/refs/tags/0.12.0.tar.gz
  URL_HASH  MD5=373a795e450fd1c8251df624be784710
)

FetchContent_Declare(
  libfmt
  URL       https://github.com/fmtlib/fmt/archive/refs/tags/9.1.0.tar.gz
  URL_HASH  MD5=21fac48cae8f3b4a5783ae06b443973a
)

FetchContent_Declare(
  glfw
  URL       https://github.com/glfw/glfw/archive/refs/tags/3.3.8.tar.gz
  URL_HASH  MD5=55d99dc968f4cec01a412562a7cf851c
)

FetchContent_Declare(
  fast_float
  URL       https://github.com/fastfloat/fast_float/archive/refs/tags/v7.0.0.tar.gz
  URL_HASH  MD5=b5accc3951d38b22179cf29d9459079a
)
