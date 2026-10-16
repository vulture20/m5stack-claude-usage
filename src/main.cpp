#include <M5Unified.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "claude_api.h"
#include "portal.h"
#include "ui.h"

static Config cfg;
static UsageData usage;
static String status = "Starte ...";
static bool statusErr = false;
static uint32_t nextFetchMs = 0;
static uint32_t lastDrawMs = 0;
static uint32_t rateBackoffSec = 0;

static const uint8_t BRIGHTNESS_STEPS[] = {30, 80, 128, 200, 255};

static bool connectWiFi(uint32_t timeoutMs) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(cfg.ssid.c_str(), cfg.wifiPass.c_str());
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) delay(200);
    return WiFi.status() == WL_CONNECTED;
}

static void enterPortal(uint32_t timeoutMs) {
    WiFi.disconnect(true);
    portal::run(cfg, timeoutMs);
    // Abbruch oder Timeout ohne Speichern: mit der bisherigen Konfiguration neu starten
    ESP.restart();
}

static String clockString() {
    time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    char buf[8];
    strftime(buf, sizeof(buf), "%H:%M", &lt);
    return buf;
}

static void redraw() {
    ui::drawUsage(usage, status, statusErr);
    lastDrawMs = millis();
}

static void fetch() {
    status = "Aktualisiere ...";
    statusErr = false;
    redraw();

    ApiResult r = claude::fetchUsage(cfg, usage);
    uint32_t nextSec = cfg.intervalSec;
    switch (r.status) {
        case ApiStatus::Ok:
            rateBackoffSec = 0;
            status = "Stand " + clockString() + "   A: Neu  B lang: Setup";
            statusErr = false;
            break;
        case ApiStatus::RateLimited:
            rateBackoffSec = rateBackoffSec ? min<uint32_t>(rateBackoffSec * 2, 1800) : 120;
            nextSec = max(rateBackoffSec, r.retryAfterSec);
            status = "Rate-Limit, neuer Versuch in " + String(nextSec / 60) + " min";
            statusErr = true;
            break;
        case ApiStatus::AuthFailed:
            nextSec = 1800;
            status = "Anmeldung ungueltig - B lang: Setup";
            statusErr = true;
            break;
        default:
            nextSec = 60;
            status = r.http > 0 ? "Fehler HTTP " + String(r.http) : "Netzwerkfehler";
            statusErr = true;
            break;
    }
    Serial.printf("[main] Abfrage: status=%d http=%d %s\n", (int)r.status, r.http, r.msg.c_str());
    nextFetchMs = millis() + nextSec * 1000UL;
    redraw();
}

void setup() {
    Serial.begin(115200);
    auto mc = M5.config();
    M5.begin(mc);
    configLoad(cfg);
    M5.Display.setBrightness(cfg.brightness);
    ui::begin();

    M5.update();
    if (cfg.ssid.isEmpty() || M5.BtnB.isPressed()) enterPortal(0);

    ui::showMessage("Verbinde mit WLAN", cfg.ssid, "B lang: Setup");
    if (!connectWiFi(20000)) {
        // WLAN weg oder falsch konfiguriert: Setup fuer 5 Minuten anbieten, dann neu versuchen
        Serial.println("[main] WLAN-Verbindung fehlgeschlagen, starte Setup-Portal");
        enterPortal(5 * 60 * 1000UL);
    }
    Serial.printf("[main] Verbunden, IP %s\n", WiFi.localIP().toString().c_str());

    ui::showMessage("Verbunden", WiFi.localIP().toString(), "Synchronisiere Uhrzeit ...");
    configTzTime(cfg.tz.c_str(), "pool.ntp.org", "time.google.com");
    // TLS-Zertifikatspruefung braucht eine gueltige Uhrzeit
    for (int i = 0; i < 100 && time(nullptr) < 1700000000; i++) delay(100);

    nextFetchMs = millis();
}

void loop() {
    M5.update();

    if (M5.BtnB.pressedFor(2000)) enterPortal(10 * 60 * 1000UL);
    if (M5.BtnA.wasClicked()) nextFetchMs = millis();
    if (M5.BtnC.wasClicked()) {
        size_t i = 0;
        while (i < sizeof(BRIGHTNESS_STEPS) && BRIGHTNESS_STEPS[i] <= cfg.brightness) i++;
        cfg.brightness = BRIGHTNESS_STEPS[i % sizeof(BRIGHTNESS_STEPS)];
        M5.Display.setBrightness(cfg.brightness);
        configSaveBrightness(cfg);
    }

    if (WiFi.status() != WL_CONNECTED) {
        if (!statusErr || status != "WLAN getrennt ...") {
            status = "WLAN getrennt ...";
            statusErr = true;
            redraw();
        }
        nextFetchMs = millis();  // nach dem Reconnect sofort abfragen
    } else if ((int32_t)(millis() - nextFetchMs) >= 0) {
        fetch();
    }

    // Countdown und Uhr aktuell halten
    if (millis() - lastDrawMs > 30000) redraw();
    delay(20);
}
