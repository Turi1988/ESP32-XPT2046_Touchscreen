// ============================================================================
//  SPIEL: TETRIS (spiel_aktiv == SPIEL_TETRIS)
//  Eigene Datei: esp32-tft-tetris.ino
//
//  Klassisches Tetris mit:
//  - 10x20 Spielfeld
//  - 7 verschiedene Tetrominoes (I, O, T, S, Z, J, L)
//  - Highscore-Speicherung auf SD-Karte
//  - Echtzeit-Spielablauf mit Gravity
//  - Reihen-Erkennung und Löschen
// ============================================================================

// ============================================================================
//  KONSTANTEN UND DEFINITIONEN
// ============================================================================

#define TETRIS_BREITE        10
#define TETRIS_HOEHE         20
#define TETRIS_ZELLENGR      16

#define TETRIS_FELD_X        ((SCREEN_W - (TETRIS_BREITE * TETRIS_ZELLENGR)) / 2)
#define TETRIS_FELD_Y        (HEADER_HOEHE + 26)

#define TETRIS_PREVIEW_X     (TETRIS_FELD_X + TETRIS_BREITE * TETRIS_ZELLENGR + 12)
#define TETRIS_PREVIEW_Y     (HEADER_HOEHE + 30)

#define TETRIS_PHASE_SPIEL   0
#define TETRIS_PHASE_PAUSE   1
#define TETRIS_PHASE_ENDE    2

#define TETRIS_GRAVITY_START 800   // ms, je Stufe -20ms schneller
#define TETRIS_GRAVITY_MIN   80

static const char* TETRIS_HS_DATEI = "/tetris_hs.txt";
#define TETRIS_MAX_SCORES    10

// ============================================================================
//  FARBEN
// ============================================================================

#define FARBE_TETRIS_HINTERGRUND   tft.color565(10, 15, 25)
#define FARBE_TETRIS_BRETT         tft.color565(20, 25, 40)
#define FARBE_TETRIS_GRID          tft.color565(30, 35, 50)
#define FARBE_TETRIS_I             tft.color565(0, 240, 240)      // Cyan
#define FARBE_TETRIS_O             tft.color565(240, 240, 0)      // Gelb
#define FARBE_TETRIS_T             tft.color565(160, 0, 240)      // Magenta
#define FARBE_TETRIS_S             tft.color565(0, 240, 0)        // Gruen
#define FARBE_TETRIS_Z             tft.color565(240, 0, 0)        // Rot
#define FARBE_TETRIS_J             tft.color565(0, 0, 240)        // Blau
#define FARBE_TETRIS_L             tft.color565(240, 160, 0)      // Orange
#define FARBE_TETRIS_TEXT          tft.color565(200, 200, 200)
#define FARBE_TETRIS_TEXT_AKTIV    tft.color565(255, 255, 255)

// ============================================================================
//  TETROMINOE-STRUKTUREN
// ============================================================================

#define TETRIS_ARTEN 7

struct TetrominoeForm {
  int blöcke[4][2];   // 4 Blöcke, je x,y relativ zum Pivot
  uint16_t farbe;
};

const TetrominoeForm tetris_formen[TETRIS_ARTEN] = {
  // I
  { { {0,0}, {1,0}, {2,0}, {3,0} }, FARBE_TETRIS_I },
  // O
  { { {0,0}, {1,0}, {0,1}, {1,1} }, FARBE_TETRIS_O },
  // T
  { { {0,1}, {1,0}, {1,1}, {2,1} }, FARBE_TETRIS_T },
  // S
  { { {1,0}, {2,0}, {0,1}, {1,1} }, FARBE_TETRIS_S },
  // Z
  { { {0,0}, {1,0}, {1,1}, {2,1} }, FARBE_TETRIS_Z },
  // J
  { { {0,0}, {0,1}, {1,1}, {2,1} }, FARBE_TETRIS_J },
  // L
  { { {2,0}, {0,1}, {1,1}, {2,1} }, FARBE_TETRIS_L }
};

// ============================================================================
//  GAME-STATE
// ============================================================================

int tetris_spielfeld[TETRIS_BREITE][TETRIS_HOEHE];  // 0=leer, 1-7=Tetrominoe
int tetris_farben[TETRIS_BREITE][TETRIS_HOEHE];     // Farben der Zellen

struct TetrisTetrominoe {
  int art;          // 0-6
  int x, y;         // Position des Pivot
  int rotation;     // 0-3 (für komplexere Rotationen)
};

TetrisTetrominoe tetris_aktuell;
TetrisTetrominoe tetris_vorschau;

