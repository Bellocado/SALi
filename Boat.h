#pragma once
#include "raylib.h"
#include "Wind.h"

inline float dot(Vector2 a, Vector2 b) {
    return a.x * b.x + a.y * b.y;
}

inline float cross(Vector2 a, Vector2 b) {
    return a.x * b.y - a.y * b.x;
}

inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

struct Boat {
    float heading = 0.0f;
    float speed = 0.0f;
    float worldX =  0.0f;
    float worldY = 0.0f;
    float mainSheet = 0.15f;
    float outhaul = 0.9f;
    float rudder = 0.0f;
    bool vangOn = false;
    bool cunningOn = false;
    float dispTWA = 0.0f;
    float dispVMG = 0.0f;
    float sideSign = 1.0f;
    float driftDeg = 0.0f;
    float boomSheet = 0.15f;
    float boomSide = 1.0f;
    Vector2 awFrom = { 0.0f, -1.0f };
};

float normalizeAngle(float a);
Vector2 headingToVec(float deg);
float polarSpeed(float twa, float tws);
float sailEffectiveness(float twa, float mainSheet, float outhaul);
float minSailAngle();
void updateBoatPhysics(Boat& b, const Wind& w, float dt);
void resetBoat(Boat& b);
