// ============================================================================
//  SPIEL: LIMO TYCOON (spiel_aktiv == SPIEL_TYCOON)
//  Eigene Datei: esp32-tft-tycoon.ino
//
//  Regeln:
//  - Mehrtaegiges Wirtschaftsspiel. Ablauf pro Tag:
//    1) PLANUNG: Verkaufspreis pro Becher festlegen, Wetter des Tages sehen
//    2) VERKAUF: Kunden kommen automatisch (Tempo/Anzahl abhaengig von
//       Ausbaustufe, Wetter und Preis), verbrauchen Zutaten, bringen Geld
//    3) ABSCHLUSS: Tageszettel mit Einnahmen, Verbrauch, Verderb, Kontostand
//    4) SHOP: Zutaten/Kuechengeraete/Erweiterungen kaufen, dann naechster Tag
//  - Zutaten: Eiswuerfel, Zitronen, Orangen (aktuell ungenutzt, fuer
//    spaetere Rezepte vorbereitet), Zucker, Becher - in wachsenden
//    Gebindegroessen (10er/25er/50er) kaeuflich
//  - Fehlen Zutaten waehrend des Verkaufs, gehen Kunden verloren
//  - Ausbaustufen Tisch->Wagen->Kiosk: mehr Kunden pro Tag, mehr Lagerplatz
//  - Kuechengeraete: Entsafter/Zapfanlage erhoehen den Kundendurchsatz,
//    Kuehlbox verringert den taeglichen Verderb von Eiswuerfeln/Zitronen
// ============================================================================

// ----------------------------------------------------------------------------
//  PHASEN
// ----------------------------------------------------------------------------
#define TYCOON_PHASE_PLANUNG    0
#define TYCOON_PHASE_VERKAUF    1
#define TYCOON_PHASE_ABSCHLUSS  2
#define TYCOON_PHASE_SHOP       3

int tycoon_phase = TYCOON_PHASE_PLANUNG;

// ----------------------------------------------------------------------------
//  WETTER DES TAGES (beeinflusst die Kundenzahl)
// ----------------------------------------------------------------------------
#define TYCOON_WETTER_SONNE     0
#define TYCOON_WETTER_REGEN     1
#define TYCOON_WETTER_GEWITTER  2

int tycoon_wetter_heute = TYCOON_WETTER_SONNE;

// ----------------------------------------------------------------------------
//  FARBEN
// ----------------------------------------------------------------------------
#define FARBE_TYCOON_HINTERGRUND   tft.color565(15, 25, 45)
#define FARBE_TYCOON_TEXT          tft.color565(255, 255, 255)
#define FARBE_TYCOON_RAHMEN        tft.color565(90, 90, 100)
#define FARBE_TYCOON_BUTTON        tft.color565(60, 90, 160)
#define FARBE_TYCOON_BUTTON_PLUS   tft.color565(40, 150, 60)
#define FARBE_TYCOON_BUTTON_MINUS  tft.color565(160, 50, 50)
#define FARBE_TYCOON_START         tft.color565(200, 170, 20)
#define FARBE_TYCOON_ZETTEL_BG     tft.color565(240, 235, 215)
#define FARBE_TYCOON_ZETTEL_TEXT   tft.color565(30, 30, 30)
#define FARBE_TYCOON_TAB_AKTIV     tft.color565(200, 170, 20)
#define FARBE_TYCOON_TAB_INAKTIV   tft.color565(50, 55, 70)
#define FARBE_TYCOON_KARTE         tft.color565(30, 45, 70)
#define FARBE_TYCOON_GESPERRT      tft.color565(60, 60, 65)

// ----------------------------------------------------------------------------
//  ZUTATEN (als Array, damit Verbrauch/Shop generisch per Schleife gehen)
// ----------------------------------------------------------------------------
#define TYCOON_ZUTAT_EISWUERFEL  0
#define TYCOON_ZUTAT_ZITRONEN    1
#define TYCOON_ZUTAT_ORANGEN     2
#define TYCOON_ZUTAT_ZUCKER      3
#define TYCOON_ZUTAT_BECHER      4
#define TYCOON_ANZAHL_ZUTATEN    5

const char* tycoon_zutat_namen[TYCOON_ANZAHL_ZUTATEN] = {
  "Eiswuerfel", "Zitronen", "Orangen", "Zucker", "Becher"
};

// aktueller Lagerbestand
int tycoon_bestand[TYCOON_ANZAHL_ZUTATEN] = { 20, 10, 0, 10, 10 };

// Rezept fuer EINEN Becher Limo (Orangen aktuell 0 - fuer spaetere Rezepte
// wie z.B. "Orangenlimo" vorbereitet, wird noch nicht verbraucht)
int tycoon_rezept[TYCOON_ANZAHL_ZUTATEN] = { 2, 1, 0, 1, 1 };

// Gebindegroessen (10er/25er/50er) und Preise je Zutat
int tycoon_gebinde_groesse[3] = { 10, 25, 50 };
float tycoon_zutat_preis[TYCOON_ANZAHL_ZUTATEN][3] = {
  { 1.00, 2.00, 3.50 },   // Eiswuerfel
  { 3.00, 6.50, 12.00 },  // Zitronen
  { 3.50, 7.50, 14.00 },  // Orangen
  { 1.50, 3.00, 5.50 },   // Zucker
  { 1.00, 2.00, 3.50 }    // Becher
};

// ----------------------------------------------------------------------------
//  AUSBAUSTUFEN DES STANDS: TISCH -> WAGEN -> KIOSK
// ----------------------------------------------------------------------------
#define TYCOON_ANZAHL_STANDSTUFEN 3

