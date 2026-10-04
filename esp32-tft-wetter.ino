// ============================================================================
//  WETTER-SEITE (view_index == 2)
//  Eigene Datei: esp32_tft_wetter.ino
//  Zeigt aktuelle Wetterdaten der Station Duesseldorf-Bilk (HA-Entity
//  weather.dusseldorf_bilk) und wechselt alle 30 Sekunden zwischen
//  Detailansicht und 3-Tage-Vorschau.
//
//  HOME-ASSISTANT-ANBINDUNG:
//  - Aktuelle Werte (Temperatur, Feuchte, Druck, Wind, Zustand) kommen als
//    State/Attribute der Entity ueber GET /api/states/<entity_id>
//  - Die 3-Tage-Vorschau kommt ueber den Service-Aufruf
//    POST /api/services/weather/get_forecasts?return_response
//    (neuere Home-Assistant-Versionen liefern Vorschauen nicht mehr als
//    einfaches Attribut, sondern nur noch ueber diesen Weg)
//  HA_HOST / HA_PORT / HA_TOKEN sind bereits in esp32-tft-temp.ino definiert.
// ============================================================================

#include <HTTPClient.h>
#include <ArduinoJson.h>

// ----------------------------------------------------------------------------
//  ENTITY-ID DER WETTERSTATION
// ----------------------------------------------------------------------------
const char* ENTITY_WETTER = "weather.dusseldorf_bilk";

// wie oft die Wetterdaten (Details + Vorschau) neu geholt werden
#define WETTER_DATEN_INTERVALL_MS   300000   // alle 5 Minuten
// wie oft zwischen Detailansicht und 3-Tage-Vorschau gewechselt wird
#define WETTER_ANSICHT_INTERVALL_MS  30000   // alle 30 Sekunden

unsigned long wetter_letzte_datenaktualisierung = 0;
unsigned long wetter_letzter_ansicht_wechsel = 0;
int wetter_aktuelle_ansicht = 0;   // 0 = Details, 1 = 3-Tage-Vorschau

// ----------------------------------------------------------------------------
//  AKTUELLE WETTERWERTE
// ----------------------------------------------------------------------------
float wetter_temperatur = 0.0;
float wetter_luftfeuchte = 0.0;
float wetter_wind = 0.0;
float wetter_luftdruck = 0.0;
String wetter_zustand = "cloudy";   // moegliche Werte siehe Zuordnung unten

// ----------------------------------------------------------------------------
//  3-TAGE-VORSCHAU
// ----------------------------------------------------------------------------
struct WetterVorschauTag {
  String datum;        // "DD.MM." extrahiert aus dem HA-Datetime-String
  float temp_hoch;
  float temp_tief;
  String zustand;
};
WetterVorschauTag wetter_vorschau[3];
int wetter_vorschau_anzahl = 0;

// ----------------------------------------------------------------------------
//  ZUSTANDS-ZUORDNUNG: HA-Zustand -> Anzeigetext, Symbol, Farbe
//  Die Symbole sind einfache, mit TFT_eSPI gezeichnete Formen
//  (kein Icon-Font noetig, dadurch unabhaengig von zusaetzlichen Bibliotheken)
// ----------------------------------------------------------------------------

struct WetterZustand {
  const char* schluessel;   // HA-Zustandswert
  const char* anzeigetext;
  uint16_t farbe;
};

WetterZustand wetter_zustaende[] = {
  { "sunny",          "Sonnig",             0 },
  { "clear-night",     "Klar",               0 },
  { "partlycloudy",    "Teils bewoelkt",     0 },
  { "cloudy",           "Bewoelkt",           0 },
  { "rainy",            "Regen",              0 },
  { "pouring",          "Starkregen",         0 },
  { "snowy",             "Schnee",             0 },
  { "snowy-rainy",       "Schneeregen",        0 },
  { "fog",                "Nebel",              0 },
  { "windy",               "Windig",             0 },
  { "lightning",            "Gewitter",           0 },
  { "lightning-rainy",       "Gewitter mit Regen", 0 },
  { "hail",                    "Hagel",              0 }
};
#define ANZAHL_WETTERZUSTAENDE 13

