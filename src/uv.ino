/***********************************************************************
  For all libraries from Adafruit:
  Adafruit invests time and resources providing this 
  open source code, please support Adafruit and open-
  source hardware by purchasing products from Adafruit!
  Written by Limor Fried/Ladyada for Adafruit Industries 
  BSD license, all text above must be included in any redistribution
 ***********************************************************************/


//---------- LIBRARY INCLUDES
#include "SPI.h"                     // Arduino 1.8.2
#include "Adafruit_SPIFlash.h"       // Adafruit v1.0.0 https://github.com/adafruit/Adafruit_SPIFlash
#include "Adafruit_SPIFlash_FatFs.h" // Adafruit v1.0.0 https://github.com/adafruit/Adafruit_SPIFlash
#include "SimpleTimer.h"             // Timed actions http://playground.arduino.cc/Code/SimpleTimer
#include "RTCZero.h"                 // Real time clock v1.5.2 http://www.arduino.cc/en/Reference/RTCZero
#include "Adafruit_SharpMem.h"       // Adafruit v1.0.4 https://github.com/adafruit/Adafruit_SHARP_Memory_Display 
#include "Adafruit_GFX.h"            // Adafruit v1.2.2 https://github.com/adafruit/Adafruit-GFX-Library
#include "Adafruit_Sensor.h"         // Adafruit v1.0.2 https://github.com/adafruit/Adafruit_Sensor
#include "Adafruit_BME280.h"         // Adafruit v1.0.5 https://github.com/adafruit/Adafruit_BME280_Library
#include "Adafruit_VEML6070.h"       // Adafruit 5-26-16 https://github.com/adafruit/Adafruit_VEML6070


//---------- GLOBAL DECLARATIONS
#define FLASH_TYPE      SPIFLASHTYPE_W25Q16BV           // SPI flash chip type
#define FLASH_SS        SS1                    			// flash chip CS pin (Feather MO Express)
#define FLASH_SPI_PORT  SPI1                   			// use SPI bus 2 (Feather MO Express)
Adafruit_SPIFlash flash(FLASH_SS, &FLASH_SPI_PORT);     // initiate SPI flash system (hardware SPI)
Adafruit_M0_Express_CircuitPython pythonfs(flash);      // initiate Circuit Python file system
#define FILE_NAME		"uvlog.csv"                     // http://elm-chan.org/fsw/ff/en/open.html

#define SPEAKER			A1                              // buzzer pin 
#define VBAT_PIN		A7                              // Adafruit Feather battery monitoring pin
#define BUTTON     		5                               // button pin

SimpleTimer timer;                                      // initiate function timers
RTCZero rtc;                                            // initiate real time clock

#define SHARP_CS 		12
Adafruit_SharpMem display(SCK, MOSI, SHARP_CS);         // 96px x 96px  16 chr x 12 lines
#define BLACK           0                               // foreground color
#define WHITE           1                               // background color
#define CHAR_W          6                               // display size 1 character width
#define CHAR_H          8                               // display size 1 character height
#define ROTATION        2                               // display orientation
#define MARGIN          3                               // margin for graphics


Adafruit_BME280 bme;                                    // initiate BME280 barometer temp humd sensor
#define BME280_ADDRESS 			0x76                    // BME280 I2C Address
#define SEALEVELPRESSURE_HPA 	(1017.8)                // sea level ref reading 1013.25 mb 


Adafruit_VEML6070 uv = Adafruit_VEML6070();             // initiate Adafruit VEML6070 UVB sensor


