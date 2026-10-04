// ============================================================================
//  SPIEL: SNAKE (spiel_aktiv == SPIEL_SNAKE)
//  Eigene Datei: esp32-tft-snake.ino
//
//  Regeln:
//  - Wand rundherum: Kollision mit der Wand = Game Over
//  - Kollision mit dem eigenen Koerper = Game Over
//  - Essen bleibt immer gleich gross, nur EIN Essen gleichzeitig auf dem Feld
//  - Schlange waechst pro gefressenem Essen um genau ein Segment
//  - Punktestand oben zaehlt bei jedem gefressenen Essen um 1 hoch
//  - Der erreichte Punktestand wird am Spielende zum Highscore, falls er
//    den bisherigen Highscore uebertrifft (Highscore aktuell nur im
//    Arbeitsspeicher, kein Neustart-sicherer Speicher - SD-Karte folgt spaeter)
//  - Steuerung: vier Touch-Zonen auf dem Bildschirm (links/rechts/oben/unten
//    antippen zum Abbiegen in diese Richtung)
// ============================================================================

// ----------------------------------------------------------------------------
//  SPIELFELD-EINSTELLUNGEN
// ----------------------------------------------------------------------------
#define SNAKE_FELD_X       10                 // linke obere Ecke Spielfeld
#define SNAKE_FELD_Y       (HEADER_HOEHE + 10)
#define SNAKE_ZELLE         10                 // Groesse einer Feldzelle in Pixeln
#define SNAKE_SPALTEN       30                 // Spielfeldbreite in Zellen
#define SNAKE_ZEILEN         18                 // Spielfeldhoehe in Zellen
#define SNAKE_MAX_LAENGE   (SNAKE_SPALTEN * SNAKE_ZEILEN)

#define SNAKE_SCHRITT_MS    150                // Spielgeschwindigkeit (ms je Schritt)

// Touch-Zonen fuer Steuerung: Bildschirm wird in vier Dreiecke/Bereiche um die
// Spielfeldmitte aufgeteilt. Vereinfachte Variante: vier Rechteck-Zonen an
// den Raendern des Spielfelds.
#define SNAKE_TOUCHZONE_BREITE 60

// ----------------------------------------------------------------------------
//  FARBEN
// ----------------------------------------------------------------------------
#define FARBE_SNAKE_HINTERGRUND  tft.color565(15, 25, 45)
#define FARBE_SNAKE_WAND          tft.color565(120, 120, 130)
#define FARBE_SNAKE_KOERPER       tft.color565(40, 180, 80)
#define FARBE_SNAKE_KOPF          tft.color565(80, 230, 120)
#define FARBE_SNAKE_ESSEN         tft.color565(220, 50, 50)
#define FARBE_SNAKE_TEXT          tft.color565(255, 255, 255)

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
  int spalte;
  int zeile;
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

unsigned long snake_letzter_schritt = 0;

// ----------------------------------------------------------------------------
//  SPIEL STARTEN / ZURUECKSETZEN
//  Aufrufen, wenn die Snake-Kachel auf der Spieleseite angetippt wird
// ----------------------------------------------------------------------------
void snake_spiel_starten() {
  snake_laenge = 3;
  int start_spalte = SNAKE_SPALTEN / 2;
  int start_zeile = SNAKE_ZEILEN / 2;

  for (int i = 0; i < snake_laenge; i++) {
    snake_koerper[i].spalte = start_spalte - i;
    snake_koerper[i].zeile = start_zeile;
  }

  snake_richtung = SNAKE_RICHTUNG_RECHTS;
  snake_naechste_richtung = SNAKE_RICHTUNG_RECHTS;
  snake_punkte = 0;
  snake_spiel_vorbei = false;
  snake_spiel_laeuft = true;
  snake_letzter_schritt = millis();

  snake_essen_neu_platzieren();

  tft.fillScreen(FARBE_SNAKE_HINTERGRUND);
  snake_rahmen_zeichnen();
  snake_kopfzeile_zeichnen();
}

// ----------------------------------------------------------------------------
//  NEUES ESSEN AN ZUFAELLIGER, FREIER STELLE PLATZIEREN
// ----------------------------------------------------------------------------
void snake_essen_neu_platzieren() {
  bool frei;
  int versuch_spalte, versuch_zeile;

  do {
    frei = true;
    versuch_spalte = random(0, SNAKE_SPALTEN);
    versuch_zeile = random(0, SNAKE_ZEILEN);

    for (int i = 0; i < snake_laenge; i++) {
      if (snake_koerper[i].spalte == versuch_spalte &&
          snake_koerper[i].zeile == versuch_zeile) {
        frei = false;
        break;
      }
    }
  } while (!frei);

  snake_essen_spalte = versuch_spalte;
  snake_essen_zeile = versuch_zeile;
}

