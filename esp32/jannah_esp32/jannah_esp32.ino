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
#include <soc/usb_serial_jtag_struct.h>

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
#define PIN_BAT_ADC 12 // Batterijspanningsmeting via interne spanningsdeler (R19=200K, R21=100K)

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
const uint16_t C_BLUE   = RGB(0, 150, 255);      // Helder Bluetooth-blauw statusicoon
const uint16_t C_BAT_GREEN = RGB(52, 215, 80);   // Apple oplaadgroen (#34C759)
const uint16_t C_BAT_RED   = RGB(255, 60, 60);   // Apple laag-batterij rood (#FF3B30)

struct Dhikr {
  const char *title;
  const char *desc;
  uint16_t target;
  char keyChar;
};

const Dhikr ADHKAR[] = {
  {"Subhanallah", "Verheven is Allah boven iedere tekortkoming", 33, 's'},
  {"Alhamdulillah", "Alle lof en dank behoort toe aan Allah", 33, 'a'},
  {"Allahu Akbar", "Allah is de Grootste boven alles", 34, 'k'},
  {"Istighfar", "Ik vraag Allah om vergeving en keer berouwvol tot Hem", 100, 'i'},
  {"Tahlil", "Er is geen godheid die het recht heeft aanbeden te worden behalve Allah", 100, 't'},
  {"Salawat", "O Allah, schenk Uw zegeningen en vrede aan Muhammad en zijn familie", 100, 'w'},
  {"SubhanAllah wa-bihamdihi", "Verheven is Allah en alle lof behoort Hem toe - wist zonden als schuim van de zee", 100, 'b'},
  {"SubhanAllah l-Azim wa-bihamdihi", "Verheven is Allah de Geweldige, en aan Hem behoort alle lof", 100, 'z'},
  {"La hawla wa-la quwwata illa billah", "Er is geen macht en geen kracht behalve door Allah - een schat uit het Paradijs", 100, 'h'},
  {"La ilaha illa Allah wahdahu la sharika lah", "Niemand heeft het recht aanbeden te worden behalve Allah alleen, zonder deelgenoot. Aan Hem behoort het koninkrijk en alle lof, en Hij is over alles machtig", 100, 'u'},
};
const int N = sizeof(ADHKAR) / sizeof(ADHKAR[0]);

enum Page { HOME, TESBIH, ADHKAR_LIST, BRIGHTNESS, SYNC };
const int NUM_PAGES = 5;
Page page = HOME;
Page lastPageBeforeSync = TESBIH;
int current = 0;
uint16_t counts[N] = {0};
uint8_t brightness = 200;  // Helderheid 15..255
int adhkarScrollY = 0;      // Scrollpositie voor de lijst met 10 adhkar
bool touchActive = false;   // Vlag of scherm momenteel wordt aangeraakt
bool bleActive = false;     // Bluetooth staat standaard UIT om batterij te sparen
uint32_t lastActivityTime = 0;
const uint32_t AUTO_OFF_TIMEOUT_MS = 120000; // Automatisch uitschakelen na 2 minuten inactiviteit (120 sec)
Preferences prefs;
bool dirty = true;

// Leest de accuspanning van de LiPo via IO12 (R19=200K, R21=100K spanningsdeler, factor 3.0)
float getBatteryVoltage() {
  uint32_t sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogReadMilliVolts(PIN_BAT_ADC);
    delayMicroseconds(50);
  }
  float v_adc = (sum / 16.0f) / 1000.0f;
  return v_adc * 3.0f;
}

int getBatteryPercent(float v) {
  if (v <= 2.50f) return 0;  // Geen batterij of leeg
  if (v >= 4.18f) return 100;
  if (v <= 3.40f) return 0;
  if (v > 3.90f) {
    return 75 + (int)((v - 3.90f) / (4.18f - 3.90f) * 25.0f);
  } else if (v > 3.75f) {
    return 35 + (int)((v - 3.75f) / (3.90f - 3.75f) * 40.0f);
  } else {
    return (int)((v - 3.40f) / (3.75f - 3.40f) * 35.0f);
  }
}

bool isCharging() {
  // 1. USB-Serial/JTAG host verbonden
  if (USB_SERIAL_JTAG.int_raw.sof_int_raw) return true;
  // 2. Of spanning is op laadniveau van de ETA6098
  float v = getBatteryVoltage();
  if (v >= 4.18f) return true;
  return false;
}

