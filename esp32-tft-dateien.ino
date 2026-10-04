// ============================================================================
//  PROGRAMM: DATEIEN (prog_aktiv == PROG_DATEIEN)
//  Wurzelverzeichnis der SD-Karte auflisten. Nutzt denselben Slot wie der
//  SD-Test (CS = GPIO14).
// ============================================================================

#define DATEIEN_MAX 8

struct DateiEintrag {
  char name[24];
  uint32_t groesse;
  bool ist_ordner;
};

DateiEintrag dateien_liste[DATEIEN_MAX];
int dateien_anzahl = 0;
int dateien_auswahl = -1;
int dateien_btn_x, dateien_btn_y, dateien_btn_w, dateien_btn_h;

void dateien_liste_laden() {
  dateien_anzahl = 0;
  dateien_auswahl = -1;
  if (!sd_stand.mount_ok) {
    sd_test_ausfuehren();
  }
  if (!sd_stand.mount_ok) {
    return;
  }
  sd_bus_sichern();
  File wurzel = SD.open("/");
  if (!wurzel) {
    sd_bus_freigeben();
    return;
  }
  File eintrag = wurzel.openNextFile();
  while (eintrag) {
    const char* rohname = eintrag.name();
    if (dateien_anzahl < DATEIEN_MAX && rohname != nullptr && rohname[0] != '.') {
      DateiEintrag &d = dateien_liste[dateien_anzahl];
      strncpy(d.name, rohname, sizeof(d.name) - 1);
      d.name[sizeof(d.name) - 1] = '\0';
      d.groesse = eintrag.size();
      d.ist_ordner = eintrag.isDirectory();
      dateien_anzahl++;
    }
    eintrag.close();
    eintrag = wurzel.openNextFile();
  }
  wurzel.close();
  sd_bus_freigeben();
}

void dateien_zeichnen() {
  tft.fillRect(0, HEADER_HOEHE, SCREEN_W, SCREEN_H - HEADER_HOEHE, FARBE_HINTERGRUND);
  int y = HEADER_HOEHE + 8;
  tft.setTextDatum(ML_DATUM);
  if (!sd_stand.mount_ok) {
    tft.setTextColor(FARBE_TEXT, FARBE_HINTERGRUND);
    tft.drawString("Keine SD-Karte", 12, y, 2);
    tft.drawString("SD-CS = GPIO14", 12, y + 22, 1);
    tft.drawString("Karte einlegen, dann Aktualisieren", 12, y + 38, 1);
  } else if (dateien_anzahl == 0) {
    tft.setTextColor(FARBE_TEXT, FARBE_HINTERGRUND);
    tft.drawString("Karte leer", 12, y, 2);
  } else {
    tft.setTextColor(FARBE_TEXT, FARBE_HINTERGRUND);
    tft.drawString("Wurzelverzeichnis", 12, y, 1);
    int zeile = y + 16;
    for (int i = 0; i < dateien_anzahl; i++) {
      uint16_t bg = (i == dateien_auswahl) ? tft.color565(50, 90, 120) : FARBE_HINTERGRUND;
      tft.fillRect(8, zeile - 6, SCREEN_W - 16, 16, bg);
      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(FARBE_TEXT, bg);
      String name = String(dateien_liste[i].ist_ordner ? "/" : "") + dateien_liste[i].name;
      tft.drawString(name, 12, zeile, 1);
      tft.setTextDatum(MR_DATUM);
      String groesse = dateien_liste[i].ist_ordner ? "DIR" : (String(dateien_liste[i].groesse) + " B");
      tft.drawString(groesse, SCREEN_W - 12, zeile, 1);
      zeile += 16;
    }
    if (dateien_auswahl >= 0 && dateien_auswahl < dateien_anzahl) {
      tft.setTextDatum(ML_DATUM);
      tft.setTextColor(tft.color565(180, 200, 210), FARBE_HINTERGRUND);
      tft.drawString("angewaehlt", 12, SCREEN_H - 42, 1);
    }
  }

  dateien_btn_w = 150;
  dateien_btn_h = 26;
  dateien_btn_x = (SCREEN_W - dateien_btn_w) / 2;
  dateien_btn_y = SCREEN_H - dateien_btn_h - 6;
  tft.fillRoundRect(dateien_btn_x, dateien_btn_y, dateien_btn_w, dateien_btn_h, 4,
                    tft.color565(50, 90, 120));
  tft.drawRoundRect(dateien_btn_x, dateien_btn_y, dateien_btn_w, dateien_btn_h, 4, FARBE_RAHMEN);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TEXT, tft.color565(50, 90, 120));
  tft.drawString("Aktualisieren", dateien_btn_x + dateien_btn_w / 2,
                 dateien_btn_y + dateien_btn_h / 2, 1);
}

void dateien_starten() {
  dateien_liste_laden();
  dateien_zeichnen();
}

void dateien_verlassen() {
  dateien_auswahl = -1;
}

void dateien_touch_behandeln() {
  if (touch_x >= dateien_btn_x && touch_x <= dateien_btn_x + dateien_btn_w &&
      touch_y >= dateien_btn_y && touch_y <= dateien_btn_y + dateien_btn_h) {
    SD.end();
    delay(30);
    sd_stand.mount_ok = false;
    dateien_liste_laden();
    bildschirm_komplett_neu_zeichnen();
    return;
  }
  if (!sd_stand.mount_ok) return;
  int zeile0 = HEADER_HOEHE + 24;
  for (int i = 0; i < dateien_anzahl; i++) {
    int y = zeile0 + i * 16;
    if (touch_y >= y - 6 && touch_y <= y + 10) {
      dateien_auswahl = i;
      dateien_zeichnen();
      return;
    }
  }
}
