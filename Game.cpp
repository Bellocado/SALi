#include "Game.h"
#include "UI.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

namespace {

    Color uiColor(int r, int g, int b, int a) {
        Color c;
        c.r = static_cast<unsigned char>(r);
        c.g = static_cast<unsigned char>(g);
        c.b = static_cast<unsigned char>(b);
        c.a = static_cast<unsigned char>(a);
        return c;
    }

    Vector2 midpoint(Vector2 a, Vector2 b) {
        return { (a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f };
    }

    void drawSailCurve(Vector2 p0, Vector2 ctrl, Vector2 p1, Color col) {
        constexpr int steps = 16;
        Vector2 prev = p0;

        for (int i = 1; i <= steps; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(steps);
            const float a = (1.0f - t) * (1.0f - t);
            const float b = 2.0f * (1.0f - t) * t;
            const float c = t * t;

            Vector2 pt = {
                a * p0.x + b * ctrl.x + c * p1.x,
                a * p0.y + b * ctrl.y + c * p1.y
            };

            DrawLineEx(prev, pt, 3.5f, col);
            prev = pt;
        }
    }

} // namespace

void Game::Update(float dt) {
    if (state_ == State::Play) {
        prevBoatPos_ = { boat_.worldX, boat_.worldY };

        handleBoatControls(dt);
        updateBoatPhysics(boat_, wind_, dt);

        // Track 360-degree penalty turns.
        if (pendingPenalties_ > 0) {
            float raw = boat_.heading - lastPenaltyHeading_;
            raw = normalizeAngle(raw + 180.0f) - 180.0f;

            penaltyTurnAccum_ += raw;

            if (std::fabs(penaltyTurnAccum_) >= 360.0f) {
                --pendingPenalties_;
                penaltyTurnAccum_ = 0.0f;
            }
        }

        lastPenaltyHeading_ = boat_.heading;

        if (raceMode_) {
            if (prestartActive_) {
                prestartTime_ -= dt;

                if (prestartTime_ <= 0.0f) {
                    prestartTime_ = 0.0f;
                    prestartActive_ = false;
                    raceStarted_ = true;
                } else {
                    checkEarlyStart();
                    handleBuoyCollisions();
                }
            } else if (raceStarted_ && !raceFinished_) {
                raceTime_ += dt;
                checkRaceProgress();
                handleBuoyCollisions();

                if (raceFinished_ && !raceFinishedReported_) {
                    saveRaceResult();
                    raceFinishedReported_ = true;
                }
            }
        }

        if (collisionCooldown_ > 0.0f) {
            collisionCooldown_ -= dt;
            if (collisionCooldown_ < 0.0f) {
                collisionCooldown_ = 0.0f;
            }
        }
    } else if (state_ == State::Setup) {
        handleSetupInput(dt);
    }
}

void Game::Draw() {
    ClearBackground(uiColor(245, 240, 230, 255));

    switch (state_) {
        case State::Menu:    drawMenu();    break;
        case State::Setup:   drawSetup();   break;
        case State::Play:    drawPlay();    break;
        case State::Credits: drawCredits(); break;
    }
}

void Game::handleBoatControls(float dt) {
    boat_.rudder = IsKeyDown(KEY_A) ? -40.0f : (IsKeyDown(KEY_D) ? 40.0f : 0.0f);

    const float geoMin = minSailAngle();
    if (boat_.mainSheet < geoMin) {
        boat_.mainSheet = geoMin;
    }

    if (IsKeyDown(KEY_W)) {
        boat_.mainSheet = clampf(boat_.mainSheet - dt, geoMin, 1.0f);
    }
    if (IsKeyDown(KEY_S)) {
        boat_.mainSheet = clampf(boat_.mainSheet + dt, geoMin, 1.0f);
    }

    if (IsKeyPressed(KEY_V)) {
        boat_.vangOn = !boat_.vangOn;
    }
    if (IsKeyPressed(KEY_C)) {
        boat_.cunningOn = !boat_.cunningOn;
    }

    if (IsKeyDown(KEY_Q)) {
        boat_.outhaul = clampf(boat_.outhaul - dt * 0.25f, 0.0f, 1.0f);
    }
    if (IsKeyDown(KEY_E)) {
        boat_.outhaul = clampf(boat_.outhaul + dt * 0.25f, 0.0f, 1.0f);
    }
}

void Game::handleSetupInput(float dt) {
    caretTimer_ += dt;

    if (clickedOn(kDirRect)) {
        angleActive_ = true;
        speedActive_ = false;
    } else if (clickedOn(kSpdRect)) {
        speedActive_ = true;
        angleActive_ = false;
    } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        angleActive_ = false;
        speedActive_ = false;
    }

    int ch = GetCharPressed();
    while (ch > 0) {
        if (angleActive_) {
            if (((ch >= '0' && ch <= '9') || ch == '.' || ch == '-') &&
                std::strlen(angleBuf_) < sizeof(angleBuf_) - 1) {
                const size_t len = std::strlen(angleBuf_);
            angleBuf_[len] = static_cast<char>(ch);
            angleBuf_[len + 1] = '\0';
                }
        } else if (speedActive_) {
            if (((ch >= '0' && ch <= '9') || ch == '.' || ch == '-') &&
                std::strlen(speedBuf_) < sizeof(speedBuf_) - 1) {
                const size_t len = std::strlen(speedBuf_);
            speedBuf_[len] = static_cast<char>(ch);
            speedBuf_[len + 1] = '\0';
                }
        }

        ch = GetCharPressed();
    }

    if (angleActive_ && IsKeyPressed(KEY_BACKSPACE)) {
        const size_t len = std::strlen(angleBuf_);
        if (len > 0) {
            angleBuf_[len - 1] = '\0';
        }
    }

    if (speedActive_ && IsKeyPressed(KEY_BACKSPACE)) {
        const size_t len = std::strlen(speedBuf_);
        if (len > 0) {
            speedBuf_[len - 1] = '\0';
        }
    }
}