// ----------------------------------------------------------------------------
//  FARBE FUER EINEN WETTERZUSTAND ERMITTELN (zur Laufzeit, da tft.color565
//  erst nach tft.init() nutzbar ist)
// ----------------------------------------------------------------------------
uint16_t wetter_farbe_fuer_zustand(String zustand) {
  if (zustand == "sunny")              return tft.color565(255, 200, 0);
  if (zustand == "clear-night")        return tft.color565(180, 190, 255);
  if (zustand == "partlycloudy")       return tft.color565(230, 210, 120);
  if (zustand == "cloudy")             return tft.color565(200, 200, 200);
  if (zustand == "rainy")              return tft.color565(70, 150, 255);
  if (zustand == "pouring")            return tft.color565(30, 100, 255);
  if (zustand == "snowy")              return tft.color565(220, 240, 255);
  if (zustand == "snowy-rainy")        return tft.color565(150, 200, 255);
  if (zustand == "fog")                return tft.color565(170, 170, 180);
  if (zustand == "windy")              return tft.color565(120, 220, 200);
  if (zustand == "lightning")          return tft.color565(255, 230, 0);
  if (zustand == "lightning-rainy")    return tft.color565(255, 230, 0);
  if (zustand == "hail")               return tft.color565(180, 220, 255);
  return tft.color565(200, 200, 200);
}

String wetter_text_fuer_zustand(String zustand) {
  for (int i = 0; i < ANZAHL_WETTERZUSTAENDE; i++) {
    if (zustand == wetter_zustaende[i].schluessel) {
      return String(wetter_zustaende[i].anzeigetext);
    }
  }
  return "Unbekannt";
}

// ----------------------------------------------------------------------------
//  EINFACHES WETTERSYMBOL ZEICHNEN (gezeichnete Form statt Icon-Font)
//  mitte_x/mitte_y = Zentrum des Symbols, radius = ungefaehre Groesse
// ----------------------------------------------------------------------------
void wettersymbol_zeichnen(int mitte_x, int mitte_y, int radius, String zustand, uint16_t farbe) {
  if (zustand == "sunny") {
    tft.fillCircle(mitte_x, mitte_y, radius / 2, farbe);
    for (int winkel = 0; winkel < 360; winkel += 45) {
      float rad = winkel * 3.14159 / 180.0;
      int x1 = mitte_x + cos(rad) * (radius / 2 + 4);
      int y1 = mitte_y + sin(rad) * (radius / 2 + 4);
      int x2 = mitte_x + cos(rad) * (radius + 4);
      int y2 = mitte_y + sin(rad) * (radius + 4);
      tft.drawLine(x1, y1, x2, y2, farbe);
    }
  } else if (zustand == "clear-night") {
    tft.fillCircle(mitte_x, mitte_y, radius / 2, farbe);
    tft.fillCircle(mitte_x + radius / 3, mitte_y - radius / 4, radius / 2 - 2, FARBE_HINTERGRUND);
  } else if (zustand == "rainy" || zustand == "pouring" || zustand == "snowy-rainy") {
    tft.fillRoundRect(mitte_x - radius, mitte_y - radius / 3, radius * 2, radius / 2, 8, tft.color565(200, 200, 200));
    for (int i = -1; i <= 1; i++) {
      tft.drawLine(mitte_x + i * 10, mitte_y + radius / 4, mitte_x + i * 10 - 4, mitte_y + radius, farbe);
    }
  } else if (zustand == "snowy") {
    tft.fillRoundRect(mitte_x - radius, mitte_y - radius / 3, radius * 2, radius / 2, 8, tft.color565(200, 200, 200));
    for (int i = -1; i <= 1; i++) {
      tft.fillCircle(mitte_x + i * 10, mitte_y + radius / 2, 2, farbe);
    }
  } else if (zustand == "lightning" || zustand == "lightning-rainy") {
    tft.fillRoundRect(mitte_x - radius, mitte_y - radius / 3, radius * 2, radius / 2, 8, tft.color565(160, 160, 170));
    tft.fillTriangle(mitte_x - 4, mitte_y + radius / 4, mitte_x + 6, mitte_y + radius / 4, mitte_x - 2, mitte_y + radius, farbe);
  } else if (zustand == "fog") {
    for (int i = 0; i < 4; i++) {
      tft.drawFastHLine(mitte_x - radius, mitte_y - radius / 2 + i * 8, radius * 2, farbe);
    }
  } else if (zustand == "windy") {
    for (int i = 0; i < 3; i++) {
      tft.drawFastHLine(mitte_x - radius, mitte_y - radius / 2 + i * 10, radius * 2 - i * 10, farbe);
    }
  } else if (zustand == "hail") {
    tft.fillRoundRect(mitte_x - radius, mitte_y - radius / 3, radius * 2, radius / 2, 8, tft.color565(200, 200, 200));
    for (int i = -1; i <= 1; i++) {
      tft.fillCircle(mitte_x + i * 10, mitte_y + radius / 2, 3, farbe);
    }
  } else {
    // cloudy, partlycloudy, Standardfall: Wolke
    tft.fillRoundRect(mitte_x - radius, mitte_y - radius / 3, radius * 2, radius / 2, 8, farbe);
    tft.fillCircle(mitte_x - radius / 2, mitte_y - radius / 3, radius / 2, farbe);
    tft.fillCircle(mitte_x + radius / 3, mitte_y - radius / 2, radius / 2, farbe);
    if (zustand == "partlycloudy") {
      tft.fillCircle(mitte_x - radius - 6, mitte_y - radius, radius / 3, tft.color565(255, 200, 0));
    }
  }
}

