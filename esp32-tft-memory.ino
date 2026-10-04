// ============================================================================
//  SPIEL: MEMORY (spiel_aktiv == SPIEL_MEMORY)
//  Eigene Datei: esp32-tft-memory.ino
//
//  Regeln (2 Spieler abwechselnd am selben Geraet):
//  - 24 Karten (12 Paare, Werte 1-12), verdeckt in einem 6x4-Raster
//  - Ein Spielzug: zwei Karten antippen und aufdecken
//  - Passen die Werte zusammen: Karten bleiben aufgedeckt, Punkt fuer den
//    aktuellen Spieler, derselbe Spieler ist erneut am Zug
//  - Passen sie nicht: nach kurzer Anzeigezeit werden beide wieder
//    verdeckt und der andere Spieler ist am Zug
//  - Sind alle Paare gefunden, endet das Spiel mit Punktestand/Sieger
// ============================================================================

// ----------------------------------------------------------------------------
//  SPIELFELD-EINSTELLUNGEN
// ----------------------------------------------------------------------------
#define MEMORY_SPALTEN        6
#define MEMORY_ZEILEN         4
#define MEMORY_ANZAHL_KARTEN  (MEMORY_SPALTEN * MEMORY_ZEILEN)   // 24
#define MEMORY_ANZAHL_PAARE   (MEMORY_ANZAHL_KARTEN / 2)          // 12

#define MEMORY_KARTE_BREITE   48
#define MEMORY_KARTE_HOEHE    44
#define MEMORY_GAP              2
#define MEMORY_FELD_X           10
#define MEMORY_FELD_Y           (HEADER_HOEHE + 26)

#define MEMORY_VERGLEICH_WARTEZEIT_MS  800

// ----------------------------------------------------------------------------
//  PHASEN
// ----------------------------------------------------------------------------
#define MEMORY_PHASE_SPIEL  0
#define MEMORY_PHASE_ENDE   1

int memory_phase = MEMORY_PHASE_SPIEL;

// ----------------------------------------------------------------------------
//  FARBEN
// ----------------------------------------------------------------------------
#define FARBE_MEMORY_HINTERGRUND        tft.color565(15, 25, 45)
#define FARBE_MEMORY_KARTE_VERDECKT     tft.color565(60, 90, 160)
#define FARBE_MEMORY_KARTE_AUFGEDECKT   tft.color565(35, 45, 70)
#define FARBE_MEMORY_KARTE_GEFUNDEN     tft.color565(40, 130, 60)
#define FARBE_MEMORY_TEXT               tft.color565(255, 255, 255)
#define FARBE_MEMORY_RAHMEN             tft.color565(90, 90, 100)
#define FARBE_MEMORY_SPIELER1           tft.color565(60, 130, 220)
#define FARBE_MEMORY_SPIELER2           tft.color565(220, 120, 40)

// ----------------------------------------------------------------------------
//  DATENSTRUKTUR EINE KARTE
// ----------------------------------------------------------------------------
struct MemoryKarte {
  int wert;          // 1..MEMORY_ANZAHL_PAARE, Paarkennung
  bool aufgedeckt;    // aktuell sichtbar (temporaer waehrend Zug, oder dauerhaft wenn gefunden)
  bool gefunden;       // dauerhaft aufgedeckt, da erfolgreich gepaart
};

MemoryKarte memory_karten[MEMORY_ANZAHL_KARTEN];

// ----------------------------------------------------------------------------
//  ZUG-ZUSTAND
// ----------------------------------------------------------------------------
int memory_erste_auswahl = -1;
int memory_zweite_auswahl = -1;
bool memory_wartet_auf_verdecken = false;
unsigned long memory_wartezeit_start = 0;

int memory_punkte_spieler1 = 0;
int memory_punkte_spieler2 = 0;
int memory_aktueller_spieler = 1;   // 1 oder 2

// ----------------------------------------------------------------------------
//  HILFSFUNKTIONEN
// ----------------------------------------------------------------------------
void memory_position_von_index(int index, int &px, int &py) {
  int spalte = index % MEMORY_SPALTEN;
  int zeile = index / MEMORY_SPALTEN;
  px = MEMORY_FELD_X + spalte * (MEMORY_KARTE_BREITE + MEMORY_GAP);
  py = MEMORY_FELD_Y + zeile * (MEMORY_KARTE_HOEHE + MEMORY_GAP);
}

int memory_index_von_touch(int tx, int ty) {
  if (tx < MEMORY_FELD_X || ty < MEMORY_FELD_Y) return -1;
  int spalte = (tx - MEMORY_FELD_X) / (MEMORY_KARTE_BREITE + MEMORY_GAP);
  int zeile = (ty - MEMORY_FELD_Y) / (MEMORY_KARTE_HOEHE + MEMORY_GAP);
  if (spalte < 0 || spalte >= MEMORY_SPALTEN || zeile < 0 || zeile >= MEMORY_ZEILEN) return -1;

  // pruefen, ob der Touch wirklich innerhalb der Kartenflaeche liegt (nicht im Gap)
  int px, py;
  int index = zeile * MEMORY_SPALTEN + spalte;
  memory_position_von_index(index, px, py);
  if (tx > px + MEMORY_KARTE_BREITE || ty > py + MEMORY_KARTE_HOEHE) return -1;

  return index;
}

