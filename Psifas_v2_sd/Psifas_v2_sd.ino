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

#define DS3231_I2C_ADDRESS 0x68
#define SD_SS 16

File data_file;
ESP8266WebServer server(80);

String dir_path = "/";
String file_path = "";

byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;

void setup()
{
  Serial.begin(115200);

  SPI.begin();
  Wire.begin(4, 5);

  wifi_station_disconnect();

  Serial.print("Initializing SD card...");

  if (!SD.begin(SD_SS, 2000000))
  {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  pinMode(SD_SS, OUTPUT);
  digitalWrite(SD_SS, HIGH);

  String wifi_name = "test_ap2";
  String wifi_pw = "";
  WiFi.softAP(wifi_name.c_str(), wifi_pw.c_str());

  IPAddress myIP = WiFi.softAPIP();
  Serial.print("My IP address: ");
  Serial.println(myIP);

  server.on("/download", HTTP_GET, serverDownload);
  server.on("/stream", HTTP_GET, serverStream);
  server.begin();

  delay(1000);
  lepton_enabled();
  delay(1000);
  set_lepton_mode("AGC");

  create_new_dir();
  create_new_file();
}

void create_new_dir()
{
  int c = 0;
  
  File root = SD.open(dir_path);
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
}

int frame_skipper = 0;
void loop()
{
  digitalWrite(SD_SS, HIGH);
  lepton_enabled();
  read_lepton_frame();

  if (frame_skipper == 14) // approx 0.5 fps
  {
    sd_enable();

    data_file.write(lepton_image, 4800);
    data_file.flush();
    frame_skipper = 0;

    Serial.println("Fout");
  }
  frame_skipper++;

  server.handleClient();
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

  digitalWrite(SD_SS, HIGH);
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
  SPI.setFrequency(2000000);
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

  file_path += " ";
  file_path += String(dayOfMonth);
  file_path += "-";
  file_path += String(month);
  file_path += "-";
  file_path += String(year);

  SD.open(file_path, FILE_WRITE);
  
  Serial.println(file_path);
}

