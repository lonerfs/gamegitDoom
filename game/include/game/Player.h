#pragma once

#include <cmath>

class Player {
public:
    float x, y;
    float angle;
    float speed;

    Player() {
        x = 1056.0f;
        y = 1056.0f;
        angle = 0.0f;
        speed = 5.0f;
    }

    void moveForward() {
        x += cos(angle) * speed;
        y += sin(angle) * speed;
    }

    void moveBackward() {
        x -= cos(angle) * speed;
        y -= sin(angle) * speed;
    }

    void strafeLeft() {
        x += sin(angle) * speed;
        y -= cos(angle) * speed;
    }

    void strafeRight() {
        x -= sin(angle) * speed;
        y += cos(angle) * speed;
    }

    void turnLeft(float delta) {
        angle -= delta;
    }

    void turnRight(float delta) {
        angle += delta;
    }
};