// ============================================================================
//  TEMPERATUREN-SEITE (view_index == 1)
//  6 Kacheln, 2 Reihen zu 3: Wohnzimmer, Schlafzimmer, Badezimmer,
//  Balkon, Serverschrank, Kinderzimmer
//  Wohnzimmer, Schlafzimmer, Balkon und Serverschrank haben echte
//  Home-Assistant-Sensoren, Badezimmer und Kinderzimmer sind Platzhalter
//  ohne Wert.
//
//  HOME-ASSISTANT-ANBINDUNG:
//  Werte werden per REST-API von Home Assistant geholt
//  (GET http://<HA_IP>/api/states/<entity_id>, Header "Authorization: Bearer <Token>").
//  Benoetigte Bibliotheken: HTTPClient.h (Teil des ESP32-Boardpakets),
//  ArduinoJson (ueber Bibliotheksverwalter installieren).
// ============================================================================

#include <HTTPClient.h>
#include <ArduinoJson.h>

// ----------------------------------------------------------------------------
//  HOME-ASSISTANT-ZUGANGSDATEN
//  HA_TOKEN bitte durch den eigenen Long-Lived Access Token ersetzen
//  (Home Assistant -> Profil -> ganz unten "Langlebige Zugriffstoken" -> Token erstellen)
// ----------------------------------------------------------------------------
const char* HA_HOST  = "192.168.178.101";
const int   HA_PORT  = 8123;
const char* HA_TOKEN = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJkMjFhMWQ0MjliOGQ0ZTUzYjZlMzU5ZDczNTU3MzQ1ZiIsImlhdCI6MTc5MDUzOTU1OSwiZXhwIjoyMTA1ODk5NTU5fQ.bQEfLbBnKI7S_PY6PXeichhncpx6mVmAQ6HC_Iry9rE";

// wie oft die Sensorwerte abgefragt werden
#define TEMPERATUR_AKTUALISIERUNG_MS 60000   // alle 60 Sekunden

unsigned long temperatur_letzte_aktualisierung = 0;

// ----------------------------------------------------------------------------
//  EINSTELLUNGSBEREICH - SCHWELLENWERTE PRO KACHEL
//  Je Kachel: untere Schwelle (Blau->Gruen) und obere Schwelle (Gruen->Rot)
//  Hier individuell fuer jeden Raum anpassen, besonders wichtig fuer
//  Balkon und Serverschrank.
// ----------------------------------------------------------------------------

float SCHWELLE_UNTEN_WOHNZIMMER   = 19.0;
float SCHWELLE_OBEN_WOHNZIMMER    = 25.0;

float SCHWELLE_UNTEN_SCHLAFZIMMER = 16.0;
float SCHWELLE_OBEN_SCHLAFZIMMER  = 22.0;

float SCHWELLE_UNTEN_BADEZIMMER   = 18.0;
float SCHWELLE_OBEN_BADEZIMMER    = 26.0;

float SCHWELLE_UNTEN_BALKON       = 0.0;
float SCHWELLE_OBEN_BALKON        = 30.0;

float SCHWELLE_UNTEN_SERVERSCHRANK = 15.0;
float SCHWELLE_OBEN_SERVERSCHRANK  = 35.0;

float SCHWELLE_UNTEN_KINDERZIMMER = 19.0;
float SCHWELLE_OBEN_KINDERZIMMER  = 24.0;

// ----------------------------------------------------------------------------
//  ENTITY-IDS DER HOME-ASSISTANT-SENSOREN
//  Bad und Kinderzimmer haben aktuell keine Sensoren -> leerer String
// ----------------------------------------------------------------------------
const char* ENTITY_TEMP_WOHNZIMMER    = "sensor.esp01s_2_temperatur";
const char* ENTITY_FEUCHTE_WOHNZIMMER = "sensor.esp01s_2_luftfeuchtigkeit";

const char* ENTITY_TEMP_SCHLAFZIMMER    = "sensor.esp01s_7_temperatur";
const char* ENTITY_FEUCHTE_SCHLAFZIMMER = "sensor.esp01s_7_luftfeuchtigkeit";

const char* ENTITY_TEMP_BALKON    = "sensor.esp_s01_4_temperatur";
const char* ENTITY_FEUCHTE_BALKON = "sensor.luftfeuchtigkeit_4";

const char* ENTITY_TEMP_SERVERSCHRANK    = "sensor.tech_esp01s_1_temperatur_server";
const char* ENTITY_FEUCHTE_SERVERSCHRANK = "sensor.tech_esp01s_1_luftfeuchtigkeit_server";

// ----------------------------------------------------------------------------
//  FARBEN FUER DIE TEMPERATURKACHELN (Grundfarbe je Raum, kraeftig/unterschiedlich)
// ----------------------------------------------------------------------------
#define FARBE_TEMP_WOHNZIMMER    tft.color565(40, 90, 150)
#define FARBE_TEMP_SCHLAFZIMMER  tft.color565(120, 60, 130)
#define FARBE_TEMP_BADEZIMMER    tft.color565(30, 140, 140)
#define FARBE_TEMP_BALKON        tft.color565(60, 130, 70)
#define FARBE_TEMP_SERVERSCHRANK tft.color565(150, 90, 30)
#define FARBE_TEMP_KINDERZIMMER  tft.color565(150, 60, 90)

