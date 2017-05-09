#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include "ESP8266FtpServer.h"
#include <Wire.h>

extern "C" {
#include "user_interface.h"
}

ESP8266WebServer server(80);
FtpServer ftpSrv;   //set #define FTP_DEBUG in ESP8266FtpServer.h to see ftp verbose on serial

void handleRoot() {
  server.send(200, "text/plain", "hello from esp8266!");
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup(void){
  Serial.begin(115200);
  Serial.println("");

  Wire.begin(4,5);
  pinMode(16,OUTPUT);
  digitalWrite(16,HIGH);  
  
  SPI.setHwCs(true);

  wifi_station_disconnect();

  String wifi_name = "test_ap2";
  String wifi_pw = "";
  WiFi.softAP(wifi_name.c_str(), wifi_pw.c_str());

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("My IP address: ");
  Serial.println(myIP);
  server.on("/", handleRoot);

  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");

  /////FTP Setup, ensure SPIFFS is started before ftp;  /////////
  
  if (SD.begin(15,50000000)) 
  {
      Serial.println("SD opened!");
      ftpSrv.begin("esp8266","esp8266");    //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)
  }    

  SPI.setHwCs(true);
  SPI.setDataMode(SPI_MODE0);
  SPI.setFrequency(50000000); 
}

void loop(void)
{
  ftpSrv.handleFTP();        //make sure in loop you call handleFTP()!!  
  server.handleClient();
}
