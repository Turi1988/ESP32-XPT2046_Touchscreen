// ============================================================================
//  SCHALTER-SEITE (view_index == 4)
//  Eigene Datei: esp32-tft-schalter.ino
//
//  Hier anpassen:
//    - SCHALTER_NAME_1 … 6   Anzeigenamen auf dem Display
//    - SCHALTER_ENTITY_1 … 6 Entity-IDs aus Home Assistant
//
//  Sensible Daten (WLAN, HA-Token, HA-URL) liegen nur in secrets.h
// ============================================================================

#include <HTTPClient.h>
#include <WiFi.h>

// ----------------------------------------------------------------------------
//  ANZEIGENAMEN (Klartext auf den Kacheln)
// ----------------------------------------------------------------------------
const char* SCHALTER_NAME_1 = "Licht";
const char* SCHALTER_NAME_2 = "Schalter 2";
const char* SCHALTER_NAME_3 = "Schalter 3";
const char* SCHALTER_NAME_4 = "Schalter 4";
const char* SCHALTER_NAME_5 = "Schalter 5";
const char* SCHALTER_NAME_6 = "Schalter 6";

// ----------------------------------------------------------------------------
//  ENTITY-IDS (Home Assistant: Einstellungen -> Entitaeten)
//  Beispiele: "light.wohnzimmer", "switch.steckdose_kueche"
// ----------------------------------------------------------------------------
const char* SCHALTER_ENTITY_1 = "switch.smart_plug";
const char* SCHALTER_ENTITY_2 = "switch.schalter_2";
const char* SCHALTER_ENTITY_3 = "switch.schalter_3";
const char* SCHALTER_ENTITY_4 = "switch.schalter_4";
const char* SCHALTER_ENTITY_5 = "switch.schalter_5";
const char* SCHALTER_ENTITY_6 = "switch.schalter_6";

// ----------------------------------------------------------------------------
//  FARBEN
// ----------------------------------------------------------------------------
#define FARBE_SCHALTER_AUS        tft.color565(40, 55, 80)
#define FARBE_SCHALTER_AN         tft.color565(30, 140, 90)
#define FARBE_RAHMEN_GEDRUECKT    tft.color565(255, 200, 0)

// ----------------------------------------------------------------------------
//  DATENSTRUKTUR
// ----------------------------------------------------------------------------
struct SchalterKachel {
  int x, y, breite, hoehe;
  const char* name;
  const char* entity_id;
  bool zustand_an;
  bool hat_dauerzustand;   // true = an/aus, false = nur Taster
};

SchalterKachel schalter_kacheln[6];

int schalter_gedrueckt_index = -1;
unsigned long schalter_gedrueckt_zeit = 0;
#define SCHALTER_GEDRUECKT_ANZEIGEDAUER 250

// ----------------------------------------------------------------------------
//  LAYOUT
// ----------------------------------------------------------------------------
void schalterseite_layout_aufbauen() {
  int tile_w = 90;
  int tile_h = 90;
  int gap_x = 12;
  int gap_y = 14;
  int start_x = (SCREEN_W - (tile_w * 3 + gap_x * 2)) / 2;
  int start_y = HEADER_HOEHE + 15;

  const char* namen[6] = {
    SCHALTER_NAME_1, SCHALTER_NAME_2, SCHALTER_NAME_3,
    SCHALTER_NAME_4, SCHALTER_NAME_5, SCHALTER_NAME_6
  };
  const char* entities[6] = {
    SCHALTER_ENTITY_1, SCHALTER_ENTITY_2, SCHALTER_ENTITY_3,
    SCHALTER_ENTITY_4, SCHALTER_ENTITY_5, SCHALTER_ENTITY_6
  };

  for (int i = 0; i < 3; i++) {
    schalter_kacheln[i] = {
      start_x + i * (tile_w + gap_x), start_y, tile_w, tile_h,
      namen[i], entities[i], false, true
    };
  }
  int y2 = start_y + tile_h + gap_y;
  for (int i = 0; i < 3; i++) {
    schalter_kacheln[3 + i] = {
      start_x + i * (tile_w + gap_x), y2, tile_w, tile_h,
      namen[3 + i], entities[3 + i], false, true
    };
  }
}

// ----------------------------------------------------------------------------
//  ZEICHNEN
// ----------------------------------------------------------------------------
static void schalter_kachel_zeichnen(int i) {
  SchalterKachel &k = schalter_kacheln[i];
  uint16_t grundfarbe = k.zustand_an ? FARBE_SCHALTER_AN : FARBE_SCHALTER_AUS;
  uint16_t rahmenfarbe = (i == schalter_gedrueckt_index) ? FARBE_RAHMEN_GEDRUECKT : FARBE_RAHMEN;

  tft.fillRoundRect(k.x, k.y, k.breite, k.hoehe, 8, grundfarbe);
  tft.drawRoundRect(k.x, k.y, k.breite, k.hoehe, 8, rahmenfarbe);
  if (i == schalter_gedrueckt_index) {
    tft.drawRoundRect(k.x + 1, k.y + 1, k.breite - 2, k.hoehe - 2, 7, FARBE_RAHMEN_GEDRUECKT);
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TEXT, grundfarbe);
  tft.drawString(k.name, k.x + k.breite / 2, k.y + k.hoehe / 2 - 10, 1);
  tft.drawString(k.zustand_an ? "AN" : "AUS", k.x + k.breite / 2, k.y + k.hoehe / 2 + 15, 1);
}

