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
    BOSS
};

enum MobState {
    IDLE,
    CHASE,
    ATTACK
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

    bool invulnerable;
    int summonedMobsCount;
    float vulnerabilityTimer;

    float attackRange;
    float shootRange;
    float projectileSpeed;
    int projectileDamage;

    float wanderTimer;
    float wanderAngle;
    float wanderSpeed;

    Mob(float _x, float _y, MobType _type) : x(_x), y(_y), type(_type), state(IDLE),
        attackCooldown(0.0f), fireCooldown(0.0f), awakened(false),
        dodgeTimer(0.0f), dodgeAngle(0.0f), summonCooldown(0.0f), isSummoning(false),
        invulnerable(false), summonedMobsCount(0), vulnerabilityTimer(0.0f),
        wanderTimer(0.0f), wanderAngle(0.0f), wanderSpeed(30.0f)
    {
        switch(_type) {
            case ZOMBIE:
                hp = 50; maxHp = 50;
                speed = 120.0f;
                size = 56;
                attackRange = 80.0f;
                shootRange = 500.0f;
                projectileSpeed = 400.0f;
                projectileDamage = 10;
                fireCooldown = 0.0f;
                wanderSpeed = 80.0f;
                break;
            case BOSS:
                hp = 400; maxHp = 400;
                speed = 80.0f;
                size = 96;
                attackRange = 120.0f;
                shootRange = 600.0f;
                projectileSpeed = 500.0f;
                projectileDamage = 25;
                fireCooldown = 0.0f;
                summonCooldown = 10.0f;
                isSummoning = false;
                invulnerable = false;
                summonedMobsCount = 0;
                vulnerabilityTimer = 0.0f;
                wanderSpeed = 60.0f;
                break;
        }
    }

    bool hasLineOfSight(float tx, float ty, const std::vector<Linedef>& lines, const std::vector<Vertex>& vertices) const {
        float dx = tx - x;
        float dy = ty - y;
        float dist = std::sqrt(dx*dx + dy*dy);
        if (dist < 0.1f) return true;
        float step = 0.5f;
        float nx = dx / dist;
        float ny = dy / dist;
        for (float d = 0; d < dist; d += step) {
            float cx = x + nx * d;
            float cy = y + ny * d;
            for (const auto& line : lines) {
                if (line.startVertex >= vertices.size() || line.endVertex >= vertices.size()) continue;
                const Vertex& v1 = vertices[line.startVertex];
                const Vertex& v2 = vertices[line.endVertex];
                float cross1 = nx * (v1.y - cy) - ny * (v1.x - cx);
                float cross2 = nx * (v2.y - cy) - ny * (v2.x - cx);
                if (cross1 * cross2 < 0) {
                    float ldx = v2.x - v1.x;
                    float ldy = v2.y - v1.y;
                    float cross3 = ldx * (cy - v1.y) - ldy * (cx - v1.x);
                    float cross4 = ldx * (ty - v1.y) - ldy * (tx - v1.x);
                    if (cross3 * cross4 < 0) return false;
                }
            }
        }
        return true;
    }

    void update(float dt, const Player& player, const std::vector<Linedef>& lines, const std::vector<Vertex>& vertices) {
        float dx = player.x - x;
        float dy = player.y - y;
        float distToPlayer = std::sqrt(dx*dx + dy*dy);

        if (!awakened) {
            if (distToPlayer < 500.0f && hasLineOfSight(player.x, player.y, lines, vertices)) {
                awakened = true;
                state = CHASE;
                std::cout << "Mob awakened at (" << x << "," << y << ")" << std::endl;
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
            return;
        }

        if (attackCooldown > 0.0f) attackCooldown -= dt;
        if (fireCooldown > 0.0f) fireCooldown -= dt;
        if (type == BOSS && summonCooldown > 0.0f) summonCooldown -= dt;
        if (type == BOSS && vulnerabilityTimer > 0.0f) {
            vulnerabilityTimer -= dt;
            if (vulnerabilityTimer <= 0.0f) invulnerable = false;
        }

        if (awakened && distToPlayer < shootRange && hasLineOfSight(player.x, player.y, lines, vertices)) {
            state = CHASE;
            if (distToPlayer > attackRange * 0.8f) {
                float moveX = dx / distToPlayer * speed * dt;
                float moveY = dy / distToPlayer * speed * dt;
                float newX = x + moveX;
                float newY = y + moveY;
                float halfSize = size / 2.0f;
                if (!collidesWithWall(newX, newY, halfSize, lines, vertices)) {
                    x = newX;
                    y = newY;
                } else {
                    if (!collidesWithWall(newX, y, halfSize, lines, vertices)) x = newX;
                    if (!collidesWithWall(x, newY, halfSize, lines, vertices)) y = newY;
                }
            }
        } else if (awakened && distToPlayer < shootRange && !hasLineOfSight(player.x, player.y, lines, vertices)) {
            if (distToPlayer > attackRange * 0.8f) {
                float moveX = dx / distToPlayer * speed * dt;
                float moveY = dy / distToPlayer * speed * dt;
                float newX = x + moveX;
                float newY = y + moveY;
                float halfSize = size / 2.0f;
                if (!collidesWithWall(newX, newY, halfSize, lines, vertices)) {
                    x = newX;
                    y = newY;
                }
            }
        } else if (awakened && distToPlayer >= shootRange) {
            if (distToPlayer > 0.01f) {
                float moveX = dx / distToPlayer * speed * dt;
                float moveY = dy / distToPlayer * speed * dt;
                float newX = x + moveX;
                float newY = y + moveY;
                float halfSize = size / 2.0f;
                if (!collidesWithWall(newX, newY, halfSize, lines, vertices)) {
                    x = newX;
                    y = newY;
                }
            }
        } else if (!awakened) {
            state = IDLE;
            wanderTimer -= dt;
            if (wanderTimer <= 0.0f) {
                wanderTimer = 1.5f + (rand() % 100) / 50.0f;
                wanderAngle = (rand() % 360) * M_PI / 180.0f;
            }
            float moveX = cos(wanderAngle) * wanderSpeed * dt;
            float moveY = sin(wanderAngle) * wanderSpeed * dt;
            float newX = x + moveX;
            float newY = y + moveY;
            float halfSize = size / 2.0f;
            if (!collidesWithWall(newX, newY, halfSize, lines, vertices)) {
                x = newX;
                y = newY;
            } else {
                wanderAngle += M_PI / 2;
            }
        }
    }

    void tryAttack(Player& player, float dt, int& playerHealth) {
        float dx = player.x - x;
        float dy = player.y - y;
        float dist = std::sqrt(dx*dx + dy*dy);

        if (dist < attackRange && attackCooldown <= 0.0f) {
            int damage = (type == BOSS) ? 30 : 10;
            playerHealth -= damage;
            attackCooldown = 1.0f;
            if (playerHealth < 0) playerHealth = 0;
            std::cout << "Player hit by " << (type == BOSS ? "Boss" : "Zombie") << "! HP: " << playerHealth << std::endl;
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
        if (type == BOSS) summonCooldown = 10.0f;
    }

    std::vector<Mob> summonMobsAround() {
        std::vector<Mob> summoned;
        if (type == BOSS && isSummoning) {
            for (int i = 0; i < 2; i++) {
                float angle = (float)rand() / RAND_MAX * 2.0f * M_PI;
                float dist = 80.0f;
                float spawnX = x + cos(angle) * dist;
                float spawnY = y + sin(angle) * dist;
                summoned.emplace_back(spawnX, spawnY, ZOMBIE);
            }
            summonedMobsCount = 2;
            invulnerable = true;
            isSummoning = false;
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