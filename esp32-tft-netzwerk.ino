// ============================================================================
//  NETZWERK-DIENSTE: OTA + SD-KARTEN-ZUGRIFF UEBER WLAN
//  Datei: esp32-tft-netzwerk.ino
//
//  Enthalten:
//  1) ArduinoOTA  -> Firmware-Update aus der Arduino-IDE (Netzwerk-Port)
//  2) Web-OTA     -> Firmware-Update per Browser (http://<ip>/update)
//  3) SD-Webserver -> Dateien der SD-Karte im Browser anzeigen / herunterladen
//                    (http://<ip>/sd)
//
//  Voraussetzung: WLAN ist bereits verbunden (wlan_und_zeit_einrichten()).
//  Wird aus setup() und loop() des Hauptdateis aufgerufen.
// ============================================================================

#include <ArduinoOTA.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>

// Webserver auf Port 80 (Standard-HTTP)
WebServer netz_server(80);

// Hostname im lokalen Netz (http://esp32-tft.local)
static const char* NETZ_HOSTNAME = "esp32-tft";

// OTA-Passwort (leer = kein Passwort, fuer den Anfang ok)
static const char* OTA_PASSWORT = "1234";

// --------------------------------------------------------------------------
//  HILFSFUNKTIONEN SD-BUS (gleiche Logik wie in esp32-tft-sd.ino)
// --------------------------------------------------------------------------
static void netz_sd_bus_sichern() {
  digitalWrite(TOUCH_CS_PIN, HIGH);
  digitalWrite(SD_CS_PIN, HIGH);
#ifdef TFT_CS
  digitalWrite(TFT_CS, HIGH);
#endif
}

// --------------------------------------------------------------------------
//  WEB: STARTSEITE
// --------------------------------------------------------------------------
static void netz_handle_root() {
  String html = F(
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>ESP32-TFT</title>"
    "<style>"
    "body{font-family:system-ui,sans-serif;background:#0a1a33;color:#eee;margin:0;padding:16px;}"
    "h1{font-size:1.4rem;margin:0 0 12px;}"
    "a{color:#7ec8ff;text-decoration:none;}"
    ".card{background:#1a2a44;border-radius:8px;padding:14px;margin:10px 0;}"
    ".btn{display:inline-block;background:#2a6;color:#fff;padding:10px 16px;"
    "border-radius:6px;margin:6px 6px 0 0;}"
    ".btn2{background:#368;}"
    ".info{font-size:0.9rem;opacity:0.85;}"
    "</style></head><body>"
    "<h1>ESP32 TFT Steuerzentrale</h1>"
  );

  html += "<div class='card info'>";
  html += "IP: " + WiFi.localIP().toString() + "<br>";
  html += "SSID: " + WiFi.SSID() + "<br>";
  html += "Hostname: " + String(NETZ_HOSTNAME) + ".local<br>";
  html += "Free Heap: " + String(ESP.getFreeHeap()) + " Byte";
  html += "</div>";

  html += F(
    "<div class='card'>"
    "<a class='btn' href='/sd'>SD-Karte durchsuchen</a>"
    "<a class='btn btn2' href='/update'>Firmware OTA</a>"
    "</div>"
    "</body></html>"
  );

  netz_server.send(200, "text/html", html);
}

// --------------------------------------------------------------------------
//  WEB: SD-DATEILISTE
// --------------------------------------------------------------------------
static void netz_handle_sd() {
  if (!sd_stand.mount_ok) {
    // Versuch, die Karte nachzuloaden
    netz_sd_bus_sichern();
    if (!SD.begin(SD_CS_PIN, tft.getSPIinstance(), 4000000)) {
      netz_server.send(500, "text/plain", "SD-Karte nicht gemountet");
      return;
    }
    sd_stand.mount_ok = true;
  }

  netz_sd_bus_sichern();

  String html = F(
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>SD-Karte</title>"
    "<style>"
    "body{font-family:system-ui,sans-serif;background:#0a1a33;color:#eee;margin:0;padding:16px;}"
    "h1{font-size:1.3rem;}"
    "a{color:#7ec8ff;}"
    "table{width:100%;border-collapse:collapse;margin-top:12px;}"
    "th,td{text-align:left;padding:8px 6px;border-bottom:1px solid #334;}"
    "th{opacity:0.7;font-size:0.85rem;}"
    ".back{display:inline-block;margin-bottom:10px;}"
    ".dir{color:#9fd;}"
    "</style></head><body>"
    "<a class='back' href='/'>← Zurück</a>"
    "<h1>SD-Karte</h1>"
    "<table><tr><th>Name</th><th>Größe</th><th></th></tr>"
  );

  File wurzel = SD.open("/");
  if (!wurzel) {
    html += "<tr><td colspan='3'>Wurzelverzeichnis nicht lesbar</td></tr>";
  } else {
    File eintrag = wurzel.openNextFile();
    while (eintrag) {
      const char* name = eintrag.name();
      if (name && name[0] != '.') {
        String n = String(name);
        // führenden Slash entfernen falls vorhanden
        if (n.startsWith("/")) n = n.substring(1);

        html += "<tr><td>";
        if (eintrag.isDirectory()) {
          html += "<span class='dir'>📁 " + n + "/</span>";
        } else {
          html += "📄 " + n;
        }
        html += "</td><td>";
        if (eintrag.isDirectory()) {
          html += "-";
        } else {
          html += String(eintrag.size()) + " B";
        }
        html += "</td><td>";
        if (!eintrag.isDirectory()) {
          html += "<a href='/sd/download?f=" + n + "'>Download</a>";
        }
        html += "</td></tr>";
      }
      eintrag.close();
      eintrag = wurzel.openNextFile();
    }
    wurzel.close();
  }

  html += F("</table></body></html>");
  netz_server.send(200, "text/html", html);
}

