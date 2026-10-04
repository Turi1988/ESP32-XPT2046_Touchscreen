// ============================================================================
//  PROGRAMME: STOPPUHR, TIMER, WUERFEL, LAMPE, INFO
//  Eigene Datei: esp32-tft-tools.ino
// ============================================================================

// ----------------------------------------------------------------------------
//  STOPPUHR
// ----------------------------------------------------------------------------
bool stoppuhr_laeuft = false;
unsigned long stoppuhr_start_ms = 0;
unsigned long stoppuhr_stand_ms = 0;
unsigned long stoppuhr_letzte_anzeige = 0;
int stop_btn_start_x, stop_btn_reset_x, stop_btn_y, stop_btn_w, stop_btn_h;

String stoppuhr_text(unsigned long ms) {
  unsigned long cs = (ms / 10) % 100;
  unsigned long se = (ms / 1000) % 60;
  unsigned long mi = (ms / 60000) % 100;
  char buf[12];
  snprintf(buf, sizeof(buf), "%02lu:%02lu.%02lu", mi, se, cs);
  return String(buf);
}

unsigned long stoppuhr_aktuell() {
  if (stoppuhr_laeuft) {
    return stoppuhr_stand_ms + (millis() - stoppuhr_start_ms);
  }
  return stoppuhr_stand_ms;
}

void stoppuhr_zeit_zeichnen() {
  tft.fillRect(10, HEADER_HOEHE + 24, SCREEN_W - 20, 54, FARBE_HINTERGRUND);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TEXT, FARBE_HINTERGRUND);
  tft.drawString(stoppuhr_text(stoppuhr_aktuell()), SCREEN_W / 2, HEADER_HOEHE + 50, 6);
}

void stoppuhr_knoepfe_zeichnen() {
  stop_btn_w = 120;
  stop_btn_h = 36;
  stop_btn_y = SCREEN_H - 52;
  stop_btn_start_x = 20;
  stop_btn_reset_x = SCREEN_W - 20 - stop_btn_w;
  uint16_t startfarbe = stoppuhr_laeuft ? tft.color565(140, 50, 50) : tft.color565(30, 140, 90);
  tft.fillRoundRect(stop_btn_start_x, stop_btn_y, stop_btn_w, stop_btn_h, 6, startfarbe);
  tft.drawRoundRect(stop_btn_start_x, stop_btn_y, stop_btn_w, stop_btn_h, 6, FARBE_RAHMEN);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TEXT, startfarbe);
  tft.drawString(stoppuhr_laeuft ? "Stop" : "Start",
                 stop_btn_start_x + stop_btn_w / 2, stop_btn_y + stop_btn_h / 2, 2);

  tft.fillRoundRect(stop_btn_reset_x, stop_btn_y, stop_btn_w, stop_btn_h, 6,
                    tft.color565(50, 70, 95));
  tft.drawRoundRect(stop_btn_reset_x, stop_btn_y, stop_btn_w, stop_btn_h, 6, FARBE_RAHMEN);
  tft.setTextColor(FARBE_TEXT, tft.color565(50, 70, 95));
  tft.drawString("Reset", stop_btn_reset_x + stop_btn_w / 2, stop_btn_y + stop_btn_h / 2, 2);
}

void stoppuhr_starten() {
  tft.fillRect(0, HEADER_HOEHE, SCREEN_W, SCREEN_H - HEADER_HOEHE, FARBE_HINTERGRUND);
  stoppuhr_zeit_zeichnen();
  stoppuhr_knoepfe_zeichnen();
}

void stoppuhr_verlassen() {
  if (stoppuhr_laeuft) {
    stoppuhr_stand_ms = stoppuhr_aktuell();
    stoppuhr_laeuft = false;
  }
}

void stoppuhr_tick_ausfuehren() {
  if (!stoppuhr_laeuft) return;
  unsigned long jetzt = millis();
  if (jetzt - stoppuhr_letzte_anzeige < 80) return;
  stoppuhr_letzte_anzeige = jetzt;
  stoppuhr_zeit_zeichnen();
}

void stoppuhr_touch_behandeln() {
  if (touch_y >= stop_btn_y && touch_y <= stop_btn_y + stop_btn_h) {
    if (touch_x >= stop_btn_start_x && touch_x <= stop_btn_start_x + stop_btn_w) {
      if (stoppuhr_laeuft) {
        stoppuhr_stand_ms = stoppuhr_aktuell();
        stoppuhr_laeuft = false;
      } else {
        stoppuhr_start_ms = millis();
        stoppuhr_laeuft = true;
      }
      stoppuhr_knoepfe_zeichnen();
      stoppuhr_zeit_zeichnen();
    } else if (touch_x >= stop_btn_reset_x && touch_x <= stop_btn_reset_x + stop_btn_w) {
      stoppuhr_laeuft = false;
      stoppuhr_stand_ms = 0;
      stoppuhr_knoepfe_zeichnen();
      stoppuhr_zeit_zeichnen();
    }
  }
}

