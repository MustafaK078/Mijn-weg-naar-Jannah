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
#include "qrcode.h"

// Webapp URL voor de QR-code overdracht (pas aan indien nodig)
const char *APP_SYNC_URL = "https://mustafak078.github.io/Mijn-weg-naar-Jannah/#esp32:";

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
Arduino_ST7789 *gfx = new Arduino_ST7789(bus, PIN_RST, 0 /* portrait */, false /* IPS false */,
                                         172, 320, 34, 0, 34, 0);

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
};

const Dhikr ADHKAR[] = {
  {"Subhanallah", "Subhan Allah", "Gezegend is Allah", 33},
  {"Alhamdulillah", "Alhamdu lillah", "Alle lof is voor Allah", 33},
  {"Allahu Akbar", "Allahu Akbar", "Allah is de Grootste", 34},
  {"Istighfar", "Astaghfirullah", "Ik zoek vergeving bij Allah", 100},
  {"Tahlil", "La ilaha illallah", "Er is geen god dan Allah", 100},
  {"Salawat", "Allahumma salli ala Muhammad", "O Allah, zegen Muhammad", 100},
};
const int N = sizeof(ADHKAR) / sizeof(ADHKAR[0]);

enum Page { HOME, TESBIH, ADHKAR_LIST, BRIGHTNESS, SYNC };
const int NUM_PAGES = 5;
Page page = HOME;
int current = 0;
uint16_t counts[N] = {0};
uint8_t brightness = 200;  // Helderheid 15..255
Preferences prefs;
bool dirty = true;

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

void drawTesbih() {
  drawFrame();
  const Dhikr &d = ADHKAR[current];
  centerText("TESBIH", 18, 1, C_GOLD);

#if HAS_BLE_KEYBOARD
  if (bleKeyboard.isConnected()) {
    gfx->setTextSize(1);
    gfx->setTextColor(C_GOLD);
    gfx->setCursor(136, 18);
    gfx->print("BT");
    gfx->fillCircle(154, 21, 3, C_GOLD);
  }
#endif

  centerText(d.title, 34, 2, C_WHITE);

  const int cx = 86, cy = 145, r = 62;
  gfx->drawCircle(cx, cy, r + 2, C_GREEN2);

  updateTesbihCounter();

  centerText(d.phrase, 230, 1, C_GOLD);
  centerText(d.meaning, 248, 1, C_CREAM);
  centerText("tik: +1   veeg: pagina", 290, 1, C_MUTED);
}

