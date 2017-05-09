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

#define TRIGGER_PIN 16

void setup()
{
  Serial.begin(921600);
  Serial.println("####");

  pinMode(TRIGGER_PIN,OUTPUT);
  digitalWrite(TRIGGER_PIN,LOW);
  
  wifi_station_disconnect();

  delay(1000);
  lepton_init();
  delay(1000);
  set_lepton_mode("VID");   
      
  String wifi_name = "test_ap2";
  String wifi_pw = "";
  WiFi.softAP(wifi_name.c_str(), wifi_pw.c_str());

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("My IP address: ");
  Serial.println(myIP);

  server.begin();
  server.setNoDelay(true);

  Serial.println("TCP server started on port 5555");  
}

int fs = 0;

void loop()
{
 if (server.hasClient())
 {
  if(!m_client.connected() || !m_client)
  {
    digitalWrite(TRIGGER_PIN,LOW); // IF CLIENT CONNECTS WHILE TRIGGER IS ON
    
    if(m_client)
    {
      Serial.println("stop");
      m_client.stop();
    }    
    m_client = server.available();
    m_client.setNoDelay(true);
    Serial.println("connected");
    set_lepton_mode("AGC");
  }
 }
 if(m_client && m_client.connected())
 {
      if(m_client.available())
      {
        char c = m_client.read();

        switch(c)
        {
          case '0': //BOOM
            digitalWrite(TRIGGER_PIN,HIGH);
            delay(1000);
            digitalWrite(TRIGGER_PIN,LOW);
          break;
          case '1':
            set_lepton_mode("AGC");
          break;
        }
      }
      read_lepton_frame();
      if(fs==3)
      {
        fs=0;
        Serial.println(m_client.write(lepton_image));
        m_client.write(0x12);m_client.write(0x34);m_client.write(0x56);
      }
      fs++;
 }   
 else
 {
   if(m_client) // got here because client disconnected;
   {
      set_lepton_mode("VID");
      m_client.stop();
   }

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
 }
}


