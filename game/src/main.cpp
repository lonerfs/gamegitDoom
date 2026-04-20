#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <fstream>
#include <sstream>
#include "game/WadLoader.h"
#include "game/DoomMap.h"
#include "game/Player.h"
#include "SDL3/SDL.h"
#include "SDL3_ttf/SDL_ttf.h"


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

//Меню

struct MenuButton {
    SDL_FRect rect;
    std::string label;
    bool hover;
};

void drawText(SDL_Renderer* renderer, TTF_Font* font, const std::string& text,
              int x, int y, SDL_Color color) {
    if (!font) return;
    // ВАЖНО: четвёртый параметр — длина строки
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), text.length(), color);
    if (!surface) return;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_FRect dstRect = {static_cast<float>(x), static_cast<float>(y),
                             static_cast<float>(surface->w), static_cast<float>(surface->h)};
        SDL_RenderTexture(renderer, texture, nullptr, &dstRect);
        SDL_DestroyTexture(texture);
    }
    SDL_DestroySurface(surface);
}

int showMainMenu(SDL_Renderer* renderer, TTF_Font* font, int windowWidth, int windowHeight) {
    MenuButton playBtn = {{static_cast<float>(windowWidth/2 - 100),
                           static_cast<float>(windowHeight/2 - 40), 200, 50}, "Play Game", false};
    MenuButton editorBtn = {{static_cast<float>(windowWidth/2 - 100),
                             static_cast<float>(windowHeight/2 + 20), 200, 50}, "Map Editor", false};
    MenuButton exitBtn = {{static_cast<float>(windowWidth/2 - 100),
                           static_cast<float>(windowHeight/2 + 80), 200, 50}, "Exit", false};

    bool menuRunning = true;
    int selected = -1;

    while (menuRunning) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                selected = 2;
                menuRunning = false;
            }
            if (event.type == SDL_EVENT_MOUSE_MOTION) {
                SDL_Point point = {static_cast<int>(event.motion.x), static_cast<int>(event.motion.y)};
                SDL_Rect rect = {static_cast<int>(playBtn.rect.x), static_cast<int>(playBtn.rect.y),
                                 static_cast<int>(playBtn.rect.w), static_cast<int>(playBtn.rect.h)};
                playBtn.hover = SDL_PointInRect(&point, &rect);
                rect = {static_cast<int>(editorBtn.rect.x), static_cast<int>(editorBtn.rect.y),
                        static_cast<int>(editorBtn.rect.w), static_cast<int>(editorBtn.rect.h)};
                editorBtn.hover = SDL_PointInRect(&point, &rect);
                rect = {static_cast<int>(exitBtn.rect.x), static_cast<int>(exitBtn.rect.y),
                        static_cast<int>(exitBtn.rect.w), static_cast<int>(exitBtn.rect.h)};
                exitBtn.hover = SDL_PointInRect(&point, &rect);
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                SDL_Point point = {static_cast<int>(event.button.x), static_cast<int>(event.button.y)};
                SDL_Rect rect = {static_cast<int>(playBtn.rect.x), static_cast<int>(playBtn.rect.y),
                                 static_cast<int>(playBtn.rect.w), static_cast<int>(playBtn.rect.h)};
                if (SDL_PointInRect(&point, &rect)) {
                    selected = 0;
                    menuRunning = false;
                }
                rect = {static_cast<int>(editorBtn.rect.x), static_cast<int>(editorBtn.rect.y),
                        static_cast<int>(editorBtn.rect.w), static_cast<int>(editorBtn.rect.h)};
                if (SDL_PointInRect(&point, &rect)) {
                    selected = 1;
                    menuRunning = false;
                }
                rect = {static_cast<int>(exitBtn.rect.x), static_cast<int>(exitBtn.rect.y),
                        static_cast<int>(exitBtn.rect.w), static_cast<int>(exitBtn.rect.h)};
                if (SDL_PointInRect(&point, &rect)) {
                    selected = 2;
                    menuRunning = false;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        SDL_Color titleColor = {255, 0, 0, 255};
        drawText(renderer, font, "UNDOOM", windowWidth/2 - 60, windowHeight/2 - 120, titleColor);

        SDL_Color btnColor = {100, 100, 100, 255};
        SDL_Color btnHoverColor = {150, 150, 150, 255};
        SDL_Color textColor = {255, 255, 255, 255};

        for (auto* btn : {&playBtn, &editorBtn, &exitBtn}) {
            SDL_SetRenderDrawColor(renderer,
                btn->hover ? btnHoverColor.r : btnColor.r,
                btn->hover ? btnHoverColor.g : btnColor.g,
                btn->hover ? btnHoverColor.b : btnColor.b, 255);
            SDL_RenderFillRect(renderer, &btn->rect);
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderRect(renderer, &btn->rect);
            int textX = static_cast<int>(btn->rect.x + (btn->rect.w - static_cast<float>(btn->label.length())*12)/2);
            int textY = static_cast<int>(btn->rect.y + (btn->rect.h - 20)/2);
            drawText(renderer, font, btn->label, textX, textY, textColor);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
    return selected;
}

// Игра

void runGame(SDL_Renderer* renderer, SDL_Window* window, TTF_Font* font) {
    WadLoader wad;
    if (!wad.load("freedoom1.wad")) {
        std::cerr << "Не удалось загрузить WAD файл" << std::endl;
        return;
    }
    DoomMap map;
    if (!map.loadFromWad(wad, "E1M1")) {
        std::cerr << "Не удалось загрузить карту E1M1" << std::endl;
        return;
    }

    Player player;
    player.x = 1280.0f;
    player.y = 1280.0f;

    const auto& lines = map.getLinedefs();
    const auto& vertices = map.getVertices();
    const float FOV = 90.0f * M_PI / 180.0f;
    int windowWidth, windowHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);

    bool gameRunning = true;
    SDL_Event event;
    Uint64 lastTime = SDL_GetTicks();

    while (gameRunning) {
        Uint64 currentTime = SDL_GetTicks();
        float deltaTime = (currentTime - lastTime) / 1000.0f;
        lastTime = currentTime;

        const bool* keyboard = SDL_GetKeyboardState(nullptr);
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                gameRunning = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                gameRunning = false;
            }
            if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                SDL_GetWindowSize(window, &windowWidth, &windowHeight);
            }
        }

        float moveSpeed = player.speed * deltaTime * 30;
        float turnSpeed = 2.0f * deltaTime;

        if (keyboard[SDL_SCANCODE_W]) {
            float dx = cos(player.angle) * moveSpeed;
            float dy = sin(player.angle) * moveSpeed;
            player.moveWithSliding(dx, dy, lines, vertices);
        }
        if (keyboard[SDL_SCANCODE_S]) {
            float dx = -cos(player.angle) * moveSpeed;
            float dy = -sin(player.angle) * moveSpeed;
            player.moveWithSliding(dx, dy, lines, vertices);
        }
        if (keyboard[SDL_SCANCODE_A]) {
            float dx = sin(player.angle) * moveSpeed;
            float dy =  -cos(player.angle) * moveSpeed;
            player.moveWithSliding(dx, dy, lines, vertices);
        }
        if (keyboard[SDL_SCANCODE_D]) {
            float dx =  -sin(player.angle) * moveSpeed;
            float dy = cos(player.angle) * moveSpeed;
            player.moveWithSliding(dx, dy, lines, vertices);
        }
        if (keyboard[SDL_SCANCODE_LEFT]) player.turnLeft(turnSpeed);
        if (keyboard[SDL_SCANCODE_RIGHT]) player.turnRight(turnSpeed);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        for (int x = 0; x < windowWidth; x++) {
            float rayAngle = player.angle + (x - windowWidth / 2) * FOV / windowWidth;
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
                if (closestDist < 0.001f) closestDist = 0.001f;
                float wallHeight = 30000.0f / closestDist;
                if (wallHeight > windowHeight) wallHeight = windowHeight;
                int wallTop = (windowHeight - wallHeight) / 2;
                int wallBottom = wallTop + wallHeight;

                SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
                if (wallTop > 0) SDL_RenderLine(renderer, static_cast<float>(x), 0.0f, static_cast<float>(x), static_cast<float>(wallTop));
                SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255);
                if (wallBottom < windowHeight) SDL_RenderLine(renderer, static_cast<float>(x), static_cast<float>(wallBottom), static_cast<float>(x), static_cast<float>(windowHeight));
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                SDL_RenderLine(renderer, static_cast<float>(x), static_cast<float>(wallTop), static_cast<float>(x), static_cast<float>(wallBottom));
            } else {
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
                SDL_RenderLine(renderer, static_cast<float>(x), 0.0f, static_cast<float>(x), static_cast<float>(windowHeight));
            }
        }

        static int frameCount = 0;
        frameCount++;
        if (frameCount % 60 == 0) {
            float rayAngle = player.angle;
            float dx = cos(rayAngle), dy = sin(rayAngle);
            float closestDist = std::numeric_limits<float>::max();
            for (size_t i = 0; i < lines.size(); i++) {
                const auto& line = lines[i];
                const Vertex& v1 = vertices[line.startVertex];
                const Vertex& v2 = vertices[line.endVertex];
                HitResult hit = rayLineIntersection(player.x, player.y, dx, dy, v1.x, v1.y, v2.x, v2.y);
                if (hit.hit && hit.distance < closestDist) closestDist = hit.distance;
            }
            std::cout << "Player: (" << player.x << ", " << player.y << ")  Angle: " << player.angle
                      << "  Wall dist: " << closestDist << std::endl;
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }
}

