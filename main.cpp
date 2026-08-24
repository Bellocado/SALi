#include "raylib.h"
#include "Game.h"
#include "UI.h"
#include <cmath>

int main() {
    constexpr int kVirtualWidth = 1280;
    constexpr int kVirtualHeight = 720;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(kVirtualWidth, kVirtualHeight, "SALi - ILCA 4");

    SetWindowMinSize(960, 540);
    SetTargetFPS(60);

    RenderTexture2D target = LoadRenderTexture(kVirtualWidth, kVirtualHeight);
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);

    Game game;

    while (!WindowShouldClose() && !game.ShouldClose()) {
        const float dt = GetFrameTime();
        const float scale = std::fmin(static_cast<float>(GetScreenWidth()) / kVirtualWidth, static_cast<float>(GetScreenHeight()) / kVirtualHeight);
        const float destX = (GetScreenWidth() - kVirtualWidth * scale) * 0.5f;
        const float destY = (GetScreenHeight() - kVirtualHeight * scale) * 0.5f;

        const float mouseScale = 1.0f / scale;
        const float mouseOffsetX = -destX * mouseScale;
        const float mouseOffsetY = -destY * mouseScale;
        SetVirtualMouseTransform(mouseOffsetX, mouseOffsetY, mouseScale);

        game.Update(dt);

        BeginTextureMode(target);
        {
            game.Draw();
        }
        EndTextureMode();

        const Rectangle dest = {
            destX,
            destY,
            kVirtualWidth * scale,
            kVirtualHeight * scale
        };

        BeginDrawing();
        {
            ClearBackground(BLACK);

            const Rectangle src = {
                0.0f,
                0.0f,
                static_cast<float>(kVirtualWidth),
                -static_cast<float>(kVirtualHeight)
            };

            DrawTexturePro(
                target.texture,
                src,
                dest,
                { 0.0f, 0.0f },
                0.0f,
                WHITE
            );
        }
        EndDrawing();
    }

    UnloadRenderTexture(target);
    CloseWindow();

    return 0;
}
