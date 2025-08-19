#include <SD.h>
#include "src/Unicode/unicode.h"
#include "icons.h"
#include "USB.h"

// For BLE device scanning
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// https://gitlab.com/DJPX/advanced-keyboard-support-arduino
#include "src/USBHID-Keyboard/USBHIDKeyboard.h"
USBHIDKeyboard Keyboard;
#include "src/Keyboard-Layouts/KeyboardLayout_ES.h"
#include "src/Keyboard-Layouts/KeyboardLayout_DE.h"
#include "src/Keyboard-Layouts/KeyboardLayout_US.h"
#include "src/Keyboard-Layouts/KeyboardLayout_PT.h"
#include "src/Keyboard-Layouts/KeyboardLayout_FR.h"
#include "src/Keyboard-Layouts/KeyboardLayout_SE.h"
#include "src/Keyboard-Layouts/KeyboardLayout_IT.h"
#include "src/Keyboard-Layouts/KeyboardLayout_HU.h"
#include "src/Keyboard-Layouts/KeyboardLayout_DK.h"
#include "src/Keyboard-Layouts/KeyboardLayout_BR.h"
#include "src/Keyboard-Layouts/KeyboardLayout_GB.h"
#include "src/Keyboard-Layouts/KeyboardLayout_NO.h"
#include "src/Keyboard-Layouts/KeyboardLayout_JP.h"
#include "src/Keyboard-Layouts/KeyboardLayout_BE.h"

KeyboardLayout *layout;

int currentKBLayout = 0;

// https://github.com/T-vK/ESP32-BLE-Keyboard
#include "src/BLE-Keyboard/BleKeyboard.h"
BleKeyboard BLEKeyboard("GoodCard :)", "VoidNoi", 100);

// Performance and memory optimization constants
#define OPTIMIZED_MAX_FILES 50          // Reduced from 100 for memory efficiency
#define OPTIMIZED_ELEMENT_COUNT 250     // Reduced from 500 for memory efficiency  
#define OPTIMIZED_MAX_BLE_DEVICES 10    // Reduced from 20 for memory efficiency
#define BLE_SCAN_TIME 3                 // Reduced from 5 for faster scanning
#define BLE_KEY_DELAY 15                // Optimized from 20ms for better performance
#define SCREEN_UPDATE_DELAY 50          // Add delay for smoother UI updates

// Additional variable to control BLE key press timing
int bleKeyDelay = BLE_KEY_DELAY; // milliseconds between key events in BLE mode

// BLE scan settings - optimized for performance
int scanTime = BLE_SCAN_TIME; // seconds to scan for BLE devices
BLEScan* pBLEScan;
String bleDeviceNames[OPTIMIZED_MAX_BLE_DEVICES];
String bleDeviceAddresses[OPTIMIZED_MAX_BLE_DEVICES];
int bleDeviceCount = 0;
bool activeBLEConnection = false; // Flag to track if we're in active connection mode

#include <SPI.h>
#include "M5Cardputer.h"

#define display M5Cardputer.Display
#define kb M5Cardputer.Keyboard


// Optimized file management constants
const int maxFiles = OPTIMIZED_MAX_FILES;

#include "keys.h"

File myFile;
String root = "/BadCard";
String path = root;
String pathArray[maxFiles] = {root};
int pathLen = 0;

int fileAmount;

int mainCursor = 0;
int scriptCursor = 0;
int kbLayoutsCursor = 0;

// Enhanced main menu with settings option
String sdFiles[maxFiles] = {"NEW SCRIPT", "NEW FOLDER", "ACTIVATE BLE", "KB LAYOUT", "SETTINGS"};
int fileType[maxFiles] = {1, 2, 3, 4, 7};

const int scriptOptionsAmount = 3;
const char* scriptMenuOptions[scriptOptionsAmount] = {"Execute script", "Edit script", "Delete script"};

// Optimized keyboard layouts array - using const char* for memory efficiency
const int kbLayoutsLen = 15; // Needs 1 more than the amount of layouts to prevent a visual bug in the menu
const char* kbLayouts[kbLayoutsLen] = {"en_US", "es_ES", "de_DE", "pt_PT", "fr_FR", "sv_SE", "it_IT", "hu_HU", "da_DK", "pt_BR", "en_GB", "nb_NO", "ja_JP", "fr_BE"};

// Optimized memory usage - reduced from 500 to 250 elements
const int ELEMENT_COUNT_MAX = OPTIMIZED_ELEMENT_COUNT;
String fileText[ELEMENT_COUNT_MAX];

int cursorPosX, cursorPosY, screenPosX, screenPosY = 0;

int newFileLines = 0;

int letterHeight = 16;
int letterWidth = 12;

int deletingFolder = false;

String cursor = "|";

// Configuration management system - new feature
struct BadCardConfig {
  int keyboardLayout;
  int bleKeyDelay;
  int scanTime;
  bool showIcons;
  char version[10];
};

BadCardConfig config = {0, BLE_KEY_DELAY, BLE_SCAN_TIME, false, "1.1.0"};

// Save configuration to SD card
void saveConfig() {
  showProgressIndicator("Saving config...", 50);
  File configFile = SD.open("/BadCard/config.dat", FILE_WRITE);
  if (configFile) {
    configFile.write((uint8_t*)&config, sizeof(config));
    configFile.close();
    showProgressIndicator("Config saved!", 100);
  }
  delay(500);
}

// Load configuration from SD card
void loadConfig() {
  File configFile = SD.open("/BadCard/config.dat", FILE_READ);
  if (configFile) {
    if (configFile.size() == sizeof(config)) {
      configFile.read((uint8_t*)&config, sizeof(config));
      currentKBLayout = config.keyboardLayout;
      bleKeyDelay = config.bleKeyDelay;
      scanTime = config.scanTime;
      showIcons = config.showIcons;
    }
    configFile.close();
  }
}

String fileName;

bool creatingFile = false;
bool editingFile = false;
bool isBLE = false;
bool showIcons = false;

void getCurrentPath() {
  path = "";
  for (int i = 0; i <= pathLen; i++) {
    path += pathArray[i];
  }
}

void getDirectory(String directory, int &amount, String* fileArray, int* types) {
  File dir = SD.open(directory);

  String tempFilesArray[maxFiles];
  int tempFilesAmount = 0;
  int tempDirsAmount = amount;

  while (true) {
    File entry =  dir.openNextFile();
    if (!entry) {
      // no more files
      break;
    }

    String fileName = entry.name();
    // Add file to list if not named language.lang
    if (fileName != "language.lang") {
      if (entry.isDirectory()) {
        fileArray[tempDirsAmount] = fileName; // Store directories first
        types[tempDirsAmount] = 6; // Set type directory
        tempDirsAmount++;
      } else {
        tempFilesArray[tempFilesAmount] = fileName; // Store files in a temporary array
        tempFilesAmount++;
      }
      amount++;
    }
    
    if (amount > maxFiles) {
      display.fillScreen(BLACK);
      display.println("Can't store any more scripts");
    }
  }
  int filesCount = 0;
  // Set files after directories and set the type to file
  for(int i = tempDirsAmount; i < amount; i++) {
    if (tempFilesAmount > 0) {
      fileArray[i] = tempFilesArray[filesCount];
      types[i] = 5;
      filesCount++;
    }
  }
}

// Performance optimization: Add progress indicator for long operations
void showProgressIndicator(const char* message, int progress = -1) {
  display.fillRect(50, 100, 220, 40, BLACK);
  display.drawRect(50, 100, 220, 40, PURPLE);
  display.setTextColor(PURPLE);
  display.setCursor(60, 110);
  display.print(message);
  if (progress >= 0) {
    display.setCursor(60, 125);
    display.print("Progress: ");
    display.print(progress);
    display.print("%");
  }
  delay(SCREEN_UPDATE_DELAY);
}

