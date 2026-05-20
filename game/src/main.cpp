#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <fstream>
#include <sstream>
#include <cstring>
#include <chrono>
#include <future>
#include <map>
#include <cstdlib>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include "game/DoomMap.h"
#include "game/Player.h"
#include "game/WadLoader.h"
#include "game/RenderThread.h"
#include "game/Mob.h"

HitResult rayLineIntersection(float x0, float y0, float dx, float dy,
                              float x1, float y1, float x2, float y2);

struct BulletHole {
    float x, y;
    float distance;
    int lifetime;
};

struct HitFlash {
    float x, y;
    int lifetime;
};

struct Pickup {
    float x, y;
    int type;
    bool active;
};

struct Texture {
    SDL_Texture* texture;
    int width;
    int height;
    std::vector<Uint32> pixelData;
};

bool isInBossSpawnArea(float x, float y) {
    if (x >= 992 && x <= 1216 && y >= 1056 && y <= 1184) return true;
    if (x >= 1536 && x <= 1728 && y >= 1056 && y <= 1184) return true;
    if (x >= 1568 && x <= 1664 && y >= 672 && y <= 896) return true;
    if (x >= 1568 && x <= 1728 && y >= 256 && y <= 448) return true;
    if (x >= 992 && x <= 1280 && y >= 256 && y <= 448) return true;
    return false;
}

struct SpawnSquare {
    float x1, y1, x2, y2;
};

std::vector<SpawnSquare> getBossSpawnSquares() {
    return {
        {992, 1056, 1216, 1184},
        {1536, 1056, 1728, 1184},
        {1568, 672, 1664, 896},
        {1568, 256, 1728, 448},
        {992, 256, 1280, 448}
    };
}

void spawnMobsInRandomSquare(std::vector<Mob>& mobs, Mob& boss) {
    auto squares = getBossSpawnSquares();
    int squareIndex = rand() % squares.size();
    auto& square = squares[squareIndex];
    MobType types[] = {ZOMBIE, IMP};

    for (int i = 0; i < 5; i++) {
        float spawnX = square.x1 + (float)(rand() % (int)(square.x2 - square.x1));
        float spawnY = square.y1 + (float)(rand() % (int)(square.y2 - square.y1));
        MobType type = types[rand() % 2];
        mobs.emplace_back(spawnX, spawnY, type);
        boss.summonedMobsCount++;
    }
    boss.invulnerable = true;
}

Uint32 bilinearFilter(const Texture& tex, float u, float v) {
    float fx = u * tex.width;
    float fy = v * tex.height;
    int x0 = (int)fx;
    int y0 = (int)fy;
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= tex.width) x1 = tex.width - 1;
    if (y1 >= tex.height) y1 = tex.height - 1;
    float dx = fx - x0;
    float dy = fy - y0;
    Uint32 p00 = tex.pixelData[y0 * tex.width + x0];
    Uint32 p10 = tex.pixelData[y0 * tex.width + x1];
    Uint32 p01 = tex.pixelData[y1 * tex.width + x0];
    Uint32 p11 = tex.pixelData[y1 * tex.width + x1];
    float r00 = (p00 >> 16) & 0xFF;
    float g00 = (p00 >> 8) & 0xFF;
    float b00 = p00 & 0xFF;
    float r10 = (p10 >> 16) & 0xFF;
    float g10 = (p10 >> 8) & 0xFF;
    float b10 = p10 & 0xFF;
    float r01 = (p01 >> 16) & 0xFF;
    float g01 = (p01 >> 8) & 0xFF;
    float b01 = p01 & 0xFF;
    float r11 = (p11 >> 16) & 0xFF;
    float g11 = (p11 >> 8) & 0xFF;
    float b11 = p11 & 0xFF;
    float r0 = r00 * (1 - dx) + r10 * dx;
    float g0 = g00 * (1 - dx) + g10 * dx;
    float b0 = b00 * (1 - dx) + b10 * dx;
    float r1 = r01 * (1 - dx) + r11 * dx;
    float g1 = g01 * (1 - dx) + g11 * dx;
    float b1 = b01 * (1 - dx) + b11 * dx;
    float r = r0 * (1 - dy) + r1 * dy;
    float g = g0 * (1 - dy) + g1 * dy;
    float b = b0 * (1 - dy) + b1 * dy;
    return (0xFF << 24) | ((int)r << 16) | ((int)g << 8) | (int)b;
}

