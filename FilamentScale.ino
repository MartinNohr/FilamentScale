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

   The update() function checks for new data and starts the next conversion. In order to achieve maximum effective
   sample rate, update() should be called at least as often as the HX711 sample rate; >10Hz@10SPS, >80Hz@80SPS.
   If you have other time consuming code running (i.e. a graphical LCD), consider calling update() from an interrupt routine,
   see example file "Read_1x_load_cell_interrupt_driven.ino".
*/
#include "FilamentScale.h"

void setup() {
    Serial.begin(115200); delay(10);
    //Serial.println("Starting...");
	CRotaryDialButton::begin(DIAL_A, DIAL_B, DIAL_BTN, GPIO_NUM_0, GPIO_NUM_35, (gpio_num_t)-1, (gpio_num_t)-1, &DialSettings);
	DialSettings.m_bToggleDial = true;
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

	menuPtr = new MenuInfo;
    MenuStack.push(menuPtr);
    MenuStack.top()->menu = MainMenu;
    MenuStack.top()->index = 0;
    MenuStack.top()->offset = 0;
	// read the saved settings after checking the version
	SaveLoadSettings(false, true);
	SaveLoadSettings(false);
	// 0 can't be used, it will cause a calibration failure later
	if (calibrationValue == 0.0)
		calibrationValue = 400;
	Serial.println("calval: " + String(calibrationValue));
	SetLcdBrightness(nDisplayBrightness);
	// a sanity check
	if (calibrationValue > 5000 || calibrationValue < -200) {
		DisplayLine(0, "suspicious calval: " + String(calibrationValue), TFT_RED);
		delay(1000);
	}
    LoadCell.begin();
    // tare precision can be improved by adding a few seconds of stabilizing time, 2000 in this case
    LoadCell.start(2000, false);	// false means don't tare the scale on startup
    if (LoadCell.getTareTimeoutFlag()) {
		DisplayLine(0, "LoadCell Failed", TFT_RED);
		delay(1000);
		DisplayLine(2, "Timeout, check MCU>HX711", TFT_RED);
		delay(2000);
    }
	else {
        LoadCell.setCalFactor(calibrationValue); // set calibration factor (float)
		LoadCell.update();
		LoadCell.setTareOffset(tareOffset);
		DisplayLine(0, "Startup complete", TFT_GREEN);
		delay(1000);
	}
	int tries = 100;
	while (bFoundLoadcell && !LoadCell.update()) {
		if (tries-- <= 0) {
			DisplayLine(0, "Loadcell not responding", TFT_RED);
			delay(2000);
			bFoundLoadcell = false;
		}
		delay(10);
	}
	if (bFoundLoadcell) {
		LoadCell.refreshDataSet();
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
		else if ((int)LoadCell.getSPS() > 100) {
			Serial.println("!!Sampling rate is higher than specification, check MCU>HX711 wiring and pin designations");
		}
	}
	// clear the button buffer
	CRotaryDialButton::clear();
	// reset the usage counters
	ResetUsage();
	tft.fillScreen(TFT_BLACK);
}

