// ============================================================================
//  SPIEL: SUDOKU (spiel_aktiv == SPIEL_SUDOKU)
//  Eigene Datei: esp32-tft-sudoku.ino
//
//  Regeln:
//  - Klassisches 9x9 Sudoku, 3 Schwierigkeitsgrade (Leicht/Mittel/Schwer),
//    die die Anzahl der vorgegebenen Zahlen bestimmen
//  - Vollstaendiges Raetsel wird bei Spielstart direkt auf dem ESP32 per
//    randomisiertem Backtracking erzeugt (keine Eindeutigkeits-Pruefung,
//    fuer ein Gelegenheits-Puzzle ausreichend)
//  - Bedienung: Zelle antippen zum Auswaehlen, dann Zahl im Ziffernblock
//    rechts antippen. "Loeschen" leert die ausgewaehlte Zelle wieder.
//  - Falsch platzierte Zahlen (Duplikat in Zeile/Spalte/3x3-Box) werden
//    rot markiert, vorgegebene Zahlen sind nicht editierbar
//  - Sind alle Zellen korrekt gefuellt, erscheint die Geloest-Anzeige
// ============================================================================

// ----------------------------------------------------------------------------
//  PHASEN
// ----------------------------------------------------------------------------
#define SUDOKU_PHASE_AUSWAHL  0
#define SUDOKU_PHASE_SPIEL    1
#define SUDOKU_PHASE_GELOEST  2

int sudoku_phase = SUDOKU_PHASE_AUSWAHL;

// ----------------------------------------------------------------------------
//  SCHWIERIGKEITSGRADE: ANZAHL DER ENTFERNTEN (leeren) ZELLEN VON 81
// ----------------------------------------------------------------------------
#define SUDOKU_LEICHT_ENTFERNT  30
#define SUDOKU_MITTEL_ENTFERNT  40
#define SUDOKU_SCHWER_ENTFERNT  50

// ----------------------------------------------------------------------------
//  LAYOUT-KONSTANTEN
// ----------------------------------------------------------------------------
#define SUDOKU_ZELLE            22
#define SUDOKU_GITTER_X          6
#define SUDOKU_GITTER_Y          (HEADER_HOEHE + 6)
#define SUDOKU_GITTER_BREITE    (9 * SUDOKU_ZELLE)

#define SUDOKU_PANEL_X           (SUDOKU_GITTER_X + SUDOKU_GITTER_BREITE + 10)
#define SUDOKU_NUMPAD_Y          (SUDOKU_GITTER_Y + 16)
#define SUDOKU_NUMPAD_BTN_W      28
#define SUDOKU_NUMPAD_BTN_H      24
#define SUDOKU_NUMPAD_GAP         3
#define SUDOKU_NUMPAD_HOEHE      (3 * SUDOKU_NUMPAD_BTN_H + 2 * SUDOKU_NUMPAD_GAP)

#define SUDOKU_LOESCHEN_Y        (SUDOKU_NUMPAD_Y + SUDOKU_NUMPAD_HOEHE + 10)
#define SUDOKU_NEU_Y             (SUDOKU_LOESCHEN_Y + 26 + 8)
#define SUDOKU_PANEL_BTN_BREITE  90

// ----------------------------------------------------------------------------
//  FARBEN
// ----------------------------------------------------------------------------
#define FARBE_SUDOKU_HINTERGRUND        tft.color565(15, 25, 45)
#define FARBE_SUDOKU_RAHMEN             tft.color565(80, 80, 90)
#define FARBE_SUDOKU_RAHMEN_DICK        tft.color565(210, 210, 220)
#define FARBE_SUDOKU_ZELLE_NORMAL       tft.color565(25, 35, 55)
#define FARBE_SUDOKU_ZELLE_VORGEGEBEN   tft.color565(35, 45, 70)
#define FARBE_SUDOKU_ZELLE_AUSGEWAEHLT  tft.color565(60, 95, 150)
#define FARBE_SUDOKU_TEXT_VORGEGEBEN    tft.color565(255, 255, 255)
#define FARBE_SUDOKU_TEXT_EIGEN         tft.color565(130, 200, 255)
#define FARBE_SUDOKU_TEXT_FEHLER        tft.color565(255, 90, 90)
#define FARBE_SUDOKU_BUTTON             tft.color565(60, 90, 160)
#define FARBE_SUDOKU_BUTTON_LOESCHEN    tft.color565(160, 50, 50)
#define FARBE_SUDOKU_BUTTON_NEU         tft.color565(200, 170, 20)

