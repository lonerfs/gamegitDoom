#include <SDL3/SDL.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include "game/DoomMap.h"
#include "game/WadLoader.h"

class MapDrawer {
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    int windowWidth;
    int windowHeight;
    float scale;
    int offsetX;
    int offsetY;

public:
    MapDrawer(int width = 1024, int height = 768)
        : window(nullptr), renderer(nullptr), windowWidth(width), windowHeight(height) {}

    ~MapDrawer() {
        cleanup();
    }

    bool initSDL() {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
            return false;
        }

        window = SDL_CreateWindow("Doom Map Viewer",
                                  windowWidth, windowHeight,
                                  SDL_WINDOW_OPENGL);
        if (!window) {
            std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
            SDL_Quit();
            return false;
        }

        renderer = SDL_CreateRenderer(window, nullptr);
        if (!renderer) {
            std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
            SDL_DestroyWindow(window);
            SDL_Quit();
            return false;
        }

        return true;
    }

    void calculateViewParameters(const std::vector<Vertex>& vertices) {
        if (vertices.empty()) return;

        int16_t minX = vertices[0].x, maxX = vertices[0].x;
        int16_t minY = vertices[0].y, maxY = vertices[0].y;

        for (const auto& v : vertices) {
            minX = std::min(minX, v.x);
            maxX = std::max(maxX, v.x);
            minY = std::min(minY, v.y);
            maxY = std::max(maxY, v.y);
        }

        std::cout << "Map bounds: X [" << minX << ", " << maxX
                  << "], Y [" << minY << ", " << maxY << "]" << std::endl;

        const int MARGIN = 50;
        float rangeX = maxX - minX;
        float rangeY = maxY - minY;
        float scaleX = (windowWidth - 2 * MARGIN) / rangeX;
        float scaleY = (windowHeight - 2 * MARGIN) / rangeY;
        scale = std::min(scaleX, scaleY);

        offsetX = MARGIN + static_cast<int>((windowWidth - 2 * MARGIN - rangeX * scale) / 2) - static_cast<int>(minX * scale);
        offsetY = MARGIN + static_cast<int>((windowHeight - 2 * MARGIN - rangeY * scale) / 2) + static_cast<int>(maxY * scale);

        std::cout << "Scale: " << scale << std::endl;
        std::cout << "Offset: (" << offsetX << ", " << offsetY << ")" << std::endl;
    }

    void doomToScreen(int doomX, int doomY, int& screenX, int& screenY) {
        screenX = static_cast<int>(doomX * scale) + offsetX;
        screenY = static_cast<int>(-doomY * scale) + offsetY;
    }

    void drawMap(const DoomMap& map) {
        const auto& vertices = map.getVertices();
        const auto& linedefs = map.getLinedefs();

        if (vertices.empty() || linedefs.empty()) {
            std::cerr << "Map has no data to draw!" << std::endl;
            return;
        }

        calculateViewParameters(vertices);

        std::cout << "Drawing " << linedefs.size() << " lines..." << std::endl;
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

            for (const auto& line : linedefs) {
                if (line.startVertex >= vertices.size() || line.endVertex >= vertices.size()) {
                    continue;
                }

                const Vertex& v1 = vertices[line.startVertex];
                const Vertex& v2 = vertices[line.endVertex];

                int x1, y1, x2, y2;
                doomToScreen(v1.x, v1.y, x1, y1);
                doomToScreen(v2.x, v2.y, x2, y2);

                SDL_RenderLine(renderer, x1, y1, x2, y2);
            }

            SDL_RenderPresent(renderer);
            SDL_Delay(10);
        }
    }

    void cleanup() {
        if (renderer) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }
        if (window) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }
        SDL_Quit();
    }
};

bool drawMapFromWad(const std::string& wadFilename, const std::string& mapName) {
    WadLoader wad;
    if (!wad.load(wadFilename)) {
        std::cerr << "Failed to load WAD file: " << wadFilename << std::endl;
        return false;
    }

    DoomMap map;
    if (!map.loadFromWad(wad, mapName)) {
        std::cerr << "Failed to load map: " << mapName << std::endl;
        return false;
    }

    MapDrawer drawer;
    if (!drawer.initSDL()) {
        return false;
    }

    drawer.drawMap(map);
    return true;
}

bool drawMapFromTextFile(const std::string& filename) {
    DoomMap map;
    if (!map.loadFromTextFile(filename)) {
        std::cerr << "Failed to load map from text file: " << filename << std::endl;
        return false;
    }

    MapDrawer drawer;
    if (!drawer.initSDL()) {
        return false;
    }

    drawer.drawMap(map);
    return true;
}