static float filteredBatteryV = 0.0f;
static int lastDrawnBatteryPct = -1;
static bool lastDrawnBatteryCharging = false;

// Apple-stijl batterij-indicator linksboven
void drawBatteryIcon(bool updateToTft = false) {
  float rawV = getBatteryVoltage();
  if (filteredBatteryV < 1.0f) {
    filteredBatteryV = rawV;
  } else {
    filteredBatteryV = (filteredBatteryV * 0.85f) + (rawV * 0.15f); // Vloeiende filtering tegen ruis
  }

  int pct = getBatteryPercent(filteredBatteryV);
  pct = constrain(pct, 0, 100);
  bool charging = isCharging();

  // Als we alleen een periodieke update doen en er is niets veranderd, niet hertekenen
  if (updateToTft && pct == lastDrawnBatteryPct && charging == lastDrawnBatteryCharging) {
    return;
  }
  lastDrawnBatteryPct = pct;
  lastDrawnBatteryCharging = charging;

  uint16_t color = C_WHITE;
  if (charging) {
    color = C_BAT_GREEN; // Apple groen tijdens opladen
  } else if (pct <= 20) {
    color = C_BAT_RED;   // Apple rood bij laag batterijniveau (<= 20%)
  }

  const int bx = 8, by = 14;

  // Maak batterijzone schoon
  gfx->fillRect(bx, by - 2, 54, 14, C_BG);

  // Apple-stijl batterijlichaam (afgeronde rechthoek)
  gfx->drawRoundRect(bx, by, 18, 9, 2, color);
  // Terminal / pluspool aan de rechterkant
  gfx->fillRect(bx + 18, by + 2, 2, 5, color);

  // Binnenste vulling volgens percentage
  int fillW = map(pct, 0, 100, 0, 14);
  fillW = constrain(fillW, 0, 14);
  if (pct > 0 && fillW == 0) fillW = 1;

  if (fillW > 0) {
    gfx->fillRect(bx + 2, by + 2, fillW, 5, color);
  }

  // Als het oplaadt: klein Apple-bliksempje
  if (charging) {
    gfx->drawPixel(bx + 9, by + 2, C_BG);
    gfx->drawPixel(bx + 8, by + 3, C_BG);
    gfx->drawPixel(bx + 9, by + 4, C_BG);
    gfx->drawPixel(bx + 8, by + 5, C_BG);
    gfx->drawPixel(bx + 10, by + 4, C_BG);
  }

  // Percentage tekst (bijv. 85%)
  char pbuf[8];
  snprintf(pbuf, sizeof(pbuf), "%d%%", pct);
  gfx->setTextSize(1);
  gfx->setTextColor(color);
  gfx->setCursor(bx + 23, by + 1);
  gfx->print(pbuf);

  // PUSH NAAR TFT: met volledige breedte (172 pixels) zodat de framebuffer stride exact klopt en er NOOIT rare tekens/glitches ontstaan
  if (updateToTft && page != BRIGHTNESS) {
    tft->draw16bitRGBBitmap(0, 10, gfx->getFramebuffer() + 10 * 172, 172, 18);
  }
}

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
  drawBatteryIcon(false);
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

  // 1. Gouden progressiestipjes op de buitenrand (voortgang van de huidige ronde)
  uint16_t cyclePos = (d.target > 0) ? (c % d.target) : 0;
  if (cyclePos == 0 && c > 0) cyclePos = d.target;
  float frac = (d.target > 0) ? ((float)cyclePos / d.target) : 1.0f;
  int steps = (int)(frac * 60);
  gfx->drawCircle(cx, cy, r + 2, C_GREEN2);
  for (int i = 0; i < steps; i++) {
    float a = (-90 + i * 6.0f) * PI / 180.0f;
    gfx->fillCircle(cx + (r + 2) * cos(a), cy + (r + 2) * sin(a), 3, C_GOLD);
  }

  // 2. Licht beige binnenste knop hertekenen
  gfx->fillCircle(cx, cy, r - 6, C_BEIGE);
  gfx->drawCircle(cx, cy, r - 6, C_GOLD);

  // 3. Tellergetal in diepgroen op de licht beige knop (totale optelling)
  char buf[12];
  snprintf(buf, sizeof(buf), "%u", c);
  int16_t x1, y1;
  uint16_t w, h;
  gfx->setTextSize(c >= 1000 ? 3 : 4);
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
  if (counts[current] < 65535) counts[current]++;
  save();
  updateTesbihCounter();
  gfx->flush();
}

