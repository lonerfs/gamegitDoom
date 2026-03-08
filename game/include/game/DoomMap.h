#pragma once

#include <vector>
#include <cstdint>
#include <string>

class WadLoader;

#pragma pack(push, 1)
struct Vertex {
    int16_t x, y;
};

struct Linedef {
    uint16_t startVertex;
    uint16_t endVertex;
    uint16_t flags;
    uint16_t type;
    uint16_t sectorTag;
    uint16_t rightSidedef;   // 0xFFFF если нет
    uint16_t leftSidedef;    // 0xFFFF если нет
};

struct Sidedef {
    int16_t xOffset;
    int16_t yOffset;
    char upperTex[8];
    char lowerTex[8];
    char middleTex[8];
    uint16_t sector;
};

struct Sector {
    int16_t floorHeight;
    int16_t ceilingHeight;
    char floorTex[8];
    char ceilingTex[8];
    uint16_t lightLevel;
    uint16_t special;
    uint16_t tag;
};

struct Thing {
    int16_t x, y;
    int16_t angle;
    int16_t type;
    uint16_t flags;
};
#pragma pack(pop)

class DoomMap {
public:
    bool loadFromWad(const WadLoader& wad, const std::string& mapName);

    const std::vector<Vertex>& getVertices() const { return vertices; }
    const std::vector<Linedef>& getLinedefs() const { return linedefs; }
    const std::vector<Sidedef>& getSidedefs() const { return sidedefs; }
    const std::vector<Sector>& getSectors() const { return sectors; }
    const std::vector<Thing>& getThings() const { return things; }

private:
    std::vector<Vertex> vertices;
    std::vector<Linedef> linedefs;
    std::vector<Sidedef> sidedefs;
    std::vector<Sector> sectors;
    std::vector<Thing> things;
};