// ----------------------------------------------------------------------------
//  AKTUELLE WERTE VON HOME ASSISTANT HOLEN
//  Ein GET auf /api/states/<entity>, State + Attribute in einem Rutsch
// ----------------------------------------------------------------------------
void wetterwerte_aktualisieren() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "http://" + String(HA_HOST) + ":" + String(HA_PORT) +
               "/api/states/" + String(ENTITY_WETTER);

  http.begin(url);
  http.addHeader("Authorization", "Bearer " + String(HA_TOKEN));
  http.addHeader("Content-Type", "application/json");

  int http_code = http.GET();

  if (http_code == 200) {
    String antwort = http.getString();

    // Filter: nur benoetigte Felder parsen, spart Speicher
    StaticJsonDocument<160> filter;
    filter["state"] = true;
    filter["attributes"]["temperature"] = true;
    filter["attributes"]["humidity"] = true;
    filter["attributes"]["pressure"] = true;
    filter["attributes"]["wind_speed"] = true;

    StaticJsonDocument<384> doc;
    DeserializationError fehler = deserializeJson(doc, antwort, DeserializationOption::Filter(filter));

    if (!fehler) {
      const char* zustand_text = doc["state"];
      if (zustand_text != nullptr) wetter_zustand = String(zustand_text);

      if (!doc["attributes"]["temperature"].isNull())
        wetter_temperatur = doc["attributes"]["temperature"];
      if (!doc["attributes"]["humidity"].isNull())
        wetter_luftfeuchte = doc["attributes"]["humidity"];
      if (!doc["attributes"]["pressure"].isNull())
        wetter_luftdruck = doc["attributes"]["pressure"];
      if (!doc["attributes"]["wind_speed"].isNull())
        wetter_wind = doc["attributes"]["wind_speed"];
    } else {
      Serial.print("Wetter JSON-Fehler: ");
      Serial.println(fehler.c_str());
    }
  } else {
    Serial.print("Wetter HTTP-Fehler: ");
    Serial.println(http_code);
  }

  http.end();
}

