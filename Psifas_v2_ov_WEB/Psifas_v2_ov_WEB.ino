#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <SPI.h>
#include "ArduCAM.h"

extern "C" {
  #include "user_interface.h"
}

ESP8266WebServer server(80);

void setup()
{
  delay(1000);
  Serial.begin(921600);
  delay(1000);
   
  wifi_station_disconnect();

  String wifi_name = "test_ap2";
  String wifi_pw = "";
  WiFi.softAP(wifi_name.c_str(), wifi_pw.c_str());

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("My IP address: ");
  Serial.println(myIP);

  //server.on("/serverStream_OV640x480", HTTP_GET, serverStream_OV640x480);
  server.on("/serverStream_OV320x240", HTTP_GET, serverStream_OV320x240);
  server.begin();
  
  Serial.println("go to /stream");  
}

void loop()
{
  server.handleClient();
}

void streamOVFrames(WiFiClient * client)
{
  String response;
  
  Serial.println("Stream on"); 
  
  while (1) 
  {
    uint16_t f_size = getFrame();

    
    if(f_size!=0)
    {
      response = "--frame\r\n";
      response += "Content-Type: image/jpg\r\n\r\n";
      server.sendContent(response);
          
      Serial.println(client->write(arducam_img,f_size));
    }
    
    yield();
    if(!client->connected()) break;
  }

  Serial.println("Stream off"); 
}

void serverStream_OV320x240() 
{  
  WiFiClient client = server.client();

  arducam_init(OV5642_320x240);

  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);
  
  streamOVFrames(&client);
}

void serverStream_OV640x480() 
{  
  WiFiClient client = server.client();

  arducam_init(OV5642_640x480);

  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);
  
  streamOVFrames(&client);
}