// Optimized menu rendering with reduced redraws
void printMenu(int cursor, String* strings, int stringsAmount, int screenDirection, bool addIcons) {
  int textPosX = addIcons ? 40 : 20;
  for (int i = 0; i <= stringsAmount; i++) {
    int prevString = i-screenDirection;
    
    if (screenDirection != 0) {
      if (addIcons){
        display.setSwapBytes(true);
        display.fillRect(20, i*20, 16, 16, BLACK);
        display.pushImage(20, i*20, 16, 16, (uint16_t *)icons[fileType[i+screenPosY]]);
      }
      display.setTextColor(BLACK);
      display.drawString(strings[prevString+screenPosY], textPosX, i*20);
          
      display.setTextColor(PURPLE);
      display.drawString(strings[i+screenPosY], textPosX, i*20);
    } else {
      if (addIcons){
        display.setSwapBytes(true);
        display.pushImage(20, i*20, 16, 16, (uint16_t *)icons[fileType[i]]);
      }
      display.setTextColor(PURPLE);
      display.drawString(strings[i], textPosX, i*20);
    }
  }
}

void handleMenus(int options, void (*executeFunction)(), int& cursor, String* strings, bool addIcons) {
  display.fillScreen(BLACK);
  cursor = 0;
  int screenDirection; // -1 = down | 0 = none | 1 = up;
  while (true) {
    M5Cardputer.update();
    if (kb.isChange()) {
      if (screenPosY == 0) {
        screenDirection = 0;
      }
      display.setTextColor(BLACK);
      int drawCursor = cursor*20;
        
      display.drawString(">", 5, drawCursor);

      if (kb.isKeyPressed(';') && cursor > 0){
        cursor--;

        if (screenPosY > 0 && cursor > 0) {
          screenPosY--;
          screenDirection = -1;
        }
      } else if (kb.isKeyPressed('.') && cursor < options) {
        cursor++;

        if (cursor * 20 >= display.height() - 20) {
          screenPosY++;
          screenDirection = 1;
        }
      }
      
      drawCursor = cursor*20;
      if (cursor * 20 > display.height()-20) {
        drawCursor = (display.height() - 20) - 15;
      }

      display.setTextColor(PURPLE);

      display.drawString(">", 5, drawCursor);

      printMenu(cursor, strings, options, screenDirection, addIcons);

      if (kb.isKeyPressed(KEY_ENTER)) {
        screenPosY = 0;
        delay(100);

        executeFunction();
        break;
      }
    }
  }
}

// Enhanced script execution with progress tracking and error handling
void executeScript() {
  display.fillScreen(BLACK);
  showProgressIndicator("Preparing script...", 0);
  
  String fileName = path + "/" + sdFiles[mainCursor];

  if (!SD.exists(fileName)) {
    display.fillScreen(BLACK);
    display.setTextColor(WHITE);
    display.setCursor(10, 50);
    display.println("ERROR: Script file not found!");
    display.setTextColor(PURPLE);
    delay(2000);
    return;
  }

  // Initialize keyboard based on connection type
  if (!isBLE) {
    USB.begin();
    Keyboard.begin(layout);
    showProgressIndicator("USB keyboard ready", 25);
  } else {
    BLEKeyboard.setLayout(layout);
    showProgressIndicator("BLE keyboard ready", 25);
  }

  delay(500);
  
  showProgressIndicator("Opening script file...", 50);

  // Open the file for reading
  myFile = SD.open(fileName, FILE_READ);
  
  if (!myFile) {
    display.fillScreen(BLACK);
    display.setTextColor(WHITE);
    display.setCursor(10, 50);
    display.println("ERROR: Cannot open script file!");
    display.setTextColor(PURPLE);
    delay(2000);
    return;
  }

  showProgressIndicator("Executing script...", 75);
  
  // Track execution progress
  int fileSize = myFile.size();
  int bytesProcessed = 0;
  int lineCount = 0;
  
  // Process lines from file with LF EOL (0x0a), not CR+LF (0x0a+0x0d)
  String line = "";
  while (myFile.available()) {
    char c = myFile.read();
    bytesProcessed++;
    
    // Update progress every 100 bytes to avoid too many updates
    if (bytesProcessed % 100 == 0 && fileSize > 0) {
      int progress = 75 + (bytesProcessed * 20) / fileSize;
      showProgressIndicator("Executing...", progress);
    }

    if ((int) c == 0x0a) {         // Line ending (LF) reached
      lineCount++;
      processLine(line);           // Process script line
      line = "";                   // Clean the line to process next
    } else if((int) c != 0x0d) {   // If char isn't a carriage return (CR)
      line += c;                   // Put char into line
    }
  }

  // Process any remaining line without LF
  if (line.length() > 0) {
    processLine(line);
    lineCount++;
  }

  myFile.close();
  
  // End keyboard control
  if (isBLE) {
    BLEKeyboard.end();
  } else {
    Keyboard.end();
  }
  
  showProgressIndicator("Script completed!", 100);
  delay(1000);
  
  // Show execution summary
  display.fillScreen(BLACK);
  display.setCursor(10, 30);
  display.println("Execution Summary:");
  display.setCursor(10, 50);
  display.println("Lines processed: " + String(lineCount));
  display.setCursor(10, 70);
  display.println("Bytes processed: " + String(bytesProcessed));
  display.setCursor(10, 90);
  display.println("Press any key to continue");
  
  while (true) {
    M5Cardputer.update();
    if (kb.isChange() && kb.isPressed()) {
      break;
    }
  }
}

