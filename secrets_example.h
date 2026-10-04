// ============================================================================
//  SECRETS BEISPIEL - darf ins Git / geteilt werden
//
//  1) Diese Datei kopieren nach: secrets.h
//  2) In secrets.h die echten Werte eintragen
//  3) secrets.h niemals committen oder weitergeben
//
//  Schalter-Namen und Entity-IDs stehen in esp32-tft-schalter.ino
// ============================================================================

#pragma once

// ---------- WLAN ----------
#define WLAN_SSID       "DEIN_WLAN_NAME"
#define WLAN_PASSWORT   "DEIN_WLAN_PASSWORT"

// ---------- Home Assistant ----------
// z.B. "http://192.168.1.50:8123"  (ohne Slash am Ende)
#define HA_BASE_URL     "http://homeassistant.local:8123"

// Long-Lived Access Token:
// HA -> Profil (unten links) -> Sicherheit -> Langzeit-Zugangstoken erstellen
#define HA_TOKEN        "DEIN_HA_LONG_LIVED_TOKEN"
