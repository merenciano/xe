cmake_minimum_required(VERSION 3.13)

set(CMAKE_DEBUG_POSTFIX _d)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED true)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)

project(Xe VERSION 0.0.1)

add_library(xe OBJECT)
set_target_properties(xe PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

target_sources(xe PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xe_scene.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xe_platform.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/xe_renderer.c
)

target_include_directories(xe PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/include
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/llulu/include
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/spine/spine-c/include
)

target_compile_options(xe PRIVATE
    -Wall
)

add_compile_definitions(
    $<$<CONFIG:Debug>:XE_DEBUG>
)

if (WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

add_subdirectory(extern)
add_subdirectory(test)
