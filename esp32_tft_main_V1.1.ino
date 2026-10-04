// ============================================================================
//  ESP32 TFT HAUPTPROGRAMM - GRUNDGERUEST (VERKNUEPFT MIT ALLEN SEITEN)
//  Kopfleiste + Hauptmenü + Standby-Modus + Touch-Navigation
//  Bibliotheken: TFT_eSPI, XPT2046_Touchscreen, SPI, WiFi, FS, SD
//  Zeitquelle: NTP ueber WLAN
//
//  TOUCH-VERKABELUNG (XPT2046, wie in den Projektnotizen):
//  T_CLK -> GPIO18 (gemeinsam mit SCK des Displays)
//  T_CS  -> GPIO27
//  T_DIN -> GPIO23 (gemeinsam mit MOSI des Displays)
//  T_DO  -> GPIO19 (gemeinsam mit MISO des Displays)
//  Falls in der Bibliotheksverwaltung noch nicht vorhanden: Bibliothek
//  "XPT2046_Touchscreen" von Paul Stoffregen installieren.
//
//  SD-KARTEN-SLOT DES 2.8"-DISPLAYS (gemeinsamer SPI-Bus):
//  SD_CS   -> GPIO14 (D14)
//  SD_MOSI -> GPIO23 (gemeinsam mit Display MOSI / T_DIN)
//  SD_MISO -> GPIO19 (gemeinsam mit Display MISO / T_DO)
//  SD_SCK  -> GPIO18 (gemeinsam mit Display SCK  / T_CLK)
//  Testseite: Hauptmenue-Kachel "SD-Karte". Details in esp32-tft-sd.ino.
// ============================================================================

#include <TFT_eSPI.h>
#include <SPI.h>
#include <WiFi.h>
#include <time.h>
#include <XPT2046_Touchscreen.h>
#include "FS.h"
#include "SD.h"

TFT_eSPI tft = TFT_eSPI();

// ----------------------------------------------------------------------------
//  TOUCH-CONTROLLER EINRICHTEN
// ----------------------------------------------------------------------------
#define TOUCH_CS_PIN   27
#define SD_CS_PIN      14
XPT2046_Touchscreen touchscreen(TOUCH_CS_PIN);

// Rohe Touch-Rueckgabewerte des XPT2046 liegen typischerweise zwischen circa
// 200 und 3900. Diese Werte ggf. anhand des seriellen Monitors feinjustieren,
// falls die Touch-Position nicht genau zum Angezeigten passt.
#define TOUCH_ROH_X_MIN 200
#define TOUCH_ROH_X_MAX 3900
#define TOUCH_ROH_Y_MIN 200
#define TOUCH_ROH_Y_MAX 3900

// ============================================================================
//  EINSTELLUNGSBEREICH - HIER SPAETER EIGENE WERTE EINTRAGEN
// ============================================================================

const char* WLAN_SSID     = "Nest";
const char* WLAN_PASSWORT = "Spectaculum88";

const char* NTP_SERVER = "pool.ntp.org";
const char* ZEITZONE   = "CET-1CEST,M3.5.0,M10.5.0/3";

// ---------- Bildschirmgroesse (Querformat) ----------
#define SCREEN_W 320
#define SCREEN_H 240

// ---------- Touch Kalibrierung ----------
uint16_t calData[5] = { 275, 3620, 264, 3532, 1 };

#define HEADER_HOEHE 22

#define STANDBY_MINUTEN 10

// ============================================================================
//  FARBEN (RGB565 - ueber TFT_eSPI Makro erzeugt)
// ============================================================================

#define FARBE_HINTERGRUND   tft.color565(10, 26, 51)
#define FARBE_HEADER        tft.color565(45, 45, 50)
#define FARBE_TEXT          tft.color565(255, 255, 255)
#define FARBE_RAHMEN        tft.color565(170, 170, 170)
#define FARBE_RAHMEN_AKTIV  tft.color565(255, 210, 0)

