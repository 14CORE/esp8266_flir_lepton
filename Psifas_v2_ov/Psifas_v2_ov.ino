#include <Wire.h>
#include <SPI.h>
#include "ArduCAM.h"

void setup()
{
  delay(1000);
  Serial.begin(921600);
  delay(1000);
  
  arducam_init(OV5642_320x240);
}

void loop()
{
  getFrame();
}
