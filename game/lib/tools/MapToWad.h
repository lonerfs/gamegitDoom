#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <cstring>

#pragma pack(push, 1)
struct WadHeader {
    char identification[4];  // "PWAD" для наших карт
    int32_t lumpCount;
    int32_t directoryOffset;
};

struct LumpInfo {
    int32_t filePos;
    int32_t size;
    char name[8];
};
#pragma pack(pop)

class MapToWad {
public:
    static bool convertTextMapToWad(const std::string& textFilename,
                                    const std::string& wadFilename,
                                    const std::string& mapName) {

        // Сначала загружаем текстовую карту
        std::vector<Vertex> vertices;
        std::vector<Linedef> linedefs;
        std::vector<Sidedef> sidedefs;
        std::vector<Sector> sectors;
        std::vector<Thing> things;

        if (!loadTextMap(textFilename, vertices, linedefs, sidedefs, sectors, things)) {
            return false;
        }

        // Создаем WAD файл
        return writeWadFile(wadFilename, mapName, vertices, linedefs, sidedefs, sectors, things);
    }

    static bool createEmptyWad(const std::string& wadFilename) {
        std::ofstream file(wadFilename, std::ios::binary);
        if (!file) {
            std::cerr << "Cannot create WAD file: " << wadFilename << std::endl;
            return false;
        }

        WadHeader header;
        memcpy(header.identification, "PWAD", 4);
        header.lumpCount = 0;
        header.directoryOffset = sizeof(WadHeader);

        file.write(reinterpret_cast<char*>(&header), sizeof(header));
        return true;
    }

