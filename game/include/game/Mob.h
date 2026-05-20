#pragma once

#include <cmath>
#include <vector>
#include <algorithm>
#include <iostream>
#include <optional>
#include "game/DoomMap.h"
#include "game/Player.h"

enum MobType {
    ZOMBIE,
    IMP,
    DEMON,
    BOSS
};

enum MobState {
    IDLE,
    CHASE,
    ATTACK
};

struct EnemyProjectile {
    float x, y;
    float vx, vy;
    float damage;
    float lifetime;
    int type;
};

struct Mob {
    float x, y;
    MobType type;
    MobState state;
    int hp;
    int maxHp;
    float attackCooldown;
    float fireCooldown;
    float speed;
    float size;
    bool awakened;
    float dodgeTimer;
    float dodgeAngle;

    float summonCooldown;
    bool isSummoning;

    // Добавлено для совместимости с main.cpp
    bool invulnerable;
    int summonedMobsCount;
    float vulnerabilityTimer;

    Mob(float _x, float _y, MobType _type) : x(_x), y(_y), type(_type), state(IDLE),
        attackCooldown(0.0f), fireCooldown(0.0f), awakened(false),
        dodgeTimer(0.0f), dodgeAngle(0.0f), summonCooldown(0.0f), isSummoning(false),
        invulnerable(false), summonedMobsCount(0), vulnerabilityTimer(0.0f)
    {
        switch(type) {
            case ZOMBIE:
                hp = 40; maxHp = 40;
                speed = 50.0f;
                size = 48;
                fireCooldown = 999.0f;
                break;
            case IMP:
                hp = 60; maxHp = 60;
                speed = 70.0f;
                size = 56;
                fireCooldown = 1.2f;
                break;
            case DEMON:
                hp = 100; maxHp = 100;
                speed = 100.0f;
                size = 64;
                fireCooldown = 0.9f;
                break;
            case BOSS:
                hp = 300; maxHp = 300;
                speed = 40.0f;
                size = 96;
                fireCooldown = 2.5f;
                summonCooldown = 10.0f;
                isSummoning = false;
                invulnerable = false;
                summonedMobsCount = 0;
                vulnerabilityTimer = 0.0f;
                break;
        }
    }

    bool hasLineOfSight(float tx, float ty, const std::vector<Linedef>& lines, const std::vector<Vertex>& vertices) const {
        float dx = tx - x;
        float dy = ty - y;

        for (const auto& line : lines) {
            if (line.startVertex >= vertices.size() || line.endVertex >= vertices.size()) continue;
            const Vertex& v1 = vertices[line.startVertex];
            const Vertex& v2 = vertices[line.endVertex];

            float v1x = v1.x - x; float v1y = v1.y - y;
            float v2x = v2.x - x; float v2y = v2.y - y;
            float cross1 = dx * v1y - dy * v1x;
            float cross2 = dx * v2y - dy * v2x;

            if (cross1 * cross2 < 0) {
                float ldx = v2.x - v1.x; float ldy = v2.y - v1.y;
                float cross3 = ldx * (-v1y) - ldy * (-v1x);
                float cross4 = ldx * (dy - v1y) - ldy * (dx - v1x);
                if (cross3 * cross4 < 0) return false;
            }
        }
        return true;
    }

    std::optional<EnemyProjectile> update(float dt, const Player& player, const std::vector<Linedef>& lines, const std::vector<Vertex>& vertices) {
        float dx = player.x - x;
        float dy = player.y - y;
        float dist = std::sqrt(dx*dx + dy*dy);

        if (!awakened) {
            if (dist < 600.0f && hasLineOfSight(player.x, player.y, lines, vertices)) {
                awakened = true;
                state = CHASE;
            }
        }

        if (dodgeTimer > 0.0f) {
            float dodgeSpeed = speed * 1.2f;
            float dxDodge = cos(dodgeAngle) * dodgeSpeed * dt;
            float dyDodge = sin(dodgeAngle) * dodgeSpeed * dt;
            float halfSize = size / 2.0f;
            float newX = x + dxDodge;
            float newY = y + dyDodge;
            if (!collidesWithWall(newX, newY, halfSize, lines, vertices)) {
                x = newX;
                y = newY;
            } else {
                if (!collidesWithWall(newX, y, halfSize, lines, vertices)) x = newX;
                if (!collidesWithWall(x, newY, halfSize, lines, vertices)) y = newY;
            }
            dodgeTimer -= dt;
            return std::nullopt;
        }

        float stopDist = 0.0f;
        switch(type) {
            case ZOMBIE: stopDist = 80.0f;  break;
            case IMP:    stopDist = 150.0f; break;
            case DEMON:  stopDist = 150.0f; break;
            case BOSS:   stopDist = 200.0f; break;
        }

        if (awakened && dist > stopDist && dist > 0.01f) {
            dx /= dist;
            dy /= dist;
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
        }

        if (attackCooldown > 0.0f) attackCooldown -= dt;
        if (fireCooldown > 0.0f) fireCooldown -= dt;

        if (type == BOSS && summonCooldown > 0.0f) {
            summonCooldown -= dt;
        }

        if (type == BOSS && vulnerabilityTimer > 0.0f) {
            vulnerabilityTimer -= dt;
            if (vulnerabilityTimer <= 0.0f) {
                invulnerable = false;
            }
        }

        if (awakened && type != ZOMBIE) {
            if (dist < 500.0f && hasLineOfSight(player.x, player.y, lines, vertices)) {
                if (fireCooldown <= 0.0f) {
                    if (type == BOSS) {
                        fireCooldown = 2.5f;
                        for (int i = -1; i <= 1; i++) {
                            EnemyProjectile proj;
                            float angleToPlayer = atan2(dy, dx);
                            float angleOffset = i * 0.25f;
                            float finalAngle = angleToPlayer + angleOffset;
                            float projSpeed = 150.0f;
                            proj.x = x;
                            proj.y = y;
                            proj.vx = cos(finalAngle) * projSpeed;
                            proj.vy = sin(finalAngle) * projSpeed;
                            proj.damage = 20.0f;
                            proj.lifetime = 3.0f;
                            proj.type = 2;
                            return proj;
                        }
                    } else {
                        fireCooldown = (type == IMP) ? 1.2f : 0.9f;
                        EnemyProjectile proj;
                        float dxN = dx / dist;
                        float dyN = dy / dist;
                        float projSpeed = (type == IMP) ? 120.0f : 180.0f;
                        proj.x = x;
                        proj.y = y;
                        proj.vx = dxN * projSpeed;
                        proj.vy = dyN * projSpeed;
                        proj.damage = (type == IMP) ? 8.0f : 15.0f;
                        proj.lifetime = 3.0f;
                        proj.type = (type == IMP) ? 0 : 1;
                        return proj;
                    }
                }
            }
        }
        return std::nullopt;
    }