const char* tycoon_stand_namen[TYCOON_ANZAHL_STANDSTUFEN] = { "Tisch", "Wagen", "Kiosk" };
int   tycoon_stand_max_kunden[TYCOON_ANZAHL_STANDSTUFEN]     = { 15, 30, 50 };
int   tycoon_stand_lager_kapazitaet[TYCOON_ANZAHL_STANDSTUFEN] = { 50, 100, 200 };
float tycoon_stand_kosten[TYCOON_ANZAHL_STANDSTUFEN]          = { 0, 150, 400 };
int   tycoon_stand_freischalt_tag[TYCOON_ANZAHL_STANDSTUFEN]  = { 1, 3, 7 };

int tycoon_standstufe = 0;   // Index in obigen Arrays, Start = Tisch

// ----------------------------------------------------------------------------
//  KUECHENGERAETE
// ----------------------------------------------------------------------------
#define TYCOON_GERAET_ENTSAFTER   0
#define TYCOON_GERAET_KUEHLBOX    1
#define TYCOON_GERAET_ZAPFANLAGE  2
#define TYCOON_ANZAHL_GERAETE     3

const char* tycoon_geraet_namen[TYCOON_ANZAHL_GERAETE] = {
  "Entsafter", "Kuehlbox", "Zapfanlage"
};
const char* tycoon_geraet_beschreibung[TYCOON_ANZAHL_GERAETE] = {
  "Mehr Kunden pro Tag",
  "Weniger Verderb",
  "Mehr Kunden pro Tag"
};
float tycoon_geraet_kosten[TYCOON_ANZAHL_GERAETE]          = { 80, 100, 150 };
int   tycoon_geraet_freischalt_tag[TYCOON_ANZAHL_GERAETE]  = { 2, 4, 6 };
bool  tycoon_geraet_gekauft[TYCOON_ANZAHL_GERAETE]         = { false, false, false };

// ----------------------------------------------------------------------------
//  SPIELSTAND
// ----------------------------------------------------------------------------
float tycoon_geld = 20.0;
int   tycoon_tag = 1;
float tycoon_preis_pro_becher = 1.00;

// Tagesstatistik fuer den Abschlusszettel
float tycoon_tag_einnahmen = 0;
int   tycoon_tag_verkaufte_becher = 0;
int   tycoon_tag_verlorene_kunden = 0;
int   tycoon_tag_verbrauch[TYCOON_ANZAHL_ZUTATEN];
int   tycoon_tag_verderb_eiswuerfel = 0;
int   tycoon_tag_verderb_zitronen = 0;

// Verkaufsphasen-Ablauf
#define TYCOON_VERKAUF_DAUER_MS  20000   // ein Verkaufstag dauert 20 Sekunden
#define TYCOON_VERKAUF_TICK_MS     800   // alle 800ms eine Kundenwelle
unsigned long tycoon_verkauf_start = 0;
unsigned long tycoon_letzter_tick = 0;
int tycoon_tag_kunden_gesamt = 0;
int tycoon_tag_kunden_bearbeitet = 0;
int tycoon_kunden_pro_tick = 1;

// Shop
#define TYCOON_SHOP_TAB_ZUTATEN      0
#define TYCOON_SHOP_TAB_GERAETE      1
#define TYCOON_SHOP_TAB_ERWEITERUNG  2
int tycoon_shop_tab = TYCOON_SHOP_TAB_ZUTATEN;

// Layout-Konstanten Shop
#define TYCOON_SHOP_TAB_BREITE   64
#define TYCOON_SHOP_INHALT_X     (TYCOON_SHOP_TAB_BREITE + 6)
#define TYCOON_SHOP_ZEILE_HOEHE  40

// kleine Statusmeldung unten im Shop (z.B. "Nicht genug Geld")
char tycoon_shop_meldung[32] = "";
unsigned long tycoon_shop_meldung_zeit = 0;
#define TYCOON_SHOP_MELDUNG_DAUER_MS 1500

// ----------------------------------------------------------------------------
//  HILFSFUNKTION: TOUCH-PUNKT IN RECHTECK?
// ----------------------------------------------------------------------------
bool tycoon_touch_in_rect(int x, int y, int w, int h) {
  return touch_x >= x && touch_x <= x + w && touch_y >= y && touch_y <= y + h;
}

// ----------------------------------------------------------------------------
//  WETTER-HILFSFUNKTIONEN
// ----------------------------------------------------------------------------
const char* tycoon_wettertext(int wetter) {
  if (wetter == TYCOON_WETTER_SONNE) return "Sonnig";
  if (wetter == TYCOON_WETTER_REGEN) return "Regen";
  return "Gewitter";
}

uint16_t tycoon_wetterfarbe(int wetter) {
  if (wetter == TYCOON_WETTER_SONNE) return tft.color565(255, 200, 0);
  if (wetter == TYCOON_WETTER_REGEN) return tft.color565(70, 150, 255);
  return tft.color565(255, 230, 0);
}

// einfaches gezeichnetes Wettersymbol (Sonne / Regenwolke / Gewitterwolke)
void tycoon_wettersymbol_zeichnen(int mx, int my, int radius, int wetter, uint16_t farbe) {
  if (wetter == TYCOON_WETTER_SONNE) {
    tft.fillCircle(mx, my, radius / 2, farbe);
    for (int winkel = 0; winkel < 360; winkel += 45) {
      float rad = winkel * 3.14159 / 180.0;
      int x1 = mx + cos(rad) * (radius / 2 + 3);
      int y1 = my + sin(rad) * (radius / 2 + 3);
      int x2 = mx + cos(rad) * (radius + 3);
      int y2 = my + sin(rad) * (radius + 3);
      tft.drawLine(x1, y1, x2, y2, farbe);
    }
  } else if (wetter == TYCOON_WETTER_REGEN) {
    tft.fillRoundRect(mx - radius, my - radius / 3, radius * 2, radius / 2, 8, tft.color565(200, 200, 200));
    for (int i = -1; i <= 1; i++) {
      tft.drawLine(mx + i * 10, my + radius / 4, mx + i * 10 - 4, my + radius, farbe);
    }
  } else {
    // Gewitter: Wolke + Blitz
    tft.fillRoundRect(mx - radius, my - radius / 3, radius * 2, radius / 2, 8, tft.color565(160, 160, 170));
    tft.fillTriangle(mx - 4, my + radius / 4, mx + 6, my + radius / 4, mx - 2, my + radius, farbe);
  }
}

