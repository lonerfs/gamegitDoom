#pragma once

#include <string>
#include <vector>
#include <cstdint>

#pragma pack(push, 1)
struct WadHeader {
    char identification[4];
    int32_t lumpCount;
    int32_t directoryOffset;
};

struct LumpInfo {
    int32_t filePos;
    int32_t size;
    char name[8];
};
#pragma pack(pop)

class WadLoader {
public:
    bool load(const std::string& filename);
    const std::vector<LumpInfo>& getLumps() const { return lumps; }
    std::vector<char> getLumpData(const std::string& name) const;
    std::vector<char> getLumpData(int index) const;
private:
    std::string wadFilename;
    std::vector<LumpInfo> lumps;
};