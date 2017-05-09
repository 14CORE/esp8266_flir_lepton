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
#define Size_Per_Day        REQ_FPS*LEPTON_IMAGE_SIZE*60*2//60*24 // in bytes

File data_file;
ESP8266WebServer server(80);

String dir_path = "/";
String file_path = "";

byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
int frame_skipper_counter = 0;

void setup()
{
  Serial.begin(115200);

  SPI.begin();
  Wire.begin(4, 5);
  SPI.setHwCs(true);
  
  wifi_station_disconnect();

/*
  String wifi_name = "test_ap2";
  String wifi_pw = "";
  WiFi.softAP(wifi_name.c_str(), wifi_pw.c_str());

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("My IP address: ");
  Serial.println(myIP);

  server.on("/download", HTTP_GET, serverDownload);
  server.on("/stream", HTTP_GET, serverStream);
  server.begin();

*/ 

  pinMode(LEPTON_SS,OUTPUT);
  digitalWrite(LEPTON_SS,HIGH);
  
  //setDS3231time(0, 9, 15, 2, 13, 2, 17);

  Serial.print("Initializing SD card...");

  if (!SD.begin(15,40000000))
  {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  delay(2000);
  
  create_new_dir();
  create_new_file();

  delay(300);
  lepton_enabled();
  delay(300);
  set_lepton_mode("AGC");

}

unsigned long sumbytes=0;

void loop()
{
  bool res  = read_lepton_frame();
  
  if(res)
  {
    if (frame_skipper_counter ==  27/REQ_FPS)
    {
      unsigned long ts = millis();

      //Serial.end();
      
      SPI.setHwCs(true);
      sd_enable();      
      data_file.write(lepton_image, 4800);
      data_file.flush();
      frame_skipper_counter = 0;
      if(data_file.size()>Size_Per_Day)create_new_file();
      
      lepton_enabled();    

      Serial.println(millis()-ts);
    }
    frame_skipper_counter++;
  }

  //server.handleClient();
}

void serverDownload()
{
  sd_enable();
  data_file.close();
  data_file = SD.open("test11.txt", FILE_READ);

  unsigned long file_size = data_file.size();
  Serial.println(file_size);

  WiFiClient client = server.client();

  server.setContentLength(file_size);
  server.sendHeader("Content-Disposition", "attachment; filename=mbmp.dat");
  server.send(200, "application/octet-stream", "");

  data_file.seek(0);

  while (1)
  {
    unsigned long read_len = data_file.read(lepton_image, 4800);
    if (read_len != 4800)
    {
      Serial.println("done");
      break;
    }
    client.write(lepton_image, 4800);
    yield();
  }

  data_file.close();
  data_file = SD.open("test11.txt", FILE_WRITE);
}

void serverStream()
{
  WiFiClient client = server.client();

  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

//  digitalWrite(SD_SS, HIGH);
  lepton_enabled();

  while (1)
  {
    read_lepton_frame();

    response = "--frame\r\n";
    response += "Content-Type: image/bmp\r\n\r\n";
    server.sendContent(response);

    client.write(bmp_header, 1078);
    client.write(lepton_image, 4800);

    yield();
    if (!client.connected()) break;
  }
}

void sd_enable()
{
  SPI.setHwCs(false);
  SPI.setDataMode(SPI_MODE0);
  SPI.setFrequency(40000000); 
}

byte decToBcd(byte val)
{
  return ( (val / 10 * 16) + (val % 10) );
}

byte bcdToDec(byte val)
{
  return ( (val / 16 * 10) + (val % 16) );
}

void setDS3231time(byte _second, byte _minute, byte _hour, byte _dayOfWeek, byte _dayOfMonth, byte _month, byte _year)
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set next input to start at the seconds register
  Wire.write(decToBcd(_second)); // set seconds
  Wire.write(decToBcd(_minute)); // set minutes
  Wire.write(decToBcd(_hour)); // set hours
  Wire.write(decToBcd(_dayOfWeek)); // set day of week (1=Sunday, 7=Saturday)
  Wire.write(decToBcd(_dayOfMonth)); // set date (1 to 31)
  Wire.write(decToBcd(_month)); // set month
  Wire.write(decToBcd(_year)); // set year (0 to 99)
  Wire.endTransmission();
}

void readDS3231time()
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Wire.endTransmission();

  Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
  // request seven bytes of data from DS3231 starting from register 00h
  second = bcdToDec(Wire.read() & 0x7f);
  minute = bcdToDec(Wire.read());
  hour = bcdToDec(Wire.read() & 0x3f);
  dayOfWeek = bcdToDec(Wire.read());
  dayOfMonth = bcdToDec(Wire.read());
  month = bcdToDec(Wire.read());
  year = bcdToDec(Wire.read());
}

void create_new_file()
{
  readDS3231time();

  file_path = dir_path+"/";

  if (hour < 10)
  {
    file_path += "0";
  }
  file_path += String(hour);

  if (minute < 10)
  {
    file_path += "0";
  }
  file_path += String(minute);
  
  if (dayOfMonth < 10)
  {
    file_path += "0";
  }
  file_path += String(dayOfMonth);
  
  if (month < 10)
  {
    file_path += "0";
  }
  file_path += String(month);
  file_path += ".x";
  
  data_file = SD.open(file_path, FILE_WRITE);
  
  Serial.println(file_path);
}


void create_new_dir()
{
  int c = 0;
  
  File root = SD.open(dir_path);
  while (true) 
  {
    yield();
    File entry =  root.openNextFile();
    c++;
    
    if (!entry) 
    {
      dir_path = String(c);
      Serial.println("New Directory: " + dir_path);
      SD.mkdir(dir_path);
      break;
    }
    entry.close();
  }
  root.close();
}


