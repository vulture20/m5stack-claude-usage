#pragma once
#include "config.h"

namespace portal {
// Spannt einen temporaeren WPA2-AP mit Zufallspasswort auf und bedient die
// Konfigurationsseite (Captive Portal). Nach dem Speichern startet das Geraet neu.
// Kehrt zurueck bei Abbruch (Taste A) oder nach timeoutMs ohne Aktivitaet (0 = nie).
void run(Config& cfg, uint32_t timeoutMs);
}