Texture loadTexture(SDL_Renderer* renderer, const char* filename) {
    Texture tex = {nullptr, 64, 64, {}};
    SDL_Surface* surf = IMG_Load(filename);
    if (surf) {
        SDL_Surface* converted = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);
        if (converted) {
            tex.width = converted->w;
            tex.height = converted->h;
            tex.pixelData.resize(tex.width * tex.height);
            memcpy(tex.pixelData.data(), converted->pixels, tex.width * tex.height * 4);

            for (size_t i = 0; i < tex.pixelData.size(); i++) {
                Uint32 pixel = tex.pixelData[i];
                Uint8 r = (pixel >> 16) & 0xFF;
                Uint8 g = (pixel >> 8) & 0xFF;
                Uint8 b = pixel & 0xFF;
                if (r > 245 && g > 245 && b > 245) {
                    tex.pixelData[i] = 0;
                }
            }

            SDL_Surface* modifiedSurf = SDL_CreateSurface(tex.width, tex.height, SDL_PIXELFORMAT_RGBA32);
            memcpy(modifiedSurf->pixels, tex.pixelData.data(), tex.width * tex.height * 4);
            tex.texture = SDL_CreateTextureFromSurface(renderer, modifiedSurf);
            SDL_DestroySurface(modifiedSurf);
            SDL_DestroySurface(converted);
        }
        SDL_DestroySurface(surf);

        if (tex.texture) {
            // Меняем LINEAR на NEAREST для чётких пикселей (убираем размытие)
            SDL_SetTextureScaleMode(tex.texture, SDL_SCALEMODE_NEAREST);
            std::cout << "Loaded texture: " << filename << std::endl;
        }
    } else {
        std::cerr << "Failed to load texture: " << filename << std::endl;
        SDL_Surface* dummy = SDL_CreateSurface(64, 64, SDL_PIXELFORMAT_RGBA32);
        tex.pixelData.resize(64 * 64);
        Uint32* pixels = (Uint32*)dummy->pixels;
        for (int y = 0; y < 64; y++) {
            for (int x = 0; x < 64; x++) {
                Uint8 intensity = ((x/8 + y/8) % 2) ? 255 : 180;
                Uint32 color = (0xFF << 24) | (intensity << 16) | (intensity << 8) | 0;
                pixels[y * 64 + x] = color;
                tex.pixelData[y * 64 + x] = color;
            }
        }
        tex.texture = SDL_CreateTextureFromSurface(renderer, dummy);
        SDL_SetTextureScaleMode(tex.texture, SDL_SCALEMODE_NEAREST);
        tex.width = tex.height = 64;
        SDL_DestroySurface(dummy);
    }
    return tex;
}

void drawWallColumn(SDL_Renderer* renderer, int x, int top, int bottom, int texX, const Texture& tex, float distance) {
    if (top >= bottom) return;
    int height = bottom - top;
    const int STRIP_SIZE = 16;  // Чёткие полосы, нет размытия
    for (int y = top; y < bottom; y += STRIP_SIZE) {
        int yEnd = std::min(y + STRIP_SIZE, bottom);
        float t = (float)(y - top) / height;
        float v = t;
        float u = (float)texX / tex.width;
        Uint8 r, g, b;
        if (!tex.pixelData.empty() && texX >= 0 && texX < tex.width) {
            Uint32 filteredColor = bilinearFilter(tex, u, v);
            r = (filteredColor >> 16) & 0xFF;
            g = (filteredColor >> 8) & 0xFF;
            b = filteredColor & 0xFF;
        } else {
            r = 220; g = 180; b = 0;
        }
        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderLine(renderer, x, y, x, yEnd - 1);
    }
}

bool rayIntersectsMob(float x0, float y0, float dx, float dy,
                      const Mob& mob, float& dist, float& hitX, float& hitY) {
    float half = mob.size / 2.0f;
    float left   = mob.x - half;
    float right  = mob.x + half;
    float bottom = mob.y - half;
    float top    = mob.y + half;

    if (x0 >= left && x0 <= right && y0 >= bottom && y0 <= top) {
        dist = 0.0f;
        hitX = x0;
        hitY = y0;
        return true;
    }

    float tmin = -std::numeric_limits<float>::max();
    float tmax = std::numeric_limits<float>::max();

    if (std::abs(dx) > 1e-6) {
        float t1 = (left - x0) / dx;
        float t2 = (right - x0) / dx;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
    }
    if (std::abs(dy) > 1e-6) {
        float t1 = (bottom - y0) / dy;
        float t2 = (top - y0) / dy;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
    }

    if (tmin <= tmax && tmax > 0) {
        dist = (tmin > 0) ? tmin : tmax;
        hitX = x0 + dist * dx;
        hitY = y0 + dist * dy;
        return true;
    }
    return false;
}

void performShot(float x, float y, float angle,
                 const std::vector<Linedef>& lines,
                 const std::vector<Vertex>& vertices,
                 std::vector<Mob>& mobs,
                 std::vector<BulletHole>& bulletHoles,
                 std::vector<HitFlash>& hitFlashes,
                 int& pistolAmmo, int& shotgunAmmo,
                 int currentWeapon,
                 int& playerHealth, int playerMaxHealth) {
    int pellets = (currentWeapon == 0) ? 1 : 3;
    float spread = (currentWeapon == 0) ? 0.0f : 0.08f;
    int ammo = (currentWeapon == 0) ? pistolAmmo : shotgunAmmo;
    if (ammo <= 0) return;

    for (int i = 0; i < pellets; ++i) {
        float offset = 0.0f;
        if (pellets > 1) offset = ((float)i / (pellets - 1) - 0.5f) * spread;
        float spreadAngle = angle + offset;
        float dx = cos(spreadAngle);
        float dy = sin(spreadAngle);
        float closestDist = std::numeric_limits<float>::max();
        Mob* hitMob = nullptr;
        float hitX = 0, hitY = 0;

        for (auto& mob : mobs) {
            float d, ix, iy;
            if (rayIntersectsMob(x, y, dx, dy, mob, d, ix, iy)) {
                if (d < closestDist) {
                    closestDist = d;
                    hitMob = &mob;
                    hitX = ix; hitY = iy;
                }
            }
        }

        for (size_t li = 0; li < lines.size(); ++li) {
            if (lines[li].startVertex >= vertices.size() || lines[li].endVertex >= vertices.size()) continue;
            const Vertex& v1 = vertices[lines[li].startVertex];
            const Vertex& v2 = vertices[lines[li].endVertex];
            HitResult h = rayLineIntersection(x, y, dx, dy, v1.x, v1.y, v2.x, v2.y);
            if (h.hit && h.distance < closestDist && h.distance > 0.1f) {
                closestDist = h.distance;
                hitMob = nullptr;
                hitX = h.hitX;
                hitY = h.hitY;
            }
        }

        if (hitMob) {
            int damage = (currentWeapon == 0) ? 20 : 10;
            hitMob->takeDamage(damage);
            hitFlashes.push_back({hitX, hitY, 6});
            if (hitFlashes.size() > 50) hitFlashes.erase(hitFlashes.begin());

            if (!hitMob->isAlive()) {
                if (rand() % 100 < 40) playerHealth = std::min(playerMaxHealth, playerHealth + 10);
                if (rand() % 100 < 30) shotgunAmmo += 4;
                if (rand() % 100 < 20) pistolAmmo += 10;
            }
        } else if (closestDist < 1000000.0f) {
            bulletHoles.push_back({hitX, hitY, 150});
            if (bulletHoles.size() > 100) bulletHoles.erase(bulletHoles.begin());
        }
    }
    if (currentWeapon == 0) pistolAmmo--;
    else shotgunAmmo--;
    if (bulletHoles.size() > 50)
        bulletHoles.erase(bulletHoles.begin(), bulletHoles.begin() + 10);
}

