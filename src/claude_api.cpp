#include "claude_api.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "root_ca.h"

// Nicht dokumentierter Endpunkt, den Claude Code fuer /usage verwendet.
static const char* USAGE_URL = "https://api.anthropic.com/api/oauth/usage";
static const char* OAUTH_BETA = "oauth-2025-04-20";

struct HttpResp {
    int code;
    String body;
    uint32_t retryAfter;
};

static HttpResp doRequest(const String& url, const String& bearer, const String& postBody) {
    WiFiClientSecure client;
    client.setCACert(ROOT_CA_BUNDLE);
    HTTPClient http;
    http.setTimeout(15000);
    http.setReuse(false);
    if (!http.begin(client, url)) return {-1, "begin() fehlgeschlagen", 0};
    http.addHeader("Accept", "application/json");
    http.addHeader("User-Agent", "m5stack-claude-usage/1.0");
    if (bearer.length()) {
        http.addHeader("Authorization", "Bearer " + bearer);
        http.addHeader("anthropic-beta", OAUTH_BETA);
    }
    const char* hdrs[] = {"retry-after"};
    http.collectHeaders(hdrs, 1);
    int code;
    if (postBody.length()) {
        http.addHeader("Content-Type", "application/json");
        code = http.POST(postBody);
    } else {
        code = http.GET();
    }
    HttpResp r{code, code > 0 ? http.getString() : HTTPClient::errorToString(code),
               (uint32_t)http.header("retry-after").toInt()};
    http.end();
    return r;
}

// Fehlertext aus {"error":{"message":..}} bzw. {"error":"..","error_description":".."}
static String errorText(const HttpResp& r) {
    if (r.code < 0) return r.body;
    JsonDocument doc;
    if (deserializeJson(doc, r.body) == DeserializationError::Ok) {
        if (doc["error"]["message"].is<const char*>()) return doc["error"]["message"].as<String>();
        if (doc["error_description"].is<const char*>()) return doc["error_description"].as<String>();
        if (doc["error"].is<const char*>()) return doc["error"].as<String>();
    }
    return "HTTP " + String(r.code);
}

static ApiResult statusFromHttp(const HttpResp& r) {
    ApiResult res;
    res.http = r.code;
    res.retryAfterSec = r.retryAfter;
    res.msg = errorText(r);
    if (r.code < 0) res.status = ApiStatus::NetError;
    else if (r.code == 429) res.status = ApiStatus::RateLimited;
    else if (r.code == 400 || r.code == 401 || r.code == 403) res.status = ApiStatus::AuthFailed;
    else res.status = ApiStatus::HttpError;
    return res;
}

static ApiResult refreshTokens(Config& cfg) {
    if (!cfg.refreshToken.length()) return {ApiStatus::AuthFailed, 0, 0, "Kein Refresh-Token"};

    JsonDocument req;
    req["grant_type"] = "refresh_token";
    req["refresh_token"] = cfg.refreshToken;
    req["client_id"] = cfg.clientId;
    String body;
    serializeJson(req, body);

    Serial.println("[api] Erneuere Access-Token");
    HttpResp r = doRequest(cfg.tokenUrl, "", body);
    if (r.code != 200) {
        ApiResult res = statusFromHttp(r);
        Serial.printf("[api] Refresh fehlgeschlagen: %d %s\n", r.code, res.msg.c_str());
        return res;
    }

    JsonDocument doc;
    if (deserializeJson(doc, r.body) != DeserializationError::Ok || !doc["access_token"].is<const char*>())
        return {ApiStatus::ParseError, r.code, 0, "Token-Antwort unlesbar"};

    cfg.accessToken = doc["access_token"].as<String>();
    // Der Refresh-Token rotiert: der alte ist ab jetzt ungueltig.
    if (doc["refresh_token"].is<const char*>()) cfg.refreshToken = doc["refresh_token"].as<String>();
    uint32_t expiresIn = doc["expires_in"] | 0;
    time_t now = time(nullptr);
    cfg.expiresAt = (expiresIn && now > 1700000000) ? (uint64_t)now + expiresIn : 0;
    configSaveTokens(cfg);
    Serial.printf("[api] Token erneuert, gueltig fuer %u s\n", expiresIn);
    return {ApiStatus::Ok, r.code, 0, ""};
}

