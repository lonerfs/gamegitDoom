#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include "game/WadLoader.h"   // WadHeader, LumpInfo уже определены здесь
#include "game/DoomMap.h"     // Vertex, Linedef, Sidedef, Sector, Thing

class MapToWad {
public:
    static bool convertTextMapToWad(const std::string& textFilename,
                                    const std::string& wadFilename,
                                    const std::string& mapName) {
        // Заглушка для компиляции
        std::cout << "convertTextMapToWad not implemented\n";
        return false;
    }

    static bool createEmptyWad(const std::string& wadFilename) {
        std::cout << "createEmptyWad not implemented\n";
        return false;
    }

    static bool addMapToWad(const std::string& wadFilename,
                           const std::string& textFilename,
                           const std::string& mapName) {
        std::cout << "addMapToWad not implemented\n";
        return false;
    }
};