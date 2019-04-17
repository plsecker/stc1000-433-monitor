# stc1000-433-monitor

## Introduction

Arduino sketch for the ESP8266 Wifi chip which listens to the 433MHz transmission from the STC-1000, collecting the first packet it receives with a good CRC. The transmitted packet conforms to the "Fine Offset" weather station protocol.  The STC-1000 firmware transmits its current temperature and the state of its relays (heating, cooling).  This information, along with a 'local' DS18B20 temperature reading, is served up as a simple webpage.


## HARDWARE

    *ESP8266 NodeMCU (ESP12-E) board
    *Maxim Integrated DS18B20 Local Temperature sensor
    *433MHz receiver coupled to a STC-1000 Temperature Controller flashed with 433MHz firmware [STC1000p](https://github.com/matsstaff/stc1000p)

Special consideration needs to be given to pull-ups for the DS18B20 and voltages for common 433MHz receivers (ie. ensure appropriate GPIO input is < 3.3V)

## License

The code in this project is licensed under the MIT license - see LICENSE for details.

## Links

 * [DS18B20 Datasheet](http://datasheets.maximintegrated.com/en/ds/DS18B20.pdf)