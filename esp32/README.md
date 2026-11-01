# Jannah ESP32 companion

Firmware voor de Waveshare ESP32-S3-Touch-LCD-1.47 (172×320, verticaal).
Houdt de 6 vaste adhkar bij: Subhanallah, Alhamdulillah, Allahu Akbar, Istighfar, Tahlil en Salawat.
Met **Bluetooth batch-sync (BLE Keyboard)** direct naar de Mijn weg naar Jannah webapp op je iPhone!

## Hoe werkt de synchronisatie?

### Offline opslag & Bluetooth Batch-overdracht
- **Geen lockscreen-problemen:** Tijdens het dhikr doen zendt de ESP32 géén losse toetsaanslagen. Je iPhone kan dus gewoon veilig vergrendeld in je zak blijven zonder dat er per ongeluk cijfers in je toegangscode-scherm getypt worden.
- **Offline geheugen:** Alle tellingen worden veilig opgeslagen in het flashgeheugen van de ESP32 (`Preferences`), zelfs als de batterij leeg raakt of het apparaat herstart.
- **Synchroniseren op verzoek:**
  1. Open de **Mijn weg naar Jannah** webapp op je iPhone.
  2. Zorg dat Bluetooth verbonden is met **"Jannah Tasbih"**.
  3. Veeg op de ESP32 naar pagina 5 (**SYNC**).
  4. Tik op **"STUUR VIA BT"**.
  5. De webapp onderschept de batchtellingen automatisch en telt ze direct op bij je dhikr en goede daden!
  6. De ESP32 zet de tellers na verzending automatisch terug naar 0.

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
  - Tesbih: +1 dhikr (offline opgeteld in flash)
  - Adhkar-overzicht: Kies een specifieke dhikr
  - Helderheid: Pas schermverlichting aan
  - Sync: 
    - **"STUUR VIA BT"**: Draagt alle tellingen in één keer over naar de geopende webapp
    - **"RESET TELLERS"**: Zet alle tellers handmatig op 0
- **Veeg links/rechts**: Blader tussen pagina's:
  `HOME` ↔ `TESBIH` ↔ `ADHKAR` ↔ `HELDERHEID` ↔ `SYNC`
- **Lang indrukken op Tesbih (0,8 sec)**: Zet de huidige dhikr-teller op 0.
- **Fysieke BOOT-knop (GPIO0)**: Kort = +1 / volgende, Lang = wissel pagina.
