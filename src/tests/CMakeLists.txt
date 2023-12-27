include(CTest)

set(test_srcs
  argparser.cpp
  functional.cpp
  lang.cpp
  linal.cpp
  string.cpp
  tmp.cpp
  transforms.cpp
  tuple.cpp
  union.cpp
  viewport.cpp
)

add_custom_target(tests)

foreach(src ${test_srcs})
  string(REPLACE ".cpp" "" name ${src})
  string(PREPEND name "test_")

  add_executable(${name} ${src})
  add_test(NAME ${name} COMMAND ${CMAKE_CURRENT_BINARY_DIR}/${name} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  target_link_libraries(${name} PRIVATE ${FMTLIB})
  add_dependencies(tests ${name})
endforeach()

target_include_directories(test_viewport PUBLIC ${OPENGL_INCLUDE_DIR})
target_link_libraries(test_viewport PUBLIC ${GL_LIBS})
target_compile_definitions(test_viewport PUBLIC ${GL_DEFS})

target_compile_options(test_lang PUBLIC $<$<CONFIG:RelWithDebInfo>:-save-temps>)
target_compile_options(test_lang PUBLIC $<$<CONFIG:RelWithDebInfo>:-fverbose-asm>)
target_compile_options(test_lang PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
target_compile_options(test_lang PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=alignment>)
target_link_options(test_lang PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=address>)
target_link_options(test_lang PUBLIC $<$<CONFIG:RelWithDebInfo>:-fsanitize=alignment>)
