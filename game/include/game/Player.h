#pragma once

#include <cmath>
#include <vector>
#include <algorithm>
#include "game/DoomMap.h"

class Player {
public:
    float x, y;
    float angle;
    float speed;

    Player() {
        x = 1280.0f;
        y = 1280.0f;
        angle = 0.0f;
        speed = 5.0f;
    }

    void turnLeft(float delta) {
        angle -= delta;
    }

    void turnRight(float delta) {
        angle += delta;
    }

    void moveWithSliding(float dx, float dy, const std::vector<Linedef>& lines, const std::vector<Vertex>& vertices) {
        const float PLAYER_RADIUS = 16.0f;

        float newX = x + dx;
        float newY = y + dy;

        if (!collidesWithWall(newX, newY, PLAYER_RADIUS, lines, vertices)) {
            x = newX;
            y = newY;
            return;
        }
        newX = x + dx;
        newY = y;
        if (!collidesWithWall(newX, newY, PLAYER_RADIUS, lines, vertices)) {
            x = newX;
            newY = y + dy;
            if (!collidesWithWall(x, newY, PLAYER_RADIUS, lines, vertices)) {
                y = newY;
            }
            return;
        }

        newX = x;
        newY = y + dy;
        if (!collidesWithWall(newX, newY, PLAYER_RADIUS, lines, vertices)) {
            y = newY;
            newX = x + dx;
            if (!collidesWithWall(newX, y, PLAYER_RADIUS, lines, vertices)) {
                x = newX;
            }
            return;
        }
    }

private:
    bool collidesWithWall(float cx, float cy, float r, const std::vector<Linedef>& lines, const std::vector<Vertex>& vertices) const {
        for (const auto& line : lines) {
            const Vertex& v1 = vertices[line.startVertex];
            const Vertex& v2 = vertices[line.endVertex];
            if (lineIntersectsCircle(cx, cy, r, v1.x, v1.y, v2.x, v2.y)) {
                return true;
            }
        }
        return false;
    }

    bool lineIntersectsCircle(float cx, float cy, float r, float x1, float y1, float x2, float y2) const {
        // Проверяем концы отрезка
        float dx1 = cx - x1;
        float dy1 = cy - y1;
        float dx2 = cx - x2;
        float dy2 = cy - y2;
        if (dx1*dx1 + dy1*dy1 < r*r || dx2*dx2 + dy2*dy2 < r*r) {
            return true;
        }

        float lineDx = x2 - x1;
        float lineDy = y2 - y1;
        float len = lineDx*lineDx + lineDy*lineDy;
        if (len == 0) return false;

        float t = ((cx - x1)*lineDx + (cy - y1)*lineDy) / len;
        if (t < 0) t = 0;
        if (t > 1) t = 1;

        float projX = x1 + t * lineDx;
        float projY = y1 + t * lineDy;

        float dist2 = (cx - projX)*(cx - projX) + (cy - projY)*(cy - projY);
        return dist2 < r*r;
    }
};