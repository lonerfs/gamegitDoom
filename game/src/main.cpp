#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <fstream>
#include <sstream>
#include <cstring>
#include <SDL3/SDL.h>
<<<<<<< HEAD
#include "game/WadLoader.h"
=======
#include <SDL3_image/SDL_image.h>
>>>>>>> 3c2a8b4c06806b14ae40e339208cc59ac610bbc4
#include "game/DoomMap.h"
#include "game/Player.h"
#include "SDL3/SDL.h"

struct HitResult {
    bool hit;
    float distance;
    int linedefIndex;
    float hitX, hitY;
};

HitResult rayLineIntersection(float x0, float y0, float dx, float dy,
                              float x1, float y1, float x2, float y2) {
    HitResult result = {false, 0, -1, 0, 0};

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
    }
    return result;
}

struct Texture {
    SDL_Texture* texture;
    int width;
    int height;
};

Texture loadTexture(SDL_Renderer* renderer, const char* filename) {
    Texture tex;
    SDL_Surface* surf = IMG_Load(filename);
    if (surf) {
        tex.texture = SDL_CreateTextureFromSurface(renderer, surf);
        tex.width = surf->w;
        tex.height = surf->h;
        SDL_DestroySurface(surf);
        std::cout << "Loaded texture: " << filename << " (" << tex.width << "x" << tex.height << ")" << std::endl;
    } else {
        std::cerr << "Failed to load texture: " << filename << std::endl;
        tex.texture = nullptr;
        tex.width = 64;
        tex.height = 64;
    }
    return tex;
}

void fillSurfaceColor(SDL_Surface* surf, Uint8 r, Uint8 g, Uint8 b) {
    const SDL_PixelFormatDetails* formatDetails = SDL_GetPixelFormatDetails(surf->format);
    Uint32 color = SDL_MapRGB(formatDetails, NULL, r, g, b);
    SDL_FillSurfaceRect(surf, NULL, color);
}

SDL_Texture* createFallbackTexture(SDL_Renderer* renderer, int w, int h, Uint8 r, Uint8 g, Uint8 b) {
    SDL_Surface* surf = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_RGBA32);
    if (!surf) return nullptr;

    fillSurfaceColor(surf, r, g, b);

    const SDL_PixelFormatDetails* formatDetails = SDL_GetPixelFormatDetails(surf->format);
    Uint32 darkColor = SDL_MapRGB(formatDetails, NULL, r * 0.7f, g * 0.7f, b * 0.7f);

    for (int i = 0; i < w; i += 16) {
        SDL_Rect rect = {i, 0, 8, h};
        SDL_FillSurfaceRect(surf, &rect, darkColor);
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_DestroySurface(surf);
    return tex;
}

