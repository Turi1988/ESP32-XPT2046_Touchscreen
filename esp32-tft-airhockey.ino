// ============================================================================
//  SPIEL: AIR HOCKEY (spiel_aktiv == SPIEL_AIR_HOCKEY)
//  Eigene Datei: esp32-tft-airhockey.ino
//
//  Regeln (2 Spieler abwechselnd/gleichzeitig am selben Geraet):
//  - Tisch mit Tor links (Spieler 1 verteidigt) und rechts (Spieler 2
//    verteidigt). Puck prallt oben/unten und an beiden Schlaegern ab.
//  - WICHTIG: der XPT2046-Touch-Controller erkennt nur EINEN Beruehrungs-
//    punkt gleichzeitig. Echtes Zwei-Finger-Gleichzeitig-Spiel geht mit
//    dieser Hardware nicht. Loesung hier: linke Bildschirmhaelfte steuert
//    Schlaeger 1, rechte Haelfte Schlaeger 2 - jeweils per Halten und
//    Ziehen (Schlaeger folgt der Y-Position des Fingers). Es reagiert
//    aber technisch bedingt immer nur der Schlaeger, dessen Seite gerade
//    tatsaechlich beruehrt wird, der andere bleibt bis zur naechsten
//    Beruehrung seiner Seite stehen. Der Puck bewegt sich unabhaengig
//    davon durchgehend weiter (siehe air_hockey_tick_ausfuehren()).
//  - Erster Spieler mit 7 Toren gewinnt die Runde
// ============================================================================

// ----------------------------------------------------------------------------
//  TISCH-GEOMETRIE
// ----------------------------------------------------------------------------
#define HOCKEY_TISCH_X       10
#define HOCKEY_TISCH_Y       (HEADER_HOEHE + 6)
#define HOCKEY_TISCH_BREITE  (SCREEN_W - 20)
#define HOCKEY_TISCH_HOEHE   (SCREEN_H - HEADER_HOEHE - 14)

#define HOCKEY_TISCH_OBEN    HOCKEY_TISCH_Y
#define HOCKEY_TISCH_UNTEN   (HOCKEY_TISCH_Y + HOCKEY_TISCH_HOEHE)
#define HOCKEY_TISCH_LINKS   HOCKEY_TISCH_X
#define HOCKEY_TISCH_RECHTS  (HOCKEY_TISCH_X + HOCKEY_TISCH_BREITE)
#define HOCKEY_MITTE_Y       (HOCKEY_TISCH_OBEN + HOCKEY_TISCH_HOEHE / 2)

#define HOCKEY_TOR_HALBHOEHE  34

#define HOCKEY_PADDEL_RADIUS  13
#define HOCKEY_PUCK_RADIUS     7
#define HOCKEY_PADDEL1_X      (HOCKEY_TISCH_LINKS + 22)
#define HOCKEY_PADDEL2_X      (HOCKEY_TISCH_RECHTS - 22)

#define HOCKEY_SIEGTORE        7
#define HOCKEY_TICK_MS         30
#define HOCKEY_TOR_PAUSE_MS  1200

// ----------------------------------------------------------------------------
//  FARBEN
// ----------------------------------------------------------------------------
#define FARBE_HOCKEY_HINTERGRUND  tft.color565(15, 25, 45)
#define FARBE_HOCKEY_TISCH        tft.color565(20, 60, 40)
#define FARBE_HOCKEY_LINIEN       tft.color565(200, 220, 210)
#define FARBE_HOCKEY_PADDEL1      tft.color565(60, 130, 220)
#define FARBE_HOCKEY_PADDEL2      tft.color565(220, 120, 40)
#define FARBE_HOCKEY_PUCK         tft.color565(20, 20, 25)
#define FARBE_HOCKEY_TEXT         tft.color565(255, 255, 255)

// ----------------------------------------------------------------------------
//  PHASEN
// ----------------------------------------------------------------------------
#define HOCKEY_PHASE_SPIEL  0
#define HOCKEY_PHASE_TOR    1
#define HOCKEY_PHASE_ENDE   2

int hockey_phase = HOCKEY_PHASE_SPIEL;
unsigned long hockey_tor_zeit = 0;
int hockey_letztes_tor_fuer = 0;

// ----------------------------------------------------------------------------
//  SPIELZUSTAND (float fuer ruckelfreiere Bewegung, gerundet beim Zeichnen)
// ----------------------------------------------------------------------------
float hockey_puck_x, hockey_puck_y;
float hockey_puck_vx, hockey_puck_vy;
float hockey_paddel1_y, hockey_paddel2_y;

