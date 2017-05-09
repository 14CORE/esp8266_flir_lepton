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
#include <Servo.h>

extern "C" {
#include "user_interface.h"
}

#define DS3231_I2C_ADDRESS  0x68
#define STREAM_FPS          9
#define Size_Per_Day        100000000 // every 100 MB

File data_file;

WiFiServer stream_server(111);
WiFiServer data_server(222);
WiFiClient stream_client;
WiFiClient data_client;

Servo myservo;

FtpServer ftpSrv;   //set #define FTP_DEBUG in ESP8266FtpServer.h to see ftp verbose on serial

String dir_path = "/";
String file_path = "";

byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
int stream_frame_skipper = 0;
int record_frame_skipper = 0;

bool ftp_con = false;

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
  int servo_pos;
};

char tmp_buff[15] = {0};
int  data_indx = 0, p_id = 0;
bool data_started = false;
bool configured = false;
unsigned long last_push = 0;
unsigned long last_detection = 0;

_conf my_conf;

bool sd_in = true;

void setup()
{
  Serial.begin(115200);

  delay(500);
  Serial.println();

  SPI.begin();
  Wire.begin(4, 5);
  EEPROM.begin(512);
  myservo.attach(0); 
  
  wifi_station_disconnect();

  configured = EEPROM.read(0) == 1;
  if (configured)
  {
    EEPROM_readAnything(1, my_conf);
    my_conf.vid_state = false;
  }
  WiFi.softAP("config_ap", "12345678");

  stream_server.begin();
  data_server.begin();
  stream_server.setNoDelay(true);
  data_server.setNoDelay(true);

  //TEMP//
  setDS3231time(0, 18, 11, 2, 14, 2, 17);
  readDS3231time();
  
  SPI .setHwCs(false);
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  Serial.print("Initializing SD card...");

  if(SD.begin(16))
  {
    Serial.println("initialization done.");
    create_new_dir();
    create_new_file();
  }
  else
  {
    Serial.println("initialization failed!");
    sd_in = false;
  }
  ftpSrv.begin("esp8266", "esp8266", &ftp_connection_cb, &ftp_disconnect_cb, lepton_image);   //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)

  delay(300);
  SPI.setHwCs(true);
  delay(300);
  init_my_conf();
}

void loop()
{
  if (!ftp_con)
  {
    if(configured)
    {
      if (read_lepton_frame())
      {
        if(!my_conf.use_vid || (((millis() - last_detection)/60000 < my_conf.rec_time) && !my_conf.vid_state) )
        {
          if (record_frame_skipper >=  27 / my_conf.fps)
          {
            Serial.println(".");
            frame_to_sd();
            record_frame_skipper =0;
          }      
          record_frame_skipper++;
        }
        else
        {
          if(my_conf.use_vid && !my_conf.vid_state)
          {
            Serial.println("Time out. VID ON");
            set_lepton_mode("VID");
            my_conf.vid_state = true;
            
            if(data_client.connected())config_to_client();
          }
        }
        Serial.print(">");
      }
     if(my_conf.use_vid && my_conf.vid_state)
      {
        if(millis()-last_push>100)
        {
           last_push=millis();
           lepton_push_vid_sample();
           unsigned long metric = lepton_calc_delta_sum();
           
           if(metric>my_conf.limit)
           {
              Serial.println("Detected. AGC ON");
              last_detection = millis();
              set_lepton_mode("AGC");
              my_conf.vid_state = false;
              if(data_client.connected())config_to_client();
           }
           if(data_client.connected())
           {
              data_client.print("@");
              data_client.print(metric);
              data_client.print("%");   
                                   
           }
        }      
      }
    }
    handle_stream_client();
    handle_data_client();
  }
  if(sd_in) ftpSrv.handleFTP();
}

