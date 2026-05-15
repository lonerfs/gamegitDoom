#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include "game/DoomMap.h"
#include "game/WadLoader.h"
#include "MapToWad.h"

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
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_RenderFillRect(renderer, &btn->rect);
    if (btn->pressed) {
        SDL_SetRenderDrawColor(renderer, 80, 80, 80, 255);
        SDL_RenderRect(renderer, &btn->rect);
    }
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
        SDL_DestroySurface(surf);
    }
}

enum EditMode { MODE_VERTEX, MODE_LINE, MODE_THING };
EditMode currentMode = MODE_VERTEX;
DoomMap map;
int selectedVertex = -1;
int selectedThing = -1;
int pendingLineStart = -1;

float viewScale = 1.0f;
int viewOffsetX = 400;
int viewOffsetY = 360;
int gridSize = 64;

std::string currentFileName = "level1.txt";

struct ThingType {
    const char* name;
    int doomEdNum;
};

ThingType thingTypes[] = {
    {"Stimpack", 2011},
    {"Medikit", 2012},
    {"SoulSphere", 2018},
    {"GreenArmor", 2022},
    {"BlueArmor", 2019},
    {"Shotgun", 2001},
    {"SuperShotgun", 82},
    {"Chaingun", 2002},
    {"RocketLauncher", 2003},
    {"PlasmaRifle", 2004},
    {"BFG9000", 2006},
    {"Clip", 2007},
    {"Shells", 2008},
    {"Rocket", 2010},
    {"Cell", 2047},
    {"ExplosiveBarrel", 31},
    {"Zombieman", 3004},
    {"ShotgunGuy", 9},
    {"Imp", 3001},
    {"Demon", 3002},
    {"Cacodemon", 3005}
};

void worldToScreen(int wx, int wy, int& sx, int& sy) {
    sx = (int)(wx * viewScale + viewOffsetX);
    sy = (int)(-wy * viewScale + viewOffsetY);
}

void screenToWorld(int sx, int sy, int& wx, int& wy) {
    wx = (int)round((sx - viewOffsetX) / viewScale);
    wy = (int)round(-(sy - viewOffsetY) / viewScale);
}

void centerViewOnMap() {
    const auto& verts = map.getVertices();
    if (verts.empty()) return;

    int minX = verts[0].x, maxX = verts[0].x;
    int minY = verts[0].y, maxY = verts[0].y;
    for (const auto& v : verts) {
        minX = std::min(minX, (int)v.x);
        maxX = std::max(maxX, (int)v.x);
        minY = std::min(minY, (int)v.y);
        maxY = std::max(maxY, (int)v.y);
    }

    int centerX = (minX + maxX) / 2;
    int centerY = (minY + maxY) / 2;

    viewOffsetX = 400 - centerX;
    viewOffsetY = 360 + centerY;
    viewScale = 1.0f;
}

void drawGrid(SDL_Renderer* renderer, int screenWidth, int screenHeight) {
    int worldTopLeftX, worldTopLeftY;
    int worldTopRightX, worldTopRightY;
    int worldBottomLeftX, worldBottomLeftY;
    int worldBottomRightX, worldBottomRightY;

    screenToWorld(0, 0, worldTopLeftX, worldTopLeftY);
    screenToWorld(screenWidth, 0, worldTopRightX, worldTopRightY);
    screenToWorld(0, screenHeight, worldBottomLeftX, worldBottomLeftY);
    screenToWorld(screenWidth, screenHeight, worldBottomRightX, worldBottomRightY);

    int minWorldX = std::min({worldTopLeftX, worldTopRightX, worldBottomLeftX, worldBottomRightX});
    int maxWorldX = std::max({worldTopLeftX, worldTopRightX, worldBottomLeftX, worldBottomRightX});
    int minWorldY = std::min({worldTopLeftY, worldTopRightY, worldBottomLeftY, worldBottomRightY});
    int maxWorldY = std::max({worldTopLeftY, worldTopRightY, worldBottomLeftY, worldBottomRightY});

    int startGridX = ((minWorldX / gridSize) - 1) * gridSize;
    int endGridX = ((maxWorldX / gridSize) + 2) * gridSize;
    int startGridY = ((minWorldY / gridSize) - 1) * gridSize;
    int endGridY = ((maxWorldY / gridSize) + 2) * gridSize;

    SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);

    for (int x = startGridX; x <= endGridX; x += gridSize) {
        int screenX1, screenY1, screenX2, screenY2;
        worldToScreen(x, startGridY, screenX1, screenY1);
        worldToScreen(x, endGridY, screenX2, screenY2);
        SDL_RenderLine(renderer, screenX1, screenY1, screenX2, screenY2);
    }

    for (int y = startGridY; y <= endGridY; y += gridSize) {
        int screenX1, screenY1, screenX2, screenY2;
        worldToScreen(startGridX, y, screenX1, screenY1);
        worldToScreen(endGridX, y, screenX2, screenY2);
        SDL_RenderLine(renderer, screenX1, screenY1, screenX2, screenY2);
    }
}

