// ============================================================================
//  SPIEL: PIPE MANIA / ROHR (spiel_aktiv == SPIEL_PIPE_MANIA)
//  Eigene Datei: esp32-tft-rohr.ino
//
//  Regeln:
//  - Spielfeld 6 Spalten x 6 Zeilen aus Rohrfeldern
//  - Rohrtypen: gerade (waagerecht/senkrecht), kurvig (4 Ausrichtungen),
//    T-Stueck (4 Ausrichtungen)
//  - Naechstes verfuegbares Rohrteil wird rechts als kleine Vorschau gezeigt
//    (wie bei Tetris die naechste Figur)
//  - Rechte Spalte zeigt zusaetzlich den Punktestand
//  - Bauphase mit Zeitlimit: Rohre antippen/platzieren, um eine Verbindung
//    von Start- zu Zielfeld zu legen
//  - Nach Ablauf der Bauzeit startet das Wasser am Startfeld und laeuft
//    sichtbar Rohr fuer Rohr weiter (jedes durchflossene Rohr wird innen
//    hellblau eingefaerbt), bis entweder das Zielfeld erreicht ist
//    (Level geschafft) oder die Rohrverbindung abbricht (Game Over)
//  - Punkte: 1 Punkt pro Rohr, durch das das Wasser erfolgreich
//    hindurchgeflossen ist, bevor es "ins Nichts" laeuft
// ============================================================================

// ----------------------------------------------------------------------------
//  SPIELFELD-EINSTELLUNGEN
// ----------------------------------------------------------------------------
#define ROHR_SPALTEN         6
#define ROHR_ZEILEN          6
#define ROHR_FELD_X          10
#define ROHR_FELD_Y          (HEADER_HOEHE + 10)
#define ROHR_ZELLE            34    // Kantenlaenge eines Rohrfelds in Pixeln

#define ROHR_BAUZEIT_MS       30000   // 30 Sekunden Bauzeit
#define ROHR_FLIESS_SCHRITT_MS 400    // Zeit pro Rohr-Abschnitt beim Durchfliessen

// rechte Seitenspalte fuer Punktestand + Vorschau
#define ROHR_SEITE_X          (ROHR_FELD_X + ROHR_SPALTEN * ROHR_ZELLE + 20)

// ----------------------------------------------------------------------------
//  ROHRTYPEN
//  GERADE: durchgehend in eine Achse (waagerecht oder senkrecht je nach drehung)
//  KURVE: verbindet zwei benachbarte Seiten (90 Grad)
//  T_STUECK: verbindet drei der vier Seiten
// ----------------------------------------------------------------------------
#define ROHR_TYP_GERADE     0
#define ROHR_TYP_KURVE      1
#define ROHR_TYP_T_STUECK   2
#define ROHR_TYP_LEER       3   // noch nicht platziertes Feld

// Rohr-Ausrichtung: welche der vier Seiten (oben/rechts/unten/links) offen sind,
// als Bitmaske: 1=oben, 2=rechts, 4=unten, 8=links
#define ROHR_SEITE_OBEN   1
#define ROHR_SEITE_RECHTS 2
#define ROHR_SEITE_UNTEN  4
#define ROHR_SEITE_LINKS  8

// ----------------------------------------------------------------------------
//  FARBEN
// ----------------------------------------------------------------------------
#define FARBE_ROHR_HINTERGRUND   tft.color565(15, 25, 45)
#define FARBE_ROHR_LEERFELD      tft.color565(35, 45, 65)
#define FARBE_ROHR_KOERPER       tft.color565(150, 110, 30)
#define FARBE_ROHR_WASSER        tft.color565(90, 200, 255)   // helles Blau
#define FARBE_ROHR_START         tft.color565(40, 170, 80)
#define FARBE_ROHR_ZIEL          tft.color565(200, 50, 50)
#define FARBE_ROHR_TEXT          tft.color565(255, 255, 255)
#define FARBE_ROHR_RAHMEN        tft.color565(90, 90, 100)

// ----------------------------------------------------------------------------
//  DATENSTRUKTUR EIN FELD
// ----------------------------------------------------------------------------
struct RohrFeld {
  int typ;          // ROHR_TYP_*
  uint8_t seiten;    // Bitmaske offener Seiten (0 wenn leer)
  bool vom_wasser_erreicht;
};

RohrFeld rohr_feld[ROHR_SPALTEN][ROHR_ZEILEN];

