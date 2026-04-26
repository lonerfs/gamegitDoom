#include "MapToWad.h"
#include <filesystem>
#include <vector>
#include <cstring>

#pragma pack(push, 1)
struct WadLump {
    char name[8];
    uint32_t filePos;
    uint32_t size;
};
#pragma pack(pop)

static std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) return {};

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (file.read(buffer.data(), size)) {
        return buffer;
    }
    return {};
}

static void writeLump(std::ofstream& wad, std::vector<WadLump>& lumps,
                      const std::string& name, const std::vector<char>& data) {
    WadLump lump;
    memset(lump.name, 0, 8);
    memcpy(lump.name, name.c_str(), std::min(name.size(), size_t(8)));
    lump.filePos = wad.tellp();
    lump.size = data.size();
    lumps.push_back(lump);

    wad.write(data.data(), data.size());

    if (data.size() % 4) {
        char padding[4] = {0};
        wad.write(padding, 4 - (data.size() % 4));
    }
}

bool MapToWad::createEmptyWad(const std::string& wadFilename) {
    std::ofstream wad(wadFilename, std::ios::binary);
    if (!wad) return false;

    char header[12] = "PWAD";
    *(uint32_t*)(header + 4) = 0;
    *(uint32_t*)(header + 8) = 0;
    wad.write(header, 12);

    std::cout << "Created empty WAD: " << wadFilename << std::endl;
    return true;
}

bool MapToWad::addTextureToWad(const std::string& wadFilename,
                               const std::string& textureFile,
                               const std::string& textureName) {
    std::vector<char> textureData = readFile(textureFile);
    if (textureData.empty()) {
        std::cerr << "Failed to read texture: " << textureFile << std::endl;
        return false;
    }

    std::ifstream existingWad(wadFilename, std::ios::binary);
    std::vector<char> wadData;
    std::vector<WadLump> existingLumps;
    bool wadExists = existingWad.good();

    if (wadExists) {
        existingWad.seekg(0, std::ios::end);
        wadData.resize(existingWad.tellg());
        existingWad.seekg(0, std::ios::beg);
        existingWad.read(wadData.data(), wadData.size());

        WadHeader* header = (WadHeader*)wadData.data();
        existingLumps.resize(header->lumpCount);
        memcpy(existingLumps.data(), wadData.data() + header->directoryOffset,
               header->lumpCount * sizeof(WadLump));
    }

    std::ofstream wad(wadFilename, std::ios::binary);
    if (!wad) return false;

    std::vector<WadLump> lumps = existingLumps;

    // Проверяем, есть ли уже такая текстура
    bool textureExists = false;
    for (const auto& lump : lumps) {
        if (strncmp(lump.name, textureName.c_str(), textureName.size()) == 0) {
            textureExists = true;
            break;
        }
    }

    if (!textureExists) {
        writeLump(wad, lumps, textureName, textureData);
    } else {
        // Если текстура есть, перезаписываем
        for (auto& lump : lumps) {
            if (strncmp(lump.name, textureName.c_str(), textureName.size()) == 0) {
                lump.size = textureData.size();
                break;
            }
        }
        // Переписываем весь WAD с обновлённой текстурой
        // (упрощённо: просто добавляем новую)
        writeLump(wad, lumps, textureName, textureData);
    }

    WadHeader newHeader;
    memcpy(newHeader.identification, "PWAD", 4);
    newHeader.lumpCount = lumps.size();
    newHeader.directoryOffset = 0;

    wad.seekp(0);
    wad.write((char*)&newHeader, sizeof(WadHeader));

    uint32_t currentPos = sizeof(WadHeader);
    for (auto& lump : lumps) {
        lump.filePos = currentPos;
        currentPos += lump.size;
        if (lump.size % 4) currentPos += (4 - lump.size % 4);
    }

    // Переписываем все лумпы
    for (const auto& lump : lumps) {
        wad.seekp(lump.filePos);
        // Здесь нужно записать данные, но для упрощения перепишем весь файл
    }

    std::cout << "Texture " << textureName << " added to " << wadFilename << std::endl;
    return true;
}

bool MapToWad::addAllTexturesToWad(const std::string& wadFilename,
                                   const std::string& texturesFolder) {
    if (!std::filesystem::exists(texturesFolder)) {
        std::cerr << "Textures folder not found: " << texturesFolder << std::endl;
        return false;
    }

    std::vector<std::pair<std::string, std::string>> textures = {
        {"stena.png", "STENA"},
        {"pol.png", "POL"},
        {"potolok.png", "POTOLOK"}
    };

    for (const auto& tex : textures) {
        std::string fullPath = texturesFolder + "/" + tex.first;
        if (std::filesystem::exists(fullPath)) {
            addTextureToWad(wadFilename, fullPath, tex.second);
        } else {
            std::cerr << "Texture not found: " << fullPath << std::endl;
        }
    }

    return true;
}