// ----------------------------------------------------------------------------
//  SPIEL STARTEN / ZURUECKSETZEN
// ----------------------------------------------------------------------------
void memory_spiel_starten() {
  // Werte 1..12 je zweimal in ein Array packen und mischen
  int werte[MEMORY_ANZAHL_KARTEN];
  for (int i = 0; i < MEMORY_ANZAHL_PAARE; i++) {
    werte[i * 2] = i + 1;
    werte[i * 2 + 1] = i + 1;
  }
  for (int i = MEMORY_ANZAHL_KARTEN - 1; i > 0; i--) {
    int j = random(0, i + 1);
    int tausch = werte[i];
    werte[i] = werte[j];
    werte[j] = tausch;
  }

  for (int i = 0; i < MEMORY_ANZAHL_KARTEN; i++) {
    memory_karten[i].wert = werte[i];
    memory_karten[i].aufgedeckt = false;
    memory_karten[i].gefunden = false;
  }

  memory_erste_auswahl = -1;
  memory_zweite_auswahl = -1;
  memory_wartet_auf_verdecken = false;
  memory_punkte_spieler1 = 0;
  memory_punkte_spieler2 = 0;
  memory_aktueller_spieler = 1;
  memory_phase = MEMORY_PHASE_SPIEL;

  tft.fillScreen(FARBE_MEMORY_HINTERGRUND);
  memory_kopfzeile_zeichnen();
  for (int i = 0; i < MEMORY_ANZAHL_KARTEN; i++) memory_karte_zeichnen(i);
}

// ----------------------------------------------------------------------------
//  KOPFZEILE: PUNKTESTAND BEIDER SPIELER, AKTUELLER SPIELER HERVORGEHOBEN
// ----------------------------------------------------------------------------
void memory_kopfzeile_zeichnen() {
  int box_breite = 140, box_hoehe = 20;
  int y = HEADER_HOEHE + 3;

  uint16_t farbe1 = (memory_aktueller_spieler == 1) ? FARBE_MEMORY_SPIELER1 : FARBE_MEMORY_KARTE_AUFGEDECKT;
  uint16_t farbe2 = (memory_aktueller_spieler == 2) ? FARBE_MEMORY_SPIELER2 : FARBE_MEMORY_KARTE_AUFGEDECKT;

  tft.fillRoundRect(5, y, box_breite, box_hoehe, 4, farbe1);
  tft.fillRoundRect(SCREEN_W - 5 - box_breite, y, box_breite, box_hoehe, 4, farbe2);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_MEMORY_TEXT, farbe1);
  char text1[20];
  snprintf(text1, sizeof(text1), "Spieler 1: %d", memory_punkte_spieler1);
  tft.drawString(text1, 5 + box_breite / 2, y + box_hoehe / 2, 1);

  tft.setTextColor(FARBE_MEMORY_TEXT, farbe2);
  char text2[20];
  snprintf(text2, sizeof(text2), "Spieler 2: %d", memory_punkte_spieler2);
  tft.drawString(text2, SCREEN_W - 5 - box_breite / 2, y + box_hoehe / 2, 1);
}

// ----------------------------------------------------------------------------
//  EINE KARTE ZEICHNEN (verdeckt / aufgedeckt / gefunden)
// ----------------------------------------------------------------------------
void memory_karte_zeichnen(int index) {
  int px, py;
  memory_position_von_index(index, px, py);
  MemoryKarte &karte = memory_karten[index];

  uint16_t hintergrund;
  if (karte.gefunden) {
    hintergrund = FARBE_MEMORY_KARTE_GEFUNDEN;
  } else if (karte.aufgedeckt) {
    hintergrund = FARBE_MEMORY_KARTE_AUFGEDECKT;
  } else {
    hintergrund = FARBE_MEMORY_KARTE_VERDECKT;
  }

  tft.fillRoundRect(px, py, MEMORY_KARTE_BREITE, MEMORY_KARTE_HOEHE, 5, hintergrund);
  tft.drawRoundRect(px, py, MEMORY_KARTE_BREITE, MEMORY_KARTE_HOEHE, 5, FARBE_MEMORY_RAHMEN);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_MEMORY_TEXT, hintergrund);

  if (karte.aufgedeckt || karte.gefunden) {
    char text[3];
    snprintf(text, sizeof(text), "%d", karte.wert);
    tft.drawString(text, px + MEMORY_KARTE_BREITE / 2, py + MEMORY_KARTE_HOEHE / 2, 2);
  } else {
    tft.drawString("?", px + MEMORY_KARTE_BREITE / 2, py + MEMORY_KARTE_HOEHE / 2, 2);
  }
}