static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

// "2025-10-16T22:00:00.123456+00:00" bzw. "...Z" -> Unix-Zeit (UTC)
static time_t parseIso8601(const char* s) {
    if (!s) return 0;
    int Y, M, D, h, m, sec;
    if (sscanf(s, "%4d-%2d-%2dT%2d:%2d:%2d", &Y, &M, &D, &h, &m, &sec) != 6) return 0;
    const char* p = s + 19;
    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p)) p++;
    }
    long off = 0;
    if (*p == '+' || *p == '-') {
        int oh = 0, om = 0;
        sscanf(p + 1, "%2d:%2d", &oh, &om);
        off = (oh * 3600L + om * 60L) * (*p == '-' ? -1 : 1);
    }
    return (time_t)(daysFromCivil(Y, M, D) * 86400 + h * 3600L + m * 60L + sec - off);
}

static String labelFor(const char* key) {
    if (!strcmp(key, "five_hour")) return "Sitzung (5h)";
    if (!strcmp(key, "seven_day")) return "Woche (alle Modelle)";
    if (!strcmp(key, "seven_day_opus")) return "Woche Opus";
    if (!strcmp(key, "seven_day_sonnet")) return "Woche Sonnet";
    if (!strcmp(key, "seven_day_oauth_apps")) return "Woche OAuth-Apps";
    if (!strcmp(key, "extra_usage")) return "Extra-Nutzung";
    String s = key;
    s.replace('_', ' ');
    return s;
}

// Nimmt jedes Objekt der Antwort, das ein numerisches "utilization" hat. So bleiben
// neue Limit-Typen ohne Codeaenderung sichtbar; null-Eintraege werden uebersprungen.
static bool parseUsage(const String& body, UsageData& out) {
    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok || !doc.is<JsonObject>()) return false;
    UsageData d;
    for (JsonPair kv : doc.as<JsonObject>()) {
        if (d.count >= MAX_BARS) break;
        JsonVariant v = kv.value();
        if (!v.is<JsonObject>() || !v["utilization"].is<float>()) continue;
        UsageBar& b = d.bars[d.count++];
        b.label = labelFor(kv.key().c_str());
        b.pct = v["utilization"].as<float>();
        b.resetsAt = parseIso8601(v["resets_at"] | (const char*)nullptr);
    }
    d.fetchedAt = time(nullptr);
    d.valid = true;
    out = d;
    return true;
}

namespace claude {

ApiResult fetchUsage(Config& cfg, UsageData& out) {
    time_t now = time(nullptr);
    bool timeOk = now > 1700000000;
    bool canRefresh = cfg.refreshToken.length() > 0;

    if (canRefresh && (!cfg.accessToken.length() ||
                       (timeOk && cfg.expiresAt && (uint64_t)now + 300 >= cfg.expiresAt))) {
        ApiResult rr = refreshTokens(cfg);
        if (rr.status != ApiStatus::Ok) return rr;
    }
    if (!cfg.accessToken.length()) return {ApiStatus::AuthFailed, 0, 0, "Kein Token konfiguriert"};

    HttpResp r = doRequest(USAGE_URL, cfg.accessToken, "");
    if (r.code == 401 && canRefresh) {
        ApiResult rr = refreshTokens(cfg);
        if (rr.status != ApiStatus::Ok) return rr;
        r = doRequest(USAGE_URL, cfg.accessToken, "");
    }
    if (r.code != 200) {
        ApiResult res = statusFromHttp(r);
        Serial.printf("[api] Usage-Abfrage fehlgeschlagen: %d %s\n", r.code, res.msg.c_str());
        return res;
    }
    if (!parseUsage(r.body, out)) {
        Serial.printf("[api] Unerwartete Antwort: %.200s\n", r.body.c_str());
        return {ApiStatus::ParseError, r.code, 0, "Antwort unlesbar"};
    }
    return {ApiStatus::Ok, r.code, 0, ""};
}

}  // namespace claude
