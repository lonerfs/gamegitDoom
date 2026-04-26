#include "game/RenderThread.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <future>
#include <iostream>
#include <memory>

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
        result.distance = t * std::sqrt(dx*dx + dy*dy);
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

template<typename F>
void ThreadPool::enqueue(F&& f) {
    auto taskPtr = std::make_shared<std::packaged_task<std::vector<ColumnResult>()>>(std::forward<F>(f));
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        tasks.emplace([taskPtr]() { (*taskPtr)(); });
    }
    condition.notify_one();
}

template void ThreadPool::enqueue<std::packaged_task<std::vector<ColumnResult>()>>(std::packaged_task<std::vector<ColumnResult>()>&&);

std::vector<ColumnResult> renderColumnsRange(
    int startX, int endX,
    const Player& player,
    const std::vector<Linedef>& lines,
    const std::vector<Vertex>& vertices,
    int windowWidth, int windowHeight
) {
    const float FOV = M_PI / 2.0f;
    std::vector<ColumnResult> results;
    results.reserve(endX - startX);

    for (int x = startX; x < endX; ++x) {
        float rayAngle = player.angle + (x - windowWidth/2) * FOV / windowWidth;
        float dx = cos(rayAngle);
        float dy = sin(rayAngle);

        float closestDist = std::numeric_limits<float>::max();
        int hitLinedef = -1;
        float hitU = 0;

        for (size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].startVertex >= vertices.size() || lines[i].endVertex >= vertices.size()) continue;
            const Vertex& v1 = vertices[lines[i].startVertex];
            const Vertex& v2 = vertices[lines[i].endVertex];
            HitResult hit = rayLineIntersection(player.x, player.y, dx, dy, v1.x, v1.y, v2.x, v2.y);
            if (hit.hit && hit.distance < closestDist && hit.distance > 0.1f) {
                closestDist = hit.distance;
                hitLinedef = i;
                hitU = hit.hitU;
            }
        }

        ColumnResult col;
        col.x = x;
        if (hitLinedef != -1 && closestDist > 0.1f) {
            float correctedDist = closestDist * cos(rayAngle - player.angle);
            if (correctedDist < 0.1f) correctedDist = 0.1f;
            float wallHeight = (128.0f / correctedDist) * 350.0f;
            if (wallHeight > windowHeight) wallHeight = windowHeight;
            col.wallTop = (windowHeight - wallHeight) / 2;
            col.wallBottom = col.wallTop + static_cast<int>(wallHeight);
            if (col.wallTop < 0) col.wallTop = 0;
            if (col.wallBottom > windowHeight) col.wallBottom = windowHeight;
            col.hitU = hitU;
        } else {
            col.wallTop = windowHeight;
            col.wallBottom = windowHeight;
            col.hitU = 0;
        }
        results.push_back(col);
    }
    return results;
}