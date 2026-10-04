// ============================================================================
//  PROGRAMME-UEBERSICHT (view_index == 6)
//  Eigene Datei: esp32-tft-programme.ino
//  8 Kacheln wie die Spiele-Seite: Paint, Rechner und weitere Werkzeuge.
// ============================================================================

#define FARBE_PROG_PAINT     tft.color565(40, 90, 150)
#define FARBE_PROG_RECHNER   tft.color565(50, 120, 90)
#define FARBE_PROG_STOPPUHR  tft.color565(140, 90, 40)
#define FARBE_PROG_TIMER     tft.color565(130, 60, 70)
#define FARBE_PROG_DATEIEN   tft.color565(50, 100, 130)
#define FARBE_PROG_WUERFEL   tft.color565(90, 70, 140)
#define FARBE_PROG_LAMPE     tft.color565(170, 150, 50)
#define FARBE_PROG_INFO      tft.color565(70, 80, 95)

struct ProgKachel {
  int x, y, breite, hoehe;
  uint16_t farbe;
  const char* name;
  int prog_id;
};

ProgKachel programme_kacheln[8];

void programme_layout_aufbauen() {
  int tile_w = 68;
  int tile_h = 85;
  int gap_x = 8;
  int gap_y = 10;
  int start_x = (SCREEN_W - (tile_w * 4 + gap_x * 3)) / 2;
  int start_y = HEADER_HOEHE + 12;

  programme_kacheln[0] = { start_x, start_y, tile_w, tile_h,
    FARBE_PROG_PAINT, "Paint", PROG_PAINT };
  programme_kacheln[1] = { start_x + (tile_w + gap_x), start_y, tile_w, tile_h,
    FARBE_PROG_RECHNER, "Rechner", PROG_RECHNER };
  programme_kacheln[2] = { start_x + (tile_w + gap_x) * 2, start_y, tile_w, tile_h,
    FARBE_PROG_STOPPUHR, "Stoppuhr", PROG_STOPPUHR };
  programme_kacheln[3] = { start_x + (tile_w + gap_x) * 3, start_y, tile_w, tile_h,
    FARBE_PROG_TIMER, "Timer", PROG_TIMER };

  int y2 = start_y + tile_h + gap_y;
  programme_kacheln[4] = { start_x, y2, tile_w, tile_h,
    FARBE_PROG_DATEIEN, "Dateien", PROG_DATEIEN };
  programme_kacheln[5] = { start_x + (tile_w + gap_x), y2, tile_w, tile_h,
    FARBE_PROG_WUERFEL, "Wuerfel", PROG_WUERFEL };
  programme_kacheln[6] = { start_x + (tile_w + gap_x) * 2, y2, tile_w, tile_h,
    FARBE_PROG_LAMPE, "Lampe", PROG_LAMPE };
  programme_kacheln[7] = { start_x + (tile_w + gap_x) * 3, y2, tile_w, tile_h,
    FARBE_PROG_INFO, "Info", PROG_INFO };
}

void programme_zeichnen() {
  for (int i = 0; i < 8; i++) {
    ProgKachel &k = programme_kacheln[i];
    tft.fillRoundRect(k.x, k.y, k.breite, k.hoehe, 8, k.farbe);
    uint16_t rahmen = (blink_kachel_index == i) ? FARBE_RAHMEN_AKTIV : FARBE_RAHMEN;
    tft.drawRoundRect(k.x, k.y, k.breite, k.hoehe, 8, rahmen);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(FARBE_TEXT, k.farbe);
    tft.drawString(k.name, k.x + k.breite / 2, k.y + k.hoehe / 2, 1);
  }
}

void programme_touch_behandeln() {
  for (int i = 0; i < 8; i++) {
    ProgKachel &k = programme_kacheln[i];
    if (touch_x >= k.x && touch_x <= k.x + k.breite &&
        touch_y >= k.y && touch_y <= k.y + k.hoehe) {
      blink_kachel_index = i;
      blink_start_zeit = millis();
      kachel_rahmen_zeichnen(k.x, k.y, k.breite, k.hoehe, FARBE_RAHMEN_AKTIV);
      if (k.prog_id != PROG_KEINS) {
        prog_aktiv = k.prog_id;
      }
      return;
    }
  }
}

void programm_aktiv_verlassen() {
  if (prog_aktiv == PROG_PAINT) {
    paint_verlassen();
  } else if (prog_aktiv == PROG_RECHNER) {
    rechner_verlassen();
  } else if (prog_aktiv == PROG_STOPPUHR) {
    stoppuhr_verlassen();
  } else if (prog_aktiv == PROG_TIMER) {
    timer_verlassen();
  } else if (prog_aktiv == PROG_DATEIEN) {
    dateien_verlassen();
  } else if (prog_aktiv == PROG_WUERFEL) {
    wuerfel_verlassen();
  } else if (prog_aktiv == PROG_LAMPE) {
    lampe_verlassen();
  } else if (prog_aktiv == PROG_INFO) {
    info_verlassen();
  }
  prog_aktiv = PROG_KEINS;
}