bool pointInPolygon(float px, float py, const std::vector<std::pair<float, float>>& polygon) {
    bool inside = false;
    for (size_t i = 0, j = polygon.size() - 1; i < polygon.size(); j = i++) {
        float xi = polygon[i].first, yi = polygon[i].second;
        float xj = polygon[j].first, yj = polygon[j].second;

        bool intersect = ((yi > py) != (yj > py)) &&
                         (px < (xj - xi) * (py - yi) / (yj - yi) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

// Проверка точки внутри любого препятствия
bool pointInAnyObstacle(float px, float py) {
    std::vector<std::vector<std::pair<float, float>>> obstacles = {
        {{64,128}, {160,128}, {160,96}, {64,96}},
        {{192,224}, {192,288}, {288,288}, {288,224}},
        {{96,256}, {96,352}, {128,352}, {128,256}},
        {{64,352}, {0,352}, {0,288}, {64,288}},
        {{64,608}, {160,608}, {160,544}, {64,544}}
    };

    for (const auto& obs : obstacles) {
        if (pointInPolygon(px, py, obs)) {
            return true;
        }
    }
    return false;
}

int main() {
    DoomMap map;
<<<<<<< HEAD
    if (!map.loadFromTextFile("level3.txt")) {
        std::cerr << "Не удалось загрузить карту из level3.txt" << std::endl;
=======
    if (!map.loadFromTextFile("level1.txt")) {
        std::cerr << "Не удалось загрузить карту из level1.txt" << std::endl;
>>>>>>> 3c2a8b4c06806b14ae40e339208cc59ac610bbc4
        return 1;
    }

    const auto& vertices = map.getVertices();
    const auto& lines = map.getLinedefs();

    if (vertices.empty() || lines.empty()) {
        std::cerr << "Карта не содержит вершин или линий!" << std::endl;
        return 1;
    }

    std::cout << "Загружена карта: " << vertices.size() << " вершин, " << lines.size() << " линий" << std::endl;

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
<<<<<<< HEAD
    player.speed = 150.0f;

    std::cout << "Стартовая позиция: (" << player.x << ", " << player.y << ")" << std::endl;
    std::cout << "Границы карты: X[" << minX << "," << maxX << "] Y[" << minY << "," << maxY << "]" << std::endl;
    std::cout << "Управление: WASD - движение, стрелки - поворот" << std::endl;
=======
    player.speed = 400.0f;

    std::cout << "Стартовая позиция: (" << player.x << ", " << player.y << ")" << std::endl;
    std::cout << "Границы карты: X[" << minX << "," << maxX << "] Y[" << minY << "," << maxY << "]" << std::endl;
    std::cout << "Управление: WASD - движение, стрелки влево/вправо - поворот, ESC - выход" << std::endl;

    std::vector<std::pair<float, float>> floorPolygon = {
        {0, -128}, {-128, 0}, {-128, 64}, {-32, 64}, {-32, 128},
        {-96, 160}, {-128, 256}, {-128, 352}, {-96, 416}, {-32, 448},
        {-32, 576}, {-128, 608}, {0, 704}, {224, 704}, {352, 608},
        {256, 576}, {256, 448}, {320, 416}, {352, 352}, {352, 256},
        {320, 160}, {256, 128}, {256, 64}, {352, 64}, {352, 0}, {224, -128}
    };
>>>>>>> 3c2a8b4c06806b14ae40e339208cc59ac610bbc4

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Ошибка инициализации SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    const int WINDOW_WIDTH = 1280;
    const int WINDOW_HEIGHT = 960;
<<<<<<< HEAD
    SDL_Window* window = SDL_CreateWindow("Doom Clone - Your Map", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL);
=======
    SDL_Window* window = SDL_CreateWindow("Doom Clone - Your Map", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
>>>>>>> 3c2a8b4c06806b14ae40e339208cc59ac610bbc4
    if (!window) {
        std::cerr << "Ошибка создания окна: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "Ошибка создания рендерера: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

<<<<<<< HEAD
=======
    Texture wallTex = loadTexture(renderer, "textures/stena.png");
    Texture floorTex = loadTexture(renderer, "textures/pol.png");
    Texture ceilingTex = loadTexture(renderer, "textures/potolok.png");

    if (!wallTex.texture) {
        wallTex.texture = createFallbackTexture(renderer, 64, 64, 139, 69, 19);
        wallTex.width = 64;
        wallTex.height = 64;
    }
    if (!floorTex.texture) {
        floorTex.texture = createFallbackTexture(renderer, 64, 64, 80, 80, 80);
        floorTex.width = 64;
        floorTex.height = 64;
    }
    if (!ceilingTex.texture) {
        ceilingTex.texture = createFallbackTexture(renderer, 64, 64, 200, 200, 200);
        ceilingTex.width = 64;
        ceilingTex.height = 64;
    }

>>>>>>> 3c2a8b4c06806b14ae40e339208cc59ac610bbc4
    const float FOV = 90.0f * M_PI / 180.0f;

    bool running = true;
    SDL_Event event;
    Uint64 lastTime = SDL_GetTicks();

    while (running) {
        Uint64 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        if (deltaTime > 0.033f) deltaTime = 0.033f;

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

<<<<<<< HEAD
        float moveSpeed = player.speed * deltaTime;
        float turnSpeed = 2.0f * deltaTime;
=======
        const bool* keyboard = SDL_GetKeyboardState(nullptr);

        float dx = 0, dy = 0;
        float moveSpeed = player.speed * deltaTime;
        float turnSpeed = 4.0f * deltaTime;
>>>>>>> 3c2a8b4c06806b14ae40e339208cc59ac610bbc4

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
<<<<<<< HEAD
        }

        // Временное отключение коллизий для теста
        player.x += dx;
        player.y += dy;

        if (keyboard[SDL_SCANCODE_LEFT]) player.turnLeft(turnSpeed);
        if (keyboard[SDL_SCANCODE_RIGHT]) player.turnRight(turnSpeed);
=======
        }

        if (dx != 0 || dy != 0) {
            player.moveWithSliding(dx, dy, lines, vertices);
        }

        if (keyboard[SDL_SCANCODE_LEFT]) player.angle -= turnSpeed;
        if (keyboard[SDL_SCANCODE_RIGHT]) player.angle += turnSpeed;

        while (player.angle < 0) player.angle += 2 * M_PI;
        while (player.angle >= 2 * M_PI) player.angle -= 2 * M_PI;
>>>>>>> 3c2a8b4c06806b14ae40e339208cc59ac610bbc4

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        // Стены
        for (int x = 0; x < WINDOW_WIDTH; x++) {
            float rayAngle = player.angle + (x - WINDOW_WIDTH / 2) * FOV / WINDOW_WIDTH;
            float dxRay = cos(rayAngle);
            float dyRay = sin(rayAngle);

            float closestDist = std::numeric_limits<float>::max();
            int hitLinedef = -1;
            float hitX = 0, hitY = 0;

            for (size_t i = 0; i < lines.size(); i++) {
                const auto& line = lines[i];
                if (line.startVertex >= vertices.size() || line.endVertex >= vertices.size()) continue;

                const Vertex& v1 = vertices[line.startVertex];
                const Vertex& v2 = vertices[line.endVertex];

                HitResult hit = rayLineIntersection(player.x, player.y, dxRay, dyRay,
                                                    v1.x, v1.y, v2.x, v2.y);
                if (hit.hit && hit.distance < closestDist) {
                    closestDist = hit.distance;
                    hitLinedef = i;
                    hitX = hit.hitX;
                    hitY = hit.hitY;
                }
            }

            if (hitLinedef != -1 && closestDist > 0.1f) {
<<<<<<< HEAD
                float dist = closestDist;
                float wallHeight = 30000.0f / dist;
=======
                float correctedDist = closestDist * cos(rayAngle - player.angle);
                if (correctedDist < 0.1f) correctedDist = 0.1f;

                float wallHeight = (64.0f / correctedDist) * WINDOW_HEIGHT;
>>>>>>> 3c2a8b4c06806b14ae40e339208cc59ac610bbc4
                if (wallHeight > WINDOW_HEIGHT) wallHeight = WINDOW_HEIGHT;

                int wallTop = (WINDOW_HEIGHT - wallHeight) / 2;
                int wallBottom = wallTop + wallHeight;

                if (wallTop < 0) wallTop = 0;
                if (wallBottom > WINDOW_HEIGHT) wallBottom = WINDOW_HEIGHT;

<<<<<<< HEAD
                SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
                if (wallTop > 0) {
                    SDL_RenderLine(renderer, x, 0, x, wallTop);
=======
                const auto& line = lines[hitLinedef];
                const Vertex& v1 = vertices[line.startVertex];
                const Vertex& v2 = vertices[line.endVertex];

                float lineDx = v2.x - v1.x;
                float lineDy = v2.y - v1.y;
                float lineLen = sqrt(lineDx*lineDx + lineDy*lineDy);

                float hitPos = 0;
                if (lineLen > 0) {
                    float toHitX = hitX - v1.x;
                    float toHitY = hitY - v1.y;
                    hitPos = (toHitX * lineDx + toHitY * lineDy) / lineLen;
>>>>>>> 3c2a8b4c06806b14ae40e339208cc59ac610bbc4
                }

                int texX = (int)(hitPos * wallTex.width) % wallTex.width;
                if (texX < 0) texX += wallTex.width;

<<<<<<< HEAD
                SDL_SetRenderDrawColor(renderer, 200, 100, 50, 255);
                SDL_RenderLine(renderer, x, wallTop, x, wallBottom);
            } else {
                SDL_SetRenderDrawColor(renderer, 100, 150, 200, 255);
                SDL_RenderLine(renderer, x, 0, x, WINDOW_HEIGHT / 2);

                SDL_SetRenderDrawColor(renderer, 50, 50, 80, 255);
                SDL_RenderLine(renderer, x, WINDOW_HEIGHT / 2, x, WINDOW_HEIGHT);
=======
                for (int y = wallTop; y < wallBottom; y++) {
                    float texYcoord = (float)(y - wallTop) / wallHeight;
                    int texY = (int)(texYcoord * wallTex.height) % wallTex.height;

                    SDL_FRect srcRect = {(float)texX, (float)texY, 1, 1};
                    SDL_FRect dstRect = {(float)x, (float)y, 1, 1};
                    SDL_RenderTexture(renderer, wallTex.texture, &srcRect, &dstRect);
                }
            }
        }

        // Потолок (только внутри основного полигона, но не внутри препятствий)
        for (int x = 0; x < WINDOW_WIDTH; x++) {
            float rayAngle = player.angle + (x - WINDOW_WIDTH / 2) * FOV / WINDOW_WIDTH;
            float cosRay = cos(rayAngle);
            float sinRay = sin(rayAngle);

            for (int y = 0; y < WINDOW_HEIGHT / 2; y++) {
                float ratio = (float)(WINDOW_HEIGHT / 2 - y) / (WINDOW_HEIGHT / 2);
                if (ratio < 0.01f) ratio = 0.01f;
                float distance = 64.0f / ratio;

                float worldX = player.x + cosRay * distance;
                float worldY = player.y + sinRay * distance;

                if (pointInPolygon(worldX, worldY, floorPolygon) && !pointInAnyObstacle(worldX, worldY)) {
                    int texX = (int)(worldX * 0.5f) % ceilingTex.width;
                    if (texX < 0) texX += ceilingTex.width;
                    int texY = (int)(worldY * 0.5f) % ceilingTex.height;
                    if (texY < 0) texY += ceilingTex.height;

                    SDL_FRect srcRect = {(float)texX, (float)texY, 1, 1};
                    SDL_FRect dstRect = {(float)x, (float)y, 1, 1};
                    SDL_RenderTexture(renderer, ceilingTex.texture, &srcRect, &dstRect);
                }
            }
        }

        // Пол (только внутри основного полигона, но не внутри препятствий)
        for (int x = 0; x < WINDOW_WIDTH; x++) {
            float rayAngle = player.angle + (x - WINDOW_WIDTH / 2) * FOV / WINDOW_WIDTH;
            float cosRay = cos(rayAngle);
            float sinRay = sin(rayAngle);

            for (int y = WINDOW_HEIGHT / 2; y < WINDOW_HEIGHT; y++) {
                float ratio = (float)(y - WINDOW_HEIGHT / 2) / (WINDOW_HEIGHT / 2);
                if (ratio < 0.01f) ratio = 0.01f;
                float distance = 32.0f / ratio;

                float worldX = player.x + cosRay * distance;
                float worldY = player.y + sinRay * distance;

                if (pointInPolygon(worldX, worldY, floorPolygon) && !pointInAnyObstacle(worldX, worldY)) {
                    int texX = (int)(worldX * 0.5f) % floorTex.width;
                    if (texX < 0) texX += floorTex.width;
                    int texY = (int)(worldY * 0.5f) % floorTex.height;
                    if (texY < 0) texY += floorTex.height;

                    SDL_FRect srcRect = {(float)texX, (float)texY, 1, 1};
                    SDL_FRect dstRect = {(float)x, (float)y, 1, 1};
                    SDL_RenderTexture(renderer, floorTex.texture, &srcRect, &dstRect);
                }
>>>>>>> 3c2a8b4c06806b14ae40e339208cc59ac610bbc4
            }
        }

        static int frameCount = 0;
        frameCount++;
<<<<<<< HEAD
        if (frameCount % 30 == 0) {
=======
        if (frameCount % 60 == 0) {
>>>>>>> 3c2a8b4c06806b14ae40e339208cc59ac610bbc4
            std::cout << "Player: (" << player.x << ", " << player.y << ")  Angle: " << player.angle << std::endl;
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    SDL_DestroyTexture(wallTex.texture);
    SDL_DestroyTexture(floorTex.texture);
    SDL_DestroyTexture(ceilingTex.texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}