#define FARBE_MENU_TEMPERATUREN tft.color565(46, 111, 64)
#define FARBE_MENU_WETTER       tft.color565(46, 95, 138)
#define FARBE_MENU_SPIELE       tft.color565(138, 90, 46)
#define FARBE_MENU_SCHALTER     tft.color565(120, 60, 130)
#define FARBE_MENU_SD           tft.color565(50, 100, 130)
#define FARBE_MENU_PROGRAMME    tft.color565(60, 130, 120)

// ============================================================================
//  SPIEL-KENNUNGEN
//  Zentral hier definiert, damit sie beim Zusammenbau durch die Arduino-IDE
//  (Reihenfolge: Hauptdatei zuerst, dann alphabetisch) bereits bekannt sind,
//  bevor esp32-tft-spiele.ino und die einzelnen Spiele-Dateien sie benutzen.
//  -1 = kein Spiel aktiv (wir sind auf der Uebersichtsseite)
// ============================================================================
#define SPIEL_KEINS        -1
#define SPIEL_SNAKE          0
#define SPIEL_PIPE_MANIA     1
#define SPIEL_TYCOON         2
#define SPIEL_SUDOKU         3
#define SPIEL_MEMORY         4
#define SPIEL_VIER_GEWINNT   5
#define SPIEL_AIR_HOCKEY     6

int spiel_aktiv = SPIEL_KEINS;

// ============================================================================
//  PROGRAMM-KENNUNGEN (Untermenue "Programme", view_index == 6)
// ============================================================================
#define PROG_KEINS      -1
#define PROG_PAINT       0
#define PROG_RECHNER     1
#define PROG_STOPPUHR    2
#define PROG_TIMER       3
#define PROG_DATEIEN     4
#define PROG_WUERFEL     5
#define PROG_LAMPE       6
#define PROG_INFO        7

int prog_aktiv = PROG_KEINS;

// ============================================================================
//  SD-TESTSTAND (von esp32-tft-sd.ino und esp32-tft-dateien.ino genutzt)
//  Hier im Hauptfile, weil die Arduino-IDE Dateien alphabetisch anhaengt
//  und dateien.ino sonst vor der Struktur-Definition kaeme.
// ============================================================================
#define SD_MAX_DATEIEN 6

struct SdDateiInfo {
  char name[24];
  uint32_t groesse;
  bool ist_ordner;
};

struct SdTestStand {
  bool mount_ok;
  bool write_ok;
  bool read_ok;
  uint8_t kartentyp;
  uint64_t groesse_bytes;
  uint64_t benutzt_bytes;
  uint32_t spi_hz;
  char meldung[96];
  int dateianzahl;
  SdDateiInfo dateien[SD_MAX_DATEIEN];
};

SdTestStand sd_stand;

// ============================================================================
//  GLOBALE ZUSTANDSVARIABLEN
// ============================================================================

int view_index = 0;

unsigned long letzte_touch_zeit = 0;
bool standby_aktiv = false;
int standby_unteransicht = 0;
unsigned long standby_letzter_wechsel = 0;
#define STANDBY_WECHSEL_INTERVALL 20000

int touch_x = 0;
int touch_y = 0;
bool touch_gerade_gedrueckt = false;
bool touch_wurde_losgelassen = true;

int blink_kachel_index = -1;
unsigned long blink_start_zeit = 0;
#define BLINK_DAUER_MS 200

// ============================================================================
//  STRUKTUR FUER EINE HAUPTMENUE-KACHEL
// ============================================================================
struct MenuKachel {
  int x, y, breite, hoehe;
  uint16_t farbe;
  const char* name;
  int ziel_view;
};

MenuKachel hauptmenu_kacheln[6];