// ----------------------------------------------------------------------------
//  TIMER (Countdown)
// ----------------------------------------------------------------------------
int timer_minuten = 5;
bool timer_laeuft = false;
unsigned long timer_ende_ms = 0;
unsigned long timer_letzte_anzeige = 0;
bool timer_alarm = false;
int timer_plus_x, timer_minus_x, timer_go_x, timer_btn_y, timer_btn_w, timer_btn_h;

void timer_rest_zeichnen() {
  unsigned long rest = 0;
  if (timer_laeuft) {
    unsigned long jetzt = millis();
    rest = (jetzt >= timer_ende_ms) ? 0 : (timer_ende_ms - jetzt);
  } else if (!timer_alarm) {
    rest = (unsigned long)timer_minuten * 60000UL;
  }
  unsigned long se = (rest / 1000) % 60;
  unsigned long mi = rest / 60000;
  char buf[8];
  snprintf(buf, sizeof(buf), "%02lu:%02lu", mi, se);

  bool flash = timer_alarm && ((millis() / 400) % 2 == 0);
  uint16_t bg = flash ? tft.color565(140, 40, 40) : FARBE_HINTERGRUND;
  tft.fillRect(10, HEADER_HOEHE + 20, SCREEN_W - 20, 70, bg);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TEXT, bg);
  tft.drawString(buf, SCREEN_W / 2, HEADER_HOEHE + 52, 6);
}

void timer_knoepfe_zeichnen() {
  timer_btn_w = 88;
  timer_btn_h = 36;
  timer_btn_y = SCREEN_H - 52;
  timer_minus_x = 12;
  timer_plus_x = 116;
  timer_go_x = 220;
  if (timer_laeuft) {
    tft.fillRoundRect(12, timer_btn_y, SCREEN_W - 24, timer_btn_h, 6,
                      tft.color565(140, 50, 50));
    tft.drawRoundRect(12, timer_btn_y, SCREEN_W - 24, timer_btn_h, 6, FARBE_RAHMEN);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(FARBE_TEXT, tft.color565(140, 50, 50));
    tft.drawString("Abbrechen", SCREEN_W / 2, timer_btn_y + timer_btn_h / 2, 2);
    return;
  }
  tft.fillRoundRect(timer_minus_x, timer_btn_y, timer_btn_w, timer_btn_h, 6,
                    tft.color565(50, 70, 95));
  tft.fillRoundRect(timer_plus_x, timer_btn_y, timer_btn_w, timer_btn_h, 6,
                    tft.color565(50, 70, 95));
  tft.fillRoundRect(timer_go_x, timer_btn_y, timer_btn_w, timer_btn_h, 6,
                    tft.color565(30, 140, 90));
  tft.drawRoundRect(timer_minus_x, timer_btn_y, timer_btn_w, timer_btn_h, 6, FARBE_RAHMEN);
  tft.drawRoundRect(timer_plus_x, timer_btn_y, timer_btn_w, timer_btn_h, 6, FARBE_RAHMEN);
  tft.drawRoundRect(timer_go_x, timer_btn_y, timer_btn_w, timer_btn_h, 6, FARBE_RAHMEN);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TEXT, tft.color565(50, 70, 95));
  tft.drawString("-1 min", timer_minus_x + timer_btn_w / 2, timer_btn_y + timer_btn_h / 2, 1);
  tft.drawString("+1 min", timer_plus_x + timer_btn_w / 2, timer_btn_y + timer_btn_h / 2, 1);
  tft.setTextColor(FARBE_TEXT, tft.color565(30, 140, 90));
  tft.drawString("Start", timer_go_x + timer_btn_w / 2, timer_btn_y + timer_btn_h / 2, 2);
}

void timer_starten() {
  tft.fillRect(0, HEADER_HOEHE, SCREEN_W, SCREEN_H - HEADER_HOEHE, FARBE_HINTERGRUND);
  timer_rest_zeichnen();
  timer_knoepfe_zeichnen();
}

void timer_verlassen() {
  timer_laeuft = false;
  timer_alarm = false;
}