    static bool addMapToWad(const std::string& wadFilename,
                           const std::string& textFilename,
                           const std::string& mapName) {
        // Загружаем существующий WAD или создаем новый
        std::vector<LumpInfo> existingLumps;
        bool fileExists = false;

        std::ifstream checkFile(wadFilename, std::ios::binary);
        if (checkFile) {
            fileExists = true;
            checkFile.close();
        }

        if (!fileExists) {
            if (!createEmptyWad(wadFilename)) {
                return false;
            }
        }

        // Загружаем текстовую карту
        std::vector<Vertex> vertices;
        std::vector<Linedef> linedefs;
        std::vector<Sidedef> sidedefs;
        std::vector<Sector> sectors;
        std::vector<Thing> things;

        if (!loadTextMap(textFilename, vertices, linedefs, sidedefs, sectors, things)) {
            return false;
        }

        // Добавляем карту в WAD
        return appendMapToWad(wadFilename, mapName, vertices, linedefs, sidedefs, sectors, things);
    }

private:
    static bool loadTextMap(const std::string& filename,
                           std::vector<Vertex>& vertices,
                           std::vector<Linedef>& linedefs,
                           std::vector<Sidedef>& sidedefs,
                           std::vector<Sector>& sectors,
                           std::vector<Thing>& things) {

        std::ifstream file(filename);
        if (!file) {
            std::cerr << "Cannot open map file: " << filename << std::endl;
            return false;
        }

        vertices.clear();
        linedefs.clear();
        sidedefs.clear();
        sectors.clear();
        things.clear();

        std::string line;
        std::string section;
        int lineNum = 0;

        std::cout << "Loading map from text file: " << filename << std::endl;

        // Создаем сектор по умолчанию
        Sector defaultSector;
        defaultSector.floorHeight = 0;
        defaultSector.ceilingHeight = 128;
        strncpy(defaultSector.floorTex, "FLOOR4_8", 8);
        strncpy(defaultSector.ceilingTex, "CEIL3_5", 8);
        defaultSector.lightLevel = 160;
        defaultSector.special = 0;
        defaultSector.tag = 0;

        bool hasSectors = false;

        while (std::getline(file, line)) {
            lineNum++;

            // Удаляем комментарии
            size_t commentPos = line.find('#');
            if (commentPos != std::string::npos) {
                line = line.substr(0, commentPos);
            }

            // Пропускаем пустые строки
            size_t start = line.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) continue;

            // Обработка секций
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
                } else {
                    std::cerr << "  Error parsing VERTICES at line " << lineNum << std::endl;
                }
            }
            else if (section == "LINEDEFS") {
                int start, end, flags = 0, type = 0, tag = 0, rightSidedef = 0, leftSidedef = 0xFFFF;
                // Формат: start end [flags type tag rightSidedef leftSidedef]
                iss >> start >> end;
                if (!iss.fail()) {
                    iss >> flags >> type >> tag >> rightSidedef >> leftSidedef;
                    linedefs.push_back({
                        static_cast<uint16_t>(start),
                        static_cast<uint16_t>(end),
                        static_cast<uint16_t>(flags),
                        static_cast<uint16_t>(type),
                        static_cast<uint16_t>(tag),
                        static_cast<uint16_t>(rightSidedef),
                        static_cast<uint16_t>(leftSidedef)
                    });
                }
            }
            else if (section == "SIDEDEFS") {
                int xOffset, yOffset, sector;
                char upperTex[9] = {0}, lowerTex[9] = {0}, middleTex[9] = {0};
                // Формат: xOffset yOffset upperTex lowerTex middleTex sector
                iss >> xOffset >> yOffset >> upperTex >> lowerTex >> middleTex >> sector;
                if (!iss.fail()) {
                    Sidedef sd;
                    sd.xOffset = static_cast<int16_t>(xOffset);
                    sd.yOffset = static_cast<int16_t>(yOffset);
                    strncpy(sd.upperTex, upperTex, 8);
                    strncpy(sd.lowerTex, lowerTex, 8);
                    strncpy(sd.middleTex, middleTex, 8);
                    sd.sector = static_cast<uint16_t>(sector);
                    sidedefs.push_back(sd);
                }
            }
            else if (section == "SECTORS") {
                int floorHeight, ceilingHeight, lightLevel, special, tag;
                char floorTex[9] = {0}, ceilingTex[9] = {0};
                // Формат: floorHeight ceilingHeight floorTex ceilingTex lightLevel special tag
                iss >> floorHeight >> ceilingHeight >> floorTex >> ceilingTex >> lightLevel >> special >> tag;
                if (!iss.fail()) {
                    Sector s;
                    s.floorHeight = static_cast<int16_t>(floorHeight);
                    s.ceilingHeight = static_cast<int16_t>(ceilingHeight);
                    strncpy(s.floorTex, floorTex, 8);
                    strncpy(s.ceilingTex, ceilingTex, 8);
                    s.lightLevel = static_cast<uint16_t>(lightLevel);
                    s.special = static_cast<uint16_t>(special);
                    s.tag = static_cast<uint16_t>(tag);
                    sectors.push_back(s);
                    hasSectors = true;
                }
            }
            else if (section == "THINGS") {
                int x, y, angle, type, flags;
                // Формат: x y angle type flags
                iss >> x >> y >> angle >> type >> flags;
                if (!iss.fail()) {
                    things.push_back({
                        static_cast<int16_t>(x),
                        static_cast<int16_t>(y),
                        static_cast<int16_t>(angle),
                        static_cast<int16_t>(type),
                        static_cast<uint16_t>(flags)
                    });
                } else {
                    // Простой формат: x y type
                    iss.clear();
                    iss.str(line);
                    iss >> x >> y >> type;
                    if (!iss.fail()) {
                        things.push_back({static_cast<int16_t>(x), static_cast<int16_t>(y), 0, static_cast<int16_t>(type), 7});
                    }
                }
            }
        }

        // Если нет секторов, создаем один по умолчанию
        if (!hasSectors && !sidedefs.empty()) {
            sectors.push_back(defaultSector);
            std::cout << "  Created default sector" << std::endl;

            // Обновляем ссылки на сектор в сайдефах
            for (auto& sd : sidedefs) {
                sd.sector = 0;
            }
        }

        std::cout << "Loaded map from " << filename << ":\n"
                  << "  Vertices: " << vertices.size() << "\n"
                  << "  Linedefs: " << linedefs.size() << "\n"
                  << "  Sidedefs: " << sidedefs.size() << "\n"
                  << "  Sectors: " << sectors.size() << "\n"
                  << "  Things: " << things.size() << std::endl;

        return true;
    }

    static bool writeWadFile(const std::string& wadFilename,
                            const std::string& mapName,
                            const std::vector<Vertex>& vertices,
                            const std::vector<Linedef>& linedefs,
                            const std::vector<Sidedef>& sidedefs,
                            const std::vector<Sector>& sectors,
                            const std::vector<Thing>& things) {

        std::ofstream file(wadFilename, std::ios::binary | std::ios::trunc);
        if (!file) {
            std::cerr << "Cannot create WAD file: " << wadFilename << std::endl;
            return false;
        }

        // Собираем все данные карты
        std::vector<char> mapData;
        std::vector<std::pair<std::string, std::vector<char>>> lumps;

        // Маркер начала карты
        LumpInfo mapMarker;
        memset(mapMarker.name, 0, 8);
        strncpy(mapMarker.name, mapName.c_str(), std::min(mapName.size(), size_t(8)));
        lumps.push_back({mapName, {}});

        // THINGS
        if (!things.empty()) {
            std::vector<char> thingData(things.size() * sizeof(Thing));
            memcpy(thingData.data(), things.data(), thingData.size());
            lumps.push_back({"THINGS", thingData});
        }

        // LINEDEFS
        if (!linedefs.empty()) {
            std::vector<char> linedefData(linedefs.size() * sizeof(Linedef));
            memcpy(linedefData.data(), linedefs.data(), linedefData.size());
            lumps.push_back({"LINEDEFS", linedefData});
        }

        // SIDEDEFS
        if (!sidedefs.empty()) {
            std::vector<char> sidedefData(sidedefs.size() * sizeof(Sidedef));
            memcpy(sidedefData.data(), sidedefs.data(), sidedefData.size());
            lumps.push_back({"SIDEDEFS", sidedefData});
        }

        // VERTEXES
        if (!vertices.empty()) {
            std::vector<char> vertexData(vertices.size() * sizeof(Vertex));
            memcpy(vertexData.data(), vertices.data(), vertexData.size());
            lumps.push_back({"VERTEXES", vertexData});
        }

        // SECTORS
        if (!sectors.empty()) {
            std::vector<char> sectorData(sectors.size() * sizeof(Sector));
            memcpy(sectorData.data(), sectors.data(), sectorData.size());
            lumps.push_back({"SECTORS", sectorData});
        }

        // Записываем заголовок
        WadHeader header;
        memcpy(header.identification, "PWAD", 4);
        header.lumpCount = static_cast<int32_t>(lumps.size());
        header.directoryOffset = 0; // Пока неизвестно

        file.write(reinterpret_cast<char*>(&header), sizeof(header));

        // Записываем данные lumps
        std::vector<LumpInfo> directory;
        int32_t currentPos = sizeof(header);

        for (auto& lump : lumps) {
            LumpInfo info;
            info.filePos = currentPos;
            info.size = static_cast<int32_t>(lump.second.size());
            memset(info.name, 0, 8);
            strncpy(info.name, lump.first.c_str(), std::min(lump.first.size(), size_t(8)));

            directory.push_back(info);

            if (!lump.second.empty()) {
                file.write(lump.second.data(), lump.second.size());
                currentPos += lump.second.size();
            }

            // Выравнивание (опционально)
            while (currentPos % 4 != 0) {
                char pad = 0;
                file.write(&pad, 1);
                currentPos++;
            }
        }

        // Записываем директорию в конец файла
        header.directoryOffset = currentPos;
        file.write(reinterpret_cast<char*>(directory.data()), directory.size() * sizeof(LumpInfo));

        // Возвращаемся и обновляем заголовок с правильным directoryOffset
        file.seekp(0);
        file.write(reinterpret_cast<char*>(&header), sizeof(header));

        std::cout << "WAD file created successfully: " << wadFilename << std::endl;
        std::cout << "Total lumps: " << lumps.size() << std::endl;

        return true;
    }

    static bool appendMapToWad(const std::string& wadFilename,
                              const std::string& mapName,
                              const std::vector<Vertex>& vertices,
                              const std::vector<Linedef>& linedefs,
                              const std::vector<Sidedef>& sidedefs,
                              const std::vector<Sector>& sectors,
                              const std::vector<Thing>& things) {

        // Сначала читаем существующий WAD
        std::ifstream inFile(wadFilename, std::ios::binary);
        if (!inFile) {
            return writeWadFile(wadFilename, mapName, vertices, linedefs, sidedefs, sectors, things);
        }

        // Читаем заголовок
        WadHeader header;
        inFile.read(reinterpret_cast<char*>(&header), sizeof(header));

        // Читаем существующую директорию
        inFile.seekg(header.directoryOffset);
        std::vector<LumpInfo> existingLumps(header.lumpCount);
        inFile.read(reinterpret_cast<char*>(existingLumps.data()),
                   header.lumpCount * sizeof(LumpInfo));

        inFile.close();

        // Собираем новые lump'ы для карты
        std::vector<std::pair<std::string, std::vector<char>>> newLumps;

        // Маркер карты
        newLumps.push_back({mapName, {}});

        if (!things.empty()) {
            std::vector<char> thingData(things.size() * sizeof(Thing));
            memcpy(thingData.data(), things.data(), thingData.size());
            newLumps.push_back({"THINGS", thingData});
        }

        if (!linedefs.empty()) {
            std::vector<char> linedefData(linedefs.size() * sizeof(Linedef));
            memcpy(linedefData.data(), linedefs.data(), linedefData.size());
            newLumps.push_back({"LINEDEFS", linedefData});
        }

        if (!sidedefs.empty()) {
            std::vector<char> sidedefData(sidedefs.size() * sizeof(Sidedef));
            memcpy(sidedefData.data(), sidedefs.data(), sidedefData.size());
            newLumps.push_back({"SIDEDEFS", sidedefData});
        }

        if (!vertices.empty()) {
            std::vector<char> vertexData(vertices.size() * sizeof(Vertex));
            memcpy(vertexData.data(), vertices.data(), vertexData.size());
            newLumps.push_back({"VERTEXES", vertexData});
        }

        if (!sectors.empty()) {
            std::vector<char> sectorData(sectors.size() * sizeof(Sector));
            memcpy(sectorData.data(), sectors.data(), sectorData.size());
            newLumps.push_back({"SECTORS", sectorData});
        }

        // Перезаписываем файл с обновленными данными
        std::ofstream outFile(wadFilename, std::ios::binary | std::ios::trunc);
        if (!outFile) {
            std::cerr << "Cannot write to WAD file: " << wadFilename << std::endl;
            return false;
        }

        // Копируем все существующие данные
        // Сначала заголовок (пока с временными значениями)
        WadHeader newHeader;
        memcpy(newHeader.identification, header.identification, 4);
        newHeader.lumpCount = header.lumpCount + static_cast<int32_t>(newLumps.size());
        newHeader.directoryOffset = 0;

        outFile.write(reinterpret_cast<char*>(&newHeader), sizeof(newHeader));

        // Копируем все существующие lump'ы
        std::ifstream dataFile(wadFilename, std::ios::binary);
        dataFile.seekg(sizeof(WadHeader));

        std::vector<char> buffer(1024 * 1024); // 1MB буфер
        int32_t bytesToCopy = header.directoryOffset - sizeof(WadHeader);
        int32_t bytesCopied = 0;

        while (bytesCopied < bytesToCopy) {
            int32_t chunkSize = std::min<int32_t>(buffer.size(), bytesToCopy - bytesCopied);
            dataFile.read(buffer.data(), chunkSize);
            outFile.write(buffer.data(), chunkSize);
            bytesCopied += chunkSize;
        }

        dataFile.close();

        // Добавляем новые lump'ы
        int32_t currentPos = header.directoryOffset;

        for (auto& lump : newLumps) {
            if (!lump.second.empty()) {
                outFile.write(lump.second.data(), lump.second.size());
                currentPos += lump.second.size();

                // Выравнивание
                while (currentPos % 4 != 0) {
                    char pad = 0;
                    outFile.write(&pad, 1);
                    currentPos++;
                }
            }
        }

        // Обновленная директория
        std::vector<LumpInfo> newDirectory;

        // Добавляем существующие записи
        for (const auto& lump : existingLumps) {
            LumpInfo info = lump;
            newDirectory.push_back(info);
        }

        // Добавляем новые записи
        int32_t lumpPos = header.directoryOffset;
        for (const auto& lump : newLumps) {
            LumpInfo info;
            info.filePos = lumpPos;
            info.size = static_cast<int32_t>(lump.second.size());
            memset(info.name, 0, 8);
            strncpy(info.name, lump.first.c_str(), std::min(lump.first.size(), size_t(8)));

            newDirectory.push_back(info);

            if (!lump.second.empty()) {
                lumpPos += lump.second.size();
                // Выравнивание
                while (lumpPos % 4 != 0) lumpPos++;
            }
        }

        // Записываем директорию
        newHeader.directoryOffset = currentPos;
        outFile.write(reinterpret_cast<char*>(newDirectory.data()),
                     newDirectory.size() * sizeof(LumpInfo));

        // Обновляем заголовок
        outFile.seekp(0);
        outFile.write(reinterpret_cast<char*>(&newHeader), sizeof(newHeader));

        std::cout << "Map " << mapName << " added to " << wadFilename << std::endl;

        return true;
    }
};