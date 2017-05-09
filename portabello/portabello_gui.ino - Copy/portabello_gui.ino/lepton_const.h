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
#define LEPTON_IMAGE_SIZE 4800
#define LEPTON_WIDTH 80
#define LEPTON_TH 200000
#define SAMPLES_SIZE 5

byte line_buffer[LEPTON_PACKET_CONTENT_LENGTH];
byte lepton_image[LEPTON_IMAGE_SIZE];
unsigned long vid_samples[SAMPLES_SIZE];


void read_lepton_line()
{    
  SPI.beginTransaction(SPISettings(20000000, MSBFIRST, SPI_MODE0));

  for (int i = 0; i < LEPTON_PACKET_CONTENT_LENGTH; i++)
  {
    line_buffer[i] = SPI.transfer(0x00);
  }

  SPI.endTransaction();
}

bool read_lepton_frame()
{
  byte line_id = 0;
  bool frame_complete = false, valid_line = false;

  unsigned long start_time = millis();
  int counter = 0;
  while (millis() - start_time < 100)
  {
    read_lepton_line();
    line_id = line_buffer[1];
    valid_line = ((line_buffer[0] & 0x0f) != 0x0f) && (line_id < 60);

    if (valid_line)
    {

      for (int i = 0; i < LEPTON_WIDTH; i++)
      {
        lepton_image[LEPTON_IMAGE_SIZE-(line_id * LEPTON_WIDTH+i+1)] = line_buffer[5+(LEPTON_WIDTH-i-1)*2];//fill reversed
      }
    }
    if (line_id == 59)
    {
      return true;
    }
  }
  delay(200);
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


void wait_for_ready()
{
  byte data_write[2];
  byte data_read[2];
  
  unsigned long t_start = millis();
  while (millis() - t_start < 100)
  {
    yield();
    data_write[0] = (0x00);
    data_write[1] = (STATUS_REG);
    lepton_write_i2c_bytes(data_write, 2); // request to read STATUS register
    lepton_read_i2c_bytes(data_read, 2); // recive 2 bytes
    if ((data_read[1] & 0x01) == 0) break; // mask out STATUS registers bit0 and test if its clear
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
  
  wait_for_ready();
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

void lepton_run_ffc()
{
  byte data_write[4];

  /*
     Fill DATA0 register with requested state
  */
  data_write[0] = (0x00); //ADDRESS MSB
  data_write[1] = (DATA0); //ADDRESS LSB
  data_write[2] = (0x00); //DATA MSB
  data_write[3] = (0x01); //DATA LSB
  lepton_write_i2c_bytes(data_write, 4);

  /*
     Fill DATALEN register with the number of 16-bit registers holding relevant data,
     starting from DATA0. In this case, only DATA0 holds "SET" command
  */
  data_write[0] = (0x00);
  data_write[1] = (DATALEN_REG);
  data_write[2] = (0x00);
  data_write[3] = (0x00);
  lepton_write_i2c_bytes(data_write, 4);

  wait_for_ready();
  /*
     Call AGC SET command. This will tell the Lepton to SET the
     value previously filled in DATA0 as the current state of AGC.
  */
  data_write[0] = (0x00);
  data_write[1] = (COMMANDID_REG);
  data_write[2] = (SYS);
  data_write[3] = (0x42);
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
  wait_for_ready();

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

  wait_for_ready();
  
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
  wait_for_ready();
  
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

  wait_for_ready();

  data_write[0] = (0x00);
  data_write[1] = (COMMANDID_REG);
  data_write[2] = (VID);
  data_write[3] = (0x18); // get metric result
  lepton_write_i2c_bytes(data_write, 4);

  /*
     Keep polling STATUS Register untill the BUSY bit is cleared
  */

  
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
    lepton_enable_agc(true);
  }
  if (_mode == "VID")
  {
    lepton_enable_agc(false);
    lepton_enable_vid_focus_calc(true);        
    lepton_set_vid_tresh(LEPTON_TH);
    lepton_init_samples_arr();
  }
}


void lepton_set_roi (byte StartCol, byte StartRow, byte EndCol, byte EndRow)  // min/max start values: (column; 1 / <EndCol-1 , row; 1 / <EndRow-1)
{                                                                             // min/max end values: (column; >StartCol+1 / 78, row; >StartRow+1 / 58) 
  byte data_write[10];
    
  data_write[0] = (0x00);
  data_write[1] = (DATA0);
  
  data_write[2] = (0);
  data_write[3] = (StartCol);
  
  data_write[4] = (0);
  data_write[5] = (StartRow);

  data_write[6] = (0);
  data_write[7] = (EndCol);

  data_write[8] = (0);
  data_write[9] = (EndRow);
  
  lepton_write_i2c_bytes(data_write, 10);
  wait_for_ready();
  /*
     The previous step filled in 2 16-bit registers, therefore the data length is 2
  */
  data_write[0] = (0x00);
  data_write[1] = (DATALEN_REG);
  data_write[2] = (0x00);
  data_write[3] = (0x04);
  lepton_write_i2c_bytes(data_write, 4);
  wait_for_ready();
  /*
     Execute SET ROI Focus Metric command
  */
  data_write[0] = (0x00);
  data_write[1] = (COMMANDID_REG);
  data_write[2] = (VID);
  data_write[3] = (0x11); // set ROI
  lepton_write_i2c_bytes(data_write, 4);
}


