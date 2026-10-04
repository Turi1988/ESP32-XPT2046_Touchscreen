// ============================================================================
//  SD-KARTEN-TESTSEITE (view_index == 5)
//  Eigene Datei: esp32-tft-sd.ino
//
//  Der Micro-SD-Slot des 2.8"-ILI9341-Displays haengt am gemeinsamen
//  VSPI-Bus von Display und Touch. Nur der Chip-Select ist extra verdrahtet:
//
//    SD_CS   -> GPIO14 (D14)
//    SD_MOSI -> GPIO23  (gemeinsam mit Display MOSI / T_DIN)
//    SD_MISO -> GPIO19  (gemeinsam mit Display MISO / T_DO)
//    SD_SCK  -> GPIO18  (gemeinsam mit Display SCK  / T_CLK)
//
//  Wichtig: Am Modul gibt es oft eigene Pins SD_MOSI / SD_MISO / SD_SCK.
//  Die muessen mit den drei Display-SPI-Leitungen verbunden sein, sonst
//  bleibt der Slot stumm. SD_MISO darf nicht offen bleiben.
//
//  Karte: FAT32 (oder FAT16), 3.3 V, nicht schreibgeschuetzt.
//  Bibliotheken: FS.h und SD.h (beide im ESP32-Arduino-Core, keine Extra-Lib).
//
//  Der Test macht vier Dinge:
//    1) Mount (zuerst 10 MHz, bei Misserfolg 4 MHz)
//    2) Kartentyp + Groesse lesen
//    3) /esp32_test.txt schreiben und wieder lesen
//    4) Wurzelverzeichnis auflisten
//
//  Ergebnis steht auf dem Display und im seriellen Monitor (115200 Baud).
// ============================================================================

int sd_btn_x = 0;
int sd_btn_y = 0;
int sd_btn_w = 0;
int sd_btn_h = 0;

#define FARBE_SD_OK     tft.color565(30, 140, 90)
#define FARBE_SD_FEHLER tft.color565(160, 50, 50)
#define FARBE_SD_INFO   tft.color565(28, 48, 78)
#define FARBE_SD_BTN    tft.color565(50, 90, 120)

static const char* SD_TESTDATEI = "/esp32_test.txt";
static const char* SD_TESTINHALT = "ESP32 TFT SD-Test OK";

// ----------------------------------------------------------------------------
//  SPI-BUS: Display, Touch und SD teilen MOSI/MISO/SCK. Unbenutzte CS-Leitungen
//  muessen HIGH sein, sonst stoeren sie den Transfer.
// ----------------------------------------------------------------------------
void sd_bus_sichern() {
  digitalWrite(TOUCH_CS_PIN, HIGH);
  digitalWrite(SD_CS_PIN, HIGH);
#ifdef TFT_CS
  digitalWrite(TFT_CS, HIGH);
#endif
}

void sd_bus_freigeben() {
  digitalWrite(SD_CS_PIN, HIGH);
}

const char* sd_typ_name(uint8_t typ) {
  switch (typ) {
    case CARD_MMC:  return "MMC";
    case CARD_SD:   return "SDSC";
    case CARD_SDHC: return "SDHC";
    default:        return "unbekannt";
  }
}

String sd_groesse_text(uint64_t bytes) {
  if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
    float gb = bytes / (1024.0f * 1024.0f * 1024.0f);
    return String(gb, 2) + " GB";
  }
  float mb = bytes / (1024.0f * 1024.0f);
  return String(mb, 1) + " MB";
}

bool sd_mount_mit_frequenz(uint32_t hz) {
  sd_bus_sichern();
  Serial.print("SD: Mount bei ");
  Serial.print(hz / 1000000);
  Serial.println(" MHz ...");
  bool ok = SD.begin(SD_CS_PIN, tft.getSPIinstance(), hz);
  if (!ok) {
    SD.end();
    delay(30);
  }
  return ok;
}

void sd_dateien_auflisten() {
  sd_stand.dateianzahl = 0;
  File wurzel = SD.open("/");
  if (!wurzel) {
    return;
  }

  File eintrag = wurzel.openNextFile();
  while (eintrag) {
    const char* rohname = eintrag.name();
    if (sd_stand.dateianzahl < SD_MAX_DATEIEN && rohname != nullptr && rohname[0] != '.') {
      SdDateiInfo &info = sd_stand.dateien[sd_stand.dateianzahl];
      strncpy(info.name, rohname, sizeof(info.name) - 1);
      info.name[sizeof(info.name) - 1] = '\0';
      info.groesse = eintrag.size();
      info.ist_ordner = eintrag.isDirectory();
      sd_stand.dateianzahl++;
    }
    eintrag.close();
    eintrag = wurzel.openNextFile();
  }
  wurzel.close();
}

