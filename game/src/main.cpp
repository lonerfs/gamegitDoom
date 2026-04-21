#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <fstream>
#include <sstream>
#include <cstring>
#include <SDL3/SDL.h>
#include "game/WadLoader.h"
#include "game/DoomMap.h"
#include "game/Player.h"

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

int main() {
    DoomMap map;
    if (!map.loadFromTextFile("level3.txt")) {
        std::cerr << "Не удалось загрузить карту из level3.txt" << std::endl;
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
    player.speed = 150.0f;

    std::cout << "Стартовая позиция: (" << player.x << ", " << player.y << ")" << std::endl;
    std::cout << "Границы карты: X[" << minX << "," << maxX << "] Y[" << minY << "," << maxY << "]" << std::endl;
    std::cout << "Управление: WASD - движение, стрелки - поворот" << std::endl;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Ошибка инициализации SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    const int WINDOW_WIDTH = 1280;
    const int WINDOW_HEIGHT = 960;
    SDL_Window* window = SDL_CreateWindow("Doom Clone - Your Map", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL);
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

    const float FOV = 90.0f * M_PI / 180.0f;

    bool running = true;
    SDL_Event event;
    Uint64 lastTime = SDL_GetTicks();

    while (running) {
        Uint64 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;
        if (deltaTime > 0.033f) deltaTime = 0.033f;

        const bool* keyboard = SDL_GetKeyboardState(nullptr);

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

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

        if (keyboard[SDL_SCANCODE_LEFT]) player.turnLeft(turnSpeed);
        if (keyboard[SDL_SCANCODE_RIGHT]) player.turnRight(turnSpeed);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        for (int x = 0; x < WINDOW_WIDTH; x++) {
            float rayAngle = player.angle + (x - WINDOW_WIDTH / 2) * FOV / WINDOW_WIDTH;
            float dxRay = cos(rayAngle);
            float dyRay = sin(rayAngle);

            float closestDist = std::numeric_limits<float>::max();
            int hitLinedef = -1;

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
                }
            }

            if (hitLinedef != -1 && closestDist > 0.1f) {
                float dist = closestDist;
                float wallHeight = 30000.0f / dist;
                if (wallHeight > WINDOW_HEIGHT) wallHeight = WINDOW_HEIGHT;

                int wallTop = (WINDOW_HEIGHT - wallHeight) / 2;
                int wallBottom = wallTop + wallHeight;

                if (wallTop < 0) wallTop = 0;
                if (wallBottom > WINDOW_HEIGHT) wallBottom = WINDOW_HEIGHT;

                SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
                if (wallTop > 0) {
                    SDL_RenderLine(renderer, x, 0, x, wallTop);
                }

                SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
                if (wallBottom < WINDOW_HEIGHT) {
                    SDL_RenderLine(renderer, x, wallBottom, x, WINDOW_HEIGHT);
                }

                SDL_SetRenderDrawColor(renderer, 200, 100, 50, 255);
                SDL_RenderLine(renderer, x, wallTop, x, wallBottom);
            } else {
                SDL_SetRenderDrawColor(renderer, 100, 150, 200, 255);
                SDL_RenderLine(renderer, x, 0, x, WINDOW_HEIGHT / 2);

                SDL_SetRenderDrawColor(renderer, 50, 50, 80, 255);
                SDL_RenderLine(renderer, x, WINDOW_HEIGHT / 2, x, WINDOW_HEIGHT);
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

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}