// Start- und Zielposition (fest: Start links oben, Ziel rechts unten)
int rohr_start_spalte = 0;
int rohr_start_zeile = 0;
int rohr_ziel_spalte = ROHR_SPALTEN - 1;
int rohr_ziel_zeile = ROHR_ZEILEN - 1;

// naechstes verfuegbares Rohrteil (Vorschau)
int rohr_naechster_typ = ROHR_TYP_GERADE;
uint8_t rohr_naechste_seiten = ROHR_SEITE_OBEN | ROHR_SEITE_UNTEN;

int rohr_punkte = 0;

bool rohr_bauphase_aktiv = false;
bool rohr_fliessphase_aktiv = false;
bool rohr_spiel_vorbei = false;
bool rohr_level_geschafft = false;

unsigned long rohr_bauphase_start = 0;
unsigned long rohr_letzter_fliess_schritt = 0;

// Reihenfolge der Felder, durch die das Wasser bereits geflossen ist
int rohr_fliessweg_spalte[ROHR_SPALTEN * ROHR_ZEILEN];
int rohr_fliessweg_zeile[ROHR_SPALTEN * ROHR_ZEILEN];
int rohr_fliessweg_laenge = 0;
int rohr_fliess_index = 0;

// ----------------------------------------------------------------------------
//  ZUFAELLIGES NEUES ROHRTEIL FUER DIE VORSCHAU ERZEUGEN
// ----------------------------------------------------------------------------
void rohr_neues_vorschauteil_erzeugen() {
  int zufallstyp = random(0, 3);   // 0=gerade, 1=kurve, 2=t-stueck
  int zufallsdrehung = random(0, 4);

  if (zufallstyp == 0) {
    rohr_naechster_typ = ROHR_TYP_GERADE;
    rohr_naechste_seiten = (zufallsdrehung % 2 == 0)
      ? (ROHR_SEITE_OBEN | ROHR_SEITE_UNTEN)
      : (ROHR_SEITE_LINKS | ROHR_SEITE_RECHTS);
  } else if (zufallstyp == 1) {
    rohr_naechster_typ = ROHR_TYP_KURVE;
    switch (zufallsdrehung) {
      case 0: rohr_naechste_seiten = ROHR_SEITE_OBEN | ROHR_SEITE_RECHTS; break;
      case 1: rohr_naechste_seiten = ROHR_SEITE_RECHTS | ROHR_SEITE_UNTEN; break;
      case 2: rohr_naechste_seiten = ROHR_SEITE_UNTEN | ROHR_SEITE_LINKS; break;
      case 3: rohr_naechste_seiten = ROHR_SEITE_LINKS | ROHR_SEITE_OBEN; break;
    }
  } else {
    rohr_naechster_typ = ROHR_TYP_T_STUECK;
    switch (zufallsdrehung) {
      case 0: rohr_naechste_seiten = ROHR_SEITE_OBEN | ROHR_SEITE_RECHTS | ROHR_SEITE_UNTEN; break;
      case 1: rohr_naechste_seiten = ROHR_SEITE_RECHTS | ROHR_SEITE_UNTEN | ROHR_SEITE_LINKS; break;
      case 2: rohr_naechste_seiten = ROHR_SEITE_UNTEN | ROHR_SEITE_LINKS | ROHR_SEITE_OBEN; break;
      case 3: rohr_naechste_seiten = ROHR_SEITE_LINKS | ROHR_SEITE_OBEN | ROHR_SEITE_RECHTS; break;
    }
  }
}

// ----------------------------------------------------------------------------
//  SPIEL STARTEN / ZURUECKSETZEN
// ----------------------------------------------------------------------------
void rohr_spiel_starten() {
  for (int s = 0; s < ROHR_SPALTEN; s++) {
    for (int z = 0; z < ROHR_ZEILEN; z++) {
      rohr_feld[s][z].typ = ROHR_TYP_LEER;
      rohr_feld[s][z].seiten = 0;
      rohr_feld[s][z].vom_wasser_erreicht = false;
    }
  }

  rohr_punkte = 0;
  rohr_bauphase_aktiv = true;
  rohr_fliessphase_aktiv = false;
  rohr_spiel_vorbei = false;
  rohr_level_geschafft = false;
  rohr_bauphase_start = millis();
  rohr_fliessweg_laenge = 0;
  rohr_fliess_index = 0;

  rohr_neues_vorschauteil_erzeugen();

  tft.fillScreen(FARBE_ROHR_HINTERGRUND);
  rohr_spielfeld_komplett_zeichnen();
  rohr_seitenleiste_zeichnen();
}