// ----------------------------------------------------------------------------
//  SPIELDATEN
// ----------------------------------------------------------------------------
int  sudoku_loesung[9][9];
int  sudoku_raster[9][9];
bool sudoku_vorgegeben[9][9];

int sudoku_ausgewaehlte_zeile = -1;
int sudoku_ausgewaehlte_spalte = -1;

// ----------------------------------------------------------------------------
//  HILFSFUNKTION: TOUCH-PUNKT IN RECHTECK?
// ----------------------------------------------------------------------------
bool sudoku_touch_in_rect(int x, int y, int w, int h) {
  return touch_x >= x && touch_x <= x + w && touch_y >= y && touch_y <= y + h;
}

// ============================================================================
//  SUDOKU-GENERATOR (randomisiertes Backtracking)
// ============================================================================
bool sudoku_gueltig_an_position(int gitter[9][9], int zeile, int spalte, int wert) {
  for (int i = 0; i < 9; i++) {
    if (gitter[zeile][i] == wert) return false;
    if (gitter[i][spalte] == wert) return false;
  }
  int box_zeile = (zeile / 3) * 3;
  int box_spalte = (spalte / 3) * 3;
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) {
      if (gitter[box_zeile + r][box_spalte + c] == wert) return false;
    }
  }
  return true;
}

bool sudoku_fuelle_rekursiv(int gitter[9][9], int position) {
  if (position == 81) return true;
  int zeile = position / 9;
  int spalte = position % 9;

  int zahlen[9] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  for (int i = 8; i > 0; i--) {
    int j = random(0, i + 1);
    int tausch = zahlen[i];
    zahlen[i] = zahlen[j];
    zahlen[j] = tausch;
  }

  for (int i = 0; i < 9; i++) {
    int wert = zahlen[i];
    if (sudoku_gueltig_an_position(gitter, zeile, spalte, wert)) {
      gitter[zeile][spalte] = wert;
      if (sudoku_fuelle_rekursiv(gitter, position + 1)) return true;
      gitter[zeile][spalte] = 0;
    }
  }
  return false;
}

// ----------------------------------------------------------------------------
//  NEUES RAETSEL ERZEUGEN: vollstaendige Loesung erzeugen, dann N Zellen
//  wieder leeren, um das spielbare Raster zu erhalten
// ----------------------------------------------------------------------------
void sudoku_neues_spiel_generieren(int anzahl_entfernt) {
  // kurzer Hinweis, da die Erzeugung einen Moment dauern kann
  tft.fillScreen(FARBE_SUDOKU_HINTERGRUND);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_SUDOKU_TEXT_VORGEGEBEN, FARBE_SUDOKU_HINTERGRUND);
  tft.drawString("Erstelle Sudoku...", SCREEN_W / 2, SCREEN_H / 2, 2);

  for (int z = 0; z < 9; z++) {
    for (int s = 0; s < 9; s++) {
      sudoku_loesung[z][s] = 0;
    }
  }
  sudoku_fuelle_rekursiv(sudoku_loesung, 0);

  for (int z = 0; z < 9; z++) {
    for (int s = 0; s < 9; s++) {
      sudoku_raster[z][s] = sudoku_loesung[z][s];
      sudoku_vorgegeben[z][s] = true;
    }
  }

  int entfernt = 0;
  while (entfernt < anzahl_entfernt) {
    int z = random(0, 9);
    int s = random(0, 9);
    if (sudoku_raster[z][s] != 0) {
      sudoku_raster[z][s] = 0;
      sudoku_vorgegeben[z][s] = false;
      entfernt++;
    }
  }

  sudoku_ausgewaehlte_zeile = -1;
  sudoku_ausgewaehlte_spalte = -1;
  sudoku_phase = SUDOKU_PHASE_SPIEL;
  sudoku_spiel_zeichnen();
}

