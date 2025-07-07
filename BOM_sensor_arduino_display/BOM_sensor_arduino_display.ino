#include <SPI.h>       // For TFT display communication (hardware SPI)
#include <Wire.h>      // For I2C communication with both sensors
#include "Adafruit_GFX.h" // Core graphics library
#include "Adafruit_HX8357.h" // Specific library for HX8357D TFT controller
#include "Adafruit_SHT31.h" // Library for SHT85 (SHT31 is compatible)

#define TFT_CS    10
#define TFT_DC     9
#define TFT_RST    8
// SCK -> Digital Pin 52
// MOSI -> Digital Pin 51
// MISO -> Digital Pin 50

// Initialize the TFT display object with the defined pins
Adafruit_HX8357 tft = Adafruit_HX8357(TFT_CS, TFT_DC, TFT_RST);

// I2C address of the pressure sensor (found by scanner)
const int SSC_SENSOR_I2C_ADDRESS = 0x78; 

// min and max in data sheet
// Minimum pressure of the sensor's range (0 psi for absolute pressure)
const float SSC_P_MIN = 0.0;     

// Maximum pressure of the sensor's range (1.6BA converted to psi)        
const float SSC_P_MAX = 23.21; 

// Pressure unit       
const String SSC_PRESSURE_UNIT = "mmHg";

const float PSI_TO_MMHG_FACTOR = 51.715;

// Constants for 14-bit digital output transfer function (from datasheet)
// 10% of 2^14
const float SSC_OUTPUT_MIN_RAW = 1638.0; 
// 90% of 2^14
const float SSC_OUTPUT_MAX_RAW = 14746.0;
// span of possible values
const float SSC_DIGITAL_SPAN = SSC_OUTPUT_MAX_RAW - SSC_OUTPUT_MIN_RAW;

// SHT85 Temp/Humidity Sensor
// The SHT85 uses the same I2C address as SHT31D by default (0x44)
// Initialize SHT31 object
Adafruit_SHT31 sht31 = Adafruit_SHT31(); 

const int data_size = 5;

void setup() {
  Serial.begin(115200); 
  while (!Serial);

  Serial.println("Initializing TFT Display for Arduino Mega...");
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(HX8357_WHITE);
  tft.setTextColor(HX8357_BLACK, HX8357_WHITE); 
  tft.setTextSize(4); 

  Serial.println("Initializing I2C for Pressure Sensor (Mega: SDA=D20, SCL=D21)");
  
  // Initialize I2C communication
  Wire.begin(); 

  // Initialize SHT85 sensor
  Serial.print("Initializing SHT85 sensor");
   // Default 0x44, in datasheet
  if (!sht31.begin(0x44)) {  
    Serial.println("Couldn't find SHT85 sensor");
    tft.setCursor(10, 10);
    tft.setTextColor(HX8357_RED);
    tft.println("SHT85 Error");
    while (1) delay(1); // Halt if sensor not found
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

    // Status 00 = normal operation, 01 = device in command mode, 10 = stale data, 11 = diagnostic condition
    int status = (highByte >> 6) & 0x03; // Mask to get only the two most significant bits

    // Combine the lower 6 bits of the high byte with the low byte to form the 14-bit raw pressure data
    rawPressure = ((highByte & 0x3F) << 8) | lowByte; // Mask high byte (0x3F = 00111111) to get data bits, then shift and combine

    // convert raw 14 bit reading to meaningful pressure value
    float pressurePercentFSS = ((float)rawPressure - SSC_OUTPUT_MIN_RAW) * 100.0 / SSC_DIGITAL_SPAN;

    // map percentage to actual physical pressure range.
    float pressureValuePSI = SSC_P_MIN + (pressurePercentFSS / 100.0) * (SSC_P_MAX - SSC_P_MIN);

    // convert to mm mercury
    float pressureValueMMHG = pressureValuePSI * PSI_TO_MMHG_FACTOR;

    // Output to Serial Monitor
    Serial.print("SSC Raw: ");
    Serial.print(rawPressure);
    Serial.print(", Status: ");
    Serial.print(status);
    Serial.print(", Pressure: ");_
    Serial.print(pressureValueMMHG, 4); 
    Serial.print("" + SSC_PRESSURE_UNIT);

    // Clear the area where the previous pressure reading was
    tft.fillRect(10, 60, 200, 35, HX8357_WHITE);

    // Set cursor, text size, and color for pressure reading
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

  // Read SHT85 Temp/Humidity Sensor
  float temperature = sht31.readTemperature();
  float humidity = sht31.readHumidity();
  // Output to Serial Monitor
  Serial.print(" | SHT Temp: "); Serial.print(temperature, 2); Serial.print("C");
  Serial.print(" | SHT Hum: "); Serial.print(humidity, 2); Serial.println("%");
  // Display Temperature on TFT
  // Clear previous temp reading area
  tft.fillRect(10, 170, 200, 35, HX8357_WHITE);
  // Position below "Temp:" label
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

  // Display Humidity on TFT
  // Clear previous humidity reading area
  tft.fillRect(10, 280, 200, 35, HX8357_WHITE);
  // Position below "Humidity:" label
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

  delay(1000);
}