// ----------------------------------------------------------------------------
//  KOMPLETTES SPIELFELD ZEICHNEN (Raster, Start-/Zielfeld)
// ----------------------------------------------------------------------------
void rohr_spielfeld_komplett_zeichnen() {
  for (int s = 0; s < ROHR_SPALTEN; s++) {
    for (int z = 0; z < ROHR_ZEILEN; z++) {
      rohr_feld_zeichnen(s, z);
    }
  }
}

// ----------------------------------------------------------------------------
//  EIN EINZELNES FELD ZEICHNEN
// ----------------------------------------------------------------------------
void rohr_feld_zeichnen(int spalte, int zeile) {
  int px = ROHR_FELD_X + spalte * ROHR_ZELLE;
  int py = ROHR_FELD_Y + zeile * ROHR_ZELLE;

  uint16_t hintergrundfarbe = FARBE_ROHR_LEERFELD;
  if (spalte == rohr_start_spalte && zeile == rohr_start_zeile) {
    hintergrundfarbe = FARBE_ROHR_START;
  } else if (spalte == rohr_ziel_spalte && zeile == rohr_ziel_zeile) {
    hintergrundfarbe = FARBE_ROHR_ZIEL;
  }

  tft.fillRect(px, py, ROHR_ZELLE - 1, ROHR_ZELLE - 1, hintergrundfarbe);
  tft.drawRect(px, py, ROHR_ZELLE - 1, ROHR_ZELLE - 1, FARBE_ROHR_RAHMEN);

  RohrFeld &feld = rohr_feld[spalte][zeile];
  if (feld.typ == ROHR_TYP_LEER) return;

  int mitte_x = px + ROHR_ZELLE / 2;
  int mitte_y = py + ROHR_ZELLE / 2;
  int rohr_dicke = 10;

  uint16_t rohrfarbe = feld.vom_wasser_erreicht ? FARBE_ROHR_WASSER : FARBE_ROHR_KOERPER;

  // Mittelstueck
  tft.fillRect(mitte_x - rohr_dicke / 2, mitte_y - rohr_dicke / 2,
               rohr_dicke, rohr_dicke, rohrfarbe);

  // Verbindung zu jeder offenen Seite als Balken von der Mitte zum Rand
  if (feld.seiten & ROHR_SEITE_OBEN) {
    tft.fillRect(mitte_x - rohr_dicke / 2, py, rohr_dicke, ROHR_ZELLE / 2, rohrfarbe);
  }
  if (feld.seiten & ROHR_SEITE_UNTEN) {
    tft.fillRect(mitte_x - rohr_dicke / 2, mitte_y, rohr_dicke, ROHR_ZELLE / 2, rohrfarbe);
  }
  if (feld.seiten & ROHR_SEITE_LINKS) {
    tft.fillRect(px, mitte_y - rohr_dicke / 2, ROHR_ZELLE / 2, rohr_dicke, rohrfarbe);
  }
  if (feld.seiten & ROHR_SEITE_RECHTS) {
    tft.fillRect(mitte_x, mitte_y - rohr_dicke / 2, ROHR_ZELLE / 2, rohr_dicke, rohrfarbe);
  }
}