void addVertex(DoomMap& m, int16_t x, int16_t y) {
    const_cast<std::vector<Vertex>&>(m.getVertices()).push_back({x, y});
}

void addLinedef(DoomMap& m, const Linedef& line) {
    const_cast<std::vector<Linedef>&>(m.getLinedefs()).push_back(line);
}

void addThing(DoomMap& m, int16_t x, int16_t y, int16_t type) {
    Thing t;
    t.x = x;
    t.y = y;
    t.angle = 0;
    t.type = type;
    t.flags = 7;
    const_cast<std::vector<Thing>&>(m.getThings()).push_back(t);
}

void deleteVertex(DoomMap& m, int index) {
    if (index < 0 || index >= (int)m.getVertices().size()) return;

    auto& vertices = const_cast<std::vector<Vertex>&>(m.getVertices());
    auto& linedefs = const_cast<std::vector<Linedef>&>(m.getLinedefs());

    for (int i = (int)linedefs.size() - 1; i >= 0; i--) {
        if (linedefs[i].startVertex == index || linedefs[i].endVertex == index) {
            linedefs.erase(linedefs.begin() + i);
        } else {
            if (linedefs[i].startVertex > index) linedefs[i].startVertex--;
            if (linedefs[i].endVertex > index) linedefs[i].endVertex--;
        }
    }

    vertices.erase(vertices.begin() + index);
}

void deleteThing(DoomMap& m, int index) {
    if (index < 0 || index >= (int)m.getThings().size()) return;
    auto& things = const_cast<std::vector<Thing>&>(m.getThings());
    things.erase(things.begin() + index);
}

bool saveToTextFile(DoomMap& m, const std::string& filename) {
    std::ofstream file(filename);
    if (!file) {
        std::cerr << "Cannot save to file: " << filename << std::endl;
        return false;
    }

    const auto& vertices = m.getVertices();
    const auto& linedefs = m.getLinedefs();
    const auto& things = m.getThings();

    file << "[VERTICES]\n";
    for (const auto& v : vertices) {
        file << v.x << " " << v.y << "\n";
    }

    file << "\n[LINEDEFS]\n";
    for (const auto& l : linedefs) {
        file << l.startVertex << " " << l.endVertex << " 4 0 0 0 0 65535\n";
    }

    file << "\n[THINGS]\n";
    for (const auto& t : things) {
        file << t.x << " " << t.y << " 0 " << t.type << " " << t.flags << "\n";
    }

    file.close();
    std::cout << "Saved to " << filename << std::endl;
    return true;
}

bool setModeVertex(void*) {
    currentMode = MODE_VERTEX;
    pendingLineStart = -1;
    selectedVertex = -1;
    selectedThing = -1;
    return true;
}

bool setModeLine(void*) {
    currentMode = MODE_LINE;
    pendingLineStart = -1;
    selectedVertex = -1;
    selectedThing = -1;
    return true;
}

bool setModeThing(void*) {
    currentMode = MODE_THING;
    pendingLineStart = -1;
    selectedVertex = -1;
    selectedThing = -1;
    return true;
}

bool deleteSelected(void*) {
    if (selectedVertex != -1) {
        deleteVertex(map, selectedVertex);
        selectedVertex = -1;
        pendingLineStart = -1;
        std::cout << "Vertex deleted" << std::endl;
    }
    if (selectedThing != -1) {
        deleteThing(map, selectedThing);
        selectedThing = -1;
        std::cout << "Thing deleted" << std::endl;
    }
    return true;
}

bool saveCurrentMap(void*) {
    saveToTextFile(map, currentFileName);
    std::cout << "Saved to " << currentFileName << std::endl;
    return true;
}

