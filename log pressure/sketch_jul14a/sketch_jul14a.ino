#include <SPI.h>             // for TFT
#include <Wire.h>            // for I²C (RTC & pressure)
#include <RTClib.h>          // DS3231 RTC
#include <Adafruit_GFX.h>    // core graphics
#include <Adafruit_HX8357.h> // HX8357 TFT
#include <Adafruit_SHT31.h>  // SHT31 temp/humidity

// TFT (hardware SPI on an Uno)
#define TFT_CS   10
#define TFT_DC    9
#define TFT_RST   8
Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);

// RTC & sensors
RTC_DS3231     rtc;
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// SSC pressure sensor I²C address + conversion constants
const int   SSC_I2C_ADDR     = 0x78;
const float SSC_P_MIN        = 0.0;
const float SSC_P_MAX        = 23.21;
const float PSI_TO_MMHG      = 51.715;
const float SSC_MIN_RAW      = 1638.0;
const float SSC_MAX_RAW      = 14746.0;
const float SSC_RAW_SPAN     = SSC_MAX_RAW - SSC_MIN_RAW;

// Text sizes
const int TEXT_SIZE_DATA = 5;
const int TEXT_SIZE_TIME = 3;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // — RTC init & auto‑set if it lost power —
  Wire.begin();
  if (!rtc.begin()) {
    Serial.println("RTC not found!");
    while (1) delay(10);
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power; setting to compile time");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // — SHT31 init —
  if (!sht31.begin(0x44)) {
    Serial.println("SHT31 not found!");
    while (1) delay(10);
  }

  // — TFT init —
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(HX8357_WHITE);
  tft.setTextColor(HX8357_BLACK, HX8357_WHITE);
  tft.setTextSize(4);
  tft.setCursor(10,  10); tft.println("Pressure:");
  tft.setCursor(10, 120); tft.println("Temp:");
  tft.setCursor(10, 230); tft.println("Humidity:");
}

float rawToMmHg(uint16_t raw) {
  float pct = (raw - SSC_MIN_RAW) * 100.0 / SSC_RAW_SPAN;
  float psi = SSC_P_MIN + pct/100.0 * (SSC_P_MAX - SSC_P_MIN);
  return psi * PSI_TO_MMHG;
}

void loop() {
  // — timestamp —
  DateTime now = rtc.now();
  char ts[20];
  snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
           now.year(), now.month(), now.day(),
           now.hour(), now.minute(), now.second());

  // — read pressure sensor —
  Wire.requestFrom(SSC_I2C_ADDR, (uint8_t)2);
  float mmHg = NAN;
  if (Wire.available() == 2) {
    uint8_t hb = Wire.read(), lb = Wire.read();
    uint16_t raw = ((hb & 0x3F) << 8) | lb;
    mmHg = rawToMmHg(raw);
  } else {
    Serial.println("Pressure read error");
  }

  // — read temp + humidity —
  float tempC = sht31.readTemperature();
  float hum   = sht31.readHumidity();

  // — print to Serial —
  Serial.print(ts);
  Serial.print(" | ");
  Serial.print(mmHg, 2);  Serial.print(" mmHg, ");
  Serial.print(tempC, 2); Serial.print(" C, ");
  Serial.print(hum, 2);   Serial.println(" %");

  // — update TFT —
  // Pressure
  tft.fillRect(10,  60, 200, 35, HX8357_WHITE);
  tft.setCursor(10,  60);
  tft.setTextSize(TEXT_SIZE_DATA);
  tft.setTextColor(HX8357_RED, HX8357_WHITE);
  if (!isnan(mmHg)) {
    tft.print(mmHg, 2);
    tft.print(" mmHg");
  } else {
    tft.print("ERR");
  }

  // Temp
  tft.fillRect(10, 170, 200, 35, HX8357_WHITE);
  tft.setCursor(10, 170);
  tft.print(isnan(tempC) ? 0.0 : tempC, 2);
  tft.print(" C");

  // Humidity
  tft.fillRect(10, 280, 200, 35, HX8357_WHITE);
  tft.setCursor(10, 280);
  tft.print(isnan(hum) ? 0.0 : hum, 2);
  tft.print(" %");

  // Date/time display
  tft.setTextSize(TEXT_SIZE_TIME);
  tft.setTextColor(HX8357_BLACK, HX8357_WHITE);
  tft.fillRect(10, 350, 220, 60, HX8357_WHITE);
  tft.setCursor(10, 350);
  tft.print(ts);

  delay(1000);
}