// ----------------------------------------------------------------------------
//  SPIEL STARTEN (erster Aufruf, Tag 1)
// ----------------------------------------------------------------------------
void tycoon_spiel_starten() {
  tycoon_geld = 20.0;
  tycoon_tag = 1;
  tycoon_preis_pro_becher = 1.00;
  tycoon_standstufe = 0;

  int start_bestand[TYCOON_ANZAHL_ZUTATEN] = { 20, 10, 0, 10, 10 };
  for (int i = 0; i < TYCOON_ANZAHL_ZUTATEN; i++) tycoon_bestand[i] = start_bestand[i];

  for (int i = 0; i < TYCOON_ANZAHL_GERAETE; i++) tycoon_geraet_gekauft[i] = false;

  tycoon_wetter_heute = random(0, 3);
  tycoon_phase = TYCOON_PHASE_PLANUNG;
  tycoon_planung_zeichnen();
}

// ----------------------------------------------------------------------------
//  NEUEN TAG BEGINNEN (aus dem Shop heraus aufgerufen)
// ----------------------------------------------------------------------------
void tycoon_naechster_tag_starten() {
  tycoon_tag++;
  tycoon_wetter_heute = random(0, 3);
  tycoon_phase = TYCOON_PHASE_PLANUNG;
  tycoon_planung_zeichnen();
}

// ============================================================================
//  PHASE 1: PLANUNG
// ============================================================================
void tycoon_planung_zeichnen() {
  tft.fillScreen(FARBE_TYCOON_HINTERGRUND);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TYCOON_TEXT, FARBE_TYCOON_HINTERGRUND);

  char titel[28];
  snprintf(titel, sizeof(titel), "Tag %d - Planung", tycoon_tag);
  tft.drawString(titel, SCREEN_W / 2, HEADER_HOEHE + 14, 2);

  // Wetter des Tages, links
  uint16_t wf = tycoon_wetterfarbe(tycoon_wetter_heute);
  tycoon_wettersymbol_zeichnen(55, HEADER_HOEHE + 55, 20, tycoon_wetter_heute, wf);
  tft.setTextColor(FARBE_TYCOON_TEXT, FARBE_TYCOON_HINTERGRUND);
  tft.drawString(tycoon_wettertext(tycoon_wetter_heute), 55, HEADER_HOEHE + 82, 1);

  // Kasse + Standstufe, rechts
  tft.setTextDatum(TR_DATUM);
  char geld_text[24];
  snprintf(geld_text, sizeof(geld_text), "Kasse: %.2f EUR", tycoon_geld);
  tft.drawString(geld_text, SCREEN_W - 10, HEADER_HOEHE + 32, 1);
  char stand_text[24];
  snprintf(stand_text, sizeof(stand_text), "Stand: %s", tycoon_stand_namen[tycoon_standstufe]);
  tft.drawString(stand_text, SCREEN_W - 10, HEADER_HOEHE + 48, 1);

  // Bestandsuebersicht, rechts darunter
  int by = HEADER_HOEHE + 68;
  for (int i = 0; i < TYCOON_ANZAHL_ZUTATEN; i++) {
    if (i == TYCOON_ZUTAT_ORANGEN) continue;  // aktuell ungenutzt, nicht anzeigen
    char zeile[24];
    snprintf(zeile, sizeof(zeile), "%s: %d", tycoon_zutat_namen[i], tycoon_bestand[i]);
    tft.drawString(zeile, SCREEN_W - 10, by, 1);
    by += 13;
  }

  // Preisbereich
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Verkaufspreis pro Becher:", SCREEN_W / 2, HEADER_HOEHE + 125, 1);

  tft.fillRoundRect(60, HEADER_HOEHE + 140, 40, 40, 6, FARBE_TYCOON_BUTTON_MINUS);
  tft.drawString("-", 80, HEADER_HOEHE + 160, 4);
  tft.fillRoundRect(220, HEADER_HOEHE + 140, 40, 40, 6, FARBE_TYCOON_BUTTON_PLUS);
  tft.drawString("+", 240, HEADER_HOEHE + 160, 4);

  char preis_text[16];
  snprintf(preis_text, sizeof(preis_text), "%.2f EUR", tycoon_preis_pro_becher);
  tft.fillRect(110, HEADER_HOEHE + 140, 100, 40, FARBE_TYCOON_HINTERGRUND);
  tft.drawString(preis_text, SCREEN_W / 2, HEADER_HOEHE + 160, 4);

  // Buttons unten: Shop / Verkauf starten
  tft.fillRoundRect(20, HEADER_HOEHE + 190, 130, 32, 8, FARBE_TYCOON_BUTTON);
  tft.drawString("Shop", 85, HEADER_HOEHE + 206, 2);

  tft.fillRoundRect(170, HEADER_HOEHE + 190, 130, 32, 8, FARBE_TYCOON_START);
  tft.setTextColor(tft.color565(30, 30, 30), FARBE_TYCOON_START);
  tft.drawString("Verkauf starten", 235, HEADER_HOEHE + 206, 1);
}