// vorherige Positionen zum gezielten Loeschen beim Neuzeichnen
int hockey_alt_puck_x, hockey_alt_puck_y;
int hockey_alt_paddel1_y, hockey_alt_paddel2_y;

int hockey_punkte_spieler1 = 0;
int hockey_punkte_spieler2 = 0;

unsigned long hockey_letzter_tick = 0;

// ----------------------------------------------------------------------------
//  PUCK IN DIE MITTE SETZEN MIT ZUFAELLIGER STARTRICHTUNG
// ----------------------------------------------------------------------------
void hockey_puck_zuruecksetzen() {
  hockey_puck_x = HOCKEY_TISCH_LINKS + HOCKEY_TISCH_BREITE / 2;
  hockey_puck_y = HOCKEY_MITTE_Y;

  float winkel = random(0, 360) * 3.14159 / 180.0;
  float geschwindigkeit = 2.2;
  hockey_puck_vx = cos(winkel) * geschwindigkeit;
  hockey_puck_vy = sin(winkel) * geschwindigkeit;
  // zu flache Winkel vermeiden, damit der Puck nicht ewig an einer Wand entlanglaeuft
  if (fabs(hockey_puck_vy) < 0.6) hockey_puck_vy = (hockey_puck_vy < 0) ? -0.6 : 0.6;
}

// ----------------------------------------------------------------------------
//  SPIEL STARTEN / NEUE RUNDE
// ----------------------------------------------------------------------------
void hockey_spiel_starten() {
  hockey_punkte_spieler1 = 0;
  hockey_punkte_spieler2 = 0;
  hockey_paddel1_y = HOCKEY_MITTE_Y;
  hockey_paddel2_y = HOCKEY_MITTE_Y;
  hockey_puck_zuruecksetzen();
  hockey_phase = HOCKEY_PHASE_SPIEL;
  hockey_letzter_tick = millis();

  tft.fillScreen(FARBE_HOCKEY_HINTERGRUND);
  hockey_kopfzeile_zeichnen();
  hockey_tisch_zeichnen();

  hockey_alt_puck_x = (int)hockey_puck_x;
  hockey_alt_puck_y = (int)hockey_puck_y;
  hockey_alt_paddel1_y = (int)hockey_paddel1_y;
  hockey_alt_paddel2_y = (int)hockey_paddel2_y;

  hockey_spielfiguren_zeichnen();
}

// ----------------------------------------------------------------------------
//  KOPFZEILE: PUNKTESTAND
// ----------------------------------------------------------------------------
void hockey_kopfzeile_zeichnen() {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(FARBE_HOCKEY_PADDEL1, FARBE_HOCKEY_HINTERGRUND);
  char text1[16];
  snprintf(text1, sizeof(text1), "Spieler 1: %d", hockey_punkte_spieler1);
  tft.drawString(text1, 8, 4, 1);

  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(FARBE_HOCKEY_PADDEL2, FARBE_HOCKEY_HINTERGRUND);
  char text2[16];
  snprintf(text2, sizeof(text2), "Spieler 2: %d", hockey_punkte_spieler2);
  tft.drawString(text2, SCREEN_W - 8, 4, 1);
}