void timer_tick_ausfuehren() {
  if (!timer_laeuft && !timer_alarm) return;
  unsigned long jetzt = millis();
  if (timer_laeuft && jetzt >= timer_ende_ms) {
    timer_laeuft = false;
    timer_alarm = true;
    timer_knoepfe_zeichnen();
  }
  if (jetzt - timer_letzte_anzeige < 200) return;
  timer_letzte_anzeige = jetzt;
  timer_rest_zeichnen();
}

void timer_touch_behandeln() {
  if (touch_y < timer_btn_y || touch_y > timer_btn_y + timer_btn_h) return;
  if (timer_laeuft || timer_alarm) {
    timer_laeuft = false;
    timer_alarm = false;
    timer_starten();
    return;
  }
  if (touch_x >= timer_minus_x && touch_x <= timer_minus_x + timer_btn_w) {
    if (timer_minuten > 1) timer_minuten--;
    timer_rest_zeichnen();
  } else if (touch_x >= timer_plus_x && touch_x <= timer_plus_x + timer_btn_w) {
    if (timer_minuten < 99) timer_minuten++;
    timer_rest_zeichnen();
  } else if (touch_x >= timer_go_x && touch_x <= timer_go_x + timer_btn_w) {
    timer_laeuft = true;
    timer_alarm = false;
    timer_ende_ms = millis() + (unsigned long)timer_minuten * 60000UL;
    timer_knoepfe_zeichnen();
    timer_rest_zeichnen();
  }
}

// ----------------------------------------------------------------------------
//  WUERFEL
// ----------------------------------------------------------------------------
int wuerfel_augen[2] = { 1, 1 };
bool wuerfel_rollt = false;
unsigned long wuerfel_roll_start = 0;
unsigned long wuerfel_letzter_tick = 0;
int wuerfel_btn_x, wuerfel_btn_y, wuerfel_btn_w, wuerfel_btn_h;

void wuerfel_wuerfel_zeichnen(int cx, int cy, int augen) {
  int s = 72;
  tft.fillRoundRect(cx - s / 2, cy - s / 2, s, s, 10, tft.color565(245, 245, 240));
  tft.drawRoundRect(cx - s / 2, cy - s / 2, s, s, 10, tft.color565(40, 40, 45));
  uint16_t punkt = tft.color565(20, 20, 24);
  int d = 16;
  if (augen == 1 || augen == 3 || augen == 5) tft.fillCircle(cx, cy, 6, punkt);
  if (augen >= 2) {
    tft.fillCircle(cx - d, cy - d, 6, punkt);
    tft.fillCircle(cx + d, cy + d, 6, punkt);
  }
  if (augen >= 4) {
    tft.fillCircle(cx + d, cy - d, 6, punkt);
    tft.fillCircle(cx - d, cy + d, 6, punkt);
  }
  if (augen == 6) {
    tft.fillCircle(cx - d, cy, 6, punkt);
    tft.fillCircle(cx + d, cy, 6, punkt);
  }
}

void wuerfel_paar_zeichnen() {
  int cy = HEADER_HOEHE + 78;
  wuerfel_wuerfel_zeichnen(SCREEN_W / 2 - 56, cy, wuerfel_augen[0]);
  wuerfel_wuerfel_zeichnen(SCREEN_W / 2 + 56, cy, wuerfel_augen[1]);
}

void wuerfel_knoepfe_zeichnen() {
  wuerfel_btn_w = 160;
  wuerfel_btn_h = 34;
  wuerfel_btn_x = (SCREEN_W - wuerfel_btn_w) / 2;
  wuerfel_btn_y = SCREEN_H - 48;
  tft.fillRoundRect(wuerfel_btn_x, wuerfel_btn_y, wuerfel_btn_w, wuerfel_btn_h, 6,
                    tft.color565(90, 70, 140));
  tft.drawRoundRect(wuerfel_btn_x, wuerfel_btn_y, wuerfel_btn_w, wuerfel_btn_h, 6, FARBE_RAHMEN);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TEXT, tft.color565(90, 70, 140));
  tft.drawString(wuerfel_rollt ? "..." : "Wuerfeln",
                 wuerfel_btn_x + wuerfel_btn_w / 2, wuerfel_btn_y + wuerfel_btn_h / 2, 2);
}

void wuerfel_starten() {
  tft.fillRect(0, HEADER_HOEHE, SCREEN_W, SCREEN_H - HEADER_HOEHE, FARBE_HINTERGRUND);
  wuerfel_paar_zeichnen();
  wuerfel_knoepfe_zeichnen();
}

void wuerfel_verlassen() {
  wuerfel_rollt = false;
}