void tycoon_planung_touch_behandeln() {
  // Preis minus
  if (tycoon_touch_in_rect(60, HEADER_HOEHE + 140, 40, 40)) {
    tycoon_preis_pro_becher -= 0.10;
    if (tycoon_preis_pro_becher < 0.10) tycoon_preis_pro_becher = 0.10;
    tycoon_planung_zeichnen();
    return;
  }
  // Preis plus
  if (tycoon_touch_in_rect(220, HEADER_HOEHE + 140, 40, 40)) {
    tycoon_preis_pro_becher += 0.10;
    if (tycoon_preis_pro_becher > 5.00) tycoon_preis_pro_becher = 5.00;
    tycoon_planung_zeichnen();
    return;
  }
  // Shop-Button
  if (tycoon_touch_in_rect(20, HEADER_HOEHE + 190, 130, 32)) {
    tycoon_phase = TYCOON_PHASE_SHOP;
    tycoon_shop_tab = TYCOON_SHOP_TAB_ZUTATEN;
    tycoon_shop_zeichnen();
    return;
  }
  // Verkauf starten
  if (tycoon_touch_in_rect(170, HEADER_HOEHE + 190, 130, 32)) {
    tycoon_verkauf_starten();
    return;
  }
}

// ============================================================================
//  PHASE 2: VERKAUF
// ============================================================================
void tycoon_verkauf_starten() {
  // Basis-Kundenzahl anhand Standstufe
  int basis_kunden = tycoon_stand_max_kunden[tycoon_standstufe];

  // Geraete-Bonus: Entsafter/Zapfanlage erhoehen den Durchsatz
  float geraete_faktor = 1.0;
  if (tycoon_geraet_gekauft[TYCOON_GERAET_ENTSAFTER])  geraete_faktor += 0.15;
  if (tycoon_geraet_gekauft[TYCOON_GERAET_ZAPFANLAGE]) geraete_faktor += 0.25;

  // Wetter-Einfluss
  float wetter_faktor = 1.0;
  if (tycoon_wetter_heute == TYCOON_WETTER_REGEN) wetter_faktor = 0.6;
  else if (tycoon_wetter_heute == TYCOON_WETTER_GEWITTER) wetter_faktor = 0.35;

  // Preis-Einfluss: teurer schreckt ab, guenstiger lockt (Referenz 1.00 EUR)
  float preis_faktor = 1.0 / tycoon_preis_pro_becher;
  if (preis_faktor > 1.6) preis_faktor = 1.6;
  if (preis_faktor < 0.3) preis_faktor = 0.3;

  tycoon_tag_kunden_gesamt = (int)(basis_kunden * geraete_faktor * wetter_faktor * preis_faktor);
  if (tycoon_tag_kunden_gesamt < 1) tycoon_tag_kunden_gesamt = 1;

  int anzahl_ticks = TYCOON_VERKAUF_DAUER_MS / TYCOON_VERKAUF_TICK_MS;
  tycoon_kunden_pro_tick = tycoon_tag_kunden_gesamt / anzahl_ticks;
  if (tycoon_kunden_pro_tick < 1) tycoon_kunden_pro_tick = 1;

  tycoon_tag_kunden_bearbeitet = 0;
  tycoon_tag_einnahmen = 0;
  tycoon_tag_verkaufte_becher = 0;
  tycoon_tag_verlorene_kunden = 0;
  for (int i = 0; i < TYCOON_ANZAHL_ZUTATEN; i++) tycoon_tag_verbrauch[i] = 0;

  tycoon_verkauf_start = millis();
  tycoon_letzter_tick = millis();
  tycoon_phase = TYCOON_PHASE_VERKAUF;

  tycoon_verkauf_zeichnen_basis();
  tycoon_verkauf_werte_aktualisieren();
}

void tycoon_verkauf_zeichnen_basis() {
  tft.fillScreen(FARBE_TYCOON_HINTERGRUND);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TYCOON_TEXT, FARBE_TYCOON_HINTERGRUND);

  char titel[28];
  snprintf(titel, sizeof(titel), "Tag %d - Verkauf laeuft", tycoon_tag);
  tft.drawString(titel, SCREEN_W / 2, HEADER_HOEHE + 16, 2);

  uint16_t wf = tycoon_wetterfarbe(tycoon_wetter_heute);
  tycoon_wettersymbol_zeichnen(SCREEN_W / 2, HEADER_HOEHE + 50, 16, tycoon_wetter_heute, wf);

  // Fortschrittsbalken-Rahmen
  tft.drawRoundRect(30, HEADER_HOEHE + 80, SCREEN_W - 60, 16, 4, FARBE_TYCOON_RAHMEN);
}

