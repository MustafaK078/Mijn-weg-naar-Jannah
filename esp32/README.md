# Jannah ESP32 companion

Firmware voor de Waveshare ESP32-S3-Touch-LCD-1.47 (172×320, verticaal).
Houdt de 6 vaste adhkar bij: Subhanallah, Alhamdulillah, Allahu Akbar, Istighfar, Tahlil en Salawat.
Met **Bluetooth live-sync (BLE Keyboard)** en **offline QR-code sync** direct naar de Mijn weg naar Jannah webapp op je iPhone!

## Ondersteunde synchronisatie

### 1. Bluetooth BLE Keyboard (Real-time live meeklikken)
- Werkt standaard op iPhone (Safari & PWA) zonder extra software.
- ESP32 gedraagt zich als een draadloos Bluetooth-toetsenbord genaamd **"Jannah Tasbih"**.
- Bij elke tik zendt de ESP32 de corresponderende dhikr-toets ('1' t/m '6') of Spatiebalk naar de iPhone.
- De webapp telt direct live mee, en werkt je goede daden en hartmeter meteen bij!

### 2. QR-code Synchronisatie (Offline backup)
- Veeg op het scherm van de ESP32 naar de **SYNC** pagina.
- Het scherm toont een scherpe QR-code met de 6 tellingen.
- Scan simpelweg met je **iPhone Camera app**: iOS toont een melding om de webapp te openen.
- Alle 6 tellingen worden in één klik overgedragen en opgeteld bij je goede daden.
- Tik daarna op **"RESET TELLERS"** op de ESP32 om fris aan de volgende sessie te beginnen.

---

## Installatie in Arduino IDE 2

1. Open [Arduino IDE](https://www.arduino.cc/en/software).
2. Installeer via **Boards Manager**: `esp32 by Espressif Systems` (versie ≥ 3.0.0).
3. Installeer via **Library Manager**:
   - `GFX Library for Arduino` (door moononournation)
   - `ESP32 BLE Keyboard` (door Severin von Kitzing / T-vK)
   *(De QR-code bibliotheek `qrcode.h` & `qrcode.c` zit al ingebouwd in de sketch-map).*
4. Sluit de ESP32 met een USB-C kabel aan.
5. Open `jannah_esp32/jannah_esp32.ino`.
6. Kies instellingen:
   - **Board**: `ESP32S3 Dev Module`
   - **USB CDC On Boot**: `Enabled`
   - **Flash Size**: `16MB`
   - **PSRAM**: `OPI PSRAM`
   - De juiste **COM-poort**
7. Klik op **Upload**.

---

## iPhone koppelen via Bluetooth

1. Open op je iPhone: **Instellingen > Bluetooth**.
2. Zet Bluetooth aan en zoek naar **"Jannah Tasbih"**.
3. Tik op **Verbind**.
4. Op het scherm van je ESP32 verschijnt rechtsboven een gouden `BT` indicator zodra de verbinding actief is.

---

## Bediening van het apparaat

- **Tik**:
  - Home: Starten
  - Tesbih: +1 dhikr (en zendt BLE toets naar iPhone)
  - Adhkar-overzicht: Kies een specifieke dhikr
  - Helderheid: Pas schermverlichting aan
  - Sync: Tik op "RESET TELLERS" om alle tellers op 0 te zetten
- **Veeg links/rechts**: Blader tussen pagina's:
  `HOME` ↔ `TESBIH` ↔ `ADHKAR` ↔ `HELDERHEID` ↔ `SYNC`
- **Lang indrukken op Tesbih (0,8 sec)**: Zet de huidige dhikr-teller op 0.
- **Fysieke BOOT-knop (GPIO0)**: Kort = +1 / volgende, Lang = wissel pagina.