//---------- GLOBAL VARIABLES
int FITZ          = 13;                                 // Fitzpatrick Score                0-32
int SPF           = 30;                                 // SPF Applied                      0-100
bool H2O          = 0;                                  // On Water                         bool
bool SNOW         = 0;                                  // Snow Present                     bool 
double UVINDEX;                                         // UV Index                         0.00-26.00
double BARO_MB;                                         // Barometer (Pa)                   0-1086
double ALT_M;                                           // Altitude (m)                     0-11000
int MINS2MED;                                           // Mins to Minimal Erythemal Dose   0-inf
double TEMP_C;                                          // Temperature degrees (Celsius)    0-50
double TEMP_F;                                          // Temperature degrees (Fahrenheit) 0-120
double BARO_PA;                                         // Barometer (pascals)              0-10860
double BARO_HG;                                         // Barometer (inches mercury)           
double HUMD;                                            // Relative humididty (%)           0-100
double DEWPOINT;                                        // Dew Point (Fahrenheit)           0-100
double HEATIX;                                          // Heat Index (Fahrenheit)          0-150
double VOLTS;                                           // Adafruit Feather volts measured
char DATE[10];                                          // Date stamp
char TIME[10];                                          // Time stamp
int RECORDS;                                            // Number of records written
int LINE[13] = {0,8,16,24,32,40,48,56,64,72,80,88,96};  // Array for line numbers ea 1 ch high


//---------- SPECIAL FUNCTIONS
double med () {                                         // mins to Minimal Erythemal Dose (MED)
    
    double uvi_f =                                      // UV index factoring
                     (UVINDEX * (ALT_M * 1.2)) +        // alt factor (1.2 x meters above sea level)
                     (UVINDEX * (H2O * 1.5)) +          // on water UVindex factor (1.5x)
                     (UVINDEX * (SNOW * 1.85));         // on snow UVindex factor (1.85x)
                    
    double s2med_b = (-3.209E-5 * pow(FITZ, 5)) +       // Fitzpatrick score @ 1 UV idx secs to MED
                     (2.959E-3 * pow(FITZ, 4)) -        // 5th order polynomial plot
                     (0.103 * pow(FITZ, 3)) +
                     (1.664* pow(FITZ, 2)) +
                     (3.82 * FITZ) + 
                     34.755;
                     
    double s2med = ((s2med_b / uvi_f) * SPF);           // combine factors 
                                                        //   secs to MED at UVindex 1 + 
                                                        //   UV index amplifiing factors
                                                        
    int m2med = s2med / 60;                             // convert secs to mins
    if (m2med > 480) m2med = 480;                       // max at 6 hours
    return m2med;
}
double c2f(double celsius) {                            // convert Celcius to Fahrenheit
    return ((celsius * 9) / 5) + 32; 
}
double pa2hg(double pascals) {                          // convert Pascals to in. mercury
    return (pascals * 0.000295333727); 
}
double dewPointF(double cel, int hum) {                 // calc Dew Point
    double Td = (237.7 * ((17.271 * cel) / 
                (237.7 + cel) + 
                log(hum * 0.01))) / 
                (17.271 - ((17.271 * cel) / 
                (237.7 + cel) + 
                log(hum * 0.01)));
    return c2f(Td);
}
double heatIndex(double tempF, double humd_l) {         // calc Heat Index
    if (tempF < 80 || humd_l < 40) { 
        double pass = tempF; return pass;
    }
    double c1=-42.38, c2=2.049;
    double c3=10.14, c4=-0.2248;
    double c5=-6.838e-3, c6=-5.482e-2;
    double c7=1.228e-3, c8=8.528e-4;
    double c9=-1.99e-6;
    double t = tempF;
    double r = humd_l;
    double a = ((c5 * t) + c2) * t + c1;
    double b = ((c7 * t) + c4) * t + c3;
    double c = ((c9 * t) + c8) * t + c6;
    double rv = (c * r + b) * r + a;
    return rv;
}
char *dtostrf (double val, signed char width, unsigned char prec, char *sout) {
  // convert floats to fixed string
  // val      Your float variable;
  // width    Length of the string that will be created INCLUDING decimal point;
  // prec     Number of digits after the deimal point to print;
  // sout     Destination of output buffer
    char fmt[20];
    sprintf(fmt, "%%%d.%df", width, prec);
    sprintf(sout, fmt, val);
    return sout;
}


