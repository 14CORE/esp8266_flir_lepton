#define LEPTON_ADDRESS  (0x2A) // shifted left by 2, some strange arduino bug

#define POWER_REG (0x00)
#define STATUS_REG (0x02)
#define DATA_CRC_REG (0x28)

#define COMMANDID_REG (0x04)
#define DATALEN_REG (0x06)

#define DATA0 (0x08)
#define DATA1 (0x0A)

#define AGC (0x01)
#define SYS (0x02)
#define VID (0x03)
#define OEM (0x48)

#define GET (0x00)
#define SET (0x01)
#define RUN (0x02)

#define LEPTON_PACKET_CONTENT_LENGTH 164
#define lepton_image_SIZE 9600
#define LEPTON_WIDTH 160
#define LEPTON_TH 200000
#define SAMPLES_SIZE 10

byte line_buffer[LEPTON_PACKET_CONTENT_LENGTH];
byte lepton_image[lepton_image_SIZE];
unsigned long vid_samples[SAMPLES_SIZE];


void lepton_init()
{
  SPI.begin();
  Wire.begin(4, 5);

  SPI.setHwCs(true);
  SPI.setDataMode(SPI_MODE3);
  SPI.setFrequency(20000000);
}

void read_lepton_line()
{
  delayMicroseconds(100);
  for (int i = 0; i < LEPTON_PACKET_CONTENT_LENGTH; i++)
  {
    line_buffer[i] = SPI.transfer(0x00);
  }
}

bool read_lepton_frame()
{
  byte line_id = 0;
  bool frame_complete = false, valid_line = false;

  unsigned long start_time = millis();

  while (millis() - start_time < 100)
  {
    //yield();
    read_lepton_line();
    line_id = line_buffer[1];
    valid_line = ((line_buffer[0] & 0x0f) != 0x0f) && (line_id < 60);

    if (valid_line)
    {
      for (int i = 0; i < LEPTON_PACKET_CONTENT_LENGTH-4; i ++)
      {
        lepton_image[line_id * LEPTON_WIDTH + i] = line_buffer[i + 4];
      }
    }
    if (line_id == 59)
    {
      return true;
    }
  }
  digitalWrite(15, HIGH); // resync
  delay(185);
  return false;
}

void lepton_write_i2c_bytes(byte * buff, byte len)
{
  delay(5);
  Wire.beginTransmission(LEPTON_ADDRESS);
  for (byte i = 0; i < len; i++)
  {
    Wire.write(buff[i]);
  }
  Wire.endTransmission();
}

void lepton_read_i2c_bytes(byte * buff, byte len)
{
  Wire.requestFrom(LEPTON_ADDRESS, len);

  for (int i = 0 ; i < len; i++)
  {
    buff[i] = Wire.read();
  }
}

void lepton_enable_agc(bool en)
{
  byte data_write[4];

  /*
     Fill DATA0 register with requested state
  */
  data_write[0] = (0x00); //ADDRESS MSB
  data_write[1] = (DATA0); //ADDRESS LSB
  data_write[2] = (0x00); //DATA MSB
  data_write[3] = en ? 1 : 0; //DATA LSB
  lepton_write_i2c_bytes(data_write, 4);

  /*
     Fill DATALEN register with the number of 16-bit registers holding relevant data,
     starting from DATA0. In this case, only DATA0 holds "SET" command
  */
  data_write[0] = (0x00);
  data_write[1] = (DATALEN_REG);
  data_write[2] = (0x00);
  data_write[3] = (0x01);
  lepton_write_i2c_bytes(data_write, 4);

  /*
     Call AGC SET command. This will tell the Lepton to SET the
     value previously filled in DATA0 as the current state of AGC.
  */
  data_write[0] = (0x00);
  data_write[1] = (COMMANDID_REG);
  data_write[2] = (AGC);
  data_write[3] = (SET);
  lepton_write_i2c_bytes(data_write, 4);
}

void lepton_set_vid_tresh(unsigned long tresh)
{
  byte data_write[6];

  /*
     'tresh' is a 4 byte value.
     The correct way to fill the data registers is:
     DATA0[MSB] = tresh[8:15]
     DATA0[LSB] = tresh[0:7]
     DATA1[MSB] = tresh[24:31]
     DATA1[LSB] = tresh[16:23]
  */
  data_write[0] = (0x00);
  data_write[1] = (DATA0);
  data_write[2] = ((tresh >> 8) & 0x0ff);
  data_write[3] = ((tresh)    & 0x0ff);
  data_write[4] = ((tresh >> 24) & 0x0ff);
  data_write[5] = ((tresh >> 16) & 0x0ff);
  lepton_write_i2c_bytes(data_write, 6);

  /*
     The previous step filled in 2 16-bit registers, therefor the data length is 2
  */
  data_write[0] = (0x00);
  data_write[1] = (DATALEN_REG);
  data_write[2] = (0x00);
  data_write[3] = (0x02);
  lepton_write_i2c_bytes(data_write, 4);

  /*
     Execute SET VID Focus Metric Threshold command
  */
  data_write[0] = (0x00);
  data_write[1] = (COMMANDID_REG);
  data_write[2] = (VID);
  data_write[3] = (0x15); // set metric treshhold
  lepton_write_i2c_bytes(data_write, 4);
}


