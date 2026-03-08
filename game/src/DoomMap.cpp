#include "game/DoomMap.h"
#include "game/WadLoader.h"
#include <iostream>
#include <cstring>

bool DoomMap::loadFromWad(const WadLoader& wad, const std::string& mapName) {
    int mapIndex = -1;
    const auto& lumps = wad.getLumps();
    for (size_t i = 0; i < lumps.size(); ++i) {
        if (strncmp(lumps[i].name, mapName.c_str(), mapName.size()) == 0) {
            if (mapName.size() == 8 || lumps[i].name[mapName.size()] == '\0') {
                mapIndex = static_cast<int>(i);
                break;
            }
        }
    }
    if (mapIndex == -1) {
        std::cerr << "Map " << mapName << " not found in WAD" << std::endl;
        return false;
    }

    // Отладка: выводим первые 20 лумпов после маркера карты
    std::cout << "Lumps after map " << mapName << ":" << std::endl;
    for (int i = mapIndex + 1; i < lumps.size(); ++i) {
        std::string nameStr(lumps[i].name, strnlen(lumps[i].name, 8));
        std::cout << "  " << nameStr << std::endl;
        if (i - mapIndex > 20) break;
    }

    auto findLumpAfter = [&](const std::string& lumpName) -> std::vector<char> {
        for (int i = mapIndex + 1; i < lumps.size(); ++i) {
            if (strncmp(lumps[i].name, lumpName.c_str(), lumpName.size()) == 0) {
                if (lumpName.size() == 8 || lumps[i].name[lumpName.size()] == '\0') {
                    return wad.getLumpData(i);
                }
            }
        }
        return {};
    };

    vertices.clear();
    linedefs.clear();
    sidedefs.clear();
    sectors.clear();
    things.clear();

    auto vertexData = findLumpAfter("VERTEXES");
    if (!vertexData.empty()) {
        vertices.resize(vertexData.size() / sizeof(Vertex));
        memcpy(vertices.data(), vertexData.data(), vertexData.size());
    }

    auto linedefData = findLumpAfter("LINEDEFS");
    if (!linedefData.empty()) {
        linedefs.resize(linedefData.size() / sizeof(Linedef));
        memcpy(linedefs.data(), linedefData.data(), linedefData.size());
    }

    auto sidedefData = findLumpAfter("SIDEDEFS");
    if (!sidedefData.empty()) {
        sidedefs.resize(sidedefData.size() / sizeof(Sidedef));
        memcpy(sidedefs.data(), sidedefData.data(), sidedefData.size());
    }

    auto sectorData = findLumpAfter("SECTORS");
    if (!sectorData.empty()) {
        sectors.resize(sectorData.size() / sizeof(Sector));
        memcpy(sectors.data(), sectorData.data(), sectorData.size());
    }

    auto thingData = findLumpAfter("THINGS");
    if (!thingData.empty()) {
        things.resize(thingData.size() / sizeof(Thing));
        memcpy(things.data(), thingData.data(), thingData.size());
    }

    std::cout << "Loaded map " << mapName << ": "
              << vertices.size() << " vertices, "
              << linedefs.size() << " linedefs, "
              << sidedefs.size() << " sidedefs, "
              << sectors.size() << " sectors, "
              << things.size() << " things" << std::endl;

    return true;
}