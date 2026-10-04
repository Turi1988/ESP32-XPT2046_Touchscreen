// ============================================================================
//  SPIEL: VIER GEWINNT (spiel_aktiv == SPIEL_VIER_GEWINNT)
//  Eigene Datei: esp32-tft-viergewinnt.ino
//
//  Regeln (2 Spieler abwechselnd am selben Geraet):
//  - Klassisches Vier-Gewinnt auf einem 7x6-Feld
//  - Spalte antippen wirft einen Stein hinein, er faellt bis zur untersten
//    freien Reihe dieser Spalte
//  - Wer zuerst 4 eigene Steine in einer Reihe hat (waagerecht, senkrecht
//    oder diagonal), gewinnt - die vier Gewinnsteine werden hervorgehoben
//  - Ist das Feld voll ohne Gewinner, endet die Runde unentschieden
//  - Der Siegzaehler oben bleibt ueber mehrere Runden hinweg erhalten
//    (nur im Arbeitsspeicher, geht beim Neustart des ESP32 verloren)
// ============================================================================

#define VIER_SPALTEN   7
#define VIER_ZEILEN    6
#define VIER_ZELLE     30

#define VIER_FELD_X    ((SCREEN_W - VIER_SPALTEN * VIER_ZELLE) / 2)
#define VIER_FELD_Y    (HEADER_HOEHE + 26)

#define VIER_PHASE_SPIEL  0
#define VIER_PHASE_ENDE   1

int vier_phase = VIER_PHASE_SPIEL;

#define FARBE_VIER_HINTERGRUND   tft.color565(15, 25, 45)
#define FARBE_VIER_BRETT         tft.color565(30, 90, 160)
#define FARBE_VIER_LEER          tft.color565(15, 25, 45)
#define FARBE_VIER_SPIELER1      tft.color565(220, 60, 60)
#define FARBE_VIER_SPIELER2      tft.color565(230, 200, 40)
#define FARBE_VIER_GEWINNRAHMEN  tft.color565(255, 255, 255)
#define FARBE_VIER_TEXT          tft.color565(255, 255, 255)

int vier_board[VIER_SPALTEN][VIER_ZEILEN];   // 0=leer, 1=Spieler1, 2=Spieler2
int vier_aktueller_spieler = 1;

int vier_siege_spieler1 = 0;
int vier_siege_spieler2 = 0;

// die 4 Gewinnfelder, falls ein Sieg gefunden wurde (fuer Hervorhebung)
int vier_gewinn_spalte[4];
int vier_gewinn_zeile[4];
bool vier_hat_gewinner = false;

// ----------------------------------------------------------------------------
//  SPIEL STARTEN / NEUE RUNDE
// ----------------------------------------------------------------------------
void vier_spiel_starten() {
  for (int s = 0; s < VIER_SPALTEN; s++) {
    for (int z = 0; z < VIER_ZEILEN; z++) {
      vier_board[s][z] = 0;
    }
  }
  vier_aktueller_spieler = 1;
  vier_phase = VIER_PHASE_SPIEL;
  vier_hat_gewinner = false;

  tft.fillScreen(FARBE_VIER_HINTERGRUND);
  vier_kopfzeile_zeichnen();
  vier_brett_zeichnen();
}

// ----------------------------------------------------------------------------
//  KOPFZEILE: WER IST DRAN + SIEGZAEHLER
// ----------------------------------------------------------------------------
void vier_kopfzeile_zeichnen() {
  tft.fillRect(0, HEADER_HOEHE, SCREEN_W, VIER_FELD_Y - HEADER_HOEHE, FARBE_VIER_HINTERGRUND);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(FARBE_VIER_TEXT, FARBE_VIER_HINTERGRUND);
  char siege_text[24];
  snprintf(siege_text, sizeof(siege_text), "Siege: %d : %d", vier_siege_spieler1, vier_siege_spieler2);
  tft.drawString(siege_text, 8, HEADER_HOEHE + 6, 1);

  tft.setTextDatum(TR_DATUM);
  uint16_t spielerfarbe = (vier_aktueller_spieler == 1) ? FARBE_VIER_SPIELER1 : FARBE_VIER_SPIELER2;
  tft.setTextColor(spielerfarbe, FARBE_VIER_HINTERGRUND);
  char dran_text[20];
  snprintf(dran_text, sizeof(dran_text), "Spieler %d ist dran", vier_aktueller_spieler);
  tft.drawString(dran_text, SCREEN_W - 8, HEADER_HOEHE + 6, 1);
}

