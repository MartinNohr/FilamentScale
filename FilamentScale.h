#pragma once
#include <HX711_ADC.h>
#include <EEPROM.h>
#include <TFT_eSPI.h>
//#include <vector>
#include <stack>
#include <queue>
#include <array>

#define DIAL_BTN 15
#define DIAL_A 13
#define DIAL_B 12
#define FRAMEBUTTON 22

#include "RotaryDialButton.h"
#include "fonts.h"
#include <time.h>

char VersionString[] = "01.00";

// use these to control the LCD brightness
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;

// menu code definitions
#define BTN_SELECT  CRotaryDialButton::BTN_CLICK
#define BTN_NONE    CRotaryDialButton::BTN_NONE
#define BTN_LEFT    CRotaryDialButton::BTN_LEFT
#define BTN_RIGHT   CRotaryDialButton::BTN_RIGHT
#define BTN_LONG    CRotaryDialButton::BTN_LONGPRESS

// display things
TFT_eSPI tft = TFT_eSPI();       // Invoke custom library
#define TFT_ENABLE 4
// settings
int nDisplayBrightness = 100;           // this is in %

bool bSettingsMode = false;     // set true when settings are displayed
bool bAllowMenuWrap = false;
uint16_t menuLineColor = TFT_CYAN;
uint16_t menuLineActiveColor = TFT_WHITE;

// keep a list of spool weights
#define MAX_SPOOL_WEIGHTS 100
// for the user we make the index 1 based, so use this macro to access
#define SPOOL_INDEX (nActiveSpool-1)
int SpoolWeights[MAX_SPOOL_WEIGHTS];
int nActiveSpool = 1;	// the currently selected spool
float calibrationValue; // calibration value
long tareOffset;
long nLengthConversion = 33312;		// implied 2 decimals
#define LENGTH_CONVERSION ((float)(nLengthConversion) / 100)
int fullSpoolFilament = 1000;		// grams on a full spool
int serialPrintInterval = 2; //increase value to slow down serial print activity, seconds

struct saveValues {
    void* val;
    int size;
};
// these values are saved in eeprom, the version is first
const saveValues saveValueList[] = {
    {VersionString,sizeof(VersionString)},                      // first
	{&calibrationValue, sizeof(calibrationValue)},
	{&tareOffset, sizeof(tareOffset)},
	{&nLengthConversion, sizeof(nLengthConversion)},
	{&fullSpoolFilament, sizeof(fullSpoolFilament)},
	{&nActiveSpool, sizeof(nActiveSpool)},
	{SpoolWeights, sizeof(SpoolWeights)},
	{&nDisplayBrightness,sizeof(nDisplayBrightness)},
};


// functions
bool HandleMenus();
void ShowMenu(struct MenuItem* menu);
void GetIntegerValue(MenuItem* menu);
void CalculateSpoolWeight(MenuItem* menu = NULL);
void Calibrate(MenuItem* menu = NULL);
void DisplayLine(int line, String text, int16_t color = TFT_WHITE);
void DisplayMenuLine(int line, int displine, String text);
void SetFactorySettings(MenuItem* menu);
void SaveSpoolSettings(MenuItem* menu = NULL);
void LoadSpoolSettings(MenuItem* menu = NULL);
void ChangeSpoolWeight(MenuItem* menu);
void WeighEmptySpool(MenuItem* menu);
void SetMenuDisplayWeight(MenuItem* menu, int flag);
void SetMenuDisplayBrightness(MenuItem* menu, int flag);
void SetTare(MenuItem* menu = NULL);
void ResetUsage(MenuItem* menu = NULL);
bool SavedSettings(bool save, bool bOnlySignature = false);

enum eDisplayOperation {
	eText,              // handle text with optional %s value
	eTextInt,           // handle text with optional %d value
	eBool,              // handle bool using %s and on/off values
	eMenu,              // load another menu
	eExit,              // closes this menu
	eIfEqual,           // start skipping menu entries if match with data value
	eElse,              // toggles the skipping
	eEndif,             // ends an if block
	eBuiltinOptions,    // use an internal settings menu if available, see the internal name,function list below (BuiltInFiles[])
	eReboot,            // reboot the system
	eList,              // used to make a selection from multiple choices
	eTerminate,         // must be last in a menu
};

