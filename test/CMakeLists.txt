add_executable(farto)

set_target_properties(farto PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

target_sources(farto PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.c
)

target_include_directories(farto PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/extern/include
    ${CMAKE_SOURCE_DIR}/extern/llulu/include
    ${CMAKE_SOURCE_DIR}/extern/spine/spine-c/include
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(farto PRIVATE
    orxata
    orxata_extern
    glfw
    spine-c
)
