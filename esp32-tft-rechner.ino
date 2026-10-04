// ============================================================================
//  PROGRAMM: TASCHENRECHNER (prog_aktiv == PROG_RECHNER)
//  Grundrechenarten, Vorzeichen, Dezimalpunkt. Ergebnis max. 10 Stellen.
// ============================================================================

#define RECHNER_ZEILEN 5
#define RECHNER_SPALTEN 4

const char* RECHNER_TASTEN[RECHNER_ZEILEN][RECHNER_SPALTEN] = {
  { "C",  "CE", "<-", "/" },
  { "7",  "8", "9", "*" },
  { "4",  "5", "6", "-" },
  { "1",  "2", "3", "+" },
  { "+/-","0", ".", "=" }
};

int rechner_btn_x[RECHNER_ZEILEN][RECHNER_SPALTEN];
int rechner_btn_y[RECHNER_ZEILEN][RECHNER_SPALTEN];
int rechner_btn_w = 72;
int rechner_btn_h = 32;

String rechner_anzeige = "0";
double rechner_akku = 0;
char rechner_op = 0;
bool rechner_neue_zahl = true;
bool rechner_error = false;

String rechner_format(double v) {
  if (isnan(v) || isinf(v)) {
    return "Error";
  }
  char buf[16];
  dtostrf(v, 1, 6, buf);
  String s = String(buf);
  s.trim();
  int komma = s.indexOf('.');
  if (komma >= 0) {
    while (s.length() > 1 && s.endsWith("0")) {
      s.remove(s.length() - 1);
    }
    if (s.endsWith(".")) {
      s.remove(s.length() - 1);
    }
  }
  if (s.length() > 10) {
    s = s.substring(0, 10);
  }
  return s;
}

void rechner_anzeige_zeichnen() {
  int y = HEADER_HOEHE + 4;
  tft.fillRoundRect(8, y, SCREEN_W - 16, 34, 4, tft.color565(20, 28, 40));
  tft.drawRoundRect(8, y, SCREEN_W - 16, 34, 4, FARBE_RAHMEN);
  tft.setTextDatum(MR_DATUM);
  tft.setTextColor(FARBE_TEXT, tft.color565(20, 28, 40));
  tft.drawString(rechner_anzeige, SCREEN_W - 16, y + 17, 4);
}

void rechner_tasten_zeichnen() {
  for (int z = 0; z < RECHNER_ZEILEN; z++) {
    for (int s = 0; s < RECHNER_SPALTEN; s++) {
      int x = rechner_btn_x[z][s];
      int y = rechner_btn_y[z][s];
      const char* t = RECHNER_TASTEN[z][s];
      uint16_t farbe = tft.color565(40, 55, 78);
      if (t[0] == '=' ) farbe = tft.color565(40, 130, 90);
      else if (t[0] == 'C') farbe = tft.color565(140, 50, 50);
      else if (t[0] == '+' || t[0] == '-' || t[0] == '*' || t[0] == '/') {
        farbe = tft.color565(50, 90, 120);
      }
      tft.fillRoundRect(x, y, rechner_btn_w, rechner_btn_h, 4, farbe);
      tft.drawRoundRect(x, y, rechner_btn_w, rechner_btn_h, 4, FARBE_RAHMEN);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(FARBE_TEXT, farbe);
      tft.drawString(t, x + rechner_btn_w / 2, y + rechner_btn_h / 2, 2);
    }
  }
}

void rechner_starten() {
  tft.fillRect(0, HEADER_HOEHE, SCREEN_W, SCREEN_H - HEADER_HOEHE, FARBE_HINTERGRUND);
  int gap = 4;
  rechner_btn_w = (SCREEN_W - 16 - gap * 3) / 4;
  rechner_btn_h = 32;
  int start_y = HEADER_HOEHE + 44;
  for (int z = 0; z < RECHNER_ZEILEN; z++) {
    for (int s = 0; s < RECHNER_SPALTEN; s++) {
      rechner_btn_x[z][s] = 8 + s * (rechner_btn_w + gap);
      rechner_btn_y[z][s] = start_y + z * (rechner_btn_h + gap);
    }
  }
  rechner_anzeige_zeichnen();
  rechner_tasten_zeichnen();
}

