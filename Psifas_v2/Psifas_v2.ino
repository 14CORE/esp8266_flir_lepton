#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <WiFiServer.h> 
#include <Wire.h>
#include <SPI.h>
#include "lepton_const.h"

extern "C" {
  #include "user_interface.h"
}

WiFiServer server(5555);
WiFiClient m_client;

void setup()
{
  Serial.begin(921600);
  Serial.println("####");
  
  lepton_init();
  lepton_enable_agc(true);
  
  //lepton_enable_vid_focus_calc(true);  
  //lepton_set_vid_tresh(150000);
  //Serial.println(lepton_get_vid_tresh());
}

int fs = 0;

void loop()
{
   read_lepton_frame();
   Serial.write(lepton_image);
   Serial.write(0x12);Serial.write(0x34);Serial.write(0x56);
}