// ============================================================================
//  SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);

  // CS-Leitungen inaktiv (HIGH), bevor irgendwer den SPI-Bus anfasst.
  // GPIO14 hat beim Boot einen Pulldown - ohne das wuerde der SD-Slot
  // waehrend tft.init() mitselektiert.
  pinMode(TOUCH_CS_PIN, OUTPUT);
  digitalWrite(TOUCH_CS_PIN, HIGH);
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(SD_CS_PIN, HIGH);
#ifdef TFT_CS
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
#endif

  tft.init();
  tft.setRotation(1);

  // Touch-Controller einrichten. Er teilt sich SCK/MOSI/MISO mit dem Display,
  // braucht aber ein eigenes CS-Pin (siehe TOUCH_CS_PIN oben).
  touchscreen.begin();
  touchscreen.setRotation(1);

  wlan_und_zeit_einrichten();
  wetterwerte_aktualisieren();
  wettervorschau_aktualisieren();

  hauptmenu_layout_aufbauen();
  temperaturseite_layout_aufbauen();
  spieleseite_layout_aufbauen();
  schalterseite_layout_aufbauen();
  sdseite_layout_aufbauen();
  programme_layout_aufbauen();
  sd_test_ausfuehren();

  letzte_touch_zeit = millis();
  standby_letzter_wechsel = millis();

  bildschirm_komplett_neu_zeichnen();
}

// ============================================================================
//  WLAN VERBINDEN UND NTP-ZEIT HOLEN
// ============================================================================
void wlan_und_zeit_einrichten() {
  WiFi.begin(WLAN_SSID, WLAN_PASSWORT);

  Serial.print("Verbinde mit WLAN");
  unsigned long start_versuch = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start_versuch < 15000) {
    delay(300);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWLAN verbunden, hole Uhrzeit per NTP...");
    configTzTime(ZEITZONE, NTP_SERVER);

    struct tm zeitinfo;
    int versuche = 0;
    while (!getLocalTime(&zeitinfo) && versuche < 20) {
      delay(300);
      versuche++;
    }
    Serial.println("Uhrzeit gesetzt.");
  } else {
    Serial.println("\nWLAN-Verbindung fehlgeschlagen, Uhrzeit bleibt bis zum naechsten Neustart ungesetzt.");
  }
}

// ============================================================================
//  HAUPTMENUE LAYOUT AUFBAUEN
// ============================================================================
void hauptmenu_layout_aufbauen() {
  int tile_w = 90;
  int tile_h = 90;
  int gap_x = 15;
  int gap_y = 15;
  int start_x = (SCREEN_W - (tile_w * 3 + gap_x * 2)) / 2;
  int start_y = HEADER_HOEHE + 15;

  hauptmenu_kacheln[0] = { start_x, start_y, tile_w, tile_h, FARBE_MENU_TEMPERATUREN, "Temperaturen", 1 };
  hauptmenu_kacheln[1] = { start_x + (tile_w + gap_x), start_y, tile_w, tile_h, FARBE_MENU_WETTER, "Wetter", 2 };
  hauptmenu_kacheln[2] = { start_x + (tile_w + gap_x) * 2, start_y, tile_w, tile_h, FARBE_MENU_SPIELE, "Spiele", 3 };

  int y2 = start_y + tile_h + gap_y;
  hauptmenu_kacheln[3] = { start_x, y2, tile_w, tile_h, FARBE_MENU_SCHALTER, "Schalter", 4 };
  hauptmenu_kacheln[4] = { start_x + (tile_w + gap_x), y2, tile_w, tile_h, FARBE_MENU_SD, "SD-Karte", 5 };
  hauptmenu_kacheln[5] = { start_x + (tile_w + gap_x) * 2, y2, tile_w, tile_h, FARBE_MENU_PROGRAMME, "Programme", 6 };
}