void processLine(String line) {
  /*
   * Process Ducky Script according to the official documentation (see https://github.com/hak5darren/USB-Rubber-Ducky/wiki/Duckyscript).
   *
   * (1) Commands without payload:
   *  - ENTER
   *  - MENU <=> APP
   *  - DOWNARROW <=> DOWN
   *  - LEFTARROW <=> LEFT
   *  - RIGHTARROW <=> RIGHT
   *  - UPARROW <=> UP
   *  - BREAK <=> PAUSE
   *  - CAPSLOCK
   *  - DELETE
   *  - END
   *  - ESC <=> ESCAPE
   *  - HOME
   *  - INSERT
   *  - NUMLOCK
   *  - PAGEUP
   *  - PAGEDOWN
   *  - PRINTSCREEN
   *  - SCROLLLOCK
   *  - SPACE
   *  - TAB
   *  - REPLAY (global commands aren't implemented)
   *
   * (2) Commands with payload:
   *  - DEFAULT_DELAY <=> DEFAULTDELAY (global commands aren't implemented.)
   *  - DELAY (+int)
   *  - STRING (+string)
   *  - GUI <=> WINDOWS (+char)
   *  - SHIFT (+char or key)
   *  - ALT (+char or key)
   *  - CTRL <=> CONTROL (+char or key)
   *  - REM (+string)
   *
   */
  
  display.setCursor(display.width()/2-line.length()/2*letterWidth, display.height()-letterWidth*2);
  display.setTextColor(PURPLE);
  display.println(line);

  int space = line.indexOf(' ');  // Find the first 'space' that'll be used to separate the payload from the command
  String command = "";
  String payload = "";
  
  if (space == -1) {  // There is no space -> (1)
    if (
      line == "ENTER" ||
      line == "MENU" || line == "APP" |
      line == "DOWNARROW" || line == "DOWN" ||
      line == "LEFTARROW" || line == "LEFT" ||
      line == "RIGHTARROW" || line == "RIGHT" ||
      line == "UPARROW" || line == "UP" ||
      line == "BREAK" || line == "PAUSE" ||
      line == "CAPSLOCK" ||
      line == "DELETE" ||
      line == "END" ||
      line == "ESC" || line == "ESCAPE" ||
      line == "HOME" ||
      line == "INSERT" ||
      line == "NUMLOCK" ||
      line == "PAGEUP" ||
      line == "PAGEDOWN" ||
      line == "PRINTSCREEN" ||
      line == "SCROLLLOCK" ||
      line == "SPACE" ||
      line == "TAB"
    ) {
      command = line;
    }
  } else {  // Has a space -> (2)
    command = line.substring(0, space);   // Get chars in line from start to space position
    payload = line.substring(space + 1);  // Get chars in line from after space position to EOL

    if (
      command == "DELAY" ||
      command == "STRING" ||
      command == "GUI" || command == "WINDOWS" ||
      command == "SHIFT" ||
      command == "ALT" ||
      command == "CTRL" || command == "CONTROL" ||
      command == "REM"
    ) { } else {
      // Invalid command
      command = "";
      payload = "";
    }
  }
  
  if (payload == "" && command != "") {                       // Command from (1)
    // Check BLE connection before processing commands
    if (isBLE && !BLEKeyboard.isConnected()) {
      display.fillScreen(BLACK);
      display.setTextColor(WHITE);
      display.setCursor(10, 50);
      display.println("BLE DISCONNECTED!");
      display.setCursor(10, 70);
      display.println("Cannot execute: " + command);
      delay(3000);
      return;
    }
    
    processCommand(command);                                // Process command
    // Add extra delay for BLE to ensure key is registered
    if (isBLE) delay(bleKeyDelay);
  } else if (command == "DELAY") {                            // Delay before the next command
    delay((int) payload.toInt());                           // Convert payload to integer and make pause for 'payload' time
  } else if (command == "STRING") {
    int payloadLen = payload.length();
    unsigned char array[payloadLen];
    for (int i = 0; i < payloadLen; i++) {
      array[i] = payload[i];
    }
    array[payloadLen] = '\0';

    char16_t uString[payloadLen];

    utf8_to_utf16(&array[0], payloadLen, &uString[0], payloadLen);
    uString[payloadLen] = '\0';
    
    // For BLE, type characters one by one with a delay for better reliability
    if (isBLE) {
      // Check if BLE is still connected before sending characters
      if (!BLEKeyboard.isConnected()) {
        display.fillScreen(BLACK);
        display.setTextColor(WHITE);
        display.setCursor(10, 50);
        display.println("BLE DISCONNECTED!");
        display.setCursor(10, 70);
        display.println("Script execution stopped");
        delay(3000);
        return;
      }
      
      for (int i = 0; i < payload.length(); i++) {
        char c = payload[i];
        
        // Verify connection before each character for long strings
        if (!BLEKeyboard.isConnected()) {
          display.fillScreen(BLACK);
          display.setTextColor(WHITE);
          display.setCursor(10, 50);
          display.println("BLE DISCONNECTED!");
          display.setCursor(10, 70);
          display.println("At character: " + String(i + 1) + "/" + String(payload.length()));
          delay(3000);
          return;
        }
        
        BLEKeyboard.write(c);
        delay(bleKeyDelay);  // Add delay between each character for BLE
      }
    } else {
      Keyboard.write(uString);  // Type-in the payload
    }
                       
    // String processing

  } else if (command == "REM") {                              // Comment
  } else if (command != "") {                                 // Command from (2)
    String remaining = line;                                // Prepare commands to run
    while (remaining.length() > 0) {                        // For command in remaining commands
      int space = remaining.indexOf(' ');                 // Find the first 'space' that'll be used to separate commands
      if (space != -1) {                                  // If this isn't the last command
        processCommand(remaining.substring(0, space));  // Process command
        // Add extra delay for BLE when processing multiple commands
        if (isBLE) delay(bleKeyDelay);
        remaining = remaining.substring(space + 1);     // Pop command from remaining commands
      } else {                                            // If this is the last command
        processCommand(remaining);                      // Pop command from remaining commands
        // Add extra delay for BLE at the end of command execution
        if (isBLE) delay(bleKeyDelay);
        remaining = "";                                 // Clear commands (end of loop)
      }
    } 
  }  else {
    // Invalid command
  }

  
  display.setCursor(display.width()/2-line.length()/2*letterWidth, display.height()-letterWidth*2);
  display.setTextColor(BLACK);
  display.println(line);
  
  if (isBLE) {
    // Add a longer delay before releasing all keys in BLE mode to ensure all keys are registered
    delay(bleKeyDelay * 2);
    BLEKeyboard.releaseAll();
  } else {
    Keyboard.releaseAll();
  }
}

void keyboardPress(uint8_t key) {
  if (isBLE) {
    BLEKeyboard.press(key);
    // Add a small delay after pressing a key in BLE mode to ensure it's registered
    delay(bleKeyDelay/2);
  } else {
    Keyboard.press(key);
  }
}

void processCommand(String command) {
  /*
   * Process commands by pressing corresponding key
   * (see https://www.arduino.cc/en/Reference/KeyboardModifiers or
   *      http://www.usb.org/developers/hidpage/Hut1_12v2.pdf#page=53)
   */

  if (command.length() == 1) { 
        // Process key (used for example for WIN L command)
    char c = (char) command[0];  // Convert string (1-char length) to char      
    keyboardPress(c);           // Press the key onkeyboard
  } else if (command == "ENTER") {
    keyboardPress(KEY_RETURN);
  } else if (command == "MENU" || command == "APP") {
    keyboardPress(KEY_MENU);
  } else if (command == "DOWNARROW" || command == "DOWN") {
    keyboardPress(KEY_DOWN_ARROW);
  } else if (command == "LEFTARROW" || command == "LEFT") {
    keyboardPress(KEY_LEFT_ARROW);
  } else if (command == "RIGHTARROW" || command == "RIGHT") {
    keyboardPress(KEY_RIGHT_ARROW);
  } else if (command == "UPARROW" || command == "UP") {
    keyboardPress(KEY_UP_ARROW);
  } else if (command == "BREAK" || command == "PAUSE") {
    keyboardPress(KEY_PAUSE);
  } else if (command == "CAPSLOCK") {
    keyboardPress(KEY_CAPS_LOCK);
  } else if (command == "DELETE" || command == "DEL") {
    keyboardPress(KEY_DELETE);
  } else if (command == "END") {
    keyboardPress(KEY_END);
  } else if (command == "ESC" || command == "ESCAPE") {
    keyboardPress(KEY_ESC);
  } else if (command == "HOME") {
    keyboardPress(KEY_HOME);
  } else if (command == "INSERT") {
    keyboardPress(KEY_INSERT);
  } else if (command == "NUMLOCK") {
    keyboardPress(KEY_NUMLOCK);
  } else if (command == "PAGEUP") {
    keyboardPress(KEY_PAGE_UP);
  } else if (command == "PAGEDOWN") {
    keyboardPress(KEY_PAGE_DOWN);
  } else if (command == "PRINTSCREEN") {
    keyboardPress(KEY_PRINTSCREEN);
  } else if (command == "SCROLLLOCK") {
    keyboardPress(KEY_SCROLLLOCK);
  } else if (command == "SPACE") {
    keyboardPress(KEY_SPACE);
  } else if (command == "BACKSPACE") {
    keyboardPress(KEY_BACKSPACE);
  } else if (command == "TAB") {
    keyboardPress(KEY_TAB);
  } else if (command == "GUI" || command == "WINDOWS") {
    keyboardPress(KEY_LEFT_GUI);
  } else if (command == "SHIFT") {
    keyboardPress(KEY_RIGHT_SHIFT);
  } else if (command == "ALT") {
    keyboardPress(KEY_LEFT_ALT);
  } else if (command == "CTRL" || command == "CONTROL") {
    keyboardPress(KEY_LEFT_CTRL);
  } else if (command == "F1" || command == "FUNCTION1") {
    keyboardPress(KEY_F1);
  } else if (command == "F2" || command == "FUNCTION2") {
    keyboardPress(KEY_F2);
  } else if (command == "F3" || command == "FUNCTION3") {
    keyboardPress(KEY_F3);
  } else if (command == "F4" || command == "FUNCTION4") {
    keyboardPress(KEY_F4);
  } else if (command == "F5" || command == "FUNCTION5") {
    keyboardPress(KEY_F5);
  } else if (command == "F6" || command == "FUNCTION6") {
    keyboardPress(KEY_F6);
  } else if (command == "F7" || command == "FUNCTION7") {
    keyboardPress(KEY_F7);
  } else if (command == "F8" || command == "FUNCTION8") {
    keyboardPress(KEY_F8);
  } else if (command == "F9" || command == "FUNCTION9") {
    keyboardPress(KEY_F9);
  } else if (command == "F10" || command == "FUNCTION10") {
    keyboardPress(KEY_F10);
  } else if (command == "F11" || command == "FUNCTION11") {
    keyboardPress(KEY_F11);
  } else if (command == "F12" || command == "FUNCTION12") {
    keyboardPress(KEY_F12);
  }
}

