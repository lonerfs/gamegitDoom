#include <iostream>
#include <vector>
#include <list>
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

struct BulletHole {
    float x, y;
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

struct AchievementNotification {
    std::string text;
    float lifetime;
    float maxLifetime;
};

struct Texture {
    SDL_Texture* texture;
    int width;
    int height;
    std::vector<Uint32> pixelData;
};

struct GameSettings {
    int windowWidth = 1280;
    int windowHeight = 960;
    int renderScale = 2;
    bool soundEnabled = true;
    int soundVolume = 100;
};

GameSettings g_settings;

void saveSettings() {
    std::ofstream file("settings.dat", std::ios::binary);
    if (file) {
        file.write((char*)&g_settings, sizeof(g_settings));
    }
}

void loadSettings() {
    std::ifstream file("settings.dat", std::ios::binary);
    if (file) {
        file.read((char*)&g_settings, sizeof(g_settings));
    } else {
        saveSettings();
    }
}

struct SaveData {
    int level;
    float playerX, playerY, playerAngle;
    int playerHealth;
    int pistolAmmo, shotgunAmmo;
    int pistolMag, shotgunMag;
    int currentWeapon;
    std::vector<std::tuple<float, float, int, int>> mobs;
    std::vector<std::tuple<float, float, int, bool>> pickups;
};

void saveGame(const SaveData& save, const std::string& filename = "savegame.dat") {
    std::ofstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to save game!" << std::endl;
        return;
    }
    file.write((char*)&save.level, sizeof(save.level));
    file.write((char*)&save.playerX, sizeof(save.playerX));
    file.write((char*)&save.playerY, sizeof(save.playerY));
    file.write((char*)&save.playerAngle, sizeof(save.playerAngle));
    file.write((char*)&save.playerHealth, sizeof(save.playerHealth));
    file.write((char*)&save.pistolAmmo, sizeof(save.pistolAmmo));
    file.write((char*)&save.shotgunAmmo, sizeof(save.shotgunAmmo));
    file.write((char*)&save.pistolMag, sizeof(save.pistolMag));
    file.write((char*)&save.shotgunMag, sizeof(save.shotgunMag));
    file.write((char*)&save.currentWeapon, sizeof(save.currentWeapon));

    size_t mobCount = save.mobs.size();
    file.write((char*)&mobCount, sizeof(mobCount));
    for (const auto& mob : save.mobs) {
        float x, y; int type, hp;
        std::tie(x, y, type, hp) = mob;
        file.write((char*)&x, sizeof(x));
        file.write((char*)&y, sizeof(y));
        file.write((char*)&type, sizeof(type));
        file.write((char*)&hp, sizeof(hp));
    }

    size_t pickupCount = save.pickups.size();
    file.write((char*)&pickupCount, sizeof(pickupCount));
    for (const auto& p : save.pickups) {
        float x, y; int type; bool active;
        std::tie(x, y, type, active) = p;
        file.write((char*)&x, sizeof(x));
        file.write((char*)&y, sizeof(y));
        file.write((char*)&type, sizeof(type));
        file.write((char*)&active, sizeof(active));
    }
    file.close();
    std::cout << "Game saved!" << std::endl;
}

bool loadGame(SaveData& save, const std::string& filename = "savegame.dat") {
    std::ifstream file(filename, std::ios::binary);
    if (!file) return false;

    file.read((char*)&save.level, sizeof(save.level));
    file.read((char*)&save.playerX, sizeof(save.playerX));
    file.read((char*)&save.playerY, sizeof(save.playerY));
    file.read((char*)&save.playerAngle, sizeof(save.playerAngle));
    file.read((char*)&save.playerHealth, sizeof(save.playerHealth));
    file.read((char*)&save.pistolAmmo, sizeof(save.pistolAmmo));
    file.read((char*)&save.shotgunAmmo, sizeof(save.shotgunAmmo));
    file.read((char*)&save.pistolMag, sizeof(save.pistolMag));
    file.read((char*)&save.shotgunMag, sizeof(save.shotgunMag));
    file.read((char*)&save.currentWeapon, sizeof(save.currentWeapon));

    size_t mobCount;
    file.read((char*)&mobCount, sizeof(mobCount));
    save.mobs.clear();
    for (size_t i = 0; i < mobCount; i++) {
        float x, y; int type, hp;
        file.read((char*)&x, sizeof(x));
        file.read((char*)&y, sizeof(y));
        file.read((char*)&type, sizeof(type));
        file.read((char*)&hp, sizeof(hp));
        save.mobs.emplace_back(x, y, type, hp);
    }

    size_t pickupCount;
    file.read((char*)&pickupCount, sizeof(pickupCount));
    save.pickups.clear();
    for (size_t i = 0; i < pickupCount; i++) {
        float x, y; int type; bool active;
        file.read((char*)&x, sizeof(x));
        file.read((char*)&y, sizeof(y));
        file.read((char*)&type, sizeof(type));
        file.read((char*)&active, sizeof(active));
        save.pickups.emplace_back(x, y, type, active);
    }
    file.close();
    std::cout << "Game loaded from level " << save.level << std::endl;
    return true;
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
                if (r > 240 && g > 240 && b > 240) {
                    tex.pixelData[i] = 0;
                } else {
                    tex.pixelData[i] = (0xFF << 24) | (r << 16) | (g << 8) | b;
                }
            }
            SDL_Surface* modifiedSurf = SDL_CreateSurface(tex.width, tex.height, SDL_PIXELFORMAT_RGBA32);
            memcpy(modifiedSurf->pixels, tex.pixelData.data(), tex.width * tex.height * 4);
            tex.texture = SDL_CreateTextureFromSurface(renderer, modifiedSurf);
            SDL_SetTextureBlendMode(tex.texture, SDL_BLENDMODE_BLEND);
            SDL_DestroySurface(modifiedSurf);
            SDL_DestroySurface(converted);
        }
        SDL_DestroySurface(surf);
        if (tex.texture) {
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
        SDL_SetTextureBlendMode(tex.texture, SDL_BLENDMODE_BLEND);
        tex.width = tex.height = 64;
        SDL_DestroySurface(dummy);
    }
    return tex;
}

void drawWallColumn(SDL_Renderer* renderer, int x, int top, int bottom, int texX, const Texture& tex, int width) {
    if (top >= bottom || !tex.texture || width <= 0) return;
    SDL_FRect srcRect;
    srcRect.x = (float)texX;
    srcRect.y = 0;
    srcRect.w = 1.0f;
    srcRect.h = (float)tex.height;
    SDL_FRect dstRect;
    dstRect.x = (float)x;
    dstRect.y = (float)top;
    dstRect.w = (float)width;
    dstRect.h = (float)(bottom - top);
    SDL_RenderTexture(renderer, tex.texture, &srcRect, &dstRect);
}

bool rayIntersectsMob(float x0, float y0, float dx, float dy,
                      const Mob& mob, float& dist, float& hitX, float& hitY) {
    float half = mob.size / 2.0f;
    float left = mob.x - half;
    float right = mob.x + half;
    float bottom = mob.y - half;
    float top = mob.y + half;
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
                 std::list<Mob>& mobs,
                 std::vector<BulletHole>& bulletHoles,
                 std::vector<HitFlash>& hitFlashes,
                 int& pistolAmmo, int& shotgunAmmo,
                 int& pistolMag, int& shotgunMag,
                 int currentWeapon,
                 int& playerHealth, int playerMaxHealth) {
    int pellets = (currentWeapon == 0) ? 1 : 5;
    float spread = (currentWeapon == 0) ? 0.0f : 0.12f;
    int ammoInMag = (currentWeapon == 0) ? pistolMag : shotgunMag;
    if (ammoInMag <= 0) return;

    if (currentWeapon == 0) pistolMag--;
    else shotgunMag--;

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
                    hitX = ix;
                    hitY = iy;
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
            int damage = (currentWeapon == 0) ? 25 : 15;
            hitMob->takeDamage(damage);
            hitFlashes.push_back({hitX, hitY, 6});
        } else if (closestDist < 1000000.0f) {
            bulletHoles.push_back({hitX, hitY, 150});
        }
    }
}

