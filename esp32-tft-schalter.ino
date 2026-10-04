// ============================================================================
//  SCHALTER-SEITE (view_index == 4)
//  Eigene Datei: esp32-tft-schalter.ino
//
//  NOTIZEN FUER DIE HOME-ASSISTANT-VERKNUEPFUNG (bitte vor Aktivierung lesen):
//
//  1) Fuer jeden Schalter unten bei "ENTITY-IDS" die Platzhalter-Zeile
//     (z.B. "switch.schalter_1_beispiel") durch die echte Entity-ID aus
//     Home Assistant ersetzen (zu finden unter Einstellungen -> Geraete &
//     Dienste -> Entitaeten, oder Entwicklerwerkzeuge -> Zustaende).
//  2) Den Anzeigenamen jedes Schalters (aktuell "Schalter 1" bis "Schalter 6")
//     im Bereich "SCHALTER-NAMEN" auf den gewuenschten Klartext aendern.
//  3) Aktuell wird nur der lokale Zustand (an/aus) im ESP32 gehalten und
//     farblich dargestellt - es findet NOCH KEINE echte Kommunikation mit
//     Home Assistant statt. Sobald die API- bzw. MQTT-Anbindung im
//     Hauptprogramm steht, muss beim Antippen zusaetzlich der Befehl
//     "Zustand umschalten" an die jeweilige Entity-ID gesendet werden
//     (z.B. per Home Assistant API: Dienst homeassistant.toggle mit der
//     jeweiligen Entity-ID) - siehe TODO in schalter_touch_behandeln().
//  4) Ebenso sollte der angezeigte Zustand idealerweise regelmaessig aus
//     Home Assistant abgefragt werden (z.B. alle paar Sekunden per API),
//     damit die Anzeige stimmt, auch wenn der Schalter anderswo (App,
//     Sprachassistent) umgestellt wurde. Aktuell simuliert
//     schalter_zustaende_aktualisieren() das nur als TODO-Platzhalter.
//  5) Falls ein Schalter kein einfacher an/aus-Schalter ist, sondern z.B.
//     ein Skript oder eine Szene ausloesen soll: dafuer reicht ebenfalls
//     ein Toggle- bzw. Trigger-Aufruf, aber ohne dauerhaften an/aus-Zustand -
//     in dem Fall ggf. hat_dauerzustand bei der jeweiligen Kachel auf
//     false setzen (siehe Datenstruktur SchalterKachel weiter unten).
// ============================================================================

// ----------------------------------------------------------------------------
//  SCHALTER-NAMEN  (hier Klartext-Bezeichnung nach Belieben aendern)
// ----------------------------------------------------------------------------
const char* SCHALTER_NAME_1 = "Schalter 1";
const char* SCHALTER_NAME_2 = "Schalter 2";
const char* SCHALTER_NAME_3 = "Schalter 3";
const char* SCHALTER_NAME_4 = "Schalter 4";
const char* SCHALTER_NAME_5 = "Schalter 5";
const char* SCHALTER_NAME_6 = "Schalter 6";

// ----------------------------------------------------------------------------
//  ENTITY-IDS  (hier durch die echten Home-Assistant-Entity-IDs ersetzen)
// ----------------------------------------------------------------------------
const char* SCHALTER_ENTITY_1 = "switch.schalter_1_beispiel";
const char* SCHALTER_ENTITY_2 = "switch.schalter_2_beispiel";
const char* SCHALTER_ENTITY_3 = "switch.schalter_3_beispiel";
const char* SCHALTER_ENTITY_4 = "switch.schalter_4_beispiel";
const char* SCHALTER_ENTITY_5 = "switch.schalter_5_beispiel";
const char* SCHALTER_ENTITY_6 = "switch.schalter_6_beispiel";

// ----------------------------------------------------------------------------
//  FARBEN
// ----------------------------------------------------------------------------
#define FARBE_SCHALTER_AUS        tft.color565(40, 55, 80)
#define FARBE_SCHALTER_AN         tft.color565(30, 140, 90)
#define FARBE_RAHMEN_GEDRUECKT    tft.color565(255, 200, 0)

// ----------------------------------------------------------------------------
//  DATENSTRUKTUR EINER SCHALTER-KACHEL
// ----------------------------------------------------------------------------
struct SchalterKachel {
  int x, y, breite, hoehe;
  const char* name;
  const char* entity_id;
  bool zustand_an;
  bool hat_dauerzustand;   // true = an/aus-Schalter, false = Taster/Szene
};

SchalterKachel schalter_kacheln[6];

// merkt sich, welche Kachel gerade gedrueckt/angetippt wird, fuer die
// farbliche Rahmen-Hervorhebung
int schalter_gedrueckt_index = -1;
unsigned long schalter_gedrueckt_zeit = 0;
#define SCHALTER_GEDRUECKT_ANZEIGEDAUER 200  // ms

