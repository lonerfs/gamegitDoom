cmake_minimum_required(VERSION 3.20)
project(game)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Пути к библиотекам SDL3
set(SDL3_BASE "${CMAKE_SOURCE_DIR}/lib/SDL3-3.4.2/x86_64-w64-mingw32")
set(SDL3_TTF_BASE "${CMAKE_SOURCE_DIR}/lib/SDL3_ttf-3.2.2/x86_64-w64-mingw32")
set(SDL3_IMAGE_BASE "${CMAKE_SOURCE_DIR}/lib/SDL3_image-3.4.2/x86_64-w64-mingw32")

include_directories(${SDL3_BASE}/include)
include_directories(${SDL3_TTF_BASE}/include)
include_directories(${SDL3_IMAGE_BASE}/include)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/lib/tools)

file(GLOB_RECURSE GAME_SOURCES
        "src/DoomMap.cpp"
        "src/WadLoader.cpp"
)

# game
add_executable(game ${GAME_SOURCES} src/main.cpp src/RenderThread.cpp)

target_link_libraries(game
        ${SDL3_BASE}/lib/libSDL3.dll.a
        ${SDL3_TTF_BASE}/lib/libSDL3_ttf.dll.a
        ${SDL3_IMAGE_BASE}/lib/libSDL3_image.dll.a
)

add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${SDL3_BASE}/bin/SDL3.dll $<TARGET_FILE_DIR:game>/SDL3.dll
        COMMAND ${CMAKE_COMMAND} -E copy ${SDL3_TTF_BASE}/bin/SDL3_ttf.dll $<TARGET_FILE_DIR:game>/SDL3_ttf.dll
        COMMAND ${CMAKE_COMMAND} -E copy ${SDL3_IMAGE_BASE}/bin/SDL3_image.dll $<TARGET_FILE_DIR:game>/SDL3_image.dll
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/lib/textures $<TARGET_FILE_DIR:game>/textures
)

add_executable(map_editor ${GAME_SOURCES} lib/tools/editor_main.cpp lib/tools/MapToWad.cpp)

target_link_libraries(map_editor
        ${SDL3_BASE}/lib/libSDL3.dll.a
        ${SDL3_TTF_BASE}/lib/libSDL3_ttf.dll.a
        ${SDL3_IMAGE_BASE}/lib/libSDL3_image.dll.a
)

add_custom_command(TARGET map_editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${SDL3_BASE}/bin/SDL3.dll $<TARGET_FILE_DIR:map_editor>/SDL3.dll
        COMMAND ${CMAKE_COMMAND} -E copy ${SDL3_TTF_BASE}/bin/SDL3_ttf.dll $<TARGET_FILE_DIR:map_editor>/SDL3_ttf.dll
        COMMAND ${CMAKE_COMMAND} -E copy ${SDL3_IMAGE_BASE}/bin/SDL3_image.dll $<TARGET_FILE_DIR:map_editor>/SDL3_image.dll
        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/lib/textures $<TARGET_FILE_DIR:map_editor>/textures
)