void Game::startGame() {
    float angle = static_cast<float>(std::atof(angleBuf_));
    if (!std::isfinite(angle)) {
        angle = 0.0f;
    }
    angle = normalizeAngle(angle);

    float speed = static_cast<float>(std::atof(speedBuf_));
    if (!std::isfinite(speed) || speed < 0.0f) {
        speed = 0.0f;
    }

    resetWind(wind_);
    wind_.trueAngle = angle;
    wind_.trueSpeed = speed;

    resetBoat(boat_);

    if (raceMode_) {
        resetRace();
        generateCourse();

        prestartActive_ = true;
        raceStarted_ = false;
        raceFinished_ = false;
        raceFinishedReported_ = false;
        bfd_ = false;
        pendingPenalties_ = 0;
        penaltyTurnAccum_ = 0.0f;
        lastPenaltyHeading_ = 0.0f;

        prestartTime_ = static_cast<float>(prestartMinutes_ * 60);
        raceTime_ = 0.0f;
        collisionCooldown_ = 0.0f;
        currentCheckpoint_ = 0;
        markLineACrossed_ = false;

        prevBoatPos_ = { boat_.worldX, boat_.worldY };
    } else {
        boat_.worldX = 0.0f;
        boat_.worldY = 0.0f;
        boat_.heading = wind_.trueAngle;
        boat_.speed = 0.0f;
    }

    state_ = State::Play;
}

void Game::resetRace() {
    prestartActive_ = false;
    raceStarted_ = false;
    raceFinished_ = false;
    raceFinishedReported_ = false;
    bfd_ = false;
    pendingPenalties_ = 0;
    penaltyTurnAccum_ = 0.0f;
    lastPenaltyHeading_ = 0.0f;
    prestartTime_ = static_cast<float>(prestartMinutes_ * 60);
    raceTime_ = 0.0f;
    collisionCooldown_ = 0.0f;
    currentCheckpoint_ = 0;
    markLineACrossed_ = false;

    course_.clear();
    buoys_.clear();
}

void Game::generateCourse() {
    course_.clear();
    buoys_.clear();

    const Vector2 upwind = headingToVec(wind_.trueAngle);
    const Vector2 right = { -upwind.y, upwind.x };   // starboard side
    const Vector2 left = { -right.x, -right.y };     // port side
    const Vector2 downwind = { -upwind.x, -upwind.y };

    constexpr float kUpwindLength         = 1300.0f;
    constexpr float kReachOffset          = 750.0f; // longer reach, to the left
    constexpr float kReachDownwindOffset  = 300.0f; // reach mark sits downwind of Mark 1
    constexpr float kLeewardLegLength     = 1250.0f;
    constexpr float kGateHalfWidth        = 160.0f;
    constexpr float kStartHalfWidth       = 180.0f;
    constexpr float kFinishReachLength    = 480.0f;
    constexpr float kFinishDownwindOffset = 220.0f; // finish lower than the gate
    constexpr float kFinishHalfWidth      = 150.0f;
    constexpr float kBuoyRadius           = 22.0f;
    constexpr float kMarkRoundingRadius   = 80.0f;
    constexpr float kCrossingThreshold    = 55.0f;

    const Vector2 startCenter = { 0.0f, 0.0f };

    const Vector2 startA = {
        startCenter.x - right.x * kStartHalfWidth,
        startCenter.y - right.y * kStartHalfWidth
    };
    const Vector2 startB = {
        startCenter.x + right.x * kStartHalfWidth,
        startCenter.y + right.y * kStartHalfWidth
    };

    startBuoyPos_ = startA;
    startBoatPos_ = startB;

    CourseCheckpoint cp;

    // 1. Start line
    cp.type = CourseCheckpoint::Type::StartLine;
    cp.a = startA;
    cp.b = startB;
    cp.radius = kCrossingThreshold;
    cp.passDir = upwind;
    course_.push_back(cp);

    buoys_.push_back({ startA, kBuoyRadius });
    buoys_.push_back({ startB, kBuoyRadius });

    // 2. Mark 1 — windward mark
    const Vector2 mark1 = {
        startCenter.x + upwind.x * kUpwindLength,
        startCenter.y + upwind.y * kUpwindLength
    };

    cp.type = CourseCheckpoint::Type::Mark;
    cp.a = mark1;
    cp.b = mark1;
    cp.radius = kMarkRoundingRadius;
    cp.passDir = { 0.0f, 0.0f };
    course_.push_back(cp);

    buoys_.push_back({ mark1, kBuoyRadius });

    // 3. Mark 2 — reaching mark.
    //    Left of Mark 1, longer reach, and a bit downwind.
    const Vector2 mark2 = {
        mark1.x + left.x * kReachOffset + downwind.x * kReachDownwindOffset,
        mark1.y + left.y * kReachOffset + downwind.y * kReachDownwindOffset
    };

    cp.type = CourseCheckpoint::Type::Mark;
    cp.a = mark2;
    cp.b = mark2;
    cp.radius = kMarkRoundingRadius;
    cp.passDir = { 0.0f, 0.0f };
    course_.push_back(cp);

    buoys_.push_back({ mark2, kBuoyRadius });

    // 4. Leeward gate 3s/3p, downwind of Mark 2.
    const Vector2 gateCenter = {
        mark2.x + downwind.x * kLeewardLegLength,
        mark2.y + downwind.y * kLeewardLegLength
    };

    const Vector2 gate3s = {
        gateCenter.x + right.x * kGateHalfWidth,
        gateCenter.y + right.y * kGateHalfWidth
    };
    const Vector2 gate3p = {
        gateCenter.x - right.x * kGateHalfWidth,
        gateCenter.y - right.y * kGateHalfWidth
    };

    cp.type = CourseCheckpoint::Type::Gate;
    cp.a = gate3s;
    cp.b = gate3p;
    cp.radius = kCrossingThreshold;
    cp.passDir = downwind;
    course_.push_back(cp);

    buoys_.push_back({ gate3s, kBuoyRadius });
    buoys_.push_back({ gate3p, kBuoyRadius });

    // 5. Mark 2 again — upwind return to the reach mark.
    cp.type = CourseCheckpoint::Type::Mark;
    cp.a = mark2;
    cp.b = mark2;
    cp.radius = kMarkRoundingRadius;
    cp.passDir = { 0.0f, 0.0f };
    course_.push_back(cp);

    buoys_.push_back({ mark2, kBuoyRadius });

    // 6. Right gate buoy only (3s).
    cp.type = CourseCheckpoint::Type::Mark;
    cp.a = gate3s;
    cp.b = gate3s;
    cp.radius = kMarkRoundingRadius;
    cp.passDir = { 0.0f, 0.0f };
    course_.push_back(cp);

    buoys_.push_back({ gate3s, kBuoyRadius });

    // 7. Finish line — reach finish, lower than the gate.
    const Vector2 finishDirRaw = {
        right.x * kFinishReachLength + downwind.x * kFinishDownwindOffset,
        right.y * kFinishReachLength + downwind.y * kFinishDownwindOffset
    };
    const float finishDirLen = std::sqrt(finishDirRaw.x * finishDirRaw.x +
    finishDirRaw.y * finishDirRaw.y);
    const Vector2 finishDir = {
        finishDirRaw.x / finishDirLen,
        finishDirRaw.y / finishDirLen
    };

    const Vector2 finishCenter = {
        gate3s.x + finishDirRaw.x,
        gate3s.y + finishDirRaw.y
    };

    const Vector2 finishTangent = { -finishDir.y, finishDir.x };

    const Vector2 finishA = {
        finishCenter.x - finishTangent.x * kFinishHalfWidth,
        finishCenter.y - finishTangent.y * kFinishHalfWidth
    };
    const Vector2 finishB = {
        finishCenter.x + finishTangent.x * kFinishHalfWidth,
        finishCenter.y + finishTangent.y * kFinishHalfWidth
    };

    finishBuoyPos_ = finishA;
    finishBoatPos_ = finishB;

    cp.type = CourseCheckpoint::Type::FinishLine;
    cp.a = finishA;
    cp.b = finishB;
    cp.radius = kCrossingThreshold;
    cp.passDir = finishDir;
    course_.push_back(cp);

    buoys_.push_back({ finishA, kBuoyRadius });
    buoys_.push_back({ finishB, kBuoyRadius });

    // Spawn just downwind of the start line.
    const Vector2 spawn = {
        startCenter.x + downwind.x * 80.0f,
        startCenter.y + downwind.y * 80.0f
    };

    boat_.worldX = spawn.x;
    boat_.worldY = spawn.y;
    boat_.heading = wind_.trueAngle;
    boat_.speed = 0.0f;
    prevBoatPos_ = spawn;
}