// ============================================================================
//  HAUPTSCHLEIFE
// ============================================================================
void loop() {
  touch_abfragen();

  unsigned long jetzt = millis();
  unsigned long standby_grenze_ms = (unsigned long)STANDBY_MINUTEN * 60UL * 1000UL;

  if (!standby_aktiv && (jetzt - letzte_touch_zeit > standby_grenze_ms)) {
    standby_aktiv = true;
    standby_unteransicht = 0;
    standby_letzter_wechsel = jetzt;
    bildschirm_komplett_neu_zeichnen();
  }

  if (standby_aktiv) {
    if (jetzt - standby_letzter_wechsel > STANDBY_WECHSEL_INTERVALL) {
      standby_unteransicht = (standby_unteransicht + 1) % 2;
      standby_letzter_wechsel = jetzt;
      bildschirm_komplett_neu_zeichnen();
    }
  }

  if (blink_kachel_index != -1 && (jetzt - blink_start_zeit > BLINK_DAUER_MS)) {
    blink_kachel_index = -1;
    bool auf_uebersicht = (view_index == 0) ||
      (view_index == 3 && spiel_aktiv == SPIEL_KEINS) ||
      (view_index == 6 && prog_aktiv == PROG_KEINS);
    if (auf_uebersicht) {
      bildschirm_komplett_neu_zeichnen();
    }
  }

  if (view_index == 1 && !standby_aktiv) {
    temperaturseite_periodisch_aktualisieren();
  }
  if (view_index == 2 && !standby_aktiv) {
    wetterseite_periodisch_aktualisieren();
  }

  header_uhrzeit_aktualisieren();
  standby_uhr_aktualisieren();

  if (!standby_aktiv && view_index == 3 && spiel_aktiv != SPIEL_KEINS) {
    if (spiel_aktiv == SPIEL_SNAKE) {
      snake_schritt_ausfuehren();
    } else if (spiel_aktiv == SPIEL_PIPE_MANIA) {
      rohr_bauphase_pruefen();
      rohr_fliessphase_ausfuehren();
    } else if (spiel_aktiv == SPIEL_TYCOON) {
      tycoon_verkauf_tick_ausfuehren();
    } else if (spiel_aktiv == SPIEL_MEMORY) {
      memory_tick_ausfuehren();
    } else if (spiel_aktiv == SPIEL_AIR_HOCKEY) {
      hockey_tick_ausfuehren();
    }
  }

  if (!standby_aktiv && view_index == 4) {
    schalterseite_blink_zuruecksetzen();
  }

  if (!standby_aktiv && view_index == 6 && prog_aktiv != PROG_KEINS) {
    if (prog_aktiv == PROG_STOPPUHR) {
      stoppuhr_tick_ausfuehren();
    } else if (prog_aktiv == PROG_TIMER) {
      timer_tick_ausfuehren();
    } else if (prog_aktiv == PROG_WUERFEL) {
      wuerfel_tick_ausfuehren();
    }
  }

  if (view_index == 6 && prog_aktiv == PROG_PAINT) {
    delay(12);
  } else {
    delay(30);
  }
}

