# Jannah ESP32 companion

Firmware voor de Waveshare ESP32-S3-Touch-LCD-1.47 (172×320, verticaal).
Houdt 10 vaste adhkar bij en synchroniseert naadloos met de "Mijn weg naar Jannah" webapp op je iPhone!

## De 10 Adhkar

1. **Subhanallah** (s)
2. **Alhamdulillah** (a)
3. **Allahu Akbar** (k)
4. **Istighfar** (i)
5. **Tahlil** (t)
6. **Salawat** (w)
7. **SubḥānAllāhi wa-biḥamdihī** (b)
8. **SubḥānAllāhi l-ʿAẓīm wa-biḥamdihī** (z)
9. **Lā ḥawla wa-lā quwwata illā bi-llāh** (h)
10. **Lā ilāha illa Allāh waḥdahu lā sharīka lah** (u)

Op de **ADHKAR**-pagina kun je soepel verticaal door de hele lijst scrollen en een gewenste dhikr aantikken.

---

## Twee synchronisatiemodi (Live & Cache)

- **Live synchronisatie (Webapp geopend):**
  - Standaard staat **BT LIVE** aan (te zien aan de gouden statusknop rechtsboven op het Tesbih-scherm).
  - Elke tik verstuurt een commando (`esp32 live <sleutel>`).
  - Omdat er geen losse cijfers worden verzonden, kan het toegangscodescherm van je iPhone nooit per ongeluk cijfers invoeren!
  - In de webapp zie je de teller, het hart en de daden direct live oplopen.
  - Tik op de **BT LIVE / BT CACHE** knop rechtsboven om te wisselen naar offline cache-modus.

- **Batch synchronisatie (Telefoon vergrendeld of offline):**
  - Tellingen worden altijd lokaal in flash (`Preferences`) opgeslagen.
  - Zodra je de app opent: veeg eenvoudig omhoog vanaf elk scherm naar de **SYNC**-pagina en tik op **"STUUR VIA BT"**.
  - Alle 10 tellingen worden in één keer verstuurd en de tellers op het apparaat worden gereset naar 0.

---

## iPhone koppelen via Bluetooth

1. Open op je iPhone: **Instellingen > Bluetooth**.
2. Zet Bluetooth aan en zoek naar **"Jannah Tasbih"**.
3. Tik op **Verbind**.
4. Zodra verbonden brandt er in de rechterbovenhoek een subtiel **blauw rondje** als statusindicatie, zonder de "TESBIH" titel te belemmeren.

---

## Bediening & Gebaren

- **Veeg omhoog (vanaf elk scherm)**: Direct naar de **SYNC** pagina met de "STUUR VIA BT" knop!
- **Veeg omlaag (op SYNC)**: Direct terug naar het vorige scherm.
- **Veeg links / rechts**: Wisselen tussen pagina's met vloeiende hardware-accelerated animatie zonder flikkering.
- **Scrollen op ADHKAR-pagina**: Veeg omhoog/omlaag over de lijst met 10 adhkar.
- **Fysieke BOOT-knop (GPIO0)**:
  - **Kort indrukken**: Telt direct **+1** op bij de huidige Tesbih!
  - **3 seconden ingedrukt houden**: Zet de ESP32 **uit** (gaat in diepe slaap / deep sleep met display en backlight uitgeschakeld).
  - **Inschakelen**: Druk kort op de BOOT-knop terwijl het apparaat uit staat om hem direct weer in te schakelen!
- **Scherm tikken**:
  - Home: Starten
  - Tesbih: +1 tellen (of aantikken van BT badge rechtsboven om live/cache te toggelen)
  - Lang vasthouden op Tesbih (0,8 sec): Reset huidige teller naar 0
  - Helderheid: Slider verslepen of tikken op `-` / `+`
  - Sync: Tik op "STUUR VIA BT" of "RESET TELLERS"
