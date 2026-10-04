// ============================================================================
//  SPIEL: SNAKE (spiel_aktiv == SPIEL_SNAKE)
//  Eigene Datei: esp32-tft-snake.ino
//
//  Regeln:
//  - Wand rundherum: Kollision mit der Wand = Game Over
//  - Kollision mit dem eigenen Koerper = Game Over
//  - Nur EIN Essen gleichzeitig, sofort neues wenn gefressen
//  - Schlange waechst pro Essen um genau ein Segment
//  - Punktestand + Highscore oben
//  - Am Spielende: Score mit Datum/Uhrzeit in /highscores/snake.txt (SD)
//  - Nach Game Over: 15 s Top-10-Highscore-Liste (aktueller Eintrag hervorgehoben)
//  - Steuerung: Touch-Zonen (Dreiecke um Bildschirmmitte)
//
//  SD-Ordnerstruktur (empfohlen fuer alle Spiele):
//    /highscores/
//      snake.txt
//      pipe_mania.txt
//      sudoku_easy.txt / sudoku_mittel.txt / sudoku_schwer.txt
//      memory.txt
//      ...
//  Format pro Zeile (einfaches Text, sortiert absteigend nach Punkten):
//    PUNKTE;YYYY-MM-DD HH:MM:SS
//    Beispiel: 42;2026-10-04 14:30:15
// ============================================================================

// ----------------------------------------------------------------------------
//  SPIELFELD-EINSTELLUNGEN
// ----------------------------------------------------------------------------
#define SNAKE_FELD_X       10
#define SNAKE_FELD_Y       (HEADER_HOEHE + 10)
#define SNAKE_ZELLE         10
#define SNAKE_SPALTEN       30
#define SNAKE_ZEILEN         18
#define SNAKE_MAX_LAENGE   (SNAKE_SPALTEN * SNAKE_ZEILEN)

#define SNAKE_SCHRITT_MS    140

#define SNAKE_HS_MAX        10
#define SNAKE_HS_DATEI      "/highscores/snake.txt"
#define SNAKE_HS_ANZEIGE_MS 15000

// ----------------------------------------------------------------------------
//  FARBEN
// ----------------------------------------------------------------------------
#define FARBE_SNAKE_HINTERGRUND  tft.color565(12, 22, 40)
#define FARBE_SNAKE_RASTER       tft.color565(18, 32, 55)
#define FARBE_SNAKE_WAND          tft.color565(90, 110, 140)
#define FARBE_SNAKE_KOERPER       tft.color565(40, 175, 85)
#define FARBE_SNAKE_KOERPER2      tft.color565(30, 140, 70)
#define FARBE_SNAKE_KOPF          tft.color565(90, 240, 130)
#define FARBE_SNAKE_AUGE          tft.color565(20, 30, 40)
#define FARBE_SNAKE_ESSEN         tft.color565(230, 55, 55)
#define FARBE_SNAKE_ESSEN_GLANZ   tft.color565(255, 160, 160)
#define FARBE_SNAKE_TEXT          tft.color565(255, 255, 255)
#define FARBE_SNAKE_HS_AKTUELL    tft.color565(255, 210, 40)
#define FARBE_SNAKE_HS_ZEILE      tft.color565(25, 40, 65)

// ----------------------------------------------------------------------------
//  RICHTUNGEN
// ----------------------------------------------------------------------------
#define SNAKE_RICHTUNG_LINKS   0
#define SNAKE_RICHTUNG_RECHTS  1
#define SNAKE_RICHTUNG_OBEN    2
#define SNAKE_RICHTUNG_UNTEN   3

// ----------------------------------------------------------------------------
//  SPIELZUSTAND
// ----------------------------------------------------------------------------
struct SnakeSegment {
  int8_t spalte;
  int8_t zeile;
};

SnakeSegment snake_koerper[SNAKE_MAX_LAENGE];
int snake_laenge = 0;
int snake_richtung = SNAKE_RICHTUNG_RECHTS;
int snake_naechste_richtung = SNAKE_RICHTUNG_RECHTS;

