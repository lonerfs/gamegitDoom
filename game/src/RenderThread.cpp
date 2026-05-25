#include "game/RenderThread.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <iostream>
#include <cstring>

static const int ANGLE_RES = 3600;
static float COS_TABLE[ANGLE_RES];
static float SIN_TABLE[ANGLE_RES];
static bool trigTablesInitialized = false;

void initTrigTables() {
    if (trigTablesInitialized) return;
    for (int i = 0; i < ANGLE_RES; ++i) {
        float rad = i * 0.1f * M_PI / 180.0f;
        COS_TABLE[i] = cosf(rad);
        SIN_TABLE[i] = sinf(rad);
    }
    trigTablesInitialized = true;
    std::cout << "Trig tables initialized (" << ANGLE_RES << " entries)" << std::endl;
}

static inline void fastSinCos(float rad, float& s, float& c) {
    int idx = int(rad * (180.0f / M_PI) * 10.0f) % ANGLE_RES;
    if (idx < 0) idx += ANGLE_RES;
    c = COS_TABLE[idx];
    s = SIN_TABLE[idx];
}

HitResult rayLineIntersection(float x0, float y0, float dx, float dy,
                              float x1, float y1, float x2, float y2) {
    HitResult res = {false, 0, -1, 0, 0, 0};
    float vx = x2 - x1;
    float vy = y2 - y1;
    float wx = x0 - x1;
    float wy = y0 - y1;
    float det = dx * vy - dy * vx;
    if (det == 0.0f) return res;
    float invDet = 1.0f / det;
    float t = (vx * wy - vy * wx) * invDet;
    if (t <= 0.0f) return res;
    float u = (dx * wy - dy * wx) * invDet;
    if (u < 0.0f || u > 1.0f) return res;
    res.hit = true;
    res.distance = t;
    res.hitX = x0 + t * dx;
    res.hitY = y0 + t * dy;
    res.hitU = u;
    return res;
}

// Полная версия: проверяем ВСЕ линии, без отсечения
void renderColumnsRangeDirect(
    int startX, int endX,
    std::vector<ColumnResult>& outColumns,
    const Player& player,
    const DoomMap& map,
    int screenWidth, int screenHeight
) {
    const float FOV = 60.0f * M_PI / 180.0f;
    const float HALF_FOV = FOV / 2.0f;

    const auto& lines = map.getLinedefs();
    const auto& vertices = map.getVertices();
    const auto& sidedefs = map.getSidedefs();
    const auto& sectors = map.getSectors();

    for (int x = startX; x < endX; ++x) {
        float screenAngle = ((float)x / screenWidth) * FOV - HALF_FOV;
        float rayAngle = player.angle + screenAngle;
        float dx, dy;
        fastSinCos(rayAngle, dy, dx);

        float closestDist = 1e6f;
        int hitLinedef = -1;
        float hitU = 0;
        float hitX = 0, hitY = 0;

        // Перебираем ВСЕ линии карты
        for (size_t li = 0; li < lines.size(); ++li) {
            const Linedef& ld = lines[li];
            if (ld.startVertex >= vertices.size() || ld.endVertex >= vertices.size()) continue;
            const Vertex& v1 = vertices[ld.startVertex];
            const Vertex& v2 = vertices[ld.endVertex];
            HitResult hit = rayLineIntersection(player.x, player.y, dx, dy, v1.x, v1.y, v2.x, v2.y);
            if (hit.hit && hit.distance < closestDist && hit.distance > 0.01f) {
                closestDist = hit.distance;
                hitLinedef = li;
                hitU = hit.hitU;
                hitX = hit.hitX;
                hitY = hit.hitY;
            }
        }

        ColumnResult col;
        col.x = x;
        col.distance = closestDist;
        col.hitU = hitU;
        col.hitX = hitX;
        col.hitY = hitY;
        col.linedefIndex = hitLinedef;

        if (hitLinedef != -1 && closestDist < 1e6f) {
            float distForHeight = closestDist;
            if (distForHeight < 0.1f) distForHeight = 0.1f;
            float wallHeight = 128.0f;
            if (hitLinedef >= 0 && hitLinedef < (int)lines.size()) {
                int sidedefIdx = lines[hitLinedef].rightSidedef;
                if (sidedefIdx >= 0 && sidedefIdx < (int)sidedefs.size()) {
                    int sectorNum = sidedefs[sidedefIdx].sector;
                    if (sectorNum >= 0 && sectorNum < (int)sectors.size()) {
                        wallHeight = sectors[sectorNum].ceilingHeight - sectors[sectorNum].floorHeight;
                        if (wallHeight < 16) wallHeight = 16;
                        col.sector = sectorNum;
                    }
                }
            }
            float screenWallHeight = (wallHeight / distForHeight) * (screenHeight / 1.5f);
            if (screenWallHeight > screenHeight) screenWallHeight = screenHeight;
            col.wallTop = (screenHeight - screenWallHeight) / 2;
            col.wallBottom = col.wallTop + screenWallHeight;
            if (col.wallTop < 0) col.wallTop = 0;
            if (col.wallBottom > screenHeight) col.wallBottom = screenHeight;
        } else {
            col.wallTop = screenHeight;
            col.wallBottom = screenHeight;
        }
        outColumns[x] = col;
    }
}

ThreadPool::ThreadPool(size_t threads) : stop(false) {
    for (size_t i = 0; i < threads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queueMutex);
                    condition.wait(lock, [this] { return stop || !tasks.empty(); });
                    if (stop && tasks.empty()) return;
                    task = std::move(tasks.front());
                    tasks.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();
    for (auto& worker : workers) worker.join();
}