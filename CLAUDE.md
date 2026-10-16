# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Projekt

Firmware für einen **M5Stack Basic Core (ESP32)**, die per WLAN die Abo-Limits (Pro/Max) des Claude-Kontos abfragt: 5-Stunden-Fenster und Wochenlimits in Prozent, mit Reset-Zeitpunkt. Konfiguriert wird über einen temporären WLAN-AP mit Captive Portal. Code-Kommentare und UI-Texte sind auf Deutsch.

## Befehle

```bash
pio run                      # bauen
pio run -t upload            # flashen (Port fest in platformio.ini: /dev/ttyUSB3)
pio device monitor           # serielles Log, 115200 Baud
python3 tools/claude_oauth_login.py   # eigenes OAuth-Token-Paar fürs Gerät erzeugen
```

Es gibt keine Tests. Plattform ist `espressif32@6.13.0` (Arduino-Core 2.0.17, C++11-Defaults). Deshalb sind Structs mit Default-Member-Initialisierern keine Aggregate, und `return {…}` funktioniert für sie nicht (siehe `ApiResult`).

## Architektur

- `main.cpp`: Ablaufsteuerung. Ohne SSID oder mit gedrückter Taste B beim Boot geht es ins Portal. Scheitert die WLAN-Verbindung, läuft das Portal 5 Minuten lang und danach folgt ein Neustart. Anschließend NTP (nötig für die TLS-Prüfung) und eine periodische Abfrage mit Backoff bei 429. Tasten: A = jetzt aktualisieren, B lang = Setup, C = Helligkeit.
- `claude_api.cpp`: `GET https://api.anthropic.com/api/oauth/usage` mit `Authorization: Bearer <OAuth-Access-Token>` und `anthropic-beta: oauth-2025-04-20`. Der Endpunkt ist **nicht dokumentiert**, Claude Code nutzt ihn für `/usage`. Die Antwort wird generisch geparst: Jedes Top-Level-Objekt mit numerischem `utilization` (0–100) wird zu einem Balken (max. 4), `resets_at` ist ISO-8601. Ist der Token abgelaufen oder kommt 401, wird er über den Refresh-Token erneuert (`POST <tokenUrl>` mit `grant_type=refresh_token` und `client_id`).
- **Refresh-Tokens rotieren**: Nach jedem Refresh werden die neuen Tokens sofort im NVS gespeichert (`configSaveTokens`). Teilt sich das Gerät ein Token-Paar mit Claude Code (`~/.claude/.credentials.json`), meldet der jeweils erste Refresh den anderen ab. Deshalb gibt es `tools/claude_oauth_login.py`, das ein separates Token-Paar per PKCE-Login erzeugt. Dessen Authorize-, Redirect- und Token-URLs sind nicht offiziell dokumentiert und per CLI-Flag überschreibbar. Token-URL und Client-ID sind auch im Gerät unter „Erweitert“ einstellbar.
- `portal.cpp`: AP `ClaudeUsage-XXXX` mit zufälligem WPA2-Passwort, das als Text und WLAN-QR-Code auf dem Display erscheint. Dazu DNSServer (alle Namen → AP-IP) und WebServer. Secrets werden nie an die Seite zurückgegeben: Leere Felder bedeuten „unverändert“. Ins Feld „JSON einfügen“ passen die Ausgabe des Login-Skripts oder der Inhalt von `.credentials.json` (`claudeAiOauth.accessToken/refreshToken/expiresAt` in ms).
- `config.cpp`: alles in `Preferences`, Namespace `cusage`. NVS ist nicht verschlüsselt. Das `nvs_open failed: NOT_FOUND` beim allerersten Boot ist harmlos.
- `ui.cpp`: Direktes Zeichnen auf 320×240 ohne Sprite, weil der Basic kein PSRAM hat und TLS Heap braucht. Die GFX-Fonts kennen nur ASCII, deshalb **keine Umlaute in Display-Texten** („ueber“ statt „über“). Die Webseite ist UTF-8 und darf Umlaute haben.
- `root_ca.h`: eingebettete Root-CAs (GTS R1/R4, GlobalSign Root CA, ISRG X1/X2) für `api.anthropic.com` und `platform.claude.com`/`console.anthropic.com`. Die Datei ist aus `/etc/ssl/certs` und `pki.goog` generiert. GlobalSign Root CA läuft am **2028-01-28** ab. Wechselt ein Host die CA, scheitert TLS mit „Netzwerkfehler“. Die aktuelle Kette lässt sich mit `openssl s_client -showcerts -connect <host>:443` prüfen.

## Hinweise

- `Serial.begin(115200)` muss explizit vor `M5.begin()` stehen. Die eingesetzte M5Unified-Version initialisiert die serielle Schnittstelle nicht selbst; ohne den Aufruf erscheinen nur `log_e`-Ausgaben.
- Beim Öffnen des Ports setzen DTR/RTS das Board zurück.