void reloadWeapon(int currentWeapon, int& pistolAmmo, int& shotgunAmmo, int& pistolMag, int& shotgunMag) {
    if (currentWeapon == 0) {
        int needed = 7 - pistolMag;
        int toReload = std::min(needed, pistolAmmo);
        pistolMag += toReload;
        pistolAmmo -= toReload;
        std::cout << "Pistol reloaded: " << pistolMag << "/7" << std::endl;
    } else {
        int needed = 2 - shotgunMag;
        int toReload = std::min(needed, shotgunAmmo);
        shotgunMag += toReload;
        shotgunAmmo -= toReload;
        std::cout << "Shotgun reloaded: " << shotgunMag << "/2" << std::endl;
    }
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

bool showGameOverScreen(SDL_Renderer* renderer, TTF_Font* font, int windowWidth, int windowHeight) {
    bool waiting = true;
    bool restart = false;
    SDL_Event event;
    while (waiting) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) return false;
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_R) { restart = true; waiting = false; }
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) return false;
            }
        }
        SDL_SetRenderDrawColor(renderer, 0,0,0,255);
        SDL_RenderClear(renderer);
        SDL_Surface* titleSurf = TTF_RenderText_Solid(font, "GAME OVER", 0, SDL_Color{255,0,0,255});
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
            SDL_FRect titleRect = {(float)(windowWidth/2 - titleSurf->w/2), (float)(windowHeight/2 - 150), (float)titleSurf->w, (float)titleSurf->h};
            SDL_RenderTexture(renderer, titleTex, NULL, &titleRect);
            SDL_DestroyTexture(titleTex);
            SDL_DestroySurface(titleSurf);
        }
        SDL_Surface* restartSurf = TTF_RenderText_Solid(font, "Press R to restart", 0, SDL_Color{255,255,255,255});
        if (restartSurf) {
            SDL_Texture* restartTex = SDL_CreateTextureFromSurface(renderer, restartSurf);
            SDL_FRect textRect = {(float)(windowWidth/2 - restartSurf->w/2), (float)(windowHeight/2 + 50), (float)restartSurf->w, (float)restartSurf->h};
            SDL_RenderTexture(renderer, restartTex, NULL, &textRect);
            SDL_DestroyTexture(restartTex);
            SDL_DestroySurface(restartSurf);
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    return restart;
}

bool showVictoryScreen(SDL_Renderer* renderer, TTF_Font* font, TTF_Font* smallFont, int windowWidth, int windowHeight, int monsters, int ammo, int dmg) {
    bool waiting = true;
    bool restart = false;
    SDL_Event event;
    while (waiting) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) return false;
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_R || event.key.scancode == SDL_SCANCODE_RETURN) { restart = true; waiting = false; }
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) return false;
            }
        }
        SDL_SetRenderDrawColor(renderer, 0,0,0,255);
        SDL_RenderClear(renderer);

        SDL_Surface* titleSurf = TTF_RenderText_Solid(font, "VICTORY!", 0, SDL_Color{255,215,0,255});
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
            SDL_FRect titleRect = {(float)(windowWidth/2 - titleSurf->w/2), (float)(windowHeight/2 - 200), (float)titleSurf->w, (float)titleSurf->h};
            SDL_RenderTexture(renderer, titleTex, NULL, &titleRect);
            SDL_DestroyTexture(titleTex);
            SDL_DestroySurface(titleSurf);
        }

        char statText[128];
        snprintf(statText, sizeof(statText), "Monsters Killed: %d", monsters);
        SDL_Surface* mSurf = TTF_RenderText_Solid(smallFont, statText, 0, {255,255,255,255});
        if (mSurf) {
            SDL_Texture* mTex = SDL_CreateTextureFromSurface(renderer, mSurf);
            SDL_FRect mRect = {(float)(windowWidth/2 - mSurf->w/2), (float)(windowHeight/2 - 100), (float)mSurf->w, (float)mSurf->h};
            SDL_RenderTexture(renderer, mTex, NULL, &mRect);
            SDL_DestroyTexture(mTex); SDL_DestroySurface(mSurf);
        }

        snprintf(statText, sizeof(statText), "Ammo Spent: %d", ammo);
        SDL_Surface* aSurf = TTF_RenderText_Solid(smallFont, statText, 0, {255,255,255,255});
        if (aSurf) {
            SDL_Texture* aTex = SDL_CreateTextureFromSurface(renderer, aSurf);
            SDL_FRect aRect = {(float)(windowWidth/2 - aSurf->w/2), (float)(windowHeight/2 - 60), (float)aSurf->w, (float)aSurf->h};
            SDL_RenderTexture(renderer, aTex, NULL, &aRect);
            SDL_DestroyTexture(aTex); SDL_DestroySurface(aSurf);
        }

        snprintf(statText, sizeof(statText), "Damage Taken: %d", dmg);
        SDL_Surface* dSurf = TTF_RenderText_Solid(smallFont, statText, 0, {255,255,255,255});
        if (dSurf) {
            SDL_Texture* dTex = SDL_CreateTextureFromSurface(renderer, dSurf);
            SDL_FRect dRect = {(float)(windowWidth/2 - dSurf->w/2), (float)(windowHeight/2 - 20), (float)dSurf->w, (float)dSurf->h};
            SDL_RenderTexture(renderer, dTex, NULL, &dRect);
            SDL_DestroyTexture(dTex); SDL_DestroySurface(dSurf);
        }

        SDL_Surface* restartSurf = TTF_RenderText_Solid(font, "Press R or ENTER to go to Main Menu", 0, SDL_Color{150,150,150,255});
        if (restartSurf) {
            SDL_Texture* restartTex = SDL_CreateTextureFromSurface(renderer, restartSurf);
            SDL_FRect textRect = {(float)(windowWidth/2 - restartSurf->w/2), (float)(windowHeight/2 + 80), (float)restartSurf->w, (float)restartSurf->h};
            SDL_RenderTexture(renderer, restartTex, NULL, &textRect);
            SDL_DestroyTexture(restartTex);
            SDL_DestroySurface(restartSurf);
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    return restart;
}

bool showLevelCompleteScreen(SDL_Renderer* renderer, TTF_Font* font, int windowWidth, int windowHeight, int currentLevel) {
    bool waiting = true;
    bool next = false;
    SDL_Event event;
    while (waiting) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) return false;
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_RETURN || event.key.scancode == SDL_SCANCODE_SPACE) { next = true; waiting = false; }
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) return false;
            }
        }
        SDL_SetRenderDrawColor(renderer,0,0,0,255);
        SDL_RenderClear(renderer);
        char titleText[64];
        snprintf(titleText, sizeof(titleText), "LEVEL %d COMPLETE!", currentLevel);
        SDL_Surface* titleSurf = TTF_RenderText_Solid(font, titleText, 0, SDL_Color{0,255,0,255});
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
            SDL_FRect titleRect = {(float)(windowWidth/2 - titleSurf->w/2), (float)(windowHeight/2 - 150), (float)titleSurf->w, (float)titleSurf->h};
            SDL_RenderTexture(renderer, titleTex, NULL, &titleRect);
            SDL_DestroyTexture(titleTex);
            SDL_DestroySurface(titleSurf);
        }
        SDL_Surface* nextSurf = TTF_RenderText_Solid(font, "Press ENTER for next level", 0, SDL_Color{255,255,255,255});
        if (nextSurf) {
            SDL_Texture* nextTex = SDL_CreateTextureFromSurface(renderer, nextSurf);
            SDL_FRect textRect = {(float)(windowWidth/2 - nextSurf->w/2), (float)(windowHeight/2 + 50), (float)nextSurf->w, (float)nextSurf->h};
            SDL_RenderTexture(renderer, nextTex, NULL, &textRect);
            SDL_DestroyTexture(nextTex);
            SDL_DestroySurface(nextSurf);
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    return next;
}

