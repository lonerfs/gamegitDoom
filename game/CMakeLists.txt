cmake_minimum_required(VERSION 3.20)
project(game)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDL3_BASE "${CMAKE_SOURCE_DIR}/lib/SDL3-3.4.2/x86_64-w64-mingw32")
set(SDL3_TTF_BASE "${CMAKE_SOURCE_DIR}/lib/SDL3_ttf-3.2.2/x86_64-w64-mingw32")
set(SDL3_IMAGE_BASE "${CMAKE_SOURCE_DIR}/lib/SDL3_image-3.4.2/x86_64-w64-mingw32")

include_directories(${SDL3_BASE}/include)
include_directories(${SDL3_TTF_BASE}/include)
include_directories(${SDL3_IMAGE_BASE}/include)
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(${CMAKE_SOURCE_DIR}/lib/tools)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(game
        src/main.cpp
        src/RenderThread.cpp
        src/DoomMap.cpp
        src/WadLoader.cpp
        include/game/Mob.h
)

target_link_libraries(game
        ${SDL3_BASE}/lib/libSDL3.dll.a
        ${SDL3_TTF_BASE}/lib/libSDL3_ttf.dll.a
        ${SDL3_IMAGE_BASE}/lib/libSDL3_image.dll.a
        pthread
)

add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${SDL3_BASE}/bin/SDL3.dll $<TARGET_FILE_DIR:game>/SDL3.dll
        COMMAND ${CMAKE_COMMAND} -E copy ${SDL3_TTF_BASE}/bin/SDL3_ttf.dll $<TARGET_FILE_DIR:game>/SDL3_ttf.dll
        COMMAND ${CMAKE_COMMAND} -E copy ${SDL3_IMAGE_BASE}/bin/SDL3_image.dll $<TARGET_FILE_DIR:game>/SDL3_image.dll
)

add_custom_command(TARGET game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/textures/stena.png $<TARGET_FILE_DIR:game>/stena.png
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/textures/mob_walking.png $<TARGET_FILE_DIR:game>/mob_walking.png
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/textures/mob_attack.png $<TARGET_FILE_DIR:game>/mob_attack.png
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/textures/mob_death.png $<TARGET_FILE_DIR:game>/mob_death.png
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/textures/boss.png $<TARGET_FILE_DIR:game>/boss.png
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/textures/boss_attack.png $<TARGET_FILE_DIR:game>/boss_attack.png
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/textures/boss_death.png $<TARGET_FILE_DIR:game>/boss_death.png
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/textures/gun.png $<TARGET_FILE_DIR:game>/gun.png
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/textures/shooting_gun.png $<TARGET_FILE_DIR:game>/shooting_gun.png
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/textures/shotgun.png $<TARGET_FILE_DIR:game>/shotgun.png
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/textures/shotgun_shot.png $<TARGET_FILE_DIR:game>/shotgun_shot.png
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/textures/firstaidkit.png $<TARGET_FILE_DIR:game>/firstaidkit.png
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/textures/pistol_cartridges.png $<TARGET_FILE_DIR:game>/pistol_cartridges.png
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/lib/textures/shotgun_cartridges.png $<TARGET_FILE_DIR:game>/shotgun_cartridges.png
)

add_executable(map_editor
        lib/tools/editor_main.cpp
        lib/tools/MapToWad.cpp
        src/DoomMap.cpp
        src/WadLoader.cpp
)

target_link_libraries(map_editor
        ${SDL3_BASE}/lib/libSDL3.dll.a
        ${SDL3_TTF_BASE}/lib/libSDL3_ttf.dll.a
        ${SDL3_IMAGE_BASE}/lib/libSDL3_image.dll.a
        pthread
)

add_custom_command(TARGET map_editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${SDL3_BASE}/bin/SDL3.dll $<TARGET_FILE_DIR:map_editor>/SDL3.dll
        COMMAND ${CMAKE_COMMAND} -E copy ${SDL3_TTF_BASE}/bin/SDL3_ttf.dll $<TARGET_FILE_DIR:map_editor>/SDL3_ttf.dll
        COMMAND ${CMAKE_COMMAND} -E copy ${SDL3_IMAGE_BASE}/bin/SDL3_image.dll $<TARGET_FILE_DIR:map_editor>/SDL3_image.dll
)