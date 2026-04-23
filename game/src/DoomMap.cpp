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
<<<<<<< Updated upstream
    int sidedefCount = 0;
    int sectorCount = 0;
=======
>>>>>>> Stashed changes

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
<<<<<<< Updated upstream
            int start, end, flags, type, sectorTag, rightSidedef, leftSidedef;
            if (iss >> start >> end >> flags >> type >> sectorTag >> rightSidedef >> leftSidedef) {
                linedefs.push_back({
                    static_cast<uint16_t>(start),
                    static_cast<uint16_t>(end),
                    static_cast<uint16_t>(flags),
                    static_cast<uint16_t>(type),
                    static_cast<uint16_t>(sectorTag),
                    static_cast<uint16_t>(rightSidedef),
                    static_cast<uint16_t>(leftSidedef)
=======
            int start, end;
            if (iss >> start >> end) {
                linedefs.push_back({
                    static_cast<uint16_t>(start),
                    static_cast<uint16_t>(end),
                    0, 0, 0, 0, 0xFFFF
>>>>>>> Stashed changes
                });
                linedefCount++;
            } else {
                std::cerr << "  Error parsing LINEDEFS at line " << lineNum << ": " << line << std::endl;
            }
        }
<<<<<<< Updated upstream
        else if (section == "SIDEDEFS") {
            int xOffset, yOffset, sector;
            std::string upperTex, lowerTex, middleTex;
            if (iss >> xOffset >> yOffset >> upperTex >> lowerTex >> middleTex >> sector) {
                Sidedef sd;
                sd.xOffset = static_cast<int16_t>(xOffset);
                sd.yOffset = static_cast<int16_t>(yOffset);
                strncpy(sd.upperTex, upperTex.c_str(), 8);
                sd.upperTex[7] = '\0';
                strncpy(sd.lowerTex, lowerTex.c_str(), 8);
                sd.lowerTex[7] = '\0';
                strncpy(sd.middleTex, middleTex.c_str(), 8);
                sd.middleTex[7] = '\0';
                sd.sector = static_cast<uint16_t>(sector);
                sidedefs.push_back(sd);
                sidedefCount++;
            } else {
                std::cerr << "  Error parsing SIDEDEFS at line " << lineNum << ": " << line << std::endl;
            }
        }
        else if (section == "SECTORS") {
            int floorHeight, ceilingHeight, lightLevel, special, tag;
            std::string floorTex, ceilingTex;
            if (iss >> floorHeight >> ceilingHeight >> floorTex >> ceilingTex >> lightLevel >> special >> tag) {
                Sector s;
                s.floorHeight = static_cast<int16_t>(floorHeight);
                s.ceilingHeight = static_cast<int16_t>(ceilingHeight);
                strncpy(s.floorTex, floorTex.c_str(), 8);
                s.floorTex[7] = '\0';
                strncpy(s.ceilingTex, ceilingTex.c_str(), 8);
                s.ceilingTex[7] = '\0';
                s.lightLevel = static_cast<uint16_t>(lightLevel);
                s.special = static_cast<uint16_t>(special);
                s.tag = static_cast<uint16_t>(tag);
                sectors.push_back(s);
                sectorCount++;
            } else {
                std::cerr << "  Error parsing SECTORS at line " << lineNum << ": " << line << std::endl;
            }
        }
        else if (section == "THINGS") {
            int x, y, angle, type, flags;
            if (iss >> x >> y >> angle >> type >> flags) {
                things.push_back({
                    static_cast<int16_t>(x),
                    static_cast<int16_t>(y),
                    static_cast<int16_t>(angle),
                    static_cast<int16_t>(type),
                    static_cast<uint16_t>(flags)
                });
=======
        else if (section == "THINGS") {
            int x, y, type;
            if (iss >> x >> y >> type) {
                things.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y), 0, static_cast<int16_t>(type), 0});
>>>>>>> Stashed changes
            } else {
                std::cerr << "  Error parsing THINGS at line " << lineNum << ": " << line << std::endl;
            }
        }
    }

    std::cout << "Loaded map from " << filename << ":\n"
              << "  Vertices: " << vertexCount << "\n"
              << "  Linedefs: " << linedefCount << "\n"
<<<<<<< Updated upstream
              << "  Sidedefs: " << sidedefCount << "\n"
              << "  Sectors: " << sectorCount << "\n"
=======
>>>>>>> Stashed changes
              << "  Things: " << things.size() << std::endl;

    if (vertices.empty()) {
        std::cerr << "Warning: No vertices loaded!" << std::endl;
    }
    if (linedefs.empty()) {
        std::cerr << "Warning: No linedefs loaded!" << std::endl;
    }

    return true;
}