// Ampelfarben fuer den Temperaturwert selbst (Blau/Gruen/Rot je nach Schwelle)
#define FARBE_TEMP_KALT tft.color565(80, 160, 255)
#define FARBE_TEMP_NORMAL tft.color565(120, 230, 120)
#define FARBE_TEMP_WARM tft.color565(255, 90, 90)

// ----------------------------------------------------------------------------
//  DATENSTRUKTUR EINER TEMPERATUR-KACHEL
// ----------------------------------------------------------------------------
struct TemperaturKachel {
  int x, y, breite, hoehe;
  uint16_t farbe;
  const char* name;
  bool hat_sensor;             // false = Platzhalter ohne echten Wert
  const char* entity_temp;     // Entity-ID Temperatur (nur relevant wenn hat_sensor)
  const char* entity_feuchte;  // Entity-ID Luftfeuchtigkeit (nur relevant wenn hat_sensor)
  float wert_temp;             // aktueller Temperaturwert
  float wert_feuchte;          // aktueller Feuchtewert
  float schwelle_unten;
  float schwelle_oben;
};

TemperaturKachel temperatur_kacheln[6];

// ----------------------------------------------------------------------------
//  LAYOUT AUFBAUEN (einmalig beim Start aufrufen, z.B. in setup())
// ----------------------------------------------------------------------------
void temperaturseite_layout_aufbauen() {
  int tile_w = 95;
  int tile_h = 85;
  int gap_x = 8;
  int gap_y = 10;
  int start_x = (SCREEN_W - (tile_w * 3 + gap_x * 2)) / 2;
  int start_y = HEADER_HOEHE + 12;

  // Reihe 1
  temperatur_kacheln[0] = { start_x, start_y, tile_w, tile_h,
    FARBE_TEMP_WOHNZIMMER, "Wohnzimmer", true,
    ENTITY_TEMP_WOHNZIMMER, ENTITY_FEUCHTE_WOHNZIMMER, 0, 0,
    SCHWELLE_UNTEN_WOHNZIMMER, SCHWELLE_OBEN_WOHNZIMMER };

  temperatur_kacheln[1] = { start_x + (tile_w + gap_x), start_y, tile_w, tile_h,
    FARBE_TEMP_SCHLAFZIMMER, "Schlafzimmer", true,
    ENTITY_TEMP_SCHLAFZIMMER, ENTITY_FEUCHTE_SCHLAFZIMMER, 0, 0,
    SCHWELLE_UNTEN_SCHLAFZIMMER, SCHWELLE_OBEN_SCHLAFZIMMER };

  temperatur_kacheln[2] = { start_x + (tile_w + gap_x) * 2, start_y, tile_w, tile_h,
    FARBE_TEMP_BADEZIMMER, "Badezimmer", false,
    "", "", 0, 0,
    SCHWELLE_UNTEN_BADEZIMMER, SCHWELLE_OBEN_BADEZIMMER };

  // Reihe 2
  int y2 = start_y + tile_h + gap_y;
  temperatur_kacheln[3] = { start_x, y2, tile_w, tile_h,
    FARBE_TEMP_BALKON, "Balkon", true,
    ENTITY_TEMP_BALKON, ENTITY_FEUCHTE_BALKON, 0, 0,
    SCHWELLE_UNTEN_BALKON, SCHWELLE_OBEN_BALKON };

  temperatur_kacheln[4] = { start_x + (tile_w + gap_x), y2, tile_w, tile_h,
    FARBE_TEMP_SERVERSCHRANK, "Serverschrank", true,
    ENTITY_TEMP_SERVERSCHRANK, ENTITY_FEUCHTE_SERVERSCHRANK, 0, 0,
    SCHWELLE_UNTEN_SERVERSCHRANK, SCHWELLE_OBEN_SERVERSCHRANK };

  temperatur_kacheln[5] = { start_x + (tile_w + gap_x) * 2, y2, tile_w, tile_h,
    FARBE_TEMP_KINDERZIMMER, "Kinderzimmer", false,
    "", "", 0, 0,
    SCHWELLE_UNTEN_KINDERZIMMER, SCHWELLE_OBEN_KINDERZIMMER };
}