int snake_essen_spalte = 0;
int snake_essen_zeile = 0;

int snake_punkte = 0;
int snake_highscore = 0;

bool snake_spiel_vorbei = false;
bool snake_spiel_laeuft = false;
bool snake_hs_anzeige = false;
unsigned long snake_hs_anzeige_start = 0;
int snake_hs_aktuell_rang = -1;

unsigned long snake_letzter_schritt = 0;

struct SnakeHsEintrag {
  int punkte;
  char zeitstempel[20];
};
SnakeHsEintrag snake_hs_liste[SNAKE_HS_MAX];
int snake_hs_anzahl = 0;

// ----------------------------------------------------------------------------
//  VORWAERTSDEKLARATIONEN
// ----------------------------------------------------------------------------
void snake_essen_neu_platzieren();
void snake_essen_zeichnen();
void snake_zelle_zeichnen(int spalte, int zeile, uint16_t farbe);
void snake_kopf_zeichnen(int spalte, int zeile);
void snake_rahmen_zeichnen();
void snake_kopfzeile_zeichnen();
void snake_hs_laden();
void snake_hs_liste_zeichnen();
void snake_spielfeld_zeichnen();

// ----------------------------------------------------------------------------
//  SD-BUS HELFER
// ----------------------------------------------------------------------------
static void snake_sd_bus_sichern() {
  digitalWrite(TOUCH_CS_PIN, HIGH);
  digitalWrite(SD_CS_PIN, HIGH);
#ifdef TFT_CS
  digitalWrite(TFT_CS, HIGH);
#endif
}

// ----------------------------------------------------------------------------
//  HIGHSCORE LADEN VON SD
// ----------------------------------------------------------------------------
void snake_hs_laden() {
  snake_hs_anzahl = 0;
  snake_highscore = 0;

  if (!sd_stand.mount_ok) return;

  snake_sd_bus_sichern();

  if (!SD.exists("/highscores")) {
    SD.mkdir("/highscores");
  }

  File f = SD.open(SNAKE_HS_DATEI, FILE_READ);
  if (!f) return;

  while (f.available() && snake_hs_anzahl < SNAKE_HS_MAX) {
    String zeile = f.readStringUntil('\n');
    zeile.trim();
    if (zeile.length() < 5) continue;

    int trenner = zeile.indexOf(';');
    if (trenner <= 0) continue;

    int pkt = zeile.substring(0, trenner).toInt();
    String ts = zeile.substring(trenner + 1);
    ts.trim();

    snake_hs_liste[snake_hs_anzahl].punkte = pkt;
    strncpy(snake_hs_liste[snake_hs_anzahl].zeitstempel,
            ts.c_str(), sizeof(snake_hs_liste[0].zeitstempel) - 1);
    snake_hs_liste[snake_hs_anzahl].zeitstempel[sizeof(snake_hs_liste[0].zeitstempel) - 1] = '\0';
    snake_hs_anzahl++;
  }
  f.close();

  if (snake_hs_anzahl > 0) {
    snake_highscore = snake_hs_liste[0].punkte;
  }
}

