#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include "game/WadLoader.h"
#include "game/DoomMap.h"

class MapToWad {
public:
    static bool addMapToWad(const std::string& wadFilename,
                           const std::string& textFilename,
                           const std::string& mapName);

    static bool createEmptyWad(const std::string& wadFilename);

    static bool exportAllMapsToWad(const std::string& mapsFolder,
                                   const std::string& wadFilename);

    static bool addTextureToWad(const std::string& wadFilename,
                                const std::string& textureFile,
                                const std::string& textureName);

    static bool addAllTexturesToWad(const std::string& wadFilename,
                                    const std::string& texturesFolder);
};