// ----------------------------------------------------------------------------
//  RAHMEN UND KOPFZEILE (Punktestand / Highscore) ZEICHNEN
// ----------------------------------------------------------------------------
void snake_rahmen_zeichnen() {
  int feld_breite_px = SNAKE_SPALTEN * SNAKE_ZELLE;
  int feld_hoehe_px = SNAKE_ZEILEN * SNAKE_ZELLE;
  tft.drawRect(SNAKE_FELD_X - 2, SNAKE_FELD_Y - 2,
               feld_breite_px + 4, feld_hoehe_px + 4, FARBE_SNAKE_WAND);
}

void snake_kopfzeile_zeichnen() {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(FARBE_SNAKE_TEXT, FARBE_SNAKE_HINTERGRUND);
  tft.fillRect(0, 0, SCREEN_W, HEADER_HOEHE, FARBE_SNAKE_HINTERGRUND);

  char text_punkte[24];
  snprintf(text_punkte, sizeof(text_punkte), "Punkte: %d", snake_punkte);
  tft.drawString(text_punkte, 8, 8, 1);

  char text_highscore[28];
  snprintf(text_highscore, sizeof(text_highscore), "Highscore: %d", snake_highscore);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(text_highscore, SCREEN_W - 8, 8, 1);
}

// ----------------------------------------------------------------------------
//  EINE ZELLE ZEICHNEN (fuer Koerper, Kopf, Essen, oder Loeschen)
// ----------------------------------------------------------------------------
void snake_zelle_zeichnen(int spalte, int zeile, uint16_t farbe) {
  int px = SNAKE_FELD_X + spalte * SNAKE_ZELLE;
  int py = SNAKE_FELD_Y + zeile * SNAKE_ZELLE;
  tft.fillRect(px, py, SNAKE_ZELLE - 1, SNAKE_ZELLE - 1, farbe);
}

// ----------------------------------------------------------------------------
//  EIN SPIELSCHRITT (Bewegung, Kollision, Essen)
//  Regelmaessig in loop() aufrufen, wenn spiel_aktiv == SPIEL_SNAKE
// ----------------------------------------------------------------------------
void snake_schritt_ausfuehren() {
  if (!snake_spiel_laeuft) return;
  if (millis() - snake_letzter_schritt < SNAKE_SCHRITT_MS) return;
  snake_letzter_schritt = millis();

  // Richtungswechsel nur anwenden, wenn es keine direkte Umkehr ist
  // (verhindert, dass die Schlange sich sofort selbst rammt)
  bool ist_umkehr =
    (snake_naechste_richtung == SNAKE_RICHTUNG_LINKS  && snake_richtung == SNAKE_RICHTUNG_RECHTS) ||
    (snake_naechste_richtung == SNAKE_RICHTUNG_RECHTS && snake_richtung == SNAKE_RICHTUNG_LINKS)  ||
    (snake_naechste_richtung == SNAKE_RICHTUNG_OBEN   && snake_richtung == SNAKE_RICHTUNG_UNTEN)  ||
    (snake_naechste_richtung == SNAKE_RICHTUNG_UNTEN  && snake_richtung == SNAKE_RICHTUNG_OBEN);

  if (!ist_umkehr) {
    snake_richtung = snake_naechste_richtung;
  }

  int neue_spalte = snake_koerper[0].spalte;
  int neue_zeile = snake_koerper[0].zeile;

  switch (snake_richtung) {
    case SNAKE_RICHTUNG_LINKS:  neue_spalte--; break;
    case SNAKE_RICHTUNG_RECHTS: neue_spalte++; break;
    case SNAKE_RICHTUNG_OBEN:   neue_zeile--;  break;
    case SNAKE_RICHTUNG_UNTEN:  neue_zeile++;  break;
  }

  // Kollision mit der Wand
  if (neue_spalte < 0 || neue_spalte >= SNAKE_SPALTEN ||
      neue_zeile < 0 || neue_zeile >= SNAKE_ZEILEN) {
    snake_spiel_beenden();
    return;
  }

  // Kollision mit dem eigenen Koerper
  for (int i = 0; i < snake_laenge; i++) {
    if (snake_koerper[i].spalte == neue_spalte &&
        snake_koerper[i].zeile == neue_zeile) {
      snake_spiel_beenden();
      return;
    }
  }

  bool essen_gefressen =
    (neue_spalte == snake_essen_spalte && neue_zeile == snake_essen_zeile);

  // Koerper um ein Segment nach vorne verschieben (von hinten nach vorne kopieren)
  int alte_laenge = snake_laenge;
  if (essen_gefressen && snake_laenge < SNAKE_MAX_LAENGE) {
    snake_laenge++;
  }

  for (int i = snake_laenge - 1; i > 0; i--) {
    snake_koerper[i] = snake_koerper[i - 1];
  }
  snake_koerper[0].spalte = neue_spalte;
  snake_koerper[0].zeile = neue_zeile;

  // altes Schwanzende loeschen, falls die Schlange nicht gewachsen ist
  if (!essen_gefressen) {
    SnakeSegment &schwanz = snake_koerper[alte_laenge - 1];
    snake_zelle_zeichnen(schwanz.spalte, schwanz.zeile, FARBE_SNAKE_HINTERGRUND);
  }

  // vorheriger Kopf wird zu normalem Koerpersegment
  if (alte_laenge >= 1) {
    snake_zelle_zeichnen(snake_koerper[1].spalte, snake_koerper[1].zeile, FARBE_SNAKE_KOERPER);
  }

  // neuer Kopf
  snake_zelle_zeichnen(snake_koerper[0].spalte, snake_koerper[0].zeile, FARBE_SNAKE_KOPF);

  if (essen_gefressen) {
    snake_punkte++;
    snake_kopfzeile_zeichnen();
    snake_essen_neu_platzieren();
    snake_zelle_zeichnen(snake_essen_spalte, snake_essen_zeile, FARBE_SNAKE_ESSEN);
  }
}