void showSettingsMenu(SDL_Renderer* renderer, TTF_Font* font, int& windowWidth, int& windowHeight, SDL_Window* window) {
    const std::vector<std::pair<int,int>> resolutions = {{1280,960}, {1024,768}, {800,600}};
    int resIndex = 0;
    for (size_t i=0; i<resolutions.size(); ++i) {
        if (resolutions[i].first == g_settings.windowWidth && resolutions[i].second == g_settings.windowHeight) {
            resIndex = i;
            break;
        }
    }
    int renderScaleIndex = g_settings.renderScale == 1 ? 0 : (g_settings.renderScale == 2 ? 1 : 2);
    bool soundEnabled = g_settings.soundEnabled;
    bool menuRunning = true;
    SDL_Event event;

    while (menuRunning) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) { menuRunning = false; }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) menuRunning = false;
                if (event.key.scancode == SDL_SCANCODE_UP) {
                    resIndex = (resIndex - 1 + resolutions.size()) % resolutions.size();
                    g_settings.windowWidth = resolutions[resIndex].first;
                    g_settings.windowHeight = resolutions[resIndex].second;
                    SDL_SetWindowSize(window, g_settings.windowWidth, g_settings.windowHeight);
                    windowWidth = g_settings.windowWidth;
                    windowHeight = g_settings.windowHeight;
                    saveSettings();
                }
                if (event.key.scancode == SDL_SCANCODE_DOWN) {
                    resIndex = (resIndex + 1) % resolutions.size();
                    g_settings.windowWidth = resolutions[resIndex].first;
                    g_settings.windowHeight = resolutions[resIndex].second;
                    SDL_SetWindowSize(window, g_settings.windowWidth, g_settings.windowHeight);
                    windowWidth = g_settings.windowWidth;
                    windowHeight = g_settings.windowHeight;
                    saveSettings();
                }
                if (event.key.scancode == SDL_SCANCODE_LEFT) {
                    renderScaleIndex = (renderScaleIndex - 1 + 3) % 3;
                    g_settings.renderScale = (renderScaleIndex == 0 ? 1 : (renderScaleIndex == 1 ? 2 : 3));
                    saveSettings();
                }
                if (event.key.scancode == SDL_SCANCODE_RIGHT) {
                    renderScaleIndex = (renderScaleIndex + 1) % 3;
                    g_settings.renderScale = (renderScaleIndex == 0 ? 1 : (renderScaleIndex == 1 ? 2 : 3));
                    saveSettings();
                }
                if (event.key.scancode == SDL_SCANCODE_S) {
                    soundEnabled = !soundEnabled;
                    g_settings.soundEnabled = soundEnabled;
                    saveSettings();
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0,0,0,255);
        SDL_RenderClear(renderer);

        SDL_Surface* titleSurf = TTF_RenderText_Solid(font, "SETTINGS", 0, SDL_Color{255,255,255,255});
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
            SDL_FRect titleRect = {(float)(windowWidth/2 - titleSurf->w/2), 50, (float)titleSurf->w, (float)titleSurf->h};
            SDL_RenderTexture(renderer, titleTex, NULL, &titleRect);
            SDL_DestroyTexture(titleTex);
            SDL_DestroySurface(titleSurf);
        }

        char resText[128];
        snprintf(resText, sizeof(resText), "Resolution: %dx%d (Use UP/DOWN)", g_settings.windowWidth, g_settings.windowHeight);
        SDL_Surface* resSurf = TTF_RenderText_Solid(font, resText, 0, SDL_Color{200,200,200,255});
        if (resSurf) {
            SDL_Texture* resTex = SDL_CreateTextureFromSurface(renderer, resSurf);
            SDL_FRect resRect = {(float)(windowWidth/2 - resSurf->w/2), 150, (float)resSurf->w, (float)resSurf->h};
            SDL_RenderTexture(renderer, resTex, NULL, &resRect);
            SDL_DestroyTexture(resTex);
            SDL_DestroySurface(resSurf);
        }

        char scaleText[128];
        snprintf(scaleText, sizeof(scaleText), "Render Scale: %d (LEFT/RIGHT)", g_settings.renderScale);
        SDL_Surface* scaleSurf = TTF_RenderText_Solid(font, scaleText, 0, SDL_Color{200,200,200,255});
        if (scaleSurf) {
            SDL_Texture* scaleTex = SDL_CreateTextureFromSurface(renderer, scaleSurf);
            SDL_FRect scaleRect = {(float)(windowWidth/2 - scaleSurf->w/2), 220, (float)scaleSurf->w, (float)scaleSurf->h};
            SDL_RenderTexture(renderer, scaleTex, NULL, &scaleRect);
            SDL_DestroyTexture(scaleTex);
            SDL_DestroySurface(scaleSurf);
        }

        char soundText[128];
        snprintf(soundText, sizeof(soundText), "Sound: %s (Press S to toggle)", soundEnabled ? "ON" : "OFF");
        SDL_Surface* soundSurf = TTF_RenderText_Solid(font, soundText, 0, SDL_Color{200,200,200,255});
        if (soundSurf) {
            SDL_Texture* soundTex = SDL_CreateTextureFromSurface(renderer, soundSurf);
            SDL_FRect soundRect = {(float)(windowWidth/2 - soundSurf->w/2), 290, (float)soundSurf->w, (float)soundSurf->h};
            SDL_RenderTexture(renderer, soundTex, NULL, &soundRect);
            SDL_DestroyTexture(soundTex);
            SDL_DestroySurface(soundSurf);
        }

        SDL_Surface* backSurf = TTF_RenderText_Solid(font, "Press ESC to return", 0, SDL_Color{150,150,150,255});
        if (backSurf) {
            SDL_Texture* backTex = SDL_CreateTextureFromSurface(renderer, backSurf);
            SDL_FRect backRect = {(float)(windowWidth/2 - backSurf->w/2), 400, (float)backSurf->w, (float)backSurf->h};
            SDL_RenderTexture(renderer, backTex, NULL, &backRect);
            SDL_DestroyTexture(backTex);
            SDL_DestroySurface(backSurf);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
}

int showMainMenu(SDL_Renderer* renderer, TTF_Font* font, int windowWidth, int windowHeight, SDL_Window* window) {
    struct MenuButton {
        SDL_FRect rect;
        std::string label;
        bool hover;
    };
    MenuButton newGameBtn = {{(float)(windowWidth/2-100), (float)(windowHeight/2-80), 200, 50}, "New Game", false};
    MenuButton continueBtn = {{(float)(windowWidth/2-100), (float)(windowHeight/2-20), 200, 50}, "Continue", false};
    MenuButton settingsBtn = {{(float)(windowWidth/2-100), (float)(windowHeight/2+40), 200, 50}, "Settings", false};
    MenuButton exitBtn = {{(float)(windowWidth/2-100), (float)(windowHeight/2+100), 200, 50}, "Exit", false};

    bool menuRunning = true;
    int selected = -1;
    SDL_Event event;

    while (menuRunning) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) { selected = 1; menuRunning = false; }
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                SDL_Point point = {static_cast<int>(event.motion.x), static_cast<int>(event.motion.y)};
                SDL_Rect rect = {(int)newGameBtn.rect.x, (int)newGameBtn.rect.y, (int)newGameBtn.rect.w, (int)newGameBtn.rect.h};
                newGameBtn.hover = SDL_PointInRect(&point, &rect);
                rect = {(int)continueBtn.rect.x, (int)continueBtn.rect.y, (int)continueBtn.rect.w, (int)continueBtn.rect.h};
                continueBtn.hover = SDL_PointInRect(&point, &rect);
                rect = {(int)settingsBtn.rect.x, (int)settingsBtn.rect.y, (int)settingsBtn.rect.w, (int)settingsBtn.rect.h};
                settingsBtn.hover = SDL_PointInRect(&point, &rect);
                rect = {(int)exitBtn.rect.x, (int)exitBtn.rect.y, (int)exitBtn.rect.w, (int)exitBtn.rect.h};
                exitBtn.hover = SDL_PointInRect(&point, &rect);
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                float mx = event.button.x, my = event.button.y;
                if (mx >= newGameBtn.rect.x && mx <= newGameBtn.rect.x+newGameBtn.rect.w && my >= newGameBtn.rect.y && my <= newGameBtn.rect.y+newGameBtn.rect.h) { selected = 0; menuRunning = false; }
                if (mx >= continueBtn.rect.x && mx <= continueBtn.rect.x+continueBtn.rect.w && my >= continueBtn.rect.y && my <= continueBtn.rect.y+continueBtn.rect.h) { selected = 2; menuRunning = false; }
                if (mx >= settingsBtn.rect.x && mx <= settingsBtn.rect.x+settingsBtn.rect.w && my >= settingsBtn.rect.y && my <= settingsBtn.rect.y+settingsBtn.rect.h) { selected = 3; menuRunning = false; }
                if (mx >= exitBtn.rect.x && mx <= exitBtn.rect.x+exitBtn.rect.w && my >= exitBtn.rect.y && my <= exitBtn.rect.y+exitBtn.rect.h) { selected = 1; menuRunning = false; }
            }
        }
        SDL_SetRenderDrawColor(renderer,0,0,0,255);
        SDL_RenderClear(renderer);
        SDL_Surface* titleSurf = TTF_RenderText_Solid(font, "UNDOOM", 0, SDL_Color{255,0,0,255});
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
            SDL_FRect titleRect = {(float)(windowWidth/2 - titleSurf->w/2), (float)(windowHeight/2 - 180), (float)titleSurf->w, (float)titleSurf->h};
            SDL_RenderTexture(renderer, titleTex, NULL, &titleRect);
            SDL_DestroyTexture(titleTex);
            SDL_DestroySurface(titleSurf);
        }
        SDL_Color btnColor = {100,100,100,255}, btnHover = {150,150,150,255}, textColor = {255,255,255,255};
        for (auto* btn : {&newGameBtn, &continueBtn, &settingsBtn, &exitBtn}) {
            SDL_SetRenderDrawColor(renderer, btn->hover?btnHover.r:btnColor.r, btn->hover?btnHover.g:btnColor.g, btn->hover?btnHover.b:btnColor.b,255);
            SDL_RenderFillRect(renderer, &btn->rect);
            SDL_SetRenderDrawColor(renderer,255,255,255,255);
            SDL_RenderRect(renderer, &btn->rect);
            SDL_Surface* btnSurf = TTF_RenderText_Solid(font, btn->label.c_str(), 0, textColor);
            if (btnSurf) {
                SDL_Texture* btnTex = SDL_CreateTextureFromSurface(renderer, btnSurf);
                float textX = btn->rect.x + (btn->rect.w - btnSurf->w)/2;
                float textY = btn->rect.y + (btn->rect.h - btnSurf->h)/2;
                SDL_FRect textRect = {textX, textY, (float)btnSurf->w, (float)btnSurf->h};
                SDL_RenderTexture(renderer, btnTex, NULL, &textRect);
                SDL_DestroyTexture(btnTex);
                SDL_DestroySurface(btnSurf);
            }
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    if (selected == 3) {
        showSettingsMenu(renderer, font, windowWidth, windowHeight, window);
        return showMainMenu(renderer, font, windowWidth, windowHeight, window);
    }
    return selected;
}