void sd_test_ausfuehren() {
  memset(&sd_stand, 0, sizeof(sd_stand));
  strncpy(sd_stand.meldung, "kein Mount", sizeof(sd_stand.meldung) - 1);

  Serial.println("----------------------------------------");
  Serial.println("SD: Test startet, CS = GPIO14");

  sd_stand.mount_ok = sd_mount_mit_frequenz(10000000);
  if (sd_stand.mount_ok) {
    sd_stand.spi_hz = 10000000;
  } else {
    Serial.println("SD: 10 MHz fehlgeschlagen, versuche 4 MHz ...");
    sd_stand.mount_ok = sd_mount_mit_frequenz(4000000);
    if (sd_stand.mount_ok) {
      sd_stand.spi_hz = 4000000;
    }
  }

  if (!sd_stand.mount_ok) {
    strncpy(sd_stand.meldung,
            "Mount fehlgeschlagen. Karte, FAT32, SD_MISO?",
            sizeof(sd_stand.meldung) - 1);
    Serial.println("SD: Mount fehlgeschlagen.");
    Serial.println("SD: Pruefen: Karte eingelegt, FAT32, SD_CS=D14,");
    Serial.println("    SD_MOSI=GPIO23, SD_MISO=GPIO19, SD_SCK=GPIO18.");
    sd_bus_freigeben();
    return;
  }

  sd_stand.kartentyp = SD.cardType();
  if (sd_stand.kartentyp == CARD_NONE) {
    strncpy(sd_stand.meldung, "Kein Medium erkannt", sizeof(sd_stand.meldung) - 1);
    Serial.println("SD: CARD_NONE nach erfolgreichem Mount.");
    SD.end();
    sd_stand.mount_ok = false;
    sd_bus_freigeben();
    return;
  }

  sd_stand.groesse_bytes = SD.cardSize();
  sd_stand.benutzt_bytes = SD.usedBytes();

  Serial.print("SD: Typ=");
  Serial.print(sd_typ_name(sd_stand.kartentyp));
  Serial.print(" Groesse=");
  Serial.print(sd_groesse_text(sd_stand.groesse_bytes));
  Serial.print(" SPI=");
  Serial.print(sd_stand.spi_hz / 1000000);
  Serial.println(" MHz");

  Serial.print("SD: schreibe ");
  Serial.println(SD_TESTDATEI);
  File datei = SD.open(SD_TESTDATEI, FILE_WRITE);
  if (datei) {
    datei.println(SD_TESTINHALT);
    datei.close();
    sd_stand.write_ok = true;
    Serial.println("SD: Schreiben OK");
  } else {
    strncpy(sd_stand.meldung, "Schreiben fehlgeschlagen", sizeof(sd_stand.meldung) - 1);
    Serial.println("SD: Schreiben fehlgeschlagen (Schreibschutz?)");
  }

  if (sd_stand.write_ok) {
    Serial.print("SD: lese ");
    Serial.println(SD_TESTDATEI);
    File lesen = SD.open(SD_TESTDATEI, FILE_READ);
    if (lesen) {
      String inhalt = lesen.readString();
      lesen.close();
      inhalt.trim();
      sd_stand.read_ok = inhalt.indexOf(SD_TESTINHALT) >= 0;
      Serial.print("SD: gelesen: ");
      Serial.println(inhalt);
      if (!sd_stand.read_ok) {
        strncpy(sd_stand.meldung, "Inhalt stimmt nicht", sizeof(sd_stand.meldung) - 1);
      }
    } else {
      strncpy(sd_stand.meldung, "Lesen fehlgeschlagen", sizeof(sd_stand.meldung) - 1);
      Serial.println("SD: Datei nicht lesbar");
    }
  }

  sd_dateien_auflisten();
  Serial.print("SD: ");
  Serial.print(sd_stand.dateianzahl);
  Serial.println(" Datei(en) im Wurzelverzeichnis");

  if (sd_stand.mount_ok && sd_stand.write_ok && sd_stand.read_ok) {
    strncpy(sd_stand.meldung, "Slot funktioniert", sizeof(sd_stand.meldung) - 1);
    Serial.println("SD: Test bestanden.");
  } else if (sd_stand.mount_ok) {
    if (sd_stand.meldung[0] == '\0' || strcmp(sd_stand.meldung, "kein Mount") == 0) {
      strncpy(sd_stand.meldung, "Mount OK, Dateitest fehlerhaft", sizeof(sd_stand.meldung) - 1);
    }
    Serial.println("SD: Mount OK, aber Schreib-/Lesetest nicht bestanden.");
  }

  sd_bus_freigeben();
}

// ----------------------------------------------------------------------------
void sdseite_layout_aufbauen() {
  sd_btn_w = 150;
  sd_btn_h = 28;
  sd_btn_x = (SCREEN_W - sd_btn_w) / 2;
  sd_btn_y = SCREEN_H - sd_btn_h - 8;
}