// Редактор карт

void runMapEditor(SDL_Renderer* renderer, SDL_Window* window, TTF_Font* font) {
    std::vector<Vertex> editVertices;
    std::vector<Linedef> editLinedefs;

    float cameraX = 0.0f, cameraY = 0.0f;
    float zoom = 0.1f;
    int windowWidth, windowHeight;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);

    int selectedVertex = -1;

    std::cout << "Map Editor: создана новая пустая карта." << std::endl;
    std::cout << "Управление: ЛКМ - добавить вершину, ПКМ - создать линию (с последней выбранной), S - сохранить, ESC - выход" << std::endl;

    bool editorRunning = true;
    SDL_Event event;

    while (editorRunning) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                editorRunning = false;
            }
            if (event.type == SDL_EVENT_KEY_DOWN) {
                if (event.key.key == SDLK_ESCAPE) {
                    editorRunning = false;
                } else if (event.key.key == SDLK_S) {
                    std::ofstream out("map_editor_output.txt");
                    if (out) {
                        out << "[VERTICES]\n";
                        for (const auto& v : editVertices) {
                            out << v.x << " " << v.y << "\n";
                        }
                        out << "[LINEDEFS]\n";
                        for (const auto& l : editLinedefs) {
                            out << l.startVertex << " " << l.endVertex << "\n";
                        }
                        out << "[THINGS]\n";
                        std::cout << "Карта сохранена в map_editor_output.txt" << std::endl;
                    } else {
                        std::cerr << "Не удалось сохранить карту" << std::endl;
                    }
                }
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                int mx = event.button.x, my = event.button.y;
                float worldX = (mx - windowWidth/2) / zoom + cameraX;
                float worldY = (my - windowHeight/2) / zoom + cameraY;

                if (event.button.button == SDL_BUTTON_LEFT) {
                    editVertices.push_back({static_cast<int16_t>(worldX), static_cast<int16_t>(worldY)});
                    std::cout << "Добавлена вершина " << editVertices.size()-1 << ": (" << worldX << ", " << worldY << ")" << std::endl;
                }
                else if (event.button.button == SDL_BUTTON_RIGHT) {
                    float bestDist = 20.0f / zoom;
                    int bestIdx = -1;
                    for (size_t i = 0; i < editVertices.size(); i++) {
                        float dx = editVertices[i].x - worldX;
                        float dy = editVertices[i].y - worldY;
                        float dist = std::sqrt(dx*dx + dy*dy);
                        if (dist < bestDist) {
                            bestDist = dist;
                            bestIdx = i;
                        }
                    }
                    if (bestIdx != -1) {
                        if (selectedVertex == -1) {
                            selectedVertex = bestIdx;
                            std::cout << "Выбрана вершина " << bestIdx << " для создания линии" << std::endl;
                        } else {
                            if (selectedVertex != bestIdx) {
                                editLinedefs.push_back({static_cast<uint16_t>(selectedVertex),
                                                        static_cast<uint16_t>(bestIdx),
                                                        0, 0, 0, 0, 0xFFFF});
                                std::cout << "Добавлена линия " << editLinedefs.size()-1 << ": " << selectedVertex << " - " << bestIdx << std::endl;
                            }
                            selectedVertex = -1;
                        }
                    } else {
                        selectedVertex = -1;
                    }
                }
            }
            if (event.type == SDL_EVENT_MOUSE_WHEEL) {
                float newZoom = zoom * (1.0f - event.wheel.y * 0.1f);
                if (newZoom >= 0.02f && newZoom <= 2.0f) zoom = newZoom;
            }
            if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                SDL_GetWindowSize(window, &windowWidth, &windowHeight);
            }
        }

        const bool* keyboard = SDL_GetKeyboardState(nullptr);
        float moveSpeed = 20.0f / zoom;
        if (keyboard[SDL_SCANCODE_W]) cameraY -= moveSpeed;
        if (keyboard[SDL_SCANCODE_S]) cameraY += moveSpeed;
        if (keyboard[SDL_SCANCODE_A]) cameraX -= moveSpeed;
        if (keyboard[SDL_SCANCODE_D]) cameraX += moveSpeed;

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        // Сетка
        SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
        int step = 100;
        float startX = cameraX - windowWidth/(2*zoom);
        float endX = cameraX + windowWidth/(2*zoom);
        float startY = cameraY - windowHeight/(2*zoom);
        float endY = cameraY + windowHeight/(2*zoom);
        int firstX = static_cast<int>(std::floor(startX / step)) * step;
        for (int x = firstX; x <= endX; x += step) {
            int sx = static_cast<int>((x - cameraX) * zoom + windowWidth/2);
            SDL_RenderLine(renderer, static_cast<float>(sx), 0.0f, static_cast<float>(sx), static_cast<float>(windowHeight));
        }
        int firstY = static_cast<int>(std::floor(startY / step)) * step;
        for (int y = firstY; y <= endY; y += step) {
            int sy = static_cast<int>((y - cameraY) * zoom + windowHeight/2);
            SDL_RenderLine(renderer, 0.0f, static_cast<float>(sy), static_cast<float>(windowWidth), static_cast<float>(sy));
        }

        // Линии
        SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
        for (const auto& line : editLinedefs) {
            if (line.startVertex < editVertices.size() && line.endVertex < editVertices.size()) {
                const Vertex& v1 = editVertices[line.startVertex];
                const Vertex& v2 = editVertices[line.endVertex];
                int x1 = static_cast<int>((v1.x - cameraX) * zoom + windowWidth/2);
                int y1 = static_cast<int>((v1.y - cameraY) * zoom + windowHeight/2);
                int x2 = static_cast<int>((v2.x - cameraX) * zoom + windowWidth/2);
                int y2 = static_cast<int>((v2.y - cameraY) * zoom + windowHeight/2);
                SDL_RenderLine(renderer, static_cast<float>(x1), static_cast<float>(y1),
                               static_cast<float>(x2), static_cast<float>(y2));
            }
        }

        // Вершины
        for (size_t i = 0; i < editVertices.size(); i++) {
            const Vertex& v = editVertices[i];
            int sx = static_cast<int>((v.x - cameraX) * zoom + windowWidth/2);
            int sy = static_cast<int>((v.y - cameraY) * zoom + windowHeight/2);
            if (selectedVertex == static_cast<int>(i)) {
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            }
            for (int dy = -2; dy <= 2; dy++) {
                for (int dx = -2; dx <= 2; dx++) {
                    if (dx*dx + dy*dy <= 4) {
                        SDL_RenderPoint(renderer, static_cast<float>(sx+dx), static_cast<float>(sy+dy));
                    }
                }
            }
        }

        if (font) {
            SDL_Color helpColor = {255, 255, 255, 255};
            drawText(renderer, font, "ESC: exit   S: save   LMB: add vertex   RMB: create line", 10, 10, helpColor);
            drawText(renderer, font, "WASD: move camera   Mouse wheel: zoom", 10, 30, helpColor);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }
}


int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Ошибка инициализации SDL: " << SDL_GetError() << std::endl;
        return 1;
    }
    if (TTF_Init() < 0) {
        std::cerr << "Ошибка инициализации SDL_ttf: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    const int WINDOW_WIDTH = 1280;
    const int WINDOW_HEIGHT = 960;
    SDL_Window* window = SDL_CreateWindow("UNDOOM", WINDOW_WIDTH, WINDOW_HEIGHT,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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

    TTF_Font* font = TTF_OpenFont("arial.ttf", 24);
    if (!font) {
        font = TTF_OpenFont("C:/Windows/Fonts/Arial.ttf", 24);
        if (!font) {
            std::cerr << "Предупреждение: не удалось загрузить шрифт. Текст не будет отображаться." << std::endl;
        }
    }

    bool quit = false;
    while (!quit) {
        int choice = showMainMenu(renderer, font, WINDOW_WIDTH, WINDOW_HEIGHT);
        switch (choice) {
            case 0:
                runGame(renderer, window, font);
                break;
            case 1:
                runMapEditor(renderer, window, font);
                break;
            case 2:
                quit = true;
                break;
            default:
                break;
        }
    }

    if (font) TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}