std::vector<bool> bMenuValid;   // set to indicate menu item  is valid
struct MenuItem {
	enum eDisplayOperation op;
	const char* text;                   // text to display
	union {
		void(*function)(MenuItem*);     // called on click
		MenuItem* menu;                 // jump to another menu
	};
	void* value;                  // associated variable
	long min;                           // the minimum value, also used for ifequal
	long max;                           // the maximum value, also size to compare for if
	int decimals;                       // 0 for int, 1 for 0.1
	char* on;                           // text for boolean true
	char* off;                          // text for boolean false
	// flag is 1 for first time, 0 for changes, and -1 for last call, and -2 for display text call
	void(*change)(MenuItem*, int flag); // call for each change, example: brightness change show effect
};
typedef MenuItem MenuItem;

MenuItem SpoolMenu[] = {
	{eExit,"Previous Menu"},
	{eTextInt,"Active Spool: %2d",GetIntegerValue,&nActiveSpool,1,MAX_SPOOL_WEIGHTS},
	{eText,"Spool Wt from Full",CalculateSpoolWeight},
	{eTextInt,"Weigh Empty Spool",WeighEmptySpool},
	{eTextInt,"Empty Spool Wt: %d g",ChangeSpoolWeight,NULL,1,2000,0,NULL,NULL,SetMenuDisplayWeight},
	{eTextInt,"Full Filament Wt: %d g",GetIntegerValue,&fullSpoolFilament,100,2000},
	{eText,"Save Settings",SaveSpoolSettings},
	//{eText,"Load Spool Settings",LoadSpoolSettings},
	{eExit,"Previous Menu"},
	// make sure this one is last
	{eTerminate}
};
MenuItem ScaleMenu[] = {
	{eExit,"Previous Menu"},
	{eText,"Tare (reset zero)",SetTare},
	{eText,"Calibrate Weight",Calibrate},
	{eTextInt,"Wt to Length: %d.%02d",GetIntegerValue,&nLengthConversion,30000,40000,2},
	{eText,"Save Settings",SaveSpoolSettings},
	{eExit,"Previous Menu"},
	// make sure this one is last
	{eTerminate}
};
MenuItem SystemMenu[] = {
	{eExit,"Previous Menu"},
	{eTextInt,"Display Brightness: %d",GetIntegerValue,&nDisplayBrightness,0,100,0,NULL,NULL,SetMenuDisplayBrightness},
	{eTextInt,"Display Update: %dS",GetIntegerValue,&serialPrintInterval,1,30},
	{eText,"Save Settings",SaveSpoolSettings},
	{eText,"Factory Settings",SetFactorySettings},
	{eReboot,"Reboot System"},
	{eExit,"Previous Menu"},
	// make sure this one is last
	{eTerminate}
};
MenuItem MainMenu[] = {
	{eExit,"Main Screen"},
	{eText,"Reset Usage Rate",ResetUsage},
	{eMenu,"Spool Settings",{.menu = SpoolMenu}},
	{eMenu,"Scale Settings",{.menu = ScaleMenu}},
	{eMenu,"System Settings",{.menu = SystemMenu}},
	{eExit,"Main Screen"},
	// make sure this one is last
	{eTerminate}
};

// a stack for menus so we can find our way back
struct MENUINFO {
	int index;      // active entry
	int offset;     // scrolled amount
	int menucount;  // how many entries in this menu
	MenuItem* menu; // pointer to the menu
};
typedef MENUINFO MenuInfo;
MenuInfo* menuPtr;
std::stack<MenuInfo*> MenuStack;

bool bMenuChanged = true;

// load cell HX711 info
// pins:
const int HX711_dout = 21; //mcu > HX711 dout pin
const int HX711_sck = 22; //mcu > HX711 sck pin
bool bFoundLoadcell = true;

// HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);

// consumption rate numbers
time_t usageStartTime = 0;
long usageStartAmount = 0;