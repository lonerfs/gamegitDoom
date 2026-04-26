#pragma once

#include <cmath>
#include <vector>
#include <algorithm>
#include <iostream>
#include "game/DoomMap.h"
#include "game/Player.h"

enum MobType {
    ZOMBIE,
    IMP,
    DEMON
};

struct Mob {
    float x, y;
    MobType type;
    int hp;
    int maxHp;
    float attackCooldown;
    float speed;
    int size;             

    Mob(float _x, float _y, MobType _type) : x(_x), y(_y), type(_type), attackCooldown(0.0f) {
        switch(type) {
            case ZOMBIE:
                hp = 40; maxHp = 40;
                speed = 50.0f;
                size = 64;
                break;
            case IMP:
                hp = 60; maxHp = 60;
                speed = 70.0f;
                size = 80;
                break;
            case DEMON:
                hp = 100; maxHp = 100;
                speed = 100.0f;
                size = 96;
                break;
        }
    }

    void update(float dt, const Player& player,
                const std::vector<Linedef>& lines,
                const std::vector<Vertex>& vertices) {
        float dx = player.x - x;
        float dy = player.y - y;
        float len = std::sqrt(dx*dx + dy*dy);
        if (len > 0.01f) {
            dx /= len;
            dy /= len;
        }
        float move = speed * dt;
        float newX = x + dx * move;
        float newY = y + dy * move;

        float halfSize = size / 2.0f;
        if (!collidesWithWall(newX, newY, halfSize, lines, vertices)) {
            x = newX;
            y = newY;
        } else {
            if (!collidesWithWall(newX, y, halfSize, lines, vertices)) x = newX;
            if (!collidesWithWall(x, newY, halfSize, lines, vertices)) y = newY;
        }

        if (attackCooldown > 0.0f) attackCooldown -= dt;
    }

    void tryAttack(Player& player, float dt, int& playerHealth) {
        float dx = player.x - x;
        float dy = player.y - y;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist < 80.0f && attackCooldown <= 0.0f) { // увеличен радиус атаки
            int damage = 0;
            switch(type) {
                case ZOMBIE: damage = 5; break;
                case IMP:    damage = 10; break;
                case DEMON:  damage = 20; break;
            }
            playerHealth -= damage;
            attackCooldown = 1.0f;
            if (playerHealth < 0) playerHealth = 0;
            std::cout << "Player hit by " << (type==ZOMBIE?"Zombie":(type==IMP?"Imp":"Demon"))
                      << "! HP: " << playerHealth << std::endl;
        }
    }

    void takeDamage(int dmg) {
        hp -= dmg;
        if (hp < 0) hp = 0;
    }

    bool isAlive() const { return hp > 0; }

private:
    bool collidesWithWall(float cx, float cy, float r,
                          const std::vector<Linedef>& lines,
                          const std::vector<Vertex>& vertices) const {
        for (const auto& line : lines) {
            if (line.startVertex >= vertices.size() || line.endVertex >= vertices.size()) continue;
            const Vertex& v1 = vertices[line.startVertex];
            const Vertex& v2 = vertices[line.endVertex];
            if (lineIntersectsCircle(cx, cy, r, v1.x, v1.y, v2.x, v2.y))
                return true;
        }
        return false;
    }

    bool lineIntersectsCircle(float cx, float cy, float r,
                              float x1, float y1, float x2, float y2) const {
        float dx1 = cx - x1, dy1 = cy - y1;
        float dx2 = cx - x2, dy2 = cy - y2;
        if (dx1*dx1 + dy1*dy1 < r*r) return true;
        if (dx2*dx2 + dy2*dy2 < r*r) return true;

        float lineDx = x2 - x1;
        float lineDy = y2 - y1;
        float len2 = lineDx*lineDx + lineDy*lineDy;
        if (len2 == 0.0f) return false;
        float t = ((cx - x1)*lineDx + (cy - y1)*lineDy) / len2;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float projX = x1 + t * lineDx;
        float projY = y1 + t * lineDy;
        float dist2 = (cx - projX)*(cx - projX) + (cy - projY)*(cy - projY);
        return dist2 < r*r;
    }
};