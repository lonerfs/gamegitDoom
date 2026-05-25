#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <algorithm>

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
    uint16_t rightSidedef;
    uint16_t leftSidedef;
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
    bool loadFromTextFile(const std::string& filename);

    const std::vector<Vertex>& getVertices() const { return vertices; }
    const std::vector<Linedef>& getLinedefs() const { return linedefs; }
    const std::vector<Sidedef>& getSidedefs() const { return sidedefs; }
    const std::vector<Sector>& getSectors() const { return sectors; }
    const std::vector<Thing>& getThings() const { return things; }

    void setVertices(const std::vector<Vertex>& verts) { vertices = verts; }
    void setLinedefs(const std::vector<Linedef>& lines) { linedefs = lines; }
    void setSidedefs(const std::vector<Sidedef>& sides) { sidedefs = sides; }
    void setSectors(const std::vector<Sector>& secs) { sectors = secs; }
    void setThings(const std::vector<Thing>& thgs) { things = thgs; }

    void clear() {
        vertices.clear();
        linedefs.clear();
        sidedefs.clear();
        sectors.clear();
        things.clear();
        cellLines.clear();
        minX = minY = maxX = maxY = 0;
    }

    // Пространственная оптимизация: сетка
    void buildGrid(int gridCellSize = 256);
    const std::vector<int>& getLinesInCell(float x, float y) const;
    int getMinX() const { return minX; }
    int getMaxX() const { return maxX; }
    int getMinY() const { return minY; }
    int getMaxY() const { return maxY; }
    int getGridCols() const { return gridCols; }
    int getGridRows() const { return gridRows; }
    int getGridCellSize() const { return gridCellSize; }

private:
    std::vector<Vertex> vertices;
    std::vector<Linedef> linedefs;
    std::vector<Sidedef> sidedefs;
    std::vector<Sector> sectors;
    std::vector<Thing> things;

    // для сетки
    int gridCellSize = 256;
    int minX = 0, maxX = 0, minY = 0, maxY = 0;
    int gridCols = 0, gridRows = 0;
    std::vector<std::vector<int>> cellLines; // [col + row*gridCols] -> list of linedef indices
};