//---------- SETUP
void setup() {
    Serial.begin(115200);                               // start serial port for debugging
    
	pinMode(VBAT_PIN, INPUT);                           // set battery pin
    pinMode(BUTTON, INPUT_PULLUP);                      // set button pin


    //--------- REAL TIME CLOCK
    Serial.println("REAL TIME CLOCK");   
    
	byte seconds, minutes, hours;                       // set local time variables
	byte days, months, years;  
    int thour, tminute, tsecond;
    int tmonth, tday, tyear;
    char s_month[5];
    static const char month_names[] = 
        "JanFebMarAprMayJunJulAugSepOctNovDec";
    rtc.begin();                                        // initiate real time clock
    sscanf(__DATE__, "%s %d %d",                        // parse date out of epic
                    s_month, &tday, &tyear);            
    sscanf(__TIME__, "%d:%d:%d",                        // parse time out of epic
                    &thour, &tminute, &tsecond);        
    tmonth = (strstr(month_names, s_month) -            // parse month out of epic
                    month_names) / 3;   
    years = tyear - 2000;                               // adjust epic year from start
    months =  tmonth + 1;                               // set month
    days = tday;                                        // set day
    hours = thour;                                      // set hour
    minutes = tminute;                                  // set minute
    seconds = tsecond;                                  // set second                    
    rtc.setTime(hours, minutes, seconds);               // set real time clock to complile time
    rtc.setDate(days, months, years);                   // set real time clock to complile date


    //--------- DISPLAY
    Serial.println("DISPLAY START");

    display.begin();                                    // initiaite disp & Adafrtuit GFX Library
    display.clearDisplay();                             // clear display
    display.setTextSize(1);                             // set text size
    display.setRotation(ROTATION);                      // set rotation
    display.setTextColor(BLACK, WHITE);                 // set text color fore & back


    //--------- SPI FLASH
    Serial.println("SPI FLASH");

    if (!flash.begin(FLASH_TYPE)) {                     // initiate file system
        displayWrite  ("FLASH INIT FAIL ");
        Serial.println("FLASH INIT FAIL ");
        while(1);
    }

    Serial.print("Flash chip JEDEC ID: 0x");
    Serial.println(flash.GetJEDECID(), HEX);

    if (!pythonfs.begin()) {                            // mount Circuit Python file system
        displayWrite  ("MOUNT: FAIL     ");
        Serial.println("MOUNT: FAIL     ");
        while(1);
    }

    displayWrite("MOUNT: SUCCESS  ");
    Serial.println("MOUNT: SUCCESS  ");

    if (pythonfs.exists("data.txt")) {
        File data = pythonfs.open("data.txt", FILE_READ);
        Serial.println("Print data.txt");
        while (data.available()) {
            char c = data.read();
            Serial.print(c);
        }
        Serial.println();
    }
    else {
        displayWrite  ("FILE READ: FAIL ");
        Serial.println("FILE READ: FAIL ");
    }

    //--------- SET UP LOG FILE
    Serial.println("SET UP LOG FILE");

    File data = pythonfs.open("uvlog.csv", FILE_WRITE); // Circuit Python File Open
    if (data) {                                         // write header record
        data.print("date,");
        data.print("time,");
        data.print("batt(v),");
        data.print("uvi,");
        data.print("med(mins),");
        data.print("alt(m),");
        data.print("temp(c),");
        data.print("temp(f),");
        data.print("baro(mb),");
        data.print("humd(%),");
        data.print("dewpoint(f),");
        data.println("heat index(f),");
        data.close();

        Serial.print("date,");
        Serial.print("time,");
        Serial.print("batt(v),");
        Serial.print("uvi,");
        Serial.print("med(mins),");
        Serial.print("alt(m),");
        Serial.print("temp(c),");
        Serial.print("temp(f),");
        Serial.print("baro(mb),");
        Serial.print("baro(hg),");
        Serial.print("humd(%),");
        Serial.print("dewpoint(f),");
        Serial.println("heat index(f)");

        displayWrite  ("HEADER: SUCCESS ");
        Serial.println("HEADER: SUCCESS ");
    }
    else {
        displayWrite  ("HEADER: FAIL    ");
        Serial.println("HEADER: FAIL    ");
    }
    displayWrite  ("FILEOPS FINISHED");
    Serial.println("FILEOPS FINISHED");


    //--------- SENSORS

    uv.begin(VEML6070_1_T);                             // initiate VEML6070 - pass int time constant

	if (! bme.begin(BME280_ADDRESS)) {                  // initiate BME280
        displayWrite  ("BME280 FAIL     ");
        Serial.println("BME280 FAIL     ");
	    while (1);
    }

    timer.setInterval(20000, FileWrite);                // write readings to SD every 20 seconds
    timer.setInterval(2000, GetReadings);               // get sensor readings every 2 seconds

    chime(1);                                           // chime (x) time(s)

    Serial.println("SETUP DONE");
}

