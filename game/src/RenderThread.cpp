#include "game/RenderThread.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <iostream>
#include <cstring>

HitResult rayLineIntersection(float x0, float y0, float dx, float dy,
                              float x1, float y1, float x2, float y2) {
    HitResult result = {false, 0, -1, 0, 0, 0};
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
        result.distance = t;
        result.linedefIndex = -1;
        result.hitX = x0 + t * dx;
        result.hitY = y0 + t * dy;
        result.hitU = u;
    }
    return result;
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

// Оптимизированная версия для GPU - расчеты на CPU, отрисовка на GPU
std::vector<ColumnResult> renderColumnsRange(
    int startX, int endX,
    const Player& player,
    const std::vector<Linedef>& lines,
    const std::vector<Vertex>& vertices,
    const std::vector<Sidedef>& sidedefs,
    const std::vector<Sector>& sectors,
    int screenWidth, int screenHeight
) {
    const float FOV = 60.0f * M_PI / 180.0f;
    const float HALF_FOV = FOV / 2.0f;
    std::vector<ColumnResult> results;
    results.reserve(endX - startX);

    for (int x = startX; x < endX; ++x) {
        float screenAngle = ((float)x / screenWidth) * FOV - HALF_FOV;
        float rayAngle = player.angle + screenAngle;
        float dx = cos(rayAngle);
        float dy = sin(rayAngle);

        float closestDist = 1000000.0f;
        int hitLinedef = -1;
        float hitU = 0;
        float hitX = 0, hitY = 0;

        // Поиск ближайшей стены (CPU-intensive, но многопоточно)
        for (size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].startVertex >= vertices.size() || lines[i].endVertex >= vertices.size()) continue;
            const Vertex& v1 = vertices[lines[i].startVertex];
            const Vertex& v2 = vertices[lines[i].endVertex];

            HitResult hit = rayLineIntersection(player.x, player.y, dx, dy, v1.x, v1.y, v2.x, v2.y);
            if (hit.hit && hit.distance < closestDist && hit.distance > 0.01f) {
                closestDist = hit.distance;
                hitLinedef = i;
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

        if (hitLinedef != -1 && closestDist < 1000000.0f) {
            // Без коррекции рыбьего глаза
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
        results.push_back(col);
    }
    return results;
}