// ----------------------------------------------------------------------------
//  SPIEL BEENDEN (Kollision) - Highscore aktualisieren, Game-Over-Anzeige
// ----------------------------------------------------------------------------
void snake_spiel_beenden() {
  snake_spiel_laeuft = false;
  snake_spiel_vorbei = true;

  if (snake_punkte > snake_highscore) {
    snake_highscore = snake_punkte;
  }

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_SNAKE_TEXT, FARBE_SNAKE_HINTERGRUND);
  int mitte_x = SNAKE_FELD_X + (SNAKE_SPALTEN * SNAKE_ZELLE) / 2;
  int mitte_y = SNAKE_FELD_Y + (SNAKE_ZEILEN * SNAKE_ZELLE) / 2;

  tft.fillRect(mitte_x - 70, mitte_y - 30, 140, 60, FARBE_SNAKE_HINTERGRUND);
  tft.drawRect(mitte_x - 70, mitte_y - 30, 140, 60, FARBE_SNAKE_WAND);
  tft.drawString("GAME OVER", mitte_x, mitte_y - 12, 1);

  char text_endstand[24];
  snprintf(text_endstand, sizeof(text_endstand), "Punkte: %d", snake_punkte);
  tft.drawString(text_endstand, mitte_x, mitte_y + 10, 1);

  snake_kopfzeile_zeichnen();

  // TODO: sobald die Home-Assistant-/SD-Karten-Anbindung steht, koennte der
  // Highscore hier dauerhaft gespeichert werden, statt nur im Arbeitsspeicher
}

// ----------------------------------------------------------------------------
//  TOUCH-STEUERUNG: BILDSCHIRM IN VIER ZONEN AUFGETEILT
//  In touch_abfragen() aufrufen, wenn spiel_aktiv == SPIEL_SNAKE
//
//  Aufteilung: der Bildschirm wird an der Spielfeldmitte in vier Dreiecke
//  aufgeteilt (oben/unten/links/rechts), je nachdem in welchem Dreieck der
//  Touch-Punkt liegt, wird in diese Richtung abgebogen. Ausserdem beendet
//  ein Antippen des Spielfelds nach Game Over einen Neustart.
// ----------------------------------------------------------------------------
void snake_touch_behandeln() {
  if (snake_spiel_vorbei) {
    // nach Game Over: irgendwo antippen = Neustart
    snake_spiel_starten();
    return;
  }

  if (!snake_spiel_laeuft) return;

  int mitte_x = SCREEN_W / 2;
  int mitte_y = SCREEN_H / 2;

  int dx = touch_x - mitte_x;
  int dy = touch_y - mitte_y;

  // Dreiecks-Zuordnung ueber Diagonalen: je nachdem welcher Betrag groesser
  // ist (|dx| vs |dy|) und welches Vorzeichen, ergibt sich die Richtung
  if (abs(dx) > abs(dy)) {
    snake_naechste_richtung = (dx > 0) ? SNAKE_RICHTUNG_RECHTS : SNAKE_RICHTUNG_LINKS;
  } else {
    snake_naechste_richtung = (dy > 0) ? SNAKE_RICHTUNG_UNTEN : SNAKE_RICHTUNG_OBEN;
  }
}

// ----------------------------------------------------------------------------
//  SNAKE VERLASSEN (z.B. Home-Button gedrueckt) - Spiel anhalten
// ----------------------------------------------------------------------------
void snake_spiel_verlassen() {
  snake_spiel_laeuft = false;
}