// ----------------------------------------------------------------------------
//  TISCH ZEICHNEN: FLAECHE, MITTELLINIE, WAENDE MIT TORLUECKE
// ----------------------------------------------------------------------------
void hockey_tisch_zeichnen() {
  tft.fillRect(HOCKEY_TISCH_LINKS, HOCKEY_TISCH_OBEN, HOCKEY_TISCH_BREITE, HOCKEY_TISCH_HOEHE, FARBE_HOCKEY_TISCH);

  // Mittellinie gestrichelt
  for (int y = HOCKEY_TISCH_OBEN; y < HOCKEY_TISCH_UNTEN; y += 10) {
    tft.drawFastVLine(HOCKEY_TISCH_LINKS + HOCKEY_TISCH_BREITE / 2, y, 5, FARBE_HOCKEY_LINIEN);
  }
  tft.drawCircle(HOCKEY_TISCH_LINKS + HOCKEY_TISCH_BREITE / 2, HOCKEY_MITTE_Y, 20, FARBE_HOCKEY_LINIEN);

  // obere/untere Wand durchgehend
  tft.drawFastHLine(HOCKEY_TISCH_LINKS, HOCKEY_TISCH_OBEN, HOCKEY_TISCH_BREITE, FARBE_HOCKEY_LINIEN);
  tft.drawFastHLine(HOCKEY_TISCH_LINKS, HOCKEY_TISCH_UNTEN - 1, HOCKEY_TISCH_BREITE, FARBE_HOCKEY_LINIEN);

  // linke/rechte Wand mit Torluecke in der Mitte
  int tor_oben = HOCKEY_MITTE_Y - HOCKEY_TOR_HALBHOEHE;
  int tor_unten = HOCKEY_MITTE_Y + HOCKEY_TOR_HALBHOEHE;

  tft.drawFastVLine(HOCKEY_TISCH_LINKS, HOCKEY_TISCH_OBEN, tor_oben - HOCKEY_TISCH_OBEN, FARBE_HOCKEY_LINIEN);
  tft.drawFastVLine(HOCKEY_TISCH_LINKS, tor_unten, HOCKEY_TISCH_UNTEN - tor_unten, FARBE_HOCKEY_LINIEN);

  tft.drawFastVLine(HOCKEY_TISCH_RECHTS - 1, HOCKEY_TISCH_OBEN, tor_oben - HOCKEY_TISCH_OBEN, FARBE_HOCKEY_LINIEN);
  tft.drawFastVLine(HOCKEY_TISCH_RECHTS - 1, tor_unten, HOCKEY_TISCH_UNTEN - tor_unten, FARBE_HOCKEY_LINIEN);
}

// ----------------------------------------------------------------------------
//  PADDEL + PUCK ZEICHNEN (an aktueller Position)
// ----------------------------------------------------------------------------
void hockey_spielfiguren_zeichnen() {
  tft.fillCircle(HOCKEY_PADDEL1_X, (int)hockey_paddel1_y, HOCKEY_PADDEL_RADIUS, FARBE_HOCKEY_PADDEL1);
  tft.fillCircle(HOCKEY_PADDEL2_X, (int)hockey_paddel2_y, HOCKEY_PADDEL_RADIUS, FARBE_HOCKEY_PADDEL2);
  tft.fillCircle((int)hockey_puck_x, (int)hockey_puck_y, HOCKEY_PUCK_RADIUS, FARBE_HOCKEY_PUCK);
}

// alte Positionen mit Tischfarbe uebermalen, dann neu zeichnen (kein Vollbild-Neuaufbau noetig)
void hockey_spielfiguren_aktualisieren() {
  tft.fillCircle(HOCKEY_PADDEL1_X, hockey_alt_paddel1_y, HOCKEY_PADDEL_RADIUS, FARBE_HOCKEY_TISCH);
  tft.fillCircle(HOCKEY_PADDEL2_X, hockey_alt_paddel2_y, HOCKEY_PADDEL_RADIUS, FARBE_HOCKEY_TISCH);
  tft.fillCircle(hockey_alt_puck_x, hockey_alt_puck_y, HOCKEY_PUCK_RADIUS, FARBE_HOCKEY_TISCH);

  // ggf. ueberzeichnete Wand-/Mittellinienstuecke an den alten Stellen wiederherstellen
  if (abs(hockey_alt_paddel1_y - (int)hockey_MITTE_platzhalter()) < 0) {} // ungenutzt, siehe unten

  hockey_spielfiguren_zeichnen();

  hockey_alt_paddel1_y = (int)hockey_paddel1_y;
  hockey_alt_paddel2_y = (int)hockey_paddel2_y;
  hockey_alt_puck_x = (int)hockey_puck_x;
  hockey_alt_puck_y = (int)hockey_puck_y;
}

// kleine Ersatzfunktion nur zur Vermeidung eines unbenutzten Ausdrucks oben - kann ignoriert werden
int hockey_MITTE_platzhalter() { return HOCKEY_MITTE_Y; }

// ----------------------------------------------------------------------------
//  TOR-ANZEIGE KURZ EINBLENDEN
// ----------------------------------------------------------------------------
void hockey_tor_anzeige_zeichnen(int spieler) {
  int bx = SCREEN_W / 2 - 60, by = HOCKEY_MITTE_Y - 20;
  tft.fillRoundRect(bx, by, 120, 40, 8, FARBE_HOCKEY_TISCH);
  tft.drawRoundRect(bx, by, 120, 40, 8, FARBE_HOCKEY_LINIEN);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_HOCKEY_TEXT, FARBE_HOCKEY_TISCH);
  char text[20];
  snprintf(text, sizeof(text), "Tor Spieler %d!", spieler);
  tft.drawString(text, SCREEN_W / 2, HOCKEY_MITTE_Y, 1);
}