void tycoon_verkauf_werte_aktualisieren() {
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TYCOON_TEXT, FARBE_TYCOON_HINTERGRUND);

  // Fortschrittsbalken fuellen
  unsigned long vergangen = millis() - tycoon_verkauf_start;
  if (vergangen > TYCOON_VERKAUF_DAUER_MS) vergangen = TYCOON_VERKAUF_DAUER_MS;
  int fuell_breite = (int)((SCREEN_W - 64) * vergangen / TYCOON_VERKAUF_DAUER_MS);
  tft.fillRect(32, HEADER_HOEHE + 82, SCREEN_W - 64, 12, FARBE_TYCOON_HINTERGRUND);
  tft.fillRect(32, HEADER_HOEHE + 82, fuell_breite, 12, FARBE_TYCOON_START);

  // Kennzahlen
  tft.fillRect(20, HEADER_HOEHE + 105, SCREEN_W - 40, 90, FARBE_TYCOON_HINTERGRUND);

  char zeile1[28];
  snprintf(zeile1, sizeof(zeile1), "Verkaufte Becher: %d", tycoon_tag_verkaufte_becher);
  tft.drawString(zeile1, SCREEN_W / 2, HEADER_HOEHE + 118, 1);

  char zeile2[28];
  snprintf(zeile2, sizeof(zeile2), "Einnahmen: %.2f EUR", tycoon_tag_einnahmen);
  tft.drawString(zeile2, SCREEN_W / 2, HEADER_HOEHE + 138, 1);

  char zeile3[28];
  snprintf(zeile3, sizeof(zeile3), "Verlorene Kunden: %d", tycoon_tag_verlorene_kunden);
  tft.drawString(zeile3, SCREEN_W / 2, HEADER_HOEHE + 158, 1);

  bool zutaten_leer = false;
  for (int i = 0; i < TYCOON_ANZAHL_ZUTATEN; i++) {
    if (tycoon_rezept[i] > 0 && tycoon_bestand[i] < tycoon_rezept[i]) { zutaten_leer = true; break; }
  }
  if (zutaten_leer) {
    tft.setTextColor(tft.color565(255, 90, 90), FARBE_TYCOON_HINTERGRUND);
    tft.drawString("Zutaten werden knapp!", SCREEN_W / 2, HEADER_HOEHE + 182, 1);
    tft.setTextColor(FARBE_TYCOON_TEXT, FARBE_TYCOON_HINTERGRUND);
  }
}

// Regelmaessig in loop() aufrufen, wenn spiel_aktiv == SPIEL_TYCOON und
// tycoon_phase == TYCOON_PHASE_VERKAUF
void tycoon_verkauf_tick_ausfuehren() {
  if (tycoon_phase != TYCOON_PHASE_VERKAUF) return;
  if (millis() - tycoon_letzter_tick < TYCOON_VERKAUF_TICK_MS) return;
  tycoon_letzter_tick = millis();

  int diese_charge = tycoon_kunden_pro_tick;
  for (int i = 0; i < diese_charge; i++) {
    if (tycoon_tag_kunden_bearbeitet >= tycoon_tag_kunden_gesamt) break;
    tycoon_tag_kunden_bearbeitet++;

    bool genug_zutaten = true;
    for (int z = 0; z < TYCOON_ANZAHL_ZUTATEN; z++) {
      if (tycoon_bestand[z] < tycoon_rezept[z]) { genug_zutaten = false; break; }
    }

    if (!genug_zutaten) {
      tycoon_tag_verlorene_kunden++;
      continue;
    }

    for (int z = 0; z < TYCOON_ANZAHL_ZUTATEN; z++) {
      tycoon_bestand[z] -= tycoon_rezept[z];
      tycoon_tag_verbrauch[z] += tycoon_rezept[z];
    }
    tycoon_tag_verkaufte_becher++;
    tycoon_tag_einnahmen += tycoon_preis_pro_becher;
    tycoon_geld += tycoon_preis_pro_becher;
  }

  tycoon_verkauf_werte_aktualisieren();

  bool zeit_um = (millis() - tycoon_verkauf_start >= TYCOON_VERKAUF_DAUER_MS);
  bool alle_kunden_fertig = (tycoon_tag_kunden_bearbeitet >= tycoon_tag_kunden_gesamt);
  if (zeit_um || alle_kunden_fertig) {
    tycoon_verkauf_beenden();
  }
}

void tycoon_verkauf_beenden() {
  // Verderb am Tagesende: ein Teil von Eiswuerfeln/Zitronen wird schlecht,
  // die Kuehlbox reduziert diesen Anteil deutlich
  float verderb_rate = tycoon_geraet_gekauft[TYCOON_GERAET_KUEHLBOX] ? 0.05 : 0.15;

  tycoon_tag_verderb_eiswuerfel = (int)(tycoon_bestand[TYCOON_ZUTAT_EISWUERFEL] * verderb_rate);
  tycoon_bestand[TYCOON_ZUTAT_EISWUERFEL] -= tycoon_tag_verderb_eiswuerfel;

  tycoon_tag_verderb_zitronen = (int)(tycoon_bestand[TYCOON_ZUTAT_ZITRONEN] * verderb_rate);
  tycoon_bestand[TYCOON_ZUTAT_ZITRONEN] -= tycoon_tag_verderb_zitronen;

  tycoon_phase = TYCOON_PHASE_ABSCHLUSS;
  tycoon_abschluss_zeichnen();
}

