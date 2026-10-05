# ESP32-XPT2046_Touchscreen
ESP32 Projekt zum verbinden eines 2.8 tft 240x320 ili9341 XPT2046_Touchscreen. 

Was noch fehlt:


~~- ota installer via IDE und weboberfläche~~ ✅ erledigt (esp32-tft-netzwerk.ino)
~~- zugriff auf SD karte über netzwerk~~ ✅ erledigt (esp32-tft-netzwerk.ino)

### Netzwerk-Dienste (neu)
Nach WLAN-Verbindung stehen bereit:

| Dienst | Adresse | Beschreibung |
|--------|---------|--------------|
| Status | `http://esp32-tft.local/` oder `http://<IP>/` | Übersicht + Links |
| SD-Karte | `http://esp32-tft.local/sd` | Dateiliste + Download |
| Web-OTA | `http://esp32-tft.local/update` | Firmware per Browser flashen |
| ArduinoOTA | Netzwerk-Port in der Arduino-IDE | Direkt aus der IDE updaten |

Programme:
- eine zweite Programme seite. oben in der Leiste wird mittig 1/2 und 2/2 als seitenzahl gezeigt. Bei antippen wechselt er auf die andere programmseite
- Ideen - Programmkachel Netzwerk. Auswahl zwichen WLAN und Bluetooth.
  Wlan: hier können WLANs gescannt werden und mit infos wie 2.4 oder 5 GHz, und verschlüsselungs technologie angezeigt, das verbundene Wlan nach geräte durchsuchen und diese mit IP               anzeigen
Bluetooth: Es können Geräte in der nähe gescannt werden und diese Angezeigt
Verbesserung:
- Highscore datei für jedes Spiel auf sd karte
- Einträge sortiert nach Punktzahl oder bestleistung
- wenn Spiel geschafft wurde, Wird der erreichte punktestand (snake,pipe mania) oder benötigte zeit ( sudoku ) zusammen mit      dem aktuellen datum in die highscore liste gespeichert. Bei sudoku soll es eine  highscore liste pro schwierigkeit grad.
Programme Verbesserung:
-Datei Browser:
Daten hochladen einbauen 
Wenn man zb. ein Bild runterlädt, kommt immer eine Datei namens download.bin und nicht das eigentliche Bild. Auch der datei name sollte nicht immer gleich sein 

ideen:
Mini-Farmsimulator
Turmverteidigungsspiel
Benachrichtigungs-seite
Tic-Tac-Toe
Uhrzeit / Datum manuell einstellen


Eier uhr mit auswahl der ei größe und garpunkt

Symbole für obere statusleiste:
Status:
-Wlan
-Bluetooth
-SD Karte
-Akku Ladezustand
-Ota (bisherige entf.)

Spiele:
-brauchen hauptmenü
-Tetris Vorauswahl ( + Wert - ) um fallgeschw. ab Start zu ändern
Spielfeld ist immernoch gequetscht
- Bricks, Tetris





