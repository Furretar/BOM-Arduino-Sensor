// for TFT display communication (hardware SPI)
#include <SPI.h> 
// for I2C communication with both sensors
#include <Wire.h> 
// core graphics library
#include "Adafruit_GFX.h" 
// library for HX8357D TFT controller
#include "Adafruit_HX8357.h" 
// library for SHT85
#include "Adafruit_SHT31.h" 

#define TFT_CS    10
#define TFT_DC     9
#define TFT_RST    8
// SCK -> digital Pin 52
// MOSI -> digital Pin 51
// MISO -> digital Pin 50

// initializes the TFT display object with the defined pins
Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);

// I2C address of 7-bit pressure sensor, defined in data sheet
const int SSC_SENSOR_I2C_ADDRESS = 0x78; 
 
// minimum pressure of the sensor's range (0 psi for absolute pressure), in data sheet
const float SSC_P_MIN = 0.0;     

// maximum pressure of the sensor's range (1.6BA converted to psi), in data sheet
const float SSC_P_MAX = 23.21; 

// pressure unit       
const String SSC_PRESSURE_UNIT = "mmHg";

// conversion for psi to mmhg
const float PSI_TO_MMHG_FACTOR = 51.715;

// constants for 14-bit digital output transfer function (from datasheet)
// 10% of 2^14
const float SSC_OUTPUT_MIN_RAW = 1638.0; 
// 90% of 2^14
const float SSC_OUTPUT_MAX_RAW = 14746.0;
// span of possible values
const float SSC_DIGITAL_SPAN = SSC_OUTPUT_MAX_RAW - SSC_OUTPUT_MIN_RAW;

// SHT85 Temp/Humidity Sensor
// uses I2C address 0x44, in data sheet
// initialize SHT31 object
Adafruit_SHT31 sht31 = Adafruit_SHT31(); 

// has 5 bytes of data, low/high temp, low/high humidity, and checksum
const int data_size = 5;

void setup() {
  Serial.begin(115200); 
  while (!Serial);

  Serial.println("Initializing TFT Display");
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(HX8357_WHITE);
  tft.setTextColor(HX8357_BLACK, HX8357_WHITE); 
  tft.setTextSize(4); 

  Serial.println("Initializing I2C for Pressure Sensor (Mega: SDA=D20, SCL=D21)");
  
  // initialize I2C communication
  Wire.begin(); 

  // initialize SHT85 sensor
  Serial.print("Initializing SHT85 sensor");
   // default 0x44, in datasheet
  if (!sht31.begin(0x44)) {  
    Serial.println("Couldn't find SHT85 sensor");
    tft.setCursor(10, 10);
    tft.setTextColor(HX8357_RED);
    tft.println("SHT85 Error");
    // halt if sensor not found
    while (1) delay(1); 
  } else {
    Serial.println("SHT85 sensor found");
  }

  tft.setTextSize(data_size);
  tft.setCursor(10, 10);
  tft.println("Pressure:");
  tft.setCursor(10, 120); 
  tft.println("Temp:");
  tft.setCursor(10, 230);
  tft.println("Humidity:");
}

void loop() {
  unsigned int rawPressure = 0;
  Wire.requestFrom(SSC_SENSOR_I2C_ADDRESS, 2);

  if (Wire.available() == 2) {
    byte highByte = Wire.read();
    byte lowByte = Wire.read();

    // status 00 = normal operation, 01 = device in command mode, 10 = stale data, 11 = diagnostic condition
    int status = (highByte >> 6) & 0x03; // Mask to get only the two most significant bits

    // combine the lower 6 bits of the high byte with the low byte to form the 14-bit raw pressure data
    // mask unneeded high byte's 2 bits (0x3F), then shift bits
    rawPressure = ((highByte & 0x3F) << 8) | lowByte; 

    // convert raw 14 bit reading to meaningful pressure value
    float pressurePercentFSS = ((float)rawPressure - SSC_OUTPUT_MIN_RAW) * 100.0 / SSC_DIGITAL_SPAN;

    // map percentage to actual physical pressure range.
    float pressureValuePSI = SSC_P_MIN + (pressurePercentFSS / 100.0) * (SSC_P_MAX - SSC_P_MIN);

    // convert to mm mercury
    float pressureValueMMHG = pressureValuePSI * PSI_TO_MMHG_FACTOR;

    // output to Serial Monitor
    Serial.print("SSC Raw: ");
    Serial.print(rawPressure);
    Serial.print(", Status: ");
    Serial.print(status);
    Serial.print(", Pressure: ");
    Serial.print(pressureValueMMHG, 4); 
    Serial.print("" + SSC_PRESSURE_UNIT);

    // clear the area where the previous pressure reading was
    tft.fillRect(10, 60, 200, 35, HX8357_WHITE);

    // set cursor, text size, and color for pressure reading
    // "__ mmHg"
    tft.setCursor(10, 60);
    tft.setTextSize(data_size);
    tft.setTextColor(HX8357_RED);
    // 3 decimal places
    tft.setTextSize(data_size);
    tft.print(pressureValueMMHG, 2);
    tft.print(SSC_PRESSURE_UNIT);

  } else {
    Serial.print("ERROR: Did not receive 2 bytes from SSC sensor.");
    tft.fillRect(10, 60, 90, 35, HX8357_WHITE);
    tft.setCursor(10, 40);
    tft.setTextSize(2);
    tft.setTextColor(HX8357_RED);
    tft.println("SSC Error!");
  }

  // read SHT85 temp/humidity Sensor
  float temperature = sht31.readTemperature();
  float humidity = sht31.readHumidity();
  // output to Serial Monitor
  Serial.print(" | SHT Temp: "); Serial.print(temperature, 2); Serial.print("C");
  Serial.print(" | SHT Hum: "); Serial.print(humidity, 2); Serial.println("%");
  // display Temperature on TFT
  // clear previous temp reading area
  tft.fillRect(10, 170, 200, 35, HX8357_WHITE);
  // position below "Temp:" label
  tft.setCursor(10, 170); 
  tft.setTextSize(data_size);
  tft.setTextColor(HX8357_RED);
  if (!isnan(temperature)) {
    tft.print(temperature, 2);
    tft.print("C");
  } else {
    tft.setTextColor(HX8357_RED);
    tft.println("Read Error");
  }

  // display Humidity on display
  // clear previous humidity reading area
  tft.fillRect(10, 280, 200, 35, HX8357_WHITE);
  // position below "Humidity:" label
  tft.setCursor(10, 280); 
  tft.setTextSize(data_size);
  tft.setTextColor(HX8357_RED);
  if (!isnan(humidity)) { 
    tft.print(humidity, 2); 
    tft.print("%");
  } else {
    tft.setTextColor(HX8357_RED);
    tft.println("Read Error");
  }

  // update every second
  delay(1000);
}