bool exportToWad(void*) {
    std::string mapName;
    if (currentFileName == "level1.txt") mapName = "MAP01";
    else if (currentFileName == "level2.txt") mapName = "MAP02";
    else if (currentFileName == "level3.txt") mapName = "MAP03";
    else mapName = "MAP01";

    MapToWad::addAllTexturesToWad("mymaps.wad", "textures");

    if (MapToWad::addMapToWad("mymaps.wad", currentFileName, mapName)) {
        std::cout << "Exported " << currentFileName << " with textures to mymaps.wad as " << mapName << std::endl;
    } else {
        std::cerr << "Failed to export to WAD" << std::endl;
    }
    return true;
}

bool loadLevel1(void*) {
    currentFileName = "level1.txt";
    map.loadFromTextFile(currentFileName);
    selectedVertex = -1;
    selectedThing = -1;
    pendingLineStart = -1;
    centerViewOnMap();
    std::cout << "Loaded " << currentFileName << std::endl;
    return true;
}

bool loadLevel2(void*) {
    currentFileName = "level2.txt";
    map.loadFromTextFile(currentFileName);
    selectedVertex = -1;
    selectedThing = -1;
    pendingLineStart = -1;
    centerViewOnMap();
    std::cout << "Loaded " << currentFileName << std::endl;
    return true;
}

