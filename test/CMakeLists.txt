add_executable(xet)

set_target_properties(xet PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

target_sources(xet PRIVATE
        demos/spine-demo/main.c
        demos/spine-demo/xe_spine.c
)

target_include_directories(xet PRIVATE
    demos/spine-demo
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

add_executable(scene_test ${CMAKE_CURRENT_SOURCE_DIR}/test_scene.c)
target_include_directories(scene_test PRIVATE
    ${CMAKE_SOURCE_DIR}/extern/include
    ${CMAKE_SOURCE_DIR}/extern/llulu/include
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(scene_test PRIVATE
    xe
    xe_extern
    glfw
)

add_test(NAME scene_test COMMAND scene_test)
