#include "UI.h"

#include <cmath>

namespace {

    float gMouseOffsetX = 0.0f;
    float gMouseOffsetY = 0.0f;
    float gMouseScale = 1.0f;

    Color uiColor(int r, int g, int b, int a) {
        Color c;
        c.r = static_cast<unsigned char>(r);
        c.g = static_cast<unsigned char>(g);
        c.b = static_cast<unsigned char>(b);
        c.a = static_cast<unsigned char>(a);
        return c;
    }

} // namespace

void SetVirtualMouseTransform(float offsetX, float offsetY, float scale) {
    gMouseOffsetX = offsetX;
    gMouseOffsetY = offsetY;
    gMouseScale = scale;
}

Vector2 GetVirtualMousePosition() {
    const Vector2 physical = GetMousePosition();
    return {
        physical.x * gMouseScale + gMouseOffsetX,
        physical.y * gMouseScale + gMouseOffsetY
    };
}

bool button(Rectangle r, const char* text, int fontSize) {
    const Vector2 mouse = GetVirtualMousePosition();
    const bool hover = CheckCollisionPointRec(mouse, r);

    const Color fill = hover ? uiColor(200, 200, 200, 255)
    : uiColor(230, 230, 230, 255);

    DrawRectangleRec(r, fill);
    DrawRectangleLines(
        static_cast<int>(r.x),
                       static_cast<int>(r.y),
                       static_cast<int>(r.width),
                       static_cast<int>(r.height),
                       BLACK
    );

    const int textWidth = MeasureText(text, fontSize);
    const int tx = static_cast<int>(r.x + (r.width - textWidth) * 0.5f);
    const int ty = static_cast<int>(r.y + (r.height - fontSize) * 0.5f);
    DrawText(text, tx, ty, fontSize, BLACK);

    return hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

void drawTextField(Rectangle rec, const char* text, bool active, float caretTimer) {
    DrawRectangleRec(rec, uiColor(230, 230, 230, 255));
    DrawRectangleLines(
        static_cast<int>(rec.x),
                       static_cast<int>(rec.y),
                       static_cast<int>(rec.width),
                       static_cast<int>(rec.height),
                       BLACK
    );

    const int textX = static_cast<int>(rec.x + 6);
    const int textY = static_cast<int>(rec.y + 6);
    DrawText(text, textX, textY, 16, BLACK);

    if (active && std::fmod(caretTimer, 1.0f) < 0.5f) {
        const int caretX = textX + MeasureText(text, 16);
        DrawText("|", caretX, textY, 16, BLACK);
    }
}

bool clickedOn(Rectangle rec) {
    return IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
    CheckCollisionPointRec(GetVirtualMousePosition(), rec);
}