void insertLine(String file[], int y){
  for (int i = newFileLines; i > y; i--){
    file[i] = file[i-1];
  }
  file[y] = '\0';
}

void insertChar(String file[], int y, int x, char letter){
  if (cursorPosX >= file[y].length()) {
    file[y] += letter;
  } else {
    String modLine = file[y].substring(0,x-1) + letter + file[y].substring(x-1,file[y].length());
    file[y] = modLine;
  }
}

void removeLine(String file[], int y){
  for (int i = y; i < newFileLines; i++) {
    file[i] = file[i+1];
  }
  file[newFileLines] = '\0';
  cursorPosY--;
  newFileLines--;
}

void cleanNewFile() {
  cleanArray<String*>(fileText, newFileLines);
  cursorPosX = 0;
  cursorPosY = 0;
  screenPosX = 0;
  screenPosY = 0;
}

// Enhanced file saving with error handling and progress indication
void saveFileChanges() {
  showProgressIndicator("Saving file...", 0);
  
  String fullPath = path + "/" + fileName + ".txt";
  myFile = SD.open(fullPath, FILE_WRITE);
  
  if (myFile) {
    showProgressIndicator("Writing data...", 25);
    
    for (int i = 0; i <= newFileLines; i++) {
      int textLen = fileText[i].length();
      if (textLen > 0) {
        char charText[textLen + 1]; // +1 for null terminator
        strcpy(charText, fileText[i].c_str());

        unsigned char unChar[textLen];
        for (int x = 0; x < textLen; x++) {
          unChar[x] = static_cast<unsigned char>(charText[x]);
        }

        myFile.write(unChar, textLen);
      }
      myFile.print('\n');
      
      // Update progress
      if (newFileLines > 0) {
        int progress = 25 + (i * 50) / newFileLines;
        if (i % 10 == 0) { // Update every 10 lines to avoid too many updates
          showProgressIndicator("Writing data...", progress);
        }
      }
    }

    myFile.close();
    showProgressIndicator("File saved successfully!", 100);
    delay(1000);
    return;
  } else {
    display.fillScreen(BLACK);
    display.setTextColor(WHITE);
    display.setCursor(10, 50);
    display.println("ERROR: Could not save file!");
    display.setCursor(10, 70);
    display.println("Check SD card and try again");
    display.setTextColor(PURPLE);
    delay(2000);
    return;
  }
}


void editFile() {
  // Open the file for reading
  myFile = SD.open(path + "/" + fileName, FILE_READ);
  if (myFile) {
    // Load the file into the fileText array
    newFileLines = 0;
    String line = "";
    while (myFile.available()) {
      char c = myFile.read();
      if ((int)c == 0x0a) {
        fileText[newFileLines] = line;
        newFileLines++;
        line = "";
      }
      else if ((int)c != 0x0d) {
        line += c;
      }
    }
    myFile.close();

    // Display the file content:
    display.fillScreen(BLACK);
    display.setTextSize(2);
    for (int i = 0; i <= newFileLines; i++) {
      display.drawString(fileText[i], 0, i * letterHeight);
    }
  }
  else {
    display.println("Error occurred while opening file");
    return;
  }
  display.setCursor(1, 1);
  editingFile = true;
  fileWrite();
}

void newFile() {
  fileName = '\0';
  cleanNewFile();
  display.fillScreen(BLACK);
  display.setCursor(1,1);
  creatingFile = true;
  fileWrite();
}

void deleteScript() {
  String fileName = path + "/" + sdFiles[mainCursor];
  display.fillScreen(BLACK);
  display.setCursor(1,1);
  display.println("Deleting script...");
  if (SD.remove(fileName)) {
    display.println("Script deleted");
    sdFiles[fileAmount] = '\0';
    fileAmount--;
    delay(1000);
  } else {
    display.println("Script couldn't be deleted");
    delay(1000);
  }
  return;
}

void scriptOptions() {
  if (scriptCursor == 0) {
    executeScript();
  }
  else if (scriptCursor == 1) {
    fileName = sdFiles[mainCursor];
    editFile();
  }
  else if (scriptCursor == 2) {
    deleteScript();
  }
}

void setKBLayout(int layoutNum) {
  switch (layoutNum) {
    case 0:
      layout = new KeyboardLayout_US();
      break;
    case 1:
      layout = new KeyboardLayout_ES();
      break;
    case 2:
      layout = new KeyboardLayout_DE();
      break;
    case 3:
      layout = new KeyboardLayout_PT();
      break;
    case 4:
      layout = new KeyboardLayout_FR();
      break;
    case 5:
      layout = new KeyboardLayout_SE();
      break;
    case 6:
      layout = new KeyboardLayout_IT();
      break;
    case 7:
      layout = new KeyboardLayout_HU();
      break;
    case 8:
      layout = new KeyboardLayout_DK();
      break;
    case 9:
      layout = new KeyboardLayout_BR();
      break;
    case 10:
      layout = new KeyboardLayout_GB();
      break;
    case 11:
      layout = new KeyboardLayout_NO();
      break;
    case 12:
      layout = new KeyboardLayout_JP();
      break;
    case 13:
      layout = new KeyboardLayout_BE();
      break;
  }
}

// Enhanced keyboard layout switching with config persistence
void kbLayoutsOptions() {
  currentKBLayout = kbLayoutsCursor;
  setKBLayout(currentKBLayout);
  
  // Save layout preference to config
  config.keyboardLayout = currentKBLayout;
  saveConfig();
  
  setLang(currentKBLayout);

  return;
}

template <typename T> void cleanArray(T array, int length) {
  for (int i = 0; i <= length; i++) {
    array[i] = '\0';
  }
	length = 0;
}

void handleFolders() {
  display.setTextSize(2);
  while(true) {
    if (pathArray[pathLen] != root) {
      fileAmount = 3;
      if (mainCursor > 2 && fileType[scriptCursor] < 8 && !deletingFolder) {
        pathArray[pathLen] = "/" + sdFiles[mainCursor];
      }
      getCurrentPath();
      sdFiles[0] = "..";
      sdFiles[1] = "NEW SCRIPT";
      sdFiles[2] = "NEW FOLDER";
      fileType[0] = 7;
      fileType[1] = 1;
      fileType[2] = 2;
    } else {
      fileAmount = 6; // Increased from 5 to 6 for the new Settings option
      path = root;
      pathLen = 0;
      
      sdFiles[0] = "NEW SCRIPT";
      sdFiles[1] = "NEW FOLDER";
      sdFiles[2] = isBLE ? "BLE ACTIVATED" : "BLE PASSIVE MODE";
      sdFiles[3] = "BLE SCAN & CONNECT";
      sdFiles[4] = "KB LAYOUT";
      sdFiles[5] = "SETTINGS";
      
      fileType[0] = 1;
      fileType[1] = 2;
      fileType[2] = 3;
      fileType[3] = 8; // New file type for BLE scan and connect
      fileType[4] = 4;
      fileType[5] = 7; // New file type for Settings
    }
    
    getDirectory(path, fileAmount, sdFiles, fileType);
    // Empties an extra value at the end to prevent previous files from appearing in the menu
    sdFiles[fileAmount] = '\0';
    fileType[fileAmount] = '\0';
    
    mainCursor = 0;
    deletingFolder = false;
    handleMenus(fileAmount-1, mainOptions, mainCursor, sdFiles, showIcons);
  }
}

void newFolder() {
  fileName = '\0';

  display.setCursor(1,1);
  display.fillScreen(BLACK);

  display.setCursor(display.width()/2-(12/2)*letterWidth, 0);
  display.println("Folder Name:");
  display.drawString(fileName, display.width()/2-(fileName.length()/2)*letterWidth, letterHeight);
  saveFile();
}