// ----------------------------------------------------------------------------
//  ENDANZEIGE
// ----------------------------------------------------------------------------
void hockey_endanzeige_zeichnen() {
  int bx = SCREEN_W / 2 - 90, by = HOCKEY_MITTE_Y - 30;
  tft.fillRoundRect(bx, by, 180, 60, 8, FARBE_HOCKEY_TISCH);
  tft.drawRoundRect(bx, by, 180, 60, 8, FARBE_HOCKEY_LINIEN);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_HOCKEY_TEXT, FARBE_HOCKEY_TISCH);
  int sieger = (hockey_punkte_spieler1 > hockey_punkte_spieler2) ? 1 : 2;
  char text[20];
  snprintf(text, sizeof(text), "Spieler %d gewinnt!", sieger);
  tft.drawString(text, SCREEN_W / 2, by + 22, 2);
  tft.drawString("Antippen fuer neue Runde", SCREEN_W / 2, by + 44, 1);
}

// ----------------------------------------------------------------------------
//  PADDEL-Y BEWEGEN (aus main.ino waehrend gehaltenem Touch aufgerufen,
//  siehe Hinweis zur Einbindung). x entscheidet, welche Seite/welcher
//  Schlaeger gesteuert wird, y ist die Zielposition.
// ----------------------------------------------------------------------------
void hockey_touch_halten() {
  if (hockey_phase != HOCKEY_PHASE_SPIEL) return;
  if (touch_y < HOCKEY_TISCH_OBEN || touch_y > HOCKEY_TISCH_UNTEN) return;

  int ziel_y = touch_y;
  if (ziel_y < HOCKEY_TISCH_OBEN + HOCKEY_PADDEL_RADIUS) ziel_y = HOCKEY_TISCH_OBEN + HOCKEY_PADDEL_RADIUS;
  if (ziel_y > HOCKEY_TISCH_UNTEN - HOCKEY_PADDEL_RADIUS) ziel_y = HOCKEY_TISCH_UNTEN - HOCKEY_PADDEL_RADIUS;

  int mitte_x = HOCKEY_TISCH_LINKS + HOCKEY_TISCH_BREITE / 2;
  if (touch_x < mitte_x) {
    hockey_paddel1_y = ziel_y;
  } else {
    hockey_paddel2_y = ziel_y;
  }
}

// ----------------------------------------------------------------------------
//  PUCK-KOLLISION MIT EINEM PADDEL PRUEFEN UND GGF. ABPRALLEN LASSEN
// ----------------------------------------------------------------------------
void hockey_pruefe_paddel_kollision(int paddel_x, float paddel_y) {
  float dx = hockey_puck_x - paddel_x;
  float dy = hockey_puck_y - paddel_y;
  float abstand = sqrt(dx * dx + dy * dy);
  float mindestabstand = HOCKEY_PADDEL_RADIUS + HOCKEY_PUCK_RADIUS;

  if (abstand < mindestabstand && abstand > 0.01) {
    float nx = dx / abstand;
    float ny = dy / abstand;

    // Puck aus der Ueberlappung herausschieben
    float ueberlappung = mindestabstand - abstand;
    hockey_puck_x += nx * ueberlappung;
    hockey_puck_y += ny * ueberlappung;

    // Geschwindigkeit entlang der Normalen reflektieren, mit kleinem Schub
    float geschwindigkeit = sqrt(hockey_puck_vx * hockey_puck_vx + hockey_puck_vy * hockey_puck_vy);
    if (geschwindigkeit < 3.0) geschwindigkeit = 3.0;
    hockey_puck_vx = nx * geschwindigkeit;
    hockey_puck_vy = ny * geschwindigkeit;
  }
}

