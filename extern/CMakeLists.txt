add_library(xe_extern OBJECT
        llulu/include/llulu/lu_log.h)
set_target_properties(xe_extern PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

if (NOT ${CMAKE_C_COMPILER_ID} STREQUAL "TinyCC")
    include(FetchContent)
    FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4)

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    set(GLFW_LIBRARY_TYPE "STATIC" CACHE STRING "" FORCE)

    FetchContent_MakeAvailable(glfw)
endif()

target_sources(xe_extern PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/glad.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stb_image.c
    ${CMAKE_CURRENT_SOURCE_DIR}/llulu/src/lu_time.c
    ${CMAKE_CURRENT_SOURCE_DIR}/llulu/src/lu_error.c
    ${CMAKE_CURRENT_SOURCE_DIR}/llulu/src/lu_log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/llulu/src/lu_hooks.c
)

target_include_directories(xe_extern PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/llulu/include
)

add_subdirectory(spine)