void rechner_verlassen() {
}

void rechner_reset() {
  rechner_anzeige = "0";
  rechner_akku = 0;
  rechner_op = 0;
  rechner_neue_zahl = true;
  rechner_error = false;
}

double rechner_anwenden(double a, double b, char op) {
  switch (op) {
    case '+': return a + b;
    case '-': return a - b;
    case '*': return a * b;
    case '/': return (b == 0) ? NAN : a / b;
    default:  return b;
  }
}

void rechner_taste(const char* t) {
  if (strcmp(t, "C") == 0) {
    rechner_reset();
    rechner_anzeige_zeichnen();
    return;
  }
  if (strcmp(t, "CE") == 0) {
    rechner_anzeige = "0";
    rechner_neue_zahl = true;
    rechner_error = false;
    rechner_anzeige_zeichnen();
    return;
  }
  if (strcmp(t, "<-") == 0) {
    if (!rechner_neue_zahl && rechner_anzeige.length() > 0) {
      rechner_anzeige.remove(rechner_anzeige.length() - 1);
      if (rechner_anzeige.length() == 0 || rechner_anzeige == "-") {
        rechner_anzeige = "0";
        rechner_neue_zahl = true;
      }
      rechner_anzeige_zeichnen();
    }
    return;
  }
  if (rechner_error) {
    return;
  }
  if (strcmp(t, "+/-") == 0) {
    if (rechner_anzeige.startsWith("-")) {
      rechner_anzeige.remove(0, 1);
    } else if (rechner_anzeige != "0") {
      rechner_anzeige = "-" + rechner_anzeige;
    }
    rechner_anzeige_zeichnen();
    return;
  }
  if ((t[0] >= '0' && t[0] <= '9') || t[0] == '.') {
    if (rechner_neue_zahl) {
      rechner_anzeige = (t[0] == '.') ? "0." : String(t);
      rechner_neue_zahl = false;
    } else {
      if (t[0] == '.' && rechner_anzeige.indexOf('.') >= 0) return;
      if (rechner_anzeige.length() >= 10) return;
      if (rechner_anzeige == "0" && t[0] != '.') {
        rechner_anzeige = String(t);
      } else {
        rechner_anzeige += t;
      }
    }
    rechner_anzeige_zeichnen();
    return;
  }
  double wert = rechner_anzeige.toDouble();
  if (t[0] == '=') {
    if (rechner_op != 0) {
      double r = rechner_anwenden(rechner_akku, wert, rechner_op);
      rechner_anzeige = rechner_format(r);
      rechner_error = (rechner_anzeige == "Error");
      rechner_akku = r;
      rechner_op = 0;
      rechner_neue_zahl = true;
    }
    rechner_anzeige_zeichnen();
    return;
  }
  if (rechner_op != 0 && !rechner_neue_zahl) {
    double r = rechner_anwenden(rechner_akku, wert, rechner_op);
    rechner_anzeige = rechner_format(r);
    rechner_error = (rechner_anzeige == "Error");
    wert = r;
    rechner_anzeige_zeichnen();
    if (rechner_error) return;
  }
  rechner_akku = wert;
  rechner_op = t[0];
  rechner_neue_zahl = true;
}

void rechner_touch_behandeln() {
  for (int z = 0; z < RECHNER_ZEILEN; z++) {
    for (int s = 0; s < RECHNER_SPALTEN; s++) {
      int x = rechner_btn_x[z][s];
      int y = rechner_btn_y[z][s];
      if (touch_x >= x && touch_x <= x + rechner_btn_w &&
          touch_y >= y && touch_y <= y + rechner_btn_h) {
        rechner_taste(RECHNER_TASTEN[z][s]);
        return;
      }
    }
  }
}