// ============================================================================
//  PHASE 3: TAGESABSCHLUSS-ZETTEL
// ============================================================================
void tycoon_abschluss_zeichnen() {
  tft.fillScreen(FARBE_TYCOON_HINTERGRUND);

  int zx = 30, zy = HEADER_HOEHE + 8, zw = SCREEN_W - 60, zh = SCREEN_H - HEADER_HOEHE - 16;
  tft.fillRoundRect(zx, zy, zw, zh, 6, FARBE_TYCOON_ZETTEL_BG);
  tft.drawRoundRect(zx, zy, zw, zh, 6, FARBE_TYCOON_RAHMEN);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TYCOON_ZETTEL_TEXT, FARBE_TYCOON_ZETTEL_BG);

  char titel[24];
  snprintf(titel, sizeof(titel), "Abschluss Tag %d", tycoon_tag);
  tft.drawString(titel, SCREEN_W / 2, zy + 16, 2);

  uint16_t wf = tycoon_wetterfarbe(tycoon_wetter_heute);
  tycoon_wettersymbol_zeichnen(zx + 30, zy + 42, 14, tycoon_wetter_heute, wf);
  tft.setTextColor(FARBE_TYCOON_ZETTEL_TEXT, FARBE_TYCOON_ZETTEL_BG);
  tft.setTextDatum(TL_DATUM);

  int zeile_y = zy + 36;
  int zeile_hoehe = 14;
  int text_x = zx + 55;

  char puffer[40];

  snprintf(puffer, sizeof(puffer), "Verkauft: %d Becher", tycoon_tag_verkaufte_becher);
  tft.drawString(puffer, text_x, zeile_y, 1); zeile_y += zeile_hoehe;

  snprintf(puffer, sizeof(puffer), "Einnahmen: %.2f EUR", tycoon_tag_einnahmen);
  tft.drawString(puffer, text_x, zeile_y, 1); zeile_y += zeile_hoehe;

  if (tycoon_tag_verlorene_kunden > 0) {
    snprintf(puffer, sizeof(puffer), "Verlorene Kunden: %d", tycoon_tag_verlorene_kunden);
    tft.drawString(puffer, text_x, zeile_y, 1); zeile_y += zeile_hoehe;
  }

  zeile_y += 4;
  tft.drawString("Verbrauch:", zx + 15, zeile_y, 1); zeile_y += zeile_hoehe;
  for (int i = 0; i < TYCOON_ANZAHL_ZUTATEN; i++) {
    if (tycoon_rezept[i] == 0) continue;
    snprintf(puffer, sizeof(puffer), "  %s: %d", tycoon_zutat_namen[i], tycoon_tag_verbrauch[i]);
    tft.drawString(puffer, zx + 15, zeile_y, 1); zeile_y += zeile_hoehe;
  }

  if (tycoon_tag_verderb_eiswuerfel > 0 || tycoon_tag_verderb_zitronen > 0) {
    zeile_y += 4;
    snprintf(puffer, sizeof(puffer), "Verderb: %d Eis, %d Zitronen",
             tycoon_tag_verderb_eiswuerfel, tycoon_tag_verderb_zitronen);
    tft.drawString(puffer, zx + 15, zeile_y, 1); zeile_y += zeile_hoehe;
  }

  zeile_y += 8;
  tft.setTextDatum(MC_DATUM);
  snprintf(puffer, sizeof(puffer), "Kontostand: %.2f EUR", tycoon_geld);
  tft.drawString(puffer, SCREEN_W / 2, zeile_y, 2);

  tft.drawString("Antippen fuer den Shop", SCREEN_W / 2, zy + zh - 14, 1);
}

void tycoon_abschluss_touch_behandeln() {
  tycoon_phase = TYCOON_PHASE_SHOP;
  tycoon_shop_tab = TYCOON_SHOP_TAB_ZUTATEN;
  tycoon_shop_zeichnen();
}

// ============================================================================
//  PHASE 4: SHOP (Reiter Zutaten / Kuechengeraete / Erweiterungen)
// ============================================================================
void tycoon_shop_zeichnen() {
  tft.fillScreen(FARBE_TYCOON_HINTERGRUND);

  // Kopfzeile
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TYCOON_TEXT, FARBE_TYCOON_HINTERGRUND);
  char titel[24];
  snprintf(titel, sizeof(titel), "Shop - Tag %d", tycoon_tag);
  tft.drawString(titel, SCREEN_W / 2, 6, 1);
  char geld_text[20];
  snprintf(geld_text, sizeof(geld_text), "%.2f EUR", tycoon_geld);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(geld_text, SCREEN_W - 6, HEADER_HOEHE + 4, 1);

  // linke Reiterleiste
  const char* tab_namen[3] = { "Zutaten", "Geraete", "Ausbau" };
  int tab_hoehe = (SCREEN_H - HEADER_HOEHE - 20) / 3;
  for (int i = 0; i < 3; i++) {
    int ty = HEADER_HOEHE + 20 + i * tab_hoehe;
    uint16_t farbe = (i == tycoon_shop_tab) ? FARBE_TYCOON_TAB_AKTIV : FARBE_TYCOON_TAB_INAKTIV;
    tft.fillRect(0, ty, TYCOON_SHOP_TAB_BREITE, tab_hoehe - 2, farbe);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(FARBE_TYCOON_TEXT, farbe);
    tft.drawString(tab_namen[i], TYCOON_SHOP_TAB_BREITE / 2, ty + tab_hoehe / 2, 1);
  }

  // Inhalt je Reiter
  if (tycoon_shop_tab == TYCOON_SHOP_TAB_ZUTATEN) {
    tycoon_shop_zutaten_zeichnen();
  } else if (tycoon_shop_tab == TYCOON_SHOP_TAB_GERAETE) {
    tycoon_shop_geraete_zeichnen();
  } else {
    tycoon_shop_erweiterung_zeichnen();
  }

  // Naechster-Tag-Button unten rechts
  tft.fillRoundRect(SCREEN_W - 110, SCREEN_H - 28, 106, 24, 6, FARBE_TYCOON_START);
  tft.setTextColor(tft.color565(30, 30, 30), FARBE_TYCOON_START);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Naechster Tag", SCREEN_W - 57, SCREEN_H - 16, 1);
}

void tycoon_shop_zutaten_zeichnen() {
  int x = TYCOON_SHOP_INHALT_X;
  int y = HEADER_HOEHE + 18;

  for (int i = 0; i < TYCOON_ANZAHL_ZUTATEN; i++) {
    int ry = y + i * TYCOON_SHOP_ZEILE_HOEHE;

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(FARBE_TYCOON_TEXT, FARBE_TYCOON_HINTERGRUND);
    char zeile[24];
    snprintf(zeile, sizeof(zeile), "%s: %d", tycoon_zutat_namen[i], tycoon_bestand[i]);
    tft.drawString(zeile, x, ry, 1);

    for (int g = 0; g < 3; g++) {
      int bx = x + g * 50;
      int by = ry + 16;
      tft.fillRoundRect(bx, by, 44, 16, 3, FARBE_TYCOON_BUTTON);
      tft.setTextDatum(MC_DATUM);
      char btn_text[10];
      snprintf(btn_text, sizeof(btn_text), "+%d", tycoon_gebinde_groesse[g]);
      tft.drawString(btn_text, bx + 22, by + 8, 1);
    }
  }

  tycoon_shop_meldung_zeichnen();
}

