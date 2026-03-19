#include <SDL3/SDL.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include "game/WadLoader.h"
#include "game/DoomMap.h"
#include "game/Player.h"

void doomToScreen(int doomX, int doomY, float scale, int offsetX, int offsetY, int& screenX, int& screenY) {
    screenX = static_cast<int>(doomX * scale) + offsetX;
    screenY = static_cast<int>(-doomY * scale) + offsetY;
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

    const auto& vertices = map.getVertices();
    if (!vertices.empty()) {
        float avgX = 0, avgY = 0;
        for (const auto& v : vertices) {
            avgX += v.x;
            avgY += v.y;
        }
        avgX /= vertices.size();
        avgY /= vertices.size();
        std::cout << "Средняя точка карты: (" << avgX << ", " << avgY << ")" << std::endl;
        std::cout << "Игрок стартует в: (" << player.x << ", " << player.y << ")" << std::endl;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Ошибка инициализации SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    const int WINDOW_WIDTH = 1024;
    const int WINDOW_HEIGHT = 768;
    SDL_Window* window = SDL_CreateWindow("Doom Clone - 3D Engine", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_OPENGL);
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

    float minX = vertices[0].x, maxX = vertices[0].x;
    float minY = vertices[0].y, maxY = vertices[0].y;
    for (const auto& v : vertices) {
        minX = std::min(minX, (float)v.x);
        maxX = std::max(maxX, (float)v.x);
        minY = std::min(minY, (float)v.y);
        maxY = std::max(maxY, (float)v.y);
    }

    const int MARGIN = 50;
    float rangeX = maxX - minX;
    float rangeY = maxY - minY;
    float scaleX = (WINDOW_WIDTH - 2 * MARGIN) / rangeX;
    float scaleY = (WINDOW_HEIGHT - 2 * MARGIN) / rangeY;
    float scale2D = std::min(scaleX, scaleY);

    int offsetX = MARGIN + static_cast<int>((WINDOW_WIDTH - 2 * MARGIN - rangeX * scale2D) / 2) - static_cast<int>(minX * scale2D);
    int offsetY = MARGIN + static_cast<int>((WINDOW_HEIGHT - 2 * MARGIN - rangeY * scale2D) / 2) + static_cast<int>(maxY * scale2D);

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

        if (keyboard[SDL_SCANCODE_W]) {
            player.moveForward();
        }
        if (keyboard[SDL_SCANCODE_S]) {
            player.moveBackward();
        }
        if (keyboard[SDL_SCANCODE_A]) {
            player.strafeLeft();
        }
        if (keyboard[SDL_SCANCODE_D]) {
            player.strafeRight();
        }

        float turnSpeed = 2.0f * deltaTime;
        if (keyboard[SDL_SCANCODE_LEFT]) {
            player.turnLeft(turnSpeed);
        }
        if (keyboard[SDL_SCANCODE_RIGHT]) {
            player.turnRight(turnSpeed);
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);

        for (const auto& line : map.getLinedefs()) {
            const Vertex& v1 = vertices[line.startVertex];
            const Vertex& v2 = vertices[line.endVertex];

            int x1, y1, x2, y2;
            doomToScreen(v1.x, v1.y, scale2D, offsetX, offsetY, x1, y1);
            doomToScreen(v2.x, v2.y, scale2D, offsetX, offsetY, x2, y2);

            SDL_RenderLine(renderer, x1, y1, x2, y2);
        }

        int playerScreenX, playerScreenY;
        doomToScreen(player.x, player.y, scale2D, offsetX, offsetY, playerScreenX, playerScreenY);
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_FRect playerRect = { playerScreenX - 3.0f, playerScreenY - 3.0f, 6.0f, 6.0f };
        SDL_RenderFillRect(renderer, &playerRect);
        int lookX, lookY;
        doomToScreen(player.x + cos(player.angle) * 100,
                     player.y + sin(player.angle) * 100,
                     scale2D, offsetX, offsetY, lookX, lookY);
        SDL_RenderLine(renderer, playerScreenX, playerScreenY, lookX, lookY);

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}