// Main routine for the following hardware:

//    ESP8266 NodeMCU (ESP12-E) board
//    DS18B20 Local Temp sensor
//    433MHz receiver coupled to a STC-1000 Temperature Controller flashed with 433MHz firmware https://github.com/matsstaff/stc1000p
//
// Program to listen to the 433MHz transmission from the STC-1000, collecting the first packet it receives with a good CRC. The transmitted packet conforms to the "Fine Offset" weather station protocol.  The STC-1000 firmware transmits its current temperature and the state of its relays (heating, cooling).  This information, along with the local board's temperature is served up as a simple webpage

// Currently, there appears to be some interference with Wifi and 433MHz reception.  Consequently the program turns off Wifi (into sleep mode) whilst scanning for 433MHz Fine Offset packets. Upon reception of a valid packet the wifi is reenabled before the next packet is due.

// Setup Wi-Fi:
//  * Enter SECRET_SSID in "secrets.h"
//  * Enter SECRET_PASS in "secrets.h"

// Philip Secker  Apr  2019

// Including the ESP8266 WiFi library
#include <ESP8266WiFi.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "BetterWH2.h"
#include "secrets.h"


// Replace with your network details
char ssid[] = SECRET_SSID;   // your network SSID (name)
char pass[] = SECRET_PASS;   // your network password

// wh2 defines
struct Fineoffset {
  boolean valid_packet;
  int     temperature;
  int     humidity;
};
boolean wh2_process(struct Fineoffset *stc_values);
extern volatile unsigned long counter;


#define CPU_FREQUENCY    80                  // valid 80, 160

// Local temperature is sampled with DS18B20
// Data wire is plugged into pin D2 of NodeMCU
#define ONE_WIRE_BUS 4

// Setup a oneWire instance
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to DS18B20 ('Local' Temperature)
DallasTemperature DS18B20(&oneWire);
char temperatureLString[7];
float maxLocalTemperature = 0;
float minLocalTemperature = 1000;
char temperatureRString[7];
float maxRemoteTemperature = 0;
float minRemoteTemperature = 1000;

// Web Server on port 80
WiFiServer server(80);

// only runs once on boot
void setup() {
  // Initializing serial port for debugging purposes
  Serial.begin(115200);

  system_update_cpu_freq(CPU_FREQUENCY);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);    // turn the LED off by making the voltage HIGH

  //Init STC-1000 433MHz receiver
  setup_wh2();

  //Init local temp sensor
  DS18B20.begin(); // IC Default 9 bit. If you have troubles consider upping it 12. Ups the delay giving the IC more time to process the temperature measurement

  // Connecting to WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);  // Connect to WPA/WPA2 network. Change this line if using open or WEP network

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Starting the web server
  server.begin();
  Serial.println("Web server running. Waiting for the ESP IP...");
  delay(5000);

  // Printing the ESP IP address
  Serial.println(WiFi.localIP());

  WiFi.printDiag(Serial);

  WiFi.forceSleepBegin();                  // turn off ESP8266 RF
  delay(10);                                // give RF section time to shutdown
}

boolean getLocalTemperature() {
  float tempC;

  DS18B20.requestTemperatures();

  tempC = DS18B20.getTempCByIndex(0);
  dtostrf(tempC, 3, 1, temperatureLString);


  if (tempC < minLocalTemperature) minLocalTemperature = tempC;
  if (tempC > maxLocalTemperature) maxLocalTemperature = tempC;

  return (!(tempC == 85.0 || tempC == (-127.0)));
}

