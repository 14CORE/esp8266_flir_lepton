#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <ESP8266mDNS.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <EEPROM.h>
#include "lepton_const.h"
#include "ESP8266FtpServer.h"
#include "eepromreadrwite.h"

extern "C" {
#include "user_interface.h"
}

#define DS3231_I2C_ADDRESS  0x68
#define STREAM_FPS          9
#define Size_Per_Day        100000000 // every 100 MB

File data_file;

String dir_path = "/";
String file_path = "";

byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
int stream_frame_skipper = 0;
int record_frame_skipper = 0;
int millic = 0;

bool ftp_con = false;

const byte detPin = 0;
bool det = false;

struct _conf {
  bool use_vid;
  bool vid_state;
  byte fps;
  int rec_time;
  int  wifi_interval;
  char wifi_name[9];
  char wifi_pw[9];
  byte scol, srow, ecol, erow;
  unsigned long limit;
};

char tmp_buff[15] = {0};
int  data_indx = 0, p_id = 0;
bool data_started = false;
bool configured = false;
unsigned long last_push = 0;
unsigned long last_detection = 0;

_conf my_conf;

void setup()
{
  Serial.begin(115200);

  delay(500);
  Serial.println();

  SPI.begin();
  Wire.begin(4, 5);
  EEPROM.begin(512);

  my_conf.vid_state = false;
  configured = EEPROM.read(0) == 1;
  if (configured)
  {
    EEPROM_readAnything(1, my_conf);
    my_conf.vid_state = false;
    my_conf.use_vid = false;
  }

  //TEMP//
  setDS3231time(0, 18, 11, 2, 14, 2, 17);

  SPI.setHwCs(false);
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  pinMode(detPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(detPin), detect, FALLING);

  Serial.print("Initializing SD card...");

  if (!SD.begin(16))
  {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  create_new_dir();
  create_new_file();

  delay(300);
  SPI.setHwCs(true);
  delay(300);
  init_my_conf();

  Serial.println("AGC ON");
  set_lepton_mode("AGC");
  my_conf.vid_state = false;
              
}

void loop()
{
  if(configured)
    {
      if (read_lepton_frame())
      {
          if (det)
          {
            Serial.println("detection!!!! ☜(°O°☜) ");
            frame_to_sd();
            delay(1000);
            det = false;
          }      
        Serial.print(">");
      }
    }
}



void init_my_conf()
{  
  Serial.print("use_vid ");Serial.println(my_conf.use_vid); 
  Serial.print("vid ");Serial.println(my_conf.vid_state); 
  Serial.print("fps ");Serial.println(my_conf.fps); 
  Serial.print("wifi_interval ");Serial.println(my_conf.wifi_interval);
  Serial.print("wifi_name ");Serial.println(my_conf.wifi_name); 
  Serial.print("wifi_pw ");Serial.println(my_conf.wifi_pw); 
  Serial.print("scol ");Serial.println(my_conf.scol);
  Serial.print("srow ");Serial.println(my_conf.srow); 
  Serial.print("ecol ");Serial.println(my_conf.ecol);
  Serial.print("erow ");Serial.println(my_conf.erow); 
  Serial.print("limit ");Serial.println(my_conf.limit); 
  Serial.print("rec_time ");Serial.println(my_conf.rec_time);
    
  if(my_conf.use_vid && my_conf.vid_state)
  {
    Serial.println("VID ON");
    set_lepton_mode("VID");
    lepton_set_roi(my_conf.scol,my_conf.srow,my_conf.ecol,my_conf.erow);
  }
  else
  {
    Serial.println("AGC ON");
    set_lepton_mode("AGC");
    last_detection = millis();
  }
}


void frame_to_sd()
{
  SPI.setHwCs(false);
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  data_file.write(lepton_image, 4800);
  data_file.flush();
  if (data_file.size() > Size_Per_Day)
  {
    data_file.close();
    create_new_file();
  }

  SPI.setHwCs(true);
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

  file_path = dir_path + "/";

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

  if (data_file) Serial.println("created");
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


void detect()
{
  det = true;
}

