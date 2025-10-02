add_executable(xet)

set_target_properties(xet PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

target_sources(xet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xe_spine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xe_spine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xe_nuklear.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xe_nuklear.h
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xe_nuklear_incl.h
)

target_include_directories(xet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/extern/include
    ${CMAKE_SOURCE_DIR}/extern/llulu/include
    ${CMAKE_SOURCE_DIR}/extern/spine/spine-c/include
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(xet PRIVATE
    xe
    xe_extern
    glfw
    spine-c
)

add_executable(hello)

set_target_properties(hello PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

target_sources(hello PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hello.c
)

target_include_directories(hello PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/extern/include
    ${CMAKE_SOURCE_DIR}/extern/llulu/include
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(hello PRIVATE
    xe
    xe_extern
    glfw
)

