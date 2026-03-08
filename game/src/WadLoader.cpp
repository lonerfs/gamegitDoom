#include "game/WadLoader.h"
#include <fstream>
#include <iostream>
#include <cstring>

bool WadLoader::load(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Cannot open WAD file: " << filename << std::endl;
        return false;
    }

    WadHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (file.gcount() != sizeof(header)) {
        std::cerr << "Failed to read WAD header" << std::endl;
        return false;
    }

    if (strncmp(header.identification, "IWAD", 4) != 0 && strncmp(header.identification, "PWAD", 4) != 0) {
        std::cerr << "Not a valid WAD file (signature: " << std::string(header.identification, 4) << ")" << std::endl;
        return false;
    }

    std::cout << "WAD type: " << std::string(header.identification, 4) << std::endl;
    std::cout << "Lump count: " << header.lumpCount << std::endl;
    std::cout << "Directory offset: " << header.directoryOffset << std::endl;

    file.seekg(header.directoryOffset);
    lumps.resize(header.lumpCount);
    file.read(reinterpret_cast<char*>(lumps.data()), header.lumpCount * sizeof(LumpInfo));
    if (file.gcount() != header.lumpCount * sizeof(LumpInfo)) {
        std::cerr << "Failed to read lump directory" << std::endl;
        return false;
    }

    wadFilename = filename;
    return true;
}

std::vector<char> WadLoader::getLumpData(const std::string& name) const {
    for (const auto& lump : lumps) {
        // Сравниваем первые name.size() символов
        if (strncmp(lump.name, name.c_str(), name.size()) == 0) {
            if (name.size() == 8 || lump.name[name.size()] == '\0') {
                return getLumpData(&lump - lumps.data());
            }
        }
    }
    return {};
}

std::vector<char> WadLoader::getLumpData(int index) const {
    if (index < 0 || index >= lumps.size()) return {};
    const auto& lump = lumps[index];
    std::vector<char> data(lump.size);
    std::ifstream file(wadFilename, std::ios::binary);
    if (!file) return {};
    file.seekg(lump.filePos);
    file.read(data.data(), lump.size);
    if (file.gcount() != lump.size) return {};
    return data;
}