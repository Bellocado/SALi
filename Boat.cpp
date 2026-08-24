#include "Boat.h"

#include <cmath>

float normalizeAngle(float a) {
    a = std::fmod(a, 360.0f);
    if (a < 0.0f) {
        a += 360.0f;
    }
    return a;
}

Vector2 headingToVec(float deg) {
    Vector2 v;
    v.x = std::sin(deg * DEG2RAD);
    v.y = -std::cos(deg * DEG2RAD);
    return v;
}

Color makeColor(int r, int g, int b, int a) {
    Color c;
    c.r = static_cast<unsigned char>(r);
    c.g = static_cast<unsigned char>(g);
    c.b = static_cast<unsigned char>(b);
    c.a = static_cast<unsigned char>(a);
    return c;
}

float polarSpeed(float twa, float tws) {
    if (twa < 30.0f) {
        return 0.0f;
    }

    const float twsFactor = clampf(tws / 12.0f, 0.2f, 1.5f);

    float shape;
    if (twa < 45.0f) {
        shape = lerp(0.0f, 0.72f, (twa - 30.0f) / 15.0f);
    } else if (twa < 90.0f) {
        shape = lerp(0.72f, 1.0f, (twa - 45.0f) / 45.0f);
    } else if (twa < 130.0f) {
        shape = lerp(1.0f, 0.88f, (twa - 90.0f) / 40.0f);
    } else {
        shape = lerp(0.88f, 0.75f, (twa - 130.0f) / 50.0f);
    }

    return shape * 5.8f * twsFactor;
}

float sailEffectiveness(float twa, float mainSheet, float outhaul) {
    float ideal = clampf((twa - 35.0f) / 145.0f, 0.0f, 1.0f);
    ideal = lerp(ideal, ideal * 0.75f, clampf(outhaul, 0.0f, 1.0f));

    const float error = std::fabs(mainSheet - ideal);
    return clampf(1.0f - error * error * 5.0f, 0.0f, 1.0f);
}

float minSailAngle() {
    constexpr float BUM_MIN_STOP = 15.0f;
    const float v = (BUM_MIN_STOP - 5.0f) / 80.0f;
    return clampf(v, 0.0f, 1.0f);
}

void updateBoatPhysics(Boat& b, const Wind& w, float dt) {
    const Vector2 fwd = headingToVec(b.heading);
    const Vector2 windFrom = headingToVec(w.trueAngle);

    const Vector2 windVelocity = {
        -windFrom.x * w.trueSpeed,
        -windFrom.y * w.trueSpeed
    };

    const Vector2 boatVelocity = {
        fwd.x * b.speed,
        fwd.y * b.speed
    };

    Vector2 aw = {
        windVelocity.x - boatVelocity.x,
        windVelocity.y - boatVelocity.y
    };

    const float awsMag = std::sqrt(aw.x * aw.x + aw.y * aw.y);
    b.awFrom = (awsMag > 1e-6f)
    ? Vector2{ -aw.x / awsMag, -aw.y / awsMag }
    : windFrom;

    const float twa = std::acos(clampf(dot(fwd, b.awFrom), -1.0f, 1.0f)) * RAD2DEG;

    // Hysteresis for tacking: keep the old side until the wind crosses enough.
    const float sideCross = cross(fwd, b.awFrom);
    if (sideCross > 0.18f) {
        b.sideSign = 1.0f;
    } else if (sideCross < -0.18f) {
        b.sideSign = -1.0f;
    }

    // Wind limit: the point where the sail would start luffing.
    const float windLimit = clampf((twa - 30.0f) / 140.0f, minSailAngle(), 1.0f);

    // Update actual boom position.
    if (b.mainSheet >= windLimit) {
        b.boomSheet = windLimit;
    } else {
        b.boomSheet = b.mainSheet;
    }

    const float sideSmooth = clampf(dt * 8.0f, 0.0f, 1.0f);
    b.boomSide += (b.sideSign - b.boomSide) * sideSmooth;

    // If the sheet is too loose, the sail is flapping and has no power.
    const bool luffing = b.mainSheet > windLimit || twa < 25.0f;

    const float trimEff = luffing ? 0.0f : sailEffectiveness(twa, b.boomSheet, b.outhaul);
    const float vangPower = b.vangOn ? (twa < 90.0f ? 1.06f : 0.92f) : 1.0f;
    const float cunningPower = b.cunningOn ? 0.98f : 1.0f;
    const float powerCoeff = vangPower * cunningPower;

    const float idealOuthaul = clampf(1.0f - (twa - 30.0f) / 150.0f, 0.0f, 1.0f);
    const float outhaulError = std::fabs(b.outhaul - idealOuthaul);
    const float outhaulBonus = clampf(1.0f - outhaulError * 3.0f, 0.0f, 1.0f);
    const float outhaulCoeff = 1.0f + outhaulBonus * 0.35f;

    const float targetSpeed = polarSpeed(twa, awsMag) * trimEff * powerCoeff * outhaulCoeff;

    b.speed += (targetSpeed - b.speed) * dt;
    b.heading = normalizeAngle(
        b.heading + (b.rudder * (0.5f + b.speed * 0.2f)) * dt
    );

    const float driftAmt = (twa < 90.0f)
    ? (1.0f - twa / 90.0f) * (4.0f / (1.0f + b.speed))
    : 0.0f;
    b.driftDeg = driftAmt * -b.sideSign;

    const Vector2 courseDir = headingToVec(b.heading + b.driftDeg);
    b.worldX += courseDir.x * b.speed * 10.0f * dt;
    b.worldY += courseDir.y * b.speed * 10.0f * dt;

    b.dispTWA = twa;
    b.dispVMG = b.speed * std::cos(twa * DEG2RAD);
}

void resetBoat(Boat& b) {
    b = Boat{};
    b.heading = 90.0f;
}