void wuerfel_tick_ausfuehren() {
  if (!wuerfel_rollt) return;
  unsigned long jetzt = millis();
  if (jetzt - wuerfel_letzter_tick < 70) return;
  wuerfel_letzter_tick = jetzt;
  wuerfel_augen[0] = 1 + (int)(jetzt % 6);
  wuerfel_augen[1] = 1 + (int)((jetzt / 7) % 6);
  wuerfel_paar_zeichnen();
  if (jetzt - wuerfel_roll_start > 700) {
    wuerfel_augen[0] = 1 + random(0, 6);
    wuerfel_augen[1] = 1 + random(0, 6);
    wuerfel_rollt = false;
    wuerfel_paar_zeichnen();
    wuerfel_knoepfe_zeichnen();
  }
}

void wuerfel_touch_behandeln() {
  if (wuerfel_rollt) return;
  if (touch_x >= wuerfel_btn_x && touch_x <= wuerfel_btn_x + wuerfel_btn_w &&
      touch_y >= wuerfel_btn_y && touch_y <= wuerfel_btn_y + wuerfel_btn_h) {
    wuerfel_rollt = true;
    wuerfel_roll_start = millis();
    wuerfel_knoepfe_zeichnen();
  }
}

// ----------------------------------------------------------------------------
//  LAMPE
// ----------------------------------------------------------------------------
int lampe_stufe = 2;  // 0 aus, 1 dimm, 2 hell

uint16_t lampe_farbe() {
  if (lampe_stufe == 0) return tft.color565(10, 12, 16);
  if (lampe_stufe == 1) return tft.color565(180, 170, 140);
  return tft.color565(255, 250, 230);
}

void lampe_starten() {
  // Kopfleiste neu zeichnen, damit Home-Button immer erreichbar bleibt
  header_zeichnen();

  uint16_t farbe = lampe_farbe();
  tft.fillRect(0, HEADER_HOEHE, SCREEN_W, SCREEN_H - HEADER_HOEHE, farbe);
  tft.setTextDatum(MC_DATUM);
  uint16_t text = (lampe_stufe == 0) ? FARBE_TEXT : tft.color565(30, 30, 32);
  tft.setTextColor(text, farbe);
  const char* label = (lampe_stufe == 0) ? "AUS" : (lampe_stufe == 1 ? "DIMM" : "HELL");
  tft.drawString(label, SCREEN_W / 2, SCREEN_H / 2, 4);
  tft.drawString("antippen zum Umschalten", SCREEN_W / 2, SCREEN_H / 2 + 28, 1);
}

void lampe_verlassen() {
  lampe_stufe = 2;
}

void lampe_touch_behandeln() {
  lampe_stufe = (lampe_stufe + 1) % 3;
  lampe_starten();
}

// ----------------------------------------------------------------------------
//  INFO
// ----------------------------------------------------------------------------
void info_starten() {
  tft.fillRect(0, HEADER_HOEHE, SCREEN_W, SCREEN_H - HEADER_HOEHE, FARBE_HINTERGRUND);
  int y = HEADER_HOEHE + 12;
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(FARBE_TEXT, FARBE_HINTERGRUND);

  tft.drawString("ESP32 TFT Labor", 12, y, 2);
  y += 22;
  tft.drawString(String("Chip: ") + ESP.getChipModel(), 12, y, 1);
  y += 14;
  tft.drawString("Heap frei: " + String(ESP.getFreeHeap()) + " B", 12, y, 1);
  y += 14;
  unsigned long sek = millis() / 1000;
  char up[20];
  snprintf(up, sizeof(up), "Uptime: %lu min %lu s", sek / 60, sek % 60);
  tft.drawString(up, 12, y, 1);
  y += 18;

  tft.drawString("WLAN", 12, y, 2);
  y += 18;
  if (WiFi.status() == WL_CONNECTED) {
    tft.drawString(String("SSID  ") + WiFi.SSID(), 12, y, 1);
    y += 14;
    tft.drawString(String("IP    ") + WiFi.localIP().toString(), 12, y, 1);
    y += 14;
    tft.drawString(String("RSSI  ") + String(WiFi.RSSI()) + " dBm", 12, y, 1);
  } else {
    tft.drawString("nicht verbunden", 12, y, 1);
  }
  y += 18;
  tft.drawString("SD-Slot GPIO14", 12, y, 2);
  y += 16;
  tft.drawString(sd_stand.mount_ok ? "Karte erkannt" : "keine Karte", 12, y, 1);
}

void info_verlassen() {
}
