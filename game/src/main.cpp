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

    for (int i = 0; i < 5; i++) {
        float spawnX = square.x1 + (float)(rand() % (int)(square.x2 - square.x1));
        float spawnY = square.y1 + (float)(rand() % (int)(square.y2 - square.y1));
        mobs.emplace_back(spawnX, spawnY, ZOMBIE);
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
                if (r > 245 && g > 245 && b > 245) tex.pixelData[i] = 0;
            }
            SDL_Surface* modifiedSurf = SDL_CreateSurface(tex.width, tex.height, SDL_PIXELFORMAT_RGBA32);
            memcpy(modifiedSurf->pixels, tex.pixelData.data(), tex.width * tex.height * 4);
            tex.texture = SDL_CreateTextureFromSurface(renderer, modifiedSurf);
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
        SDL_SetTextureScaleMode(tex.texture, SDL_SCALEMODE_NEAREST);
        tex.width = tex.height = 64;
        SDL_DestroySurface(dummy);
    }
    return tex;
}

void drawWallColumn(SDL_Renderer* renderer, int x, int top, int bottom, int texX, const Texture& tex, float distance) {
    if (top >= bottom) return;
    int height = bottom - top;
    const int STRIP_SIZE = 16;
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
            int damage = (currentWeapon == 0) ? 20 : 40;
            hitMob->takeDamage(damage);
            hitFlashes.push_back({hitX, hitY, 6});
            if (hitFlashes.size() > 50) hitFlashes.erase(hitFlashes.begin());
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

bool circleLineCollision(float cx, float cy, float r, float x1, float y1, float x2, float y2) {
    float dx1 = cx - x1, dy1 = cy - y1;
    float dx2 = cx - x2, dy2 = cy - y2;
    if (dx1*dx1 + dy1*dy1 < r*r) return true;
    if (dx2*dx2 + dy2*dy2 < r*r) return true;
    float lineDx = x2 - x1;
    float lineDy = y2 - y1;
    float len2 = lineDx*lineDx + lineDy*lineDy;
    if (len2 == 0.0f) return false;
    float t = ((cx - x1) * lineDx + (cy - y1) * lineDy) / len2;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float projX = x1 + t * lineDx;
    float projY = y1 + t * lineDy;
    float dist2 = (cx - projX)*(cx - projX) + (cy - projY)*(cy - projY);
    return dist2 < r*r;
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
                if (event.key.scancode == SDL_SCANCODE_ESCAPE || event.key.scancode == SDL_SCANCODE_Q) return false;
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                float mx = event.button.x, my = event.button.y;
                if (mx >= windowWidth/2 - 100 && mx <= windowWidth/2 + 100 &&
                    my >= windowHeight/2 + 50 && my <= windowHeight/2 + 100) { restart = true; waiting = false; }
                if (mx >= windowWidth/2 - 100 && mx <= windowWidth/2 + 100 &&
                    my >= windowHeight/2 + 120 && my <= windowHeight/2 + 170) return false;
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
        SDL_Color btnColor = {100,100,100,255};
        SDL_Color btnHover = {150,150,150,255};
        float mx, my;
        SDL_GetMouseState(&mx, &my);
        bool hoverRestart = (mx >= windowWidth/2-100 && mx <= windowWidth/2+100 && my >= windowHeight/2+50 && my <= windowHeight/2+100);
        bool hoverQuit = (mx >= windowWidth/2-100 && mx <= windowWidth/2+100 && my >= windowHeight/2+120 && my <= windowHeight/2+170);
        SDL_FRect restartRect = {(float)(windowWidth/2-100), (float)(windowHeight/2+50), 200, 50};
        SDL_SetRenderDrawColor(renderer, hoverRestart?btnHover.r:btnColor.r, hoverRestart?btnHover.g:btnColor.g, hoverRestart?btnHover.b:btnColor.b,255);
        SDL_RenderFillRect(renderer, &restartRect);
        SDL_SetRenderDrawColor(renderer,255,255,255,255);
        SDL_RenderRect(renderer, &restartRect);
        SDL_Surface* restartSurf = TTF_RenderText_Solid(font, "RESTART", 0, SDL_Color{255,255,255,255});
        if (restartSurf) {
            SDL_Texture* restartTex = SDL_CreateTextureFromSurface(renderer, restartSurf);
            SDL_FRect textRect = {(float)(windowWidth/2 - restartSurf->w/2), (float)(windowHeight/2+50 + (50-restartSurf->h)/2), (float)restartSurf->w, (float)restartSurf->h};
            SDL_RenderTexture(renderer, restartTex, NULL, &textRect);
            SDL_DestroyTexture(restartTex);
            SDL_DestroySurface(restartSurf);
        }
        SDL_FRect quitRect = {(float)(windowWidth/2-100), (float)(windowHeight/2+120), 200, 50};
        SDL_SetRenderDrawColor(renderer, hoverQuit?btnHover.r:btnColor.r, hoverQuit?btnHover.g:btnColor.g, hoverQuit?btnHover.b:btnColor.b,255);
        SDL_RenderFillRect(renderer, &quitRect);
        SDL_SetRenderDrawColor(renderer,255,255,255,255);
        SDL_RenderRect(renderer, &quitRect);
        SDL_Surface* quitSurf = TTF_RenderText_Solid(font, "QUIT", 0, SDL_Color{255,255,255,255});
        if (quitSurf) {
            SDL_Texture* quitTex = SDL_CreateTextureFromSurface(renderer, quitSurf);
            SDL_FRect textRect = {(float)(windowWidth/2 - quitSurf->w/2), (float)(windowHeight/2+120 + (50-quitSurf->h)/2), (float)quitSurf->w, (float)quitSurf->h};
            SDL_RenderTexture(renderer, quitTex, NULL, &textRect);
            SDL_DestroyTexture(quitTex);
            SDL_DestroySurface(quitSurf);
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
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                float mx = event.button.x, my = event.button.y;
                if (mx >= windowWidth/2-100 && mx <= windowWidth/2+100 && my >= windowHeight/2+50 && my <= windowHeight/2+100) { next = true; waiting = false; }
                if (mx >= windowWidth/2-100 && mx <= windowWidth/2+100 && my >= windowHeight/2+120 && my <= windowHeight/2+170) return false;
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
        SDL_Color btnColor = {100,100,100,255};
        SDL_Color btnHover = {150,150,150,255};
        float mx, my;
        SDL_GetMouseState(&mx, &my);
        bool hoverNext = (mx >= windowWidth/2-100 && mx <= windowWidth/2+100 && my >= windowHeight/2+50 && my <= windowHeight/2+100);
        bool hoverQuit = (mx >= windowWidth/2-100 && mx <= windowWidth/2+100 && my >= windowHeight/2+120 && my <= windowHeight/2+170);
        SDL_FRect nextRect = {(float)(windowWidth/2-100), (float)(windowHeight/2+50), 200, 50};
        SDL_SetRenderDrawColor(renderer, hoverNext?btnHover.r:btnColor.r, hoverNext?btnHover.g:btnColor.g, hoverNext?btnHover.b:btnColor.b,255);
        SDL_RenderFillRect(renderer, &nextRect);
        SDL_SetRenderDrawColor(renderer,255,255,255,255);
        SDL_RenderRect(renderer, &nextRect);
        SDL_Surface* nextSurf = TTF_RenderText_Solid(font, "NEXT LEVEL", 0, SDL_Color{255,255,255,255});
        if (nextSurf) {
            SDL_Texture* nextTex = SDL_CreateTextureFromSurface(renderer, nextSurf);
            SDL_FRect textRect = {(float)(windowWidth/2 - nextSurf->w/2), (float)(windowHeight/2+50 + (50-nextSurf->h)/2), (float)nextSurf->w, (float)nextSurf->h};
            SDL_RenderTexture(renderer, nextTex, NULL, &textRect);
            SDL_DestroyTexture(nextTex);
            SDL_DestroySurface(nextSurf);
        }
        SDL_FRect quitRect = {(float)(windowWidth/2-100), (float)(windowHeight/2+120), 200, 50};
        SDL_SetRenderDrawColor(renderer, hoverQuit?btnHover.r:btnColor.r, hoverQuit?btnHover.g:btnColor.g, hoverQuit?btnHover.b:btnColor.b,255);
        SDL_RenderFillRect(renderer, &quitRect);
        SDL_SetRenderDrawColor(renderer,255,255,255,255);
        SDL_RenderRect(renderer, &quitRect);
        SDL_Surface* quitSurf = TTF_RenderText_Solid(font, "QUIT", 0, SDL_Color{255,255,255,255});
        if (quitSurf) {
            SDL_Texture* quitTex = SDL_CreateTextureFromSurface(renderer, quitSurf);
            SDL_FRect textRect = {(float)(windowWidth/2 - quitSurf->w/2), (float)(windowHeight/2+120 + (50-quitSurf->h)/2), (float)quitSurf->w, (float)quitSurf->h};
            SDL_RenderTexture(renderer, quitTex, NULL, &textRect);
            SDL_DestroyTexture(quitTex);
            SDL_DestroySurface(quitSurf);
        }
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    return next;
}

int showMainMenu(SDL_Renderer* renderer, TTF_Font* font, int windowWidth, int windowHeight) {
    struct MenuButton {
        SDL_FRect rect;
        std::string label;
        bool hover;
    };
    MenuButton playBtn = {{(float)(windowWidth/2-100), (float)(windowHeight/2-20), 200, 50}, "Play Game", false};
    MenuButton exitBtn = {{(float)(windowWidth/2-100), (float)(windowHeight/2+50), 200, 50}, "Exit", false};
    bool menuRunning = true;
    int selected = -1;
    SDL_Event event;
    while (menuRunning) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) { selected = 1; menuRunning = false; }
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                SDL_Point point = {static_cast<int>(event.motion.x), static_cast<int>(event.motion.y)};
                SDL_Rect rect = {(int)playBtn.rect.x, (int)playBtn.rect.y, (int)playBtn.rect.w, (int)playBtn.rect.h};
                playBtn.hover = SDL_PointInRect(&point, &rect);
                rect = {(int)exitBtn.rect.x, (int)exitBtn.rect.y, (int)exitBtn.rect.w, (int)exitBtn.rect.h};
                exitBtn.hover = SDL_PointInRect(&point, &rect);
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                float mx = event.button.x, my = event.button.y;
                if (mx >= playBtn.rect.x && mx <= playBtn.rect.x+playBtn.rect.w && my >= playBtn.rect.y && my <= playBtn.rect.y+playBtn.rect.h) { selected = 0; menuRunning = false; }
                if (mx >= exitBtn.rect.x && mx <= exitBtn.rect.x+exitBtn.rect.w && my >= exitBtn.rect.y && my <= exitBtn.rect.y+exitBtn.rect.h) { selected = 1; menuRunning = false; }
            }
        }
        SDL_SetRenderDrawColor(renderer,0,0,0,255);
        SDL_RenderClear(renderer);
        SDL_Surface* titleSurf = TTF_RenderText_Solid(font, "UNDOOM", 0, SDL_Color{255,0,0,255});
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
            SDL_FRect titleRect = {(float)(windowWidth/2 - titleSurf->w/2), (float)(windowHeight/2 - 120), (float)titleSurf->w, (float)titleSurf->h};
            SDL_RenderTexture(renderer, titleTex, NULL, &titleRect);
            SDL_DestroyTexture(titleTex);
            SDL_DestroySurface(titleSurf);
        }
        SDL_Color btnColor = {100,100,100,255}, btnHover = {150,150,150,255}, textColor = {255,255,255,255};
        for (auto* btn : {&playBtn, &exitBtn}) {
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
    return selected;
}

