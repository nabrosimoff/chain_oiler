#include <LiquidCrystal.h>
#include <math.h>
#include <EEPROM.h>

#define LCD_POWER_PIN A0

#define SYM_UP      0
#define SYM_DOWN    1
#define SYM_DEGREE  2
#define SYM_DOT     3
#define SYM_RIGHT   4
#define SYM_LEFT    5

#define LEFT_TOP_BUTTON_PIN      10
#define LEFT_BOTTOM_BUTTON_PIN   13
#define RIGHT_TOP_BUTTON_PIN     11
#define RIGHT_BOTTOM_BUTTON_PIN  12

#define CODE_ADDR 0
#define SETTINGS_ADDR 4

#define OILER_PIN 3


boolean lt_is_down = false;
boolean lb_is_down = false;
boolean rt_is_down = false;
boolean rb_is_down = false;

boolean saved = false;

#define TEMP_PIN A1

LiquidCrystal lcd(4, 5, 6, 7, 8, 9);

byte symUp[8] = {
  B00000,
  B00100,
  B01110,
  B11111,
  B00000,
  B00000,
  B00000,
};

byte symDown[8] = {
  B00000,
  B00000,
  B00000,
  B11111,
  B01110,
  B00100,
  B00000,
};

byte symDegree[8] = {
  B00110,
  B01001,
  B01001,
  B00110,
  B00000,
  B00000,
  B00000,
  B00000,
};

byte symDot[8] = {
  B00000,
  B00000,
  B01110,
  B11111,
  B11111,
  B01110,
  B00000,
  B00000,
};

byte symRight[8] = {
  B10000,
  B11000,
  B11100,
  B11110,
  B11110,
  B11100,
  B11000,
  B10000,
};

byte symLeft[8] = {
  B00001,
  B00011,
  B00111,
  B01111,
  B01111,
  B00111,
  B00011,
  B00001,
};

String menus[] = {
  "Lub. mode",
  "Radius",
  "Viscocity",
  "Distance",
  "Mode",
  "Filling   ",
  "Temperature",
  "Wait for",
  "Open for",
  "Save&exit",
};

String modes[] = {
  "Sport",
  "Default",
  "Rain",
};

float modesCoef[] = {
  1.0,
  0.65,
  1.5,
};

String lubricateMethods[] = {
  "Time",
  "Distance"
};

String viscocities[] = {
  "SAE 80",
  "SAE 90",
};

int menuId = 0;

#define LUB_MODE_ID    0
#define RADIUS_ID      1
#define VISCOCITY_ID   2
#define DISTANCE_ID    3
#define MODE_ID        4
#define FILLING_ID     5
#define TEMPERATURE_ID 6
#define WAIT_ID        7
#define OPEN_ID        8
#define SAVE_ID        9

unsigned char menuValues[9] = {1};


#define MODE_TIME 0
#define MODE_DIST 1
bool menuPosChanged = false;

volatile unsigned int turnsCount = 0;

boolean oilerOpened = false;
unsigned long sinceClosed = 0;
unsigned long sinceOpened = 0;
unsigned long lastMillis = 0;

void setup() {
  
  Serial.begin(9600);
  pinMode(TEMP_PIN, INPUT);
  pinMode(OILER_PIN, OUTPUT);
  initButtonsPins();
  lcdTurnOn();
  initLCD();
  lastMillis = millis();
  
  setNavigationButtons();
  
  //lcd.setCursor(15, 0);
  //lcd.write(byte(SYM_DEGREE));
  //lcd.setCursor(15,1);
  //lcd.write(byte(SYM_DOT));
  attachInterrupt(digitalPinToInterrupt(2), newTurn, FALLING);
  
  boolean right = isCodeRight();
  Serial.print("Code right: ");
  Serial.println(right);
  Serial.print("Settings: ");
  if (right) {
    loadSettings();
    for (int i = 0; i < 9; ++i) {
      Serial.print(menuValues[i], DEC);
      Serial.print(" ");
    }
    Serial.print("\n");
  }
  else {
    for (int i = 0; i <= SAVE_ID; i++) {
      menuValues[i] = 1;
    }
  }
  
  
  lcdMenuUpdate(menuId);
}

