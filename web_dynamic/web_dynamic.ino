#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <Wire.h>
#include <SPI.h>
#include "lepton_const.h"

extern "C" {
#include "user_interface.h"
}

#define trigger 16
#define trigger2 0

ESP8266WebServer server(80);
ESP8266WebServer server_stream(81);
WiFiClient stream_client;

//<iframe src='http://192.168.4.1:81/stream' height='60' width='80'></iframe>\

unsigned long last_metric = 0,lim=10000;

String XML;
String webSite = 
"<!DOCTYPE HTML>\
 <SCRIPT>\
 var xmlHttp=createXmlHttpObject();\
 function createXmlHttpObject(){\
  if(window.XMLHttpRequest){\
     xmlHttp=new XMLHttpRequest();\
  }else{\
     xmlHttp=new ActiveXObject('Microsoft.XMLHTTP');\
  }\
  return xmlHttp;\
 }\
 function process(){\
  if(xmlHttp.readyState==0 || xmlHttp.readyState==4){\
    xmlHttp.open('PUT','xml',true);\
    xmlHttp.onreadystatechange=handleServerResponse;\ 
    xmlHttp.send(null);\
  }\
  setTimeout('process()',100);\
 }\
 function handleServerResponse(){\
  if(xmlHttp.readyState==4 && xmlHttp.status==200){\
    xmlResponse=xmlHttp.responseXML;\
    xmldoc1 = xmlResponse.getElementsByTagName('runtime')[0].firstChild.nodeValue;\
    xmldoc2 = xmlResponse.getElementsByTagName('metric')[0].firstChild.nodeValue;\
    document.getElementById('runtime').innerHTML=xmldoc1;\
    document.getElementById('metric').innerHTML=xmldoc2;\
  }\
 }\
 </SCRIPT>\
  <BODY onload='process()'>\
  <iframe width='0' height='0' border='0' name='dummyframe' id='dummyframe'></iframe>\
  <BR>This is the ESP website ...<BR>\
  Runtime = <A id='runtime'></A><BR>\
  Metric = <A id='metric'></A><BR>\
  <form action='http://192.168.4.1/submit' method='POST'  target='dummyframe'>\
  StartCol: <input type='text' name='StartCol'>1-78<br>\
  StartRow: <input type='text' name='StartRow'>1-58<br>\
  EndCol: <input type='text' name='EndCol'>StartCol-78<br>\
  EndRow: <input type='text' name='EndRow'>StartRow-58<br>\
  Treshold: <input type='text' name='Treshold'>default:10000<br>\
  Limit: <input type='text' name='Limit'>default:10000<br>\  
  <input type='submit' value='Submit'>\
  </form>\
  </BODY>\
  </HTML>";
  
void buildXML()
{
  XML="<?xml version='1.0'?>";
  XML+="<vals>";
  XML+="<runtime>";
  XML+=millis2time();
  XML+="</runtime>";
  XML+="<metric>";
  XML+=String(last_metric);
  XML+="</metric>";
  XML+="</vals>";
}

String millis2time(){
  String Time="";
  unsigned long ss;
  byte mm,hh;
  ss=millis()/1000;
  hh=ss/3600;
  mm=(ss-hh*3600)/60;
  ss=(ss-hh*3600)-mm*60;
  if(hh<10)Time+="0";
  Time+=(String)hh+":";
  if(mm<10)Time+="0";
  Time+=(String)mm+":";
  if(ss<10)Time+="0";
  Time+=(String)ss;
  return Time;
}

void handleWebsite()
{
  server.send(200,"text/html",webSite);
}

void handleXML(){
  buildXML();
  server.send(200,"text/xml",XML);
}

void  handleSubmit()
{
  if (server.args() > 0 ) {
    for ( uint8_t i = 0; i < server.args(); i++ ) 
    {
      unsigned long val = atol(server.arg(i).c_str());
      
      if(server.argName(i) == "StartCol"){
          scol = val;
      }else if(server.argName(i) == "StartRow"){
          srow = val;
      }else if(server.argName(i) == "EndCol"){
          ecol = val;
      }else if(server.argName(i) == "EndRow"){
          erow = val;
      }else if(server.argName(i) == "Treshold"){
          th = val;
      }else if(server.argName(i) == "Limit"){
          lim = val;
      }
    }
  }

  Serial.print(scol);Serial.print(",");
  Serial.print(srow);Serial.print(",");
  Serial.print(ecol);Serial.print(",");
  Serial.print(erow);Serial.println();
  
  lepton_set_vid_tresh(th);
  lepton_set_roi(scol,srow,ecol,erow);
}


void stream_unfiltered() 
{  
  WiFiClient uf_client = server_stream.client();
  
  set_lepton_mode("AGC");
  digitalWrite(trigger,LOW);
  
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server_stream.sendContent(response);
  
  while (1) 
  {
    read_lepton_frame();

    response = "--frame\r\n";
    response += "Content-Type: image/bmp\r\n\r\n";
    server_stream.sendContent(response);
   
    uf_client.write(lepton_img_header,1078);
    uf_client.write(lepton_image,4800);

    yield();
    if(!uf_client.connected()) break;
  }

  set_lepton_mode("VID");   
  Serial.println("Stream off"); 
}


void stream() {
  
  stream_client = server_stream.client();

  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  server_stream.sendContent(response);
}

void setup() {
  Serial.begin(115200);  
  SPI.begin();
  Wire.begin(4, 5);

  wifi_station_disconnect();

  String wifi_name = "test_ap3";
  String wifi_pw = "";
  WiFi.softAP(wifi_name.c_str(), wifi_pw.c_str());

  IPAddress myIP = WiFi.softAPIP();
  
  server.on("/",handleWebsite);
  server.on("/xml",handleXML);
  server.on("/submit", handleSubmit);
  server_stream.on("/", stream);
  server_stream.on("/nofilter", stream_unfiltered);
  server.begin();  
  server_stream.begin();

  delay(500);
  lepton_init();
  delay(500);
  set_lepton_mode("VID");

  pinMode(trigger2, OUTPUT);
  pinMode(trigger, OUTPUT);
  digitalWrite(trigger,LOW);
  digitalWrite(trigger2,HIGH);
}

unsigned long last_push = 0;

void loop() 
{
 server.handleClient();

 if(stream_client.connected())
 {
    String response = "--frame\r\n";
    response += "Content-Type: image/bmp\r\n\r\n";
    server_stream.sendContent(response);
    if(read_lepton_frame())
    {
        paintROI();
        stream_client.write(lepton_img_header, 1078);
        stream_client.write(lepton_image, 4800);
    }
    else
    {
      Serial.println(">>");
    }
  }
  else
  {
    server_stream.handleClient();
  }
  if(millis()-last_push>100)
  {
     last_push=millis();
     lepton_push_vid_sample();
     last_metric = lepton_calc_delta_sum();
     if(last_metric>lim)
     {
       digitalWrite(trigger,HIGH);
       digitalWrite(trigger2,LOW);
     }
     else
     {
       digitalWrite(trigger,LOW);
       digitalWrite(trigger2,HIGH);
     }
  }
} 