// ============================================================================
//  TOUCH ABFRAGEN
//  Liest den XPT2046 aus, rechnet die rohen Werte auf Bildschirmkoordinaten
//  um (inklusive Drehung, passend zu tft.setRotation(1)) und leitet die
//  Beruehrung an die jeweils aktive Seite weiter.
// ============================================================================
void touch_abfragen() {
  if (!touchscreen.touched()) {
    if (view_index == 6 && prog_aktiv == PROG_PAINT) {
      paint_strich_ende();
    }
    touch_gerade_gedrueckt = false;
    touch_wurde_losgelassen = true;
    return;
  }

  // Sonderfall Air Hockey: der Schlaeger soll dem Finger folgen, solange er
  // gehalten wird - dafuer NICHT auf die uebliche Einzelklick-Entprellung
  // warten, sondern bei jeder Beruehrung sofort reagieren
  if (view_index == 3 && spiel_aktiv == SPIEL_AIR_HOCKEY) {
    TS_Point hockey_punkt = touchscreen.getPoint();
    touch_x = map(hockey_punkt.x, TOUCH_ROH_X_MIN, TOUCH_ROH_X_MAX, 0, SCREEN_W);
    touch_y = map(hockey_punkt.y, TOUCH_ROH_Y_MIN, TOUCH_ROH_Y_MAX, 0, SCREEN_H);
    touch_x = constrain(touch_x, 0, SCREEN_W - 1);
    touch_y = constrain(touch_y, 0, SCREEN_H - 1);
    letzte_touch_zeit = millis();

    if (touch_y >= 0 && touch_y <= HEADER_HOEHE &&
        touch_x >= SCREEN_W - 60 && touch_x <= SCREEN_W - 30) {
      if (!touch_wurde_losgelassen) return;
      touch_wurde_losgelassen = false;
      hockey_spiel_verlassen();
      spiel_aktiv = SPIEL_KEINS;
      view_index = 0;
      bildschirm_komplett_neu_zeichnen();
      return;
    }

    hockey_touch_halten();
    return;
  }

  // Paint: Strich folgen, solange der Finger gehalten wird
  if (view_index == 6 && prog_aktiv == PROG_PAINT) {
    TS_Point paint_punkt = touchscreen.getPoint();
    touch_x = map(paint_punkt.x, TOUCH_ROH_X_MIN, TOUCH_ROH_X_MAX, 0, SCREEN_W);
    touch_y = map(paint_punkt.y, TOUCH_ROH_Y_MIN, TOUCH_ROH_Y_MAX, 0, SCREEN_H);
    touch_x = constrain(touch_x, 0, SCREEN_W - 1);
    touch_y = constrain(touch_y, 0, SCREEN_H - 1);
    letzte_touch_zeit = millis();

    if (touch_y >= 0 && touch_y <= HEADER_HOEHE &&
        touch_x >= SCREEN_W - 60 && touch_x <= SCREEN_W - 30) {
      if (!touch_wurde_losgelassen) return;
      touch_wurde_losgelassen = false;
      paint_verlassen();
      prog_aktiv = PROG_KEINS;
      view_index = 0;
      bildschirm_komplett_neu_zeichnen();
      return;
    }

    paint_touch_halten();
    return;
  }

  // Erst reagieren, wenn der Finger zwischendurch losgelassen wurde
  // (verhindert, dass ein einzelner Druck als viele Klicks gezaehlt wird)
  if (!touch_wurde_losgelassen) {
    return;
  }
  touch_wurde_losgelassen = false;

  TS_Point punkt = touchscreen.getPoint();

  // Rohwerte auf Bildschirmaufloesung abbilden. Die Achsen sind beim
  // XPT2046 gegenueber dem Display oft vertauscht/gespiegelt - falls die
  // Touch-Position nicht zum Antippen passt, hier X/Y tauschen oder
  // map()-Richtung umdrehen.
  touch_x = map(punkt.x, TOUCH_ROH_X_MIN, TOUCH_ROH_X_MAX, 0, SCREEN_W);
  touch_y = map(punkt.y, TOUCH_ROH_Y_MIN, TOUCH_ROH_Y_MAX, 0, SCREEN_H);
  touch_x = constrain(touch_x, 0, SCREEN_W - 1);
  touch_y = constrain(touch_y, 0, SCREEN_H - 1);
  touch_gerade_gedrueckt = true;

  Serial.print("Touch erkannt: x=");
  Serial.print(touch_x);
  Serial.print(" y=");
  Serial.println(touch_y);

  letzte_touch_zeit = millis();

  if (standby_aktiv) {
    standby_aktiv = false;
    bildschirm_komplett_neu_zeichnen();
    touch_gerade_gedrueckt = false;
    return;
  }

  if (touch_y >= 0 && touch_y <= HEADER_HOEHE) {
    if (touch_x >= SCREEN_W - 60 && touch_x <= SCREEN_W - 30) {
      if (view_index == 3 && spiel_aktiv != SPIEL_KEINS) {
        if (spiel_aktiv == SPIEL_SNAKE) {
          snake_spiel_verlassen();
        } else if (spiel_aktiv == SPIEL_PIPE_MANIA) {
          rohr_spiel_verlassen();
        } else if (spiel_aktiv == SPIEL_TYCOON) {
          tycoon_spiel_verlassen();
        } else if (spiel_aktiv == SPIEL_SUDOKU) {
          sudoku_spiel_verlassen();
        } else if (spiel_aktiv == SPIEL_MEMORY) {
          memory_spiel_verlassen();
        } else if (spiel_aktiv == SPIEL_VIER_GEWINNT) {
          vier_spiel_verlassen();
        }
        // SPIEL_AIR_HOCKEY wird bereits oben im Sonderfall behandelt
        spiel_aktiv = SPIEL_KEINS;
      }
      if (view_index == 6 && prog_aktiv != PROG_KEINS) {
        programm_aktiv_verlassen();
      }
      view_index = 0;
      bildschirm_komplett_neu_zeichnen();
      touch_gerade_gedrueckt = false;
      return;
    } else if (touch_x >= SCREEN_W - 28 && touch_x <= SCREEN_W - 4) {
      touch_gerade_gedrueckt = false;
      return;
    }
  }

  if (view_index == 0) {
    for (int i = 0; i < 6; i++) {
      MenuKachel &k = hauptmenu_kacheln[i];
      if (touch_x >= k.x && touch_x <= k.x + k.breite &&
          touch_y >= k.y && touch_y <= k.y + k.hoehe) {
        blink_kachel_index = i;
        blink_start_zeit = millis();
        kachel_rahmen_zeichnen(k.x, k.y, k.breite, k.hoehe, FARBE_RAHMEN_AKTIV);
        if (k.ziel_view != -1) {
          view_index = k.ziel_view;
          bildschirm_komplett_neu_zeichnen();
        }
        touch_gerade_gedrueckt = false;
        return;
      }
    }
  } else if (view_index == 3) {
    if (spiel_aktiv == SPIEL_KEINS) {
      spieleseite_touch_behandeln();
      if (spiel_aktiv == SPIEL_SNAKE) {
        snake_spiel_starten();
      } else if (spiel_aktiv == SPIEL_PIPE_MANIA) {
        rohr_spiel_starten();
      } else if (spiel_aktiv == SPIEL_TYCOON) {
        tycoon_spiel_starten();
      } else if (spiel_aktiv == SPIEL_SUDOKU) {
        sudoku_spiel_starten();
      } else if (spiel_aktiv == SPIEL_MEMORY) {
        memory_spiel_starten();
      } else if (spiel_aktiv == SPIEL_VIER_GEWINNT) {
        vier_spiel_starten();
      } else if (spiel_aktiv == SPIEL_AIR_HOCKEY) {
        hockey_spiel_starten();
      }
    } else if (spiel_aktiv == SPIEL_SNAKE) {
      snake_touch_behandeln();
    } else if (spiel_aktiv == SPIEL_PIPE_MANIA) {
      rohr_touch_behandeln();
    } else if (spiel_aktiv == SPIEL_TYCOON) {
      tycoon_touch_behandeln();
    } else if (spiel_aktiv == SPIEL_SUDOKU) {
      sudoku_touch_behandeln();
    } else if (spiel_aktiv == SPIEL_MEMORY) {
      memory_touch_behandeln();
    } else if (spiel_aktiv == SPIEL_VIER_GEWINNT) {
      vier_touch_behandeln();
    } else if (spiel_aktiv == SPIEL_AIR_HOCKEY) {
      hockey_touch_behandeln();
    }
  } else if (view_index == 4) {
    schalterseite_touch_behandeln();
  } else if (view_index == 5) {
    sdseite_touch_behandeln();
  } else if (view_index == 6) {
    if (prog_aktiv == PROG_KEINS) {
      programme_touch_behandeln();
      if (prog_aktiv == PROG_PAINT) {
        paint_starten();
      } else if (prog_aktiv == PROG_RECHNER) {
        rechner_starten();
      } else if (prog_aktiv == PROG_STOPPUHR) {
        stoppuhr_starten();
      } else if (prog_aktiv == PROG_TIMER) {
        timer_starten();
      } else if (prog_aktiv == PROG_DATEIEN) {
        dateien_starten();
      } else if (prog_aktiv == PROG_WUERFEL) {
        wuerfel_starten();
      } else if (prog_aktiv == PROG_LAMPE) {
        lampe_starten();
      } else if (prog_aktiv == PROG_INFO) {
        info_starten();
      }
    } else if (prog_aktiv == PROG_RECHNER) {
      rechner_touch_behandeln();
    } else if (prog_aktiv == PROG_STOPPUHR) {
      stoppuhr_touch_behandeln();
    } else if (prog_aktiv == PROG_TIMER) {
      timer_touch_behandeln();
    } else if (prog_aktiv == PROG_DATEIEN) {
      dateien_touch_behandeln();
    } else if (prog_aktiv == PROG_WUERFEL) {
      wuerfel_touch_behandeln();
    } else if (prog_aktiv == PROG_LAMPE) {
      lampe_touch_behandeln();
    }
  }

  touch_gerade_gedrueckt = false;
}

