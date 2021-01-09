/*
 Name:		FilamentScale.ino
 Created:	1/2/2021 6:20:25 PM
 Author:	martin nohr
*/

/*
   -------------------------------------------------------------------------------------
   HX711_ADC
   Arduino library for HX711 24-Bit Analog-to-Digital Converter for Weight Scales
   Olav Kallhovd sept2017
   -------------------------------------------------------------------------------------
*/

/*
   Settling time (number of samples) and data filtering can be adjusted in the config.h file
   For calibration and storing the calibration value in eeprom, see example file "Calibration.ino"

   The update() function checks for new data and starts the next conversion. In order to acheive maximum effective
   sample rate, update() should be called at least as often as the HX711 sample rate; >10Hz@10SPS, >80Hz@80SPS.
   If you have other time consuming code running (i.e. a graphical LCD), consider calling update() from an interrupt routine,
   see example file "Read_1x_load_cell_interrupt_driven.ino".

   This is an example sketch on how to use this library
*/
#include <vector>
#define DIAL_BTN 15
#define DIAL_A 12
#define DIAL_B 13
#define FRAMEBUTTON 22
#include "RotaryDialButton.h"

#include <HX711_ADC.h>
#include <EEPROM.h>
// display things
#include <TFT_eSPI.h>
#include "fonts.h"
TFT_eSPI tft = TFT_eSPI();       // Invoke custom library
#define TFT_ENABLE 4
// use these to control the LCD brightness
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;
// functions
void DisplayLine(int line, String text, int16_t color = TFT_WHITE);

//pins:
const int HX711_dout = 21; //mcu > HX711 dout pin
const int HX711_sck = 22; //mcu > HX711 sck pin

//HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);
// where to store the calibration value
const int calVal_calVal_eepromAdress = 0;
long t;

// storage for each filament spool
struct FILSPOOL {
    int spoolWeight;
    int remainingWeight;
};
typedef FILSPOOL FilSpool;
std::vector<FilSpool>SpoolArray;

void setup() {
    Serial.begin(115200); delay(10);
    Serial.println();
    Serial.println("Starting...");
    CRotaryDialButton::getInstance()->begin(DIAL_A, DIAL_B, DIAL_BTN);
    tft.init();
    // configure LCD PWM functionalitites
    pinMode(TFT_ENABLE, OUTPUT);
    digitalWrite(TFT_ENABLE, 1);
    ledcSetup(ledChannel, freq, resolution);
    // attach the channel to the GPIO to be controlled
    ledcAttachPin(TFT_ENABLE, ledChannel);
    ledcWrite(ledChannel, 255);
    tft.fillScreen(TFT_BLACK);
    tft.setRotation(3);
    tft.setFreeFont(&Dialog_bold_16);
    DisplayLine(1, "Filament");

    float calibrationValue; // calibration value
    calibrationValue = 696.0; // uncomment this if you want to set this value in the sketch
  //EEPROM.begin(512); // uncomment this if you use ESP8266 and want to fetch this value from eeprom

    LoadCell.begin();
    long stabilizingtime = 2000; // tare precision can be improved by adding a few seconds of stabilizing time
    boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
    LoadCell.start(stabilizingtime, _tare);
    if (LoadCell.getTareTimeoutFlag()) {
        tft.setTextColor(TFT_RED);
        tft.setCursor(0, 15);
        tft.println("Load Cell Failed");
        Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
    }
    else {
        LoadCell.setCalFactor(calibrationValue); // set calibration factor (float)
        Serial.println("Startup is complete");
    }
    while (!LoadCell.update());
    Serial.print("Calibration value: ");
    Serial.println(LoadCell.getCalFactor());
    Serial.print("HX711 measured conversion time ms: ");
    Serial.println(LoadCell.getConversionTime());
    Serial.print("HX711 measured sampling rate HZ: ");
    Serial.println(LoadCell.getSPS());
    Serial.print("HX711 measured settlingtime ms: ");
    Serial.println(LoadCell.getSettlingTime());
    Serial.println("Note that the settling time may increase significantly if you use delay() in your sketch!");
    if (LoadCell.getSPS() < 7) {
        Serial.println("!!Sampling rate is lower than specification, check MCU>HX711 wiring and pin designations");
    }
    else if (LoadCell.getSPS() > 100) {
        Serial.println("!!Sampling rate is higher than specification, check MCU>HX711 wiring and pin designations");
    }
}

void loop() {
    static boolean newDataReady = 0;
    const int serialPrintInterval = 500; //increase value to slow down serial print activity

    // check for new data/start next conversion:
    if (LoadCell.update()) newDataReady = true;

    // get smoothed value from the dataset:
    if (newDataReady) {
        if (millis() > t + serialPrintInterval) {
            float i = LoadCell.getData();
            Serial.print("Load_cell output val: ");
            Serial.println(i);
            newDataReady = 0;
            t = millis();
			DrawProgressBar(0, 0, tft.width() - 1, 12, (int)(i * tft.width() / 1500));
        }
    }

    // receive command from serial terminal, send 't' to initiate tare operation:
    if (Serial.available() > 0) {
        float i;
        char inByte = Serial.read();
        if (inByte == 't') LoadCell.tareNoDelay();
    }

    // check if last tare operation is complete:
    if (LoadCell.getTareStatus() == true) {
        Serial.println("Tare complete");
    }
}

void DisplayLine(int line, String text, int16_t color)
{
    int charHeight = tft.fontHeight();
    int y = line * charHeight;
    tft.fillRect(0, y, tft.width(), charHeight, TFT_BLACK);
    tft.setTextColor(color);
    tft.drawString(text, 0, y);
}

// draw a progress bar
void DrawProgressBar(int x, int y, int dx, int dy, int percent)
{
    percent = constrain(percent, 0, 100);
    tft.drawRoundRect(x, y, dx, dy, 2, TFT_WHITE);
    int fill = (dx - 2) * percent / 100;
    // fill the filled part
    tft.fillRect(x + 1, y + 1, fill, dy - 2, TFT_GREEN);
    // blank the empty part
    tft.fillRect(x + 1 + fill, y + 1, dx - 2 - fill, dy - 2, TFT_BLACK);
}
