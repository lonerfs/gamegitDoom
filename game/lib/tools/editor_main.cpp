#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include "game/DoomMap.h"
#include "game/WadLoader.h"
#include "MapToWad.h"

// -------------------------------
// Структура кнопки (используем SDL_FRect)
// -------------------------------
struct Button {
    SDL_FRect rect;
    const char* text;
    bool pressed;
    bool (*onClick)(void* userData);
    void* userData;
};

bool Button_HandleEvent(Button* btn, SDL_Event* ev) {
    if (ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev->button.button == 1) {
        SDL_FPoint point = { (float)ev->button.x, (float)ev->button.y };
        if (SDL_PointInRectFloat(&point, &btn->rect)) {
            btn->pressed = true;
            return true;
        }
    }
    if (ev->type == SDL_EVENT_MOUSE_BUTTON_UP && btn->pressed) {
        btn->pressed = false;
        SDL_FPoint point = { (float)ev->button.x, (float)ev->button.y };
        if (SDL_PointInRectFloat(&point, &btn->rect)) {
            if (btn->onClick) btn->onClick(btn->userData);
        }
        return true;
    }
    return false;
}

void Button_Render(SDL_Renderer* renderer, Button* btn, TTF_Font* font) {
    // Рамка кнопки
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderFillRect(renderer, &btn->rect);
    if (btn->pressed) {
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
        SDL_RenderRect(renderer, &btn->rect);
    }
    // Текст
    SDL_Surface* surf = TTF_RenderText_Solid(font, btn->text, 0, SDL_Color{255,255,255,255});
    if (surf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        float texW, texH;
        SDL_GetTextureSize(tex, &texW, &texH);
        SDL_FRect dst = {
            btn->rect.x + (btn->rect.w - texW) / 2.0f,
            btn->rect.y + (btn->rect.h - texH) / 2.0f,
            texW, texH
        };
        SDL_RenderTexture(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
        SDL_DestroySurface(surf);  // вместо SDL_FreeSurface
    }
}

// -------------------------------
// Состояние редактора
// -------------------------------
enum EditMode { MODE_VERTEX, MODE_LINE };
EditMode currentMode = MODE_VERTEX;
DoomMap map;
int selectedVertex = -1;
int pendingLineStart = -1; // для режима линий: первая выбранная вершина

// Параметры отображения
float viewScale = 1.0f;
int viewOffsetX = 200; // отступ для панели инструментов
int viewOffsetY = 0;

// Преобразование координат
void worldToScreen(int wx, int wy, int& sx, int& sy) {
    sx = wx * viewScale + viewOffsetX;
    sy = -wy * viewScale + viewOffsetY;
}

void screenToWorld(int sx, int sy, int& wx, int& wy) {
    wx = (sx - viewOffsetX) / viewScale;
    wy = -(sy - viewOffsetY) / viewScale;
}

// Вспомогательные функции для добавления (т.к. геттеры константные)
void addVertex(DoomMap& m, int16_t x, int16_t y) {
    // Временный костыль: используем константный каст (некрасиво, но работает)
    const_cast<std::vector<Vertex>&>(m.getVertices()).push_back({x, y});
}
void addLinedef(DoomMap& m, const Linedef& line) {
    const_cast<std::vector<Linedef>&>(m.getLinedefs()).push_back(line);
}

// -------------------------------
// Callback'и для кнопок
// -------------------------------
bool setModeVertex(void*) {
    currentMode = MODE_VERTEX;
    pendingLineStart = -1;
    selectedVertex = -1;
    return true;
}
bool setModeLine(void*) {
    currentMode = MODE_LINE;
    pendingLineStart = -1;
    selectedVertex = -1;
    return true;
}
bool saveMap(void*) {
    // map.saveToTextFile("lastmap.txt"); // пока нет такого метода
    std::cout << "Save not implemented yet\n";
    return true;
}
bool exportToWad(void*) {
    // map.saveToTextFile("_temp.txt");
    // if (MapToWad::addMapToWad("freedoom1.wad", "_temp.txt", "MAP01")) {
    //     std::cout << "Exported\n";
    // }
    std::cout << "Export disabled\n";
    return true;
}

// -------------------------------
// Главная функция
// -------------------------------
int main(int argc, char* argv[]) {
    // Инициализация SDL
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    if (!TTF_Init()) {
        std::cerr << "TTF_Init Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Doom Map Editor", 1280, 720, 0);
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        TTF_Quit(); SDL_Quit(); return 1;
    }
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window); TTF_Quit(); SDL_Quit(); return 1;
    }

    // Загружаем шрифт
    TTF_Font* font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 14);
    if (!font) {
        std::cerr << "Failed to load font: " << SDL_GetError() << std::endl;
    }

    // Загружаем карту, если есть
    map.loadFromTextFile("lastmap.txt");

    // Создаём кнопки (используем float координаты)
    std::vector<Button> buttons;
    buttons.push_back({{10,10,80,30}, "Vertex", false, setModeVertex, nullptr});
    buttons.push_back({{100,10,80,30}, "Line", false, setModeLine, nullptr});
    buttons.push_back({{190,10,80,30}, "Save", false, saveMap, nullptr});
    buttons.push_back({{280,10,100,30}, "To WAD", false, exportToWad, nullptr});

    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) running = false;

            // Обработка кнопок
            for (auto& btn : buttons) {
                if (Button_HandleEvent(&btn, &ev)) break;
            }

            // Обработка мыши на карте
            if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == 1) {
                int mx = ev.button.x;
                int my = ev.button.y;
                // Клик в области карты (правее панели)
                if (mx > viewOffsetX) {
                    int wx, wy;
                    screenToWorld(mx, my, wx, wy);
                    if (currentMode == MODE_VERTEX) {
                        // Добавить новую вершину
                        addVertex(map, (int16_t)wx, (int16_t)wy);
                        selectedVertex = map.getVertices().size() - 1;
                    }
                    else if (currentMode == MODE_LINE) {
                        // Найти ближайшую вершину
                        int bestIdx = -1;
                        float bestDist = 20.0f;
                        for (size_t i = 0; i < map.getVertices().size(); ++i) {
                            int sx, sy;
                            worldToScreen(map.getVertices()[i].x, map.getVertices()[i].y, sx, sy);
                            float dx = sx - mx;
                            float dy = sy - my;
                            float dist = std::sqrt(dx*dx + dy*dy);
                            if (dist < bestDist) {
                                bestDist = dist;
                                bestIdx = i;
                            }
                        }
                        if (bestIdx != -1) {
                            if (pendingLineStart == -1) {
                                pendingLineStart = bestIdx;
                                selectedVertex = bestIdx;
                            } else {
                                // Создаём линию
                                Linedef line;
                                line.startVertex = pendingLineStart;
                                line.endVertex = bestIdx;
                                line.flags = 1;
                                line.type = 0;
                                line.sectorTag = 0;
                                line.rightSidedef = 0;
                                line.leftSidedef = 0xFFFF;
                                addLinedef(map, line);
                                pendingLineStart = -1;
                                selectedVertex = -1;
                            }
                        }
                    }
                }
            }
        }

        // Отрисовка
        SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
        SDL_RenderClear(renderer);

        // Панель инструментов
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        SDL_FRect panel = {0, 0, (float)viewOffsetX-5, 720};
        SDL_RenderFillRect(renderer, &panel);

        // Кнопки
        for (auto& btn : buttons) {
            Button_Render(renderer, &btn, font);
        }

        // Карта
        const auto& verts = map.getVertices();
        const auto& lines = map.getLinedefs();

        // Линии
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        for (const auto& line : lines) {
            if (line.startVertex >= verts.size() || line.endVertex >= verts.size()) continue;
            int x1,y1,x2,y2;
            worldToScreen(verts[line.startVertex].x, verts[line.startVertex].y, x1, y1);
            worldToScreen(verts[line.endVertex].x, verts[line.endVertex].y, x2, y2);
            SDL_RenderLine(renderer, x1, y1, x2, y2);
        }

        // Вершины
        for (size_t i = 0; i < verts.size(); ++i) {
            int sx, sy;
            worldToScreen(verts[i].x, verts[i].y, sx, sy);
            SDL_Color col;
            if (i == selectedVertex) col = {255,255,0,255};
            else if (i == pendingLineStart) col = {0,255,0,255};
            else col = {255,0,0,255};
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
            SDL_FRect r = {(float)sx-3, (float)sy-3, 6,6};
            SDL_RenderFillRect(renderer, &r);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    // Сохраняем карту при выходе
    // map.saveToTextFile("lastmap.txt");

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}