void schalterseite_zeichnen() {
  for (int i = 0; i < 6; i++) {
    schalter_kachel_zeichnen(i);
  }
}

// ----------------------------------------------------------------------------
//  HOME ASSISTANT API (Token + URL nur aus secrets.h)
// ----------------------------------------------------------------------------
bool ha_entity_toggle(const char* entity_id) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("HA: kein WLAN");
    return false;
  }
  if (entity_id == nullptr || entity_id[0] == '\0') return false;
  if (strcmp(HA_TOKEN, "DEIN_HA_LONG_LIVED_TOKEN") == 0) {
    Serial.println("HA: Token noch Platzhalter – nur lokaler Zustand");
    return false;
  }

  String eid(entity_id);
  int punkt = eid.indexOf('.');
  String domain = (punkt > 0) ? eid.substring(0, punkt) : String("switch");
  String url = String(HA_BASE_URL) + "/api/services/" + domain + "/toggle";

  HTTPClient http;
  http.setTimeout(4000);
  http.begin(url);
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);
  http.addHeader("Content-Type", "application/json");

  String body = String("{\"entity_id\":\"") + entity_id + "\"}";
  int code = http.POST(body);
  Serial.printf("HA toggle %s -> HTTP %d\n", entity_id, code);
  http.end();
  return (code >= 200 && code < 300);
}

bool ha_entity_ist_an(const char* entity_id, bool* out_an) {
  if (WiFi.status() != WL_CONNECTED || out_an == nullptr) return false;
  if (strcmp(HA_TOKEN, "DEIN_HA_LONG_LIVED_TOKEN") == 0) return false;

  String url = String(HA_BASE_URL) + "/api/states/" + entity_id;
  HTTPClient http;
  http.setTimeout(3000);
  http.begin(url);
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);

  int code = http.GET();
  bool ok = false;
  if (code == 200) {
    String payload = http.getString();
    if (payload.indexOf("\"state\":\"on\"") >= 0) {
      *out_an = true;
      ok = true;
    } else if (payload.indexOf("\"state\":\"off\"") >= 0) {
      *out_an = false;
      ok = true;
    }
  }
  http.end();
  return ok;
}

// ----------------------------------------------------------------------------
//  TOUCH
// ----------------------------------------------------------------------------
void schalterseite_touch_behandeln() {
  for (int i = 0; i < 6; i++) {
    SchalterKachel &k = schalter_kacheln[i];
    if (touch_x >= k.x && touch_x <= k.x + k.breite &&
        touch_y >= k.y && touch_y <= k.y + k.hoehe) {

      schalter_gedrueckt_index = i;
      schalter_gedrueckt_zeit = millis();

      if (k.hat_dauerzustand) {
        k.zustand_an = !k.zustand_an;
      }
      schalter_kachel_zeichnen(i);

      bool ha_ok = ha_entity_toggle(k.entity_id);
      if (ha_ok) {
        bool echt = k.zustand_an;
        if (ha_entity_ist_an(k.entity_id, &echt)) {
          k.zustand_an = echt;
          schalter_kachel_zeichnen(i);
        }
      }

      Serial.printf("Schalter %d (%s) -> %s (HA %s)\n",
                    i, k.name, k.zustand_an ? "AN" : "AUS",
                    ha_ok ? "ok" : "lokal");
      return;
    }
  }
}

void schalterseite_blink_zuruecksetzen() {
  if (schalter_gedrueckt_index != -1 &&
      millis() - schalter_gedrueckt_zeit > SCHALTER_GEDRUECKT_ANZEIGEDAUER) {
    int i = schalter_gedrueckt_index;
    schalter_gedrueckt_index = -1;
    schalter_kachel_zeichnen(i);
  }
}

void schalterzustaende_aktualisieren() {
  if (WiFi.status() != WL_CONNECTED) return;
  if (strcmp(HA_TOKEN, "DEIN_HA_LONG_LIVED_TOKEN") == 0) return;

  bool geaendert = false;
  for (int i = 0; i < 6; i++) {
    bool an = false;
    if (ha_entity_ist_an(schalter_kacheln[i].entity_id, &an)) {
      if (schalter_kacheln[i].zustand_an != an) {
        schalter_kacheln[i].zustand_an = an;
        geaendert = true;
      }
    }
  }
  if (geaendert && view_index == 4) {
    schalterseite_zeichnen();
  }
}
