#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <RTClib.h>

const int chipSelect = A0;
const char* LOG_FILE_NAME = "datalog.txt";

const float SSC_P_MIN = 0.0;     
const float SSC_P_MAX = 23.21;  
const float PSI_TO_MMHG = 51.715;   

const float SSC_OUTPUT_MIN_RAW = 1638.0; 
const float SSC_OUTPUT_MAX_RAW = 14746.0;
const float SSC_DIGITAL_SPAN = SSC_OUTPUT_MAX_RAW - SSC_OUTPUT_MIN_RAW;

const int SSC_SENSOR_I2C_ADDRESS = 0x78; 

RTC_DS3231 rtc;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Wire.begin();

  Serial.println(F("Initializing SD card..."));
  if (!SD.begin(chipSelect)) {
    Serial.println(F("SD initialization failed!"));
    while (true);
  }

  Serial.println(F("Initializing RTC..."));
  if (!rtc.begin()) {
    Serial.println(F("RTC failed!"));
    while (true);
  }

  if (rtc.lostPower()) {
    Serial.println(F("RTC lost power, setting time..."));
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  File dataFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
  if (dataFile) {
    if (dataFile.size() == 0) {
      dataFile.println(F("Timestamp,Pressure_mmHg"));
      Serial.println(F("Header written."));
    }
    dataFile.close();
  } else {
    Serial.println(F("Failed to open log file for header!"));
    while (true);
  }

  Serial.println(F("Setup complete."));
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
    float pressureValueMMHG = pressureValuePSI * PSI_TO_MMHG;

    DateTime now = rtc.now();
    char timestamp[20];
    snprintf(timestamp, sizeof(timestamp), "%02d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());

    String line = String(timestamp) + "," + String(pressureValueMMHG, 2);

    File dataFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
    if (dataFile) {
      dataFile.println(line);
      dataFile.close();
      Serial.println(line);
    } else {
      Serial.println(F("Error opening log file."));
    }
  } else {
    Serial.println(F("I2C data not available."));
  }

  delay(1000);
}