void loop() {
  if (menuId == TEMPERATURE_ID) {
    updateTemperature();
  } 
  
  lt_is_down = buttonClicked(LEFT_TOP_BUTTON_PIN, lt_is_down);
  
  if (lt_is_down) {
    scrollMenuUp();
    menuPosChanged = true;
  }

  lb_is_down = buttonClicked(LEFT_BOTTOM_BUTTON_PIN, lb_is_down); 
  if (lb_is_down) {
    scrollMenuDown();
    menuPosChanged = true;
  }  
  
  rt_is_down = buttonClicked(RIGHT_TOP_BUTTON_PIN, lt_is_down);
  
  if (rt_is_down) {
    scrollMenuRight();
    menuPosChanged = true;
  }

  rb_is_down = buttonClicked(RIGHT_BOTTOM_BUTTON_PIN, lb_is_down);
    
  if (rb_is_down) {
    scrollMenuLeft();
    menuPosChanged = true;
  }
  
  if (menuPosChanged) {
    lcdMenuUpdate(menuId);
    menuPosChanged = false;
    saved = false;
    delay(100);
  }
  
  if (!oilerOpened) {
    if (menuValues[LUB_MODE_ID] == MODE_TIME) {
      sinceClosed += timeDiff(lastMillis);
      if (sinceClosed >= menuValues[WAIT_ID] * 1000) {
        oilerOpened = true;
        sinceOpened = 0;
      }
    }
  }
  
  if (!oilerOpened) {
    if (menuValues[LUB_MODE_ID] == MODE_DIST) {
      if (turnsCount >= distToTurns(menuValues[RADIUS_ID]
                                  , menuValues[DISTANCE_ID])) {
        oilerOpened = true;
      }
    }
  }
  
  if (oilerOpened) {
    sinceOpened += timeDiff(lastMillis);
    if (sinceOpened >= int(float(menuValues[OPEN_ID] * 100) * modesCoef[menuValues[MODE_ID]])) {
      oilerOpened = false;
      sinceClosed = 0;
      turnsCount = 0;
    }
  }
  
  setOiling(oilerOpened);
  
  lastMillis = millis();
  //Serial.print(menus[menuId]);
  //Serial.print("\n");

  //Serial.println(turnsCount);
  
  delay(150);
}

unsigned int distToTurns(unsigned char inRadiusInch, unsigned char inDistance) {
  float singleTurnLen = 2 * 3.14 * (float(inRadiusInch) * 2.54 + 10.0);
  float turnsPerDist = float(inDistance) * 100000.0 / singleTurnLen;
  return int(turnsPerDist);
}

int timeDiff(unsigned long inLastTime) {
  unsigned long curTime = millis();
  if (curTime > inLastTime) {
    return curTime - inLastTime;
  }
  else {
    return 0xFFFFFFFF - inLastTime + curTime;
  }
}

void newTurn() {
  turnsCount++;
}

void scrollMenuRight() {
  switch (menuId) {
    case LUB_MODE_ID:
      nextLubMode();
      break;
    case RADIUS_ID:
      nextRadius();
      break;
    case VISCOCITY_ID:
      nextViscocity();
      break;
    case DISTANCE_ID:
      nextDistance();
      break;
    case MODE_ID:
      nextMode();
      break;
    case FILLING_ID:
      break; //TODO
    case WAIT_ID:
      nextWait();
      break;
    case OPEN_ID:
      nextOpen();
      break;
  }
}

void nextLubMode() {
  if (menuValues[LUB_MODE_ID] == 0) {
    menuValues[LUB_MODE_ID] = 1;
  }
  else {
    menuValues[LUB_MODE_ID] = 0;
  }
}

void nextRadius() {
  if (menuValues[RADIUS_ID] != 255) {
    menuValues[RADIUS_ID]++;
  }
  else {
    menuValues[RADIUS_ID] = 1;
  }
}

void nextViscocity() {
  if (menuValues[VISCOCITY_ID] == 0) {
    menuValues[VISCOCITY_ID] = 1;
  }
  else {
    menuValues[VISCOCITY_ID] = 0;
  } 
}

void nextDistance() {
  if (menuValues[DISTANCE_ID] != 255) {
    menuValues[DISTANCE_ID]++;
  }
  else {
    menuValues[DISTANCE_ID] = 1;
  }
}

void nextMode() {
  if (menuValues[MODE_ID] != 2) {
    menuValues[MODE_ID]++;
  }
  else {
    menuValues[MODE_ID] = 0;
  }
}

void nextWait() {
  if (menuValues[WAIT_ID] != 255) {
    menuValues[WAIT_ID]++;
  }
  else {
    menuValues[WAIT_ID] = 1;
  }
}

void nextOpen() {
  if (menuValues[OPEN_ID] != 255) {
    menuValues[OPEN_ID]++;
  }
  else {
    menuValues[OPEN_ID] = 1;
  }
}

void scrollMenuLeft() {
  switch (menuId) {
    case LUB_MODE_ID:
      prevLubMode();
      break;
    case RADIUS_ID:
      prevRadius();
      break;
    case VISCOCITY_ID:
      prevViscocity();
      break;
    case DISTANCE_ID:
      prevDistance();
      break;
    case MODE_ID:
      prevMode();
      break;
    case FILLING_ID:
      break; //TODO
    case WAIT_ID:
      prevWait();
      break;
    case OPEN_ID:
      prevOpen();
      break;
    case SAVE_ID:
      saveSettings();
      saved = true;
      break;
  }
}