bool runGame(SDL_Renderer* renderer, SDL_Window* window, TTF_Font* font, TTF_Font* smallFont,
             const Texture& wallTex,
             const Texture& mobWalkingTex, const Texture& mobAttackTex, const Texture& mobDeathTex,
             const Texture& bossWalkingTex, const Texture& bossAttackTex, const Texture& bossDeathTex,
             const Texture& gunTex, const Texture& shootingGunTex,
             const Texture& shotgunTex, const Texture& shotgunShotTex,
             const Texture& firstAidKitTex,
             const Texture& pistolCartridgesTex, const Texture& shotgunCartridgesTex,
             int levelNum) {

    const int WINDOW_WIDTH = 1280;
    const int WINDOW_HEIGHT = 960;

    std::string levelFile = "level" + std::to_string(levelNum) + ".txt";
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
    player.angle = 0.0f;
    player.speed = 240.0f;
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

    std::cout << "Level " << levelNum << " has " << mobs.size() << " enemies" << std::endl;

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
    int playerHealth = 100;
    int pistolAmmo = 60;
    int shotgunAmmo = 12;
    int currentWeapon = 0;
    const int playerMaxHealth = 100;
    int shootFlashFrames = 0;
    SDL_Event event;

    while (!levelComplete) {
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
            if (event.type == SDL_EVENT_QUIT) { levelComplete = true; return false; }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE) { levelComplete = true; return false; }
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

        if (playerHealth <= 0) {
            levelComplete = true;
            return false;
        }

        const float PICKUP_RADIUS = 32.0f;
        for (auto& pickup : pickups) {
            if (!pickup.active) continue;
            float dxPickup = pickup.x - player.x;
            float dyPickup = pickup.y - player.y;
            float dist = sqrtf(dxPickup*dxPickup + dyPickup*dyPickup);
            if (dist < PICKUP_RADIUS) {
                pickup.active = false;
                if (pickup.type == 2011) {
                    playerHealth = std::min(playerMaxHealth, playerHealth + 60);
                } else if (pickup.type == 2007) {
                    pistolAmmo += 20;
                } else if (pickup.type == 2008) {
                    shotgunAmmo += 6;
                }
            }
        }

        // Обновление мобов
        for (auto& mob : mobs) {
            auto& state = mobRenderStates[&mob];

            if (state.attackAnimTime > 0) state.attackAnimTime -= dt;

            if (!mob.isAlive() && !state.isDying) {
                state.isDying = true;
                state.deathTimer = 0.1f;
            }

            if (!state.isDying && mob.isAlive()) {
                float oldAttackCooldown = mob.attackCooldown;
                float oldFireCooldown = mob.fireCooldown;

                mob.update(dt, player, lines, vertices);

                bool isAttackingNow = false;
                if (mob.attackCooldown > 0 && oldAttackCooldown <= 0) isAttackingNow = true;
                if (mob.fireCooldown > 0 && oldFireCooldown <= 0 && mob.type != ZOMBIE) isAttackingNow = true;

                if (isAttackingNow) {
                    state.attackAnimTime = 0.2f;
                }

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
                    return false;
                }
            }
        }

        // Удаление мёртвых мобов
        for (auto it = mobs.begin(); it != mobs.end(); ) {
            auto& state = mobRenderStates[&(*it)];
            if (!it->isAlive() && state.isDying) {
                state.deathTimer -= dt;
                if (state.deathTimer <= 0.0f) {
                    for (auto& boss : mobs) {
                        if (boss.type == BOSS && boss.summonedMobsCount > 0) {
                            boss.summonedMobsCount--;
                            if (boss.summonedMobsCount == 0 && boss.invulnerable) {
                                boss.invulnerable = false;
                                boss.vulnerabilityTimer = 1.0f;
                            }
                            break;
                        }
                    }
                    it = mobs.erase(it);
                } else {
                    ++it;
                }
            } else if (!it->isAlive() && !state.isDying) {
                state.isDying = true;
                state.deathTimer = 0.1f;
                ++it;
            } else {
                ++it;
            }
        }

        if (mobs.empty()) {
            levelComplete = true;
            break;
        }

        if (bulletHoles.size() > 200) bulletHoles.erase(bulletHoles.begin(), bulletHoles.begin() + 50);
        if (hitFlashes.size() > 100) hitFlashes.erase(hitFlashes.begin(), hitFlashes.begin() + 20);

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

        // Рендер сцены
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

        // Пулевые отверстия
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

        // Вспышки попаданий
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

        // Аптечки и патроны
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
            float spriteHeight = 72.0f * 200.0f / distToPlayer;
            if (spriteHeight < 27) spriteHeight = 27;
            float spriteWidth = spriteHeight;
            float screenY = WINDOW_HEIGHT/2 + (56.0f / distToPlayer) * (WINDOW_HEIGHT / 1.5f) * 0.85f - spriteHeight;
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

        // Сортировка мобов по дальности
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

            // Высота спрайта
            float screenHeight = (MOB_HEIGHT_WORLD / dist) * PROJ_SCALE;
            if (screenHeight < 8) screenHeight = 8;
            if (screenHeight > WINDOW_HEIGHT) screenHeight = WINDOW_HEIGHT;

            // Уровень пола (где ноги моба)
            float floorY = WINDOW_HEIGHT/2 + (VIEW_HEIGHT - FLOOR_HEIGHT) / dist * PROJ_SCALE;
            float topY = floorY - screenHeight;
            float bottomY = floorY;

            if (topY < 0) topY = 0;
            if (bottomY > WINDOW_HEIGHT) bottomY = WINDOW_HEIGHT;
            if (topY >= bottomY) continue;

            float screenWidth = screenHeight * 0.7f;

            const Texture* mobTex = nullptr;
            auto it = mobRenderStates.find(const_cast<Mob*>(&mob));
            bool isDying = (it != mobRenderStates.end() && it->second.isDying);
            bool isAttacking = (it != mobRenderStates.end() && it->second.attackAnimTime > 0 && !isDying);

            if (mob.type == BOSS) {
                if (isDying && bossDeathTex.texture)
                    mobTex = &bossDeathTex;
                else if (isAttacking && bossAttackTex.texture)
                    mobTex = &bossAttackTex;
                else if (bossWalkingTex.texture)
                    mobTex = &bossWalkingTex;
            } else {
                if (isDying && mobDeathTex.texture)
                    mobTex = &mobDeathTex;
                else if (isAttacking && mobAttackTex.texture)
                    mobTex = &mobAttackTex;
                else if (mobWalkingTex.texture)
                    mobTex = &mobWalkingTex;
            }

            if (mobTex && mobTex->texture) {
                SDL_SetTextureBlendMode(mobTex->texture, SDL_BLENDMODE_BLEND);
                SDL_FRect srcRect = {0, 0, (float)mobTex->width, (float)mobTex->height};
                SDL_FRect dstRect = {screenX - screenWidth/2, topY, screenWidth, screenHeight};
                SDL_RenderTexture(renderer, mobTex->texture, &srcRect, &dstRect);
            } else {
                SDL_Color baseColor = (mob.type == BOSS) ? SDL_Color{150,0,150,255} : SDL_Color{0,200,0,255};
                float factor = (float)mob.hp / mob.maxHp;
                SDL_SetRenderDrawColor(renderer, baseColor.r*factor, baseColor.g*factor, baseColor.b*factor, 255);
                SDL_FRect rect = {screenX - screenWidth/2, topY, screenWidth, screenHeight};
                SDL_RenderFillRect(renderer, &rect);
            }
        }

        // Полоска здоровья босса
        for (const auto& mob : mobs) {
            if (mob.type == BOSS && mob.isAlive()) {
                auto it = mobRenderStates.find(const_cast<Mob*>(&mob));
                if (it == mobRenderStates.end() || !it->second.isDying) {
                    float healthPercent = (float)mob.hp / mob.maxHp;
                    int barWidth = 400;
                    int barX = (WINDOW_WIDTH - barWidth) / 2;
                    int barY = 30;
                    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 200);
                    SDL_FRect bg = {(float)barX, (float)barY, (float)barWidth, 16};
                    SDL_RenderFillRect(renderer, &bg);
                    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
                    SDL_FRect hp = {(float)barX, (float)barY, (float)(barWidth * healthPercent), 16};
                    SDL_RenderFillRect(renderer, &hp);
                }
                break;
            }
        }

        // Оружие в руках
        int weaponW = (currentWeapon == 0) ? 1000 : 800;
        int weaponH = (currentWeapon == 0) ? 600 : 480;
        int weaponX = (WINDOW_WIDTH - weaponW) / 2;
        int weaponY = WINDOW_HEIGHT - weaponH - 20;
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

        // HUD
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

    return true;
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Ошибка инициализации SDL: " << SDL_GetError() << std::endl;
        return 1;
    }
    if (TTF_Init() < 0) {
        std::cerr << "Ошибка инициализации SDL_ttf: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    const int WINDOW_WIDTH = 1280;
    const int WINDOW_HEIGHT = 960;
    SDL_Window* window = SDL_CreateWindow("UNDOOM", WINDOW_WIDTH, WINDOW_HEIGHT,
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

    TTF_Font* font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 24);
    if (!font) font = TTF_OpenFont("arial.ttf", 24);
    if (!font) {
        std::cerr << "Предупреждение: не удалось загрузить шрифт." << std::endl;
    }

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
    int currentLevel = 1;
    const int MAX_LEVELS = 3;

    while (running && currentLevel <= MAX_LEVELS) {
        int choice = showMainMenu(renderer, font, WINDOW_WIDTH, WINDOW_HEIGHT);
        if (choice == 0) {
            bool completed = runGame(renderer, window, font, smallFont,
                                     wallTex, mobWalkingTex, mobAttackTex, mobDeathTex,
                                     bossWalkingTex, bossAttackTex, bossDeathTex,
                                     gunTex, shootingGunTex, shotgunTex, shotgunShotTex,
                                     firstAidKitTex, pistolCartridgesTex, shotgunCartridgesTex,
                                     currentLevel);
            if (completed) {
                if (currentLevel < MAX_LEVELS) {
                    bool goNext = showLevelCompleteScreen(renderer, font, WINDOW_WIDTH, WINDOW_HEIGHT, currentLevel);
                    if (goNext) {
                        currentLevel++;
                        std::cout << "Moving to level " << currentLevel << std::endl;
                    } else {
                        running = false;
                    }
                } else {
                    std::cout << "Congratulations! You completed all levels!" << std::endl;
                    running = false;
                }
            } else {
                bool restart = showGameOverScreen(renderer, font, WINDOW_WIDTH, WINDOW_HEIGHT);
                if (restart) {
                    currentLevel = 1;
                    std::cout << "Restarting from level 1" << std::endl;
                } else {
                    running = false;
                }
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