// ============================================================================
//  BILDSCHIRM KOMPLETT NEU ZEICHNEN
// ============================================================================
void bildschirm_komplett_neu_zeichnen() {
  tft.fillScreen(FARBE_HINTERGRUND);

  if (standby_aktiv) {
    if (standby_unteransicht == 0) {
      standby_grosse_uhr_zeichnen();
    } else {
      standby_wetter_zeichnen();
    }
    return;
  }

  header_zeichnen();

  if (view_index == 0) {
    hauptmenu_zeichnen();
  } else if (view_index == 1) {
    temperaturseite_zeichnen();
  } else if (view_index == 2) {
    wetterseite_zeichnen();
  } else if (view_index == 3) {
    if (spiel_aktiv == SPIEL_KEINS) {
      spieleseite_zeichnen();
    } else if (spiel_aktiv == SPIEL_SNAKE) {
      snake_spiel_starten();
    } else if (spiel_aktiv == SPIEL_PIPE_MANIA) {
      rohr_spiel_starten();
    } else if (spiel_aktiv == SPIEL_TYCOON) {
      tycoon_spiel_starten();
    } else if (spiel_aktiv == SPIEL_SUDOKU) {
      sudoku_spiel_starten();
    } else if (spiel_aktiv == SPIEL_MEMORY) {
      memory_spiel_starten();
    } else if (spiel_aktiv == SPIEL_VIER_GEWINNT) {
      vier_spiel_starten();
    } else if (spiel_aktiv == SPIEL_AIR_HOCKEY) {
      hockey_spiel_starten();
    }
  } else if (view_index == 4) {
    schalterseite_zeichnen();
  } else if (view_index == 5) {
    sdseite_zeichnen();
  } else if (view_index == 6) {
    if (prog_aktiv == PROG_KEINS) {
      programme_zeichnen();
    } else if (prog_aktiv == PROG_PAINT) {
      paint_starten();
    } else if (prog_aktiv == PROG_RECHNER) {
      rechner_starten();
    } else if (prog_aktiv == PROG_STOPPUHR) {
      stoppuhr_starten();
    } else if (prog_aktiv == PROG_TIMER) {
      timer_starten();
    } else if (prog_aktiv == PROG_DATEIEN) {
      dateien_starten();
    } else if (prog_aktiv == PROG_WUERFEL) {
      wuerfel_starten();
    } else if (prog_aktiv == PROG_LAMPE) {
      lampe_starten();
    } else if (prog_aktiv == PROG_INFO) {
      info_starten();
    }
  }
}