void prevLubMode() {
  nextLubMode();
}

void prevRadius() {
  if (menuValues[RADIUS_ID] != 1) {
    menuValues[RADIUS_ID]--;
  }
  else {
    menuValues[RADIUS_ID] = 255;
  }
}

void prevViscocity() {
  nextViscocity(); 
}

void prevDistance() {
  if (menuValues[DISTANCE_ID] != 1) {
    menuValues[DISTANCE_ID]--;
  }
  else {
    menuValues[DISTANCE_ID] = 255;
  }
}

void prevMode() {
  if (menuValues[MODE_ID] != 0) {
    menuValues[MODE_ID]--;
  }
  else {
    menuValues[MODE_ID] = 2;
  }
}

void prevWait() {
  if (menuValues[WAIT_ID] != 1) {
    menuValues[WAIT_ID]--;
  }
  else {
    menuValues[WAIT_ID] = 255;
  }
}

void prevOpen() {
  if (menuValues[OPEN_ID] != 1) {
    menuValues[OPEN_ID]--;
  }
  else {
    menuValues[OPEN_ID] = 255;
  }
}

void saveSettings() {
  for (int i = 0; i < 9; ++i)
    EEPROM.write(SETTINGS_ADDR + i, menuValues[i]);
    
  EEPROM.write(CODE_ADDR, 0xDE);
  EEPROM.write(CODE_ADDR + 1, 0xAD);
  EEPROM.write(CODE_ADDR + 2, 0xBE);
  EEPROM.write(CODE_ADDR + 3, 0xEF);
  
  Serial.println("Code path phrase written");
}

boolean isCodeRight() {
  Serial.print("Readen code phrase: ");
  Serial.print(EEPROM.read(CODE_ADDR), HEX);
  Serial.print(EEPROM.read(CODE_ADDR + 1), HEX);
  Serial.print(EEPROM.read(CODE_ADDR + 2), HEX);
  Serial.println(EEPROM.read(CODE_ADDR + 3), HEX);
  return (EEPROM.read(CODE_ADDR) == 0xDE &&
  EEPROM.read(CODE_ADDR + 1) == 0xAD &&
  EEPROM.read(CODE_ADDR + 2) == 0xBE &&
  EEPROM.read(CODE_ADDR + 3) == 0xEF);
}

void loadSettings() {
  for (int i = 0; i < 9; ++i)
    menuValues[i] = EEPROM.read(SETTINGS_ADDR + i);
}

void printOk() {
  lcd.setCursor(5, 1);
  lcd.print("SAVED!");
}

void scrollMenuUp() {
  if (menuId == 0) {
    menuId = SAVE_ID;
  }
  else
  if (menuValues[LUB_MODE_ID] != MODE_TIME && menuId == SAVE_ID) {
    menuId = WAIT_ID - 1;
  }
  else
  if (menuValues[LUB_MODE_ID] == MODE_TIME && menuId == RADIUS_ID + 3) {
    menuId = RADIUS_ID - 1;
  }
  else {
    menuId--;
  }
}

void scrollMenuDown() {
  if (menuId == SAVE_ID) {
    menuId = 0;
  }
  else
  if (menuValues[LUB_MODE_ID] != MODE_TIME && menuId == WAIT_ID - 1) {
    menuId = SAVE_ID;
  }
  else
  if (menuValues[LUB_MODE_ID] == MODE_TIME && menuId == RADIUS_ID - 1) {
    menuId = RADIUS_ID + 3;
  }
  else {
    menuId++;
  }
}

void lcdMenuUpdate(int id) {
  
  String currMenu = menus[id];
  int pos = 8 - (currMenu.length() / 2) - 1;
  lcd.setCursor(1,0);
  lcd.print("               ");
  lcd.setCursor(pos, 0);
  lcd.print(currMenu);
  lcd.setCursor(1,1);
  lcd.print("               ");
  
  switch (menuId) {
    case LUB_MODE_ID:
      updateLubMenuItem();
      break;
    case RADIUS_ID:
      updateRadiusItem();
      break;
    case VISCOCITY_ID:
      updateViscocityItem();
      break;
    case DISTANCE_ID:
      updateDistanceItem();
      break;
    case MODE_ID:
      updateModeItem();
      break;
    case FILLING_ID:
      printFillingButtons();
      break;
    case WAIT_ID:
      printWaitItem();
      break;
    case OPEN_ID:
      printOpenItem();
  }
  
  if (menuId != SAVE_ID &&
      menuId != TEMPERATURE_ID &&
      menuId != FILLING_ID) {
      setLeftRightButtons();
  }
  else if (menuId == SAVE_ID) {
    lcd.setCursor(15,1);
    lcd.write(SYM_DOT);
  }
  
  if (saved) {
    printOk();
  }
}

