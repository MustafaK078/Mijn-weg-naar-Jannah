// Jannah ESP32 companion - Waveshare ESP32-S3-Touch-LCD-1.47 (172x320, portrait)
// Library: "GFX Library for Arduino" (moononournation)
// Library (optioneel): "ESP32 BLE Keyboard" (Severin von Kitzing / T-vK)
// QR-code generator: ingebouwd via qrcode.h & qrcode.c in deze map
// Board: ESP32S3 Dev Module, USB CDC On Boot: Enabled
//
// Bediening:
//   - Tik: starten (Home) / +1 (Tesbih) / kies dhikr (Adhkar) / helderheid / tellers resetten (Sync)
//   - Veeg links/rechts: wissel van pagina (Home <-> Tesbih <-> Adhkar <-> Helderheid <-> Sync)
//   - Lang vasthouden op Tesbih: teller van huidige dhikr resetten naar 0
// BOOT-knop (GPIO0): kort = +1 / volgende, lang = wissel pagina

#include <Arduino_GFX_Library.h>
#include <Preferences.h>
#include <Wire.h>
#include <esp_sleep.h>

#include <BleKeyboard.h>
#define HAS_BLE_KEYBOARD 1
BleKeyboard bleKeyboard("Jannah Tasbih", "Mustafa", 100);

#define PIN_DC   45
#define PIN_CS   21
#define PIN_SCK  38
#define PIN_MOSI 39
#define PIN_RST  40
#define PIN_BL   46
#define PIN_BTN  0

Arduino_DataBus *bus = new Arduino_ESP32SPI(PIN_DC, PIN_CS, PIN_SCK, PIN_MOSI);
Arduino_ST7789 *tft = new Arduino_ST7789(bus, PIN_RST, 0 /* portrait */, false /* IPS false */,
                                         172, 320, 34, 0, 34, 0);
Arduino_Canvas *gfx = new Arduino_Canvas(172, 320, tft);

uint16_t *oldBuf = nullptr;

enum TransitionDir {
  TRANS_LEFT,   // Volgende pagina (veeg links, schuift naar links)
  TRANS_RIGHT,  // Vorige pagina (veeg rechts, schuift naar rechts)
  TRANS_UP,     // Omhoog vegen (schuift omhoog naar BT Sync)
  TRANS_DOWN,   // Omlaag vegen (schuift omlaag terug vanaf BT Sync)
  TRANS_NONE    // Direct / zonder animatie
};

#define RGB(r, g, b) (uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

// Waveshare 1.47" hardware gebruikt een BGR-paneel (bit 3 in MADCTL = 1)
#define BGR_ORDER 1

// Jannah kleurenpalet: Zuiver diep islamitisch groen, warm goud, licht beige & wit
const uint16_t C_BG     = RGB(20, 130, 25);      // Zuiver diep islamitisch groen
const uint16_t C_CARD   = RGB(25, 145, 35);    // Iets lichtere groene kaartkleur
const uint16_t C_GREEN2 = RGB(22, 125, 48);    // Helder groen ring-/accentkleur
const uint16_t C_BEIGE  = RGB(246, 238, 218);  // Warme licht beige knopkleur (#F6EEDA)
const uint16_t C_GOLD   = RGB(240, 215, 0);   // Echt stralend goud
const uint16_t C_WHITE  = RGB(255, 255, 255);  // Helder zuiver wit
const uint16_t C_CREAM  = RGB(242, 236, 222);  // Zacht crème voor subtiele tekst
const uint16_t C_MUTED  = RGB(150, 185, 160);  // Zacht saliegrijs voor voetteksten
const uint16_t C_GREEN  = RGB(20, 130, 25);      // Donkergroen voor tekst op de beige knop

struct Dhikr {
  const char *title;
  const char *phrase;
  const char *meaning;
  uint16_t target;
  char keyChar;
};