void showPauseMenu(SDL_Renderer* renderer, TTF_Font* font, int windowWidth, int windowHeight,
                   bool& gameRunning, SaveData& currentSave, bool& saveRequested, bool& loadRequested, bool& quitToMenu) {
    bool inPause = true;
    SDL_Event event;
    while (inPause) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) { gameRunning = false; inPause = false; }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) { inPause = false; }
                if (event.key.scancode == SDL_SCANCODE_S) {
                    saveRequested = true;
                    inPause = false;
                }
                if (event.key.scancode == SDL_SCANCODE_L) {
                    loadRequested = true;
                    inPause = false;
                }
                if (event.key.scancode == SDL_SCANCODE_Q) {
                    quitToMenu = true;
                    gameRunning = false;
                    inPause = false;
                }
            }
        }
        SDL_SetRenderDrawColor(renderer, 0,0,0,200);
        SDL_RenderClear(renderer);

        SDL_Surface* pauseSurf = TTF_RenderText_Solid(font, "PAUSED", 0, SDL_Color{255,255,255,255});
        if (pauseSurf) {
            SDL_Texture* pauseTex = SDL_CreateTextureFromSurface(renderer, pauseSurf);
            SDL_FRect pauseRect = {(float)(windowWidth/2 - pauseSurf->w/2), 100, (float)pauseSurf->w, (float)pauseSurf->h};
            SDL_RenderTexture(renderer, pauseTex, NULL, &pauseRect);
            SDL_DestroyTexture(pauseTex);
            SDL_DestroySurface(pauseSurf);
        }

        SDL_Surface* resumeSurf = TTF_RenderText_Solid(font, "Press ESC to Resume", 0, SDL_Color{200,200,200,255});
        if (resumeSurf) {
            SDL_Texture* resumeTex = SDL_CreateTextureFromSurface(renderer, resumeSurf);
            SDL_FRect resumeRect = {(float)(windowWidth/2 - resumeSurf->w/2), 200, (float)resumeSurf->w, (float)resumeSurf->h};
            SDL_RenderTexture(renderer, resumeTex, NULL, &resumeRect);
            SDL_DestroyTexture(resumeTex);
            SDL_DestroySurface(resumeSurf);
        }

        SDL_Surface* saveSurf = TTF_RenderText_Solid(font, "Press S to Save Game", 0, SDL_Color{200,200,200,255});
        if (saveSurf) {
            SDL_Texture* saveTex = SDL_CreateTextureFromSurface(renderer, saveSurf);
            SDL_FRect saveRect = {(float)(windowWidth/2 - saveSurf->w/2), 260, (float)saveSurf->w, (float)saveSurf->h};
            SDL_RenderTexture(renderer, saveTex, NULL, &saveRect);
            SDL_DestroyTexture(saveTex);
            SDL_DestroySurface(saveSurf);
        }

        SDL_Surface* loadSurf = TTF_RenderText_Solid(font, "Press L to Load Game", 0, SDL_Color{200,200,200,255});
        if (loadSurf) {
            SDL_Texture* loadTex = SDL_CreateTextureFromSurface(renderer, loadSurf);
            SDL_FRect loadRect = {(float)(windowWidth/2 - loadSurf->w/2), 320, (float)loadSurf->w, (float)loadSurf->h};
            SDL_RenderTexture(renderer, loadTex, NULL, &loadRect);
            SDL_DestroyTexture(loadTex);
            SDL_DestroySurface(loadSurf);
        }

        SDL_Surface* quitSurf = TTF_RenderText_Solid(font, "Press Q to Quit to Menu", 0, SDL_Color{200,200,200,255});
        if (quitSurf) {
            SDL_Texture* quitTex = SDL_CreateTextureFromSurface(renderer, quitSurf);
            SDL_FRect quitRect = {(float)(windowWidth/2 - quitSurf->w/2), 380, (float)quitSurf->w, (float)quitSurf->h};
            SDL_RenderTexture(renderer, quitTex, NULL, &quitRect);
            SDL_DestroyTexture(quitTex);
            SDL_DestroySurface(quitSurf);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
}

bool mobCollidesWithWall(float cx, float cy, float r,
                         const std::vector<Linedef>& lines,
                         const std::vector<Vertex>& vertices) {
    for (const auto& line : lines) {
        if (line.startVertex >= vertices.size() || line.endVertex >= vertices.size()) continue;
        const Vertex& v1 = vertices[line.startVertex];
        const Vertex& v2 = vertices[line.endVertex];
        float lineDx = v2.x - v1.x;
        float lineDy = v2.y - v1.y;
        float len2 = lineDx*lineDx + lineDy*lineDy;
        if (len2 == 0.0f) continue;
        float t = ((cx - v1.x) * lineDx + (cy - v1.y) * lineDy) / len2;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float projX = v1.x + t * lineDx;
        float projY = v1.y + t * lineDy;
        float dist2 = (cx - projX)*(cx - projX) + (cy - projY)*(cy - projY);
        if (dist2 < r*r) return true;
    }
    return false;
}

