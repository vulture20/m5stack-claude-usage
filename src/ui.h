#pragma once
#include <Arduino.h>
#include "claude_api.h"

// Die eingebauten GFX-Fonts kennen nur ASCII: auf dem Display keine Umlaute verwenden.
namespace ui {
void begin();
void showMessage(const String& title, const String& line1, const String& line2 = "",
                 uint16_t titleColor = 0xFFFF);
void showPortal(const String& apSsid, const String& apPass, const String& ip);
void drawUsage(const UsageData& data, const String& status, bool statusIsError);
}