void handle_stream_client()
{
  if (stream_server.hasClient())
  {
    if (!stream_client.connected() || !stream_client)
    {
      if (stream_client)
      {
        stream_client.stop();
      }
      stream_client = stream_server.available();
      stream_client.setNoDelay(true);
    }
  }

  if (stream_client)
  {
    if(stream_client.connected())
    {
      if (stream_frame_skipper >=  27 / STREAM_FPS)
      {
          stream_client.write(lepton_image, 4800);
          stream_frame_skipper=0;
      }
      stream_frame_skipper++;
    }
    else
    {
      //client disconnected
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
  Serial.print("servo_pos ");Serial.println(my_conf.servo_pos);
  
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

void config_to_client()
{
  data_client.print("$");
  data_client.print(my_conf.use_vid); data_client.print(",");
  data_client.print(my_conf.vid_state); data_client.print(",");
  data_client.print(my_conf.fps); data_client.print(",");
  data_client.print(my_conf.wifi_interval); data_client.print(",");
  data_client.print(my_conf.wifi_name); data_client.print(",");
  data_client.print(my_conf.wifi_pw); data_client.print(",");
  data_client.print(my_conf.scol); data_client.print(",");
  data_client.print(my_conf.srow); data_client.print(",");
  data_client.print(my_conf.ecol); data_client.print(",");
  data_client.print(my_conf.erow); data_client.print(",");
  data_client.print(my_conf.limit); data_client.print(",");
  data_client.print(my_conf.rec_time); data_client.print(",");
  data_client.print(my_conf.servo_pos); data_client.print(",");
  data_client.print("#");
}

void handle_data_client()
{
  if (data_server.hasClient())
  {
    if (!data_client.connected() || !data_client)
    {
      if (data_client)
      {
        data_client.stop();
      }
      data_client = data_server.available();
      data_client.setNoDelay(true);

      // send current config
      if (configured)
      {
        config_to_client();
      }
    }
  }

  if (data_client && data_client.connected())
  {
    _conf tmp_conf;

    while (data_client.available())
    {
      char c = data_client.read();

      if (c == '$')
      {
        data_started = true;
        continue;
      }
      if (c == '#')
      {
        if(tmp_conf.servo_pos!=my_conf.servo_pos)
        {
            my_conf.servo_pos = tmp_conf.servo_pos;
            myservo.write(tmp_conf.servo_pos);
            Serial.println(my_conf.servo_pos);
        }
        else
        {
          //validata?
          EEPROM_writeAnything(1, tmp_conf);
          EEPROM.write(0, 1);
          EEPROM.commit();
          EEPROM_readAnything(1, my_conf);
          config_to_client();
          init_my_conf();          
        }
        
        configured = true;
        data_started = false;
        data_indx = 0;
        p_id = 0;
        break;
      }

      if (data_started)
      {
        if (c == ',')
        {
          tmp_buff[data_indx] = 0;

          switch (p_id)
          {
            case 0:
              tmp_conf.use_vid = bool(atoi(tmp_buff));
              break;
            case 1:
              tmp_conf.vid_state = bool(atoi(tmp_buff));
              break;
            case 2:
              tmp_conf.fps = atoi(tmp_buff);
              break;
            case 3:
              tmp_conf.wifi_interval = atoi(tmp_buff);
              break;
            case 4:
              memcpy(tmp_conf.wifi_name, tmp_buff, 8);
              tmp_conf.wifi_name[8] = 0;
              break;
            case 5:
              memcpy(tmp_conf.wifi_pw, tmp_buff, 8);
              tmp_conf.wifi_pw[8] = 0;
              break;
            case 6:
              tmp_conf.scol = atoi(tmp_buff);
              break;
            case 7:
              tmp_conf.srow = atoi(tmp_buff);
              break;
            case 8:
              tmp_conf.ecol = atoi(tmp_buff);
              break;
            case 9:
              tmp_conf.erow = atoi(tmp_buff);
              break;
            case 10:
              tmp_conf.limit = atol(tmp_buff);
              break;
            case 11:
              tmp_conf.rec_time = atoi(tmp_buff);
              break;             
            case 12:
              tmp_conf.servo_pos = atoi(tmp_buff);
              break;
          }
          p_id++;
          data_indx = 0;
        }
        else
        {
          tmp_buff[data_indx] = c;
          data_indx++;
        }
      }
    }
  }
}


void frame_to_sd()
{
  if(!sd_in) return;
  
  SPI.setHwCs(false);
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  
  unsigned long ts = millis();
  
  data_file.write(lepton_image, 4800);
  data_file.flush();
  if (data_file.size() > Size_Per_Day)
  {
    data_file.close();
    create_new_file();
  }
  
  Serial.println(millis()-ts);

  pinMode(16, OUTPUT);
  digitalWrite(16,HIGH);
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
  Serial.print(second);Serial.print(",");
  Serial.print(minute);Serial.print(",");
  Serial.print(hour);Serial.print(",");
  Serial.print(dayOfWeek);Serial.print(",");
  Serial.print(dayOfMonth);Serial.print(",");
  Serial.print(month);Serial.print(",");
  Serial.print(year);Serial.println();
}

void create_new_file()
{
  if(!sd_in) return;
  
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
  if(!sd_in)return;
  
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


void ftp_connection_cb()
{
  delay(1);
  SPI.setHwCs(false);
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);  
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