void drawAdhkar() {
  drawFrame();
  centerText("ADHKAR", 18, 1, C_GOLD);
  for (int i = 0; i < N; i++) {
    int y = 36 + i * 44;
    bool sel = (i == current);
    gfx->fillRoundRect(8, y, 156, 38, 10, sel ? C_BEIGE : C_CARD);
    gfx->drawRoundRect(8, y, 156, 38, 10, sel ? C_GOLD : C_GREEN2);
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
  centerText("tik: kies   veeg: pagina", 300, 1, C_MUTED);
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

// Tekent de QR-code pagina voor synchronisatie naar de iPhone app
void drawSync() {
  drawFrame();
  centerText("SYNC NAAR APP", 14, 2, C_GOLD);
  centerText("Scan met iPhone camera", 34, 1, C_CREAM);

  char url[170];
  snprintf(url, sizeof(url),
           "%ss=%u,a=%u,k=%u,i=%u,t=%u,w=%u",
           APP_SYNC_URL,
           counts[0], counts[1], counts[2],
           counts[3], counts[4], counts[5]);

  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(5)];
  int8_t err = qrcode_initText(&qrcode, qrcodeData, 5, ECC_LOW, url);

  if (err == 0) {
    const uint8_t scale = 4;
    const int qrW = qrcode.size * scale;   // 37 * 4 = 148 px
    const int startX = (172 - qrW) / 2;    // 12 px marge links/rechts
    const int startY = 48;

    // Witte achtergrondplaat met afgeronde hoeken
    gfx->fillRoundRect(startX - 4, startY - 4, qrW + 8, qrW + 8, 8, C_WHITE);

    // QR-modules tekenen
    for (uint8_t y = 0; y < qrcode.size; y++) {
      for (uint8_t x = 0; x < qrcode.size; x++) {
        if (qrcode_getModule(&qrcode, x, y)) {
          gfx->fillRect(startX + x * scale, startY + y * scale, scale, scale, 0x0000 /* zwart */);
        }
      }
    }
  } else {
    centerText("QR genereren mislukt", 100, 1, C_GOLD);
  }

  uint32_t total = counts[0] + counts[1] + counts[2] + counts[3] + counts[4] + counts[5];
  char sumBuf[32];
  snprintf(sumBuf, sizeof(sumBuf), "Totaal: %u dhikr", total);
  centerText(sumBuf, 206, 1, C_GOLD);

#if HAS_BLE_KEYBOARD
  if (bleKeyboard.isConnected()) {
    centerText("Bluetooth: Verbonden", 222, 1, C_WHITE);
  } else {
    centerText("Bluetooth: Gereed voor koppeling", 222, 1, C_CREAM);
  }
#else
  centerText("Klaar om te scannen", 222, 1, C_CREAM);
#endif

  // Knop om tellers te resetten na sync
  gfx->fillRoundRect(16, 238, 140, 46, 10, C_CARD);
  gfx->drawRoundRect(16, 238, 140, 46, 10, C_GOLD);
  centerText("RESET TELLERS", 246, 1, C_GOLD);
  centerText("Tik na sync om te legen", 262, 1, C_CREAM);

  centerText("veeg voor home", 298, 1, C_MUTED);
}

void render() {
  switch (page) {
    case HOME: drawHome(); break;
    case TESBIH: drawTesbih(); break;
    case ADHKAR_LIST: drawAdhkar(); break;
    case BRIGHTNESS: drawBrightness(); break;
    case SYNC: drawSync(); break;
  }
}

void transitionTo(Page next) {
  if (next == page) return;
  for (int y = 0; y < 320; y += 32) {
    gfx->fillRect(0, y, 172, 32, C_CARD);
    delay(8);
  }
  page = next;
  render();
}

void save() {
  prefs.putBytes("counts", counts, sizeof(counts));
  prefs.putInt("cur", current);
}

void onShortPress() {
  switch (page) {
    case HOME:
      transitionTo(TESBIH);
      return;
    case TESBIH:
      counts[current]++;
      if (counts[current] > ADHKAR[current].target) counts[current] = 1;
      save();
      updateTesbihCounter();
#if HAS_BLE_KEYBOARD
      if (bleKeyboard.isConnected()) {
        // Verstuur '1'..'6' via BLE toetsenbord zodat de iPhone webapp meteen meetelt
        bleKeyboard.write('1' + current);
      }
#endif
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
      prefs.putUChar("bright", brightness);
      return;
    case SYNC:
      return;
  }
}

void onLongPress() {
  transitionTo((Page)((page + 1) % NUM_PAGES));
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

void changePage(int dir) {
  transitionTo((Page)((page + NUM_PAGES + dir) % NUM_PAGES));
}

void onTap(int x, int y) {
  switch (page) {
    case HOME:
      transitionTo(TESBIH);
      return;
    case TESBIH:
      onShortPress();
      return;
    case ADHKAR_LIST: {
      int i = (y - 36) / 44;
      if (i >= 0 && i < N) {
        current = i;
        save();
        transitionTo(TESBIH);
      }
      return;
    }
    case BRIGHTNESS: {
      if (y >= 120 && y <= 170) {
        int newB = map(x, 20, 152, 15, 255);
        applyBrightness(newB);
        updateBrightnessUI();
        prefs.putUChar("bright", brightness);
      } else if (x >= 20 && x <= 80 && y >= 185 && y <= 237) {
        int newB = (brightness > 40) ? (brightness - 25) : 15;
        applyBrightness(newB);
        updateBrightnessUI();
        prefs.putUChar("bright", brightness);
      } else if (x >= 92 && x <= 152 && y >= 185 && y <= 237) {
        int newB = (brightness < 230) ? (brightness + 25) : 255;
        applyBrightness(newB);
        updateBrightnessUI();
        prefs.putUChar("bright", brightness);
      }
      return;
    }
    case SYNC: {
      // Tik op de "RESET TELLERS" knop
      if (x >= 16 && x <= 156 && y >= 238 && y <= 286) {
        for (int i = 0; i < N; i++) counts[i] = 0;
        save();
        drawSync();
      }
      return;
    }
  }
}

void handleTouch() {
  static bool down = false, moved = false, draggingSlider = false;
  static int sx, sy, lx, ly;
  static uint32_t downAt = 0, lastSeen = 0;
  int x, y;
  uint32_t now = millis();
  if (readTouch(x, y)) {
    lastSeen = now;
    if (!down) {
      down = true;
      moved = false;
      draggingSlider = (page == BRIGHTNESS && y >= 120 && y <= 175);
      sx = x;
      sy = y;
      downAt = now;
    }
    lx = x;
    ly = y;
    if (abs(lx - sx) > 20 || abs(ly - sy) > 20) moved = true;

    if (draggingSlider && page == BRIGHTNESS) {
      int newB = map(x, 20, 152, 15, 255);
      newB = constrain(newB, 15, 255);
      if (newB != brightness) {
        applyBrightness(newB);
        updateBrightnessUI();
      }
    }
  } else if (down && now - lastSeen > 60) {
    down = false;
    if (draggingSlider) {
      prefs.putUChar("bright", brightness);
      draggingSlider = false;
      return;
    }
    int dx = lx - sx, dy = ly - sy;
    if (abs(dx) > 50 && abs(dx) > abs(dy)) {
      changePage(dx < 0 ? 1 : -1);
    } else if (!moved) {
      if (page == TESBIH && now - downAt > 800) {
        counts[current] = 0;  // Lang vasthouden = teller resetten
        save();
        updateTesbihCounter();
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
  if (prefs.getBytesLength("counts") == sizeof(counts)) prefs.getBytes("counts", counts, sizeof(counts));
  current = constrain(prefs.getInt("cur", 0), 0, N - 1);
  brightness = prefs.getUChar("bright", 220);
  applyBrightness(brightness);
  Serial.println("Jannah: backlight gereed");
  Serial.println("Jannah: scherm starten");
  gfx->begin();
  gfx->startWrite();
  bus->writeCommand(0x36);
  bus->write(0x40 | (BGR_ORDER ? 0x08 : 0x00));
  gfx->endWrite();
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
  } else if (pressed && down && !longFired && now - downAt > 700) {
    longFired = true;
    onLongPress();
  } else if (!pressed && down) {
    down = false;
    if (!longFired && now - downAt > 30) onShortPress();
  }

  handleTouch();

  if (dirty) {
    dirty = false;
    render();
  }
  delay(5);
}