void makeFolder() {
  getCurrentPath();
  SD.mkdir(path + "/" + fileName);
  return;
}

void deleteFolderMenu() {
  int deleteCursor = 1;
  int deleteCursorPosX = display.width()/2+2*letterWidth/2+letterWidth;
  //Displays the delete folder menu
  display.fillScreen(BLACK); 
  display.setCursor(display.width()/2-15*letterWidth/2, display.height()/2-letterHeight*3);
  display.println("Deleting folder");
  display.setCursor(display.width()/2-12*letterWidth/2, display.height()/2-letterHeight/2*3);
  display.println("Are you sure?");
  display.setCursor(display.width()/2-10*letterWidth/2, display.height()/2);
  display.println("Yes     No");
  // Handles menu controls
  while (true) {
    M5Cardputer.update();
    if (kb.isChange()) {
      display.setTextColor(BLACK);
      display.drawString(">", deleteCursorPosX, display.height()/2);
      // "," is pressed, select yes
      if (kb.isKeyPressed(',')) {
        deleteCursor = 0;
        
        deleteCursorPosX = display.width()/2-10*letterWidth/2-letterWidth;
      }
      // "/" is pressed, select no
      if (kb.isKeyPressed('/')) {
        deleteCursor = 1;

        deleteCursorPosX = display.width()/2+2*letterWidth/2+letterWidth;
      }
      
      display.setTextColor(PURPLE);
      display.drawString(">", deleteCursorPosX, display.height()/2);

      // Cursor is on yes ("0"), attempt to delete folder
      if (kb.isKeyPressed(KEY_ENTER)) {
        if (deleteCursor == 0) {
          String folderPath = path + "/" + sdFiles[mainCursor];
          File dir = SD.open(folderPath);
          // The folders have files inside, try to delete them
          if (!SD.rmdir(folderPath)){
            while (true) {
              File folderFile =  dir.openNextFile();
              if (!folderFile) { // No more files
                if (SD.rmdir(folderPath)) { // All files have been removed, delete folder
                  display.fillScreen(BLACK);
                  display.setCursor(1,1);
                  display.println("Folder successfully deleted");
                } else { // The folder couldn't be removed for some unknown reason
                  display.fillScreen(BLACK);
                  display.setCursor(1,1);
                  display.println("Folder couldn't be deleted");
                }

                mainCursor = 0;
                break;
              }
              
              String folderFileName = folderFile.name();
              // If the file is a directory stop the function and let the user remove it manually
              // This is done because I haven't implemented recursive file/folder deletion
              if (folderFile.isDirectory()) {
                display.fillScreen(BLACK);
                display.setCursor(1,1);
                display.println("Remove folders first");
                mainCursor = 0;
                break;
              } else { // The file couldn't be removed for some unknown reason
                if (!SD.remove(folderPath + "/" + folderFileName)) {
                  display.fillScreen(BLACK);
                  display.setCursor(1,1);
                  display.println("Couldn't remove a file");
                }
              }
            }
          } else { // If the folder doesn't have any files inside, delete it
            display.fillScreen(BLACK);
            display.setCursor(1,1);
            display.println("Folder successfully deleted");
          }
          dir.close();
          delay(1500);
        }
        // Return to current folder
        return;
      }
    }
  }
}

void mainOptions() {
  switch (fileType[mainCursor]) {
  case 1: // New File
    newFile();
    break;
  case 2: // New Folder
    newFolder();
    break;
  case 3: // BLE toggle
    display.fillScreen(BLACK);
    if (!isBLE) {
      isBLE = true;
      // Set the security mode to low (no authorization required) for headless servers
      BLEKeyboard.setSecurityMode(BLE_SECURITY_LOW);
      BLEKeyboard.begin();
      sdFiles[2] = "BLE ACTIVATED";
      display.setTextSize(1);
      display.setCursor(5, 5);
      display.println("BLE activated in headless mode");
      display.println("No authorization required");
      delay(1500);
    } else {
      isBLE = false;
      BLEKeyboard.end();
      sdFiles[2] = "ACTIVATE BLE";
      display.setTextSize(1);
      display.setCursor(5, 5);
      display.println("BLE deactivated");
      delay(1000);
    }
    break;
  case 4: // Keyboard Layouts
    handleMenus(kbLayoutsLen-2, kbLayoutsOptions, kbLayoutsCursor, kbLayouts, false); // We remove 1 more than the others from kbLayoutsLen to compensate for the extra 1 added at declaration
    break;
  case 7: // Settings - new feature
    settingsMenu();
    break;
  case 5: // File
    fileType[0] = 8;
    fileType[1] = 9;
    fileType[2] = 10;
    handleMenus(scriptOptionsAmount-1, scriptOptions, scriptCursor, scriptMenuOptions, showIcons);
    break;
  case 6: // Folder
    // If G0 button is pressed show delete folder menu
    if (digitalRead(0)==0) {
      deletingFolder = true;
      deleteFolderMenu();
    } else {
      pathLen++;
    }
    break;
  case 7: // Previous folder
    sdFiles[pathLen] = '\0';
    pathLen--;
    break;
  case 8: // BLE Scan & Connect
    showBLEDeviceList();
    break;
  }
  return;
}

