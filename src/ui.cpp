#include "ui.h"
#include <M5Unified.h>
#include <WiFi.h>

namespace ui {

static const int W = 320;
static const int HEADER_H = 24;
static const int FOOTER_Y = 224;

static uint16_t C_ACCENT, C_DIM, C_PANEL, C_BAR_BG, C_GREEN, C_AMBER, C_RED;

void begin() {
    auto& d = M5.Display;
    C_ACCENT = d.color565(217, 119, 87);
    C_DIM = d.color565(150, 150, 150);
    C_PANEL = d.color565(30, 30, 34);
    C_BAR_BG = d.color565(55, 55, 60);
    C_GREEN = d.color565(76, 175, 80);
    C_AMBER = d.color565(255, 193, 7);
    C_RED = d.color565(244, 67, 54);
    d.fillScreen(TFT_BLACK);
}

static uint16_t pctColor(float pct) {
    if (pct < 50) return C_GREEN;
    if (pct < 80) return C_AMBER;
    return C_RED;
}

static void drawHeader() {
    auto& d = M5.Display;
    d.fillRect(0, 0, W, HEADER_H, C_PANEL);
    d.setFont(&fonts::FreeSansBold9pt7b);
    d.setTextDatum(middle_left);
    d.setTextColor(C_ACCENT);
    d.drawString("Claude Usage", 6, HEADER_H / 2);

    // WLAN-Signal als vier Balken ganz rechts
    int bars = 0;
    if (WiFi.status() == WL_CONNECTED) {
        int rssi = WiFi.RSSI();
        bars = rssi > -55 ? 4 : rssi > -65 ? 3 : rssi > -75 ? 2 : 1;
    }
    for (int i = 0; i < 4; i++) {
        int h = 4 + i * 4;
        d.fillRect(W - 28 + i * 6, 20 - h, 4, h, i < bars ? TFT_WHITE : C_BAR_BG);
    }

    time_t now = time(nullptr);
    if (now > 1700000000) {
        struct tm lt;
        localtime_r(&now, &lt);
        char buf[8];
        strftime(buf, sizeof(buf), "%H:%M", &lt);
        d.setFont(&fonts::Font2);
        d.setTextDatum(middle_right);
        d.setTextColor(TFT_WHITE);
        d.drawString(buf, W - 36, HEADER_H / 2);
    }
}

static String formatReset(time_t t) {
    if (!t) return "";
    time_t now = time(nullptr);
    if (now < 1700000000) return "";
    long diff = (long)(t - now);
    if (diff <= 0) return "Reset faellig";
    long days = diff / 86400, hours = (diff % 86400) / 3600, mins = (diff % 3600) / 60;
    char rel[24];
    if (days > 0) snprintf(rel, sizeof(rel), "%ldd %ldh", days, hours);
    else if (hours > 0) snprintf(rel, sizeof(rel), "%ldh %02ldm", hours, mins);
    else snprintf(rel, sizeof(rel), "%ldm", mins);

    static const char* WD[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
    struct tm lt;
    localtime_r(&t, &lt);
    char buf[56];
    snprintf(buf, sizeof(buf), "Reset in %s  (%s %02d:%02d)", rel, WD[lt.tm_wday], lt.tm_hour, lt.tm_min);
    return buf;
}

static void drawBar(const UsageBar& b, int y) {
    auto& d = M5.Display;
    uint16_t col = pctColor(b.pct);
    d.setFont(&fonts::Font2);
    d.setTextDatum(top_left);
    d.setTextColor(TFT_WHITE);
    d.drawString(b.label, 8, y);
    char pct[12];
    snprintf(pct, sizeof(pct), "%.0f %%", b.pct);
    d.setTextDatum(top_right);
    d.setTextColor(col);
    d.drawString(pct, W - 8, y);

    const int bx = 8, by = y + 18, bw = W - 16, bh = 12;
    d.fillRoundRect(bx, by, bw, bh, 4, C_BAR_BG);
    float p = b.pct < 0 ? 0 : b.pct > 100 ? 100 : b.pct;
    int fw = (int)(bw * p / 100.0f + 0.5f);
    if (fw > 0) d.fillRoundRect(bx, by, fw < 8 ? 8 : fw, bh, 4, col);

    String r = formatReset(b.resetsAt);
    if (r.length()) {
        d.setTextDatum(top_left);
        d.setTextColor(C_DIM);
        d.drawString(r, 8, y + 32);
    }
}

void drawUsage(const UsageData& data, const String& status, bool statusIsError) {
    auto& d = M5.Display;
    d.startWrite();
    drawHeader();
    d.fillRect(0, HEADER_H, W, FOOTER_Y - HEADER_H, TFT_BLACK);
    if (!data.valid || data.count == 0) {
        d.setFont(&fonts::Font2);
        d.setTextDatum(middle_center);
        d.setTextColor(C_DIM);
        d.drawString(data.valid ? "Keine Limits in der Antwort" : "Noch keine Daten", W / 2, 120);
    } else {
        int avail = FOOTER_Y - HEADER_H - 4;
        int rowH = avail / data.count;
        if (rowH > 62) rowH = 62;
        int y0 = HEADER_H + 4 + (avail - rowH * data.count) / 2;
        for (int i = 0; i < data.count; i++) drawBar(data.bars[i], y0 + i * rowH);
    }
    d.fillRect(0, FOOTER_Y, W, 240 - FOOTER_Y, C_PANEL);
    d.setFont(&fonts::Font2);
    d.setTextDatum(middle_left);
    d.setTextColor(statusIsError ? C_RED : C_DIM);
    d.drawString(status, 4, FOOTER_Y + 8);
    d.endWrite();
}

void showMessage(const String& title, const String& line1, const String& line2, uint16_t titleColor) {
    auto& d = M5.Display;
    d.startWrite();
    d.fillScreen(TFT_BLACK);
    d.setTextDatum(middle_center);
    d.setFont(&fonts::FreeSansBold9pt7b);
    d.setTextColor(titleColor);
    d.drawString(title, W / 2, 90);
    d.setFont(&fonts::Font2);
    d.setTextColor(TFT_WHITE);
    d.drawString(line1, W / 2, 125);
    d.setTextColor(C_DIM);
    d.drawString(line2, W / 2, 150);
    d.endWrite();
}

void showPortal(const String& apSsid, const String& apPass, const String& ip) {
    auto& d = M5.Display;
    d.startWrite();
    d.fillScreen(TFT_BLACK);
    d.setFont(&fonts::FreeSansBold9pt7b);
    d.setTextDatum(top_left);
    d.setTextColor(C_ACCENT);
    d.drawString("WLAN-Setup", 8, 6);

    String qr = "WIFI:T:WPA;S:" + apSsid + ";P:" + apPass + ";;";
    d.fillRect(4, 30, 160, 160, TFT_WHITE);
    d.qrcode(qr.c_str(), 9, 35, 150, 3);

    d.setFont(&fonts::Font2);
    int x = 172, y = 32;
    auto line = [&](const String& s, uint16_t c) {
        d.setTextColor(c);
        d.drawString(s, x, y);
        y += 18;
    };
    line("1. WLAN verbinden", C_DIM);
    line(apSsid, TFT_WHITE);
    line("Passwort:", C_DIM);
    line(apPass, TFT_WHITE);
    y += 8;
    line("2. Im Browser oeffnen", C_DIM);
    line("http://" + ip, TFT_WHITE);

    d.setTextColor(C_DIM);
    d.setTextDatum(bottom_center);
    d.drawString("A: Abbrechen", 65, 238);
    d.endWrite();
}

}  // namespace ui