// ----------------------------------------------------------------------------
//  SEITENLEISTE: PUNKTESTAND UND VORSCHAU DES NAECHSTEN ROHRTEILS
// ----------------------------------------------------------------------------
void rohr_seitenleiste_zeichnen() {
  tft.fillRect(ROHR_SEITE_X, ROHR_FELD_Y, SCREEN_W - ROHR_SEITE_X - 5, 200, FARBE_ROHR_HINTERGRUND);

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(FARBE_ROHR_TEXT, FARBE_ROHR_HINTERGRUND);
  char text_punkte[20];
  snprintf(text_punkte, sizeof(text_punkte), "Punkte: %d", rohr_punkte);
  tft.drawString(text_punkte, ROHR_SEITE_X, ROHR_FELD_Y, 1);

  if (rohr_bauphase_aktiv) {
    unsigned long verbleibend_ms = ROHR_BAUZEIT_MS - (millis() - rohr_bauphase_start);
    if (verbleibend_ms > ROHR_BAUZEIT_MS) verbleibend_ms = 0;  // Ueberlauf abfangen
    char text_zeit[20];
    snprintf(text_zeit, sizeof(text_zeit), "Zeit: %lus", verbleibend_ms / 1000);
    tft.fillRect(ROHR_SEITE_X, ROHR_FELD_Y + 20, 100, 16, FARBE_ROHR_HINTERGRUND);
    tft.drawString(text_zeit, ROHR_SEITE_X, ROHR_FELD_Y + 20, 1);
  }

  tft.drawString("Naechstes:", ROHR_SEITE_X, ROHR_FELD_Y + 50, 1);

  // Vorschau-Kachel fuer das naechste Rohrteil
  int vx = ROHR_SEITE_X;
  int vy = ROHR_FELD_Y + 70;
  int vg = ROHR_ZELLE;
  tft.fillRect(vx, vy, vg - 1, vg - 1, FARBE_ROHR_LEERFELD);
  tft.drawRect(vx, vy, vg - 1, vg - 1, FARBE_ROHR_RAHMEN);

  int vmx = vx + vg / 2;
  int vmy = vy + vg / 2;
  int vd = 10;
  tft.fillRect(vmx - vd / 2, vmy - vd / 2, vd, vd, FARBE_ROHR_KOERPER);
  if (rohr_naechste_seiten & ROHR_SEITE_OBEN)
    tft.fillRect(vmx - vd / 2, vy, vd, vg / 2, FARBE_ROHR_KOERPER);
  if (rohr_naechste_seiten & ROHR_SEITE_UNTEN)
    tft.fillRect(vmx - vd / 2, vmy, vd, vg / 2, FARBE_ROHR_KOERPER);
  if (rohr_naechste_seiten & ROHR_SEITE_LINKS)
    tft.fillRect(vx, vmy - vd / 2, vg / 2, vd, FARBE_ROHR_KOERPER);
  if (rohr_naechste_seiten & ROHR_SEITE_RECHTS)
    tft.fillRect(vmx, vmy - vd / 2, vg / 2, vd, FARBE_ROHR_KOERPER);
}

// ----------------------------------------------------------------------------
//  BAUPHASE: REGELMAESSIG IN loop() AUFRUFEN, WENN spiel_aktiv == SPIEL_PIPE_MANIA
//  Prueft Zeitlimit und startet ggf. die Fliessphase
// ----------------------------------------------------------------------------
void rohr_bauphase_pruefen() {
  if (!rohr_bauphase_aktiv) return;

  // Zeitanzeige regelmaessig aktualisieren
  static unsigned long letzte_anzeige_aktualisierung = 0;
  if (millis() - letzte_anzeige_aktualisierung > 1000) {
    letzte_anzeige_aktualisierung = millis();
    rohr_seitenleiste_zeichnen();
  }

  if (millis() - rohr_bauphase_start >= ROHR_BAUZEIT_MS) {
    rohr_bauphase_aktiv = false;
    rohr_fliessphase_starten();
  }
}

// ----------------------------------------------------------------------------
//  TOUCH-BEHANDLUNG WAEHREND DER BAUPHASE: Rohrteil auf angetipptes Feld legen
//  In touch_abfragen() aufrufen, wenn spiel_aktiv == SPIEL_PIPE_MANIA
// ----------------------------------------------------------------------------
void rohr_touch_behandeln() {
  if (rohr_spiel_vorbei || rohr_level_geschafft) {
    // nach Spielende: irgendwo antippen = Neustart
    rohr_spiel_starten();
    return;
  }

  if (!rohr_bauphase_aktiv) return;

  if (touch_x < ROHR_FELD_X || touch_y < ROHR_FELD_Y) return;

  int spalte = (touch_x - ROHR_FELD_X) / ROHR_ZELLE;
  int zeile = (touch_y - ROHR_FELD_Y) / ROHR_ZELLE;

  if (spalte < 0 || spalte >= ROHR_SPALTEN || zeile < 0 || zeile >= ROHR_ZEILEN) return;

  // Start- und Zielfeld koennen nicht ueberbaut werden
  if ((spalte == rohr_start_spalte && zeile == rohr_start_zeile) ||
      (spalte == rohr_ziel_spalte && zeile == rohr_ziel_zeile)) {
    return;
  }

  // Rohrteil auf das Feld setzen und neues Vorschauteil erzeugen
  rohr_feld[spalte][zeile].typ = rohr_naechster_typ;
  rohr_feld[spalte][zeile].seiten = rohr_naechste_seiten;
  rohr_feld_zeichnen(spalte, zeile);

  rohr_neues_vorschauteil_erzeugen();
  rohr_seitenleiste_zeichnen();
}