int tetris_score = 0;
int tetris_level = 1;
int tetris_reihen = 0;
int tetris_phase = TETRIS_PHASE_SPIEL;

unsigned long tetris_letzte_gravity = 0;
unsigned long tetris_gravity_ms = TETRIS_GRAVITY_START;

struct TetrisHighscore {
  int punkte;
  char name[16];
  char datum[20];
};

TetrisHighscore tetris_highscores[TETRIS_MAX_SCORES];
int tetris_hs_anzahl = 0;

// ============================================================================
//  HIGHSCORE-FUNKTIONEN
// ============================================================================

void tetris_hs_laden() {
  tetris_hs_anzahl = 0;
  if (!sd_stand.mount_ok) return;

  sd_bus_sichern();
  File datei = SD.open(TETRIS_HS_DATEI, FILE_READ);
  if (!datei) {
    sd_bus_freigeben();
    return;
  }

  while (datei.available() && tetris_hs_anzahl < TETRIS_MAX_SCORES) {
    String zeile = datei.readStringUntil('\n');
    zeile.trim();
    if (zeile.length() == 0) continue;

    int komma1 = zeile.indexOf(',');
    int komma2 = zeile.indexOf(',', komma1 + 1);

    if (komma1 > 0 && komma2 > komma1) {
      String punkte_str = zeile.substring(0, komma1);
      String name_str = zeile.substring(komma1 + 1, komma2);
      String datum_str = zeile.substring(komma2 + 1);

      TetrisHighscore &hs = tetris_highscores[tetris_hs_anzahl];
      hs.punkte = punkte_str.toInt();
      strncpy(hs.name, name_str.c_str(), sizeof(hs.name) - 1);
      hs.name[sizeof(hs.name) - 1] = '\0';
      strncpy(hs.datum, datum_str.c_str(), sizeof(hs.datum) - 1);
      hs.datum[sizeof(hs.datum) - 1] = '\0';
      tetris_hs_anzahl++;
    }
  }
  datei.close();
  sd_bus_freigeben();
}

void tetris_hs_speichern() {
  if (!sd_stand.mount_ok) return;

  sd_bus_sichern();
  File datei = SD.open(TETRIS_HS_DATEI, FILE_WRITE);
  if (!datei) {
    sd_bus_freigeben();
    return;
  }

  for (int i = 0; i < tetris_hs_anzahl; i++) {
    datei.print(tetris_highscores[i].punkte);
    datei.print(",");
    datei.print(tetris_highscores[i].name);
    datei.print(",");
    datei.println(tetris_highscores[i].datum);
  }

  datei.close();
  sd_bus_freigeben();
}

int tetris_hs_platz_finden(int punkte) {
  for (int i = 0; i < tetris_hs_anzahl; i++) {
    if (punkte > tetris_highscores[i].punkte) return i;
  }
  return (tetris_hs_anzahl < TETRIS_MAX_SCORES) ? tetris_hs_anzahl : -1;
}

void tetris_hs_neuer_eintrag(int punkte) {
  int platz = tetris_hs_platz_finden(punkte);
  if (platz == -1) return;

  if (tetris_hs_anzahl < TETRIS_MAX_SCORES) {
    tetris_hs_anzahl++;
  }

  for (int i = tetris_hs_anzahl - 1; i > platz; i--) {
    tetris_highscores[i] = tetris_highscores[i - 1];
  }

  tetris_highscores[platz].punkte = punkte;
  strncpy(tetris_highscores[platz].name, "Player", sizeof(tetris_highscores[platz].name) - 1);

  time_t jetzt = time(nullptr);
  struct tm *lt = localtime(&jetzt);
  strftime(tetris_highscores[platz].datum, sizeof(tetris_highscores[platz].datum),
           "%d.%m.%Y %H:%M", lt);

  tetris_hs_speichern();
}

// ============================================================================
//  SPIELFELD-OPERATIONEN
// ============================================================================

void tetris_spielfeld_clearen() {
  for (int x = 0; x < TETRIS_BREITE; x++) {
    for (int y = 0; y < TETRIS_HOEHE; y++) {
      tetris_spielfeld[x][y] = 0;
      tetris_farben[x][y] = 0;
    }
  }
}

bool tetris_kollision_pruefe(const TetrisTetrominoe &t, int offset_x = 0, int offset_y = 0) {
  const TetrominoeForm &form = tetris_formen[t.art];
  for (int i = 0; i < 4; i++) {
    int bx = t.x + form.blöcke[i][0] + offset_x;
    int by = t.y + form.blöcke[i][1] + offset_y;

    if (bx < 0 || bx >= TETRIS_BREITE || by < 0 || by >= TETRIS_HOEHE) return true;
    if (tetris_spielfeld[bx][by] != 0) return true;
  }
  return false;
}

