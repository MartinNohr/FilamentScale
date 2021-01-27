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
   Settling time (number of samples) and data filtering can be adjusted in the config.h file
   For calibration and storing the calibration value in eeprom, see example file "Calibration.ino"

   The update() function checks for new data and starts the next conversion. In order to acheive maximum effective
   sample rate, update() should be called at least as often as the HX711 sample rate; >10Hz@10SPS, >80Hz@80SPS.
   If you have other time consuming code running (i.e. a graphical LCD), consider calling update() from an interrupt routine,
   see example file "Read_1x_load_cell_interrupt_driven.ino".
*/
#include "FilamentScale.h"

void setup() {
    Serial.begin(115200); delay(10);
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

    float calibrationValue; // calibration value
    //calibrationValue = 696.0; // uncomment this if you want to set this value in the sketch
    calibrationValue = 335; // uncomment this if you want to set this value in the sketch
	EEPROM.begin(512); // fetch this value from eeprom
    menuPtr = new MenuInfo;
    MenuStack.push(menuPtr);
    MenuStack.top()->menu = MainMenu;
    MenuStack.top()->index = 0;
    MenuStack.top()->offset = 0;

    LoadCell.begin();
    long stabilizingtime = 2000; // tare precision can be improved by adding a few seconds of stabilizing time
    boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
    LoadCell.start(stabilizingtime, _tare);
    if (LoadCell.getTareTimeoutFlag()) {
        tft.setTextColor(TFT_RED);
        tft.setCursor(0, 15);
        tft.println("Load Cell Failed");
		delay(5000);
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
    // clear the button buffer
    CRotaryDialButton::getInstance()->clear();
	tft.fillScreen(TFT_BLACK);
}

void loop() {
	static bool bLastSettingsMode = true;
    static boolean newDataReady = 0;
    const int serialPrintInterval = 1000; //increase value to slow down serial print activity

    static bool didsomething = false;
	if (!bSettingsMode) {
		if (bLastSettingsMode) {
			bLastSettingsMode = false;
			DisplayLine(6, "Click for Menu",TFT_BLUE);
		}
		if (CRotaryDialButton::getCount()) {
			CRotaryDialButton::Button btn = CRotaryDialButton::dequeue();
			if (btn == CRotaryDialButton::BTN_CLICK) {
				bLastSettingsMode = bSettingsMode = true;
			}
		}
	}
	else {
		didsomething = HandleMenus();
	}

    // check for new data/start next conversion:
	if (!bSettingsMode && LoadCell.update())
		newDataReady = true;

    // get smoothed value from the dataset:
	if (!bSettingsMode && newDataReady) {
        if (millis() > t + serialPrintInterval) {
            float i = LoadCell.getData();
            //Serial.print("Load_cell output val: ");
            //Serial.println(i);
            newDataReady = 0;
            t = millis();
            int percent = (int)(i * tft.width() / 1500);
            percent = constrain(percent, 0, 100);
			DrawProgressBar(0, 0, tft.width() - 1, 12, percent);
            String st;
			st = "Spool(" + String(nCurrentSpool) + "): " + String(percent) + "%";
            DisplayLine(1, st);
            st = "Weight: " + String(i);
            DisplayLine(2, st);
        }
    }

    // receive command from serial terminal
    if (Serial.available() > 0) {
		String str;
		Serial.setTimeout(10000);
		Serial.println("reading serial");
		str = Serial.readStringUntil('\n');
		Serial.println("str: " + str);
		char inByte = ' ';
		//char inByte = Serial.read();
		//Serial.println("got char: " + String(inByte));
		switch (inByte) {
		case 't':
			LoadCell.tareNoDelay();
			break;
		case 'r':
			// manual calibrate
			Calibrate();
			break;
		}
		delay(1000);
		//Serial.flush();
    }

    // check if last tare operation is complete:
    if (LoadCell.getTareStatus() == true) {
        Serial.println("Tare complete");
    }
}

void Calibrate(MenuItem* menu)
{
	tft.fillScreen(TFT_BLACK);
	DisplayLine(0, "Remove spools");
	DisplayLine(1, "Push for next step");
	CRotaryDialButton::waitButton(false, 60000);
	return;
	Serial.println("***");
	Serial.println("Start calibration:");
	Serial.println("Place the load cell an a level stable surface.");
	Serial.println("Remove any load applied to the load cell.");
	Serial.println("Send 't' from serial monitor to set the tare offset.");

	boolean _resume = false;
	while (_resume == false) {
		LoadCell.update();
		if (Serial.available() > 0) {
			if (Serial.available() > 0) {
				float i;
				char inByte = Serial.read();
				if (inByte == 't') LoadCell.tareNoDelay();
			}
		}
		if (LoadCell.getTareStatus() == true) {
			Serial.println("Tare complete");
			_resume = true;
		}
	}

	Serial.println("Now, place your known mass on the loadcell.");
	Serial.println("Then send the weight of this mass (i.e. 100.0) from serial monitor.");

	float known_mass = 0;
	_resume = false;
	while (_resume == false) {
		LoadCell.update();
		if (Serial.available() > 0) {
			known_mass = Serial.parseFloat();
			if (known_mass != 0) {
				Serial.print("Known mass is: ");
				Serial.println(known_mass);
				_resume = true;
			}
		}
	}

	LoadCell.refreshDataSet(); //refresh the dataset to be sure that the known mass is measured correct
	float newCalibrationValue = LoadCell.getNewCalibration(known_mass); //get the new calibration value

	Serial.print("New calibration value has been set to: ");
	Serial.print(newCalibrationValue);
	Serial.println(", use this as calibration value (calFactor) in your project sketch.");
	Serial.print("Save this value to EEPROM address ");
	Serial.print(calVal_eepromAddress);
	Serial.println("? y/n");

	_resume = false;
	while (_resume == false) {
		if (Serial.available() > 0) {
			char inByte = Serial.read();
			if (inByte == 'y') {
				EEPROM.begin(512);
				EEPROM.put(calVal_eepromAddress, newCalibrationValue);
				EEPROM.commit();
				EEPROM.get(calVal_eepromAddress, newCalibrationValue);
				Serial.print("Value ");
				Serial.print(newCalibrationValue);
				Serial.print(" saved to EEPROM address: ");
				Serial.println(calVal_eepromAddress);
				_resume = true;

			}
			else if (inByte == 'n') {
				Serial.println("Value not saved to EEPROM");
				_resume = true;
			}
		}
	}

	Serial.println("End calibration");
	Serial.println("***");
	Serial.println("To re-calibrate, send 'r' from serial monitor.");
	Serial.println("For manual edit of the calibration value, send 'c' from serial monitor.");
	Serial.println("***");
}

void changeSavedCalFactor() {
	float oldCalibrationValue = LoadCell.getCalFactor();
	boolean _resume = false;
	Serial.println("***");
	Serial.print("Current value is: ");
	Serial.println(oldCalibrationValue);
	Serial.println("Now, send the new value from serial monitor, i.e. 696.0");
	float newCalibrationValue;
	while (_resume == false) {
		if (Serial.available() > 0) {
			newCalibrationValue = Serial.parseFloat();
			if (newCalibrationValue != 0) {
				Serial.print("New calibration value is: ");
				Serial.println(newCalibrationValue);
				LoadCell.setCalFactor(newCalibrationValue);
				_resume = true;
			}
		}
	}
	_resume = false;
	Serial.print("Save this value to EEPROM adress ");
	Serial.print(calVal_eepromAddress);
	Serial.println("? y/n");
	while (_resume == false) {
		if (Serial.available() > 0) {
			char inByte = Serial.read();
			if (inByte == 'y') {
				EEPROM.begin(512);
				EEPROM.put(calVal_eepromAddress, newCalibrationValue);
				EEPROM.commit();
				EEPROM.get(calVal_eepromAddress, newCalibrationValue);
				Serial.print("Value ");
				Serial.print(newCalibrationValue);
				Serial.print(" saved to EEPROM address: ");
				Serial.println(calVal_eepromAddress);
				_resume = true;
			}
			else if (inByte == 'n') {
				Serial.println("Value not saved to EEPROM");
				_resume = true;
			}
		}
	}
	Serial.println("End change calibration value");
	Serial.println("***");
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

bool RunMenus(int button)
{
	// see if we got a menu match
	bool gotmatch = false;
	int menuix = 0;
	MenuInfo* oldMenu;
	bool bExit = false;
	for (int ix = 0; !gotmatch && MenuStack.top()->menu[ix].op != eTerminate; ++ix) {
		// see if this is one is valid
		if (!MenuStack.top()->menu[ix].valid) {
			continue;
		}
		//Serial.println("menu button: " + String(button));
		if (button == BTN_SELECT && menuix == MenuStack.top()->index) {
			//Serial.println("got match " + String(menuix) + " " + String(MenuStack.top()->index));
			gotmatch = true;
			//Serial.println("clicked on menu");
			// got one, service it
			switch (MenuStack.top()->menu[ix].op) {
			case eText:
			case eTextInt:
			case eTextCurrentFile:
			case eBool:
				bMenuChanged = true;
				if (MenuStack.top()->menu[ix].function) {
					(*MenuStack.top()->menu[ix].function)(&MenuStack.top()->menu[ix]);
				}
				break;
			case eList:
				bMenuChanged = true;
				if (MenuStack.top()->menu[ix].function) {
					(*MenuStack.top()->menu[ix].function)(&MenuStack.top()->menu[ix]);
				}
				bExit = true;
				// if there is a value, set the min value in it
				if (MenuStack.top()->menu[ix].value) {
					*(int*)MenuStack.top()->menu[ix].value = MenuStack.top()->menu[ix].min;
				}
				break;
			case eMenu:
				if (MenuStack.top()->menu) {
					oldMenu = MenuStack.top();
					MenuStack.push(new MenuInfo);
					MenuStack.top()->menu = oldMenu->menu[ix].menu;
					bMenuChanged = true;
					MenuStack.top()->index = 0;
					MenuStack.top()->offset = 0;
					//Serial.println("change menu");
					// check if the new menu is an eList and if it has a value, if it does, set the index to it
					if (MenuStack.top()->menu->op == eList && MenuStack.top()->menu->value) {
						int ix = *(int*)MenuStack.top()->menu->value;
						MenuStack.top()->index = ix;
						// adjust offset if necessary
						if (ix > 4) {
							MenuStack.top()->offset = ix - 4;
						}
					}
				}
				break;
			case eExit: // go back a level
				bExit = true;
				break;
			case eReboot:
				ESP.restart();
				break;
			}
		}
		++menuix;
	}
	// if no match, and we are in a submenu, go back one level, or if bExit is set
	if (bExit || (!bMenuChanged && MenuStack.size() > 1)) {
		bMenuChanged = true;
		if (MenuStack.size() <= 1) {
			bSettingsMode = false;
			tft.fillScreen(TFT_BLACK);
		}
		else {
			menuPtr = MenuStack.top();
			MenuStack.pop();
			delete menuPtr;
		}
	}
}

#define MENU_LINES 7
// display the menu
// if MenuStack.top()->index is > MENU_LINES, then shift the lines up by enough to display them
// remember that we only have room for MENU_LINES lines
void ShowMenu(struct MenuItem* menu)
{
	MenuStack.top()->menucount = 0;
	int y = 0;
	int x = 0;
	char line[100];
	bool skip = false;
	// loop through the menu
	for (; menu->op != eTerminate; ++menu) {
		menu->valid = false;
		switch (menu->op) {
		case eIfEqual:
			// skip the next one if match, only booleans are handled so far
			skip = *(bool*)menu->value != (menu->min ? true : false);
			//Serial.println("ifequal test: skip: " + String(skip));
			break;
		case eElse:
			skip = !skip;
			break;
		case eEndif:
			skip = false;
			break;
		}
		if (skip) {
			menu->valid = false;
			continue;
		}
		char line[100], xtraline[100];
		// only displayable menu items should be in this switch
		line[0] = '\0';
		int val;
		bool exists;
		switch (menu->op) {
		case eTextInt:
		case eText:
		case eTextCurrentFile:
			menu->valid = true;
			if (menu->value) {
				val = *(int*)menu->value;
				if (menu->op == eText) {
					sprintf(line, menu->text, (char*)(menu->value));
				}
				else if (menu->op == eTextInt) {
					if (menu->decimals == 0) {
						sprintf(line, menu->text, val);
					}
					else {
						sprintf(line, menu->text, val / 10, val % 10);
					}
				}
				//Serial.println("menu text1: " + String(line));
			}
			else {
				if (menu->op == eTextCurrentFile) {
					//sprintf(line, menu->text, MakeMIWFilename(FileNames[CurrentFileIndex], false).c_str());
					//Serial.println("menu text2: " + String(line));
				}
				else {
					strcpy(line, menu->text);
					//Serial.println("menu text3: " + String(line));
				}
			}
			// next line
			++y;
			break;
			//case eList:
			//	menu->valid = true;
			//	// the list of macro files
			//	// min holds the macro number
			//	val = menu->min;
			//	// see if the macro is there and append the text
			//	exists = SD.exists("/" + String(val) + ".miw");
			//	sprintf(line, menu->text, val, exists ? menu->on : menu->off);
			//	// next line
			//	++y;
			//	break;
		case eBool:
			menu->valid = true;
			if (menu->value) {
				// clean extra bits, just in case
				bool* pb = (bool*)menu->value;
				//*pb &= 1;
				sprintf(line, menu->text, *pb ? menu->on : menu->off);
				//Serial.println("bool line: " + String(line));
			}
			else {
				strcpy(line, menu->text);
			}
			// increment displayable lines
			++y;
			break;
		case eMenu:
		case eExit:
		case eReboot:
			menu->valid = true;
			if (menu->value) {
				sprintf(xtraline, menu->text, *(int*)menu->value);
			}
			else {
				strcpy(xtraline, menu->text);
			}
			if (menu->op == eExit)
				sprintf(line, "%s%s", "-", xtraline);
			else
				sprintf(line, "%s%s", (menu->op == eReboot) ? "" : "+", xtraline);
			++y;
			break;
		}
		if (strlen(line) && y >= MenuStack.top()->offset) {
			DisplayMenuLine(y - 1, y - 1 - MenuStack.top()->offset, line);
		}
	}
	MenuStack.top()->menucount = y;
	// blank the rest of the lines
	for (int ix = y; ix < MENU_LINES; ++ix) {
		DisplayLine(ix, "");
	}
	// show line if menu has been scrolled
	if (MenuStack.top()->offset > 0)
		tft.drawLine(0, 0, 5, 0, menuLineActiveColor);
	// show bottom line if last line is showing
	if (MenuStack.top()->offset + (MENU_LINES - 1) < MenuStack.top()->menucount - 1)
		tft.drawLine(0, tft.height() - 1, 5, tft.height() - 1, menuLineActiveColor);
	else
		tft.drawLine(0, tft.height() - 1, 5, tft.height() - 1, TFT_BLACK);
	// see if we need to clean up the end, like when the menu shrank due to a choice
	int extra = MenuStack.top()->menucount - MenuStack.top()->offset - MENU_LINES;
	while (extra < 0) {
		DisplayLine(MENU_LINES + extra, "");
		++extra;
	}
}

// toggle a boolean value
void ToggleBool(MenuItem* menu)
{
	bool* pb = (bool*)menu->value;
	*pb = !*pb;
	if (menu->change != NULL) {
		(*menu->change)(menu, -1);
	}
	//Serial.println("autoload: " + String(bAutoLoadSettings));
	//Serial.println("fixed time: " + String(bFixedTime));
}

// get integer values
void GetIntegerValue(MenuItem* menu)
{
	// -1 means to reset to original
	int stepSize = 1;
	int originalValue = *(int*)menu->value;
	//Serial.println("int: " + String(menu->text) + String(*(int*)menu->value));
	char line[50];
	CRotaryDialButton::Button button = BTN_NONE;
	bool done = false;
	tft.fillScreen(TFT_BLACK);
	DisplayLine(1, "Range: " + String(menu->min) + " to " + String(menu->max));
	DisplayLine(3, "Long Press to Accept");
	int oldVal = *(int*)menu->value;
	if (menu->change != NULL) {
		(*menu->change)(menu, 1);
	}
	do {
		//Serial.println("button: " + String(button));
		switch (button) {
		case BTN_LEFT:
			if (stepSize != -1)
				*(int*)menu->value -= stepSize;
			break;
		case BTN_RIGHT:
			if (stepSize != -1)
				*(int*)menu->value += stepSize;
			break;
		case BTN_SELECT:
			if (stepSize == -1) {
				stepSize = 1;
			}
			else {
				stepSize *= 10;
			}
			if (stepSize > (menu->max / 10)) {
				stepSize = -1;
			}
			break;
		case BTN_LONG:
			if (stepSize == -1) {
				*(int*)menu->value = originalValue;
				stepSize = 1;
			}
			else {
				done = true;
			}
			break;
		}
		// make sure within limits
		*(int*)menu->value = constrain(*(int*)menu->value, menu->min, menu->max);
		// show slider bar
		tft.fillRect(0, 2 * tft.fontHeight(), tft.width() - 1, 6, TFT_BLACK);
		DrawProgressBar(0, 2 * tft.fontHeight() + 5, tft.width() - 1, 6, map(*(int*)menu->value, menu->min, menu->max, 0, 100));
		if (menu->decimals == 0) {
			sprintf(line, menu->text, *(int*)menu->value);
		}
		else {
			sprintf(line, menu->text, *(int*)menu->value / 10, *(int*)menu->value % 10);
		}
		DisplayLine(0, line);
		DisplayLine(4, stepSize == -1 ? "Reset: long press (Click +)" : "step: " + String(stepSize) + " (Click +)");
		if (menu->change != NULL && oldVal != *(int*)menu->value) {
			(*menu->change)(menu, 0);
			oldVal = *(int*)menu->value;
		}
		while (!done && (button = ReadButton()) == BTN_NONE) {
			delay(1);
		}
	} while (!done);
	if (menu->change != NULL) {
		(*menu->change)(menu, -1);
	}
}

void SetDisplayBrightness(int val)
{
	ledcWrite(ledChannel, map(val, 0, 100, 0, 255));
}

void UpdateDisplayBrightness(MenuItem* menu, int flag)
{
	// control LCD brightness
	SetDisplayBrightness(*(int*)menu->value);
}

uint16_t ColorList[] = {
	//TFT_NAVY,
	//TFT_MAROON,
	//TFT_OLIVE,
	TFT_WHITE,
	TFT_LIGHTGREY,
	TFT_BLUE,
	TFT_SKYBLUE,
	TFT_CYAN,
	TFT_RED,
	TFT_BROWN,
	TFT_GREEN,
	TFT_MAGENTA,
	TFT_YELLOW,
	TFT_ORANGE,
	TFT_GREENYELLOW,
	TFT_GOLD,
	TFT_SILVER,
	TFT_VIOLET,
	TFT_PURPLE,
};

// find the color in the list
int FindMenuColor(uint16_t col)
{
	int ix;
	int colors = sizeof(ColorList) / sizeof(*ColorList);
	for (ix = 0; ix < colors; ++ix) {
		if (col == ColorList[ix])
			break;
	}
	return constrain(ix, 0, colors - 1);
}

void SetMenuColors(MenuItem* menu)
{
	int maxIndex = sizeof(ColorList) / sizeof(*ColorList) - 1;
	int mode = 0;	// 0 for active menu line, 1 for menu line
	int colorIndex = FindMenuColor(menuLineColor);
	int colorActiveIndex = FindMenuColor(menuLineActiveColor);
	tft.fillScreen(TFT_BLACK);
	DisplayLine(4, "Rotate change value");
	DisplayLine(5, "Long Press Exit");
	bool done = false;
	bool change = true;
	while (!done) {
		if (change) {
			DisplayLine(3, String("Click: ") + (mode ? "Normal" : "Active") + " Color");
			DisplayLine(0, "Active", menuLineActiveColor);
			DisplayLine(1, "Normal", menuLineColor);
			change = false;
		}
		switch (CRotaryDialButton::getInstance()->dequeue()) {
		case CRotaryDialButton::BTN_CLICK:
			if (mode == 0)
				mode = 1;
			else
				mode = 0;
			change = true;
			break;
		case CRotaryDialButton::BTN_LONGPRESS:
			done = true;
			break;
		case CRotaryDialButton::BTN_RIGHT:
			change = true;
			if (mode)
				colorIndex = ++colorIndex;
			else
				colorActiveIndex = ++colorActiveIndex;
			break;
		case CRotaryDialButton::BTN_LEFT:
			change = true;
			if (mode)
				colorIndex = --colorIndex;
			else
				colorActiveIndex = --colorActiveIndex;
			break;
		}
		colorIndex = constrain(colorIndex, 0, maxIndex);
		menuLineColor = ColorList[colorIndex];
		colorActiveIndex = constrain(colorActiveIndex, 0, maxIndex);
		menuLineActiveColor = ColorList[colorActiveIndex];
	}
}

// handle the menus
bool HandleMenus()
{
	if (bMenuChanged) {
		ShowMenu(MenuStack.top()->menu);
		bMenuChanged = false;
	}
	bool didsomething = true;
	CRotaryDialButton::Button button = ReadButton();
	int lastOffset = MenuStack.top()->offset;
	int lastMenu = MenuStack.top()->index;
	int lastMenuCount = MenuStack.top()->menucount;
	switch (button) {
	case BTN_SELECT:
		RunMenus(button);
		bMenuChanged = true;
		break;
	case BTN_RIGHT:
		if (bAllowMenuWrap || MenuStack.top()->index < MenuStack.top()->menucount - 1) {
			++MenuStack.top()->index;
		}
		if (MenuStack.top()->index >= MenuStack.top()->menucount) {
			MenuStack.top()->index = 0;
			bMenuChanged = true;
			MenuStack.top()->offset = 0;
		}
		// see if we need to scroll the menu
		if (MenuStack.top()->index - MenuStack.top()->offset > (MENU_LINES - 1)) {
			if (MenuStack.top()->offset < MenuStack.top()->menucount - MENU_LINES) {
				++MenuStack.top()->offset;
			}
		}
		break;
	case BTN_LEFT:
		if (bAllowMenuWrap || MenuStack.top()->index > 0) {
			--MenuStack.top()->index;
		}
		if (MenuStack.top()->index < 0) {
			MenuStack.top()->index = MenuStack.top()->menucount - 1;
			bMenuChanged = true;
			MenuStack.top()->offset = MenuStack.top()->menucount - MENU_LINES;
		}
		// see if we need to adjust the offset
		if (MenuStack.top()->offset && MenuStack.top()->index < MenuStack.top()->offset) {
			--MenuStack.top()->offset;
		}
		break;
	case BTN_LONG:
		tft.fillScreen(TFT_BLACK);
		bSettingsMode = false;
		bMenuChanged = true;
		break;
	default:
		didsomething = false;
		break;
	}
	// check some conditions that should redraw the menu
	if (lastMenu != MenuStack.top()->index || lastOffset != MenuStack.top()->offset) {
		bMenuChanged = true;
		//Serial.println("menu changed");
	}
	return didsomething;
}

// check buttons and return if one pressed
enum CRotaryDialButton::Button ReadButton()
{
	enum CRotaryDialButton::Button retValue = BTN_NONE;
	// read the next button, or NONE it none there
	retValue = CRotaryDialButton::getInstance()->dequeue();
	//if (retValue != BTN_NONE)
	//	Serial.println("button:" + String(retValue));
	return retValue;
}

// the star is used to indicate active menu line
void DisplayMenuLine(int line, int displine, String text)
{
	bool hilite = MenuStack.top()->index == line;
	String mline = (hilite ? "*" : " ") + text;
	if (displine < MENU_LINES)
		DisplayLine(displine, mline, hilite ? menuLineActiveColor : menuLineColor);
}

void DisplayLine(int line, String text, int16_t color)
{
	int charHeight = tft.fontHeight();
	int y = line * charHeight;
	tft.fillRect(0, y, tft.width(), charHeight, TFT_BLACK);
	tft.setTextColor(color);
	tft.drawString(text, 0, y);
}