void bootLogo(){
  
  M5Canvas backgroundSprite(&display);
  M5Canvas versionSprite(&display);
  M5Canvas logoSprite(&display);
  M5Canvas fwNameSprite(&display);
  M5Canvas pressKeySprite(&display);

  unsigned long t;

  t = millis();

  char version[] = "v1.7.2";
  int versionLength = sizeof(version)-1;
  int versionWidth = versionLength*letterWidth/2+3;
  int versionHeight = letterHeight/2+2;

  char fwName[] = "BadCard";
  int fwNameLength = sizeof(fwName)-1;
  double fwNamePosX = fwNameLength % 2 == 1 ? (display.width()/2-letterWidth/2)-(fwNameLength/2)*letterWidth : display.width()/2-(fwNameLength/2)*letterWidth;
  //int fwNamePosY = display.height()/2 - 50;
  int fwNamePosY = letterHeight/2;
  
  char pressKeyText[] = "Press any key...";
  int pressKeyTextLength = sizeof(pressKeyText)-1;
  int pressKeyPosX = display.width()/2 - 95;
  //int pressKeyPosY = display.height()/2 + 40;
  int pressKeyPosY = display.height() - letterHeight-4;
  
  int offset = 8;
  
  backgroundSprite.createSprite(display.width()+letterWidth*2, display.height());
  versionSprite.createSprite(versionWidth, versionHeight);
  logoSprite.createSprite(display.width()/2 + 70, display.height()/2-10);
  fwNameSprite.createSprite(fwNameLength*letterWidth+10, letterHeight+10);
  pressKeySprite.createSprite(pressKeyTextLength*letterWidth+10, letterHeight+10);

  int move = 0;
  int mosaicSpriteSize = letterWidth*2.5; // Change the number to adjust the distance between the mosaic's sprites
    
  while(true) {
    //Background animation loop
    backgroundSprite.setTextSize(2);
    backgroundSprite.fillSprite(BLACK);
    for(int x = 0; x < 15; x++) {
      for (int y = 0; y < 15; y++) {
        //Use a character
        //backgroundSprite.setCursor(x*letterWidth*1.5-move, y*letterWidth*1.5-move);
        //backgroundSprite.setTextColor(0x5c0a5cU);
        //backgroundSprite.println("+");

        //Or use an image
        backgroundSprite.setSwapBytes(true);
        backgroundSprite.pushImage(x*mosaicSpriteSize-move, y*mosaicSpriteSize-move, 16, 16, (uint16_t *)icons[8]);
      }
    }
    for (int x = 0; x <= display.width()+letterWidth; x++) {
      for (int y = 0; y <= display.height(); y++) {
        if (x % 2 == 0 || y % 2 == 0) {
          backgroundSprite.drawPixel(x, y, BLACK);
        }
      }
    }

    if (move >= mosaicSpriteSize) {
      move = 0;
    }
    //Slow down animation
    if (millis() -t >= 80) {
      move++;
      t = millis();
    }

    //Prints logo
    logoSprite.setTextSize(1);
    logoSprite.setTextColor(PURPLE);
    /* logoSprite.drawLine(1, 1, 1, display.height()/2-12, PURPLE); */
    /* logoSprite.drawLine(1, display.height()/2-12, display.width()/2+60+offset, display.height()/2-12, PURPLE); */
    /* logoSprite.drawLine(display.width()/2+60 + offset, display.height()/2-12, display.width()/2+68, 1, PURPLE); */
    /* logoSprite.drawLine(display.width()/2+60 + offset, 1, 1, 1, PURPLE); */
    logoSprite.setCursor(offset, 2);
    logoSprite.println(" __   __     ______     __    ");
    logoSprite.setCursor(offset, 12);
    logoSprite.println("/\\\ \"-.\\ \\   /\\  __ \\   /\\ \\   ");
    logoSprite.setCursor(offset, 22);
    logoSprite.println("\\ \\ \\-.  \\  \\ \\ \\/\\ \\  \\ \\ \\  ");
    logoSprite.setCursor(offset, 32);
    logoSprite.println(" \\ \\_\\\\\"\\_\\  \\ \\_____\\  \\ \\_\\ ");
    logoSprite.setCursor(offset, 42);
    logoSprite.println("  \\/_/ \\/_/   \\/_____/   \\/_/ ");
    
    //Prints version
    versionSprite.setTextSize(1);
    versionSprite.setTextColor(PURPLE);
    versionSprite.setCursor(0, 0);
    versionSprite.drawLine(0, versionHeight-2, versionWidth-2, versionHeight-2, PURPLE);
    versionSprite.drawLine(versionWidth-2, versionHeight-2, versionWidth-2, 0, PURPLE);
    versionSprite.println(version);
    
    //Prints firmware name
    fwNameSprite.setTextSize(2);
    fwNameSprite.setTextColor(PURPLE);
    fwNameSprite.setCursor(offset-2, offset-2);
    /* fwNameSprite.drawLine(1, 1, 1, letterHeight+offset, PURPLE); */
    fwNameSprite.drawLine(1, letterHeight+offset, fwNameLength*letterWidth+offset, letterHeight+offset, PURPLE);
    /* fwNameSprite.drawLine(fwNameLength*letterWidth+offset, letterHeight+offset, fwNameLength*letterWidth+offset, 1, PURPLE); */
    /* fwNameSprite.drawLine(fwNameLength*letterWidth+offset, 1, 1, 1, PURPLE); */
    fwNameSprite.println(fwName);
    
    //Prints press any key
    pressKeySprite.setTextSize(2);
    pressKeySprite.setTextColor(PURPLE);
    pressKeySprite.setCursor(offset-2, offset-2);
    pressKeySprite.drawLine(1, 1, 1, letterHeight+offset, PURPLE);
    pressKeySprite.drawLine(1, letterHeight+offset, pressKeyTextLength*letterWidth+offset, letterHeight+offset, PURPLE);
    pressKeySprite.drawLine(pressKeyTextLength*letterWidth+offset, letterHeight+offset, pressKeyTextLength*letterWidth+offset, 1, PURPLE);
    pressKeySprite.drawLine(pressKeyTextLength*letterWidth+offset, 1, 1, 1, PURPLE);
    pressKeySprite.println(pressKeyText);
    
    // Pushes the sprites onto the canvas
    versionSprite.pushSprite(&backgroundSprite, letterWidth, 0);
    logoSprite.pushSprite(&backgroundSprite, display.width()/2 - 95 + letterWidth, display.height()/2 - 30);
    fwNameSprite.pushSprite(&backgroundSprite, fwNamePosX + letterWidth-5, fwNamePosY-10);
    pressKeySprite.pushSprite(&backgroundSprite, pressKeyPosX + letterWidth-5, pressKeyPosY-5);
    backgroundSprite.pushSprite(-letterWidth, 0);
    
    M5Cardputer.update();
    
    // If any key is pressed go to main menu
    if (kb.isChange()) {
      delay(100);
      return;
    }
  }
}

void setLang(int lang) {
  File langFile = SD.open(root + "/" + "language" + ".lang", FILE_WRITE);
  langFile.print(lang);
  langFile.close();
}

void getLang() {
  String langPath = root + "/" + "language" + ".lang";

  if (SD.exists(langPath)) {
    File langFile = SD.open(langPath, FILE_READ);
    if (langFile) {
      String line;
      while (langFile.available()) {
        char c = langFile.read();

        if ((int)c != 0x0d) {
          line += c;
        }
      }
      langFile.close();
      int langNum = line.toInt();
      currentKBLayout = langNum;
      setKBLayout(currentKBLayout);
    }
  } else {
    File langFile = SD.open(langPath, FILE_WRITE);
    langFile.print("0");
    langFile.close();
    layout = new KeyboardLayout_US();
  }
}

// Add this class to create a callback for BLE scanning
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (bleDeviceCount < OPTIMIZED_MAX_BLE_DEVICES) {
      // Only store devices with names
      if (advertisedDevice.haveName()) {
        String deviceName = advertisedDevice.getName().c_str();
        String deviceAddress = advertisedDevice.getAddress().toString().c_str();
        
        // Check if device is already in the list
        bool deviceExists = false;
        for (int i = 0; i < bleDeviceCount; i++) {
          if (bleDeviceAddresses[i] == deviceAddress) {
            deviceExists = true;
            break;
          }
        }
        
        if (!deviceExists) {
          bleDeviceNames[bleDeviceCount] = deviceName;
          bleDeviceAddresses[bleDeviceCount] = deviceAddress;
          bleDeviceCount++;
        }
      }
    }
  }
};

// Function to scan for available BLE devices - optimized with progress indicator
void scanForBLEDevices() {
  display.fillScreen(BLACK);
  showProgressIndicator("Scanning BLE devices...", 0);
  
  // Clear previous results
  bleDeviceCount = 0;
  for (int i = 0; i < OPTIMIZED_MAX_BLE_DEVICES; i++) {
    bleDeviceNames[i] = "";
    bleDeviceAddresses[i] = "";
  }
  
  BLEDevice::init("BadCard Scanner");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
  
  showProgressIndicator("Scanning BLE devices...", 50);
  
  // Start scan with optimized time
  pBLEScan->start(scanTime, false);
  
  showProgressIndicator("Scan complete!", 100);
  delay(500); // Brief pause to show completion
  
  display.fillScreen(BLACK);
  if (bleDeviceCount > 0) {
    display.setCursor(10, 10);
    display.println("Found " + String(bleDeviceCount) + " devices");
    display.println("Select a device to connect:");
  } else {
    display.setCursor(10, 10);
    display.println("No devices found.");
    display.println("Press any key to return.");
    while (true) {
      M5Cardputer.update();
      if (kb.isChange() && kb.isPressed()) {
        return;
      }
    }
  }
}