void loop() {
	static bool bLastSettingsMode = true;
	static boolean newDataReady = false;
	static bool didsomething = false;
	static int lastMinute = -1;
	if (bSettingsMode) {
		didsomething = HandleMenus();
	}
	else {
		if (bLastSettingsMode) {
			bLastSettingsMode = false;
			DisplayLine(6, "Long Press for Menu", TFT_BLUE);
		}
		if (CRotaryDialButton::getCount()) {
			CRotaryDialButton::Button btn = CRotaryDialButton::dequeue();
			if (btn == CRotaryDialButton::BTN_LONGPRESS) {
				bLastSettingsMode = bSettingsMode = true;
			}
		}
	}

    // check for new data/start next conversion:
	if (!bSettingsMode && bFoundLoadcell && LoadCell.update())
		newDataReady = true;

	static unsigned long timeholder = 0;
    // get smoothed value from the dataset:
	if (!bSettingsMode && newDataReady) {
		if (millis() > timeholder + (serialPrintInterval * 1000)) {
			float weight = LoadCell.getData();
			newDataReady = false;
			timeholder = millis();
			int filamentWeight = (int)((weight - SpoolWeights[SPOOL_INDEX]) + 0.5);
			filamentWeight = constrain(filamentWeight, 0, filamentWeight);
			int percent = (filamentWeight * 100 / fullSpoolFilament);
            percent = constrain(percent, 0, 100);
			DrawProgressBar(0, 0, tft.width() - 1, 12, percent);
            String st;
			st = "Spool " + String(nActiveSpool) + " @ " + String(percent) + "%";
            DisplayLine(1, st);
			st = "Weight: " + String(filamentWeight) + " g";
			DisplayLine(2, st);
			float length = filamentWeight * LENGTH_CONVERSION / 1000.0;
			length = constrain(length, 0, length);
			st = "Length: " + String(length) + " m";
			DisplayLine(3, st);
			// if the usage is 0, then it was reset, so we get the latest value
			if (usageStartAmount == 0) {
				usageStartAmount = filamentWeight;
			}
			// calculate usage rate
			time_t timeNow = time(NULL);
			double elapsedTime = difftime(timeNow, usageStartTime);
			int seconds = (int)round(elapsedTime);
			if (seconds) {
				double rate = (double)(usageStartAmount - filamentWeight) / seconds * 60.0;
				rate = constrain(rate, 0, rate);
				st = "Usage: " + String(rate, 1) + " g/Min";
				DisplayLine(4, st);
				// now get remaining time
				if (rate > 0.0) {
					double minutesLeft = filamentWeight / rate;
					st = "Time Left: " + String((int)(minutesLeft / 60.0)) + ":" + String((int)minutesLeft % 60) + " H:M";
					DisplayLine(5, st);
				}
				else
				{
					DisplayLine(5, "");
				}
			}
		}
    }
}

// set LCD brighntess, 0 to 100
void SetLcdBrightness(uint b)
{
	uint val = constrain(b, 0, 100);
	int bright = map(val, 0, 100, 0, 255);
	ledcWrite(ledChannel, bright);
}

CRotaryDialButton::Button ClickContinue(char* text=NULL)
{
	DisplayLine(6, text == NULL ? "Click to Continue" : text, TFT_BLUE);
	return CRotaryDialButton::waitButton(false, -1);
}

// zero the scale
void SetTare(MenuItem* menu)
{
	tft.fillScreen(TFT_BLACK);
	DisplayLine(0, "Remove Spool");
	DisplayLine(1, "Replace Nut");
	ClickContinue();
	tft.fillScreen(TFT_BLACK);
	DisplayLine(0, "Setting Scale to Zero");
	LoadCell.update();
	LoadCell.tare();
	// get the value so we can save it
	tareOffset = LoadCell.getTareOffset();
	DisplayLine(0, "Scale has been zeroed");
	ClickContinue();
}

// reset the usage numbers
void ResetUsage(MenuItem* menu)
{
	// set the start time
	time(&usageStartTime);
	// clear the usage
	usageStartAmount = 0;
	tft.fillScreen(TFT_BLACK);
	if (menu) {
		DisplayLine(0, "Usage Rate Reset");
		delay(1000);
	}
}

void SetMenuDisplayWeight(MenuItem* menu, int flag)
{
	// TODO: should probably check for eText or eTextInt here
	if (flag == -2) {
		menu->value = &SpoolWeights[SPOOL_INDEX];
	}
}

void SetMenuDisplayBrightness(MenuItem* menu, int flag)
{
	SetLcdBrightness(nDisplayBrightness);
}

void ChangeSpoolWeight(MenuItem* menu)
{
	// add the address and get the integer
	menu->value = &SpoolWeights[SPOOL_INDEX];
	GetIntegerValue(menu);
}

// weight an actual empty spool
void WeighEmptySpool(MenuItem* menu)
{
	tft.fillScreen(TFT_BLACK);
	DisplayLine(0, "Remove spool");
	ClickContinue();
	DisplayLine(0, "Setting Tare...");
	LoadCell.update();
	LoadCell.tare();
	DisplayLine(0, "Tare Complete");
	tareOffset = LoadCell.getTareOffset();
	delay(500);
	DisplayLine(0, "Load Empty Spool");
	ClickContinue();
	DisplayLine(0, "Weighing...");
	LoadCell.update();
	delay(500);
	LoadCell.refreshDataSet(); //refresh the dataset to be sure that the known mass is measured correct
	float emptyWeight = LoadCell.getData();
	SpoolWeights[SPOOL_INDEX] = emptyWeight;
	DisplayLine(0, "Spool Weight: " + String(SpoolWeights[SPOOL_INDEX]));
	ClickContinue();
}