// ----------------------------------------------------------------------------
//  HIGHSCORE SPEICHERN
//  Rueckgabe: Rang 0..9 wenn in Top 10, sonst -1
// ----------------------------------------------------------------------------
int snake_hs_eintrag_einfuegen(int punkte) {
  char ts[20] = "---- -- -- --:--:--";
  struct tm zeitinfo;
  if (getLocalTime(&zeitinfo, 0)) {
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &zeitinfo);
  }

  int einfuege_pos = snake_hs_anzahl;
  for (int i = 0; i < snake_hs_anzahl; i++) {
    if (punkte > snake_hs_liste[i].punkte) {
      einfuege_pos = i;
      break;
    }
  }

  if (einfuege_pos >= SNAKE_HS_MAX) {
    return -1;
  }

  int neue_anzahl = snake_hs_anzahl < SNAKE_HS_MAX ? snake_hs_anzahl + 1 : SNAKE_HS_MAX;
  for (int i = neue_anzahl - 1; i > einfuege_pos; i--) {
    snake_hs_liste[i] = snake_hs_liste[i - 1];
  }

  snake_hs_liste[einfuege_pos].punkte = punkte;
  strncpy(snake_hs_liste[einfuege_pos].zeitstempel, ts,
          sizeof(snake_hs_liste[0].zeitstempel) - 1);
  snake_hs_liste[einfuege_pos].zeitstempel[sizeof(snake_hs_liste[0].zeitstempel) - 1] = '\0';
  snake_hs_anzahl = neue_anzahl;

  if (punkte > snake_highscore) {
    snake_highscore = punkte;
  }

  if (sd_stand.mount_ok) {
    snake_sd_bus_sichern();
    if (!SD.exists("/highscores")) {
      SD.mkdir("/highscores");
    }
    File f = SD.open(SNAKE_HS_DATEI, FILE_WRITE);
    if (f) {
      for (int i = 0; i < snake_hs_anzahl; i++) {
        f.printf("%d;%s\n", snake_hs_liste[i].punkte, snake_hs_liste[i].zeitstempel);
      }
      f.close();
    }
  }

  return einfuege_pos;
}

// ----------------------------------------------------------------------------
//  TOP-10 LISTE ZEICHNEN
// ----------------------------------------------------------------------------
void snake_hs_liste_zeichnen() {
  tft.fillScreen(FARBE_SNAKE_HINTERGRUND);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_SNAKE_HS_AKTUELL, FARBE_SNAKE_HINTERGRUND);
  tft.drawString("HIGHSCORE TOP 10", SCREEN_W / 2, 16, 2);

  char untertitel[40];
  snprintf(untertitel, sizeof(untertitel), "Diese Runde: %d Punkte", snake_punkte);
  tft.setTextColor(FARBE_SNAKE_TEXT, FARBE_SNAKE_HINTERGRUND);
  tft.drawString(untertitel, SCREEN_W / 2, 34, 1);

  int start_y = 48;
  int zeilen_h = 17;

  if (snake_hs_anzahl == 0) {
    tft.drawString("(noch keine Eintraege)", SCREEN_W / 2, start_y + 40, 1);
  } else {
    for (int i = 0; i < snake_hs_anzahl; i++) {
      int y = start_y + i * zeilen_h;
      bool ist_aktuell = (i == snake_hs_aktuell_rang);

      uint16_t bg = ist_aktuell ? tft.color565(60, 50, 20) : FARBE_SNAKE_HS_ZEILE;
      uint16_t fg = ist_aktuell ? FARBE_SNAKE_HS_AKTUELL : FARBE_SNAKE_TEXT;

      tft.fillRoundRect(8, y, SCREEN_W - 16, zeilen_h - 1, 3, bg);

      char zeile[48];
      snprintf(zeile, sizeof(zeile), "%2d.  %4d   %s",
               i + 1, snake_hs_liste[i].punkte, snake_hs_liste[i].zeitstempel);

      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(fg, bg);
      tft.drawString(zeile, 14, y + (zeilen_h - 1) / 2, 1);
    }
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(tft.color565(140, 160, 180), FARBE_SNAKE_HINTERGRUND);
  tft.drawString("Tippen = neues Spiel", SCREEN_W / 2, SCREEN_H - 12, 1);
}

