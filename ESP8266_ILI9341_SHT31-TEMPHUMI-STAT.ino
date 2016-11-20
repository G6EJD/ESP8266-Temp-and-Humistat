/*************************************************** 
 
 ***************************************************
*/

// You can use any (4 or) 5 pins 
#define sclk D5 //--- connect this to the display module SCL pin      (Serial Clock)
#define mosi D7 //--- connect this to the display module SDA/MOSI pin (Serial Data)
#define rst  D6 //--- connect this to the display module RES pin      (Reset)
#define dc   D8 //--- connect this to the display module DC  pin      (Data or Command)
#define cs   D0 //--- connect this to the display module CS  pin      (Chip Select)

#define autoscale_on  true
#define autoscale_off false
#define barchart_on   true
#define barchart_off  false

#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <SPI.h>
#include <Wire.h>
#include <ClosedCube_SHT31D.h>

// Create the SHT31D temperature and humidity sensor object
ClosedCube_SHT31D sht31d;

// Create the display object
Adafruit_ILI9341 display = Adafruit_ILI9341(cs, dc);
//------------------------------------------------------------------
int high_set_T, high_set_RH, high_clear_T, high_clear_RH, low_set_T, low_set_RH, low_clear_T, low_clear_RH, device_status;
float last_temp, last_humi;
#define max_readings 200
float temp_readings[max_readings+1] = {0};
float humi_readings[max_readings+1] = {0};
int   reading = 1;
  
void setup(void) {
  Serial.begin(9600);
  display.begin();
  display.fillScreen(ILI9341_BLACK);
  display.setRotation(3);
  display.setTextSize(2);
  Wire.begin(D3,D4);  // Wire.begin([SDA], [SCL])
  sht31d.begin(0x44); // Device is at address 0x44
  if (sht31d.periodicStart(REPEATABILITY_HIGH, FREQUENCY_10HZ) != NO_ERROR) Serial.println("[ERROR] Cannot start periodic mode");
  pinMode(D2,INPUT);  // sets D2 as input for the ALERT PIN
  pinMode(D1,OUTPUT); // sets D2 as input for TEMPSTAT on/off output
  pinMode(D6,OUTPUT); // sets D2 as input for HUMISTAT on/off output
  // Remember : If high clear > high set the Alert feature is disabled
  if (sht31d.writeAlertHigh(25,24,60,55) != NO_ERROR) Serial.println("[ERROR] Cannot set HIGH temperature and humidity alerts"); // 25°C 24°C 60% 55%
  if (sht31d.writeAlertLow(18,16,30,27)  != NO_ERROR) Serial.println("[ERROR] Cannot set LOW temperature and humidity alerts");  // 18°C 16°C 30% 27%
  sht31_read_alert_settings(); // Get report from device of current set and clear values
  clear_arrays();              // Clear graph arrays
}