void tetris_tetrominoe_ablegen(const TetrisTetrominoe &t) {
  const TetrominoeForm &form = tetris_formen[t.art];
  uint16_t farbe = form.farbe;

  for (int i = 0; i < 4; i++) {
    int bx = t.x + form.blöcke[i][0];
    int by = t.y + form.blöcke[i][1];

    if (bx >= 0 && bx < TETRIS_BREITE && by >= 0 && by < TETRIS_HOEHE) {
      tetris_spielfeld[bx][by] = t.art + 1;
      tetris_farben[bx][by] = farbe;
    }
  }
}

int tetris_volle_reihen_loeschen() {
  int loescht = 0;

  for (int y = TETRIS_HOEHE - 1; y >= 0; y--) {
    bool voll = true;
    for (int x = 0; x < TETRIS_BREITE; x++) {
      if (tetris_spielfeld[x][y] == 0) {
        voll = false;
        break;
      }
    }

    if (voll) {
      loescht++;
      for (int yy = y; yy > 0; yy--) {
        for (int x = 0; x < TETRIS_BREITE; x++) {
          tetris_spielfeld[x][yy] = tetris_spielfeld[x][yy - 1];
          tetris_farben[x][yy] = tetris_farben[x][yy - 1];
        }
      }
      for (int x = 0; x < TETRIS_BREITE; x++) {
        tetris_spielfeld[x][0] = 0;
        tetris_farben[x][0] = 0;
      }
      y++;
    }
  }

  if (loescht > 0) {
    tetris_reihen += loescht;
    int punkte_array[5] = { 0, 40, 100, 300, 1200 };
    tetris_score += punkte_array[loescht] * tetris_level;
    tetris_level = 1 + (tetris_reihen / 10);
    tetris_gravity_ms = TETRIS_GRAVITY_START - (tetris_level - 1) * 20;
    if (tetris_gravity_ms < TETRIS_GRAVITY_MIN) tetris_gravity_ms = TETRIS_GRAVITY_MIN;
  }

  return loescht;
}

// ============================================================================
//  TETROMINOE-VERWALTUNG
// ============================================================================

void tetris_neues_tetrominoe() {
  tetris_aktuell = tetris_vorschau;
  tetris_aktuell.x = TETRIS_BREITE / 2;
  tetris_aktuell.y = 0;
  tetris_aktuell.rotation = 0;

  tetris_vorschau.art = random(0, TETRIS_ARTEN);
  tetris_vorschau.x = 0;
  tetris_vorschau.y = 0;
  tetris_vorschau.rotation = 0;

  if (tetris_kollision_pruefe(tetris_aktuell)) {
    tetris_phase = TETRIS_PHASE_ENDE;
  }
}

// ============================================================================
//  RENDERING-FUNKTIONEN
// ============================================================================

void tetris_zelle_zeichnen(int x, int y, uint16_t farbe, bool leer = false) {
  int px = TETRIS_FELD_X + x * TETRIS_ZELLENGR;
  int py = TETRIS_FELD_Y + y * TETRIS_ZELLENGR;

  if (leer) {
    tft.fillRect(px + 1, py + 1, TETRIS_ZELLENGR - 2, TETRIS_ZELLENGR - 2, FARBE_TETRIS_BRETT);
    tft.drawRect(px, py, TETRIS_ZELLENGR, TETRIS_ZELLENGR, FARBE_TETRIS_GRID);
  } else {
    tft.fillRect(px + 1, py + 1, TETRIS_ZELLENGR - 2, TETRIS_ZELLENGR - 2, farbe);
    tft.drawRect(px, py, TETRIS_ZELLENGR, TETRIS_ZELLENGR, FARBE_TETRIS_GRID);
  }
}

void tetris_spielfeld_zeichnen() {
  tft.fillRoundRect(TETRIS_FELD_X - 2, TETRIS_FELD_Y - 2,
                     TETRIS_BREITE * TETRIS_ZELLENGR + 4, TETRIS_HOEHE * TETRIS_ZELLENGR + 4,
                     4, FARBE_TETRIS_BRETT);

  for (int y = 0; y < TETRIS_HOEHE; y++) {
    for (int x = 0; x < TETRIS_BREITE; x++) {
      if (tetris_spielfeld[x][y] == 0) {
        tetris_zelle_zeichnen(x, y, 0, true);
      } else {
        tetris_zelle_zeichnen(x, y, tetris_farben[x][y], false);
      }
    }
  }
}