// ----------------------------------------------------------------------------
//  LAYOUT AUFBAUEN (einmalig beim Start aufrufen, z.B. in setup())
// ----------------------------------------------------------------------------
void schalterseite_layout_aufbauen() {
  int tile_w = 90;
  int tile_h = 90;
  int gap_x = 12;
  int gap_y = 14;
  int start_x = (SCREEN_W - (tile_w * 3 + gap_x * 2)) / 2;
  int start_y = HEADER_HOEHE + 15;

  schalter_kacheln[0] = { start_x, start_y, tile_w, tile_h,
    SCHALTER_NAME_1, SCHALTER_ENTITY_1, false, true };

  schalter_kacheln[1] = { start_x + (tile_w + gap_x), start_y, tile_w, tile_h,
    SCHALTER_NAME_2, SCHALTER_ENTITY_2, false, true };

  schalter_kacheln[2] = { start_x + (tile_w + gap_x) * 2, start_y, tile_w, tile_h,
    SCHALTER_NAME_3, SCHALTER_ENTITY_3, false, true };

  int y2 = start_y + tile_h + gap_y;

  schalter_kacheln[3] = { start_x, y2, tile_w, tile_h,
    SCHALTER_NAME_4, SCHALTER_ENTITY_4, false, true };

  schalter_kacheln[4] = { start_x + (tile_w + gap_x), y2, tile_w, tile_h,
    SCHALTER_NAME_5, SCHALTER_ENTITY_5, false, true };

  schalter_kacheln[5] = { start_x + (tile_w + gap_x) * 2, y2, tile_w, tile_h,
    SCHALTER_NAME_6, SCHALTER_ENTITY_6, false, true };
}

// ----------------------------------------------------------------------------
//  SCHALTER-SEITE ZEICHNEN
//  Bei view_index == 4 aus bildschirm_komplett_neu_zeichnen() aufrufen
//  (Kopfzeile mit Uhrzeit und Home-Button wird wie gewohnt vorher vom
//  Hauptprogramm gezeichnet, dunkelblauer Standard-Hintergrund gilt weiter)
// ----------------------------------------------------------------------------
void schalterseite_zeichnen() {
  for (int i = 0; i < 6; i++) {
    SchalterKachel &k = schalter_kacheln[i];

    uint16_t grundfarbe = k.zustand_an ? FARBE_SCHALTER_AN : FARBE_SCHALTER_AUS;
    uint16_t rahmenfarbe = (i == schalter_gedrueckt_index) ? FARBE_RAHMEN_GEDRUECKT : FARBE_RAHMEN;

    tft.fillRoundRect(k.x, k.y, k.breite, k.hoehe, 8, grundfarbe);
    tft.drawRoundRect(k.x, k.y, k.breite, k.hoehe, 8, rahmenfarbe);
    if (i == schalter_gedrueckt_index) {
      // zusaetzliche zweite Umrandung fuer deutlichere farbliche Abhebung
      tft.drawRoundRect(k.x + 1, k.y + 1, k.breite - 2, k.hoehe - 2, 7, FARBE_RAHMEN_GEDRUECKT);
    }

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(FARBE_TEXT, grundfarbe);
    tft.drawString(k.name, k.x + k.breite / 2, k.y + k.hoehe / 2 - 10, 1);

    tft.setTextColor(FARBE_TEXT, grundfarbe);
    tft.drawString(k.zustand_an ? "AN" : "AUS", k.x + k.breite / 2, k.y + k.hoehe / 2 + 15, 1);
  }
}

// ----------------------------------------------------------------------------
//  TOUCH-BEHANDLUNG FUER DIE SCHALTER-SEITE
//  In touch_abfragen() aufrufen, wenn view_index == 4
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

      // TODO: sobald die Home-Assistant-Anbindung im Hauptprogramm steht,
      // hier den Toggle-Befehl fuer k.entity_id an Home Assistant senden,
      // z.B. per API-Dienst homeassistant.toggle mit entity_id = k.entity_id

      schalterseite_zeichnen();
      return;
    }
  }
}

// ----------------------------------------------------------------------------
//  GEDRUECKT-RAHMEN NACH KURZER ZEIT WIEDER ENTFERNEN
//  Regelmaessig in loop() aufrufen, wenn view_index == 4
// ----------------------------------------------------------------------------
void schalterseite_blink_zuruecksetzen() {
  if (schalter_gedrueckt_index != -1 &&
      millis() - schalter_gedrueckt_zeit > SCHALTER_GEDRUECKT_ANZEIGEDAUER) {
    schalter_gedrueckt_index = -1;
    schalterseite_zeichnen();
  }
}

// ----------------------------------------------------------------------------
//  ZUSTAENDE AUS HOME ASSISTANT AKTUALISIEREN
//  TODO-Platzhalter: sobald API-/MQTT-Anbindung steht, hier fuer jede
//  Entity-ID den aktuellen Zustand abfragen und schalter_kacheln[i].zustand_an
//  entsprechend setzen, danach schalterseite_zeichnen() aufrufen
// ----------------------------------------------------------------------------
void schalterzustaende_aktualisieren() {
  // TODO: Home-Assistant-Abfrage ergaenzen
}