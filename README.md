# ESP32-XPT2046_Touchscreen
ESP32 Projekt zum verbinden eines 2.8 tft 240x320 ili9341 XPT2046_Touchscreen. 

Was noch fehlt:
-secrets datei

-ota installer via IDE und weboberfläche

-zugriff auf SD karte über netzwerk

Spiele:
-Ideen - Bricks, Tetris

Programme:
- eine zweite Programme seite. oben in der Leiste wird mittig 1/2 und 2/2 als seitenzahl gezeigt. Bei antippen wechselt er auf die andere programmseite
- Ideen - Programmkachel Netzwerk. Auswahl zwichen WLAN und Bluetooth.
  Wlan: hier können WLANs gescannt werden und mit infos wie 2.4 oder 5 GHz, und verschlüsselungs technologie angezeigt, das verbundene Wlan nach geräte durchsuchen und diese mit IP               anzeigen
Bluetooth: Es können Geräte in der nähe gescannt werden und diese Angezeigt
Verbesserung:
- Highscore datei für jedes Spiel auf sd karte
- Einträge sortiert nach Punktzahl oder bestleistung
- wenn Spiel geschafft wurde, Wird der erreichte punktestand (snake,pipe mania) oder benötigte zeit ( sudoku ) zusammen mit      dem aktuellen datum in die highscore liste gespeichert. Bei sudoku soll es eine  highscore liste pro schwierigkeit geben

Probleme:
- snake, essen

Fehler:
-Screensaver Wetter anzeige ist noch Platzhalter, soll wechseln zwichen Uhrzeit, Wetter in Bilk und Wetter 3 Tage vorhersage
-Füge der Uhr im Screensaver ein Datum hinzu


erledigt:

Funktion
Adresse / Nutzung
ArduinoOTA
In der Arduino-IDE unter „Port“ den Netzwerk-Port esp32-tft wählen → normal hochladen
Web-OTA
Browser: http://esp32-tft.local/update oder http://<IP>/update → .bin auswählen und flashen
SD-Zugriff
Browser: http://esp32-tft.local/sd → Dateiliste + Download-Links
Statusseite
http://esp32-tft.local/ → IP, SSID, Heap, Links zu den Diensten


