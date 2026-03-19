cmake_minimum_required(VERSION 3.20)
project(game)

set(CMAKE_CXX_STANDARD 17)

set(SDL3_ARCH_PATH ${CMAKE_SOURCE_DIR}/lib/SDL3-3.4.2/x86_64-w64-mingw32)
list(APPEND CMAKE_PREFIX_PATH ${SDL3_ARCH_PATH})
find_package(SDL3 REQUIRED CONFIG)

set(SOURCES
        src/DoomMap.cpp
        src/main.cpp
        src/WadLoader.cpp
        src/drawing_maps.cpp
)

set(TOOL_SOURCES
        tools/map_converter.cpp
        tools/addmap.cpp
)

include_directories(include)

add_executable(game ${SOURCES})
target_link_libraries(game PRIVATE SDL3::SDL3)

add_executable(map_converter ${TOOL_SOURCES})
target_link_libraries(map_converter PRIVATE)


add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL3_ARCH_PATH}/bin/SDL3.dll
        $<TARGET_FILE_DIR:game>
)

add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/freedoom1.wad
        $<TARGET_FILE_DIR:game>
)

add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:game>/maps
)