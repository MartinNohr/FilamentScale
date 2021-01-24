#pragma once

//#include <time.h>
//#include "SPI.h"
//
//#include <EEPROM.h>
//#include "RotaryDialButton.h"
//#include <TFT_eSPI.h>
#include <stack>

// menu code definitions
#define BTN_SELECT  CRotaryDialButton::BTN_CLICK
#define BTN_NONE    CRotaryDialButton::BTN_NONE
#define BTN_LEFT    CRotaryDialButton::BTN_LEFT
#define BTN_RIGHT   CRotaryDialButton::BTN_RIGHT
#define BTN_LONG    CRotaryDialButton::BTN_LONGPRESS

CRotaryDialButton::Button ReadButton();

// settings
RTC_DATA_ATTR int nDisplayBrightness = 100;           // this is in %

bool bSettingsMode = false;     // set true when settings are displayed
int nCurrentSpool = 0;          // the currently loaded spool

// functions
void GetIntegerValue(MenuItem* menu);
void Calibrate(MenuItem* menu);

enum eDisplayOperation {
    eText,              // handle text with optional %s value
    eTextInt,           // handle text with optional %d value
    eTextCurrentFile,   // adds current basefilename for %s in string
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
    const void* value;                  // associated variable
    long min;                           // the minimum value, also used for ifequal
    long max;                           // the maximum value, also size to compare for if
    int decimals;                       // 0 for int, 1 for 0.1
    char* on;                           // text for boolean true
    char* off;                          // text for boolean false
    // flag is 1 for first time, 0 for changes, and -1 for last call
    void(*change)(MenuItem*, int flag); // call for each change, example: brightness change show effect
};
typedef MenuItem MenuItem;

MenuItem EepromMenu[] = {
    {eExit,false,"Previous Menu"},
 /*   {eBool,false,"Autoload Saved: %s",ToggleBool,&bAutoLoadSettings,0,0,0,"On","Off"},
    {eText,false,"Save Current Settings",SaveEepromSettings},
    {eText,false,"Load Saved Settings",LoadEepromSettings},
 */   {eExit,false,"Previous Menu"},
    // make sure this one is last
    {eTerminate}
};
MenuItem MainMenu[] = {
    {eTextInt,false,"Current Spool: %2d",GetIntegerValue,&nCurrentSpool,0,99},
    {eText,false,"Calibrate",Calibrate},
    {eMenu,false,"Eeprom",{.menu = EepromMenu}},
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