// ============================================================================
//  SPIEL STARTEN (Aufruf von der Spieleseite aus)
// ============================================================================
void sudoku_spiel_starten() {
  sudoku_phase = SUDOKU_PHASE_AUSWAHL;
  sudoku_auswahl_zeichnen();
}

// ============================================================================
//  PHASE 1: SCHWIERIGKEIT AUSWAEHLEN
// ============================================================================
void sudoku_auswahl_zeichnen() {
  tft.fillScreen(FARBE_SUDOKU_HINTERGRUND);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_SUDOKU_TEXT_VORGEGEBEN, FARBE_SUDOKU_HINTERGRUND);
  tft.drawString("Sudoku - Schwierigkeit", SCREEN_W / 2, HEADER_HOEHE + 24, 2);

  int btn_breite = 160, btn_hoehe = 40, gap = 14;
  int btn_x = (SCREEN_W - btn_breite) / 2;
  int start_y = HEADER_HOEHE + 60;

  const char* namen[3] = { "Leicht", "Mittel", "Schwer" };
  for (int i = 0; i < 3; i++) {
    int by = start_y + i * (btn_hoehe + gap);
    tft.fillRoundRect(btn_x, by, btn_breite, btn_hoehe, 8, FARBE_SUDOKU_BUTTON);
    tft.drawRoundRect(btn_x, by, btn_breite, btn_hoehe, 8, FARBE_SUDOKU_RAHMEN_DICK);
    tft.drawString(namen[i], SCREEN_W / 2, by + btn_hoehe / 2, 2);
  }
}

void sudoku_auswahl_touch_behandeln() {
  int btn_breite = 160, btn_hoehe = 40, gap = 14;
  int btn_x = (SCREEN_W - btn_breite) / 2;
  int start_y = HEADER_HOEHE + 60;

  for (int i = 0; i < 3; i++) {
    int by = start_y + i * (btn_hoehe + gap);
    if (sudoku_touch_in_rect(btn_x, by, btn_breite, btn_hoehe)) {
      int entfernt = SUDOKU_LEICHT_ENTFERNT;
      if (i == 1) entfernt = SUDOKU_MITTEL_ENTFERNT;
      if (i == 2) entfernt = SUDOKU_SCHWER_ENTFERNT;
      sudoku_neues_spiel_generieren(entfernt);
      return;
    }
  }
}

// ============================================================================
//  PHASE 2: SPIEL (GITTER + ZIFFERNBLOCK)
// ============================================================================
bool sudoku_zelle_hat_konflikt(int zeile, int spalte) {
  int wert = sudoku_raster[zeile][spalte];
  if (wert == 0) return false;

  for (int i = 0; i < 9; i++) {
    if (i != spalte && sudoku_raster[zeile][i] == wert) return true;
    if (i != zeile && sudoku_raster[i][spalte] == wert) return true;
  }
  int box_zeile = (zeile / 3) * 3;
  int box_spalte = (spalte / 3) * 3;
  for (int r = 0; r < 3; r++) {
    for (int c = 0; c < 3; c++) {
      int rz = box_zeile + r, rs = box_spalte + c;
      if ((rz != zeile || rs != spalte) && sudoku_raster[rz][rs] == wert) return true;
    }
  }
  return false;
}