void loop() {
  SHT31D device = sht31d.periodicFetchData(); // SHT31D temperature and humidity values returns result.t and result.rh or in this example device.t and device.rh
  display.setTextColor(ILI9341_YELLOW);
  display.println(" SHT3xD Thermo & Humistat"); 
  bool alert_state = digitalRead(D2); // read the ALERT pin state
  device_status = read_status_register();             // updates device_status
  clear_status_register();
  bool temp_status  = device_status & 0x0400; // Either High or low temp alert triggered
  bool humi_status  = device_status & 0x0800; // Either High or low humi alert triggered
  int x = 90, y = 21; // Vary these to move display around.
  display.drawRoundRect(30,17,265,29,5,ILI9341_YELLOW);
  if (last_temp != round(device.t*10)/10 || last_humi != round(device.rh*10)/10) { // test to 0.1 precision, if no change don't update screen
    display.fillRoundRect(31,18,263,27,5,ILI9341_BLACK); //fillRect(x,y,w,h,r,colour);
    display_xy(x-50,y,String(device.t,1)+String(char(247))+"C",ILI9341_RED,3); // display temperature
    display_xy(x+110,y,String(device.rh,1)+"%",ILI9341_GREEN,3);               // display humidity
  }
  // Display alert set-points
  display_xy(x-50,y+180," High  ON: "+String(high_set_T)+String(char(247))+"C",ILI9341_WHITE,1);
  display_xy(x-50,y+190," High OFF: "+String(high_clear_T)+String(char(247))+"C",ILI9341_WHITE,1);
  display_xy(x-50,y+200," Low   ON: "+String(low_set_T)+String(char(247))+"C",ILI9341_WHITE,1);
  display_xy(x-50,y+210," Low  OFF: "+String(low_clear_T)+String(char(247))+"C",ILI9341_WHITE,1);

  display_xy(x+115,y+180,"High  ON: "+String(high_set_RH)+"%",ILI9341_WHITE,1);
  display_xy(x+115,y+190,"High OFF: "+String(high_clear_RH)+"%",ILI9341_WHITE,1);
  display_xy(x+115,y+200,"Low   ON: "+String(low_set_RH)+"%",ILI9341_WHITE,1);
  display_xy(x+115,y+210,"Low  OFF: "+String(low_clear_RH)+"%",ILI9341_WHITE,1);

  display.fillRect(0,180,320,15,ILI9341_BLACK); //fillRect(x,y,w,h,color);
  if (alert_state) display_xy(135,180,"ALERT",ILI9341_MAGENTA,2); else display_xy(145,180,"LOW",ILI9341_GREEN,2);
  //display_xy(10,80,String(device_status,HEX),ILI9341_WHITE,2);
  
  display_xy(63,167,"Tempstat",ILI9341_WHITE,1);
  if (temp_status) display_xy(70,180,"OFF",ILI9341_GREEN,2); else display_xy(80,180,"ON",ILI9341_RED,2);
  digitalWrite(D1, temp_status ? LOW:HIGH); // If temp_status is true LOW else HIGH
  
  display_xy(218,167,"Humistat",ILI9341_WHITE,1);
  if (humi_status) display_xy(235,180,"ON",ILI9341_RED,2);  else display_xy(225,180,"OFF",ILI9341_GREEN,2);
  digitalWrite(D6, humi_status ? HIGH:LOW); // If humi_status is true HIGH else LOW
  
  last_temp = round(device.t*10)/10; // needed to detect a change and to help reduce screen updates, but no more than 0.1 precision
  last_humi = round(device.rh*10)/10; // needed to detect a change and to help reduce screen updates, but no more than 0.1 precision

  temp_readings[reading] = device.t;
  humi_readings[reading] = device.rh;

  // DrawGraph(int x_pos, int y_pos, int width, int height, int Y1_Max, String title, float data_array1[max_readings], boolean auto_scale, boolean barchart_mode, int colour)
  DrawGraph(20,75, 130,75,35, "Temperature",temp_readings, autoscale_off, barchart_off, ILI9341_RED);
  DrawGraph(180,75,130,75,100,"Humidity",   humi_readings, autoscale_off, barchart_off, ILI9341_GREEN);
 
  reading = reading + 1;
  if (reading > max_readings) { // if number of readings exceeds max_readings (e.g. 100) then shift all array data to the left to effectively scroll the display left
    reading = max_readings;
    for (int i = 1; i < max_readings; i++) {
      temp_readings[i] = temp_readings[i+1];
      humi_readings[i] = humi_readings[i+1];
    }
    temp_readings[reading] = device.t;
    humi_readings[reading] = device.rh;
  }
  display.setTextSize(2);
  display.setCursor(0,0); // Cursor back to home
  delay(1000);
}

int get_parameter (int MSB, int LSB) {
  int reading,temp = 0;
  Wire.beginTransmission(0x44);    // transmit to device #44
  Wire.write(byte(MSB));           // sets register pointer to read Alert
  Wire.write(byte(LSB));           // sets register pointer to read Alert
  Wire.endTransmission();          // stop transmitting
  Wire.requestFrom(0x44, 3);       // request 3 bytes from slave device #44
  if (2 <= Wire.available()) {     // receive reading from sensor if three bytes were received
    reading  = Wire.read();        // receive high byte (overwrites previous reading)
    reading  = reading << 8;       // shift high byte to be high 8 bits
    reading |= Wire.read();        // receive low byte as lower 8 bits
    temp     = Wire.read();        // receive and discard CRC
  }
  return reading;
}

float convert_to_temp (int value) {
  // Temp'C = -45 + 175.St/(2^16-1)
  return -45 + 175 * ((value<<7)&0xFF80)/65535+1;
}

float convert_to_humi (int value) {
  // RH% = 100.Srh/(2^16-1)
  return  100*(value&0xFE00)/65535+1;
}

void sht31_read_alert_settings(){
  high_set_T    = convert_to_temp(get_parameter(0xE1,0x1F)); 
  high_set_RH   = convert_to_humi(get_parameter(0xE1,0x1F));
  high_clear_T  = convert_to_temp(get_parameter(0xE1,0x14));
  high_clear_RH = convert_to_humi(get_parameter(0xE1,0x14));
  low_clear_T   = convert_to_temp(get_parameter(0xE1,0x09));
  low_clear_RH  = convert_to_humi(get_parameter(0xE1,0x09));
  low_set_T     = convert_to_temp(get_parameter(0xE1,0x02));
  low_set_RH    = convert_to_humi(get_parameter(0xE1,0x02));
}

void clear_status_register() {
  Wire.beginTransmission(0x44);    // transmit to device #44
  Wire.write(0x30);                // clears the status register
  Wire.write(0x41);                // clears the status register
  Wire.endTransmission();          // stop transmitting
}

void display_xy(int x, int y, String text, int txt_colour, int txt_size) {
  display.setCursor(x,y);
  display.setTextColor(txt_colour);
  display.setTextSize(txt_size);
  display.print(text);
  display.setTextSize(2); // Back to default text size
}

