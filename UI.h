#pragma once

#include "raylib.h"

void SetVirtualMouseTransform(float offsetX, float offsetY, float scale);
Vector2 GetVirtualMousePosition();

bool button(Rectangle r, const char* text, int fontSize = 20);
void drawTextField(Rectangle rec, const char* text, bool active, float caretTimer);
bool clickedOn(Rectangle rec);