void Game::saveRaceResult() {
    FILE* file = std::fopen("leaderboard.txt", "a");
    if (file == nullptr) {
        return;
    }

    if (bfd_) {
        std::fprintf(file, "BFD\n");
    } else {
        std::fprintf(file, "Time: %.2f s\n", raceTime_);
    }

    std::fclose(file);
}

void Game::checkRaceProgress() {
    if (course_.empty() || currentCheckpoint_ >= static_cast<int>(course_.size())) {
        return;
    }

    const CourseCheckpoint& cp = course_[currentCheckpoint_];
    const Vector2 curr = { boat_.worldX, boat_.worldY };
    const Vector2 prev = prevBoatPos_;

    bool completed = false;

    if (cp.type == CourseCheckpoint::Type::StartLine ||
        cp.type == CourseCheckpoint::Type::Gate ||
        cp.type == CourseCheckpoint::Type::FinishLine) {

        const Vector2 center = midpoint(cp.a, cp.b);

    const float prevSide = dot({ prev.x - center.x, prev.y - center.y },
                               cp.passDir);
    const float currSide = dot({ curr.x - center.x, curr.y - center.y },
                               cp.passDir);

    if (prevSide <= 0.0f && currSide > 0.0f &&
        distanceToSegment(curr, cp.a, cp.b) < cp.radius) {
        completed = true;
        }
        } else if (cp.type == CourseCheckpoint::Type::Mark) {
            const Vector2 markPos = cp.a;

            Vector2 prevCenter = markPos;
            Vector2 nextCenter = markPos;

            if (currentCheckpoint_ > 0) {
                const CourseCheckpoint& pc = course_[currentCheckpoint_ - 1];
                if (pc.type == CourseCheckpoint::Type::StartLine ||
                    pc.type == CourseCheckpoint::Type::Gate ||
                    pc.type == CourseCheckpoint::Type::FinishLine) {
                    prevCenter = midpoint(pc.a, pc.b);
                    } else {
                        prevCenter = pc.a;
                    }
            }

            if (currentCheckpoint_ + 1 < static_cast<int>(course_.size())) {
                const CourseCheckpoint& nc = course_[currentCheckpoint_ + 1];
                if (nc.type == CourseCheckpoint::Type::StartLine ||
                    nc.type == CourseCheckpoint::Type::Gate ||
                    nc.type == CourseCheckpoint::Type::FinishLine) {
                    nextCenter = midpoint(nc.a, nc.b);
                    } else {
                        nextCenter = nc.a;
                    }
            }

            const Vector2 approachRaw = {
                markPos.x - prevCenter.x,
                markPos.y - prevCenter.y
            };
            const float approachLen = std::sqrt(approachRaw.x * approachRaw.x +
            approachRaw.y * approachRaw.y);
            const Vector2 approachDir = {
                approachRaw.x / (approachLen + 1e-6f),
                approachRaw.y / (approachLen + 1e-6f)
            };

            const Vector2 departRaw = {
                nextCenter.x - markPos.x,
                nextCenter.y - markPos.y
            };
            const float departLen = std::sqrt(departRaw.x * departRaw.x +
            departRaw.y * departRaw.y);
            const Vector2 departDir = {
                departRaw.x / (departLen + 1e-6f),
                departRaw.y / (departLen + 1e-6f)
            };

            const float entryOffset = 90.0f;
            const float exitOffset  = 90.0f;
            const float lineHalfLen = 350.0f;

            const Vector2 entryMid = {
                markPos.x - approachDir.x * entryOffset,
                markPos.y - approachDir.y * entryOffset
            };
            const Vector2 entryTangent = { -approachDir.y, approachDir.x };

            const Vector2 exitMid = {
                markPos.x + departDir.x * exitOffset,
                markPos.y + departDir.y * exitOffset
            };
            const Vector2 exitTangent = { -departDir.y, departDir.x };

            if (!markLineACrossed_) {
                if (crossLineCheck(prev, curr, entryMid, approachDir,
                    entryTangent, lineHalfLen)) {
                    markLineACrossed_ = true;
                    }
            } else {
                if (crossLineCheck(prev, curr, exitMid, departDir,
                    exitTangent, lineHalfLen)) {
                    completed = true;
                markLineACrossed_ = false;
                    }
            }
        }

        if (completed && cp.type == CourseCheckpoint::Type::FinishLine && pendingPenalties_ > 0) {
            completed = false;
        }

        if (completed) {
            ++currentCheckpoint_;

            if (currentCheckpoint_ >= static_cast<int>(course_.size())) {
                raceFinished_ = true;
            }
        }
}

void Game::checkEarlyStart() {
    if (course_.empty() || currentCheckpoint_ != 0) {
        return;
    }

    // Only in the final minute before the start.
    if (prestartTime_ <= 0.0f || prestartTime_ > 60.0f) {
        return;
    }

    const CourseCheckpoint& cp = course_[0];
    const Vector2 curr = { boat_.worldX, boat_.worldY };
    const Vector2 prev = prevBoatPos_;
    const Vector2 upwind = headingToVec(wind_.trueAngle);
    const Vector2 center = midpoint(cp.a, cp.b);

    const float prevSide = dot({ prev.x - center.x, prev.y - center.y }, upwind);
    const float currSide = dot({ curr.x - center.x, curr.y - center.y }, upwind);

    if (prevSide <= 0.0f && currSide > 0.0f &&
        distanceToSegment(curr, cp.a, cp.b) < cp.radius &&
        !bfd_) {
        bfd_ = true;
        }
}