// ----------------------------------------------------------------------------
//  SPIELBRETT ZEICHNEN (Hintergrundplatte + alle Loecher)
// ----------------------------------------------------------------------------
void vier_brett_zeichnen() {
  tft.fillRoundRect(VIER_FELD_X - 4, VIER_FELD_Y - 4,
                     VIER_SPALTEN * VIER_ZELLE + 8, VIER_ZEILEN * VIER_ZELLE + 8,
                     6, FARBE_VIER_BRETT);
  for (int s = 0; s < VIER_SPALTEN; s++) {
    for (int z = 0; z < VIER_ZEILEN; z++) {
      vier_feld_zeichnen(s, z);
    }
  }
}

// ----------------------------------------------------------------------------
//  EIN EINZELNES LOCH ZEICHNEN (leer, Spieler 1, Spieler 2, ggf. hervorgehoben)
// ----------------------------------------------------------------------------
bool vier_ist_gewinnfeld(int spalte, int zeile) {
  if (!vier_hat_gewinner) return false;
  for (int i = 0; i < 4; i++) {
    if (vier_gewinn_spalte[i] == spalte && vier_gewinn_zeile[i] == zeile) return true;
  }
  return false;
}

void vier_feld_zeichnen(int spalte, int zeile) {
  int mx = VIER_FELD_X + spalte * VIER_ZELLE + VIER_ZELLE / 2;
  int my = VIER_FELD_Y + zeile * VIER_ZELLE + VIER_ZELLE / 2;
  int radius = VIER_ZELLE / 2 - 3;

  uint16_t farbe;
  int wert = vier_board[spalte][zeile];
  if (wert == 1) farbe = FARBE_VIER_SPIELER1;
  else if (wert == 2) farbe = FARBE_VIER_SPIELER2;
  else farbe = FARBE_VIER_LEER;

  tft.fillCircle(mx, my, radius, farbe);

  if (vier_ist_gewinnfeld(spalte, zeile)) {
    tft.drawCircle(mx, my, radius, FARBE_VIER_GEWINNRAHMEN);
    tft.drawCircle(mx, my, radius - 1, FARBE_VIER_GEWINNRAHMEN);
  }
}

// ----------------------------------------------------------------------------
//  NAECHSTE FREIE ZEILE IN EINER SPALTE FINDEN (von unten nach oben)
//  Rueckgabe -1, falls die Spalte voll ist
// ----------------------------------------------------------------------------
int vier_naechste_freie_zeile(int spalte) {
  for (int z = VIER_ZEILEN - 1; z >= 0; z--) {
    if (vier_board[spalte][z] == 0) return z;
  }
  return -1;
}

// ----------------------------------------------------------------------------
//  GEWINNPRUEFUNG AB EINEM GESETZTEN STEIN IN ALLE 4 LINIENRICHTUNGEN
// ----------------------------------------------------------------------------
bool vier_pruefe_gewinn(int start_spalte, int start_zeile, int spieler) {
  int richtungen[4][2] = { {1, 0}, {0, 1}, {1, 1}, {1, -1} };

  for (int r = 0; r < 4; r++) {
    int dx = richtungen[r][0];
    int dy = richtungen[r][1];

    int treffer_spalte[7];
    int treffer_zeile[7];
    int anzahl = 0;

    int s = start_spalte, z = start_zeile;
    while (s - dx >= 0 && s - dx < VIER_SPALTEN && z - dy >= 0 && z - dy < VIER_ZEILEN &&
           vier_board[s - dx][z - dy] == spieler) {
      s -= dx; z -= dy;
    }

    while (s >= 0 && s < VIER_SPALTEN && z >= 0 && z < VIER_ZEILEN && vier_board[s][z] == spieler) {
      if (anzahl < 7) { treffer_spalte[anzahl] = s; treffer_zeile[anzahl] = z; anzahl++; }
      s += dx; z += dy;
    }

    if (anzahl >= 4) {
      for (int i = 0; i < 4; i++) {
        vier_gewinn_spalte[i] = treffer_spalte[i];
        vier_gewinn_zeile[i] = treffer_zeile[i];
      }
      return true;
    }
  }
  return false;
}

