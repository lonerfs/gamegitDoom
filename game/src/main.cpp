#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <fstream>
#include <sstream>
#include <cstring>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include "game/DoomMap.h"
#include "game/Player.h"
#include "game/WadLoader.h"

struct HitResult {
    bool hit;
    float distance;
    int linedefIndex;
    float hitX, hitY;
    float hitU;
};

HitResult rayLineIntersection(float x0, float y0, float dx, float dy,
                              float x1, float y1, float x2, float y2) {
    HitResult result = {false, 0, -1, 0, 0, 0};
    float vx = x2 - x1;
    float vy = y2 - y1;
    float wx = x0 - x1;
    float wy = y0 - y1;
    float det = dx * vy - dy * vx;
    if (std::abs(det) < 1e-6f) return result;
    float t = (vx * wy - vy * wx) / det;
    float u = (dx * wy - dy * wx) / det;
    if (t > 0 && u >= 0 && u <= 1) {
        result.hit = true;
        result.distance = t * std::sqrt(dx*dx + dy*dy);
        result.hitX = x0 + t * dx;
        result.hitY = y0 + t * dy;
        result.hitU = u;
    }
    return result;
}

HitResult shootRay(float x, float y, float angle, const std::vector<Linedef>& lines, const std::vector<Vertex>& vertices) {
    float dx = cos(angle);
    float dy = sin(angle);
    float closestDist = std::numeric_limits<float>::max();
    HitResult best = {false, 0, -1, 0, 0, 0};
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].startVertex >= vertices.size() || lines[i].endVertex >= vertices.size()) continue;
        const Vertex& v1 = vertices[lines[i].startVertex];
        const Vertex& v2 = vertices[lines[i].endVertex];
        HitResult hit = rayLineIntersection(x, y, dx, dy, v1.x, v1.y, v2.x, v2.y);
        if (hit.hit && hit.distance < closestDist && hit.distance > 0.1f) {
            closestDist = hit.distance;
            best = hit;
            best.linedefIndex = i;
        }
    }
    return best;
}

bool isPointVisible(float px, float py, float tx, float ty, const std::vector<Linedef>& lines, const std::vector<Vertex>& vertices) {
    float dx = tx - px;
    float dy = ty - py;
    float dist = std::sqrt(dx*dx + dy*dy);
    if (dist < 0.1f) return true;
    float step = 0.1f;
    float nx = dx / dist;
    float ny = dy / dist;
    for (float d = 0; d < dist; d += step) {
        float cx = px + nx * d;
        float cy = py + ny * d;
        for (const auto& line : lines) {
            if (line.startVertex >= vertices.size() || line.endVertex >= vertices.size()) continue;
            const Vertex& v1 = vertices[line.startVertex];
            const Vertex& v2 = vertices[line.endVertex];
            HitResult hit = rayLineIntersection(px, py, nx, ny, v1.x, v1.y, v2.x, v2.y);
            if (hit.hit && hit.distance < d+step && hit.distance > 0.1f) {
                // Если пересечение ближе, чем точка, то не видно
                if (hit.distance < dist - 0.1f) return false;
            }
        }
    }
    return true;
}

SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& path) {
    std::string fullPath = "textures/" + path;
    SDL_Surface* surf = IMG_Load(fullPath.c_str());
    if (!surf) {
        std::cerr << "Failed to load texture: " << fullPath << " - " << SDL_GetError() << std::endl;
        return nullptr;
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    return tex;
}

SDL_Texture* createBrickTexture(SDL_Renderer* renderer, int w, int h) {
    SDL_Surface* surf = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    if (!surf) return nullptr;
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(surf->format);
    Uint32 brick = SDL_MapRGB(fmt, NULL, 180, 80, 60);
    Uint32 mortar = SDL_MapRGB(fmt, NULL, 100, 100, 100);
    SDL_FillSurfaceRect(surf, NULL, mortar);
    int brickW = w / 8;
    int brickH = h / 6;
    if (brickW < 2) brickW = 2;
    if (brickH < 2) brickH = 2;
    for (int row = 0; row < 6; ++row) {
        int y = row * brickH;
        int offset = (row % 2 == 0) ? 0 : brickW / 2;
        for (int col = 0; col < 12; ++col) {
            int x = offset + col * brickW;
            if (x + brickW > w) break;
            SDL_Rect rect = {x, y, brickW, brickH-1};
            SDL_FillSurfaceRect(surf, &rect, brick);
        }
    }
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    return tex;
}

struct MapInfo {
    std::string name;
    std::string source;
    bool isCustom;
};

std::vector<MapInfo> getAllMaps(const WadLoader& wad) {
    std::vector<MapInfo> maps;
    const auto& lumps = wad.getLumps();
    if (std::ifstream("level1.txt").good()) maps.push_back({"MY LEVEL 1", "txt", true});
    if (std::ifstream("level2.txt").good()) maps.push_back({"MY LEVEL 2", "txt", true});
    if (std::ifstream("level3.txt").good()) maps.push_back({"MY LEVEL 3", "txt", true});
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
    std::cout << "=== LUMP LIST ===" << std::endl;
    const auto& lumps = wad.getLumps();
    for (size_t i = 0; i < lumps.size(); i++) {
        std::string name(lumps[i].name, 8);
        name.erase(std::find(name.begin(), name.end(), '\0'), name.end());
        std::cout << i << ": '" << name << "' (size: " << lumps[i].size << ")" << std::endl;
    }
    std::cout << "================" << std::endl;
}

struct BulletHole {
    float x, y;
    float distance;
    int lifetime;
};

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

    WadLoader wad;
    if (!wad.load("freedoom1.wad")) {
        std::cerr << "Warning: freedoom1.wad not loaded (custom maps only)" << std::endl;
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

        SDL_Surface* ts = TTF_RenderText_Solid(font, "SELECT MAP (press ENTER)", 0, {255,100,0,255});
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            SDL_FRect rect = {WINDOW_WIDTH/2.0f - ts->w/2.0f, WINDOW_HEIGHT/2.0f - ts->h/2.0f, (float)ts->w, (float)ts->h};
            SDL_RenderTexture(renderer, tt, NULL, &rect);
            SDL_DestroyTexture(tt);
            SDL_DestroySurface(ts);
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

    MapInfo selected = maps[selectedMapIndex];
    DoomMap gameMap;
    if (selected.isCustom) gameMap.loadFromTextFile("level" + std::to_string(selectedMapIndex+1) + ".txt");
    else gameMap.loadFromWad(wad, selected.name);

    auto vertices = gameMap.getVertices();
    auto lines = gameMap.getLinedefs();
    if (vertices.empty() || lines.empty()) return 1;

    Player player;
    player.x = 112.0f;
    player.y = -64.0f;
    player.angle = 0.0f;
    player.speed = 150.0f;

    SDL_Texture* wallTex = loadTexture(renderer, "stena.png");
    if (!wallTex) wallTex = createBrickTexture(renderer, 64, 64);

    const float FOV = M_PI/2.0f;
    Uint64 lastTime = SDL_GetTicks();

    Uint64 lastShootTime = 0;
    const Uint64 SHOOT_DELAY_MS = 300;
    int shootFlashFrames = 0;
    std::vector<BulletHole> bulletHoles;  // список дырок

    while (running) {
        Uint64 now = SDL_GetTicks();
        float dt = (now - lastTime)/1000.0f;
        lastTime = now;
        if (dt > 0.033f) dt = 0.033f;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE) running = false;

            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_SPACE) {
                if (now - lastShootTime >= SHOOT_DELAY_MS) {
                    lastShootTime = now;
                    shootFlashFrames = 5;
                    HitResult shot = shootRay(player.x, player.y, player.angle, lines, vertices);
                    if (shot.hit) {
                        std::cout << "Shot hit at distance " << shot.distance
                                  << ", coords: (" << shot.hitX << ", " << shot.hitY << ")" << std::endl;
                        BulletHole hole;
                        hole.x = shot.hitX;
                        hole.y = shot.hitY;
                        hole.distance = shot.distance;
                        hole.lifetime = 300;   // ~5 секунд при 60 FPS
                        bulletHoles.push_back(hole);
                        if (bulletHoles.size() > 50) bulletHoles.erase(bulletHoles.begin());
                    }
                }
            }
        }


        const bool* keys = SDL_GetKeyboardState(nullptr);
        float move = player.speed * dt;
        float turn = 2.0f * dt;

        float dx = 0, dy = 0;
        if (keys[SDL_SCANCODE_W]) { dx += cos(player.angle)*move; dy += sin(player.angle)*move; }
        if (keys[SDL_SCANCODE_S]) { dx -= cos(player.angle)*move; dy -= sin(player.angle)*move; }
        if (keys[SDL_SCANCODE_A]) { dx += sin(player.angle)*move; dy -= cos(player.angle)*move; }
        if (keys[SDL_SCANCODE_D]) { dx -= sin(player.angle)*move; dy += cos(player.angle)*move; }

        if (dx != 0 || dy != 0) {
            player.moveWithSliding(dx, dy, lines, vertices);
        }

        if (keys[SDL_SCANCODE_LEFT]) player.angle -= turn;
        if (keys[SDL_SCANCODE_RIGHT]) player.angle += turn;
        while (player.angle < 0) player.angle += 2*M_PI;
        while (player.angle >= 2*M_PI) player.angle -= 2*M_PI;

        for (auto it = bulletHoles.begin(); it != bulletHoles.end(); ) {
            it->lifetime--;
            if (it->lifetime <= 0) it = bulletHoles.erase(it);
            else ++it;
        }

        SDL_SetRenderDrawColor(renderer, 0,0,0,255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 60, 30, 15, 255);
        SDL_FRect floorRect = {0, (float)WINDOW_HEIGHT/2, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT/2};
        SDL_RenderFillRect(renderer, &floorRect);

        for (int y = 0; y < WINDOW_HEIGHT/2; ++y) {
            float t = (float)(y) / (WINDOW_HEIGHT/2);
            int bright = 40 + (int)(t * 60);
            SDL_SetRenderDrawColor(renderer, bright, bright, bright, 255);
            SDL_RenderLine(renderer, 0, y, WINDOW_WIDTH, y);
        }

        for (int x = 0; x < WINDOW_WIDTH; ++x) {
            float rayAngle = player.angle + (x - WINDOW_WIDTH/2) * FOV / WINDOW_WIDTH;
            float dxRay = cos(rayAngle);
            float dyRay = sin(rayAngle);
            float closest = std::numeric_limits<float>::max();
            int hitL = -1;
            float hitU = 0;
            for (size_t i = 0; i < lines.size(); ++i) {
                if (lines[i].startVertex >= vertices.size() || lines[i].endVertex >= vertices.size()) continue;
                Vertex& v1 = vertices[lines[i].startVertex];
                Vertex& v2 = vertices[lines[i].endVertex];
                HitResult h = rayLineIntersection(player.x, player.y, dxRay, dyRay, v1.x, v1.y, v2.x, v2.y);
                if (h.hit && h.distance < closest && h.distance > 0.1f) {
                    closest = h.distance;
                    hitL = i;
                    hitU = h.hitU;
                }
            }
            if (hitL != -1 && closest > 0.1f) {
                float d = closest * cos(rayAngle - player.angle);
                if (d < 0.1f) d = 0.1f;
                float wallHeight = (128.0f / d) * 350.0f;
                if (wallHeight > WINDOW_HEIGHT) wallHeight = WINDOW_HEIGHT;
                int top = (WINDOW_HEIGHT - wallHeight)/2;
                int bot = top + wallHeight;
                if (top < 0) top = 0;
                if (bot > WINDOW_HEIGHT) bot = WINDOW_HEIGHT;

                float w, hT;
                SDL_GetTextureSize(wallTex, &w, &hT);
                int texX = (int)(hitU * w);
                if (texX < 0) texX = 0;
                if (texX >= (int)w) texX = (int)w-1;
                SDL_FRect src = {(float)texX, 0, 1, hT};
                SDL_FRect dst = {(float)x, (float)top, 1, (float)(bot-top)};
                SDL_RenderTexture(renderer, wallTex, &src, &dst);
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

            // Определяем экранный x
            float screenX = (angleDiff / (FOV/2)) * (WINDOW_WIDTH/2) + WINDOW_WIDTH/2;
            if (screenX < 0 || screenX >= WINDOW_WIDTH) continue;

            float dist = std::sqrt(dxTo*dxTo + dyTo*dyTo);
            float spriteHeight = 20.0f * 200.0f / dist;
            if (spriteHeight < 4) spriteHeight = 4;
            float spriteYcenter = WINDOW_HEIGHT/2;
            float screenY = spriteYcenter - spriteHeight/2;

            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_FRect rect = {screenX - spriteHeight/2, screenY, spriteHeight, spriteHeight};
            SDL_RenderFillRect(renderer, &rect);
        }

        if (shootFlashFrames > 0) {
            shootFlashFrames--;
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            int flashSize = 80;
            SDL_FRect flashRect = { (WINDOW_WIDTH - flashSize)/2.0f, (WINDOW_HEIGHT - flashSize)/2.0f, flashSize, flashSize };
            SDL_RenderFillRect(renderer, &flashRect);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    SDL_DestroyTexture(wallTex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    if (smallFont != font) TTF_CloseFont(smallFont);
    TTF_Quit();
    SDL_Quit();
    return 0;
}