void Game::handleBuoyCollisions() {
    if (collisionCooldown_ > 0.0f) {
        return;
    }

    const Vector2 boatPos = { boat_.worldX, boat_.worldY };

    for (const auto& [pos, radius] : buoys_) {
        const float dx = boatPos.x - pos.x;
        const float dy = boatPos.y - pos.y;
        const float dist = std::sqrt(dx * dx + dy * dy);

        if (dist < radius) {
            ++pendingPenalties_;
            collisionCooldown_ = 1.5f;
            break;
        }
    }
}

bool Game::crossLineCheck(Vector2 prev, Vector2 curr, Vector2 lineMid, Vector2 passDir, Vector2 tangent, float halfLen) const {
                              const Vector2 lineA = {
                                  lineMid.x - tangent.x * halfLen,
                                  lineMid.y - tangent.y * halfLen
                              };
                              const Vector2 lineB = {
                                  lineMid.x + tangent.x * halfLen,
                                  lineMid.y + tangent.y * halfLen
                              };

                              const float prevSide = dot({ prev.x - lineMid.x, prev.y - lineMid.y },
                                                         passDir);
                              const float currSide = dot({ curr.x - lineMid.x, curr.y - lineMid.y },
                                                         passDir);

                              if (prevSide <= 0.0f && currSide > 0.0f) {
                                  return distanceToSegment(curr, lineA, lineB) < 180.0f;
                              }

                              return false;
                          }

float Game::distanceToSegment(Vector2 p, Vector2 a, Vector2 b) const {
    const Vector2 ab = { b.x - a.x, b.y - a.y };
    const Vector2 ap = { p.x - a.x, p.y - a.y };

    const float len2 = ab.x * ab.x + ab.y * ab.y;
    float t = len2 > 0.0f ? (ap.x * ab.x + ap.y * ab.y) / len2 : 0.0f;
    t = clampf(t, 0.0f, 1.0f);

    const float dx = p.x - (a.x + ab.x * t);
    const float dy = p.y - (a.y + ab.y * t);
    return std::sqrt(dx * dx + dy * dy);
}

Vector2 Game::worldToScreen(Vector2 world) const {
    return {
        world.x - boat_.worldX + 640.0f,
        world.y - boat_.worldY + 360.0f
    };
}

Vector2 Game::nextTargetPosition() const {
    if (course_.empty() || currentCheckpoint_ >= static_cast<int>(course_.size())) {
        return { 0.0f, 0.0f };
    }

    const CourseCheckpoint& cp = course_[currentCheckpoint_];

    if (cp.type == CourseCheckpoint::Type::StartLine ||
        cp.type == CourseCheckpoint::Type::FinishLine ||
        cp.type == CourseCheckpoint::Type::Gate) {
        return midpoint(cp.a, cp.b);
        }

        return cp.a;
}

bool Game::isTargetVisible(Vector2 screenPos) const {
    const float margin = 50.0f;
    return screenPos.x >= margin &&
    screenPos.x <= 1280.0f - margin &&
    screenPos.y >= margin &&
    screenPos.y <= 720.0f - margin;
}

Game::FlagPhase Game::currentFlagPhase() const {
    if (!raceMode_ || !prestartActive_) {
        return FlagPhase::None;
    }

    const float remain = prestartTime_;
    const float duration = static_cast<float>(prestartMinutes_ * 60);

    // 4.7 flag is up for the entire prestart.
    if (remain > 0.0f) {
        // Black flag up from duration - 60 until 60 seconds before start.
        const float blackUpFrom = duration - 60.0f; // e.g. 240 for 5 min
        if (remain <= blackUpFrom && remain > 60.0f) {
            return FlagPhase::PrepUp;
        }

        // Black flag down during final minute.
        return FlagPhase::PrepDown;
    }

    return FlagPhase::None;
}

void Game::drawMenu() {
    DrawText("SALi - ILCA 4", 480, 100, 64, BLACK);

    Rectangle rPlay = { 540, 220, 200, 50 };
    Rectangle rCredits = { 540, 290, 200, 50 };
    Rectangle rExit = { 540, 360, 200, 50 };
    Rectangle rRaceToggle = { 540, 430, 200, 50 };
    Rectangle rPrestart = { 540, 500, 200, 50 };

    if (button(rPlay, "Play", 24)) {
        std::snprintf(angleBuf_, sizeof(angleBuf_), "%.0f", wind_.trueAngle);
        std::snprintf(speedBuf_, sizeof(speedBuf_), "%.1f", wind_.trueSpeed);
        angleActive_ = false;
        speedActive_ = false;
        caretTimer_ = 0.0f;
        state_ = State::Setup;
    }

    if (button(rCredits, "Credits", 24)) {
        state_ = State::Credits;
    }

    if (button(rExit, "Exit", 24)) {
        shouldClose_ = true;
    }

    const char* raceLabel = raceMode_ ? "Race: ON" : "Race: OFF";
    if (button(rRaceToggle, raceLabel, 22)) {
        raceMode_ = !raceMode_;
    }

    char prestartLabel[64];
    std::snprintf(prestartLabel, sizeof(prestartLabel),
                  "Prestart: %d min", prestartMinutes_);

    if (button(rPrestart, prestartLabel, 20)) {
        ++prestartMinutes_;
        if (prestartMinutes_ > 5) {
            prestartMinutes_ = 1;
        }
    }
}

void Game::drawSetup() {
    DrawText("Setup Wind", 500, 100, 48, BLACK);
    DrawText("Adjust wind direction and speed before starting.",
             420, 160, 20, DARKGRAY);

    DrawText("Direction (deg):", 420, 200, 16, DARKGRAY);
    drawTextField(kDirRect, angleBuf_, angleActive_, caretTimer_);

    DrawText("Speed (kts):", 420, 252, 16, DARKGRAY);
    drawTextField(kSpdRect, speedBuf_, speedActive_, caretTimer_);

    Rectangle rStart = { 520, 360, 200, 48 };
    Rectangle rBack = { 520, 420, 200, 40 };

    if (button(rStart, "Start", 24)) {
        startGame();
    }

    if (button(rBack, "Back", 20)) {
        state_ = State::Menu;
    }
}

