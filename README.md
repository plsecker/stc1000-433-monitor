# stc1000-433-monitor

## Introduction

Arduino sketch for the ESP8266 Wifi chip which listens to the 433MHz transmission from the STC-1000 Temperature Controller flashed with new firmware (https://github.com/matsstaff/stc1000p), collecting the first packet it receives with a good CRC. The transmitted packet conforms to the "Fine Offset" weather station protocol.  The STC-1000p firmware transmits its current temperature and the state of its relays (heating, cooling).  This information, along with a 'local' DS18B20 temperature reading, is served up as a simple webpage from the ESP.

Useful for a scenario whereby one wishes to monitor a temperature control system, eg. beer fermentation, which resides in an area with no WiFi.  The temperature controller will transmit (depending on the device used) 10-100m to an ESP8266-based Arduino board which is connected to WiFi.  The system has a compile option to serve up readings to a Thingspeak IoT server.

## SOFTWARE
Install ESP8266 support for Arduino https://github.com/esp8266/Arduino
Add libraries for DallasTemperature, OneWire

## HARDWARE (tested)

 * ESP8266 NodeMCU (ESP12-E) board
 * STC-1000 Temperature Controller flashed with 433MHz firmware [STC1000p](https://github.com/matsstaff/stc1000p)
 * 433MHz Transmitter eg. [STX882](https://www.nicerf.com/product_132_43.html)
 * 433MHz Receiver eg. [SRX882](https://www.nicerf.com/product_132_82.html)
 * Maxim Integrated DS18B20 Local Temperature sensor (not essential to the operation)
 
Special consideration needs to be given to pull-ups for the DS18B20 and voltages for common 433MHz receivers (ie. ensure appropriate GPIO input is < 3.3V)

## License

The code in this project is licensed under the MIT license - see LICENSE for details.

## Links

 * [DS18B20 Datasheet](http://datasheets.maximintegrated.com/en/ds/DS18B20.pdf)