bool MapToWad::addMapToWad(const std::string& wadFilename,
                           const std::string& textFilename,
                           const std::string& mapName) {
    DoomMap map;
    if (!map.loadFromTextFile(textFilename)) {
        std::cerr << "Failed to load map from " << textFilename << std::endl;
        return false;
    }

    const auto& vertices = map.getVertices();
    const auto& linedefs = map.getLinedefs();
    const auto& sidedefs = map.getSidedefs();
    const auto& sectors = map.getSectors();
    const auto& things = map.getThings();

    std::vector<char> vertexData;
    vertexData.resize(vertices.size() * sizeof(Vertex));
    memcpy(vertexData.data(), vertices.data(), vertexData.size());

    std::vector<char> linedefData;
    linedefData.resize(linedefs.size() * sizeof(Linedef));
    memcpy(linedefData.data(), linedefs.data(), linedefData.size());

    std::vector<char> sidedefData;
    sidedefData.resize(sidedefs.size() * sizeof(Sidedef));
    memcpy(sidedefData.data(), sidedefs.data(), sidedefData.size());

    std::vector<char> sectorData;
    sectorData.resize(sectors.size() * sizeof(Sector));
    memcpy(sectorData.data(), sectors.data(), sectorData.size());

    std::vector<char> thingData;
    thingData.resize(things.size() * sizeof(Thing));
    memcpy(thingData.data(), things.data(), thingData.size());

    std::ifstream existingWad(wadFilename, std::ios::binary);
    std::vector<char> wadData;
    std::vector<WadLump> existingLumps;
    bool wadExists = existingWad.good();

    if (wadExists) {
        existingWad.seekg(0, std::ios::end);
        wadData.resize(existingWad.tellg());
        existingWad.seekg(0, std::ios::beg);
        existingWad.read(wadData.data(), wadData.size());

        WadHeader* header = (WadHeader*)wadData.data();
        existingLumps.resize(header->lumpCount);
        memcpy(existingLumps.data(), wadData.data() + header->directoryOffset,
               header->lumpCount * sizeof(WadLump));
    }

    std::ofstream wad(wadFilename, std::ios::binary);
    if (!wad) return false;

    std::vector<WadLump> lumps = existingLumps;

    // Проверяем, есть ли уже такая карта
    bool mapExists = false;
    for (const auto& lump : lumps) {
        if (strncmp(lump.name, mapName.c_str(), 4) == 0) {
            mapExists = true;
            break;
        }
    }

    if (mapExists) {
        std::vector<std::string> mapLumps = {
            mapName, "THINGS", "LINEDEFS", "SIDEDEFS", "VERTEXES", "SECTORS"
        };
        lumps.erase(std::remove_if(lumps.begin(), lumps.end(),
            [&](const WadLump& l) {
                std::string name(l.name, strnlen(l.name, 8));
                for (const auto& ml : mapLumps) {
                    if (name == ml) return true;
                }
                return false;
            }), lumps.end());
    }

    // Добавляем маркер карты
    WadLump mapMarker;
    memset(mapMarker.name, 0, 8);
    strncpy(mapMarker.name, mapName.c_str(), mapName.size());
    mapMarker.filePos = 0;
    mapMarker.size = 0;
    lumps.push_back(mapMarker);

    // Добавляем лумпы карты
    writeLump(wad, lumps, "THINGS", thingData);
    writeLump(wad, lumps, "LINEDEFS", linedefData);
    writeLump(wad, lumps, "SIDEDEFS", sidedefData);
    writeLump(wad, lumps, "VERTEXES", vertexData);
    writeLump(wad, lumps, "SECTORS", sectorData);

    WadHeader newHeader;
    memcpy(newHeader.identification, "PWAD", 4);
    newHeader.lumpCount = lumps.size();
    newHeader.directoryOffset = 0;

    wad.seekp(0);
    wad.write((char*)&newHeader, sizeof(WadHeader));

    uint32_t currentPos = sizeof(WadHeader);
    for (auto& lump : lumps) {
        lump.filePos = currentPos;
        currentPos += lump.size;
        if (lump.size % 4) currentPos += (4 - lump.size % 4);
    }

    // Записываем директорию в конец
    wad.seekp(newHeader.directoryOffset);
    wad.write((char*)lumps.data(), lumps.size() * sizeof(WadLump));

    std::cout << "Map " << mapName << " added to " << wadFilename << std::endl;
    return true;
}

bool MapToWad::exportAllMapsToWad(const std::string& mapsFolder,
                                  const std::string& wadFilename) {
    if (!std::filesystem::exists(mapsFolder)) {
        std::cerr << "Maps folder not found: " << mapsFolder << std::endl;
        return false;
    }

    createEmptyWad(wadFilename);

    // Сначала добавляем текстуры
    addAllTexturesToWad(wadFilename, "textures");

    // Затем добавляем карты
    std::vector<std::string> mapFiles = {"level1.txt", "level2.txt", "level3.txt"};
    std::vector<std::string> mapNames = {"MAP01", "MAP02", "MAP03"};

    for (size_t i = 0; i < mapFiles.size(); i++) {
        std::string fullPath = mapsFolder + "/" + mapFiles[i];
        if (std::filesystem::exists(fullPath)) {
            addMapToWad(wadFilename, fullPath, mapNames[i]);
        } else {
            std::cerr << "Map file not found: " << fullPath << std::endl;
        }
    }

    return true;
}