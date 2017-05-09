#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>

extern "C" {
#include "user_interface.h"
}

ESP8266WebServer server(80);

void handleRoot() {
  int sec = millis() / 1000;
   int min = sec / 60;
   int hr = min / 60;
   char temp[420];
   snprintf ( temp, 400,

"<html>\
  <head>\
    <title>ESP8266 Demo</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Hello from ESP8266!</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <form action='http://192.168.4.1/submit' method='POST'>\
    F_name: <input type='text' name='fname'><br>\
    <input type='submit' value='Submit'>\
</form>\
  </body>\
</html>"

  ,hr, min % 60, sec % 60);
  server.send(200, "text/html", temp);
}

void  handleSubmit()
{
  if (server.args() > 0 ) {
    for ( uint8_t i = 0; i < server.args(); i++ ) {
      if (server.argName(i) == "fname") {
       Serial.println(server.arg(i));
      }
     }
  }
}
void setup() {
  delay(1000);
  Serial.begin(115200);
  
  wifi_station_disconnect();

  String wifi_name = "test_ap4";
  String wifi_pw = "";
  WiFi.softAP(wifi_name.c_str(), wifi_pw.c_str());

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("My IP address: ");
  Serial.println(myIP);
  
  server.on("/", handleRoot);
  server.on("/submit", handleSubmit);
  server.begin();
}

void loop() {
   server.handleClient();
}
