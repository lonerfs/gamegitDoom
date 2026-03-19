#include <SDL3/SDL.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include "game/WadLoader.h"
#include "game/DoomMap.h"

void doomToScreen(int doomX, int doomY, float scale, int offsetX, int offsetY, int& screenX, int& screenY) {
    screenX = static_cast<int>(doomX * scale) + offsetX;
    screenY = static_cast<int>(-doomY * scale) + offsetY;
}

int main(int argc, char* argv[]) {
    WadLoader wad;
    if (!wad.load("freedoom1.wad")) {
        std::cerr << "Failed to load WAD file" << std::endl;
        return 1;
    }

    DoomMap map;
    if (!map.loadFromWad(wad, "E1M8")) {
        std::cerr << "Failed to load map E1M8" << std::endl;
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return 1;
    }

    const int WINDOW_WIDTH = 1024;
    const int WINDOW_HEIGHT = 768;

    SDL_Window* window = SDL_CreateWindow("Doom Clone - 2D Map",
                                          WINDOW_WIDTH, WINDOW_HEIGHT,
                                          SDL_WINDOW_OPENGL);
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    const auto& vertices = map.getVertices();
    if (vertices.empty()) {
        std::cerr << "Map has no vertices!" << std::endl;
        return 1;
    }

    int16_t minX = vertices[0].x, maxX = vertices[0].x;
    int16_t minY = vertices[0].y, maxY = vertices[0].y;
    for (const auto& v : vertices) {
        minX = std::min(minX, v.x);
        maxX = std::max(maxX, v.x);
        minY = std::min(minY, v.y);
        maxY = std::max(maxY, v.y);
    }

    std::cout << "Map bounds: X [" << minX << ", " << maxX << "], Y [" << minY << ", " << maxY << "]" << std::endl;

    const int MARGIN = 50;
    float rangeX = maxX - minX;
    float rangeY = maxY - minY;
    float scaleX = (WINDOW_WIDTH - 2 * MARGIN) / rangeX;
    float scaleY = (WINDOW_HEIGHT - 2 * MARGIN) / rangeY;
    float scale = std::min(scaleX, scaleY);

    int offsetX = MARGIN + static_cast<int>((WINDOW_WIDTH - 2 * MARGIN - rangeX * scale) / 2) - static_cast<int>(minX * scale);
    int offsetY = MARGIN + static_cast<int>((WINDOW_HEIGHT - 2 * MARGIN - rangeY * scale) / 2) + static_cast<int>(maxY * scale);

    std::cout << "Scale: " << scale << std::endl;
    std::cout << "Offset: (" << offsetX << ", " << offsetY << ")" << std::endl;
    std::cout << "Press ESC or close window to exit" << std::endl;

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    running = false;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

        const auto& linedefs = map.getLinedefs();
        const auto& vertices = map.getVertices();
        for (const auto& line : linedefs) {
            if (line.startVertex >= vertices.size() || line.endVertex >= vertices.size()) {
                continue;
            }

            const Vertex& v1 = vertices[line.startVertex];
            const Vertex& v2 = vertices[line.endVertex];
            int x1, y1, x2, y2;
            doomToScreen(v1.x, v1.y, scale, offsetX, offsetY, x1, y1);
            doomToScreen(v2.x, v2.y, scale, offsetX, offsetY, x2, y2);

            SDL_RenderLine(renderer, x1, y1, x2, y2);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}