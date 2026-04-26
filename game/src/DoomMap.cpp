#include "game/DoomMap.h"
#include "game/WadLoader.h"
#include <iostream>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>

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
        std::cerr << "Map " << mapName << " not found" << std::endl;
        return false;
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

    clear();

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

    std::cout << "Loaded WAD map " << mapName << ": "
              << vertices.size() << " verts, "
              << linedefs.size() << " lines" << std::endl;

    return !vertices.empty();
}

bool DoomMap::loadFromTextFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Cannot open: " << filename << std::endl;
        return false;
    }

    clear();

    std::string line;
    std::string section;

    std::cout << "Loading text map: " << filename << std::endl;

    while (std::getline(file, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
        if (line.empty() || line[0] == ';') continue;

        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        if (line[0] == '[') {
            size_t end = line.find(']');
            if (end != std::string::npos) {
                section = line.substr(1, end - 1);
            }
            continue;
        }

        std::istringstream iss(line);

        if (section == "VERTICES") {
            int x, y;
            if (iss >> x >> y) {
                Vertex v;
                v.x = x; v.y = y;
                vertices.push_back(v);
            }
        }
        else if (section == "LINEDEFS") {
            int a,b,c,d,e,f,g;
            if (iss >> a >> b >> c >> d >> e >> f >> g) {
                Linedef l;
                l.startVertex = a; l.endVertex = b; l.flags = c;
                l.type = d; l.sectorTag = e; l.rightSidedef = f; l.leftSidedef = g;
                linedefs.push_back(l);
            }
        }
        else if (section == "THINGS") {
            int x, y, angle, type, flags;
            if (iss >> x >> y >> angle >> type >> flags) {
                Thing t;
                t.x = x; t.y = y; t.angle = angle; t.type = type; t.flags = flags;
                things.push_back(t);
            }
        }
    }

    // Если нет SIDEDEFS - создаем пустые
    if (sidedefs.empty() && !linedefs.empty()) {
        for (size_t i = 0; i < linedefs.size(); i++) {
            Sidedef sd;
            sd.xOffset = 0;
            sd.yOffset = 0;
            memset(sd.upperTex, 0, 8);
            memset(sd.lowerTex, 0, 8);
            memset(sd.middleTex, 0, 8);
            sd.sector = 0;
            sidedefs.push_back(sd);
            linedefs[i].rightSidedef = i;
        }
        std::cout << "Created " << sidedefs.size() << " empty SIDEDEFS" << std::endl;
    }

    // Если нет SECTORS - создаем один сектор
    if (sectors.empty()) {
        Sector sec;
        sec.floorHeight = 0;
        sec.ceilingHeight = 160;
        memset(sec.floorTex, 0, 8);
        memset(sec.ceilingTex, 0, 8);
        sec.lightLevel = 160;
        sec.special = 0;
        sec.tag = 0;
        sectors.push_back(sec);
        std::cout << "Created default SECTOR" << std::endl;
    }

    std::cout << "Loaded text map: V=" << vertices.size()
              << " L=" << linedefs.size()
              << " S=" << sidedefs.size()
              << " Sec=" << sectors.size()
              << " T=" << things.size() << std::endl;

    return !vertices.empty();
}