// ----------------------------------------------------------------------------
//  EINEN EINZELNEN ZUSTANDSWERT VON HOME ASSISTANT HOLEN
//  Gibt den Zustand (state) als float zurueck, oder NAN bei Fehler
// ----------------------------------------------------------------------------
float ha_sensorwert_holen(const char* entity_id) {
  if (WiFi.status() != WL_CONNECTED) return NAN;
  if (entity_id == nullptr || strlen(entity_id) == 0) return NAN;

  HTTPClient http;
  String url = "http://" + String(HA_HOST) + ":" + String(HA_PORT) +
               "/api/states/" + String(entity_id);

  http.begin(url);
  http.addHeader("Authorization", "Bearer " + String(HA_TOKEN));
  http.addHeader("Content-Type", "application/json");

  int http_code = http.GET();
  float ergebnis = NAN;

  if (http_code == 200) {
    String antwort = http.getString();

    StaticJsonDocument<512> doc;
    DeserializationError fehler = deserializeJson(doc, antwort);

    if (!fehler) {
      const char* zustand_text = doc["state"];
      if (zustand_text != nullptr) {
        ergebnis = atof(zustand_text);
      }
    } else {
      Serial.print("HA JSON-Fehler bei ");
      Serial.print(entity_id);
      Serial.print(": ");
      Serial.println(fehler.c_str());
    }
  } else {
    Serial.print("HA HTTP-Fehler bei ");
    Serial.print(entity_id);
    Serial.print(": ");
    Serial.println(http_code);
  }

  http.end();
  return ergebnis;
}

// ----------------------------------------------------------------------------
//  SENSORWERTE AKTUALISIEREN
//  Holt fuer jede Kachel mit hat_sensor == true Temperatur und Feuchte
//  von Home Assistant. Regelmaessig in loop() aufrufen (siehe
//  temperaturseite_periodisch_aktualisieren() weiter unten).
// ----------------------------------------------------------------------------
void temperaturwerte_aktualisieren() {
  for (int i = 0; i < 6; i++) {
    TemperaturKachel &k = temperatur_kacheln[i];
    if (!k.hat_sensor) continue;

    float neuer_temp = ha_sensorwert_holen(k.entity_temp);
    if (!isnan(neuer_temp)) {
      k.wert_temp = neuer_temp;
    }

    float neue_feuchte = ha_sensorwert_holen(k.entity_feuchte);
    if (!isnan(neue_feuchte)) {
      k.wert_feuchte = neue_feuchte;
    }
  }
}

// ----------------------------------------------------------------------------
//  PERIODISCHE AKTUALISIERUNG - REGELMAESSIG IN loop() AUFRUFEN,
//  WENN view_index == 1 (nur dann, um unnoetige HA-Abfragen zu vermeiden)
// ----------------------------------------------------------------------------
void temperaturseite_periodisch_aktualisieren() {
  unsigned long jetzt = millis();
  if (jetzt - temperatur_letzte_aktualisierung < TEMPERATUR_AKTUALISIERUNG_MS) return;
  temperatur_letzte_aktualisierung = jetzt;

  temperaturwerte_aktualisieren();
  temperaturseite_zeichnen();
}

// ----------------------------------------------------------------------------
//  FARBE FUER EINEN TEMPERATURWERT ANHAND DER SCHWELLEN BESTIMMEN
// ----------------------------------------------------------------------------
uint16_t temperatur_ampelfarbe(float wert, float unten, float oben) {
  if (wert <= unten) return FARBE_TEMP_KALT;
  if (wert >= oben) return FARBE_TEMP_WARM;
  return FARBE_TEMP_NORMAL;
}

// ----------------------------------------------------------------------------
//  TEMPERATURSEITE ZEICHNEN
//  Bei view_index == 1 aus bildschirm_komplett_neu_zeichnen() aufrufen
// ----------------------------------------------------------------------------
void temperaturseite_zeichnen() {
  for (int i = 0; i < 6; i++) {
    TemperaturKachel &k = temperatur_kacheln[i];

    tft.fillRoundRect(k.x, k.y, k.breite, k.hoehe, 8, k.farbe);
    tft.drawRoundRect(k.x, k.y, k.breite, k.hoehe, 8, FARBE_RAHMEN);

    int mitte_x = k.x + k.breite / 2;

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(FARBE_TEXT, k.farbe);
    tft.drawString(k.name, mitte_x, k.y + 16, 1);

    if (k.hat_sensor) {
      uint16_t ampelfarbe = temperatur_ampelfarbe(k.wert_temp, k.schwelle_unten, k.schwelle_oben);
      tft.setTextColor(ampelfarbe, k.farbe);
      char temp_text[12];
      snprintf(temp_text, sizeof(temp_text), "%.1f C", k.wert_temp);
      tft.drawString(temp_text, mitte_x, k.y + 42, 2);

      tft.setTextColor(FARBE_TEXT, k.farbe);
      char feuchte_text[10];
      snprintf(feuchte_text, sizeof(feuchte_text), "%.0f %%", k.wert_feuchte);
      tft.drawString(feuchte_text, mitte_x, k.y + 66, 1);
    } else {
      // Platzhalter-Kachel ohne echten Sensor
      tft.setTextColor(tft.color565(200, 200, 200), k.farbe);
      tft.drawString("-- C", mitte_x, k.y + 42, 2);
      tft.drawString("kein Sensor", mitte_x, k.y + 66, 1);
    }
  }
}