void Game::drawPlay() {
    drawGrid();
    drawWindArrow();

    if (raceMode_) {
        drawCourse();
    }

    drawBoat();
    drawOverlay();
    drawWindex();
    drawRaceInfo();
    drawRaceFlagsPanel();
    drawOffscreenArrow();
    drawMinimap();

    Rectangle rBack = { 20, 20, 100, 34 };
    if (button(rBack, "Menu", 18)) {
        state_ = State::Menu;
    }
}

void Game::drawCredits() {
    DrawText("Credits", 560, 120, 48, BLACK);
    DrawText("Author: Bwello", 420, 220, 20, DARKGRAY);
    DrawText("Libraries: raylib", 420, 250, 20, DARKGRAY);

    Rectangle rBack = { 540, 560, 200, 40 };
    if (button(rBack, "Back", 20)) {
        state_ = State::Menu;
    }
}

void Game::drawGrid() {
    float gx = std::fmod(640.0f - boat_.worldX, 100.0f);
    if (gx < 0.0f) {
        gx += 100.0f;
    }

    float gy = std::fmod(360.0f - boat_.worldY, 100.0f);
    if (gy < 0.0f) {
        gy += 100.0f;
    }

    const Color gridColor = uiColor(60, 100, 150, 80);

    for (int i = -1; i < 15; ++i) {
        const float x = i * 100.0f + gx;
        DrawLineV({ x, 0.0f }, { x, 720.0f }, gridColor);
    }

    for (int i = -1; i < 9; ++i) {
        const float y = i * 100.0f + gy;
        DrawLineV({ 0.0f, y }, { 1280.0f, y }, gridColor);
    }
}

void Game::drawBoat() {
    const Vector2 pos = { 640.0f, 360.0f };
    const Vector2 fwd = headingToVec(boat_.heading);
    const Vector2 right = { fwd.y, -fwd.x };

    const Vector2 p1 = { pos.x + fwd.x * 65.0f, pos.y + fwd.y * 65.0f };
    const Vector2 p2 = {
        pos.x - fwd.x * 35.0f - right.x * 20.0f,
        pos.y - fwd.y * 35.0f - right.y * 20.0f
    };
    const Vector2 p3 = {
        pos.x - fwd.x * 35.0f + right.x * 20.0f,
        pos.y - fwd.y * 35.0f + right.y * 20.0f
    };

    DrawTriangle(p1, p3, p2, LIGHTGRAY);
    DrawTriangleLines(p1, p2, p3, BLACK);

    const Vector2 mast = { pos.x + fwd.x * 40.0f, pos.y + fwd.y * 40.0f };
    //const float boomAngle = boat_.heading + 180.0f + boat_.sideSign * (5.0f + boat_.boomSheet * 80.0f);
    const float twaCenter = clampf(boat_.dispTWA / 30.0f, 0.0f, 1.0f);
    const float boomAngle = boat_.heading + 180.0f + boat_.boomSide * (5.0f + boat_.boomSheet * 80.0f) * twaCenter;
    const Vector2 boomDir = headingToVec(boomAngle);
    const Vector2 boomEnd = {
        mast.x + boomDir.x * 85.0f,
        mast.y + boomDir.y * 85.0f
    };

    Vector2 normal = {
        -(boomEnd.y - mast.y),
        boomEnd.x - mast.x
    };

    const float normalLen = std::sqrt(normal.x * normal.x + normal.y * normal.y);
    if (normalLen > 0.0f) {
        normal.x /= normalLen;
        normal.y /= normalLen;
    }

    constexpr float baseBelly = 25.0f;
    const float vangFactor = boat_.vangOn ? 0.85f : 1.0f;
    const float belly = baseBelly * vangFactor *
    (1.0f - boat_.outhaul * 0.8f) *
    (1.0f - boat_.boomSheet * 0.4f);

    const float drawSide = boat_.boomSide >= 0.0f ? 1.0f : -1.0f;

    Vector2 ctrl = {
        (mast.x + boomEnd.x) * 0.5f + normal.x * belly * drawSide,
        (mast.y + boomEnd.y) * 0.5f + normal.y * belly * drawSide
    };

    /*if (boat_.dispTWA < 25.0f) {
        const float flap = std::sin(GetTime() * 25.0f) * 6.0f;
        ctrl.x += normal.x * flap * boat_.sideSign;
        ctrl.y += normal.y * flap * boat_.sideSign;
    }*/

    const float visualWindLimit = clampf((boat_.dispTWA - 30.0f) / 140.0f, minSailAngle(), 1.0f);

    const bool visualLuffing = boat_.dispTWA < 25.0f || boat_.mainSheet > visualWindLimit;

    if (visualLuffing) {
        const float flap = std::sin(GetTime() * 25.0f) * 6.0f;
        ctrl.x += normal.x * flap * boat_.sideSign;
        ctrl.y += normal.y * flap * boat_.sideSign;
    }

    DrawLineEx(mast, boomEnd, 4.0f, DARKGRAY);
    drawSailCurve(mast, ctrl, boomEnd, ORANGE);
}

