// ============================================================================
//  SPIELE-UEBERSICHTSSEITE (view_index == 3)
//  Eigene Datei: esp32-tft-spiele.ino
//  8 Kacheln, 2 Reihen zu 4: oben Singleplayer, unten Multiplayer.
//  Beim Antippen wird spiel_aktiv gesetzt, das eigentliche Spiel
//  (eigene Datei je Spiel) uebernimmt dann die Anzeige/Steuerung.
// ============================================================================

// ----------------------------------------------------------------------------
//  SPIEL-KENNUNGEN
//  -1 = kein Spiel aktiv (wir sind auf der Uebersichtsseite)

// ----------------------------------------------------------------------------
//  FARBEN FUER DIE SPIELE-KACHELN (kraeftig, jede eigene Farbe)
// ----------------------------------------------------------------------------
#define FARBE_SPIEL_SNAKE        tft.color565(40, 150, 60)
#define FARBE_SPIEL_PIPE_MANIA   tft.color565(150, 110, 30)
#define FARBE_SPIEL_TYCOON       tft.color565(200, 170, 20)
#define FARBE_SPIEL_SUDOKU       tft.color565(60, 90, 160)
#define FARBE_SPIEL_MEMORY       tft.color565(140, 50, 130)
#define FARBE_SPIEL_VIER_GEWINNT tft.color565(30, 130, 150)
#define FARBE_SPIEL_AIR_HOCKEY   tft.color565(160, 40, 60)
#define FARBE_SPIEL_PLATZHALTER  tft.color565(70, 70, 80)

// ----------------------------------------------------------------------------
//  DATENSTRUKTUR EINER SPIELE-KACHEL
// ----------------------------------------------------------------------------
struct SpielKachel {
  int x, y, breite, hoehe;
  uint16_t farbe;
  const char* name;
  int spiel_id;   // -1 = Platzhalter, kein Spiel hinterlegt
};

SpielKachel spiele_kacheln[8];

// ----------------------------------------------------------------------------
//  LAYOUT AUFBAUEN (einmalig beim Start aufrufen, z.B. in setup())
// ----------------------------------------------------------------------------
void spieleseite_layout_aufbauen() {
  int tile_w = 68;
  int tile_h = 85;
  int gap_x = 8;
  int gap_y = 10;
  int start_x = (SCREEN_W - (tile_w * 4 + gap_x * 3)) / 2;
  int start_y = HEADER_HOEHE + 12;

  // --- Reihe 1: Singleplayer ---
  spiele_kacheln[0] = { start_x, start_y, tile_w, tile_h,
    FARBE_SPIEL_SNAKE, "Snake", SPIEL_SNAKE };

  spiele_kacheln[1] = { start_x + (tile_w + gap_x), start_y, tile_w, tile_h,
    FARBE_SPIEL_PIPE_MANIA, "Pipe Mania", SPIEL_PIPE_MANIA };

  spiele_kacheln[2] = { start_x + (tile_w + gap_x) * 2, start_y, tile_w, tile_h,
    FARBE_SPIEL_TYCOON, "Limo Tycoon", SPIEL_TYCOON };

  spiele_kacheln[3] = { start_x + (tile_w + gap_x) * 3, start_y, tile_w, tile_h,
    FARBE_SPIEL_SUDOKU, "Sudoku", SPIEL_SUDOKU };

  // --- Reihe 2: Multiplayer ---
  int y2 = start_y + tile_h + gap_y;

  spiele_kacheln[4] = { start_x, y2, tile_w, tile_h,
    FARBE_SPIEL_MEMORY, "Memory", SPIEL_MEMORY };

  spiele_kacheln[5] = { start_x + (tile_w + gap_x), y2, tile_w, tile_h,
    FARBE_SPIEL_VIER_GEWINNT, "4 Gewinnt", SPIEL_VIER_GEWINNT };

  spiele_kacheln[6] = { start_x + (tile_w + gap_x) * 2, y2, tile_w, tile_h,
    FARBE_SPIEL_AIR_HOCKEY, "Air Hockey", SPIEL_AIR_HOCKEY };

  spiele_kacheln[7] = { start_x + (tile_w + gap_x) * 3, y2, tile_w, tile_h,
    FARBE_SPIEL_PLATZHALTER, "---", -1 };
}

// ----------------------------------------------------------------------------
//  SPIELE-UEBERSICHT ZEICHNEN
//  Bei view_index == 3 UND spiel_aktiv == SPIEL_KEINS aus
//  bildschirm_komplett_neu_zeichnen() aufrufen
// ----------------------------------------------------------------------------
void spieleseite_zeichnen() {
  for (int i = 0; i < 8; i++) {
    SpielKachel &k = spiele_kacheln[i];

    tft.fillRoundRect(k.x, k.y, k.breite, k.hoehe, 8, k.farbe);
    tft.drawRoundRect(k.x, k.y, k.breite, k.hoehe, 8, FARBE_RAHMEN);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(FARBE_TEXT, k.farbe);
    tft.drawString(k.name, k.x + k.breite / 2, k.y + k.hoehe / 2, 1);
  }

  // kleine Trennlinie zwischen Singleplayer- und Multiplayer-Reihe, mit Beschriftung
  int trenn_y = spiele_kacheln[0].y + spiele_kacheln[0].hoehe + 5;
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(tft.color565(180, 180, 190), FARBE_HINTERGRUND);
}

// ----------------------------------------------------------------------------
//  TOUCH-BEHANDLUNG FUER DIE SPIELE-UEBERSICHT
//  In touch_abfragen() aufrufen, wenn view_index == 3 und kein Spiel aktiv ist
// ----------------------------------------------------------------------------
void spieleseite_touch_behandeln() {
  for (int i = 0; i < 8; i++) {
    SpielKachel &k = spiele_kacheln[i];
    if (touch_x >= k.x && touch_x <= k.x + k.breite &&
        touch_y >= k.y && touch_y <= k.y + k.hoehe) {
      blink_kachel_index = i;
      blink_start_zeit = millis();
      kachel_rahmen_zeichnen(k.x, k.y, k.breite, k.hoehe, FARBE_RAHMEN_AKTIV);
      if (k.spiel_id != -1) {
        spiel_aktiv = k.spiel_id;
        // TODO: hier jeweiliges Spiel-Setup aufrufen, sobald die einzelnen
        // Spiele-Dateien existieren, z.B.:
        // if (spiel_aktiv == SPIEL_SNAKE) snake_spiel_starten();
      }
      return;
    }
  }
}