// ----------------------------------------------------------------------------
//  PRUEFEN, OB ALLE PAARE GEFUNDEN SIND
// ----------------------------------------------------------------------------
bool memory_alle_gefunden() {
  for (int i = 0; i < MEMORY_ANZAHL_KARTEN; i++) {
    if (!memory_karten[i].gefunden) return false;
  }
  return true;
}

// ----------------------------------------------------------------------------
//  ENDANZEIGE MIT PUNKTESTAND UND SIEGER
// ----------------------------------------------------------------------------
void memory_endanzeige_zeichnen() {
  int bx = SCREEN_W / 2 - 100, by = SCREEN_H / 2 - 45, bw = 200, bh = 90;
  tft.fillRoundRect(bx, by, bw, bh, 8, FARBE_MEMORY_KARTE_AUFGEDECKT);
  tft.drawRoundRect(bx, by, bw, bh, 8, FARBE_MEMORY_RAHMEN);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_MEMORY_TEXT, FARBE_MEMORY_KARTE_AUFGEDECKT);

  const char* ergebnis;
  if (memory_punkte_spieler1 > memory_punkte_spieler2) ergebnis = "Spieler 1 gewinnt!";
  else if (memory_punkte_spieler2 > memory_punkte_spieler1) ergebnis = "Spieler 2 gewinnt!";
  else ergebnis = "Unentschieden!";

  tft.drawString(ergebnis, SCREEN_W / 2, by + 22, 2);

  char punkte_text[24];
  snprintf(punkte_text, sizeof(punkte_text), "%d : %d", memory_punkte_spieler1, memory_punkte_spieler2);
  tft.drawString(punkte_text, SCREEN_W / 2, by + 50, 2);

  tft.drawString("Antippen fuer neues Spiel", SCREEN_W / 2, by + 74, 1);
}

// ----------------------------------------------------------------------------
//  TOUCH-BEHANDLUNG
//  In touch_abfragen() aufrufen, wenn spiel_aktiv == SPIEL_MEMORY
// ----------------------------------------------------------------------------
void memory_touch_behandeln() {
  if (memory_phase == MEMORY_PHASE_ENDE) {
    memory_spiel_starten();
    return;
  }

  // waehrend der Anzeigezeit eines Fehlversuchs werden Eingaben ignoriert
  if (memory_wartet_auf_verdecken) return;

  int index = memory_index_von_touch(touch_x, touch_y);
  if (index == -1) return;

  MemoryKarte &karte = memory_karten[index];
  if (karte.gefunden || karte.aufgedeckt) return;   // bereits aufgedeckt/gefunden

  karte.aufgedeckt = true;
  memory_karte_zeichnen(index);

  if (memory_erste_auswahl == -1) {
    memory_erste_auswahl = index;
    return;
  }

  memory_zweite_auswahl = index;

  if (memory_karten[memory_erste_auswahl].wert == memory_karten[memory_zweite_auswahl].wert) {
    // Treffer: beide Karten bleiben aufgedeckt, Punkt, gleicher Spieler weiter
    memory_karten[memory_erste_auswahl].gefunden = true;
    memory_karten[memory_zweite_auswahl].gefunden = true;
    memory_karte_zeichnen(memory_erste_auswahl);
    memory_karte_zeichnen(memory_zweite_auswahl);

    if (memory_aktueller_spieler == 1) memory_punkte_spieler1++;
    else memory_punkte_spieler2++;

    memory_erste_auswahl = -1;
    memory_zweite_auswahl = -1;
    memory_kopfzeile_zeichnen();

    if (memory_alle_gefunden()) {
      memory_phase = MEMORY_PHASE_ENDE;
      memory_endanzeige_zeichnen();
    }
  } else {
    // kein Treffer: kurz warten, dann beide verdecken und Spieler wechseln
    memory_wartet_auf_verdecken = true;
    memory_wartezeit_start = millis();
  }
}

// ----------------------------------------------------------------------------
//  REGELMAESSIG IN loop() AUFRUFEN, WENN spiel_aktiv == SPIEL_MEMORY
//  Verdeckt nach Ablauf der Wartezeit zwei nicht passende Karten wieder
//  und wechselt den Spieler
// ----------------------------------------------------------------------------
void memory_tick_ausfuehren() {
  if (!memory_wartet_auf_verdecken) return;
  if (millis() - memory_wartezeit_start < MEMORY_VERGLEICH_WARTEZEIT_MS) return;

  memory_karten[memory_erste_auswahl].aufgedeckt = false;
  memory_karten[memory_zweite_auswahl].aufgedeckt = false;
  memory_karte_zeichnen(memory_erste_auswahl);
  memory_karte_zeichnen(memory_zweite_auswahl);

  memory_erste_auswahl = -1;
  memory_zweite_auswahl = -1;
  memory_wartet_auf_verdecken = false;

  memory_aktueller_spieler = (memory_aktueller_spieler == 1) ? 2 : 1;
  memory_kopfzeile_zeichnen();
}

// ----------------------------------------------------------------------------
//  MEMORY VERLASSEN (z.B. Home-Button gedrueckt)
// ----------------------------------------------------------------------------
void memory_spiel_verlassen() {
  memory_wartet_auf_verdecken = false;
}