// ----------------------------------------------------------------------------
//  3-TAGE-VORSCHAU VON HOME ASSISTANT HOLEN
//  Service-Aufruf weather.get_forecasts mit return_response
// ----------------------------------------------------------------------------
void wettervorschau_aktualisieren() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  String url = "http://" + String(HA_HOST) + ":" + String(HA_PORT) +
               "/api/services/weather/get_forecasts?return_response";

  http.begin(url);
  http.addHeader("Authorization", "Bearer " + String(HA_TOKEN));
  http.addHeader("Content-Type", "application/json");

  String body = "{\"entity_id\":\"" + String(ENTITY_WETTER) + "\",\"type\":\"daily\"}";
  int http_code = http.POST(body);

  wetter_vorschau_anzahl = 0;

  if (http_code == 200) {
    String antwort = http.getString();

    // Filter: nur die ersten drei Vorschau-Tage mit den benoetigten Feldern
    StaticJsonDocument<256> filter;
    JsonObject f_tag = filter["service_response"][ENTITY_WETTER]["forecast"][0].to<JsonObject>();
    f_tag["datetime"] = true;
    f_tag["temperature"] = true;
    f_tag["templow"] = true;
    f_tag["condition"] = true;

    DynamicJsonDocument doc(1536);
    DeserializationError fehler = deserializeJson(doc, antwort, DeserializationOption::Filter(filter));

    if (!fehler) {
      JsonArray tage = doc["service_response"][ENTITY_WETTER]["forecast"].as<JsonArray>();
      int i = 0;
      for (JsonObject tag : tage) {
        if (i >= 3) break;

        const char* datetime_text = tag["datetime"];
        if (datetime_text != nullptr && strlen(datetime_text) >= 10) {
          // "YYYY-MM-DDTHH:MM:SS+00:00" -> "DD.MM."
          char datum_kurz[7];
          datum_kurz[0] = datetime_text[8];
          datum_kurz[1] = datetime_text[9];
          datum_kurz[2] = '.';
          datum_kurz[3] = datetime_text[5];
          datum_kurz[4] = datetime_text[6];
          datum_kurz[5] = '.';
          datum_kurz[6] = '\0';
          wetter_vorschau[i].datum = String(datum_kurz);
        } else {
          wetter_vorschau[i].datum = "--.--.";
        }

        wetter_vorschau[i].temp_hoch = tag["temperature"] | 0.0;
        wetter_vorschau[i].temp_tief = tag["templow"] | 0.0;
        const char* zustand_text = tag["condition"];
        wetter_vorschau[i].zustand = (zustand_text != nullptr) ? String(zustand_text) : "cloudy";

        i++;
      }
      wetter_vorschau_anzahl = i;
    } else {
      Serial.print("Vorschau JSON-Fehler: ");
      Serial.println(fehler.c_str());
    }
  } else {
    Serial.print("Vorschau HTTP-Fehler: ");
    Serial.println(http_code);
  }

  http.end();
}

// ----------------------------------------------------------------------------
//  PERIODISCHE AKTUALISIERUNG UND ANSICHTSWECHSEL
//  Regelmaessig in loop() aufrufen, wenn view_index == 2 und nicht Standby
// ----------------------------------------------------------------------------
void wetterseite_periodisch_aktualisieren() {
  unsigned long jetzt = millis();

  if (jetzt - wetter_letzte_datenaktualisierung > WETTER_DATEN_INTERVALL_MS) {
    wetter_letzte_datenaktualisierung = jetzt;
    wetterwerte_aktualisieren();
    wettervorschau_aktualisieren();
    wetterseite_zeichnen();
  }

  if (jetzt - wetter_letzter_ansicht_wechsel > WETTER_ANSICHT_INTERVALL_MS) {
    wetter_letzter_ansicht_wechsel = jetzt;
    wetter_aktuelle_ansicht = (wetter_aktuelle_ansicht + 1) % 2;
    wetterseite_zeichnen();
  }
}

// ----------------------------------------------------------------------------
//  WETTERSEITE ZEICHNEN - VERTEILER JE NACH AKTUELLER ANSICHT
//  Bei view_index == 2 aus bildschirm_komplett_neu_zeichnen() aufrufen
// ----------------------------------------------------------------------------
void wetterseite_zeichnen() {
  tft.fillRect(0, HEADER_HOEHE, SCREEN_W, SCREEN_H - HEADER_HOEHE, FARBE_HINTERGRUND);

  if (wetter_aktuelle_ansicht == 0) {
    wetterseite_details_zeichnen();
  } else {
    wetterseite_vorschau_zeichnen();
  }
}

