#include <iostream>
#include <string>
#include "game/DoomMap.h"
#include "game/WadLoader.h"

// Функции объявлены в drawing_maps.cpp
bool drawMapFromTextFile(const std::string& filename);
bool drawMapFromWad(const std::string& wadFilename, const std::string& mapName);

int main() {
    std::cout << "=====================================\n";
    std::cout << "     DOOM MAP VIEWER\n";
    std::cout << "=====================================\n";
    std::cout << "1 - Draw E1M8 from freedoom1.wad\n";
    std::cout << "2 - Draw mymap.txt\n";
    std::cout << "Choice: ";

    int choice;
    std::cin >> choice;

    bool result = false;

    if (choice == 1) {
        result = drawMapFromWad("freedoom1.wad", "E1M8");
    } else if (choice == 2) {
        result = drawMapFromTextFile("mymap.txt");
    } else {
        std::cerr << "Invalid choice!" << std::endl;
        return 1;
    }

    if (!result) {
        std::cerr << "Failed to draw map!" << std::endl;
        return 1;
    }

    return 0;
}