void updateLubMenuItem() {
    String lubMenuItemStr = lubricateMethods[menuValues[LUB_MODE_ID]];
    int pos = 8 - (lubMenuItemStr.length() / 2) - 1;
    lcd.setCursor(pos, 1);
    lcd.print(lubMenuItemStr);
}

void updateRadiusItem() {
    char radius[4];
    String radiusStr = itoa(menuValues[RADIUS_ID], radius, 10) ;
    radiusStr = radiusStr + "R";
    int pos = 8 - (radiusStr.length() / 2) - 1;
    lcd.setCursor(pos, 1);
    lcd.print(radiusStr);
}

void updateViscocityItem() {
    String viscMenuItemStr = viscocities[menuValues[VISCOCITY_ID]];
    int pos = 8 - (viscMenuItemStr.length() / 2) - 1;
    lcd.setCursor(pos, 1);
    lcd.print(viscMenuItemStr);
}

void updateDistanceItem() {
    char trash[4];
    String distStr = itoa(menuValues[DISTANCE_ID], trash, 10) ;
    distStr = distStr + "00m";
    int pos = 8 - (distStr.length() / 2) - 1;
    lcd.setCursor(pos, 1);
    lcd.print(distStr);
}

void updateModeItem() {
    String modeMenuItemStr = modes[menuValues[MODE_ID]];
    int pos = 8 - (modeMenuItemStr.length() / 2) - 1;
    lcd.setCursor(pos, 1);
    lcd.print(modeMenuItemStr);
}

void printFillingButtons() {
  lcd.setCursor(11, 0);
  lcd.print("START");
  lcd.setCursor(12, 1);
  lcd.print("STOP");
}

void printWaitItem() {
    char trash[4];
    String waitStr = itoa(menuValues[WAIT_ID], trash, 10) ;
    waitStr = waitStr + "sec";
    int pos = 8 - (waitStr.length() / 2) - 1;
    lcd.setCursor(pos, 1);
    lcd.print(waitStr);
}

void printOpenItem() {
    char trash[4];
    String openStr = itoa(menuValues[OPEN_ID], trash, 10) ;
    openStr = openStr + "00msec";
    int pos = 8 - (openStr.length() / 2) - 1;
    lcd.setCursor(pos, 1);
    lcd.print(openStr);
}

int getTemperature() {
  float voltage = analogRead(TEMP_PIN) * 5.0 / 1023.0;
  float temp = 1.0/(log(voltage / 2.5) / 4300.0 + 1.0 / 298.0) - 273.0;
  return int(round(temp));
}

void initButtonsPins() {
  pinMode(LEFT_TOP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(LEFT_BOTTOM_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RIGHT_TOP_BUTTON_PIN, INPUT_PULLUP);
  pinMode(RIGHT_BOTTOM_BUTTON_PIN, INPUT_PULLUP);
}

void setNavigationButtons() {
  lcd.setCursor(0,0);
  lcd.write(byte(SYM_UP));
  lcd.setCursor(0,1);
  lcd.write(SYM_DOWN);
}

void setLeftRightButtons() {
  lcd.setCursor(15,0);
  lcd.write(byte(SYM_RIGHT));
  lcd.setCursor(15,1);
  lcd.write(SYM_LEFT);
}

void initLCD() {
  pinMode(LCD_POWER_PIN, OUTPUT);
  lcd.begin(16, 2);
  lcd.createChar(SYM_UP, symUp);
  lcd.createChar(SYM_DOWN, symDown);
  lcd.createChar(SYM_DEGREE, symDegree);
  lcd.createChar(SYM_DOT, symDot);
  lcd.createChar(SYM_RIGHT, symRight);
  lcd.createChar(SYM_LEFT, symLeft);
}

void lcdTurnOn() {
  analogWrite(LCD_POWER_PIN, 255);
}

void lcdTurnOff() {
  analogWrite(LCD_POWER_PIN, 0);
}

void updateTemperature() {
  int temperature = getTemperature();
  if (temperature < 0) {
    lcd.setCursor(5, 1);
  }
  else {
    lcd.setCursor(6, 1);
  }
  lcd.print(temperature);
  lcd.write(byte(SYM_DEGREE));
  lcd.print("C");
}

boolean buttonIsDown(int btnPin) {
    return (!digitalRead(btnPin));
}

boolean buttonClicked(int btnPin, boolean wasDown) {
  boolean isDown = buttonIsDown(btnPin);
  if (buttonIsDown(btnPin) && !wasDown) {
    delay(10);
    isDown = buttonIsDown(btnPin);
  }
  return isDown;
}

void setOiling(boolean inOpen) {
  digitalWrite(OILER_PIN, inOpen);
}


 
