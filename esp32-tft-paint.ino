// ============================================================================
//  PROGRAMM: PAINT (prog_aktiv == PROG_PAINT)
//  Freihandzeichnen mit dem Finger. Palette unten, Leinwand direkt auf dem
//  Display (kein Extra-Framebuffer, Zeichnung geht bei Home/Standby verloren).
// ============================================================================

#define PAINT_TOOL_H     30
#define PAINT_TOOL_Y     (SCREEN_H - PAINT_TOOL_H)
#define PAINT_FARBEN_N   8
#define PAINT_RADIUS     3

uint16_t paint_palette[PAINT_FARBEN_N];
uint16_t paint_farbe = 0xFFFF;
int paint_letzter_x = -1;
int paint_letzter_y = -1;
bool paint_werkzeug_verbraucht = false;

void paint_palette_fuellen() {
  paint_palette[0] = tft.color565(0, 0, 0);
  paint_palette[1] = tft.color565(255, 255, 255);
  paint_palette[2] = tft.color565(200, 40, 40);
  paint_palette[3] = tft.color565(220, 120, 30);
  paint_palette[4] = tft.color565(220, 200, 40);
  paint_palette[5] = tft.color565(40, 160, 70);
  paint_palette[6] = tft.color565(40, 90, 200);
  paint_palette[7] = tft.color565(160, 50, 160);
}

void paint_werkzeugleiste_zeichnen() {
  tft.fillRect(0, PAINT_TOOL_Y, SCREEN_W, PAINT_TOOL_H, tft.color565(30, 32, 38));
  int swatch = 28;
  int gap = 2;
  int start_x = 4;
  for (int i = 0; i < PAINT_FARBEN_N; i++) {
    int x = start_x + i * (swatch + gap);
    int y = PAINT_TOOL_Y + 2;
    tft.fillRoundRect(x, y, swatch, PAINT_TOOL_H - 4, 3, paint_palette[i]);
    if (paint_palette[i] == paint_farbe) {
      tft.drawRoundRect(x, y, swatch, PAINT_TOOL_H - 4, 3, FARBE_RAHMEN_AKTIV);
      tft.drawRoundRect(x + 1, y + 1, swatch - 2, PAINT_TOOL_H - 6, 2, FARBE_RAHMEN_AKTIV);
    } else {
      tft.drawRoundRect(x, y, swatch, PAINT_TOOL_H - 4, 3, FARBE_RAHMEN);
    }
  }
  int bx = start_x + PAINT_FARBEN_N * (swatch + gap) + 4;
  tft.fillRoundRect(bx, PAINT_TOOL_Y + 2, SCREEN_W - bx - 4, PAINT_TOOL_H - 4, 3,
                    tft.color565(70, 70, 78));
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TEXT, tft.color565(70, 70, 78));
  tft.drawString("C", bx + (SCREEN_W - bx - 4) / 2, PAINT_TOOL_Y + PAINT_TOOL_H / 2, 2);
}

void paint_leinwand_leeren() {
  tft.fillRect(0, HEADER_HOEHE, SCREEN_W, PAINT_TOOL_Y - HEADER_HOEHE, TFT_BLACK);
}

void paint_starten() {
  paint_palette_fuellen();
  paint_farbe = paint_palette[1];
  paint_letzter_x = -1;
  paint_werkzeug_verbraucht = false;
  paint_leinwand_leeren();
  paint_werkzeugleiste_zeichnen();
}

void paint_verlassen() {
  paint_letzter_x = -1;
}

void paint_strich_ende() {
  paint_letzter_x = -1;
  paint_werkzeug_verbraucht = false;
}

void paint_punkt_clip(int x, int y) {
  int y_min = HEADER_HOEHE + PAINT_RADIUS;
  int y_max = PAINT_TOOL_Y - 1 - PAINT_RADIUS;
  if (y < y_min) y = y_min;
  if (y > y_max) y = y_max;
  if (x < PAINT_RADIUS) x = PAINT_RADIUS;
  if (x > SCREEN_W - 1 - PAINT_RADIUS) x = SCREEN_W - 1 - PAINT_RADIUS;
  tft.fillCircle(x, y, PAINT_RADIUS, paint_farbe);
}

void paint_linie(int x0, int y0, int x1, int y1) {
  int dx = abs(x1 - x0);
  int dy = abs(y1 - y0);
  int steps = dx > dy ? dx : dy;
  if (steps == 0) {
    paint_punkt_clip(x0, y0);
    return;
  }
  for (int i = 0; i <= steps; i++) {
    int x = x0 + (x1 - x0) * i / steps;
    int y = y0 + (y1 - y0) * i / steps;
    paint_punkt_clip(x, y);
  }
}

void paint_werkzeug_antippen(int x) {
  int swatch = 28;
  int gap = 2;
  int start_x = 4;
  for (int i = 0; i < PAINT_FARBEN_N; i++) {
    int sx = start_x + i * (swatch + gap);
    if (x >= sx && x < sx + swatch) {
      paint_farbe = paint_palette[i];
      paint_werkzeugleiste_zeichnen();
      return;
    }
  }
  paint_leinwand_leeren();
}

void paint_touch_halten() {
  if (touch_y >= PAINT_TOOL_Y) {
    if (!paint_werkzeug_verbraucht) {
      paint_werkzeug_antippen(touch_x);
      paint_werkzeug_verbraucht = true;
    }
    paint_letzter_x = -1;
    return;
  }
  if (touch_y <= HEADER_HOEHE) {
    return;
  }
  paint_werkzeug_verbraucht = false;
  if (paint_letzter_x < 0) {
    paint_punkt_clip(touch_x, touch_y);
  } else {
    paint_linie(paint_letzter_x, paint_letzter_y, touch_x, touch_y);
  }
  paint_letzter_x = touch_x;
  paint_letzter_y = touch_y;
}