// ----------------------------------------------------------------------------
//  DETAILANSICHT: Symbol, Temperatur, Zustand, Feuchte, Wind, Druck
// ----------------------------------------------------------------------------
void wetterseite_details_zeichnen() {
  int mitte_x = SCREEN_W / 2;

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TEXT, FARBE_HINTERGRUND);
  tft.drawString("WETTER BILK", mitte_x, HEADER_HOEHE + 18, 2);

  uint16_t zustandsfarbe = wetter_farbe_fuer_zustand(wetter_zustand);
  String zustandstext = wetter_text_fuer_zustand(wetter_zustand);

  wettersymbol_zeichnen(90, HEADER_HOEHE + 65, 26, wetter_zustand, zustandsfarbe);

  tft.setTextColor(FARBE_TEXT, FARBE_HINTERGRUND);
  char temp_text[12];
  snprintf(temp_text, sizeof(temp_text), "%.1f C", wetter_temperatur);
  tft.drawString(temp_text, 200, HEADER_HOEHE + 65, 4);

  tft.drawString(zustandstext, mitte_x, HEADER_HOEHE + 105, 2);

  char feuchte_text[32];
  snprintf(feuchte_text, sizeof(feuchte_text), "Luftfeuchtigkeit: %.0f %%", wetter_luftfeuchte);
  tft.drawString(feuchte_text, mitte_x, HEADER_HOEHE + 140, 1);

  char wind_text[24];
  snprintf(wind_text, sizeof(wind_text), "Wind: %.0f km/h", wetter_wind);
  tft.drawString(wind_text, mitte_x, HEADER_HOEHE + 162, 1);

  char druck_text[24];
  snprintf(druck_text, sizeof(druck_text), "Luftdruck: %.0f hPa", wetter_luftdruck);
  tft.drawString(druck_text, mitte_x, HEADER_HOEHE + 184, 1);
}

// ----------------------------------------------------------------------------
//  3-TAGE-VORSCHAU: drei Spalten mit Datum, Symbol, Hoch/Tief
// ----------------------------------------------------------------------------
void wetterseite_vorschau_zeichnen() {
  int mitte_x = SCREEN_W / 2;

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TEXT, FARBE_HINTERGRUND);
  tft.drawString("3-TAGE-VORSCHAU", mitte_x, HEADER_HOEHE + 18, 2);

  if (wetter_vorschau_anzahl == 0) {
    tft.drawString("Keine Vorschaudaten", mitte_x, SCREEN_H / 2, 2);
    return;
  }

  int spalten_breite = SCREEN_W / 3;

  for (int i = 0; i < wetter_vorschau_anzahl; i++) {
    int spalte_mitte_x = spalten_breite * i + spalten_breite / 2;
    uint16_t farbe = wetter_farbe_fuer_zustand(wetter_vorschau[i].zustand);

    tft.drawString(wetter_vorschau[i].datum, spalte_mitte_x, HEADER_HOEHE + 45, 1);

    wettersymbol_zeichnen(spalte_mitte_x, HEADER_HOEHE + 90, 22, wetter_vorschau[i].zustand, farbe);

    char hoch_tief_text[16];
    snprintf(hoch_tief_text, sizeof(hoch_tief_text), "%.0f / %.0f", wetter_vorschau[i].temp_hoch, wetter_vorschau[i].temp_tief);
    tft.drawString(hoch_tief_text, spalte_mitte_x, HEADER_HOEHE + 140, 2);

    tft.setTextColor(FARBE_TEXT, FARBE_HINTERGRUND);
    tft.drawString(wetter_text_fuer_zustand(wetter_vorschau[i].zustand), spalte_mitte_x, HEADER_HOEHE + 165, 1);
  }
}