bool runGame(SDL_Renderer* renderer, SDL_Window* window, TTF_Font* font, TTF_Font* smallFont,
             const Texture& wallTex,
             const Texture& mobWalkingTex, const Texture& mobAttackTex, const Texture& mobDeathTex,
             const Texture& bossWalkingTex, const Texture& bossAttackTex, const Texture& bossDeathTex,
             const Texture& gunTex, const Texture& shootingGunTex,
             const Texture& shotgunTex, const Texture& shotgunShotTex,
             const Texture& firstAidKitTex,
             const Texture& pistolCartridgesTex, const Texture& shotgunCartridgesTex,
             int startLevel, bool isContinue) {

    const int MAX_PISTOL_AMMO = 60;
    const int MAX_SHOTGUN_AMMO = 20;
    const int PISTOL_MAG_SIZE = 7;
    const int SHOTGUN_MAG_SIZE = 2;
    int currentLevel = startLevel;
    SaveData saveData;

    int totalMonstersKilled = 0;
    int totalAmmoSpent = 0;
    int totalDamageTaken = 0;

    if (isContinue && loadGame(saveData)) {
        currentLevel = saveData.level;
    }

    while (currentLevel <= 3) {
        std::string levelFile = "level" + std::to_string(currentLevel) + ".txt";
        DoomMap gameMap;
        if (!gameMap.loadFromTextFile(levelFile)) {
            std::cerr << "Failed to load " << levelFile << std::endl;
            return false;
        }

        auto vertices = gameMap.getVertices();
        auto lines = gameMap.getLinedefs();
        auto sidedefs = gameMap.getSidedefs();
        auto sectors = gameMap.getSectors();

        if (vertices.empty() || lines.empty()) return false;

        Player player;
        player.speed = 280.0f;

        std::list<Mob> mobs;
        std::vector<Pickup> pickups;

        const auto& things = gameMap.getThings();

        if (isContinue && currentLevel == saveData.level && saveData.mobs.size() > 0) {
            player.x = saveData.playerX;
            player.y = saveData.playerY;
            player.angle = saveData.playerAngle;
            for (const auto& m : saveData.mobs) {
                float x, y; int type, hp;
                std::tie(x, y, type, hp) = m;
                Mob mob(x, y, (type == 4) ? BOSS : ZOMBIE);
                mob.hp = hp;
                mobs.push_back(mob);
            }
            for (const auto& p : saveData.pickups) {
                float x, y; int type; bool active;
                std::tie(x, y, type, active) = p;
                pickups.push_back({x, y, type, active});
            }
        } else {
            for (const auto& t : things) {
                if (t.type == 1) {
                    player.x = (float)t.x;
                    player.y = (float)t.y;
                } else if (t.type == 2 || t.type == 3) {
                    mobs.emplace_back((float)t.x, (float)t.y, ZOMBIE);
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
        }

        int initialMobCount = mobs.size();

        int playerHealth = 100;
        int pistolAmmo = 30;
        int shotgunAmmo = 8;
        int pistolMag = PISTOL_MAG_SIZE;
        int shotgunMag = SHOTGUN_MAG_SIZE;
        int currentWeapon = 0;

        if (isContinue && currentLevel == saveData.level) {
            playerHealth = saveData.playerHealth;
            pistolAmmo = saveData.pistolAmmo;
            shotgunAmmo = saveData.shotgunAmmo;
            pistolMag = saveData.pistolMag;
            shotgunMag = saveData.shotgunMag;
            currentWeapon = saveData.currentWeapon;
        }

        const int playerMaxHealth = 100;

        std::cout << "Level " << currentLevel << " has " << mobs.size() << " enemies" << std::endl;

        struct MobRenderState {
            bool isDying = false;
            float deathTimer = 0.0f;
            float attackAnimTime = 0.0f;
            float rangedCooldown = 0.0f;
            float textureCycleTimer = 0.0f;
            bool useAttackTexture = false;
            float wanderTimer = 0.0f;
            float wanderAngle = 0.0f;
            Mob* bossPtr = nullptr;
        };
        std::map<Mob*, MobRenderState> mobRenderStates;

        Mob* bossPtr = nullptr;
        for (auto& mob : mobs) {
            if (mob.type == BOSS) {
                bossPtr = &mob;
                break;
            }
        }

        const float FOV = 60.0f * M_PI / 180.0f;
        Uint64 lastTime = SDL_GetTicks();
        Uint64 lastShootTime = 0;
        const Uint64 SHOOT_DELAY_MS = 133;
        std::vector<BulletHole> bulletHoles;
        std::vector<HitFlash> hitFlashes;
        std::vector<AchievementNotification> achievements;

        float playerDamageFlash = 0.0f;

        int frameCount = 0;
        Uint64 lastFpsTime = SDL_GetTicks();
        int currentFPS = 0;

        unsigned int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;
        ThreadPool threadPool(numThreads);

        int RENDER_WIDTH = g_settings.windowWidth / g_settings.renderScale;

        bool levelComplete = false;
        int shootFlashFrames = 0;
        SDL_Event event;

        Uint64 lastSaveTime = SDL_GetTicks();
        const Uint64 SAVE_INTERVAL_MS = 30000;

        const int TARGET_FPS = 15;
        const Uint64 TARGET_FRAME_DURATION = 1000 / TARGET_FPS;

        bool bossDying = false;
        float bossDeathTimer = 0.0f;

        bool gameRunning = true;
        bool saveRequested = false;
        bool loadRequested = false;
        bool quitToMenu = false;

        while (!levelComplete && gameRunning) {
            Uint64 frameStart = SDL_GetTicks();
            Uint64 now = frameStart;
            float dt = (now - lastTime)/1000.0f;
            lastTime = now;
            if (dt > 0.033f) dt = 0.033f;

            if (playerDamageFlash > 0) playerDamageFlash -= dt;

            if (now - lastSaveTime >= SAVE_INTERVAL_MS) {
                lastSaveTime = now;
                SaveData save;
                save.level = currentLevel;
                save.playerX = player.x;
                save.playerY = player.y;
                save.playerAngle = player.angle;
                save.playerHealth = playerHealth;
                save.pistolAmmo = pistolAmmo;
                save.shotgunAmmo = shotgunAmmo;
                save.pistolMag = pistolMag;
                save.shotgunMag = shotgunMag;
                save.currentWeapon = currentWeapon;
                for (const auto& mob : mobs) {
                    save.mobs.emplace_back(mob.x, mob.y, (mob.type == BOSS) ? 4 : 2, mob.hp);
                }
                for (const auto& p : pickups) {
                    if (p.active) {
                        save.pickups.emplace_back(p.x, p.y, p.type, p.active);
                    }
                }
                saveGame(save);
            }

            frameCount++;
            if (now - lastFpsTime >= 1000) {
                currentFPS = frameCount;
                frameCount = 0;
                lastFpsTime = now;
            }

            const bool* keys = SDL_GetKeyboardState(nullptr);

            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) { gameRunning = false; levelComplete = true; }
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    showPauseMenu(renderer, font, g_settings.windowWidth, g_settings.windowHeight,
                                  gameRunning, saveData, saveRequested, loadRequested, quitToMenu);
                    if (quitToMenu) { levelComplete = true; gameRunning = false; break; }
                    if (saveRequested) {
                        SaveData save;
                        save.level = currentLevel;
                        save.playerX = player.x;
                        save.playerY = player.y;
                        save.playerAngle = player.angle;
                        save.playerHealth = playerHealth;
                        save.pistolAmmo = pistolAmmo;
                        save.shotgunAmmo = shotgunAmmo;
                        save.pistolMag = pistolMag;
                        save.shotgunMag = shotgunMag;
                        save.currentWeapon = currentWeapon;
                        for (const auto& mob : mobs) {
                            save.mobs.emplace_back(mob.x, mob.y, (mob.type == BOSS) ? 4 : 2, mob.hp);
                        }
                        for (const auto& p : pickups) {
                            if (p.active) {
                                save.pickups.emplace_back(p.x, p.y, p.type, p.active);
                            }
                        }
                        saveGame(save);
                        saveRequested = false;
                    }
                    if (loadRequested) {
                        SaveData load;
                        if (loadGame(load)) {
                            currentLevel = load.level;
                            player.x = load.playerX;
                            player.y = load.playerY;
                            player.angle = load.playerAngle;
                            playerHealth = load.playerHealth;
                            pistolAmmo = load.pistolAmmo;
                            shotgunAmmo = load.shotgunAmmo;
                            pistolMag = load.pistolMag;
                            shotgunMag = load.shotgunMag;
                            currentWeapon = load.currentWeapon;
                            mobs.clear();
                            for (const auto& m : load.mobs) {
                                float x,y; int type,hp;
                                std::tie(x,y,type,hp) = m;
                                Mob mob(x,y,(type==4)?BOSS:ZOMBIE);
                                mob.hp = hp;
                                mobs.push_back(mob);
                            }
                            pickups.clear();
                            for (const auto& p : load.pickups) {
                                float x,y; int type; bool active;
                                std::tie(x,y,type,active) = p;
                                pickups.push_back({x,y,type,active});
                            }
                            levelComplete = true;
                        }
                        loadRequested = false;
                    }
                }
                if (event.type == SDL_EVENT_KEY_DOWN) {
                    if (event.key.scancode == SDL_SCANCODE_1) currentWeapon = 0;
                    if (event.key.scancode == SDL_SCANCODE_2) currentWeapon = 1;
                    if (event.key.scancode == SDL_SCANCODE_R) {
                        reloadWeapon(currentWeapon, pistolAmmo, shotgunAmmo, pistolMag, shotgunMag);
                    }
                }
                if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_SPACE) {
                    if (now - lastShootTime >= SHOOT_DELAY_MS) {
                        lastShootTime = now;
                        shootFlashFrames = 2;

                        int ammoPrev = (currentWeapon == 0) ? pistolAmmo + pistolMag : shotgunAmmo + shotgunMag;

                        performShot(player.x, player.y, player.angle,
                                    lines, vertices, mobs, bulletHoles, hitFlashes,
                                    pistolAmmo, shotgunAmmo, pistolMag, shotgunMag,
                                    currentWeapon, playerHealth, playerMaxHealth);

                        int ammoNow = (currentWeapon == 0) ? pistolAmmo + pistolMag : shotgunAmmo + shotgunMag;
                        totalAmmoSpent += (ammoPrev - ammoNow);
                    }
                }
            }

            float move = player.speed * dt;
            float turn = 4.0f * dt;
            float dx = 0, dy = 0;
            if (keys[SDL_SCANCODE_W]) { dx += cos(player.angle)*move; dy += sin(player.angle)*move; }
            if (keys[SDL_SCANCODE_S]) { dx -= cos(player.angle)*move; dy -= sin(player.angle)*move; }
            if (keys[SDL_SCANCODE_A]) { dx += sin(player.angle)*move; dy -= cos(player.angle)*move; }
            if (keys[SDL_SCANCODE_D]) { dx -= sin(player.angle)*move; dy += cos(player.angle)*move; }

            if (dx != 0 || dy != 0) player.moveWithSliding(dx, dy, lines, vertices);

            const float PLAYER_SIZE = 32.0f;
            for (auto& mob : mobs) {
                if (!mob.isAlive()) continue;
                float dxMob = player.x - mob.x;
                float dyMob = player.y - mob.y;
                float dist = sqrtf(dxMob*dxMob + dyMob*dyMob);
                float minDist = PLAYER_SIZE + mob.size / 2.0f;
                if (dist < minDist) {
                    float angle = atan2(dyMob, dxMob);
                    float overlap = minDist - dist;
                    player.x += cos(angle) * overlap;
                    player.y += sin(angle) * overlap;
                    player.moveWithSliding(0, 0, lines, vertices);
                    dxMob = player.x - mob.x;
                    dyMob = player.y - mob.y;
                    dist = sqrtf(dxMob*dxMob + dyMob*dyMob);
                    if (dist < minDist) {
                        overlap = minDist - dist;
                        mob.x -= cos(angle) * overlap;
                        mob.y -= sin(angle) * overlap;
                    }
                }
            }

            if (keys[SDL_SCANCODE_LEFT]) player.angle -= turn;
            if (keys[SDL_SCANCODE_RIGHT]) player.angle += turn;
            while (player.angle < 0) player.angle += 2*M_PI;
            while (player.angle >= 2*M_PI) player.angle -= 2*M_PI;

            if (shootFlashFrames > 0) shootFlashFrames--;

            if (playerHealth <= 0) {
                levelComplete = true;
                gameRunning = false;
            }

            const float PICKUP_RADIUS = 40.0f;
            for (auto& pickup : pickups) {
                if (!pickup.active) continue;
                float dxPickup = pickup.x - player.x;
                float dyPickup = pickup.y - player.y;
                float dist = sqrtf(dxPickup*dxPickup + dyPickup*dyPickup);
                if (dist < PICKUP_RADIUS) {
                    pickup.active = false;
                    if (pickup.type == 2011) {
                        playerHealth = std::min(playerMaxHealth, playerHealth + 60);
                        std::cout << "Picked up medkit! HP: " << playerHealth << std::endl;
                    } else if (pickup.type == 2007) {
                        pistolAmmo = std::min(MAX_PISTOL_AMMO, pistolAmmo + 30);
                        std::cout << "Picked up pistol ammo! Total: " << pistolAmmo << std::endl;
                    } else if (pickup.type == 2008) {
                        shotgunAmmo = std::min(MAX_SHOTGUN_AMMO, shotgunAmmo + 12);
                        std::cout << "Picked up shotgun ammo! Total: " << shotgunAmmo << std::endl;
                    }
                }
            }

            int hpBeforeDamage = playerHealth;

            const Uint32 MOB_UPDATE_INTERVAL_MS = 100;
            for (auto& mob : mobs) {
                auto& state = mobRenderStates[&mob];
                if (mob.isAlive() && state.isDying) {
                    state.isDying = false;
                }
                if (state.attackAnimTime > 0) state.attackAnimTime -= dt;
                if (state.rangedCooldown > 0) state.rangedCooldown -= dt;
                state.textureCycleTimer -= dt;
                if (state.textureCycleTimer <= 0.0f) {
                    state.textureCycleTimer = 0.15f;
                    state.useAttackTexture = !state.useAttackTexture;
                }
                if (!mob.isAlive()) {
                    if (!state.isDying) {
                        state.isDying = true;
                        state.deathTimer = 0.3f;
                        if (mob.type == BOSS) {
                            state.deathTimer = 1.0f;
                            bossDying = true;
                            bossDeathTimer = 1.0f;
                            achievements.push_back({"KILLED BOSS", 3.0f, 3.0f});
                        }
                    }
                    continue;
                }
                if (now - mob.lastUpdateTime >= MOB_UPDATE_INTERVAL_MS) {
                    mob.lastUpdateTime = now;
                    // Призыв миньонов (босс)
                    if (mob.type == BOSS && bossPtr != nullptr) {
                        if (mob.summonCooldown > 0.0f) {
                            mob.summonCooldown -= dt;
                        }
                        if (mob.summonCooldown <= 0.0f && bossPtr->summonCount < 6) {
                            int toSpawn = std::min(2, 6 - bossPtr->summonCount);
                            if (toSpawn > 0) {
                                mobRenderStates[bossPtr].attackAnimTime = 0.5f;
                                std::vector<Mob> summoned = mob.summonMobsAround(toSpawn);
                                for (Mob& newMob : summoned) {
                                    mobs.push_back(newMob);
                                    MobRenderState newState;
                                    newState.bossPtr = bossPtr;
                                    mobRenderStates[&mobs.back()] = newState;
                                    bossPtr->addSummon(1);
                                }
                                mob.summonCooldown = 3.0f;
                                std::cout << "Boss summoned " << toSpawn << " minions! Total alive: "
                                          << bossPtr->summonCount << std::endl;
                            }
                        }
                    }
                    float dxToPlayer = player.x - mob.x;
                    float dyToPlayer = player.y - mob.y;
                    float distToPlayer = sqrtf(dxToPlayer*dxToPlayer + dyToPlayer*dyToPlayer);
                    bool canSeePlayer = isPointVisible(mob.x, mob.y, player.x, player.y, lines, vertices);
                    float mobSpeed = (mob.type == BOSS) ? 280.0f : 320.0f;
                    float stopDist = (mob.type == BOSS) ? 300.0f : 250.0f;
                    if (canSeePlayer) {
                        if (distToPlayer > stopDist + 15.0f) {
                            float nx = dxToPlayer / distToPlayer;
                            float ny = dyToPlayer / distToPlayer;
                            float step = mobSpeed * dt;
                            float newX = mob.x + nx * step;
                            float newY = mob.y + ny * step;
                            if (!mobCollidesWithWall(newX, mob.y, mob.size/2, lines, vertices))
                                mob.x = newX;
                            if (!mobCollidesWithWall(mob.x, newY, mob.size/2, lines, vertices))
                                mob.y = newY;
                        }
                        else if (distToPlayer < stopDist - 15.0f) {
                            float nx = -dxToPlayer / distToPlayer;
                            float ny = -dyToPlayer / distToPlayer;
                            float step = mobSpeed * dt;
                            float newX = mob.x + nx * step;
                            float newY = mob.y + ny * step;
                            if (!mobCollidesWithWall(newX, mob.y, mob.size/2, lines, vertices))
                                mob.x = newX;
                            if (!mobCollidesWithWall(mob.x, newY, mob.size/2, lines, vertices))
                                mob.y = newY;
                        }
                        float maxRange = (mob.type == BOSS) ? 900.0f : 700.0f;
                        float minRange = 100.0f;
                        if (distToPlayer <= maxRange && distToPlayer >= minRange && state.rangedCooldown <= 0.0f) {
                            int dmg = (mob.type == BOSS) ? 15 : 10;
                            playerHealth -= dmg;
                            playerDamageFlash = 0.1f;
                            state.rangedCooldown = (mob.type == BOSS) ? 4.0f : 3.0f;
                            state.attackAnimTime = 0.15f;
                        }
                    } else {
                        float wanderSpeed = (mob.type == BOSS) ? 140.0f : 180.0f;
                        state.wanderTimer -= dt;
                        if (state.wanderTimer <= 0.0f) {
                            state.wanderTimer = 1.0f + (rand() % 100) / 50.0f;
                            state.wanderAngle = (rand() % 360) * M_PI / 180.0f;
                        }
                        float stepX = cos(state.wanderAngle) * wanderSpeed * dt;
                        float stepY = sin(state.wanderAngle) * wanderSpeed * dt;
                        float newX = mob.x + stepX;
                        float newY = mob.y + stepY;
                        if (!mobCollidesWithWall(newX, mob.y, mob.size/2, lines, vertices))
                            mob.x = newX;
                        if (!mobCollidesWithWall(mob.x, newY, mob.size/2, lines, vertices))
                            mob.y = newY;
                    }
                }
                mob.tryAttack(player, dt, playerHealth);
                if (playerHealth <= 0) return false;
            }

            int hpLostThisFrame = hpBeforeDamage - playerHealth;
            if (hpLostThisFrame > 0) totalDamageTaken += hpLostThisFrame;

            for (auto it = mobs.begin(); it != mobs.end(); ) {
                Mob* mobPtr = &(*it);
                auto stateIt = mobRenderStates.find(mobPtr);
                if (stateIt != mobRenderStates.end() && !it->isAlive() && stateIt->second.isDying) {
                    stateIt->second.deathTimer -= dt;
                    if (stateIt->second.deathTimer <= 0.0f) {
                        totalMonstersKilled++;
                        if (it->type == ZOMBIE && stateIt->second.bossPtr != nullptr) {
                            stateIt->second.bossPtr->removeSummon();
                        }
                        if (it->type == BOSS) {
                            for(auto& pair : mobRenderStates) pair.second.bossPtr = nullptr;
                        }
                        mobRenderStates.erase(stateIt);
                        it = mobs.erase(it);
                    } else {
                        ++it;
                    }
                } else {
                    ++it;
                }
            }

            bool readyToExit = false;
            if (mobs.empty() && !bossDying) {
                if (initialMobCount > 0) {
                    readyToExit = true;
                    achievements.push_back({"LEVEL " + std::to_string(currentLevel) + " COMPLETED", 3.0f, 3.0f});
                } else {
                    if (keys[SDL_SCANCODE_RETURN]) {
                        readyToExit = true;
                        achievements.push_back({"LEVEL " + std::to_string(currentLevel) + " COMPLETED", 3.0f, 3.0f});
                    }
                }
            }
            if (bossDying && bossDeathTimer > 0) {
                bossDeathTimer -= dt;
                if (bossDeathTimer <= 0.0f) {
                    showVictoryScreen(renderer, font, smallFont, g_settings.windowWidth, g_settings.windowHeight, totalMonstersKilled, totalAmmoSpent, totalDamageTaken);
                    return true;
                }
            }
            if (readyToExit) {
                if (currentLevel < 3) {
                    bool next = showLevelCompleteScreen(renderer, font, g_settings.windowWidth, g_settings.windowHeight, currentLevel);
                    if (!next) return true;
                } else {
                    showVictoryScreen(renderer, font, smallFont, g_settings.windowWidth, g_settings.windowHeight, totalMonstersKilled, totalAmmoSpent, totalDamageTaken);
                    return true;
                }
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
            for (auto it = achievements.begin(); it != achievements.end(); ) {
                it->lifetime -= dt;
                if (it->lifetime <= 0) it = achievements.erase(it);
                else ++it;
            }

            SDL_SetRenderDrawColor(renderer, 0,0,0,255);
            SDL_RenderClear(renderer);
            SDL_SetRenderDrawColor(renderer, 26, 20, 16, 255);
            SDL_FRect floorRect = {0, (float)g_settings.windowHeight/2, (float)g_settings.windowWidth, (float)g_settings.windowHeight/2};
            SDL_RenderFillRect(renderer, &floorRect);
            SDL_SetRenderDrawColor(renderer, 43, 43, 43, 255);
            SDL_FRect ceilingRect = {0, 0, (float)g_settings.windowWidth, (float)g_settings.windowHeight/2};
            SDL_RenderFillRect(renderer, &ceilingRect);

            std::vector<ColumnResult> allColumns(RENDER_WIDTH);
            int chunk = RENDER_WIDTH / numThreads;
            std::vector<std::future<void>> futures;
            for (unsigned int i = 0; i < numThreads; ++i) {
                int startX = i * chunk;
                int endX = (i == numThreads - 1) ? RENDER_WIDTH : (i + 1) * chunk;
                futures.push_back(std::async(std::launch::async,
                    [startX, endX, &allColumns, &player, &gameMap, RENDER_WIDTH]() {
                            renderColumnsRangeDirect(startX, endX, allColumns, player, gameMap, RENDER_WIDTH, g_settings.windowHeight);
                }));
            }
            for (auto& fut : futures) fut.get();

            for (const auto& col : allColumns) {
                if (col.wallTop >= col.wallBottom) continue;
                int texX = (int)(col.hitU * wallTex.width) % wallTex.width;
                if (texX < 0) texX += wallTex.width;
                int screenX = col.x * g_settings.renderScale;
                drawWallColumn(renderer, screenX, col.wallTop, col.wallBottom, texX, wallTex, g_settings.renderScale);
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
                float screenX = (angleDiff / (FOV/2)) * (g_settings.windowWidth/2) + g_settings.windowWidth/2;
                if (screenX < 0 || screenX >= g_settings.windowWidth) continue;
                float spriteHeight = 72.0f * 200.0f / distToPlayer;
                if (spriteHeight < 27) spriteHeight = 27;
                float spriteWidth = spriteHeight;
                float screenY = g_settings.windowHeight/2 + (56.0f / distToPlayer) * (g_settings.windowHeight / 1.5f) * 0.85f - spriteHeight;
                const Texture* pickupTex = nullptr;
                if (pickup.type == 2011) pickupTex = &firstAidKitTex;
                else if (pickup.type == 2007) pickupTex = &pistolCartridgesTex;
                else if (pickup.type == 2008) pickupTex = &shotgunCartridgesTex;
                if (pickupTex && pickupTex->texture) {
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
            const float PROJ_SCALE = (float)g_settings.windowHeight / 1.5f;

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
                float screenX = (angleDiff / (FOV/2)) * (g_settings.windowWidth/2) + g_settings.windowWidth/2;
                if (screenX < 0 || screenX >= g_settings.windowWidth) continue;

                float screenHeight = (MOB_HEIGHT_WORLD / dist) * PROJ_SCALE;
                if (screenHeight < 8) screenHeight = 8;
                if (screenHeight > g_settings.windowHeight) screenHeight = g_settings.windowHeight;

                float floorY = g_settings.windowHeight/2 + (VIEW_HEIGHT) / dist * PROJ_SCALE;
                float topY = floorY - screenHeight + (screenHeight * 0.25f);
                float bottomY = floorY + (screenHeight * 0.25f);

                if (topY < 0) {
                    bottomY -= topY;
                    topY = 0;
                }
                if (bottomY > g_settings.windowHeight) bottomY = g_settings.windowHeight;
                if (topY >= bottomY) continue;

                float screenWidthMob = screenHeight * 0.7f;

                auto stateIt = mobRenderStates.find(const_cast<Mob*>(&mob));
                bool isDying = (stateIt != mobRenderStates.end() && stateIt->second.isDying);
                bool isAttackingNow = (stateIt != mobRenderStates.end() && (stateIt->second.attackAnimTime > 0 || mob.attackCooldown > 0));

                const Texture* mobTex = nullptr;
                if (mob.type == BOSS) {
                    if (isDying && bossDeathTex.texture) mobTex = &bossDeathTex;
                    else if (isAttackingNow && bossAttackTex.texture) mobTex = &bossAttackTex;
                    else if (bossWalkingTex.texture) mobTex = &bossWalkingTex;
                    else mobTex = &bossWalkingTex;
                } else {
                    if (isDying && mobDeathTex.texture) mobTex = &mobDeathTex;
                    else if (isAttackingNow && mobAttackTex.texture) mobTex = &mobAttackTex;
                    else if (mobWalkingTex.texture) mobTex = &mobWalkingTex;
                    else mobTex = &mobWalkingTex;
                }

                if (mobTex && mobTex->texture) {
                    SDL_FRect srcRect = {0, 0, (float)mobTex->width, (float)mobTex->height};
                    SDL_FRect dstRect = {screenX - screenWidthMob/2, topY, screenWidthMob, screenHeight};
                    SDL_RenderTexture(renderer, mobTex->texture, &srcRect, &dstRect);
                }
            }

            for (const auto& mob : mobs) {
                if (mob.type == BOSS && mob.isAlive()) {
                    float healthPercent = (float)mob.hp / mob.maxHp;
                    int barWidth = 400;
                    int barX = (g_settings.windowWidth - barWidth) / 2;
                    int barY = 30;
                    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 200);
                    SDL_FRect bg = {(float)barX, (float)barY, (float)barWidth, 16};
                    SDL_RenderFillRect(renderer, &bg);
                    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
                    SDL_FRect hp = {(float)barX, (float)barY, (float)(barWidth * healthPercent), 16};
                    SDL_RenderFillRect(renderer, &hp);
                    break;
                }
            }

            int weaponW = (currentWeapon == 0) ? 1000 : 800;
            int weaponH = (currentWeapon == 0) ? 600 : 480;
            int weaponX = (g_settings.windowWidth - weaponW) / 2;
            int weaponY = g_settings.windowHeight - weaponH - 20;
            const Texture* currentWeaponTex = nullptr;
            if (currentWeapon == 0)
                currentWeaponTex = (shootFlashFrames > 0) ? &shootingGunTex : &gunTex;
            else
                currentWeaponTex = (shootFlashFrames > 0) ? &shotgunShotTex : &shotgunTex;
            if (currentWeaponTex && currentWeaponTex->texture) {
                SDL_FRect srcRect = {0, 0, (float)currentWeaponTex->width, (float)currentWeaponTex->height};
                SDL_FRect dstRect = {(float)weaponX, (float)weaponY, (float)weaponW, (float)weaponH};
                SDL_RenderTexture(renderer, currentWeaponTex->texture, &srcRect, &dstRect);
            }

            // ========== БАРЫ ЗДОРОВЬЯ И ПАТРОНОВ ==========
            const int BAR_X = 30;
            const int BAR_Y = g_settings.windowHeight - 100;
            const int BAR_WIDTH = 250;
            const int BAR_HEIGHT = 25;
            const int BAR_SPACING = 10;

            float healthPercent = (float)playerHealth / playerMaxHealth;
            SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
            SDL_FRect healthBg = { (float)BAR_X, (float)BAR_Y, (float)BAR_WIDTH, (float)BAR_HEIGHT };
            SDL_RenderFillRect(renderer, &healthBg);
            SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255);
            SDL_FRect healthBar = { (float)BAR_X, (float)BAR_Y, (float)(BAR_WIDTH * healthPercent), (float)BAR_HEIGHT };
            SDL_RenderFillRect(renderer, &healthBar);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderRect(renderer, &healthBg);

            char healthText[32];
            snprintf(healthText, sizeof(healthText), "HEALTH: %d", playerHealth);
            SDL_Surface* healthSurf = TTF_RenderText_Solid(smallFont, healthText, 0, {255,255,255,255});
            if (healthSurf) {
                SDL_Texture* healthTex = SDL_CreateTextureFromSurface(renderer, healthSurf);
                SDL_FRect healthRect = { (float)(BAR_X + 5), (float)(BAR_Y + 3), (float)healthSurf->w, (float)healthSurf->h };
                SDL_RenderTexture(renderer, healthTex, NULL, &healthRect);
                SDL_DestroyTexture(healthTex);
                SDL_DestroySurface(healthSurf);
            }

            int currentAmmoMag = (currentWeapon == 0) ? pistolMag : shotgunMag;
            int currentAmmoReserve = (currentWeapon == 0) ? pistolAmmo : shotgunAmmo;
            float magPercent = (float)currentAmmoMag / ((currentWeapon == 0) ? PISTOL_MAG_SIZE : SHOTGUN_MAG_SIZE);
            SDL_FRect magBg = { (float)BAR_X, (float)(BAR_Y + BAR_HEIGHT + BAR_SPACING), (float)BAR_WIDTH, (float)BAR_HEIGHT };
            SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
            SDL_RenderFillRect(renderer, &magBg);
            SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255);
            SDL_FRect magBar = { (float)BAR_X, (float)(BAR_Y + BAR_HEIGHT + BAR_SPACING), (float)(BAR_WIDTH * magPercent), (float)BAR_HEIGHT };
            SDL_RenderFillRect(renderer, &magBar);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderRect(renderer, &magBg);

            char ammoText[64];
            snprintf(ammoText, sizeof(ammoText), "%s: %d/%d", (currentWeapon == 0) ? "PISTOL" : "SHOTGUN",
                     currentAmmoMag, (currentWeapon == 0) ? PISTOL_MAG_SIZE : SHOTGUN_MAG_SIZE);
            SDL_Surface* ammoSurf = TTF_RenderText_Solid(smallFont, ammoText, 0, {255,255,255,255});
            if (ammoSurf) {
                SDL_Texture* ammoTex = SDL_CreateTextureFromSurface(renderer, ammoSurf);
                SDL_FRect ammoRect = { (float)(BAR_X + 5), (float)(BAR_Y + BAR_HEIGHT + BAR_SPACING + 3), (float)ammoSurf->w, (float)ammoSurf->h };
                SDL_RenderTexture(renderer, ammoTex, NULL, &ammoRect);
                SDL_DestroyTexture(ammoTex);
                SDL_DestroySurface(ammoSurf);
            }

            char reserveText[64];
            snprintf(reserveText, sizeof(reserveText), "Reserve: %d", currentAmmoReserve);
            SDL_Surface* reserveSurf = TTF_RenderText_Solid(smallFont, reserveText, 0, {200,200,200,255});
            if (reserveSurf) {
                SDL_Texture* reserveTex = SDL_CreateTextureFromSurface(renderer, reserveSurf);
                SDL_FRect reserveRect = { (float)(BAR_X + 5), (float)(BAR_Y + BAR_HEIGHT + BAR_SPACING + 28), (float)reserveSurf->w, (float)reserveSurf->h };
                SDL_RenderTexture(renderer, reserveTex, NULL, &reserveRect);
                SDL_DestroyTexture(reserveTex);
                SDL_DestroySurface(reserveSurf);
            }

            int aliveMobs = 0;
            for (const auto& mob : mobs) {
                if (mob.isAlive()) aliveMobs++;
            }
            float rightCornerY = 20.0f;

            char monstersText[32];
            snprintf(monstersText, sizeof(monstersText), "MONSTERS: %d", aliveMobs);
            SDL_Surface* monstersSurf = TTF_RenderText_Solid(smallFont, monstersText, 0, {255, 255, 255, 255});
            if (monstersSurf) {
                SDL_Texture* monstersTex = SDL_CreateTextureFromSurface(renderer, monstersSurf);
                SDL_FRect monstersRect = { (float)(g_settings.windowWidth - monstersSurf->w - 20), rightCornerY, (float)monstersSurf->w, (float)monstersSurf->h };
                SDL_RenderTexture(renderer, monstersTex, NULL, &monstersRect);
                SDL_DestroyTexture(monstersTex);
                SDL_DestroySurface(monstersSurf);
            }

            char achText[64];
            if (currentLevel == 1) snprintf(achText, sizeof(achText), "LEVELS CLEARED: NONE");
            else if (currentLevel == 2) snprintf(achText, sizeof(achText), "LEVELS CLEARED: 1");
            else snprintf(achText, sizeof(achText), "LEVELS CLEARED: 1, 2");

            SDL_Surface* achSurf = TTF_RenderText_Solid(smallFont, achText, 0, {255, 215, 0, 255});
            if (achSurf) {
                SDL_Texture* achTex = SDL_CreateTextureFromSurface(renderer, achSurf);
                SDL_FRect achRect = { (float)(g_settings.windowWidth - achSurf->w - 20), rightCornerY + 25.0f, (float)achSurf->w, (float)achSurf->h };
                SDL_RenderTexture(renderer, achTex, NULL, &achRect);
                SDL_DestroyTexture(achTex);
                SDL_DestroySurface(achSurf);
            }

            float achY = 80.0f;
            for (const auto& ach : achievements) {
                float alpha = std::min(1.0f, ach.lifetime / ach.maxLifetime);
                SDL_Color color = {255, 215, 0, (Uint8)(255 * alpha)};
                SDL_Surface* achSurfPop = TTF_RenderText_Solid(smallFont, ach.text.c_str(), 0, color);
                if (achSurfPop) {
                    SDL_Texture* achTexPop = SDL_CreateTextureFromSurface(renderer, achSurfPop);
                    SDL_FRect achRect = { (float)(g_settings.windowWidth - achSurfPop->w - 20), achY, (float)achSurfPop->w, (float)achSurfPop->h };
                    SDL_RenderTexture(renderer, achTexPop, NULL, &achRect);
                    SDL_DestroyTexture(achTexPop);
                    SDL_DestroySurface(achSurfPop);
                    achY += achSurfPop->h + 5;
                }
            }

            if (initialMobCount == 0 && mobs.empty() && !bossDying) {
                std::string skipText = "MAP IS EMPTY! PRESS ENTER TO SKIP LEVEL.";
                SDL_Surface* skipSurf = TTF_RenderText_Solid(font, skipText.c_str(), 0, {255, 255, 0, 255});
                if (skipSurf) {
                    SDL_Texture* skipTex = SDL_CreateTextureFromSurface(renderer, skipSurf);
                    SDL_FRect skipRect = {(float)(g_settings.windowWidth/2 - skipSurf->w/2), 120.0f, (float)skipSurf->w, (float)skipSurf->h};
                    SDL_RenderTexture(renderer, skipTex, NULL, &skipRect);
                    SDL_DestroyTexture(skipTex);
                    SDL_DestroySurface(skipSurf);
                }
            }

            std::string hudText = "FPS: " + std::to_string(currentFPS);
            SDL_Surface* hudSurf = TTF_RenderText_Solid(smallFont, hudText.c_str(), 0, {255,255,255,255});
            if (hudSurf) {
                SDL_Texture* hudTex = SDL_CreateTextureFromSurface(renderer, hudSurf);
                SDL_FRect hudRect = {20, 20, (float)hudSurf->w, (float)hudSurf->h};
                SDL_RenderTexture(renderer, hudTex, NULL, &hudRect);
                SDL_DestroyTexture(hudTex);
                SDL_DestroySurface(hudSurf);
            }

            SDL_RenderPresent(renderer);
            Uint64 frameEnd = SDL_GetTicks();
            Uint64 elapsed = frameEnd - frameStart;
            if (elapsed < TARGET_FRAME_DURATION) {
                SDL_Delay(TARGET_FRAME_DURATION - elapsed);
            }
        }

        if (currentLevel < 3) {
            currentLevel++;
            isContinue = false;
        } else {
            break;
        }
    }

    return true;
}

