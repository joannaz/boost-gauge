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

// 8 o'clock to 3 o'clock
float psiToAngle(float psi) {
  psi = constrain(psi, -10.0f, 40.0f);
  return mapf(psi, -10.0f, 40.0f, 150.0f, 360.0f);
}

void polarToXY(float angleDeg, int radius, int &x, int &y) {
  float a = angleDeg * DEG_TO_RAD;
  x = CX + cosf(a) * radius;
  y = CY + sinf(a) * radius;
}

void drawSoftPixelRing(float angleDeg, int r1, int r2, uint16_t col) {
  int x, y;
  for (int r = r1; r <= r2; r++) {
    polarToXY(angleDeg, r, x, y);
    spr.drawPixel(x, y, col);
  }
}

void drawBezel() {
  spr.fillScreen(TFT_BLACK);

  spr.fillCircle(CX, CY, 118, rgb(24, 24, 28));
  spr.fillCircle(CX, CY, 114, rgb(6, 6, 8));
  spr.drawCircle(CX, CY, 116, rgb(72, 72, 80));
  spr.drawCircle(CX, CY, 115, rgb(48, 48, 54));
  spr.drawCircle(CX, CY, 113, rgb(18, 18, 22));

  spr.fillCircle(CX, CY, 104, rgb(10, 10, 16));
}

void drawArcBand() {
  for (int p = -10; p <= 40; p++) {
    float a = psiToAngle((float)p);

    if (p <= 0) {
      drawSoftPixelRing(a, 84, 87, rgb(70, 70, 76));
      drawSoftPixelRing(a, 88, 96, rgb(205, 205, 214));
      drawSoftPixelRing(a, 97, 106, rgb(255, 255, 255));
    } else if (p < 30) {
      drawSoftPixelRing(a, 84, 87, rgb(40, 22, 72));
      drawSoftPixelRing(a, 88, 96, rgb(105, 62, 205));
      drawSoftPixelRing(a, 97, 106, rgb(185, 145, 255));
    } else {
      drawSoftPixelRing(a, 84, 87, rgb(70, 70, 76));
      drawSoftPixelRing(a, 88, 96, rgb(205, 205, 214));
      drawSoftPixelRing(a, 97, 106, rgb(255, 255, 255));
    }
  }
}

void drawTicks() {
  for (int p = -10; p <= 40; p += 2) {
    float a = psiToAngle((float)p);
    bool major = (p % 10 == 0);

    int r1 = major ? 68 : 80;
    int r2 = 102;

    int x1, y1, x2, y2;
    polarToXY(a, r1, x1, y1);
    polarToXY(a, r2, x2, y2);

    if (major) {
      spr.drawLine(x1, y1, x2, y2, rgb(150, 150, 160));
      spr.drawLine(x1 + 1, y1, x2 + 1, y2, TFT_WHITE);
    } else {
      spr.drawLine(x1, y1, x2, y2, rgb(90, 90, 104));
    }
  }
}

void drawLabels() {
  spr.setTextDatum(middle_center);

  spr.setFont(&fonts::Font4);
  spr.setTextColor(TFT_WHITE, rgb(10, 10, 16));

  const int psiVals[] = {0, 10, 20, 30, 40};
  const char* psiTxt[] = {"0", "10", "20", "30", "40"};

  for (int i = 0; i < 5; i++) {
    int x, y;
    polarToXY(psiToAngle(psiVals[i]), 58, x, y);
    spr.drawString(psiTxt[i], x, y);
  }

  spr.setFont(&fonts::Font2);
  spr.setTextColor(rgb(220, 220, 228), rgb(10, 10, 16));
  spr.drawString("psi", 184, 146);
}

void drawHub() {
  spr.fillCircle(CX, CY, 18, rgb(18, 18, 24));
  spr.fillCircle(CX, CY, 13, rgb(42, 34, 62));
  spr.fillCircle(CX, CY, 7, rgb(10, 10, 14));
  spr.drawCircle(CX, CY, 18, rgb(68, 68, 76));
}

void drawNeedle(float psi) {
  float a = psiToAngle(psi);

  int tipX, tipY, leftX, leftY, rightX, rightY;

  polarToXY(a, 72, tipX, tipY);
  polarToXY(a + 90.0f, 9, leftX, leftY);
  polarToXY(a - 90.0f, 9, rightX, rightY);
  spr.fillTriangle(leftX, leftY, rightX, rightY, tipX, tipY, rgb(32, 18, 60));

  polarToXY(a, 69, tipX, tipY);
  polarToXY(a + 90.0f, 6, leftX, leftY);
  polarToXY(a - 90.0f, 6, rightX, rightY);
  spr.fillTriangle(leftX, leftY, rightX, rightY, tipX, tipY, rgb(110, 82, 220));

  polarToXY(a, 65, tipX, tipY);
  polarToXY(a + 90.0f, 3, leftX, leftY);
  polarToXY(a - 90.0f, 3, rightX, rightY);
  spr.fillTriangle(leftX, leftY, rightX, rightY, tipX, tipY, TFT_WHITE);

  spr.fillCircle(CX, CY, 7, rgb(32, 18, 60));
  spr.fillCircle(CX, CY, 5, rgb(110, 82, 220));
  spr.fillCircle(CX, CY, 3, TFT_WHITE);
}

void drawValueBox(float psi) {
  int boxW = 90;
  int boxH = 36;
  int x = CX - boxW / 2;
  int y = 175;

  spr.fillRoundRect(x, y, boxW, boxH, 8, rgb(14, 14, 18));
  spr.drawRoundRect(x, y, boxW, boxH, 8, rgb(60, 60, 70));

  spr.setTextDatum(middle_center);
  spr.setFont(&fonts::Font4);
  spr.setTextColor(TFT_WHITE, rgb(14, 14, 18));

  char buf[12];
  snprintf(buf, sizeof(buf), "%.1f", psi);
  spr.drawString(buf, CX, y + boxH / 2);
}

void renderGauge(float psi) {
  drawBezel();
  drawArcBand();
  drawTicks();
  drawLabels();
  drawNeedle(psi);
  drawHub();
  drawValueBox(psi);
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
  for (int i = 0; i < count; i++) {
    sum += psiHistory[i];
  }

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
  sweepSegment(0.0f, 40.0f, 30, 6);
  sweepSegment(40.0f, -10.0f, 34, 6);
  sweepSegment(-10.0f, 0.0f, 16, 6);
}

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setBrightness(255);
  tft.setRotation(0);

  analogReadResolution(12);
  pinMode(4, INPUT);

  spr.setColorDepth(16);
  if (!spr.createSprite(240, 240)) {
    tft.fillScreen(TFT_RED);
    delay(1000);
    tft.fillScreen(TFT_BLACK);
    return;
  }

  renderGauge(0.0f);
  startupSweep();
}

void loop() {
  float targetPsi = readPsiSmoothed();
  displayedPsi += (targetPsi - displayedPsi) * 0.22f;
  renderGauge(displayedPsi);
  delay(20);
}