// Delayed startup until get a valid STC-1000 Temp, then serve html requests for 45s before return to scanning for stc1000 (sends every 48s)
// This scans with wifi turned off, giving better reception ...
void loop() {
  char minTempString[7];
  char maxTempString[7];
  char humidityRString[15];
  struct Fineoffset stc1000;
  float  tempC, humidity;

  // print something useful on monitor every 5s
  if (!(counter % (5 * TICKS_SEC))) {
    if (getLocalTemperature())
      Serial.println(temperatureLString);
  }

  // core 433MHz packet processor
  if (wh2_process(&stc1000)) {
    counter = 0;                        // reset counter - ~45s before must disable wifi

    WiFi.forceSleepWake();              // waken wifi

    Serial.println("Got Remote Temp");
    digitalWrite(LED_BUILTIN, LOW);     // turn the LED on
    delay(500);                         // wait
    digitalWrite(LED_BUILTIN, HIGH);    // turn the LED off

    // process packet's struct
    tempC = (float)stc1000.temperature / 10;
    dtostrf(tempC, 3, 1, temperatureRString);
    if (tempC < minRemoteTemperature) minRemoteTemperature = tempC;
    if (tempC > maxRemoteTemperature) maxRemoteTemperature = tempC;
    //    humidity = (float)stc1000.humidity;
    //    dtostrf(humidity, 3, 1, humidityRString);
    // override value with meaning
    if (stc1000.humidity == COOLING_CODE)
      strcpy(humidityRString, "Cooling");
    else if (stc1000.humidity == HEATING_CODE)
      strcpy(humidityRString, "Heating");
    else if (stc1000.humidity == RELAY_OFF)
      strcpy(humidityRString, "Relay's off");
    Serial.println(humidityRString);

    // Listen for new clients for ~45s then return to Fine Offset scan
    while (counter < (FINEOFFSET_PERIOD - FINEOFFSET_REARM) * TICKS_SEC) {
      WiFiClient client = server.available();
      if (client) {
        Serial.println("New client");
        // boolean to locate when the http request ends
        boolean blank_line = true;
        while (client.connected()) {
          if (client.available()) {
            char c = client.read();
            //            Serial.println("Client available");
            if (c == '\n' && blank_line) {

              client.println("HTTP/1.1 200 OK");
              client.println("Content-Type: text/html");
              client.println("Connection: close");
              client.println();
              // your actual web page that displays temperature
              client.println("<!DOCTYPE HTML>");
              client.println("<html>");
              client.println("<head> <meta charset='UTF-8'> <title>STC1000 Temperature Monitor</title> <meta name='viewport' content='width=device-width, initial-scale=1'> <link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/skeleton/2.0.4/skeleton.css' /> <style> .temp { font-size: 60px; } </style> <style> .max { font-size: 20px; } </style> </head><body><h1>STC-1000 Temperature Monitor</h1>");
              client.println("<h1 class='temp'>");
              client.println("Local:");
              if (getLocalTemperature()) {
                Serial.println("Got Local Temp");
                client.print(temperatureLString);
                client.println("°</h1>");
              }

              client.println("<h1 class='max'>");
              client.println("<p>Min: ");

              dtostrf(minLocalTemperature, 3, 2, minTempString);
              dtostrf(maxLocalTemperature, 3, 2, maxTempString);

              client.print(minTempString);
              client.println("°    Max: ");
              client.print(maxTempString);
              client.println("°</p></h1>");

              client.println("<h1 class='temp'>");
              client.println("STC:");
              client.print(temperatureRString);
              client.println("°</h1>");

              client.println("<h1 class='max'>");
              client.println("<p>Relay: ");
              client.println(humidityRString);
              client.println("</p></h1>");

              client.println("<h1 class='max'>");
              client.println("<p>Min: ");

              dtostrf(minRemoteTemperature, 3, 2, minTempString);
              dtostrf(maxRemoteTemperature, 3, 2, maxTempString);

              client.print(minTempString);
              client.println("°    Max: ");
              client.print(maxTempString);
              client.println("°</p></h1>");


              client.println("</body></html>");
              break;
            }
            if (c == '\n') {
              // when starts reading a new line
              blank_line = true;
            }
            else if (c != '\r') {
              // when finds a character on the current line
              blank_line = false;
            }
          }
        }
        // closing the client connection
        delay(1);
        client.stop();
        Serial.println("Client disconnected.");
      }
    }
    WiFi.forceSleepBegin();                  // turn off ESP8266 RF before 433MHz scanning
  }

}
