/****************************************************/
/* Sensor related definition 												*/
/****************************************************/
#include "OV5642_regs.h"

#define OV5642		3

#define OV5642_320x240 		(0x00)	//320x240
#define OV5642_640x480		(0x01)	//640x480

/* Register initialization tables for SENSORs */
/* Terminating list entry for reg */
#define SENSOR_REG_TERM_8BIT                0xFF
#define SENSOR_REG_TERM_16BIT               0xFFFF
#define SENSOR_VAL_TERM_8BIT                0xFF
#define SENSOR_VAL_TERM_16BIT               0xFFFF


#define MAX_FIFO_SIZE		0x7FFFF			//512KByte

/****************************************************/
/* ArduChip registers definition 											*/
/****************************************************/
#define RWBIT									0x80  //READ AND WRITE BIT IS BIT[7]

#define ARDUCHIP_TEST1       	0x00  //TEST register
#define ARDUCHIP_FRAMES			0x01  //FRAME control register, Bit[2:0] = Number of frames to be captured																	//On 5MP_Plus platforms bit[2:0] = 7 means continuous capture until frame buffer is full

#define ARDUCHIP_MODE      		0x02  //Mode register
#define MCU2LCD_MODE       		0x00

#define ARDUCHIP_TIM       		0x03  //Timming control
#define HREF_LEVEL_MASK    		0x01  //0 = High active , 		1 = Low active
#define VSYNC_LEVEL_MASK   		0x02  //0 = High active , 		1 = Low active
#define LCD_BKEN_MASK      		0x04  //0 = Enable, 					1 = Disable
#define PCLK_delay_MASK  		0x08  //0 = data no delay,		1 = data delayed one PCLK

#define ARDUCHIP_FIFO      		0x04  //FIFO and I2C control
#define FIFO_CLEAR_MASK    		0x01
#define FIFO_START_MASK    		0x02
#define FIFO_RDPTR_RST_MASK     0x10
#define FIFO_WRPTR_RST_MASK     0x20

#define ARDUCHIP_GPIO			0x06  //GPIO Write Register
#define BURST_FIFO_READ			0x3C  //Burst FIFO read operation
#define SINGLE_FIFO_READ		0x3D  //Single FIFO read operation

#define ARDUCHIP_REV       		0x40  //ArduCHIP revision
#define VER_LOW_MASK       		0x3F
#define VER_HIGH_MASK      		0xC0

#define ARDUCHIP_TRIG      		0x41  //Trigger source
#define VSYNC_MASK         		0x01
#define SHUTTER_MASK       		0x02
#define CAP_DONE_MASK      		0x08

#define FIFO_SIZE1				0x42  //Camera write FIFO size[7:0] for burst to read
#define FIFO_SIZE2				0x43  //Camera write FIFO size[15:8]
#define FIFO_SIZE3				0x44  //Camera write FIFO size[18:16]

#define AC_CS 0
#define ARDUCAM_SIZE 10000

byte arducam_img[ARDUCAM_SIZE];

uint8_t get_bit(uint8_t addr, uint8_t bit);

void arducam_init(uint8_t resolution);

uint16_t getFrame();

void ON();

void OFF();

void ENABLE();

void DISABLE();

void flush_fifo(void);

void start_capture(void);

void clear_fifo_flag(void);

uint8_t read_fifo(void);

uint32_t read_fifo_length(void);

void set_fifo_burst();

void arducam_write_i2c_bytes(byte * buff, byte len);

void arducam_read_i2c_bytes(byte * buff, byte len);

void wrSensorReg16_8(int regID, int regDat);

void rdSensorReg16_8(uint16_t regID, uint8_t* regDat);

void wrSensorRegs16_8(const sensor_reg reglist[], uint16_t size);

void OV5642_set_JPEG_size(uint8_t size);

void set_mode(uint8_t mode);

uint8_t bus_read(uint8_t address);

int bus_write(uint8_t address, uint8_t value);

uint8_t read_reg(uint8_t addr);

void write_reg(uint8_t addr, uint8_t data);

void clear_bit(uint8_t addr, uint8_t bit);

void set_bit(uint8_t addr, uint8_t bit);

void clear_bit(uint8_t addr, uint8_t bit);

