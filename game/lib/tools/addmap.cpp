#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>
#include "MapToWad.h"

std::string findNextMapName(const std::string& wadFilename) {
    std::ifstream file(wadFilename, std::ios::binary);
    if (!file) return "MAP01";

    WadHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (file.gcount() != sizeof(header)) return "MAP01";

    file.seekg(header.directoryOffset);
    std::vector<LumpInfo> lumps(header.lumpCount);
    file.read(reinterpret_cast<char*>(lumps.data()), header.lumpCount * sizeof(LumpInfo));

    std::vector<int> existingNumbers;
    for (const auto& lump : lumps) {
        std::string name(lump.name, 8);
        name.erase(std::find(name.begin(), name.end(), '\0'), name.end());
        if (name.size() >= 5 && name.substr(0,3) == "MAP" && isdigit(name[3]) && isdigit(name[4])) {
            int num = std::stoi(name.substr(3,2));
            existingNumbers.push_back(num);
        }
    }

    if (existingNumbers.empty()) return "MAP01";

    std::sort(existingNumbers.begin(), existingNumbers.end());
    int next = 1;
    for (int num : existingNumbers) {
        if (num == next) ++next;
        else break;
    }
    if (next > 99) next = 99;
    char buf[6];
    snprintf(buf, sizeof(buf), "MAP%02d", next);
    return buf;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: addmap <map.txt>\n";
        return 1;
    }

    std::string textFile = argv[1];
    std::string wadFile = "freedoom1.wad";

    std::ifstream test(textFile);
    if (!test) {
        std::cerr << "Cannot open " << textFile << std::endl;
        return 1;
    }

    std::string mapName = findNextMapName(wadFile);
    std::cout << "Adding map as " << mapName << " in " << wadFile << std::endl;

    bool result = MapToWad::addMapToWad(wadFile, textFile, mapName);
    if (result) {
        std::cout << "Map successfully added. You can now select it in game.\n";
        return 0;
    } else {
        std::cerr << "Failed to add map.\n";
        return 1;
    }
}