// Show the list of BLE devices and let the user select one
void showBLEDeviceList() {
  scanForBLEDevices();
  
  if (bleDeviceCount == 0) return;
  
  int deviceCursor = 0;
  int screenDirection = 0;
  int screenPosY = 0;
  
  display.fillScreen(BLACK);
  for (int i = 0; i < bleDeviceCount; i++) {
    display.setCursor(30, i * 20 + 10);
    display.println(bleDeviceNames[i]);
  }
  
  while (true) {
    M5Cardputer.update();
    if (kb.isChange()) {
      display.setTextColor(BLACK);
      int drawCursor = deviceCursor * 20 + 10;
      
      display.drawString(">", 10, drawCursor);

      if (kb.isKeyPressed(';') && deviceCursor > 0) {
        deviceCursor--;
        
        if (screenPosY > 0 && deviceCursor > 0) {
          screenPosY--;
          screenDirection = -1;
        }
      } else if (kb.isKeyPressed('.') && deviceCursor < bleDeviceCount - 1) {
        deviceCursor++;
        
        if (deviceCursor * 20 >= display.height() - 20) {
          screenPosY++;
          screenDirection = 1;
        }
      }
      
      drawCursor = deviceCursor * 20 + 10;
      if (deviceCursor * 20 > display.height() - 20) {
        drawCursor = (display.height() - 20) - 15 + 10;
      }

      display.setTextColor(PURPLE);
      display.drawString(">", 10, drawCursor);
      
      // Redraw device list with scrolling if needed
      if (screenDirection != 0) {
        display.fillRect(30, 0, display.width() - 30, display.height(), BLACK);
        for (int i = 0; i < bleDeviceCount - screenPosY && i < display.height() / 20; i++) {
          display.setCursor(30, i * 20 + 10);
          display.println(bleDeviceNames[i + screenPosY]);
        }
        screenDirection = 0;
      }
      
      // Enter key connects to selected device
      if (kb.isKeyPressed(KEY_ENTER)) {
        connectToBLEDevice(bleDeviceAddresses[deviceCursor]);
        break;
      }
      
      // ESC key cancels and returns to menu
      if (kb.isKeyPressed(KEY_ESC)) {
        break;
      }
    }
  }
}

// Enhanced BLE connection with improved feedback and status checking
void connectToBLEDevice(String deviceAddress) {
  showProgressIndicator("Initializing BLE...", 0);
  
  // Enable comprehensive error logging for troubleshooting
  BLEKeyboard.enableErrorLogging(true);
  
  // Initialize BLE with bypass authentication settings
  BLEKeyboard.setSecurityMode(BLE_SECURITY_LOW);
  
  showProgressIndicator("Starting BLE service...", 25);
  BLEKeyboard.begin();
  
  // Check if BLE initialization was successful
  delay(1000); // Give BLE time to initialize
  
  showProgressIndicator("Connecting to device...", 50);
  
  // Wait for connection with timeout and better error reporting
  int connectionAttempts = 0;
  const int maxAttempts = 15;  // Increased attempts for better reliability
  
  while (!BLEKeyboard.isConnected() && connectionAttempts < maxAttempts) {
    delay(500);
    connectionAttempts++;
    
    // Provide more detailed progress feedback
    int progress = 50 + (connectionAttempts * 3);
    String statusMsg = "Connecting... (" + String(connectionAttempts) + "/" + String(maxAttempts) + ")";
    showProgressIndicator(statusMsg.c_str(), progress);
  }
  
  if (BLEKeyboard.isConnected()) {
    // Connection successful
    isBLE = true;
    activeBLEConnection = true;
    sdFiles[2] = "BLE ACTIVATED";
    
    showProgressIndicator("Connected successfully!", 100);
    delay(500);
    
    display.fillScreen(BLACK);
    display.setTextColor(WHITE);
    display.setCursor(10, 30);
    display.println("BLE CONNECTION ESTABLISHED");
    display.setTextColor(PURPLE);
    display.setCursor(10, 50);
    display.println("Device: " + deviceAddress);
    display.setCursor(10, 70);
    display.println("Mode: Headless (No Auth)");
    display.setCursor(10, 90);
    display.println("Ready to execute scripts!");
    delay(2000);
  } else {
    // Connection failed - provide detailed error information
    display.fillScreen(BLACK);
    display.setTextColor(WHITE);
    display.setCursor(10, 10);
    display.println("BLE CONNECTION FAILED");
    display.setTextColor(PURPLE);
    display.setCursor(10, 30);
    display.println("Target: " + deviceAddress);
    display.setCursor(10, 50);
    display.println("Attempts: " + String(connectionAttempts) + "/" + String(maxAttempts));
    display.setCursor(10, 70);
    display.println("Possible causes:");
    display.setCursor(10, 90);
    display.println("- Device out of range");
    display.setCursor(10, 110);
    display.println("- Device not pairable");
    display.setCursor(10, 130);
    display.println("- BLE initialization failed");
    
    // Clean up BLE resources on failure
    BLEKeyboard.end();
    
    display.setTextColor(WHITE);
    display.setCursor(10, 160);
    display.println("Press any key to retry");
    
    while (true) {
      M5Cardputer.update();
      if (kb.isChange() && kb.isPressed()) {
        break;
      }
    }
  }
}

// New feature: Settings menu for advanced configuration
const int settingsOptionsAmount = 4;
const char* settingsMenuOptions[settingsOptionsAmount] = {"BLE Key Delay", "Scan Time", "Show Icons", "Reset Config"};
int settingsCursor = 0;

void settingsMenu() {
  handleMenus(settingsOptionsAmount - 1, settingsOptions, settingsCursor, (String*)settingsMenuOptions, false);
}

void settingsOptions() {
  switch(settingsCursor) {
    case 0: // BLE Key Delay
      adjustBLEKeyDelay();
      break;
    case 1: // Scan Time
      adjustScanTime();
      break;
    case 2: // Show Icons
      toggleShowIcons();
      break;
    case 3: // Reset Config
      resetConfig();
      break;
  }
}

void adjustBLEKeyDelay() {
  display.fillScreen(BLACK);
  display.setCursor(10, 10);
  display.println("BLE Key Delay: " + String(bleKeyDelay) + "ms");
  display.println("Use +/- to adjust, Enter to save");
  
  while (true) {
    M5Cardputer.update();
    if (kb.isChange()) {
      if (kb.isKeyPressed('+') || kb.isKeyPressed('=')) {
        if (bleKeyDelay < 100) bleKeyDelay += 5;
        display.fillRect(150, 10, 50, 20, BLACK);
        display.setCursor(150, 10);
        display.print(String(bleKeyDelay) + "ms");
      } else if (kb.isKeyPressed('-')) {
        if (bleKeyDelay > 5) bleKeyDelay -= 5;
        display.fillRect(150, 10, 50, 20, BLACK);
        display.setCursor(150, 10);
        display.print(String(bleKeyDelay) + "ms");
      } else if (kb.isKeyPressed(KEY_ENTER)) {
        config.bleKeyDelay = bleKeyDelay;
        BLEKeyboard.setDelay(bleKeyDelay);
        saveConfig();
        break;
      }
    }
  }
}

void adjustScanTime() {
  display.fillScreen(BLACK);
  display.setCursor(10, 10);
  display.println("Scan Time: " + String(scanTime) + "s");
  display.println("Use +/- to adjust, Enter to save");
  
  while (true) {
    M5Cardputer.update();
    if (kb.isChange()) {
      if (kb.isKeyPressed('+') || kb.isKeyPressed('=')) {
        if (scanTime < 10) scanTime++;
        display.fillRect(120, 10, 50, 20, BLACK);
        display.setCursor(120, 10);
        display.print(String(scanTime) + "s");
      } else if (kb.isKeyPressed('-')) {
        if (scanTime > 1) scanTime--;
        display.fillRect(120, 10, 50, 20, BLACK);
        display.setCursor(120, 10);
        display.print(String(scanTime) + "s");
      } else if (kb.isKeyPressed(KEY_ENTER)) {
        config.scanTime = scanTime;
        saveConfig();
        break;
      }
    }
  }
}

void toggleShowIcons() {
  showIcons = !showIcons;
  config.showIcons = showIcons;
  saveConfig();
  
  display.fillScreen(BLACK);
  display.setCursor(10, 50);
  display.println("Icons " + String(showIcons ? "enabled" : "disabled"));
  delay(1000);
}

void resetConfig() {
  display.fillScreen(BLACK);
  display.setCursor(10, 50);
  display.println("Reset config? Press Y to confirm");
  
  while (true) {
    M5Cardputer.update();
    if (kb.isChange()) {
      if (kb.isKeyPressed('y') || kb.isKeyPressed('Y')) {
        config = {0, BLE_KEY_DELAY, BLE_SCAN_TIME, false, "1.1.0"};
        currentKBLayout = 0;
        bleKeyDelay = BLE_KEY_DELAY;
        scanTime = BLE_SCAN_TIME;
        showIcons = false;
        saveConfig();
        display.setCursor(10, 70);
        display.println("Config reset successfully!");
        delay(1500);
        break;
      } else if (kb.isPressed()) {
        break;
      }
    }
  }
}

