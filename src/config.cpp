#include "config.h"
#include <Preferences.h>

static const char* NS = "cusage";

void configLoad(Config& cfg) {
    Preferences p;
    p.begin(NS, true);
    cfg.ssid = p.getString("ssid", "");
    cfg.wifiPass = p.getString("wpass", "");
    cfg.accessToken = p.getString("atok", "");
    cfg.refreshToken = p.getString("rtok", "");
    cfg.expiresAt = p.getULong64("exp", 0);
    cfg.tokenUrl = p.getString("turl", DEFAULT_TOKEN_URL);
    cfg.clientId = p.getString("cid", DEFAULT_CLIENT_ID);
    cfg.tz = p.getString("tz", DEFAULT_TZ);
    cfg.intervalSec = p.getUInt("intv", DEFAULT_INTERVAL_SEC);
    cfg.brightness = p.getUChar("bri", 128);
    p.end();
    if (cfg.intervalSec < MIN_INTERVAL_SEC) cfg.intervalSec = MIN_INTERVAL_SEC;
}

void configSave(const Config& cfg) {
    Preferences p;
    p.begin(NS, false);
    p.putString("ssid", cfg.ssid);
    p.putString("wpass", cfg.wifiPass);
    p.putString("turl", cfg.tokenUrl);
    p.putString("cid", cfg.clientId);
    p.putString("tz", cfg.tz);
    p.putUInt("intv", cfg.intervalSec);
    p.end();
    configSaveTokens(cfg);
}

void configSaveTokens(const Config& cfg) {
    Preferences p;
    p.begin(NS, false);
    p.putString("atok", cfg.accessToken);
    p.putString("rtok", cfg.refreshToken);
    p.putULong64("exp", cfg.expiresAt);
    p.end();
}

void configSaveBrightness(const Config& cfg) {
    Preferences p;
    p.begin(NS, false);
    p.putUChar("bri", cfg.brightness);
    p.end();
}

void configErase() {
    Preferences p;
    p.begin(NS, false);
    p.clear();
    p.end();
}