// ----------------------------------------------------------------------------
//  FLIESSPHASE STARTEN: Wasserweg vom Startfeld aus verfolgen und in
//  rohr_fliessweg_spalte/zeile ablegen (auch das Startfeld selbst hat
//  feste Verbindung Richtung erstem Nachbarn)
// ----------------------------------------------------------------------------
void rohr_fliessphase_starten() {
  rohr_fliessweg_laenge = 0;
  rohr_fliess_index = 0;

  int aktuelle_spalte = rohr_start_spalte;
  int aktuelle_zeile = rohr_start_zeile;
  int von_seite = -1;   // Seite, aus der wir kamen (nicht erneut zurueck verfolgen)

  rohr_fliessweg_spalte[rohr_fliessweg_laenge] = aktuelle_spalte;
  rohr_fliessweg_zeile[rohr_fliessweg_laenge] = aktuelle_zeile;
  rohr_fliessweg_laenge++;

  bool weiterverfolgen = true;
  while (weiterverfolgen && rohr_fliessweg_laenge < ROHR_SPALTEN * ROHR_ZEILEN) {
    // Nachbarfelder in jede offene Richtung pruefen (ausser der Herkunftsseite)
    int naechste_spalte = -1, naechste_zeile = -1;

    struct { int dx, dy; uint8_t seite_von_hier, seite_vom_nachbar; } richtungen[4] = {
      {0, -1, ROHR_SEITE_OBEN, ROHR_SEITE_UNTEN},
      {1, 0, ROHR_SEITE_RECHTS, ROHR_SEITE_LINKS},
      {0, 1, ROHR_SEITE_UNTEN, ROHR_SEITE_OBEN},
      {-1, 0, ROHR_SEITE_LINKS, ROHR_SEITE_RECHTS}
    };

    // aktuelle Feld-Seiten ermitteln (Start-/Zielfeld gelten als "offen in alle
    // Richtungen, in die ein angrenzendes Rohr passt")
    uint8_t aktuelle_seiten;
    if (aktuelle_spalte == rohr_start_spalte && aktuelle_zeile == rohr_start_zeile) {
      aktuelle_seiten = ROHR_SEITE_OBEN | ROHR_SEITE_RECHTS | ROHR_SEITE_UNTEN | ROHR_SEITE_LINKS;
    } else if (aktuelle_spalte == rohr_ziel_spalte && aktuelle_zeile == rohr_ziel_zeile) {
      aktuelle_seiten = ROHR_SEITE_OBEN | ROHR_SEITE_RECHTS | ROHR_SEITE_UNTEN | ROHR_SEITE_LINKS;
    } else {
      aktuelle_seiten = rohr_feld[aktuelle_spalte][aktuelle_zeile].seiten;
    }

    for (int i = 0; i < 4; i++) {
      if (!(aktuelle_seiten & richtungen[i].seite_von_hier)) continue;
      if (richtungen[i].seite_von_hier == (uint8_t)von_seite) continue;

      int ns = aktuelle_spalte + richtungen[i].dx;
      int nz = aktuelle_zeile + richtungen[i].dy;
      if (ns < 0 || ns >= ROHR_SPALTEN || nz < 0 || nz >= ROHR_ZEILEN) continue;

      uint8_t nachbar_seiten;
      if (ns == rohr_start_spalte && nz == rohr_start_zeile) {
        nachbar_seiten = ROHR_SEITE_OBEN | ROHR_SEITE_RECHTS | ROHR_SEITE_UNTEN | ROHR_SEITE_LINKS;
      } else if (ns == rohr_ziel_spalte && nz == rohr_ziel_zeile) {
        nachbar_seiten = ROHR_SEITE_OBEN | ROHR_SEITE_RECHTS | ROHR_SEITE_UNTEN | ROHR_SEITE_LINKS;
      } else {
        nachbar_seiten = rohr_feld[ns][nz].seiten;
      }

      if (nachbar_seiten & richtungen[i].seite_vom_nachbar) {
        naechste_spalte = ns;
        naechste_zeile = nz;
        von_seite = richtungen[i].seite_vom_nachbar;
        break;
      }
    }

    if (naechste_spalte == -1) {
      weiterverfolgen = false;
    } else {
      rohr_fliessweg_spalte[rohr_fliessweg_laenge] = naechste_spalte;
      rohr_fliessweg_zeile[rohr_fliessweg_laenge] = naechste_zeile;
      rohr_fliessweg_laenge++;
      aktuelle_spalte = naechste_spalte;
      aktuelle_zeile = naechste_zeile;

      if (aktuelle_spalte == rohr_ziel_spalte && aktuelle_zeile == rohr_ziel_zeile) {
        weiterverfolgen = false;
      }
    }
  }

  rohr_fliessphase_aktiv = true;
  rohr_letzter_fliess_schritt = millis();
}

