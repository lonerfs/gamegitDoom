#include <SDL3/SDL.h>
#include <iostream>
#include "game/WadLoader.h"
#include "game/DoomMap.h"

int main(int argc, char* argv[]) {
    WadLoader wad;
    if (!wad.load("freedoom1.wad")) {
        return 1;
    }

    DoomMap map;
    if (!map.loadFromWad(wad, "E1M1")) {
        return 1;
    }

    std::cout << "Map loaded successfully. Press Enter to exit...";
    std::cin.get();
    return 0;
}