int state       = HIGH;                                 // button state
int reading;                                            // button transient state
int previous    = LOW;                                  // stored button state 
long time       = 0;                                    // button timer
long debounce   = 200;                                  // check for phantom presses (ms)

void loop() {
    timer.run();                                        // check timers
    checkButton();                                      // check for button press
}

void chime(int times) {
    for (int i = 0; i < times; i++) {
        for (int ii = 0; ii < 3; ii++) {
            int melody[] = {506, 1911};
            tone(A1, melody[ii], 300);
            delay(300);
        }
    }
}

void checkButton() {                                    // button handling
    reading = digitalRead(BUTTON);
    if (reading == HIGH && previous == 
            LOW && millis() - time > debounce) {
        if (state == HIGH) state = LOW;
        else state = HIGH;
        time = millis();
    }
    previous = reading;
    if (state == HIGH) {
        displayWrite("Min Erythml Dose");
    }
    else {
        displayWrite("Sensor Readings ");
    }
}


void displayWrite(String statmsg) {                     // draw screen
    header();
    body();
    display.setCursor(0, display.height()-CHAR_H);
    display.print(statmsg);
    display.refresh();
}


void header() {                                         // draw header
    double batt_l = ((analogRead(VBAT_PIN) * 6.6)/1024);
    char batt_d[4];
    dtostrf(batt_l, 2, 1, batt_d);
    char head[13];
    sprintf(head, "%02d:%02d:%02d %sv",
        rtc.getHours(),rtc.getMinutes(), rtc.getSeconds(), batt_d);
    display.setCursor(0, LINE[0]);
    display.print(head);

    char head2[16];
    sprintf(head2, "%02d/%02d  %i recs",
            rtc.getMonth(),rtc.getDay(), RECORDS);
    display.setCursor(0, LINE[1]);
    display.print(head2);

    // ---------- BATTERY ICON VARIABLES
    int BATTICON_WIDTH      = 3 * CHAR_W;               // icon width 3 characters wide
    int BATTICON_STARTX     = display.width() -         // start x position
                            BATTICON_WIDTH; 
    int BATTICON_STARTY     = LINE[0];                  // start y position
    int BATTICON_BARWIDTH3  = ((BATTICON_WIDTH) / 4);   // bar width as equal divs of width


    // ---------- DRAW BATTERY ICON
    display.drawLine( BATTICON_STARTX + 1, 
                      BATTICON_STARTY,
                      BATTICON_STARTX + BATTICON_WIDTH - 4,
                      BATTICON_STARTY, 
                      BLACK);

    display.drawLine( BATTICON_STARTX, 
                      BATTICON_STARTY + 1,
                      BATTICON_STARTX, 
                      BATTICON_STARTY + 5, 
                      BLACK);

    display.drawLine( BATTICON_STARTX + 1,
                      BATTICON_STARTY + 6,
                      BATTICON_STARTX + BATTICON_WIDTH - 4,
                      BATTICON_STARTY + 6, 
                      BLACK);

    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 3,
                      BATTICON_STARTY + 1, 
                      BLACK);

    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 2,
                      BATTICON_STARTY + 1, 
                      BLACK);

    display.drawLine( BATTICON_STARTX + BATTICON_WIDTH - 1,
                      BATTICON_STARTY + 2, 
                      BATTICON_STARTX + 
                      BATTICON_WIDTH - 1, 
                      BATTICON_STARTY + 4, 
                      BLACK);

    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 2,
                      BATTICON_STARTY + 5, 
                      BLACK);

    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 3,
                      BATTICON_STARTY + 5, 
                      BLACK);

    display.drawPixel(BATTICON_STARTX + BATTICON_WIDTH - 3,
                      BATTICON_STARTY + 6, 
                      BLACK);


    // ---------- FILL BATTERY ICON
    if (batt_l > 4.19) {                                // FULL
        display.fillRect(BATTICON_STARTX + 2,           // x
                         BATTICON_STARTY + 2,           // y
                         BATTICON_BARWIDTH3 * 3,        // w
                         3,                             // h
                         BLACK);
        return;
    }

    if (batt_l > 4.00) {                                // 3 bars
        for (uint8_t i = 0; i < 3; i++) {
            display.fillRect(BATTICON_STARTX + 2 + 
                            (i * BATTICON_BARWIDTH3),
                             BATTICON_STARTY + 2,
                             BATTICON_BARWIDTH3 - 1,
                             3, 
                             BLACK);
        }
        return;
    }

    if (batt_l > 3.80) {                                // 2 bars
        for (uint8_t i = 0; i < 2; i++) {
            display.fillRect(BATTICON_STARTX + 2 + 
                            (i * BATTICON_BARWIDTH3),
                             BATTICON_STARTY + 2, 
                             BATTICON_BARWIDTH3 - 1, 
                             3, 
                             BLACK);
        }
        return;
    }

    if (batt_l > 3.40) {                                // 1 bar
            display.fillRect(BATTICON_STARTX + 2,
                             BATTICON_STARTY + 2, 
                             BATTICON_BARWIDTH3 - 1, 
                             3, 
                             BLACK);
    }
}