// ----------------------------------------------------------------------------
//  SPIEL STARTEN
// ----------------------------------------------------------------------------
void snake_spiel_starten() {
  static bool seeded = false;
  if (!seeded) {
    randomSeed((uint32_t)esp_random());
    seeded = true;
  }

  snake_hs_laden();

  snake_laenge = 3;
  int start_spalte = SNAKE_SPALTEN / 2;
  int start_zeile  = SNAKE_ZEILEN / 2;

  for (int i = 0; i < snake_laenge; i++) {
    snake_koerper[i].spalte = start_spalte - i;
    snake_koerper[i].zeile  = start_zeile;
  }

  snake_richtung = SNAKE_RICHTUNG_RECHTS;
  snake_naechste_richtung = SNAKE_RICHTUNG_RECHTS;
  snake_punkte = 0;
  snake_spiel_vorbei = false;
  snake_spiel_laeuft = true;
  snake_hs_anzeige = false;
  snake_hs_aktuell_rang = -1;
  snake_letzter_schritt = millis();

  snake_essen_neu_platzieren();
  snake_spielfeld_zeichnen();
}

// ----------------------------------------------------------------------------
//  KOMPLETTES SPIELFELD ZEICHNEN
// ----------------------------------------------------------------------------
void snake_spielfeld_zeichnen() {
  tft.fillScreen(FARBE_SNAKE_HINTERGRUND);
  snake_kopfzeile_zeichnen();
  snake_rahmen_zeichnen();

  for (int z = 0; z < SNAKE_ZEILEN; z++) {
    for (int s = 0; s < SNAKE_SPALTEN; s++) {
      if (((s + z) & 1) == 0) {
        snake_zelle_zeichnen(s, z, FARBE_SNAKE_RASTER);
      }
    }
  }

  for (int i = snake_laenge - 1; i >= 1; i--) {
    uint16_t f = (i & 1) ? FARBE_SNAKE_KOERPER : FARBE_SNAKE_KOERPER2;
    snake_zelle_zeichnen(snake_koerper[i].spalte, snake_koerper[i].zeile, f);
  }
  snake_kopf_zeichnen(snake_koerper[0].spalte, snake_koerper[0].zeile);

  // Essen von Anfang an sichtbar
  snake_essen_zeichnen();
}

// ----------------------------------------------------------------------------
//  NEUES ESSEN
// ----------------------------------------------------------------------------
void snake_essen_neu_platzieren() {
  bool frei;
  int versuch_spalte, versuch_zeile;
  int max_versuche = SNAKE_SPALTEN * SNAKE_ZEILEN + 10;
  int versuche = 0;

  do {
    frei = true;
    versuch_spalte = random(0, SNAKE_SPALTEN);
    versuch_zeile  = random(0, SNAKE_ZEILEN);

    for (int i = 0; i < snake_laenge; i++) {
      if (snake_koerper[i].spalte == versuch_spalte &&
          snake_koerper[i].zeile  == versuch_zeile) {
        frei = false;
        break;
      }
    }
    versuche++;
  } while (!frei && versuche < max_versuche);

  snake_essen_spalte = versuch_spalte;
  snake_essen_zeile  = versuch_zeile;
}

// ----------------------------------------------------------------------------
//  ZEICHEN-HELFER
// ----------------------------------------------------------------------------
void snake_zelle_zeichnen(int spalte, int zeile, uint16_t farbe) {
  int px = SNAKE_FELD_X + spalte * SNAKE_ZELLE;
  int py = SNAKE_FELD_Y + zeile  * SNAKE_ZELLE;
  tft.fillRect(px, py, SNAKE_ZELLE - 1, SNAKE_ZELLE - 1, farbe);
}

void snake_essen_zeichnen() {
  int px = SNAKE_FELD_X + snake_essen_spalte * SNAKE_ZELLE;
  int py = SNAKE_FELD_Y + snake_essen_zeile  * SNAKE_ZELLE;
  int r  = (SNAKE_ZELLE - 2) / 2;
  int cx = px + r;
  int cy = py + r;
  tft.fillCircle(cx, cy, r, FARBE_SNAKE_ESSEN);
  tft.fillCircle(cx - 1, cy - 2, 1, FARBE_SNAKE_ESSEN_GLANZ);
}

