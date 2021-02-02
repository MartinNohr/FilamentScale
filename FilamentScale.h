#pragma once
#include <HX711_ADC.h>
#include <EEPROM.h>
#include <TFT_eSPI.h>
//#include <vector>
#include <stack>
#include <queue>
#include "RotaryDialButton.h"
#include "fonts.h"

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
};


// functions
bool HandleMenus();
void ShowMenu(struct MenuItem* menu);
void GetIntegerValue(MenuItem* menu);
void CalculateSpoolWeight(MenuItem* menu = NULL);
void Calibrate(MenuItem* menu = NULL);
void DisplayLine(int line, String text, int16_t color = TFT_WHITE);
void DisplayMenuLine(int line, int displine, String text);
void SaveSpoolSettings(MenuItem* menu = NULL);
void LoadSpoolSettings(MenuItem* menu = NULL);
void ChangeSpoolWeight(MenuItem* menu);
void WeighEmptySpool(MenuItem* menu);
void SetMenuDisplayWeight(MenuItem* menu, int flag);
void SetTare(MenuItem* menu);
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

struct MenuItem {
	enum eDisplayOperation op;
	bool valid;                         // set to true if displayed for use
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
	{eExit,false,"Previous Menu"},
	{eTextInt,false,"Active Spool: %2d",GetIntegerValue,&nActiveSpool,1,MAX_SPOOL_WEIGHTS},
	{eText,false,"Spool Wt from Full",CalculateSpoolWeight},
	{eTextInt,false,"Weigh Empty Spool",WeighEmptySpool},
	{eTextInt,false,"Empty Spool Wt: %d g",ChangeSpoolWeight,NULL,1,2000,0,NULL,NULL,SetMenuDisplayWeight},
	{eTextInt,false,"Full Spool Wt: %d g",GetIntegerValue,&fullSpoolFilament,100,2000},
	{eText,false,"Save Settings",SaveSpoolSettings},
	//{eText,false,"Load Spool Settings",LoadSpoolSettings},
	{eExit,false,"Previous Menu"},
	// make sure this one is last
	{eTerminate}
};
MenuItem ScaleMenu[] = {
	{eExit,false,"Previous Menu"},
	{eText,false,"Tare (reset zero)",SetTare},
	{eText,false,"Calibrate Weight",Calibrate},
	{eTextInt,false,"Wt to Length: %d.%d",GetIntegerValue,&nLengthConversion,30000,40000,2},
	{eText,false,"Save Settings",SaveSpoolSettings},
	{eExit,false,"Previous Menu"},
	// make sure this one is last
	{eTerminate}
};
MenuItem MainMenu[] = {
	{eExit,false,"Main Screen"},
	{eMenu,false,"Spool Settings",{.menu = SpoolMenu}},
	{eMenu,false,"Scale Settings",{.menu = ScaleMenu}},
	{eReboot,false,"Reboot System"},
	{eExit,false,"Main Screen"},
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

// HX711 constructor:
HX711_ADC LoadCell(HX711_dout, HX711_sck);