bool loadLevel3(void*) {
    currentFileName = "level3.txt";
    map.loadFromTextFile(currentFileName);
    selectedVertex = -1;
    selectedThing = -1;
    pendingLineStart = -1;
    centerViewOnMap();
    std::cout << "Loaded " << currentFileName << std::endl;
    return true;
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    // Получаем размер экрана для полноэкранного режима
    SDL_DisplayID display = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display);
    int screenWidth = mode->w;
    int screenHeight = mode->h;

    // Создаём окно на весь экран
    SDL_Window* window = SDL_CreateWindow("Doom Map Editor", screenWidth, screenHeight, SDL_WINDOW_FULLSCREEN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    TTF_Font* font = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 14);
    TTF_Font* smallFont = TTF_OpenFont("C:/Windows/Fonts/arial.ttf", 11);

    currentFileName = "level1.txt";
    if (!map.loadFromTextFile(currentFileName)) {
        std::cerr << "Failed to load " << currentFileName << ", starting with empty map" << std::endl;
    }
    centerViewOnMap();

    std::vector<Button> buttons;
    buttons.push_back({{10,10,80,30}, "Vertex", false, setModeVertex, nullptr});
    buttons.push_back({{100,10,80,30}, "Line", false, setModeLine, nullptr});
    buttons.push_back({{190,10,80,30}, "Thing", false, setModeThing, nullptr});
    buttons.push_back({{280,10,80,30}, "Delete", false, deleteSelected, nullptr});
    buttons.push_back({{370,10,80,30}, "Save", false, saveCurrentMap, nullptr});
    buttons.push_back({{460,10,100,30}, "To WAD", false, exportToWad, nullptr});

    buttons.push_back({{570,10,80,30}, "Level 1", false, loadLevel1, nullptr});
    buttons.push_back({{660,10,80,30}, "Level 2", false, loadLevel2, nullptr});
    buttons.push_back({{750,10,80,30}, "Level 3", false, loadLevel3, nullptr});

    int currentThingType = 0;
    bool running = true;
    bool fullscreen = true;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) running = false;

            // Переключение полноэкранного режима по F11
            if (ev.type == SDL_EVENT_KEY_DOWN && ev.key.scancode == SDL_SCANCODE_F11) {
                fullscreen = !fullscreen;
                if (fullscreen) {
                    SDL_SetWindowFullscreen(window, true);
                } else {
                    SDL_SetWindowFullscreen(window, false);
                    SDL_SetWindowSize(window, 1280, 720);
                    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
                }
            }

            if (ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.scancode == SDL_SCANCODE_MINUS || ev.key.scancode == SDL_SCANCODE_KP_MINUS) {
                    gridSize = std::max(16, gridSize / 2);
                    std::cout << "Grid size: " << gridSize << std::endl;
                }
                if (ev.key.scancode == SDL_SCANCODE_EQUALS || ev.key.scancode == SDL_SCANCODE_KP_PLUS) {
                    gridSize = std::min(256, gridSize * 2);
                    std::cout << "Grid size: " << gridSize << std::endl;
                }
                if (ev.key.scancode == SDL_SCANCODE_TAB && currentMode == MODE_THING) {
                    currentThingType = (currentThingType + 1) % (sizeof(thingTypes)/sizeof(thingTypes[0]));
                    std::cout << "Thing type: " << thingTypes[currentThingType].name << " (DoomEdNum: " << thingTypes[currentThingType].doomEdNum << ")" << std::endl;
                }
                if (ev.key.scancode == SDL_SCANCODE_DELETE) {
                    deleteSelected(nullptr);
                }
            }

            if (ev.type == SDL_EVENT_MOUSE_WHEEL) {
                float mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);

                int worldX, worldY;
                screenToWorld((int)mouseX, (int)mouseY, worldX, worldY);

                if (ev.wheel.y > 0) {
                    viewScale *= 1.1f;
                    if (viewScale > 4.0f) viewScale = 4.0f;
                } else if (ev.wheel.y < 0) {
                    viewScale /= 1.1f;
                    if (viewScale < 0.25f) viewScale = 0.25f;
                }

                int newScreenX, newScreenY;
                worldToScreen(worldX, worldY, newScreenX, newScreenY);
                viewOffsetX += (int)(mouseX - newScreenX);
                viewOffsetY += (int)(mouseY - newScreenY);
            }

            for (auto& btn : buttons) {
                Button_HandleEvent(&btn, &ev);
            }

            if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == 1) {
                int mx = ev.button.x;
                int my = ev.button.y;

                bool clickedOnButton = false;
                for (auto& btn : buttons) {
                    SDL_FPoint point = {(float)mx, (float)my};
                    if (SDL_PointInRectFloat(&point, &btn.rect)) {
                        clickedOnButton = true;
                        break;
                    }
                }

                if (clickedOnButton) {
                    continue;
                }

                int clickedVertex = -1;
                float closestDist = 15.0f;
                const auto& verts = map.getVertices();
                for (size_t i = 0; i < verts.size(); ++i) {
                    int sx, sy;
                    worldToScreen(verts[i].x, verts[i].y, sx, sy);
                    float dx = sx - mx;
                    float dy = sy - my;
                    float dist = std::sqrt(dx*dx + dy*dy);
                    if (dist < closestDist) {
                        closestDist = dist;
                        clickedVertex = i;
                    }
                }

                int clickedThing = -1;
                const auto& things = map.getThings();
                for (size_t i = 0; i < things.size(); ++i) {
                    int sx, sy;
                    worldToScreen(things[i].x, things[i].y, sx, sy);
                    float dx = sx - mx;
                    float dy = sy - my;
                    float dist = std::sqrt(dx*dx + dy*dy);
                    if (dist < closestDist) {
                        closestDist = dist;
                        clickedThing = i;
                        clickedVertex = -1;
                    }
                }

                if (clickedVertex != -1 && currentMode != MODE_THING) {
                    if (currentMode == MODE_LINE) {
                        if (pendingLineStart == -1) {
                            pendingLineStart = clickedVertex;
                            selectedVertex = clickedVertex;
                        } else if (pendingLineStart != clickedVertex) {
                            Linedef line;
                            line.startVertex = pendingLineStart;
                            line.endVertex = clickedVertex;
                            line.flags = 1;
                            line.type = 0;
                            line.sectorTag = 0;
                            line.rightSidedef = 0;
                            line.leftSidedef = 0xFFFF;
                            addLinedef(map, line);
                            pendingLineStart = -1;
                            selectedVertex = -1;
                        } else {
                            pendingLineStart = -1;
                            selectedVertex = -1;
                        }
                    } else if (currentMode == MODE_VERTEX) {
                        selectedVertex = clickedVertex;
                        selectedThing = -1;
                        pendingLineStart = -1;
                    }
                }
                else if (clickedThing != -1 && currentMode != MODE_THING) {
                    selectedThing = clickedThing;
                    selectedVertex = -1;
                    pendingLineStart = -1;
                }
                else if (currentMode == MODE_VERTEX) {
                    int wx, wy;
                    screenToWorld(mx, my, wx, wy);

                    int snappedX = ((wx + gridSize/2) / gridSize) * gridSize;
                    int snappedY = ((wy + gridSize/2) / gridSize) * gridSize;

                    bool exists = false;
                    for (const auto& v : map.getVertices()) {
                        if (v.x == snappedX && v.y == snappedY) {
                            exists = true;
                            break;
                        }
                    }

                    if (!exists) {
                        addVertex(map, (int16_t)snappedX, (int16_t)snappedY);
                        selectedVertex = map.getVertices().size() - 1;
                        std::cout << "Added vertex at (" << snappedX << ", " << snappedY << ")" << std::endl;
                    }
                }
                else if (currentMode == MODE_THING) {
                    int wx, wy;
                    screenToWorld(mx, my, wx, wy);

                    int snappedX = ((wx + gridSize/2) / gridSize) * gridSize;
                    int snappedY = ((wy + gridSize/2) / gridSize) * gridSize;

                    addThing(map, (int16_t)snappedX, (int16_t)snappedY, thingTypes[currentThingType].doomEdNum);
                    selectedThing = map.getThings().size() - 1;
                    std::cout << "Added " << thingTypes[currentThingType].name << " at (" << snappedX << ", " << snappedY << ")" << std::endl;
                }
            }

            if (ev.type == SDL_EVENT_MOUSE_MOTION && (ev.motion.state & SDL_BUTTON_LMASK)) {
                if (selectedVertex != -1 && currentMode == MODE_VERTEX) {
                    int mx = ev.motion.x;
                    int my = ev.motion.y;
                    int wx, wy;
                    screenToWorld(mx, my, wx, wy);

                    int snappedX = ((wx + gridSize/2) / gridSize) * gridSize;
                    int snappedY = ((wy + gridSize/2) / gridSize) * gridSize;

                    auto& verts = const_cast<std::vector<Vertex>&>(map.getVertices());
                    verts[selectedVertex].x = snappedX;
                    verts[selectedVertex].y = snappedY;
                }
                if (selectedThing != -1 && currentMode == MODE_THING) {
                    int mx = ev.motion.x;
                    int my = ev.motion.y;
                    int wx, wy;
                    screenToWorld(mx, my, wx, wy);

                    int snappedX = ((wx + gridSize/2) / gridSize) * gridSize;
                    int snappedY = ((wy + gridSize/2) / gridSize) * gridSize;

                    auto& things = const_cast<std::vector<Thing>&>(map.getThings());
                    things[selectedThing].x = snappedX;
                    things[selectedThing].y = snappedY;
                }
            }
        }

        // Получаем актуальные размеры окна (на случай смены режима)
        int winW, winH;
        SDL_GetWindowSize(window, &winW, &winH);

        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        SDL_RenderClear(renderer);

        drawGrid(renderer, winW, winH);

        for (auto& btn : buttons) {
            Button_Render(renderer, &btn, font);
        }

        SDL_SetRenderDrawColor(renderer, 80, 80, 100, 255);
        SDL_FRect modePanel = {10, 50, 550, 25};
        SDL_RenderFillRect(renderer, &modePanel);

        std::string modeText;
        if (currentMode == MODE_VERTEX) {
            modeText = "Mode: VERTEX - Click to add/move vertices | +/- to change grid | Delete to remove selected";
        } else if (currentMode == MODE_LINE) {
            modeText = "Mode: LINE - Click on two vertices to create a line";
        } else {
            modeText = "Mode: THING - Current: " + std::string(thingTypes[currentThingType].name) + " (Press TAB to change)";
        }

        std::string fileText = "Current: " + currentFileName + " | F11 - Fullscreen";

        SDL_Surface* fileSurf = TTF_RenderText_Solid(smallFont, fileText.c_str(), 0, SDL_Color{200,200,200,255});
        if (fileSurf) {
            SDL_Texture* fileTex = SDL_CreateTextureFromSurface(renderer, fileSurf);
            SDL_FRect fileDst = {15, 80, (float)fileSurf->w, (float)fileSurf->h};
            SDL_RenderTexture(renderer, fileTex, NULL, &fileDst);
            SDL_DestroyTexture(fileTex);
            SDL_DestroySurface(fileSurf);
        }

        SDL_Surface* modeSurf = TTF_RenderText_Solid(font, modeText.c_str(), 0, SDL_Color{255,255,255,255});
        if (modeSurf) {
            SDL_Texture* modeTex = SDL_CreateTextureFromSurface(renderer, modeSurf);
            SDL_FRect modeDst = {15, 53, (float)modeSurf->w, (float)modeSurf->h};
            SDL_RenderTexture(renderer, modeTex, NULL, &modeDst);
            SDL_DestroyTexture(modeTex);
            SDL_DestroySurface(modeSurf);
        }

        const auto& verts = map.getVertices();
        const auto& lines = map.getLinedefs();
        const auto& things = map.getThings();

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        for (const auto& line : lines) {
            if (line.startVertex >= verts.size() || line.endVertex >= verts.size()) continue;
            int x1,y1,x2,y2;
            worldToScreen(verts[line.startVertex].x, verts[line.startVertex].y, x1, y1);
            worldToScreen(verts[line.endVertex].x, verts[line.endVertex].y, x2, y2);
            SDL_RenderLine(renderer, x1, y1, x2, y2);
        }

        for (size_t i = 0; i < verts.size(); ++i) {
            int sx, sy;
            worldToScreen(verts[i].x, verts[i].y, sx, sy);

            if ((int)i == selectedVertex) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                SDL_FRect r = {(float)sx-6, (float)sy-6, 12, 12};
                SDL_RenderFillRect(renderer, &r);
            } else if ((int)i == pendingLineStart) {
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
                SDL_FRect r = {(float)sx-5, (float)sy-5, 10, 10};
                SDL_RenderFillRect(renderer, &r);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
                SDL_FRect r = {(float)sx-4, (float)sy-4, 8, 8};
                SDL_RenderFillRect(renderer, &r);
            }

            char coordText[32];
            snprintf(coordText, sizeof(coordText), "%d,%d", verts[i].x, verts[i].y);
            SDL_Surface* coordSurf = TTF_RenderText_Solid(smallFont, coordText, 0, SDL_Color{200, 200, 200, 255});
            if (coordSurf) {
                SDL_Texture* coordTex = SDL_CreateTextureFromSurface(renderer, coordSurf);
                SDL_FRect coordDst = {(float)sx + 8, (float)sy - 8, (float)coordSurf->w, (float)coordSurf->h};
                SDL_RenderTexture(renderer, coordTex, NULL, &coordDst);
                SDL_DestroyTexture(coordTex);
                SDL_DestroySurface(coordSurf);
            }
        }

        for (size_t i = 0; i < things.size(); ++i) {
            int sx, sy;
            worldToScreen(things[i].x, things[i].y, sx, sy);

            const char* typeName = "Unknown";
            for (const auto& tt : thingTypes) {
                if (tt.doomEdNum == things[i].type) {
                    typeName = tt.name;
                    break;
                }
            }

            if ((int)i == selectedThing) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 200);
                SDL_FRect r = {(float)sx-8, (float)sy-8, 16, 16};
                SDL_RenderFillRect(renderer, &r);
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                SDL_RenderRect(renderer, &r);
            } else {
                SDL_SetRenderDrawColor(renderer, 0, 255, 255, 200);
                SDL_FRect r = {(float)sx-5, (float)sy-5, 10, 10};
                SDL_RenderFillRect(renderer, &r);
            }

            char thingText[64];
            snprintf(thingText, sizeof(thingText), "%s", typeName);
            SDL_Surface* thingSurf = TTF_RenderText_Solid(smallFont, thingText, 0, SDL_Color{0,255,255,255});
            if (thingSurf) {
                SDL_Texture* thingTex = SDL_CreateTextureFromSurface(renderer, thingSurf);
                SDL_FRect thingDst = {(float)sx + 8, (float)sy - 12, (float)thingSurf->w, (float)thingSurf->h};
                SDL_RenderTexture(renderer, thingTex, NULL, &thingDst);
                SDL_DestroyTexture(thingTex);
                SDL_DestroySurface(thingSurf);
            }

            char coordText[32];
            snprintf(coordText, sizeof(coordText), "%d,%d", things[i].x, things[i].y);
            SDL_Surface* coordSurf = TTF_RenderText_Solid(smallFont, coordText, 0, SDL_Color{150,150,150,255});
            if (coordSurf) {
                SDL_Texture* coordTex = SDL_CreateTextureFromSurface(renderer, coordSurf);
                SDL_FRect coordDst = {(float)sx + 8, (float)sy + 4, (float)coordSurf->w, (float)coordSurf->h};
                SDL_RenderTexture(renderer, coordTex, NULL, &coordDst);
                SDL_DestroyTexture(coordTex);
                SDL_DestroySurface(coordSurf);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(10);
    }

    TTF_CloseFont(font);
    TTF_CloseFont(smallFont);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}