const Dhikr ADHKAR[] = {
  {"Subhanallah", "Subhan Allah", "Verheven is Allah boven iedere tekortkoming", 33, 's'},
  {"Alhamdulillah", "Alhamdu lillah", "Alle lof en dank behoort toe aan Allah", 33, 'a'},
  {"Allahu Akbar", "Allahu Akbar", "Allah is de Grootste boven alles", 34, 'k'},
  {"Istighfar", "Astaghfirullah wa-atubu ilayh", "Ik vraag Allah om vergeving en keer berouwvol tot Hem", 100, 'i'},
  {"Tahlil", "La ilaha illallah", "Er is geen godheid die het recht heeft aanbeden te worden behalve Allah", 100, 't'},
  {"Salawat", "Allahumma salli ala Muhammad wa ala ali Muhammad", "O Allah, schenk Uw zegeningen en vrede aan Muhammad en zijn familie", 100, 'w'},
  {"SubhanAllah wa-b.", "SubhanAllahi wa-bihamdihi", "Verheven is Allah en alle lof behoort Hem toe - wist zonden als schuim van de zee", 100, 'b'},
  {"SubhanAllah l-Azim", "SubhanAllahi l-Azim wa-bihamdihi", "Verheven is Allah de Geweldige, en aan Hem behoort alle lof", 100, 'z'},
  {"La hawla wa-la q.", "La hawla wa-la quwwata illa billah", "Er is geen macht en geen kracht behalve door Allah - een schat uit het Paradijs", 100, 'h'},
  {"Tahlil Tamm", "La ilaha illa Allah wahdahu la sharika lah, lahul-mulku wa-lahul-hamd, wa-huwa ala kulli shay'in qadir", "Niemand heeft het recht aanbeden te worden behalve Allah alleen, zonder deelgenoot. Aan Hem behoort het koninkrijk en alle lof, en Hij is over alles machtig", 100, 'u'},
};
const int N = sizeof(ADHKAR) / sizeof(ADHKAR[0]);

enum Page { HOME, TESBIH, ADHKAR_LIST, BRIGHTNESS, SYNC };
const int NUM_PAGES = 5;
Page page = HOME;
Page lastPageBeforeSync = TESBIH;
int current = 0;
uint16_t counts[N] = {0};
uint8_t brightness = 200;  // Helderheid 15..255
bool liveSync = true;       // Live verzending wanneer verbonden
int adhkarScrollY = 0;      // Scrollpositie voor de lijst met 10 adhkar
bool touchActive = false;   // Vlag of scherm momenteel wordt aangeraakt
Preferences prefs;
bool dirty = true;

void save() {
  prefs.putBytes("counts", counts, sizeof(counts));
  prefs.putInt("cur", current);
}

void applyBrightness(uint8_t b) {
  brightness = constrain(b, 15, 255);
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
  ledcAttach(PIN_BL, 5000, 8);
  ledcWrite(PIN_BL, brightness);
#else
  analogWrite(PIN_BL, brightness);
#endif
}

void centerText(const char *s, int y, int size, uint16_t color) {
  int16_t x1, y1;
  uint16_t w, h;
  gfx->setTextSize(size);
  gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  gfx->setTextColor(color);
  gfx->setCursor((172 - w) / 2, y);
  gfx->print(s);
}

void drawFrame() {
  gfx->fillScreen(C_BG);
}

void drawHome() {
  drawFrame();
  centerText("Mijn weg", 45, 2, C_CREAM);
  centerText("naar", 68, 2, C_CREAM);
  centerText("Jannah", 92, 3, C_GOLD);

  centerText("Bismillah", 145, 2, C_WHITE);
  gfx->drawFastHLine(56, 170, 60, C_GOLD);

  // De licht beige knop
  gfx->fillRoundRect(16, 200, 140, 66, 16, C_BEIGE);
  gfx->drawRoundRect(16, 200, 140, 66, 16, C_GOLD);
  centerText("START", 215, 2, C_GREEN);
  centerText("Tik om te beginnen", 240, 1, C_GREEN);

  centerText("veeg voor adhkar", 290, 1, C_MUTED);
}