void tetris_tetrominoe_zeichnen(const TetrisTetrominoe &t, bool ghost = false) {
  const TetrominoeForm &form = tetris_formen[t.art];
  uint16_t farbe = ghost ? tft.color565(80, 80, 80) : form.farbe;

  for (int i = 0; i < 4; i++) {
    int bx = t.x + form.blöcke[i][0];
    int by = t.y + form.blöcke[i][1];

    if (bx >= 0 && bx < TETRIS_BREITE && by >= 0 && by < TETRIS_HOEHE) {
      tetris_zelle_zeichnen(bx, by, farbe, false);
    }
  }
}

void tetris_info_zeichnen() {
  int info_x = TETRIS_PREVIEW_X;
  int info_y = TETRIS_PREVIEW_Y;

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(FARBE_TETRIS_TEXT_AKTIV, FARBE_TETRIS_HINTERGRUND);

  tft.drawString("NEXT", info_x, info_y, 2);

  tft.fillRoundRect(info_x - 2, info_y + 24, 80, 80, 4, FARBE_TETRIS_BRETT);
  for (int i = 0; i < 4; i++) {
    int xx = (i % 2) * TETRIS_ZELLENGR + info_x;
    int yy = (i / 2) * TETRIS_ZELLENGR + info_y + 26;
    tft.drawRect(xx, yy, TETRIS_ZELLENGR, TETRIS_ZELLENGR, FARBE_TETRIS_GRID);
  }

  TetrisTetrominoe preview = tetris_vorschau;
  preview.x = info_x / TETRIS_ZELLENGR + 1;
  preview.y = info_y / TETRIS_ZELLENGR + 3;

  int info_y2 = info_y + 114;
  char score_text[20];
  snprintf(score_text, sizeof(score_text), "Score: %d", tetris_score);
  tft.drawString(score_text, info_x, info_y2, 1);

  char level_text[16];
  snprintf(level_text, sizeof(level_text), "Level: %d", tetris_level);
  tft.drawString(level_text, info_x, info_y2 + 14, 1);

  char zeilen_text[16];
  snprintf(zeilen_text, sizeof(zeilen_text), "Lines: %d", tetris_reihen);
  tft.drawString(zeilen_text, info_x, info_y2 + 28, 1);
}

void tetris_kopfzeile_zeichnen() {
  tft.fillRect(0, HEADER_HOEHE, SCREEN_W, TETRIS_FELD_Y - HEADER_HOEHE, FARBE_TETRIS_HINTERGRUND);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TETRIS_TEXT_AKTIV, FARBE_TETRIS_HINTERGRUND);
  tft.drawString("TETRIS", SCREEN_W / 2, HEADER_HOEHE + 8, 2);
}

void tetris_endanzeige_zeichnen() {
  int bx = SCREEN_W / 2 - 85, by = TETRIS_FELD_Y + (TETRIS_HOEHE * TETRIS_ZELLENGR) / 2 - 35;
  int bw = 170, bh = 70;

  tft.fillRoundRect(bx, by, bw, bh, 8, FARBE_TETRIS_BRETT);
  tft.drawRoundRect(bx, by, bw, bh, 8, FARBE_TETRIS_TEXT_AKTIV);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TETRIS_TEXT_AKTIV, FARBE_TETRIS_BRETT);

  tft.drawString("GAME OVER", SCREEN_W / 2, by + 18, 2);

  char score_text[20];
  snprintf(score_text, sizeof(score_text), "Score: %d", tetris_score);
  tft.drawString(score_text, SCREEN_W / 2, by + 42, 1);

  tft.drawString("Antippen fuer Neustart", SCREEN_W / 2, by + 58, 1);
}

void tetris_hs_anzeige_zeichnen() {
  tft.fillRect(0, HEADER_HOEHE, SCREEN_W, SCREEN_H - HEADER_HOEHE, FARBE_TETRIS_HINTERGRUND);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TETRIS_TEXT_AKTIV, FARBE_TETRIS_HINTERGRUND);
  tft.drawString("HIGH SCORES", SCREEN_W / 2, HEADER_HOEHE + 8, 2);

  int y = HEADER_HOEHE + 30;
  tft.setTextDatum(TL_DATUM);

  if (tetris_hs_anzahl == 0) {
    tft.setTextColor(FARBE_TETRIS_TEXT, FARBE_TETRIS_HINTERGRUND);
    tft.drawString("Keine Scores vorhanden", 12, y, 1);
  } else {
    for (int i = 0; i < tetris_hs_anzahl && i < 10; i++) {
      char line[40];
      snprintf(line, sizeof(line), "%d. %s - %d (%.10s)",
               i + 1, tetris_highscores[i].name, tetris_highscores[i].punkte,
               tetris_highscores[i].datum);
      tft.setTextColor(FARBE_TETRIS_TEXT, FARBE_TETRIS_HINTERGRUND);
      tft.drawString(line, 12, y, 1);
      y += 14;
    }
  }

  tft.drawString("Antippen fuer neues Spiel", 12, SCREEN_H - 20, 1);
}