bool isPointVisible(float px, float py, float tx, float ty,
                    const std::vector<Linedef>& lines,
                    const std::vector<Vertex>& vertices) {
    float dx = tx - px;
    float dy = ty - py;
    float dist = std::sqrt(dx*dx + dy*dy);
    if (dist < 0.1f) return true;
    float step = 0.5f;
    float nx = dx / dist;
    float ny = dy / dist;
    for (float d = 0; d < dist; d += step) {
        for (const auto& line : lines) {
            if (line.startVertex >= vertices.size() || line.endVertex >= vertices.size()) continue;
            const Vertex& v1 = vertices[line.startVertex];
            const Vertex& v2 = vertices[line.endVertex];
            HitResult hit = rayLineIntersection(px, py, nx, ny, v1.x, v1.y, v2.x, v2.y);
            if (hit.hit && hit.distance < d+step && hit.distance > 0.1f) {
                if (hit.distance < dist - 0.1f) return false;
            }
        }
    }
    return true;
}

struct MapInfo {
    std::string name;
    std::string source;
    bool isCustom;
};

std::vector<MapInfo> getAllMaps(const WadLoader& wad) {
    std::vector<MapInfo> maps;
    if (std::ifstream("level1.txt").good()) maps.push_back({"★ MY LEVEL 1", "txt", true});
    if (std::ifstream("level2.txt").good()) maps.push_back({"★ MY LEVEL 2", "txt", true});
    if (std::ifstream("level3.txt").good()) maps.push_back({"★ MY LEVEL 3", "txt", true});
    const auto& lumps = wad.getLumps();
    for (const auto& lump : lumps) {
        std::string name(lump.name, 8);
        name.erase(std::find(name.begin(), name.end(), '\0'), name.end());
        if (name.size() >= 5 && name.substr(0, 3) == "MAP" && isdigit(name[3]) && isdigit(name[4]))
            maps.push_back({name, "wad", false});
        else if (name.size() >= 4 && name[0] == 'E' && isdigit(name[1]) && name[2] == 'M' && isdigit(name[3]))
            maps.push_back({name, "wad", false});
    }
    return maps;
}

void listAllLumps(const WadLoader& wad) {
    const auto& lumps = wad.getLumps();
    for (size_t i = 0; i < lumps.size(); i++) {
        std::string name(lumps[i].name, 8);
        name.erase(std::find(name.begin(), name.end(), '\0'), name.end());
        std::cout << i << ": '" << name << "'" << std::endl;
    }
}