// Snelle, trillingvrije update van de tesbih-teller
void updateTesbihCounter() {
  const Dhikr &d = ADHKAR[current];
  uint16_t c = counts[current];
  const int cx = 86, cy = 145, r = 62;

  // 1. Gouden progressiestipjes op de buitenrand
  float frac = min(1.0f, (float)c / d.target);
  int steps = (int)(frac * 60);
  gfx->drawCircle(cx, cy, r + 2, C_GREEN2);
  for (int i = 0; i < steps; i++) {
    float a = (-90 + i * 6.0f) * PI / 180.0f;
    gfx->fillCircle(cx + (r + 2) * cos(a), cy + (r + 2) * sin(a), 3, C_GOLD);
  }

  // 2. Licht beige binnenste knop hertekenen
  gfx->fillCircle(cx, cy, r - 6, C_BEIGE);
  gfx->drawCircle(cx, cy, r - 6, C_GOLD);

  // 3. Tellergetal in diepgroen op de licht beige knop
  char buf[12];
  snprintf(buf, sizeof(buf), "%u", c);
  int16_t x1, y1;
  uint16_t w, h;
  gfx->setTextSize(4);
  gfx->getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  gfx->setTextColor(C_GREEN);
  gfx->setCursor(cx - w / 2, cy - h / 2 - 4);
  gfx->print(buf);

  // 4. Doel / target op de knop
  snprintf(buf, sizeof(buf), "/ %u", d.target);
  gfx->setTextSize(1);
  gfx->getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
  gfx->setTextColor(C_GREEN);
  gfx->setCursor(cx - w / 2, cy + 22);
  gfx->print(buf);
}

void incrementCurrentDhikr() {
  counts[current]++;
  if (counts[current] > ADHKAR[current].target) counts[current] = 1;
  save();
  updateTesbihCounter();
  gfx->flush();

#if HAS_BLE_KEYBOARD
  if (liveSync && bleKeyboard.isConnected()) {
    char liveMsg[32];
    snprintf(liveMsg, sizeof(liveMsg), "esp32 live %c\n", ADHKAR[current].keyChar);
    bleKeyboard.print(liveMsg);
  }
#endif
}

void drawMarqueeLine(const char *text, int y, uint16_t color, int offset) {
  int16_t x1, y1;
  uint16_t w, h;
  gfx->setTextSize(1);
  gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  if (w <= 156) {
    gfx->setTextColor(color);
    gfx->setCursor((172 - w) / 2, y);
    gfx->print(text);
  } else {
    int spacing = 36;
    int totalSpan = w + spacing;
    int shift = offset % totalSpan;
    int x = 8 - shift;

    gfx->setTextColor(color);
    gfx->setCursor(x, y);
    gfx->print(text);

    if (x + w < 164) {
      gfx->setCursor(x + w + 10, y);
      gfx->print("*");
      gfx->setCursor(x + totalSpan, y);
      gfx->print(text);
    }
  }
}

void renderMarqueeArea(int offset) {
  const Dhikr &d = ADHKAR[current];
  gfx->fillRect(0, 222, 172, 60, C_BG);
  drawMarqueeLine(d.phrase, 228, C_GOLD, offset);
  drawMarqueeLine(d.meaning, 250, C_CREAM, offset);
}

void updateMarquee() {
  if (page != TESBIH || dirty || touchActive) return;
  static uint32_t lastMarqueeTime = 0;
  static int lastCurrent = -1;
  static int marqueeTicks = 0;
  uint32_t now = millis();

  if (current != lastCurrent) {
    lastCurrent = current;
    marqueeTicks = 0;
  }

  if (now - lastMarqueeTime < 35) return;
  lastMarqueeTime = now;
  marqueeTicks++;

  int offset = (marqueeTicks > 35) ? (marqueeTicks - 35) : 0;

  renderMarqueeArea(offset);
  tft->draw16bitRGBBitmap(0, 222, gfx->getFramebuffer() + 222 * 172, 172, 60);
}

void drawTesbih() {
  drawFrame();
  const Dhikr &d = ADHKAR[current];
  centerText("TESBIH", 18, 1, C_GOLD);

#if HAS_BLE_KEYBOARD
  if (bleKeyboard.isConnected()) {
    gfx->setTextSize(1);
    gfx->setTextColor(C_GOLD);
    gfx->setCursor(96, 18);
    gfx->print(liveSync ? "BT LIVE" : "BT CACHE");
    gfx->fillCircle(160, 21, 3, liveSync ? C_GOLD : C_MUTED);
  }
#endif

  centerText(d.title, 34, 2, C_WHITE);

  const int cx = 86, cy = 145, r = 62;
  gfx->drawCircle(cx, cy, r + 2, C_GREEN2);

  updateTesbihCounter();

  renderMarqueeArea(0);
  centerText("tik: +1  omhoog: BT sync", 294, 1, C_MUTED);
}