// ============================================================================
//  SPIEL-FUNKTIONEN
// ============================================================================

void tetris_spiel_starten() {
  tetris_spielfeld_clearen();
  tetris_score = 0;
  tetris_level = 1;
  tetris_reihen = 0;
  tetris_phase = TETRIS_PHASE_SPIEL;
  tetris_letzte_gravity = millis();
  tetris_gravity_ms = TETRIS_GRAVITY_START;

  tetris_vorschau.art = random(0, TETRIS_ARTEN);
  tetris_vorschau.x = 0;
  tetris_vorschau.y = 0;
  tetris_vorschau.rotation = 0;

  tetris_hs_laden();

  tft.fillScreen(FARBE_TETRIS_HINTERGRUND);
  tetris_kopfzeile_zeichnen();
  tetris_spielfeld_zeichnen();
  tetris_info_zeichnen();
  tetris_neues_tetrominoe();
  tetris_tetrominoe_zeichnen(tetris_aktuell, false);
}

void tetris_tick_ausfuehren() {
  if (tetris_phase != TETRIS_PHASE_SPIEL) return;

  unsigned long jetzt = millis();
  if (jetzt - tetris_letzte_gravity < tetris_gravity_ms) return;

  tetris_letzte_gravity = jetzt;

  if (!tetris_kollision_pruefe(tetris_aktuell, 0, 1)) {
    // Tetrominoe kann runter
    tetris_tetrominoe_zeichnen(tetris_aktuell, false);  // Alte Position clearen
    tetris_aktuell.y++;
    tetris_tetrominoe_zeichnen(tetris_aktuell, false);
  } else {
    // Tetrominoe ablegen
    tetris_tetrominoe_ablegen(tetris_aktuell);
    tetris_spielfeld_zeichnen();

    // Volle Reihen löschen
    tetris_volle_reihen_loeschen();
    tetris_spielfeld_zeichnen();

    // Neues Tetrominoe
    tetris_neues_tetrominoe();
    if (tetris_phase == TETRIS_PHASE_ENDE) {
      tetris_endanzeige_zeichnen();
    } else {
      tetris_tetrominoe_zeichnen(tetris_aktuell, false);
      tetris_info_zeichnen();
    }
  }
}

void tetris_touch_behandeln() {
  if (tetris_phase == TETRIS_PHASE_ENDE) {
    tetris_hs_anzeige_zeichnen();
    return;
  }

  if (tetris_phase == TETRIS_PHASE_SPIEL) {
    int feld_breite = TETRIS_BREITE * TETRIS_ZELLENGR;
    int feld_hoehe = TETRIS_HOEHE * TETRIS_ZELLENGR;

    // Links-Rechts-Bewegung (linke Seite)
    if (touch_x >= TETRIS_FELD_X && touch_x < TETRIS_FELD_X + feld_breite / 3) {
      if (!tetris_kollision_pruefe(tetris_aktuell, -1, 0)) {
        tetris_tetrominoe_zeichnen(tetris_aktuell, false);
        tetris_aktuell.x--;
        tetris_tetrominoe_zeichnen(tetris_aktuell, false);
      }
    }
    // Rechts-Bewegung (rechte Seite)
    else if (touch_x > TETRIS_FELD_X + (feld_breite * 2 / 3) &&
             touch_x <= TETRIS_FELD_X + feld_breite) {
      if (!tetris_kollision_pruefe(tetris_aktuell, 1, 0)) {
        tetris_tetrominoe_zeichnen(tetris_aktuell, false);
        tetris_aktuell.x++;
        tetris_tetrominoe_zeichnen(tetris_aktuell, false);
      }
    }
    // Rotation (mittlerer Bereich) - vereinfacht, echte Rotation erfordert Rotation Matrix
    else if (touch_x >= TETRIS_FELD_X + feld_breite / 3 &&
             touch_x <= TETRIS_FELD_X + (feld_breite * 2 / 3)) {
      tetris_aktuell.rotation = (tetris_aktuell.rotation + 1) % 4;
      tetris_spielfeld_zeichnen();
      tetris_tetrominoe_zeichnen(tetris_aktuell, false);
    }
  }
}

void tetris_spiel_verlassen() {
  // Kein aktiver Timer nötig
}