bool vier_ist_voll() {
  for (int s = 0; s < VIER_SPALTEN; s++) {
    if (vier_board[s][0] == 0) return false;
  }
  return true;
}

// ----------------------------------------------------------------------------
//  ENDANZEIGE (SIEG ODER UNENTSCHIEDEN)
// ----------------------------------------------------------------------------
void vier_endanzeige_zeichnen(const char* nachricht) {
  int bx = SCREEN_W / 2 - 90, by = VIER_FELD_Y + (VIER_ZEILEN * VIER_ZELLE) / 2 - 30;
  int bw = 180, bh = 60;
  tft.fillRoundRect(bx, by, bw, bh, 8, FARBE_VIER_BRETT);
  tft.drawRoundRect(bx, by, bw, bh, 8, FARBE_VIER_GEWINNRAHMEN);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_VIER_TEXT, FARBE_VIER_BRETT);
  tft.drawString(nachricht, SCREEN_W / 2, by + 22, 2);
  tft.drawString("Antippen fuer neue Runde", SCREEN_W / 2, by + 44, 1);
}

// ----------------------------------------------------------------------------
//  TOUCH-BEHANDLUNG
//  In touch_abfragen() aufrufen, wenn spiel_aktiv == SPIEL_VIER_GEWINNT
// ----------------------------------------------------------------------------
void vier_touch_behandeln() {
  if (vier_phase == VIER_PHASE_ENDE) {
    vier_spiel_starten();
    return;
  }

  if (touch_x < VIER_FELD_X || touch_x >= VIER_FELD_X + VIER_SPALTEN * VIER_ZELLE) return;
  if (touch_y < VIER_FELD_Y || touch_y >= VIER_FELD_Y + VIER_ZEILEN * VIER_ZELLE) return;

  int spalte = (touch_x - VIER_FELD_X) / VIER_ZELLE;
  int zeile = vier_naechste_freie_zeile(spalte);
  if (zeile == -1) return;   // Spalte voll

  vier_board[spalte][zeile] = vier_aktueller_spieler;
  vier_feld_zeichnen(spalte, zeile);

  if (vier_pruefe_gewinn(spalte, zeile, vier_aktueller_spieler)) {
    vier_hat_gewinner = true;
    for (int i = 0; i < 4; i++) vier_feld_zeichnen(vier_gewinn_spalte[i], vier_gewinn_zeile[i]);

    if (vier_aktueller_spieler == 1) vier_siege_spieler1++;
    else vier_siege_spieler2++;

    vier_phase = VIER_PHASE_ENDE;
    vier_kopfzeile_zeichnen();
    char nachricht[20];
    snprintf(nachricht, sizeof(nachricht), "Spieler %d gewinnt!", vier_aktueller_spieler);
    vier_endanzeige_zeichnen(nachricht);
    return;
  }

  if (vier_ist_voll()) {
    vier_phase = VIER_PHASE_ENDE;
    vier_endanzeige_zeichnen("Unentschieden!");
    return;
  }

  vier_aktueller_spieler = (vier_aktueller_spieler == 1) ? 2 : 1;
  vier_kopfzeile_zeichnen();
}

// ----------------------------------------------------------------------------
//  VIER GEWINNT VERLASSEN (z.B. Home-Button gedrueckt)
// ----------------------------------------------------------------------------
void vier_spiel_verlassen() {
  // kein laufender Timer, Spielstand bleibt einfach im Speicher erhalten
}