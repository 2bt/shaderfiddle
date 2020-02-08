cmake_minimum_required(VERSION 3.7)

project(fiddle)

add_compile_options(-std=c++17 -Wall -g -Og)

file(GLOB SRC
    "src/*.hpp"
    "src/*.cpp"
    )

add_executable(${PROJECT_NAME} ${SRC})

find_package(glm REQUIRED)
find_package(sdl2 REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_search_module(GLEW REQUIRED glew)
pkg_search_module(SDL2IMAGE REQUIRED SDL2_image)

target_include_directories(${PROJECT_NAME} SYSTEM PUBLIC
    ${GLEW_INCLUDE_DIRS}
    ${SDL2_INCLUDE_DIRS}
    ${SDL2IMAGE_INCLUDE_DIRS}
    )


target_link_libraries(${PROJECT_NAME}
    ${GLEW_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${SDL2IMAGE_LIBRARIES}
    uv
    )