void renderMarqueeLine(int offset) {
  const Dhikr &d = ADHKAR[current];
  const char *text = d.desc;
  const int lineY = 246;
  const int bandY = 238;
  const int bandH = 20;

  gfx->fillRect(0, bandY, 172, bandH, C_BG);
  gfx->setTextSize(1);
  gfx->setTextColor(C_CREAM);
  gfx->setTextWrap(false);

  int len = strlen(text);
  int textPixelWidth = len * 6;

  if (textPixelWidth <= 156) {
    int x = (172 - textPixelWidth) / 2;
    gfx->setCursor(x, lineY);
    gfx->print(text);
  } else {
    int gap = 36;
    int totalSpan = textPixelWidth + gap;
    int shift = offset % totalSpan;
    int startX = 8 - shift;

    gfx->setCursor(startX, lineY);
    gfx->print(text);

    if (startX + textPixelWidth < 164) {
      gfx->setCursor(startX + textPixelWidth + 10, lineY);
      gfx->print("*");
      gfx->setCursor(startX + totalSpan, lineY);
      gfx->print(text);
    }
  }
}

// Titel-regel: beweegt/scrollt soepel alleen als de titel breder is dan het scherm
void renderTitleLine(int offset) {
  const Dhikr &d = ADHKAR[current];
  const char *text = d.title;
  const int lineY = 34;
  const int bandY = 30;
  const int bandH = 24;

  gfx->fillRect(0, bandY, 172, bandH, C_BG);
  gfx->setTextSize(2);
  gfx->setTextColor(C_WHITE);
  gfx->setTextWrap(false);

  int16_t x1, y1;
  uint16_t tw, th;
  gfx->getTextBounds(text, 0, 0, &x1, &y1, &tw, &th);

  if (tw <= 156) {
    int x = (172 - tw) / 2;
    gfx->setCursor(x, lineY);
    gfx->print(text);
  } else {
    int gap = 36;
    int totalSpan = tw + gap;
    int shift = offset % totalSpan;
    int startX = 8 - shift;

    gfx->setCursor(startX, lineY);
    gfx->print(text);

    if (startX + tw < 164) {
      gfx->setCursor(startX + tw + 10, lineY);
      gfx->print("*");
      gfx->setCursor(startX + totalSpan, lineY);
      gfx->print(text);
    }
  }
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

  // 1. Beschrijving marquee (beneden)
  renderMarqueeLine(offset);
  tft->draw16bitRGBBitmap(0, 238, gfx->getFramebuffer() + 238 * 172, 172, 20);

  // 2. Titel marquee (boven) - beweegt alleen als titel breder is dan 156px
  const Dhikr &d = ADHKAR[current];
  int16_t x1, y1;
  uint16_t tw, th;
  gfx->setTextSize(2);
  gfx->getTextBounds(d.title, 0, 0, &x1, &y1, &tw, &th);
  if (tw > 156) {
    renderTitleLine(offset);
    tft->draw16bitRGBBitmap(0, 30, gfx->getFramebuffer() + 30 * 172, 172, 24);
  }
}