void body() {
    // ----------- UV INDEX
    char uvi_d[4]; 
    dtostrf(UVINDEX, 2, 1, uvi_d);
    char uvi[16];
    sprintf(uvi, "UV index %s", uvi_d);
    display.setCursor(0, LINE[2]);
    display.print(uvi);



    // ----------- DRAW UVI BAR
    display.fillRect(MARGIN,                            // clear bar
                    LINE[3],
                    display.width() - (MARGIN * 2),
                    CHAR_H,
                    WHITE);

    display.drawRect(MARGIN,                            // draw clear bar
                    LINE[3], 
                    display.width() - (MARGIN * 2),
                    CHAR_H,
                    BLACK);

    display.fillRect(MARGIN,                            // fill bar w/ last UV index reading
                    LINE[3],
                    (UVINDEX * ((display.width() -      // cover 16 divisions
                            (MARGIN * 2)) / 16)),
                    CHAR_H,
                    BLACK);


    // ---------- Mins to Minimal Erythemal Dose 
    char m2med[16];
    sprintf(m2med, "MED in %i min", MINS2MED);
    display.setCursor(0, LINE[4]); 
    display.print(m2med);


    // ---------- ALTITUDE
    char alt_d[4];
    dtostrf(ALT_M, 4, 0, alt_d);
 
    char alt[16];
    sprintf(alt, "Alt %s mtrs", alt_d);
    display.setCursor(0, LINE[5]); 
    display.print(alt);


    // ---------- TEMPC  TEMPF
    char temp_c_d[3]; 
    dtostrf(TEMP_C, 3, 0, temp_c_d);

    char temp_f_d[3]; 
    dtostrf(TEMP_F, 3, 0, temp_f_d);

    char temps[16];
    sprintf(temps, "Temps %sC %sF", temp_c_d, temp_f_d);
    display.setCursor(0, LINE[6]); 
    display.print(temps);


    // ---------- HUMD  BARO
    char humd_d[2]; 
    dtostrf(HUMD, 2, 0, humd_d);

    char baro_d[5];
    dtostrf(BARO_HG, 2, 2, baro_d);

    char humdbaro[16];
    sprintf(humdbaro, "%s%%rh %shg", humd_d, baro_d);
    display.setCursor(0, LINE[7]); 
    display.print(humdbaro);


    // ---------- DEWPOINT
    char dew_d[2]; 
    dtostrf(DEWPOINT, 2, 0, dew_d);

    char dew[16];
    sprintf(dew, "Dewpoint %sf", dew_d);
    display.setCursor(0, LINE[8]); 
    display.print(dew);


    // ---------- HEAT INDEX
    char heat_d[3]; 
    dtostrf(HEATIX, 3, 0, heat_d);

    char heat[16];
    sprintf(heat, "Heat Index %sf", heat_d);
    display.setCursor(0, LINE[9]);
    display.print(heat);
}


