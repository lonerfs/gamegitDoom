#pragma once

#include <vector>
#include <future>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>
#include <queue>
#include "game/DoomMap.h"
#include "game/Player.h"

struct HitResult {
    bool hit;
    float distance;
    int linedefIndex;
    float hitX, hitY;
    float hitU;
};

struct ColumnResult {
    int x;
    float distance;
    float hitU;
    float hitX, hitY;
    int linedefIndex;
    int wallTop, wallBottom;
    int sector;
};

class ThreadPool {
public:
    ThreadPool(size_t threads);
    ~ThreadPool();
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::invoke_result_t<F, Args...>> {
        using return_type = typename std::invoke_result_t<F, Args...>;
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        std::future<return_type> res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            tasks.emplace([task]() { (*task)(); });
        }
        condition.notify_one();
        return res;
    }
private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};

HitResult rayLineIntersection(float x0, float y0, float dx, float dy,
                              float x1, float y1, float x2, float y2);

void initTrigTables();

void renderColumnsRangeDirect(
    int startX, int endX,
    std::vector<ColumnResult>& outColumns,
    const Player& player,
    const DoomMap& map,
    int screenWidth, int screenHeight
);