// ============================================================================
//  KOPFLEISTE ZEICHNEN (Uhrzeit + Home-Button + Platzhalter-Button)
// ============================================================================
void header_zeichnen() {
  tft.fillRect(0, 0, SCREEN_W, HEADER_HOEHE, FARBE_HEADER);

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(FARBE_TEXT, FARBE_HEADER);
  tft.drawString(aktuelle_uhrzeit_als_text(), 6, HEADER_HOEHE / 2, 1);

  int hb_x = SCREEN_W - 60, hb_y = 2, hb_w = 30, hb_h = HEADER_HOEHE - 4;
  tft.fillRoundRect(hb_x, hb_y, hb_w, hb_h, 3, tft.color565(70, 70, 78));
  tft.setTextDatum(MC_DATUM);
  tft.drawString("H", hb_x + hb_w / 2, hb_y + hb_h / 2, 1);

  int pb_x = SCREEN_W - 28, pb_y = 3, pb_w = 24, pb_h = HEADER_HOEHE - 6;
  tft.fillRoundRect(pb_x, pb_y, pb_w, pb_h, 3, tft.color565(70, 70, 78));
  tft.drawString("-", pb_x + pb_w / 2, pb_y + pb_h / 2, 1);
}

unsigned long letzte_uhrzeit_aktualisierung = 0;
void header_uhrzeit_aktualisieren() {
  if (standby_aktiv) return;
  unsigned long jetzt = millis();
  if (jetzt - letzte_uhrzeit_aktualisierung < 1000) return;
  letzte_uhrzeit_aktualisierung = jetzt;

  tft.fillRect(0, 0, 70, HEADER_HOEHE, FARBE_HEADER);
  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(FARBE_TEXT, FARBE_HEADER);
  tft.drawString(aktuelle_uhrzeit_als_text(), 6, HEADER_HOEHE / 2, 1);
}