void sudoku_zelle_zeichnen(int zeile, int spalte) {
  int px = SUDOKU_GITTER_X + spalte * SUDOKU_ZELLE;
  int py = SUDOKU_GITTER_Y + zeile * SUDOKU_ZELLE;

  bool ausgewaehlt = (zeile == sudoku_ausgewaehlte_zeile && spalte == sudoku_ausgewaehlte_spalte);
  uint16_t hintergrund;
  if (ausgewaehlt) {
    hintergrund = FARBE_SUDOKU_ZELLE_AUSGEWAEHLT;
  } else if (sudoku_vorgegeben[zeile][spalte]) {
    hintergrund = FARBE_SUDOKU_ZELLE_VORGEGEBEN;
  } else {
    hintergrund = FARBE_SUDOKU_ZELLE_NORMAL;
  }

  tft.fillRect(px, py, SUDOKU_ZELLE - 1, SUDOKU_ZELLE - 1, hintergrund);

  int wert = sudoku_raster[zeile][spalte];
  if (wert != 0) {
    uint16_t textfarbe;
    if (sudoku_vorgegeben[zeile][spalte]) {
      textfarbe = FARBE_SUDOKU_TEXT_VORGEGEBEN;
    } else if (sudoku_zelle_hat_konflikt(zeile, spalte)) {
      textfarbe = FARBE_SUDOKU_TEXT_FEHLER;
    } else {
      textfarbe = FARBE_SUDOKU_TEXT_EIGEN;
    }
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(textfarbe, hintergrund);
    char text[2];
    text[0] = '0' + wert;
    text[1] = '\0';
    tft.drawString(text, px + SUDOKU_ZELLE / 2, py + SUDOKU_ZELLE / 2, 2);
  }

  // duenne Rahmen fuer jede Zelle
  tft.drawRect(px, py, SUDOKU_ZELLE, SUDOKU_ZELLE, FARBE_SUDOKU_RAHMEN);
}

void sudoku_gitter_zeichnen() {
  for (int z = 0; z < 9; z++) {
    for (int s = 0; s < 9; s++) {
      sudoku_zelle_zeichnen(z, s);
    }
  }
  // dicke Linien fuer die 3x3-Boxen
  for (int i = 0; i <= 9; i += 3) {
    int x = SUDOKU_GITTER_X + i * SUDOKU_ZELLE;
    tft.drawFastVLine(x, SUDOKU_GITTER_Y, SUDOKU_GITTER_BREITE, FARBE_SUDOKU_RAHMEN_DICK);
    tft.drawFastVLine(x + 1, SUDOKU_GITTER_Y, SUDOKU_GITTER_BREITE, FARBE_SUDOKU_RAHMEN_DICK);
    int y = SUDOKU_GITTER_Y + i * SUDOKU_ZELLE;
    tft.drawFastHLine(SUDOKU_GITTER_X, y, SUDOKU_GITTER_BREITE, FARBE_SUDOKU_RAHMEN_DICK);
    tft.drawFastHLine(SUDOKU_GITTER_X, y + 1, SUDOKU_GITTER_BREITE, FARBE_SUDOKU_RAHMEN_DICK);
  }
}