void CalculateSpoolWeight(MenuItem* menu)
{
	// calculate the spool weight by using a full spool and entering the known filament weight
	/*
	1. Load Spool
	2. Enter known filament weight in grams
	3. Spool weight = measured weight - filament weight
	4. Save spool weight in current spool
	5. Save to eeprom?
	*/
	int weight = 1200;
	MenuItem weightMenu = { eTextInt, "Enter Total Grams: %d", GetIntegerValue, &weight, 1, 2000 };
	DisplayLine(0, "Load New Spool");
	ClickContinue();
	GetIntegerValue(&weightMenu);
	tft.fillScreen(TFT_BLACK);
	LoadCell.update();
	delay(500);
	LoadCell.refreshDataSet(); //refresh the dataset to be sure that the known mass is measured correct
	float totalWeight = LoadCell.getData();
	SpoolWeights[SPOOL_INDEX] = totalWeight - weight;
	DisplayLine(0, "Spool Weight: " + String(SpoolWeights[SPOOL_INDEX]));
	ClickContinue();
}

// read or store values in EEPROM
bool SaveLoadSettings(bool save, bool bOnlySignature)
{
	EEPROM.begin(1024);
	bool retvalue = true;
	int blockpointer = 0;
	for (int ix = 0; ix < (sizeof(saveValueList) / sizeof(*saveValueList)); blockpointer += saveValueList[ix++].size) {
		if (save) {
			size_t written;
			written = EEPROM.writeBytes(blockpointer, saveValueList[ix].val, saveValueList[ix].size);
			if (ix == 0 && bOnlySignature) {
				break;
			}
		}
		else {  // load
			if (ix == 0) {
				// check signature
				char svalue[sizeof(VersionString)];
				memset(svalue, 0, sizeof(svalue));
				size_t bytesread = EEPROM.readBytes(0, svalue, sizeof(VersionString));
				if (strcmp(svalue, VersionString)) {
					DisplayLine(0, "fixing bad eeprom version...", TFT_RED);
					delay(1000);
					return SaveLoadSettings(true);
				}
				if (bOnlySignature) {
					return true;
				}
			}
			else {
				EEPROM.readBytes(blockpointer, saveValueList[ix].val, saveValueList[ix].size);
			}
		}
	}
	if (save) {
		retvalue = EEPROM.commit();
	}
	else {
	}
	DisplayLine(0, String(save ? "Settings Saved" : "Settings Loaded"));
	return retvalue;
}

// save the array of weights and the current spool to the eeprom
void SaveSpoolSettings(MenuItem* menu)
{
	tft.fillScreen(TFT_BLACK);
	SaveLoadSettings(true);
	DisplayLine(0, "Settings Saved");
	delay(1000);
}

// save the array of weights and the current spool to the eeprom
void LoadSpoolSettings(MenuItem* menu)
{
	SaveLoadSettings(false);
}