// --------------------------------------------------------------------------
//  WEB: SD-DATEI HERUNTERLADEN
// --------------------------------------------------------------------------
static void netz_handle_sd_download() {
  if (!netz_server.hasArg("f")) {
    netz_server.send(400, "text/plain", "Parameter f fehlt");
    return;
  }

  String dateiname = netz_server.arg("f");
  // Sicherheitsfilter: keine Pfad-Traversal
  if (dateiname.indexOf("..") >= 0 || dateiname.indexOf('/') >= 0 || dateiname.indexOf('\\') >= 0) {
    netz_server.send(400, "text/plain", "Ungültiger Dateiname");
    return;
  }

  netz_sd_bus_sichern();

  String pfad = "/" + dateiname;
  File datei = SD.open(pfad, FILE_READ);
  if (!datei || datei.isDirectory()) {
    if (datei) datei.close();
    netz_server.send(404, "text/plain", "Datei nicht gefunden");
    return;
  }

  netz_server.streamFile(datei, "application/octet-stream");
  datei.close();
}

// --------------------------------------------------------------------------
//  WEB: OTA-UPDATE SEITE
// --------------------------------------------------------------------------
static void netz_handle_update() {
  String html = F(
    "<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>OTA Update</title>"
    "<style>"
    "body{font-family:system-ui,sans-serif;background:#0a1a33;color:#eee;margin:0;padding:16px;}"
    "h1{font-size:1.3rem;}"
    ".card{background:#1a2a44;border-radius:8px;padding:16px;margin:12px 0;}"
    "input[type=file]{margin:10px 0;}"
    "button{background:#2a6;color:#fff;border:0;padding:10px 18px;border-radius:6px;font-size:1rem;}"
    ".back{color:#7ec8ff;}"
    "</style></head><body>"
    "<a class='back' href='/'>← Zurück</a>"
    "<h1>Firmware OTA Update</h1>"
    "<div class='card'>"
    "<p>Wähle eine <b>.bin</b>-Datei (Sketch → Export compiled Binary)</p>"
    "<form method='POST' action='/update' enctype='multipart/form-data'>"
    "<input type='file' name='update' accept='.bin' required><br>"
    "<button type='submit'>Hochladen &amp; flashen</button>"
    "</form>"
    "</div>"
    "<p style='opacity:0.7;font-size:0.85rem'>Nach dem Update startet der ESP32 neu.</p>"
    "</body></html>"
  );
  netz_server.send(200, "text/html", html);
}

// --------------------------------------------------------------------------
//  WEB: OTA UPLOAD VERARBEITEN
// --------------------------------------------------------------------------
static void netz_handle_update_post() {
  HTTPUpload& upload = netz_server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("OTA: Start %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("OTA: Erfolg, %u Bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

static void netz_handle_update_finish() {
  if (Update.hasError()) {
    netz_server.send(500, "text/plain", "Update fehlgeschlagen – siehe Serial");
  } else {
    netz_server.sendHeader("Connection", "close");
    netz_server.send(200, "text/plain", "Update OK – ESP32 startet neu ...");
    delay(500);
    ESP.restart();
  }
}

// --------------------------------------------------------------------------
//  OTA + WEBSERVER EINRICHTEN (einmalig nach WLAN-Verbindung)
// --------------------------------------------------------------------------
void netzwerk_dienste_einrichten() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Netzwerk-Dienste: kein WLAN, übersprungen.");
    return;
  }

  // ---- mDNS ----
  if (MDNS.begin(NETZ_HOSTNAME)) {
    Serial.print("mDNS: http://");
    Serial.print(NETZ_HOSTNAME);
    Serial.println(".local");
  }

  // ---- ArduinoOTA (IDE-Netzwerkport) ----
  ArduinoOTA.setHostname(NETZ_HOSTNAME);
  if (strlen(OTA_PASSWORT) > 0) {
    ArduinoOTA.setPassword(OTA_PASSWORT);
  }

  ArduinoOTA.onStart([]() {
    Serial.println("ArduinoOTA: Start");
    // Display kurz informieren
    tft.fillRect(0, 0, SCREEN_W, HEADER_HOEHE, tft.color565(120, 60, 20));
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(FARBE_TEXT, tft.color565(120, 60, 20));
    tft.drawString("OTA läuft ...", SCREEN_W / 2, HEADER_HOEHE / 2, 2);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nArduinoOTA: Ende");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("ArduinoOTA Fehler[%u]\n", error);
  });
  ArduinoOTA.begin();
  Serial.println("ArduinoOTA bereit (IDE Netzwerk-Port).");

  // ---- Webserver Routen ----
  netz_server.on("/", HTTP_GET, netz_handle_root);
  netz_server.on("/sd", HTTP_GET, netz_handle_sd);
  netz_server.on("/sd/download", HTTP_GET, netz_handle_sd_download);
  netz_server.on("/update", HTTP_GET, netz_handle_update);
  netz_server.on("/update", HTTP_POST,
                 netz_handle_update_finish,
                 netz_handle_update_post);

  netz_server.onNotFound([]() {
    netz_server.send(404, "text/plain", "Nicht gefunden");
  });

  netz_server.begin();
  Serial.print("Webserver gestartet: http://");
  Serial.println(WiFi.localIP());
  Serial.println("  /          Status");
  Serial.println("  /sd        SD-Dateiliste");
  Serial.println("  /update    Web-OTA");
}

// --------------------------------------------------------------------------
//  IN LOOP() AUFRUFEN
// --------------------------------------------------------------------------
void netzwerk_dienste_tick() {
  if (WiFi.status() == WL_CONNECTED) {
    ArduinoOTA.handle();
    netz_server.handleClient();
  }
}
