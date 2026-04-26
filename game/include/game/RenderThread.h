#pragma once

#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>
#include <atomic>
#include <memory>
#include "game/DoomMap.h"
#include "game/Player.h"

struct HitResult {
    bool hit;
    float distance;
    int linedefIndex;
    float hitX, hitY;
    float hitU;
};

HitResult rayLineIntersection(float x0, float y0, float dx, float dy,
                              float x1, float y1, float x2, float y2);

struct ColumnResult {
    int x;
    int wallTop;
    int wallBottom;
    float hitU;
    float distance;
};


class ThreadPool {
public:
    ThreadPool(size_t threads);
    ~ThreadPool();

    template<typename F>
    void enqueue(F&& f);

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
};


std::vector<ColumnResult> renderColumnsRange(
    int startX, int endX,
    const Player& player,
    const std::vector<Linedef>& lines,
    const std::vector<Vertex>& vertices,
    int windowWidth, int windowHeight
);