void drawAdhkar() {
  drawFrame();

  for (int i = 0; i < N; i++) {
    int y = 38 + i * 44 - adhkarScrollY;
    if (y < -38 || y > 320) continue;

    bool sel = (i == current);
    gfx->fillRoundRect(8, y, 150, 38, 10, sel ? C_BEIGE : C_CARD);
    gfx->drawRoundRect(8, y, 150, 38, 10, sel ? C_GOLD : C_GREEN2);
    gfx->setTextSize(1);
    gfx->setTextColor(sel ? C_GREEN : C_GOLD);
    gfx->setCursor(16, y + 8);
    gfx->print(ADHKAR[i].title);
    gfx->setTextColor(sel ? C_GREEN : C_CREAM);
    gfx->setCursor(16, y + 22);
    gfx->print(counts[i]);
    gfx->print(" / ");
    gfx->print(ADHKAR[i].target);
  }

  // Scrollbar indicator
  int maxScroll = max(1, N * 44 - 245);
  int trackH = 245;
  int barH = max(24, (245 * 245) / (N * 44));
  int barY = 36 + (adhkarScrollY * (trackH - barH)) / maxScroll;
  gfx->fillRoundRect(164, barY, 4, barH, 2, C_GOLD);

  // Vaste koptekst
  gfx->fillRect(0, 0, 172, 34, C_BG);
  centerText("ADHKAR (10)", 18, 1, C_GOLD);

  // Vaste voettekst
  gfx->fillRect(0, 286, 172, 34, C_BG);
  centerText("tik: kies  sleep: scroll", 298, 1, C_MUTED);
}

void updateBrightnessUI() {
  int pct = (brightness * 100 + 127) / 255;
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);

  gfx->fillRect(20, 65, 132, 45, C_BG);
  centerText(buf, 72, 4, C_WHITE);

  const int tx = 20, ty = 135, tw = 132, th = 26;
  int fillW = map(brightness, 15, 255, 14, tw);
  fillW = constrain(fillW, 14, tw);

  gfx->fillRoundRect(tx, ty, tw, th, 13, C_CARD);
  gfx->fillRoundRect(tx, ty, fillW, th, 13, C_GOLD);
  gfx->drawRoundRect(tx, ty, tw, th, 13, C_GOLD);

  int kx = tx + fillW - 7;
  kx = constrain(kx, tx + 13, tx + tw - 13);
  gfx->fillCircle(kx, ty + th / 2, 11, C_BEIGE);
  gfx->drawCircle(kx, ty + th / 2, 11, C_GOLD);
}

void drawBrightness() {
  drawFrame();
  centerText("HELDERHEID", 22, 1, C_GOLD);
  centerText("Schermverlichting", 40, 1, C_CREAM);

  updateBrightnessUI();

  gfx->fillRoundRect(20, 185, 60, 52, 12, C_BEIGE);
  gfx->drawRoundRect(20, 185, 60, 52, 12, C_GOLD);
  gfx->setTextSize(4);
  gfx->setTextColor(C_GREEN);
  gfx->setCursor(43, 195);
  gfx->print("-");

  gfx->fillRoundRect(92, 185, 60, 52, 12, C_BEIGE);
  gfx->drawRoundRect(92, 185, 60, 52, 12, C_GOLD);
  gfx->setTextSize(4);
  gfx->setTextColor(C_GREEN);
  gfx->setCursor(113, 195);
  gfx->print("+");

  centerText("Sleep slider of tik -/+", 260, 1, C_CREAM);
  centerText("veeg voor sync", 295, 1, C_MUTED);
}