void Game::drawMinimap() {
    const Rectangle area = { 1000.0f, 500.0f, 250.0f, 190.0f };

    DrawRectangleRounded(area, 0.08f, 6, uiColor(10, 14, 22, 220));
    DrawRectangleLinesEx(
        { area.x, area.y, area.width, area.height },
        1.5f,
        uiColor(80, 90, 110, 255)
    );

    if (course_.empty()) {
        return;
    }

    float minX = boat_.worldX;
    float maxX = boat_.worldX;
    float minY = boat_.worldY;
    float maxY = boat_.worldY;

    for (const auto& cp : course_) {
        minX = std::fmin(minX, cp.a.x);
        maxX = std::fmax(maxX, cp.a.x);
        minY = std::fmin(minY, cp.a.y);
        maxY = std::fmax(maxY, cp.a.y);

        minX = std::fmin(minX, cp.b.x);
        maxX = std::fmax(maxX, cp.b.x);
        minY = std::fmin(minY, cp.b.y);
        maxY = std::fmax(maxY, cp.b.y);
    }

    for (const auto& [pos, radius] : buoys_) {
        (void)radius;
        minX = std::fmin(minX, pos.x);
        maxX = std::fmax(maxX, pos.x);
        minY = std::fmin(minY, pos.y);
        maxY = std::fmax(maxY, pos.y);
    }

    constexpr float padding = 250.0f;
    minX -= padding;
    maxX += padding;
    minY -= padding;
    maxY += padding;

    const float worldW = maxX - minX;
    const float worldH = maxY - minY;

    if (worldW < 1.0f || worldH < 1.0f) {
        return;
    }

    const float scale = std::fmin(
        (area.width - 24.0f) / worldW,
                                  (area.height - 24.0f) / worldH
    );

    const float worldCenterX = (minX + maxX) * 0.5f;
    const float worldCenterY = (minY + maxY) * 0.5f;

    auto map = [&](Vector2 w) -> Vector2 {
        return {
            area.x + area.width * 0.5f + (w.x - worldCenterX) * scale,
            area.y + area.height * 0.5f + (w.y - worldCenterY) * scale
        };
    };

    // Course lines
    for (size_t i = 0; i < course_.size(); ++i) {
        const CourseCheckpoint& cp = course_[i];

        const Vector2 a = map(cp.a);
        const Vector2 b = map(cp.b);

        if (cp.type == CourseCheckpoint::Type::StartLine) {
            DrawLineEx(a, b, 2.5f, GREEN);
        } else if (cp.type == CourseCheckpoint::Type::FinishLine) {
            DrawLineEx(a, b, 2.5f, BLUE);
        } else if (cp.type == CourseCheckpoint::Type::Gate) {
            DrawLineEx(a, b, 1.5f, DARKGRAY);
        }
    }

    // Buoys
    for (const auto& [pos, radius] : buoys_) {
        (void)radius;
        const Vector2 p = map(pos);
        DrawCircleV(p, 3.5f, ORANGE);
    }

    // Highlight current target
    if (currentCheckpoint_ >= 0 &&
        currentCheckpoint_ < static_cast<int>(course_.size())) {

        const CourseCheckpoint& cp = course_[currentCheckpoint_];

    if (cp.type == CourseCheckpoint::Type::Mark) {
        const Vector2 p = map(cp.a);
        DrawCircleV(p, 5.5f, YELLOW);
        DrawCircleLines(static_cast<int>(p.x),
                        static_cast<int>(p.y),
                        6,
                        YELLOW);
    } else if (cp.type == CourseCheckpoint::Type::Gate) {
        const Vector2 a = map(cp.a);
        const Vector2 b = map(cp.b);

        DrawCircleV(a, 5.5f, YELLOW);
        DrawCircleV(b, 5.5f, YELLOW);

        DrawCircleLines(static_cast<int>(a.x),
                        static_cast<int>(a.y),
                        6,
                        YELLOW);
        DrawCircleLines(static_cast<int>(b.x),
                        static_cast<int>(b.y),
                        6,
                        YELLOW);
    } else if (cp.type == CourseCheckpoint::Type::StartLine) {
        DrawLineEx(map(cp.a), map(cp.b), 2.5f, YELLOW);
    } else if (cp.type == CourseCheckpoint::Type::FinishLine) {
        DrawLineEx(map(cp.a), map(cp.b), 2.5f, YELLOW);
    }
        }

        // Player boat
        const Vector2 bp = map({ boat_.worldX, boat_.worldY });
        const Vector2 fwd = headingToVec(boat_.heading);
        const Vector2 right = { fwd.y, -fwd.x };

        const Vector2 tip = {
            bp.x + fwd.x * 9.0f,
            bp.y + fwd.y * 9.0f
        };
        const Vector2 backLeft = {
            bp.x - fwd.x * 5.0f - right.x * 5.0f,
            bp.y - fwd.y * 5.0f - right.y * 5.0f
        };
        const Vector2 backRight = {
            bp.x - fwd.x * 5.0f + right.x * 5.0f,
            bp.y - fwd.y * 5.0f + right.y * 5.0f
        };

        DrawTriangle(tip, backRight, backLeft, WHITE);
        DrawTriangleLines(tip, backLeft, backRight, BLACK);
}

void Game::drawWindArrow() {
    const Vector2 center = { 640.0f, 60.0f };
    const Vector2 windDir = headingToVec(wind_.trueAngle);

    DrawLineEx(
        center,
        { center.x + windDir.x * 60.0f, center.y + windDir.y * 60.0f },
        3.0f,
        BLUE
    );

    DrawCircleV(center, 4.0f, BLUE);
    DrawText(TextFormat("TWD %.0f deg", wind_.trueAngle), 540, 20, 16, DARKGRAY);
}

void Game::drawOverlay() {
    Rectangle uiRect = { 20, 480, 260, 220 };
    DrawRectangleRounded(uiRect, 0.1f, 8, uiColor(15, 20, 30, 240));

    DrawText(TextFormat("SPEED: %.1f kts", boat_.speed),
             40, 500, 22, GREEN);
    DrawText(TextFormat("VMG:   %.1f kts", std::fabs(boat_.dispVMG)),
             40, 530, 22, boat_.dispVMG > 0.0f ? SKYBLUE : ORANGE);
    DrawText(TextFormat("TWA:   %.0f deg", boat_.dispTWA),
             40, 560, 20, WHITE);

    DrawText(TextFormat("VANG: [%s]", boat_.vangOn ? "ON" : "OFF"),
             40, 590, 16, boat_.vangOn ? LIME : GRAY);
    DrawText(TextFormat("CUNN: [%s]", boat_.cunningOn ? "ON" : "OFF"),
             140, 590, 16, boat_.cunningOn ? LIME : GRAY);

    DrawText("OUTHAUL", 40, 610, 12, GRAY);
    constexpr int barX = 40;
    constexpr int barY = 625;
    constexpr int barWidth = 120;
    constexpr int barHeight = 12;

    const float idealOuthaul = clampf(
        1.0f - (boat_.dispTWA - 30.0f) / 150.0f,
                                      0.0f,
                                      1.0f
    );
    const float outhaulError = std::fabs(boat_.outhaul - idealOuthaul);
    const float outhaulEff = clampf(1.0f - outhaulError * outhaulError * 10.0f,
                                    0.0f, 1.0f);

    DrawRectangle(barX, barY, barWidth, barHeight, uiColor(30, 30, 30, 255));
    DrawRectangle(
        barX,
        barY,
        static_cast<int>(std::round(barWidth * boat_.outhaul)),
                  barHeight,
                  outhaulEff > 0.85f ? LIME : ORANGE
    );

    const int markerX = barX + static_cast<int>(std::round(barWidth * idealOuthaul));
    DrawLine(markerX, barY - 3, markerX, barY + barHeight + 1, DARKGRAY);

    DrawText(TextFormat("%.2f", boat_.outhaul),
             barX + barWidth + 8,
             barY - 2,
             12,
             DARKGRAY);

    const float trimEff = sailEffectiveness(boat_.dispTWA,
                                            boat_.boomSheet,
                                            boat_.outhaul);
    DrawText("FLOW", 40, 655, 12, GRAY);
    DrawRectangle(40, 670, 100, 12, uiColor(30, 30, 30, 255));
    DrawRectangle(
        40,
        670,
        static_cast<int>(100 * trimEff),
                  12,
                  trimEff > 0.85f ? LIME : RED
    );

    DrawText("V: Vang  C: Cunn", 40, 692, 12, DARKGRAY);
    DrawText("W/S: Sheet  Q/E: Outhaul", 40, 708, 12, DARKGRAY);
}

