#include <SDL3/SDL.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
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

int main(int argc, char* argv[]) {
    WadLoader wad;
    if (!wad.load("freedoom1.wad")) {
        std::cerr << "Не удалось загрузить WAD файл" << std::endl;
        return 1;
    }

    DoomMap map;
    if (!map.loadFromWad(wad, "E1M1")) {
        std::cerr << "Не удалось загрузить карту E1M1" << std::endl;
        return 1;
    }

    Player player;
    player.x = 1280.0f;
    player.y = 1280.0f;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Ошибка инициализации SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    const int WINDOW_WIDTH = 1280;
    const int WINDOW_HEIGHT = 960;
    SDL_Window* window = SDL_CreateWindow("Doom Clone - Raycasting", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL);
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

    const auto& lines = map.getLinedefs();
    const auto& vertices = map.getVertices();

    const float FOV = 90.0f * M_PI / 180.0f;

    bool running = true;
    SDL_Event event;
    Uint64 lastTime = SDL_GetTicks();

    while (running) {
        Uint64 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        const bool* keyboard = SDL_GetKeyboardState(nullptr);

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        float moveSpeed = player.speed * deltaTime * 30;
        if (keyboard[SDL_SCANCODE_W]) player.moveForward();
        if (keyboard[SDL_SCANCODE_S]) player.moveBackward();
        if (keyboard[SDL_SCANCODE_A]) player.strafeLeft();
        if (keyboard[SDL_SCANCODE_D]) player.strafeRight();

        float turnSpeed = 2.0f * deltaTime;
        if (keyboard[SDL_SCANCODE_LEFT]) player.turnLeft(turnSpeed);
        if (keyboard[SDL_SCANCODE_RIGHT]) player.turnRight(turnSpeed);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        for (int x = 0; x < WINDOW_WIDTH; x++) {
            float rayAngle = player.angle + (x - WINDOW_WIDTH / 2) * FOV / WINDOW_WIDTH;
            float dx = cos(rayAngle);
            float dy = sin(rayAngle);

            float closestDist = std::numeric_limits<float>::max();
            int hitLinedef = -1;

            for (size_t i = 0; i < lines.size(); i++) {
                const auto& line = lines[i];
                const Vertex& v1 = vertices[line.startVertex];
                const Vertex& v2 = vertices[line.endVertex];

                HitResult hit = rayLineIntersection(player.x, player.y, dx, dy,
                                                    v1.x, v1.y, v2.x, v2.y);
                if (hit.hit && hit.distance < closestDist) {
                    closestDist = hit.distance;
                    hitLinedef = i;
                }
            }

            if (hitLinedef != -1) {
                float wallHeight = 30000.0f / closestDist;
                if (wallHeight > WINDOW_HEIGHT) wallHeight = WINDOW_HEIGHT;

                int wallTop = (WINDOW_HEIGHT - wallHeight) / 2;
                int wallBottom = wallTop + wallHeight;

                SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
                if (wallTop > 0) {
                    SDL_RenderLine(renderer, x, 0, x, wallTop);
                }

                SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
                if (wallBottom < WINDOW_HEIGHT) {
                    SDL_RenderLine(renderer, x, wallBottom, x, WINDOW_HEIGHT);
                }

                // Рисуем стену
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                SDL_RenderLine(renderer, x, wallTop, x, wallBottom);
            } else {

                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
                SDL_RenderLine(renderer, x, 0, x, WINDOW_HEIGHT);
            }
        }

        static int frameCount = 0;
        frameCount++;
        if (frameCount % 60 == 0) {
            float rayAngle = player.angle;
            float dx = cos(rayAngle);
            float dy = sin(rayAngle);
            float closestDist = std::numeric_limits<float>::max();
            for (size_t i = 0; i < lines.size(); i++) {
                const auto& line = lines[i];
                const Vertex& v1 = vertices[line.startVertex];
                const Vertex& v2 = vertices[line.endVertex];
                HitResult hit = rayLineIntersection(player.x, player.y, dx, dy,
                                                    v1.x, v1.y, v2.x, v2.y);
                if (hit.hit && hit.distance < closestDist) {
                    closestDist = hit.distance;
                }
            }
            std::cout << "Player: (" << player.x << ", " << player.y << ")  Angel: " << player.angle
                      << "  Wall ->: " << closestDist << std::endl;
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}