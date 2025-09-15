add_executable(xet)

set_target_properties(xet PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

target_sources(xet PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xe_spine.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xe_spine.h
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

add_executable(hello_world)
set_target_properties(hello_world PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
target_sources(hello_world PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hello_world.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xe_nuklear.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xe_nuklear.h
    ${CMAKE_SOURCE_DIR}/extern/include/nuklear.h
)

target_include_directories(hello_world PRIVATE
    ${CMAKE_SOURCE_DIR}/extern/include
    ${CMAKE_SOURCE_DIR}/extern/llulu/include
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(hello_world PRIVATE
    xe
    xe_extern
    glfw
)

