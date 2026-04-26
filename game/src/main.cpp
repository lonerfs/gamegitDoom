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
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <SDL3_image/SDL_image.h>
#include "game/DoomMap.h"
#include "game/Player.h"
#include "game/WadLoader.h"
#include "game/RenderThread.h"

struct BulletHole {
    float x, y;
    float distance;
    int lifetime;
};

SDL_Texture* loadTexture(SDL_Renderer* renderer, const std::string& path) {
    std::string fullPath = "textures/" + path;
    SDL_Surface* surf = IMG_Load(fullPath.c_str());
    if (!surf) return nullptr;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    return tex;
}

SDL_Texture* createFallbackTexture(SDL_Renderer* renderer, int w, int h) {
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

void shootShotgun(float x, float y, float angle, const std::vector<Linedef>& lines, const std::vector<Vertex>& vertices,
                  std::vector<BulletHole>& bulletHoles, int& ammo) {
    if (ammo <= 0) {
        std::cout << "Out of shotgun ammo!" << std::endl;
        return;
    }
    const int PELLETS = 5;
    const float SPREAD = 0.1f;
    for (int i = 0; i < PELLETS; ++i) {
        float spreadAngle = angle + ((float)rand() / RAND_MAX - 0.5f) * SPREAD;
        HitResult hit = shootRay(x, y, spreadAngle, lines, vertices);
        if (hit.hit) {
            BulletHole hole;
            hole.x = hit.hitX;
            hole.y = hit.hitY;
            hole.distance = hit.distance;
            hole.lifetime = 300;
            bulletHoles.push_back(hole);
            if (bulletHoles.size() > 100) bulletHoles.erase(bulletHoles.begin());
        }
    }
    ammo--;
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

enum WeaponType { PISTOL, SHOTGUN };
struct Inventory {
    WeaponType currentWeapon;
    int shotgunAmmo;
    Inventory() : currentWeapon(PISTOL), shotgunAmmo(10) {}
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

        // Отрисовка меню
        SDL_SetRenderDrawColor(renderer, 20,20,30,255);
        SDL_RenderClear(renderer);

        SDL_Surface* titleSurf = TTF_RenderText_Solid(font, "SELECT MAP", 0, {255,100,0,255});
        if (titleSurf) {
            SDL_Texture* titleTex = SDL_CreateTextureFromSurface(renderer, titleSurf);
            SDL_FRect tr = {WINDOW_WIDTH/2.0f - titleSurf->w/2.0f, 50, (float)titleSurf->w, (float)titleSurf->h};
            SDL_RenderTexture(renderer, titleTex, NULL, &tr);
            SDL_DestroyTexture(titleTex);
            SDL_DestroySurface(titleSurf);
        }

        SDL_SetRenderDrawColor(renderer, 40,40,50,255);
        SDL_FRect listPanel = {50,120,350,700};
        SDL_RenderFillRect(renderer, &listPanel);
        SDL_SetRenderDrawColor(renderer, 100,100,120,255);
        SDL_RenderRect(renderer, &listPanel);

        int visibleCount = std::min(maxVisible, (int)maps.size()-scrollOffset);
        for (int i=0; i<visibleCount; i++) {
            int idx = scrollOffset+i;
            SDL_Color col = (idx==selectedMapIndex)? SDL_Color{255,200,0,255} : SDL_Color{200,200,200,255};
            std::string name = maps[idx].name;
            if (maps[idx].isCustom) name = "★ "+name;
            SDL_Surface* surf = TTF_RenderText_Solid(smallFont, name.c_str(), 0, col);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_FRect rect = {70, 140.0f+i*35, (float)surf->w, (float)surf->h};
                SDL_RenderTexture(renderer, tex, NULL, &rect);
                SDL_DestroyTexture(tex);
                SDL_DestroySurface(surf);
            }
        }

        if ((int)maps.size() > maxVisible) {
            float percent = (float)scrollOffset/(maps.size()-maxVisible);
            SDL_FRect bar = {390, 120+percent*700, 10, 20};
            SDL_SetRenderDrawColor(renderer, 200,200,200,255);
            SDL_RenderFillRect(renderer, &bar);
        }

        SDL_SetRenderDrawColor(renderer, 30,30,40,255);
        SDL_FRect previewPanel = {450,120,780,700};
        SDL_RenderFillRect(renderer, &previewPanel);
        SDL_SetRenderDrawColor(renderer, 100,100,120,255);
        SDL_RenderRect(renderer, &previewPanel);

        std::string selName = maps[selectedMapIndex].name;
        if (maps[selectedMapIndex].isCustom) selName = "★ "+selName;
        SDL_Surface* nameSurf = TTF_RenderText_Solid(font, selName.c_str(), 0, {255,200,0,255});
        if (nameSurf) {
            SDL_Texture* nameTex = SDL_CreateTextureFromSurface(renderer, nameSurf);
            SDL_FRect nr = {WINDOW_WIDTH/2.0f - nameSurf->w/2.0f, 130, (float)nameSurf->w, (float)nameSurf->h};
            SDL_RenderTexture(renderer, nameTex, NULL, &nr);
            SDL_DestroyTexture(nameTex);
            SDL_DestroySurface(nameSurf);
        }

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

        SDL_Surface* instr = TTF_RenderText_Solid(smallFont, "UP/DOWN - select | ENTER - start | ★ - YOUR MAPS", 0, {150,150,150,255});
        if (instr) {
            SDL_Texture* ti = SDL_CreateTextureFromSurface(renderer, instr);
            SDL_FRect ir = {WINDOW_WIDTH/2.0f - instr->w/2.0f, 850, (float)instr->w, (float)instr->h};
            SDL_RenderTexture(renderer, ti, NULL, &ir);
            SDL_DestroyTexture(ti);
            SDL_DestroySurface(instr);
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
    if (!wallTex) wallTex = createFallbackTexture(renderer, 64, 64);
    SDL_Texture* floorTex = loadTexture(renderer, "pol.png");
    if (!floorTex) floorTex = createFallbackTexture(renderer, 64, 64);
    SDL_Texture* ceilingTex = loadTexture(renderer, "potolok.png");
    if (!ceilingTex) ceilingTex = createFallbackTexture(renderer, 64, 64);

    const float FOV = M_PI/2.0f;
    Uint64 lastTime = SDL_GetTicks();

    Uint64 lastShootTime = 0;
    const Uint64 SHOOT_DELAY_MS = 300;
    int shootFlashFrames = 0;
    std::vector<BulletHole> bulletHoles;
    Inventory inventory;

    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
    ThreadPool threadPool(numThreads);

    while (running) {
        Uint64 now = SDL_GetTicks();
        float dt = (now - lastTime)/1000.0f;
        lastTime = now;
        if (dt > 0.033f) dt = 0.033f;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_1) {
                    inventory.currentWeapon = PISTOL;
                    std::cout << "Switched to Pistol" << std::endl;
                }
                if (event.key.scancode == SDL_SCANCODE_2) {
                    inventory.currentWeapon = SHOTGUN;
                    std::cout << "Switched to Shotgun, ammo: " << inventory.shotgunAmmo << std::endl;
                }
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_SPACE) {
                if (now - lastShootTime >= SHOOT_DELAY_MS) {
                    lastShootTime = now;
                    shootFlashFrames = 5;
                    if (inventory.currentWeapon == PISTOL) {
                        HitResult shot = shootRay(player.x, player.y, player.angle, lines, vertices);
                        if (shot.hit) {
                            std::cout << "Pistol shot at distance " << shot.distance << std::endl;
                            BulletHole hole;
                            hole.x = shot.hitX;
                            hole.y = shot.hitY;
                            hole.distance = shot.distance;
                            hole.lifetime = 300;
                            bulletHoles.push_back(hole);
                            if (bulletHoles.size() > 100) bulletHoles.erase(bulletHoles.begin());
                        }
                    } else if (inventory.currentWeapon == SHOTGUN) {
                        if (inventory.shotgunAmmo <= 0) {
                            std::cout << "No shotgun ammo!" << std::endl;
                        } else {
                            std::cout << "Shotgun fired!" << std::endl;
                            shootShotgun(player.x, player.y, player.angle, lines, vertices, bulletHoles, inventory.shotgunAmmo);
                        }
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

        int chunk = WINDOW_WIDTH / numThreads;
        std::vector<std::future<std::vector<ColumnResult>>> futures;
        for (unsigned int i = 0; i < numThreads; ++i) {
            int startX = i * chunk;
            int endX = (i == numThreads - 1) ? WINDOW_WIDTH : (i + 1) * chunk;
            std::packaged_task<std::vector<ColumnResult>()> task(
                [startX, endX, &player, &lines, &vertices, WINDOW_WIDTH, WINDOW_HEIGHT]() {
                    return renderColumnsRange(startX, endX, player, lines, vertices, WINDOW_WIDTH, WINDOW_HEIGHT);
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

        float texW, texH;
        SDL_GetTextureSize(wallTex, &texW, &texH);
        for (const auto& col : allColumns) {
            if (col.wallTop >= col.wallBottom) continue;
            int texX = (int)(col.hitU * texW) % (int)texW;
            if (texX < 0) texX += (int)texW;
            SDL_FRect srcRect = { (float)texX, 0.0f, 1.0f, texH };
            SDL_FRect dstRect = { (float)col.x, (float)col.wallTop, 1.0f, (float)(col.wallBottom - col.wallTop) };
            SDL_RenderTexture(renderer, wallTex, &srcRect, &dstRect);
        }

        SDL_SetRenderDrawColor(renderer, 60, 30, 15, 255);
        SDL_FRect floorRect = {0, (float)WINDOW_HEIGHT/2, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT/2};
        SDL_RenderFillRect(renderer, &floorRect);
        for (int y = 0; y < WINDOW_HEIGHT/2; ++y) {
            float t = (float)(y) / (WINDOW_HEIGHT/2);
            int bright = 40 + (int)(t * 60);
            SDL_SetRenderDrawColor(renderer, bright, bright, bright, 255);
            SDL_RenderLine(renderer, 0, y, WINDOW_WIDTH, y);
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
            float spriteHeight = 20.0f * 200.0f / dist;
            if (spriteHeight < 4) spriteHeight = 4;
            float screenY = WINDOW_HEIGHT/2 - spriteHeight/2;
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
            SDL_FRect rect = {screenX - spriteHeight/2, screenY, spriteHeight, spriteHeight};
            SDL_RenderFillRect(renderer, &rect);
        }

        if (shootFlashFrames > 0) {
            shootFlashFrames--;
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            float flashSize = 80.0f;
            SDL_FRect flashRect = { (WINDOW_WIDTH - flashSize)/2.0f, (WINDOW_HEIGHT - flashSize)/2.0f, flashSize, flashSize };
            SDL_RenderFillRect(renderer, &flashRect);
        }

        std::string hudText;
        if (inventory.currentWeapon == PISTOL) hudText = "PISTOL (INF)";
        else hudText = "SHOTGUN (" + std::to_string(inventory.shotgunAmmo) + ")";
        SDL_Surface* hudSurf = TTF_RenderText_Solid(smallFont, hudText.c_str(), 0, {255,255,255,255});
        if (hudSurf) {
            SDL_Texture* hudTex = SDL_CreateTextureFromSurface(renderer, hudSurf);
            SDL_FRect hudRect = {20, 20, (float)hudSurf->w, (float)hudSurf->h};
            SDL_RenderTexture(renderer, hudTex, NULL, &hudRect);
            SDL_DestroyTexture(hudTex);
            SDL_DestroySurface(hudSurf);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    SDL_DestroyTexture(wallTex);
    SDL_DestroyTexture(floorTex);
    SDL_DestroyTexture(ceilingTex);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    if (smallFont != font) TTF_CloseFont(smallFont);
    TTF_Quit();
    SDL_Quit();
    return 0;
}