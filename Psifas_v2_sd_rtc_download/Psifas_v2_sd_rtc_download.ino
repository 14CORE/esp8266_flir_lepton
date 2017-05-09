#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "lepton_const.h"
#include "ESP8266FtpServer.h"

extern "C" {
#include "user_interface.h"
}

#define DS3231_I2C_ADDRESS  0x68
#define LEPTON_SS           16
#define REQ_FPS             9
#define Size_Per_Day        REQ_FPS*LEPTON_IMAGE_SIZE*60*2//60*24 // in bytes

File data_file;
ESP8266WebServer server(80);
FtpServer ftpSrv;   //set #define FTP_DEBUG in ESP8266FtpServer.h to see ftp verbose on serial

String dir_path = "/";
String file_path = "";

byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
int frame_skipper_counter = 0;

bool ftp_con = false;

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

  setDS3231time(0, 18, 11, 2, 14, 2, 17);

  SPI.setHwCs(false);
  Serial.print("Initializing SD card...");

  if (!SD.begin(16))
  {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");
    
  ftpSrv.begin("esp8266","esp8266", &ftp_connection_cb, &ftp_disconnect_cb, lepton_image);    //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)

  create_new_dir();
  create_new_file();
  
  delay(300);
  SPI.setHwCs(true);
  delay(300);
  set_lepton_mode("AGC");
}

void ftp_connection_cb()
{
  delay(1);
  SPI.setHwCs(false);
  delay(1);
  data_file.close();
  delay(1);
  Serial.println("file closed");
  ftp_con = true;
}

void ftp_disconnect_cb()
{
  delay(1);
  create_new_dir();
  delay(1);
  create_new_file();
  delay(1);
  SPI.setHwCs(true);
  ftp_con = false;
}


void loop()
{   
  if(!ftp_con)
  {
    Serial.print(">");
    bool res  = read_lepton_frame();
    if(res)
    {
      if (frame_skipper_counter >=  27/REQ_FPS)
      {        

        unsigned long ts = millis();

        SPI.setHwCs(false);  
        pinMode(15,OUTPUT);
        digitalWrite(15,HIGH);

        data_file.write(lepton_image, 4800);
        data_file.flush();
        frame_skipper_counter = 0;
        if(data_file.size()>Size_Per_Day)
        {
          data_file.close();
          create_new_file();
        }
        
        SPI.setHwCs(true);    
        Serial.println(millis()-ts);
      }
      else
      {
        delay(25);
      }
      frame_skipper_counter++;
    }
    server.handleClient();
  }
  ftpSrv.handleFTP();
}

void serverStream()
{
  WiFiClient client = server.client();

  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server.sendContent(response);

  while (1)
  {
    bool res  = read_lepton_frame();
    if(res)
    {
      response = "--frame\r\n";
      response += "Content-Type: image/bmp\r\n\r\n";
      //response += "Content-Length: 5878\r\n\r\n";
      server.sendContent(response);
  
      client.write(bmp_header, 1078);
      client.write(lepton_image, 4800);

      /* Record while displaying 
      
      if (frame_skipper_counter >=  27/REQ_FPS)
      {        
        unsigned long ts = millis();

        SPI.setHwCs(false);  
        pinMode(15,OUTPUT);
        digitalWrite(15,HIGH);

        data_file.write(lepton_image, 4800);
        data_file.flush();
        frame_skipper_counter = 0;
        if(data_file.size()>Size_Per_Day)
        {
          data_file.close();
          create_new_file();
        }
        
        SPI.setHwCs(true);    
        Serial.println(millis()-ts);
        
      }
      */
      frame_skipper_counter++;
    }
    
    yield();
    if (!client.connected()) break;
  }
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
  Serial.println(Wire.endTransmission());
}

void readDS3231time()
{
  Wire.beginTransmission(DS3231_I2C_ADDRESS);
  Wire.write(0); // set DS3231 register pointer to 00h
  Serial.println(Wire.endTransmission());

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

  if(data_file) Serial.println("created");
  else Serial.println("error");
  Serial.println(file_path);
}


void create_new_dir()
{
  int c = 0;

  dir_path  = "/";
  
  File root = SD.open(dir_path);
  root.rewindDirectory();
  while (true) 
  {
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


