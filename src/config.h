#pragma once
#include <Arduino.h>

#define DEFAULT_TOKEN_URL "https://platform.claude.com/v1/oauth/token"
#define DEFAULT_CLIENT_ID "9d1c250a-e61b-44d9-88ed-5944d1962f5e"
#define DEFAULT_TZ "CET-1CEST,M3.5.0,M10.5.0/3"
#define DEFAULT_INTERVAL_SEC 300
#define MIN_INTERVAL_SEC 60

struct Config {
    String ssid;
    String wifiPass;
    String accessToken;
    String refreshToken;
    uint64_t expiresAt = 0;  // Unix-Sekunden, 0 = unbekannt
    String tokenUrl = DEFAULT_TOKEN_URL;
    String clientId = DEFAULT_CLIENT_ID;
    String tz = DEFAULT_TZ;
    uint32_t intervalSec = DEFAULT_INTERVAL_SEC;
    uint8_t brightness = 128;
};

void configLoad(Config& cfg);
void configSave(const Config& cfg);
// Schreibt nur die OAuth-Tokens (nach einem Refresh), um NVS-Schreibzugriffe klein zu halten.
void configSaveTokens(const Config& cfg);
void configSaveBrightness(const Config& cfg);
void configErase();
