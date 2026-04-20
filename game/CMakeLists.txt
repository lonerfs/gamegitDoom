cmake_minimum_required(VERSION 3.20)
project(game)

set(CMAKE_CXX_STANDARD 17)

# Пути к SDL3 и SDL3_ttf (при необходимости подставьте свои)
set(SDL3_ARCH_PATH ${CMAKE_SOURCE_DIR}/lib/SDL3-3.4.2/x86_64-w64-mingw32)
list(APPEND CMAKE_PREFIX_PATH ${SDL3_ARCH_PATH})
find_package(SDL3 REQUIRED CONFIG)

set(SDL3_TTF_PATH ${CMAKE_SOURCE_DIR}/lib/SDL3_ttf-3.2.2/x86_64-w64-mingw32)
list(APPEND CMAKE_PREFIX_PATH ${SDL3_TTF_PATH})
find_package(SDL3_ttf REQUIRED CONFIG)

set(SOURCES
        src/main.cpp
        src/WadLoader.cpp
        src/drawing_maps.cpp
        src/DoomMap.cpp
)

include_directories(include)

add_executable(game ${SOURCES})
target_link_libraries(game PRIVATE SDL3::SDL3 SDL3_ttf::SDL3_ttf)

# Копирование SDL3.dll
add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL3_ARCH_PATH}/bin/SDL3.dll
        $<TARGET_FILE_DIR:game>
)

# Копирование SDL3_ttf.dll
add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SDL3_TTF_PATH}/bin/SDL3_ttf.dll
        $<TARGET_FILE_DIR:game>
)

# Копирование WAD-файла
add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/freedoom1.wad
        $<TARGET_FILE_DIR:game>
)

# Создание директории для карт
add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:game>/maps
)

# Копирование шрифта только если он существует (опционально)
if(EXISTS ${CMAKE_SOURCE_DIR}/arial.ttf)
    add_custom_command(TARGET game POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_SOURCE_DIR}/arial.ttf
            $<TARGET_FILE_DIR:game>
    )
endif()