uint8_t get_bit(uint8_t addr, uint8_t bit)
{
  uint8_t temp;
  temp = read_reg(addr);
  temp = temp & bit;
  return temp;
}

void arducam_init(uint8_t resolution)
{
  SPI.begin();
  Wire.begin(4, 5);
  Wire.setClock(100000L);

  pinMode(15,OUTPUT);
  digitalWrite(15,HIGH);
  
  SPI.setHwCs(false);
  SPI.setDataMode(SPI_MODE0);
  SPI.setFrequency(7000000);

  pinMode(AC_CS,OUTPUT);
  digitalWrite(AC_CS,HIGH);
  
  //SPI TEST//
  write_reg(ARDUCHIP_TEST1, 0x55);
  uint8_t temp = read_reg(ARDUCHIP_TEST1);
  
  if (temp != 0x55)
  {
    Serial.println("SPI communication with OV failed");
  }
  
  uint8_t vid,pid;
  //I2C TEST//
  wrSensorReg16_8(0xff, 0x01);
  rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
  rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);

  if((vid != 0x56) || (pid != 0x42))
  {
      Serial.println("I2C communication with OV failed");
      return;
  }
  
  wrSensorReg16_8(0x3008, 0x80);
  wrSensorRegs16_8(OV5642_QVGA_Preview, sizeof(OV5642_QVGA_Preview));
  
  delay(200);
  
  wrSensorRegs16_8(OV5642_JPEG_Capture_QSXGA, sizeof(OV5642_JPEG_Capture_QSXGA));
  wrSensorReg16_8(0x3818, 0xa8);
  wrSensorReg16_8(0x3621, 0x10);
  wrSensorReg16_8(0x3801, 0xb0);
  wrSensorReg16_8(0x4407, 0x0C);

  write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
  OV5642_set_JPEG_size(resolution);

  delay(1000);

  clear_fifo_flag();
}

uint16_t getFrame()
{        
  uint16_t ov_size =0;
  
  flush_fifo();
  clear_fifo_flag();
  start_capture();
  
  uint16_t tstart = millis();
  
  while(1)
  {
	  yield();
    if(get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK))break;
    if(millis()-tstart>1000)return false;
  }

  uint32_t length = 0;
  length = read_fifo_length();
  if ((length >= ARDUCAM_SIZE) || (length == 0))
  {
    clear_fifo_flag();
    return 0;
  }          

  //Serial.println(length);

  ENABLE();
  delay(1);
  set_fifo_burst();//Set fifo burst mode
  
  uint8_t data,last_data,is_header=0;
  
  data = SPI.transfer(0x00);
  data = (byte)(data >> 1) | (data << 7);

  length -- ;
  
  while ( length-- )
  {
    yield();
    last_data = data;
  	data = SPI.transfer(0x00);
    data = (byte)(data >> 1) | (data << 7);
  
    if (is_header == 1)
    {
      arducam_img[ov_size] = data;
      ov_size++;
    }
    else if ((data == 0xD8) & (last_data == 0xFF))
    {
      is_header = 1;
      arducam_img[ov_size] = last_data;
      ov_size++;
      arducam_img[ov_size] = data;
      ov_size++;
    }
    
    if ( (data == 0xD9) && (last_data == 0xFF) ) //If find the end ,break while,
      break;
  }
  DISABLE();
  
  clear_fifo_flag();
  
  return ov_size;
}

void ON()
{
}

void OFF()
{

}
void ENABLE()
{
	digitalWrite(AC_CS,LOW);
}

void DISABLE()
{
	digitalWrite(AC_CS,HIGH);
}

