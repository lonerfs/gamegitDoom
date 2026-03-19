cmake_minimum_required(VERSION 3.20)
project(game)

set(CMAKE_CXX_STANDARD 17)

# ------ SDL3 (64-bit) ------
set(SDL3_ARCH_PATH ${CMAKE_SOURCE_DIR}/lib/SDL3-3.4.2/x86_64-w64-mingw32)
list(APPEND CMAKE_PREFIX_PATH ${SDL3_ARCH_PATH})
find_package(SDL3 REQUIRED CONFIG)

# ------ SDL3_ttf (64-bit) ------
set(SDL3_TTF_PATH ${CMAKE_SOURCE_DIR}/lib/SDL3_ttf-3.2.2/x86_64-w64-mingw32)
list(APPEND CMAKE_PREFIX_PATH ${SDL3_TTF_PATH})
find_package(SDL3_ttf REQUIRED CONFIG)

# ------ Основная игра ------
set(SOURCES
        src/DoomMap.cpp
        src/main.cpp
        src/WadLoader.cpp
        src/drawing_maps.cpp
)

include_directories(include)

add_executable(game ${SOURCES})
target_link_libraries(game PRIVATE SDL3::SDL3)

# ------ Инструменты конвертации (пока отключены) ------
# set(TOOL_SOURCES
#         lib/tools/map_converter.cpp
#         lib/tools/addmap.cpp
# )
# add_executable(map_converter ${TOOL_SOURCES})
# target_link_libraries(map_converter PRIVATE)

# ------ Редактор карт ------
set(EDITOR_SOURCES
        lib/tools/editor_main.cpp      # <-- файл лежит в lib/tools!
        src/DoomMap.cpp
        src/WadLoader.cpp
)

add_executable(map_editor WIN32 ${EDITOR_SOURCES})
target_include_directories(map_editor PRIVATE
        ${CMAKE_SOURCE_DIR}/lib/tools       # для MapToWad.h
        ${CMAKE_SOURCE_DIR}/include         # для game/DoomMap.h и game/WadLoader.h
)
target_link_libraries(map_editor PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf)

# Копируем DLL-ки
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
add_custom_command(TARGET map_editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL3_ARCH_PATH}/bin/SDL3.dll
        $<TARGET_FILE_DIR:map_editor>
)
add_custom_command(TARGET map_editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL3_TTF_PATH}/bin/SDL3_ttf.dll
        $<TARGET_FILE_DIR:map_editor>
)

# Папка для карт (для game)
add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:game>/maps
)