void drawTesbih() {
  drawFrame();
  drawBatteryIcon(false);
  centerText("TESBIH", 18, 1, C_GOLD);

#if HAS_BLE_KEYBOARD
  if (bleActive && bleKeyboard.isConnected()) {
    // Alleen een blauw rondje in de rechterbovenhoek - belemmert de TESBIH tekst nooit
    gfx->fillCircle(158, 20, 4, C_BLUE);
  }
#endif

  renderTitleLine(0);

  const int cx = 86, cy = 145, r = 62;
  gfx->drawCircle(cx, cy, r + 2, C_GREEN2);

  updateTesbihCounter();

  // Enkele regel scrollende volledige omschrijving
  renderMarqueeLine(0);
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
    char cardTitle[24];
    strncpy(cardTitle, ADHKAR[i].title, 20);
    cardTitle[20] = '\0';
    if (strlen(ADHKAR[i].title) > 20) {
      cardTitle[17] = '.';
      cardTitle[18] = '.';
      cardTitle[19] = '.';
    }
    gfx->print(cardTitle);
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
  drawBatteryIcon(false);
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
  drawBatteryIcon(false);
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

void startBle() {
#if HAS_BLE_KEYBOARD
  if (!bleActive) {
    bleKeyboard.begin();
    bleActive = true;
    Serial.println("Jannah: Bluetooth geactiveerd");
  }
#endif
}

void stopBle() {
#if HAS_BLE_KEYBOARD
  if (bleActive) {
    bleKeyboard.end();
    bleActive = false;
    Serial.println("Jannah: Bluetooth uitgeschakeld voor batterijbesparing");
  }
#endif
}

// Tekent de Bluetooth sync pagina voor synchronisatie naar de iPhone app
void drawSync() {
  drawFrame();
  drawBatteryIcon(false);
  centerText("SYNC", 14, 1, C_GOLD);
  centerText("Draag dhikr over naar app", 26, 1, C_CREAM);

  // Kaart met overzicht van alle 10 dhikr-tellingen
  gfx->fillRoundRect(10, 36, 152, 148, 8, C_CARD);
  gfx->drawRoundRect(10, 36, 152, 148, 8, C_GREEN2);

  char lineBuf[24];
  for (int i = 0; i < N; i++) {
    int rowY = 38 + i * 14;
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
  if (!bleActive) {
    centerText("Bluetooth: UIT (Besparing)", 188, 1, C_CREAM);

    // Knop 1: Activeer Bluetooth
    gfx->fillRoundRect(12, 202, 148, 44, 10, C_BEIGE);
    gfx->drawRoundRect(12, 202, 148, 44, 10, C_GOLD);
    centerText("BT AANZETTEN", 214, 1, C_GREEN);
    centerText("Tik om te verbinden", 230, 1, C_GREEN);

    // Knop 2: Reset tellers
    gfx->fillRoundRect(12, 254, 148, 32, 8, C_CARD);
    gfx->drawRoundRect(12, 254, 148, 32, 8, C_GOLD);
    centerText("RESET TELLERS", 265, 1, C_GOLD);
  } else {
    if (bleKeyboard.isConnected()) {
      centerText("Bluetooth: VERBONDEN", 188, 1, C_BAT_GREEN);
    } else {
      centerText("Bluetooth: Zoeken... (Koppel)", 188, 1, C_BLUE);
    }

    // Knop 1: Stuur direct via Bluetooth naar de geopende webapp
    gfx->fillRoundRect(12, 202, 148, 42, 10, C_BEIGE);
    gfx->drawRoundRect(12, 202, 148, 42, 10, C_GOLD);
    centerText("STUUR VIA BT", 213, 1, C_GREEN);
    centerText("Naar geopende webapp", 229, 1, C_GREEN);

    // Knop 2: Bluetooth uitzetten (besparing)
    gfx->fillRoundRect(12, 250, 148, 28, 8, C_CARD);
    gfx->drawRoundRect(12, 250, 148, 28, 8, C_MUTED);
    centerText("BT UITZETTEN", 259, 1, C_WHITE);

    // Knop 3: Handmatig tellers legen
    gfx->fillRoundRect(12, 282, 148, 22, 6, C_CARD);
    centerText("RESET TELLERS", 288, 1, C_GOLD);
  }
#else
  centerText("Bluetooth niet actief", 188, 1, C_CREAM);
  gfx->fillRoundRect(12, 254, 148, 32, 8, C_CARD);
  gfx->drawRoundRect(12, 254, 148, 32, 8, C_GOLD);
  centerText("RESET TELLERS", 265, 1, C_GOLD);
#endif

  centerText("veeg omlaag = terug", 310, 1, C_MUTED);
}

void sendBatchBle() {
#if HAS_BLE_KEYBOARD
  if (!bleActive) {
    startBle();
    drawSync();
    gfx->flush();
    return;
  }
  if (!bleKeyboard.isConnected()) {
    gfx->fillRect(10, 200, 152, 48, C_BG);
    centerText("BT NIET VERBONDEN!", 212, 1, C_WHITE);
    centerText("Koppel in iPhone BT", 228, 1, C_GOLD);
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

  gfx->fillRoundRect(12, 202, 148, 42, 10, C_GOLD);
  centerText("VERZONDEN!", 213, 1, C_GREEN);
  centerText("Tellers geleegd naar 0", 229, 1, C_GREEN);
  gfx->flush();

  for (int i = 0; i < N; i++) counts[i] = 0;
  save();
  delay(1400);
  stopBle(); // Direct BT uitschakelen na verzenden om batterij te sparen
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
  if (page == SYNC && next != SYNC) {
    stopBle(); // Automatisch Bluetooth uitschakelen om stroom te sparen
  }
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
      if (!bleActive) {
        startBle();
        drawSync();
        gfx->flush();
      } else {
        sendBatchBle();
      }
      return;
  }
}

void powerOff() {
  stopBle(); // Zorg dat Bluetooth volledig stopt vóór deep sleep
  gfx->fillScreen(C_BG);
  centerText("Mijn weg naar Jannah", 110, 1, C_CREAM);
  centerText("Uitschakelen", 136, 2, C_GOLD);
  centerText("Druk BOOT om in te schakelen", 175, 1, C_MUTED);
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
      if (!bleActive) {
        // Knop 1: "BT AANZETTEN"
        if (y >= 200 && y <= 248) {
          startBle();
          drawSync();
          gfx->flush();
          return;
        }
        // Knop 2: "RESET TELLERS"
        if (y >= 252 && y <= 292) {
          for (int i = 0; i < N; i++) counts[i] = 0;
          save();
          drawSync();
          gfx->flush();
          return;
        }
      } else {
        // Knop 1: "STUUR VIA BT"
        if (y >= 200 && y <= 246) {
          sendBatchBle();
          return;
        }
        // Knop 2: "BT UITZETTEN"
        if (y >= 248 && y <= 278) {
          stopBle();
          drawSync();
          gfx->flush();
          return;
        }
        // Knop 3: "RESET TELLERS"
        if (y >= 280 && y <= 308) {
          for (int i = 0; i < N; i++) counts[i] = 0;
          save();
          drawSync();
          gfx->flush();
          return;
        }
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
    lastActivityTime = now;
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
    lastActivityTime = now;
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
  pinMode(PIN_BAT_ADC, INPUT);
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
  gfx->setTextWrap(false);
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
  lastActivityTime = millis();
  Serial.println("Jannah: Batterijmeting en auto-uitschakelen (120s) gereed");
  Serial.println("Jannah: Bluetooth staat standaard UIT om batterij te sparen");
}

void loop() {
  uint32_t now = millis();

  // 1. Automatisch uitschakelen na 2 minuten inactiviteit:
  // Schakelt de ESP32 volledig uit (deep sleep). Inschakelen gebeurt weer via de BOOT-knop.
  if (now - lastActivityTime >= AUTO_OFF_TIMEOUT_MS) {
    powerOff();
    return;
  }

  // 2. Fysieke BOOT-knop afhandeling
  static uint32_t downAt = 0;
  static bool down = false, longFired = false;
  bool pressed = digitalRead(PIN_BTN) == LOW;

  if (pressed && !down) {
    lastActivityTime = now;
    down = true;
    longFired = false;
    downAt = now;
  } else if (pressed && down && !longFired && now - downAt >= 3000) {
    longFired = true;
    powerOff(); // 3 seconden indrukken = uitschakelen naar deep sleep
  } else if (!pressed && down) {
    down = false;
    lastActivityTime = now;
    if (!longFired && now - downAt > 30) {
      // Kort indrukken = Tesbih +1!
      if (page != TESBIH) {
        transitionTo(TESBIH, TRANS_LEFT);
      }
      incrementCurrentDhikr();
    }
  }

  // 3. Touchscreen afhandeling
  handleTouch();

  // 4. Periodieke batterij-indicator update (elke 4 seconden)
  static uint32_t lastBatteryCheck = 0;
  if (now - lastBatteryCheck >= 4000) {
    lastBatteryCheck = now;
    drawBatteryIcon(true);
  }

#if HAS_BLE_KEYBOARD
  if (bleActive) {
    static bool lastBtState = false;
    bool btNow = bleKeyboard.isConnected();
    if (btNow != lastBtState) {
      lastBtState = btNow;
      if (page == TESBIH || page == SYNC) dirty = true;
    }
  }
#endif

  if (dirty) {
    dirty = false;
    render();
  } else {
    updateMarquee();
  }
  delay(5);
}
