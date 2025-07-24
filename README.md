# BOM-Arduino-Sensor
Pressure, temperature, and humidity sensor using an Arduino Uno microcontroller and a display.

## Materials for 1 humidity, temperature, and pressure sensor

| Item | Quantity | Cost (USD) | Link |
|------|--------|-------------|------|
| Electronics Component Fun Kit | 1<br>(enough for 3 builds) | 13.98 | [Amazon](https://www.amazon.com/REXQualis-Electronics-tie-Points-Breadboard-Potentiometer/dp/B073ZC68QG) |
| ARDUINO Uno | 1 | 27.60 | [Amazon](https://a.co/d/ct9kYeS) |
| SSCSRNN1.6BA7A3 pressure sensor | 1 | 58.74 | [Digikey](https://www.digikey.com/short/wj780tdv) |
| SHT85 humidity sensor | 1 | 28.30 | [Digikey](https://www.digikey.com/en/products/detail/sensirion-ag/SHT85/9666378) |
| 3.5" TFT 320x480 + Touchscreen Breakout Board | 1 | 39.95 | [Adafruit](https://www.adafruit.com/product/2050) |
| DS3231 RTC Module | 1 (enough for 2 builds) | 5.59 | [Amazon](https://a.co/d/eaZFmYb) |

## Fritzing diagram
<img width="1809" height="1881" alt="bom sensor 2_bb" src="https://github.com/user-attachments/assets/ace89fd3-cf81-4787-944c-415b6b55f05a" />


## Picture of finished electronics (no housing)
![20250724_084801](https://github.com/user-attachments/assets/03cb5f3d-ab3c-43e4-837c-7ca18b66bae8)
![20250724_084821](https://github.com/user-attachments/assets/c9cf6365-c7f3-4ac1-87a8-3c855e9ee163)


## Arduino external documentation
### Libraries used:
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

### Data conversions:
Raw pressure:  
    First 2 bits of the high byte are unneeded and masked using 0x3F (00111111)  
    Then the high byte is shifted to make room for the low byte (00111111 -> 00111111 00000000)
    ((highByte & 0x3F) << 8) | lowByte  


FSS (Full scale scan) value:  
    Current pressure reading as a percentage of the sensor's Full Scale Span  
    Min/max and span values are in the data sheet  
    ((float)rawPressure - SSC_OUTPUT_MIN_RAW(1638.0)) * 100.0 / SSC_DIGITAL_SPAN(14746.0 - 1638.0)

PSI value:  
    SSC_P_MIN(1638.0) + (pressurePercentFSS / 100.0) * (SSC_P_MAX(14746.0) - SSC_P_MIN(1638.0))

MMHg value:  
    pressureValuePSI * PSI_TO_MMHG_FACTOR(51.715)