// ----------------------------------------------------------------------------
//  FLIESSPHASE: REGELMAESSIG IN loop() AUFRUFEN
//  Faerbt Schritt fuer Schritt die Felder entlang des Wasserwegs hellblau ein
// ----------------------------------------------------------------------------
void rohr_fliessphase_ausfuehren() {
  if (!rohr_fliessphase_aktiv) return;
  if (millis() - rohr_letzter_fliess_schritt < ROHR_FLIESS_SCHRITT_MS) return;
  rohr_letzter_fliess_schritt = millis();

  if (rohr_fliess_index >= rohr_fliessweg_laenge) {
    rohr_fliessphase_beenden();
    return;
  }

  int spalte = rohr_fliessweg_spalte[rohr_fliess_index];
  int zeile = rohr_fliessweg_zeile[rohr_fliess_index];

  // Start-/Zielfeld werden nicht als "Rohr" gezaehlt, nur echte Rohrfelder zaehlen Punkte
  bool ist_start = (spalte == rohr_start_spalte && zeile == rohr_start_zeile);
  bool ist_ziel = (spalte == rohr_ziel_spalte && zeile == rohr_ziel_zeile);

  if (!ist_start) {
    rohr_feld[spalte][zeile].vom_wasser_erreicht = true;
    rohr_feld_zeichnen(spalte, zeile);
    if (!ist_ziel) {
      rohr_punkte++;
      rohr_seitenleiste_zeichnen();
    }
  }

  rohr_fliess_index++;

  if (ist_ziel) {
    rohr_fliessphase_aktiv = false;
    rohr_level_geschafft = true;
    rohr_endanzeige_zeichnen("ZIEL ERREICHT!");
  }
}

// ----------------------------------------------------------------------------
//  FLIESSPHASE ENDET, OHNE DASS DAS ZIEL ERREICHT WURDE
// ----------------------------------------------------------------------------
void rohr_fliessphase_beenden() {
  rohr_fliessphase_aktiv = false;
  rohr_spiel_vorbei = true;
  rohr_endanzeige_zeichnen("GAME OVER");
}

// ----------------------------------------------------------------------------
//  ENDANZEIGE (GAME OVER ODER ZIEL ERREICHT) MITTIG UEBER DEM SPIELFELD
// ----------------------------------------------------------------------------
void rohr_endanzeige_zeichnen(const char* nachricht) {
  int mitte_x = ROHR_FELD_X + (ROHR_SPALTEN * ROHR_ZELLE) / 2;
  int mitte_y = ROHR_FELD_Y + (ROHR_ZEILEN * ROHR_ZELLE) / 2;

  tft.fillRect(mitte_x - 80, mitte_y - 30, 160, 60, FARBE_ROHR_HINTERGRUND);
  tft.drawRect(mitte_x - 80, mitte_y - 30, 160, 60, FARBE_ROHR_RAHMEN);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_ROHR_TEXT, FARBE_ROHR_HINTERGRUND);
  tft.drawString(nachricht, mitte_x, mitte_y - 12, 1);

  char text_punkte[20];
  snprintf(text_punkte, sizeof(text_punkte), "Punkte: %d", rohr_punkte);
  tft.drawString(text_punkte, mitte_x, mitte_y + 10, 1);
}

// ----------------------------------------------------------------------------
//  ROHR-SPIEL VERLASSEN (z.B. Home-Button gedrueckt)
// ----------------------------------------------------------------------------
void rohr_spiel_verlassen() {
  rohr_bauphase_aktiv = false;
  rohr_fliessphase_aktiv = false;
}