// Tekent de Bluetooth sync pagina voor synchronisatie naar de iPhone app
void drawSync() {
  drawFrame();
  centerText("BLUETOOTH SYNC", 14, 1, C_GOLD);
  centerText("Draag dhikr over naar app", 28, 1, C_CREAM);

  // Kaart met overzicht van alle 10 dhikr-tellingen
  gfx->fillRoundRect(10, 36, 152, 152, 8, C_CARD);
  gfx->drawRoundRect(10, 36, 152, 152, 8, C_GREEN2);

  char lineBuf[24];
  for (int i = 0; i < N; i++) {
    int rowY = 39 + i * 14;
    gfx->setTextSize(1);
    gfx->setTextColor(C_WHITE);
    gfx->setCursor(14, rowY);
    char shortName[16];
    strncpy(shortName, ADHKAR[i].title, 13);
    shortName[13] = '\0';
    gfx->print(shortName);

    snprintf(lineBuf, sizeof(lineBuf), "%u", counts[i]);
    int16_t x1, y1; uint16_t w, h;
    gfx->getTextBounds(lineBuf, 0, 0, &x1, &y1, &w, &h);
    gfx->setTextColor(C_GOLD);
    gfx->setCursor(156 - w, rowY);
    gfx->print(lineBuf);
  }

#if HAS_BLE_KEYBOARD
  if (bleKeyboard.isConnected()) {
    centerText("Bluetooth: Verbonden", 193, 1, C_WHITE);
  } else {
    centerText("Bluetooth: Niet verbonden", 193, 1, C_CREAM);
  }
#else
  centerText("Bluetooth niet actief", 193, 1, C_CREAM);
#endif

  // Knop 1: Stuur direct via Bluetooth naar de geopende webapp
  gfx->fillRoundRect(12, 206, 148, 44, 10, C_BEIGE);
  centerText("STUUR VIA BT", 218, 1, C_GREEN);
  centerText("Naar geopende webapp", 234, 1, C_GREEN);

  // Knop 2: Handmatig tellers legen
  gfx->fillRoundRect(12, 256, 148, 32, 8, C_CARD);
  gfx->drawRoundRect(12, 256, 148, 32, 8, C_GOLD);
  centerText("RESET TELLERS", 267, 1, C_GOLD);

  centerText("veeg omlaag = terug", 298, 1, C_MUTED);
}

void sendBatchBle() {
#if HAS_BLE_KEYBOARD
  if (!bleKeyboard.isConnected()) {
    gfx->fillRect(10, 204, 152, 50, C_BG);
    centerText("BT NIET VERBONDEN!", 218, 1, C_WHITE);
    centerText("Koppel in iPhone BT", 234, 1, C_GOLD);
    gfx->flush();
    delay(1400);
    drawSync();
    gfx->flush();
    return;
  }
  char msg[140];
  snprintf(msg, sizeof(msg),
           "esp32 s%u a%u k%u i%u t%u w%u b%u z%u h%u u%u\n",
           counts[0], counts[1], counts[2], counts[3], counts[4],
           counts[5], counts[6], counts[7], counts[8], counts[9]);
  bleKeyboard.print(msg);

  gfx->fillRoundRect(12, 206, 148, 44, 10, C_GOLD);
  centerText("VERZONDEN!", 218, 1, C_GREEN);
  centerText("Tellers geleegd naar 0", 234, 1, C_GREEN);
  gfx->flush();

  for (int i = 0; i < N; i++) counts[i] = 0;
  save();
  delay(1400);
  drawSync();
  gfx->flush();
#endif
}

void renderPageOnly() {
  switch (page) {
    case HOME: drawHome(); break;
    case TESBIH: drawTesbih(); break;
    case ADHKAR_LIST: drawAdhkar(); break;
    case BRIGHTNESS: drawBrightness(); break;
    case SYNC: drawSync(); break;
  }
}

void render() {
  renderPageOnly();
  gfx->flush();
}