void tycoon_shop_geraete_zeichnen() {
  int x = TYCOON_SHOP_INHALT_X;
  int y = HEADER_HOEHE + 18;
  int zeile_hoehe = 58;

  for (int i = 0; i < TYCOON_ANZAHL_GERAETE; i++) {
    int ry = y + i * zeile_hoehe;
    bool freigeschaltet = (tycoon_tag >= tycoon_geraet_freischalt_tag[i]);

    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(FARBE_TYCOON_TEXT, FARBE_TYCOON_HINTERGRUND);
    tft.drawString(tycoon_geraet_namen[i], x, ry, 1);
    tft.setTextColor(tft.color565(180, 190, 210), FARBE_TYCOON_HINTERGRUND);
    tft.drawString(tycoon_geraet_beschreibung[i], x, ry + 12, 1);

    int bw = 150, bh = 22;
    uint16_t btn_farbe;
    char btn_text[24];

    if (tycoon_geraet_gekauft[i]) {
      btn_farbe = FARBE_TYCOON_GESPERRT;
      snprintf(btn_text, sizeof(btn_text), "Gekauft");
    } else if (!freigeschaltet) {
      btn_farbe = FARBE_TYCOON_GESPERRT;
      snprintf(btn_text, sizeof(btn_text), "Ab Tag %d", tycoon_geraet_freischalt_tag[i]);
    } else {
      btn_farbe = FARBE_TYCOON_BUTTON;
      snprintf(btn_text, sizeof(btn_text), "Kaufen: %.0f EUR", tycoon_geraet_kosten[i]);
    }

    tft.fillRoundRect(x, ry + 26, bw, bh, 5, btn_farbe);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(FARBE_TYCOON_TEXT, btn_farbe);
    tft.drawString(btn_text, x + bw / 2, ry + 26 + bh / 2, 1);
  }

  tycoon_shop_meldung_zeichnen();
}

void tycoon_shop_erweiterung_zeichnen() {
  int x = TYCOON_SHOP_INHALT_X;
  int y = HEADER_HOEHE + 18;

  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(FARBE_TYCOON_TEXT, FARBE_TYCOON_HINTERGRUND);
  char aktuell[32];
  snprintf(aktuell, sizeof(aktuell), "Aktuell: %s", tycoon_stand_namen[tycoon_standstufe]);
  tft.drawString(aktuell, x, y, 1);
  char kapazitaet[32];
  snprintf(kapazitaet, sizeof(kapazitaet), "Kunden/Tag: %d  Lager: %d",
           tycoon_stand_max_kunden[tycoon_standstufe],
           tycoon_stand_lager_kapazitaet[tycoon_standstufe]);
  tft.drawString(kapazitaet, x, y + 14, 1);

  if (tycoon_standstufe >= TYCOON_ANZAHL_STANDSTUFEN - 1) {
    tft.drawString("Maximale Ausbaustufe erreicht.", x, y + 40, 1);
    return;
  }

  int naechste = tycoon_standstufe + 1;
  bool freigeschaltet = (tycoon_tag >= tycoon_stand_freischalt_tag[naechste]);

  char naechste_text[32];
  snprintf(naechste_text, sizeof(naechste_text), "Naechste Stufe: %s", tycoon_stand_namen[naechste]);
  tft.drawString(naechste_text, x, y + 40, 1);
  char naechste_werte[40];
  snprintf(naechste_werte, sizeof(naechste_werte), "Kunden/Tag: %d  Lager: %d",
           tycoon_stand_max_kunden[naechste], tycoon_stand_lager_kapazitaet[naechste]);
  tft.drawString(naechste_werte, x, y + 54, 1);

  uint16_t btn_farbe;
  char btn_text[24];
  if (!freigeschaltet) {
    btn_farbe = FARBE_TYCOON_GESPERRT;
    snprintf(btn_text, sizeof(btn_text), "Ab Tag %d", tycoon_stand_freischalt_tag[naechste]);
  } else {
    btn_farbe = FARBE_TYCOON_BUTTON;
    snprintf(btn_text, sizeof(btn_text), "Kaufen: %.0f EUR", tycoon_stand_kosten[naechste]);
  }

  tft.fillRoundRect(x, y + 74, 170, 26, 5, btn_farbe);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(FARBE_TYCOON_TEXT, btn_farbe);
  tft.drawString(btn_text, x + 85, y + 87, 1);

  tycoon_shop_meldung_zeichnen();
}

void tycoon_shop_meldung_setzen(const char* text) {
  strncpy(tycoon_shop_meldung, text, sizeof(tycoon_shop_meldung) - 1);
  tycoon_shop_meldung[sizeof(tycoon_shop_meldung) - 1] = '\0';
  tycoon_shop_meldung_zeit = millis();
}

void tycoon_shop_meldung_zeichnen() {
  if (tycoon_shop_meldung[0] == '\0') return;
  if (millis() - tycoon_shop_meldung_zeit > TYCOON_SHOP_MELDUNG_DAUER_MS) {
    tycoon_shop_meldung[0] = '\0';
    return;
  }
  tft.setTextDatum(BL_DATUM);
  tft.setTextColor(tft.color565(255, 120, 120), FARBE_TYCOON_HINTERGRUND);
  tft.drawString(tycoon_shop_meldung, TYCOON_SHOP_INHALT_X, SCREEN_H - 10, 1);
}