void snake_kopf_zeichnen(int spalte, int zeile) {
  int px = SNAKE_FELD_X + spalte * SNAKE_ZELLE;
  int py = SNAKE_FELD_Y + zeile  * SNAKE_ZELLE;
  tft.fillRect(px, py, SNAKE_ZELLE - 1, SNAKE_ZELLE - 1, FARBE_SNAKE_KOPF);

  int ax1, ay1, ax2, ay2;
  switch (snake_richtung) {
    case SNAKE_RICHTUNG_RECHTS:
      ax1 = px + 6; ay1 = py + 2;
      ax2 = px + 6; ay2 = py + 6;
      break;
    case SNAKE_RICHTUNG_LINKS:
      ax1 = px + 2; ay1 = py + 2;
      ax2 = px + 2; ay2 = py + 6;
      break;
    case SNAKE_RICHTUNG_OBEN:
      ax1 = px + 2; ay1 = py + 2;
      ax2 = px + 6; ay2 = py + 2;
      break;
    default:
      ax1 = px + 2; ay1 = py + 6;
      ax2 = px + 6; ay2 = py + 6;
      break;
  }
  tft.fillRect(ax1, ay1, 2, 2, FARBE_SNAKE_AUGE);
  tft.fillRect(ax2, ay2, 2, 2, FARBE_SNAKE_AUGE);
}

void snake_rahmen_zeichnen() {
  int feld_breite_px = SNAKE_SPALTEN * SNAKE_ZELLE;
  int feld_hoehe_px  = SNAKE_ZEILEN  * SNAKE_ZELLE;
  tft.drawRect(SNAKE_FELD_X - 2, SNAKE_FELD_Y - 2,
               feld_breite_px + 4, feld_hoehe_px + 4, FARBE_SNAKE_WAND);
  tft.drawRect(SNAKE_FELD_X - 3, SNAKE_FELD_Y - 3,
               feld_breite_px + 6, feld_hoehe_px + 6, tft.color565(50, 70, 100));
}

void snake_kopfzeile_zeichnen() {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(FARBE_SNAKE_TEXT, FARBE_SNAKE_HINTERGRUND);
  tft.fillRect(0, 0, SCREEN_W, HEADER_HOEHE, FARBE_SNAKE_HINTERGRUND);

  char text_punkte[24];
  snprintf(text_punkte, sizeof(text_punkte), "Punkte: %d", snake_punkte);
  tft.drawString(text_punkte, 8, 8, 1);

  char text_highscore[28];
  snprintf(text_highscore, sizeof(text_highscore), "HS: %d", snake_highscore);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(text_highscore, SCREEN_W - 8, 8, 1);
}