// calibrate the scale using a known weight
void Calibrate(MenuItem* menu)
{
	int weight = 1000;
	MenuItem weightMenu = { eTextInt, "Enter Grams: %d", GetIntegerValue, &weight, 1, 2000 };
	tft.fillScreen(TFT_BLACK);
	DisplayLine(0, "Remove spool");
	ClickContinue();

	boolean _resume = false;
	DisplayLine(0, "Setting Tare...");
	LoadCell.update();
	LoadCell.tare();
	DisplayLine(0, "Tare Complete");
	tareOffset = LoadCell.getTareOffset();
	delay(500);
	DisplayLine(0, "Load Known Weight");
	ClickContinue();

	float known_mass = 0.0;
	// read the value here
	GetIntegerValue(&weightMenu);
	tft.fillScreen(TFT_BLACK);
	known_mass = (float)weight;
	DisplayLine(0, "Calibrating Wt: " + String(known_mass));
	// get the cell reading and add to dataset
	LoadCell.update();
	delay(500);
	LoadCell.refreshDataSet(); //refresh the dataset to be sure that the known mass is measured correct
	calibrationValue = LoadCell.getNewCalibration(known_mass); //get the new calibration value
	DisplayLine(0, "New Calibration: " + String(calibrationValue));
	ClickContinue();
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

// insert newlines into a string so it doesn't wrap in the middle of words when displayed
// existing newlines are honored
String FormatMultiLine(String& input)
{
	String output;
	int lineWidth = 0;
	int lastInputSpace = 0;
	int lastOutputSpace = 0;
	int lastOutputStart = 0;
	// look for spaces and add words to the output, when each line is too long start a new line
	for (int inIx = 0; inIx < input.length(); ++inIx) {
		switch (input[inIx]) {
		case '\n':
			// flush the line
			output += '\n';
			lastOutputStart = output.length();
			break;
		case ' ':
			output += input[inIx];
			// mark the space location so we can go back
			lastInputSpace = inIx;
			lastOutputSpace = output.length();
			break;
		default:
			// check the width after adding this character
			output += input[inIx];
			lineWidth = tft.textWidth(output.substring(lastOutputStart));
			if (lineWidth > tft.width()) {
				// too wide, backup
				output = output.substring(0, lastOutputSpace);
				// add a newline to the output
				output += '\n';
				inIx = lastInputSpace;
				lastOutputStart = output.length();
			}
			break;
		}
	}
	return output;
}

void ClearScreen()
{
	tft.fillScreen(TFT_BLACK);
	//ResetTextLines();
}

// display message on first line, if wait is -1, wait for a key press
void WriteMessage(String txt, bool error, int wait, bool process)
{
	ClearScreen();
	if (process) {
		txt = FormatMultiLine(txt);
	}
	if (error) {
		txt = "**" + txt + "**";
		tft.setTextColor(TFT_RED);
	}
	else {
		tft.setTextColor(menuLineColor);
	}
	tft.setCursor(0, tft.fontHeight());
	tft.setTextWrap(true);
	tft.print(txt);
	if (wait == -1) {
		// wait for a key
		while (ReadButton() == BTN_NONE)
			delay(10);
	}
	else {
		delay(wait);
	}
	tft.setTextColor(TFT_WHITE);
}

// do something from the menu depending on the button argument
// only two buttons are actually handled, SELECT and HELP
void RunMenus(int button)
{
	// save this so we can see if we need to save a new changed value
	bool lastAutoLoadFlag = bAutoLoadSettings;
	// see if we got a menu match
	bool gotmatch = false;
	int menuix = 0;
	MenuInfo* oldMenu;
	bool bExit = false;
	for (int ix = 0; !gotmatch && MenuStack.top()->menu[ix].op != eTerminate; ++ix) {
		// see if this is one is valid
		if (!bMenuValid[ix]) {
			continue;	// and don't increment menuix
		}
		if (menuix == MenuStack.top()->index) {
			gotmatch = true;
			switch (button) {
			case BTN_B0_LONG:	// handle help if there is any
				if (MenuStack.top()->menu[ix].cHelpText) {
					WriteMessage(MenuStack.top()->menu[ix].cHelpText, false, -1, true);
				}
				bMenuChanged = true;
				break;
			case BTN_SELECT:	// handle selection
				// got one, service it
				switch (MenuStack.top()->menu[ix].op) {
				case eTerminate:	// not used, tell compiler
				case eIfEqual:
				case eIfIntEqual:
				case eElse:
				case eEndif:
					break;
				case eText:
				case eTextInt:
				//case eTextCurrentFile:
				case eBool:
				case eList:
					bMenuChanged = true;
					if (MenuStack.top()->menu[ix].change != NULL) {
						(*MenuStack.top()->menu[ix].change)(&MenuStack.top()->menu[ix], 1);
					}
					if (MenuStack.top()->menu[ix].function) {
						(*MenuStack.top()->menu[ix].function)(&MenuStack.top()->menu[ix]);
					}
					if (MenuStack.top()->menu[ix].change != NULL) {
						(*MenuStack.top()->menu[ix].change)(&MenuStack.top()->menu[ix], -1);
					}
					break;
				//case eMacroList:
				//	bMenuChanged = true;
				//	if (MenuStack.top()->menu[ix].change != NULL) {
				//		(*MenuStack.top()->menu[ix].change)(&MenuStack.top()->menu[ix], 1);
				//	}
				//	if (MenuStack.top()->menu[ix].function) {
				//		(*MenuStack.top()->menu[ix].function)(&MenuStack.top()->menu[ix]);
				//	}
				//	if (MenuStack.top()->menu[ix].change != NULL) {
				//		(*MenuStack.top()->menu[ix].change)(&MenuStack.top()->menu[ix], -1);
				//	}
				//	bExit = true;
				//	// if there is a value, set the min value in it
				//	if (MenuStack.top()->menu[ix].value) {
				//		*(int*)MenuStack.top()->menu[ix].value = MenuStack.top()->menu[ix].min;
				//	}
				//	break;
				case eMenu:
					if (MenuStack.top()->menu) {
						oldMenu = MenuStack.top();
						MenuStack.push(new MenuInfo);
						MenuStack.top()->menu = oldMenu->menu[ix].menu;
						bMenuChanged = true;
						MenuStack.top()->index = 0;
						MenuStack.top()->offset = 0;
						//Serial.println("change menu");
						//// check if the new menu is an eMacroList and if it has a value, if it does, set the index to it
						//if (MenuStack.top()->menu->op == eMacroList && MenuStack.top()->menu->value) {
						//	int ix = *(int*)MenuStack.top()->menu->value;
						//	MenuStack.top()->index = ix;
						//	// adjust offset if necessary
						//	if (ix > 4) {
						//		MenuStack.top()->offset = ix - 4;
						//	}
						//}
					}
					break;
				//case eBuiltinOptions: // find it in builtins
				//	if (BuiltInFiles[currentFileIndex.nFileIndex].menu != NULL) {
				//		MenuStack.top()->index = MenuStack.top()->index;
				//		MenuStack.push(new MenuInfo);
				//		MenuStack.top()->menu = BuiltInFiles[currentFileIndex.nFileIndex].menu;
				//		MenuStack.top()->index = 0;
				//		MenuStack.top()->offset = 0;
				//	}
				//	else {
				//		WriteMessage("No settings available for:\n" + String(BuiltInFiles[currentFileIndex.nFileIndex].text));
				//	}
				//	bMenuChanged = true;
				//	break;
				case eExit: // go back a level
					bExit = true;
					break;
				case eReboot:
					WriteMessage("Rebooting in 2 seconds\nHold button for factory reset", false, 2000);
					ESP.restart();
					break;
				}
			}
		}
		++menuix;
	}
	// if no match, and we are in a submenu, go back one level, or if bExit is set
	if (bExit || (!bMenuChanged && MenuStack.size() > 1)) {
		UpMenuLevel(false);
	}
	// see if the autoload flag changed
	if (bAutoLoadSettings != lastAutoLoadFlag) {
		// the flag is now true, so we should save the current settings
		SaveLoadSettings(true);
	}
}

// display the menu
// if MenuStack.top()->index is > nMenuLineCount, then shift the lines up by enough to display them
// remember that we only have room for nMenuLineCount lines
void ShowMenu(struct MenuItem* menu)
{
	MenuStack.top()->menucount = 0;
	int y = 0;
	int x = 0;
	// load with a false to start with
	std::stack<bool> skipStack;
	skipStack.push(false);
	// this is the active stack level, I.E. which level should be processed
	int skipLevel = 1;
	bool bSkipping = false;
	// loop through the menu
	for (int menix = 0; menu->op != eTerminate; ++menu, ++menix) {
		// make sure menu valid vector is big enough
		if (bMenuValid.size() < menix + 1) {
			bMenuValid.resize(menix + 1);
		}
		bMenuValid[menix] = false;
		switch ((menu->op)) {
		case eIfEqual:
			// skip the next one if match, this is boolean only
			skipStack.push(*(bool*)menu->value != (menu->min ? true : false));
			//Serial.println("ifequal test: skip: " + String(skip));
			if (!bSkipping) {
				++skipLevel;
				bSkipping = skipStack.top();
			}
			break;
		case eIfIntEqual:
			// skip the next one if match, this is int values
			skipStack.push(*(int*)menu->value != menu->min);
			//Serial.println("ifIntequal test: skip: " + String(skip));
			if (!bSkipping) {
				++skipLevel;
				bSkipping = skipStack.top();
			}
			break;
		case eElse:
			skipStack.top() = !skipStack.top();
			break;
		case eEndif:
			skipStack.pop();
			if (!bSkipping || skipLevel > skipStack.size()) {
				--skipLevel;
			}
			break;
		default:
			break;
		}
		bSkipping = skipLevel < skipStack.size() ? true : skipStack.top();
		if (bSkipping) {
			bMenuValid[menix] = false;
			continue;
		}
		char line[120]{}, xtraline[100]{};
		// only displayable menu items should be in this switch
		line[0] = '\0';
		int val;
		bool exists = false;
		switch (menu->op) {
		case eTextInt:
		case eText:
		//case eTextCurrentFile:
			bMenuValid[menix] = true;
			if (menu->value) {
				val = *(int*)menu->value;
				if (menu->op == eText) {
					sprintf(line, menu->text, (char*)(menu->value));
				}
				else if (menu->op == eTextInt) {
					sprintf(line, menu->text, (int)(val / pow10(menu->decimals)), val % (int)(pow10(menu->decimals)));
				}
			}
			else {
				//if (menu->op == eTextCurrentFile) {
				//	sprintf(line, menu->text, MakeMIWFilename(FileNames[currentFileIndex.nFileIndex], false).c_str());
				//}
				//else {
					strcpy(line, menu->text);
				//}
			}
			// next line
			++y;
			break;
		//case eMacroList:
		//	bMenuValid[menix] = true;
		//	// the list of macro files
		//	// min holds the macro number
		//	val = menu->min;
		//	//// see if the macro is there and append the text
		//	//exists = SD.exists("/" + String(val) + ".miw");
		//	//sprintf(line, menu->text, val, exists ? menu->on : menu->off);
		//	sprintf(line, menu->text, val, MacroInfo[val].description.c_str());
		//	// next line
		//	++y;
		//	break;
		case eList:
			bMenuValid[menix] = true;
			val = *(int*)menu->value;
			sprintf(line, menu->text, menu->nameList[val]);
			// next line
			++y;
			break;
		case eBool:
			bMenuValid[menix] = true;
			if (menu->value) {
				bool* pb = (bool*)menu->value;
				sprintf(line, menu->text, *pb ? menu->on : menu->off);
			}
			else {
				strcpy(line, menu->text);
			}
			// increment displayable lines
			++y;
			break;
		//case eBuiltinOptions:
			//// for builtins only show if available
			//if (BuiltInFiles[currentFileIndex.nFileIndex].menu != NULL) {
			//	bMenuValid[menix] = true;
			//	sprintf(line, menu->text, BuiltInFiles[currentFileIndex.nFileIndex].text);
			//	++y;
			//}
			//break;
		case eMenu:
		case eExit:
		case eReboot:
			bMenuValid[menix] = true;
			if (menu->value) {
				// check for %d or %s in string, be lazy and assume %s if %d not there
				if (String(menu->text).indexOf("%d") != -1)
					sprintf(xtraline, menu->text, *(int*)menu->value);
				else
					sprintf(xtraline, menu->text, (char*)menu->value);
			}
			else {
				strcpy(xtraline, menu->text);
			}
			if (menu->op == eExit)
				sprintf(line, "%s%s", "-", xtraline);
			else
				sprintf(line, "%s%s", (menu->op == eReboot) ? "" : "+", xtraline);
			++y;
			//Serial.println("menu text4: " + String(line));
			break;
		default:
			break;
		}
		if (strlen(line) && y >= MenuStack.top()->offset) {
			DisplayMenuLine(y - 1, y - 1 - MenuStack.top()->offset, line);
		}
	}
	MenuStack.top()->menucount = y;
	// blank the rest of the lines
	for (int ix = y; ix < nMenuLineCount; ++ix) {
		DisplayLine(ix, "");
	}
	// show line if menu has been scrolled
	if (MenuStack.top()->offset > 0)
		tft.fillTriangle(0, 0, 2, 0, 0, tft.fontHeight() / 3, TFT_DARKGREY);
	//tft.drawLine(0, 0, 5, 0, menuLineActiveColor);TFT_DARKGREY
// show bottom line if last line is showing
	if (MenuStack.top()->offset + (nMenuLineCount - 1) < MenuStack.top()->menucount - 1) {
		int ypos = tft.height() - 2 - tft.fontHeight() / 3;
		tft.fillTriangle(0, ypos, 2, ypos, 0, ypos - tft.fontHeight() / 3, TFT_DARKGREY);
	}
	//if (MenuStack.top()->offset + (nMenuLineCount - 1) < MenuStack.top()->menucount - 1)
	//	tft.drawLine(0, tft.height() - 1, 5, tft.height() - 1, menuLineActiveColor);
	//else
	//	tft.drawLine(0, tft.height() - 1, 5, tft.height() - 1, TFT_BLACK);
	// see if we need to clean up the end, like when the menu shrank due to a choice
	int extra = MenuStack.top()->menucount - MenuStack.top()->offset - nMenuLineCount;
	while (extra < 0) {
		DisplayLine(nMenuLineCount + extra, "");
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
}

// get integer values
void GetIntegerValue(MenuItem* menu)
{
	if (menu->change != NULL) {
		(*menu->change)(menu, 1);
	}
	// -1 means to reset to original
	int stepSize = 1;
	int originalValue = *(int*)menu->value;
	char line[50];
	CRotaryDialButton::Button button = BTN_NONE;
	bool done = false;
	tft.fillScreen(TFT_BLACK);
	const char* fmt = menu->decimals ? "%ld.%ld" : "%ld";
	char minstr[20], maxstr[20];
	sprintf(minstr, fmt, menu->min / (int)pow10(menu->decimals), menu->min % (int)pow10(menu->decimals));
	sprintf(maxstr, fmt, menu->max / (int)pow10(menu->decimals), menu->max % (int)pow10(menu->decimals));
	DisplayLine(1, String("Range: ") + String(minstr) + " to " + String(maxstr));
	DisplayLine(3, "Long Press to Accept");
	int oldVal = *(int*)menu->value;
	do {
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
		sprintf(line, menu->text, *(int*)menu->value / (int)pow10(menu->decimals), *(int*)menu->value % (int)pow10(menu->decimals));
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
		switch (CRotaryDialButton::dequeue()) {
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

// go up one menu level, return true if anything done
// set gotoMain to go all the way back to the top
bool UpMenuLevel(bool gotoMain)
{
	if (gotoMain) {
		while (UpMenuLevel(false))
			;
	}
	else if (MenuStack.size() > 1) {
		bMenuChanged = true;
		menuPtr = MenuStack.top();
		MenuStack.pop();
		delete menuPtr;
		return true;
	}
	return false;
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
		if (MenuStack.top()->index - MenuStack.top()->offset > (nMenuLineCount - 1)) {
			if (MenuStack.top()->offset < MenuStack.top()->menucount - nMenuLineCount) {
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
			MenuStack.top()->offset = MenuStack.top()->menucount - nMenuLineCount;
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
	}
	return didsomething;
}

// check buttons and return if one pressed
enum CRotaryDialButton::Button ReadButton()
{
	enum CRotaryDialButton::Button retValue = BTN_NONE;
	// read the next button, or NONE if none there
	retValue = CRotaryDialButton::dequeue();
	delay(1);
	return retValue;
}

// the star is used to indicate active menu line
void DisplayMenuLine(int line, int displine, String text)
{
	bool hilite = MenuStack.top()->index == line;
	String mline = (hilite ? "*" : " ") + text;
	if (displine < nMenuLineCount)
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

void SetFactorySettings(MenuItem* menu)
{
	EEPROM.begin(1024);
	byte data[2];
	data[0] = 0;
	data[1] = 0;
	EEPROM.writeBytes(0, data, sizeof(data));
	EEPROM.commit();
	ESP.restart();
}