unsigned long lepton_get_vid_tresh()
{
  byte data_write[4];
  char data_read[4];
  unsigned int result;

  data_write[0] = (0x00);
  data_write[1] = (COMMANDID_REG);
  data_write[2] = (VID);
  data_write[3] = (0x14); // get metric treshhold
  lepton_write_i2c_bytes(data_write, 4);

  data_write[0] = (0x00);
  data_write[1] = (DATA0);
  lepton_write_i2c_bytes(data_write, 2);
  lepton_read_i2c_bytes((byte*)data_read, 4);

  result = data_read[2];
  result <<= 8;
  result |= data_read[3];
  result <<= 8;
  result |= data_read[0];
  result <<= 8;
  result |= data_read[1];

  return result;
}


void lepton_enable_vid_focus_calc(bool en)
{
  byte data_write[6];

  data_write[0] = (0x00);
  data_write[1] = (DATA0);
  data_write[2] = (0x00);
  data_write[3] = en ? 1 : 0;
  lepton_write_i2c_bytes(data_write, 4);

  data_write[0] = (0x00);
  data_write[1] = (DATALEN_REG);
  data_write[2] = (0x00);
  data_write[3] = (0x01);
  lepton_write_i2c_bytes(data_write, 4);

  data_write[0] = (0x00);
  data_write[1] = (COMMANDID_REG);
  data_write[2] = (VID);
  data_write[3] = (0x0D); // set focus calculation enabled state
  lepton_write_i2c_bytes(data_write, 4);
}

unsigned long lepton_get_metric()
{
  unsigned long result;

  byte data_write[4];
  char data_read[5];
  data_read[4] = 0;

  data_write[0] = (0x00);
  data_write[1] = (COMMANDID_REG);
  data_write[2] = (VID);
  data_write[3] = (0x18); // get metric result
  lepton_write_i2c_bytes(data_write, 4);

  /*
     Keep polling STATUS Register untill the BUSY bit is cleared
  */

  unsigned long t_start = millis();
  while (millis() - t_start < 500)
  {
    yield();
    data_write[0] = (0x00);
    data_write[1] = (STATUS_REG);
    lepton_write_i2c_bytes(data_write, 2); // request to read STATUS register
    lepton_read_i2c_bytes((byte*)data_read, 2); // recive 2 bytes

    if ((data_read[1] & 0x01) == 0) break; // mask out STATUS registers bit0 and test if its clear
  }

  /*
     DATA is ready to be read. 32bit value
  */
  data_write[0] = (0x00);
  data_write[1] = (DATA0);
  lepton_write_i2c_bytes(data_write, 2); // request to read starting from DATA0 address
  lepton_read_i2c_bytes((byte*)data_read, 4); // recieve 4 bytes

  result = data_read[2];
  result <<= 8;
  result |= data_read[3];
  result <<= 8;
  result |= data_read[0];
  result <<= 8;
  result |= data_read[1];

  return result;
}


void lepton_init_samples_arr()
{
  for(int i = 0 ; i < SAMPLES_SIZE ; i++)
  {
    vid_samples[i] = lepton_get_metric();
    delay(100);
  }
}

unsigned long lepton_calc_delta_sum()
{
  unsigned long sum = 0;
  for(int i = 0 ; i < SAMPLES_SIZE-1 ; i++)
  {
    sum+= vid_samples[i]>vid_samples[i+1] ? vid_samples[i]-vid_samples[i+1] : vid_samples[i+1]- vid_samples[i];
  }  
  return sum;
}

void lepton_push_vid_sample()
{
  for(int i = 1 ; i < SAMPLES_SIZE ; i++)
  {
    vid_samples[i-1]=vid_samples[i];
  }    
  vid_samples[SAMPLES_SIZE-1] = lepton_get_metric();
}

void set_lepton_mode(String _mode)
{
  if (_mode == "AGC")
  {
    lepton_enable_vid_focus_calc(false);
    delay(100);
    //lepton_enable_agc(true);
    delay(100);
  }
  if (_mode == "VID")
  {
    //lepton_enable_agc(false);
    lepton_enable_vid_focus_calc(true);
    lepton_set_vid_tresh(LEPTON_TH);
    lepton_init_samples_arr();
  }
}


