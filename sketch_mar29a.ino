#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <math.h>

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 _panel;
  lgfx::Bus_SPI _bus;
  lgfx::Light_PWM _light;

public:
  LGFX() {
    {
      auto cfg = _bus.config();
      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 27000000;
      cfg.freq_read = 16000000;
      cfg.spi_3wire = true;
      cfg.use_lock = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 6;
      cfg.pin_mosi = 7;
      cfg.pin_miso = -1;
      cfg.pin_dc = 2;
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    {
      auto cfg = _panel.config();
      cfg.pin_cs = 10;
      cfg.pin_rst = -1;
      cfg.pin_busy = -1;

      cfg.memory_width = 240;
      cfg.memory_height = 240;
      cfg.panel_width = 240;
      cfg.panel_height = 240;

      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;

      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;

      cfg.readable = false;
      cfg.invert = true;
      cfg.rgb_order = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = false;

      _panel.config(cfg);
    }

    {
      auto cfg = _light.config();
      cfg.pin_bl = 3;
      cfg.invert = false;
      cfg.freq = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }

    setPanel(&_panel);
  }
};

LGFX tft;
LGFX_Sprite spr(&tft);

static const int CX = 120;
static const int CY = 120;

float psiHistory[10] = {0};
int histIndex = 0;
bool histFilled = false;
float displayedPsi = 0.0f;

float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return spr.color565(r, g, b);
}

// Wider sweep, less cramped
float psiToAngle(float psi) {
  psi = constrain(psi, -10.0f, 40.0f);
  return mapf(psi, -10.0f, 40.0f, 150.0f, 360.0f);
}

void polarToXY(float angleDeg, int radius, int &x, int &y) {
  float a = angleDeg * DEG_TO_RAD;
  x = CX + cosf(a) * radius;
  y = CY + sinf(a) * radius;
}

void drawBezel() {
  spr.fillScreen(TFT_BLACK);

  spr.fillCircle(CX, CY, 118, rgb(30, 30, 34));
  spr.fillCircle(CX, CY, 114, rgb(10, 10, 12));
  spr.drawCircle(CX, CY, 116, rgb(72, 72, 80));
  spr.drawCircle(CX, CY, 115, rgb(50, 50, 56));
  spr.drawCircle(CX, CY, 113, rgb(22, 22, 28));

  spr.fillCircle(CX, CY, 104, rgb(8, 8, 14));
}

void drawArcBand() {
  for (int p = -10; p <= 40; p++) {
    float a = psiToAngle((float)p);
    int x1, y1, x2, y2;

    uint16_t col;
    if (p <= 0) {
      col = TFT_WHITE;
    } else if (p < 30) {
      col = rgb(145, 80, 255);
    } else {
      col = TFT_WHITE;
    }

    for (int r = 88; r <= 103; r++) {
      polarToXY(a, r, x1, y1);
      spr.drawPixel(x1, y1, col);
    }
  }
}

void drawTicks() {
  for (int p = -10; p <= 40; p += 2) {
    float a = psiToAngle((float)p);

    bool major = (p % 10 == 0);
    int r1 = major ? 66 : 76;
    int r2 = 100;

    int x1, y1, x2, y2;
    polarToXY(a, r1, x1, y1);
    polarToXY(a, r2, x2, y2);

    uint16_t c = major ? TFT_WHITE : rgb(180, 180, 200);
    spr.drawLine(x1, y1, x2, y2, c);
  }
}

void drawLabels() {
  spr.setTextDatum(middle_center);

  // Outer PSI labels only
  spr.setFont(&fonts::Font4);
  spr.setTextColor(TFT_WHITE, rgb(8, 8, 14));

  const int psiVals[] = {-10, 0, 10, 20, 30, 40};
  const char* psiTxt[] = {"-10", "0", "10", "20", "30", "40"};

  for (int i = 0; i < 6; i++) {
    int x, y;
    polarToXY(psiToAngle(psiVals[i]), 60, x, y);  // slightly pushed out
    spr.drawString(psiTxt[i], x, y);
  }

  // PSI label
  spr.setFont(&fonts::Font2);
  spr.setTextColor(TFT_WHITE, rgb(8, 8, 14));
  spr.drawString("psi", 185, 138);

  // BOOST label
  spr.setFont(&fonts::Font4);
}

void drawHub() {
  spr.fillCircle(CX, CY, 24, rgb(20, 20, 30));
  spr.fillCircle(CX, CY, 17, rgb(60, 80, 220));
  spr.fillCircle(CX, CY, 9, rgb(18, 18, 24));
}

void drawNeedle(float psi) {
  float a = psiToAngle(psi);

  int tipX, tipY, leftX, leftY, rightX, rightY;

  // glow layer
  polarToXY(a, 64, tipX, tipY);
  polarToXY(a + 90.0f, 5, leftX, leftY);
  polarToXY(a - 90.0f, 5, rightX, rightY);
  spr.fillTriangle(leftX, leftY, rightX, rightY, tipX, tipY, rgb(120, 90, 255));
  spr.fillCircle(CX, CY, 6, rgb(120, 90, 255));

  // white needle
  polarToXY(a, 62, tipX, tipY);
  polarToXY(a + 90.0f, 2, leftX, leftY);
  polarToXY(a - 90.0f, 2, rightX, rightY);
  spr.fillTriangle(leftX, leftY, rightX, rightY, tipX, tipY, TFT_WHITE);
  spr.fillCircle(CX, CY, 4, TFT_WHITE);
}

