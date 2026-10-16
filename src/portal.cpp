#include "portal.h"
#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include "ui.h"

namespace portal {

static WebServer server(80);
static DNSServer dns;
static Config* cfgp = nullptr;
static String networks[20];
static int networkCount = 0;
static uint32_t lastActivity = 0;

static String esc(const String& s) {
    String o;
    o.reserve(s.length() + 8);
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            case '\'': o += "&#39;"; break;
            default: o += c;
        }
    }
    return o;
}

static const char PAGE_HEAD[] PROGMEM = R"HTML(<!doctype html><html lang="de"><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Claude Usage – Setup</title><style>
:root{color-scheme:light dark;--bg:#faf9f5;--fg:#1f1e1d;--mut:#6b6a66;--card:#fff;--bd:#dedcd4;--acc:#c96442}
@media(prefers-color-scheme:dark){:root{--bg:#1f1e1d;--fg:#ecebe6;--mut:#a3a29c;--card:#2a2927;--bd:#3d3c39}}
body{margin:0;background:var(--bg);color:var(--fg);font:16px/1.45 system-ui,sans-serif}
main{max-width:560px;margin:0 auto;padding:16px}
h1{font-size:1.4rem;margin:.2em 0 .8em}h2{font-size:1.05rem;margin:0 0 .6em}
section{background:var(--card);border:1px solid var(--bd);border-radius:10px;padding:14px 16px;margin-bottom:14px}
label{display:block;font-weight:600;margin:.7em 0 .25em}
input,textarea{box-sizing:border-box;width:100%;padding:9px 10px;border:1px solid var(--bd);border-radius:7px;background:var(--bg);color:var(--fg);font:inherit}
textarea{font-family:ui-monospace,monospace;font-size:13px;min-height:5em}
.hint{color:var(--mut);font-size:.88rem;margin:.3em 0 0}.ok{color:#3f8f4f}.warn{color:#b8572f}
.row{display:flex;gap:8px;align-items:center}.row input[type=checkbox]{width:auto}
button{padding:11px 18px;border:0;border-radius:8px;background:var(--acc);color:#fff;font:inherit;font-weight:600;cursor:pointer}
button.sec{background:transparent;color:var(--warn,#b8572f);border:1px solid var(--bd)}
details summary{cursor:pointer;font-weight:600}code{font-size:.9em}
</style></head><body><main><h1>Claude Usage – Einrichtung</h1>
)HTML";

static void handleRoot() {
    lastActivity = millis();
    Config& c = *cfgp;
    String h;
    h.reserve(7000);
    h += FPSTR(PAGE_HEAD);
    h += F("<form method='post' action='/save'><section><h2>WLAN</h2><label for='ssid'>Netzwerk (SSID)</label>"
           "<input id='ssid' name='ssid' list='nets' required autocomplete='off' value='");
    h += esc(c.ssid);
    h += F("'><datalist id='nets'>");
    for (int i = 0; i < networkCount; i++) h += "<option value='" + esc(networks[i]) + "'>";
    h += F("</datalist><label for='wpass'>Passwort</label><input id='wpass' name='wpass' type='password' "
           "autocomplete='off' placeholder='");
    h += c.wifiPass.length() ? F("gespeichert – leer lassen zum Beibehalten") : F("");
    h += F("'><div class='row'><input type='checkbox' id='wopen' name='wopen'><label for='wopen' "
           "style='margin:0;font-weight:400'>Offenes Netz (kein Passwort)</label></div></section>");

    h += F("<section><h2>Claude-Anmeldung</h2><p class='hint'>Status: ");
    if (c.refreshToken.length()) h += F("<span class='ok'>Refresh-Token gespeichert</span>");
    else if (c.accessToken.length()) h += F("<span class='warn'>nur Access-Token (läuft ab)</span>");
    else h += F("<span class='warn'>keine Tokens</span>");
    h += F("</p><p class='hint'><b>Empfohlen:</b> auf dem PC <code>python3 tools/claude_oauth_login.py</code> "
           "ausführen. Das erzeugt ein <i>eigenes</i> Token-Paar nur für dieses Gerät.</p>"
           "<p class='hint warn'>Tokens aus <code>~/.claude/.credentials.json</code> funktionieren auch, "
           "aber Refresh-Tokens rotieren: Sobald Gerät oder Claude Code erneuert, wird der jeweils "
           "andere abgemeldet.</p>"
           "<label for='atok'>Access-Token</label><input id='atok' name='atok' type='password' "
           "autocomplete='off' placeholder='sk-ant-oat01-… (leer = unverändert)'>"
           "<label for='rtok'>Refresh-Token</label><input id='rtok' name='rtok' type='password' "
           "autocomplete='off' placeholder='sk-ant-ort01-… (leer = unverändert)'>"
           "<label for='exp'>Ablaufzeit (expiresAt, optional)</label><input id='exp' name='exp' "
           "inputmode='numeric' placeholder='Unix-Zeit in ms oder s'>"
           "<details style='margin-top:.8em'><summary>…oder JSON einfügen</summary>"
           "<p class='hint'>Ausgabe von <code>claude_oauth_login.py</code> oder Inhalt von "
           "<code>.credentials.json</code>; überschreibt die Felder oben.</p>"
           "<textarea name='creds' autocomplete='off' spellcheck='false'></textarea></details></section>");

    h += F("<section><details><summary>Erweitert</summary><label for='intv'>Abfrageintervall (Sekunden, min. 60)</label>"
           "<input id='intv' name='intv' type='number' min='60' value='");
    h += String(c.intervalSec);
    h += F("'><label for='tz'>Zeitzone (POSIX-TZ)</label><input id='tz' name='tz' value='");
    h += esc(c.tz);
    h += F("'><label for='turl'>OAuth-Token-URL</label><input id='turl' name='turl' value='");
    h += esc(c.tokenUrl);
    h += F("'><label for='cid'>OAuth-Client-ID</label><input id='cid' name='cid' value='");
    h += esc(c.clientId);
    h += F("'></details></section><button type='submit'>Speichern &amp; neu starten</button></form>"
           "<form method='post' action='/erase' onsubmit=\"return confirm('Alle Einstellungen löschen?')\" "
           "style='margin-top:22px'><button class='sec' type='submit'>Alle Einstellungen löschen</button></form>"
           "</main></body></html>");
    server.send(200, "text/html; charset=utf-8", h);
}

static void sendInfo(const String& title, const String& text) {
    String h = FPSTR(PAGE_HEAD);
    h += "<section><h2>" + title + "</h2><p>" + text + "</p></section></main></body></html>";
    server.send(200, "text/html; charset=utf-8", h);
}

static void applyCredentialJson(Config& c, const String& json, String& err) {
    JsonDocument doc;
    if (deserializeJson(doc, json) != DeserializationError::Ok) {
        err = "JSON konnte nicht gelesen werden.";
        return;
    }
    JsonObject o = doc["claudeAiOauth"].is<JsonObject>() ? doc["claudeAiOauth"].as<JsonObject>()
                                                         : doc.as<JsonObject>();
    auto str = [&](const char* a, const char* b) -> const char* {
        return o[a].is<const char*>() ? o[a].as<const char*>() : o[b] | (const char*)nullptr;
    };
    const char* at = str("accessToken", "access_token");
    const char* rt = str("refreshToken", "refresh_token");
    if (!at && !rt) {
        err = "Im JSON wurden keine Tokens gefunden.";
        return;
    }
    if (at) c.accessToken = at;
    if (rt) c.refreshToken = rt;
    uint64_t exp = o["expiresAt"].is<uint64_t>() ? o["expiresAt"].as<uint64_t>() : o["expires_at"] | (uint64_t)0;
    c.expiresAt = exp > 100000000000ULL ? exp / 1000 : exp;
}

static void handleSave() {
    lastActivity = millis();
    Config& c = *cfgp;
    String ssid = server.arg("ssid");
    ssid.trim();
    if (!ssid.length()) {
        sendInfo("Fehler", "Bitte eine SSID angeben. <a href='/'>Zurück</a>");
        return;
    }
    c.ssid = ssid;
    if (server.hasArg("wopen")) c.wifiPass = "";
    else if (server.arg("wpass").length()) c.wifiPass = server.arg("wpass");

    String at = server.arg("atok"), rt = server.arg("rtok"), exp = server.arg("exp");
    at.trim();
    rt.trim();
    exp.trim();
    if (at.length()) {
        c.accessToken = at;
        c.expiresAt = 0;
    }
    if (rt.length()) c.refreshToken = rt;
    if (exp.length()) {
        uint64_t v = strtoull(exp.c_str(), nullptr, 10);
        c.expiresAt = v > 100000000000ULL ? v / 1000 : v;
    }
    String creds = server.arg("creds");
    creds.trim();
    if (creds.length()) {
        String err;
        applyCredentialJson(c, creds, err);
        if (err.length()) {
            sendInfo("Fehler", err + " <a href='/'>Zurück</a>");
            return;
        }
    }

    long iv = server.arg("intv").toInt();
    c.intervalSec = iv < MIN_INTERVAL_SEC ? MIN_INTERVAL_SEC : (uint32_t)iv;
    String tz = server.arg("tz"), turl = server.arg("turl"), cid = server.arg("cid");
    tz.trim();
    turl.trim();
    cid.trim();
    c.tz = tz.length() ? tz : String(DEFAULT_TZ);
    c.tokenUrl = turl.length() ? turl : String(DEFAULT_TOKEN_URL);
    c.clientId = cid.length() ? cid : String(DEFAULT_CLIENT_ID);

    configSave(c);
    sendInfo("Gespeichert", "Das Gerät startet neu und verbindet sich mit <b>" + esc(c.ssid) +
                                "</b>. Dieses Setup-WLAN verschwindet dabei.");
    ui::showMessage("Gespeichert", "Starte neu ...");
    delay(1500);
    ESP.restart();
}

static void handleErase() {
    configErase();
    sendInfo("Gelöscht", "Alle Einstellungen wurden gelöscht. Das Gerät startet neu.");
    ui::showMessage("Einstellungen geloescht", "Starte neu ...");
    delay(1500);
    ESP.restart();
}

static void handleNotFound() {
    // Captive Portal: alle fremden URLs (generate_204, hotspot-detect, ...) auf die Startseite
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString() + "/", true);
    server.send(302, "text/plain", "");
}

static String randomPassword() {
    static const char CH[] = "abcdefghjkmnpqrstuvwxyz23456789";
    String p;
    for (int i = 0; i < 10; i++) p += CH[esp_random() % (sizeof(CH) - 1)];
    return p;
}

static void scanNetworks() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    int n = WiFi.scanNetworks();
    networkCount = 0;
    for (int i = 0; i < n && networkCount < 20; i++) {
        String s = WiFi.SSID(i);
        if (!s.length()) continue;
        bool dup = false;
        for (int j = 0; j < networkCount; j++) dup |= networks[j] == s;
        if (!dup) networks[networkCount++] = s;
    }
    WiFi.scanDelete();
}

void run(Config& cfg, uint32_t timeoutMs) {
    cfgp = &cfg;
    ui::showMessage("WLAN-Setup", "Suche Netzwerke ...");
    scanNetworks();

    uint8_t mac[6];
    WiFi.macAddress(mac);
    char ssid[24];
    snprintf(ssid, sizeof(ssid), "ClaudeUsage-%02X%02X", mac[4], mac[5]);
    String pass = randomPassword();

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, pass.c_str());
    delay(100);
    IPAddress ip = WiFi.softAPIP();
    dns.start(53, "*", ip);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/erase", HTTP_POST, handleErase);
    server.onNotFound(handleNotFound);
    server.begin();

    Serial.printf("[portal] AP %s gestartet, http://%s/\n", ssid, ip.toString().c_str());
    ui::showPortal(ssid, pass, ip.toString());

    lastActivity = millis();
    while (true) {
        dns.processNextRequest();
        server.handleClient();
        M5.update();
        if (M5.BtnA.wasClicked()) break;
        if (WiFi.softAPgetStationNum() > 0) lastActivity = millis();
        if (timeoutMs && millis() - lastActivity > timeoutMs) break;
        delay(2);
    }
    server.stop();
    dns.stop();
    WiFi.softAPdisconnect(true);
}

}  // namespace portal