void Game::drawWindex() {
    constexpr int cx = 240;
    constexpr int cy = 540;

    DrawCircleLines(cx, cy, 30, GRAY);

    const float relAw = std::atan2(boat_.awFrom.x, -boat_.awFrom.y) * RAD2DEG -
    boat_.heading;

    const Vector2 tip = {
        cx + std::sin(relAw * DEG2RAD) * 25.0f,
        cy - std::cos(relAw * DEG2RAD) * 25.0f
    };

    DrawLineEx({ static_cast<float>(cx), static_cast<float>(cy) },
               tip,
               2.0f,
               RED);
    DrawCircleV(tip, 3.0f, RED);
}

void Game::drawCourse() {
    if (course_.empty()) {
        return;
    }

    drawMarkRoomIndicators();

    for (size_t i = 0; i < course_.size(); ++i) {
        const CourseCheckpoint& cp = course_[i];

        const Vector2 sa = worldToScreen(cp.a);
        const Vector2 sb = worldToScreen(cp.b);

        const bool isCurrent = static_cast<int>(i) == currentCheckpoint_;

        if (cp.type == CourseCheckpoint::Type::StartLine) {
            DrawLineEx(sa, sb, 7.0f, GREEN);
        } else if (cp.type == CourseCheckpoint::Type::FinishLine) {
            DrawLineEx(sa, sb, 7.0f, BLUE);
        } else if (cp.type == CourseCheckpoint::Type::Gate) {
            DrawLineEx(sa, sb, 3.0f, DARKGRAY);

            DrawCircleV(sa, 18.0f, isCurrent ? YELLOW : ORANGE);
            DrawCircleLines(static_cast<int>(sa.x), static_cast<int>(sa.y),
                            18.0f, BLACK);

            DrawCircleV(sb, 18.0f, isCurrent ? YELLOW : ORANGE);
            DrawCircleLines(static_cast<int>(sb.x), static_cast<int>(sb.y),
                            18.0f, BLACK);
        } else if (cp.type == CourseCheckpoint::Type::Mark) {
            DrawCircleV(sa, 20.0f, isCurrent ? YELLOW : ORANGE);
            DrawCircleLines(static_cast<int>(sa.x), static_cast<int>(sa.y),
                            20.0f, BLACK);
        }
    }

    drawStartFinishObjects();
}

void Game::drawStartFinishObjects() {
    drawMarkerBuoy(startBuoyPos_, ORANGE, ORANGE, "START");
    drawMarkBoat(startBoatPos_, wind_.trueAngle, DARKGRAY, ORANGE, "RC");

    drawMarkerBuoy(finishBuoyPos_, BLUE, BLUE, "FINISH");
    drawMarkBoat(finishBoatPos_, wind_.trueAngle, LIGHTGRAY, BLUE, "RC");
}

void Game::drawMarkRoomIndicators() {
    constexpr float kMarkRoomRadius = 70.0f;

    for (const auto& [pos, unusedRadius] : buoys_) {
        (void)unusedRadius;

        const Vector2 screenPos = worldToScreen(pos);

        DrawCircleV(screenPos, kMarkRoomRadius, Fade(YELLOW, 0.06f));
        DrawCircleLines(static_cast<int>(screenPos.x),
                        static_cast<int>(screenPos.y),
                        static_cast<int>(kMarkRoomRadius),
                        Fade(YELLOW, 0.25f));
    }
}

void Game::drawOffscreenArrow() {
    if (!raceMode_ || course_.empty()) {
        return;
    }

    const Vector2 target = nextTargetPosition();
    const Vector2 targetScreen = worldToScreen(target);

    if (isTargetVisible(targetScreen)) {
        return;
    }

    const Vector2 screenCenter = { 640.0f, 360.0f };
    Vector2 dir = {
        targetScreen.x - screenCenter.x,
        targetScreen.y - screenCenter.y
    };

    const float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len < 1e-4f) {
        return;
    }

    dir.x /= len;
    dir.y /= len;

    const float edgeX = clampf(targetScreen.x, 60.0f, 1220.0f);
    const float edgeY = clampf(targetScreen.y, 60.0f, 660.0f);
    const Vector2 edge = { edgeX, edgeY };

    DrawCircleV(edge, 14.0f, YELLOW);
    DrawCircleLines(static_cast<int>(edge.x),
                    static_cast<int>(edge.y),
                    14,
                    BLACK);

    const Vector2 tip = {
        edge.x + dir.x * 30.0f,
        edge.y + dir.y * 30.0f
    };
    const Vector2 base1 = {
        edge.x - dir.y * 12.0f,
        edge.y + dir.x * 12.0f
    };
    const Vector2 base2 = {
        edge.x + dir.y * 12.0f,
        edge.y - dir.x * 12.0f
    };

    DrawTriangle(tip, base1, base2, YELLOW);
}

