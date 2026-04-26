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

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL_Init error: " << SDL_GetError() << std::endl;
        return 1;
    }
    if (TTF_Init() < 0) {
        std::cerr << "TTF_Init error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    TTF_Font* font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 24);
    TTF_Font* smallFont = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 18);
    if (!font) font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 20);
    if (!smallFont) smallFont = font;
    if (!font) {
        std::cerr << "Failed to load font" << std::endl;
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    const int WINDOW_WIDTH = 1280;
    const int WINDOW_HEIGHT = 960;
    SDL_Window* window = SDL_CreateWindow("Doom Clone", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window) {
        std::cerr << "Window creation error: " << SDL_GetError() << std::endl;
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "Renderer creation error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    WadLoader wad;
    if (!wad.load("freedoom1.wad")) {
        std::cerr << "Failed to load freedoom1.wad" << std::endl;
        // continue anyway, maybe custom levels only
    } else {
        listAllLumps(wad);
    }

    auto maps = getAllMaps(wad);
    if (maps.empty()) {
        std::cerr << "No maps found!" << std::endl;
        // fallback: create a dummy map? but better exit
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    int selectedMapIndex = 0;
    int scrollOffset = 0;
    const int maxVisible = 15;
    bool inMenu = true;
    bool running = true;
    SDL_Event event;

    DoomMap previewMap;
    std::vector<Vertex> previewVertices;
    std::vector<Linedef> previewLines;
    bool hasPreview = false;

    auto loadPreview = [&](int idx) {
        if (maps[idx].isCustom) {
            std::string txtFile = "level" + std::to_string(idx + 1) + ".txt";
            hasPreview = previewMap.loadFromTextFile(txtFile);
        } else {
            hasPreview = previewMap.loadFromWad(wad, maps[idx].name);
        }
        if (hasPreview) {
            previewVertices = previewMap.getVertices();
            previewLines = previewMap.getLinedefs();
        }
    };

    loadPreview(0);

    while (inMenu && running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
                inMenu = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_UP && selectedMapIndex > 0) {
                    selectedMapIndex--;
                    if (selectedMapIndex < scrollOffset) scrollOffset = selectedMapIndex;
                    loadPreview(selectedMapIndex);
                }
                if (event.key.scancode == SDL_SCANCODE_DOWN && selectedMapIndex < (int)maps.size() - 1) {
                    selectedMapIndex++;
                    if (selectedMapIndex >= scrollOffset + maxVisible) scrollOffset = selectedMapIndex - maxVisible + 1;
                    loadPreview(selectedMapIndex);
                }
                if (event.key.scancode == SDL_SCANCODE_RETURN || event.key.scancode == SDL_SCANCODE_SPACE) {
                    inMenu = false;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);

        // Title
        SDL_Surface* titleSurf = TTF_RenderText_Solid(font, "SELECT MAP", 0, SDL_Color{255, 100, 0, 255});
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
            SDL_FRect titleRect = {640.0f - titleSurf->w / 2.0f, 50.0f, (float)titleSurf->w, (float)titleSurf->h};
            SDL_RenderTexture(renderer, titleTex, NULL, &titleRect);
            SDL_DestroyTexture(titleTex);
            SDL_DestroySurface(titleSurf);
        }

        // List panel
        SDL_SetRenderDrawColor(renderer, 40, 40, 50, 255);
        SDL_FRect listPanel = {50, 120, 350, 700};
        SDL_RenderFillRect(renderer, &listPanel);
        SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
        SDL_RenderRect(renderer, &listPanel);

        int visibleCount = std::min(maxVisible, (int)maps.size() - scrollOffset);
        for (int i = 0; i < visibleCount; i++) {
            int mapIdx = scrollOffset + i;
            if (mapIdx >= (int)maps.size()) break;
            SDL_Color color = (mapIdx == selectedMapIndex) ? SDL_Color{255, 200, 0, 255} : SDL_Color{200, 200, 200, 255};
            std::string displayName = maps[mapIdx].name;
            if (maps[mapIdx].isCustom) displayName = "★ " + displayName;
            SDL_Surface* mapSurf = TTF_RenderText_Solid(smallFont, displayName.c_str(), 0, color);
            if (mapSurf) {
                SDL_Texture* mapTex = SDL_CreateTextureFromSurface(renderer, mapSurf);
                float yPos = 140.0f + i * 35.0f;
                SDL_FRect mapRect = {70, yPos, (float)mapSurf->w, (float)mapSurf->h};
                SDL_RenderTexture(renderer, mapTex, NULL, &mapRect);
                SDL_DestroyTexture(mapTex);
                SDL_DestroySurface(mapSurf);
            }
        }

        if ((int)maps.size() > maxVisible) {
            float scrollPercent = (float)scrollOffset / (maps.size() - maxVisible);
            float scrollY = 120 + scrollPercent * 700;
            SDL_FRect scrollBar = {390, scrollY, 10, 20};
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            SDL_RenderFillRect(renderer, &scrollBar);
        }

        // Preview panel
        SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
        SDL_FRect previewPanel = {450, 120, 780, 700};
        SDL_RenderFillRect(renderer, &previewPanel);
        SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
        SDL_RenderRect(renderer, &previewPanel);

        std::string selectedName = maps[selectedMapIndex].name;
        if (maps[selectedMapIndex].isCustom) selectedName = "★ " + selectedName;
        SDL_Surface* nameSurf = TTF_RenderText_Solid(font, selectedName.c_str(), 0, SDL_Color{255, 200, 0, 255});
        if (nameSurf) {
            SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
            SDL_FRect nameRect = {640.0f - nameSurf->w / 2.0f, 130.0f, (float)nameSurf->w, (float)nameSurf->h};
            SDL_RenderTexture(renderer, nameTex, NULL, &nameRect);
            SDL_DestroyTexture(nameTex);
            SDL_DestroySurface(nameSurf);
        }

        // Draw preview map (2D)
        if (hasPreview && !previewVertices.empty()) {
            int minX = previewVertices[0].x, maxX = previewVertices[0].x;
            int minY = previewVertices[0].y, maxY = previewVertices[0].y;
            for (const auto& v : previewVertices) {
                minX = std::min(minX, (int)v.x);
                maxX = std::max(maxX, (int)v.x);
                minY = std::min(minY, (int)v.y);
                maxY = std::max(maxY, (int)v.y);
            }
            float rangeX = maxX - minX;
            float rangeY = maxY - minY;
            float scaleX = 700.0f / rangeX;
            float scaleY = 600.0f / rangeY;
            float scale = std::min(scaleX, scaleY) * 0.8f;
            float offsetX = 460 + (700 - rangeX * scale) / 2 - minX * scale;
            float offsetY = 180 + (600 - rangeY * scale) / 2 + maxY * scale;
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            for (const auto& line : previewLines) {
                if (line.startVertex >= previewVertices.size() || line.endVertex >= previewVertices.size()) continue;
                const Vertex& v1 = previewVertices[line.startVertex];
                const Vertex& v2 = previewVertices[line.endVertex];
                int sx1 = (int)(v1.x * scale + offsetX);
                int sy1 = (int)(-v1.y * scale + offsetY);
                int sx2 = (int)(v2.x * scale + offsetX);
                int sy2 = (int)(-v2.y * scale + offsetY);
                SDL_RenderLine(renderer, sx1, sy1, sx2, sy2);
            }
        }

        // Instructions
        SDL_Surface* instrSurf = TTF_RenderText_Solid(smallFont,
            "UP/DOWN - select | ENTER - start | ★ - YOUR MAPS",
            0, SDL_Color{150, 150, 150, 255});
        if (instrSurf) {
            SDL_Texture* instrTex = SDL_CreateTextureFromSurface(renderer, instrSurf);
            SDL_FRect instrRect = {640.0f - instrSurf->w / 2.0f, 850.0f, (float)instrSurf->w, (float)instrSurf->h};
            SDL_RenderTexture(renderer, instrTex, NULL, &instrRect);
            SDL_DestroyTexture(instrTex);
            SDL_DestroySurface(instrSurf);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    if (!running) {
        // Clean up and exit
        TTF_CloseFont(font);
        if (smallFont != font) TTF_CloseFont(smallFont);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 0;
    }

    // --- Load selected map for gameplay ---
    MapInfo selectedMapInfo = maps[selectedMapIndex];
    DoomMap gameMap;
    bool mapLoaded = false;
    if (selectedMapInfo.isCustom) {
        std::string txtFile = "level" + std::to_string(selectedMapIndex + 1) + ".txt";
        mapLoaded = gameMap.loadFromTextFile(txtFile);
        std::cout << "Loading custom map from: " << txtFile << std::endl;
    } else {
        mapLoaded = gameMap.loadFromWad(wad, selectedMapInfo.name);
        std::cout << "Loading WAD map: " << selectedMapInfo.name << std::endl;
    }
    if (!mapLoaded) {
        std::cerr << "Failed to load map " << selectedMapInfo.name << std::endl;
        TTF_CloseFont(font);
        if (smallFont != font) TTF_CloseFont(smallFont);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    auto vertices = gameMap.getVertices();
    auto lines = gameMap.getLinedefs();
    if (vertices.empty() || lines.empty()) {
        std::cerr << "Map has no vertices or lines!" << std::endl;
        return 1;
    }

    int minX = vertices[0].x, maxX = vertices[0].x;
    int minY = vertices[0].y, maxY = vertices[0].y;
    for (const auto& v : vertices) {
        minX = std::min(minX, (int)v.x);
        maxX = std::max(maxX, (int)v.x);
        minY = std::min(minY, (int)v.y);
        maxY = std::max(maxY, (int)v.y);
    }
    float centerX = (minX + maxX) / 2.0f;
    float centerY = (minY + maxY) / 2.0f;

    Player player;
    player.x = centerX;
    player.y = centerY;
    player.angle = 0.0f;
    player.speed = 150.0f;

    bool useMyTextures = selectedMapInfo.isCustom;
    SDL_Texture* wallTex = nullptr;
    SDL_Texture* floorTex = nullptr;
    SDL_Texture* ceilingTex = nullptr;
    if (useMyTextures) {
        floorTex = loadTexture(renderer, "pol.png");
        ceilingTex = loadTexture(renderer, "potolok.png");
        wallTex = loadTexture(renderer, "stena.png");
        std::cout << "Custom textures loaded." << std::endl;
    }

    const float FOV = 90.0f * M_PI / 180.0f;
    Uint64 lastTime = SDL_GetTicks();

    while (running) {
        Uint64 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        if (deltaTime > 0.033f) deltaTime = 0.033f;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE) running = false;
        }

        const bool* keyboard = SDL_GetKeyboardState(nullptr);
        float moveSpeed = player.speed * deltaTime;
        float turnSpeed = 2.0f * deltaTime;
        float dx = 0, dy = 0;
        if (keyboard[SDL_SCANCODE_W]) { dx += cos(player.angle) * moveSpeed; dy += sin(player.angle) * moveSpeed; }
        if (keyboard[SDL_SCANCODE_S]) { dx -= cos(player.angle) * moveSpeed; dy -= sin(player.angle) * moveSpeed; }
        if (keyboard[SDL_SCANCODE_A]) { dx += sin(player.angle) * moveSpeed; dy -= cos(player.angle) * moveSpeed; }
        if (keyboard[SDL_SCANCODE_D]) { dx -= sin(player.angle) * moveSpeed; dy += cos(player.angle) * moveSpeed; }
        player.x += dx;
        player.y += dy;
        if (keyboard[SDL_SCANCODE_LEFT]) player.angle -= turnSpeed;
        if (keyboard[SDL_SCANCODE_RIGHT]) player.angle += turnSpeed;
        while (player.angle < 0) player.angle += 2*M_PI;
        while (player.angle >= 2*M_PI) player.angle -= 2*M_PI;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Floor (optimized: fill rect or stretch texture)
        if (useMyTextures && floorTex) {
            SDL_FRect floorRect = {0, (float)WINDOW_HEIGHT/2, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT/2};
            SDL_RenderTexture(renderer, floorTex, NULL, &floorRect);
        } else {
            SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
            SDL_FRect floorRect = {0, (float)WINDOW_HEIGHT/2, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT/2};
            SDL_RenderFillRect(renderer, &floorRect);
        }

        // Ceiling
        if (useMyTextures && ceilingTex) {
            SDL_FRect ceilingRect = {0, 0, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT/2};
            SDL_RenderTexture(renderer, ceilingTex, NULL, &ceilingRect);
        } else {
            SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
            SDL_FRect ceilingRect = {0, 0, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT/2};
            SDL_RenderFillRect(renderer, &ceilingRect);
        }

        // Walls (one call per column)
        for (int x = 0; x < WINDOW_WIDTH; ++x) {
            float rayAngle = player.angle + (x - WINDOW_WIDTH/2) * FOV / WINDOW_WIDTH;
            float dxRay = cos(rayAngle);
            float dyRay = sin(rayAngle);
            float closestDist = std::numeric_limits<float>::max();
            int hitLinedef = -1;
            float hitU = 0;
            for (size_t i = 0; i < lines.size(); ++i) {
                const auto& line = lines[i];
                if (line.startVertex >= vertices.size() || line.endVertex >= vertices.size()) continue;
                const Vertex& v1 = vertices[line.startVertex];
                const Vertex& v2 = vertices[line.endVertex];
                HitResult hit = rayLineIntersection(player.x, player.y, dxRay, dyRay, v1.x, v1.y, v2.x, v2.y);
                if (hit.hit && hit.distance < closestDist && hit.distance > 0.1f) {
                    closestDist = hit.distance;
                    hitLinedef = i;
                    hitU = hit.hitU;
                }
            }
            if (hitLinedef != -1 && closestDist > 0.1f) {
                float correctedDist = closestDist * cos(rayAngle - player.angle);
                if (correctedDist < 0.1f) correctedDist = 0.1f;
                float wallHeight = (128.0f / correctedDist) * 200.0f;
                if (wallHeight > WINDOW_HEIGHT) wallHeight = WINDOW_HEIGHT;
                int wallTop = (WINDOW_HEIGHT - wallHeight) / 2;
                int wallBottom = wallTop + wallHeight;
                if (wallTop < 0) wallTop = 0;
                if (wallBottom > WINDOW_HEIGHT) wallBottom = WINDOW_HEIGHT;

                if (useMyTextures && wallTex) {
                    float w, h;
                    SDL_GetTextureSize(wallTex, &w, &h);
                    int texX = (int)(hitU * w);
                    if (texX < 0) texX = 0;
                    if (texX >= (int)w) texX = (int)w - 1;
                    SDL_FRect srcRect = {(float)texX, 0, 1, h};
                    SDL_FRect dstRect = {(float)x, (float)wallTop, 1, (float)(wallBottom - wallTop)};
                    SDL_RenderTexture(renderer, wallTex, &srcRect, &dstRect);
                } else {
                    int brightness = (int)(255.0f / (correctedDist * 0.3f + 1.0f));
                    if (brightness > 255) brightness = 255;
                    if (brightness < 50) brightness = 50;
                    SDL_SetRenderDrawColor(renderer, brightness, 0, 0, 255);
                    SDL_RenderLine(renderer, x, wallTop, x, wallBottom);
                }
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    if (wallTex) SDL_DestroyTexture(wallTex);
    if (floorTex) SDL_DestroyTexture(floorTex);
    if (ceilingTex) SDL_DestroyTexture(ceilingTex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    if (smallFont != font) TTF_CloseFont(smallFont);
    TTF_Quit();
    SDL_Quit();
    return 0;
}