int main() {
    const int WINDOW_WIDTH = 1280;
    const int WINDOW_HEIGHT = 960;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;
    if (TTF_Init() < 0) { SDL_Quit(); return 1; }

    TTF_Font* font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 24);
    TTF_Font* smallFont = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 18);
    if (!font) font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 20);
    if (!smallFont) smallFont = font;
    if (!font) { TTF_Quit(); SDL_Quit(); return 1; }

    SDL_Window* window = SDL_CreateWindow("Doom Clone", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window) { TTF_Quit(); SDL_Quit(); return 1; }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) { SDL_DestroyWindow(window); TTF_Quit(); SDL_Quit(); return 1; }

    SDL_SetRenderVSync(renderer, 0);

    Texture wallTex = loadTexture(renderer, "stena.png");
    Texture mobWalkingTex = loadTexture(renderer, "mob_walking.png");
    Texture mobAttackTex = loadTexture(renderer, "mob_attack.png");
    Texture mobDeathTex = loadTexture(renderer, "mob_death.png");
    Texture bossWalkingTex = loadTexture(renderer, "boss.png");
    Texture bossAttackTex = loadTexture(renderer, "boss_attack.png");
    Texture bossDeathTex = loadTexture(renderer, "boss_death.png");

    Texture gunTex = loadTexture(renderer, "gun.png");
    Texture shootingGunTex = loadTexture(renderer, "shooting_gun.png");
    Texture shotgunTex = loadTexture(renderer, "shotgun.png");
    Texture shotgunShotTex = loadTexture(renderer, "shotgun_shot.png");

    Texture firstAidKitTex = loadTexture(renderer, "firstaidkit.png");
    Texture pistolCartridgesTex = loadTexture(renderer, "pistol_cartridges.png");
    Texture shotgunCartridgesTex = loadTexture(renderer, "shotgun_cartridges.png");

    WadLoader wad;
    if (!wad.load("freedoom1.wad")) {
        std::cerr << "Warning: freedoom1.wad not loaded" << std::endl;
    } else {
        listAllLumps(wad);
    }

    auto maps = getAllMaps(wad);
    if (maps.empty()) { std::cerr << "No maps found!" << std::endl; return 1; }

    int selectedMapIndex = 0, scrollOffset = 0, maxVisible = 15;
    bool inMenu = true, running = true;
    SDL_Event event;

    DoomMap previewMap;
    std::vector<Vertex> previewVertices;
    std::vector<Linedef> previewLines;
    bool hasPreview = false;

    auto loadPreview = [&](int idx) {
        if (maps[idx].isCustom) hasPreview = previewMap.loadFromTextFile("level" + std::to_string(idx+1) + ".txt");
        else hasPreview = previewMap.loadFromWad(wad, maps[idx].name);
        if (hasPreview) { previewVertices = previewMap.getVertices(); previewLines = previewMap.getLinedefs(); }
    };
    loadPreview(0);

    while (inMenu && running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) { running = false; inMenu = false; }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_UP && selectedMapIndex > 0) {
                    selectedMapIndex--;
                    if (selectedMapIndex < scrollOffset) scrollOffset = selectedMapIndex;
                    loadPreview(selectedMapIndex);
                }
                if (event.key.scancode == SDL_SCANCODE_DOWN && selectedMapIndex < (int)maps.size()-1) {
                    selectedMapIndex++;
                    if (selectedMapIndex >= scrollOffset+maxVisible) scrollOffset = selectedMapIndex - maxVisible + 1;
                    loadPreview(selectedMapIndex);
                }
                if (event.key.scancode == SDL_SCANCODE_RETURN || event.key.scancode == SDL_SCANCODE_SPACE) inMenu = false;
            }
        }

        SDL_SetRenderDrawColor(renderer, 20,20,30,255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 40,40,50,255);
        SDL_FRect listPanel = {50,120,350,700};
        SDL_RenderFillRect(renderer, &listPanel);

        int visibleCount = std::min(maxVisible, (int)maps.size()-scrollOffset);
        for (int i=0; i<visibleCount; i++) {
            int idx = scrollOffset+i;
            SDL_Color col = (idx==selectedMapIndex)? SDL_Color{255,200,0,255} : SDL_Color{200,200,200,255};
            std::string name = maps[idx].name;
            SDL_Surface* surf = TTF_RenderText_Solid(smallFont, name.c_str(), 0, col);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_FRect rect = {70, 140.0f+i*35, (float)surf->w, (float)surf->h};
                SDL_RenderTexture(renderer, tex, NULL, &rect);
                SDL_DestroyTexture(tex);
                SDL_DestroySurface(surf);
            }
        }

        SDL_SetRenderDrawColor(renderer, 30,30,40,255);
        SDL_FRect previewPanel = {450,120,780,700};
        SDL_RenderFillRect(renderer, &previewPanel);

        if (hasPreview && !previewVertices.empty()) {
            int minX=previewVertices[0].x, maxX=minX, minY=previewVertices[0].y, maxY=minY;
            for (auto& v: previewVertices) {
                minX=std::min(minX,(int)v.x); maxX=std::max(maxX,(int)v.x);
                minY=std::min(minY,(int)v.y); maxY=std::max(maxY,(int)v.y);
            }
            float sx = 700.0f/(maxX-minX), sy = 600.0f/(maxY-minY);
            float scale = std::min(sx,sy)*0.8f;
            float offX = 460 + (700 - (maxX-minX)*scale)/2 - minX*scale;
            float offY = 180 + (600 - (maxY-minY)*scale)/2 + maxY*scale;
            SDL_SetRenderDrawColor(renderer, 255,255,255,255);
            for (auto& line: previewLines) {
                if (line.startVertex>=previewVertices.size() || line.endVertex>=previewVertices.size()) continue;
                auto& v1=previewVertices[line.startVertex];
                auto& v2=previewVertices[line.endVertex];
                int x1 = v1.x*scale+offX, y1 = -v1.y*scale+offY;
                int x2 = v2.x*scale+offX, y2 = -v2.y*scale+offY;
                SDL_RenderLine(renderer, x1, y1, x2, y2);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    if (!running) {
        TTF_CloseFont(font); if(smallFont!=font) TTF_CloseFont(smallFont);
        SDL_DestroyRenderer(renderer); SDL_DestroyWindow(window);
        TTF_Quit(); SDL_Quit();
        return 0;
    }

    int playerHealth = 100;
    int pistolAmmo = 50;
    int shotgunAmmo = 10;
    int currentWeapon = 0;
    const int playerMaxHealth = 100;
    int shootFlashFrames = 0;

    std::vector<std::string> levelOrder = {"level1.txt", "level2.txt", "level3.txt"};
    int currentLevelIdx = 0;

    while (running && currentLevelIdx < 3) {
        DoomMap gameMap;
        std::string currentLevelFile = levelOrder[currentLevelIdx];
        gameMap.loadFromTextFile(currentLevelFile);

        auto vertices = gameMap.getVertices();
        auto lines = gameMap.getLinedefs();
        auto sidedefs = gameMap.getSidedefs();
        auto sectors = gameMap.getSectors();

        if (vertices.empty() || lines.empty()) break;

        Player player;
        player.angle = 0.0f;
        player.speed = 200.0f;
        bool spawnFound = false;
        const auto& things = gameMap.getThings();

        for (const auto& t : things) {
            if (t.type == 1) {
                player.x = (float)t.x;
                player.y = (float)t.y;
                spawnFound = true;
                break;
            }
        }
        if (!spawnFound) {
            int minX = vertices[0].x, maxX = vertices[0].x;
            int minY = vertices[0].y, maxY = vertices[0].y;
            for (const auto& v : vertices) {
                minX = std::min(minX, (int)v.x);
                maxX = std::max(maxX, (int)v.x);
                minY = std::min(minY, (int)v.y);
                maxY = std::max(maxY, (int)v.y);
            }
            player.x = (minX + maxX) / 2.0f;
            player.y = (minY + maxY) / 2.0f;
        }

        std::vector<Mob> mobs;
        std::vector<Pickup> pickups;

        for (const auto& t : things) {
            if (t.type == 1) {
                player.x = (float)t.x;
                player.y = (float)t.y;
            } else if (t.type == 2) {
                mobs.emplace_back((float)t.x, (float)t.y, ZOMBIE);
            } else if (t.type == 3) {
                mobs.emplace_back((float)t.x, (float)t.y, IMP);
            } else if (t.type == 4) {
                mobs.emplace_back((float)t.x, (float)t.y, BOSS);
            } else if (t.type == 2011) {
                pickups.push_back({(float)t.x, (float)t.y, 2011, true});
            } else if (t.type == 2007) {
                pickups.push_back({(float)t.x, (float)t.y, 2007, true});
            } else if (t.type == 2008) {
                pickups.push_back({(float)t.x, (float)t.y, 2008, true});
            }
        }

        struct MobRenderState {
            bool isDying = false;
            float deathTimer = 0.0f;
            float attackAnimTime = 0.0f;
            bool isSummoning = false;
            float summonAnimTime = 0.0f;
        };
        std::map<Mob*, MobRenderState> mobRenderStates;

        const float FOV = 60.0f * M_PI / 180.0f;
        Uint64 lastTime = SDL_GetTicks();
        Uint64 lastShootTime = 0;
        const Uint64 SHOOT_DELAY_MS = 300;
        std::vector<BulletHole> bulletHoles;
        std::vector<HitFlash> hitFlashes;

        int frameCount = 0;
        Uint64 lastFpsTime = SDL_GetTicks();
        int currentFPS = 0;

        unsigned int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        ThreadPool threadPool(numThreads);

        int RENDER_SCALE = 2;
        int RENDER_WIDTH = WINDOW_WIDTH / RENDER_SCALE;

        bool levelComplete = false;
        float bossSummonTimer = 0.0f;

        while (running && !levelComplete) {
            Uint64 now = SDL_GetTicks();
            float dt = (now - lastTime)/1000.0f;
            lastTime = now;
            if (dt > 0.033f) dt = 0.033f;

            frameCount++;
            if (now - lastFpsTime >= 1000) {
                currentFPS = frameCount;
                frameCount = 0;
                lastFpsTime = now;
            }

            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) { running = false; levelComplete = true; break; }
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE) { running = false; levelComplete = true; break; }
                if (event.type == SDL_EVENT_KEY_DOWN) {
                    if (event.key.scancode == SDL_SCANCODE_1) currentWeapon = 0;
                    if (event.key.scancode == SDL_SCANCODE_2) currentWeapon = 1;
                    if (event.key.scancode == SDL_SCANCODE_F1) { RENDER_SCALE = 1; RENDER_WIDTH = WINDOW_WIDTH / RENDER_SCALE; }
                    if (event.key.scancode == SDL_SCANCODE_F2) { RENDER_SCALE = 2; RENDER_WIDTH = WINDOW_WIDTH / RENDER_SCALE; }
                    if (event.key.scancode == SDL_SCANCODE_F3) { RENDER_SCALE = 3; RENDER_WIDTH = WINDOW_WIDTH / RENDER_SCALE; }
                }
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_SPACE) {
                    if (now - lastShootTime >= SHOOT_DELAY_MS) {
                        lastShootTime = now;
                        shootFlashFrames = 2;
                        performShot(player.x, player.y, player.angle,
                                    lines, vertices, mobs, bulletHoles, hitFlashes,
                                    pistolAmmo, shotgunAmmo, currentWeapon,
                                    playerHealth, playerMaxHealth);
                        if (playerHealth <= 0) {
                            running = false;
                            levelComplete = true;
                            break;
                        }
                    }
                }
            }

            const bool* keys = SDL_GetKeyboardState(nullptr);
            float move = player.speed * dt;
            float turn = 3.0f * dt;
            float dx = 0, dy = 0;
            if (keys[SDL_SCANCODE_W]) { dx += cos(player.angle)*move; dy += sin(player.angle)*move; }
            if (keys[SDL_SCANCODE_S]) { dx -= cos(player.angle)*move; dy -= sin(player.angle)*move; }
            if (keys[SDL_SCANCODE_A]) { dx += sin(player.angle)*move; dy -= cos(player.angle)*move; }
            if (keys[SDL_SCANCODE_D]) { dx -= sin(player.angle)*move; dy += cos(player.angle)*move; }
            if (dx != 0 || dy != 0) player.moveWithSliding(dx, dy, lines, vertices);
            if (keys[SDL_SCANCODE_LEFT]) player.angle -= turn;
            if (keys[SDL_SCANCODE_RIGHT]) player.angle += turn;
            while (player.angle < 0) player.angle += 2*M_PI;
            while (player.angle >= 2*M_PI) player.angle -= 2*M_PI;

            if (shootFlashFrames > 0) shootFlashFrames--;

            const float PICKUP_RADIUS = 32.0f;
            for (auto& pickup : pickups) {
                if (!pickup.active) continue;
                float dxPickup = pickup.x - player.x;
                float dyPickup = pickup.y - player.y;
                float dist = sqrtf(dxPickup*dxPickup + dyPickup*dyPickup);
                if (dist < PICKUP_RADIUS) {
                    pickup.active = false;
                    if (pickup.type == 2011) {
                        playerHealth = std::min(playerMaxHealth, playerHealth + 25);
                    } else if (pickup.type == 2007) {
                        pistolAmmo += 20;
                    } else if (pickup.type == 2008) {
                        shotgunAmmo += 6;
                    }
                }
            }

            for (auto& mob : mobs) {
                auto& state = mobRenderStates[&mob];

                if (state.attackAnimTime > 0) state.attackAnimTime -= dt;

                if (!mob.isAlive() && !state.isDying) {
                    state.isDying = true;
                    state.deathTimer = 0.5f;
                }

                if (!state.isDying && mob.isAlive()) {
                    float oldAttackCooldown = mob.attackCooldown;
                    float oldFireCooldown = mob.fireCooldown;

                    std::optional<EnemyProjectile> proj = mob.update(dt, player, lines, vertices);

                    bool isAttackingNow = false;

                    if (proj.has_value()) {
                        isAttackingNow = true;
                    }
                    else if (mob.attackCooldown > 0 && oldAttackCooldown <= 0) {
                        isAttackingNow = true;
                    }
                    else if (mob.fireCooldown > 0 && oldFireCooldown <= 0 && mob.type != ZOMBIE) {
                        isAttackingNow = true;
                    }

                    if (isAttackingNow) state.attackAnimTime = 0.2f;

                    if (mob.type == BOSS && mob.isAlive() && mob.canSummon() && isAttackingNow) {
                        auto summoned = mob.summonMobsAround();
                        for (auto& newMob : summoned) {
                            mobs.push_back(newMob);
                        }
                        mob.isSummoning = true;
                        state.isSummoning = true;
                        state.summonAnimTime = 0.3f;
                        state.attackAnimTime = 0.3f;
                        mob.resetSummonCooldown();
                    }

                    mob.tryAttack(player, dt, playerHealth);
                    if (playerHealth <= 0) {
                        running = false;
                        levelComplete = true;
                        break;
                    }
                }
            }

            for (auto& mob : mobs) {
                if (mob.type == BOSS && mob.vulnerabilityTimer > 0.0f) {
                    mob.vulnerabilityTimer -= dt;
                    if (mob.vulnerabilityTimer <= 0.0f) {
                        mob.invulnerable = false;
                    }
                }
            }

            for (auto it = mobs.begin(); it != mobs.end(); ) {
                bool wasAlive = it->isAlive();
                if (!wasAlive) {
                    for (auto& mob : mobs) {
                        if (mob.type == BOSS && mob.summonedMobsCount > 0) {
                            mob.summonedMobsCount--;
                            if (mob.summonedMobsCount == 0 && mob.invulnerable) {
                                mob.invulnerable = false;
                                mob.vulnerabilityTimer = 1.0f;
                            }
                            break;
                        }
                    }
                    it = mobs.erase(it);
                } else {
                    ++it;
                }
            }

            if (mobs.empty() && !levelComplete) {
                levelComplete = true;
                break;
            }

            for (auto it = bulletHoles.begin(); it != bulletHoles.end(); ) {
                it->lifetime--;
                if (it->lifetime <= 0) it = bulletHoles.erase(it);
                else ++it;
            }
            for (auto it = hitFlashes.begin(); it != hitFlashes.end(); ) {
                it->lifetime--;
                if (it->lifetime <= 0) it = hitFlashes.erase(it);
                else ++it;
            }

            SDL_SetRenderDrawColor(renderer, 0,0,0,255);
            SDL_RenderClear(renderer);

            SDL_SetRenderDrawColor(renderer, 26, 20, 16, 255);
            SDL_FRect floorRect = {0, (float)WINDOW_HEIGHT/2, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT/2};
            SDL_RenderFillRect(renderer, &floorRect);
            SDL_SetRenderDrawColor(renderer, 43, 43, 43, 255);
            SDL_FRect ceilingRect = {0, 0, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT/2};
            SDL_RenderFillRect(renderer, &ceilingRect);

            int chunk = RENDER_WIDTH / numThreads;
            std::vector<std::future<std::vector<ColumnResult>>> futures;
            for (unsigned int i = 0; i < numThreads; ++i) {
                int startX = i * chunk;
                int endX = (i == numThreads - 1) ? RENDER_WIDTH : (i + 1) * chunk;
                std::packaged_task<std::vector<ColumnResult>()> task(
                    [startX, endX, &player, &lines, &vertices, &sidedefs, &sectors, RENDER_WIDTH, WINDOW_HEIGHT]() {
                        return renderColumnsRange(startX, endX, player, lines, vertices, sidedefs, sectors, RENDER_WIDTH, WINDOW_HEIGHT);
                    });
                futures.push_back(task.get_future());
                threadPool.enqueue(std::move(task));
            }

            std::vector<ColumnResult> allColumns;
            for (auto& fut : futures) {
                auto slice = fut.get();
                allColumns.insert(allColumns.end(), slice.begin(), slice.end());
            }
            std::sort(allColumns.begin(), allColumns.end(),
                      [](const ColumnResult& a, const ColumnResult& b) { return a.x < b.x; });

            for (const auto& col : allColumns) {
                if (col.wallTop >= col.wallBottom) continue;
                int texX = (int)(col.hitU * wallTex.width) % wallTex.width;
                if (texX < 0) texX += wallTex.width;
                int screenX = col.x * RENDER_SCALE;
                for (int w = 0; w < RENDER_SCALE; w++) {
                    drawWallColumn(renderer, screenX + w, col.wallTop, col.wallBottom, texX, wallTex, col.distance);
                }
            }

            for (const auto& hole : bulletHoles) {
                if (!isPointVisible(player.x, player.y, hole.x, hole.y, lines, vertices)) continue;
                float dxTo = hole.x - player.x;
                float dyTo = hole.y - player.y;
                float angleToPoint = atan2(dyTo, dxTo);
                float angleDiff = angleToPoint - player.angle;
                while (angleDiff < -M_PI) angleDiff += 2*M_PI;
                while (angleDiff > M_PI) angleDiff -= 2*M_PI;
                float screenX = (angleDiff / (FOV/2)) * (WINDOW_WIDTH/2) + WINDOW_WIDTH/2;
                if (screenX < 0 || screenX >= WINDOW_WIDTH) continue;
                float dist = std::sqrt(dxTo*dxTo + dyTo*dyTo);
                float spriteHeight = 12.0f * 200.0f / dist;
                if (spriteHeight < 4) spriteHeight = 4;
                float screenY = WINDOW_HEIGHT/2 - spriteHeight/2;
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                SDL_FRect rect = {screenX - spriteHeight/2, screenY, spriteHeight, spriteHeight};
                SDL_RenderFillRect(renderer, &rect);
            }

            for (const auto& flash : hitFlashes) {
                if (!isPointVisible(player.x, player.y, flash.x, flash.y, lines, vertices)) continue;
                float dxTo = flash.x - player.x;
                float dyTo = flash.y - player.y;
                float angleToPoint = atan2(dyTo, dxTo);
                float angleDiff = angleToPoint - player.angle;
                while (angleDiff < -M_PI) angleDiff += 2*M_PI;
                while (angleDiff > M_PI) angleDiff -= 2*M_PI;
                float screenX = (angleDiff / (FOV/2)) * (WINDOW_WIDTH/2) + WINDOW_WIDTH/2;
                if (screenX < 0 || screenX >= WINDOW_WIDTH) continue;
                float dist = std::sqrt(dxTo*dxTo + dyTo*dyTo);
                float spriteHeight = 18.0f * 200.0f / dist;
                if (spriteHeight < 8) spriteHeight = 8;
                float screenY = WINDOW_HEIGHT/2 - spriteHeight/2;
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                SDL_FRect rect = {screenX - spriteHeight/2, screenY, spriteHeight, spriteHeight};
                SDL_RenderFillRect(renderer, &rect);
            }

            for (const auto& pickup : pickups) {
                if (!pickup.active) continue;
                if (!isPointVisible(player.x, player.y, pickup.x, pickup.y, lines, vertices)) continue;

                float dxTo = pickup.x - player.x;
                float dyTo = pickup.y - player.y;
                float distToPlayer = sqrtf(dxTo*dxTo + dyTo*dyTo);
                float angleToPoint = atan2(dyTo, dxTo);
                float angleDiff = angleToPoint - player.angle;
                while (angleDiff < -M_PI) angleDiff += 2*M_PI;
                while (angleDiff > M_PI) angleDiff -= 2*M_PI;
                float screenX = (angleDiff / (FOV/2)) * (WINDOW_WIDTH/2) + WINDOW_WIDTH/2;
                if (screenX < 0 || screenX >= WINDOW_WIDTH) continue;

                float spriteHeight = 64.0f * 200.0f / distToPlayer;
                if (spriteHeight < 24) spriteHeight = 24;
                float spriteWidth = spriteHeight;
                float screenY = WINDOW_HEIGHT/2 + (56.0f / distToPlayer) * (WINDOW_HEIGHT / 1.5f) * 0.7f - spriteHeight;

                const Texture* pickupTex = nullptr;
                if (pickup.type == 2011) pickupTex = &firstAidKitTex;
                else if (pickup.type == 2007) pickupTex = &pistolCartridgesTex;
                else if (pickup.type == 2008) pickupTex = &shotgunCartridgesTex;

                if (pickupTex && pickupTex->texture) {
                    SDL_SetTextureBlendMode(pickupTex->texture, SDL_BLENDMODE_BLEND);
                    SDL_FRect srcRect = {0, 0, (float)pickupTex->width, (float)pickupTex->height};
                    SDL_FRect dstRect = {screenX - spriteWidth/2, screenY, spriteWidth, spriteHeight};
                    SDL_RenderTexture(renderer, pickupTex->texture, &srcRect, &dstRect);
                }
            }

            std::vector<std::reference_wrapper<Mob>> sortedMobs(mobs.begin(), mobs.end());
            std::sort(sortedMobs.begin(), sortedMobs.end(),
                      [&player](const Mob& a, const Mob& b) {
                          float da = (a.x-player.x)*(a.x-player.x)+(a.y-player.y)*(a.y-player.y);
                          float db = (b.x-player.x)*(b.x-player.x)+(b.y-player.y)*(b.y-player.y);
                          return da > db;
                      });

            const float MOB_HEIGHT_WORLD = 72.0f;
            const float VIEW_HEIGHT = 41.0f;
            const float FLOOR_HEIGHT = 0.0f;
            const float PROJ_SCALE = (float)WINDOW_HEIGHT / 1.5f;

            for (const auto& mobRef : sortedMobs) {
                const Mob& mob = mobRef.get();
                if (!isPointVisible(player.x, player.y, mob.x, mob.y, lines, vertices)) continue;

                float dx = mob.x - player.x;
                float dy = mob.y - player.y;
                float dist = sqrtf(dx*dx + dy*dy);
                if (dist < 0.01f) continue;

                float angle = atan2(dy, dx);
                float angleDiff = angle - player.angle;
                while (angleDiff < -M_PI) angleDiff += 2*M_PI;
                while (angleDiff > M_PI) angleDiff -= 2*M_PI;
                float screenX = (angleDiff / (FOV/2)) * (WINDOW_WIDTH/2) + WINDOW_WIDTH/2;
                if (screenX < 0 || screenX >= WINDOW_WIDTH) continue;

                float screenHeight = (MOB_HEIGHT_WORLD / dist) * PROJ_SCALE;
                if (screenHeight < 8) screenHeight = 8;
                if (screenHeight > WINDOW_HEIGHT) screenHeight = WINDOW_HEIGHT;

                float y_floor = WINDOW_HEIGHT/2 + (VIEW_HEIGHT - FLOOR_HEIGHT) / dist * PROJ_SCALE;
                float topY = y_floor - screenHeight;
                float bottomY = y_floor;
                if (topY < 0) topY = 0;
                if (bottomY > WINDOW_HEIGHT) bottomY = WINDOW_HEIGHT;
                if (topY >= bottomY) continue;

                float screenWidth = screenHeight * 0.7f;

                const Texture* mobTex = nullptr;
                auto it = mobRenderStates.find(const_cast<Mob*>(&mob));
                bool isDying = (it != mobRenderStates.end() && it->second.isDying);
                bool isAttacking = (it != mobRenderStates.end() && it->second.attackAnimTime > 0 && !isDying);

                if (mob.type == BOSS) {
                    if (isDying && bossDeathTex.texture) {
                        mobTex = &bossDeathTex;
                    } else if ((isAttacking || (it != mobRenderStates.end() && it->second.isSummoning)) && bossAttackTex.texture) {
                        mobTex = &bossAttackTex;
                    } else if (bossWalkingTex.texture) {
                        mobTex = &bossWalkingTex;
                    }
                } else {
                    if (isDying && mobDeathTex.texture) {
                        mobTex = &mobDeathTex;
                    } else if (isAttacking && mobAttackTex.texture) {
                        mobTex = &mobAttackTex;
                    } else {
                        mobTex = &mobWalkingTex;
                    }
                }

                if (mobTex && mobTex->texture) {
                    SDL_SetTextureBlendMode(mobTex->texture, SDL_BLENDMODE_BLEND);
                    SDL_FRect srcRect = {0, 0, (float)mobTex->width, (float)mobTex->height};
                    SDL_FRect dstRect = {screenX - screenWidth/2, topY, screenWidth, screenHeight};
                    SDL_RenderTexture(renderer, mobTex->texture, &srcRect, &dstRect);
                } else {
                    SDL_Color baseColor;
                    switch (mob.type) {
                        case ZOMBIE: baseColor = {0, 200, 0, 255}; break;
                        case IMP:    baseColor = {200, 100, 0, 255}; break;
                        case DEMON:  baseColor = {200, 0, 0, 255}; break;
                        case BOSS:   baseColor = {150, 0, 150, 255}; break;
                    }
                    float factor = (float)mob.hp / mob.maxHp;
                    SDL_SetRenderDrawColor(renderer,
                        (Uint8)(baseColor.r * factor),
                        (Uint8)(baseColor.g * factor),
                        (Uint8)(baseColor.b * factor), 255);
                    SDL_FRect rect = {screenX - screenWidth/2, topY, screenWidth, screenHeight};
                    SDL_RenderFillRect(renderer, &rect);
                }
            }

            int weaponW = 500;
            int weaponH = 300;
            int weaponX = (WINDOW_WIDTH - weaponW) / 2;
            int weaponY = WINDOW_HEIGHT - weaponH - 20;

            const Texture* currentWeaponTex = nullptr;
            if (currentWeapon == 0) {
                currentWeaponTex = (shootFlashFrames > 0) ? &shootingGunTex : &gunTex;
            } else {
                currentWeaponTex = (shootFlashFrames > 0) ? &shotgunShotTex : &shotgunTex;
            }

            if (currentWeaponTex && currentWeaponTex->texture) {
                SDL_FRect srcRect = {0, 0, (float)currentWeaponTex->width, (float)currentWeaponTex->height};
                SDL_FRect dstRect = {(float)weaponX, (float)weaponY, (float)weaponW, (float)weaponH};
                SDL_RenderTexture(renderer, currentWeaponTex->texture, &srcRect, &dstRect);
            }

            std::string hudText = "HP: " + std::to_string(playerHealth) + "  ";
            if (currentWeapon == 0) hudText += "PISTOL (" + std::to_string(pistolAmmo) + ")";
            else hudText += "SHOTGUN (" + std::to_string(shotgunAmmo) + ")";
            hudText += "  FPS: " + std::to_string(currentFPS);

            SDL_Surface* hudSurf = TTF_RenderText_Solid(smallFont, hudText.c_str(), 0, {255,255,255,255});
            if (hudSurf) {
                SDL_Texture* hudTex = SDL_CreateTextureFromSurface(renderer, hudSurf);
                SDL_FRect hudRect = {20, 20, (float)hudSurf->w, (float)hudSurf->h};
                SDL_RenderTexture(renderer, hudTex, NULL, &hudRect);
                SDL_DestroyTexture(hudTex);
                SDL_DestroySurface(hudSurf);
            }

            SDL_RenderPresent(renderer);
            SDL_Delay(15);
        }

        if (playerHealth <= 0) break;
        currentLevelIdx++;
    }

    SDL_DestroyTexture(wallTex.texture);
    SDL_DestroyTexture(mobWalkingTex.texture);
    SDL_DestroyTexture(mobAttackTex.texture);
    SDL_DestroyTexture(mobDeathTex.texture);
    SDL_DestroyTexture(bossWalkingTex.texture);
    SDL_DestroyTexture(bossAttackTex.texture);
    SDL_DestroyTexture(bossDeathTex.texture);
    SDL_DestroyTexture(gunTex.texture);
    SDL_DestroyTexture(shootingGunTex.texture);
    SDL_DestroyTexture(shotgunTex.texture);
    SDL_DestroyTexture(shotgunShotTex.texture);
    SDL_DestroyTexture(firstAidKitTex.texture);
    SDL_DestroyTexture(pistolCartridgesTex.texture);
    SDL_DestroyTexture(shotgunCartridgesTex.texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    if (smallFont != font) TTF_CloseFont(smallFont);
    TTF_Quit();
    SDL_Quit();
    return 0;
}