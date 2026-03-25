#include "game/DoomMap.h"
#include "game/WadLoader.h"
#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>

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

bool DoomMap::loadFromTextFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Cannot open map file: " << filename << std::endl;
        return false;
    }

    vertices.clear();
    linedefs.clear();
    things.clear();
    sidedefs.clear();
    sectors.clear();

    std::string line;
    std::string section;
    int lineNum = 0;
    int vertexCount = 0;
    int linedefCount = 0;

    std::cout << "Loading map from text file: " << filename << std::endl;

    while (std::getline(file, line)) {
        lineNum++;

        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;

        if (line[start] == '#') continue;

        if (line[start] == '[') {
            size_t end = line.find(']', start);
            if (end != std::string::npos) {
                section = line.substr(start + 1, end - start - 1);
                std::cout << "  Section: [" << section << "]" << std::endl;
            }
            continue;
        }

        std::istringstream iss(line);

        if (section == "VERTICES") {
            int x, y;
            if (iss >> x >> y) {
                vertices.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y)});
                vertexCount++;
            } else {
                std::cerr << "  Error parsing VERTICES at line " << lineNum << ": " << line << std::endl;
            }
        }
        else if (section == "LINEDEFS") {
            int start, end;
            if (iss >> start >> end) {
                linedefs.push_back({
                    static_cast<uint16_t>(start),
                    static_cast<uint16_t>(end),
                    0, 0, 0, 0, 0xFFFF
                });
                linedefCount++;
            } else {
                std::cerr << "  Error parsing LINEDEFS at line " << lineNum << ": " << line << std::endl;
            }
        }
        else if (section == "THINGS") {
            int x, y, type;
            if (iss >> x >> y >> type) {
                things.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y), 0, static_cast<int16_t>(type), 0});
            } else {
                std::cerr << "  Error parsing THINGS at line " << lineNum << ": " << line << std::endl;
            }
        }
    }

    std::cout << "Loaded map from " << filename << ":\n"
              << "  Vertices: " << vertexCount << "\n"
              << "  Linedefs: " << linedefCount << "\n"
              << "  Things: " << things.size() << std::endl;

    if (vertices.empty()) {
        std::cerr << "Warning: No vertices loaded!" << std::endl;
    }
    if (linedefs.empty()) {
        std::cerr << "Warning: No linedefs loaded!" << std::endl;
    }

    return true;
}