void sd_zeile(int y, const char* links, const String& rechts, uint16_t farbe) {
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(farbe, FARBE_HINTERGRUND);
  tft.drawString(links, 12, y, 1);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(rechts, SCREEN_W - 12, y, 1);
}

void sdseite_zeichnen() {
  bool alles_ok = sd_stand.mount_ok && sd_stand.write_ok && sd_stand.read_ok;
  uint16_t statusfarbe = alles_ok ? FARBE_SD_OK : (sd_stand.mount_ok ? FARBE_SD_BTN : FARBE_SD_FEHLER);
  const char* status_text = alles_ok ? "Karte erkannt" : (sd_stand.mount_ok ? "Teilweise OK" : "Keine Karte");

  tft.fillRoundRect(8, HEADER_HOEHE + 6, SCREEN_W - 16, 26, 4, statusfarbe);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TEXT, statusfarbe);
  tft.drawString(status_text, SCREEN_W / 2, HEADER_HOEHE + 19, 2);

  int y = HEADER_HOEHE + 44;
  if (!sd_stand.mount_ok) {
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(FARBE_TEXT, FARBE_HINTERGRUND);
    tft.drawString("SD-CS = GPIO14 (D14)", 12, y, 1);
    tft.drawString("MOSI23  MISO19  SCK18", 12, y + 14, 1);
    tft.drawString(sd_stand.meldung, 12, y + 32, 1);
    tft.drawString("FAT32-Karte eingelegt?", 12, y + 46, 1);
  } else {
    sd_zeile(y, "Typ", sd_typ_name(sd_stand.kartentyp), FARBE_TEXT);
    sd_zeile(y + 14, "Groesse", sd_groesse_text(sd_stand.groesse_bytes), FARBE_TEXT);
    sd_zeile(y + 28, "Belegt", sd_groesse_text(sd_stand.benutzt_bytes), FARBE_TEXT);
    sd_zeile(y + 42, "SPI", String(sd_stand.spi_hz / 1000000) + " MHz", FARBE_TEXT);

    uint16_t schreibfarbe = sd_stand.write_ok ? FARBE_SD_OK : FARBE_SD_FEHLER;
    uint16_t lesefarbe = sd_stand.read_ok ? FARBE_SD_OK : FARBE_SD_FEHLER;
    sd_zeile(y + 58, "Schreibtest", sd_stand.write_ok ? "OK" : "FEHLER", schreibfarbe);
    sd_zeile(y + 72, "Lesetest", sd_stand.read_ok ? "OK" : "FEHLER", lesefarbe);

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(FARBE_TEXT, FARBE_HINTERGRUND);
    tft.drawString("Dateien:", 12, y + 90, 1);

    int liste_y = y + 104;
    if (sd_stand.dateianzahl == 0) {
      tft.setTextColor(tft.color565(160, 170, 180), FARBE_HINTERGRUND);
      tft.drawString("(leer)", 12, liste_y, 1);
    } else {
      for (int i = 0; i < sd_stand.dateianzahl; i++) {
        SdDateiInfo &d = sd_stand.dateien[i];
        String links = String(d.ist_ordner ? "/" : "") + d.name;
        String rechts = d.ist_ordner ? "DIR" : (String(d.groesse) + " B");
        sd_zeile(liste_y + i * 12, links.c_str(), rechts, FARBE_TEXT);
      }
    }
  }

  tft.fillRoundRect(sd_btn_x, sd_btn_y, sd_btn_w, sd_btn_h, 4, FARBE_SD_BTN);
  tft.drawRoundRect(sd_btn_x, sd_btn_y, sd_btn_w, sd_btn_h, 4, FARBE_RAHMEN);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TEXT, FARBE_SD_BTN);
  tft.drawString("Erneut testen", sd_btn_x + sd_btn_w / 2, sd_btn_y + sd_btn_h / 2, 1);
}

void sdseite_touch_behandeln() {
  if (touch_x >= sd_btn_x && touch_x <= sd_btn_x + sd_btn_w &&
      touch_y >= sd_btn_y && touch_y <= sd_btn_y + sd_btn_h) {
    tft.fillRoundRect(sd_btn_x, sd_btn_y, sd_btn_w, sd_btn_h, 4, FARBE_RAHMEN_AKTIV);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(FARBE_HINTERGRUND, FARBE_RAHMEN_AKTIV);
    tft.drawString("teste ...", sd_btn_x + sd_btn_w / 2, sd_btn_y + sd_btn_h / 2, 1);

    SD.end();
    delay(40);
    sd_test_ausfuehren();
    bildschirm_komplett_neu_zeichnen();
  }
}