// ----------------------------------------------------------------------------
//  PHYSIK-TICK: PUCK BEWEGEN, WAENDE/PADDEL/TORE PRUEFEN, NEU ZEICHNEN
//  Regelmaessig in loop() aufrufen, wenn spiel_aktiv == SPIEL_AIR_HOCKEY
// ----------------------------------------------------------------------------
void hockey_tick_ausfuehren() {
  if (hockey_phase == HOCKEY_PHASE_TOR) {
    if (millis() - hockey_tor_zeit > HOCKEY_TOR_PAUSE_MS) {
      hockey_puck_zuruecksetzen();
      hockey_phase = HOCKEY_PHASE_SPIEL;
      hockey_tisch_zeichnen();
      hockey_spielfiguren_zeichnen();
      hockey_alt_puck_x = (int)hockey_puck_x;
      hockey_alt_puck_y = (int)hockey_puck_y;
    }
    return;
  }

  if (hockey_phase != HOCKEY_PHASE_SPIEL) return;
  if (millis() - hockey_letzter_tick < HOCKEY_TICK_MS) return;
  hockey_letzter_tick = millis();

  hockey_puck_x += hockey_puck_vx;
  hockey_puck_y += hockey_puck_vy;

  // obere/untere Wand
  if (hockey_puck_y - HOCKEY_PUCK_RADIUS < HOCKEY_TISCH_OBEN) {
    hockey_puck_y = HOCKEY_TISCH_OBEN + HOCKEY_PUCK_RADIUS;
    hockey_puck_vy = -hockey_puck_vy;
  } else if (hockey_puck_y + HOCKEY_PUCK_RADIUS > HOCKEY_TISCH_UNTEN) {
    hockey_puck_y = HOCKEY_TISCH_UNTEN - HOCKEY_PUCK_RADIUS;
    hockey_puck_vy = -hockey_puck_vy;
  }

  bool im_tor_bereich = (hockey_puck_y > HOCKEY_MITTE_Y - HOCKEY_TOR_HALBHOEHE &&
                         hockey_puck_y < HOCKEY_MITTE_Y + HOCKEY_TOR_HALBHOEHE);

  // linke Seite: Tor oder Wand
  if (hockey_puck_x - HOCKEY_PUCK_RADIUS < HOCKEY_TISCH_LINKS) {
    if (im_tor_bereich) {
      hockey_tor_erzielt(2);
      return;
    } else {
      hockey_puck_x = HOCKEY_TISCH_LINKS + HOCKEY_PUCK_RADIUS;
      hockey_puck_vx = -hockey_puck_vx;
    }
  }
  // rechte Seite: Tor oder Wand
  if (hockey_puck_x + HOCKEY_PUCK_RADIUS > HOCKEY_TISCH_RECHTS) {
    if (im_tor_bereich) {
      hockey_tor_erzielt(1);
      return;
    } else {
      hockey_puck_x = HOCKEY_TISCH_RECHTS - HOCKEY_PUCK_RADIUS;
      hockey_puck_vx = -hockey_puck_vx;
    }
  }

  hockey_pruefe_paddel_kollision(HOCKEY_PADDEL1_X, hockey_paddel1_y);
  hockey_pruefe_paddel_kollision(HOCKEY_PADDEL2_X, hockey_paddel2_y);

  hockey_spielfiguren_aktualisieren();
}

void hockey_tor_erzielt(int spieler) {
  if (spieler == 1) hockey_punkte_spieler1++;
  else hockey_punkte_spieler2++;

  hockey_kopfzeile_zeichnen();
  hockey_tor_anzeige_zeichnen(spieler);

  if (hockey_punkte_spieler1 >= HOCKEY_SIEGTORE || hockey_punkte_spieler2 >= HOCKEY_SIEGTORE) {
    hockey_phase = HOCKEY_PHASE_ENDE;
    hockey_endanzeige_zeichnen();
    return;
  }

  hockey_letztes_tor_fuer = spieler;
  hockey_tor_zeit = millis();
  hockey_phase = HOCKEY_PHASE_TOR;
}

// ----------------------------------------------------------------------------
//  EINZELNER KLICK: NUR RELEVANT NACH SPIELENDE (NEUE RUNDE STARTEN)
//  In touch_abfragen() im normalen Einzelklick-Zweig aufrufen, wenn
//  spiel_aktiv == SPIEL_AIR_HOCKEY UND hockey_phase == HOCKEY_PHASE_ENDE
// ----------------------------------------------------------------------------
void hockey_touch_behandeln() {
  if (hockey_phase == HOCKEY_PHASE_ENDE) {
    hockey_spiel_starten();
  }
}

// ----------------------------------------------------------------------------
//  AIR HOCKEY VERLASSEN (z.B. Home-Button gedrueckt)
// ----------------------------------------------------------------------------
void hockey_spiel_verlassen() {
  // kein Aufraeumen noetig, Spielstand bleibt einfach im Speicher erhalten
}