unsigned long letzte_standby_uhr_aktualisierung = 0;
void standby_uhr_aktualisieren() {
  if (!standby_aktiv || standby_unteransicht != 0) return;
  unsigned long jetzt = millis();
  if (jetzt - letzte_standby_uhr_aktualisierung < 1000) return;
  letzte_standby_uhr_aktualisierung = jetzt;
  standby_grosse_uhr_zeichnen();
}

// ============================================================================
//  ZEIT-HILFSFUNKTIONEN (NTP, ueber die interne ESP32-Systemzeit)
// ============================================================================
String aktuelle_uhrzeit_als_text() {
  struct tm zeitinfo;
  if (!getLocalTime(&zeitinfo, 0)) {
    return "--:--";
  }
  char puffer[6];
  strftime(puffer, sizeof(puffer), "%H:%M", &zeitinfo);
  return String(puffer);
}

String aktuelles_datum_als_text() {
  struct tm zeitinfo;
  if (!getLocalTime(&zeitinfo, 0)) {
    return "--.--.----";
  }
  char puffer[11];
  strftime(puffer, sizeof(puffer), "%d.%m.%Y", &zeitinfo);
  return String(puffer);
}

// ============================================================================
//  HAUPTMENUE ZEICHNEN
// ============================================================================
void hauptmenu_zeichnen() {
  for (int i = 0; i < 6; i++) {
    MenuKachel &k = hauptmenu_kacheln[i];
    tft.fillRoundRect(k.x, k.y, k.breite, k.hoehe, 8, k.farbe);

    uint16_t rahmenfarbe = (blink_kachel_index == i) ? FARBE_RAHMEN_AKTIV : FARBE_RAHMEN;
    tft.drawRoundRect(k.x, k.y, k.breite, k.hoehe, 8, rahmenfarbe);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(FARBE_TEXT, k.farbe);
    tft.drawString(k.name, k.x + k.breite / 2, k.y + k.hoehe / 2, 2);
  }
}

void kachel_rahmen_zeichnen(int x, int y, int w, int h, uint16_t farbe) {
  tft.drawRoundRect(x, y, w, h, 8, farbe);
}

// ============================================================================
//  STANDBY-ANSICHTEN
// ============================================================================
void standby_grosse_uhr_zeichnen() {
  tft.fillRect(0, SCREEN_H / 2 - 60, SCREEN_W, 120, FARBE_HINTERGRUND);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TEXT, FARBE_HINTERGRUND);
  tft.drawString(aktuelle_uhrzeit_als_text(), SCREEN_W / 2, SCREEN_H / 2 - 20, 7);
  tft.drawString(aktuelles_datum_als_text(), SCREEN_W / 2, SCREEN_H / 2 + 40, 4);
}

void standby_wetter_zeichnen() {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TEXT, FARBE_HINTERGRUND);
  tft.drawString("Wetter (Standby) - folgt", SCREEN_W / 2, SCREEN_H / 2, 2);
}