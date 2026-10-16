# M5Stack Claude Usage

Firmware für den **M5Stack Basic Core (ESP32)**, die die Abo-Limits eines Claude-Kontos (Pro/Max) auf dem Display anzeigt: das 5-Stunden-Fenster und die Wochenlimits in Prozent, jeweils mit der Zeit bis zum Zurücksetzen. Das Gerät verbindet sich per WLAN. Eingerichtet wird es über einen temporären Access Point mit Einrichtungsseite.

> **Hinweis:** Die Daten kommen vom nicht dokumentierten Endpunkt `https://api.anthropic.com/api/oauth/usage`, den auch Claude Code für `/usage` verwendet. Anthropic kann ihn jederzeit ändern. Das ist ein inoffizielles Projekt ohne Verbindung zu Anthropic.

## Funktionen

- Bis zu vier farbige Balken (grün < 50 %, gelb < 80 %, rot ab 80 %) mit Reset-Countdown
- Neue Arten von Limits aus der API erscheinen ohne Codeänderung
- Einrichtung über den AP `ClaudeUsage-XXXX` mit zufälligem WPA2-Passwort, das auch als WLAN-QR-Code auf dem Display steht
- Automatische Token-Erneuerung; die neuen Tokens werden im Flash gespeichert
- TLS-Prüfung mit eingebetteten Root-Zertifikaten statt `setInsecure()`
- Backoff bei Rate-Limits, Wiederverbindung nach WLAN-Ausfall

## Voraussetzungen

- M5Stack Basic Core (ESP32, ohne PSRAM)
- [PlatformIO](https://platformio.org/)
- Ein Claude-Abo (Pro oder Max)

## Bauen und Flashen

Der serielle Port ist in `platformio.ini` auf `/dev/ttyUSB3` gesetzt. Passe `upload_port` und `monitor_port` an deinen Rechner an.

```bash
pio run -t upload
pio device monitor
```

## Einrichtung

1. **Token erzeugen** (auf dem PC):

   ```bash
   python3 tools/claude_oauth_login.py
   ```

   Die angezeigte URL im Browser öffnen, mit dem Claude-Konto anmelden und den angezeigten Code zurückkopieren. Das Skript prüft den neuen Token und gibt am Ende JSON aus. Es braucht nur die Python-Standardbibliothek.

2. **Mit dem Gerät verbinden:** Beim ersten Start öffnet das Gerät den AP `ClaudeUsage-XXXX`. Passwort und QR-Code stehen auf dem Display. Die Einrichtungsseite öffnet sich automatisch, sonst `http://192.168.4.1/` aufrufen.

3. **Eintragen:** WLAN auswählen, Passwort eingeben und das JSON aus Schritt 1 unter „…oder JSON einfügen“ einfügen. Nach dem Speichern startet das Gerät neu.

### Warum ein eigenes Token-Paar?

OAuth-Refresh-Tokens werden bei jeder Erneuerung ausgetauscht, der alte Token wird ungültig. Kopierst du die Tokens aus `~/.claude/.credentials.json`, nutzen Gerät und Claude Code dasselbe Paar. Wer zuerst erneuert, meldet den anderen ab. Das Login-Skript erzeugt deshalb eine eigene Anmeldung nur für das Gerät.

Unter „Erweitert“ lassen sich Abfrageintervall (Standard 300 s, mindestens 60 s), Zeitzone (POSIX-TZ, Standard Mitteleuropa), OAuth-Token-URL und Client-ID ändern. Das Login-Skript kennt dafür entsprechende Optionen (`--help`).

## Bedienung

| Taste | Funktion |
|---|---|
| A | sofort aktualisieren (im Setup: abbrechen) |
| B lang (2 s) | Einrichtung öffnen; auch beim Einschalten gedrückt halten |
| C | Helligkeit durchschalten |

Ist das WLAN beim Start nicht erreichbar, öffnet das Gerät die Einrichtung für 5 Minuten und versucht es danach erneut.

## Sicherheit

- Die Tokens liegen **unverschlüsselt** im NVS-Flash. Wer das Gerät in die Hand bekommt, kann sie auslesen.
- Die Einrichtungsseite gibt gespeicherte Passwörter und Tokens nie zurück; leere Felder bedeuten „unverändert“.
- Der Setup-AP ist WPA2-geschützt und nur temporär aktiv.

## Wartung

Die Root-Zertifikate stehen in `src/root_ca.h`. **GlobalSign Root CA läuft am 28.01.2028 ab.** Wechselt ein Anthropic-Host die Zertifizierungsstelle, schlagen die Abfragen mit „Netzwerkfehler“ fehl. Die aktuelle Kette lässt sich so prüfen:

```bash
openssl s_client -showcerts -connect api.anthropic.com:443 </dev/null
```

## Projektstruktur

| Datei | Inhalt |
|---|---|
| `src/main.cpp` | Ablauf, Tasten, Abfrageintervall und Backoff |
| `src/claude_api.cpp` | Usage-Abfrage, Token-Erneuerung, JSON-Auswertung |
| `src/portal.cpp` | Access Point, Captive Portal, Einrichtungsseite |
| `src/ui.cpp` | Display-Ausgabe |
| `src/config.cpp` | Einstellungen im NVS |
| `src/root_ca.h` | eingebettete Root-Zertifikate |
| `tools/claude_oauth_login.py` | PKCE-Login für ein eigenes Token-Paar |
