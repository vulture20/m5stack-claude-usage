#pragma once
#include <Arduino.h>
#include <time.h>
#include "config.h"

#define MAX_BARS 4

struct UsageBar {
    String label;
    float pct = 0;         // 0..100
    time_t resetsAt = 0;   // UTC, 0 = unbekannt
};

struct UsageData {
    UsageBar bars[MAX_BARS];
    int count = 0;
    time_t fetchedAt = 0;
    bool valid = false;
};

enum class ApiStatus { Ok, AuthFailed, RateLimited, HttpError, NetError, ParseError };

struct ApiResult {
    ApiStatus status;
    int http;
    uint32_t retryAfterSec;
    String msg;
};

namespace claude {
// Holt /api/oauth/usage. Erneuert bei Bedarf (Ablauf oder 401) den Access-Token
// ueber den Refresh-Token und speichert die rotierten Tokens im NVS.
ApiResult fetchUsage(Config& cfg, UsageData& out);
}