//Reset the FIFO pointer to ZERO
void flush_fifo(void)
{
	write_reg(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
}

//Send capture command
void start_capture(void)
{
  write_reg(ARDUCHIP_FIFO, FIFO_START_MASK);
}

//Clear FIFO Complete flag
void clear_fifo_flag(void)
{
  write_reg(ARDUCHIP_FIFO, FIFO_CLEAR_MASK);
}

//Read FIFO single
uint8_t read_fifo(void)
{
  uint8_t data;
  data = bus_read(SINGLE_FIFO_READ);
  return data;
}

//Read Write FIFO length
//Support ArduCAM Mini only
uint32_t read_fifo_length(void)
{
  uint32_t len1, len2, len3, length = 0;
  len1 = read_reg(FIFO_SIZE1);
  len2 = read_reg(FIFO_SIZE2);
  len3 = read_reg(FIFO_SIZE3) & 0x7f;
  length = ((len3 << 16) | (len2 << 8) | len1) & 0x07fffff;
  return length; 
}

//Send read fifo burst command
//Support ArduCAM Mini only
void set_fifo_burst()
{
  uint8_t cmd = BURST_FIFO_READ;
  SPI.transfer(BURST_FIFO_READ);
}

void arducam_write_i2c_bytes(byte * buff, byte len)
{
  Wire.beginTransmission(0x78>>1);
  for (byte i = 0; i < len; i++)
  {
    Wire.write(buff[i]);
  }
  Wire.endTransmission();
  delay(1);
}

void arducam_read_i2c_bytes(byte * buff, byte len)
{
  Wire.requestFrom(0x78>>1, len);

  for (int i = 0 ; i < len; i++)
  {
    buff[i] = Wire.read();
  }
}


//I2C Array Write 16bit address, 8bit data
void wrSensorRegs16_8(const sensor_reg reglist[], uint16_t size)
{
  for(int i = 0; i< size/4 ; i++)
  {
      wrSensorReg16_8(reglist[i].reg, reglist[i].val);
  }
}

//I2C Write 16bit address, 8bit data
void wrSensorReg16_8(int regID, int regDat)
{
  uint8_t data[3] ={ regID>>8, regID &0x00ff , regDat & 0x00FF};
  
  arducam_write_i2c_bytes(data,3);
}

//I2C Read 16bit address, 8bit data
void rdSensorReg16_8(uint16_t regID, uint8_t* regDat)
{
  uint8_t data[2] ={ regID>>8, regID &0x00ff };
  
  arducam_write_i2c_bytes(data, 2);
  arducam_read_i2c_bytes(regDat, 1);
}


void OV5642_set_JPEG_size(uint8_t size)
{
  switch (size)
  {
    case OV5642_320x240:
      wrSensorRegs16_8(ov5642_320x240, sizeof(ov5642_320x240));
      break;
    case OV5642_640x480:
      wrSensorRegs16_8(ov5642_640x480, sizeof(ov5642_640x480));
      break;
  }
}


//Set ArduCAM working mode
//MCU2LCD_MODE: MCU writes the LCD screen GRAM
//CAM2LCD_MODE: Camera takes control of the LCD screen
//LCD2MCU_MODE: MCU read the LCD screen GRAM

void set_mode(uint8_t mode)
{
   write_reg(ARDUCHIP_MODE, MCU2LCD_MODE);
}

//Low level SPI read operation
uint8_t bus_read(uint8_t address) 
{
  uint8_t value;
  // take the SS pin low to select the chip:
  
  ENABLE();
  //  send in the address and value via SPI:
  
  SPI.transfer(address);
  value = SPI.transfer(0x00);
  // correction for bit rotation from readback
  value = (byte)(value >> 1) | (value << 7);
  // take the SS pin high to de-select the chip:
  DISABLE();
  
  return value;
}

//Low level SPI write operation
int bus_write(uint8_t address, uint8_t value) 
{
  // take the SS pin low to select the chip:
  
  ENABLE();
  //  send in the address and value via SPI:

  SPI.transfer(address);
  SPI.transfer(value);
  
  // take the SS pin high to de-select the chip:
  DISABLE();
  
  return 1;
}

//Read ArduChip internal registers
uint8_t read_reg(uint8_t addr)
{
  uint8_t data;
  data = bus_read(addr & 0x7F);
  return data;
}

//Write ArduChip internal registers
void write_reg(uint8_t addr, uint8_t data)
{
  bus_write(addr | 0x80, data);
}

void set_bit(uint8_t addr, uint8_t bit)
{
  uint8_t temp;
  temp = read_reg(addr);
  write_reg(addr, temp | bit);

}

//Clear corresponding bit
void clear_bit(uint8_t addr, uint8_t bit)
{
  uint8_t temp;
  temp = read_reg(addr);
  write_reg(addr, temp & (~bit));
}