void transitionTo(Page next, TransitionDir dir = TRANS_NONE) {
  if (next == page && dir != TRANS_NONE) return;

  uint16_t *currentFb = gfx->getFramebuffer();
  if (dir == TRANS_NONE || !oldBuf || !currentFb) {
    page = next;
    render();
    return;
  }

  // 1. Bewaar huidige schermweergave in transitiebuffer
  memcpy(oldBuf, currentFb, 172 * 320 * sizeof(uint16_t));

  // 2. Teken de nieuwe pagina in het canvasgeheugen
  page = next;
  renderPageOnly();
  uint16_t *newBuf = gfx->getFramebuffer();

  // 3. Vloeiende glijdende transitie (hardware-geclipt via draw16bitRGBBitmap)
  if (dir == TRANS_LEFT) {
    // Oud schuift naar links (-172), nieuw komt binnen vanaf rechts (172 -> 0)
    const int steps[] = {25, 55, 95, 135, 160, 172};
    for (int i = 0; i < 6; i++) {
      int shift = steps[i];
      tft->draw16bitRGBBitmap(-shift, 0, oldBuf, 172, 320);
      tft->draw16bitRGBBitmap(172 - shift, 0, newBuf, 172, 320);
    }
  } else if (dir == TRANS_RIGHT) {
    // Oud schuift naar rechts (+172), nieuw komt binnen vanaf links (-172 -> 0)
    const int steps[] = {25, 55, 95, 135, 160, 172};
    for (int i = 0; i < 6; i++) {
      int shift = steps[i];
      tft->draw16bitRGBBitmap(shift, 0, oldBuf, 172, 320);
      tft->draw16bitRGBBitmap(-172 + shift, 0, newBuf, 172, 320);
    }
  } else if (dir == TRANS_UP) {
    // Omhoog swipen naar BT Sync: oud schuift omhoog (-320), nieuw komt vanaf onder (320 -> 0)
    const int steps[] = {35, 85, 150, 220, 275, 305, 320};
    for (int i = 0; i < 7; i++) {
      int shift = steps[i];
      tft->draw16bitRGBBitmap(0, -shift, oldBuf, 172, 320);
      tft->draw16bitRGBBitmap(0, 320 - shift, newBuf, 172, 320);
    }
  } else if (dir == TRANS_DOWN) {
    // Omlaag swipen vanaf BT Sync: oud schuift omlaag (+320), nieuw komt vanaf boven (-320 -> 0)
    const int steps[] = {35, 85, 150, 220, 275, 305, 320};
    for (int i = 0; i < 7; i++) {
      int shift = steps[i];
      tft->draw16bitRGBBitmap(0, shift, oldBuf, 172, 320);
      tft->draw16bitRGBBitmap(0, -320 + shift, newBuf, 172, 320);
    }
  }
}

void onShortPress() {
  switch (page) {
    case HOME:
      transitionTo(TESBIH, TRANS_LEFT);
      return;
    case TESBIH:
      incrementCurrentDhikr();
      return;
    case ADHKAR_LIST:
      current = (current + 1) % N;
      save();
      dirty = true;
      break;
    case BRIGHTNESS:
      brightness = (brightness > 180) ? 100 : 240;
      applyBrightness(brightness);
      updateBrightnessUI();
      gfx->flush();
      prefs.putUChar("bright", brightness);
      return;
    case SYNC:
      sendBatchBle();
      return;
  }
}

void powerOff() {
  gfx->fillScreen(C_BG);
  centerText("Mijn weg naar Jannah", 120, 1, C_CREAM);
  centerText("Uitschakelen...", 145, 2, C_GOLD);
  centerText("Druk BOOT om in te schakelen", 185, 1, C_MUTED);
  gfx->flush();
  save();
  delay(1200);

  digitalWrite(PIN_BL, LOW);
  tft->startWrite();
  bus->writeCommand(0x10); // ST7789 sleep in
  tft->endWrite();

  while (digitalRead(PIN_BTN) == LOW) {
    delay(20);
  }
  delay(200);

  esp_sleep_enable_ext1_wakeup(1ULL << PIN_BTN, ESP_EXT1_WAKEUP_ANY_LOW);
  esp_deep_sleep_start();
}

// ---- Touch (AXS5106L, I2C) ----
#define TP_SDA 42
#define TP_SCL 41
#define TP_RST 47
#define TP_ADDR 0x63

bool readTouch(int &x, int &y) {
  Wire.beginTransmission(TP_ADDR);
  Wire.write(0x01);
  if (Wire.endTransmission(true) != 0) return false;
  uint8_t d[14];
  if (Wire.requestFrom((uint8_t)TP_ADDR, (uint8_t)14) != 14) return false;
  for (int i = 0; i < 14; i++) d[i] = Wire.read();
  if ((d[1] & 0x0F) == 0) return false;
  x = 172 - (((d[2] & 0x0F) << 8) | d[3]);
  y = ((d[4] & 0x0F) << 8) | d[5];
  x = constrain(x, 0, 171);
  y = constrain(y, 0, 319);
  return true;
}