void GetReadings() {                                        // readings by timer interval
    sprintf(DATE, "%02d%02d%02d",                           // date from real time clock
        rtc.getYear(),rtc.getMonth(),rtc.getDay());
    sprintf(TIME, "%02d%02d%02d",                           // time from real time clock
        rtc.getHours(),rtc.getMinutes(), rtc.getSeconds()); 
    UVINDEX     = uv.readUV();                              // VEML6070 UV reading
    UVINDEX     /= 100.0;                                   // convert to UV Index
    VOLTS       = (analogRead(VBAT_PIN) * 6.6)/1024;        // FEATHER battery voltage reading
    TEMP_C      = bme.readTemperature();                    // BME280 temperature (Celsius) reading
    TEMP_F      = c2f(TEMP_C);                              // convert Celsius reading to Fahrenheit
    BARO_PA     = bme.readPressure();                       // BME280 barometric pressure (Pascals) reading 
    BARO_MB     = BARO_PA / 100;                            // convert Pascals reading to millibars
    BARO_HG     = pa2hg(BARO_PA);                           // convert Pascals reading to Inches Mercury
    ALT_M       = bme.readAltitude(SEALEVELPRESSURE_HPA);   // BME280 calculated altitude reading
    if (ALT_M < 10.00) ALT_M = 0;                           // zero altitude if under 10 meters 
    HUMD        = bme.readHumidity();                       // BME280 relative humididty (%) reading
    DEWPOINT    = dewPointF(TEMP_C, HUMD);                  // calc Dew Point
    HEATIX      = heatIndex(TEMP_F, HUMD);                  // calc Heat Index
    MINS2MED    = med();                                    // calc mins to Minimal Erythemal Dose (MED)

    Serial.print("READINGS:");
    Serial.print(DATE);         Serial.print(" | ");
    Serial.print(TIME);         Serial.print(" | ");
    Serial.print(VOLTS);        Serial.print(" | ");
    Serial.print(UVINDEX);      Serial.print(" | ");
    Serial.print(MINS2MED);     Serial.print(" | ");
    Serial.print(TEMP_C);       Serial.print(" | ");
    Serial.print(TEMP_F);       Serial.print(" | ");
    Serial.print(HUMD);         Serial.print(" | ");
    Serial.print(ALT_M);        Serial.print(" | ");
    Serial.print(BARO_PA);      Serial.print(" | ");
    Serial.print(BARO_MB);      Serial.print(" | ");
    Serial.print(BARO_HG);      Serial.print(" | ");
    Serial.print(DEWPOINT);     Serial.print(" | ");
    Serial.println(HEATIX);
}


void FileWrite() {

    File data = pythonfs.open("uvlog.csv", FILE_WRITE);

  	if (data) {	
  	    data.print(DATE);			    data.print(",");
    	data.print(TIME);		        data.print(",");
    	data.print(VOLTS);  	        data.print(",");
    	data.print(UVINDEX, 1);		    data.print(",");
        data.print(med(),0);            data.print(",");
    	data.print(ALT_M,1);		    data.print(",");
        data.print(TEMP_C,0);           data.print(",");
        data.print(TEMP_F,0);           data.print(",");
        data.print(BARO_MB,0);          data.print(",");
        data.print(HUMD,0);             data.print(",");
        data.print(DEWPOINT,0);         data.print(",");
        data.println(HEATIX,0);
    	data.close();

        Serial.print("FILE WRITE: ");
        Serial.print(DATE);             Serial.print(" | ");
        Serial.print(TIME);             Serial.print(" | ");
        Serial.print(VOLTS);            Serial.print(" | ");
        Serial.print(UVINDEX);          Serial.print(" | ");
        Serial.print(MINS2MED);         Serial.print(" | ");
        Serial.print(ALT_M);            Serial.print(" | ");
        Serial.print(TEMP_C);           Serial.print(" | ");
        Serial.print(TEMP_F);           Serial.print(" | ");
        Serial.print(BARO_MB);          Serial.print(" | ");
        Serial.print(HUMD);             Serial.print(" | ");
        Serial.print(DEWPOINT);         Serial.print(" | ");
        Serial.print(HEATIX);           Serial.print(" | ");
        Serial.println(RECORDS);

		RECORDS++;
  	}
}