    void tryAttack(Player& player, float dt, int& playerHealth) {
        float dx = player.x - x;
        float dy = player.y - y;
        float dist = std::sqrt(dx*dx + dy*dy);

        if (dist < 80.0f && attackCooldown <= 0.0f) {
            int damage = 0;
            switch(type) {
                case ZOMBIE: damage = 5; break;
                case IMP:    damage = 10; break;
                case DEMON:  damage = 20; break;
                case BOSS:   damage = 30; break;
            }
            playerHealth -= damage;
            attackCooldown = 1.0f;
            if (playerHealth < 0) playerHealth = 0;
            std::cout << "Player hit by " << (type==ZOMBIE?"Zombie":(type==IMP?"Imp":(type==DEMON?"Demon":"Boss")))
                      << "! HP: " << playerHealth << std::endl;
        }
    }

    void takeDamage(int dmg) {
        if (invulnerable) return;
        hp -= dmg;
        if (hp < 0) hp = 0;
        awakened = true;
        if (hp > 0) {
            dodgeTimer = 0.3f;
            dodgeAngle = static_cast<float>(rand()) / RAND_MAX * 2.0f * M_PI;
        }
    }

    bool isAlive() const { return hp > 0; }

    bool canSummon() const {
        return type == BOSS && awakened && summonCooldown <= 0.0f;
    }

    void resetSummonCooldown() {
        if (type == BOSS) {
            summonCooldown = 10.0f;
        }
    }

    std::vector<Mob> summonMobs() {
        std::vector<Mob> summoned;
        if (type == BOSS && !isAlive()) {
            MobType types[] = {ZOMBIE, IMP};
            for (int i = 0; i < 2; i++) {
                float angle = (float)rand() / RAND_MAX * 2.0f * M_PI;
                float dist = 100.0f;
                float spawnX = x + cos(angle) * dist;
                float spawnY = y + sin(angle) * dist;
                summoned.emplace_back(spawnX, spawnY, types[rand() % 2]);
            }
        }
        return summoned;
    }

    std::vector<Mob> summonMobsAround() {
        std::vector<Mob> summoned;
        if (type == BOSS) {
            MobType types[] = {ZOMBIE, IMP};
            for (int i = 0; i < 2; i++) {
                float angle = (float)rand() / RAND_MAX * 2.0f * M_PI;
                float dist = 80.0f;
                float spawnX = x + cos(angle) * dist;
                float spawnY = y + sin(angle) * dist;
                summoned.emplace_back(spawnX, spawnY, types[rand() % 2]);
            }
            summonedMobsCount = 2;
            invulnerable = true;
        }
        return summoned;
    }

private:
    bool collidesWithWall(float cx, float cy, float r, const std::vector<Linedef>& lines, const std::vector<Vertex>& vertices) const {
        for (const auto& line : lines) {
            if (line.startVertex >= vertices.size() || line.endVertex >= vertices.size()) continue;
            const Vertex& v1 = vertices[line.startVertex];
            const Vertex& v2 = vertices[line.endVertex];
            if (lineIntersectsCircle(cx, cy, r, v1.x, v1.y, v2.x, v2.y)) return true;
        }
        return false;
    }

    bool lineIntersectsCircle(float cx, float cy, float r, float x1, float y1, float x2, float y2) const {
        float dx1 = cx - x1, dy1 = cy - y1;
        float dx2 = cx - x2, dy2 = cy - y2;
        if (dx1*dx1 + dy1*dy1 < r*r) return true;
        if (dx2*dx2 + dy2*dy2 < r*r) return true;
        float lineDx = x2 - x1;
        float lineDy = y2 - y1;
        float len2 = lineDx*lineDx + lineDy*lineDy;
        if (len2 == 0.0f) return false;
        float t = ((cx - x1) * lineDx + (cy - y1) * lineDy) / len2;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        float projX = x1 + t * lineDx;
        float projY = y1 + t * lineDy;
        float dist2 = (cx - projX)*(cx - projX) + (cy - projY)*(cy - projY);
        return dist2 < r*r;
    }
};