extern void initTrigTables();

int main() {
    initTrigTables();
    loadSettings();

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Ошибка инициализации SDL: " << SDL_GetError() << std::endl;
        return 1;
    }
    if (TTF_Init() < 0) {
        std::cerr << "Ошибка инициализации SDL_ttf: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("UNDOOM", g_settings.windowWidth, g_settings.windowHeight,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "Ошибка создания окна: " << SDL_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "Ошибка создания рендерера: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderVSync(renderer, 0);

    TTF_Font* font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 28);
    if (!font) font = TTF_OpenFont("arial.ttf", 28);
    TTF_Font* smallFont = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 18);
    if (!smallFont) smallFont = font;

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

    bool running = true;
    while (running) {
        int choice = showMainMenu(renderer, font, g_settings.windowWidth, g_settings.windowHeight, window);
        if (choice == 0) {
            bool completed = runGame(renderer, window, font, smallFont,
                                     wallTex, mobWalkingTex, mobAttackTex, mobDeathTex,
                                     bossWalkingTex, bossAttackTex, bossDeathTex,
                                     gunTex, shootingGunTex, shotgunTex, shotgunShotTex,
                                     firstAidKitTex, pistolCartridgesTex, shotgunCartridgesTex,
                                     1, false);
            if (!completed) {
                bool restart = showGameOverScreen(renderer, font, g_settings.windowWidth, g_settings.windowHeight);
                if (!restart) running = false;
            }
        } else if (choice == 2) {
            bool completed = runGame(renderer, window, font, smallFont,
                                     wallTex, mobWalkingTex, mobAttackTex, mobDeathTex,
                                     bossWalkingTex, bossAttackTex, bossDeathTex,
                                     gunTex, shootingGunTex, shotgunTex, shotgunShotTex,
                                     firstAidKitTex, pistolCartridgesTex, shotgunCartridgesTex,
                                     1, true);
            if (!completed) {
                bool restart = showGameOverScreen(renderer, font, g_settings.windowWidth, g_settings.windowHeight);
                if (!restart) running = false;
            }
        } else if (choice == 1) {
            running = false;
        }
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

    if (font) TTF_CloseFont(font);
    if (smallFont && smallFont != font) TTF_CloseFont(smallFont);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}