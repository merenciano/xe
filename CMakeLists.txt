cmake_minimum_required(VERSION 3.13)

set(CMAKE_DEBUG_POSTFIX _d)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)
set(CMAKE_EXPORT_COMPILE_COMMANDS True)

project(Orxata VERSION 0.0.1)

# Lib
add_library(orxata OBJECT)
set_target_properties(orxata PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

target_sources(orxata PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/orxata.c
)

target_include_directories(orxata PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/extern/include
)

if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(orxata PRIVATE
        /W4
    )
else()
    target_compile_options(orxata PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

add_subdirectory(extern)
add_subdirectory(test)