void sudoku_numpad_zeichnen() {
  tft.setTextDatum(MC_DATUM);

  for (int n = 1; n <= 9; n++) {
    int reihe = (n - 1) / 3;
    int spalte = (n - 1) % 3;
    int bx = SUDOKU_PANEL_X + spalte * (SUDOKU_NUMPAD_BTN_W + SUDOKU_NUMPAD_GAP);
    int by = SUDOKU_NUMPAD_Y + reihe * (SUDOKU_NUMPAD_BTN_H + SUDOKU_NUMPAD_GAP);

    tft.fillRoundRect(bx, by, SUDOKU_NUMPAD_BTN_W, SUDOKU_NUMPAD_BTN_H, 4, FARBE_SUDOKU_BUTTON);
    tft.setTextColor(FARBE_SUDOKU_TEXT_VORGEGEBEN, FARBE_SUDOKU_BUTTON);
    char text[2];
    text[0] = '0' + n;
    text[1] = '\0';
    tft.drawString(text, bx + SUDOKU_NUMPAD_BTN_W / 2, by + SUDOKU_NUMPAD_BTN_H / 2, 2);
  }

  tft.fillRoundRect(SUDOKU_PANEL_X, SUDOKU_LOESCHEN_Y, SUDOKU_PANEL_BTN_BREITE, 26, 5, FARBE_SUDOKU_BUTTON_LOESCHEN);
  tft.setTextColor(FARBE_SUDOKU_TEXT_VORGEGEBEN, FARBE_SUDOKU_BUTTON_LOESCHEN);
  tft.drawString("Loeschen", SUDOKU_PANEL_X + SUDOKU_PANEL_BTN_BREITE / 2, SUDOKU_LOESCHEN_Y + 13, 1);

  tft.fillRoundRect(SUDOKU_PANEL_X, SUDOKU_NEU_Y, SUDOKU_PANEL_BTN_BREITE, 26, 5, FARBE_SUDOKU_BUTTON_NEU);
  tft.setTextColor(tft.color565(30, 30, 30), FARBE_SUDOKU_BUTTON_NEU);
  tft.drawString("Neues Spiel", SUDOKU_PANEL_X + SUDOKU_PANEL_BTN_BREITE / 2, SUDOKU_NEU_Y + 13, 1);
}

void sudoku_spiel_zeichnen() {
  tft.fillScreen(FARBE_SUDOKU_HINTERGRUND);
  sudoku_gitter_zeichnen();
  sudoku_numpad_zeichnen();
}

// ----------------------------------------------------------------------------
//  PRUEFEN OB DAS RAETSEL VOLLSTAENDIG UND FEHLERFREI GELOEST IST
// ----------------------------------------------------------------------------
bool sudoku_ist_geloest() {
  for (int z = 0; z < 9; z++) {
    for (int s = 0; s < 9; s++) {
      if (sudoku_raster[z][s] == 0) return false;
      if (sudoku_zelle_hat_konflikt(z, s)) return false;
    }
  }
  return true;
}

// ----------------------------------------------------------------------------
//  WERT IN DER AUSGEWAEHLTEN ZELLE SETZEN (0 = loeschen)
// ----------------------------------------------------------------------------
void sudoku_wert_setzen(int wert) {
  if (sudoku_ausgewaehlte_zeile == -1) return;
  if (sudoku_vorgegeben[sudoku_ausgewaehlte_zeile][sudoku_ausgewaehlte_spalte]) return;

  sudoku_raster[sudoku_ausgewaehlte_zeile][sudoku_ausgewaehlte_spalte] = wert;
  sudoku_zelle_zeichnen(sudoku_ausgewaehlte_zeile, sudoku_ausgewaehlte_spalte);

  if (sudoku_ist_geloest()) {
    sudoku_phase = SUDOKU_PHASE_GELOEST;
    sudoku_geloest_anzeigen();
  }
}

