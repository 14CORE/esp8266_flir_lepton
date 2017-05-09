#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <SPI.h>
#include "lepton_const.h"

extern "C" {
#include "user_interface.h"
}

ESP8266WebServer server(80);


void setup()
{
  Serial.begin(115200);
  Serial.println("####");
  SPI.begin();
  Wire.begin(4, 5);

  wifi_station_disconnect();

  String wifi_name = "test_ap3";
  String wifi_pw = "";
  WiFi.softAP(wifi_name.c_str(), wifi_pw.c_str());

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("My IP address: ");
  Serial.println(myIP);

  server.on("/streamFiltered", HTTP_GET, streamFiltered);
  server.on("/streamUnfiltered", HTTP_GET, streamUnfiltered);
  server.begin();

  delay(1000);
  lepton_init();
  delay(1000);
  
}

void loop()
{
  server.handleClient();
}


void streamFiltered() 
{
  set_lepton_mode("VID");
  
  WiFiClient client = server.client();

  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  
  while (1)
  {
    response = "--frame\r\n";
    response += "Content-Type: image/bmp\r\n\r\n";
    server.sendContent(response);
    if(read_lepton_frame())
    {
        Serial.println(client.write(lepton_img_header, 1078));
        Serial.println(client.write(lepton_image, 4800));
    }
    else
    {
      Serial.println(">>");
    }
    
    yield();
    if (!client.connected()) break;
  }
}


void streamUnfiltered() {

  set_lepton_mode("AGC");
  
  WiFiClient client = server.client();

  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);


  while (1)
  {
    response = "--frame\r\n";
    response += "Content-Type: image/bmp\r\n\r\n";
    server.sendContent(response);
    if(read_lepton_frame())
    {
        Serial.println(client.write(lepton_img_header, 1078));
        Serial.println(client.write(lepton_image, 4800));
    }
    else
    {
      Serial.println(">>");
    }
    
    yield();
    if (!client.connected()) break;
  }
}