// ----------------------------------------------------------------------------
//  EIN SPIELSCHRITT
// ----------------------------------------------------------------------------
void snake_schritt_ausfuehren() {
  if (snake_hs_anzeige) {
    if (millis() - snake_hs_anzeige_start >= SNAKE_HS_ANZEIGE_MS) {
      snake_hs_anzeige = false;
    }
    return;
  }

  if (!snake_spiel_laeuft) return;
  if (millis() - snake_letzter_schritt < SNAKE_SCHRITT_MS) return;
  snake_letzter_schritt = millis();

  bool ist_umkehr =
    (snake_naechste_richtung == SNAKE_RICHTUNG_LINKS  && snake_richtung == SNAKE_RICHTUNG_RECHTS) ||
    (snake_naechste_richtung == SNAKE_RICHTUNG_RECHTS && snake_richtung == SNAKE_RICHTUNG_LINKS)  ||
    (snake_naechste_richtung == SNAKE_RICHTUNG_OBEN   && snake_richtung == SNAKE_RICHTUNG_UNTEN)  ||
    (snake_naechste_richtung == SNAKE_RICHTUNG_UNTEN  && snake_richtung == SNAKE_RICHTUNG_OBEN);

  if (!ist_umkehr) {
    snake_richtung = snake_naechste_richtung;
  }

  int neue_spalte = snake_koerper[0].spalte;
  int neue_zeile  = snake_koerper[0].zeile;

  switch (snake_richtung) {
    case SNAKE_RICHTUNG_LINKS:  neue_spalte--; break;
    case SNAKE_RICHTUNG_RECHTS: neue_spalte++; break;
    case SNAKE_RICHTUNG_OBEN:   neue_zeile--;  break;
    case SNAKE_RICHTUNG_UNTEN:  neue_zeile++;  break;
  }

  if (neue_spalte < 0 || neue_spalte >= SNAKE_SPALTEN ||
      neue_zeile  < 0 || neue_zeile  >= SNAKE_ZEILEN) {
    snake_spiel_beenden();
    return;
  }

  for (int i = 0; i < snake_laenge; i++) {
    if (snake_koerper[i].spalte == neue_spalte &&
        snake_koerper[i].zeile  == neue_zeile) {
      snake_spiel_beenden();
      return;
    }
  }

  bool essen_gefressen =
    (neue_spalte == snake_essen_spalte && neue_zeile == snake_essen_zeile);

  int alte_laenge = snake_laenge;
  if (essen_gefressen && snake_laenge < SNAKE_MAX_LAENGE) {
    snake_laenge++;
  }

  SnakeSegment alter_schwanz = snake_koerper[alte_laenge - 1];

  for (int i = snake_laenge - 1; i > 0; i--) {
    snake_koerper[i] = snake_koerper[i - 1];
  }
  snake_koerper[0].spalte = neue_spalte;
  snake_koerper[0].zeile  = neue_zeile;

  if (!essen_gefressen) {
    uint16_t bg = (((alter_schwanz.spalte + alter_schwanz.zeile) & 1) == 0)
                    ? FARBE_SNAKE_RASTER : FARBE_SNAKE_HINTERGRUND;
    snake_zelle_zeichnen(alter_schwanz.spalte, alter_schwanz.zeile, bg);
  }

  if (snake_laenge >= 2) {
    snake_zelle_zeichnen(snake_koerper[1].spalte, snake_koerper[1].zeile, FARBE_SNAKE_KOERPER);
  }

  snake_kopf_zeichnen(snake_koerper[0].spalte, snake_koerper[0].zeile);

  if (essen_gefressen) {
    snake_punkte++;
    snake_kopfzeile_zeichnen();
    snake_essen_neu_platzieren();
    snake_essen_zeichnen();
  }
}

// ----------------------------------------------------------------------------
//  SPIEL BEENDEN
// ----------------------------------------------------------------------------
void snake_spiel_beenden() {
  snake_spiel_laeuft = false;
  snake_spiel_vorbei = true;

  snake_hs_aktuell_rang = snake_hs_eintrag_einfuegen(snake_punkte);

  snake_hs_anzeige = true;
  snake_hs_anzeige_start = millis();
  snake_hs_liste_zeichnen();
}

// ----------------------------------------------------------------------------
//  TOUCH-STEUERUNG
// ----------------------------------------------------------------------------
void snake_touch_behandeln() {
  if (snake_hs_anzeige || snake_spiel_vorbei) {
    snake_hs_anzeige = false;
    snake_spiel_starten();
    return;
  }

  if (!snake_spiel_laeuft) return;

  int mitte_x = SCREEN_W / 2;
  int mitte_y = SCREEN_H / 2;

  int dx = touch_x - mitte_x;
  int dy = touch_y - mitte_y;

  if (abs(dx) > abs(dy)) {
    snake_naechste_richtung = (dx > 0) ? SNAKE_RICHTUNG_RECHTS : SNAKE_RICHTUNG_LINKS;
  } else {
    snake_naechste_richtung = (dy > 0) ? SNAKE_RICHTUNG_UNTEN : SNAKE_RICHTUNG_OBEN;
  }
}

// ----------------------------------------------------------------------------
//  SNAKE VERLASSEN
// ----------------------------------------------------------------------------
void snake_spiel_verlassen() {
  snake_spiel_laeuft = false;
  snake_hs_anzeige = false;
}