void Game::drawFlagAbove(Vector2 screenBase, Color flagColor,
                         const char* label) const {
                             DrawLineEx(screenBase,
                                        { screenBase.x, screenBase.y - 44.0f },
                                        3.0f,
                                        DARKGRAY);

                             Rectangle flagRect = {
                                 screenBase.x - 20.0f,
                                 screenBase.y - 64.0f,
                                 40.0f,
                                 24.0f
                             };

                             DrawRectangleRec(flagRect, flagColor);
                             DrawRectangleLinesEx(flagRect, 1.5f, BLACK);

                             const int fontSize = (std::strlen(label) > 6) ? 12 : 14;
                             const int textWidth = MeasureText(label, fontSize);
                             const int textX = static_cast<int>(flagRect.x +
                             (flagRect.width - textWidth) * 0.5f);
                             const int textY = static_cast<int>(flagRect.y + 5);

                             const Color textColor = (flagColor.r < 60 &&
                             flagColor.g < 60 &&
                             flagColor.b < 60) ? WHITE : BLACK;

                             DrawText(label, textX, textY, fontSize, textColor);
                         }

                         void Game::drawFlagIcon(Vector2 screenBase, Color flagColor,
                                                 const char* label) const {
                                                     DrawLineEx(screenBase,
                                                                { screenBase.x, screenBase.y - 28.0f },
                                                                2.5f,
                                                                DARKGRAY);

                                                     Rectangle flagRect = {
                                                         screenBase.x,
                                                         screenBase.y - 28.0f,
                                                         26.0f,
                                                         16.0f
                                                     };

                                                     DrawRectangleRec(flagRect, flagColor);
                                                     DrawRectangleLinesEx(flagRect, 1.0f, BLACK);

                                                     const Color textColor = (flagColor.r < 60 &&
                                                     flagColor.g < 60 &&
                                                     flagColor.b < 60) ? WHITE : BLACK;

                                                     DrawText(label,
                                                              static_cast<int>(flagRect.x + 3),
                                                              static_cast<int>(flagRect.y + 2),
                                                              10,
                                                              textColor);
                                                 }

                                                 void Game::drawMarkerBuoy(Vector2 worldPos, Color buoyColor,
                                                                           Color flagColor, const char* label) {
                                                     const Vector2 p = worldToScreen(worldPos);

                                                     DrawCircleV(p, 18.0f, buoyColor);
                                                     DrawCircleLines(static_cast<int>(p.x), static_cast<int>(p.y),
                                                                     18.0f, BLACK);

                                                     drawFlagAbove(p, flagColor, label);
                                                                           }

                                                                           void Game::drawMarkBoat(Vector2 worldPos, float headingDeg,
                                                                                                   Color hullColor, Color flagColor,
                                                                                                   const char* label) {
                                                                               const Vector2 p = worldToScreen(worldPos);
                                                                               const Vector2 fwd = headingToVec(headingDeg);
                                                                               const Vector2 right = { fwd.y, -fwd.x };

                                                                               const Vector2 p1 = {
                                                                                   p.x + fwd.x * 42.0f,
                                                                                   p.y + fwd.y * 42.0f
                                                                               };
                                                                               const Vector2 p2 = {
                                                                                   p.x - fwd.x * 24.0f - right.x * 18.0f,
                                                                                   p.y - fwd.y * 24.0f - right.y * 18.0f
                                                                               };
                                                                               const Vector2 p3 = {
                                                                                   p.x - fwd.x * 24.0f + right.x * 18.0f,
                                                                                   p.y - fwd.y * 24.0f + right.y * 18.0f
                                                                               };

                                                                               DrawTriangle(p1, p3, p2, hullColor);
                                                                               DrawTriangleLines(p1, p2, p3, BLACK);

                                                                               drawFlagAbove(p, flagColor, label);
                                                                                                   }

                                                                                                   void Game::drawRaceInfo() {
                                                                                                       if (!raceMode_) {
                                                                                                           return;
                                                                                                       }

                                                                                                       DrawText("RACE MODE", 20, 60, 20, RED);

                                                                                                       if (bfd_) {
                                                                                                           DrawText("BFD", 20, 85, 20, RED);
                                                                                                       } else if (prestartActive_) {
                                                                                                           const int minutes = static_cast<int>(prestartTime_) / 60;
                                                                                                           const int seconds = static_cast<int>(prestartTime_) % 60;

                                                                                                           DrawText(TextFormat("PRE-START: %02d:%02d", minutes, seconds),
                                                                                                                    20, 85, 20, RED);
                                                                                                       } else if (raceFinished_) {
                                                                                                           DrawText("FINISHED", 20, 85, 20, GREEN);
                                                                                                       } else {
                                                                                                           DrawText(TextFormat("TIME: %.1f s", raceTime_),
                                                                                                                    20, 85, 20, WHITE);
                                                                                                       }

                                                                                                       /*DrawText(TextFormat("PENALTIES: %d", pendingPenalties_),
                                                                                                                // 20, 110, 16, ORANGE);*/

                                                                                                       drawResultsPanel();
                                                                                                   }

                                                                                                   void Game::drawResultsPanel() {
                                                                                                       if (!raceFinished_) {
                                                                                                           return;
                                                                                                       }

                                                                                                       Rectangle panel = { 490.0f, 270.0f, 300.0f, 180.0f };
                                                                                                       DrawRectangleRounded(panel, 0.1f, 10, uiColor(15, 20, 30, 240));

                                                                                                       if (bfd_) {
                                                                                                           DrawText("RACE FINISHED", 540, 290, 20, GREEN);
                                                                                                           DrawText("BFD", 610, 350, 48, RED);
                                                                                                           DrawText("Saved to leaderboard.txt", 520, 410, 14, LIGHTGRAY);
                                                                                                       } else {
                                                                                                           DrawText("RACE FINISHED", 540, 290, 20, GREEN);

                                                                                                           DrawText(TextFormat("Elapsed: %.2f s", raceTime_),
                                                                                                                    520, 340, 20, WHITE);

                                                                                                           DrawText(TextFormat("Penalties: %d", pendingPenalties_),
                                                                                                                    520, 370, 18, ORANGE);

                                                                                                           DrawText("Saved to leaderboard.txt", 520, 410, 14, LIGHTGRAY);
                                                                                                       }
                                                                                                   }

                                                                                                   void Game::drawRaceFlagsPanel() {
                                                                                                       if (!raceMode_) {
                                                                                                           return;
                                                                                                       }

                                                                                                       Rectangle panel = { 970.0f, 20.0f, 290.0f, 135.0f };
                                                                                                       DrawRectangleRounded(panel, 0.1f, 8, uiColor(15, 20, 30, 230));

                                                                                                       DrawText("RACE FLAGS", 990, 28, 16, WHITE);

                                                                                                       const FlagPhase phase = currentFlagPhase();

                                                                                                       const bool classUp = prestartActive_ && prestartTime_ > 0.0f;
                                                                                                       const bool blackUp = phase == FlagPhase::PrepUp;

                                                                                                       drawFlagIcon({ 1015.0f, 112.0f }, ORANGE, "4.7");
                                                                                                       drawFlagIcon({ 1155.0f, 112.0f }, BLACK, "BLACK");

                                                                                                       DrawText(TextFormat("4.7:    %s", classUp ? "UP" : "DOWN"),
                                                                                                                1060, 70, 16, classUp ? LIME : GRAY);

                                                                                                       DrawText(TextFormat("Black: %s", blackUp ? "UP" : "DOWN"),
                                                                                                                1060, 95, 16, blackUp ? LIME : GRAY);

                                                                                                       const char* stateText = "RACING";
                                                                                                       if (prestartActive_) {
                                                                                                           stateText = "PRE-START";
                                                                                                       } else if (raceFinished_) {
                                                                                                           stateText = "FINISHED";
                                                                                                       }

                                                                                                       DrawText(stateText, 1060, 45, 16, WHITE);
                                                                                                   }