int read_status_register() {
  device_status = get_parameter(0xF3,0x2D);
  Serial.println(device_status,HEX);
  return device_status;
}

 /* (C) D L BIRD
 *  This function draws a graph on a ILI9341 TFT / LCD display, it requires an array to hold and constrain the data to be graphed.
 *  The variable 'max_readings' determines the maximum number of data elements for each array. Call it with the following parametric data:
 *   x_pos - the x axis top-left position of the graph
 *   y_pos - the y-axis top-left position of the graph, e.g. 100, 200 would draw the graph 100 pixels along and 200 pixels down from the top-left of the screen
 *   width - the width of the graph in pixels
 *   height - height of the graph in pixels
 *   Y1_Max - sets the scale of plotted data, for example 5000 would scale all data to a Y-axis of 5000 maximum
 *   DataArray is parsed by value, externally they can be called anything else, e.g. within the routine it is called data_array1, but externally could be temperature_readings
 *   auto_scale - a logical value (TRUE or FALSE) that switches the Y-axis autoscale On or Off
 *   barchart_on - a logical value (TRUE or FALSE) that switches the drawing mode between barhcart and line graph
 *   barchartmode - a logical value that switches on or off bar-chart graphing, otherwise line.
 *   barchart_colour - a sets the title and graph plotting colour
 *  If called with Y1_Max value of 500 and the data does not exceed 500, then if on, autoscale will retain a 0-500 Y scale and increases the scale to match the highest value  of data 
 *  to be displayed and reduces it accordingly if required. SO if the array holds a maximium value of 20, it will autoscale the y-axis to 0-20
 *  auto_scale_major_tick, if set to 1000 and autoscale it will increment the scale in 1000 steps.
 */
void DrawGraph(int x_pos, int y_pos, int width, int height, int Y1Max, String title, float DataArray[max_readings], boolean auto_scale, boolean barchart_mode, int graph_colour) {
  #define auto_scale_major_tick 5 // Sets the autoscale increment, so axis steps up in units of e.g. 5
  #define yticks 5                // 5 y-axis division markers
  int maxYscale = 0;
  if (auto_scale) {
    for (int i=1; i <= max_readings; i++ ) if (maxYscale <= DataArray[i]) maxYscale = DataArray[i];
    maxYscale = ((maxYscale + auto_scale_major_tick + 2) / auto_scale_major_tick) * auto_scale_major_tick; // Auto scale the graph and round to the nearest value defined, default was Y1Max
    if (maxYscale < Y1Max) Y1Max = maxYscale; 
  }
  //Graph the received data contained in an array
  // Draw the graph outline
  display.drawRect(x_pos,y_pos,width+4,height+3,ILI9341_WHITE);
  display.fillRect(x_pos+1,y_pos+1,width,height+1,ILI9341_BLACK);
  display.setTextSize(2);
  display.setTextColor(graph_colour);
  display.setCursor(x_pos + (width - title.length()*12)/2,y_pos-20); // 12 pixels per char assumed at size 2 (10+2 pixels)
  display.print(title);
  display.setTextSize(1);
  // Draw the data
  int x1,y1,x2,y2;
  for(int gx = 1; gx <= max_readings; gx++){
    x1 = x_pos + gx * width/max_readings + 1; 
    y1 = y_pos + height;
    x2 = x_pos + gx * width/max_readings + 1; // max_readings is the global variable that sets the maximum data that can be plotted 
    y2 = y_pos + height - constrain(DataArray[gx],0,Y1Max) * height / Y1Max + 1;
    if (barchart_mode) {
      display.drawLine(x1,y1,x2,y2,graph_colour);
    } else {
      if (DataArray[gx] != 0) {
        display.drawPixel(x2,y2,graph_colour);
        display.drawPixel(x2,y2-1,graph_colour); // Make the line a double pixel height to emphasise it, -1 makes the graph data go up!
      }
    }
  }
  //Draw the Y-axis scale
  for (int spacing = 0; spacing <= yticks; spacing++) {
    #define number_of_dashes 40
    for (int j=0; j < number_of_dashes; j++){ // Draw dashed graph grid lines
      if (spacing < yticks) display.drawFastHLine((x_pos+1+j*width/number_of_dashes),y_pos+(height*spacing/yticks),width/(2*number_of_dashes),ILI9341_WHITE);
    }
    display.setTextColor(ILI9341_YELLOW);
    display.setCursor((x_pos-25),y_pos+height*spacing/yticks-4);
    char buff[10];
    sprintf(buff, "%4d", Y1Max - Y1Max / yticks * spacing); // Right-justify upto 4-digits
    display.print(buff);
  }
}
//----------------------------------------------------------------------------------------------------

void clear_arrays() {
  for (int x = 0; x <= max_readings; x++){
    temp_readings[x] = 0;
    humi_readings[x] = 0;
  } // Clears the arrays
}
/*
 * logic examples that can e.g. be used in print statements
7==5 ? 4 : 3     // evaluates to 3, since 7 is not equal to 5 Serial.println(7==5 ? 4 : 3) will print 3 because the result of 7==5 is false
7==5+2 ? 4 : 3   // evaluates to 4, since 7 is equal to 5+2
5>3 ? a : b      // evaluates to the value of a, since 5 is greater than 3
a>b ? a : b      // evaluates to whichever is greater, a or b
*/