void touchInit() {
  Wire.begin(TP_SDA, TP_SCL, 400000);
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);
  delay(10);
  digitalWrite(TP_RST, HIGH);
  delay(60);
}

void changePage(int dir, TransitionDir tdir = TRANS_NONE) {
  Page next = (Page)((page + NUM_PAGES + dir) % NUM_PAGES);
  transitionTo(next, tdir);
}

void onTap(int x, int y) {
  switch (page) {
    case HOME:
      transitionTo(TESBIH, TRANS_LEFT);
      return;
    case TESBIH:
      // Tik op de BT status knop rechtsboven (toggle live / cache)
      if (y < 34 && x >= 90) {
        liveSync = !liveSync;
        drawTesbih();
        gfx->flush();
        return;
      }
      onShortPress();
      return;
    case ADHKAR_LIST: {
      int clickedY = y - 38 + adhkarScrollY;
      int i = clickedY / 44;
      if (i >= 0 && i < N) {
        current = i;
        save();
        transitionTo(TESBIH, TRANS_LEFT);
      }
      return;
    }
    case BRIGHTNESS: {
      if (y >= 120 && y <= 170) {
        int newB = map(x, 20, 152, 15, 255);
        applyBrightness(newB);
        updateBrightnessUI();
        gfx->flush();
        prefs.putUChar("bright", brightness);
      } else if (x >= 20 && x <= 80 && y >= 185 && y <= 237) {
        int newB = (brightness > 40) ? (brightness - 25) : 15;
        applyBrightness(newB);
        updateBrightnessUI();
        gfx->flush();
        prefs.putUChar("bright", brightness);
      } else if (x >= 92 && x <= 152 && y >= 185 && y <= 237) {
        int newB = (brightness < 230) ? (brightness + 25) : 255;
        applyBrightness(newB);
        updateBrightnessUI();
        gfx->flush();
        prefs.putUChar("bright", brightness);
      }
      return;
    }
    case SYNC: {
      // Tik op "STUUR VIA BT" knop
      if (y >= 206 && y <= 250) {
        sendBatchBle();
        return;
      }
      // Tik op "RESET TELLERS" knop
      if (y >= 256 && y <= 292) {
        for (int i = 0; i < N; i++) counts[i] = 0;
        save();
        drawSync();
        gfx->flush();
        return;
      }
      return;
    }
  }
}

