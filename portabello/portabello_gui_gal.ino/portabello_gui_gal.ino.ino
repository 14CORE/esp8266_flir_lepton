#include <ESP8266WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include "lepton_const.h"
#include "ESP8266FtpServer.h"

extern "C" 
{
  #include "user_interface.h"
}


File data_file;

FtpServer ftpSrv;

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
};

char tmp_buff[15] = {0};
int  data_indx = 0, p_id = 0;
bool data_started = false;
bool configured = false;
unsigned long last_detection = 0;
bool capture_frame = false;
String file_path;

_conf my_conf;

void setup()
{
  Serial.begin(115200);

  delay(500);
  Serial.println();

  SPI.begin();
  Wire.begin(4, 5);

  wifi_station_disconnect();

  WiFi.softAP("config_ap", "12345678");

  SPI.setHwCs(false);
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  pinMode(0,INPUT);
  
  Serial.print("Initializing SD card...");

  if (!SD.begin(16))
  {
    Serial.println("initialization failed!");
  }
  Serial.println("initialization done.");

  ftpSrv.begin("esp8266", "esp8266", &ftp_connection_cb, &ftp_disconnect_cb, lepton_image);   //username, password for ftp.  set ports in ESP8266FtpServer.h  (default 21, 50009 for PASV)

  delay(300);
  SPI.setHwCs(true);
  pinMode(16, OUTPUT);
  digitalWrite(16,HIGH);
  set_lepton_mode("AGC");
}

void loop()
{
  if (!ftp_con)
  {
    if(!read_lepton_frame())
    {
      Serial.println("x");
    }
    else
    {
      if(capture_frame)
      {
        Serial.println("saving");
        capture_frame = false;
        frame_to_sd();
      }
    }
    int sensor_val = digitalRead(0);
    if(!sensor_val && (millis()-last_detection>5000))
    {
      Serial.println("detected");
      capture_frame = true;
      last_detection = millis();
    }
  }
  ftpSrv.handleFTP();
 
}


void frame_to_sd()
{
  SPI.setHwCs(false);
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  file_path = "/" + String(get_files_count()) + ".bmp";  
  data_file = SD.open(file_path, FILE_WRITE);
  
  if (data_file)
  {
    data_file.write(lepton_image, 5878);
    data_file.close();
    Serial.println("data written");
  }
  else
  {
    Serial.println("error");
  }
  
  SPI.setHwCs(true);
  pinMode(16, OUTPUT);
  digitalWrite(16,HIGH);
}
int get_files_count()
{
  int c = 0;

  File root = SD.open("/");
  root.rewindDirectory();
  while (true)
  {
    File entry =  root.openNextFile();
    c++;

    if (!entry)
    {
      return c;
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
  data_file.close();
  ftp_con = true;
}

void ftp_disconnect_cb()
{
  SPI.setHwCs(true);
  pinMode(16, OUTPUT);
  digitalWrite(16,HIGH);
  ftp_con = false;
}

