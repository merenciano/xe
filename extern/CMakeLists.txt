add_library(orxata_extern OBJECT)
set_target_properties(orxata_extern PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

if (CMAKE_C_COMPILER_ID not strequal "TinyCC")
    include(FetchContent)
    FetchContent_Declare(glfw3 GIT_REPOSITORY https://github.com/glfw/glfw.git GIT_TAG 3.4)

    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    set(GLFW_LIBRARY_TYPE "OBJECT" CACHE STRING "" FORCE)

    FetchContent_MakeAvailable(glfw3)
endif()

target_sources(orxata_extern PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/glad.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/stb_image.c
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lu_time.c
)

target_include_directories(orxata_extern PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