void handleTouch() {
  static bool down = false, moved = false, draggingSlider = false, scrollingList = false;
  static int sx, sy, lx, ly, initialScrollY = 0;
  static uint32_t downAt = 0, lastSeen = 0;
  int x, y;
  uint32_t now = millis();
  if (readTouch(x, y)) {
    touchActive = true;
    lastSeen = now;
    if (!down) {
      down = true;
      moved = false;
      draggingSlider = (page == BRIGHTNESS && y >= 120 && y <= 175);
      scrollingList = (page == ADHKAR_LIST && y >= 35 && y <= 285);
      initialScrollY = adhkarScrollY;
      sx = x;
      sy = y;
      downAt = now;
    }
    lx = x;
    ly = y;
    if (abs(lx - sx) > 12 || abs(ly - sy) > 12) moved = true;

    if (draggingSlider && page == BRIGHTNESS) {
      int newB = map(x, 20, 152, 15, 255);
      newB = constrain(newB, 15, 255);
      if (newB != brightness) {
        applyBrightness(newB);
        updateBrightnessUI();
        gfx->flush();
      }
    } else if (scrollingList && page == ADHKAR_LIST) {
      int deltaY = ly - sy;
      int maxScroll = max(0, N * 44 - 245);
      int newScroll = constrain(initialScrollY - deltaY, 0, maxScroll);
      if (newScroll != adhkarScrollY) {
        adhkarScrollY = newScroll;
        drawAdhkar();
        gfx->flush();
      }
    }
  } else if (down && now - lastSeen > 60) {
    down = false;
    touchActive = false;
    if (draggingSlider) {
      prefs.putUChar("bright", brightness);
      draggingSlider = false;
      return;
    }
    if (scrollingList) {
      scrollingList = false;
      if (abs(ly - sy) > 15) {
        return;
      }
    }
    int dx = lx - sx, dy = ly - sy;
    int adx = abs(dx), ady = abs(dy);

    // 1. VERTICAAL SWIPEN (adx is klein, ady > 35)
    if (ady > 35 && ady > adx) {
      if (dy < 0) {
        // Omhoog swipen: direct naar de "stuur via BT" pagina (SYNC)!
        if (page != SYNC) {
          lastPageBeforeSync = page;
          transitionTo(SYNC, TRANS_UP);
        }
      } else {
        // Omlaag swipen: terug vanaf de BT Sync pagina naar vorige pagina
        if (page == SYNC) {
          transitionTo(lastPageBeforeSync, TRANS_DOWN);
        }
      }
    }
    // 2. HORIZONTAAL SWIPEN (ady is klein, adx > 35)
    else if (adx > 35 && adx > ady) {
      if (dx < 0) {
        // Veeg links -> volgende pagina
        changePage(1, TRANS_LEFT);
      } else {
        // Veeg rechts -> vorige pagina
        changePage(-1, TRANS_RIGHT);
      }
    }
    // 3. TIK OF LANG INGEDRUKT
    else if (!moved) {
      if (page == TESBIH && now - downAt > 800) {
        counts[current] = 0;  // Lang vasthouden = teller resetten
        save();
        updateTesbihCounter();
        gfx->flush();
      } else {
        onTap(sx, sy);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("Jannah: start");
  pinMode(PIN_BTN, INPUT_PULLUP);
  prefs.begin("jannah", false);
  size_t savedLen = prefs.getBytesLength("counts");
  if (savedLen == sizeof(counts)) {
    prefs.getBytes("counts", counts, sizeof(counts));
  } else if (savedLen > 0 && savedLen < sizeof(counts)) {
    prefs.getBytes("counts", counts, savedLen);
  }
  current = constrain(prefs.getInt("cur", 0), 0, N - 1);
  brightness = prefs.getUChar("bright", 220);
  applyBrightness(brightness);
  Serial.println("Jannah: backlight gereed");
  Serial.println("Jannah: scherm starten");
  tft->begin();
  gfx->begin();
  tft->startWrite();
  bus->writeCommand(0x36);
  bus->write(0x40 | (BGR_ORDER ? 0x08 : 0x00));
  tft->endWrite();

  if (psramFound()) {
    oldBuf = (uint16_t *)ps_malloc(172 * 320 * sizeof(uint16_t));
  }
  if (!oldBuf) {
    oldBuf = (uint16_t *)malloc(172 * 320 * sizeof(uint16_t));
  }
  if (oldBuf) {
    Serial.println("Jannah: transitie-buffer gealloceerd");
  } else {
    Serial.println("Jannah: WAARSCHUWING - geen geheugen voor transitie-buffer");
  }
  Serial.println("Jannah: scherm klaar");
  touchInit();

#if HAS_BLE_KEYBOARD
  bleKeyboard.begin();
  Serial.println("Jannah: Bluetooth BLE Keyboard gestart ('Jannah Tasbih')");
#else
  Serial.println("Jannah: BLE Keyboard niet geladen (installeer 'ESP32 BLE Keyboard' via Library Manager)");
#endif
}

void loop() {
  static uint32_t downAt = 0;
  static bool down = false, longFired = false;
  bool pressed = digitalRead(PIN_BTN) == LOW;
  uint32_t now = millis();

  if (pressed && !down) {
    down = true;
    longFired = false;
    downAt = now;
  } else if (pressed && down && !longFired && now - downAt >= 3000) {
    longFired = true;
    powerOff(); // 3 seconden indrukken = uitschakelen naar deep sleep
  } else if (!pressed && down) {
    down = false;
    if (!longFired && now - downAt > 30) {
      // Kort indrukken = Tesbih +1!
      if (page != TESBIH) {
        transitionTo(TESBIH, TRANS_LEFT);
      }
      incrementCurrentDhikr();
    }
  }

  handleTouch();

  if (dirty) {
    dirty = false;
    render();
  } else {
    updateMarquee();
  }
  delay(5);
}
