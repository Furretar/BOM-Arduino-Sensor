#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>
#include <Adafruit_GFX.h>
#include <Adafruit_HX8357.h>
#include <Adafruit_SHT31.h>

// TFT setup
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8
Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);

// RTC & sensors
RTC_DS3231 rtc;
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// SSC Pressure Sensor constants
const int   SSC_I2C_ADDR = 0x78;
const float SSC_P_MIN = 0.0;
const float SSC_P_MAX = 23.21;
const float PSI_TO_MMHG = 51.715;
const float SSC_MIN_RAW = 1638.0;
const float SSC_MAX_RAW = 14746.0;
const float SSC_RAW_SPAN = SSC_MAX_RAW - SSC_MIN_RAW;

// Text sizes
const int TEXT_SIZE_DATA = 4;
const int TEXT_SIZE_TIME = 4;

float rawToMmHg(uint16_t raw) {
  float pct = (raw - SSC_MIN_RAW) * 100.0 / SSC_RAW_SPAN;
  float psi = SSC_P_MIN + pct/100.0 * (SSC_P_MAX - SSC_P_MIN);
  return psi * PSI_TO_MMHG;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Wire.begin();
  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    while (1) delay(10);
  }

  // Wait for SETTIME from PC
  Serial.println("waiting for SETTIME...");

  // SHT31 init
  if (!sht31.begin(0x44)) {
    Serial.println("SHT31 not found!");
    while (1) delay(10);
  }

  // TFT init
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(HX8357_WHITE);
  tft.setTextColor(HX8357_BLACK, HX8357_WHITE);
  tft.setTextSize(4);
  tft.setCursor(10,  10); tft.println("Pressure:");
  tft.setCursor(10, 120); tft.println("Temp:");
  tft.setCursor(10, 230); tft.println("Humidity:");
}

void loop() {
  // Handle time setting over Serial
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    if (cmd.startsWith("SETTIME")) {
      int yyyy, MM, dd, hh, mm, ss;
      sscanf(cmd.c_str(), "SETTIME %d-%d-%d %d:%d:%d", &yyyy, &MM, &dd, &hh, &mm, &ss);
      rtc.adjust(DateTime(yyyy, MM, dd, hh, mm, ss));
      Serial.println("setting time and date");
    }
  }

  DateTime now = rtc.now();

  char dateStr[11], timeStr[9];
  snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", now.year(), now.month(), now.day());
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  // Read pressure
  Wire.requestFrom(SSC_I2C_ADDR, (uint8_t)2);
  float mmHg = NAN;
  if (Wire.available() == 2) {
    uint8_t hb = Wire.read(), lb = Wire.read();
    uint16_t raw = ((hb & 0x3F) << 8) | lb;
    mmHg = rawToMmHg(raw);
  }

  // Read temp + humidity
  float tempC = sht31.readTemperature();
  float hum   = sht31.readHumidity();

  // Serial output
  Serial.print(dateStr);
  Serial.print("  ");
  Serial.print(timeStr);
  Serial.print("  |  ");
  Serial.print(mmHg, 2);  Serial.print(" mmHg, ");
  Serial.print(tempC, 2); Serial.print(" C, ");
  Serial.print(hum, 2);   Serial.println(" %");

  // TFT pressure
  tft.fillRect(10,  60, 200, 35, HX8357_WHITE);
  tft.setCursor(10,  60);
  tft.setTextSize(TEXT_SIZE_DATA);
  tft.setTextColor(HX8357_RED, HX8357_WHITE);
  if (!isnan(mmHg)) {
    tft.print(mmHg, 2);
    tft.print("mmHg");
  } else {
    tft.print("ERR");
  }

  // TFT temp
  tft.fillRect(10, 170, 200, 35, HX8357_WHITE);
  tft.setCursor(10, 170);
  tft.print(isnan(tempC) ? 0.0 : tempC, 2);
  tft.print("C");

  // TFT humidity
  tft.fillRect(10, 280, 200, 35, HX8357_WHITE);
  tft.setCursor(10, 280);
  tft.print(isnan(hum) ? 0.0 : hum, 2);
  tft.print("%");

  // TFT clock
  tft.setTextSize(TEXT_SIZE_TIME);
  tft.setTextColor(HX8357_BLACK, HX8357_WHITE);
  tft.fillRect(10, 350, 220, 60, HX8357_WHITE);
  tft.setCursor(10, 350);       tft.print(dateStr);
  tft.setCursor(10, 390);       tft.print(timeStr);

  delay(1000);
}
