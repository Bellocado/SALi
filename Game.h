#pragma once

#include "raylib.h"
#include "Boat.h"
#include "Wind.h"

#include <utility>
#include <vector>

struct CourseCheckpoint {
    enum class Type {
        StartLine,
        Mark,
        Gate,
        FinishLine
    };

    Type type = Type::Mark;
    Vector2 a = { 0.0f, 0.0f };
    Vector2 b = { 0.0f, 0.0f };
    float radius = 30.0f;
    Vector2 passDir = { 0.0f, 0.0f };
};

class Game {
public:
    Game() = default;

    void Update(float dt);
    void Draw();
    bool ShouldClose() const { return shouldClose_; }

private:
    enum class State {
        Menu,
        Setup,
        Play,
        Credits
    };

    enum class FlagPhase {
        None,
        ClassUp,
        PrepUp,
        PrepDown
    };

    bool markLineACrossed_ = false;

    bool crossLineCheck(Vector2 prev, Vector2 curr, Vector2 lineMid, Vector2 passDir, Vector2 tangent, float halfLen) const;

    State state_ = State::Menu;
    Boat boat_;
    Wind wind_;

    bool shouldClose_ = false;
    bool raceMode_ = false;

    // Race state
    bool prestartActive_ = false;
    bool raceStarted_ = false;
    bool raceFinished_ = false;
    bool raceFinishedReported_ = false;
    bool bfd_ = false;
    int pendingPenalties_ = 0;
    float penaltyTurnAccum_ = 0.0f;
    float lastPenaltyHeading_ = 0.0f;

    float prestartTime_ = 60.0f;
    float raceTime_ = 0.0f;

    float collisionCooldown_ = 0.0f;

    int currentCheckpoint_ = 0;
    int prestartMinutes_ = 5;

    Vector2 prevBoatPos_ = { 0.0f, 0.0f };

    std::vector<CourseCheckpoint> course_;
    std::vector<std::pair<Vector2, float>> buoys_;

    Vector2 startBuoyPos_ = { 0.0f, 0.0f };
    Vector2 startBoatPos_ = { 0.0f, 0.0f };
    Vector2 finishBuoyPos_ = { 0.0f, 0.0f };
    Vector2 finishBoatPos_ = { 0.0f, 0.0f };

    char angleBuf_[32] = "0";
    char speedBuf_[32] = "12";

    bool angleActive_ = false;
    bool speedActive_ = false;
    float caretTimer_ = 0.0f;

    inline static const Rectangle kDirRect = { 420.0f, 220.0f, 220.0f, 28.0f };
    inline static const Rectangle kSpdRect = { 420.0f, 270.0f, 220.0f, 28.0f };

    void handleBoatControls(float dt);
    void handleSetupInput(float dt);

    void startGame();
    void resetRace();
    void generateCourse();
    void saveRaceResult();

    void drawMinimap();

    void checkRaceProgress();
    void checkEarlyStart();
    void handleBuoyCollisions();

    FlagPhase currentFlagPhase() const;
    Vector2 worldToScreen(Vector2 world) const;
    Vector2 nextTargetPosition() const;
    bool isTargetVisible(Vector2 screenPos) const;

    void drawMenu();
    void drawSetup();
    void drawPlay();
    void drawCredits();

    void drawGrid();
    void drawBoat();
    void drawWindArrow();
    void drawOverlay();
    void drawWindex();
    void drawCourse();
    void drawStartFinishObjects();
    void drawMarkRoomIndicators();
    void drawOffscreenArrow();
    void drawRaceInfo();
    void drawResultsPanel();
    void drawRaceFlagsPanel();

    void drawFlagAbove(Vector2 screenBase, Color flagColor, const char* label) const;
    void drawFlagIcon(Vector2 screenBase, Color flagColor, const char* label) const;
    void drawMarkerBuoy(Vector2 worldPos, Color buoyColor, Color flagColor, const char* label);
    void drawMarkBoat(Vector2 worldPos, float headingDeg, Color hullColor, Color flagColor, const char* label);

    float distanceToSegment(Vector2 p, Vector2 a, Vector2 b) const;
};