// Enhanced setup with configuration loading and better initialization
void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);

  SPI.begin(
    M5.getPin(m5::pin_name_t::sd_spi_sclk),
    M5.getPin(m5::pin_name_t::sd_spi_miso),
    M5.getPin(m5::pin_name_t::sd_spi_mosi),
    M5.getPin(m5::pin_name_t::sd_spi_ss));
    
  while (false == SD.begin(M5.getPin(m5::pin_name_t::sd_spi_ss), SPI)) {
    delay(1);
  }
  
  // Ensure BadCard directory exists
  if (!SD.exists(root)) {
    SD.mkdir(root);
  }
  
  display.setRotation(1);
  display.setTextColor(PURPLE);
  
  // Load user configuration
  loadConfig();
  
  // Apply loaded configuration
  BLEKeyboard.setDelay(bleKeyDelay);
  
  // Load language setting
  getLang();

  bootLogo();
  handleFolders();
}

void saveFile() {
  while(true) {
    M5Cardputer.update();
    if (kb.isChange()) {
      if (kb.isPressed()) {
        Keyboard_Class::KeysState status = kb.keysState();
        
        for (auto i : status.word) {
          fileName += i;
        }
        
        if (status.del) {
          fileName.remove(fileName.length() - 1);  
        }
        
        display.fillScreen(BLACK);
        
        String newFileName = fileType[mainCursor] == 2 ? "Folder Name:" : "File Name:";
        
        display.setCursor(display.width()/2-(newFileName.length()/2)*letterWidth, 0);
        display.println(newFileName);
        display.drawString(fileName, display.width()/2-(fileName.length()/2)*letterWidth, letterHeight);
        
        if (status.enter) {
          if (fileType[mainCursor] == 2) {
            makeFolder();
          } else {
            saveFileChanges();
          }
          break;
        }
      }
    }
  }
  return;
}


void fileWrite() {
  // Add status bar for better user experience
  auto drawStatusBar = [&]() {
    display.fillRect(0, display.height() - 20, display.width(), 20, BLACK);
    display.drawRect(0, display.height() - 20, display.width(), 20, PURPLE);
    display.setTextColor(WHITE);
    display.setCursor(5, display.height() - 15);
    
    if (creatingFile) {
      display.print("NEW");
    } else if (editingFile) {
      display.print("EDIT");
    }
    
    display.print(" | Line: " + String(cursorPosY + 1));
    display.print(" Col: " + String(cursorPosX + 1));
    display.print(" | Lines: " + String(newFileLines + 1));
    
    // Show memory usage
    int usedMemory = (newFileLines * 100) / ELEMENT_COUNT_MAX;
    display.setCursor(display.width() - 60, display.height() - 15);
    display.print("Mem: " + String(usedMemory) + "%");
    
    display.setTextColor(PURPLE);
  };

  while(true) {
    M5Cardputer.update();
    if (kb.isChange()) {
      if (kb.isPressed()) {
        Keyboard_Class::KeysState status = kb.keysState();
        int prevCursorY;

        if (status.fn && kb.isKeyPressed('`')) {
          if (creatingFile) {
            fileName = "\0";
            creatingFile = false;
            saveFile();
            break;
          }
          if (editingFile) {
            String fullName = fileName;
            fullName.remove(fullName.length()-4);
            fileName = fullName;
            editingFile = false;
            saveFile();
            break;
          }
        } else if (status.fn && kb.isKeyPressed(';') && cursorPosY > 0) {
          prevCursorY = cursorPosY;
          cursorPosY--;
          if (cursorPosX >= fileText[prevCursorY].length()) {
            cursorPosX = fileText[cursorPosY].length();
          }
          if (screenPosY > 0 && cursorPosY > 0) {
            screenPosY--;
          }
          if (cursorPosX * letterWidth > display.width() && fileText[cursorPosY].length() * letterWidth > display.width()) {
            screenPosX = (fileText[cursorPosY].length() - 19) * -letterWidth;
          } else {
            screenPosX = 0;
          }
        } else if (status.fn && kb.isKeyPressed('.') && cursorPosY < newFileLines) {
          prevCursorY = cursorPosY;
          cursorPosY++;
          if (cursorPosX >= fileText[prevCursorY].length()) {
            cursorPosX = fileText[cursorPosY].length();
          }
          if (cursorPosY * letterHeight >= display.height() - letterHeight) {
            screenPosY++;
          }
          if (cursorPosX * letterWidth > display.width() && fileText[cursorPosY].length() * letterWidth > display.width()) {
            screenPosX = (fileText[cursorPosY].length() - 19) * -letterWidth;
          } else {
            screenPosX = 0;
          }
        } else if (status.fn && kb.isKeyPressed(',') && cursorPosX > 0) {
          cursorPosX--;
          if (screenPosX < 0) {
            screenPosX += letterWidth;
          }
        } else if (status.fn && kb.isKeyPressed('/') && cursorPosX < fileText[cursorPosY].length()) {
          cursorPosX++;
          if (cursorPosX * letterWidth >= display.width()) {
            screenPosX -= letterWidth;
          }
        } else if (!status.fn) {
          for (auto i : status.word) {

            if (cursorPosX * letterWidth >= display.width() - letterWidth) {
              screenPosX -= letterWidth;
            }
            cursorPosX++;

            insertChar(fileText, cursorPosY, cursorPosX, i);
          }
        }

        if (status.del) {
          if (screenPosX < 0) {
            screenPosX += letterWidth;
          }
          if (cursorPosX > 0) {
            cursorPosX--;
            fileText[cursorPosY].remove(cursorPosX, 1);
          } else if (cursorPosX <= 0 && cursorPosY > 0) {
            
            cursorPosX = fileText[cursorPosY-1].length(); // Set cursor at the end of the previous line
            // Set screen position to where the previous line ends
            if (fileText[cursorPosY-1].length() * letterWidth > display.width()) { 
              screenPosX = (fileText[cursorPosY-1].length() - 19) * -letterWidth;
            } else {
              screenPosX = 0;
            }
            fileText[cursorPosY - 1] += fileText[cursorPosY]; // Move the entire line up to the end of the line above
            removeLine(fileText, cursorPosY);
          }
        }

        if (status.enter) {
          String currentLine = fileText[cursorPosY];
          String remainingLine = currentLine.substring(cursorPosX);
          currentLine = currentLine.substring(0, cursorPosX);

          fileText[cursorPosY] = currentLine;
          newFileLines++;
          cursorPosY++;
          insertLine(fileText, cursorPosY);
          fileText[cursorPosY] = remainingLine;

          if (cursorPosY * letterHeight >= display.height() - letterHeight) {
            screenPosY++;
          }
          screenPosX = 0;
          cursorPosX = 0;
        }

        if (cursorPosX > fileText[cursorPosY].length()) {
          cursorPosX = fileText[cursorPosY].length();
        }

        display.fillScreen(BLACK);

        display.setTextSize(1.5);

        int drawCursorX = cursorPosX * letterWidth;
        int drawCursorY = cursorPosY * letterHeight - 5;

        if (cursorPosX * letterWidth > display.width() - letterWidth) {
          drawCursorX = display.width() - letterWidth;
        }

        if (cursorPosY * letterHeight > display.height() - letterHeight) {
          drawCursorY = (display.height() - letterHeight) - 10;
        }

        display.drawString(cursor, drawCursorX - 3, drawCursorY, &fonts::Font2);
        
        display.setTextSize(2);

        for (int i = 0; i<= newFileLines; i++) {
          display.drawString(fileText[i+screenPosY], screenPosX, i*letterHeight);
        }
        
        // Draw status bar for enhanced user experience
        drawStatusBar();
      }
    }
  }
  return;
}

void loop() {
}
