#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include "game/DoomMap.h"
#include "game/WadLoader.h"

bool drawMapFromWad(const std::string& wadFilename, const std::string& mapName);

std::vector<std::string> getMapNames(const WadLoader& wad) {
    std::vector<std::string> maps;
    const auto& lumps = wad.getLumps();
    for (const auto& lump : lumps) {
        std::string name(lump.name, 8);
        name.erase(std::find(name.begin(), name.end(), '\0'), name.end());
        while (!name.empty() && name.back() == ' ') name.pop_back();

        if ((name.size() >= 4 && name[0] == 'E' && isdigit(name[1]) && name[2] == 'M' && isdigit(name[3])) ||
            (name.size() >= 5 && name.substr(0,3) == "MAP" && isdigit(name[3]) && isdigit(name[4]))) {
            maps.push_back(name);
        }
    }
    return maps;
}

void showMapList(const std::vector<std::string>& maps) {
    std::cout << "\nAvailable maps in freedoom1.wad:\n";
    std::cout << "-------------------------------------\n";
    for (size_t i = 0; i < maps.size(); ++i) {
        std::cout << "  " << i+1 << ". " << maps[i] << "\n";
        if ((i+1) % 10 == 0 && i+1 < maps.size())
            std::cout << "-------------------------------------\n";
    }
    std::cout << "-------------------------------------\n";
}

int main() {
    std::cout << "=====================================\n";
    std::cout << "        DOOM MAP VIEWER\n";
    std::cout << "=====================================\n";
    std::cout << "1 - Draw map from freedoom1.wad\n";
    std::cout << "Choice: ";

    int choice;
    std::cin >> choice;

    if (choice != 1) {
        std::cerr << "Invalid choice. Only option 1 is available.\n";
        return 1;
    }

    WadLoader wad;
    if (!wad.load("freedoom1.wad")) {
        std::cerr << "Failed to load freedoom1.wad\n";
        return 1;
    }

    auto maps = getMapNames(wad);
    if (maps.empty()) {
        std::cerr << "No maps found in freedoom1.wad!\n";
        return 1;
    }

    showMapList(maps);

    std::cout << "Select map (1-" << maps.size() << "): ";
    int mapChoice;
    std::cin >> mapChoice;
    if (mapChoice < 1 || mapChoice > static_cast<int>(maps.size())) {
        std::cerr << "Invalid selection.\n";
        return 1;
    }

    std::cout << "Loading map: " << maps[mapChoice-1] << std::endl;
    bool result = drawMapFromWad("freedoom1.wad", maps[mapChoice-1]);

    if (!result) {
        std::cerr << "Failed to draw map!\n";
        return 1;
    }

    return 0;
}