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

#define TRIGGER_PIN 16

bool wasthere = false;

void setup()
{
  Serial.begin(115200);
  Serial.println("####");

  pinMode(TRIGGER_PIN,OUTPUT);
  digitalWrite(TRIGGER_PIN,HIGH);
  
  wifi_station_disconnect();

  delay(1000);
  lepton_init();
  delay(1000);
  set_lepton_mode("AGC");   
      
  String wifi_name = "test_ap2";
  String wifi_pw = "";
  WiFi.softAP(wifi_name.c_str(), wifi_pw.c_str());

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("My IP address: ");
  Serial.println(myIP);

  server.on("/stream", HTTP_GET, serverStream);
  server.begin();
  
  Serial.println("go to /stream");  
}

void loop()
{
 server.handleClient();

/*
 lepton_push_vid_sample();
 
 unsigned long metric_val  = lepton_calc_delta_sum();

 if(metric_val>5000)
 {
  digitalWrite(TRIGGER_PIN,HIGH);
 }
 else
 {
  digitalWrite(TRIGGER_PIN,LOW);
 }
 
 Serial.println(metric_val);  

 delay(100);
 */
}


void serverStream() 
{  
  WiFiClient client = server.client();


  Serial.println("connected");
  
  set_lepton_mode("AGC");
  
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);
  
  while (1) 
  {
    read_lepton_frame();

    response = "--frame\r\n";
    response += "Content-Type: image/bmp\r\n\r\n";
    server.sendContent(response);
   
    client.write(lepton_img_header,1078);
    client.write(lepton_image,4800);
    Serial.println("frame out");
    
    yield();
    if(!client.connected()) break;
  }

 // set_lepton_mode("VID");   
  Serial.println("Stream off"); 
}