void drawValueBox(float psi) {
  spr.fillRoundRect(16, 184, 64, 24, 5, rgb(16, 16, 20));
  spr.drawRoundRect(16, 184, 64, 24, 5, rgb(58, 58, 70));

  spr.setTextDatum(middle_center);
  spr.setFont(&fonts::Font2);
  spr.setTextColor(TFT_WHITE, rgb(16, 16, 20));

  char buf[12];
  snprintf(buf, sizeof(buf), "%.1f", psi);
  spr.drawString(buf, 48, 196);
}

void drawTurboIcon(int x, int y, float s = 1.0f) {
  uint16_t shell      = rgb(210, 210, 220);
  uint16_t inner      = rgb(120, 90, 255);
  uint16_t dark       = rgb(30, 30, 40);
  uint16_t highlight  = TFT_WHITE;

  int rOuter = (int)(14 * s);
  int rInner = (int)(8 * s);
  int pipeW  = (int)(8 * s);
  int pipeH  = (int)(6 * s);

  // Main turbo housing
  spr.fillCircle(x, y, rOuter, shell);
  spr.fillCircle(x, y, rInner, dark);

  // Compressor outlet "snail nose" to the right
  spr.fillRoundRect(x + (int)(8 * s), y - (int)(4 * s), (int)(12 * s), pipeH, (int)(2 * s), shell);

  // Inlet / exhaust pipe on lower left
  spr.fillRoundRect(x - (int)(18 * s), y + (int)(6 * s), (int)(12 * s), pipeH, (int)(2 * s), shell);

  // Inner purple compressor glow
  spr.fillCircle(x, y, (int)(5 * s), inner);

  // Center hub
  spr.fillCircle(x, y, (int)(2 * s), highlight);

  // Three simple blades
  for (int i = 0; i < 3; i++) {
    float a = (-30 + i * 120) * DEG_TO_RAD;
    int x1 = x + (int)(cosf(a) * (2 * s));
    int y1 = y + (int)(sinf(a) * (2 * s));
    int x2 = x + (int)(cosf(a + 0.45f) * (6 * s));
    int y2 = y + (int)(sinf(a + 0.45f) * (6 * s));
    int x3 = x + (int)(cosf(a - 0.20f) * (4 * s));
    int y3 = y + (int)(sinf(a - 0.20f) * (4 * s));
    spr.fillTriangle(x1, y1, x2, y2, x3, y3, highlight);
  }

  // Small cut-out to make the housing look more like a snail
  spr.fillCircle(x + (int)(9 * s), y - (int)(9 * s), (int)(5 * s), rgb(8, 8, 14));

  // Outline accents
  spr.drawCircle(x, y, rOuter, rgb(90, 90, 100));
  spr.drawRoundRect(x + (int)(8 * s), y - (int)(4 * s), (int)(12 * s), pipeH, (int)(2 * s), rgb(90, 90, 100));
  spr.drawRoundRect(x - (int)(18 * s), y + (int)(6 * s), (int)(12 * s), pipeH, (int)(2 * s), rgb(90, 90, 100));
}

void renderGauge(float psi) {
  drawBezel();
  drawArcBand();
  drawTicks();
  drawLabels();
  drawNeedle(psi);
  drawHub();
  drawValueBox(psi);
  drawTurboIcon(190, 188, 0.9f);
  spr.pushSprite(0, 0);
}

float readPsiSmoothed() {
  int raw = analogRead(4);

  float bar = (raw - 587.0f) / 631.0f;
  float psi = bar * 14.504f;

  psiHistory[histIndex] = psi;
  histIndex = (histIndex + 1) % 10;
  if (histIndex == 0) histFilled = true;

  int count = histFilled ? 10 : histIndex;
  if (count <= 0) count = 1;

  float sum = 0.0f;
  for (int i = 0; i < count; i++) sum += psiHistory[i];

  psi = sum / count;
  return constrain(psi, -10.0f, 40.0f);
}

void sweepSegment(float startPsi, float endPsi, int steps, int stepDelayMs) {
  for (int i = 0; i <= steps; i++) {
    float t = (float)i / (float)steps;
    float eased = 0.5f - 0.5f * cosf(t * PI);
    float psi = startPsi + (endPsi - startPsi) * eased;
    renderGauge(psi);
    delay(stepDelayMs);
  }
}

void startupSweep() {
  sweepSegment(0.0f, 40.0f, 40, 10);
  sweepSegment(40.0f, -10.0f, 45, 10);
  sweepSegment(-10.0f, 0.0f, 20, 10);
}

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setBrightness(255);
  tft.setRotation(0);

  analogReadResolution(12);
  pinMode(4, INPUT);

  spr.setColorDepth(16);
  spr.createSprite(240, 240);

  renderGauge(0.0f);
  startupSweep();
}

void loop() {
  float targetPsi = readPsiSmoothed();
  displayedPsi += (targetPsi - displayedPsi) * 0.12f;
  renderGauge(displayedPsi);
  delay(40);
}