// ----------------------------------------------------------------------------
//  SHOP TOUCH-BEHANDLUNG
// ----------------------------------------------------------------------------
void tycoon_shop_touch_behandeln() {
  // Reiter links
  int tab_hoehe = (SCREEN_H - HEADER_HOEHE - 20) / 3;
  for (int i = 0; i < 3; i++) {
    int ty = HEADER_HOEHE + 20 + i * tab_hoehe;
    if (tycoon_touch_in_rect(0, ty, TYCOON_SHOP_TAB_BREITE, tab_hoehe - 2)) {
      tycoon_shop_tab = i;
      tycoon_shop_meldung[0] = '\0';
      tycoon_shop_zeichnen();
      return;
    }
  }

  // Naechster-Tag-Button
  if (tycoon_touch_in_rect(SCREEN_W - 110, SCREEN_H - 28, 106, 24)) {
    tycoon_naechster_tag_starten();
    return;
  }

  if (tycoon_shop_tab == TYCOON_SHOP_TAB_ZUTATEN) {
    tycoon_shop_zutaten_touch_behandeln();
  } else if (tycoon_shop_tab == TYCOON_SHOP_TAB_GERAETE) {
    tycoon_shop_geraete_touch_behandeln();
  } else {
    tycoon_shop_erweiterung_touch_behandeln();
  }
}

void tycoon_shop_zutaten_touch_behandeln() {
  int x = TYCOON_SHOP_INHALT_X;
  int y = HEADER_HOEHE + 18;
  int kapazitaet = tycoon_stand_lager_kapazitaet[tycoon_standstufe];

  for (int i = 0; i < TYCOON_ANZAHL_ZUTATEN; i++) {
    int ry = y + i * TYCOON_SHOP_ZEILE_HOEHE;
    for (int g = 0; g < 3; g++) {
      int bx = x + g * 50;
      int by = ry + 16;
      if (tycoon_touch_in_rect(bx, by, 44, 16)) {
        int menge = tycoon_gebinde_groesse[g];
        float preis = tycoon_zutat_preis[i][g];

        if (tycoon_geld < preis) {
          tycoon_shop_meldung_setzen("Nicht genug Geld!");
        } else if (tycoon_bestand[i] + menge > kapazitaet) {
          tycoon_shop_meldung_setzen("Lager ist voll!");
        } else {
          tycoon_geld -= preis;
          tycoon_bestand[i] += menge;
        }
        tycoon_shop_zeichnen();
        return;
      }
    }
  }
}

void tycoon_shop_geraete_touch_behandeln() {
  int x = TYCOON_SHOP_INHALT_X;
  int y = HEADER_HOEHE + 18;
  int zeile_hoehe = 58;

  for (int i = 0; i < TYCOON_ANZAHL_GERAETE; i++) {
    int ry = y + i * zeile_hoehe;
    if (tycoon_touch_in_rect(x, ry + 26, 150, 22)) {
      if (tycoon_geraet_gekauft[i]) return;
      if (tycoon_tag < tycoon_geraet_freischalt_tag[i]) {
        tycoon_shop_meldung_setzen("Noch nicht freigeschaltet!");
      } else if (tycoon_geld < tycoon_geraet_kosten[i]) {
        tycoon_shop_meldung_setzen("Nicht genug Geld!");
      } else {
        tycoon_geld -= tycoon_geraet_kosten[i];
        tycoon_geraet_gekauft[i] = true;
      }
      tycoon_shop_zeichnen();
      return;
    }
  }
}

void tycoon_shop_erweiterung_touch_behandeln() {
  if (tycoon_standstufe >= TYCOON_ANZAHL_STANDSTUFEN - 1) return;

  int x = TYCOON_SHOP_INHALT_X;
  int y = HEADER_HOEHE + 18;

  if (tycoon_touch_in_rect(x, y + 74, 170, 26)) {
    int naechste = tycoon_standstufe + 1;
    if (tycoon_tag < tycoon_stand_freischalt_tag[naechste]) {
      tycoon_shop_meldung_setzen("Noch nicht freigeschaltet!");
    } else if (tycoon_geld < tycoon_stand_kosten[naechste]) {
      tycoon_shop_meldung_setzen("Nicht genug Geld!");
    } else {
      tycoon_geld -= tycoon_stand_kosten[naechste];
      tycoon_standstufe = naechste;
    }
    tycoon_shop_zeichnen();
  }
}

// ============================================================================
//  ZENTRALE TOUCH-WEITERLEITUNG (aus touch_abfragen() aufrufen, wenn
//  spiel_aktiv == SPIEL_TYCOON)
// ============================================================================
void tycoon_touch_behandeln() {
  if (tycoon_phase == TYCOON_PHASE_PLANUNG) {
    tycoon_planung_touch_behandeln();
  } else if (tycoon_phase == TYCOON_PHASE_VERKAUF) {
    // waehrend des Verkaufs gibt es nichts anzutippen, Tag laeuft automatisch
  } else if (tycoon_phase == TYCOON_PHASE_ABSCHLUSS) {
    tycoon_abschluss_touch_behandeln();
  } else if (tycoon_phase == TYCOON_PHASE_SHOP) {
    tycoon_shop_touch_behandeln();
  }
}

// ============================================================================
//  TYCOON VERLASSEN (z.B. Home-Button gedrueckt)
// ============================================================================
void tycoon_spiel_verlassen() {
  // aktuell kein laufender Timer, der zwingend gestoppt werden muesste -
  // der Spielstand bleibt einfach im Speicher erhalten, bis man zurueckkehrt
}