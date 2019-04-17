# stc1000-433-monitor

    ESP8266 NodeMCU (ESP12-E) board
    DS18B20 Local Temp sensor
    433MHz receiver coupled to a STC-1000 Temperature Controller flashed with 433MHz firmware https://github.com/matsstaff/stc1000

Program to listen to the 433MHz transmission from the STC-1000, collecting the first packet it receives with a good CRC. The transmitted packet conforms to the "Fine Offset" weather station protocol.  The STC-1000 firmware transmits its current temperature and the state of its relays (heating, cooling).  This information, along with the local board's temperature is served up as a simple webpage
