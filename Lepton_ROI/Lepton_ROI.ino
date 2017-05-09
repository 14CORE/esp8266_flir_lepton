#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "lepton_const.h"

extern "C" {
#include "user_interface.h"
}

#define DS3231_I2C_ADDRESS  0x68
#define LEPTON_SS           16
#define REQ_FPS             9


bool ftp_con = false;

ESP8266WebServer server(80);
 
void setup()
{
  Serial.begin(115200);

  SPI.begin();
  Wire.begin(4, 5);
  
  wifi_station_disconnect();

  String wifi_name = "test_ap2";
  String wifi_pw = "";
  WiFi.softAP(wifi_name.c_str(), wifi_pw.c_str());

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("My IP address: ");
  Serial.println(myIP);

  server.on("/stream", HTTP_GET, serverStream);
  server.begin();

  delay(300);
  lepton_enabled();
  delay(300);
  
  lepton_enable_agc(true);
  lepton_enable_vid_focus_calc(true);
  lepton_set_vid_tresh(LEPTON_TH);
 // lepton_set_roi(10,10,30,30);
}


void loop()
{   
    server.handleClient();
}

void serverStream()
{
  WiFiClient client = server.client();

  Serial.println("Connected");
  
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  while (1)
  {
    read_lepton_frame();
    Serial.println("fout");
    
    response = "--frame\r\n";
    response += "Content-Type: image/bmp\r\n\r\n";
    server.sendContent(response);

    client.write(bmp_header, 1078);
    client.write(lepton_image, 4800);

    yield();
    if (!client.connected()) break;
  }

  Serial.println("disconnected");
}