// ============================================================================
//  PHASE 3: GELOEST-ANZEIGE
// ============================================================================
void sudoku_geloest_anzeigen() {
  int bx = SCREEN_W / 2 - 90, by = SCREEN_H / 2 - 40, bw = 180, bh = 80;
  tft.fillRoundRect(bx, by, bw, bh, 8, FARBE_SUDOKU_BUTTON_NEU);
  tft.drawRoundRect(bx, by, bw, bh, 8, FARBE_SUDOKU_RAHMEN_DICK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(tft.color565(30, 30, 30), FARBE_SUDOKU_BUTTON_NEU);
  tft.drawString("Geloest!", SCREEN_W / 2, by + 28, 2);
  tft.drawString("Antippen fuer neues Spiel", SCREEN_W / 2, by + 54, 1);
}

// ============================================================================
//  TOUCH-BEHANDLUNG SPIELPHASE (Gitter + Ziffernblock)
// ============================================================================
void sudoku_spiel_touch_behandeln() {
  // Gitter antippen
  if (touch_x >= SUDOKU_GITTER_X && touch_x < SUDOKU_GITTER_X + SUDOKU_GITTER_BREITE &&
      touch_y >= SUDOKU_GITTER_Y && touch_y < SUDOKU_GITTER_Y + SUDOKU_GITTER_BREITE) {
    int spalte = (touch_x - SUDOKU_GITTER_X) / SUDOKU_ZELLE;
    int zeile = (touch_y - SUDOKU_GITTER_Y) / SUDOKU_ZELLE;

    if (sudoku_vorgegeben[zeile][spalte]) return;  // vorgegebene Zellen nicht editierbar

    int alte_zeile = sudoku_ausgewaehlte_zeile;
    int alte_spalte = sudoku_ausgewaehlte_spalte;
    sudoku_ausgewaehlte_zeile = zeile;
    sudoku_ausgewaehlte_spalte = spalte;

    if (alte_zeile != -1) sudoku_zelle_zeichnen(alte_zeile, alte_spalte);
    sudoku_zelle_zeichnen(zeile, spalte);
    return;
  }

  // Zahlen 1-9
  for (int n = 1; n <= 9; n++) {
    int reihe = (n - 1) / 3;
    int spalte = (n - 1) % 3;
    int bx = SUDOKU_PANEL_X + spalte * (SUDOKU_NUMPAD_BTN_W + SUDOKU_NUMPAD_GAP);
    int by = SUDOKU_NUMPAD_Y + reihe * (SUDOKU_NUMPAD_BTN_H + SUDOKU_NUMPAD_GAP);
    if (sudoku_touch_in_rect(bx, by, SUDOKU_NUMPAD_BTN_W, SUDOKU_NUMPAD_BTN_H)) {
      sudoku_wert_setzen(n);
      return;
    }
  }

  // Loeschen
  if (sudoku_touch_in_rect(SUDOKU_PANEL_X, SUDOKU_LOESCHEN_Y, SUDOKU_PANEL_BTN_BREITE, 26)) {
    sudoku_wert_setzen(0);
    return;
  }

  // Neues Spiel -> zurueck zur Schwierigkeitsauswahl
  if (sudoku_touch_in_rect(SUDOKU_PANEL_X, SUDOKU_NEU_Y, SUDOKU_PANEL_BTN_BREITE, 26)) {
    sudoku_phase = SUDOKU_PHASE_AUSWAHL;
    sudoku_auswahl_zeichnen();
    return;
  }
}

// ============================================================================
//  ZENTRALE TOUCH-WEITERLEITUNG (aus touch_abfragen() aufrufen, wenn
//  spiel_aktiv == SPIEL_SUDOKU)
// ============================================================================
void sudoku_touch_behandeln() {
  if (sudoku_phase == SUDOKU_PHASE_AUSWAHL) {
    sudoku_auswahl_touch_behandeln();
  } else if (sudoku_phase == SUDOKU_PHASE_SPIEL) {
    sudoku_spiel_touch_behandeln();
  } else if (sudoku_phase == SUDOKU_PHASE_GELOEST) {
    // nach Loesung: irgendwo antippen = zurueck zur Schwierigkeitsauswahl
    sudoku_phase = SUDOKU_PHASE_AUSWAHL;
    sudoku_auswahl_zeichnen();
  }
}

// ============================================================================
//  SUDOKU VERLASSEN (z.B. Home-Button gedrueckt)
// ============================================================================
void sudoku_spiel_verlassen() {
  // kein laufender Timer, Spielstand bleibt einfach im Speicher erhalten
}