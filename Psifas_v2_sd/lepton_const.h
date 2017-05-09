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
#define lepton_image_SIZE 4800
#define bmp_header_size 1078
#define LEPTON_WIDTH 80
#define LEPTON_TH 200000
#define SAMPLES_SIZE 10

byte line_buffer[LEPTON_PACKET_CONTENT_LENGTH];
byte lepton_image[lepton_image_SIZE];
unsigned long vid_samples[SAMPLES_SIZE];


void lepton_enabled()
{
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
      for (int i = 0; i < LEPTON_WIDTH; i++)
      {
        lepton_image[lepton_image_SIZE-(line_id * LEPTON_WIDTH+i+1)] = line_buffer[5+(LEPTON_WIDTH-i-1)*2];//fill reversed
      }
    }
    if (line_id == 59)
    {
      return true;
    }
  }
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


const byte bmp_header[] = {0x42,0x4d,
0xf6,0x16,0x00,0x00,
0x00,0x00,0x00,0x00,
0x36,0x04,0x00,0x00,
0x28,0x00,0x00,0x00,
0x50,0x00,0x00,0x00,
0x3c,0x00,0x00,0x00,
0x01,0x00,
0x08,0x00,
0x00,0x00,0x00,0x00,
0xc0,0x12,0x00,0x00,
0xc4,0x0e,0x00,0x00,
0xc4,0x0e,0x00,0x00,
0x00,0x01,0x00,0x00,
0x00,0x00,0x00,0x00,
0,0,0,0,
1,1,1,0,
2,2,2,0,
3,3,3,0,
4,4,4,0,
5,5,5,0,
6,6,6,0,
7,7,7,0,
8,8,8,0,
9,9,9,0,
10,10,10,0,
11,11,11,0,
12,12,12,0,
13,13,13,0,
14,14,14,0,
15,15,15,0,
16,16,16,0,
17,17,17,0,
18,18,18,0,
19,19,19,0,
20,20,20,0,
21,21,21,0,
22,22,22,0,
23,23,23,0,
24,24,24,0,
25,25,25,0,
26,26,26,0,
27,27,27,0,
28,28,28,0,
29,29,29,0,
30,30,30,0,
31,31,31,0,
32,32,32,0,
33,33,33,0,
34,34,34,0,
35,35,35,0,
36,36,36,0,
37,37,37,0,
38,38,38,0,
39,39,39,0,
40,40,40,0,
41,41,41,0,
42,42,42,0,
43,43,43,0,
44,44,44,0,
45,45,45,0,
46,46,46,0,
47,47,47,0,
48,48,48,0,
49,49,49,0,
50,50,50,0,
51,51,51,0,
52,52,52,0,
53,53,53,0,
54,54,54,0,
55,55,55,0,
56,56,56,0,
57,57,57,0,
58,58,58,0,
59,59,59,0,
60,60,60,0,
61,61,61,0,
62,62,62,0,
63,63,63,0,
64,64,64,0,
65,65,65,0,
66,66,66,0,
67,67,67,0,
68,68,68,0,
69,69,69,0,
70,70,70,0,
71,71,71,0,
72,72,72,0,
73,73,73,0,
74,74,74,0,
75,75,75,0,
76,76,76,0,
77,77,77,0,
78,78,78,0,
79,79,79,0,
80,80,80,0,
81,81,81,0,
82,82,82,0,
83,83,83,0,
84,84,84,0,
85,85,85,0,
86,86,86,0,
87,87,87,0,
88,88,88,0,
89,89,89,0,
90,90,90,0,
91,91,91,0,
92,92,92,0,
93,93,93,0,
94,94,94,0,
95,95,95,0,
96,96,96,0,
97,97,97,0,
98,98,98,0,
99,99,99,0,
100,100,100,0,
101,101,101,0,
102,102,102,0,
103,103,103,0,
104,104,104,0,
105,105,105,0,
106,106,106,0,
107,107,107,0,
108,108,108,0,
109,109,109,0,
110,110,110,0,
111,111,111,0,
112,112,112,0,
113,113,113,0,
114,114,114,0,
115,115,115,0,
116,116,116,0,
117,117,117,0,
118,118,118,0,
119,119,119,0,
120,120,120,0,
121,121,121,0,
122,122,122,0,
123,123,123,0,
124,124,124,0,
125,125,125,0,
126,126,126,0,
127,127,127,0,
128,128,128,0,
129,129,129,0,
130,130,130,0,
131,131,131,0,
132,132,132,0,
133,133,133,0,
134,134,134,0,
135,135,135,0,
136,136,136,0,
137,137,137,0,
138,138,138,0,
139,139,139,0,
140,140,140,0,
141,141,141,0,
142,142,142,0,
143,143,143,0,
144,144,144,0,
145,145,145,0,
146,146,146,0,
147,147,147,0,
148,148,148,0,
149,149,149,0,
150,150,150,0,
151,151,151,0,
152,152,152,0,
153,153,153,0,
154,154,154,0,
155,155,155,0,
156,156,156,0,
157,157,157,0,
158,158,158,0,
159,159,159,0,
160,160,160,0,
161,161,161,0,
162,162,162,0,
163,163,163,0,
164,164,164,0,
165,165,165,0,
166,166,166,0,
167,167,167,0,
168,168,168,0,
169,169,169,0,
170,170,170,0,
171,171,171,0,
172,172,172,0,
173,173,173,0,
174,174,174,0,
175,175,175,0,
176,176,176,0,
177,177,177,0,
178,178,178,0,
179,179,179,0,
180,180,180,0,
181,181,181,0,
182,182,182,0,
183,183,183,0,
184,184,184,0,
185,185,185,0,
186,186,186,0,
187,187,187,0,
188,188,188,0,
189,189,189,0,
190,190,190,0,
191,191,191,0,
192,192,192,0,
193,193,193,0,
194,194,194,0,
195,195,195,0,
196,196,196,0,
197,197,197,0,
198,198,198,0,
199,199,199,0,
200,200,200,0,
201,201,201,0,
202,202,202,0,
203,203,203,0,
204,204,204,0,
205,205,205,0,
206,206,206,0,
207,207,207,0,
208,208,208,0,
209,209,209,0,
210,210,210,0,
211,211,211,0,
212,212,212,0,
213,213,213,0,
214,214,214,0,
215,215,215,0,
216,216,216,0,
217,217,217,0,
218,218,218,0,
219,219,219,0,
220,220,220,0,
221,221,221,0,
222,222,222,0,
223,223,223,0,
224,224,224,0,
225,225,225,0,
226,226,226,0,
227,227,227,0,
228,228,228,0,
229,229,229,0,
230,230,230,0,
231,231,231,0,
232,232,232,0,
233,233,233,0,
234,234,234,0,
235,235,235,0,
236,236,236,0,
237,237,237,0,
238,238,238,0,
239,239,239,0,
240,240,240,0,
241,241,241,0,
242,242,242,0,
243,243,243,0,
244,244,244,0,
245,245,245,0,
246,246,246,0,
247,247,247,0,
248,248,248,0,
249,249,249,0,
250,250,250,0,
251,251,251,0,
252,252,252,0,
253,253,253,0,
254,254,254,0,
255,255,255,0};

