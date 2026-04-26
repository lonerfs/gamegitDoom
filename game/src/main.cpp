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

    // Сначала добавляем ТВОИ карты (txt)
    if (std::ifstream("level1.txt").good()) {
        maps.push_back({"MY LEVEL 1", "txt", true});
    }
    if (std::ifstream("level2.txt").good()) {
        maps.push_back({"MY LEVEL 2", "txt", true});
    }
    if (std::ifstream("level3.txt").good()) {
        maps.push_back({"MY LEVEL 3", "txt", true});
    }

    // Потом WAD карты
    for (const auto& lump : lumps) {
        std::string name(lump.name, 8);
        name.erase(std::find(name.begin(), name.end(), '\0'), name.end());

        if (name.size() >= 5 && name.substr(0, 3) == "MAP" &&
            isdigit(name[3]) && isdigit(name[4])) {
            maps.push_back({name, "wad", false});
        }
        else if (name.size() >= 4 && name[0] == 'E' && isdigit(name[1]) &&
                 name[2] == 'M' && isdigit(name[3])) {
            maps.push_back({name, "wad", false});
        }
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

// Загрузка PNG текстуры из папки textures/
SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& path) {
    // Строим путь к файлу в папке textures/
    std::string fullPath = "textures/" + path;

    std::cout << "Пытаюсь загрузить: " << fullPath << std::endl;

    SDL_Surface* surf = IMG_Load(fullPath.c_str());
    if (!surf) {
        std::cerr << "Ошибка: " << SDL_GetError() << std::endl;
        return nullptr;
    }

    std::cout << "Загружено! Размер: " << surf->w << "x" << surf->h << std::endl;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    return tex;
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Ошибка инициализации SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    if (TTF_Init() < 0) {
        std::cerr << "Ошибка инициализации TTF: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    TTF_Font* font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 24);
    TTF_Font* smallFont = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 18);
    if (!font) {
        font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 20);
        smallFont = font;
    }

    const int WINDOW_WIDTH = 1280;
    const int WINDOW_HEIGHT = 960;
    SDL_Window* window = SDL_CreateWindow("Doom Clone", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
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

    WadLoader wad;
    std::string wadFile = "freedoom1.wad";
    if (!wad.load(wadFile)) {
        std::cerr << "Не удалось загрузить " << wadFile << std::endl;
        return 1;
    }

    listAllLumps(wad);

    std::vector<MapInfo> maps = getAllMaps(wad);

    if (maps.empty()) {
        std::cerr << "Нет доступных карт!" << std::endl;
        TTF_CloseFont(font);
        if (smallFont != font) TTF_CloseFont(smallFont);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    std::cout << "Найдено карт: " << maps.size() << std::endl;
    for (const auto& m : maps) {
        std::cout << "  " << m.name << " [" << m.source << "]" << std::endl;
    }

    int selectedMapIndex = 0;
    bool inMenu = true;
    bool running = true;
    SDL_Event event;

    int scrollOffset = 0;
    int maxVisible = 15;

    DoomMap previewMap;
    std::vector<Vertex> previewVertices;
    std::vector<Linedef> previewLines;
    bool hasPreview = false;

    if (!maps.empty()) {
        if (maps[0].isCustom) {
            std::string txtFile = "level" + std::to_string(selectedMapIndex + 1) + ".txt";
            hasPreview = previewMap.loadFromTextFile(txtFile);
        } else {
            hasPreview = previewMap.loadFromWad(wad, maps[0].name);
        }
        if (hasPreview) {
            previewVertices = previewMap.getVertices();
            previewLines = previewMap.getLinedefs();
        }
    }

    while (inMenu && running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
                inMenu = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_UP) {
                    if (selectedMapIndex > 0) {
                        selectedMapIndex--;
                        if (selectedMapIndex < scrollOffset) {
                            scrollOffset = selectedMapIndex;
                        }
                        if (maps[selectedMapIndex].isCustom) {
                            std::string txtFile = "level" + std::to_string(selectedMapIndex + 1) + ".txt";
                            hasPreview = previewMap.loadFromTextFile(txtFile);
                        } else {
                            hasPreview = previewMap.loadFromWad(wad, maps[selectedMapIndex].name);
                        }
                        if (hasPreview) {
                            previewVertices = previewMap.getVertices();
                            previewLines = previewMap.getLinedefs();
                        }
                    }
                }
                if (event.key.scancode == SDL_SCANCODE_DOWN) {
                    if (selectedMapIndex < (int)maps.size() - 1) {
                        selectedMapIndex++;
                        if (selectedMapIndex >= scrollOffset + maxVisible) {
                            scrollOffset = selectedMapIndex - maxVisible + 1;
                        }
                        if (maps[selectedMapIndex].isCustom) {
                            std::string txtFile = "level" + std::to_string(selectedMapIndex + 1) + ".txt";
                            hasPreview = previewMap.loadFromTextFile(txtFile);
                        } else {
                            hasPreview = previewMap.loadFromWad(wad, maps[selectedMapIndex].name);
                        }
                        if (hasPreview) {
                            previewVertices = previewMap.getVertices();
                            previewLines = previewMap.getLinedefs();
                        }
                    }
                }
                if (event.key.scancode == SDL_SCANCODE_RETURN || event.key.scancode == SDL_SCANCODE_SPACE) {
                    inMenu = false;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);

        SDL_Surface* titleSurf = TTF_RenderText_Solid(font, "SELECT MAP", 0, SDL_Color{255, 100, 0, 255});
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
            SDL_FRect titleRect = {640.0f - titleSurf->w / 2.0f, 50.0f, (float)titleSurf->w, (float)titleSurf->h};
            SDL_RenderTexture(renderer, titleTex, NULL, &titleRect);
            SDL_DestroyTexture(titleTex);
            SDL_DestroySurface(titleSurf);
        }

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
            if (maps[mapIdx].isCustom) {
                displayName = "★ " + displayName;
            }

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

        SDL_SetRenderDrawColor(renderer, 30, 30, 40, 255);
        SDL_FRect previewPanel = {450, 120, 780, 700};
        SDL_RenderFillRect(renderer, &previewPanel);
        SDL_SetRenderDrawColor(renderer, 100, 100, 120, 255);
        SDL_RenderRect(renderer, &previewPanel);

        std::string selectedName = maps[selectedMapIndex].name;
        if (maps[selectedMapIndex].isCustom) {
            selectedName = "★ " + selectedName;
        }
        SDL_Surface* nameSurf = TTF_RenderText_Solid(font, selectedName.c_str(), 0, SDL_Color{255, 200, 0, 255});
        if (nameSurf) {
            SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
            SDL_FRect nameRect = {640.0f - nameSurf->w / 2.0f, 130.0f, (float)nameSurf->w, (float)nameSurf->h};
            SDL_RenderTexture(renderer, nameTex, NULL, &nameRect);
            SDL_DestroyTexture(nameTex);
            SDL_DestroySurface(nameSurf);
        }

        if (hasPreview && !previewVertices.empty()) {
            int minX = previewVertices[0].x, maxX = previewVertices[0].x;
            int minY = previewVertices[0].y, maxY = previewVertices[0].y;
            for (const auto& v : previewVertices) {
                minX = std::min(minX, (int)v.x);
                maxX = std::max(maxX, (int)v.x);
                minY = std::min(minY, (int)v.y);
                maxY = std::max(maxY, (int)v.y);
            }

            float scaleX = 700.0f / (maxX - minX);
            float scaleY = 600.0f / (maxY - minY);
            float scale = std::min(scaleX, scaleY) * 0.8f;

            float offsetX = 460 + (700 - (maxX - minX) * scale) / 2 - minX * scale;
            float offsetY = 180 + (600 - (maxY - minY) * scale) / 2 + maxY * scale;

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
        TTF_CloseFont(font);
        if (smallFont != font) TTF_CloseFont(smallFont);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 0;
    }

    MapInfo selectedMapInfo = maps[selectedMapIndex];
    DoomMap gameMap;
    bool mapLoaded = false;

    if (selectedMapInfo.isCustom) {
        std::string txtFile = "level" + std::to_string(selectedMapIndex + 1) + ".txt";
        mapLoaded = gameMap.loadFromTextFile(txtFile);
        std::cout << "Loading YOUR map from: " << txtFile << std::endl;
    } else {
        mapLoaded = gameMap.loadFromWad(wad, selectedMapInfo.name);
        std::cout << "Loading WAD map: " << selectedMapInfo.name << std::endl;
    }

    if (!mapLoaded) {
        std::cerr << "Не удалось загрузить карту " << selectedMapInfo.name << std::endl;
        TTF_CloseFont(font);
        if (smallFont != font) TTF_CloseFont(smallFont);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    std::vector<Vertex> vertices = gameMap.getVertices();
    std::vector<Linedef> lines = gameMap.getLinedefs();

    if (vertices.empty() || lines.empty()) {
        std::cerr << "Карта не содержит вершин или линий!" << std::endl;
        TTF_CloseFont(font);
        if (smallFont != font) TTF_CloseFont(smallFont);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
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

    std::cout << "Стартовая позиция: (" << player.x << ", " << player.y << ")" << std::endl;
    std::cout << "Границы карты: X[" << minX << "," << maxX << "] Y[" << minY << "," << maxY << "]" << std::endl;
    std::cout << "Управление: WASD - движение, стрелки - поворот" << std::endl;

    // ЗАГРУЖАЕМ ТЕКСТУРЫ ДЛЯ ТВОИХ КАРТ ИЗ ПАПКИ textures/
    SDL_Texture* floorTex = nullptr;
    SDL_Texture* ceilingTex = nullptr;
    SDL_Texture* wallTex = nullptr;
    bool useMyTextures = (selectedMapInfo.isCustom);

    if (useMyTextures) {
        std::cout << "\n=== ЗАГРУЗКА ТЕКСТУР ДЛЯ ТВОЕЙ КАРТЫ ===" << std::endl;
        floorTex = loadTexture(renderer, "pol.png");
        ceilingTex = loadTexture(renderer, "potolok.png");
        wallTex = loadTexture(renderer, "stena.png");
        std::cout << "=====================================\n" << std::endl;
    }

    float floorTexW = 64, floorTexH = 64;
    float ceilingTexW = 64, ceilingTexH = 64;
    float wallTexW = 64, wallTexH = 64;
    if (floorTex) SDL_GetTextureSize(floorTex, &floorTexW, &floorTexH);
    if (ceilingTex) SDL_GetTextureSize(ceilingTex, &ceilingTexW, &ceilingTexH);
    if (wallTex) SDL_GetTextureSize(wallTex, &wallTexW, &wallTexH);

    const float FOV = 90.0f * M_PI / 180.0f;
    Uint64 lastTime = SDL_GetTicks();

    while (running) {
        Uint64 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        if (deltaTime > 0.033f) deltaTime = 0.033f;
        if (deltaTime < 0.001f) deltaTime = 0.001f;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE) {
                running = false;
            }
        }

        const bool* keyboard = SDL_GetKeyboardState(nullptr);

        float moveSpeed = player.speed * deltaTime;
        float turnSpeed = 2.0f * deltaTime;

        float dx = 0, dy = 0;

        if (keyboard[SDL_SCANCODE_W]) {
            dx += cos(player.angle) * moveSpeed;
            dy += sin(player.angle) * moveSpeed;
        }
        if (keyboard[SDL_SCANCODE_S]) {
            dx -= cos(player.angle) * moveSpeed;
            dy -= sin(player.angle) * moveSpeed;
        }
        if (keyboard[SDL_SCANCODE_A]) {
            dx += sin(player.angle) * moveSpeed;
            dy -= cos(player.angle) * moveSpeed;
        }
        if (keyboard[SDL_SCANCODE_D]) {
            dx -= sin(player.angle) * moveSpeed;
            dy += cos(player.angle) * moveSpeed;
        }

        // Временное отключение коллизий для теста
        player.x += dx;
        player.y += dy;

        if (keyboard[SDL_SCANCODE_LEFT]) player.angle -= turnSpeed;
        if (keyboard[SDL_SCANCODE_RIGHT]) player.angle += turnSpeed;

        while (player.angle < 0) player.angle += 2 * M_PI;
        while (player.angle >= 2 * M_PI) player.angle -= 2 * M_PI;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // ОТРИСОВКА ПОЛА
        if (useMyTextures && floorTex) {
            for (int y = WINDOW_HEIGHT / 2; y < WINDOW_HEIGHT; y++) {
                float vy = (float)(y - WINDOW_HEIGHT / 2) / (WINDOW_HEIGHT / 2);
                if (vy == 0) continue;

                float vz = 1.0f / vy;
                float px = player.x + cos(player.angle) * vz * 50.0f;
                float py = player.y + sin(player.angle) * vz * 50.0f;

                float tx = fmod(px / 64.0f, 1.0f);
                float ty = fmod(py / 64.0f, 1.0f);
                if (tx < 0) tx += 1;
                if (ty < 0) ty += 1;

                SDL_FRect srcRect = {tx * floorTexW, ty * floorTexH, 1, 1};
                SDL_FRect dstRect = {0, (float)y, (float)WINDOW_WIDTH, 1};
                SDL_RenderTexture(renderer, floorTex, &srcRect, &dstRect);
            }
        } else {
            SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
            SDL_FRect floorRect = {0, WINDOW_HEIGHT / 2, WINDOW_WIDTH, WINDOW_HEIGHT / 2};
            SDL_RenderFillRect(renderer, &floorRect);
        }

        // ОТРИСОВКА ПОТОЛКА
        if (useMyTextures && ceilingTex) {
            for (int y = 0; y < WINDOW_HEIGHT / 2; y++) {
                float vy = (float)(y - WINDOW_HEIGHT / 2) / (WINDOW_HEIGHT / 2);
                if (vy == 0) continue;

                float vz = -1.0f / vy;
                float px = player.x + cos(player.angle) * vz * 50.0f;
                float py = player.y + sin(player.angle) * vz * 50.0f;

                float tx = fmod(px / 64.0f, 1.0f);
                float ty = fmod(py / 64.0f, 1.0f);
                if (tx < 0) tx += 1;
                if (ty < 0) ty += 1;

                SDL_FRect srcRect = {tx * ceilingTexW, ty * ceilingTexH, 1, 1};
                SDL_FRect dstRect = {0, (float)y, (float)WINDOW_WIDTH, 1};
                SDL_RenderTexture(renderer, ceilingTex, &srcRect, &dstRect);
            }
        } else {
            SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
            SDL_FRect ceilingRect = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT / 2};
            SDL_RenderFillRect(renderer, &ceilingRect);
        }

        // ОТРИСОВКА СТЕН
        for (int x = 0; x < WINDOW_WIDTH; x++) {
            float rayAngle = player.angle + (x - WINDOW_WIDTH / 2) * FOV / WINDOW_WIDTH;
            float dxRay = cos(rayAngle);
            float dyRay = sin(rayAngle);

            float closestDist = std::numeric_limits<float>::max();
            int hitLinedef = -1;
            float hitU = 0;

            for (size_t i = 0; i < lines.size(); i++) {
                const auto& line = lines[i];
                if (line.startVertex >= vertices.size() || line.endVertex >= vertices.size()) continue;

                const Vertex& v1 = vertices[line.startVertex];
                const Vertex& v2 = vertices[line.endVertex];

                HitResult hit = rayLineIntersection(player.x, player.y, dxRay, dyRay,
                                                    v1.x, v1.y, v2.x, v2.y);
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
                int wallTop = (int)((WINDOW_HEIGHT - wallHeight) / 2);
                int wallBottom = (int)(wallTop + wallHeight);

                if (wallHeight > WINDOW_HEIGHT) wallHeight = WINDOW_HEIGHT;
                wallTop = (WINDOW_HEIGHT - wallHeight) / 2;
                wallBottom = wallTop + wallHeight;

                if (wallTop < 0) wallTop = 0;
                if (wallBottom > WINDOW_HEIGHT) wallBottom = WINDOW_HEIGHT;

                if (useMyTextures && wallTex) {
                    for (int wy = wallTop; wy < wallBottom; wy++) {
                        float t = (float)(wy - wallTop) / (wallBottom - wallTop);
                        int texY = (int)(t * wallTexH);
                        if (texY < 0) texY = 0;
                        if (texY >= (int)wallTexH) texY = (int)wallTexH - 1;
                        int texX = (int)(hitU * wallTexW);
                        if (texX < 0) texX = 0;
                        if (texX >= (int)wallTexW) texX = (int)wallTexW - 1;

                        SDL_FRect srcRect = {(float)texX, (float)texY, 1, 1};
                        SDL_FRect dstRect = {(float)x, (float)wy, 1, 1};
                        SDL_RenderTexture(renderer, wallTex, &srcRect, &dstRect);
                    }
                } else {
                    int brightness = (int)(255.0f / (correctedDist * 0.3f + 1.0f));
                    if (brightness > 255) brightness = 255;
                    if (brightness < 50) brightness = 50;

                    SDL_SetRenderDrawColor(renderer, brightness, 0, 0, 255);
                    SDL_RenderLine(renderer, x, wallTop, x, wallBottom);
                }
            }
        }

        static int frameCount = 0;
        frameCount++;
        if (frameCount % 30 == 0) {
            std::cout << "Player: (" << player.x << ", " << player.y << ")  Angle: " << player.angle << std::endl;
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    if (floorTex) SDL_DestroyTexture(floorTex);
    if (ceilingTex) SDL_DestroyTexture(ceilingTex);
    if (wallTex) SDL_DestroyTexture(wallTex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    if (smallFont != font) TTF_CloseFont(smallFont);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
