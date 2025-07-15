#include <SPI.h> 
#include <Wire.h> 
#include <RTClib.h>
#include "Adafruit_GFX.h" 
#include "Adafruit_HX8357.h" 

#define TFT_CS    10
#define TFT_DC     9
#define TFT_RST    8

Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);
RTC_DS3231 rtc;

const int SSC_SENSOR_I2C_ADDRESS = 0x78; 
const float SSC_P_MIN = 0.0;     
const float SSC_P_MAX = 23.21; 
const float PSI_TO_MMHG_FACTOR = 51.715;

const float SSC_OUTPUT_MIN_RAW = 1638.0; 
const float SSC_OUTPUT_MAX_RAW = 14746.0;
const float SSC_DIGITAL_SPAN = SSC_OUTPUT_MAX_RAW - SSC_OUTPUT_MIN_RAW;

const int textSizePressure = 4;
const int textSizeTime = 3;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Wire.begin();
  rtc.begin();

  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(HX8357_WHITE);
  tft.setTextColor(HX8357_BLACK, HX8357_WHITE);

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  tft.setTextSize(textSizePressure);
  tft.setCursor(10, 20);
  tft.setTextColor(HX8357_BLACK);
  tft.println("Pressure:");

  tft.setTextSize(textSizeTime);
  tft.setCursor(10, 120);
  tft.println("Time:");
}
void loop() {
  unsigned int rawPressure = 0;
  Wire.requestFrom(SSC_SENSOR_I2C_ADDRESS, 2);

  if (Wire.available() == 2) {
    byte highByte = Wire.read();
    byte lowByte = Wire.read();

    rawPressure = ((highByte & 0x3F) << 8) | lowByte;

    float pressurePercentFSS = ((float)rawPressure - SSC_OUTPUT_MIN_RAW) * 100.0 / SSC_DIGITAL_SPAN;
    float pressureValuePSI = SSC_P_MIN + (pressurePercentFSS / 100.0) * (SSC_P_MAX - SSC_P_MIN);
    float pressureValueMMHG = pressureValuePSI * PSI_TO_MMHG_FACTOR;

    tft.fillRect(10, 60, 220, 40, HX8357_WHITE);
    tft.setCursor(10, 60);
    tft.setTextSize(textSizePressure);
    tft.setTextColor(HX8357_BLUE);
    tft.print(pressureValueMMHG, 2);
    tft.print(" mmHg");

    Serial.print("Pressure: ");
    Serial.print(pressureValueMMHG, 2);
    Serial.println(" mmHg");
  } else {
    tft.fillRect(10, 60, 220, 40, HX8357_WHITE);
    tft.setCursor(10, 60);
    tft.setTextSize(textSizePressure);
    tft.setTextColor(HX8357_RED);
    tft.println("Sensor Error");
    Serial.println("ERROR: No data from SSC sensor");
  }

  DateTime now = rtc.now();

  // Format date string YYYY-MM-DD
  char dateStr[11];
  snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", now.year(), now.month(), now.day());
  tft.fillRect(10, 110, 220, 30, HX8357_WHITE);
  tft.setCursor(10, 110);
  tft.setTextSize(textSizeTime);
  tft.setTextColor(HX8357_BLACK);
  tft.print(dateStr);

  Serial.print(" Date: ");
  Serial.println(dateStr);

  // Format time string HH:MM:SS
  char timeStr[9];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  tft.fillRect(10, 150, 220, 30, HX8357_WHITE);
  tft.setCursor(10, 150);
  tft.setTextSize(textSizeTime);
  tft.setTextColor(HX8357_GREEN);
  tft.print(timeStr);

  Serial.print(" Time: ");
  Serial.println(timeStr);

  delay(1000);
}
