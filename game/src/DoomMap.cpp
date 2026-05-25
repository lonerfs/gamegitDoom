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

    buildGrid();
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

    buildGrid();
    return !vertices.empty();
}

void DoomMap::buildGrid(int cellSize) {
    if (vertices.empty()) return;
    gridCellSize = cellSize;
    // находим границы карты
    minX = vertices[0].x;
    maxX = vertices[0].x;
    minY = vertices[0].y;
    maxY = vertices[0].y;
    for (const auto& v : vertices) {
        if (v.x < minX) minX = v.x;
        if (v.x > maxX) maxX = v.x;
        if (v.y < minY) minY = v.y;
        if (v.y > maxY) maxY = v.y;
    }
    int extend = gridCellSize;
    minX -= extend; maxX += extend;
    minY -= extend; maxY += extend;

    gridCols = (maxX - minX) / gridCellSize + 2;
    gridRows = (maxY - minY) / gridCellSize + 2;
    cellLines.clear();
    cellLines.resize(gridCols * gridRows);

    for (size_t li = 0; li < linedefs.size(); ++li) {
        const Linedef& ld = linedefs[li];
        if (ld.startVertex >= vertices.size() || ld.endVertex >= vertices.size()) continue;
        const Vertex& v1 = vertices[ld.startVertex];
        const Vertex& v2 = vertices[ld.endVertex];
        int x1 = (v1.x - minX) / gridCellSize;
        int y1 = (v1.y - minY) / gridCellSize;
        int x2 = (v2.x - minX) / gridCellSize;
        int y2 = (v2.y - minY) / gridCellSize;
        if (x1 > x2) std::swap(x1, x2);
        if (y1 > y2) std::swap(y1, y2);
        for (int cx = x1; cx <= x2; ++cx) {
            for (int cy = y1; cy <= y2; ++cy) {
                if (cx >= 0 && cx < gridCols && cy >= 0 && cy < gridRows) {
                    cellLines[cy * gridCols + cx].push_back((int)li);
                }
            }
        }
    }
    std::cout << "Built grid: " << gridCols << "x" << gridRows << " cells, cell size " << gridCellSize << std::endl;
}

const std::vector<int>& DoomMap::getLinesInCell(float x, float y) const {
    int cx = (int)((x - minX) / gridCellSize);
    int cy = (int)((y - minY) / gridCellSize);
    if (cx < 0) cx = 0;
    if (cx >= gridCols) cx = gridCols - 1;
    if (cy < 0) cy = 0;
    if (cy >= gridRows) cy = gridRows - 1;
    return cellLines[cy * gridCols + cx];
}
